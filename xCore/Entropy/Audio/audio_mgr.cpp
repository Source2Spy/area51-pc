//==============================================================================
//
//  audio_mgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Audio.hpp"
#include "Audio/audio_types.hpp"
#include "Audio/audio_package_format.hpp"
#include "Audio/audio_command_queue.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "Audio/audio_package.hpp"
#include "Audio/audio_package_registry.hpp"
#include "Audio/audio_stream_mgr.hpp"
#include "Audio/audio_stream_decoder_factory.hpp"
#include "Audio/audio_stream_runtime.hpp"
#include "Audio/audio_descriptor_runtime.hpp"
#include "Audio/audio_spatial_mgr.hpp"
#include "Audio/audio_helpers.hpp"
#include "IOManager/io_filesystem.hpp"
#include "IOManager/io_mgr.hpp"
#include "x_workers.hpp"
#include "x_log.hpp"

//==============================================================================
//  STATE AND DEBUG SETTINGS
//==============================================================================

static xbool s_bDisableAudio = FALSE;

#if defined( rbrannon ) || defined( mreed )
#define LOG_PLAY_SUCCESS "audio_mgr::Play(success)"
#define LOG_PLAY_CLIPPED "audio_mgr::Play(clipped)"
#define LOG_PLAY_FAILURE "audio_mgr::Play(failure)"
#define LOG_PLAY_WARNING "audio_mgr::Play(warning)"

voice*   g_DebugVoice   = NULL;
element* g_DebugElement = NULL;
channel* g_DebugChannel = NULL;
f32      g_DebugTime    = 0.0f;
#endif

//#define TRAP_ON_IDENTIFIER

#ifdef TRAP_ON_IDENTIFIER
xbool g_EnableIdentifierTrap = 1;
char  g_DebugIdentifier[64]  = "FORCE_FIELD_ACTIVE";
#endif

static xbool s_Initialized = FALSE;

//==============================================================================
//  RUNTIME STORAGE
//==============================================================================

union audio_runtime_storage
{
    u64   Align64;
    void* AlignPtr;
    byte  Bytes[sizeof( audio_runtime )];
};

static audio_runtime_storage s_AudioRuntimeStorage;
static xbool                 s_AudioRuntimeConstructed = FALSE;

#undef new

//==============================================================================
//  RUNTIME LIFETIME HELPERS
//==============================================================================

static
audio_runtime* ConstructAudioRuntime( audio_mgr& Audio )
{
    ASSERT( !s_AudioRuntimeConstructed );
    s_AudioRuntimeConstructed = TRUE;
    return new( (void*)s_AudioRuntimeStorage.Bytes ) audio_runtime( Audio );
}

//==============================================================================

static
void DestructAudioRuntime( audio_runtime* pRuntime )
{
    if( pRuntime )
    {
        pRuntime->~audio_runtime();
        s_AudioRuntimeConstructed = FALSE;
    }
}

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

audio_mgr g_AudioMgr;

//==============================================================================
//  CONSTRUCTION AND RUNTIME ACCESS
//==============================================================================

audio_mgr::audio_mgr( void ) :
    m_pRuntime( ConstructAudioRuntime( *this ) ),
    m_Language( XL_LANG_ENGLISH )
{
}

//==============================================================================

audio_mgr::~audio_mgr( void )
{
    DestructAudioRuntime( m_pRuntime );
    m_pRuntime = NULL;
}

//==============================================================================

audio_runtime& audio_mgr::Runtime( void )
{
    ASSERT( m_pRuntime );
    return *m_pRuntime;
}

//==============================================================================

const audio_runtime& audio_mgr::Runtime( void ) const
{
    ASSERT( m_pRuntime );
    return *m_pRuntime;
}

//==============================================================================
//  LIFECYCLE
//==============================================================================

void audio_mgr::Init( s32 MemSize )
{
    MEMORY_OWNER( "audio_mgr::Init" );

    // Error check.
    ASSERT( s_Initialized == FALSE );

    // Initialize spatial audio.
    Runtime().Spatial.Init();

    // Init the time.
    Runtime().Time = 0.0f;

    // Clear ducking level.
    Runtime().AudioDuckLevel = 0;

    // Initialize the channel manager.
    Runtime().Channels.Init( Runtime() );

    ASSERTS( ( MemSize & 2047 ) == 0, "Memory must be in 2K increments" );

    // Initialize the audio hardware.
    Runtime().Backend.Init( Runtime(), MemSize );

    // Initialize stream decoders.
    Runtime().Decoders.Init( Runtime() );

    // Initialize the stream manager.
    Runtime().Streams.Init( Runtime() );

    // Initialize the virtual voices.
    Runtime().Voices.Init( Runtime() );

    // Initialize the package registry.
    Runtime().Packages.Init( Runtime() );

    // Initialize descriptor runtime.
    Runtime().Descriptors.Init( Runtime(), Runtime().Packages );

    // Set flag.
    s_Initialized = TRUE;

    // Create the audio owner service.
    VERIFY( StartService() );
}

//==============================================================================

void audio_mgr::Kill( void )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    // Release all voices.
    ReleaseAll();

    // Nuke the packages while the audio owner service can still execute commands.
    UnloadAllPackages();

    // Stop the audio owner service before tearing down owned state.
    StopService();

    Runtime().Descriptors.Kill();
    Runtime().Packages.Kill();
    Runtime().Spatial.Kill();

    // Kill the virtual voices.
    Runtime().Voices.Kill();
    Runtime().DeferredVoices.Reset();

    // Kill the streams.
    Runtime().Streams.Kill();

    // Kill the stream decoders.
    Runtime().Decoders.Kill();

    // Kill the audio hardware.
    Runtime().Backend.Kill();

    // Kill the channel manager.
    Runtime().Channels.Kill();

    // Clear flag.
    s_Initialized = FALSE;
}

