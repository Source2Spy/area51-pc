//==============================================================================
//
//  Headset.hpp
//
//==============================================================================

#ifndef HEADSET_HPP
#define HEADSET_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "Network/FiFo.hpp"
#include "VoicePacket.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct SDL_AudioStream;

//==============================================================================

class headset
{
public:

        //----------------------------------------------------------------------
        // Generic Public Functions
        //----------------------------------------------------------------------

        void            Init                        ( xbool EnableHardware );
        void            Kill                        ( void );
        void            Update                      ( f32 DeltaTime, xbool TalkingRequested, xbool GameVoiceEnabled );

        s32             Read                        ( void* pBuffer, s32 MaxLength );
        s32             Write                       ( const void* pBuffer, s32 Length );
        void            ClearReadFifo               ( void );
        void            ClearWriteFifo              ( void );
        void            ProvideUpdate               ( netstream& BitStream, xbool SendAudio );
        void            AcceptUpdate                ( const voice_update& Update );
        s32             GetCaptureBytes             ( void )                                { return m_ReadFifo.GetBytesUsed();      }
        xbool           IsTalking                   ( void ) const;
        xbool           IsHardwarePresent           ( void ) const;
        xbool           IsBanned                    ( void )                                { return m_VoiceBanned;                 }
        xbool           IsEnabled                   ( void )                                { return m_VoiceEnabled;                }
        xbool           IsAudible                   ( void )                                { return m_VoiceAudible;                }
        xbool           IsThroughSpeaker            ( void )                                { return m_VoiceThroughSpeaker;         }
        void            SetVoiceBanned              ( xbool IsBanned  )                     { m_VoiceBanned         = IsBanned;     }
        void            SetVoiceEnabled             ( xbool IsEnabled )                     { m_VoiceEnabled        = IsEnabled;    }
        void            SetVoiceAudible             ( xbool IsAudible )                     { m_VoiceAudible        = IsAudible;    }
        void            SetThroughSpeaker           ( xbool IsEnabled )                     { m_VoiceThroughSpeaker = IsEnabled;    }
        void            SetLoopback                 ( xbool IsEnabled );
        void            SetSpeakerVolume            ( f32 SpeakerVolume );
        void            SetVolume                   ( f32 Headset, f32 MicrophoneSensitivity );
        void            GetVolume                   ( f32& Headset, f32& MicrophoneSensitivity );
        s32             GetNumBytesInWriteFifo      ( void );
        void            UpdateLoopBack              ( void );

private:

        xbool           InitAudio                   ( void );
        void            KillAudio                   ( void );
        void            UpdateDevices               ( f32 DeltaTime, xbool Enabled, xbool TalkingRequested );
        void            UpdateCapture               ( xbool TalkingRequested );
        xbool           ReadCaptureFrames           ( void );
        void            UpdatePlayback              ( void );
        xbool           EncodeCapture               ( const s16* pSamples );
        xbool           ReadPlayback                ( s16* pSamples );
        void            CloseCapture                ( void );
        void            ClosePlayback               ( void );
        void            ClearPlayback               ( void );
        void            ApplyAudioGain              ( void );

        //----------------------------------------------------------------------
        // Voice settings and packet queues
        //----------------------------------------------------------------------

        f32                 m_HeadsetVolume         = 1.0f;
        f32                 m_MicrophoneSensitivity = 1.0f;
        xbool               m_LoopbackEnabled       = FALSE;
        xbool               m_VoiceBanned           = FALSE;
        xbool               m_VoiceEnabled          = TRUE;
        xbool               m_VoiceAudible          = TRUE;
        xbool               m_VoiceThroughSpeaker   = FALSE;
        byte                m_ReadBuffer[voice_update::FIFO_BYTES];
        byte                m_WriteBuffer[voice_update::FIFO_BYTES];
        fifo                m_ReadFifo;
        fifo                m_WriteFifo;
        voice_codec_state   m_Codec;

        //----------------------------------------------------------------------
        // SDL audio devices
        //----------------------------------------------------------------------

        xbool               m_AudioInitialized      = FALSE;
        SDL_AudioStream*    m_pCapture              = NULL;
        SDL_AudioStream*    m_pPlayback             = NULL;
        xbool               m_CaptureAvailable      = FALSE;
        f32                 m_DeviceCheckTime       = 0.0f;
        f32                 m_CaptureRetry          = 0.0f;
};

//==============================================================================
#endif // HEADSET_HPP
//==============================================================================
