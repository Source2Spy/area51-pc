//==============================================================================
//
//  audio_stream_mgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_stream_mgr.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_package.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "Audio/audio_stream_decoder_factory.hpp"
#include "Audio/audio_stream_decoder.hpp"
#include "Audio/audio_stream_runtime.hpp"
#include "x_log.hpp"
#include "x_bytestream.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#if defined(rbrannon)
#define LOG_AUDIO_STREAM_ACQUIRE_SUCCESS "stream_mgr::Acquire"
#define LOG_AUDIO_STREAM_ACQUIRE_FAIL    "stream_mgr::Acquire"
#define LOG_AUDIO_STREAM_UPDATE          "stream_mgr::Update"
#define LOG_AUDIO_STREAM_RELEASE         "stream_mgr::ReleaseStream"
#endif

#define VALID_STREAM( pStream ) ((pStream >= &m_AudioStreams[0]) && (pStream <= &m_AudioStreams[MAX_AUDIO_STREAMS-1]))

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_stream_mgr::audio_stream_mgr( void )
{
    // Nuke the audio streams.
    x_memset( m_AudioStreams, 0, sizeof( m_AudioStreams ) );
    m_pRuntime        = NULL;
    m_ARAM             = 0;
    m_nReservedStreams = 0;
}

//==============================================================================

audio_stream_mgr::~audio_stream_mgr( void )
{
}

//==============================================================================

void audio_stream_mgr::Init( audio_runtime& AudioRuntime )
{
    s32 nBytes;
    uaddr BaseRam;
    s32 i;

    // Nuke 'em.
    x_memset( m_AudioStreams, 0, sizeof( m_AudioStreams ) );
    m_pRuntime = &AudioRuntime;

    Runtime().StreamRuntime.Init( Runtime() );

    // Allocate the aram
    nBytes = MAX_AUDIO_STREAMS * MAX_STREAM_CHANNELS * STREAM_BUFFER_SIZE * 2;
    m_ARAM = (uaddr)Runtime().Backend.AllocAudioRam( nBytes );

    // Asign aram to the stream buffers
    BaseRam = m_ARAM;

    for( i=0 ; i<MAX_AUDIO_STREAMS ; i++ )
    {
        Runtime().StreamRuntime.RegisterStream( i, &m_AudioStreams[i] );

        // Stream has an io_request for a member.
        m_AudioStreams[ i ].pIoRequest = new io_request[1];

        // Asign ARAM to each stream channel.
        for( s32 j=0 ; j<MAX_STREAM_CHANNELS ; j++ )
        {
            // Asign both buffers.
            m_AudioStreams[ i ].ARAM[ j ][ 0 ] = BaseRam;
            BaseRam += STREAM_BUFFER_SIZE;
            m_AudioStreams[ i ].ARAM[ j ][ 1 ] = BaseRam;
            BaseRam += STREAM_BUFFER_SIZE;
        }
    }
}

//==============================================================================

void audio_stream_mgr::Kill( void )
{
    // For each stream...
    for( s32 i=0 ; i<MAX_AUDIO_STREAMS ; i++ )
    {
        Runtime().StreamRuntime.CloseFile( &m_AudioStreams[i] );

        // Delete the streams io_request.
        delete [] m_AudioStreams[ i ].pIoRequest;
    }

    Runtime().Backend.FlushRenderCommands();

    // Free up the aram buffers.
    Runtime().Backend.FreeAudioRam( (void*)m_ARAM );

    Runtime().StreamRuntime.Kill();

    // Nuke it!
    x_memset( m_AudioStreams, 0, MAX_AUDIO_STREAMS*sizeof(audio_stream) );
    m_pRuntime = NULL;
}

//==============================================================================

