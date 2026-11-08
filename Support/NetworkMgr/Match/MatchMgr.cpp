//==============================================================================
//
//  MatchMgr.cpp
//
//==============================================================================

#include "x_files.hpp"
#include "e_Network.hpp"
#include "MatchMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "x_log.hpp"
#include "Configuration/GameConfig.hpp"
#include "NetworkMgr/Connection/GameClient.hpp"
#include "NetworkMgr/ServerVersion.hpp"
#include "StateMgr/MapList.hpp"

//==============================================================================

match_mgr g_MatchMgr;

//------------------------------------------------------------------------------
match_mgr::match_mgr( void )
{
    m_Initialized     = FALSE;
    m_pProvider       = &GetNativeMatchProvider();
    m_pSession        = NULL;
    m_SessionRevision = 0;
    m_ServiceResult   = MATCH_RESULT_OK;
    m_ConnectStatus   = MATCH_CONN_UNAVAILABLE;
    m_State           = MATCH_IDLE;
    m_pSocket         = NULL;
    m_AcquisitionMode = ACQUIRE_INVALID;
    x_memset( m_Nickname, 0, sizeof( m_Nickname ) );
    x_memset( m_PlayerIdentifier, 0, sizeof( m_PlayerIdentifier ) );
    x_memset( m_UniqueId, 0, sizeof( m_UniqueId ) );

    m_ServerListRevision = 0;

    SetConnectStatus( MATCH_CONN_UNAVAILABLE );
}

//------------------------------------------------------------------------------
match_mgr::~match_mgr( void )
{
    Kill();
}

//------------------------------------------------------------------------------
static const server_info* GetLocalServerInfo( server_info& Info )
{
    if( !g_NetworkMgr.IsServer() || !GameMgr.GameInProgress() )
        return NULL;

    Info                  = g_ActiveConfig.GetConfig();
    Info.Players          = GameMgr.GetScore().NConnected;
    const map_entry* pMap = g_MapList.Find( Info.Level, Info.GameTypeID );

    if( pMap )
    {
        x_wstrncpy( Info.MissionName, xwstring( pMap->GetDisplayName() ), NET_MISSION_NAME_LENGTH );
        Info.MissionName[NET_MISSION_NAME_LENGTH - 1] = 0;
    }

    Info.PasswordEnabled = !!( Info.Flags & SERVER_HAS_PASSWORD );

    return &Info;
}

//==============================================================================

xbool match_mgr::Init( net_socket& Local, const net_address Broadcast )
{
    if( m_Initialized || Local.IsEmpty() )
        return FALSE;

    m_Initialized      = TRUE;
    m_pSocket          = &Local;
    m_BroadcastAddress = Broadcast;
    m_ConnectStatus    = MATCH_CONN_IDLE;
    m_AcquisitionMode  = ACQUIRE_INVALID;
    x_memset( m_Nickname, 0, sizeof( m_Nickname ) );
    x_memset( m_PlayerIdentifier, 0, sizeof( m_PlayerIdentifier ) );

    ResetServerList();
    m_pSession = m_pProvider->CreateSession( Local, g_ServerVersion );

    if( !m_pSession )
    {
        m_Initialized = FALSE;
        m_pSocket     = NULL;
        return FALSE;
    }

    m_SessionRevision = 0;
    SetState( MATCH_IDLE );

    return TRUE;
}

//==============================================================================

xbool match_mgr::SetProvider( match_provider& Provider )
{
    if( m_Initialized )
        return FALSE;

    m_pProvider = &Provider;

    return TRUE;
}

//==============================================================================

u32 match_mgr::GetCapabilities( void ) const
{
    return m_pProvider->GetCapabilities();
}

//==============================================================================

xbool match_mgr::IsReady( void ) const
{
    return m_Initialized && m_pSocket && !m_pSocket->IsEmpty();
}

//==============================================================================

xstring match_mgr::GetDirectoryAddress( void ) const
{
    return m_pSession ? m_pSession->GetDirectoryAddress() : xstring( "" );
}

//==============================================================================

match_result match_mgr::ConfigureDirectory( const char* pAddress )
{
    if( !IsReady() || g_NetworkMgr.IsClient() || g_NetworkMgr.IsServer() )
        return MATCH_RESULT_UNAVAILABLE;

    return m_pSession->ConfigureDirectory( pAddress );
}

//==============================================================================

