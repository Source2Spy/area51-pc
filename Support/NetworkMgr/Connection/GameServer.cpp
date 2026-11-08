//==============================================================================
//
//  GameServer.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#include "GameServer.hpp"
#include "NetworkMgr/ServerVersion.hpp"

#include "NetworkMgr/Match/MatchMgr.hpp"
#include "NetworkMgr/Messages/MsgMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "ClientProxy.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"

#include "x_log.hpp"

#include "NetworkMgr/NetLimits.hpp"

//#include "Entropy/VoIP/VoIP.hpp"

//==============================================================================

xbool SHOW_SERVER_PACKETS = FALSE;

#ifdef X_DEBUG
//#define SHOW_PACKET_SENDS    
#endif

f32 KICK_PING_THRESHOLD = 1000.0f;

//extern void SetFriendlyFire( xbool Flag );

//==============================================================================

s32 g_ClientsDropped   = 0;
s32 g_ClientsConnected = 0;

//==============================================================================

game_server::game_server( void )
{
    s32 i;

    m_AllowClients  = TRUE;
    m_MaxClients    = 0;
    m_PacketSendBudget = 0.0f;
    m_pClients      = NULL;
    m_State         = STATE_SERVER_IDLE;

    g_ClientsDropped = 0;
    g_ClientsConnected = 0;

    m_LocalPlayerCount = 0;
    for( i=0; i<NET_MAX_PER_CLIENT; i++ )
        m_LocalPlayerSlot[i] = -1;
}
//==============================================================================

game_server::~game_server( void )
{
}

//==============================================================================

void game_server::Init( net_socket& LocalSocket, s32 LocalPlayerCount, s32 MaxClients )
{
    m_RetiredConnections.Clear();
    m_BuildAuthentication.Clear();
    m_BanList.Clear();

    m_pLocal            = &LocalSocket;
    m_LocalPlayerCount  = LocalPlayerCount;
    m_UpdateIndex       = 0;
    m_MaxClients        = MaxClients;
    m_PacketSendBudget  = 0.0f;

    EnterState(STATE_SERVER_INIT);
}    

//==============================================================================

void game_server::Kill( void )
{
    ASSERT( GetState() == STATE_SERVER_IDLE );

    if( GetState() != STATE_SERVER_IDLE )
    {
        SetState( STATE_SERVER_SHUTDOWN );
    }

    while( GetState() != STATE_SERVER_IDLE )
    {
        Update( 0.0f );
    }

    m_BanList.Clear();

    SetState( STATE_SERVER_KILL );
    m_BuildAuthentication.Clear();
    while( GetState() != STATE_SERVER_IDLE )
    {
        Update( 0.0f );
    } 
}

//==============================================================================

net_address game_server::GetAddress( void )
{
    return m_pLocal->GetAddress();
}

//------------------------------------------------------------------------------

void game_server::AcceptLogin( const net_address& Remote, s32 ClientIndex, s32 PlayerCount )
{
    ASSERT( ClientIndex < m_MaxClients );

    client_proxy&       Client = m_pClients[ClientIndex];
    netstream           Reply;
    s32                 j;

    Client.m_ConnMgr.OpenPacket( Reply, CONN_PACKET_LOGIN_RESPONSE );
    Reply.WriteS32( PlayerCount );   // >=0 error code, means login succeeded. What else do we need to send?
    Reply.WriteU32( Client.m_ConnMgr.GetSessionID() );
    Reply.WriteU32( ClientIndex                         , 8 );
    Reply.WriteU32( g_ActiveConfig.GetMaxPlayerCount()  , 8 );
    Reply.WriteFlag( g_ActiveConfig.IsCheatsAllowed() );

    for( j = 0; j < PlayerCount; j++ )
    {
        Reply.WriteS32( Client.m_LocalPlayerSlot[j] );
        // ALSO NEED TO SEND THE NAME.
    }

    Reply.Close();

    Reply.Send(*m_pLocal, Remote);
}

