//==============================================================================
//
//  NetworkMgr.cpp
//
//==============================================================================
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//  Not to be used without express permission of Inevitable Entertainment, Inc.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Entropy.hpp"
#include "NetworkMgr.hpp"
#include "ServerVersion.hpp"
#include "NetLimits.hpp"

#include "NetworkMgr/Game/GameMgr.hpp"
#include "NetworkMgr/Connection/GameState.hpp"
#include "Configuration/GameConfig.hpp"
#include "NetworkMgr/Connection/GameServer.hpp"
#include "NetworkMgr/Connection/GameClient.hpp"
#include "InputMgr/GamePad.hpp"
#include "Voice/VoiceMgr.hpp"
#include "GameLib/LevelLoader.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StateMgr/MapList.hpp"
#include "ScriptMgr/ScriptMgr.hpp"

//==============================================================================
//  GLOBALS
//==============================================================================

network_mgr g_NetworkMgr;
s32         g_ServerVersion = SERVER_VERSION;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

network_mgr::network_mgr( void )
{
    m_LocalSocket.Clear();
    m_IsOnline         = FALSE;
    m_RequestedOnlineState = FALSE;
    m_OnlineStateChangeJob = xhandle( HNULL );
    m_pServer          = NULL;
    m_pClient          = NULL;
    m_LocalPlayerCount = 0;

    g_PendingConfig.Invalidate();
    g_ActiveConfig.Invalidate();

    // Set sane defaults — should be overwritten by game configuration later.
    g_PendingConfig.SetEliminationMode ( FALSE                                                 );
    g_PendingConfig.SetScoreLimit      ( -1                                                    );
    g_PendingConfig.SetGameTime        ( 900                                                   );
    g_PendingConfig.SetSystemID        ( 0                                                     );
    g_PendingConfig.SetLevelID         ( 0                                                     );
    g_PendingConfig.SetMaxPlayerCount  ( 16                                                    );
    g_PendingConfig.SetVersion         ( g_ServerVersion                                       );
    g_PendingConfig.SetFirePercent     ( 75                                                    );
    g_PendingConfig.SetVotePercent     ( 75                                                    );
    g_PendingConfig.SetVoiceEnabled    ( TRUE                                                  );
    g_PendingConfig.SetMutationMode    ( MUTATE_CHANGE_AT_WILL                                 );
    g_PendingConfig.SetGameType        ( xwstring("")                                          );
    g_PendingConfig.SetPassword        ( ""                                                    );
    g_PendingConfig.SetServerName      ( g_StateMgr.GetActiveSettings().GetHostSettings().m_ServerName );
}

//==============================================================================

network_mgr::~network_mgr( void )
{
    ASSERT( m_OnlineStateChangeJob.IsNull() );
}

//==============================================================================
//  LIFECYCLE
//==============================================================================

void network_mgr::Init( void )
{
    // Keep the network library alive for the lifetime of the application so
    // local networking remains available outside the online state.
    net_Init();
    net_SetVersionKey( g_ServerVersion );
}

//==============================================================================

void network_mgr::Kill( void )
{
    if( m_OnlineStateChangeJob.IsNonNull() )
    {
        FinishOnlineStateChange();
    }

    g_MatchMgr.Kill();
    net_Kill();
}

//==============================================================================

void network_mgr::SetOnline( xbool IsOnline )
{
    if( m_IsOnline == IsOnline )
    {
        return;
    }

    // -------------------------------------------------------------------------
    // Tear down the socket and notify the match manager.
    // -------------------------------------------------------------------------
    if( m_IsOnline && !IsOnline )
    {
        if( !m_LocalSocket.IsEmpty() )
        {
            m_LocalSocket.Close();
            m_CancelledConnections.Clear();
        }
        g_MatchMgr.Kill();
    }

    m_IsOnline = IsOnline;
}

//==============================================================================

void network_mgr::OnlineStateChangeJob( void* pData )
{
    network_mgr* pNetworkMgr = (network_mgr*)pData;

    ASSERT( pNetworkMgr );
    pNetworkMgr->SetOnline( pNetworkMgr->m_RequestedOnlineState );
}

//==============================================================================

xbool network_mgr::BeginOnlineStateChange( xbool IsOnline )
{
    ASSERT( m_OnlineStateChangeJob.IsNull() );
    ASSERTS( x_WorkersIsInit(), "Online state changes require x_workers to be initialized" );

    if( m_OnlineStateChangeJob.IsNonNull() || !x_WorkersIsInit() )
    {
        return FALSE;
    }

    m_RequestedOnlineState = IsOnline;
    return x_WorkerJobSubmit( OnlineStateChangeJob,
                              this,
                              IsOnline ? "Network Online" : "Network Offline",
                              m_OnlineStateChangeJob );
}

