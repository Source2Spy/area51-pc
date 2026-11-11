//==============================================================================
//
//  ConnMgr.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#include "x_files.hpp"
#include "x_log.hpp"
#include "x_threads.hpp"

#include "ConnMgr.hpp"

#include "NetworkMgr/Replication/UpdateMgr.hpp"
#include "NetworkMgr/Replication/PainQueue.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "NetworkMgr/Messages/MsgMgr.hpp"
#include "NetworkMgr/Messages/MsgClient.hpp"
#include "NetworkMgr/Replication/GMMgr.hpp"
#include "NetworkMgr/Voice/VoiceProxy.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"

#include "NetworkMgr/ServerVersion.hpp"

//==============================================================================

#if defined(TARGET_DEV) && defined(X_DEBUG)
#endif

//==============================================================================

//#define TRACK_HEARTBEATS
//#define TRACK_ACKS

#ifdef VERBOSE_NETWORK
extern const char* s_StateName[];
#endif

//==============================================================================

xbool SHOW_PACKET_INFO = FALSE;
xbool s_SkipTickCheck  = FALSE;

//==============================================================================
// Acknowledge bits class
//==============================================================================

ack_bits::ack_bits( void )
{
    Clear();
}

//------------------------------------------------------------------------------

void ack_bits::Clear( void )
{
    for( s32 i = 0; i < ACKSLOTS; i++ )
        Bits[i] = 0xFFFF;
}

//------------------------------------------------------------------------------

void ack_bits::Print( void )
{
    x_DebugMsg( "%04X", Bits[ACKSLOTS-1] );
    for( s32 i = ACKSLOTS-2; i >= 0; i-- )
        x_DebugMsg( ",%04X", Bits[i] );
}

//------------------------------------------------------------------------------

void ack_bits::ShiftLeft( s32 Shift )
{
    while( Shift > 0 )
    {
        Shift--;
        Bits[ACKSLOTS-1] <<= 1;
        for( s32 i = ACKSLOTS-2; i >= 0; i-- )
        {
            if( Bits[i  ] &  0x8000 )
                Bits[i+1] |= 0x0001;
            Bits[i] <<= 1;
        }
    }
}

//------------------------------------------------------------------------------

xbool ack_bits::Bit( s32 Position )
{
    s32 Slot = Position / (16  );
    s32 Off  = Position & (16-1);

    return( (Bits[Slot] & (1<<Off)) != 0 );
}

//------------------------------------------------------------------------------

void ack_bits::Set( s32 Position )
{
    s32 Slot = Position / (16  );
    s32 Off  = Position & (16-1);

    Bits[Slot] |= (1 << Off);
}

//------------------------------------------------------------------------------

void ack_bits::Read( netstream& BS )
{
    s32 i;
    u32 Mask;

    if( BS.ReadFlag() )
    {
        // All bits are 1.  No problem.
        for( i = 0; i < ACKSLOTS; i++ )
            Bits[i] = 0xFFFF;
    }
    else
    {
        // There are some 0 bits.  Oh well.
        BS.ReadU32( Mask, ACKSLOTS );
        for( i = 0; i < ACKSLOTS; i++ )
            if( Mask & (1<<i) )
                BS.ReadU16( Bits[i] );
    }

    /*
    for( s32 i = 0; i < ACKSLOTS; i++ )
        BS.ReadU16( Bits[i] );
    */
}

//------------------------------------------------------------------------------

void ack_bits::Write( netstream& BS )
{
    s32 i;
    u32 Mask = 0;

    ASSERT( ACKSLOTS <= 32 );

    for( i = 0; i < ACKSLOTS; i++ )
        if( Bits[i] != 0xFFFF )
            Mask |= (1 << i);

    if( Mask )
    {
        // There were some 0 bits in there.  Crap.
        BS.WriteFlag( FALSE );
        BS.WriteU32( Mask, ACKSLOTS );
        for( i = 0; i < ACKSLOTS; i++ )
            if( Mask & (1<<i) )
                BS.WriteU16( Bits[i] );
    }
    else
    {
        // All bits were 1.  Hurray!
        BS.WriteFlag( TRUE );
    }

    /*
    for( s32 i = 0; i < ACKSLOTS; i++ )
        BS.WriteU16( Bits[i] );
    */
}

//==============================================================================

void PrintBits( u32 V, s32 NBits )
{
    for( s32 i=0; i<NBits; i++ )
    {
        x_DebugMsg("%1d",(V&(1<<(NBits-i-1)))?(1):(0) );
    }
    x_DebugMsg("\n");
}

//==============================================================================
//=== CONNECTION MANAGER CLASS BODY
//==============================================================================

conn_mgr::conn_mgr( void )
{
    m_ConnectionID = 0;
    m_HeartbeatsSent = 0;
    m_LastHeartbeatRequest = -1;
    m_LastHeartbeatResponse = 0;
    m_IsConnected = FALSE;
    m_IsInitialized = FALSE;
    ClearConnection();
}

//==============================================================================

conn_mgr::~conn_mgr( void )
{
}

//==============================================================================

void conn_mgr::Init( net_socket&        Local,
                     const net_address& Remote,
                     update_mgr*        pUpdateMgr,
                     pain_queue*        pPainQueue,
                     gm_mgr*            pGMMgr,
                     voice_proxy*       pVoiceProxy,
                     s32                ClientIndex,
                     xbool              IsServer )
{
    ASSERT(!m_IsConnected);
    ClearConnection();

    m_pLocalSocket          = &Local;
    m_RemoteAddress         = Remote;
    m_PacketsSent           = 0;
    m_PacketsDropped        = 0;
    m_PacketsReceived       = 0;
    m_PacketsUnusable       = 0;
    m_ClientIndex           = ClientIndex;
    m_PacketShipInterval    = DEFAULT_SHIP_INTERVAL;
    m_HeartbeatInterval     = HEARTBEAT_INTERVAL;
    m_PacketLossIndex       = 0;
    m_ConnectTime           = 0;
    m_IsServer              = IsServer;
    m_ClientState           = STATE_CLIENT_INIT;
    m_ServerState           = STATE_SERVER_IDLE;
    x_memset(m_PacketAck,0,sizeof(m_PacketAck));
    x_memset(m_PacketNak,0,sizeof(m_PacketNak));

    m_pUpdateMgr            = pUpdateMgr;
    m_pPainQueue            = pPainQueue;
    m_pGMMgr                = pGMMgr;
    m_pVoiceProxy           = pVoiceProxy;
    /*
    m_pUpdateMgr->Init(ClientIndex);
    */

    m_DoLogging             = FALSE;
    m_LogOutgoing           = FALSE;
    m_LogIncoming           = FALSE;
//  m_DestState             = STATE_NULL;

    m_IsConnected           = TRUE;
    m_IsInitialized         = TRUE;
    m_LoginSessionID        = -1;
    m_ConnectionID          = 0;
    m_LastHeartbeatResponse = 0;
    m_HeartbeatsSent        = 0;
    m_LastHeartbeatRequest = -1;
    m_LocalStateRevision    = 0;
    m_RemoteStateRevision   = 0;

    // Logging.
    {
        if( m_IsServer )
            LOG_MESSAGE( "conn_mgr::Init", "SERVER SIDE" );
        else
            LOG_MESSAGE( "conn_mgr::Init", "CLIENT SIDE" );

        LOG_MESSAGE( "conn_mgr::Init",
                     "Client:%d -- Address:%s",
                     m_ClientIndex, 
                     m_RemoteAddress.GetStrAddress() );
    }

}