//==============================================================================

void audio_mgr::ResizeMemory( s32 NewSize )
{
    Runtime().SubmitS32CommandAsync( AUDIO_CMD_RESIZE_MEMORY, NewSize );
}

//==============================================================================
//  SERVICE THREAD AND TICKS
//==============================================================================

void audio_mgr::ServiceEntry( void* pData )
{
    ASSERT( pData );

    audio_mgr* pAudio = (audio_mgr*)pData;
    pAudio->ServiceLoop();
}

//==============================================================================

void audio_mgr::ServiceLoop( void )
{
    audio_command Command;

    Runtime().ServiceThreadId = x_GetThreadID();

    while( Runtime().ServiceRunning )
    {
        if( !Runtime().Commands.PopWait( Command ) )
        {
            continue;
        }

        // Keep voice updates running when producers continuously fill the queue.
        s32 CommandCount = 0;
        do
        {
            Runtime().ExecuteCommand( Command );
            CommandCount++;
        } while( CommandCount < AUDIO_MAX_COMMANDS && Runtime().Commands.Pop( Command ) );

        Runtime().Voices.UpdateCheckQueued();
        PeriodicUpdateNow();
        Runtime().Backend.Update();
        Runtime().Voices.PublishVoiceReadiness();
    }

    while( Runtime().Commands.Pop( Command ) )
    {
        Runtime().ExecuteCommand( Command );
    }

    Runtime().ServiceThreadId = -1;
}

//==============================================================================

xbool audio_mgr::StartService( void )
{
    ASSERT( Runtime().Service.IsNull() );

    Runtime().ServiceRunning  = TRUE;
    Runtime().ServiceThreadId = -1;

    if( !x_WorkerServiceStart( audio_mgr::ServiceEntry, this, "AudioMgr Service", Runtime().Service, X_WORKER_PRIORITY_HIGH ) )
    {
        Runtime().ServiceRunning = FALSE;
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void audio_mgr::StopService( void )
{
    if( Runtime().Service.IsNull() )
    {
        return;
    }

    Runtime().ServiceRunning = FALSE;
    Runtime().Commands.Wake();
    x_WorkerServiceWait( Runtime().Service );
    x_WorkerServiceRelease( Runtime().Service );
    Runtime().Service = x_worker_service( HNULL );
}

//==============================================================================

xbool audio_mgr::IsServiceThread( void ) const
{
    return ( Runtime().ServiceThreadId >= 0 ) && ( x_GetThreadID() == Runtime().ServiceThreadId );
}

//==============================================================================

void audio_mgr::Update( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_mgr::Update" );

    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    if( !x_isvalid( DeltaTime ) || ( DeltaTime <= 0.0f ) )
    {
        return;
    }

    DeltaTime = MIN( DeltaTime, 0.1f );

    Runtime().SubmitFloatCommandAsync( AUDIO_CMD_UPDATE, DeltaTime );
}

//==============================================================================

void audio_mgr::PeriodicUpdate( void )
{
    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_PERIODIC_UPDATE );
}

//==============================================================================

void audio_mgr::UpdateNow( f32 DeltaTime )
{
    Runtime().Time += DeltaTime;

#if defined( rbrannon )
    void AudioThrashUpdate( void );
    AudioThrashUpdate();
#endif

    Runtime().Channels.Update();
    Runtime().Voices.Update( DeltaTime );
    Runtime().Backend.SetDoBackendUpdate();
}

//==============================================================================

void audio_mgr::PeriodicUpdateNow( void )
{
    Runtime().Streams.Update();
}

//==============================================================================
//  PACKAGES
//==============================================================================

void audio_mgr::SetLanguage( x_language Language )
{
    ASSERT( ( Language >= XL_LANG_ENGLISH ) && ( Language < XL_NUM_LANGUAGES ) );
    m_Language = Language;
}

//==============================================================================

x_language audio_mgr::GetLanguage( void ) const
{
    return m_Language;
}

//==============================================================================
// Returns a localized filename for the current langauge.
// The name will change ONLY if the file starts with "DX_" which indicates
// that the file is a dialogue. Note also, that this is different from
// the naming for text files which are prepended.

const char* audio_mgr::GetLocalizedName( const char* pFileName ) const
{
    char Drive[X_MAX_DRIVE], Path[X_MAX_PATH], FName[X_MAX_FNAME], Ext[X_MAX_EXT];

    ASSERT( pFileName != NULL );
    x_splitpath( pFileName, Drive, Path, FName, Ext );

    // check that this is a dialog file
    if( ( FName[0] == 'D' ) && ( FName[1] == 'X' ) && ( FName[2] == '_' ) &&
        // if this is English, leave the file name "unmangled". This should probably be fixed in the future.
        ( m_Language != XL_LANG_ENGLISH ) )
    {
        static char Name[X_MAX_PATH];
        x_sprintf( Name,
                   "%s%s%s_%s%s",
                   Drive,
                   Path,
                   FName,
                   x_GetLocaleString( m_Language ),
                   Ext );

#if defined( X_DEBUG ) && defined( ctetrick )
        LOG_MESSAGE( "audio_mgr::GetLocalizedName", "voice file (%s) returned as (%s)", pFileName, Name );
#endif

        X_FILE* pLocalizedFile = x_fopen( Name, "rb" );

        if( pLocalizedFile )
        {
            x_fclose( pLocalizedFile );
            return (const char*)Name;
        }

        LOG_WARNING( "audio_mgr::GetLocalizedName",
                     "Missing localized audio package [%s], using English [%s].",
                     Name,
                     pFileName );
        return pFileName;
    }
    else
    {
        return pFileName;
    }
}

//==============================================================================

xbool audio_mgr::LoadPackage( const char* pFilename )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_mgr::LoadPackage()" );
    MEMORY_OWNER( "audio_mgr::LoadPackage()" );

#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif
    audio_command_result Result;

    return Runtime().SubmitTextCommandSync( AUDIO_CMD_LOAD_PACKAGE, pFilename, Result ) && Result.Bool;
}

