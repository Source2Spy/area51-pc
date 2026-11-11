//==============================================================================
//
//  audio_stream_runtime.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_stream_runtime.hpp"
#include "e_Audio.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_package.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "Audio/audio_stream_decoder_factory.hpp"
#include "IOManager/io_mgr.hpp"
#include "x_log.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#if defined(rbrannon)
#define LOG_AUDIO_STREAM_WARM_STREAM     "stream_runtime::Warm"
#define LOG_AUDIO_STREAM_READ_STREAM     "stream_runtime::Read"
#endif

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
u32 audio_stream_runtime_ring_bytes_ahead( u32 CurrentPosition, u32 WriteCursor )
{
    if( CurrentPosition > WriteCursor )
    {
        return ((STREAM_BUFFER_SIZE*2) - CurrentPosition) + WriteCursor;
    }

    return WriteCursor - CurrentPosition;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_stream_runtime::audio_stream_runtime( io_fs& FileSystem, io_mgr& Io ) :
    m_pRuntime( NULL ),
    m_FileSystem( FileSystem ),
    m_Io( Io )
{
    x_memset( m_Streams,           0, sizeof( m_Streams ) );
    x_memset( m_ReadBuffers,       0, sizeof( m_ReadBuffers ) );
    x_memset( m_DecodeLeftBuffer,  0, sizeof( m_DecodeLeftBuffer ) );
    x_memset( m_DecodeRightBuffer, 0, sizeof( m_DecodeRightBuffer ) );
    m_ActiveReadBuffer = 0;
    m_DecodeWhichBuffer = 0;
}

//==============================================================================

audio_stream_runtime::~audio_stream_runtime( void )
{
}

//==============================================================================

audio_runtime& audio_stream_runtime::Runtime( void )
{
    ASSERT( m_pRuntime );
    return *m_pRuntime;
}

//==============================================================================

void audio_stream_runtime::Init( audio_runtime& Runtime )
{
    ASSERT( m_pRuntime == NULL );

    x_memset( m_Streams, 0, sizeof( m_Streams ) );

    s32 nBytes = MAX_STREAM_CHANNELS * STREAM_BUFFER_SIZE;

    m_ReadBuffers[0] = (uaddr)x_malloc( nBytes );
    m_ReadBuffers[1] = (uaddr)x_malloc( nBytes );
    m_ActiveReadBuffer = 0;
    m_DecodeWhichBuffer = 0;
    m_pRuntime = &Runtime;
}

//==============================================================================

void audio_stream_runtime::Kill( void )
{
    if( m_ReadBuffers[0] )
    {
        x_free( (void*)m_ReadBuffers[0] );
    }
    if( m_ReadBuffers[1] )
    {
        x_free( (void*)m_ReadBuffers[1] );
    }

    x_memset( m_Streams,     0, sizeof( m_Streams ) );
    x_memset( m_ReadBuffers, 0, sizeof( m_ReadBuffers ) );
    m_ActiveReadBuffer = 0;
    m_DecodeWhichBuffer = 0;
    m_pRuntime = NULL;
}

//==============================================================================

void audio_stream_runtime::RegisterStream( s32 Slot, audio_stream* pStream )
{
    ASSERT( (Slot >= 0) && (Slot < MAX_AUDIO_STREAMS) );
    ASSERT( pStream );

    if( (Slot < 0) || (Slot >= MAX_AUDIO_STREAMS) )
    {
        return;
    }

    m_Streams[Slot] = pStream;
}

//==============================================================================

s32 audio_stream_runtime::FindSlot( audio_stream* pStream ) const
{
    if( !pStream )
    {
        return -1;
    }

    for( s32 i=0 ; i<MAX_AUDIO_STREAMS ; i++ )
    {
        if( m_Streams[i] == pStream )
        {
            return i;
        }
    }

    return -1;
}

//==============================================================================

audio_stream* audio_stream_runtime::GetSlotStream( s32 Slot )
{
    ASSERT( (Slot >= 0) && (Slot < MAX_AUDIO_STREAMS) );

    if( (Slot < 0) || (Slot >= MAX_AUDIO_STREAMS) )
    {
        return NULL;
    }

    ASSERT( m_Streams[Slot] );
    return m_Streams[Slot];
}

//==============================================================================

xbool audio_stream_runtime::IsValidStream( audio_stream* pStream ) const
{
    return (FindSlot( pStream ) >= 0);
}

//==============================================================================

channel* audio_stream_runtime::ControlChannel( audio_stream* pStream )
{
    ASSERT( pStream );

    if( pStream->pChannel[LEFT_CHANNEL] &&
        pStream->pChannel[LEFT_CHANNEL]->StreamData.StreamControl )
    {
        return pStream->pChannel[LEFT_CHANNEL];
    }

    if( pStream->pChannel[RIGHT_CHANNEL] &&
        pStream->pChannel[RIGHT_CHANNEL]->StreamData.StreamControl )
    {
        return pStream->pChannel[RIGHT_CHANNEL];
    }

    return pStream->pChannel[LEFT_CHANNEL];
}

//==============================================================================

xbool audio_stream_runtime::NeedsDecodeRefill( audio_stream* pStream )
{
    if( !pStream )
    {
        return FALSE;
    }

    if( (pStream->State != STREAM_STARTING) && (pStream->State != STREAM_RUNNING) )
    {
        return FALSE;
    }

    if( (pStream->Type == INACTIVE) ||
        !Runtime().Decoders.UsesRuntimeDecode( pStream ) ||
        (pStream->StreamDone) ||
        !Runtime().Decoders.IsOpen( pStream ) )
    {
        return FALSE;
    }

    channel* pControl = ControlChannel( pStream );

    if( !pControl )
    {
        return FALSE;
    }

    u32 CurrentPosition = pControl->CurrBufferPosition * 2;
    u32 WriteCursor     = pStream->DecodeWriteCursor;

    return audio_stream_runtime_ring_bytes_ahead( CurrentPosition, WriteCursor ) < STREAM_BUFFER_SIZE;
}

//==============================================================================

xbool audio_stream_runtime::WriteDecodedChunk( audio_stream* pStream )
{
    ASSERT( pStream );

    s32                   WhichBuffer   = -1;
    stream_type           Type          = INACTIVE;
    channel*              pLeftChannel  = NULL;
    channel*              pRightChannel = NULL;
    audio_stream_decoder* pDecoder      = NULL;

    if( NeedsDecodeRefill( pStream ) &&
        (pStream->pChannel[LEFT_CHANNEL] != NULL) &&
        ((pStream->Type != STEREO_STREAM) || (pStream->pChannel[RIGHT_CHANNEL] != NULL)) )
    {
        WhichBuffer   = m_DecodeWhichBuffer;
        Type          = pStream->Type;
        pLeftChannel  = pStream->pChannel[LEFT_CHANNEL];
        pRightChannel = pStream->pChannel[RIGHT_CHANNEL];
        pDecoder      = pStream->pDecoder;

        if( ++m_DecodeWhichBuffer >= MAX_AUDIO_STREAMS )
        {
            m_DecodeWhichBuffer = 0;
        }
    }

    if( WhichBuffer < 0 )
    {
        return FALSE;
    }

    audio_decode_result DecodeResult = pDecoder->Decode( pStream,
                                                         m_DecodeLeftBuffer[ WhichBuffer ],
                                                         m_DecodeRightBuffer[ WhichBuffer ],
                                                         512 );

    if( ((pStream->State != STREAM_STARTING) && (pStream->State != STREAM_RUNNING)) ||
        (pStream->Type != Type) ||
        (pStream->pDecoder != pDecoder) ||
        (pStream->pChannel[LEFT_CHANNEL] != pLeftChannel) ||
        (pStream->pChannel[RIGHT_CHANNEL] != pRightChannel) ||
        (pLeftChannel == NULL) ||
        ((Type == STEREO_STREAM) && (pRightChannel == NULL)) )
    {
        return FALSE;
    }

    ASSERT( DecodeResult.FramesWritten >= 0 );
    ASSERT( DecodeResult.FramesWritten <= 512 );

    s32 FramesToCopy = DecodeResult.FramesWritten;

    if( FramesToCopy > 0 )
    {
        u32 WriteCursor = pStream->DecodeWriteCursor;
        u32 Cursor      = WriteCursor;
        u32 CopyBytes   = FramesToCopy * sizeof(s16);
        uaddr ARAM      = pStream->Samples[LEFT_CHANNEL].Sample.AudioRam + Cursor;

        x_memcpy( (void*)ARAM, m_DecodeLeftBuffer[ WhichBuffer ], CopyBytes );

        if( Type == STEREO_STREAM )
        {
            ARAM = pStream->Samples[RIGHT_CHANNEL].Sample.AudioRam + WriteCursor;
            x_memcpy( (void*)ARAM, m_DecodeRightBuffer[ WhichBuffer ], CopyBytes );
        }

        Cursor += CopyBytes;

        if( Cursor >= STREAM_BUFFER_SIZE*2 )
        {
            Cursor -= STREAM_BUFFER_SIZE*2;
        }

        pStream->DecodeWriteCursor = Cursor;
    }

    if( DecodeResult.FramesWritten > 0 )
    {
        pStream->DecodedFrames += DecodeResult.FramesWritten;
    }

    if( DecodeResult.EndOfStream )
    {
        pStream->StreamDone      = TRUE;
        pStream->DecodedEndFrame = pStream->DecodedFrames;

        pStream->Samples[LEFT_CHANNEL].Sample.nSamples = pStream->DecodedEndFrame;

        if( Type == STEREO_STREAM )
        {
            pStream->Samples[RIGHT_CHANNEL].Sample.nSamples = pStream->DecodedEndFrame;
        }

        Runtime().Backend.StopStreamLoop( pLeftChannel, pStream->DecodedEndFrame );

        if( Type == STEREO_STREAM )
        {
            Runtime().Backend.StopStreamLoop( pRightChannel, pStream->DecodedEndFrame );
        }

        if( (pStream->State == STREAM_STARTING) || (pStream->State == STREAM_RUNNING) )
        {
            pStream->State = STREAM_DRAINING;
        }
    }

    xbool Result = !DecodeResult.EndOfStream && (FramesToCopy > 0);

    return Result;
}

//==============================================================================

audio_stream_runtime* audio_stream_runtime::FromRequest( io_request* pRequest )
{
    ASSERT( pRequest );

    if( !pRequest )
    {
        return NULL;
    }

    audio_stream_runtime* pRuntime = (audio_stream_runtime*)pRequest->GetUserData();
    ASSERT( pRuntime );
    return pRuntime;
}

//==============================================================================

void audio_stream_runtime::ReadRequestCallback( io_request* pRequest )
{
    audio_stream_runtime* pRuntime = FromRequest( pRequest );

    if( pRuntime )
    {
        pRuntime->Runtime().Audio.QueueStreamReadComplete( pRequest );
    }
}

//==============================================================================

void audio_stream_runtime::WarmRequestCallback( io_request* pRequest )
{
    audio_stream_runtime* pRuntime = FromRequest( pRequest );

    if( pRuntime )
    {
        pRuntime->Runtime().Audio.QueueStreamWarmComplete( pRequest );
    }
}

//==============================================================================

audio_stream* audio_stream_runtime::FindRequestStream( io_request* pRequest ) const
{
    ASSERT( pRequest );

    if( !pRequest )
    {
        return NULL;
    }

    for( s32 i=0 ; i<MAX_AUDIO_STREAMS ; i++ )
    {
        audio_stream* pStream = m_Streams[i];

        if( pStream && (pStream->pIoRequest == pRequest) )
        {
            return pStream;
        }
    }

    ASSERT( 0 );
    return NULL;
}

//==============================================================================

s32 audio_stream_runtime::GetRequestReadBufferIndex( io_request* pRequest ) const
{
    ASSERT( pRequest );

    if( !pRequest )
    {
        return -1;
    }

    void* pBuffer = pRequest->GetBuffer();

    for( s32 i=0 ; i<2 ; i++ )
    {
        if( pBuffer == (void*)m_ReadBuffers[i] )
        {
            return i;
        }
    }

    ASSERT( 0 );
    return -1;
}

//==============================================================================

void audio_stream_runtime::ReadCallback( io_request* pRequest, audio_stream* pStream, s32 ReadBufferIndex )
{
    ASSERT( pRequest->GetStatus() == io_request::COMPLETED );
    ASSERT( pStream );
    ASSERT( (ReadBufferIndex >= 0) && (ReadBufferIndex < 2) );
    (void)pRequest;
    ASSERT( !Runtime().Decoders.UsesRuntimeDecode( pStream ) );

    switch( pStream->Type )
    {
        case MONO_STREAM:
        {
            switch( pStream->CompressionType )
            {
                case ADPCM:
                case PCM:
                {
                    x_memcpy( (void*)pStream->ARAM[LEFT_CHANNEL][pStream->ARAMWriteBuffer],
                              (void*)m_ReadBuffers[ReadBufferIndex],
                              STREAM_BUFFER_SIZE );
                    break;
                }

                default:
                {
                    ASSERT( 0 );
                    break;
                }
            }

            break;
        }

        case STEREO_STREAM:
        {
            switch( pStream->CompressionType )
            {
                case ADPCM:
                case PCM:
                {
                    x_memcpy( (void*)pStream->ARAM[LEFT_CHANNEL][pStream->ARAMWriteBuffer],
                              (void*)m_ReadBuffers[ReadBufferIndex],
                              STREAM_BUFFER_SIZE );

                    x_memcpy( (void*)pStream->ARAM[RIGHT_CHANNEL][pStream->ARAMWriteBuffer],
                              (void*)(m_ReadBuffers[ReadBufferIndex]+STREAM_BUFFER_SIZE),
                              STREAM_BUFFER_SIZE );
                    break;
                }

                default:
                {
                    ASSERT( 0 );
                    break;
                }
            }

            break;
        }

        case INACTIVE:
        {
            break;
        }

        default:
        {
            ASSERT( 0 );
            break;
        }
    }

    pStream->ARAMWriteBuffer ^= 1;
}

//==============================================================================

void audio_stream_runtime::ReadComplete( io_request* pRequest )
{
    audio_stream* pStream = FindRequestStream( pRequest );
    s32 ReadBufferIndex = GetRequestReadBufferIndex( pRequest );

    if( !pStream || (ReadBufferIndex < 0) )
    {
        return;
    }

    ReadCallback( pRequest, pStream, ReadBufferIndex );
}

//==============================================================================

void audio_stream_runtime::WarmComplete( io_request* pRequest )
{
    audio_stream* pStream = FindRequestStream( pRequest );
    s32 ReadBufferIndex = GetRequestReadBufferIndex( pRequest );

    if( !pStream || (ReadBufferIndex < 0) )
    {
        return;
    }

    ReadCallback( pRequest, pStream, ReadBufferIndex );

    if( pStream->State == STREAM_WARMING )
    {
        pStream->State = STREAM_STARTING;
    }
}

//==============================================================================

xbool audio_stream_runtime::OpenFile( audio_stream* pStream )
{
    ASSERT( IsValidStream( pStream ) );

    if( !IsValidStream( pStream ) )
    {
        return FALSE;
    }

    char Filename[AUDIO_PACKAGE_FILENAME_LENGTH];
    Filename[0] = '\0';

    xbool Result = GetOpenFilename( pStream, Filename, sizeof( Filename ) );

    if( !Result )
    {
        return FALSE;
    }

    io_open_file* pFile = m_FileSystem.Open( Filename, "rb" );
    ASSERT( pFile );

    if( pFile == NULL )
    {
        return FALSE;
    }

    m_FileSystem.EnableChecksum( pFile, FALSE );

    Result = FALSE;

    if( (pStream->Type != INACTIVE) &&
        (pStream->State == STREAM_OPENING) &&
        (pStream->FileHandle == NULL) )
    {
        pStream->FileHandle = pFile;
        pFile  = NULL;
        Result = TRUE;
    }

    if( pFile )
    {
        m_FileSystem.Close( pFile );
    }

    return Result;
}

//==============================================================================

xbool audio_stream_runtime::GetOpenFilename( audio_stream* pStream, char* pFilename, s32 FilenameBytes )
{
    ASSERT( pStream->FileHandle == NULL );
    ASSERT( pFilename );
    ASSERT( FilenameBytes > 0 );

    if( (pFilename == NULL) || (FilenameBytes <= 0) )
    {
        return FALSE;
    }

    pFilename[0] = '\0';

    xbool Result = FALSE;

    if( (pStream->Type != INACTIVE) &&
        (pStream->State == STREAM_OPENING) &&
        pStream->pChannel[0] &&
        Runtime().Backend.IsValidChannel( pStream->pChannel[0] ) &&
        pStream->pChannel[0]->pElement &&
        pStream->pChannel[0]->pElement->pVoice &&
        pStream->pChannel[0]->pElement->pVoice->pPackage )
    {
        char* pIdentifier = pStream->pChannel[0]->pElement->pVoice->pPackage->GetPackageIdentifier();
        ASSERT( pIdentifier );

        if( pIdentifier )
        {
            x_strncpy( pFilename, pIdentifier, FilenameBytes-1 );
            pFilename[FilenameBytes-1] = '\0';
            Result = TRUE;
        }
    }

    return Result;
}

//==============================================================================

void audio_stream_runtime::CloseFile( audio_stream* pStream )
{
    ASSERT( IsValidStream( pStream ) );

    if( !IsValidStream( pStream ) )
    {
        return;
    }

    io_open_file* pFile = NULL;
    audio_stream_decoder* pDecoder = NULL;

    DetachFile( pStream, pFile, pDecoder );

    Runtime().Decoders.Close( pDecoder );

    if( pFile )
    {
        m_FileSystem.Close( pFile );
    }
}

//==============================================================================

void audio_stream_runtime::DetachFile( audio_stream* pStream, io_open_file*& pFile, audio_stream_decoder*& pDecoder )
{
    if( pStream->pDecoder )
    {
        pDecoder = pStream->pDecoder;
        pStream->pDecoder = NULL;
    }

    if( pStream->FileHandle )
    {
        pFile = pStream->FileHandle;
        pStream->FileHandle = NULL;
    }
}

//==============================================================================

xbool audio_stream_runtime::Warm( audio_stream* pStream, io_request::callback_fn* pCallback )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_stream_runtime::Warm" );

#ifdef LOG_AUDIO_STREAM_WARM_STREAM
    voice* pVoice = NULL;

    if( pStream && pStream->pChannel[0] && pStream->pChannel[0]->pElement )
    {
        pVoice = pStream->pChannel[0]->pElement->pVoice;
    }
    LOG_MESSAGE( LOG_AUDIO_STREAM_WARM_STREAM, "pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
#endif

    ASSERT( IsValidStream( pStream ) );

    if( !IsValidStream( pStream ) )
    {
        return FALSE;
    }

    xbool Result = WarmStream( pStream, pCallback );

    return Result;
}

//==============================================================================

xbool audio_stream_runtime::WarmStream( audio_stream* pStream, io_request::callback_fn* pCallback )
{
    xbool Result = FALSE;

    if( FindSlot( pStream ) < 0 )
    {
        return FALSE;
    }

    io_request::status status = pStream->pIoRequest->GetStatus();
#if !defined(TARGET_DEV)
    ASSERT( status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED );
    ASSERT( pStream->Type != INACTIVE );
#endif
    if( (pStream->Type != INACTIVE) &&
        (pStream->State != STREAM_STOPPING) &&
        (pStream->State != STREAM_CLOSING) &&
        (pStream->FileHandle != NULL) &&
        (status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED) )
    {
        pStream->State = STREAM_WARMING;

        pStream->WaveformCursor  = 0;
        pStream->ARAMWriteBuffer = 0;

        if( pCallback == NULL )
        {
            pCallback = WarmRequestCallback;
        }

        return QueueRead( pStream, pCallback );
    }

    return Result;
}

//==============================================================================

xbool audio_stream_runtime::Read( audio_stream* pStream, io_request::callback_fn* pCallback )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_stream_runtime::Read" );

#ifdef LOG_AUDIO_STREAM_READ_STREAM
    voice* pVoice = NULL;

    if( pStream && pStream->pChannel[0] && pStream->pChannel[0]->pElement )
    {
        pVoice = pStream->pChannel[0]->pElement->pVoice;
    }
    LOG_MESSAGE( LOG_AUDIO_STREAM_READ_STREAM, "pStream: 0x%08x, pVoice: 0x%08x", pStream, pVoice );
#endif

    ASSERT( IsValidStream( pStream ) );

    if( !IsValidStream( pStream ) )
    {
        return FALSE;
    }

    xbool Result = QueueRead( pStream, pCallback );

    return Result;
}

