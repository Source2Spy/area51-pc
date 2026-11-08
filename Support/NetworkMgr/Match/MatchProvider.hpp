#ifndef MATCH_PROVIDER_HPP
#define MATCH_PROVIDER_HPP

//==============================================================================

#include "x_string.hpp"
#include "MatchServer.hpp"

const s32 MATCH_DIRECTORY_PORT = 3659;

//==============================================================================

enum match_capability
{
    MATCH_CAP_DIRECT_CONNECT = ( 1 << 0 ),
    MATCH_CAP_HOST           = ( 1 << 1 ),
    MATCH_CAP_SERVER_BROWSER = ( 1 << 2 ),
    MATCH_CAP_LAN_DISCOVERY  = ( 1 << 6 ),
    MATCH_CAP_DIRECTORY      = ( 1 << 7 ),
};

enum match_result
{
    MATCH_RESULT_OK,
    MATCH_RESULT_UNAVAILABLE,
    MATCH_RESULT_INVALID_ADDRESS,
    MATCH_RESULT_UNSUPPORTED,
    MATCH_RESULT_TIMEOUT,
    MATCH_RESULT_PENDING,
    MATCH_RESULT_NAME_NOT_FOUND,
};

struct match_join_target
{
    xstring Transport;
    xstring Endpoint;
};

class match_session
{
public:
    virtual ~match_session( void )
    {
    }

    virtual void               StartSearch( void )                               = 0;
    virtual void               StopSearch( void )                                = 0;
    virtual void               BeginHost( void )                                 = 0;
    virtual void               EndHost( void )                                   = 0;
    virtual void               Update( f32 DeltaTime, const server_info* pHost ) = 0;
    virtual xbool              ReceivePacket( const net_address& Remote, const void* pData, s32 Length,
                                              const server_info* pHost )         = 0;

    virtual xbool              IsSearching( void ) const                         = 0;
    virtual u32                GetRevision( void ) const                         = 0;
    virtual s32                GetServerCount( void ) const                      = 0;
    virtual const server_info& GetServerInfo( s32 Index ) const                  = 0;
    virtual match_result       GetResult( void ) const                           = 0;
    virtual xstring            GetDirectoryAddress( void ) const                 = 0;
    virtual match_result       ConfigureDirectory( const char* pAddress )        = 0;
};

class match_provider
{
public:
    virtual ~match_provider( void )
    {
    }

    virtual u32            GetCapabilities( void ) const                                           = 0;
    virtual match_result   ResolveTarget( const char* pEndpoint, match_join_target& Target ) const = 0;
    virtual match_session* CreateSession( net_socket& Socket, s32 Version ) const                  = 0;
};

match_provider& GetNativeMatchProvider( void );

//==============================================================================

#endif // MATCH_PROVIDER_HPP