//==============================================================================

xbool audio_mgr::UnloadPackage( const char* pFilename )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    return Runtime().SubmitTextCommandSync( AUDIO_CMD_UNLOAD_PACKAGE, pFilename, Result ) && Result.Bool;
}

//==============================================================================

xbool audio_mgr::IsPackageLoaded( const char* pFilename )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().Packages.IsPackageLoaded( pFilename );
}

//==============================================================================

xbool audio_mgr::LoadPackageStrings( const char* pFilename, xarray<xstring>& Strings )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    char LocalizedName[X_MAX_PATH];

    x_strcpy( LocalizedName, GetLocalizedName( pFilename ) );

    return Runtime().Packages.LoadPackageStrings( pFilename, LocalizedName, Strings );
}

//==============================================================================

void audio_mgr::UnloadAllPackages( void )
{
    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_UNLOAD_ALL_PACKAGES );
}

//==============================================================================

void audio_mgr::GetLoadedPackages( xarray<xstring>& Packages )
{
    ASSERT( s_Initialized );

    Runtime().Packages.GetLoadedPackages( Packages );
}

//==============================================================================

void audio_mgr::GetLoadedPackageLookupNames( xarray<xstring>& Packages )
{
    ASSERT( s_Initialized );

    Runtime().Packages.GetLoadedPackageLookupNames( Packages );
}

//==============================================================================

void audio_mgr::DisplayPackages( void )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    ASSERT( s_Initialized );
    Runtime().Packages.DisplayPackages();
}

//==============================================================================

s32 audio_mgr::GetPackageARAM( const char* pName )
{
    ASSERT( s_Initialized );

    return Runtime().Packages.GetPackageARAM( pName );
}

//==============================================================================

char* audio_mgr::GetMusicType( const char* pFilename )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return NULL;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().Packages.GetMusicType( pFilename );
}

//==============================================================================

s32 audio_mgr::GetMusicIntensity( const char* pFilename, music_intensity*& Intensity )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        Intensity = NULL;
        return 0;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().Packages.GetMusicIntensity( pFilename, Intensity );
}

//==============================================================================
//  DESCRIPTORS
//==============================================================================

u16* audio_mgr::FindDescriptorByName( const char* pName, audio_package** pPackageResult, char*& DescriptorName )
{
    return Runtime().Packages.FindDescriptorByName( pName, pPackageResult, DescriptorName );
}

//==============================================================================

xbool audio_mgr::IsValidDescriptor( const char* pName )
{
    return Runtime().Packages.IsValidDescriptor( pName );
}

//==============================================================================

void audio_mgr::ReMergeIdentifierTables( void )
{
    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_REMERGE_IDENTIFIER_TABLES );
}

//==============================================================================

s32 audio_mgr::IsCold( char* pIdentifier )
{
    return Runtime().Descriptors.IsCold( pIdentifier );
}

//==============================================================================

u32 audio_mgr::GetUserData( const char* pIdentifier )
{
    return Runtime().Descriptors.GetUserData( pIdentifier );
}

//==============================================================================

s32 audio_mgr::GetPriority( const char* pIdentifier )
{
    return Runtime().Descriptors.GetPriority( pIdentifier );
}

//==============================================================================

f32 audio_mgr::GetFarFalloff( const char* pIdentifier )
{
    return Runtime().Descriptors.GetFarFalloff( pIdentifier );
}

//==============================================================================

f32 audio_mgr::GetNearFalloff( const char* pIdentifier )
{
    return Runtime().Descriptors.GetNearFalloff( pIdentifier );
}

//==============================================================================

f32 audio_mgr::GetLengthSeconds( const char* pIdentifier )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif
    return Runtime().Descriptors.GetLengthSeconds( pIdentifier );
}

//==============================================================================
//  VOICE HANDLES
//==============================================================================

voice* audio_mgr::IdToVoice( voice_id VoiceID )
{
    voice* pVoice   = Runtime().Voices.GetVoiceBuffer();
    s32    Index    = ( ( VoiceID & 0xffff ) - 1 );
    u32    Sequence = ( ( VoiceID >> 16 ) & 0x0000ffff );

    // Error check.
    if( ( Index < 0 ) || ( Index >= Runtime().Voices.GetNumVoices() ) )
    {
        return NULL;
    }

    // Does the sequence match?
    if( ( ( pVoice + Index )->Sequence & 0x0000ffff ) == Sequence )
    {
        return pVoice + Index;
    }
    else
    {
        return NULL;
    }
}

//==============================================================================

voice_id audio_mgr::VoiceToId( voice* pVoice )
{
    s32 Index = pVoice - Runtime().Voices.GetVoiceBuffer();

    // Error check.
    ASSERT( Runtime().Voices.IsValidVoice( pVoice ) );

    // Encode index and sequence.
    return ( ( ( Index + 1 ) & 0xffff ) + ( pVoice->Sequence << 16 ) );
}

//==============================================================================
//  PLAYBACK
//==============================================================================