//------------------------------------------------------------------------------

void game_server::RefuseLogin(const net_address& Remote, exit_reason Reason, u64 ConnectionID)
{
    netstream Reply;

    OpenConnectionPacket( Reply, CONN_PACKET_LOGIN_RESPONSE, ConnectionID );
    Reply.WriteS32(Reason);                            // Reason for refusal
    Reply.Close();
    Reply.Send(*m_pLocal, Remote);
}

//------------------------------------------------------------------------------

exit_reason s_ForceRefusal = GAME_EXIT_CONTINUE;

void game_server::ProcessLoginRequest( const net_address& Remote, netstream& BitStream, u64 ConnectionID )
{
    s32 i,j;

    s32     ServerVersion;
    s32     PlayerCount;
    char    Password[16];
    xbool   HasBuildResponse;
    byte    BuildResponse[BUILD_RESPONSE_BYTES];
    u64     PlayerIdentifier[NET_MAX_PER_CLIENT];
    char    PlayerNames[NET_MAX_PER_CLIENT][NET_NAME_LENGTH];
    byte    UniqueId[NET_MAX_ID_LENGTH];
    s32     Skin = SKIN_HAZMAT_0;

    Password[15] = '\0';

/*
    // Data per player
    xwchar  Name[16];
    s32     CharacterType;
    s32     SkinType;
    s32     VoiceType;
*/
    if( s_ForceRefusal != GAME_EXIT_CONTINUE )
    {
        RefuseLogin( Remote, s_ForceRefusal, ConnectionID );
        return;
    }
    if( !CanReadConnectionBits(BitStream, 32) )
        return;
    BitStream.ReadS32( ServerVersion );
    if( ServerVersion != g_ServerVersion )
    {
        RefuseLogin( Remote, GAME_EXIT_LOGIN_REFUSED, ConnectionID );
        return;
    }

    if( !CanReadConnectionBits( BitStream, 1 ) )
        return;

    BitStream.ReadFlag( HasBuildResponse );

    if( HasBuildResponse )
    {
        if( !CanReadConnectionBits( BitStream, sizeof(BuildResponse) * 8 ) )
            return;

        BitStream.ReadBits( BuildResponse, sizeof(BuildResponse) * 8 );
    }

    if( !CanReadConnectionBits(BitStream, 32 + NET_MAX_ID_LENGTH * 8) )
        return;
    BitStream.ReadRangedS32 ( PlayerCount, 0, x_max(NET_MAX_PER_CLIENT-1, 1) );
    BitStream.ReadRangedS32 ( Skin, SKIN_BEGIN_PLAYERS, SKIN_END_PLAYERS );
    BitStream.ReadBits      ( UniqueId, NET_MAX_ID_LENGTH*8 );
    if( !ReadConnectionString(BitStream, Password, sizeof(Password)) )
        return;


    if( BitStream.Overwrite() || (PlayerCount < 1) || (PlayerCount > NET_MAX_PER_CLIENT) )
        return;

    for( i=0; i<PlayerCount; i++ )
    {
        if( !CanReadConnectionBits(BitStream, 64) )
            return;
        BitStream.ReadU64( PlayerIdentifier[i] );
        if( !ReadConnectionString(BitStream, PlayerNames[i], NET_NAME_LENGTH) )
            return;

        if( !PlayerNames[i][0] )
            return;

        for( s32 j = 0; PlayerNames[i][j]; j++ )
        {
            if( (u8)PlayerNames[i][j] < 0x20 || (u8)PlayerNames[i][j] == 0x7F )
                return;
        }
    }
 
#if defined(X_DEBUG)
    if( !CanReadConnectionBits(BitStream, 32) )
        return;
    u32 Marker;
    BitStream.ReadU32( Marker );
    if( Marker != 0xDEADBEEF )
        return;
#endif
    if( !FinishConnectionPacket(BitStream) )
        return;

    // Validate password if server has one.
    if( g_ActiveConfig.IsPasswordEnabled() && x_strcmp( g_ActiveConfig.GetPassword(), Password ) )
    {
        RefuseLogin( Remote, GAME_EXIT_BAD_PASSWORD, ConnectionID );
        return;
    }

    if( IsBanned( Remote, UniqueId ) )
    {
        RefuseLogin( Remote, GAME_EXIT_CLIENT_BANNED, ConnectionID );
        LOG_WARNING( "game_server::ProcessLoginRequest", 
                     "Server refused banned client(%s).\n", Remote.GetStrIP() );
        return;
    }

    if( !g_ActiveConfig.IsCheatsAllowed() )
    {
        if( !build_authentication::HasKey() )
        {
            LOG_WARNING( "game_server::ProcessLoginRequest", "This build has no multiplayer authentication key." );
            RefuseLogin( Remote, GAME_EXIT_LOGIN_REFUSED, ConnectionID );
            return;
        }

        if( !HasBuildResponse )
        {
            byte Challenge[BUILD_CHALLENGE_BYTES];

            if( !m_BuildAuthentication.IssueChallenge( Remote, ConnectionID, Challenge ) )
            {
                RefuseLogin( Remote, GAME_EXIT_SERVER_BUSY, ConnectionID );
                return;
            }

            netstream Reply;
            OpenConnectionPacket( Reply, CONN_PACKET_BUILD_CHALLENGE, ConnectionID );
            Reply.WriteBits( Challenge, sizeof(Challenge) * 8 );
            Reply.Close();
            Reply.Send( *m_pLocal, Remote );
            return;
        }

        if( !m_BuildAuthentication.VerifyResponse( Remote, ConnectionID, BuildResponse ) )
        {
            RefuseLogin( Remote, GAME_EXIT_LOGIN_REFUSED, ConnectionID );
            return;
        }
    }

    if( GetClientCount() >= g_ActiveConfig.GetMaxPlayerCount() )
    {
        RefuseLogin( Remote, GAME_EXIT_SERVER_FULL, ConnectionID );
        LOG_WARNING( "game_server::ProcessLoginRequest", 
                     "Client dropped because too many clients(%d).\n", *(s32*)UniqueId );
        return;
    }

    // Check if client is already in list
    xbool FoundClient=FALSE;

    s32 Index = -1;
    for( i=0; i < m_MaxClients; i++ )
    {
        if( m_pClients[i].IsConnected() && (Remote == m_pClients[i].GetAddress()) )
        {
            if( m_pClients[i].m_ConnMgr.GetConnectionID() != ConnectionID )
            {
                if( !m_RetiredConnections.Add(Remote, m_pClients[i].m_ConnMgr.GetConnectionID()) )
                {
                    RefuseLogin( Remote, GAME_EXIT_SERVER_BUSY, ConnectionID );
                    return;
                }
                m_pClients[i].Disconnect( GAME_EXIT_PLAYER_QUIT );
                m_pClients[i].Kill();
            }
            else
            {
                Index = i;
                FoundClient = TRUE;
            }
            break;
        }
    }

    if( !FoundClient )
    {
        for( i=0; i<m_MaxClients; i++ )
        {
            if( !m_pClients[i].IsConnected() )
            {        
                Index = i;
                break;
            }
        }
    }

    // If we could not find a client slot to put this client in, just
    // bail out now and refuse the login request.
    if( Index == -1 )
    {
        LOG_WARNING( "game_server::ProcessLoginRequest", "Login refused because Server is full.\n" );
        RefuseLogin( Remote, GAME_EXIT_SERVER_FULL, ConnectionID );
        return;
    }

    if( GetAveragePing() > 600.0f )
    {
        RefuseLogin( Remote, GAME_EXIT_SERVER_BUSY, ConnectionID );
        g_ClientsConnected++;
        g_ClientsDropped++;
        LOG_WARNING( "game_server::ProcessLoginRequest", "Server dropped because it was too busy client(%d).\n", *(s32*)UniqueId );
        return;
    }

    // It wasn't in the list
    if( !FoundClient )
    {
        // Check if we have room to add one
        if( GameMgr.RoomForPlayers( PlayerCount ) )
        {
            m_pClients[Index].Init( m_pLocal,             // Local socket
                                    Remote,               // Remote address
                                    Index,                // Client index
                                    UniqueId );

            m_pClients[Index].m_ConnMgr.SetConnectionID( ConnectionID );

            s32 PlayerIndex;
            for( j = 0; j < PlayerCount; j++ )
            {
                xwchar Name[ 16 ];
                x_mstrcpy( Name, PlayerNames[j] );
                GameMgr.Connect( PlayerIndex, 
                                 Index, 
                                 PlayerIdentifier[j], 
                                 Name,
                                 Skin, 
                                 FALSE );
                ASSERT( PlayerIndex != -1 );
                m_pClients[Index].AddPlayer( PlayerIndex, PlayerNames[j], PlayerIdentifier[j] );
            }                        

            g_ActiveConfig.SetPlayerCount( g_ActiveConfig.GetPlayerCount() + PlayerCount );
            g_PendingConfig.SetPlayerCount( g_ActiveConfig.GetPlayerCount() );
            // We only change state if we were already registered. Otherwise,
            // the lack of registration would mean that timeouts in matchmgr
            // will deal with registration.
            UpdateRegistration();

            // Add to empty slot
//          m_pClients[Index].LastHeartbeat   = tgl.LogicTimeMs;
//          m_pClients[Index].HeartbeatDelay  = 0;
//          m_pClients[Index].PacketShipDelay = 0;

//          m_pClients[Index].UpdateManager.SetClientControlled( m_pClients[Index].PlayerIndex[0],
//                                                               m_pClients[Index].PlayerIndex[1]);

//          LOG_WARNING( "game_server::ProcessLoginRequest", "ConnManager inited %d\n", Index );

            // Decide on session ID
            s32 SessionID = x_irand(0,65536);
            m_pClients[Index].m_ConnMgr.SetSessionID(SessionID);

            // Send login succeeded message back

            AcceptLogin( Remote, Index, PlayerCount );

            LOG_WARNING( "game_server::ProcessLoginRequest", 
                         "Client added, index:%d, SessionID:%d",
                         Index, SessionID );
        }
        else
        {
            RefuseLogin( Remote, GAME_EXIT_SERVER_FULL, ConnectionID );
            LOG_WARNING( "game_server::ProcessLoginRequest", "Server refused Client due to overcrowding\n");
        }
    }
    else
    // It was in the list
    {
        AcceptLogin( Remote, Index, PlayerCount );
        LOG_WARNING( "game_server::ProcessLoginRequest", "Got a double client login?\n");
    }
}