match_result match_mgr::ResolveTarget( const char* pEndpoint, match_join_target& Target ) const
{
    if( !( GetCapabilities() & MATCH_CAP_DIRECT_CONNECT ) )
        return MATCH_RESULT_UNSUPPORTED;

    return m_pProvider->ResolveTarget( pEndpoint, Target );
}

//==============================================================================

match_result match_mgr::PrepareDirectConnection( const char* pEndpoint )
{
    if( !IsReady() || g_NetworkMgr.IsClient() || g_NetworkMgr.IsServer() )
        return MATCH_RESULT_UNAVAILABLE;

    match_join_target Target;
    match_result      Result = ResolveTarget( pEndpoint, Target );

    if( Result != MATCH_RESULT_OK )
        return Result;

    if( Target.Transport != "udp" )
        return MATCH_RESULT_UNSUPPORTED;

    Result = m_EndpointResolver.Begin( Target.Endpoint, NET_GAME_PORT );

    return ( Result == MATCH_RESULT_OK ) ? PollDirectConnection( 0.0f ) : Result;
}

//==============================================================================

match_result match_mgr::PollDirectConnection( f32 DeltaTime )
{
    if( !IsReady() || g_NetworkMgr.IsClient() || g_NetworkMgr.IsServer() )
    {
        CancelDirectConnection();
        return MATCH_RESULT_UNAVAILABLE;
    }

    net_address  Address;
    match_result Result = m_EndpointResolver.Poll( DeltaTime, Address );

    if( Result != MATCH_RESULT_OK )
        return Result;

    g_PendingConfig.SetRemoteAddress( Address );
    g_PendingConfig.GetConfig().External       = Address;
    g_PendingConfig.GetConfig().ConnectionType = CONNECT_DIRECT;
    g_PendingConfig.GetConfig().Flags          = 0;
    g_PendingConfig.SetPlayerCount( 1 );
    g_PendingConfig.SetServerName( xwstring( Address.GetStrAddress() ) );

    return MATCH_RESULT_OK;
}

//==============================================================================

void match_mgr::CancelDirectConnection( void )
{
    m_EndpointResolver.Cancel();
}

//==============================================================================

xbool match_mgr::PrepareHost( s32 Port )
{
    if( !IsReady() || !( GetCapabilities() & MATCH_CAP_HOST ) ||
        ( Port < 1 ) || ( Port > 65535 ) )
        return FALSE;

    if( m_pSocket->GetPort() == Port )
        return TRUE;

    m_pSocket->Close();

    if( !m_pSocket->Bind( Port, NET_FLAGS_BROADCAST | NET_FLAGS_BIND_EXACT ) )
    {
        if( !m_pSocket->Bind() )
            SetConnectStatus( MATCH_CONN_UNAVAILABLE );

        return FALSE;
    }

    m_BroadcastAddress.SetPortNumber( Port );

    return TRUE;
}

//==============================================================================

void match_mgr::SetLocalIdentity( const char* pName, const byte* pId, s32 IdLength )
{
    x_strncpy( m_Nickname, pName, sizeof( m_Nickname ) );
    m_Nickname[sizeof( m_Nickname ) - 1] = '\0';
    SetUniqueId( pId, IdLength );
    u64 Identifier = 14695981039346656037ULL;

    for( s32 i = 0; i < IdLength; i++ )
        Identifier = ( Identifier ^ pId[i] ) * 1099511628211ULL;

    m_PlayerIdentifier[0] = Identifier;
}

//==============================================================================

void match_mgr::Kill( void )
{
    m_EndpointResolver.Kill();

    if( !m_Initialized )
        return;

    m_pSession->EndHost();
    delete m_pSession;
    m_pSession      = NULL;
    m_Initialized   = FALSE;
    m_pSocket       = NULL;
    m_State         = MATCH_IDLE;
    m_ConnectStatus = MATCH_CONN_UNAVAILABLE;
    m_MutedPlayers.Clear();
    ResetServerList();
}

//==============================================================================