//==============================================================================

xbool network_mgr::IsOnlineStateChangeDone( void ) const
{
    ASSERT( m_OnlineStateChangeJob.IsNonNull() );

    return m_OnlineStateChangeJob.IsNonNull() &&
           x_WorkerJobIsDone( m_OnlineStateChangeJob );
}

//==============================================================================

void network_mgr::FinishOnlineStateChange( void )
{
    ASSERT( m_OnlineStateChangeJob.IsNonNull() );

    if( m_OnlineStateChangeJob.IsNull() )
    {
        return;
    }

    x_WorkerJobWait( m_OnlineStateChangeJob );
    x_WorkerJobRelease( m_OnlineStateChangeJob );
    m_OnlineStateChangeJob = xhandle( HNULL );
}

//==============================================================================

void network_mgr::BeginFrame( f32 RealDeltaTime )
{
    if( m_OnlineStateChangeJob.IsNonNull() )
    {
        return;
    }

    xbool       Used = FALSE;
    net_address Remote;
    netstream   BitStream;

    if( RealDeltaTime > 0.5f )
    {
        LOG_WARNING( "network_mgr::BeginFrame", "Long frame: %2.02fsec between updates", RealDeltaTime );
    }

    m_CancelledConnections.Update( m_LocalSocket, RealDeltaTime );
    g_VoiceMgr.Update( RealDeltaTime );

    // -------------------------------------------------------------------------
    // Receive network packets
    // -------------------------------------------------------------------------
    s32 MaxCount = 0;
    exit_reason const ExitReason = g_ActiveConfig.GetExitReason();

    if( m_IsOnline && m_LocalSocket.IsConnected() )
    {
        // Check for interface loss (cable pull, network down, duplicate login).
        interface_info Info;
        net_GetInterfaceInfo( -1, Info );

        if( !Info.Address || !Info.IsAvailable )
        {
            g_ActiveConfig.SetExitReason( GAME_EXIT_NETWORK_DOWN );
        }

        if( m_LocalSocket.IsConnected() &&
            ( ExitReason == GAME_EXIT_NETWORK_DOWN ||
              ExitReason == GAME_EXIT_DUPLICATE_LOGIN ) )
        {
            // Closing the socket cascades through the state machines and causes
            // the server / client to shut themselves down cleanly.
            m_LocalSocket.Close();
            m_CancelledConnections.Clear();
        }
        else
        {
            xtimer t;
            t.Start();

            while( m_LocalSocket.IsConnected() && BitStream.Receive( m_LocalSocket, Remote ) )
            {
                Used = (BitStream.GetNBytes() < (s32)(3 * sizeof(u16))) ||
                       g_MatchMgr.ReceivePacket( Remote, BitStream );
                if( !Used )
                    Used = m_CancelledConnections.ReceivePacket( Remote, BitStream );

                if( !Used && m_pServer )
                {
                    ASSERT( m_pClient == NULL );
                    Used = m_pServer->ReceivePacket( Remote, BitStream );
                }

                if( !Used && m_pClient )
                {
                    ASSERT( m_pServer == NULL );
                    Used = m_pClient->ReceivePacket( Remote, BitStream );
                }

                // Bail out if we're spending too long in the receive loop.
                if( ++MaxCount > 20 )
                {
                    LOG_WARNING( "network_mgr::BeginFrame",
                                 "Receive loop bail-out. Packets:%d  Time:%2.02fms",
                                 MaxCount, t.ReadMs() );
                    break;
                }
            }
        }
    }
}

//==============================================================================

void network_mgr::AdvanceSimulation( f32 DeltaTime )
{
    if( m_OnlineStateChangeJob.IsNonNull() )
    {
        return;
    }

    xbool const IsSplitScreenPaused = (GetLocalPlayerCount() > 1) && g_StateMgr.IsPaused();
    if( !IsSplitScreenPaused && GameMgr.GameInProgress() )
    {
        GameMgr.Logic( DeltaTime );
        NetObjMgr.Logic( DeltaTime );
    }
}

//==============================================================================