//------------------------------------------------------------------------------

xbool game_server::ReceivePacket( const net_address& Remote, netstream& BitStream )
{
    conn_packet_type PacketType;
    u64 ConnectionID;
    s32     i;

    // Was it destined for the connection manager of any of our
    // currently connected clients?
    for (i=0;i<m_MaxClients;i++)
    {
        if ( m_pClients[i].IsConnected() && (Remote == m_pClients[i].GetAddress()) )
        {
            if (m_pClients[i].ReceivePacket(BitStream))
                return TRUE;
        }
    }


    if( !ReadConnectionPacket(BitStream, PacketType, ConnectionID) )
        return FALSE;

    switch( PacketType )
    {
    case CONN_PACKET_LOGIN_REQUEST:
        // MSK NOTE 2
        if( !m_RetiredConnections.Contains(Remote, ConnectionID) )
            ProcessLoginRequest( Remote, BitStream, ConnectionID );
        break;

    case CONN_PACKET_CANCEL_LOGIN:
    {
        if( !FinishConnectionPacket(BitStream) )
            return TRUE;

        if( !m_RetiredConnections.Add(Remote, ConnectionID) )
            return TRUE;
        for( i = 0; i < m_MaxClients; i++ )
        {
            if( m_pClients[i].IsConnected() && (Remote == m_pClients[i].GetAddress()) &&
                (m_pClients[i].m_ConnMgr.GetConnectionID() == ConnectionID) )
            {
                m_pClients[i].Disconnect( GAME_EXIT_PLAYER_QUIT );
                m_pClients[i].Kill();
            }
        }
        netstream Reply;
        OpenConnectionPacket( Reply, CONN_PACKET_CANCEL_ACK, ConnectionID );
        Reply.Close();
        Reply.Send( *m_pLocal, Remote );
        break;
    }

    default:
        x_DebugMsg( "Unknown packet 0x%04X received from %s\n", 
                    PacketType, Remote.GetStrAddress() );
        return FALSE;
        break;
    }
    return TRUE;
}