//==============================================================================

void conn_mgr::ClearConnection( void )
{
    s32 i;
    m_LastSeqReceived       = -1;
    m_LastSeqSent           = -1;
    m_LastAckReceived       = -1;
    m_AckBits.Clear();

    m_AveragePing           = 16;
    m_LastPing              = 0.0f;

    ResetTimeouts();

    for( i=0; i<CONN_NUM_PACKETS; i++ )
        m_Packet[i].Seq = -1;

    for( i=0; i<CONN_NUM_HEARTBEATS; i++ )
        m_HeartbeatPing[i] = -1;

//  m_StatsIndex = 0;
    m_StatsClock = 0.0f;
    x_memset( &m_Stats, 0, sizeof(m_Stats) );
#if !defined(X_RETAIL) || defined(X_QA)
    m_StatsClockDebug = 0.0f;
    x_memset( &m_StatsDebug, 0, sizeof(m_StatsDebug) );
#endif    
}

//==============================================================================

void conn_mgr::Kill( void )
{
    if( !m_IsInitialized )
        return;

    LOG_MESSAGE( "conn_mgr::Kill", "Client:%d -- Connection shutdown", m_ClientIndex );

    m_IsConnected   = FALSE;
    m_IsInitialized = FALSE;
    ClearConnection();
    m_RemoteAddress.Clear();
    m_pUpdateMgr->Kill();
}

//==============================================================================

void conn_mgr::HandlePacketNotify( s32 PacketID, xbool Received )
{
    // Find the packet
    s32 P;
    for( P=0; P<CONN_NUM_PACKETS; P++ )
    {
        if( m_Packet[P].Seq == PacketID )
            break;
    }

    if( P==CONN_NUM_PACKETS )
    {
        //x_DebugMsg("**********************************\n");
        //x_DebugMsg("Notify for packet already cleared\n");
        //x_DebugMsg("**********************************\n");
        return;
    }

    //ASSERT( P < CONN_NUM_PACKETS );

#if defined(TRACK_ACKS)
    {
        if( Received )
            LOG_MESSAGE( "conn_mgr::HandlePacketNotify( ACK )",
                         "Client:%d -- Archive:%d -- PacketSeq:%d", 
                         m_ClientIndex, P, PacketID );
        else
            LOG_MESSAGE( "conn_mgr::HandlePacketNotify( NACK )",
                         "Client:%d -- Archive:%d -- PacketSeq:%d", 
                         m_ClientIndex, P, PacketID );
    }
#endif

    // Handle notify for this packet
    if( Received )
    {
        m_PacketAck[m_PacketLossIndex]++;
        //x_DebugMsg("Packet ack %1d %1d\n",P,m_Packet[P].Seq);
        if( SHOW_PACKET_INFO )
            x_DebugMsg("************** packet %1d acknowledged RECEIVED\n",PacketID);
    }
    else
    {
        m_PacketNak[m_PacketLossIndex]++;
        if( SHOW_PACKET_INFO )
            x_DebugMsg("************** packet %1d acknowledged DROPPED\n",PacketID);
        m_PacketsDropped++;
    }

    if( m_IsServer )
    {
        m_pUpdateMgr->PacketAck( m_Packet[P],     Received );
        m_pGMMgr    ->PacketAck( m_Packet[P],     Received );
        MsgMgr       .PacketAck( m_Packet[P].Seq, Received, m_ClientIndex );
    }
    else
    {
        m_pUpdateMgr->PacketAck( m_Packet[P], Received );
        m_pPainQueue->PacketAck( m_Packet[P], Received );
    }

    m_Packet[P].Seq = -1;
}

//==============================================================================

f32 conn_mgr::GetPacketLoss(void)
{
    s32 i;
    s32 naked;
    s32 acked;
    s32 total;
    f32 pc;

    naked = 0;
    acked = 0;
    total = 0;
    for (i=0;i<PACKET_LOSS_HISTORY;i++)
    {
        naked += m_PacketNak[i];
        acked += m_PacketAck[i];
    }

    if (naked+acked)
    {
        pc = (f32)naked / (f32)(naked+acked);
    }
    else
    {
        pc = 0.0f;
    }

    return pc;
}

//==============================================================================

s32 g_PacketSeq = 0;

void conn_mgr::OpenPacket( netstream& BitStream, conn_packet_type Type )
{
    OpenConnectionPacket( BitStream, Type, m_ConnectionID );
    if( IsMissionPacket(Type) )
        BitStream.WriteU32( (u32)GetSessionID() );
}

//==============================================================================

