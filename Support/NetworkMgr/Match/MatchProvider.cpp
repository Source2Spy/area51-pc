//==============================================================================
//
//  MatchProvider.cpp
//
//==============================================================================

#include "MatchProvider.hpp"
#include "EndpointResolver.hpp"
#include "e_Network.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "LanDiscovery.hpp"
#include "DirectoryClient.hpp"
#include "x_plus.hpp"

//==============================================================================

static xstring s_DirectoryAddress;
static xbool   s_DirectoryOverride = FALSE;

class native_match_session : public match_session
{
public:
    native_match_session( net_socket& Socket, s32 Version ) : m_Lan( Socket ), m_Directory( Socket, Version )
    {
        m_Revision          = 0;
        m_LanRevision       = 0;
        m_DirectoryRevision = 0;
        m_NextID            = 1;
        m_Searching         = FALSE;
        m_Directory.Configure( s_DirectoryOverride ? (const char*)s_DirectoryAddress : x_getenv( "A51_MATCH_DIRECTORY" ) );
    }

    virtual void StartSearch( void )
    {
        m_Searching = TRUE;
        m_Lan.Start();
        m_Directory.Start();
    }

    virtual void StopSearch( void )
    {
        m_Searching = FALSE;
        m_Lan.Stop();
        m_Directory.Stop();
    }

    virtual void BeginHost( void )
    {
        StopSearch();
        m_Lan.BeginHost();
        m_Directory.BeginHost();
    }

    virtual void EndHost( void )
    {
        m_Directory.EndHost();
    }

    virtual void Update( f32 DeltaTime, const server_info* pHost )
    {
        m_Lan.Update( DeltaTime );
        m_Directory.Update( DeltaTime, pHost );

        if( ( m_LanRevision == m_Lan.GetRevision() ) &&
            ( m_DirectoryRevision == m_Directory.GetRevision() ) )
            return;

        xarray<server> Servers;

        // Merge LAN and directory results into one entry per remote address.
        for( s32 Source = 0; Source < 2; Source++ )
        {
            s32 Count = Source ? m_Directory.GetServerCount() : m_Lan.GetServerCount();

            for( s32 i = 0; ( i < Count ) && ( Servers.GetCount() < MAX_MATCH_RESULTS ); i++ )
            {
                const server_info& Info      = Source ? m_Directory.GetServerInfo( i ) : m_Lan.GetServerInfo( i );
                xbool              Duplicate = FALSE;

                for( s32 j = 0; j < Servers.GetCount(); j++ )
                {
                    if( Servers[j].Info.Remote == Info.Remote )
                    {
                        Servers[j].Info.Sources |= Info.Sources;
                        Duplicate = TRUE;
                    }
                }

                if( Duplicate )
                    continue;

                server Entry;
                Entry.Info     = Info;
                Entry.Source   = Source;
                Entry.SourceID = Info.ID;
                Entry.Info.ID  = 0;

                // Keep browser IDs stable for unchanged server entries.
                for( s32 j = 0; j < m_Servers.GetCount(); j++ )
                {
                    const server& Old = m_Servers[j];

                    if( ( Old.Info.Remote == Info.Remote ) && ( Old.Source == Source ) &&
                        ( Old.SourceID == Info.ID ) )
                        Entry.Info.ID = Old.Info.ID;
                }

                if( !Entry.Info.ID )
                    Entry.Info.ID = m_NextID++;

                Servers.Append( Entry );
            }
        }

        m_Servers           = Servers;
        m_LanRevision       = m_Lan.GetRevision();
        m_DirectoryRevision = m_Directory.GetRevision();
        m_Revision++;
    }

    virtual xbool ReceivePacket( const net_address& Remote, const void* pData, s32 Length,
                                 const server_info* pHost )
    {
        return m_Directory.ReceivePacket( Remote, pData, Length ) ||
               m_Lan.ReceivePacket( Remote, pData, Length, pHost );
    }

    virtual xbool IsSearching( void ) const
    {
        return m_Searching;
    }

    virtual u32 GetRevision( void ) const
    {
        return m_Revision;
    }

    virtual s32 GetServerCount( void ) const
    {
        return m_Servers.GetCount();
    }

    virtual const server_info& GetServerInfo( s32 Index ) const
    {
        return m_Servers[Index].Info;
    }

    virtual match_result GetResult( void ) const
    {
        return m_Directory.GetResult();
    }

    virtual xstring GetDirectoryAddress( void ) const
    {
        return m_Directory.GetEndpoint();
    }

    virtual match_result ConfigureDirectory( const char* pAddress )
    {
        xbool Searching = IsSearching();
        StopSearch();
        match_result Result = m_Directory.Configure( pAddress );

        if( Result == MATCH_RESULT_OK )
        {
            s_DirectoryAddress  = GetDirectoryAddress();
            s_DirectoryOverride = TRUE;
        }

        if( Searching )
            StartSearch();

        return Result;
    }

private:
    struct server
    {
        server_info Info;
        s32         Source;
        s32         SourceID;
    };

    lan_discovery    m_Lan;
    directory_client m_Directory;
    xarray<server>   m_Servers;
    u32              m_Revision;
    u32              m_LanRevision;
    u32              m_DirectoryRevision;
    s32              m_NextID;
    xbool            m_Searching;
};

//==============================================================================

class native_match_provider : public match_provider
{
public:
    virtual u32 GetCapabilities( void ) const
    {
        return MATCH_CAP_DIRECT_CONNECT | MATCH_CAP_HOST |
               MATCH_CAP_SERVER_BROWSER | MATCH_CAP_LAN_DISCOVERY | MATCH_CAP_DIRECTORY;
    }

    virtual match_result ResolveTarget( const char* pEndpoint, match_join_target& Target ) const
    {
        match_endpoint Address;

        if( !Address.Parse( pEndpoint, NET_GAME_PORT ) )
            return MATCH_RESULT_INVALID_ADDRESS;

        Target.Transport = "udp";
        Target.Endpoint  = xfs( "%s:%d", Address.Host, Address.Port );
        return MATCH_RESULT_OK;
    }

    virtual match_session* CreateSession( net_socket& Socket, s32 Version ) const
    {
        return new native_match_session( Socket, Version );
    }
};

//==============================================================================

match_provider& GetNativeMatchProvider( void )
{
    static native_match_provider Provider;

    return Provider;
}
