//==============================================================================
//
//  audio_package.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "Audio/audio_package.hpp"
#include "IOManager/io_mgr.hpp"
#include "x_files.hpp"
#include "e_Audio.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

s32 N_ARAM_USED = 0; // 512*1024;

#define LOG_AUDIO_PACKAGE_LOAD "audio_package::Init"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_package::audio_package( void )
{
    m_IsLoaded      = FALSE;
    m_Link.pPrev    = NULL;
    m_Link.pNext    = NULL;
    m_Link.pPackage = NULL;
    m_pRuntime      = NULL;
    m_Filename[0]   = 0;
    m_LookupName[0] = 0;
    m_UserVolume      =
    m_UserPitch       =
    m_UserEffectSend  =
    m_UserNearFalloff =
    m_UserFarFalloff  = 1.0f;
    x_memset( &m_Header, 0, sizeof(m_Header) );
}

//==============================================================================

audio_package::~audio_package( void )
{
}

//==============================================================================

u32 audio_package::LoadHotSample( X_FILE* f, hot_sample* pHotSample, uaddr Aram )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_package::LoadHotSample");

    switch( pHotSample->CompressionType )
    {
        case ADPCM:
        case MP3:
        case PCM:
        {
            // Seek to sample offset within the file
            x_fseek( f, pHotSample->WaveformOffset, X_SEEK_SET );
            // Read the sample waveform
            x_fread( (void*)Aram, pHotSample->WaveformLength, 1, f );

            // Done.
            return pHotSample->WaveformLength;
        }
        default:
        {
            ASSERT( 0 );
            break;
        }
    }

    return 0;
}

//==============================================================================

static
void ReadSampleHeaders( X_FILE* f, sample_header* pDest, s32 nHeaders, s32 DiskSize )
{
    struct disk_sample_header
    {
        u32 AudioRam;
        u32 WaveformOffset;
        u32 WaveformLength;
        u32 LipSyncOffset;
        u32 BreakPointOffset;
        u32 CompressionType;
        s32 nSamples;
        s32 SampleRate;
        s32 LoopStart;
        s32 LoopEnd;
    };

    ASSERT( DiskSize == (s32)sizeof(disk_sample_header) );
    (void)DiskSize;

    for( s32 i=0 ; i<nHeaders ; i++ )
    {
        disk_sample_header DiskHeader;
        x_fread( &DiskHeader, sizeof(DiskHeader), 1, f );

        pDest[i].AudioRam         = 0;
        pDest[i].WaveformOffset   = DiskHeader.WaveformOffset;
        pDest[i].WaveformLength   = DiskHeader.WaveformLength;
        pDest[i].LipSyncOffset    = DiskHeader.LipSyncOffset;
        pDest[i].BreakPointOffset = DiskHeader.BreakPointOffset;
        pDest[i].CompressionType  = DiskHeader.CompressionType;
        pDest[i].nSamples         = DiskHeader.nSamples;
        pDest[i].SampleRate       = DiskHeader.SampleRate;
        pDest[i].LoopStart        = DiskHeader.LoopStart;
        pDest[i].LoopEnd          = DiskHeader.LoopEnd;
    }
}

//==============================================================================

xbool audio_package::Init( audio_runtime& Runtime, const char* pFilename, const char* pLookupName )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_package::Init");

    X_FILE*             f = NULL;
    package_identifier  PackageID;
    xbool               Result = FALSE;
    xbool               AramAccounted = FALSE;

    m_pRuntime = &Runtime;

    //
    // Clear out pointers
    //
    {
        m_IdentifierStringTable = NULL;
        m_IdentifierTable = NULL;
        m_DescriptorTable = NULL;
        m_DescriptorBuffer = NULL;
        m_MusicData = NULL;
        m_LipSyncTable = NULL;
        m_BreakPointTable = NULL;
        m_HotSamples = NULL;
        m_ColdSamples = NULL;
        m_AudioRam = 0;

        for( s32 i=0 ; i<NUM_TEMPERATURES ; i++ )
        {
            m_SampleIndices[ i ] = NULL;
        }
    }

#if defined(LOG_AUDIO_PACKAGE_LOAD)
    LOG_MESSAGE( LOG_AUDIO_PACKAGE_LOAD, "loading package: [%s]", pFilename );
