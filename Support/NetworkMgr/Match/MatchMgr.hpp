#ifndef MATCH_MGR_HPP
#define MATCH_MGR_HPP

#include "x_files.hpp"
#include "e_Network.hpp"
#include "x_bitstream.hpp"
#include "x_threads.hpp"
#include "NetworkMgr/Game/GameTypes.hpp"
#include "NetworkMgr/NetLimits.hpp"
#include "MatchProvider.hpp"
#include "MatchServer.hpp"
#include "EndpointResolver.hpp"

//------------------------------------------------------------------------------

enum match_conn_status
{
    MATCH_CONN_IDLE,
    MATCH_CONN_UNAVAILABLE,
    MATCH_CONN_CONNECTED,
    MATCH_CONN_DISCONNECTED,
    MATCH_CONN_CONNECTING,
    MATCH_CONN_SESSION_ENDED,
};

//==============================================================================

enum match_mgr_state
{
    MATCH_IDLE,
    MATCH_ACQUIRE_IDLE,
    MATCH_BECOME_SERVER,
    MATCH_UPDATE_SERVER,
    MATCH_SERVER_ACTIVE,
    MATCH_UNREGISTER_SERVER,
    MATCH_LOGIN,
    MATCH_CLIENT_ACTIVE,
};

//------------------------------------------------------------------------------
enum match_acquire
{
    ACQUIRE_INVALID = -1,
    ACQUIRE_SERVERS = 0,
};

//==============================================================================

class match_mgr
{
public:
    match_mgr( void );
    ~match_mgr( void );
    xbool        Init( net_socket& Local, const net_address Broadcast );
    xbool        SetProvider( match_provider& Provider );
    u32          GetCapabilities( void ) const;
    xbool        IsReady( void ) const;
    xstring      GetDirectoryAddress( void ) const;
    match_result ConfigureDirectory( const char* pAddress );

    match_result GetServiceResult( void ) const
    {
        return m_ServiceResult;
    }

    match_result ResolveTarget( const char* pEndpoint, match_join_target& Target ) const;
    match_result PrepareDirectConnection( const char* pEndpoint );
    match_result PollDirectConnection( f32 DeltaTime );
    void         CancelDirectConnection( void );
    xbool        PrepareHost( s32 Port );
    void         SetLocalIdentity( const char* pName, const byte* pId, s32 IdLength );
    void         Kill( void );
    void         Update( f32 DeltaTime );
    xbool        ReceivePacket( net_address& Remote, bitstream& Bitstream );
    s32          GetServerCount( void );

    u32 GetServerListRevision( void ) const
    {
        return m_ServerListRevision;
    }

    server_info* GetServerInfo( s32 Index );
    s32          FindServerInfo( const server_info& Response );
    void         SetState( match_mgr_state Mode );

    match_mgr_state GetState( void )
    {
        return m_State;
    }

    match_conn_status GetConnectStatus( void )
    {
        return m_ConnectStatus;
    }

    void        SetConnectStatus( match_conn_status Status );
    void        SetUniqueId( const byte* Id, s32 IdLength );
    const byte* GetUniqueId( s32& IdLength );

    void  StartAcquisition( match_acquire AcquisitionMode );
    void  StopAcquisition( void );
    xbool IsAcquireComplete( void );
    void  StartIndirectLookup( void );
    void  StartLogin( void );

    u64 GetPlayerIdentifier( s32 Index )
    {
        return m_PlayerIdentifier[Index];
    }

    void SetNickname( const char* pName )
    {
        x_strcpy( m_Nickname, pName );
    }

    void LockLists( void );
    void UnlockLists( void );

    xbool IsPlayerMuted( u64 Identifier );
    void  SetIsPlayerMuted( u64   Identifier,
                            xbool IsMuted );

private:
    void AppendToServerList( const server_info& Response );
    void ResetServerList( void );

    xbool             m_Initialized;
    match_provider*   m_pProvider;
    match_session*    m_pSession;
    u32               m_SessionRevision;
    match_result      m_ServiceResult;
    xarray<u64>       m_MutedPlayers;
    match_mgr_state   m_State;
    match_conn_status m_ConnectStatus;

    net_socket* m_pSocket;
    net_address m_BroadcastAddress;

    xarray<server_info> m_Servers;
    u32                 m_ServerListRevision;

    char m_Nickname[32];
    u64  m_PlayerIdentifier[NET_MAX_PER_CLIENT];
    byte m_UniqueId[NET_MAX_ID_LENGTH];

    match_acquire     m_AcquisitionMode;
    xmutex            m_ListMutex;
    endpoint_resolver m_EndpointResolver;
};

//==============================================================================

extern match_mgr g_MatchMgr;

const char* GetStateName( match_mgr_state State );
const char* GetStatusName( match_conn_status Status );

//==============================================================================
#endif
//==============================================================================
