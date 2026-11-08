//==============================================================================
//
//  Speex.hpp
//
//==============================================================================

#ifndef _SPEEX_HPP_
#define _SPEEX_HPP_

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"

#include "speex/speex.h"

//==============================================================================
//  DEFINES
//==============================================================================

#define SPEEX8_SAMPLES_PER_FRAME                  (160)
#define SPEEX8_QUALITY                            (5)
#define SPEEX8_BITS_PER_FRAME                     (220)
#define SPEEX8_BYTES_PER_EFRAME                   ((SPEEX8_BITS_PER_FRAME + 7) / 8)
#define SPEEX8_DECODE_BUFFER_BYTES                (1024)

//==============================================================================
//  STRUCTS
//==============================================================================

struct SPEEX8
{
    void*         encode_state;
    void*         decode_state;
    spx_int16_t   encode_in[ SPEEX8_SAMPLES_PER_FRAME ];
    s32           encode_in_samples;
    unsigned char decode_in[ SPEEX8_DECODE_BUFFER_BYTES ];
    s32           decode_in_bytes;
    SpeexBits     encode_bits;
    SpeexBits     decode_bits;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

#if defined( _LANGUAGE_C_PLUS_PLUS ) || defined( __cplusplus ) || defined( c_plusplus )
extern "C"
{
#endif

xbool SpeexInit   ( void );
xbool SpeexKill   ( void );
xbool SpeexReset  ( void );
xbool SpeexEncode ( const s16* pSrc, const u32 SrcSize, u8* pDest, s32* pDestSize );
xbool SpeexDecode ( const u8* pSrc, const u32 SrcSize, s16* pDest, s32* pDestSize );

#if defined( _LANGUAGE_C_PLUS_PLUS ) || defined( __cplusplus ) || defined( c_plusplus )
}
#endif

//==============================================================================
#endif // _SPEEX_HPP_
//==============================================================================