voice_id audio_mgr::Play( const char* pIdentifier, xbool AutoStart )
{
    vector3  Dummy;
    vector3& DummyRef = Dummy;

    return PlayInternal( pIdentifier, AutoStart, DummyRef, -1, FALSE, FALSE );
}

//==============================================================================

voice_id audio_mgr::Play( const char* pIdentifier, const vector3& Position, s32 ZoneID, xbool AutoStart )
{
    return PlayInternal( pIdentifier, AutoStart, Position, ZoneID, TRUE, FALSE );
}

//==============================================================================

voice_id audio_mgr::Play( const char*    pIdentifier,
                          const vector3& Position,
                          s32            ZoneID,
                          xbool          AutoStart,
                          xbool          VolumeClip )
{
    return PlayInternal( pIdentifier, AutoStart, Position, ZoneID, TRUE, VolumeClip );
}

//==============================================================================

voice_id audio_mgr::PlayVolumeClipped( const char* pIdentifier, const vector3& Position, s32 ZoneID, xbool AutoStart )
{
    return PlayInternal( pIdentifier, AutoStart, Position, ZoneID, TRUE, TRUE );
}

//==============================================================================

voice_id audio_mgr::PlayInternal( const char*    pIdentifier,
                                  xbool          AutoStart,
                                  const vector3& Position,
                                  s32            ZoneID,
                                  xbool          IsPositional,
                                  xbool          bVolumeClip,
                                  xbool          UseReservedStream,
                                  voice_id       ReplaceVoiceID )
{
    return Runtime().SubmitPlayCommand( pIdentifier,
                                        AutoStart,
                                        Position,
                                        ZoneID,
                                        IsPositional,
                                        bVolumeClip,
                                        UseReservedStream,
                                        ReplaceVoiceID );
}

//==============================================================================

voice_id audio_mgr::PlayInternalNow( const char*    pIdentifier,
                                     xbool          AutoStart,
                                     const vector3& Position,
                                     s32            ZoneID,
                                     xbool          IsPositional,
                                     xbool          bVolumeClip,
                                     xbool          UseReservedStream )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0;
    }
#else
    ASSERT( s_Initialized );
#endif

    if( s_bDisableAudio )
    {
        return 0;
    }

#ifdef TRAP_ON_IDENTIFIER
    if( g_EnableIdentifierTrap && ( x_stricmp( pIdentifier, g_DebugIdentifier ) == 0 ) )
    {
        BREAK;
    }
#endif

    audio_package* pPackage;
    u16*           pDescriptor;
    f32            PositionalVolume = 1.0f;
    f32            UserVolume       = 1.0f;
    char*          DescriptorName   = NULL;

    // Find the descriptor by name.
    pDescriptor = FindDescriptorByName( pIdentifier, &pPackage, DescriptorName );

    // Did not find it?
    if( pDescriptor )
    {
        uncompressed_parameters Params;
        voice*                  pVoice;
        f32                     AbsoluteVolume;

        // Decode the voices parameters, this is required to determine the priority and volume.
        Runtime().Descriptors.GetVoiceParameters( &Params, pDescriptor, pPackage, (char*)pIdentifier );

        // Is it positional?
        if( IsPositional )
        {
            // Calculate the falloffs.
            f32 Near = Params.NearFalloff * pPackage->GetComputedNearFalloff();
            f32 Far  = Params.FarFalloff * pPackage->GetComputedFarFalloff();

            // Calculate the 3d volume.
            Calculate3dVolume( Near, Far, Params.RolloffCurve, Position, ZoneID, PositionalVolume );

            // Is this sound volume clipped?
            if( bVolumeClip )
            {
                // Is it *REALLY* quiet...
                if( PositionalVolume <= 0.05f )
                {
// I'm sorry cap'n but I canna play this...the dilithium crystals are cracked!
#ifdef LOG_PLAY_CLIPPED
                    LOG_MESSAGE( LOG_PLAY_CLIPPED, "'%s' was VOLUME CLIPPED!", pIdentifier );
#endif // LOG_PLAY_CLIPPED

                    return 0;
                }
            }
        }

        // Calculate voices volume.
        AbsoluteVolume = PositionalVolume * UserVolume * Params.Volume * pPackage->GetComputedVolume();

        // TODO: Put in master fader calculation - apply it to AbsoluteVolume.

        // Attempt to acquire a voice.
        pVoice = Runtime().Voices.AcquireVoice( Params.Priority, AbsoluteVolume );

        if( pVoice )
        {
#if defined( rbrannon ) && defined( TRAP_ON_IDENTIFIER )
            if( g_EnableIdentifierTrap && ( x_stricmp( pIdentifier, g_DebugIdentifier ) == 0 ) )
            {
                if( g_DebugVoice == NULL )
                {
                    g_EnableIdentifierTrap = 0;
                    g_DebugVoice           = pVoice;
                    LOG_MESSAGE( "AudioDebug(audio_mgr::Play)",
                                 "Trapped: %s, pVoice: %08x",
                                 pIdentifier,
                                 pVoice );
                }
            }
#endif
#ifdef LOG_PLAY_SUCCESS
            LOG_MESSAGE( LOG_PLAY_SUCCESS, "pVoice: 0x%08x [Id:%08x] '%s'", pVoice, VoiceToId( pVoice ), pIdentifier );
#endif //LOG_PLAY_SUCCESS

            // Save descriptor name
            pVoice->pDescriptorName = DescriptorName;

            // Now that we have a voice, copy the parameters.
            pVoice->Params = Params;

            // Default is no ear specified!
            pVoice->EarID = 0;

            // Positional sound?
            if( IsPositional )
            {
                // Set the voices position.
                pVoice->Position = Position;
                pVoice->ZoneID   = ZoneID;
            }
            else
            {
                // Not positional so use the 2d pan.
                Runtime().Spatial.Calculate2dPan( pVoice->Params.Pan2d, pVoice->Params.Pan3d );
            }

            // If pan was specified, then it cannot be changed.
            if( pVoice->Params.Bits & PAN_2D && !IsPositional )
            {
                pVoice->IsPanChangeable = FALSE;
            }
            else
            {
                pVoice->IsPanChangeable = TRUE;
            }

            // Force voice positional flag until parameters have been set.
            pVoice->IsPositional = TRUE;
            Runtime().Voices.SetVoiceUserFalloff( pVoice, 1.0f, 1.0f );
            Runtime().Voices.SetVoiceUserDiffuse( pVoice, 1.0f, 1.0f );
            pVoice->IsPositional = IsPositional;

            // Set the voices parameters.
            Runtime().Voices.SetVoiceUserVolume( pVoice, UserVolume );
            Runtime().Voices.SetVoiceUserPitch( pVoice, 1.0f );
            Runtime().Voices.SetVoiceUserEffectSend( pVoice, 1.0f );

            // Ok, now initialize the voice.
            Runtime().Voices.InitSingleVoice( pVoice, pPackage );
            pVoice->UseReservedStream = UseReservedStream;

            // Set the voices descriptor.
            pVoice->pDescriptor = pDescriptor;

            // Append the descriptor to the voices element list.
            if( Runtime().Descriptors.AppendDescriptor( 0.0f, pDescriptor, pVoice, pPackage ) )
            {
                // Start the voice.
                if( AutoStart )
                {
                    Runtime().Voices.StartVoice( pVoice );
                }

                voice_id ResultID = VoiceToId( pVoice );
                Runtime().Voices.PublishVoiceId( pVoice, ResultID );

                // Its all good!
                return ( ResultID );
            }
            else
            {
                // Had problems, no elements in voice, so release the voice...
                Runtime().Voices.ReleaseVoice( pVoice, 0.0f );
            }
        }
        else
        {
#ifdef LOG_PLAY_FAILURE
            LOG_MESSAGE( LOG_PLAY_FAILURE, "'%s' could not acquire voice!", pIdentifier );
#endif // LOG_PLAY_FAILURE
        }
    }
    else
    {
#ifdef LOG_PLAY_WARNING
        LOG_WARNING( LOG_PLAY_WARNING, "'%s' identifier NOT FOUND!!!", pIdentifier );
#endif // LOG_PLAY_WARNING
    }

    return 0;
}