//==============================================================================

xbool   s_ForceClientDrop    = FALSE;
f32     g_ShipDelayPingLimit = 500.0f;
f32     g_ShipDelayModifier  =   2.5f;
f32        s_KickTimeout        =  10.0f;
xtimer  s_KickTimer;

//==============================================================================

exit_reason game_server::Update( f32 DeltaTime )
{
    s32     i, j;
//  byte    VoiceBuffer[128];
    s32     Length;
    (void)Length;

    UpdateState(DeltaTime);
    UpdateVoice(DeltaTime);

    //
    // Do heartbeats, and kicks too
    //

    for( i=0; i<m_MaxClients; i++ )
    {
        if( m_pClients[i].IsConnected() )
        {
            m_pClients[i].Update(DeltaTime,GetState());
        }
    }

    // If our average client ping time is greater than 1000ms, 
    // for 10 seconds then we look for the last client that 
    // joined and kick them.
    if ( (GetAveragePing() > KICK_PING_THRESHOLD) || s_ForceClientDrop)
    {
        if (s_KickTimer.IsRunning() || s_ForceClientDrop)
        {
            if ( (s_KickTimer.ReadSec() > s_KickTimeout) || s_ForceClientDrop)
            {
                s32 Index;

                Index = m_MaxClients-1;
                s_ForceClientDrop = FALSE;
                for( i=0; i<m_MaxClients; i++ )
                {
                    if( m_pClients[Index].IsConnected() )
                    {
                        LOG_ERROR("game_server::Update","Disconnecting client %d, Average Ping:%2.02f, KickTimer:%2.02f",Index,GetAveragePing(),(f32)s_KickTimer.ReadSec());
                        {
                            s32 j;
                            for (j=0;j<m_MaxClients;j++)
                            {
                                if (m_pClients[j].IsConnected())
                                {
                                    LOG_WARNING("game_server::Update","Client %d, ping %2.02f",j,m_pClients[j].m_ConnMgr.GetPing());
                                }
                            }
                        }
                        m_pClients[Index].Disconnect( GAME_EXIT_SERVER_BUSY );
                        // 20 seconds until we drop another client
                        s_KickTimeout = 15.0f;
                        g_ClientsDropped++;
                        break;
                    }
                    s_KickTimer.Stop();
                    s_KickTimer.Reset();
                    Index--;
                }
            }
        }
        else
        {
            s_KickTimer.Reset();
            s_KickTimer.Start();
        }
    }
    else
    {
        s_KickTimer.Stop();
        s_KickTimer.Reset();
        s_KickTimeout = 10.0f;
    }

    // Now figure out if it is time to send an update packet to
    // each of the attached clients
    if( m_State == STATE_SERVER_INGAME )          /// g_SM.MissionRunning
    {
        //
        // Dish out any net pain we may have accumulated from the clients.
        //
        {
            for( j=0; j<m_MaxClients; j++ )
            {
                if( m_pClients[j].m_IsConnected )
                {
                    while( m_pClients[j].m_PainQueue.GetCount() )
                    {
                        // Ship this pain off to the unlucky recipient.
                        net_pain NetPain;
                        m_pClients[j].m_PainQueue.GetPain( NetPain );
                        NetObjMgr.ApplyNetPain( NetPain );
                    }
                }
            }
        }

        s32 nClients=0;
        for( j = 0; j < m_MaxClients; j++ )
        {
            if( m_pClients[j].m_ConnMgr.IsConnected() )
                nClients++;
        }

        if( nClients > 0 )
        {
            //
            // For each object, update its dirty bits with respect to every 
            // client.
            //
            for( i = 0; i < NET_MAX_OBJECTS; i++ )
            {
                netobj* pObject = NetObjMgr.GetObjFromSlot( i );
                u32     Bits    = 0x00000000;

                if( pObject )
                {
                    ASSERT( pObject->net_GetSlot() == i );
                    Bits = pObject->net_GetClearDirty();

                    if( Bits )
                    {
                        for( j = 0; j < m_MaxClients; j++ )
                        {
                            if( m_pClients[j].m_ConnMgr.IsConnected() )
                            {
                                u32 Mask = pObject->net_GetUpdateMask( j );
                                m_pClients[j].m_UpdateMgr.AddDirtyBits( i, Bits & Mask );
                            }
                        }
                    }
                }

                if( NetObjMgr.JustRemoved( i ) )
                {
                    for( j = 0; j < m_MaxClients; j++ )
                    {
                        if( m_pClients[j].m_ConnMgr.IsConnected() )
                        {
                            m_pClients[j].m_UpdateMgr.ObjectRemoved( i );
                        }
                    }
                }
            }
        }

        //
        // Setup updates for game manager.
        //
        {
            u32 DirtyBits;
            u32 ScoreBits;
            u32 PlayerBits;

            GameMgr.GetClearDirty( DirtyBits, ScoreBits, PlayerBits );

            for( j=0; j<m_MaxClients; j++ )
            {
                if( m_pClients[j].m_IsConnected )
                {
                    m_pClients[j].m_GMMgr.AddDirtyBits( DirtyBits, 
                                                        ScoreBits, 
                                                        PlayerBits );
                }
            }
        }

        //
        // Now, ship packets out to the clients as needed.
        //
        // We want to prevent "outbound packet harmonic" problems where, for
        // whatever reason, the server starts trying to send packets in "bursts
        // with gaps" rather than evenly divided among clients and updates.
        //
        // Accumulate packet capacity in seconds, not frames.  Each client can
        // ship at least once per MIN_SHIP_INTERVAL, so this is the maximum
        // aggregate rate that the per-client ship timers can request.
        //
        f32 const PacketRate = nClients / MIN_SHIP_INTERVAL;
        m_PacketSendBudget += DeltaTime * PacketRate;

        // Keep the bucket bounded so a long stall cannot release an unbounded
        // burst.  One extra token peserves fractional capacity between
        // updates without turning the bucket into a frame-based throttle.
        f32 const MaxPacketBudget = (f32)nClients + 1.0f;
        if( m_PacketSendBudget > MaxPacketBudget )
        {
            m_PacketSendBudget = MaxPacketBudget;
        }

        s32 PacketsAllowed = 0;
        if( DeltaTime > 0.0f )
        {
            s32 const PacketsThisUpdate = (s32)x_ceil( DeltaTime * PacketRate );
            PacketsAllowed = MIN( (s32)m_PacketSendBudget, PacketsThisUpdate );
        }
        for( i = 0; (i < m_MaxClients) && (PacketsAllowed > 0); i++ )
        {
            if( m_pClients[m_UpdateIndex].ProvideUpdate() )
            {
                PacketsAllowed--;
                m_PacketSendBudget -= 1.0f;
            }

            m_UpdateIndex++;
            if( m_UpdateIndex >= m_MaxClients )
            {
                m_UpdateIndex = 0;
            }
        }
    }
    else
    {
        m_PacketSendBudget = 0.0f;
    }

    return GetExitReason();
}