xbool conn_mgr::ReceivePacket( netstream& BitStream )
{
    if (!m_IsConnected)
        return FALSE;

#if defined(TRACK_ACKS)
    LOG_MESSAGE( "conn_mgr::ReceivePacket", 
                 "Client:%d -- Version:%d", 
                 m_ClientIndex, net_GetVersionKey() );
#endif

    if (m_PacketLossTimer.ReadSec() > 0.1f)
    {
        m_PacketLossIndex++;
        if (m_PacketLossIndex >= PACKET_LOSS_HISTORY)
        {
            m_PacketLossIndex=0;
        }
        m_PacketAck[m_PacketLossIndex]=0;
        m_PacketNak[m_PacketLossIndex]=0;
        m_PacketLossTimer.Stop();
    }

    if (!m_PacketLossTimer.IsRunning())
    {
        m_PacketLossTimer.Reset();
        m_PacketLossTimer.Start();
    }

    if( m_DoLogging )
    {
        if( m_LogIncoming == NULL )
        {
            xfs  XFS( "To(%s)From(%s).bin",
                      m_pLocalSocket->GetStrIP(),
                      m_RemoteAddress.GetStrIP() );
            m_LogIncoming = x_fopen( XFS, "wb" );
        }

        if( m_LogIncoming )
        {
            s32 NBytes = BitStream.GetNBytesUsed();
            x_fwrite( &NBytes, 4, 1, m_LogIncoming );
            x_fwrite( BitStream.GetDataPtr(), NBytes, 1, m_LogIncoming );
        }
    }

    BitStream.RequireFiniteFloats();

    // Check the Checksum
    if( !BitStream.Validate() )
    {
        LOG_ERROR( "conn_mgr::ReceivePacket", "Client:%d -- Checksum failed.", m_ClientIndex );
        return FALSE;
    }

    // Throw out packet if not ours
    u16 Header;
    BitStream.ReadU16(Header);
    if (Header != CONN_PACKET_IDENTIFIER)
    {
        LOG_ERROR( "conn_mgr::ReceivePacket", 
                   "Client:%d -- Incorrect header!", 
                   m_ClientIndex );
        return FALSE;
    }

    s16 PacketType;
    BitStream.ReadS16(PacketType);

    if( (PacketType == CONN_PACKET_LOGIN_REQUEST) ||
        (PacketType == CONN_PACKET_CANCEL_LOGIN) || (PacketType == CONN_PACKET_CANCEL_ACK) )
        return FALSE;
    u64 ConnectionID;
    if( !CanReadConnectionBits(BitStream, 64) )
        return TRUE;
    BitStream.ReadU64( ConnectionID );
    if( BitStream.Overwrite() || !m_ConnectionID || (ConnectionID != m_ConnectionID) )
        return TRUE;

    switch (PacketType)
    {

    //----------------------------------------------------------------
    // Check if it is a heartbeat request
    case CONN_PACKET_HB_REQUEST:
        ProcessHeartbeatRequest(BitStream);
        break;
    //----------------------------------------------------------------
    case CONN_PACKET_HB_RESPONSE:
        ProcessHeartbeatResponse( BitStream );
        break;

    //----------------------------------------------------------------
    case CONN_PACKET_GAME_DATA:
        if( (m_ServerState==STATE_SERVER_INGAME) || (m_ServerState==STATE_SERVER_SYNC) )
        {
            ProcessGameData( BitStream );
        }
        break;

    //----------------------------------------------------------------
    // Any other packet type, we drop it.
    default:
        return FALSE;
        break;
    }

    m_PacketsReceived++;
    return TRUE;
}

//==============================================================================

void conn_mgr::Disconnect(exit_reason Reason)
{
    netstream Request;

    if (m_IsConnected)
    {
        // Send the client a disconnect request
        OpenPacket( Request, CONN_PACKET_DISCONNECT );
        Request.WriteS32( Reason );
        Request.Close();
        Send( Request );

        LOG_WARNING( "conn_mgr::Disconnect", "Client:%d -- Reason:%s", 
                     m_ClientIndex, GetExitReasonName(Reason) );
        m_IsConnected = FALSE;
    }
    else
    {
        LOG_ERROR( "conn_mgr::Disconnect(!)", "Client:%d -- Reason:%s -- NOT CONNECTED", 
                   m_ClientIndex, GetExitReasonName(Reason) );
    }
}

//==============================================================================

xbool conn_mgr::SendPacket( void )
{
    netstream BitStream;

    ASSERT( m_pUpdateMgr );

    if( m_IsConnected == FALSE )
        return FALSE;

    // Allocate a packet
    s32 P;
    for( P=0; P<CONN_NUM_PACKETS; P++ )
    {
        if( m_Packet[P].Seq == -1 )
            break;
    }

    if( P==CONN_NUM_PACKETS )
    {
        // Flush some of the packets
        for( s32 i=0; i<CONN_NUM_PACKETS; i++ )
        {
            HandlePacketNotify( m_LastAckReceived, FALSE );
            m_LastAckReceived++;
        }

        m_IsConnected = FALSE;
        LOG_ERROR( "conn_mgr::SendPacket(fail)",
                   "Client:%d -- Too many unacknowledged packets -- DISCONNECTED", 
                   m_ClientIndex );
        return FALSE;
    }

    m_LastSeqSent++;
    //x_DebugMsg("NSent %1d\n",m_LastSeqSent );

    m_Packet[P].Seq = m_LastSeqSent;

    // Clear packet values
    m_Packet[P].NUpdates     =  0;
    m_Packet[P].FirstUpdate  = -1;
    m_Packet[P].TargetClient = m_ClientIndex;

#if defined(TRACK_ACKS)
    LOG_MESSAGE( "conn_mgr::SendPacket", "Client:%d, PacketSeq:%d, LastReceived:%d", m_ClientIndex, m_LastSeqSent, m_LastSeqReceived );
#endif

    // Pack in known bytes
    OpenPacket( BitStream, CONN_PACKET_GAME_DATA );

    // Pack in session number
    BitStream.WriteU32( (u32)GetSessionID() );

    // Pack in new sequence number
    BitStream.WriteS32( m_LastSeqSent, 24 );

    // Pack in new acknowledge number
    BitStream.WriteS32( m_LastSeqReceived, 24 );

    // Pack in ack bits
    m_AckBits.Write( BitStream );

    ASSERT( m_pUpdateMgr );

    // Tell other managers to pack in their data
    WriteManagerData( m_Packet[P], BitStream );

    //x_DebugMsg( "(L:%d,U:%d)\n", m_Packet[P].NLife, m_Packet[P].NUpdates );

    ASSERT( m_pUpdateMgr );

    if( BitStream.Overwrite() )
    {
        HandlePacketNotify( m_Packet[P].Seq, FALSE );
        return FALSE;
    }

    // Done with the bitstream, so close it
    BitStream.Close();

    if( BitStream.Overwrite() )
    {
        HandlePacketNotify( m_Packet[P].Seq, FALSE );
        return FALSE;
    }

    // Send the packet
    BitStream.Send( *m_pLocalSocket, m_RemoteAddress );

    if( SHOW_PACKET_INFO )
    {
        x_DebugMsg("SEND PACKET(%1d) %1d\n",m_Packet[P].Seq,BitStream.GetNBytesUsed());
    }

    if( m_DoLogging )
    {
        if( m_LogOutgoing == NULL )
        {
            xfs  XFS( "To(%s)From(%s).bin",
                      m_RemoteAddress.GetStrIP(),
                      m_pLocalSocket->GetStrIP() );
            m_LogOutgoing = x_fopen( XFS, "wb" );
        }

        if( m_LogOutgoing )
        {
            s32 NBytes = BitStream.GetNBytesUsed();
            x_fwrite( &NBytes, 4, 1, m_LogOutgoing );
            x_fwrite( BitStream.GetDataPtr(), NBytes, 1, m_LogOutgoing );
        }
    }

    // Verbosity.
    #ifdef VERBOSE_NETWORK
    {
        x_DebugMsg( "==>> SEND %d <", m_LastSeqSent );
        m_AckBits.Print();
        x_DebugMsg( "> HighAck %d\n", m_LastSeqReceived );
    }
    #endif

    //x_printf("(%2d,%4d) ",P,BitStream.GetNBytesUsed());
    m_PacketsSent++;
    return TRUE;
}