void network_mgr::EndFrame( f32 RealDeltaTime )
{
    if( m_OnlineStateChangeJob.IsNonNull() )
    {
        return;
    }

    // -------------------------------------------------------------------------
    // Advance level or detect game completion.
    // -------------------------------------------------------------------------
    exit_reason ExitReason = GAME_EXIT_CONTINUE;

    if( m_pServer )
    {
        ASSERT( m_pClient == NULL );

        if( m_pServer->GetState() == STATE_SERVER_INGAME &&
            !GameMgr.GameInProgress() )
        {
            if( GameMgr.GetGameType() == GAME_CAMPAIGN )
            {
                const map_entry* pEntry = g_MapList.GetByIndex( g_StateMgr.GetLevelIndex() );

                if( pEntry->GetMapID() == NET_LAST_MAP_ID )
                {
                    g_ActiveConfig.SetExitReason( GAME_EXIT_GAME_COMPLETE );
                }
                else
                {
                    g_ActiveConfig.SetExitReason( GAME_EXIT_ADVANCE_LEVEL );
                }
            }
            else
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_ADVANCE_LEVEL );
            }
        }

        ExitReason = m_pServer->Update( RealDeltaTime );
    }

    if( m_pClient )
    {
        ASSERT( m_pServer == NULL );
        ExitReason = m_pClient->Update( RealDeltaTime );
    }

    // Propagate exit reason if nothing higher-priority already set it.
    if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE &&
        ExitReason                     != GAME_EXIT_CONTINUE )
    {
        g_ActiveConfig.SetExitReason( ExitReason );
    }

    g_MatchMgr.Update( RealDeltaTime );

    LOG_FLUSH();
}

//==============================================================================

void network_mgr::UpdateFrame( f32 RealDeltaTime )
{
    BeginFrame( RealDeltaTime );
    EndFrame( RealDeltaTime );
}

//==============================================================================

void network_mgr::LoadMissionComplete( void )
{
    if( m_pServer ) { m_pServer->LoadComplete(); }
    if( m_pClient ) { m_pClient->LoadComplete(); }
}

//==============================================================================

void network_mgr::BecomeServer( void )
{
    LOG_APP_NAME( "SERVER" );

    ASSERT( m_pServer == NULL );
    ASSERT( m_pClient == NULL );

    game_config::Commit();
    SetLocalPlayerCount( g_ActiveConfig.GetPlayerCount() );

    g_ScriptMgr.SetEnabled( (g_ActiveConfig.GetGameTypeID() == GAME_CAMPAIGN) ||
                           g_ActiveConfig.IsCheatsAllowed() );

    m_pServer = new game_server;

    if( m_LocalSocket.IsEmpty() )
    {
        m_pServer->Init( m_LocalSocket, g_ActiveConfig.GetPlayerCount(), 0 );
    }
    else
    {
        m_pServer->Init( m_LocalSocket, g_ActiveConfig.GetPlayerCount(), NET_MAX_LOCAL_CLIENTS );
        g_MatchMgr.SetState( MATCH_BECOME_SERVER );
    }

    g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
}

//==============================================================================

void network_mgr::BeginLogin( void )
{
    ASSERT( m_pClient == NULL );
    g_ScriptMgr.SetEnabled( FALSE );

    m_pClient = new game_client;
    if( !m_pClient->Init(m_LocalSocket, g_ActiveConfig.GetRemoteAddress(), 1) )
    {
        delete m_pClient;
        m_pClient = NULL;
        g_ActiveConfig.SetExitReason( GAME_EXIT_CANNOT_CONNECT );
        g_ScriptMgr.SetEnabled( TRUE );
    }
}

//==============================================================================

void network_mgr::ReenterGame( void )
{
    if( m_pServer )
    {
        ASSERT( m_pClient == NULL );
        m_pServer->SetState( STATE_SERVER_LOAD_MISSION );
    }

    if( m_pClient )
    {
        ASSERT( m_pServer == NULL );
        m_pClient->SetState( STATE_CLIENT_REQUEST_MISSION );
    }
}

//==============================================================================

void network_mgr::Disconnect( void )
{
    if( m_pServer )
    {
        g_MatchMgr.SetState( IsOnline() ? MATCH_UNREGISTER_SERVER : MATCH_IDLE );
        m_pServer->Kill();
        delete m_pServer;
        m_pServer = NULL;
    }

    if( m_pClient )
    {
        g_MatchMgr.SetState( MATCH_IDLE );
        if( m_pClient->IsConnected() )
        {
            m_CancelledConnections.Add( m_pClient->GetAddress(), m_pClient->GetConnectionID() );
            m_CancelledConnections.Update( m_LocalSocket, 0.0f );
        }
        m_pClient->Kill();
        delete m_pClient;
        m_pClient = NULL;
    }

    g_ScriptMgr.SetEnabled( TRUE );
}

