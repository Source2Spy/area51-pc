//==============================================================================
//
//  DirectoryClient.cpp
//
//==============================================================================

#include "DirectoryClient.hpp"
#include "MatchPacket.hpp"
#include <random>

//==============================================================================

static const u32 DIRECTORY_MAGIC   = 0x41353144;
static const s32 DIRECTORY_VERSION = 1;
static const s32 DIRECTORY_PACKET  = 1200;

enum directory_packet_type
{
    DIRECTORY_PUBLISH = 1,
    DIRECTORY_CHALLENGE,
    DIRECTORY_CONFIRM,
    DIRECTORY_ACK,
    DIRECTORY_WITHDRAW,
    DIRECTORY_BROWSE,
    DIRECTORY_PAGE,
    DIRECTORY_BROWSE_CONFIRM,
    DIRECTORY_RESTART,
};

//==============================================================================

static xbool CreateToken( u64& Token )
{
    try
    {
        std::random_device Random;
        Token = ( (u64)Random() << 32 ) | (u64)Random();
        return Token != 0;
    }
    catch( ... )
    {
        return FALSE;
    }
}

//==============================================================================

directory_client::directory_client( net_socket& Socket, s32 Version ) : m_Socket( Socket )
{
    m_Version        = Version;
    m_Session        = 0;
    m_Generation     = 0;
    m_Cursor         = 0;
    m_Revision       = 0;
    m_NextID         = 1;
    m_Time           = 0.0f;
    m_NextPublish    = 0.0f;
    m_NextBrowse     = 0.0f;
    m_LastBrowse     = 0.0f;
    m_Registered     = FALSE;
    m_Hosting        = FALSE;
    m_Searching      = FALSE;
    m_Result         = MATCH_RESULT_OK;
    m_ResolvePending = FALSE;
    m_Resolving      = FALSE;
    x_memset( &m_Publish, 0, sizeof( m_Publish ) );
    x_memset( &m_Browse, 0, sizeof( m_Browse ) );
    x_memset( m_LeaseCookie, 0, sizeof( m_LeaseCookie ) );
}

//==============================================================================

match_result directory_client::Configure( const char* pAddress )
{
    if( m_Hosting || m_Searching )
        return MATCH_RESULT_UNAVAILABLE;

    match_endpoint Endpoint;
    net_address    Address;
    xstring        Name;

    if( pAddress && pAddress[0] )
    {
        if( !Endpoint.Parse( pAddress, MATCH_DIRECTORY_PORT ) )
            return MATCH_RESULT_INVALID_ADDRESS;

        Name = xfs( "%s:%d", Endpoint.Host, Endpoint.Port );
        Address.Parse( Name, MATCH_DIRECTORY_PORT );
    }

    if( Name != m_Endpoint )
    {
        m_Servers.Clear();
        m_Staging.Clear();
        m_Revision++;
    }

    m_Resolver.Cancel();
    m_Resolving      = FALSE;
    m_Endpoint       = Name;
    m_Address        = Address;
    m_ResolvePending = Name.GetLength() && Address.IsEmpty();
    m_Result         = m_ResolvePending ? MATCH_RESULT_PENDING : MATCH_RESULT_OK;

    return MATCH_RESULT_OK;
}

//==============================================================================

void directory_client::BeginHost( void )
{
    EndHost();

    if( m_Endpoint.GetLength() && !CreateToken( m_Session ) )
    {
        m_Result = MATCH_RESULT_UNAVAILABLE;
        return;
    }

    m_Hosting        = TRUE;
    m_ResolvePending = m_Endpoint.GetLength() && m_Address.IsEmpty();
    m_NextPublish    = m_Time;
}

//==============================================================================

