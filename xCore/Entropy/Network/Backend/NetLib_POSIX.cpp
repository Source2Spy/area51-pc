//==============================================================================
//
//  NetLib_POSIX.cpp
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_POSIX
#error This file should only be compiled for POSIX platforms. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#if defined(TARGET_ANDROID)
#include <dlfcn.h>
#endif
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

static xbool net_IsWouldBlockError( s32 Error )
{
    return (Error == EAGAIN) || (Error == EWOULDBLOCK);
}

//==============================================================================

#if defined(TARGET_ANDROID)

// getifaddrs/freeifaddrs were introduced in API 24. Resolve them at runtime
// so the native library remains loadable on the API 21 minimum target.
typedef int  (*net_getifaddrs_fn)( struct ifaddrs** );
typedef void (*net_freeifaddrs_fn)( struct ifaddrs* );

static net_getifaddrs_fn s_GetIfAddrs = NULL;
static net_freeifaddrs_fn s_FreeIfAddrs = NULL;
static xbool s_InterfaceFunctionsResolved = FALSE;

//==============================================================================

static
xbool net_ResolveInterfaceFunctions( void )
{
    if( s_InterfaceFunctionsResolved )
    {
        return (s_GetIfAddrs != NULL) && (s_FreeIfAddrs != NULL);
    }

    s_InterfaceFunctionsResolved = TRUE;
    s_GetIfAddrs = (net_getifaddrs_fn)dlsym( RTLD_DEFAULT, "getifaddrs" );
    s_FreeIfAddrs = (net_freeifaddrs_fn)dlsym( RTLD_DEFAULT, "freeifaddrs" );
    return (s_GetIfAddrs != NULL) && (s_FreeIfAddrs != NULL);
}

//==============================================================================

static
xbool net_GetIfAddrs( struct ifaddrs** ppInterfaces )
{
    if( !net_ResolveInterfaceFunctions() )
    {
        return FALSE;
    }

    return (s_GetIfAddrs( ppInterfaces ) == 0);
}

//==============================================================================

static
void net_FreeIfAddrs( struct ifaddrs* pInterfaces )
{
    if( pInterfaces && net_ResolveInterfaceFunctions() )
    {
        s_FreeIfAddrs( pInterfaces );
    }
}

//==============================================================================

static
u32 net_GetFallbackLocalAddress( void )
{
    const s32 ProbeSocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if( ProbeSocket >= 0 )
    {
        sockaddr_in Remote;
        x_memset( &Remote, 0, sizeof(Remote) );
        Remote.sin_family      = AF_INET;
        Remote.sin_port        = htons( 53 );
        Remote.sin_addr.s_addr = htonl( 0x08080808 ); // 8.8.8.8; no packet is sent.

        if( connect( ProbeSocket, (const sockaddr*)&Remote, sizeof(Remote) ) == 0 )
        {
            sockaddr_in Local;
            socklen_t LocalLength = sizeof(Local);
            x_memset( &Local, 0, sizeof(Local) );
            if( getsockname( ProbeSocket, (sockaddr*)&Local, &LocalLength ) == 0 )
            {
                const u32 Address = ntohl( Local.sin_addr.s_addr );
                close( ProbeSocket );
                if( Address != 0 )
                {
                    return Address;
                }
            }
        }

        close( ProbeSocket );
    }

    return ntohl( INADDR_LOOPBACK );
}

#endif

//==============================================================================

