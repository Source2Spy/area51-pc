//==============================================================================
//
//  audio_mp3_stream_decoder_state.cpp
//
//  minimp3 decoder state.
//
//==============================================================================

//==============================================================================
//  MINIMP3 IMPLEMENTATION
//==============================================================================

#define MINIMP3_IMPLEMENTATION
#include "Minimp3/minimp3.h"
#undef MINIMP3_IMPLEMENTATION

//==============================================================================
//  INCLUDES
//==============================================================================

#include "audio_mp3_stream_decoder_state.hpp"
#include "Audio/audio_types.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_mp3_stream_decoder_state::audio_mp3_stream_decoder_state( void )
{
    x_memset( this, 0, sizeof( *this ) );
}

//==============================================================================

void audio_mp3_stream_decoder_state::Reset( const audio_stream* pStream )
{
    ASSERT( pStream );

    x_memset( this, 0, sizeof( *this ) );
    mp3dec_init( &Decoder );

    Channels   = (pStream->Type == STEREO_STREAM) ? 2 : 1;
    EndOfInput = FALSE;
}

//==============================================================================

void audio_mp3_stream_decoder_state::Compact( void )
{
    if( InputCursor > 0 )
    {
        if( InputCursor < InputBytes )
        {
            x_memmove( InputBuffer,
                       InputBuffer + InputCursor,
                       InputBytes - InputCursor );
            InputBytes -= InputCursor;
        }
        else
        {
            InputBytes = 0;
        }

        InputCursor = 0;
    }
}

//==============================================================================

s32 audio_mp3_stream_decoder_state::AvailableBytes( void ) const
{
    if( InputBytes <= InputCursor )
    {
        return 0;
    }

    return InputBytes - InputCursor;
}

//==============================================================================

void audio_mp3_stream_decoder_state::Finish( void )
{
    if( DecodeComplete )
    {
        return;
    }

    DecodeComplete = TRUE;
}
