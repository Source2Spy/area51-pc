//=========================================================================
//
//  GameServer.hpp
//
//=========================================================================
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//  Not to be used without express permission of Inevitable Entertainment, Inc.
//
//=========================================================================
#ifndef GAMESERVER_HPP
#define GAMESERVER_HPP

#include "x_files.hpp"
#include "e_Network.hpp"
#include "ConnMgr.hpp"
#include "NetworkMgr/Replication/UpdateMgr.hpp"
#include "NetworkMgr/Replication/NetObjMgr.hpp"
#include "GameClient.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"

class client_proxy;
//=========================================================================

struct extended_lookup
{
    char        PlayerNames[ NET_MAX_PER_CLIENT ];
};


//=========================================================================
class game_server
{
public:
                            game_server         ( void );
                           ~game_server         ( void );

        void                Init                ( net_socket& Local, s32 LocalPlayerCount, s32 MaxClients );
        void                Kill                ( void );
        void                RetireConnection    ( const net_address& Remote, u64 ID ) { m_RetiredConnections.Add(Remote, ID); }
        exit_reason         Update              ( f32 DeltaTime );
        void                UpdateVoice         ( f32 DeltaTime );
        void                SetState            ( server_state State );
        server_state        GetState            ( void )                    { return m_State; }

        net_address         GetAddress          ( void );
        void                SyncMissionLoad     ( void );
        xbool               ReceivePacket       ( const net_address& Remote, netstream& BitStream );
        void                ProcessTime         ( f32 DeltaSec );

        void                KickPlayer          ( s32 PlayerIndex );
        void                BanPlayer           ( s32 PlayerIndex );
        void                DropPlayer          ( s32 PlayerIndex );

//    void        SendMsg         ( const msg& Msg );
//  void        SendMessage     ( const char* Str, xcolor Color=XCOLOR_WHITE, u32 TeamBits=0xFFFFFFFF );
//  void        SendPopMessage  ( const char* Str, xcolor Color=XCOLOR_WHITE, s32 PlayerIndex = -1 );

        void                SendAudio           ( s32 SampleID, s32 PlayerIndex );
        void                SendAudio           ( s32 SampleID, u32 TeamBits=0xFFFFFFFF );
        void                BroadcastEndMission ( void );

        void                UnloadMission       ( void );
        xbool               CoolDown            ( void );
        xbool               DisconnectEveryone  ( void );
        xbool               IsInGame            ( s32 clientIndex );
        xbool               IsClientConnected   ( s32 ClientIndex );
                            
        void                BanClient           ( const net_address& Remote, const byte* ClientUniqueId );
        xbool               IsBanned            ( const net_address& Remote, const byte* ClientUniqueId ) const;
        s32                 GetBanCount         ( void ) const;
                                                
        void                DisplayPings        ( s32 L );
        void                DumpStats           ( X_FILE* fp = NULL );
        void                AdvanceStats        ( void );
        void                ShowLifeDelay       ( void );
        f32                 GetAveragePing      ( void );
        f32                 GetClientPing       ( s32 ClientIndex );
        s32                 GetClientCount        ( void );
        s32                 GetMaxClients       ( void )                            { return m_MaxClients;  }
        s32                 GetClientIndex      ( s32 PlayerIndex );

        s32                 GetClientPlayerCount( s32 ClientIndex );
        s32                 GetClientPlayerSlot ( s32 ClientIndex, s32 PlayerIndex );
        s32                 GetLocalPlayerCount ( void );
        s32                 GetLocalPlayerSlot  ( s32 Player );
        void                ProvideExtendedLookup( netstream& BitStream);
        void                AcceptExtendedLookup( extended_lookup& Lookup, netstream& BitStream);

        exit_reason         GetExitReason       ( void );
        void                SetExitReason       ( exit_reason Reason );
        voice_proxy&        GetVoiceProxy       ( s32 ClientIndex );
        void                LoadComplete        ( void )                            { m_MissionLoaded = TRUE; }
        xbool               IsMissionLoaded     ( void )                            { return m_MissionLoaded; }

public:
        s32                 m_MissionLoadedIndex;

private:
        struct client_ban
        {
            s32             IP;
            byte            UniqueId[NET_MAX_ID_LENGTH];
        };

        void                EnterState          ( server_state NewState );
        void                UpdateState         ( f32 DeltaTime );
        void                ExitState           ( server_state OldState );

        void                ProcessDisconnect   (const net_address& Remote, netstream& BitStream);
        void                ProcessLoginRequest (const net_address& Remote, netstream& BitStream, u64 ConnectionID);
        void                ProcessManagerData  (const net_address& Remote, netstream& BitStream);
        void                RefuseLogin         (const net_address& Remote, exit_reason Reason, u64 ConnectionID);
        void                AcceptLogin         (const net_address& Remote, s32 ClientIndex, s32 PlayerCount);
        void                UpdateRegistration  ( void );

        retired_connections m_RetiredConnections;
        build_authentication m_BuildAuthentication;
        net_socket*         m_pLocal;
        xwchar              m_Title[64];

        s32                 m_UpdateIndex;
        s32                 m_MaxClients;
        f32                 m_PacketSendBudget;
        client_proxy*       m_pClients;
        server_state        m_State;
        f32                 m_StateTime;

        xbool               m_AllowClients;
        xbool               m_MissionLoaded;
        xarray<client_ban>  m_BanList;

        s32                 m_LocalPlayerCount;
        s32                 m_LocalPlayerSlot[ NET_MAX_PER_CLIENT ];

#if defined( ENABLE_DEBUG_MENU )
#if !defined(X_RETAIL) || defined(X_QA)
        friend class debug_menu_page_multiplayer;
#endif
#endif
};

const char* GetStateName( server_state State );

//=========================================================================
#endif
//=========================================================================