void match_mgr::Update( f32 DeltaTime )
{
    if( !m_Initialized )
        return;

    if( m_pSession )
    {
        server_info Host;
        m_pSession->Update( DeltaTime, GetLocalServerInfo( Host ) );
        match_result Result = m_pSession->GetResult();

        if( Result != m_ServiceResult )
        {
            LOG_MESSAGE( "match_mgr::Update", "Session service result: %d", Result );
            m_ServiceResult = Result;
        }

        if( m_SessionRevision != m_pSession->GetRevision() )
        {
            LockLists();
            ResetServerList();

            for( s32 i = 0; i < m_pSession->GetServerCount(); i++ )
                AppendToServerList( m_pSession->GetServerInfo( i ) );

            m_SessionRevision = m_pSession->GetRevision();
            UnlockLists();
        }
    }

    if( ( m_State == MATCH_LOGIN ) || ( m_State == MATCH_CLIENT_ACTIVE ) )
    {
        exit_reason Reason = g_ActiveConfig.GetExitReason();

        if( ( Reason != GAME_EXIT_CONTINUE ) &&
            ( Reason != GAME_EXIT_ADVANCE_LEVEL ) &&
            ( Reason != GAME_EXIT_RELOAD_LEVEL ) &&
            ( Reason != GAME_EXIT_RELOAD_CHECKPOINT ) )
        {
            SetState( MATCH_IDLE );
            SetConnectStatus( MATCH_CONN_DISCONNECTED );
        }
        else if( ( m_State == MATCH_LOGIN ) && g_NetworkMgr.IsLoggedIn() )
        {
            SetConnectStatus( MATCH_CONN_CONNECTED );
            SetState( MATCH_CLIENT_ACTIVE );
        }
    }
}

//==============================================================================

xbool match_mgr::ReceivePacket( net_address& Remote, bitstream& Bitstream )
{
    if( !m_pSession )
        return FALSE;

    server_info Host;

    return m_pSession->ReceivePacket( Remote, Bitstream.GetDataPtr(),
                                      Bitstream.GetNBytes(), GetLocalServerInfo( Host ) );
}

//==============================================================================

void match_mgr::SetState( match_mgr_state NewState )
{
    m_State = NewState;

    switch( NewState )
    {
        case MATCH_BECOME_SERVER:
            StopAcquisition();

            if( m_pSession )
                m_pSession->BeginHost();

            m_State = MATCH_SERVER_ACTIVE;
            SetConnectStatus( MATCH_CONN_CONNECTED );
            break;

        case MATCH_UPDATE_SERVER:
            m_State = MATCH_SERVER_ACTIVE;
            SetConnectStatus( MATCH_CONN_CONNECTED );
            break;

        case MATCH_UNREGISTER_SERVER:
            if( m_pSession )
                m_pSession->EndHost();

            m_State = MATCH_IDLE;
            SetConnectStatus( MATCH_CONN_IDLE );
            break;

        case MATCH_IDLE:
            SetConnectStatus( MATCH_CONN_IDLE );
            break;

        default:
            break;
    }
}

//==============================================================================

void match_mgr::StartAcquisition( match_acquire AcquisitionMode )
{
    if( ( AcquisitionMode == ACQUIRE_SERVERS ) && m_pSession && IsReady() &&
        !g_NetworkMgr.IsServer() && !g_NetworkMgr.IsClient() )
    {
        m_AcquisitionMode = AcquisitionMode;
        m_pSession->StartSearch();
        return;
    }

    m_AcquisitionMode = AcquisitionMode;
    m_ConnectStatus   = MATCH_CONN_UNAVAILABLE;
    ResetServerList();
    SetState( MATCH_ACQUIRE_IDLE );
}

//==============================================================================

void match_mgr::StopAcquisition( void )
{
    if( m_pSession )
        m_pSession->StopSearch();

    m_AcquisitionMode = ACQUIRE_INVALID;
}

//==============================================================================

xbool match_mgr::IsAcquireComplete( void )
{
    if( m_pSession && m_pSession->IsSearching() )
        return FALSE;

    return ( ( m_State == MATCH_ACQUIRE_IDLE ) || ( m_State == MATCH_IDLE ) );
}

//==============================================================================

void match_mgr::StartIndirectLookup( void )
{
    m_State = MATCH_IDLE;
    SetConnectStatus( IsReady() ? MATCH_CONN_CONNECTING : MATCH_CONN_UNAVAILABLE );
}

//==============================================================================

void match_mgr::StartLogin( void )
{
    if( !IsReady() || g_NetworkMgr.IsClient() || g_NetworkMgr.IsServer() )
    {
        g_ActiveConfig.SetExitReason( GAME_EXIT_CANNOT_CONNECT );
        SetConnectStatus( MATCH_CONN_UNAVAILABLE );
        return;
    }

    SetConnectStatus( MATCH_CONN_CONNECTING );
    SetState( MATCH_LOGIN );
    g_NetworkMgr.BeginLogin();
}