void directory_client::Withdraw( void )
{
    if( m_Registered && !m_Socket.IsEmpty() )
    {
        byte         Data[40];
        match_packet Packet( Data, sizeof( Data ) );
        Packet.Write( DIRECTORY_MAGIC, 4 );
        Packet.Write( DIRECTORY_VERSION, 2 );
        Packet.Write( DIRECTORY_WITHDRAW, 2 );
        Packet.Write( m_Session, 8 );
        Packet.Write( m_Session, 8 );

        for( s32 i = 0; i < 16; i++ )
            Packet.Write( m_LeaseCookie[i], 1 );

        m_Socket.Send( m_Address, Data, Packet.GetLength() );
    }

    m_Registered     = FALSE;
    m_Publish.Active = FALSE;
}

//==============================================================================

void directory_client::EndHost( void )
{
    Withdraw();
    m_Hosting = FALSE;

    if( !m_Searching && m_Resolving )
    {
        m_Resolver.Cancel();
        m_Resolving = FALSE;
    }
}

//==============================================================================

void directory_client::Start( void )
{
    m_Searching      = TRUE;
    m_ResolvePending = m_Endpoint.GetLength() && m_Address.IsEmpty();
    m_Browse.Active  = FALSE;
    m_Generation     = 0;
    m_Cursor         = 0;
    m_Staging.Clear();
    m_NextBrowse = m_Time;
}

//==============================================================================

void directory_client::Stop( void )
{
    m_Searching     = FALSE;
    m_Browse.Active = FALSE;
    m_Staging.Clear();

    if( !m_Hosting && m_Resolving )
    {
        m_Resolver.Cancel();
        m_Resolving = FALSE;
    }
}

//==============================================================================

void directory_client::BeginRequest( request& Request, s32 Type )
{
    if( !CreateToken( Request.Token ) )
    {
        Request.Active = FALSE;
        m_Result       = MATCH_RESULT_UNAVAILABLE;
        return;
    }

    Request.Type      = Type;
    Request.Active    = TRUE;
    Request.Confirmed = FALSE;
    Request.Attempts  = 0;
    SendRequest( Request );
}

//==============================================================================

void directory_client::SendRequest( request& Request )
{
    byte         Data[DIRECTORY_PACKET];
    match_packet Packet( Data, sizeof( Data ) );
    Packet.Write( DIRECTORY_MAGIC, 4 );
    Packet.Write( DIRECTORY_VERSION, 2 );
    s32 Type = Request.Type;

    if( Request.Confirmed )
        Type = ( Type == DIRECTORY_PUBLISH ) ? DIRECTORY_CONFIRM : DIRECTORY_BROWSE_CONFIRM;

    Packet.Write( Type, 2 );
    Packet.Write( Request.Token, 8 );

    if( Request.Confirmed )
    {
        for( s32 i = 0; i < 16; i++ )
            Packet.Write( Request.Cookie[i], 1 );
    }

    for( s32 i = 0; i < Request.Length; i++ )
        Packet.Write( Request.Body[i], 1 );

    m_Socket.Send( m_Address, Data, Packet.GetLength() );
    Request.Attempts++;
    Request.Deadline = m_Time + 1.0f;
}

//==============================================================================

void directory_client::StartPage( void )
{
    match_packet Packet( m_Browse.Body, sizeof( m_Browse.Body ) );
    Packet.Write( m_Version, 4 );
    Packet.Write( m_Generation, 8 );
    Packet.Write( m_Cursor, 4 );
    m_Browse.Length = Packet.GetLength();
    m_NextBrowse    = m_Time + 5.0f;
    BeginRequest( m_Browse, DIRECTORY_BROWSE );
}

//==============================================================================

