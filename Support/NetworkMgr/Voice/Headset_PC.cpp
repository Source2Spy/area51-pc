//==============================================================================
//
//  Headset_PC.cpp
//
//==============================================================================
#include "x_types.hpp"

#if !defined(TARGET_PC)
#error This should only be getting compiled for the PC platform. Check your dependancies.
#endif

#include "headset.hpp"
#include "Speex.hpp"
//
// Even though the xbox voice code will be significantly different than PS2 voice code,
// I am still including the Provide and Accept update functions as they will allow us
// to maintain compatibility between PS2 & XBOX. XBOX live should take care of headset
// support.
//

//==============================================================================
void headset::Init( xbool )
{
    // Set this temp stuff here.
    m_EncodeBlockSize       = SPEEX8_BYTES_PER_EFRAME;
    m_DecodeBlockSize       = SPEEX8_SAMPLES_PER_FRAME * (s32)sizeof(s16);
    m_HeadsetCount          = 0;
    m_HardwareEnabled       = FALSE;
    m_IsTalking             = FALSE;
    m_LoopbackEnabled       = FALSE;
    m_VoiceBanned           = FALSE;
    m_VoiceEnabled          = FALSE;
    m_VoiceAudible          = FALSE;
    m_VoiceThroughSpeaker   = FALSE;
    m_pThread               = NULL;

    m_pEncodeBuffer = new u8[512];
    m_pDecodeBuffer = m_pEncodeBuffer+256;
    m_ReadFifo.Init(m_pEncodeBuffer,256);
    m_WriteFifo.Init(m_pDecodeBuffer,256);
}

//==============================================================================
void headset::Kill( void )
{
    m_WriteFifo.Kill();
    m_ReadFifo.Kill();
    delete[] m_pEncodeBuffer;
}

//==============================================================================
void headset::PeriodicUpdate( f32 )
{
}

//==============================================================================
void headset::Update( f32 DeltaTime )
{
    (void)DeltaTime;
}