//==============================================================================

void game_server::DisplayPings( s32 L )
{
    x_printfxy( 0, L++, "SERVER'S CLIENTS" );

    for( s32 i=0; i<m_MaxClients; i++ )
    {
        if( m_pClients[i].IsConnected() )
        {
            x_printfxy( 0, L++, "%2d] %1f",
                        i, m_pClients[i].m_ConnMgr.GetPing() );
        }
    }
}

//==============================================================================

f32 game_server::GetAveragePing( void )
{
    f32 average = 0.0f;
    s32 count   = 0;
    s32 i;

    for( i=0; i<m_MaxClients; i++ )
    {
        if( m_pClients[i].IsConnected() && m_pClients[i].HasMapLoaded() )
        {
            count++;
            average += m_pClients[i].m_ConnMgr.GetPing();
        }
    }

    if( count )
    {
        return( average / count );
    }

    return( 0.0f );
}

//==============================================================================

void game_server::ShowLifeDelay( void )
{
    s32 L = 0;
    for( s32 i=0; i<m_MaxClients; i++ )
    {
        if( m_pClients[i].IsConnected() )
        {
            x_printfxy( 0, L, "%2d] %5.1f", i, m_pClients[i].m_ConnMgr.GetPing() );
        //  m_pClients[i].UpdateManager.ShowLifeDelay(L);
            L++;
        }
    }
}