audio_stream* audio_stream_mgr::AcquireStream( u32 WaveformOffset, u32 WaveformLength, channel* pLeft, channel* pRight )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_stream_mgr::AcquireStream" );

    audio_stream* pStream                 = NULL;
    xbool         bCoolingStreamAvailable = FALSE;
    s32           nFreeStreams            = 0;
    s32           nReservedStreamsInUse   = 0;
    voice*        pVoice                  = pLeft->pElement->pVoice;
    xbool         UseReservedStream       = pVoice && pVoice->UseReservedStream;
    xbool         bWaitingForReservedStream = FALSE;
    s32           CompressionType         = pLeft->pElement->Sample.pColdSample->CompressionType;

    // Error check.
    ASSERT( WaveformLength );
    ASSERT( pLeft || (pLeft&&pRight) );

    // Find a free stream...
    for( s32 i=0 ; i<MAX_AUDIO_STREAMS ; i++ )
    {
        if( m_AudioStreams[ i ].UseReservedSlot && (m_AudioStreams[ i ].State != STREAM_FREE) )
        {
            nReservedStreamsInUse++;
        }

        // Is it marked free?
        if( (m_AudioStreams[ i ].Type == INACTIVE) && (m_AudioStreams[ i ].State == STREAM_FREE) )
        {
            // Get the streams io_request status.
            io_request::status Status = m_AudioStreams[ i ].pIoRequest->GetStatus();

            // Is the streams io_request in a stable state?
            if( (Status != io_request::QUEUED) &&
                (Status != io_request::PENDING) &&
                (Status != io_request::IN_PROGRESS) &&
                (m_AudioStreams[i].FileHandle==NULL) )
            {
                nFreeStreams++;

                // Found one!
                if( pStream == NULL )
                {
                    pStream = &m_AudioStreams[ i ];
                }
            }
            else
            {
                bCoolingStreamAvailable = TRUE;
            }
        }
    }

    if( pStream )
    {
        if( UseReservedStream )
        {
            if( (m_nReservedStreams <= 0) || (nReservedStreamsInUse >= m_nReservedStreams) )
            {
                bWaitingForReservedStream = (m_nReservedStreams > 0);
                pStream = NULL;
            }
        }
        else
        {
            s32 nReservedStreamsAvailable = m_nReservedStreams - nReservedStreamsInUse;

            if( nReservedStreamsAvailable < 0 )
            {
                nReservedStreamsAvailable = 0;
            }

            if( nFreeStreams <= nReservedStreamsAvailable )
            {
                pStream = NULL;
            }
        }
    }

    if( UseReservedStream && (m_nReservedStreams > 0) && (pStream == NULL) )
    {
        bWaitingForReservedStream = TRUE;
    }

    // Make sure a stream is available!
    if( (nFreeStreams == 0) && !bCoolingStreamAvailable && !bWaitingForReservedStream )
    {
#ifdef LOG_AUDIO_STREAM_ACQUIRE_FAIL
        {
            LOG_WARNING( LOG_AUDIO_STREAM_ACQUIRE_FAIL, "Failed! pVoice: 0x%08x", pVoice );

            if( pVoice )
            {
                x_DebugMsg( xfs("AcquireStream: Failed to acquire a stream!!!! %08x %s\n", pVoice, pVoice->pDescriptorName) );
            }
        }
#endif
        //x_DebugMsg( "AcquireStream: Failed to acquire a stream!!!! %08x\n", pVoice );
    }

    // Find one?
    if( pStream )
    {
        pStream->UseReservedSlot = UseReservedStream;

        // Left and right channel specified?
        if( pLeft && pRight )
        {
            // Its a stereo stream.
            pStream->Type = STEREO_STREAM;

            if( Runtime().Decoders.UsesRuntimeDecode( (compression_types)CompressionType ) )
            {
                pStream->ReadBufferSize = 0;
            }
            else
            {
                switch( CompressionType )
                {
                    case ADPCM: pStream->ReadBufferSize = STREAM_BUFFER_SIZE * 2; break;
                    case PCM:   pStream->ReadBufferSize = STREAM_BUFFER_SIZE * 2; break;
                    default:    ASSERT( 0 ); break;
                }
            }
        }

        // Only left channel specified?
        else if( pLeft )
        {
            // Its a mono stream.
            pStream->Type = MONO_STREAM;

            if( Runtime().Decoders.UsesRuntimeDecode( (compression_types)CompressionType ) )
            {
                pStream->ReadBufferSize = 0;
            }
            else
            {
                switch( CompressionType )
                {
                    case ADPCM: pStream->ReadBufferSize = STREAM_BUFFER_SIZE; break;
                    case PCM:   pStream->ReadBufferSize = STREAM_BUFFER_SIZE; break;
                    default:    ASSERT( 0 ); break;
                }
            }
        }
        else
        {
            // Dunno what it is...
            pStream->Type    = INACTIVE;
            pStream          = NULL;
        }
    
        // Still around?
        if( pStream )
        {
            // Fill it out.
            pStream->State                      = STREAM_RESERVED;
            pStream->pDecoder                   = NULL;
            pStream->CompressionType            = (compression_types)CompressionType;
            pStream->StreamDone                 = FALSE;
            pStream->FileHandle                 = NULL;
            pStream->WaveformOffset             = WaveformOffset;
            pStream->WaveformLength             = WaveformLength;
            pStream->WaveformCursor             = 0;
            pStream->DecodeWriteCursor          = 0;
            pStream->DecodedFrames              = 0;
            pStream->DecodedEndFrame            = 0;
            pStream->pChannel[ LEFT_CHANNEL ]   = pLeft;
            pStream->pChannel[ RIGHT_CHANNEL ]  = pRight;

#ifdef LOG_AUDIO_STREAM_ACQUIRE_SUCCESS
            {
                voice* pVoice = NULL;

                if( pLeft && pLeft->pElement )
                {
                    pVoice = pLeft->pElement->pVoice;
                }
                LOG_MESSAGE( LOG_AUDIO_STREAM_ACQUIRE_SUCCESS, "Acquired! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
            }
#endif
        }
    }

    // Is one cooling?
    if( bWaitingForReservedStream ||
        (bCoolingStreamAvailable && (pStream == NULL)) )
    {
        pStream = COOLING_STREAM;
    }

    // Tell the world.
    return pStream;
}

