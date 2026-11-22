//==============================================================================
//
//  VoiceCodec.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "VoiceCodec.hpp"
#include "opus.h"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

voice_codec_state::~voice_codec_state( void )
{
    Kill();
}

//==============================================================================

xbool voice_codec_state::Init( void )
{
    if( m_pEncoder && m_pDecoder )
    {
        return TRUE;
    }

    const s32 Channels = 1;
    const s32 Bitrate  = voice_codec::FRAME_BYTES * 8 * voice_codec::SAMPLE_RATE /
                         voice_codec::SAMPLES_PER_FRAME;

    s32 Error;
    m_pEncoder = opus_encoder_create( voice_codec::SAMPLE_RATE, Channels,
                                      OPUS_APPLICATION_VOIP, &Error );
    if( !m_pEncoder || (Error != OPUS_OK) ||
        (opus_encoder_ctl( m_pEncoder, OPUS_SET_BITRATE( Bitrate ) ) != OPUS_OK) ||
        (opus_encoder_ctl( m_pEncoder, OPUS_SET_VBR( 0 ) ) != OPUS_OK) ||
        (opus_encoder_ctl( m_pEncoder, OPUS_SET_DTX( 0 ) ) != OPUS_OK) )
    {
        Kill();
        return FALSE;
    }

    m_pDecoder = opus_decoder_create( voice_codec::SAMPLE_RATE, Channels, &Error );
    if( !m_pDecoder || (Error != OPUS_OK) )
    {
        Kill();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void voice_codec_state::Kill( void )
{
    if( m_pEncoder )
    {
        opus_encoder_destroy( m_pEncoder );
        m_pEncoder = NULL;
    }

    if( m_pDecoder )
    {
        opus_decoder_destroy( m_pDecoder );
        m_pDecoder = NULL;
    }
}

//==============================================================================

xbool voice_codec_state::ResetEncoder( void )
{
    return m_pEncoder && (opus_encoder_ctl( m_pEncoder, OPUS_RESET_STATE ) == OPUS_OK);
}

//==============================================================================

xbool voice_codec_state::ResetDecoder( void )
{
    return m_pDecoder && (opus_decoder_ctl( m_pDecoder, OPUS_RESET_STATE ) == OPUS_OK);
}

//==============================================================================

xbool voice_codec_state::Encode( const s16* pSrc, byte* pDest )
{
    if( !m_pEncoder || !pSrc || !pDest )
    {
        return FALSE;
    }

    const s32 Bytes = opus_encode( m_pEncoder, pSrc, voice_codec::SAMPLES_PER_FRAME,
                                  pDest, voice_codec::FRAME_BYTES );
    if( Bytes <= 0 )
    {
        return FALSE;
    }

    // Pad using Opus packet framing, not by appending bytes to the payload.
    return (Bytes == voice_codec::FRAME_BYTES) ||
           (opus_packet_pad( pDest, Bytes, voice_codec::FRAME_BYTES ) == OPUS_OK);
}

//==============================================================================

xbool voice_codec_state::Decode( const byte* pSrc, s16* pDest )
{
    if( !m_pDecoder || !pSrc || !pDest ||
        (opus_packet_get_nb_samples( pSrc, voice_codec::FRAME_BYTES,
                                     voice_codec::SAMPLE_RATE ) != voice_codec::SAMPLES_PER_FRAME) )
    {
        return FALSE;
    }

    return opus_decode( m_pDecoder, pSrc, voice_codec::FRAME_BYTES,
                        pDest, voice_codec::SAMPLES_PER_FRAME, 0 ) == voice_codec::SAMPLES_PER_FRAME;
}
