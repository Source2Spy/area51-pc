#include "audio_stream_mgr.hpp"
#include "audio_channel_mgr.hpp"
#include "audio_hardware.hpp"
#include "audio_package.hpp"
#include "audio_voice_mgr.hpp"
#include "audio_mp3_mgr.hpp"
#include "e_Virtual.hpp"
#include "x_log.hpp"
#include "x_bytestream.hpp"

#if defined(rbrannon)
#define LOG_AUDIO_STREAM_ACQUIRE_SUCCESS "stream_mgr::Acquire"
#define LOG_AUDIO_STREAM_ACQUIRE_FAIL    "stream_mgr::Acquire"
#define LOG_AUDIO_STREAM_WARM_STREAM     "stream_mgr::WarmStream"
#define LOG_AUDIO_STREAM_READ_STREAM     "stream_mgr::ReadStream"
#define LOG_AUDIO_STREAM_UPDATE          "stream_mgr::Update"
#define LOG_AUDIO_STREAM_RELEASE         "stream_mgr::ReleaseStream"
#endif

#define VALID_STREAM( pStream ) ((pStream >= &m_AudioStreams[0]) && (pStream <= &m_AudioStreams[MAX_AUDIO_STREAMS-1]))

//------------------------------------------------------------------------------

static void read_callback_0_0( io_request* pRequest );
static void read_callback_1_0( io_request* pRequest );
static void read_callback_2_0( io_request* pRequest );
static void read_callback_3_0( io_request* pRequest );
static void read_callback_0_1( io_request* pRequest );
static void read_callback_1_1( io_request* pRequest );
static void read_callback_2_1( io_request* pRequest );
static void read_callback_3_1( io_request* pRequest );
static void warm_callback_0_0( io_request* pRequest );
static void warm_callback_1_0( io_request* pRequest );
static void warm_callback_2_0( io_request* pRequest );
static void warm_callback_3_0( io_request* pRequest );
static void warm_callback_0_1( io_request* pRequest );
static void warm_callback_1_1( io_request* pRequest );
static void warm_callback_2_1( io_request* pRequest );
static void warm_callback_3_1( io_request* pRequest );
static void audio_stream_warm_callback( io_request* pRequest, audio_stream* pStream, s32 ReadBufferIndex );

//------------------------------------------------------------------------------

io_request::callback_fn* read_callbacks[MAX_AUDIO_STREAMS][2] = 
{
    {read_callback_0_0, read_callback_0_1 },
    {read_callback_1_0, read_callback_1_1 },
    {read_callback_2_0, read_callback_2_1 },
    {read_callback_3_0, read_callback_3_1 }
};

//------------------------------------------------------------------------------

io_request::callback_fn* warm_callbacks[MAX_AUDIO_STREAMS][2] = 
{
    { warm_callback_0_0, warm_callback_0_1 },
    { warm_callback_1_0, warm_callback_1_1 },
    { warm_callback_2_0, warm_callback_2_1 },
    { warm_callback_3_0, warm_callback_3_1 },
};

//------------------------------------------------------------------------------

audio_stream_mgr g_AudioStreamMgr;

//------------------------------------------------------------------------------
// Helper functions.

static void read_callback_0_0( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[0], 0 );
}

//------------------------------------------------------------------------------

static void read_callback_1_0( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[1], 0 );
}

//------------------------------------------------------------------------------

static void read_callback_2_0( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[2], 0 );
}

//------------------------------------------------------------------------------

static void read_callback_3_0( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[3], 0 );
}

//------------------------------------------------------------------------------

static void read_callback_0_1( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[0], 1 );
}

//------------------------------------------------------------------------------

static void read_callback_1_1( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[1], 1 );
}

//------------------------------------------------------------------------------

static void read_callback_2_1( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[2], 1 );
}

//------------------------------------------------------------------------------

static void read_callback_3_1( io_request* pRequest )
{
    audio_stream_read_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[3], 1 );
}

//------------------------------------------------------------------------------

static void warm_callback_0_0( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[0], 0 );
}

//------------------------------------------------------------------------------

static void warm_callback_1_0( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[1], 0 );
}

//------------------------------------------------------------------------------

static void warm_callback_2_0( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[2], 0 );
}

//------------------------------------------------------------------------------