//==============================================================================

void audio_stream_mgr::ReleaseStream( audio_stream* pStream )
{
    // Error check.
    ASSERT( VALID_STREAM( pStream ) );

#ifdef LOG_AUDIO_STREAM_RELEASE
    LOG_MESSAGE( LOG_AUDIO_STREAM_RELEASE, "Released! pStream: 0x%08x", pStream );
#endif // LOG_AUDIO_STREAM_RELEASE

    if( (pStream->State != STREAM_FREE) && (pStream->State != STREAM_CLOSING) )
    {
        pStream->Type       = INACTIVE;
        pStream->StreamDone = TRUE;
        pStream->State      = STREAM_STOPPING;
    }

}

//==============================================================================

void audio_stream_mgr::QueueStreamOpen( audio_stream* pStream )
{
    ASSERT( VALID_STREAM( pStream ) );

    if( (pStream->Type != INACTIVE) && (pStream->State == STREAM_RESERVED) )
    {
        pStream->State = STREAM_OPENING;
    }
}

//==============================================================================

void audio_stream_mgr::Update( void )
{
    audio_stream* pStream  = m_AudioStreams;
    s32           i;

    // Check out all the streams...
    for( i=0 ; i<MAX_AUDIO_STREAMS ; i++, pStream++ )
    {
        xbool bLeftBad   = FALSE;
        xbool bRightBad  = FALSE;
        xbool bStereoBad = FALSE;
        voice* pVoice    = NULL;

        if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
        {
            pVoice = pStream->pChannel[0]->pElement->pVoice;
        }
        if( pVoice )
        {
#ifdef LOG_AUDIO_STREAM_UPDATE
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, 
                "Volume! pStream: 0x%08x, pVoice: 0x%08x, Volume: %f", 
                        pStream, pVoice, pStream->pChannel[0]->Volume );
#endif
        }

        // Need to open the stream?
        if( pStream->State == STREAM_OPENING )
        {
#ifdef LOG_AUDIO_STREAM_UPDATE
            voice* pVoice = NULL;

            if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
            {
                pVoice = pStream->pChannel[0]->pElement->pVoice;
            }
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Open! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice  );
#endif            
            if( Runtime().StreamRuntime.OpenFile( pStream ) )
            {
                if( Runtime().Decoders.UsesRuntimeDecode( pStream ) )
                {
                    pStream->State = STREAM_STARTING;
                }
                else
                {
                    // Now warm up the stream.
                    if( !Runtime().StreamRuntime.Warm( pStream ) )
                    {
                        pStream->State = STREAM_STOPPING;
                    }
                }
            }
            else
            {
#ifdef LOG_AUDIO_STREAM_UPDATE
                voice* pVoice = NULL;

                if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
                {
                    pVoice = pStream->pChannel[0]->pElement->pVoice;
                }
                LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Open Failed! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice  );
#endif            
                pStream->State = STREAM_STOPPING;
            }
        }

        // Start the stream?
        if( (pStream->State == STREAM_STARTING) && (pStream->Type != INACTIVE) )
        {
#ifdef LOG_AUDIO_STREAM_UPDATE
            voice* pVoice = NULL;

            if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
            {
                pVoice = pStream->pChannel[0]->pElement->pVoice;
            }
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Start! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice  );
#endif

            if( (pStream->Type == INACTIVE) || (pStream->State != STREAM_STARTING) )
            {
                continue;
            }
            
            // Handle runtime decoded streams.
            if( Runtime().Decoders.UsesRuntimeDecode( pStream ) )
            {
                if( Runtime().Decoders.IsOpen( pStream ) || Runtime().Decoders.Open( pStream ) )
                {
                    // Wait for half of the PCM ring before starting playback.
                    Runtime().StreamRuntime.UpdateDecoded( pStream );

                    if( !pStream->StreamDone && pStream->DecodedFrames < STREAM_BUFFER_SIZE / sizeof( s16 ) )
                    {
                        continue;
                    }

                    if( (pStream->Type == INACTIVE) ||
                        ((pStream->State != STREAM_STARTING) && (pStream->State != STREAM_DRAINING)) )
                    {
                        continue;
                    }
                }
                else
                {
                    pStream->State = STREAM_STOPPING;
                }
            }

            // Set up the
            switch( pStream->Type )
            {
                case MONO_STREAM:
                {
                    if( pStream->State != STREAM_STOPPING )
                    {
                        // Mark left channel as loaded.
                        ASSERT( pStream->pChannel[LEFT_CHANNEL] );
                        ASSERT( pStream->pChannel[LEFT_CHANNEL]->pElement);

                        if( pStream->pChannel[LEFT_CHANNEL] )
                        {
                            pStream->pChannel[LEFT_CHANNEL]->pElement->State = ELEMENT_LOADED;
                        }
                    }

                    break;
                }

                case STEREO_STREAM:
                {
                    // Is the left channel hosed?
                    bLeftBad = (pStream->pChannel[LEFT_CHANNEL] == NULL) ||
                               (pStream->pChannel[LEFT_CHANNEL]->Type != COLD_SAMPLE) ||
                               (pStream->pChannel[LEFT_CHANNEL]->pElement == NULL) ||
                               (pStream->pChannel[LEFT_CHANNEL]->pElement->pStereoElement == NULL);

                    // Is he right channel hosed?
                    bRightBad = (pStream->pChannel[RIGHT_CHANNEL] == NULL) ||
                                (pStream->pChannel[RIGHT_CHANNEL]->Type != COLD_SAMPLE) ||
                                (pStream->pChannel[RIGHT_CHANNEL]->pElement == NULL) ||
                                (pStream->pChannel[RIGHT_CHANNEL]->pElement->pStereoElement == NULL);

                    // Is the stereo partner hosed?
                    if( !bLeftBad && !bRightBad )
                    {
                        bStereoBad = (pStream->pChannel[LEFT_CHANNEL]->pElement->pStereoElement != pStream->pChannel[RIGHT_CHANNEL]->pElement) ||
                                     (pStream->pChannel[RIGHT_CHANNEL]->pElement->pStereoElement != pStream->pChannel[LEFT_CHANNEL]->pElement);
                    }
                    
                    // Is something wrong?
                    if( bLeftBad || bRightBad || bStereoBad )
                    {
                        // Stop the stream.
                        pStream->State = STREAM_STOPPING;

                        if( (bRightBad || bStereoBad) && pStream->pChannel[LEFT_CHANNEL] )
                        {
                            if( Runtime().Backend.ReleaseChannel( pStream->pChannel[LEFT_CHANNEL] ) )
                            {
                                pStream->pChannel[LEFT_CHANNEL] = 0;
                            }
                        }

                        if( (bLeftBad || bStereoBad) && pStream->pChannel[RIGHT_CHANNEL] )
                        {
                            if( Runtime().Backend.ReleaseChannel( pStream->pChannel[RIGHT_CHANNEL] ) )
                            {
                                pStream->pChannel[RIGHT_CHANNEL] = 0;
                            }
                        }
                    }

                    if( pStream->State != STREAM_STOPPING )
                    {
                        // Mark left channel as loaded.
                        if( pStream->pChannel[LEFT_CHANNEL] )
                        {
                            pStream->pChannel[LEFT_CHANNEL]->pElement->State = ELEMENT_LOADED;
                        }

                        // Mark right channel as loaded.
                        if( pStream->pChannel[RIGHT_CHANNEL] )
                        {
                            pStream->pChannel[RIGHT_CHANNEL]->pElement->State = ELEMENT_LOADED;
                        }
                    }

                    break;
                }

                default:
                {
                    ASSERT( 0 );
                    break;
                }
            }

            if( pStream->State == STREAM_STARTING )
            {
                pStream->State = STREAM_RUNNING;
            }
        }

        // Stop the stream?
        else if( pStream->State == STREAM_STOPPING )
        {
            s32 Status = (s32)pStream->pIoRequest->GetStatus();
#ifdef LOG_AUDIO_STREAM_UPDATE
            voice* pVoice = NULL;

            if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
            {
                pVoice = pStream->pChannel[0]->pElement->pVoice;
            }
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Stop! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
#endif
            
            // Make sure the streams request is done...
            if( ( Status != io_request::QUEUED ) &&
                ( Status != io_request::PENDING ) &&
                ( Status != io_request::IN_PROGRESS ) &&
                ( !pStream->pDecoder || pStream->pDecoder->RequestStop() ) )
            {
                pStream->Type       = INACTIVE;
                pStream->StreamDone = TRUE;
                pStream->State      = STREAM_CLOSING;

                Runtime().StreamRuntime.CloseFile( pStream );

                pStream->pChannel[LEFT_CHANNEL]  = NULL;
                pStream->pChannel[RIGHT_CHANNEL] = NULL;
                pStream->WaveformCursor          = 0;
                pStream->DecodeWriteCursor       = 0;
                pStream->DecodedFrames           = 0;
                pStream->DecodedEndFrame         = 0;
                pStream->UseReservedSlot         = FALSE;
                pStream->State                   = STREAM_FREE;
            }
        }

        // If its active, then just do the hardware stream update...
        else if( ((pStream->State == STREAM_RUNNING) || (pStream->State == STREAM_DRAINING)) && (pStream->Type != INACTIVE) )
        {
            switch( pStream->Type )
            {
                case MONO_STREAM:
                {
                    Runtime().Backend.UpdateStream( pStream->pChannel[LEFT_CHANNEL] );
                    break;
                }

                case STEREO_STREAM:
                {
                    Runtime().Backend.UpdateStream( pStream->pChannel[RIGHT_CHANNEL] );
                    Runtime().Backend.UpdateStream( pStream->pChannel[LEFT_CHANNEL] );
                    break;
                }

                default:
                {
                    ASSERT( 0 );
                    break;
                }
            }

            if( (pStream->State == STREAM_RUNNING) && Runtime().Decoders.IsOpen( pStream ) )
            {
                Runtime().StreamRuntime.UpdateDecoded( pStream );
            }
        }
    }
}

