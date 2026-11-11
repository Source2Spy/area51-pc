//==============================================================================
//
//  LanDiscovery.cpp
//
//==============================================================================

#include "LanDiscovery.hpp"
#include "MatchPacket.hpp"
#include "NetworkMgr/NetworkMgr.hpp"

//==============================================================================

static const u32 LAN_PACKET_MAGIC   = 0x4135314C;
static const s32 LAN_PACKET_VERSION = 1;
static const s32 LAN_PACKET_SIZE    = 512;
static const f32 LAN_QUERY_INTERVAL = 2.0f;
static const f32 LAN_QUERY_LIFETIME = 6.0f;
static const f32 LAN_SERVER_TIMEOUT = 8.0f;

enum match_packet_type
{
    LAN_QUERY = 1,
    LAN_RESPONSE,
};

//==============================================================================

lan_discovery::lan_discovery( net_socket& Socket ) : m_Socket( Socket )
{
    m_Token          = (u64)x_GetTime() ^ ( (u64)(u32)x_rand() << 32 );
    m_Session        = m_Token ? m_Token : 1;
    m_Revision       = 0;
    m_NextID         = 1;
    m_Time           = 0.0f;
    m_NextLookup     = 0.0f;
    m_ResponseBudget = 32.0f;
    m_Searching      = FALSE;
}

//==============================================================================

void lan_discovery::BeginHost( void )
{
    if( ++m_Session == 0 )
        m_Session++;

    m_ResponseBudget = 32.0f;
}

//==============================================================================

void lan_discovery::Start( void )
{
    if( !m_Searching )
    {
        m_Servers.Clear();
        m_Revision++;
    }

    m_Queries.Clear();
    m_Searching  = TRUE;
    m_NextLookup = m_Time;
}

//==============================================================================

void lan_discovery::Stop( void )
{
    m_Searching = FALSE;
    m_Queries.Clear();
}

//==============================================================================

void lan_discovery::Update( f32 DeltaTime )
{
    m_Time += MAX( DeltaTime, 0.0f );
    m_ResponseBudget = MIN( 32.0f, m_ResponseBudget + MAX( DeltaTime, 0.0f ) * 16.0f );

    // Expire old queries and server entries before scheduling the next lookup.
    for( s32 i = m_Queries.GetCount() - 1; i >= 0; i-- )
    {
        m_Queries[i].Age += MAX( DeltaTime, 0.0f );

        if( m_Queries[i].Age > LAN_QUERY_LIFETIME )
            m_Queries.Delete( i );
    }

    for( s32 i = m_Servers.GetCount() - 1; i >= 0; i-- )
    {
        if( m_Time - m_Servers[i].LastSeen > LAN_SERVER_TIMEOUT )
        {
            m_Servers.Delete( i );
            m_Revision++;
        }
    }

    if( m_Searching && ( m_Time >= m_NextLookup ) )
    {
        xarray<s32> Broadcasts;

        for( s32 i = 0; i < 64; i++ )
        {
            interface_info Info;
            net_GetInterfaceInfo( i, Info );

            if( !Info.IsAvailable || !Info.Address || !Info.Broadcast ||
                ( ( (u32)Info.Address >> 24 ) == 127 ) || ( Info.Broadcast == Info.Address ) )
                continue;

            if( Broadcasts.Find( Info.Broadcast ) < 0 )
                Broadcasts.Append( Info.Broadcast );
        }

        if( Broadcasts.GetCount() == 0 )
        {
            interface_info Info;
            net_GetInterfaceInfo( -1, Info );
            s32 Address = ( ( (u32)Info.Address >> 24 ) == 127 ) ? Info.Address : (s32)0xFFFFFFFF;
            SendLookup( net_address( Address, NET_GAME_PORT ) );
        }

        for( s32 i = 0; i < Broadcasts.GetCount(); i++ )
            SendLookup( net_address( Broadcasts[i], NET_GAME_PORT ) );

        m_NextLookup = m_Time + LAN_QUERY_INTERVAL;
    }
}

//==============================================================================

