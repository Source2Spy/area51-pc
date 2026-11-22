//==============================================================================
//
//  VoiceMgr.hpp
//
//  On the game server, this handles arbitration and distribution of voice data to
//  all game clients. On the client, it will handle distribution to the local machine.
//
//==============================================================================

#ifndef VOICEMGR_HPP
#define VOICEMGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Network/NetStream.hpp"
#include "Headset.hpp"
#include "NetworkMgr/NetLimits.hpp"

static_assert( NET_MAX_PLAYERS <= (sizeof( u32 ) * 8), "Voice player masks require at least one bit per player." );

//==============================================================================
//  TYPES
//==============================================================================

struct speaker
{
    s32                 PlayerNum;
    f32                 TalkTime;
    xbool               MoveToEndOfQueue;
    actual_talk_mode    ActualTalkMode;
};

//==============================================================================
//  VOICE MGR
//==============================================================================

class voice_mgr
{
public:
                            voice_mgr               ( void );
                           ~voice_mgr               ( void );

        void                Init                    ( xbool EnableHeadset );
        void                Kill                    ( void );
        void                Update                  ( f32 DeltaTime );
        xbool               IsTalking               ( void )                                                    { return m_Headset.IsTalking();             }
        void                SetTalking              ( xbool bTalking );
        void                Distribute              ( s32 TalkerIndex, const byte* pBuffer, s32 Length );

        xbool               IsValidTarget           ( s32 Speaker, s32 Listener, actual_talk_mode TalkMode );
        void                DoArbitration           ( f32 DeltaTime );
        void                AcceptUpdate            ( netstream& BitStream );
        void                ProvideUpdate           ( netstream& BitStream );

        void                WriteToVoiceFifo        ( const byte* pBuffer, s32 Length );
        s32                 GetBytesInWriteFifo     ( void );
        s32                 GetLocalVoiceOwner      ( void );
        actual_talk_mode    GetLocalVoiceTalkType   ( void )                                                    { return m_LocalVoiceTalkType;              }
        xbool               IsSpeaking              ( s32 Speaker );
        void                AgeSpeaker              ( s32 Speaker,  f32 DeltaTime );
        void                SetVoiceOwner           ( s32 Listener, s32 Speaker, actual_talk_mode TalkMode );
        desired_talk_mode   GetDesiredTalkMode      ( s32 PlayerIndex );
        void                ToggleTalkMode          ( void );

        desired_talk_mode   GetLocalTalkType        ( void )                                                    { return m_CurrentTalkType;                 }
        desired_talk_mode   GetLocalDesiredTalkMode ( void )                                                    { return m_LocalDesiredTalkMode;            }
        xbool               IsHeadsetPresent        ( void )                                                    { return m_Headset.IsHardwarePresent();     }
        xbool               IsVoiceBanned           ( void )                                                    { return m_Headset.IsBanned();              }
        xbool               IsVoiceEnabled          ( void )                                                    { return m_Headset.IsEnabled();             }
        xbool               IsVoiceAudible          ( void )                                                    { return m_Headset.IsAudible();             }
        xbool               IsThroughSpeaker        ( void )                                                    { return m_Headset.IsThroughSpeaker();      }
        xbool               IsVoiceCapable          ( void );

        void                SetVoiceBanned          ( xbool IsBanned  )                                         { m_Headset.SetVoiceBanned   ( IsBanned  ); }
        void                SetVoiceEnabled         ( xbool IsEnabled )                                         { m_Headset.SetVoiceEnabled  ( IsEnabled ); }
        void                SetVoiceAudible         ( xbool IsAudible )                                         { m_Headset.SetVoiceAudible  ( IsAudible ); }
        void                SetVoiceThruSpeakers    ( xbool IsEnabled )                                         { m_Headset.SetThroughSpeaker( IsEnabled ); }
        void                SetLoopback             ( xbool IsEnabled )                                         { m_Headset.SetLoopback      ( IsEnabled ); }
        void                SetVolume               ( f32 Head, f32 Mic )                                       { m_Headset.SetVolume( Head, Mic );         }
        void                GetVolume               ( f32& Head, f32& Mic )                                     { m_Headset.GetVolume( Head, Mic );         }

        xbool               GetLocallyMuted         ( s32 PlayerIndex )
        {
            ASSERT( IN_RANGE( 0, PlayerIndex, NET_MAX_PLAYERS - 1 ) );
            if( !IN_RANGE( 0, PlayerIndex, NET_MAX_PLAYERS - 1 ) )
            {
                return FALSE;
            }

            return !!( m_LocalMutedPlayers & (u32( 1 ) << PlayerIndex) );
        }

        void                SetLocallyMuted         ( s32 PlayerIndex, xbool bMuted )
        {
            ASSERT( IN_RANGE( 0, PlayerIndex, NET_MAX_PLAYERS - 1 ) );
            if( !IN_RANGE( 0, PlayerIndex, NET_MAX_PLAYERS - 1 ) )
            {
                return;
            }

            m_LocalDirtyMutedPlayers = TRUE;
            m_LocalMutedPlayers &= ~(u32( 1 ) << PlayerIndex);
            if( bMuted )
            {
                m_LocalMutedPlayers |= (u32( 1 ) << PlayerIndex);
                if( m_Initialized && (m_PlaybackOwner == PlayerIndex) )
                {
                    StopLocalPlayback();
                }
            }
        }

        u32                 GetLocallyMutedBits     ( void )                                                    { return m_LocalMutedPlayers; }
        u32                 GetSpeakingBits         ( void )                                                    { return ( (m_LocalVoiceOwner >= 0) ? (u32( 1 ) << m_LocalVoiceOwner) : 0 ); }

        headset&            GetHeadset              ( void );

        void                SetIsGameVoiceEnabled   ( xbool Enabled );
        xbool               IsGameVoiceEnabled      ( void )                                                    { return m_bGameIsVoiceEnabled; }

private:
        void                DistributeLocalCapture  ( s32 Player );
        xbool               CanDrainVoice           ( s32 Speaker, s32 Listener, actual_talk_mode TalkMode );
        void                SetLocalVoiceOwner      ( s32 Owner, actual_talk_mode TalkMode );
        void                StopLocalPlayback       ( void );

        xbool               m_bGameIsVoiceEnabled;
        xbool               m_Initialized;
        xbool               m_TalkingRequested;
        headset             m_Headset;
        desired_talk_mode   m_CurrentTalkType;
        desired_talk_mode   m_LocalDesiredTalkMode;

        s32                 m_LocalVoiceOwner;
        actual_talk_mode    m_LocalVoiceTalkType;
        s32                 m_PlaybackOwner;
        actual_talk_mode    m_PlaybackTalkMode;

        s32                 m_MaxSpeakers;

        u32                 m_LocalMutedPlayers;
        xbool               m_LocalDirtyMutedPlayers;

        speaker             m_Speakers      [ NET_MAX_PLAYERS ];
        speaker*            m_SpeakerQueue  [ NET_MAX_PLAYERS ];
};

extern voice_mgr g_VoiceMgr;

#if defined(X_DEBUG)
extern const char* GetTalkModeName( s32 Mode );
#endif

//==============================================================================
#endif // VOICEMGR_HPP
//==============================================================================
