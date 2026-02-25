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

#include "../Apps/GameApp/Main.hpp"
#include "x_files.hpp"
#include "Entropy.hpp"
#include "NetworkMgr.hpp"
#include "ServerVersion.hpp"
#include "NetLimits.hpp"

#include "GameMgr.hpp"
#include "GameState.hpp"
#include "Configuration/GameConfig.hpp"
#include "GameServer.hpp"
#include "GameClient.hpp"
#include "Voice/VoiceMgr.hpp"
#include "GameLib/LevelLoader.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StateMgr/MapList.hpp"

#if defined(TARGET_PS2)
#   include "e_Audio.hpp"
#   include "ps2/iopmanager.hpp"
#   include "IOManager/io_mgr.hpp"
#   include "Audio/audio_hardware.hpp"
#   include "e_Memcard.hpp"
#   include "MemCardMgr/MemCardMgr.hpp"
#endif

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
}

//==============================================================================
//  LIFECYCLE
//==============================================================================

void network_mgr::Init( void )
{
#if defined(TARGET_XBOX) || defined(TARGET_PC)
    // On Xbox, net_Init is called early so silent sign-in can happen before
    // the main menu.  On PC we do the same — the call is harmless if the
    // network layer is already up.
    net_Init();
#endif
}

//==============================================================================

void network_mgr::Kill( void )
{
#if defined(TARGET_XBOX) || defined(TARGET_PC)
    g_MatchMgr.Kill();
    net_Kill();
#endif
}

//==============================================================================

void network_mgr::SetOnline( xbool IsOnline )
{
    // -------------------------------------------------------------------------
    // Tear down the socket and notify the match manager.
    // -------------------------------------------------------------------------
    if( m_IsOnline && !IsOnline )
    {
        if( !m_LocalSocket.IsEmpty() )
        {
            m_LocalSocket.Close();
        }

#if defined(TARGET_PS2)
        g_MatchMgr.Kill();
        net_Kill();
#else
        // Xbox / PC: just park the match manager; no full kill needed.
        g_MatchMgr.SetState( MATCH_IDLE );
#endif
    }

    // -------------------------------------------------------------------------
    // Reboot the IOP module when switching online state.
    // -------------------------------------------------------------------------
#if defined(TARGET_PS2)
    if( m_IsOnline != IsOnline )
    {
    #if !defined(TARGET_DEV)
        g_RscMgr.Unload( "DX_FrontEnd.audiopkg"    );
        g_RscMgr.Unload( "SFX_FrontEnd.audiopkg"   );
        g_RscMgr.Unload( "MUSIC_FrontEnd.audiopkg" );

        g_AudioMgr.UnloadAllPackages();
        g_AudioMgr.Kill();
        g_LevelLoader.UnmountDefaultFilesystems();

        // Poll memory cards before rebooting the IOP.
        g_UIMemCardMgr.Repoll( this, &network_mgr::OnRepollCB );
        g_UIMemCardMgr.Update( 0.001f );
        while( !g_UIMemCardMgr.IsActionDone() )
        {
            x_DelayThread( 1 );
        }

        g_MemcardHardware.Kill();
        g_IoMgr.Kill();
        input_Kill();
        g_IopManager.Reboot( TRUE, FALSE, "DNAS300.IMG" );
        input_Init();
        g_MemcardHardware.Init();

        // Immediately remount to clear spurious "card changed" errors.
        g_UIMemCardMgr.RebootCheck();
        g_UIMemCardMgr.Update( 0.001f );
        while( !g_UIMemCardMgr.IsActionDone() )
        {
            x_DelayThread( 1 );
        }
    #endif // !TARGET_DEV
    }

    // Going online — (re)initialise the network layer.
    if( !m_IsOnline && IsOnline )
    {
        net_Init();
    }

    // Reload front-end audio packages after the network is up (PS2 memory layout
    // is different depending on online vs offline mode).
    if( m_IsOnline != IsOnline )
    {
    #if !defined(TARGET_DEV)
        g_IoMgr.Init();
        g_LevelLoader.MountDefaultFilesystems();

        g_AudioMgr.Init( 5512 * 1024 );
        g_RscMgr.Load( PRELOAD_FILE("DX_FrontEnd.audiopkg"    ) );
        g_RscMgr.Load( PRELOAD_FILE("SFX_FrontEnd.audiopkg"   ) );
        g_RscMgr.Load( PRELOAD_FILE("MUSIC_FrontEnd.audiopkg" ) );

        global_settings& Settings = g_StateMgr.GetActiveSettings();
        Settings.CommitAudio();
    #endif // !TARGET_DEV
    }
#endif // TARGET_PS2

    // -------------------------------------------------------------------------
    // Verify interface after init and reinitialise if needed.
    // -------------------------------------------------------------------------
#if defined(TARGET_XBOX) || defined(TARGET_PC)
    if( !m_IsOnline && IsOnline )
    {
        net_Init();

        interface_info Info;
        net_GetInterfaceInfo( -1, Info );
        if( Info.NeedsServicing )
        {
            // Interface needs another cycle to come up cleanly.
            net_Kill();
            net_Init();
        }
    }
#endif

    net_SetVersionKey( g_ServerVersion );
    m_IsOnline = IsOnline;
}

//==============================================================================

