//=========================================================================
//
//  ClientProxy.cpp
//
//=========================================================================
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//  Not to be used without express permission of Inevitable Entertainment, Inc.
//

#include "ClientProxy.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "x_log.hpp"
#include "GameServer.hpp"
#include "NetworkMgr/Messages/MsgMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "StateMgr/MapList.hpp"

extern game_server* g_pServer;

//------------------------------------------------------------------------------
client_proxy::client_proxy(void)
{
    m_IsConnected       = FALSE;
    x_memset(m_UniqueId,0,sizeof(m_UniqueId));
    m_PacketSendDelay   = 0.0f;
    m_ConnMgr.ClearConnection();
    Clear();
}

//------------------------------------------------------------------------------
client_proxy::~client_proxy( void )
{
}

//------------------------------------------------------------------------------
void client_proxy::Init( net_socket*         pLocalSocket,
                         const net_address&  Remote,      
                         s32                 ClientIndex, 
                         const byte*         pUniqueId )
{
    ASSERT( !m_IsConnected );

    Clear();

    m_IsConnected       = TRUE;
    m_PacketSendDelay   = 0.0f;
    m_ClientIndex       = ClientIndex;

    MsgMgr.NewClient( m_ClientIndex );

    x_memcpy(m_UniqueId,pUniqueId,sizeof(m_UniqueId));
    m_UpdateMgr.Init( ClientIndex );
    m_PainQueue.Init( ClientIndex );
    m_GMMgr.Init();
    m_VoiceProxy.Init( ClientIndex );

    m_ConnMgr.Init( *pLocalSocket, 
                    Remote, 
                    &m_UpdateMgr,
                    &m_PainQueue,
                    &m_GMMgr, 
                    &m_VoiceProxy,
                    ClientIndex,
                    TRUE );

    EnterState( STATE_CLIENT_INIT );
}

//------------------------------------------------------------------------------
void client_proxy::Kill( void )
{
    ASSERT( m_IsConnected );
    m_IsConnected = FALSE;
    SetState( STATE_CLIENT_IDLE );
    m_UpdateMgr.Kill();
    m_GMMgr.Kill();
    m_VoiceProxy.Kill();
}

//------------------------------------------------------------------------------
void client_proxy::Update( f32 DeltaTime, server_state ServerState )
{
    m_ConnMgr.SetServerState( ServerState );

    if( (m_State != m_ConnMgr.GetClientState()) && (m_ConnMgr.IsConnected()) )
    {
        SetState( m_ConnMgr.GetClientState() );
    }

    m_ConnMgr.Update( DeltaTime );

    UpdateState( DeltaTime );

    m_PacketSendDelay -= DeltaTime;

    if( (m_ConnMgr.IsConnected()==FALSE) && m_IsConnected && (GetState()!=STATE_CLIENT_COOLDOWN) && (GetState()!=STATE_CLIENT_DISCONNECT) )
    {
        LOG_WARNING( "client_proxy::Update", 
                     "Client:%d -- Disconnecting!",
                     m_ConnMgr.GetClientIndex() );
        Disconnect( GAME_EXIT_CONNECTION_LOST );
    }
}

//------------------------------------------------------------------------------
void client_proxy::Disconnect( exit_reason Reason )
{
    netstream Request;
    //
    // NOTE: The state transition to DISCONNECT will cause the connmgr to shutdown
    // so we need to set the exit reason prior to switching states. Although the state
    // *SHOULD* only ever be set by the remote client, this is the only exception. If
    // we received a disconnect packet, then that means the client has already shutdown
    // the connection so he can't advance the state.
    //
    SetExitReason( Reason );
    SetState( STATE_CLIENT_DISCONNECT );

    LOG_WARNING( "client_proxy::Disconnect",
                 "Client:%d -- Reason:%s",
                 m_ConnMgr.GetClientIndex(), GetExitReasonName(Reason) );
    // We should only be sending disconnect packets if the server has decided to disconnect the
    // client. If the client has closed the connmgr connection (by changing it's state to DISCONNECT), then we don't
    // need to tell it again to shutdown.
}

//------------------------------------------------------------------------------
void client_proxy::Clear( void )
{
    s32 i;

    x_memset( m_UniqueId, 0, sizeof(m_UniqueId) );
    m_LocalPlayerCount = 0;
    for( i = 0; i < NET_MAX_PER_CLIENT; i++ )
    {
        m_LocalPlayerSlot[i] = -1;
    }
}