static void warm_callback_3_0( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[3], 0 );
}

//------------------------------------------------------------------------------

static void warm_callback_0_1( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[0], 1 );
}

//------------------------------------------------------------------------------

static void warm_callback_1_1( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[1], 1 );
}

//------------------------------------------------------------------------------

static void warm_callback_2_1( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[2], 1 );
}

//------------------------------------------------------------------------------

static void warm_callback_3_1( io_request* pRequest )
{
    audio_stream_warm_callback( pRequest, &g_AudioStreamMgr.m_AudioStreams[3], 1 );
}

//------------------------------------------------------------------------------

static void audio_stream_warm_callback( io_request* pRequest, audio_stream* pStream, s32 ReadBufferIndex )
{
    // Normal read callback.
    audio_stream_read_callback( pRequest, pStream, ReadBufferIndex );

    // Ok to start up the stream now!
    pStream->bStartStream = TRUE;
}

//------------------------------------------------------------------------------
// Class functions.

audio_stream_mgr::audio_stream_mgr( void )
{
    // Nuke the audio streams.
    x_memset( m_AudioStreams, 0, sizeof( m_AudioStreams ) );
    m_ARAM             = 0;
    m_MainRam          = 0;
    m_ReadBuffers[0]   = 0;
    m_ReadBuffers[1]   = 0;
    m_ActiveReadBuffer = 0;
    m_nReservedStreams = 0;

    // Save the index
    for( s32 i=0 ; i<MAX_AUDIO_STREAMS ; i++ )
        m_AudioStreams[ i ].Index = i;
}

//------------------------------------------------------------------------------

audio_stream_mgr::~audio_stream_mgr( void )
{
}

//------------------------------------------------------------------------------