void directory_client::Update( f32 DeltaTime, const server_info* pHost )
{
    m_Time += MAX( DeltaTime, 0.0f );

    xbool Publish = m_Hosting && pHost && !( pHost->Flags & ( SERVER_IS_PRIVATE | SERVER_LAN_ONLY ) );

    if( m_ResolvePending && ( m_Searching || Publish ) )
    {
        if( !m_Resolving )
        {
            m_Result    = m_Resolver.Begin( m_Endpoint, MATCH_DIRECTORY_PORT );
            m_Resolving = ( m_Result == MATCH_RESULT_PENDING );
        }

        if( m_Resolving || ( m_Result == MATCH_RESULT_OK ) )
            m_Result = m_Resolver.Poll( DeltaTime, m_Address );

        if( m_Result != MATCH_RESULT_PENDING )
        {
            m_ResolvePending = FALSE;
            m_Resolving      = FALSE;
        }
    }

    if( m_Address.IsEmpty() || m_Socket.IsEmpty() )
        return;

    if( m_Hosting && pHost && ( pHost->Flags & ( SERVER_IS_PRIVATE | SERVER_LAN_ONLY ) ) )
        Withdraw();

    if( m_Hosting && pHost && !( pHost->Flags & ( SERVER_IS_PRIVATE | SERVER_LAN_ONLY ) ) &&
        !m_Publish.Active && ( m_Time >= m_NextPublish ) )
    {
        match_packet Packet( m_Publish.Body, sizeof( m_Publish.Body ) );
        Packet.Write( m_Session, 8 );
        Packet.WriteServer( *pHost );

        if( Packet.IsValid() )
        {
            m_Publish.Length = Packet.GetLength();
            BeginRequest( m_Publish, DIRECTORY_PUBLISH );
            m_NextPublish = m_Time + 10.0f;
        }
    }

    if( m_Searching && !m_Browse.Active && ( m_Time >= m_NextBrowse ) )
        StartPage();

    request* Requests[] = { &m_Publish, &m_Browse };

    for( s32 i = 0; i < 2; i++ )
    {
        request& Request = *Requests[i];

        if( !Request.Active || ( m_Time < Request.Deadline ) )
            continue;

        if( Request.Attempts < 3 )
        {
            SendRequest( Request );
            continue;
        }

        Request.Active = FALSE;
        m_Result       = MATCH_RESULT_TIMEOUT;

        if( i == 0 )
            m_NextPublish = m_Time + 5.0f;
        else
        {
            m_Staging.Clear();
            m_Generation = 0;
            m_Cursor     = 0;
            m_NextBrowse = m_Time + 5.0f;
        }
    }

    if( m_Servers.GetCount() && ( m_Time - m_LastBrowse > 30.0f ) )
    {
        m_Servers.Clear();
        m_Revision++;
    }
}

//==============================================================================

void directory_client::CommitServers( void )
{
    for( s32 i = 0; i < m_Staging.GetCount(); i++ )
    {
        server& Entry = m_Staging[i];
        Entry.Info.ID = 0;

        for( s32 j = 0; j < m_Servers.GetCount(); j++ )
        {
            if( ( Entry.Info.Remote == m_Servers[j].Info.Remote ) &&
                ( Entry.Session == m_Servers[j].Session ) )
                Entry.Info.ID = m_Servers[j].Info.ID;
        }

        if( !Entry.Info.ID )
            Entry.Info.ID = m_NextID++;
    }

    m_Servers = m_Staging;
    m_Staging.Clear();
    m_Revision++;
    m_LastBrowse = m_Time;
    m_Generation = 0;
    m_Cursor     = 0;
    m_NextBrowse = m_Time + 5.0f;
    m_Result     = MATCH_RESULT_OK;
}

//==============================================================================

