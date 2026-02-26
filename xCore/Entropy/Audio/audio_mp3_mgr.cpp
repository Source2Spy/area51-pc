//==============================================================================
//
//  audio_mp3_mgr.cpp
//
//  minimp3 based mp3 decoder.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

// Let it be only for PC, for now...

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "audio_stream_mgr.hpp"
#include "audio_channel_mgr.hpp"
#include "audio_hardware.hpp"
#include "audio_mp3_mgr.hpp"

//==============================================================================
//  MINIMP3 INCLUDES
//==============================================================================

#define MINIMP3_IMPLEMENTATION
#include "../../3rdParty/minimp3/minimp3.h"
#undef MINIMP3_IMPLEMENTATION

//==============================================================================
//  STRUCTURES
//==============================================================================

struct audio_mp3_mgr::mp3_decoder_state
{
    mp3dec_t                Decoder;
    mp3dec_frame_info_t     FrameInfo;
    s32                     InputBytes;
    s32                     InputCursor;
    s32                     BytesConsumed;
    s32                     SamplesAvailable;
    s32                     SampleOffset;
    s32                     Channels;
    xbool                   EndOfStream;
    s16                     SamplesBuffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
    u8                      InputBuffer[MP3_BUFFER_SIZE * 2];
};

//------------------------------------------------------------------------------

xbool audio_mp3_mgr::s_Initialized = FALSE;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

audio_mp3_mgr g_AudioMP3Mgr;

//==============================================================================
// FUNCTIONS
//==============================================================================

xbool audio_mp3_mgr::IsValidStream( const audio_stream* pStream )
{
    if( pStream == NULL )
        return FALSE;

    return (pStream >= &g_AudioStreamMgr.m_AudioStreams[0]) &&
           (pStream <= &g_AudioStreamMgr.m_AudioStreams[MAX_AUDIO_STREAMS-1]);
}

//==============================================================================

s32 audio_mp3_mgr::mp3_fetch_data( audio_stream* pStream, void* pBuffer, s32 nBytes, s32 Offset )
{
    (void)Offset;

    ASSERT( pStream );
    ASSERT( IsValidStream( pStream ) );
    ASSERT( (Offset == 0) || (Offset == -1) );

    if( Offset == 0 )
    {
        ASSERT( pStream->CursorMP3 < MP3_BUFFER_SIZE );
        pStream->CursorMP3 = 0;
    }

    s32 Previous    = pStream->CursorMP3;
    s32 Current     = Previous + nBytes;
    xbool bTransition = FALSE;

    if( Current > (MP3_BUFFER_SIZE * 2) )
    {
        ASSERT( Previous <= (MP3_BUFFER_SIZE * 2) );

        s32 Length = (MP3_BUFFER_SIZE * 2) - Previous;
        if( Length )
        {
            x_memcpy( pBuffer, (void*)(pStream->MainRAM[0] + Previous), Length );
            x_memset( (void*)(pStream->MainRAM[0] + Previous), 0, Length );
            pBuffer = (void*)((uaddr)pBuffer + Length);
        }

        Current -= (MP3_BUFFER_SIZE * 2);
        x_memcpy( pBuffer, (void*)pStream->MainRAM[0], Current );
        x_memset( (void*)pStream->MainRAM[0], 0, Current );
    }
    else
    {
        x_memcpy( pBuffer, (void*)(pStream->MainRAM[0] + Previous), nBytes );
        x_memset( (void*)(pStream->MainRAM[0] + Previous), 0, nBytes );
    }

    pStream->CursorMP3 = Current;

    if( Previous < MP3_BUFFER_SIZE )
    {
        bTransition = (Current > MP3_BUFFER_SIZE);
    }
    else
    {
        bTransition = (Current <= MP3_BUFFER_SIZE);
    }

    if( bTransition && !pStream->StreamDone )
    {
        // Fill the read buffer.
        g_AudioStreamMgr.ReadStream( pStream );
    }

    return nBytes;
}

//==============================================================================

void audio_mp3_mgr::mp3_state_reset( mp3_decoder_state& State, stream_type Type )
{
    mp3dec_init( &State.Decoder );
    x_memset( &State.FrameInfo, 0, sizeof( State.FrameInfo ) );
    State.InputBytes        = 0;
    State.InputCursor       = 0;
    State.BytesConsumed     = 0;
    State.SamplesAvailable  = 0;
    State.SampleOffset      = 0;
    State.Channels          = (Type == STEREO_STREAM) ? 2 : 1;
    State.EndOfStream       = FALSE;
}

//==============================================================================