#endif

    // Error check.
    ASSERT( pFilename );
    ASSERT( pLookupName );
    const char* pStoredLookupName = pLookupName ? pLookupName : pFilename;

    // Save the filename
    ASSERT( x_strlen( pFilename ) < AUDIO_PACKAGE_FILENAME_LENGTH );
    x_strncpy( m_Filename, pFilename, AUDIO_PACKAGE_FILENAME_LENGTH );
    m_Filename[ AUDIO_PACKAGE_FILENAME_LENGTH-1 ] = 0;

    ASSERT( x_strlen( pStoredLookupName ) < AUDIO_PACKAGE_FILENAME_LENGTH );
    x_strncpy( m_LookupName, pStoredLookupName, AUDIO_PACKAGE_FILENAME_LENGTH );
    m_LookupName[ AUDIO_PACKAGE_FILENAME_LENGTH-1 ] = 0;

    // Open the file.
    f = x_fopen( pFilename, "rb" );
    if( f )
    {
        // Read in the package identifier.
        x_fread( &PackageID, sizeof(package_identifier), 1, f );

        // Correct version?
        if( !x_strncmp( PackageID.VersionID, PACKAGE_VERSION, VERSION_ID_SIZE ) )
        {
            // Correct platform?
            if( !x_strncmp( PackageID.TargetID, TARGET_ID, TARGET_ID_SIZE ) )
            {
                s32 i;

                // Now read in the header.
                x_fread( &m_Header, sizeof(package_header), 1, f );

                if( (m_Header.nDescriptors <= 0) || (m_Header.nDescriptors > 5000) )
                {
                    goto Cleanup;
                }
                if( m_Header.nIdentifiers <= 0 )
                {
                    goto Cleanup;
                }
                if( m_Header.DescriptorFootprint <= 0 )
                {
                    goto Cleanup;
                }

                ASSERT( m_Header.nDescriptors > 0 );
                ASSERT( m_Header.nIdentifiers > 0 );
                ASSERT( m_Header.DescriptorFootprint > 0 );

                if (m_Header.nDescriptors == 0 ||
                    m_Header.nIdentifiers == 0 ||
                    m_Header.DescriptorFootprint == 0)
                {
                    goto Cleanup;
                }

                if( m_Header.nSampleHeaders[ WARM ] || m_Header.nSampleIndices[ WARM ] )
                {
                    ASSERTS( 0, "Warm audio samples are not supported." );
                    goto Cleanup;
                }

                // Allocate memory for the string table.
                m_IdentifierStringTable = (char*)x_malloc( m_Header.StringTableFootprint );

                // Allocate memory for the music data.
                if( m_Header.MusicDataFootprint )
                {
                    m_MusicData = (char*)x_malloc( m_Header.MusicDataFootprint );
                }
                else
                {
                    m_MusicData = NULL;
                }

                // Allocate memory for the lip sync table.
                if( m_Header.LipSyncTableFootprint )
                {
                    m_LipSyncTable = (char*)x_malloc( m_Header.LipSyncTableFootprint );
                }
                else
                {
                    m_LipSyncTable = NULL;
                }

                // Allocate memory for the break point table
                if( m_Header.BreakPointTableFootprint )
                {
                    m_BreakPointTable = (char*)x_malloc( m_Header.BreakPointTableFootprint );
                }
                else
                {
                    m_BreakPointTable = NULL;
                }

                // Allocate memory for the descriptor identifiers
                m_IdentifierTable = (descriptor_identifier*)x_malloc( m_Header.nIdentifiers * sizeof(descriptor_identifier) );

                // Allocate memory for the descriptor table.
                m_DescriptorTable = (uaddr*)x_malloc( m_Header.nDescriptors * sizeof(uaddr) );

                // Allocate memory for the descriptors.
                m_DescriptorBuffer = (u16*)x_malloc( m_Header.DescriptorFootprint );

                // For each temperature...
                for( i=0 ; i<NUM_TEMPERATURES ; i++ )
                {
                    if( m_Header.nSampleIndices[ i ] )
                    {
                        // Allocate memory for sample header index table.
                        m_SampleIndices[ i ] = (u16*)x_malloc( (m_Header.nSampleIndices[ i ]+1) * sizeof(u16) );
                    }
                    else
                    {
                        m_SampleIndices[ i ] = 0;
                    }
                }

                // Allocate memory for the hot and cold samples
                if( m_Header.nSampleHeaders[ HOT ] )
                {
                    m_HotSamples = (void*)x_malloc( m_Header.nSampleHeaders[ HOT ] * sizeof(sample_header) );
                }
                else
                {
                    m_HotSamples = NULL;
                }

                if( m_Header.nSampleHeaders[ COLD ] )
                {
                    m_ColdSamples = (void*)x_malloc( m_Header.nSampleHeaders[ COLD ] * sizeof(sample_header) );
                }
                else
                {
                    m_ColdSamples = NULL;
                }

                // Read in the string table.
                x_fread( m_IdentifierStringTable, m_Header.StringTableFootprint, 1, f );

                // Read in the music data.
                if( m_MusicData )
                {
                    x_fread( m_MusicData, m_Header.MusicDataFootprint, 1, f );
                }

                // Read in the lipsync data
                if( m_LipSyncTable )
                {
                    x_fread( m_LipSyncTable, m_Header.LipSyncTableFootprint, 1, f );
                }

                // Read in the breakpoint data
                if( m_BreakPointTable )
                {
                    x_fread( m_BreakPointTable, m_Header.BreakPointTableFootprint, 1, f );
                }

                // Read the 32-bit on-disk identifier records into native records.
                for( i=0 ; i<m_Header.nIdentifiers ; i++ )
                {
                    struct disk_descriptor_identifier
                    {
                        u16 StringOffset;
                        u16 Index;
                        u32 PackageSlot;
                    } DiskId;

                    x_fread( &DiskId, sizeof(DiskId), 1, f );
                    m_IdentifierTable[ i ].StringOffset = DiskId.StringOffset;
                    m_IdentifierTable[ i ].Index        = DiskId.Index;
                }

                // Set the package for each identifier.
                for( i=0 ; i<m_Header.nIdentifiers ; i++ )
                {
                    m_IdentifierTable[ i ].pPackage = this;
                }

                // Read 32-bit on-disk descriptor offsets and resolve to native pointers.
                for( i=0 ; i<m_Header.nDescriptors ; i++ )
                {
                    s32 Offset;
                    x_fread( &Offset, sizeof(Offset), 1, f );
                    m_DescriptorTable[ i ] = (uaddr)m_DescriptorBuffer + (u32)Offset;
                }

                // Read in the descriptors.
                x_fread( m_DescriptorBuffer, m_Header.DescriptorFootprint, 1, f );

                // Read in the sample header indices.
                for( i=0 ; i<NUM_TEMPERATURES ; i++ )
                {
                    // Only if buffer is available.
                    if( m_SampleIndices[ i ] )
                    {
                        // Read each temperatures sample index.
                        x_fread( m_SampleIndices[ i ], m_Header.nSampleIndices[ i ]+1, sizeof(u16), f );
                    }
                }

                // Read in the hot sample headers
                if( m_HotSamples )
                {
                    ReadSampleHeaders( f, (sample_header*)m_HotSamples, m_Header.nSampleHeaders[ HOT ], m_Header.HeaderSizes[ HOT ] );
                }

                // Read in the cold sample headers
                if( m_ColdSamples )
                {
                    ReadSampleHeaders( f, (sample_header*)m_ColdSamples, m_Header.nSampleHeaders[ COLD ], m_Header.HeaderSizes[ COLD ] );
                }

                // TODO: Allocate individual aram for the samples.
                // Load the hot samples.
                N_ARAM_USED += m_Header.Aram;
                AramAccounted = TRUE;

#if defined(LOG_AUDIO_PACKAGE_LOAD)
                LOG_MESSAGE( LOG_AUDIO_PACKAGE_LOAD, "ARAM Required: %d, Total: %d", m_Header.Aram, N_ARAM_USED );
                LOG_FLUSH();
#endif
                m_AudioRam = 0;
                if( m_Header.Aram )
                {
                    m_AudioRam    = (uaddr)Runtime.Backend.AllocAudioRam( m_Header.Aram );
                    uaddr Aram    = m_AudioRam;
                    s32 TotalAram = 0;
                    u32 BlockSize = 0;
                    uaddr Base    = (uaddr)m_HotSamples;

                    #ifdef X_DEBUG
                    if( m_AudioRam==0 )
                    {
                        x_DebugMsg( "Audio package load failed!\n" );
                        x_DebugMsg( "-=> %s\n", pFilename );
                        x_DebugMsg( "Currently loaded packages:\n" );
                        Runtime.Audio.DisplayPackages();
                        ASSERT( m_AudioRam );
                    }
                    #endif

                    for( i=0 ; i<m_Header.nSampleHeaders[ HOT ] ; i++, Base += sizeof(sample_header) )
                    {
                        hot_sample* pHotSample = (hot_sample*)Base;

                        if( m_AudioRam )
                        {
                            // Set the headers aram.
                            pHotSample->AudioRam = Aram;
                            // Load the hot sample.
                            BlockSize = LoadHotSample( f, pHotSample, Aram );
                            Aram += BlockSize;
                            TotalAram += BlockSize;
                            ASSERT( TotalAram <= m_Header.Aram );
                        }
                    }
                }

                // Set the user parameter defaults.
                SetUserVolume( 1.0f );
                SetUserPitch( 1.0f );
                SetUserEffectSend( 1.0f );
                SetUserNearFalloff( 1.0f );
                SetUserFarFalloff( 1.0f );
                SetUserNearDiffuse( 1.0f );
                SetUserFarDiffuse( 1.0f );

                // Nuke the packages 3d pan.
                m_Header.Params.Pan3d.Set( 0.0f, 0.0f, 0.0f, 0.0f );

                // All done!
                m_IsLoaded = TRUE;

                // Its all good!
                Result = ((m_AudioRam)||(!m_Header.Aram)) ? TRUE : FALSE;
            }
        }
    }