//==============================================================================

void game_server::SyncMissionLoad( void )
{
}

//==============================================================================

void game_server::KickPlayer( s32 PlayerIndex )
{
    s32 i;

    for( i = 0; i < m_MaxClients; i++ )
    {
        if( m_pClients[i].KickPlayer( PlayerIndex ) )
            break;
    }
}

//==============================================================================

void game_server::BanPlayer( s32 PlayerIndex )
{
    if( !IN_RANGE( 0, PlayerIndex, NET_MAX_PLAYERS - 1 ) )
        return;

    s32 ClientIndex = GetClientIndex( PlayerIndex );
    if( (ClientIndex < 0) || !m_pClients[ClientIndex].IsConnected() )
        return;

    BanClient( m_pClients[ClientIndex].GetAddress(), m_pClients[ClientIndex].GetUniqueId() );
    m_pClients[ClientIndex].KickPlayer( PlayerIndex );
}

//==============================================================================

void game_server::DropPlayer( s32 PlayerIndex )
{
    s32 i;
    (void)PlayerIndex;

    for( i = 0; i < m_MaxClients; i++ )
    {
        /*
        if( (m_pClients[i].IsConnected()) &&
            ((m_pClients[i].m_PlayerIndex[0] == PlayerIndex) ||
             (m_pClients[i].m_PlayerIndex[1] == PlayerIndex)) )
        {
            m_pClients[i].SetStatus(CLIENT_ERR_DROPPED);
        }
        */
    }
}