//==============================================================================

void conn_mgr::ProcessHeartbeatResponse( netstream& BitStream )
{
    // Look up which heartbeat it was
    s32     Index;
    s32     State;

    if( !CanReadConnectionBits(BitStream, 96 + (m_IsServer ? 0 : 1)) )
        return;
    BitStream.ReadS32(Index);
    BitStream.ReadS32(State);
    u32 StateRevision;
    BitStream.ReadU32( StateRevision );
    if( BitStream.Overwrite() || (Index <= 0) || (StateRevision < m_RemoteStateRevision) ||
        (State < 0) || (State > (m_IsServer ? STATE_CLIENT_KILL : STATE_SERVER_KILL)) )
        return;

    #ifdef VERBOSE_NETWORK
    x_DebugMsg( "<--- RECV HBRSP %d\n", Index );
    #endif

    // If index is out of range it is probably from an old mission
    if( (Index > m_HeartbeatsSent) || (Index <= m_LastHeartbeatResponse) )
    {
        LOG_WARNING("conn_mgr::ProcessHeartbeatResponse","Ignored, out of range. Index:%d, Expected highest:%d",Index, m_HeartbeatsSent );
        return;
    }

    // Decide if it's too far in the past
    ASSERT(Index <= m_HeartbeatsSent);
    if( (m_HeartbeatsSent - Index) > CONN_NUM_HEARTBEATS )
    {
        LOG_WARNING("conn_mgr::ProcessHeartbeatResponse","Ignored, Too old. Index:%d, Highest Sent:%d",Index, m_HeartbeatsSent );
        return;
    }

    xbool FinalScore = FALSE;
    game_score Score;

    if( !m_IsServer )
    {
        FinalScore = BitStream.ReadFlag();
        if( FinalScore && ((State != STATE_SERVER_COOLDOWN) || !GameMgr.ReadFinalScore(BitStream, Score)) )
            return;
    }

    if( !FinishConnectionPacket(BitStream) )
        return;

    m_LastHeartbeatResponse = Index;
    m_RemoteStateRevision = StateRevision;

    // Read current state
    if( m_IsServer )
    {
        m_ClientState = (client_state)State;
    }
    else
    {
        m_ServerState = (server_state)State;
        if( FinalScore )
        {

            if( GameMgr.GameInProgress() )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_ADVANCE_LEVEL );
                GameMgr.EndGame();
            }
            GameMgr.AcceptFinalScore( Score );
            LOG_WARNING( "conn_mgr::ProcessHeartbeatResponse(score)",
                         "Client:%d -- Received final score -- ClientState:%s -- ServerState:%s",
                         m_ClientIndex, GetStateName(m_ClientState), GetStateName(m_ServerState) );
        }
    }
    // Confirm that the connection is still alive
    ResetTimeouts();

    // Look up time heartbeat traveled
    //s32 Time = (s32)(tgl.LogicTimeMs - m_HeartbeatSentTime[Index%CONN_NUM_HEARTBEATS]);
    f32 Time = x_TicksToMs( x_GetTime() - (xtick)m_HeartbeatSentTime[Index%CONN_NUM_HEARTBEATS] );

#if defined(TRACK_HEARTBEATS)
    LOG_MESSAGE( "conn_mgr::ProcessHeartbeatResponse",
                 "Client:%d -- Received:%d -- Ping:%d",
                 m_ClientIndex, Index, (s32)Time );
#endif

    // Store ping time
    m_HeartbeatPing[Index%CONN_NUM_HEARTBEATS] = Time;
    m_LastPing = Time;

    // Update ping
    s32 N = MIN(CONN_NUM_HEARTBEATS,m_HeartbeatsSent);
    m_AveragePing = 0;
    for( s32 i=0; i<N; i++ )
    {
        m_AveragePing += m_HeartbeatPing[ (Index + (CONN_NUM_HEARTBEATS-i))%CONN_NUM_HEARTBEATS ];
    }
    m_AveragePing *= (1.0f/N);

    #ifdef VERBOSE_NETWORK
    x_DebugMsg( "  Ping %g\n", m_AveragePing );
    #endif

    // Display ping buffer
    if( 0 )
    {
        x_printf("PINGS ");
        for( s32 j=0; j<CONN_NUM_HEARTBEATS; j++ )
        {
            x_printf("%5.1f",m_HeartbeatPing[j]);
        }
        x_printf("\n");
    }

    //if( m_AveragePing > 10000 )
    //    m_AveragePing = (f32)Time;
    //else
    //    m_AveragePing = (m_AveragePing*0.90f + (f32)Time*0.10f);

    //m_AveragePing = (f32)Time;
}

//==============================================================================

