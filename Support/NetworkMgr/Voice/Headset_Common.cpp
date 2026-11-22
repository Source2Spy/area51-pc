//==============================================================================
//
//  Headset_Common.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Headset.hpp"
#include "x_files.hpp"

//==============================================================================
//  LIFECYCLE
//==============================================================================

void headset::Init( xbool EnableHardware )
{
    m_ReadFifo.Init( m_ReadBuffer, sizeof( m_ReadBuffer ) );
    m_WriteFifo.Init( m_WriteBuffer, sizeof( m_WriteBuffer ) );
    m_LoopbackEnabled = FALSE;
    m_VoiceBanned     = FALSE;

    // A relay without local audio still needs to parse voice updates.
    if( !EnableHardware )
    {
        return;
    }

    if( !m_Codec.Init() )
    {
        x_DebugMsg( "headset::Init: Unable to initialize the Opus voice codec.\n" );
        return;
    }

    m_AudioInitialized = InitAudio();
    if( !m_AudioInitialized )
    {
        m_Codec.Kill();
    }
}

//==============================================================================

void headset::Kill( void )
{
    if( m_AudioInitialized )
    {
        KillAudio();
        m_AudioInitialized = FALSE;
    }

    m_Codec.Kill();
    m_ReadFifo.Kill();
    m_WriteFifo.Kill();
}

//==============================================================================

void headset::Update( f32 DeltaTime, xbool TalkingRequested, xbool GameVoiceEnabled )
{
    if( !m_AudioInitialized )
    {
        return;
    }

    xbool const Enabled = GameVoiceEnabled && m_VoiceEnabled && !m_VoiceBanned;
    UpdateDevices( DeltaTime, Enabled, TalkingRequested );
    if( Enabled )
    {
        UpdateCapture( TalkingRequested );
        UpdateLoopBack();
        UpdatePlayback();
    }
}

//==============================================================================
//  VOICE PACKETS
//==============================================================================

void headset::ProvideUpdate( netstream& BitStream, xbool SendAudio )
{
    voice_update Update;
    Update.Write( BitStream, m_ReadFifo, SendAudio );
}

//==============================================================================

void headset::AcceptUpdate( const voice_update& Update )
{
    Write( Update.Data, Update.Length );
}

//==============================================================================

void headset::UpdateLoopBack( void )
{
    if( !m_LoopbackEnabled )
    {
        return;
    }

    byte Frame[voice_codec::FRAME_BYTES];
    while( m_ReadFifo.Remove( Frame, sizeof( Frame ), sizeof( Frame ) ) )
    {
        Write( Frame, sizeof( Frame ) );
    }
}

//==============================================================================

xbool headset::EncodeCapture( const s16* pSamples )
{
    byte Frame[voice_codec::FRAME_BYTES];
    if( !m_Codec.Encode( pSamples, Frame ) )
    {
        x_DebugMsg( "headset::EncodeCapture: Unable to encode an Opus voice frame.\n" );
        return FALSE;
    }

    return m_ReadFifo.Insert( Frame, sizeof( Frame ), sizeof( Frame ) );
}

//==============================================================================

xbool headset::ReadPlayback( s16* pSamples )
{
    byte Frame[voice_codec::FRAME_BYTES];
    while( m_WriteFifo.Remove( Frame, sizeof( Frame ), sizeof( Frame ) ) )
    {
        if( m_Codec.Decode( Frame, pSamples ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

s32 headset::Read( void* pBuffer, s32 Length )
{
    if( (pBuffer == NULL) || (Length <= 0) )
    {
        return 0;
    }

    s32 Available = MIN( m_ReadFifo.GetBytesUsed(), Length );
    Available -= Available % voice_codec::FRAME_BYTES;

    if( (Available <= 0) ||
        !m_ReadFifo.Remove( pBuffer, Available, voice_codec::FRAME_BYTES ) )
    {
        return 0;
    }

    return Available;
}

//==============================================================================

s32 headset::Write( const void* pBuffer, s32 Length )
{
    if( !m_pPlayback || !m_VoiceEnabled || !m_VoiceAudible || m_VoiceBanned ||
        (pBuffer == NULL) ||
        (Length <= 0) ||
        (Length > static_cast<s32>( sizeof( m_WriteBuffer ) )) ||
        ((Length % voice_codec::FRAME_BYTES) != 0) )
    {
        return 0;
    }

    return m_WriteFifo.Insert( pBuffer, Length, voice_codec::FRAME_BYTES ) ? Length : 0;
}

//==============================================================================

void headset::ClearReadFifo( void )
{
    m_ReadFifo.Clear();
}

//==============================================================================

void headset::ClearWriteFifo( void )
{
    m_WriteFifo.Clear();
    ClearPlayback();

    if( m_AudioInitialized && !m_Codec.ResetDecoder() )
    {
        x_DebugMsg( "headset::ClearWriteFifo: Unable to reset the Opus decoder.\n" );
    }
}

//==============================================================================

s32 headset::GetNumBytesInWriteFifo( void )
{
    return m_WriteFifo.GetBytesUsed();
}

//==============================================================================
//  SETTINGS
//==============================================================================

void headset::SetVolume( f32 Headset, f32 MicrophoneSensitivity )
{
    m_HeadsetVolume         = x_clamp( Headset, 0.0f, 1.0f );
    m_MicrophoneSensitivity = x_clamp( MicrophoneSensitivity, 0.0f, 1.0f );
    ApplyAudioGain();
}

//==============================================================================

void headset::GetVolume( f32& Headset, f32& MicrophoneSensitivity )
{
    Headset               = m_HeadsetVolume;
    MicrophoneSensitivity = m_MicrophoneSensitivity;
}

//==============================================================================

void headset::SetSpeakerVolume( f32 SpeakerVolume )
{
    SetVolume( SpeakerVolume, m_MicrophoneSensitivity );
}

//==============================================================================

void headset::SetLoopback( xbool IsEnabled )
{
    m_LoopbackEnabled = IsEnabled;
}