xbool directory_client::ReceivePacket( const net_address& Remote, const void* pData, s32 Length )
{
    if( !pData || ( Length < 4 ) )
        return FALSE;

    match_packet Packet( (void*)pData, Length );

    if( Packet.Read( 4 ) != DIRECTORY_MAGIC )
        return FALSE;

    if( ( Remote != m_Address ) || ( Length < 16 ) || ( Length > DIRECTORY_PACKET ) ||
        ( Packet.Read( 2 ) != DIRECTORY_VERSION ) )
        return TRUE;

    s32      Type     = (s32)Packet.Read( 2 );
    u64      Token    = Packet.Read( 8 );
    request* pRequest = NULL;

    if( m_Publish.Active && ( m_Publish.Token == Token ) )
        pRequest = &m_Publish;

    if( m_Browse.Active && ( m_Browse.Token == Token ) )
        pRequest = &m_Browse;

    if( !pRequest )
        return TRUE;

    // Complete the cookie challenge before accepting publish or browse replies.
    if( Type == DIRECTORY_CHALLENGE )
    {
        s32  Operation = (s32)Packet.Read( 2 );
        byte Cookie[16];

        for( s32 i = 0; i < 16; i++ )
            Cookie[i] = (byte)Packet.Read( 1 );

        if( !Packet.IsComplete() || ( Operation != pRequest->Type ) || pRequest->Confirmed )
            return TRUE;

        x_memcpy( pRequest->Cookie, Cookie, sizeof( Cookie ) );
        pRequest->Confirmed = TRUE;
        pRequest->Attempts  = 0;
        SendRequest( *pRequest );
        return TRUE;
    }

    if( !pRequest->Confirmed )
        return TRUE;

    if( ( Type == DIRECTORY_ACK ) && ( pRequest == &m_Publish ) )
    {
        u64 Session = Packet.Read( 8 );
        s32 Lease   = (s32)Packet.Read( 2 );

        if( !Packet.IsComplete() || ( Session != m_Session ) || ( Lease < 15 ) || ( Lease > 300 ) )
            return TRUE;

        m_Registered = TRUE;
        x_memcpy( m_LeaseCookie, m_Publish.Cookie, sizeof( m_LeaseCookie ) );
        m_Publish.Active = FALSE;
        m_Result         = MATCH_RESULT_OK;
        return TRUE;
    }

    if( pRequest != &m_Browse )
        return TRUE;

    if( ( Type == DIRECTORY_RESTART ) && Packet.IsComplete() )
    {
        m_Browse.Active = FALSE;
        m_Staging.Clear();
        m_Generation = 0;
        m_Cursor     = 0;
        m_NextBrowse = m_Time + 1.0f;
        return TRUE;
    }

    if( Type != DIRECTORY_PAGE )
        return TRUE;

    // Validate the complete page before merging it into the staged search results.
    u64 Generation = Packet.Read( 8 );
    u32 Next       = (u32)Packet.Read( 4 );
    s32 Count      = (s32)Packet.Read( 1 );

    if( !Generation || ( m_Generation && ( m_Generation != Generation ) ) ||
        ( Count > MAX_MATCH_RESULTS ) ||
        ( ( Next != U32_MAX ) && ( !Count || ( Next != m_Cursor + Count ) ) ) )
        return TRUE;

    xarray<server> Page;

    for( s32 i = 0; i < Count; i++ )
    {
        server Entry;
        s32    IP     = (s32)Packet.Read( 4 );
        s32    Port   = (s32)Packet.Read( 2 );
        Entry.Session = Packet.Read( 8 );

        if( !Entry.Session || !IP || !Port || ( ( (u32)IP >> 24 ) >= 224 ) ||
            !Packet.ReadServer( Entry.Info ) || ( Entry.Info.Version != m_Version ) )
            return TRUE;

        Entry.Info.Remote.Setup( IP, Port );
        Entry.Info.Sources   = MATCH_SOURCE_INTERNET;
        Entry.Info.External  = Entry.Info.Remote;
        Entry.Info.PingDelay = -1.0f;

        for( s32 j = 0; j < m_Staging.GetCount(); j++ )
        {
            if( Entry.Info.Remote == m_Staging[j].Info.Remote )
                return TRUE;
        }

        for( s32 j = 0; j < Page.GetCount(); j++ )
        {
            if( Entry.Info.Remote == Page[j].Info.Remote )
                return TRUE;
        }

        Page.Append( Entry );
    }

    if( !Packet.IsComplete() )
        return TRUE;

    for( s32 i = 0; ( i < Page.GetCount() ) && ( m_Staging.GetCount() < MAX_MATCH_RESULTS ); i++ )
        m_Staging.Append( Page[i] );

    m_Browse.Active = FALSE;
    m_Generation    = Generation;
    m_Cursor        = Next;

    if( ( Next == U32_MAX ) || ( m_Staging.GetCount() == MAX_MATCH_RESULTS ) )
        CommitServers();
    else
        m_NextBrowse = m_Time;

    return TRUE;
}