//==============================================================================
//  VOICE COMMANDS
//==============================================================================

xbool audio_mgr::Start( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().SubmitVoiceCommandAsync( AUDIO_CMD_START, VoiceID );
}

//==============================================================================

xbool audio_mgr::Segue( voice_id VoiceID, voice_id VoiceToQ )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command Command;
    Command.Init( AUDIO_CMD_SEGUE );
    Command.VoiceID  = VoiceID;
    Command.VoiceID2 = VoiceToQ;
    return Runtime().SubmitCommandAsync( Command );
}

//==============================================================================

void audio_mgr::Pause( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    Runtime().SubmitVoiceCommandAsync( AUDIO_CMD_PAUSE, VoiceID );
}

//==============================================================================

void audio_mgr::PauseAll( void )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_PAUSE_ALL );
}

//==============================================================================

void audio_mgr::Resume( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    Runtime().SubmitVoiceCommandAsync( AUDIO_CMD_RESUME, VoiceID );
}

//==============================================================================

void audio_mgr::ResumeAll( void )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_RESUME_ALL );
}

//==============================================================================

void audio_mgr::Release( voice_id VoiceID, f32 Time )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    Runtime().SubmitVoiceFloatCommandAsync( AUDIO_CMD_RELEASE, VoiceID, Time );
}

//==============================================================================

void audio_mgr::ReleaseAll( void )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_RELEASE_ALL );
}

//==============================================================================

xbool audio_mgr::SetReleaseTime( voice_id VoiceID, f32 Time )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().SubmitVoiceFloatCommandAsync( AUDIO_CMD_SET_RELEASE_TIME, VoiceID, Time );
}

//==============================================================================

xbool audio_mgr::IsReleasing( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    return Runtime().SubmitVoiceCommandSync( AUDIO_CMD_IS_RELEASING, VoiceID, Result ) && Result.Bool;
}

//==============================================================================

voice_id audio_mgr::PlayConfigured( const char*    pIdentifier,
                                    const vector3& Position,
                                    s32            ZoneID,
                                    xbool          IsPositional,
                                    f32            Volume,
                                    f32            FarClipScale )
{
    if( !s_Initialized )
    {
        return 0;
    }

    audio_play_settings Settings;
    Settings.Volume       = Volume;
    Settings.FarClipScale = FarClipScale;
    return Runtime().SubmitPlayCommand( pIdentifier, TRUE, Position, IsPositional ? ZoneID : -1,
                                        IsPositional, IsPositional, FALSE, 0, &Settings );
}

//==============================================================================
//  CACHED VOICE STATE AND REQUESTS
//==============================================================================

f32 audio_mgr::GetCurrentPlayTimeCached( voice_id VoiceID )
{
    audio_voice_state State;
    GetVoiceStateCached( VoiceID, State );
    return State.PlayTime;
}

//==============================================================================

f32 audio_mgr::GetLengthSecondsCached( voice_id VoiceID )
{
    audio_voice_state State;
    GetVoiceStateCached( VoiceID, State );
    return State.Length;
}

