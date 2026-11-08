//==============================================================================
//
//  audio_mp3_stream_decoder.cpp
//
//  runtime minimp3 stream decoder.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "audio_mp3_stream_decoder.hpp"
#include "Audio/audio_types.hpp"
#include "IOManager/io_filesystem.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_mp3_stream_decoder::audio_mp3_stream_decoder( io_fs& FileSystem, audio_stream* pStream ) :
    m_FileSystem( FileSystem ),
    m_Stream( *pStream ),
    m_Service( HNULL ),
    m_Wake( DECODE_QUEUE_SIZE, 0 ),
    m_ChunkOffset( 0 ),
    m_DecodeComplete( FALSE )
{
    m_State.Reset( pStream );
    x_AtomicStoreRelease( &m_Stop, FALSE );
    x_AtomicStoreRelease( &m_ReadCursor, 0 );
    x_AtomicStoreRelease( &m_WriteCursor, 0 );
    x_WorkerServiceStart( ServiceEntry, this, "Audio MP3 Decode", m_Service );
}

//==============================================================================

audio_mp3_stream_decoder::~audio_mp3_stream_decoder( void )
{
    RequestStop();

    if( m_Service.IsNonNull() )
    {
        x_WorkerServiceWait( m_Service );
        x_WorkerServiceRelease( m_Service );
    }
}

//==============================================================================

xbool audio_mp3_stream_decoder::IsValid( void ) const
{
    return m_Service.IsNonNull();
}

//==============================================================================

xbool audio_mp3_stream_decoder::RequestStop( void )
{
    x_AtomicStoreRelease( &m_Stop, TRUE );
    m_Wake.Release( X_TH_NOBLOCK );
    return m_Service.IsNull() || x_WorkerServiceIsDone( m_Service );
}

//==============================================================================

void audio_mp3_stream_decoder::ServiceEntry( void* pData )
{
    static_cast<audio_mp3_stream_decoder*>( pData )->ServiceLoop();
}

//==============================================================================

void audio_mp3_stream_decoder::ServiceLoop( void )
{
    while( !x_AtomicLoadAcquire( &m_Stop ) )
    {
        const u32 Write = x_AtomicLoadRelaxed( &m_WriteCursor );
        const u32 Read  = x_AtomicLoadAcquire( &m_ReadCursor );

        if( Write - Read >= DECODE_QUEUE_SIZE )
        {
            m_Wake.Acquire( X_TH_BLOCK );
            continue;
        }

        decoded_chunk& Chunk = m_Chunks[Write % DECODE_QUEUE_SIZE];
        Chunk.Result         = DecodeNow( &m_Stream, Chunk.Left, Chunk.Right,
                                          audio_mp3_stream_decoder_state::DECODE_SAMPLES );
        x_AtomicStoreRelease( &m_WriteCursor, Write + 1 );

        if( Chunk.Result.EndOfStream )
        {
            return;
        }
    }
}

//==============================================================================

audio_decode_result audio_mp3_stream_decoder::Decode( audio_stream* pStream, s16* pBufferL, s16* pBufferR, s32 nSamples )
{
    ASSERT( pStream );
    ASSERT( nSamples >= 0 && nSamples <= audio_mp3_stream_decoder_state::DECODE_SAMPLES );
    audio_decode_result Result = { 0, FALSE };
    nSamples                   = x_min( nSamples, static_cast<s32>( audio_mp3_stream_decoder_state::DECODE_SAMPLES ) );

    if( m_DecodeComplete && nSamples > 0 )
    {
        Result.EndOfStream = TRUE;
        return Result;
    }

    while( Result.FramesWritten < nSamples )
    {
        const u32 Read  = x_AtomicLoadRelaxed( &m_ReadCursor );
        const u32 Write = x_AtomicLoadAcquire( &m_WriteCursor );

        if( Read == Write )
        {
            break;
        }

        const decoded_chunk& Chunk = m_Chunks[Read % DECODE_QUEUE_SIZE];
        const s32            Count = x_min( Chunk.Result.FramesWritten - m_ChunkOffset,
                                            nSamples - Result.FramesWritten );

        if( Count > 0 )
        {
            x_memcpy( pBufferL + Result.FramesWritten, Chunk.Left + m_ChunkOffset, Count * sizeof( s16 ) );

            if( pStream->Type == STEREO_STREAM )
            {
                x_memcpy( pBufferR + Result.FramesWritten, Chunk.Right + m_ChunkOffset, Count * sizeof( s16 ) );
            }
            m_ChunkOffset += Count;
            Result.FramesWritten += Count;
        }

        if( m_ChunkOffset == Chunk.Result.FramesWritten )
        {
            Result.EndOfStream = Chunk.Result.EndOfStream;
            m_DecodeComplete   = Result.EndOfStream;
            m_ChunkOffset      = 0;
            x_AtomicStoreRelease( &m_ReadCursor, Read + 1 );
            m_Wake.Release( X_TH_NOBLOCK );

            if( Result.EndOfStream )
            {
                break;
            }
        }
    }

    return Result;
}