//------------------------------------------------------------------------------
void client_proxy::AddPlayer( s32 Index, const char* pName, u64 Identifier )
{
    if( m_LocalPlayerCount >= NET_MAX_PER_CLIENT )
    {
        return;
    }

    m_LocalPlayerSlot[m_LocalPlayerCount] = Index;
    x_strcpy( m_PlayerNames[m_LocalPlayerCount], pName );
    m_PlayerIdentifier[m_LocalPlayerCount] = Identifier;

    m_LocalPlayerCount++;
    m_VoiceProxy.SetPlayerSlot( Index );
}

//------------------------------------------------------------------------------

s32 client_proxy::GetPlayerSlot( s32 Index )
{
    ASSERT( Index < m_LocalPlayerCount );
    return m_LocalPlayerSlot[Index];
}

//------------------------------------------------------------------------------

const net_address& client_proxy::GetAddress(void)
{
    return m_ConnMgr.GetAddress();
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
xbool client_proxy::ReceivePacket(netstream& BitStream)
{
    if( m_ConnMgr.ReceivePacket( BitStream ) )
        return TRUE;

    if( BitStream.Validate() == FALSE )
    {
        return FALSE;
    }
    // It depends on which state we're in what we do with a received netstream.
    conn_packet_type PacketType;
    u64 ConnectionID;
    s32 Error;
    if( !ReadConnectionPacket(BitStream, PacketType, ConnectionID) ||
        (ConnectionID != m_ConnMgr.GetConnectionID()) )
        return FALSE;
    if( IsMissionPacket(PacketType) )
    {
        u32 Generation;
        if( !CanReadConnectionBits(BitStream, 32) )
            return TRUE;
        BitStream.ReadU32( Generation );
        if( BitStream.Overwrite() || (Generation != (u32)m_ConnMgr.GetSessionID()) )
            return TRUE;
    }

    // If we didn't special case the packet, then we
    // deal with generic requests from the server.
    switch( PacketType )
    {
    case CONN_PACKET_END_MISSION:
        ProcessEndMission( BitStream );
        break;

    case CONN_PACKET_MISSION_REQUEST:
        ProcessMissionRequest( BitStream );
        break;

    case CONN_PACKET_VERIFY_REQUEST:
        ProcessVerifyRequest( BitStream );
        break;

    case CONN_PACKET_DISCONNECT:
        if( !CanReadConnectionBits(BitStream, 32) )
            return TRUE;
        BitStream.ReadS32(Error);
        if( !IsValidExitReason(Error) || !FinishConnectionPacket(BitStream) )
            return TRUE;

        LOG_WARNING( "client_proxy::ReceivePacket", 
                     "Client:%d -- Disconnecting!",
                     m_ConnMgr.GetClientIndex() );
        Disconnect((exit_reason)Error);
        break;

    default:
        return FALSE;
        break;
    }
    return TRUE;
}

//------------------------------------------------------------------------------
xbool client_proxy::ProvideUpdate(void)
{
    if ( (IsConnected()==FALSE) || ((GetState() != STATE_CLIENT_SYNC) && (GetState() != STATE_CLIENT_INGAME)) )
    {
        return FALSE;
    }

    // Figure out whether or not it is time to send another update
    // to this client.
    if( m_PacketSendDelay <= 0.0f )
    {
        while( m_PacketSendDelay <= 0.0f )
        {
            m_PacketSendDelay += m_ConnMgr.GetShipInterval();
        }
        return m_ConnMgr.SendPacket();
    }

    return FALSE;
}

//------------------------------------------------------------------------------
xbool client_proxy::IsInGame(void)
{
    return m_ConnMgr.GetClientState() == STATE_CLIENT_INGAME;
}

//------------------------------------------------------------------------------
xbool client_proxy::HasMapLoaded(void)
{
    return m_ConnMgr.GetClientState() > STATE_CLIENT_LOAD_MISSION;
}

//------------------------------------------------------------------------------
void client_proxy::QueueVoice(const byte* pBuffer, s32 Length)
{
    m_ConnMgr.QueueVoice(pBuffer,Length);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const byte* client_proxy::GetUniqueId(void)
{
    return m_UniqueId;
}

//------------------------------------------------------------------------------

xbool client_proxy::KickPlayer( s32 PlayerId )
{
    LOG_MESSAGE( "client_proxy::KickPlayer", 
                 "Client:%d -- Slot:%d", 
                 m_ConnMgr.GetClientIndex(), PlayerId );

    s32 i;

    if (!IsConnected())
        return FALSE;

    for( i=0 ; i<m_LocalPlayerCount ; i++ )
    {
        if( PlayerId == m_LocalPlayerSlot[i] )
        {
            LOG_WARNING( "client_proxy::KickPlayer", 
                         "Client:%d -- Disconnecting!",
                         m_ConnMgr.GetClientIndex() );
            Disconnect( GAME_EXIT_PLAYER_KICKED );
            //MsgMgr.Message( MSG_HOST_KICKED_P,  0, ARG_PLAYER, PlayerId );      
            return TRUE;
        }
    }
    return FALSE;
}

//------------------------------------------------------------------------------
s32 client_proxy::ReadFromVoiceFifo( byte* pBuffer, s32 MaxLength )
{
    return m_VoiceProxy.Read( pBuffer, MaxLength );
}

//------------------------------------------------------------------------------
void client_proxy::WriteToVoiceFifo( const byte* pBuffer, s32 Length )
{
    m_VoiceProxy.Write( pBuffer, Length );
}

//------------------------------------------------------------------------------

void client_proxy::ProcessEndMission( netstream& BitStream )
{
    (void)BitStream;
}

//------------------------------------------------------------------------------

void client_proxy::ProcessVerifyRequest( netstream& BitStream )
{
    const map_entry*    pMapEntry;
    s32                 Checksum;
    netstream           Reply;

    if( !CanReadConnectionBits(BitStream, 32) )
        return;
    BitStream.ReadS32( Checksum );
    if( !FinishConnectionPacket(BitStream) )
        return;

    pMapEntry = g_MapList.Find( g_ActiveConfig.GetLevelID() );
    ASSERT( pMapEntry );

    if( pMapEntry )
    {
        if( pMapEntry->GetFlags() & MF_DOWNLOAD_MAP )
        {
            LOG_MESSAGE( "client_proxy::ProcessVerifyRequest", "Client:%d, checksum sent:0x%08x, expected:0x%08x", m_ConnMgr.GetClientIndex(), Checksum, g_ActiveConfig.GetLevelChecksum() );

            if( g_ActiveConfig.GetLevelChecksum() != Checksum )
            {
                LOG_WARNING( "client_proxy::ProcessVerifyRequest", "Client:%d, checksum failure on custom map", m_ConnMgr.GetClientIndex() );
                Disconnect( GAME_EXIT_INVALID_MISSION );
                return;
            }
        }
    }
    else
    {
        // This *SHOULD NEVER HAPPEN* unless the packet is malformed
        Disconnect( GAME_EXIT_INVALID_MISSION );
        return;
    }

    m_ConnMgr.OpenPacket( Reply, CONN_PACKET_VERIFY_RESPONSE );
    Reply.WriteS32  ( g_ActiveConfig.GetLevelID() );
    Reply.WriteS32  ( g_ActiveConfig.GetLevelChecksum() );
    Reply.Close     ();
    m_ConnMgr.Send  ( Reply );
}

//------------------------------------------------------------------------------
void client_proxy::ProcessMissionRequest( netstream& BitStream )
{
    if( !FinishConnectionPacket(BitStream) )
        return;

    netstream    Reply;

    LOG_MESSAGE( "client_proxy::ProcessMissionRequest","Processed a map request from client %d(%s)",m_ClientIndex,m_ConnMgr.GetAddress().GetStrAddress());

    m_ConnMgr.OpenPacket( Reply, CONN_PACKET_MISSION_RESPONSE );
    Reply.WriteS32  ( g_ActiveConfig.GetLevelID() );
    Reply.WriteFlag ( g_ActiveConfig.IsFriendlyFireEnabled() );
    Reply.WriteFlag ( g_ActiveConfig.IsVoiceEnabled() );
    Reply.WriteS32  ( GameMgr.GetGameType() );
    Reply.Close     ();
    m_ConnMgr.Send  ( Reply );
}

//------------------------------------------------------------------------------
exit_reason client_proxy::GetExitReason( void )
{ 
    return m_ExitReason;
}

//------------------------------------------------------------------------------
void client_proxy::SetExitReason( exit_reason Reason )
{ 
    m_ExitReason = Reason;
}