//==============================================================================

void game_server::BanClient( const net_address& Remote, const byte* ClientUniqueId )
{
    for( s32 i = 0; i < m_BanList.GetCount(); i++ )
    {
        const client_ban& Ban = m_BanList[i];
        if( (Ban.IP == Remote.GetIP()) &&
            (x_memcmp( Ban.UniqueId, ClientUniqueId, sizeof(Ban.UniqueId) ) == 0) )
            return;
    }

    client_ban& Ban = m_BanList.Append();
    Ban.IP = Remote.GetIP();
    x_memcpy( Ban.UniqueId, ClientUniqueId, sizeof(Ban.UniqueId) );
}

//==============================================================================

xbool game_server::IsBanned( const net_address& Remote, const byte* ClientUniqueId ) const
{
    for( s32 i = 0; i < m_BanList.GetCount(); i++ )
    {
        const client_ban& Ban = m_BanList[i];
        if( (Ban.IP == Remote.GetIP()) ||
            (x_memcmp( Ban.UniqueId, ClientUniqueId, sizeof(Ban.UniqueId) ) == 0) )
            return TRUE;
    }

    return FALSE;
}

//==============================================================================

s32 game_server::GetBanCount( void ) const
{
    return m_BanList.GetCount();
}

//==============================================================================