Cleanup:
    if( f )
    {
        x_fclose( f );
    }

    if( Result == FALSE )
    {
        if( m_AudioRam )
        {
            Runtime.Backend.FreeAudioRam( (void*)m_AudioRam );
            m_AudioRam = 0;
        }

        if( AramAccounted )
        {
            N_ARAM_USED -= m_Header.Aram;
        }

        x_free(m_IdentifierStringTable);    m_IdentifierStringTable = NULL;
        x_free(m_IdentifierTable);          m_IdentifierTable = NULL;
        x_free(m_DescriptorTable);          m_DescriptorTable = NULL;
        x_free(m_DescriptorBuffer);         m_DescriptorBuffer = NULL;
        x_free(m_MusicData);                m_MusicData = NULL;
        x_free(m_LipSyncTable);             m_LipSyncTable = NULL;
        x_free(m_BreakPointTable);          m_BreakPointTable = NULL;
        x_free(m_HotSamples);               m_HotSamples = NULL;
        x_free(m_ColdSamples);              m_ColdSamples = NULL;

        for( s32 i=0 ; i<NUM_TEMPERATURES ; i++ )
        {
            x_free( m_SampleIndices[ i ] );
            m_SampleIndices[ i ] = NULL;
        }
    }

    // Tell the world...
    return Result;
}

