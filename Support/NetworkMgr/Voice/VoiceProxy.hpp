//==============================================================================
//
//  VoiceProxy.hpp
//
//  Client instance depository for voice data going out to this specific client.
//
//==============================================================================

#ifndef VOICEPROXY_HPP
#define VOICEPROXY_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Network/NetStream.hpp"
#include "Network/FiFo.hpp"
#include "VoiceMgr.hpp"

//==============================================================================
//  CLASS
//==============================================================================

class voice_proxy
{
public:
                            voice_proxy         ( void );
                           ~voice_proxy         ( void );

        void                Init                ( s32 PlayerNetSlot );
        void                Kill                ( void );

        void                ProvideUpdate       ( netstream& BitStream );
        void                AcceptUpdate        ( netstream& BitStream );
        void                Write               ( const byte* pBuffer, s32 Length );
        void                SetPlayerSlot       ( s32 PlayerSlot );
        s32                 GetPlayerSlot       ( void );
        void                SetVoiceOwner       ( s32 PlayerSlot, actual_talk_mode TalkMode, xbool Drain );
        s32                 GetVoiceOwner       ( void );
        s32                 GetOutgoingOwner    ( void ) const { return m_OutgoingOwner; }
        actual_talk_mode    GetVoiceTalkMode    ( void ) const { return m_VoiceTalkMode; }
        desired_talk_mode   GetDesiredTalkMode  ( void ) { return m_DesiredTalkMode; }
        u32                 GetMutedPlayers     ( void ) { return m_MutedPlayers; }

private:
        xbool               m_Initialized;

        s32                 m_VoiceOwner;
        s32                 m_OutgoingOwner;
        actual_talk_mode    m_VoiceTalkMode;
        u32                 m_MutedPlayers;

        s32                 m_PlayerNetSlot;

        desired_talk_mode   m_DesiredTalkMode;

        fifo                m_Outgoing;
        byte                m_OutgoingBuffer[voice_update::FIFO_BYTES];
};


//==============================================================================
#endif // VOICEMGR_HPP
//==============================================================================