//==============================================================================

audio_decode_result audio_mp3_stream_decoder::DecodeNow( audio_stream* pStream, s16* pBufferL, s16* pBufferR, s32 nSamples )
{
    ASSERT( pStream );
    ASSERT( nSamples >= 0 );
    ASSERT( nSamples <= audio_mp3_stream_decoder_state::DECODE_SAMPLES );

    audio_decode_result Result;
    Result.FramesWritten = 0;
    Result.EndOfStream   = FALSE;

    if( nSamples <= 0 )
    {
        return Result;
    }

    if( nSamples > audio_mp3_stream_decoder_state::DECODE_SAMPLES )
    {
        nSamples = audio_mp3_stream_decoder_state::DECODE_SAMPLES;
    }

    s16*  pOutL         = pBufferL;
    s16*  pOutR         = pBufferR;
    s32   SamplesNeeded = nSamples;

    while( SamplesNeeded > 0 )
    {
        if( m_State.SamplesAvailable <= 0 )
        {
            s32 Decoded = DecodeFrame( pStream );

            if( Decoded <= 0 )
            {
                break;
            }
        }

        s32 CopyCount = x_min( m_State.SamplesAvailable, SamplesNeeded );

        CopySamples( pStream->Type, pOutL, pOutR, CopyCount );

        m_State.SampleOffset     += CopyCount;
        m_State.SamplesAvailable -= CopyCount;
        Result.FramesWritten     += CopyCount;
        SamplesNeeded            -= CopyCount;
    }

    Result.EndOfStream = (m_State.DecodeComplete && (m_State.SamplesAvailable <= 0));
    return Result;
}

//==============================================================================

s32 audio_mp3_stream_decoder::Read( audio_stream* pStream, void* pBuffer, s32 nBytes )
{
    ASSERT( pStream );
    ASSERT( pBuffer );
    ASSERT( nBytes >= 0 );

    if( (nBytes <= 0) || (pStream->FileHandle == NULL) )
    {
        m_State.EndOfInput = TRUE;
        return 0;
    }

    s32 Remaining = (s32)pStream->WaveformLength - m_State.FileCursor;

    if( Remaining <= 0 )
    {
        m_State.EndOfInput = TRUE;
        return 0;
    }

    s32 FileOffset = (s32)pStream->WaveformOffset + m_State.FileCursor;
    s32 FileLeft   = pStream->FileHandle->Length - FileOffset;

    if( FileLeft <= 0 )
    {
        m_State.EndOfInput = TRUE;
        return 0;
    }

    s32 BytesToRead = x_min( nBytes, Remaining );
    BytesToRead     = x_min( BytesToRead, FileLeft );

    pStream->FileHandle->Position = FileOffset;

    s32 BytesRead = m_FileSystem.Read( pStream->FileHandle, (byte*)pBuffer, BytesToRead );

    if( BytesRead < 0 )
    {
        BytesRead = 0;
    }

    m_State.FileCursor += BytesRead;

    if( (BytesRead < BytesToRead) || (m_State.FileCursor >= (s32)pStream->WaveformLength) )
    {
        m_State.EndOfInput = TRUE;
    }

    return BytesRead;
}

//==============================================================================

