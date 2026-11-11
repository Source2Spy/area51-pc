//==============================================================================
//
//  Headset_POSIX.cpp
//
//==============================================================================

#include "x_types.hpp"
#include "Headset.hpp"
#include "Speex.hpp"

//==============================================================================

void headset::Init( xbool )
{
    m_EncodeBlockSize       = SPEEX8_BYTES_PER_EFRAME;
    m_DecodeBlockSize       = SPEEX8_SAMPLES_PER_FRAME * (s32)sizeof( s16 );
    m_HeadsetCount          = 0;
    m_HardwareEnabled       = FALSE;
    m_IsTalking             = FALSE;
    m_TalkingGeneration     = 0;
    m_TalkingRequested      = FALSE;
    m_LoopbackEnabled       = FALSE;
    m_VoiceBanned           = FALSE;
    m_VoiceEnabled          = FALSE;
    m_VoiceAudible          = FALSE;
    m_VoiceThroughSpeaker   = FALSE;

    m_pEncodeBuffer = new u8[512];
    m_pDecodeBuffer = m_pEncodeBuffer + 256;
    m_ReadFifo.Init( m_pEncodeBuffer, 256 );
    m_WriteFifo.Init( m_pDecodeBuffer, 256 );
}

//==============================================================================

void headset::Kill( void )
{
    m_WriteFifo.Kill();
    m_ReadFifo.Kill();
    delete[] m_pEncodeBuffer;
    m_pEncodeBuffer = NULL;
    m_pDecodeBuffer = NULL;
}

//==============================================================================

void headset::PeriodicUpdate( f32 DeltaTime )
{
    (void)DeltaTime;
}

//==============================================================================

void headset::Update( f32 DeltaTime )
{
    (void)DeltaTime;
}

//==============================================================================

void headset::OnHeadsetInsert( void )
{
}

//==============================================================================

void headset::OnHeadsetRemove( void )
{
}

//==============================================================================

void headset::ResetEncoder( void )
{
}

//==============================================================================
