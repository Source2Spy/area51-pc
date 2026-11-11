//==============================================================================
//
//  EndpointResolver.hpp
//
//==============================================================================

#ifndef ENDPOINT_RESOLVER_HPP
#define ENDPOINT_RESOLVER_HPP

#include "MatchProvider.hpp"
#include "x_ref.hpp"

const s32 MATCH_HOST_LENGTH     = 254;
const s32 MATCH_ENDPOINT_LENGTH = MATCH_HOST_LENGTH + 6;

//==============================================================================

struct match_endpoint
{
    char Host[MATCH_HOST_LENGTH];
    s32  Port;

    xbool Parse( const char* pEndpoint, s32 DefaultPort );
};

//==============================================================================

class endpoint_resolver
{
public:
    endpoint_resolver( void );
    ~endpoint_resolver( void );

    match_result Begin( const char* pEndpoint, s32 DefaultPort );
    match_result Poll( f32 DeltaTime, net_address& Address );
    void         Cancel( void );
    void         Kill( void );

private:
    struct request;
    struct worker;

    static void ResolveJob( void* pData );

    xref<worker>  m_pWorker;
    xref<request> m_pRequest;
    net_address   m_Address;
    match_result  m_Result;
    f32           m_Elapsed;
};

//==============================================================================
#endif // ENDPOINT_RESOLVER_HPP