xbool lan_discovery::SendLookup( const net_address& Remote )
{
    if( !m_Searching || m_Socket.IsEmpty() || ( m_Queries.GetCount() >= 256 ) )
        return FALSE;

    byte         Data[16];
    match_packet Packet( Data, sizeof( Data ) );
    Packet.Write( LAN_PACKET_MAGIC, 4 );
    Packet.Write( LAN_PACKET_VERSION, 2 );
    Packet.Write( LAN_QUERY, 2 );

    if( ++m_Token == 0 )
        m_Token++;

    Packet.Write( m_Token, 8 );

    if( !m_Socket.Send( Remote, Data, sizeof( Data ) ) )
        return FALSE;

    query& Query = m_Queries.Append();
    Query.Token  = m_Token;
    Query.Sent   = x_GetTime();
    Query.Age    = 0.0f;

    return TRUE;
}

//==============================================================================

xbool lan_discovery::ReceivePacket( const net_address& Remote, const void* pData,
                                    s32 Length, const server_info* pHost )
{
    if( !pData || ( Length < 4 ) )
        return FALSE;

    match_packet Packet( (void*)pData, Length );

    if( Packet.Read( 4 ) != LAN_PACKET_MAGIC )
        return FALSE;

    if( ( Length < 16 ) || ( Length > LAN_PACKET_SIZE ) ||
        ( Packet.Read( 2 ) != LAN_PACKET_VERSION ) )
        return TRUE;

    u32 Type  = (u32)Packet.Read( 2 );
    u64 Token = Packet.Read( 8 );

    if( Type == LAN_QUERY )
    {
        if( !Packet.IsComplete() || !Token || !pHost ||
            ( pHost->Flags & SERVER_IS_PRIVATE ) || ( m_ResponseBudget < 1.0f ) )
            return TRUE;

        byte         Response[LAN_PACKET_SIZE];
        match_packet Reply( Response, sizeof( Response ) );
        Reply.Write( LAN_PACKET_MAGIC, 4 );
        Reply.Write( LAN_PACKET_VERSION, 2 );
        Reply.Write( LAN_RESPONSE, 2 );
        Reply.Write( Token, 8 );
        Reply.Write( m_Session, 8 );
        Reply.WriteServer( *pHost );

        if( Reply.IsValid() )
        {
            m_ResponseBudget -= 1.0f;
            m_Socket.Send( Remote, Response, Reply.GetLength() );
        }

        return TRUE;
    }

    if( ( Type != LAN_RESPONSE ) || !m_Searching )
        return TRUE;

    const query* pQuery = NULL;

    for( s32 i = 0; i < m_Queries.GetCount(); i++ )
    {
        if( m_Queries[i].Token == Token )
            pQuery = &m_Queries[i];
    }

    if( !pQuery )
        return TRUE;

    server Entry;
    x_memset( &Entry.Info, 0, sizeof( Entry.Info ) );
    Entry.Session  = Packet.Read( 8 );
    Entry.Token    = Token;
    Entry.LastSeen = m_Time;

    if( !Packet.ReadServer( Entry.Info ) || !Packet.IsComplete() || !Entry.Session )
        return TRUE;

    Entry.Info.Remote    = Remote;
    Entry.Info.Sources   = MATCH_SOURCE_LAN;
    Entry.Info.External  = Remote;
    Entry.Info.PingDelay = x_TicksToMs( x_GetTime() - pQuery->Sent );

    for( s32 i = 0; i < m_Servers.GetCount(); i++ )
    {
        server& Existing = m_Servers[i];

        if( Existing.Info.Remote == Remote )
        {
            if( Token <= Existing.Token )
                return TRUE;

            Entry.Info.ID = ( Existing.Session == Entry.Session ) ? Existing.Info.ID : m_NextID++;
            Existing      = Entry;
            m_Revision++;
            return TRUE;
        }
    }

    if( m_Servers.GetCount() < MAX_MATCH_RESULTS )
    {
        Entry.Info.ID = m_NextID++;
        m_Servers.Append( Entry );
        m_Revision++;
    }

    return TRUE;
}