//==============================================================================

void audio_package::Kill( void )
{
    N_ARAM_USED -= m_Header.Aram;

    // Nuke voices that belong to this package.
    Runtime().Voices.ReleasePackagesVoices( this );
    Runtime().Backend.FlushRenderCommands();

    // Free the memory up
    x_free( m_IdentifierStringTable );
    x_free( m_IdentifierTable );
    x_free( m_DescriptorTable );
    x_free( m_DescriptorBuffer );
    if( m_MusicData )
    {
        x_free( m_MusicData );
    }
    if( m_LipSyncTable )
    {
        x_free( m_LipSyncTable );
    }
    if( m_BreakPointTable )
    {
        x_free( m_BreakPointTable );
    }
    for( s32 i=0 ; i<NUM_TEMPERATURES ; i++ )
    {
        if( m_SampleIndices[ i ] )
        {
            x_free( (void*)m_SampleIndices[ i ] );
        }
    }
    if( m_HotSamples )
    {
        x_free( m_HotSamples );
    }
    if( m_ColdSamples )
    {
        x_free( m_ColdSamples );
    }
    if( m_AudioRam )
    {
        Runtime().Backend.FreeAudioRam( (void*)m_AudioRam );
    }

    m_pRuntime = NULL;
}

//==============================================================================

void audio_package::SetUserVolume( f32 Volume )
{
    m_UserVolume = Volume;
    ComputeVolume();
}

//==============================================================================

void audio_package::SetUserPitch( f32 Pitch )
{
    m_UserPitch = Pitch;
    ComputePitch();
}

//==============================================================================

void audio_package::SetUserEffectSend( f32 EffectSend )
{
    m_UserEffectSend = EffectSend;
    ComputeEffectSend();
}

//==============================================================================

void audio_package::SetUserNearFalloff( f32 NearFalloff )
{
    m_UserNearFalloff = NearFalloff;
    ComputeNearFalloff();
}

//==============================================================================

