//==============================================================================
//
//  EndpointResolver.cpp
//
//==============================================================================

#include "EndpointResolver.hpp"
#include "x_threads.hpp"
#include "x_workers.hpp"

//==============================================================================

static const f32 DNS_LOOKUP_TIMEOUT = 10.0f;

//==============================================================================

xbool match_endpoint::Parse( const char* pEndpoint, s32 DefaultPort )
{
    if( !pEndpoint || !pEndpoint[0] || ( DefaultPort < 1 ) || ( DefaultPort > 65535 ) )
        return FALSE;

    net_address Numeric;

    if( Numeric.Parse( pEndpoint, DefaultPort ) )
    {
        x_strcpy( Host, Numeric.GetStrIP() );
        Port = Numeric.GetPort();
        return TRUE;
    }

    s32 Length = 0;

    while( pEndpoint[Length] && ( pEndpoint[Length] != ':' ) )
    {
        if( Length >= (s32)sizeof( Host ) - 1 )
            return FALSE;

        Length++;
    }

    if( Length == 0 )
        return FALSE;

    s32   LabelLength = 0;
    xbool HasLetter   = FALSE;

    for( s32 i = 0; i < Length; i++ )
    {
        char C = pEndpoint[i];

        if( C == '.' )
        {
            if( !LabelLength || ( pEndpoint[i - 1] == '-' ) )
                return FALSE;

            LabelLength = 0;
            continue;
        }

        xbool Letter = ( ( C >= 'a' ) && ( C <= 'z' ) ) || ( ( C >= 'A' ) && ( C <= 'Z' ) );

        if( !Letter && !( ( C >= '0' ) && ( C <= '9' ) ) && ( C != '-' ) )
            return FALSE;

        if( ( !LabelLength && ( C == '-' ) ) || ( ++LabelLength > 63 ) )
            return FALSE;

        HasLetter |= Letter;
    }

    if( !HasLetter || ( pEndpoint[Length - 1] == '-' ) )
        return FALSE;

    s32         ParsedPort = DefaultPort;
    const char* pPort      = pEndpoint + Length;

    if( *pPort == ':' )
    {
        pPort++;
        ParsedPort = 0;
        s32 Digits = 0;

        while( *pPort )
        {
            if( ( *pPort < '0' ) || ( *pPort > '9' ) || ( ++Digits > 5 ) )
                return FALSE;

            ParsedPort = ParsedPort * 10 + ( *pPort++ - '0' );

            if( ParsedPort > 65535 )
                return FALSE;
        }

        if( !Digits || !ParsedPort )
            return FALSE;
    }

    x_memcpy( Host, pEndpoint, Length );
    Host[Length] = '\0';
    Port         = ParsedPort;

    return TRUE;
}

//==============================================================================

struct endpoint_resolver::request : public xref_counted
{
    request( void )
    {
        x_AtomicInit( &Complete, 0 );
        x_AtomicInit( &Cancelled, 0 );
    }

    char         Host[MATCH_HOST_LENGTH];
    s32          Port;
    s32          Address;
    x_atomic_s32 Complete;
    x_atomic_s32 Cancelled;
};

struct endpoint_resolver::worker : public xref_counted
{
    worker( void ) : Running( FALSE )
    {
    }

    xmutex        Lock;
    xref<request> Pending;
    xbool         Running;
};

//==============================================================================

endpoint_resolver::endpoint_resolver( void )
{
    m_Result  = MATCH_RESULT_UNAVAILABLE;
    m_Elapsed = 0.0f;
}

//==============================================================================

endpoint_resolver::~endpoint_resolver( void )
{
    Kill();
}

//==============================================================================

void endpoint_resolver::ResolveJob( void* pData )
{
    worker* pWorker = (worker*)pData;

    for( ;; )
    {
        pWorker->Lock.Enter();
        xref<request> Request = pWorker->Pending;
        pWorker->Pending.Clear();

        if( !Request.Get() )
        {
            pWorker->Running = FALSE;
            pWorker->Lock.Exit();
            pWorker->ReleaseReference();
            return;
        }

        pWorker->Lock.Exit();

        if( !x_AtomicLoadAcquire( &Request->Cancelled ) )
        {
            Request->Address = net_ResolveName( Request->Host );
            x_AtomicStoreRelease( &Request->Complete, 1 );
        }
    }
}

//==============================================================================

match_result endpoint_resolver::Begin( const char* pEndpoint, s32 DefaultPort )
{
    Cancel();
    match_endpoint Endpoint;

    if( !Endpoint.Parse( pEndpoint, DefaultPort ) )
        return m_Result = MATCH_RESULT_INVALID_ADDRESS;

    if( m_Address.Parse( pEndpoint, DefaultPort ) )
        return m_Result = MATCH_RESULT_OK;

    if( !x_WorkersIsInit() )
        return m_Result = MATCH_RESULT_UNAVAILABLE;

    if( !m_pWorker.Get() )
        m_pWorker = new worker;

    m_pRequest = new request;
    x_strcpy( m_pRequest->Host, Endpoint.Host );
    m_pRequest->Port = Endpoint.Port;

    m_pWorker->Lock.Enter();
    m_pWorker->Pending = m_pRequest;

    if( !m_pWorker->Running )
    {
        m_pWorker->Running = TRUE;
        m_pWorker->AddReference();

        if( !x_WorkerJobSubmitDetached( ResolveJob, m_pWorker.Get(), "DNS lookup" ) )
        {
            m_pWorker->Running = FALSE;
            m_pWorker->Pending.Clear();
            m_pWorker->ReleaseReference();
            m_pWorker->Lock.Exit();
            m_pRequest.Clear();
            return m_Result = MATCH_RESULT_UNAVAILABLE;
        }
    }

    m_pWorker->Lock.Exit();
    m_Elapsed       = 0.0f;

    return m_Result = MATCH_RESULT_PENDING;
}

//==============================================================================

match_result endpoint_resolver::Poll( f32 DeltaTime, net_address& Address )
{
    if( m_Result == MATCH_RESULT_PENDING )
    {
        m_Elapsed += MAX( DeltaTime, 0.0f );

        if( x_AtomicLoadAcquire( &m_pRequest->Complete ) )
        {
            u32 IP = (u32)m_pRequest->Address;

            if( IP && ( ( IP >> 24 ) < 224 ) )
            {
                m_Address.Setup( (s32)IP, m_pRequest->Port );
                m_Result = MATCH_RESULT_OK;
            }
            else
                m_Result = MATCH_RESULT_NAME_NOT_FOUND;

            m_pRequest.Clear();
        }
        else if( m_Elapsed >= DNS_LOOKUP_TIMEOUT )
        {
            Cancel();
            m_Result = MATCH_RESULT_TIMEOUT;
        }
    }

    if( m_Result == MATCH_RESULT_OK )
        Address = m_Address;

    return m_Result;
}

//==============================================================================

void endpoint_resolver::Cancel( void )
{
    if( m_pRequest.Get() )
        x_AtomicStoreRelease( &m_pRequest->Cancelled, 1 );

    if( m_pWorker.Get() )
    {
        m_pWorker->Lock.Enter();

        if( m_pWorker->Pending.Get() == m_pRequest.Get() )
            m_pWorker->Pending.Clear();

        m_pWorker->Lock.Exit();
    }

    m_pRequest.Clear();
    m_Result  = MATCH_RESULT_UNAVAILABLE;
    m_Elapsed = 0.0f;
}

//==============================================================================

void endpoint_resolver::Kill( void )
{
    Cancel();
    m_pWorker.Clear();
}