//==============================================================================

#define LABEL_STRING(x) case x: return ( #x );

s32 match_mgr::GetServerCount( void )
{
    return m_Servers.GetCount();
}

//==============================================================================

server_info* match_mgr::GetServerInfo( s32 Index )
{
    if( ( Index < 0 ) || ( Index >= m_Servers.GetCount() ) )
        return NULL;

    return &m_Servers[Index];
}

//==============================================================================

s32 match_mgr::FindServerInfo( const server_info& Response )
{
    for( s32 i = 0; i < m_Servers.GetCount(); i++ )
    {
        if( m_Servers[i] == Response )
            return i;
    }

    return -1;
}

//==============================================================================

void match_mgr::ResetServerList( void )
{
    m_Servers.Clear();
    m_ServerListRevision++;
}

//==============================================================================

void match_mgr::AppendToServerList( const server_info& Response )
{
    m_Servers.Append( Response );
    m_ServerListRevision++;
}

//==============================================================================

const char* GetStateName( match_mgr_state Mode )
{
    switch( Mode )
    {
        case MATCH_IDLE: return "MATCH_IDLE";
        case MATCH_ACQUIRE_IDLE: return "MATCH_ACQUIRE_IDLE";
        case MATCH_BECOME_SERVER: return "MATCH_BECOME_SERVER";
        case MATCH_UPDATE_SERVER: return "MATCH_UPDATE_SERVER";
        case MATCH_SERVER_ACTIVE: return "MATCH_SERVER_ACTIVE";
        case MATCH_UNREGISTER_SERVER: return "MATCH_UNREGISTER_SERVER";
        case MATCH_LOGIN: return "MATCH_LOGIN";
        case MATCH_CLIENT_ACTIVE: return "MATCH_CLIENT_ACTIVE";
        default: ASSERT( FALSE ); return "<unknown>";
    }
}

//------------------------------------------------------------------------------
const char* GetStatusName( match_conn_status Status )
{
    switch( Status )
    {
        LABEL_STRING( MATCH_CONN_IDLE );
        LABEL_STRING( MATCH_CONN_UNAVAILABLE );
        LABEL_STRING( MATCH_CONN_CONNECTED );
        LABEL_STRING( MATCH_CONN_DISCONNECTED );
        LABEL_STRING( MATCH_CONN_CONNECTING );
        LABEL_STRING( MATCH_CONN_SESSION_ENDED );
        default: ASSERT( FALSE );
    }

    return "<undefined>";
}

//------------------------------------------------------------------------------
void match_mgr::SetUniqueId( const byte* Id, s32 IdLength )
{
    x_memset( m_UniqueId, 0, sizeof( m_UniqueId ) );

    if( IdLength >= (s32)sizeof( m_UniqueId ) )
    {
        IdLength = sizeof( m_UniqueId ) - 1;
    }

    x_memcpy( m_UniqueId, Id, IdLength );
}

//------------------------------------------------------------------------------
const byte* match_mgr::GetUniqueId( s32& IdLength )
{
    IdLength = sizeof( m_UniqueId );

    return m_UniqueId;
}

//==============================================================================

void match_mgr::LockLists( void )
{
    m_ListMutex.Enter();
}

//==============================================================================

void match_mgr::UnlockLists( void )
{
    ASSERT( m_ListMutex.IsLocked() );
    m_ListMutex.Exit();
}

//==============================================================================

void match_mgr::SetConnectStatus( match_conn_status Status )
{
    LOG_MESSAGE( "match_mgr::SetConnectStatus", "Connect status change from %s to %s", GetStatusName( m_ConnectStatus ), GetStatusName( Status ) );
    m_ConnectStatus = Status;
}

//==============================================================================

#undef LABEL_STRING

//==============================================================================

xbool match_mgr::IsPlayerMuted( u64 Identifier )
{
    return m_MutedPlayers.Find( Identifier ) >= 0;
}

//==============================================================================

void match_mgr::SetIsPlayerMuted( u64 Identifier, xbool IsMuted )
{
    s32 Index = m_MutedPlayers.Find( Identifier );

    if( IsMuted && ( Index < 0 ) )
        m_MutedPlayers.Append( Identifier );
    else if( !IsMuted && ( Index >= 0 ) )
        m_MutedPlayers.Delete( Index );
}

//==============================================================================