void audio_package::SetUserFarFalloff( f32 FarFalloff )
{
    m_UserFarFalloff = FarFalloff;
    ComputeFarFalloff();
}

//==============================================================================

void audio_package::SetUserNearDiffuse( f32 NearDiffuse )
{
    m_UserNearDiffuse = NearDiffuse;
    ComputeNearDiffuse();
}

//==============================================================================

void audio_package::SetUserFarDiffuse( f32 FarDiffuse )
{
    m_UserFarDiffuse = FarDiffuse;
    ComputeFarDiffuse();
}

//==============================================================================

void audio_package::ComputeVolume( void )
{
    f32 Volume;

    // TODO: Put in master fader.

    // Calculate the volume.
    Volume = m_UserVolume * m_Header.Params.Volume;

    // Ducking enabled?
    if( Runtime().AudioDuckLevel > 0 )
    {
        Volume *= m_Header.Params.VolumeDuck;
    }

    // Was there a change?
    if( Volume != m_Volume )
    {
        // Set the volume.
        m_Volume = Volume;

        // Update all voices that reference this package.
        Runtime().Voices.UpdateVoiceVolume( this );
    }
}

//==============================================================================

void audio_package::ComputePitch( void )
{
    f32 Pitch;

    // TODO: Put in master fader.
    
    // Calculate the pitch.
    Pitch = m_UserPitch * m_Header.Params.Pitch;

    // Was there a change?
    if( Pitch != m_Pitch )
    {
        // Set the pitch
        m_Pitch = Pitch;

        // Update all voices that reference this package.
        Runtime().Voices.UpdateVoicePitch( this );
    }
}

//==============================================================================

void audio_package::ComputeEffectSend( void )
{
    f32 EffectSend;

    // TODO: Put in master fader.

    // Calculate the effect send.
    EffectSend = m_UserEffectSend * m_Header.Params.EffectSend;

    // Was there a change?
    if( EffectSend != m_EffectSend )
    {
        // Set the effect send.
        m_EffectSend = EffectSend;

        // Update all voices that reference this package.
        Runtime().Voices.UpdateVoiceEffectSend( this );
    }
}

//==============================================================================

void audio_package::ComputeNearFalloff( void )
{
    f32 NearFalloff;

    // TODO: Put in master fader.

    // Calculate the near falloff.
    NearFalloff = m_UserNearFalloff * m_Header.Params.NearFalloff;

    // Was there a change?
    if( NearFalloff != m_NearFalloff )
    {
        // Set the near falloff.
        m_NearFalloff = NearFalloff;
    }
}

//==============================================================================

void audio_package::ComputeFarFalloff( void )
{
    f32 FarFalloff;

    // TODO: Put in master fader.

    // Calculate the far fall off.
    FarFalloff = m_UserFarFalloff * m_Header.Params.FarFalloff;

    // Was there a change?
    if( FarFalloff != m_FarFalloff )
    {
        // Set the far falloff
        m_FarFalloff = FarFalloff;
    }
}

//==============================================================================

void audio_package::ComputeNearDiffuse( void )
{
    f32 NearDiffuse;

    // TODO: Put in master fader.

    // Calculate the near falloff.
    NearDiffuse = m_UserNearDiffuse * m_Header.Params.NearDiffuse;

    // Was there a change?
    if( NearDiffuse != m_NearDiffuse )
    {
        // Set the near falloff.
        m_NearDiffuse = NearDiffuse;
    }
}

//==============================================================================

void audio_package::ComputeFarDiffuse( void )
{
    f32 FarDiffuse;

    // TODO: Put in master fader.

    // Calculate the far fall off.
    FarDiffuse = m_UserFarDiffuse * m_Header.Params.FarDiffuse;

    // Was there a change?
    if( FarDiffuse != m_FarDiffuse )
    {
        // Set the far falloff
        m_FarDiffuse = FarDiffuse;
    }
}

//==============================================================================

char* audio_package::GetMusicType( void )
{
    if( m_MusicData )
    {
        return m_MusicData;
    }
    else
    {
        return NULL;
    }
}

//==============================================================================

s32 audio_package::GetMusicIntensity( music_intensity* & Intensity )
{
    s32 Result = 0;

    Intensity = NULL;

    if( m_MusicData )
    {
        s32 mi_size = sizeof( music_intensity );
        Intensity = (music_intensity*)(m_MusicData + 32);
        Result  =  m_Header.MusicDataFootprint;
        Result -= 32;
        Result /= mi_size;
    }

    return Result;
}