xbool game_server::IsInGame( s32 clientIndex )
{
    return m_pClients[clientIndex].IsInGame();
}

//==============================================================================

s32 game_server::GetClientCount( void )
{
    s32 i;
    s32 Count;

    Count = 0;
    for (i=0;i<m_MaxClients;i++)
    {
        if (m_pClients[i].IsConnected())
        {
            Count++;
        }
    }
    return Count;
}

//==============================================================================

s32 game_server::GetClientIndex( s32 PlayerIndex )
{
    s32     i;
    s32     j;

    for( i=0; i<m_MaxClients; i++ )
    {
        for( j=0; j< m_pClients[i].GetPlayerCount(); j++ )
        {
            if( PlayerIndex == m_pClients[i].GetPlayerSlot( j ) )
            {
                return i;
            }
        }
    }

    for( i=0; i<m_LocalPlayerCount; i++ )
    {
        if( m_LocalPlayerSlot[i] == PlayerIndex )
            return -1;
    }

    return -2;
}

//==============================================================================

s32 game_server::GetClientPlayerSlot ( s32 ClientIndex, s32 PlayerIndex )
{
    if ( ClientIndex == m_MaxClients)
    {
        return GetLocalPlayerSlot(PlayerIndex);
    }

    ASSERT(m_pClients[ClientIndex].IsConnected());
    return m_pClients[ClientIndex].GetPlayerSlot(PlayerIndex);
}

//==============================================================================

s32 game_server::GetClientPlayerCount( s32 ClientIndex )
{
    ASSERT( ClientIndex != m_MaxClients );
    ASSERT(m_pClients[ClientIndex].IsConnected());
    return m_pClients[ClientIndex].GetPlayerCount();
}

//==============================================================================
s32 game_server::GetLocalPlayerCount( void )
{
    return( m_LocalPlayerCount );
}

//==============================================================================

s32 game_server::GetLocalPlayerSlot( s32 Player )
{
    ASSERT( IN_RANGE( 0, Player, NET_MAX_PER_CLIENT-1 ) );
    return( m_LocalPlayerSlot[ Player ] );
}

//==============================================================================

voice_proxy& game_server::GetVoiceProxy( s32 ClientIndex )
{
    return m_pClients[ClientIndex].m_VoiceProxy;
}

//==============================================================================
// This only deals with the local server's headset.

void game_server::UpdateVoice( f32 DeltaTime )
{
    s32     VoiceDataLength = 0;
    byte    VoiceDataBuffer[ 256 ];

    (void)DeltaTime;

    if( g_NetworkMgr.IsOnline() == FALSE )
    {
        return;
    }


    while( TRUE )
    {
        VoiceDataLength = g_VoiceMgr.ReadFromVoiceFifo( VoiceDataBuffer, g_VoiceMgr.GetHeadset().GetEncodedBlockSize() );
        if( VoiceDataLength <= 0 )
        {
            break;
        }
        g_VoiceMgr.Distribute( m_LocalPlayerSlot[0], VoiceDataBuffer, VoiceDataLength );
    }
}


//==============================================================================
void game_server::UpdateRegistration( void )
{
    // Go through, building whatever is needed to be sent to the matchmaking
    // service. We need to tell it what players are connected to this server.
}

//==============================================================================
f32 game_server::GetClientPing( s32 ClientIndex )
{
    ASSERT( ClientIndex < m_MaxClients );
    if( ClientIndex < 0 )
    {
        return 0.0f;
    }
    return m_pClients[ClientIndex].m_ConnMgr.GetPing();
}

//==============================================================================

xbool game_server::IsClientConnected( s32 ClientIndex )
{
    return m_pClients[ClientIndex].IsConnected();
}

//==============================================================================