void audio_mp3_mgr::mp3_state_compact( mp3_decoder_state& State )
{
    if( State.InputCursor > 0 )
    {
        if( State.InputCursor < State.InputBytes )
        {
            x_memmove( State.InputBuffer,
                       State.InputBuffer + State.InputCursor,
                       State.InputBytes - State.InputCursor );
            State.InputBytes -= State.InputCursor;
        }
        else
        {
            State.InputBytes = 0;
        }
        State.InputCursor = 0;
    }
}

//==============================================================================

void audio_mp3_mgr::mp3_state_refill( audio_stream* pStream, mp3_decoder_state& State )
{
    mp3_state_compact( State );

    while( (State.InputBytes < MP3_BUFFER_SIZE) && !State.EndOfStream )
    {
        s32 Remaining = (s32)pStream->WaveformLength - State.BytesConsumed;
        if( Remaining <= 0 )
        {
            State.EndOfStream = TRUE;
            break;
        }

        s32 Capacity = (s32)sizeof( State.InputBuffer ) - State.InputBytes;
        if( Capacity <= 0 )
            break;

        s32 ToFetch = x_min( Remaining, MP3_BUFFER_SIZE );
        ToFetch     = x_min( ToFetch, Capacity );

        if( ToFetch <= 0 )
            break;

        s32 OffsetValue = (State.BytesConsumed == 0) ? 0 : -1;
        s32 Bytes = mp3_fetch_data( pStream, State.InputBuffer + State.InputBytes, ToFetch, OffsetValue );
        if( Bytes <= 0 )
        {
            State.EndOfStream = TRUE;
            break;
        }

        State.InputBytes    += Bytes;
        State.BytesConsumed += Bytes;

        if( Bytes < ToFetch )
        {
            State.EndOfStream = TRUE;
            break;
        }
    }
}

//==============================================================================

s32 audio_mp3_mgr::mp3_state_available_bytes( const mp3_decoder_state& State )
{
    if( State.InputBytes <= State.InputCursor )
        return 0;
    return State.InputBytes - State.InputCursor;
}

//==============================================================================

s32 audio_mp3_mgr::mp3_state_decode_frame( audio_stream* pStream, mp3_decoder_state& State )
{
    State.SamplesAvailable = 0;
    State.SampleOffset     = 0;

    mp3_state_refill( pStream, State );

    s32 SkipCount = 0;
    const s32 MAX_SKIP_ATTEMPTS = 100;

    while( mp3_state_available_bytes( State ) > 0 )
    {
        if( SkipCount > MAX_SKIP_ATTEMPTS )
        {
            State.EndOfStream = TRUE;
            break;
        }

        s32 BytesLeft      = mp3_state_available_bytes( State );
        s32 SamplesDecoded = mp3dec_decode_frame( &State.Decoder,
                                                  State.InputBuffer + State.InputCursor,
                                                  BytesLeft,
                                                  State.SamplesBuffer,
                                                  &State.FrameInfo );
        s32 FrameBytes     = State.FrameInfo.frame_bytes;

        if( FrameBytes <= 0 )
        {
            if( State.EndOfStream ||
                (State.InputCursor == 0 &&
                 State.InputBytes >= (s32)sizeof( State.InputBuffer )) )
            {
                FrameBytes = (BytesLeft > 0) ? 1 : 0;
                SkipCount++;
            }
        }
        else
        {
            SkipCount = 0;
        }

        State.InputCursor += FrameBytes;
        if( State.InputCursor > State.InputBytes )
            State.InputCursor = State.InputBytes;

        if( SamplesDecoded > 0 )
        {
            if( State.FrameInfo.channels )
                State.Channels = State.FrameInfo.channels;

            State.SamplesAvailable = SamplesDecoded;
            State.SampleOffset     = 0;
            return SamplesDecoded;
        }

        if( State.EndOfStream && (mp3_state_available_bytes( State ) <= 0) )
            break;

        mp3_state_refill( pStream, State );
    }

    return 0;
}

//==============================================================================

audio_mp3_mgr::audio_mp3_mgr( void )
{
}

//==============================================================================

audio_mp3_mgr::~audio_mp3_mgr( void )
{
    if( s_Initialized )
    {
        Kill();
    }
}

//==============================================================================

void audio_mp3_mgr::Init( void )
{
    ASSERT( s_Initialized == FALSE );
    s_Initialized = TRUE;
}

//==============================================================================

void audio_mp3_mgr::Kill( void )
{
    ASSERT( s_Initialized );
    
    for( s32 i = 0; i < MAX_AUDIO_STREAMS; i++ )
    {
        audio_stream* pStream = &g_AudioStreamMgr.m_AudioStreams[i];
        if( pStream->HandleMP3 )
        {
            Close( pStream );
        }
    }
    
    s_Initialized = FALSE;
}