static
u32 net_GetLocalAddress( void )
{
    struct ifaddrs* pInterfaces = NULL;
    struct ifaddrs* pInterface;
    u32 FirstAddress               = 0;
    u32 FirstRunningAddress        = 0;
    u32 FirstNonLoopbackAddress    = 0;
    u32 FirstNonLoopbackRunning    = 0;

#if defined(TARGET_ANDROID)
    if( !net_GetIfAddrs( &pInterfaces ) )
    {
        return net_GetFallbackLocalAddress();
    }
#else
    if( getifaddrs( &pInterfaces ) != 0 )
    {
        return 0;
    }
#endif

    for( pInterface = pInterfaces; pInterface; pInterface = pInterface->ifa_next )
    {
        if( !pInterface->ifa_addr || (pInterface->ifa_addr->sa_family != AF_INET) )
        {
            continue;
        }

        if( (pInterface->ifa_flags & IFF_UP) == 0 )
        {
            continue;
        }

        const sockaddr_in* pAddress = (const sockaddr_in*)pInterface->ifa_addr;
        const u32 Address = ntohl( pAddress->sin_addr.s_addr );
        if( Address == 0 )
        {
            continue;
        }

        if( FirstAddress == 0 )
        {
            FirstAddress = Address;
        }

        const xbool IsRunning = (pInterface->ifa_flags & IFF_RUNNING) != 0;
        if( IsRunning && (FirstRunningAddress == 0) )
        {
            FirstRunningAddress = Address;
        }

        if( (pInterface->ifa_flags & IFF_LOOPBACK) == 0 )
        {
            if( FirstNonLoopbackAddress == 0 )
            {
                FirstNonLoopbackAddress = Address;
            }

            if( IsRunning && (FirstNonLoopbackRunning == 0) )
            {
                FirstNonLoopbackRunning = Address;
            }
        }
    }

#if defined(TARGET_ANDROID)
    net_FreeIfAddrs( pInterfaces );
#else
    freeifaddrs( pInterfaces );
#endif

    if( FirstNonLoopbackRunning != 0 )
    {
        return FirstNonLoopbackRunning;
    }
    if( FirstNonLoopbackAddress != 0 )
    {
        return FirstNonLoopbackAddress;
    }
    if( FirstRunningAddress != 0 )
    {
        return FirstRunningAddress;
    }
    return FirstAddress;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

void sys_net_Init( void )
{
    ASSERT( !s_Inited );
    s_Inited = TRUE;

    s_STAT_NPacketsSent     = 0;
    s_STAT_NPacketsReceived = 0;
    s_STAT_NBytesSent       = 0;
    s_STAT_NBytesReceived   = 0;
    s_STAT_NAddressesBound  = 0;
}

//==============================================================================

void sys_net_Kill( void )
{
    ASSERT( s_Inited );
    s_Inited = FALSE;
}

//==============================================================================

xbool net_IsInited( void )
{
    return s_Inited;
}

//==============================================================================

xbool net_socket::Bind( s32 StartPort, s32 Flags )
{
    ASSERT( s_Inited );

    struct sockaddr_in Address;
    s32 Socket;

    if( StartPort <= 0 )
    {
        StartPort = x_irand( 8192, 16384 );
    }

    if( StartPort > 65535 )
    {
        return FALSE;
    }

    x_memset( &Address, 0, sizeof(Address) );
    Address.sin_family      = AF_INET;
    Address.sin_port        = htons( (u16)StartPort );
    Address.sin_addr.s_addr = htonl( INADDR_ANY );

    if( Flags & NET_FLAGS_VDP )
    {
        ASSERTS( FALSE, "NET_FLAGS_VDP is not supported on Linux" );
    }

    Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if( Socket < 0 )
    {
        return FALSE;
    }

    while( bind( Socket, (struct sockaddr*)&Address, sizeof(Address) ) < 0 )
    {
        if( (errno == EADDRINUSE) && (StartPort < 65535) && !(Flags & NET_FLAGS_BIND_EXACT) )
        {
            StartPort++;
            Address.sin_port = htons( (u16)StartPort );
        }
        else
        {
            close( Socket );
            return FALSE;
        }
    }

    if( (Flags & NET_FLAGS_BLOCKING) == 0 )
    {
        const s32 CurrentFlags = fcntl( Socket, F_GETFL, 0 );
        if( (CurrentFlags < 0) || (fcntl( Socket, F_SETFL, CurrentFlags | O_NONBLOCK ) < 0) )
        {
            close( Socket );
            return FALSE;
        }
    }

    const u32 LocalAddress = net_GetLocalAddress();
    if( LocalAddress == 0 )
    {
        close( Socket );
        return FALSE;
    }

    m_Address.Setup( (s32)LocalAddress, StartPort );
    m_Socket = (uaddr)Socket;

    if( Flags & NET_FLAGS_BROADCAST )
    {
        const s32 Broadcast = 1;
        setsockopt( Socket, SOL_SOCKET, SO_BROADCAST, &Broadcast, sizeof(Broadcast) );
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
        close( (s32)m_Socket );
        m_Socket = BAD_SOCKET;

        if( s_STAT_NAddressesBound > 0 )
        {
            s_STAT_NAddressesBound--;
        }
    }

    m_Address.Clear();
}

//==============================================================================

xbool sys_net_Receive( net_socket& Local,
                       net_address& Remote,
                       void* pBuffer,
                       s32& BufferSize )
{
    struct sockaddr_in From;
    socklen_t           AddressSize = sizeof(From);
    saddr               RetSize;

    ASSERT( s_Inited );

    RetSize = recvfrom( (s32)Local.m_Socket,
                        pBuffer,
                        (usize)BufferSize,
                        0,
                        (struct sockaddr*)&From,
                        &AddressSize );

    if( RetSize > 0 )
    {
        Remote.Setup( (s32)ntohl( From.sin_addr.s_addr ), (s32)ntohs( From.sin_port ) );
        ASSERT( RetSize <= BufferSize );
        BufferSize = (s32)RetSize;

        s_STAT_NPacketsReceived++;
        s_STAT_NBytesReceived += BufferSize;
        return TRUE;
    }

    if( (RetSize < 0) && !net_IsWouldBlockError( errno ) && (errno != ECONNRESET) )
    {
        x_DebugMsg( "RecvFrom returned an error %d\n", errno );
    }

    BufferSize = 0;
    return FALSE;
}

//==============================================================================

void sys_net_Send( net_socket& Local,
                   const net_address& Remote,
                   const void* pBuffer,
                   s32 BufferSize )
{
    struct sockaddr_in To;
    saddr              Status;

    ASSERT( s_Inited );

    s_STAT_NPacketsSent++;
    s_STAT_NBytesSent += BufferSize;

    x_memset( &To, 0, sizeof(To) );
    To.sin_family      = AF_INET;
    To.sin_port        = htons( (u16)Remote.GetPort() );
    To.sin_addr.s_addr = htonl( (u32)Remote.GetIP() );

    Status = sendto( (s32)Local.m_Socket,
                     pBuffer,
                     (usize)BufferSize,
                     0,
                     (struct sockaddr*)&To,
                     sizeof(To) );
    if( (Status < 0) && !net_IsWouldBlockError( errno ) )
    {
        x_DebugMsg( "SendTo returned an error code %d\n", errno );
    }
}

//==============================================================================

void net_GetInterfaceInfo( s32 ID, interface_info& Info )
{
    struct ifaddrs* pInterfaces = NULL;
    struct ifaddrs* pInterface;
    struct
    {
        u32 Address;
        u32 Netmask;
        xbool Loopback;
        xbool Running;
    } Interfaces[64];
    s32 Count = 0;
    s32 Selected = ID;

    x_memset( &Info, 0, sizeof(Info) );
    Info.IsAvailable = FALSE;

#if defined(TARGET_ANDROID)
    if( !net_GetIfAddrs( &pInterfaces ) )
    {
        Info.Address     = (s32)net_GetFallbackLocalAddress();
        Info.Netmask     = 0;
        Info.Broadcast   = 0;
        Info.Nameserver  = 0;
        Info.IsAvailable = (Info.Address != 0);
        return;
    }
#else
    if( getifaddrs( &pInterfaces ) != 0 )
    {
        return;
    }
#endif

    for( pInterface = pInterfaces; pInterface && (Count < 64); pInterface = pInterface->ifa_next )
    {
        if( !pInterface->ifa_addr || !pInterface->ifa_netmask ||
            (pInterface->ifa_addr->sa_family != AF_INET) )
        {
            continue;
        }

        if( (pInterface->ifa_flags & IFF_UP) == 0 )
        {
            continue;
        }

        const sockaddr_in* pAddress = (const sockaddr_in*)pInterface->ifa_addr;
        const sockaddr_in* pNetmask = (const sockaddr_in*)pInterface->ifa_netmask;
        Interfaces[Count].Address  = ntohl( pAddress->sin_addr.s_addr );
        Interfaces[Count].Netmask  = ntohl( pNetmask->sin_addr.s_addr );
        Interfaces[Count].Loopback = (pInterface->ifa_flags & IFF_LOOPBACK) != 0;
        Interfaces[Count].Running  = (pInterface->ifa_flags & IFF_RUNNING) != 0;
        Count++;
    }

#if defined(TARGET_ANDROID)
    net_FreeIfAddrs( pInterfaces );
#else
    freeifaddrs( pInterfaces );
#endif

    if( Count == 0 )
    {
        return;
    }

    if( Selected < 0 )
    {
        Selected = -1;
        for( s32 i = 0; i < Count; i++ )
        {
            if( !Interfaces[i].Loopback && Interfaces[i].Running )
            {
                Selected = i;
                break;
            }
        }

        if( Selected < 0 )
        {
            for( s32 i = 0; i < Count; i++ )
            {
                if( !Interfaces[i].Loopback )
                {
                    Selected = i;
                    break;
                }
            }
        }

        if( Selected < 0 )
        {
            Selected = 0;
        }
    }

    if( Selected >= Count )
    {
        return;
    }

    Info.Address     = (s32)Interfaces[Selected].Address;
    Info.Netmask     = (s32)Interfaces[Selected].Netmask;
    Info.Broadcast   = (s32)((u32)Info.Address | ~(u32)Info.Netmask);
    Info.Nameserver  = 0;
    Info.IsAvailable = TRUE;
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
    net_address Dummy( (s32)IP, 0 );
    x_strcpy( pStr, Dummy.GetStrIP() );
}

//==============================================================================

void net_SetDialupInfo( char* pNumber, char* pUsername, char* pPassword )
{
    (void)pNumber;
    (void)pUsername;
    (void)pPassword;
}

//==============================================================================

void net_StartDial( s32 nRetries, s32 Timeout )
{
    (void)nRetries;
    (void)Timeout;
}

//==============================================================================

void net_ActivateConfig( xbool On )
{
    (void)On;
}

//==============================================================================

s32 net_GetConfigList( const char* pPath, net_config_list* pConfigList )
{
    (void)pPath;
    x_memset( pConfigList, 0, sizeof(net_config_list) );
    return 0;
}

//==============================================================================

s32 net_SetConfiguration( const char* pPath, s32 ConfigIndex )
{
    (void)pPath;
    (void)ConfigIndex;
    return 0;
}

//==============================================================================

s32 net_GetAttachStatus( s32& InterfaceID )
{
    InterfaceID = 0;
    return ATTACH_STATUS_ATTACHED;
}

//==============================================================================

s32 net_GetSystemId( void )
{
    return x_rand();
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

void net_GetConnectStatus( connect_status& Status )
{
    x_memset( &Status, 0, sizeof(Status) );
    Status.Status = CONNECT_STATUS_CONNECTED;
}

//==============================================================================

xbool net_socket::CanReceive( void )
{
    const s32      Socket = (s32)m_Socket;
    struct pollfd   Poll;

    if( m_Socket == BAD_SOCKET )
    {
        return FALSE;
    }

    Poll.fd      = Socket;
    Poll.events  = POLLIN;
    Poll.revents = 0;

    return (poll( &Poll, 1, 0 ) > 0) && ((Poll.revents & POLLIN) != 0);
}

//==============================================================================

xbool net_socket::CanSend( void )
{
    const s32      Socket = (s32)m_Socket;
    struct pollfd   Poll;

    if( m_Socket == BAD_SOCKET )
    {
        return FALSE;
    }

    Poll.fd      = Socket;
    Poll.events  = POLLOUT;
    Poll.revents = 0;

    return (poll( &Poll, 1, 0 ) > 0) && ((Poll.revents & POLLOUT) != 0);
}