audio_stream* audio_stream_mgr::AcquireStream( u32 WaveformOffset, u32 WaveformLength, channel* pLeft, channel* pRight )
{
    CONTEXT( "audio_stream_mgr::AcquireStream" );

    audio_stream* pStream          = NULL;
    xbool         bStreamAvailable = FALSE;
    s32           CompressionType  = pLeft->pElement->Sample.pColdSample->CompressionType;

    // Error check.
    ASSERT( WaveformLength );
    ASSERT( pLeft || (pLeft&&pRight) );

    // Find a free stream...
    for( s32 i=0 ; i<MAX_AUDIO_STREAMS ; i++ )
    {
        // Is it marked free?
        if( m_AudioStreams[ i ].Type == INACTIVE )
        {
            // At least one is marked free.
            bStreamAvailable = TRUE;

            // Get the streams io_request status.
            io_request::status Status = m_AudioStreams[ i ].pIoRequest->GetStatus();

            // Is the streams io_request in a stable state?
            if( (Status != io_request::QUEUED) && (Status != io_request::PENDING) && (Status != io_request::IN_PROGRESS) && (m_AudioStreams[i].FileHandle==NULL) )
            {
                // Found one!
                pStream = &m_AudioStreams[ i ];
                break;
            }
        }
    }

    // Make sure a stream is available!
    if( !bStreamAvailable )
    {
#ifdef LOG_AUDIO_STREAM_ACQUIRE_FAIL
        {
            voice* pVoice = NULL;
            if( pLeft && pLeft->pElement )
                pVoice = pLeft->pElement->pVoice;
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
        // Left and right channel specified?
        if( pLeft && pRight )
        {
            // Its a stereo stream.
            pStream->Type = STEREO_STREAM;
            switch( CompressionType )
            {
                case ADPCM: pStream->ReadBufferSize = STREAM_BUFFER_SIZE * 2; break;
                case MP3:   pStream->ReadBufferSize = MP3_BUFFER_SIZE; break;
                case PCM:   pStream->ReadBufferSize = STREAM_BUFFER_SIZE * 2; break;
                default:    ASSERT( 0 ); break;
            }
        }
        // Only left channel specified?
        else if( pLeft )
        {
            // Its a mono stream.
            pStream->Type = MONO_STREAM;
            switch( CompressionType )
            {
                case ADPCM: pStream->ReadBufferSize = STREAM_BUFFER_SIZE; break;
                case MP3:   pStream->ReadBufferSize = MP3_BUFFER_SIZE; break;
                case PCM:   pStream->ReadBufferSize = STREAM_BUFFER_SIZE; break;
                default:    ASSERT( 0 ); break;
            }
        }
        else
        {
            // Dunno what it is...
            pStream->Type    = INACTIVE;
            bStreamAvailable = FALSE;
            pStream          = NULL;
        }
    
        // Still around?
        if( pStream )
        {
            // Fill it out.
            pStream->bOpenStream                = FALSE;
            pStream->bStartStream               = FALSE;
            pStream->bStopStream                = FALSE;
            pStream->HandleMP3                  = NULL;
            pStream->CompressionType            = (compression_types)CompressionType;
            pStream->StreamDone                 = FALSE;
            pStream->FileHandle                 = NULL;
            pStream->WaveformOffset             = WaveformOffset;
            pStream->WaveformLength             = WaveformLength;
            pStream->WaveformCursor             = 0;
            pStream->pChannel[ LEFT_CHANNEL ]   = pLeft;
            pStream->pChannel[ RIGHT_CHANNEL ]  = pRight;

#ifdef LOG_AUDIO_STREAM_ACQUIRE_SUCCESS
            {
                voice* pVoice = NULL;
                if( pLeft && pLeft->pElement )
                    pVoice = pLeft->pElement->pVoice;
                LOG_MESSAGE( LOG_AUDIO_STREAM_ACQUIRE_SUCCESS, "Acquired! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
            }
#endif
        }
    }

    // Is one cooling?
    if( bStreamAvailable && (pStream == NULL) )
        pStream = COOLING_STREAM;

    // Tell the world.
    return pStream;
}

//------------------------------------------------------------------------------

void audio_stream_mgr::ReleaseStream( audio_stream* pStream )
{
    // Error check.
    ASSERT( VALID_STREAM( pStream ) );

#ifdef LOG_AUDIO_STREAM_RELEASE
    LOG_MESSAGE( LOG_AUDIO_STREAM_RELEASE, "Released! pStream: 0x%08x", pStream );
#endif // LOG_AUDIO_STREAM_RELEASE

    // TODO: Error check.
    pStream->Type = INACTIVE;

    // Stop it now..
    pStream->bStopStream = TRUE;
}

//------------------------------------------------------------------------------

xbool audio_stream_mgr::WarmStream( audio_stream* pStream, io_request::callback_fn* pCallback )
{
    CONTEXT( "audio_stream_mgr::WarmStream" );

#ifdef LOG_AUDIO_STREAM_WARM_STREAM
    voice* pVoice = NULL;
    if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
        pVoice = pStream->pChannel[0]->pElement->pVoice;
    LOG_MESSAGE( LOG_AUDIO_STREAM_WARM_STREAM, "pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
#endif

    xbool Result = FALSE;

    // Error check.
    ASSERT( VALID_STREAM( pStream ) );
    
    io_request::status status = pStream->pIoRequest->GetStatus();
#if !defined(TARGET_DEV)
    ASSERT( status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED );
    ASSERT( pStream->Type != INACTIVE );
#endif
    if( (pStream->Type != INACTIVE) && (status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED) )
    {
        // Start again...
        pStream->WaveformCursor  = 0;
        pStream->ARAMWriteBuffer = 0;

        // And this too...
        x_memset( (void*)pStream->MainRAM[0], 0, MP3_BUFFER_SIZE );
        x_memset( (void*)pStream->MainRAM[1], 0, MP3_BUFFER_SIZE );

        // Check the callback
        if( pCallback == NULL )
        {
            ASSERT( (pStream->Index >= 0) && (pStream->Index < MAX_AUDIO_STREAMS) );
            pCallback = warm_callbacks[ pStream->Index ][ m_ActiveReadBuffer ];
        }

        // Read from the stream.
        return ReadStream( pStream, pCallback );
    }

    return Result;
}

//------------------------------------------------------------------------------

xbool audio_stream_mgr::ReadStream( audio_stream* pStream, io_request::callback_fn* pCallback )
{
    CONTEXT( "audio_stream_mgr::ReadStream" );

#ifdef LOG_AUDIO_STREAM_READ_STREAM
    voice* pVoice = NULL;
    if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
        pVoice = pStream->pChannel[0]->pElement->pVoice;
    LOG_MESSAGE( LOG_AUDIO_STREAM_READ_STREAM, "pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
#endif

    xbool Result = FALSE;

    // Error check.
    ASSERT( VALID_STREAM( pStream ) );

    io_request::status status = pStream->pIoRequest->GetStatus();
#if !defined(TARGET_DEV)
    ASSERT( status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED );
    ASSERT( pStream->Type != INACTIVE );
#endif // !defined(TARGET_DEV)

    if( (pStream->Type != INACTIVE) && (status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED) )
    {
        // Check the callback.
        if( pCallback == NULL )
        {
            ASSERT( (pStream->Index >= 0) && (pStream->Index < MAX_AUDIO_STREAMS) );
            pCallback = read_callbacks[ pStream->Index ][ m_ActiveReadBuffer ];
        }

        // Now set the request up.
        SetRequest( pStream, pCallback );

        // Switch read buffers.
        m_ActiveReadBuffer ^= 1;

        // Update the waveform cursor
        pStream->WaveformCursor += pStream->ReadBufferSize;

        // Stream done?
        if( pStream->WaveformCursor >= pStream->WaveformLength )
        {
            // Mark stream as done.
            pStream->StreamDone = TRUE;
        }

        // Queue the io request.
        g_IoMgr.QueueRequest( pStream->pIoRequest );

        // It's all good.
        Result = TRUE;
    }
    return Result;
}

//------------------------------------------------------------------------------

void audio_stream_mgr::Update( void )
{
    audio_stream* pStream  = g_AudioStreamMgr.m_AudioStreams;
    s32           i;

    // Check out all the streams...
    for( i=0 ; i<MAX_AUDIO_STREAMS ; i++, pStream++ )
    {
        xbool bLeftBad   = FALSE;
        xbool bRightBad  = FALSE;
        xbool bStereoBad = FALSE;
        voice* pVoice    = NULL;

        if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
            pVoice = pStream->pChannel[0]->pElement->pVoice;
        if( pVoice )
        {
#ifdef LOG_AUDIO_STREAM_UPDATE
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, 
                "Volume! pStream: 0x%08x, pVoice: 0x%08x, Volume: %f", 
                        pStream, pVoice, pStream->pChannel[0]->Volume );
#endif
        }


        // Need to open the stream?
        if( pStream->bOpenStream )
        {
#ifdef LOG_AUDIO_STREAM_UPDATE
            voice* pVoice = NULL;
            if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
                pVoice = pStream->pChannel[0]->pElement->pVoice;
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Open! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice  );
#endif            
            // Reset flag.
            pStream->bOpenStream = FALSE;

            // Open the file.
            ASSERT( pStream->FileHandle == NULL );
            ASSERT( VALID_CHANNEL( pStream->pChannel[0] ) );
            if( pStream->pChannel[0]->pElement && pStream->pChannel[0]->pElement->pVoice && pStream->pChannel[0]->pElement->pVoice->pPackage )
            {
                // Open the file to stream from.
                pStream->FileHandle = g_IOFSMgr.Open( pStream->pChannel[0]->pElement->pVoice->pPackage->m_Filename, "rb" ); 
                ASSERT( pStream->FileHandle );
                g_IOFSMgr.EnableChecksum( pStream->FileHandle, FALSE );

                // Now warm up the stream.
                g_AudioStreamMgr.WarmStream( pStream );
            }
            else
            {
#ifdef LOG_AUDIO_STREAM_UPDATE
                voice* pVoice = NULL;
                if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
                    pVoice = pStream->pChannel[0]->pElement->pVoice;
                LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Open Failed! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice  );
#endif            
            }
        }

        // Start the stream?
        if( pStream->bStartStream && (pStream->Type != INACTIVE) )
        {
#ifdef LOG_AUDIO_STREAM_UPDATE
            voice* pVoice = NULL;
            if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
                pVoice = pStream->pChannel[0]->pElement->pVoice;
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Start! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice  );
#endif
            
            // Clear the flag
            pStream->bStartStream = FALSE;
            
#if defined(TARGET_PC)
            // Handle mp3 streams.
            if( pStream->CompressionType == MP3 )
            {
                g_AudioMP3Mgr.Open( pStream );

                // De-compress first part of it and dma it to aram.
                g_AudioHardware.UpdateMP3( pStream );
            }
#endif

            // Set up the
            switch( pStream->Type )
            {
                case MONO_STREAM: 
                    // Mark left channel as loaded.
                    ASSERT( pStream->pChannel[LEFT_CHANNEL] );
			        ASSERT( pStream->pChannel[LEFT_CHANNEL]->pElement);

                    if( pStream->pChannel[LEFT_CHANNEL] )
                    {
                        pStream->pChannel[LEFT_CHANNEL]->pElement->State = ELEMENT_LOADED;
                    }
                    break;

                case STEREO_STREAM:
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
                        // No hardware updates!
                        g_AudioHardware.Lock();
                        
                        // Stop the stream.
                        pStream->bStopStream = TRUE;

                        if( (bRightBad || bStereoBad) && pStream->pChannel[LEFT_CHANNEL] )
                        {
                            g_AudioHardware.ReleaseChannel( pStream->pChannel[LEFT_CHANNEL] );
                            pStream->pChannel[LEFT_CHANNEL] = 0;
                        }

                        if( (bLeftBad || bStereoBad) && pStream->pChannel[RIGHT_CHANNEL] )
                        {
                            g_AudioHardware.ReleaseChannel( pStream->pChannel[RIGHT_CHANNEL] );
                            pStream->pChannel[RIGHT_CHANNEL] = 0;
                        }
                    
                        // Ok for audio updates
                        g_AudioHardware.Unlock();
                    }

                    if( !pStream->bStopStream )
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

                default:
                    ASSERT( 0 );
                    break;
            }
        }
        // Stop the stream?
        else if( pStream->bStopStream )
        {
            s32 Status = (s32)pStream->pIoRequest->GetStatus();
#ifdef LOG_AUDIO_STREAM_UPDATE
            voice* pVoice = NULL;
            if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
                pVoice = pStream->pChannel[0]->pElement->pVoice;
            LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Stop! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
#endif
            
            // Make sure the streams request is done...
            if( (Status != io_request::QUEUED) && 
                (Status != io_request::PENDING) && 
                (Status != io_request::IN_PROGRESS) )
            {
                
                // Clear flag
                pStream->bStopStream = FALSE;

#if defined(TARGET_PC)
                // Nuke the mp3 stream.
                if( (pStream->CompressionType == MP3) && pStream->HandleMP3 )
                    g_AudioMP3Mgr.Close( pStream );
#endif

                // Close the file.
                if( pStream->FileHandle )
                {
                    g_IOFSMgr.Close( pStream->FileHandle );
                    pStream->FileHandle = NULL;
                }
                else
                {
#ifdef LOG_AUDIO_STREAM_UPDATE
                    voice* pVoice = NULL;
                    if( pStream->pChannel[0] && pStream->pChannel[0]->pElement )
                        pVoice = pStream->pChannel[0]->pElement->pVoice;
                    LOG_MESSAGE( LOG_AUDIO_STREAM_UPDATE, "Stop Failed! pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
#endif
                }
            }
        }
        // If its active, then just do the hardware stream update...
        else if( pStream->Type != INACTIVE )
        {
            switch( pStream->Type )
            {
                case MONO_STREAM:
                    g_AudioHardware.UpdateStream( pStream->pChannel[LEFT_CHANNEL] );
                    break;

                case STEREO_STREAM:
                    g_AudioHardware.UpdateStream( pStream->pChannel[RIGHT_CHANNEL] );
                    g_AudioHardware.UpdateStream( pStream->pChannel[LEFT_CHANNEL] );
                    break;

                default:
                    ASSERT( 0 );
                    break;
            }
        }
    }
}

xbool audio_stream_mgr::ReserveStreams( s32 nStreams )
{
    m_nReservedStreams += nStreams;
    ASSERT( m_nReservedStreams <= MAX_AUDIO_STREAMS );
    ASSERT( m_nReservedStreams >= 0 );
    return TRUE;
}

xbool audio_stream_mgr::UnReserveStreams( s32 nStreams )
{
    m_nReservedStreams -= nStreams;
    ASSERT( m_nReservedStreams <= MAX_AUDIO_STREAMS );
    ASSERT( m_nReservedStreams >= 0 );
    return TRUE;
}