void conn_mgr::ProcessHeartbeatRequest(netstream& BitStream)
{
    s32 Index;
    s32 State;

    if( !CanReadConnectionBits(BitStream, 96 + (m_IsServer ? 0 : 1)) )
        return;
    BitStream.ReadS32( Index );
    BitStream.ReadS32( State );
    u32 StateRevision;
    BitStream.ReadU32( StateRevision );
    if( BitStream.Overwrite() || (Index <= 0) || (StateRevision < m_RemoteStateRevision) ||
        (State < 0) || (State > (m_IsServer ? STATE_CLIENT_KILL : STATE_SERVER_KILL)) )
        return;

#if defined(TRACK_HEARTBEATS)
    LOG_MESSAGE( "conn_mgr::ProcessHeartbeatRequest",
                 "Client:%d -- Received:%d",
                 m_ClientIndex, Index );
#endif

    if( Index <= m_LastHeartbeatRequest )
        return;
    xbool FinalScore = FALSE;
    game_score Score;

    if( !m_IsServer )
    {
        FinalScore = BitStream.ReadFlag();
        if( FinalScore && ((State != STATE_SERVER_COOLDOWN) || !GameMgr.ReadFinalScore(BitStream, Score)) )
            return;
    }

    if( !FinishConnectionPacket(BitStream) )
        return;

    m_LastHeartbeatRequest = Index;
    m_RemoteStateRevision = StateRevision;

    if( m_IsServer )
    {
        m_ClientState = (client_state)State;
    }
    else
    {
        m_ServerState = (server_state)State;

        if( FinalScore )
        {   
            if( GameMgr.GameInProgress() )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_ADVANCE_LEVEL );
                GameMgr.EndGame();
            }
            GameMgr.AcceptFinalScore( Score );
            LOG_WARNING( "conn_mgr::ProcessHeartbeatRequest(score)",
                         "Client:%d -- Received final score -- ClientState:%s -- ServerState:%s",
                         m_ClientIndex, GetStateName(m_ClientState), GetStateName(m_ServerState) );
        }
    }

    ResetTimeouts();

    netstream Response;

    OpenPacket( Response, CONN_PACKET_HB_RESPONSE );
    Response.WriteS32( Index );
    if( IsServer() )
    {
        Response.WriteS32( m_ServerState );
        Response.WriteU32( m_LocalStateRevision );

        if( Response.WriteFlag( (m_ServerState == STATE_SERVER_COOLDOWN) && 
                                (m_ClientState != STATE_CLIENT_COOLDOWN) ) )
        {
            LOG_WARNING( "conn_mgr::ProcessHeartbeatRequest(score)",
                         "Client:%d -- Sent final score -- ClientState:%s -- ServerState:%s",
                         m_ClientIndex, GetStateName(m_ClientState), GetStateName(m_ServerState) );

            GameMgr.ProvideFinalScore( Response, m_ClientIndex );
        }
    }
    else
    {
        Response.WriteS32( m_ClientState );
        Response.WriteU32( m_LocalStateRevision );
    }

    // Send the packet
    Response.Close();

    Response.Send( *m_pLocalSocket, m_RemoteAddress );

    #ifdef VERBOSE_NETWORK
    x_DebugMsg( "---> SEND HBRSP %d  State %d(%s)\n",
                Index, g_SM.CurrentState, s_StateName[ g_SM.CurrentState ] );
    #endif

}

//==============================================================================

void conn_mgr::ProcessGameData(netstream& BitStream)
{

    //
    // This is a known packet type
    //
    m_PacketsReceived++;
    //x_DebugMsg("NReceived %1d\n",m_NPacketsReceived );
    // Read packet header
    s32         pkSeqNum;
    s32         pkHighestAck;
    ack_bits    pkAckBits;
    u32         SessionID;

    BitStream.ReadU32( SessionID );
    BitStream.ReadS32( pkSeqNum, 24 );
    BitStream.ReadS32( pkHighestAck, 24 );
    pkAckBits.Read( BitStream );
    if( BitStream.ReadError() )
        return;

    if( (pkHighestAck < -1) || (pkHighestAck > m_LastSeqSent) ||
        ((s64)pkHighestAck - m_LastAckReceived > ACKBITS) )
    {
        BitStream.SetReadError();
        return;
    }

    g_PacketSeq = pkSeqNum;

    // Verbosity.
#if defined(TRACK_ACKS)
    LOG_MESSAGE( "conn_mgr::ProcessGameData", "Client:%d -- PacketSeq:%d, HighestAck:%d", m_ClientIndex, pkSeqNum, pkHighestAck );
#endif

#ifdef VERBOSE_NETWORK
    {
        x_DebugMsg( "<<== RECV %d <", pkSeqNum );
        pkAckBits.Print();
        x_DebugMsg( "> HighAck %d\n", pkHighestAck );
    }
#endif

    //x_DebugMsg("RECV---- SES(%1d) SEQ(%1d)(%1d,%1d) TYPE(%08X) PING(%1.2f)\n",SessionID,pkSeqNum,m_LastSeqReceived,m_LastSeqReceived+128,KnownBytes,GetPing());

    if( SHOW_PACKET_INFO )
    {
        x_DebugMsg("RECV PACKET(%1d) %1d\n",pkSeqNum,BitStream.GetNBytesUsed());
    }

    //x_DebugMsg("HANDLE PACKET: %5d %5d ",pkSeqNum,pkHighestAck,pkAckBits);
    //PrintBits(pkAckBits,16);

    // Check if session number is wrong
    if( SessionID != (u32)GetSessionID() )
    {
        // bad packet
        m_PacketsUnusable++;
        LOG_ERROR( "conn_mgr::ProcessGameData(error)",
                   "Client:%d -- PacketSeq:%d -- Session:%d -- Expected:%d", 
                   m_ClientIndex, pkSeqNum, SessionID, GetSessionID() );
        return;
    }

    // Check if sequence number is outside the known window
    if( ((s32)pkSeqNum <= m_LastSeqReceived) ||
        ((s32)pkSeqNum  > m_LastSeqReceived+128) )
    {
        // bad packet
        m_PacketsUnusable++;
        LOG_ERROR( "conn_mgr::ProcessGameData(error)",
                   "Client:%d -- SeqNum:%d -- ValidRange:(%d...%d)",
                    m_ClientIndex, pkSeqNum, m_LastSeqReceived+1, m_LastSeqReceived+128 );
        x_DebugMsg("packet dropped for bad seq number %1d (%1d,%1d)\n",pkSeqNum,m_LastSeqReceived+1,m_LastSeqReceived+128);
        return;
    }

    //
    // Store that we have received a new packet
    //
    {
        // Shift up ack bits so all packets between new and last have zeros
        {
            s32 ShiftAmt = pkSeqNum - m_LastSeqReceived;
            if( ShiftAmt >= 128 )
                LOG_ERROR( "conn_mgr::ProcessGameData(holy crap!)",
                           "ShiftAmt: %d - BOGUS!!!!!",
                            ShiftAmt );
        }
        m_AckBits.ShiftLeft( pkSeqNum - m_LastSeqReceived );

        // Mark this packet as being received
        m_AckBits.Set( 0 );

        // Remember we have received up to this point
        m_LastSeqReceived = pkSeqNum;
    }

    // Confirm that the connection is still alive
    ResetTimeouts();

    //
    // React to packet's ack bits.
    //
    if( pkHighestAck > m_LastAckReceived )
    {
        for( s32 i = m_LastAckReceived + 1; i <= pkHighestAck; i++ )
        {
            s32 Shift = pkHighestAck - i;
            HandlePacketNotify( i, pkAckBits.Bit(Shift) );
        }

        m_LastAckReceived = pkHighestAck;
    }

    // Tell other managers to dissect packet
    ProcessManagerData( BitStream );
}

//==============================================================================

void conn_mgr::ResetTimeouts( void )
{
    m_ConfirmAliveTimeout = CONFIRM_ALIVE_TIMEOUT;
}

//==============================================================================