//==============================================================================

xbool audio_stream_runtime::QueueRead( audio_stream* pStream, io_request::callback_fn* pCallback )
{
    xbool Result = FALSE;

    if( FindSlot( pStream ) < 0 )
    {
        return FALSE;
    }

    io_request::status status = pStream->pIoRequest->GetStatus();
#if !defined(TARGET_DEV)
    ASSERT( status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED );
    ASSERT( pStream->Type != INACTIVE );
#endif

    if( (pStream->Type != INACTIVE) &&
        (pStream->State != STREAM_STOPPING) &&
        (pStream->State != STREAM_CLOSING) &&
        (pStream->FileHandle != NULL) &&
        (status == io_request::NOT_QUEUED || status == io_request::COMPLETED || status == io_request::FAILED) )
    {
        if( pCallback == NULL )
        {
            pCallback = ReadRequestCallback;
        }

        SetRequest( pStream, pCallback );

        m_ActiveReadBuffer ^= 1;

        pStream->WaveformCursor += pStream->ReadBufferSize;

        if( pStream->WaveformCursor >= pStream->WaveformLength )
        {
            pStream->StreamDone = TRUE;
        }

        m_Io.QueueRequest( pStream->pIoRequest );

        Result = TRUE;
    }

    return Result;
}

//==============================================================================

void audio_stream_runtime::UpdateDecoded( audio_stream* pStream )
{
    if( !pStream )
    {
        return;
    }

    DecodeAvailable( pStream );
}

//==============================================================================

void audio_stream_runtime::DecodeAvailable( audio_stream* pStream )
{
    while( WriteDecodedChunk( pStream ) )
    {
    }
}

//==============================================================================

void audio_stream_runtime::SetRequest( audio_stream* pStream, io_request::callback_fn* pCallback )
{
    ASSERT( IsValidStream( pStream ) );

    pStream->pIoRequest->SetRequest( pStream->FileHandle,
                                     (void*)m_ReadBuffers[m_ActiveReadBuffer],
                                     pStream->FileHandle->Offset+pStream->WaveformOffset+pStream->WaveformCursor,
                                     pStream->ReadBufferSize,
                                     io_request::HIGH_PRIORITY,
                                     FALSE,
                                     0,
                                     (uaddr)this,
                                     io_request::READ_OP,
                                     pCallback );
}