void network_mgr::Update( f32 DeltaTime )
{
    xbool       Used       = FALSE;
    net_address Remote;
    netstream   BitStream;
    exit_reason ExitReason;

#if defined(TARGET_PS2)
    extern xbool g_RebootPending;
    if( g_RebootPending )
    {
        scePrintf( "<<<POWEROFF SILENCED>>>\n" );
        g_StateMgr.Reboot( REBOOT_HALT );
    }
#endif

    if( DeltaTime > 0.5f )
    {
        LOG_WARNING( "network_mgr::Update", "Long frame: %2.02fsec between updates", DeltaTime );
    }

    // -------------------------------------------------------------------------
    // Run game logic — skip if split-screen game is paused.
    // -------------------------------------------------------------------------
    const xbool bIsSplitScreen       = ( GetLocalPlayerCount() > 1 );
    const xbool bIsPaused            = g_StateMgr.IsPaused();
    const xbool bIsSplitScreenPaused = bIsSplitScreen && bIsPaused;

    if( !bIsSplitScreenPaused && GameMgr.GameInProgress() )
    {
        GameMgr.Logic( DeltaTime );
        NetObjMgr.Logic( DeltaTime );
    }

    g_VoiceMgr.Update( DeltaTime );

#if defined(TARGET_XBOX)
    // PS2/PC runs the match manager on a separate thread; Xbox do not.
    g_MatchMgr.Update( DeltaTime );
#endif

    // -------------------------------------------------------------------------
    // Receive network packets
    // -------------------------------------------------------------------------
    s32 MaxCount = 0;
    ExitReason   = g_ActiveConfig.GetExitReason();

#if defined(TARGET_XBOX) || defined(TARGET_PC)
    if( m_IsOnline && !m_LocalSocket.IsEmpty() )
    {
        // Check for interface loss (cable pull, network down, duplicate login).
        interface_info Info;
        net_GetInterfaceInfo( -1, Info );

        if( !Info.Address || !Info.IsAvailable )
        {
            g_ActiveConfig.SetExitReason( GAME_EXIT_NETWORK_DOWN );
        }

        if( !m_LocalSocket.IsEmpty() &&
            ( ExitReason == GAME_EXIT_NETWORK_DOWN ||
              ExitReason == GAME_EXIT_DUPLICATE_LOGIN ) )
        {
            // Closing the socket cascades through the state machines and causes
            // the server / client to shut themselves down cleanly.
            m_LocalSocket.Close();
        }
        else
        {
            xtimer t;
            t.Start();

            while( m_LocalSocket.Receive( Remote, BitStream ) )
            {
                Used = FALSE;

                if( m_pServer )
                {
                    ASSERT( m_pClient == NULL );
                    Used = m_pServer->ReceivePacket( Remote, BitStream );
                }

                if( m_pClient )
                {
                    ASSERT( m_pServer == NULL );
                    Used = m_pClient->ReceivePacket( Remote, BitStream );
                }

                // Hand unrecognised packets to the match manager.
                if( !Used )
                {
                    g_MatchMgr.ReceivePacket( Remote, BitStream );
                }

                // Bail out if we're spending too long in the receive loop.
                if( ++MaxCount > 20 )
                {
                    LOG_WARNING( "network_mgr::Update",
                                 "Receive loop bail-out. Packets:%d  Time:%2.02fms",
                                 MaxCount, t.ReadMs() );
                    break;
                }
            }
        }
    }
#endif // TARGET_XBOX || TARGET_PC

    // -------------------------------------------------------------------------
    // Advance level or detect game completion.
    // -------------------------------------------------------------------------
    ExitReason = GAME_EXIT_CONTINUE;

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

        ExitReason = m_pServer->Update( DeltaTime );
    }

    if( m_pClient )
    {
        ASSERT( m_pServer == NULL );
        ExitReason = m_pClient->Update( DeltaTime );
    }

    // Propagate exit reason if nothing higher-priority already set it.
    if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE &&
        ExitReason                     != GAME_EXIT_CONTINUE )
    {
        g_ActiveConfig.SetExitReason( ExitReason );
    }

    LOG_FLUSH();
}

//==============================================================================

void network_mgr::LoadMissionComplete( void )
{
    ASSERT( g_StateMgr.IsBackgroundThreadRunning() == FALSE );

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

    m_pServer = new game_server;

    // A little hack here. We can tell whether or not we should have any client
    // instances depending on whether or not we have a local network socket since
    // a 'local' game will not have a network socket. This is to make sure that
    // the server instance takes up less space when in an offline game. This will
    // minimize offline memory footprint.
    const xbool bIsOffline = ( g_ActiveConfig.GetPlayerCount() == 1 ) &&
                             ( g_ActiveConfig.GetGameTypeID()  != GAME_CAMPAIGN );

    if( m_LocalSocket.IsEmpty() || !bIsOffline )
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
    m_pClient = new game_client;
    m_pClient->Init( m_LocalSocket, g_ActiveConfig.GetRemoteAddress(), 1 );
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
        m_pClient->Kill();
        delete m_pClient;
        m_pClient = NULL;
    }
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

voice_proxy& network_mgr::GetVoiceProxy( s32 ClientIndex )
{
    ASSERT( m_pServer );
    return m_pServer->GetVoiceProxy( ClientIndex );
}

//==============================================================================

xbool network_mgr::IsServerABuddy( const char* pSearch )
{
    ASSERT( m_pServer );
    return m_pServer->IsServerABuddy( pSearch );
}

//==============================================================================

xbool network_mgr::HasPlayerBuddy( const char* pSearch )
{
    ASSERT( m_pServer );
    return m_pServer->HasPlayerBuddy( pSearch );
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

//==============================================================================

#if defined(TARGET_PS2)
void network_mgr::OnRepollCB( void )
{
    // do nothing!
}
#endif