void conn_mgr::SendHeartbeat( void )
{
    m_HeartbeatsSent++;

    // If the connection has not received any data for a second, then we start to increment
    // the calculated ping time. This will throttle the connection to the client should no
    // data be getting received. The ping time will be set to the actual average ping when
    // the next heartbeat does get received.
    if (m_ConfirmAliveTimeout < (CONFIRM_ALIVE_TIMEOUT - 1.0f))
    {
        LOG_WARNING( "conn_mgr::SendHeartbeat",
                     "Client:%d -- No packets received, bumping up ping time to %2.02fms",
                     m_ClientIndex, m_AveragePing );
        m_AveragePing += 50.0f;
    }

    m_HeartbeatSentTime[ m_HeartbeatsSent%CONN_NUM_HEARTBEATS ] = (s64)x_GetTime();//tgl.LogicTimeMs;

#if defined(TRACK_HEARTBEATS)
    LOG_MESSAGE( "conn_mgr::SendHeartbeat", "Client:%d -- Sending request %d", m_ClientIndex, m_HeartbeatsSent );
#endif

    netstream BitStream;
    OpenPacket( BitStream, CONN_PACKET_HB_REQUEST );

    BitStream.WriteS32( m_HeartbeatsSent );
    if (IsServer())
    {
        BitStream.WriteS32( m_ServerState );
        BitStream.WriteU32( m_LocalStateRevision );
        if( BitStream.WriteFlag( (m_ServerState == STATE_SERVER_COOLDOWN) && 
                                 (m_ClientState != STATE_CLIENT_COOLDOWN) ) )
        {
            LOG_WARNING( "conn_mgr::SendHeartbeat(score)",
                         "Client:%d -- Sent final score -- ClientState:%s -- ServerState:%s",
                         m_ClientIndex, GetStateName(m_ClientState), GetStateName(m_ServerState) );

            GameMgr.ProvideFinalScore( BitStream, m_ClientIndex );
        }
    }
    else
    {
        BitStream.WriteS32( m_ClientState );
        BitStream.WriteU32( m_LocalStateRevision );
    }

    // Done with the bistream, so close it
    BitStream.Close();

    // Send the packet
    BitStream.Send(*m_pLocalSocket,m_RemoteAddress);

    {
        //char HBDst[32];
        //m_DstAddress.GetStrIP(HBDst);
        //x_DebugMsg("Heartbeat sent to %s\n",HBDst);
    }
}

//==============================================================================

f32 conn_mgr::GetPing( void )
{
    return m_AveragePing;
}

//==============================================================================

client_state conn_mgr::GetClientState( void )
{
    return m_ClientState;
}

//==============================================================================

server_state conn_mgr::GetServerState( void )
{
    return m_ServerState;
}

//==============================================================================

void conn_mgr::SetClientState( client_state State)
{
    ASSERT(!m_IsServer);
    // If there is a state transition, we zero the heartbeat timer so we
    // can get the state change to the other party as soon as possible.
    // Note: Only the client can change it's state. The server can request
    // state changes through game manager control packets.
    if (m_ClientState != State)
    {
        m_HeartbeatInterval = 0.0f;
        m_LocalStateRevision++;
    }

    m_ClientState = State;
}

//==============================================================================

void conn_mgr::SetServerState( server_state State)
{
    ASSERT(m_IsServer);
    if (m_ServerState != State)
    {
        m_HeartbeatInterval = 0.0f;
        m_LocalStateRevision++;
    }
    m_ServerState = State;
}

//==============================================================================

void conn_mgr::WriteManagerData( conn_packet& Packet, netstream& BitStream )
{
    s32 Cursor;
    s32 Limit = BitStream.GetWriteLimit();
    s32 TrailerBits = IsServer() ? 1 : 32;

//  s32 NGameManagerBits = 0;
//  s32 NUpdateBits      = 0;
//  s32 NVoiceBits       = 0;

    Packet.NUpdates     = 0;
    Packet.NPains       = 0;
    Packet.GMDirtyBits  = 0x00;
    Packet.GMScoreBits  = 0x00;
    Packet.GMPlayerBits = 0x00;

    if( !m_IsConnected )
        return;

    Cursor = BitStream.GetCursor();

#if defined(TARGET_WINDOWS)
    // Desktop voice travels in the reliable connection packet.  The server
    // owns one proxy per client; a client sends its local headset data.
    if( m_pVoiceProxy != NULL )
    {
        m_pVoiceProxy->ProvideUpdate( BitStream );
    }
    else
    {
        g_VoiceMgr.ProvideUpdate( BitStream );
    }
#endif

    m_Stats.BitsVoiceMgr += BitStream.GetCursor() - Cursor;

#if !defined(X_RETAIL) || defined(X_QA)
    if (m_Stats.MaxBitsVoiceMgr < (BitStream.GetCursor() - Cursor) )
        m_Stats.MaxBitsVoiceMgr = BitStream.GetCursor() - Cursor;
#endif

    Cursor = BitStream.GetCursor();

    // Deal with game manager.
    if( IsServer() )
    {
        BitStream.SetWriteLimit( Limit - 8 );
        m_pGMMgr->ProvideUpdate( Packet, BitStream );
        BitStream.SetWriteLimit( Limit );
        m_Stats.BitsGameMgr += BitStream.GetCursor() - Cursor;

#if !defined(X_RETAIL) || defined(X_QA)
        if (m_Stats.MaxBitsGameMgr < BitStream.GetCursor() - Cursor) 
            m_Stats.MaxBitsGameMgr = BitStream.GetCursor() - Cursor;
#endif
        Cursor = BitStream.GetCursor();
    }

    // Deal with updates.
    {
        ASSERT( m_pUpdateMgr );

        BitStream.SetWriteLimit( Limit - TrailerBits );
        m_pUpdateMgr->ProvideUpdates( Packet, BitStream, 100 );
        BitStream.SetWriteLimit( Limit );
        m_Stats.Updates       += Packet.NUpdates;
        m_Stats.BitsUpdateMgr += BitStream.GetCursor() - Cursor;
#if !defined(X_RETAIL) || defined(X_QA)
        m_Stats.BitsPlayerUpdates += Packet.BitsPlayerUpdates;
        m_Stats.BitsGhostUpdates += Packet.BitsGhostUpdates;
        m_Stats.BitsOtherUpdates += Packet.BitsOtherUpdates;

        //update stat max
        if (m_Stats.MaxBitsPlayerUpdates < Packet.BitsPlayerUpdates) 
            m_Stats.MaxBitsPlayerUpdates = Packet.BitsPlayerUpdates;
        if (m_Stats.MaxBitsGhostUpdates < Packet.BitsGhostUpdates)
            m_Stats.MaxBitsGhostUpdates = Packet.BitsGhostUpdates;
        if (m_Stats.MaxBitsOtherUpdates < Packet.BitsOtherUpdates) 
            m_Stats.MaxBitsOtherUpdates = Packet.BitsOtherUpdates;

        if (m_Stats.MaxBitsUpdateMgr < BitStream.GetCursor() - Cursor) 
            m_Stats.MaxBitsUpdateMgr = BitStream.GetCursor() - Cursor;
        if (m_Stats.MaxUpdates < Packet.NUpdates) 
            m_Stats.MaxUpdates = Packet.NUpdates;
#endif
        Cursor = BitStream.GetCursor();
    }

    // Pain queue.
    if( !IsServer() )
    {
        m_pPainQueue->ProvidePain( Packet, BitStream, CONN_PAINS_PER_PACKET );
        m_Stats.Pains         += Packet.NPains;
        m_Stats.BitsPainQueue += BitStream.GetCursor() - Cursor;

#if !defined(X_RETAIL) || defined(X_QA)
        if (m_Stats.MaxBitsPainQueue < BitStream.GetCursor() - Cursor) 
            m_Stats.MaxBitsPainQueue = BitStream.GetCursor() - Cursor;
        if (m_Stats.MaxPains < Packet.NPains) 
            m_Stats.MaxPains = Packet.NPains;
#endif

        Cursor = BitStream.GetCursor();
    }

    // Messages.
    if( IsServer() )
    {
        MsgMgr.ProvideMsg( Packet.Seq, BitStream, m_ClientIndex );
    }

    m_Stats.Packets   += 1;
    m_Stats.BitsTotal += BitStream.GetCursor();

#if !defined(X_RETAIL) || defined(X_QA)
    if (m_Stats.MaxBitsTotal < BitStream.GetCursor()) 
        m_Stats.MaxBitsTotal = BitStream.GetCursor();
#endif

//  m_Stats[m_StatsIndex].UpdatesSent += Packet.NUpdates;
//  m_Stats[m_StatsIndex].BytesSent   += BitStream.GetNBytesUsed();
//  m_Stats[m_StatsIndex].PacketsSent += 1;

    //LOG_MESSAGE("conn_mgr::WriteManagerData","V:%d, GM:%d, MM:%d Update:%d, Total:%d",NVoiceBits,NGameManagerBits,NMoveBits,NUpdateBits,BitStream.GetCursor());
}

