//==============================================================================
//
//  VoiceCodec.hpp
//
//==============================================================================

#ifndef VOICE_CODEC_HPP
#define VOICE_CODEC_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

namespace voice_codec
{
    // Fixed-size packets keep the existing FIFO and network framing.
    const s32 SAMPLE_RATE       = 16000;
    const s32 SAMPLES_PER_FRAME = 320;
    const s32 FRAME_BYTES       = 40;
}

//==============================================================================
//  TYPES
//==============================================================================

struct OpusEncoder;
struct OpusDecoder;

// Owns one encoder and one decoder. Each call consumes or produces exactly one
// frame: SAMPLES_PER_FRAME mono PCM samples or FRAME_BYTES of Opus payload.
class voice_codec_state
{
public:
                        voice_codec_state   ( void ) = default;
                       ~voice_codec_state   ( void );

                        voice_codec_state   ( const voice_codec_state& ) = delete;
    voice_codec_state&  operator=           ( const voice_codec_state& ) = delete;

    xbool               Init                ( void );
    void                Kill                ( void );
    xbool               ResetEncoder        ( void );
    xbool               ResetDecoder        ( void );
    xbool               Encode              ( const s16* pSrc, byte* pDest );
    xbool               Decode              ( const byte* pSrc, s16* pDest );

private:

    OpusEncoder*        m_pEncoder = NULL;
    OpusDecoder*        m_pDecoder = NULL;
};

//==============================================================================
#endif // VOICE_CODEC_HPP
//==============================================================================
