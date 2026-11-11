//==============================================================================
//
//  audio_stream_decoder.hpp
//
//==============================================================================

#ifndef AUDIO_STREAM_DECODER_HPP
#define AUDIO_STREAM_DECODER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Audio/audio_package_format.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_stream;

//------------------------------------------------------------------------------

struct audio_decode_result
{
    s32     FramesWritten;
    xbool   EndOfStream;
};

//==============================================================================
//  AUDIO STREAM DECODER CLASS
//==============================================================================

class audio_stream_decoder
{
public:
    virtual                         ~audio_stream_decoder   ( void ) {}

    // Returns TRUE when the decoder no longer uses the stream file.
    virtual xbool RequestStop( void )
    {
        return TRUE;
    }

    virtual audio_decode_result     Decode                  ( audio_stream* pStream,
                                                              s16*          pBufferL,
                                                              s16*          pBufferR,
                                                              s32           nSamples ) = 0;
};

//==============================================================================
#endif // AUDIO_STREAM_DECODER_HPP
//==============================================================================