//==============================================================================

void conn_mgr::ProcessManagerData( netstream& BitStream )
{
    if( !m_IsConnected )
        return;

#if defined(TARGET_WINDOWS)
    // This mirrors the desktop voice payload written above.  Server-side
    // connections terminate in a voice proxy; the local client terminates in
    // the global voice manager.
    if( IsServer() )
    {
        ASSERT( m_pVoiceProxy != NULL );
        m_pVoiceProxy->AcceptUpdate( BitStream );
    }
    else
    {
        ASSERT( m_pVoiceProxy == NULL );
        g_VoiceMgr.AcceptUpdate( BitStream );
    }
#endif

    if( BitStream.ReadError() )
        return;

    if( !IsServer() )
    {
        m_pGMMgr->AcceptUpdate( BitStream );
        if( BitStream.ReadError() )
            return;
    }

    m_pUpdateMgr->AcceptUpdates( BitStream );
    if( BitStream.ReadError() )
        return;

    if( IsServer() )
    {
        m_pPainQueue->AcceptPain( BitStream );
        if( BitStream.ReadError() )
            return;
    }

    // Messages.
    if( !IsServer() )
    {
        MsgClient.AcceptMsgs( BitStream );
    }

    FinishConnectionPacket( BitStream );
}

//==============================================================================
/*
void conn_mgr::AdvanceStats( void )
{
    // Move to next stats
    m_StatsIndex = (m_StatsIndex+1)%CONN_NUM_STATS;

    // Clear stats
    m_Stats[m_StatsIndex].LifeSent    = 0;
    m_Stats[m_StatsIndex].UpdatesSent = 0;
    m_Stats[m_StatsIndex].MovesSent   = 0;
    m_Stats[m_StatsIndex].BytesSent   = 0;
    m_Stats[m_StatsIndex].PacketsSent = 0;
}
*/
//==============================================================================
/*
void conn_mgr::DumpStats( X_FILE* fp )
{
    conn_stats S;
    x_memset(&S,0,sizeof(conn_stats));

    // Sum up all collected stats
    for( s32 i=0; i<CONN_NUM_STATS; i++ )
    {
        S.BytesSent += m_Stats[i].BytesSent;
        S.LifeSent  += m_Stats[i].LifeSent;
        S.PacketsSent += m_Stats[i].PacketsSent;
        S.UpdatesSent += m_Stats[i].UpdatesSent;
    }

    if( fp == NULL )
    {
        x_DebugMsg("Connmanager stats:\n");
        x_DebugMsg("Packets   %5d\n",S.PacketsSent);
        x_DebugMsg("BytesSent %5d  BSPP %5d\n",S.BytesSent,(S.PacketsSent)?(S.BytesSent/S.PacketsSent):(0));
        x_DebugMsg("LifeSent  %5d\n",S.LifeSent);
        x_DebugMsg("Updates   %5d\n",S.UpdatesSent);
    }
    else
    {
        x_fprintf(fp,"Connmanager stats:\n");
        x_fprintf(fp,"Packets   %5d\n",S.PacketsSent);
        x_fprintf(fp,"BytesSent %5d  BSPP %5d\n",S.BytesSent,(S.PacketsSent)?(S.BytesSent/S.PacketsSent):(0));
        x_fprintf(fp,"LifeSent  %5d\n",S.LifeSent);
        x_fprintf(fp,"Updates   %5d\n",S.UpdatesSent);
    }
}
*/
//==============================================================================

void conn_mgr::Send(netstream& BitStream)
{
    m_pLocalSocket->Send(m_RemoteAddress,BitStream);
}

//==============================================================================

void conn_mgr::SetSessionID( s32 ID )
{
    s32 LastSession;

    LastSession = GetSessionID();
    ClearConnection();
    m_LoginSessionID = ID;
    LOG_WARNING( "conn_mgr::SetSessionID", "Client:%d, ********* SESSION ID: %d", m_ClientIndex, GetSessionID() );
    if( (GetSessionID() != (LastSession+1)) && (LastSession != -1) )
    {
        LOG_WARNING( "conn_mgr::SetSessionID", "Session ID changed but not as expected. Old:%d, New:%d", LastSession, GetSessionID() );
    }

}

//==============================================================================

s32 conn_mgr::GetSessionID( void )
{
    return m_LoginSessionID;
}