//==============================================================================

audio_request* audio_mgr::RequestCreateEar( const matrix4& W2V,
                                            const vector3& Position,
                                            s32            ZoneID,
                                            f32            Volume )
{
    return s_Initialized ? Runtime().SubmitCreateEarAsync( W2V, Position, ZoneID, Volume ) : NULL;
}

//==============================================================================

audio_request* audio_mgr::RequestBreakPoints( voice_id VoiceID )
{
    return s_Initialized && VoiceID ? Runtime().SubmitBreakPointsAsync( VoiceID ) : NULL;
}

//==============================================================================

xbool audio_mgr::GetVoiceDescriptorCached( voice_id VoiceID, char* pName, s32 NameSize )
{
    if( !pName || NameSize <= 0 )
    {
        return FALSE;
    }

    pName[0] = 0;

    if( !s_Initialized )
    {
        return FALSE;
    }

    xbool Pending;
    VoiceID = Runtime().DeferredVoices.Resolve( VoiceID, Pending );
    return !Pending && Runtime().Voices.GetVoiceDescriptorCached( VoiceID, pName, NameSize );
}

//==============================================================================

f32 audio_mgr::GetVolumeCached( voice_id VoiceID )
{
    if( !s_Initialized )
    {
        return 0.0f;
    }

    xbool Pending;
    VoiceID = Runtime().DeferredVoices.Resolve( VoiceID, Pending );
    return Pending ? 0.0f : Runtime().Voices.GetVolumeCached( VoiceID );
}

//==============================================================================

s32 audio_mgr::GetPriorityCached( voice_id VoiceID )
{
    if( !s_Initialized )
    {
        return 0;
    }

    xbool Pending;
    VoiceID = Runtime().DeferredVoices.Resolve( VoiceID, Pending );
    return Pending ? 0 : Runtime().Voices.GetPriorityCached( VoiceID );
}

//==============================================================================

xbool audio_mgr::IsVoicePending( voice_id VoiceID ) const
{
    xbool Pending;
    Runtime().DeferredVoices.Resolve( VoiceID, Pending );
    return Pending;
}

//==============================================================================

xbool audio_mgr::GetVoiceStateCached( voice_id VoiceID, audio_voice_state& State )
{
    x_memset( &State, 0, sizeof( State ) );

    if( !s_Initialized || !VoiceID )
    {
        return FALSE;
    }

    VoiceID = Runtime().DeferredVoices.Resolve( VoiceID, State.Pending );

    if( State.Pending )
    {
        State.Valid = TRUE;
        return TRUE;
    }

    return Runtime().Voices.GetVoiceStateCached( VoiceID, State );
}

//==============================================================================

xbool audio_mgr::GetVoiceState( voice_id VoiceID, audio_voice_state& State )
{
    x_memset( &State, 0, sizeof( State ) );

#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    if( VoiceID == 0 )
    {
        return FALSE;
    }

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_VOICE_STATE, VoiceID, Result ) )
    {
        return FALSE;
    }

    State = Result.VoiceState;
    return TRUE;
}

//==============================================================================

xbool audio_mgr::IsValidVoiceId( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    if( VoiceID == 0 )
    {
        return FALSE;
    }

    audio_command_result Result;

    return Runtime().SubmitVoiceCommandSync( AUDIO_CMD_IS_VALID_VOICE_ID, VoiceID, Result ) && Result.Bool;
}

//==============================================================================

xbool audio_mgr::IsValidVoiceIdCached( voice_id VoiceID )
{
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    xbool Pending;
    VoiceID = Runtime().DeferredVoices.Resolve( VoiceID, Pending );
    return Pending || Runtime().Voices.IsValidVoiceIdCached( VoiceID );
}

//==============================================================================

xbool audio_mgr::IsVoiceReadyCached( voice_id VoiceID )
{
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    xbool Pending;
    VoiceID = Runtime().DeferredVoices.Resolve( VoiceID, Pending );
    return !Pending && Runtime().Voices.IsVoiceReadyCached( VoiceID );
}

//==============================================================================

xbool audio_mgr::IsVoiceReady( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    return Runtime().SubmitVoiceCommandSync( AUDIO_CMD_IS_VOICE_READY, VoiceID, Result ) && Result.Bool;
}

//==============================================================================

void audio_mgr::SetPitchLock( voice_id VoiceID,
                              xbool    bPitchLock )
{
    Runtime().SubmitVoiceBoolCommandAsync( AUDIO_CMD_SET_PITCH_LOCK, VoiceID, bPitchLock );
}

//==============================================================================
//  VOICE QUERIES AND PROPERTIES
//==============================================================================

f32 audio_mgr::GetLengthSeconds( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_LENGTH_SECONDS_VOICE, VoiceID, Result ) )
    {
        return 0.0f;
    }

    return Result.F32;
}

//==============================================================================

const char* audio_mgr::GetVoiceDescriptor( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return "NULL";
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_VOICE_DESCRIPTOR, VoiceID, Result ) || !Result.pText )
    {
        return "NULL";
    }

    return Result.pText;
}

//==============================================================================

f32 audio_mgr::GetVolume( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_VOLUME, VoiceID, Result ) )
    {
        return 0.0f;
    }

    return Result.F32;
}

//==============================================================================

xbool audio_mgr::SetVolume( voice_id VoiceID, f32 Volume )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().SubmitVoiceFloatCommandAsync( AUDIO_CMD_SET_VOLUME, VoiceID, Volume );
}

//==============================================================================

f32 audio_mgr::GetPan( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_PAN, VoiceID, Result ) )
    {
        return 0.0f;
    }

    return Result.F32;
}