//==============================================================================

void audio_mp3_mgr::Open( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );

    pStream->CursorMP3 = 0;

    x_memset( (void*)pStream->MainRAM[1], 0, MP3_BUFFER_SIZE );

    if( pStream->HandleMP3 )
    {
        mp3_decoder_state* pOldState = (mp3_decoder_state*)pStream->HandleMP3;
        x_free( pOldState );
        pStream->HandleMP3 = NULL;
    }

    mp3_decoder_state* pState = (mp3_decoder_state*)x_malloc( sizeof( mp3_decoder_state ) );
    ASSERT( pState );

    mp3_state_reset( *pState, pStream->Type );

    pStream->HandleMP3 = pState;
}

//==============================================================================

void audio_mp3_mgr::Close( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );

    if( pStream->HandleMP3 )
    {
        mp3_decoder_state* pState = (mp3_decoder_state*)pStream->HandleMP3;
        x_free( pState );
        pStream->HandleMP3 = NULL;
    }
}

//==============================================================================

void audio_mp3_mgr::Seek( audio_stream* pStream )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );

    if( pStream->HandleMP3 == NULL )
        return;

    // Lock the audio hardware.
    g_AudioHardware.Lock();

    pStream->CursorMP3 = 0;

    mp3_decoder_state* pState = (mp3_decoder_state*)pStream->HandleMP3;
    mp3_state_reset( *pState, pStream->Type );

    // Unlock it now.
    g_AudioHardware.Unlock();
}

//==============================================================================

void audio_mp3_mgr::Decode( audio_stream* pStream, s16* pBufferL, s16* pBufferR, s32 nSamples )
{
    ASSERT( s_Initialized );
    ASSERT( IsValidStream( pStream ) );
    ASSERT( nSamples >= 0 );
    ASSERT( nSamples <= 512 );

    if( (nSamples <= 0) || (nSamples > 512) )
        nSamples = 512;

    // MP3 Stream closed?
    if( (pStream==NULL) || (pStream->HandleMP3 == NULL) )
    {
        if( pBufferL )
            x_memset( pBufferL, 0, nSamples * sizeof(s16) );
        if( pBufferR )
            x_memset( pBufferR, 0, nSamples * sizeof(s16) );
        return;
    }

    mp3_decoder_state* pState = (mp3_decoder_state*)pStream->HandleMP3;

    // Lock the audio hardware.
    g_AudioHardware.Lock();

    xbool bIsStereo = (pStream->Type == STEREO_STREAM);
    s16* pOutL      = pBufferL;
    s16* pOutR      = pBufferR;
    s32  SamplesNeeded = nSamples;

    while( SamplesNeeded > 0 )
    {
        if( pState->SamplesAvailable <= 0 )
        {
            g_AudioHardware.Unlock();
            s32 Decoded = mp3_state_decode_frame( pStream, *pState );
            g_AudioHardware.Lock();
            if( Decoded <= 0 )
                break;
        }

        s32 CopyCount = x_min( pState->SamplesAvailable, SamplesNeeded );

        if( bIsStereo )
        {
            ASSERT( pOutL );
            ASSERT( pOutR );

            if( pState->Channels == 2 )
            {
                s16* pSrc = pState->SamplesBuffer + (pState->SampleOffset * 2);
                for( s32 i = 0; i < CopyCount; i++ )
                {
                    *pOutL++ = *pSrc++;
                    *pOutR++ = *pSrc++;
                }
            }
            else
            {
                s16* pSrc = pState->SamplesBuffer + pState->SampleOffset;
                for( s32 i = 0; i < CopyCount; i++ )
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

            if( pState->Channels == 2 )
            {
                s16* pSrc = pState->SamplesBuffer + (pState->SampleOffset * 2);
                for( s32 i = 0; i < CopyCount; i++ )
                {
                    *pOutL++ = pSrc[0];
                    pSrc += 2;
                }
            }
            else
            {
                s16* pSrc = pState->SamplesBuffer + pState->SampleOffset;
                x_memcpy( pOutL, pSrc, CopyCount * sizeof(s16) );
                pOutL += CopyCount;
            }
        }

        pState->SampleOffset    += CopyCount;
        pState->SamplesAvailable -= CopyCount;
        SamplesNeeded           -= CopyCount;
    }

    // Unlock it now.
    g_AudioHardware.Unlock();

    if( SamplesNeeded > 0 )
    {
        if( pOutL )
            x_memset( pOutL, 0, SamplesNeeded * sizeof(s16) );
        if( bIsStereo && pOutR )
            x_memset( pOutR, 0, SamplesNeeded * sizeof(s16) );
    }
}

//==============================================================================