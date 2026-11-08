//==============================================================================
//
//  NetLib_Windows.cpp
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_WINDOWS
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//=========================================================================
//  INCLUDES
//=========================================================================

// Auto include WinSock libs in a .NET build
#if _MSC_VER >= 1300
#pragma comment( lib, "ws2_32.lib" )
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "e_Network.hpp"

//==============================================================================

static s32 s_STAT_NPacketsSent;
static s32 s_STAT_NPacketsReceived;
static s32 s_STAT_NBytesSent;
static s32 s_STAT_NBytesReceived;
static s32 s_STAT_NAddressesBound;
extern xtimer NET_SendTime;
extern xtimer NET_ReceiveTime;

static xbool s_Inited = FALSE;

//==============================================================================
//  FUNCTIONS
//==============================================================================

void sys_net_Init( void )
{
    ASSERT( !s_Inited );

    WSADATA wsadata;
    const s32 Result = WSAStartup( MAKEWORD( 2, 2 ), &wsadata );
    if( Result != 0 )
    {
        ASSERTS( FALSE, "WSAStartup failed" );
        return;
    }

    s_STAT_NPacketsSent     = 0;
    s_STAT_NPacketsReceived = 0;
    s_STAT_NBytesSent       = 0;
    s_STAT_NBytesReceived   = 0;
    s_STAT_NAddressesBound  = 0;
    s_Inited                 = TRUE;
}

//==============================================================================

void sys_net_Kill( void )
{
    ASSERT( s_Inited );
    s_Inited = FALSE;

    WSACleanup();
}

//==============================================================================

xbool net_IsInited( void )
{
    return s_Inited;
}

//==============================================================================