//==============================================================================

xbool audio_mgr::SetPan( voice_id VoiceID, f32 Pan )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().SubmitVoiceFloatCommandAsync( AUDIO_CMD_SET_PAN, VoiceID, Pan );
}

//==============================================================================

f32 audio_mgr::GetPitch( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_PITCH, VoiceID, Result ) )
    {
        return 0.0f;
    }

    return Result.F32;
}

//==============================================================================

xbool audio_mgr::SetPitch( voice_id VoiceID, f32 Pitch )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    if( Pitch < 0.99f )
    {
        LOG_MESSAGE( "audio_mgr::SetPitch", "VoiceID: %08x, Pitch: %f", VoiceID, Pitch );
    }

    return Runtime().SubmitVoiceFloatCommandAsync( AUDIO_CMD_SET_PITCH, VoiceID, Pitch );
}

//==============================================================================

xbool audio_mgr::GetPosition( voice_id VoiceID, vector3& Position, s32& ZoneID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().SubmitGetPositionCommandSync( VoiceID, Position, ZoneID );
}

//==============================================================================

xbool audio_mgr::SetPosition( voice_id VoiceID, const vector3& Position, s32 ZoneID )
{
    // Error check.
    ASSERT( s_Initialized );

    return Runtime().SubmitSetPositionCommandAsync( VoiceID, Position, ZoneID );
}

//==============================================================================

xbool audio_mgr::SetFalloff( voice_id VoiceID, f32 Near, f32 Far )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().SubmitVoiceFalloffCommandAsync( VoiceID, Near, Far );
}

//==============================================================================

f32 audio_mgr::GetEffectSend( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_EFFECT_SEND, VoiceID, Result ) )
    {
        return 0.0f;
    }

    return Result.F32;
}

//==============================================================================

xbool audio_mgr::SetEffectSend( voice_id VoiceID, f32 EffectSend )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    return Runtime().SubmitVoiceFloatCommandAsync( AUDIO_CMD_SET_EFFECT_SEND, VoiceID, EffectSend );
}

//==============================================================================

xbool audio_mgr::HasLipSync( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return FALSE;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    return Runtime().SubmitVoiceCommandSync( AUDIO_CMD_HAS_LIP_SYNC, VoiceID, Result ) && Result.Bool;
}

//==============================================================================

f32 audio_mgr::GetLipSync( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_LIP_SYNC, VoiceID, Result ) )
    {
        return 0.0f;
    }

    return Result.F32;
}

//==============================================================================

s32 audio_mgr::GetBreakPoints( voice_id VoiceID, f32*& BreakPoints )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_BREAK_POINTS, VoiceID, Result ) )
    {
        return 0;
    }

    BreakPoints = Result.pF32;
    return Result.S32;
}

//==============================================================================

xbool audio_mgr::GetIsReady( voice_id VoiceID )
{
    audio_voice_state State;
    GetVoiceStateCached( VoiceID, State );
    return State.Ready;
}

//==============================================================================

f32 audio_mgr::GetCurrentPlayTime( voice_id VoiceID )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return 0.0f;
    }
#else
    ASSERT( s_Initialized );
#endif

    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_CURRENT_PLAY_TIME, VoiceID, Result ) )
    {
        return 0.0f;
    }

    return Result.F32;
}

//==============================================================================

s32 audio_mgr::GetPriority( voice_id VoiceID )
{
    audio_command_result Result;

    if( !Runtime().SubmitVoiceCommandSync( AUDIO_CMD_GET_PRIORITY, VoiceID, Result ) )
    {
        return 0;
    }

    return Result.S32;
}

//==============================================================================

void audio_mgr::SetVoiceEar( voice_id VoiceID, ear_id EarID )
{
    Runtime().SubmitVoiceEarCommandAsync( VoiceID, EarID );
}

//==============================================================================
//  SPATIAL AND EARS
//==============================================================================

void audio_mgr::SetClip( f32 NearClip, f32 FarClip )
{
    Runtime().SubmitClipCommandAsync( NearClip, FarClip );
}

//==============================================================================

void audio_mgr::GetClip( f32& NearClip, f32& FarClip )
{
    Runtime().Spatial.GetClip( NearClip, FarClip );
}

//==============================================================================

void audio_mgr::Calculate3dVolume( f32            NearClip,
                                   f32            FarClip,
                                   s32            VolumeRolloff,
                                   const vector3& WorldPosition,
                                   s32            ZoneID,
                                   f32&           Volume )
{
    Runtime().Spatial.Calculate3dVolume( NearClip, FarClip, VolumeRolloff, WorldPosition, ZoneID, Volume );
}

//==============================================================================

ear_id audio_mgr::CreateEar( void )
{
    audio_command_result Result;

    if( !Runtime().SubmitSimpleCommandSync( AUDIO_CMD_CREATE_EAR, Result ) )
    {
        return 0;
    }

    return Result.S32;
}

//==============================================================================

void audio_mgr::DestroyEar( ear_id EarID )
{
    Runtime().SubmitEarCommandAsync( AUDIO_CMD_DESTROY_EAR, EarID );
}

//==============================================================================

void audio_mgr::GetEar( ear_id EarID, matrix4& W2V, vector3& Position, s32& ZoneID, f32& Volume )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif

    Runtime().Spatial.GetEar( EarID, W2V, Position, ZoneID, Volume );
}

//==============================================================================

