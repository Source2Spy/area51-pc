//==============================================================================
//
//  Headset.hpp
//
//==============================================================================

#ifndef HEADSET_HPP
#define HEADSET_HPP

#include "x_types.hpp"
#include "Network/FiFo.hpp"

const s32 VOICE_SAMPLE_RATE = 8000;
const s32 VOICE_MAX_UPDATE_BYTES = 256;

//==============================================================================

class headset
{
public:

        //
        // Generic Public Functions
        //

        void            Init                        ( xbool EnableHardware );
        void            Kill                        ( void );
        void            Update                      ( f32 DeltaTime );

        s32             Read                        ( void* pBuffer, s32 MaxLength );
        s32             Write                       ( const void* pBuffer, s32 Length );
        void            ClearReadFifo                ( void );
        void            ClearWriteFifo               ( void );
        s32             GetEncodedBlockSize         ( void ) const                          { return m_EncodeBlockSize;   }
        s32             GetDecodedBlockSize         ( void ) const                          { return m_DecodeBlockSize;   }
        void            PeriodicUpdate              ( f32 DeltaTime );
        void            ProvideUpdate               ( netstream& BitStream, s32 MaxLength = VOICE_MAX_UPDATE_BYTES );
        xbool           ReadUpdate                  ( netstream& BitStream, fifo::update& Update ) const;
        void            AcceptUpdate                ( const fifo::update& Update );
        xbool           IsTalking                   ( void ) const                          { return m_IsTalking;                   }
        void            SetTalking                  ( xbool IsTalking );
        xbool           IsHardwarePresent           ( void )                                { return m_HeadsetCount>0;              }
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
        void            SetActiveHeadset            ( s32 HeadsetIndex );
        void            GetVolume                   ( f32& Headset, f32& MicrophoneSensitivity );
        s32             GetNumBytesInWriteFifo      ( void );
        void            UpdateLoopBack              ( void );

private:

        void            UpdateTalkingState           ( void );
        void            ResetEncoder                 ( void );

        //
        // Generic Private Variables
        //

        s32             m_EncodeBlockSize;
        s32             m_DecodeBlockSize;
        f32             m_HeadsetVolume;
        f32             m_MicrophoneSensitivity;
        s32             m_HeadsetCount;
        xbool           m_HardwareEnabled;
        s32             m_ActiveHeadset;
        xbool           m_IsTalking;
        u32             m_TalkingGeneration;
        xbool           m_TalkingRequested;
        xbool           m_LoopbackEnabled;
        byte*           m_pEncodeBuffer;
        byte*           m_pDecodeBuffer;
        fifo            m_ReadFifo;
        fifo            m_WriteFifo;
        xbool           m_VoiceBanned;
        xbool           m_VoiceEnabled;
        xbool           m_VoiceAudible;
        xbool           m_VoiceThroughSpeaker;
        s32             m_HeadsetMask;
        void            OnHeadsetInsert             ( void );
        void            OnHeadsetRemove             ( void );

#ifdef TARGET_WINDOWS
        void*           m_pWindowsState;
#endif
};

//==============================================================================
#endif // HEADSET_HPP
//==============================================================================