s32 audio_mp3_stream_decoder::Refill( audio_stream* pStream )
{
    s32 BytesRead = 0;

    m_State.Compact();

    while( (m_State.InputBytes < audio_mp3_stream_decoder_state::INPUT_BUFFER_SIZE) && !m_State.EndOfInput )
    {
        s32 Capacity = (s32)sizeof( m_State.InputBuffer ) - m_State.InputBytes;

        if( Capacity <= 0 )
        {
            break;
        }

        s32 Bytes = Read( pStream, m_State.InputBuffer + m_State.InputBytes, Capacity );

        if( Bytes <= 0 )
        {
            break;
        }

        m_State.InputBytes += Bytes;
        BytesRead          += Bytes;
    }

    return BytesRead;
}

//==============================================================================

s32 audio_mp3_stream_decoder::DecodeFrame( audio_stream* pStream )
{
    m_State.SamplesAvailable = 0;
    m_State.SampleOffset     = 0;

    Refill( pStream );

    while( m_State.AvailableBytes() > 0 )
    {
        mp3dec_frame_info_t FrameInfo;
        s32                 BytesLeft;
        s32                 SamplesDecoded;
        s32                 FrameBytes;

        x_memset( &FrameInfo, 0, sizeof( FrameInfo ) );

        BytesLeft      = m_State.AvailableBytes();
        SamplesDecoded = mp3dec_decode_frame( &m_State.Decoder,
                                              m_State.InputBuffer + m_State.InputCursor,
                                              BytesLeft,
                                              m_State.SamplesBuffer,
                                              &FrameInfo );
        FrameBytes     = FrameInfo.frame_bytes;

        if( (SamplesDecoded > 0) &&
            (FrameBytes > 0) &&
            (FrameInfo.layer == 3) &&
            (FrameInfo.channels > 0) &&
            (FrameInfo.channels <= 2) )
        {
            m_State.InputCursor += x_min( FrameBytes, BytesLeft );
            m_State.Channels = FrameInfo.channels;

            m_State.SamplesAvailable = SamplesDecoded;
            m_State.SampleOffset     = 0;

            return SamplesDecoded;
        }

        if( FrameBytes > 0 )
        {
            m_State.InputCursor += x_min( FrameBytes, BytesLeft );
        }
        else if( !m_State.EndOfInput )
        {
            if( Refill( pStream ) > 0 )
            {
                continue;
            }

            if( m_State.AvailableBytes() >= audio_mp3_stream_decoder_state::INPUT_BUFFER_SIZE )
            {
                m_State.InputCursor++;
            }
        }
        else
        {
            m_State.InputCursor = m_State.InputBytes;
        }

        Refill( pStream );
    }

    m_State.Finish();
    return 0;
}

//==============================================================================

void audio_mp3_stream_decoder::CopySamples( stream_type Type, s16*& pOutL, s16*& pOutR, s32 nSamples )
{
    xbool bIsStereo = (Type == STEREO_STREAM);

    if( bIsStereo )
    {
        ASSERT( pOutL );
        ASSERT( pOutR );

        if( m_State.Channels == 2 )
        {
            s16* pSrc = m_State.SamplesBuffer + (m_State.SampleOffset * 2);

            for( s32 i = 0; i < nSamples; i++ )
            {
                *pOutL++ = *pSrc++;
                *pOutR++ = *pSrc++;
            }
        }
        else
        {
            s16* pSrc = m_State.SamplesBuffer + m_State.SampleOffset;

            for( s32 i = 0; i < nSamples; i++ )
            {
                s16 Sample = *pSrc++;
                *pOutL++ = Sample;
                *pOutR++ = Sample;
            }
        }
    }
    else
    {
        ASSERT( pOutL );

        if( m_State.Channels == 2 )
        {
            s16* pSrc = m_State.SamplesBuffer + (m_State.SampleOffset * 2);

            for( s32 i = 0; i < nSamples; i++ )
            {
                *pOutL++ = pSrc[0];
                pSrc += 2;
            }
        }
        else
        {
            s16* pSrc = m_State.SamplesBuffer + m_State.SampleOffset;
            x_memcpy( pOutL, pSrc, nSamples * sizeof(s16) );
            pOutL += nSamples;
        }
    }
}