void audio_mgr::SetEar( ear_id EarID, const matrix4& W2V, const vector3& Position, s32 ZoneID, f32 Volume )
{
    // Error check.
#ifndef AUDIO_ENABLE
    if( !s_Initialized )
    {
        return;
    }
#else
    ASSERT( s_Initialized );
#endif
    Runtime().SubmitSetEarCommandAsync( EarID, W2V, Position, ZoneID, Volume );
}

//==============================================================================

void audio_mgr::UpdateEarZoneVolume( ear_id EarID, s32 ZoneID, f32 Volume )
{
    ASSERT( s_Initialized );
    Runtime().SubmitEarZoneVolumeCommandAsync( EarID, ZoneID, Volume );
}

//==============================================================================

void audio_mgr::UpdateEarZoneVolumes( ear_id EarID, f32* pVolumes )
{
    ASSERT( s_Initialized );
    Runtime().SubmitEarZoneVolumesCommandAsync( EarID, pVolumes );
}

//==============================================================================

xbool audio_mgr::IsEarInBox( const bbox& Box )
{
    return Runtime().Spatial.IsEarInBox( Box );
}

//==============================================================================

ear_id audio_mgr::GetFirstEar( void )
{
    audio_command_result Result;

    if( !Runtime().SubmitSimpleCommandSync( AUDIO_CMD_GET_FIRST_EAR, Result ) )
    {
        return 0;
    }

    return Result.S32;
}

//==============================================================================

ear_id audio_mgr::GetNextEar( void )
{
    audio_command_result Result;

    if( !Runtime().SubmitSimpleCommandSync( AUDIO_CMD_GET_NEXT_EAR, Result ) )
    {
        return 0;
    }

    return Result.S32;
}

//==============================================================================

void audio_mgr::ResetCurrentEar( void )
{
    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_RESET_CURRENT_EAR );
}

//==============================================================================

void audio_mgr::SetSpeakerConfig( s32 SpeakerConfig )
{
    Runtime().SubmitS32CommandAsync( AUDIO_CMD_SET_SPEAKER_CONFIG, SpeakerConfig );
}

//==============================================================================

s32 audio_mgr::GetSpeakerConfig( void )
{
    return Runtime().Spatial.GetSpeakerConfig();
}

//==============================================================================
//  GLOBAL MIX SETTINGS
//==============================================================================

void audio_mgr::SetMasterVolume( f32 Volume )
{
    Runtime().SubmitFloatCommandAsync( AUDIO_CMD_SET_MASTER_VOLUME, Volume );
}

//==============================================================================

void audio_mgr::SetMusicVolume( f32 Volume )
{
    Runtime().SubmitFloatCommandAsync( AUDIO_CMD_SET_MUSIC_VOLUME, Volume );
}

//==============================================================================

void audio_mgr::SetSFXVolume( f32 Volume )
{
    Runtime().SubmitFloatCommandAsync( AUDIO_CMD_SET_SFX_VOLUME, Volume );
}

//==============================================================================

void audio_mgr::SetVoiceVolume( f32 Volume )
{
    Runtime().SubmitFloatCommandAsync( AUDIO_CMD_SET_VOICE_VOLUME, Volume );
}

//==============================================================================

void audio_mgr::SetPitchFactor( f32 PitchFactor )
{
    Runtime().SubmitFloatCommandAsync( AUDIO_CMD_SET_PITCH_FACTOR, PitchFactor );
}

//==============================================================================

void audio_mgr::EnableAudioDucking( void )
{
    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_ENABLE_DUCKING );
}

//==============================================================================

void audio_mgr::DisableAudioDucking( void )
{
    Runtime().SubmitSimpleCommandAsync( AUDIO_CMD_DISABLE_DUCKING );
}

//==============================================================================

xbool audio_mgr::IsAudioDuckingEnabled( void )
{
    return ( Runtime().AudioDuckLevel > 0 );
}

//==============================================================================

s32 audio_mgr::GetAudioLevel( void )
{
    return Runtime().Backend.GetAudioLevel();
}

//==============================================================================

f32 audio_mgr::GetAudioTime( void )
{
    return Runtime().Time;
}

//==============================================================================
//  STREAM INTEGRATION
//==============================================================================

void audio_mgr::QueueStreamReadComplete( io_request* pRequest )
{
    Runtime().SubmitIoRequestCommandAsync( AUDIO_CMD_STREAM_READ_COMPLETE, pRequest );
}

//==============================================================================

void audio_mgr::QueueStreamWarmComplete( io_request* pRequest )
{
    Runtime().SubmitIoRequestCommandAsync( AUDIO_CMD_STREAM_WARM_COMPLETE, pRequest );
}

//==============================================================================

xbool audio_mgr::ReserveStreams( s32 nStreams )
{
    audio_command_result Result;

    return Runtime().SubmitS32CommandSync( AUDIO_CMD_RESERVE_STREAMS, nStreams, Result ) && Result.Bool;
}

//==============================================================================

xbool audio_mgr::UnReserveStreams( s32 nStreams )
{
    audio_command_result Result;

    return Runtime().SubmitS32CommandSync( AUDIO_CMD_UNRESERVE_STREAMS, nStreams, Result ) && Result.Bool;
}

//==============================================================================
//  DEBUG STATISTICS
//==============================================================================

xbool audio_mgr::GetDebugStats( audio_debug_stats& Stats )
{
    x_memset( &Stats, 0, sizeof( Stats ) );

    if( !m_pRuntime )
    {
        return FALSE;
    }

    audio_command_result Result;

    if( !Runtime().SubmitSimpleCommandSync( AUDIO_CMD_GET_DEBUG_STATS, Result ) || !Result.Bool )
    {
        return FALSE;
    }

    Stats = Result.DebugStats;
    return TRUE;
}