//==============================================================================

xbool audio_stream_mgr::ReserveStreams( s32 nStreams )
{
    if( (nStreams < 0) || ((m_nReservedStreams + nStreams) > MAX_AUDIO_STREAMS) )
    {
        ASSERT( 0 );
        return FALSE;
    }

    m_nReservedStreams += nStreams;
    return TRUE;
}

//==============================================================================

xbool audio_stream_mgr::UnReserveStreams( s32 nStreams )
{
    if( (nStreams < 0) || ((m_nReservedStreams - nStreams) < 0) )
    {
        ASSERT( 0 );
        return FALSE;
    }

    m_nReservedStreams -= nStreams;
    return TRUE;
}

//==============================================================================

void audio_stream_mgr::GetDebugStats( audio_debug_stats& Stats ) const
{
    Stats.ReservedStreams = m_nReservedStreams;
    Stats.ActiveStreams   = 0;
    Stats.RunningStreams  = 0;
    Stats.OpeningStreams  = 0;

    for( s32 i = 0; i < MAX_AUDIO_STREAMS; i++ )
    {
        const stream_state State = m_AudioStreams[i].State;
        Stats.ActiveStreams += State != STREAM_FREE;
        Stats.RunningStreams += State == STREAM_RUNNING;
        Stats.OpeningStreams += State == STREAM_OPENING || State == STREAM_WARMING || State == STREAM_STARTING;
    }
}
