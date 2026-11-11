#ifndef LAN_DISCOVERY_HPP
#define LAN_DISCOVERY_HPP

//==============================================================================

#include "MatchServer.hpp"
#include "x_array.hpp"

//==============================================================================

class lan_discovery
{
public:
    lan_discovery( net_socket& Socket );
    void  BeginHost( void );
    void  Start( void );
    void  Stop( void );
    void  Update( f32 DeltaTime );
    xbool SendLookup( const net_address& Remote );
    xbool ReceivePacket( const net_address& Remote, const void* pData,
                         s32 Length, const server_info* pHost );

    xbool IsSearching( void ) const
    {
        return m_Searching;
    }

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

private:
    struct query
    {
        u64   Token;
        xtick Sent;
        f32   Age;
    };

    struct server
    {
        server_info Info;
        u64         Session;
        u64         Token;
        f32         LastSeen;
    };

    net_socket&    m_Socket;
    xarray<query>  m_Queries;
    xarray<server> m_Servers;
    u64            m_Token;
    u64            m_Session;
    u32            m_Revision;
    s32            m_NextID;
    f32            m_Time;
    f32            m_NextLookup;
    f32            m_ResponseBudget;
    xbool          m_Searching;
};

//==============================================================================

#endif // LAN_DISCOVERY_HPP