//==============================================================================

voice_proxy& network_mgr::GetVoiceProxy( s32 ClientIndex )
{
    ASSERT( m_pServer );
    return m_pServer->GetVoiceProxy( ClientIndex );
}

//==============================================================================

void network_mgr::SilenceInternal( xbool ForceDisconnect )
{
    // ForceDisconnect=TRUE: close the socket immediately.
    // ForceDisconnect=FALSE: let the state machines wind down gracefully.
    (void)ForceDisconnect; // TODO: implement graceful vs forced path
    Disconnect();
}

//==============================================================================

void network_mgr::KickPlayer( s32 Index )
{
    ASSERT( m_pServer );
    m_pServer->KickPlayer( Index );
}

//==============================================================================

void network_mgr::BanPlayer( s32 Index )
{
    ASSERT( m_pServer );
    m_pServer->BanPlayer( Index );
}

//==============================================================================

s32 network_mgr::GetLocalPlayerSlot( s32 Index )
{
    if( m_pServer ) { return m_pServer->GetLocalPlayerSlot( Index ); }
    if( m_pClient ) { return m_pClient->GetLocalPlayerSlot( Index ); }

    ASSERT( FALSE );
    return 0;
}

//==============================================================================

s32 network_mgr::GetLocalSlot( s32 NetSlot )
{
    if( m_pServer )
    {
        for( s32 i = 0; i < m_LocalPlayerCount; i++ )
        {
            if( m_pServer->GetLocalPlayerSlot( i ) == NetSlot )
                return i;
        }
    }

    if( m_pClient )
    {
        for( s32 i = 0; i < m_LocalPlayerCount; i++ )
        {
            if( m_pClient->GetLocalPlayerSlot( i ) == NetSlot )
                return i;
        }
    }

    return -1;
}

//==============================================================================

xbool network_mgr::IsLocal( s32 NetSlot )
{
    return ( GetLocalSlot( NetSlot ) != -1 );
}

//==============================================================================

s32 network_mgr::GetClientIndex( void )
{
    if( m_pServer ) { return -1; }

    ASSERT( m_pClient );
    return m_pClient->GetClientIndex();
}

//==============================================================================

s32 network_mgr::GetClientIndex( s32 PlayerIndex )
{
    if( m_pServer )
    {
        return m_pServer->GetClientIndex( PlayerIndex );
    }

    ASSERT( m_pClient );

    if( IsLocal( PlayerIndex ) )
    {
        return m_pClient->GetClientIndex();
    }

    return -1;
}

//==============================================================================

s32 network_mgr::GetClientPlayerSlot( s32 ClientIndex, s32 PlayerIndex )
{
    ASSERT( m_pServer );
    return m_pServer->GetClientPlayerSlot( ClientIndex, PlayerIndex );
}

//==============================================================================

s32 network_mgr::GetClientPlayerCount( s32 ClientIndex )
{
    ASSERT( m_pServer );
    return m_pServer->GetClientPlayerCount( ClientIndex );
}

//==============================================================================

f32 network_mgr::GetClientPing( s32 ClientIndex )
{
    ASSERT( m_pServer );
    return m_pServer->GetClientPing( ClientIndex );
}

//==============================================================================

f32 network_mgr::GetAveragePing( void )
{
    ASSERT( m_pServer );
    return m_pServer->GetAveragePing();
}

//==============================================================================

pain_queue* network_mgr::GetPainQueue( void )
{
    if( m_pClient )
    {
        return m_pClient->GetPainQueue();
    }
    return NULL;
}

//==============================================================================

const char* network_mgr::GetStateName( void )
{
    if( m_pServer )
    {
        return ::GetStateName(m_pServer->GetState());
    }

    if( m_pClient )
    {
        return ::GetStateName(m_pClient->GetState());
    }

    return "<Undefined>";
}

//==============================================================================

xbool network_mgr::IsLoggedIn( void )
{
    ASSERT( m_pClient );

    return m_pClient->IsLoggedIn();
}

//==============================================================================

xbool network_mgr::IsRequestingMission( void )
{
    ASSERT( m_pClient );

    // If we lost the connection, bail out early and set the exit reason.
    if( !m_pClient->IsConnected() )
    {
        g_ActiveConfig.SetExitReason( GAME_EXIT_CONNECTION_LOST );
        return FALSE;
    }

    return ( m_pClient->GetState() == STATE_CLIENT_REQUEST_MISSION );
}
