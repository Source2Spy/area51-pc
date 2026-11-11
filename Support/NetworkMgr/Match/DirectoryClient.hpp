#ifndef DIRECTORY_CLIENT_HPP
#define DIRECTORY_CLIENT_HPP

#include "MatchProvider.hpp"
#include "EndpointResolver.hpp"
#include "x_array.hpp"

//==============================================================================

class directory_client
{
public:
    directory_client( net_socket& Socket, s32 Version );
    match_result Configure( const char* pAddress );
    void         BeginHost( void );
    void         EndHost( void );
    void         Start( void );
    void         Stop( void );
    void         Update( f32 DeltaTime, const server_info* pHost );
    xbool        ReceivePacket( const net_address& Remote, const void* pData, s32 Length );

    u32 GetRevision( void ) const
    {
        return m_Revision;
    }

    s32 GetServerCount( void ) const
    {
        return m_Servers.GetCount();
    }

    const server_info& GetServerInfo( s32 Index ) const
    {
        return m_Servers[Index].Info;
    }

    match_result GetResult( void ) const
    {
        return m_Result;
    }

    const net_address& GetAddress( void ) const
    {
        return m_Address;
    }

    const xstring& GetEndpoint( void ) const
    {
        return m_Endpoint;
    }

private:
    struct request
    {
        byte  Body[512];
        byte  Cookie[16];
        s32   Length;
        s32   Type;
        s32   Attempts;
        u64   Token;
        f32   Deadline;
        xbool Active;
        xbool Confirmed;
    };

    struct server
    {
        server_info Info;
        u64         Session;
    };

    void BeginRequest( request& Request, s32 Type );
    void SendRequest( request& Request );
    void StartPage( void );
    void CommitServers( void );
    void Withdraw( void );

    net_socket&       m_Socket;
    net_address       m_Address;
    xstring           m_Endpoint;
    endpoint_resolver m_Resolver;
    xbool             m_ResolvePending;
    xbool             m_Resolving;
    s32               m_Version;
    u64               m_Session;
    u64               m_Generation;
    u32               m_Cursor;
    u32               m_Revision;
    s32               m_NextID;
    f32               m_Time;
    f32               m_NextPublish;
    f32               m_NextBrowse;
    f32               m_LastBrowse;
    byte              m_LeaseCookie[16];
    xbool             m_Registered;
    xbool             m_Hosting;
    xbool             m_Searching;
    request           m_Publish;
    request           m_Browse;
    xarray<server>    m_Servers;
    xarray<server>    m_Staging;
    match_result      m_Result;
};

//==============================================================================

#endif // DIRECTORY_CLIENT_HPP