//==============================================================================

f32 conn_mgr::GetShipInterval(void)
{
    return m_PacketShipInterval;
}

//==============================================================================

void conn_mgr::UpdateShipInterval( f32 DeltaTime )
{
    f32 interval;

    
    interval = (m_LastPing / 2.0f) / 1000.0f;

    if (interval < MIN_SHIP_INTERVAL)
        interval = MIN_SHIP_INTERVAL;
    if (interval > MAX_SHIP_INTERVAL)
        interval = MAX_SHIP_INTERVAL;

    f32 const Adjustment = SHIP_INTERVAL_ADJUSTMENT_RATE * DeltaTime;
    if( interval < m_PacketShipInterval )
    {
        m_PacketShipInterval = MAX( interval, m_PacketShipInterval - Adjustment );
    }
    else if( interval > m_PacketShipInterval )
    {
        m_PacketShipInterval = MIN( interval, m_PacketShipInterval + Adjustment );
    }

    if (m_LastPing > 600.0f)
    {
        m_PacketShipInterval = MIN_SHIP_INTERVAL;
    }
}

//==============================================================================

void conn_mgr::Update(f32 DeltaTime)
{
    if (!m_IsConnected || m_pLocalSocket->IsEmpty())
    {
        m_IsConnected = FALSE;
        return;
    }

    // If we got a very high delta time, then this would be due to either
    // debugging or level loading, so we just reset all the timeouts just
    // to be safe. DeltaTimes should never be greater than 100ms.
    // But that would all depend on game frame rate.

    if (DeltaTime > 0.5f)
    {
        ResetTimeouts();
        DeltaTime = 0.0f;
    }

    m_HeartbeatInterval -= DeltaTime;
    if( (m_HeartbeatInterval <= 0.0f) && (DeltaTime > 0.0f) )
    {
        SendHeartbeat();
        m_HeartbeatInterval += HEARTBEAT_INTERVAL;
    }

    m_ConfirmAliveTimeout -= DeltaTime;
    if ( m_ConfirmAliveTimeout < 0.0f)
    {
        m_IsConnected = FALSE;
        LOG_WARNING( "conn_mgr::Update",
                     "Client:%d -- ConfirmAlive timeout -- DISCONNECTED",
                     m_ClientIndex );
        return;
    }

    UpdateShipInterval( DeltaTime );

    // Stat processing.
    m_StatsClock += DeltaTime;
    if( m_StatsClock > 3.0f )
    {
        if( m_IsServer )
        {
            LOG_MESSAGE( "conn_mgr::StatsPerSec", 
                "Client:%d - Packets:%d - Bytes:%d - Updates(%d):%d - Voice:%d - Game:%d", 
                m_ClientIndex, 
                (s32)( m_Stats.Packets           / m_StatsClock),
                (s32)((m_Stats.BitsTotal>>3)     / m_StatsClock),
                (s32)( m_Stats.Updates           / m_StatsClock),
                (s32)((m_Stats.BitsUpdateMgr>>3) / m_StatsClock),
                (s32)((m_Stats.BitsVoiceMgr>>3)  / m_StatsClock),
                (s32)((m_Stats.BitsGameMgr >>3)  / m_StatsClock) );
        }
        else
        {
            LOG_MESSAGE( "conn_mgr::StatsPerSec", 
                "Client:%d - Packets:%d - Bytes:%d - Updates(%d):%d - Pains(%d):%d - Voice:%d", 
                m_ClientIndex, 
                (s32)( m_Stats.Packets           / m_StatsClock),
                (s32)((m_Stats.BitsTotal>>3)     / m_StatsClock),
                (s32)( m_Stats.Updates           / m_StatsClock),
                (s32)((m_Stats.BitsUpdateMgr>>3) / m_StatsClock),
                (s32)( m_Stats.Pains             / m_StatsClock),
                (s32)((m_Stats.BitsPainQueue>>3) / m_StatsClock),
                (s32)((m_Stats.BitsVoiceMgr>>3)  / m_StatsClock) );
        }

#if !defined(X_RETAIL) || defined(X_QA)
        //backup stats for onscreen reporting
        m_StatsDebug.Packets                = m_Stats.Packets;
        m_StatsDebug.Updates                = m_Stats.Updates;
        m_StatsDebug.Pains                  = m_Stats.Pains;
        m_StatsDebug.BitsUpdateMgr          = m_Stats.BitsUpdateMgr;
        m_StatsDebug.BitsPainQueue          = m_Stats.BitsPainQueue;
        m_StatsDebug.BitsVoiceMgr           = m_Stats.BitsVoiceMgr;
        m_StatsDebug.BitsGameMgr            = m_Stats.BitsGameMgr;
        m_StatsDebug.BitsTotal              = m_Stats.BitsTotal;
        m_StatsDebug.BitsPlayerUpdates      = m_Stats.BitsPlayerUpdates;
        m_StatsDebug.BitsGhostUpdates       = m_Stats.BitsGhostUpdates;
        m_StatsDebug.BitsOtherUpdates       = m_Stats.BitsOtherUpdates;
        m_StatsDebug.MaxBitsPlayerUpdates   = m_Stats.MaxBitsPlayerUpdates;
        m_StatsDebug.MaxBitsGhostUpdates    = m_Stats.MaxBitsGhostUpdates;
        m_StatsDebug.MaxBitsOtherUpdates    = m_Stats.MaxBitsOtherUpdates;
        m_StatsDebug.MaxBitsUpdateMgr       = m_Stats.MaxBitsUpdateMgr;
        m_StatsDebug.MaxBitsPainQueue       = m_Stats.MaxBitsPainQueue;
        m_StatsDebug.MaxBitsVoiceMgr        = m_Stats.MaxBitsVoiceMgr;
        m_StatsDebug.MaxBitsGameMgr         = m_Stats.MaxBitsGameMgr;
        m_StatsDebug.MaxBitsTotal           = m_Stats.MaxBitsTotal;
        m_StatsDebug.MaxUpdates             = m_Stats.MaxUpdates;
        m_StatsDebug.MaxPains               = m_Stats.MaxPains;
        m_StatsClockDebug                   = m_StatsClock;
#endif    

        m_StatsClock = 0.0f;
        x_memset( &m_Stats, 0, sizeof(m_Stats) );
    }
}

//------------------------------------------------------------------------------

void conn_mgr::QueueVoice(const byte* pData, s32 Length)
{
    if( (pData == NULL) || (Length <= 0) )
    {
        return;
    }

    if( m_pVoiceProxy != NULL )
    {
        m_pVoiceProxy->Write( pData, Length );
    }
}

//==============================================================================