xbool    net_socket::Bind( s32 StartPort, s32 Flags )
{
    ASSERT( s_Inited );

    // Clear address in case we need to exit early

    struct sockaddr_in addr;
    SOCKET sd_dg;

    if( StartPort <= 0 )
    {
        StartPort = x_irand( 8192, 16384 );
    }

    if( StartPort > 65535 )
    {
        return FALSE;
    }

    // create an address and bind it
    x_memset(&addr,0,sizeof(struct sockaddr_in));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(StartPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // create a socket
    if( Flags & NET_FLAGS_VDP )
    {
        ASSERTS( FALSE, "NET_FLAGS_VDP is not supported on Windows" );
        sd_dg = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
    }
    else
    {
        sd_dg = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
    }

    if( sd_dg == INVALID_SOCKET )
    {
        return FALSE;
    }

    // attempt to bind to the port
    while( bind( sd_dg, (struct sockaddr *)&addr, sizeof(addr) ) == SOCKET_ERROR )
    {
        // increment port if that port is assigned
        if( (WSAGetLastError() == WSAEADDRINUSE) && (StartPort < 65535) && !(Flags & NET_FLAGS_BIND_EXACT) )
        {
            StartPort++;
            addr.sin_port = htons( StartPort );
        }
        // if some other error, nothing we can do...abort
        else
        {
            closesocket( sd_dg );
            return FALSE;
            //StartPort = net_NOSLOT;
            //break;
        }
    }

    // set this socket to non-blocking, so we can poll it
    u_long  dwNoBlock = !(Flags & NET_FLAGS_BLOCKING);
    if( ioctlsocket( sd_dg, FIONBIO, &dwNoBlock ) == SOCKET_ERROR )
    {
        closesocket( sd_dg );
        return FALSE;
    }

    // fill out the slot structure
    interface_info Info;
    net_GetInterfaceInfo( -1, Info );
    if( !Info.IsAvailable || (Info.Address == 0) )
    {
        closesocket( sd_dg );
        return FALSE;
    }

    m_Address.Setup( Info.Address, StartPort );
    m_Socket = sd_dg;

    if(Flags & NET_FLAGS_BROADCAST)
    {
        u_long dwBroadcast;
        dwBroadcast = TRUE;
        setsockopt( (SOCKET)m_Socket, SOL_SOCKET, SO_BROADCAST, (char *)&dwBroadcast, sizeof(u_long) );
    }

    s_STAT_NAddressesBound++;
    return TRUE;
}

//==============================================================================

void net_socket::Close( void )
{
    ASSERT( s_Inited );

    if( m_Socket != BAD_SOCKET )
    {
        closesocket( (SOCKET)m_Socket );
        m_Socket = BAD_SOCKET;

        if( s_STAT_NAddressesBound > 0 )
        {
            s_STAT_NAddressesBound--;
        }
    }

    m_Address.Clear();
}

//==============================================================================

xbool sys_net_Receive( net_socket&   Local,
                       net_address&  Remote,
                       void*         pBuffer,
                       s32&          BufferSize )
{
    s32   RetSize;

    ASSERT( s_Inited );

    struct sockaddr_in sockfrom;
    s32 addrsize = sizeof(sockaddr_in);

    // receive any incoming packet
    RetSize = recvfrom( (SOCKET)Local.m_Socket, 
                        (char*)pBuffer, 
                        BufferSize, 
                        0, 
                        (struct sockaddr *)&sockfrom, 
                        &addrsize );

    // if a packet was received
    if ( RetSize > 0 )
    {
        // fill out the "From" with the appropriate information
        Remote.Setup( ntohl(sockfrom.sin_addr.s_addr), ntohs(sockfrom.sin_port) );
        ASSERT( RetSize <= BufferSize );
        BufferSize = RetSize;

        s_STAT_NPacketsReceived++;
        s_STAT_NBytesReceived += BufferSize;
        return TRUE;
    }
    else
    {
        s32 LastError = WSAGetLastError();
        // We could use WSAECONNRESET to signify the socket was closed by the
        // target side. This *should* only apply to TCP sockets but it also
        // seems to apply in Windows for UDP sockets if sent to the same machine.
        if ( (LastError != WSAEWOULDBLOCK) && (LastError != WSAECONNRESET) )
        {
            x_DebugMsg("RecvFrom returned an error %d\n",LastError);
        }
        BufferSize = 0;
    }

    // No packet received
    return FALSE;
}

//==============================================================================

void sys_net_Send  ( net_socket&         Local, 
                     const net_address&  Remote, 
                     const void*         pBuffer, 
                     s32                 BufferSize )
{
    s32 status;
    s32 LastError;
    ASSERT( s_Inited );

    s_STAT_NPacketsSent++;
    s_STAT_NBytesSent += BufferSize;

    struct sockaddr_in sockto;

    // address your package and stick a stamp on it :-)
    x_memset(&sockto,0,sizeof(struct sockaddr_in));
    sockto.sin_family       = AF_INET;
    sockto.sin_port         = htons(Remote.GetPort());
    sockto.sin_addr.s_addr  = htonl( Remote.GetIP() );
    status = sendto( (SOCKET)Local.m_Socket, (const char*)pBuffer, BufferSize, 0, (struct sockaddr*)&sockto, sizeof(sockto) );
    if( status == SOCKET_ERROR )
    {
        LastError = WSAGetLastError();
        if( LastError != WSAEWOULDBLOCK )
        {
            x_DebugMsg("SendTo returned an error code %d\n", LastError);
        }
    }
}


//==============================================================================

void net_GetInterfaceInfo( s32 id,interface_info &info )
{
    INTERFACE_INFO InterfaceList[8];
    unsigned long nBytesReturned;
    s32 status;
    SOCKET socket;
    s32 nInterfaces;

    info.Address    = 0;
    info.Broadcast  = 0;
    info.Nameserver = 0;
    info.Netmask    = 0;
    info.IsAvailable= FALSE;

    socket = WSASocket(AF_INET,SOCK_DGRAM,0,0,0,0);
    if (socket == SOCKET_ERROR)
    {
        return;
    }
    status = WSAIoctl(socket,SIO_GET_INTERFACE_LIST,0,0,&InterfaceList,sizeof(InterfaceList),&nBytesReturned,0,0);
    closesocket(socket);
    if (status == SOCKET_ERROR)
    {
        return;
    }
    
    nInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
    if( nInterfaces <= 0 )
    {
        return;
    }

    if (id<0)
    {
        id = -1;
        for (s32 i=0; i<nInterfaces; i++)
        {
            if( (InterfaceList[i].iiFlags & IFF_LOOPBACK) == 0 )
            {
                id = i;
                break;
            }
        }
        // 
        // if we can't find any appropriate interfaces, let's just use the
        // loopback if it's present.
        //
        if (id < 0)
        {
            id=0;
        }
    }

    if( (id < 0) || (id >= nInterfaces) )
    {
        return;
    }

    info.Address    = ntohl(InterfaceList[id].iiAddress.AddressIn.sin_addr.S_un.S_addr);
    info.Netmask    = ntohl(InterfaceList[id].iiNetmask.AddressIn.sin_addr.S_un.S_addr);
    info.Broadcast  = (info.Address & info.Netmask) | ~info.Netmask;
    info.IsAvailable= TRUE;
}

//==============================================================================

s32 net_ResolveName( const char* pStr )
{
    struct addrinfo Hints;
    struct addrinfo* pResults = NULL;
    s32 Address = 0;

    x_memset( &Hints, 0, sizeof(Hints) );
    Hints.ai_family   = AF_INET;
    Hints.ai_socktype = SOCK_DGRAM;

    if( (getaddrinfo( pStr, NULL, &Hints, &pResults ) == 0) && pResults && pResults->ai_addr )
    {
        const sockaddr_in* pAddress = (const sockaddr_in*)pResults->ai_addr;
        Address = (s32)ntohl( pAddress->sin_addr.s_addr );
        freeaddrinfo( pResults );
    }

    return Address;
}

//==============================================================================

void net_ResolveIP( u32 IP, char* pStr )
{      
    net_address Dummy(IP,0);
    x_strcpy( pStr, Dummy.GetStrIP() );
}

//==============================================================================

void net_SetDialupInfo( char *pNumber, char *pUsername, char *pPassword )
{
}

//==============================================================================

void net_StartDial( s32 nRetries,s32 Timeout )
{
}

//==============================================================================

void net_ActivateConfig( xbool on )
{
    (void)on;
}

//==============================================================================

s32 net_GetConfigList( const char *pPath, net_config_list *pConfigList )
{
    (void)pPath;
    x_memset(pConfigList,0,sizeof(net_config_list));
    return 0;
}

//==============================================================================

s32 net_SetConfiguration( const char *pPath,s32 configindex )
{
    (void)pPath;
    (void)configindex;
    return 0;
}

//==============================================================================

s32 net_GetAttachStatus( s32 &InterfaceId )
{
    InterfaceId = 0;
    return ATTACH_STATUS_ATTACHED;
}

//==============================================================================

s32 net_GetSystemId( void )
{
    return( x_rand() );
}

//==============================================================================

void net_BeginConfig( void )
{
}

//==============================================================================

void net_EndConfig( void )
{
}

//==============================================================================

void net_GetConnectStatus( connect_status &Status )
{
    x_memset( &Status, 0, sizeof(Status) );
    Status.Status = CONNECT_STATUS_CONNECTED;
}

//==============================================================================

xbool net_socket::CanReceive( void )
{
    fd_set          fd;
    struct timeval  timeout;
    s32             rcode;

    if( m_Socket == BAD_SOCKET )
    {
        return FALSE;
    }

    // setup the fd set
    FD_ZERO(&fd);
    FD_SET((SOCKET)m_Socket, &fd);

    // setup the timeout
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // do the actual select
    rcode = select(FD_SETSIZE, &fd, NULL, NULL, &timeout);
    if((rcode == SOCKET_ERROR) || (rcode == 0))
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool net_socket::CanSend( void )
{
    fd_set          fd;
    struct timeval  timeout;
    s32             rcode;

    if( m_Socket == BAD_SOCKET )
    {
        return FALSE;
    }

    // setup the fd set
    FD_ZERO(&fd);
    FD_SET((SOCKET)m_Socket, &fd);

    // setup the timeout
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // do the actual select
    rcode = select(FD_SETSIZE, NULL, &fd, NULL, &timeout);
    if((rcode == SOCKET_ERROR) || (rcode == 0))
    {
        return FALSE;
    }

    return TRUE;
}
