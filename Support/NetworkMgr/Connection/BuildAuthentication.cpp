//==============================================================================
//
//  BuildAuthentication.cpp
//
//==============================================================================

#include "BuildAuthentication.hpp"
#include "BuildAuthenticationKey.hpp"
#include "ConnectionProtocol.hpp"
#include "NetworkMgr/ServerVersion.hpp"
#include "monocypher-ed25519.h"

//==============================================================================

xbool build_authentication::HasKey( void )
{
    return A51_HAS_BUILD_KEY;
}

//==============================================================================

xbool build_authentication::CreateResponse( u64 ConnectionID, const byte* pChallenge, byte* pResponse )
{
#if A51_HAS_BUILD_KEY
    const byte Domain[] = "Area51 build authentication v1";
    byte       Connection[12];

    for( s32 i = 0; i < 8; i++ )
        Connection[i] = (byte)(ConnectionID >> (i * 8));

    for( s32 i = 0; i < 4; i++ )
        Connection[8 + i] = (byte)((u32)g_ServerVersion >> (i * 8));

    crypto_sha512_hmac_ctx Context;
    crypto_sha512_hmac_init  ( &Context, s_BuildKey, sizeof(s_BuildKey) );
    crypto_sha512_hmac_update( &Context, Domain, sizeof(Domain) - 1 );
    crypto_sha512_hmac_update( &Context, Connection, sizeof(Connection) );
    crypto_sha512_hmac_update( &Context, pChallenge, BUILD_CHALLENGE_BYTES );
    crypto_sha512_hmac_final ( &Context, pResponse );

    return TRUE;
#else
    (void)ConnectionID;
    (void)pChallenge;
    (void)pResponse;
    return FALSE;
#endif
}

//==============================================================================

xbool build_authentication::IssueChallenge( const net_address& Remote, u64 ConnectionID, byte* pChallenge )
{
    if( !HasKey() )
        return FALSE;

    f64 Now = x_GetTimeSec();

    for( s32 i = m_Challenges.GetCount() - 1; i >= 0; i-- )
    {
        if( m_Challenges[i].Expires <= Now )
            m_Challenges.Delete( i );
    }

    for( s32 i = 0; i < m_Challenges.GetCount(); i++ )
    {
        const challenge& Challenge = m_Challenges[i];

        if( (Challenge.Remote == Remote) && (Challenge.ConnectionID == ConnectionID) )
        {
            x_memcpy( pChallenge, Challenge.Nonce, BUILD_CHALLENGE_BYTES );
            return TRUE;
        }
    }

    if( m_Challenges.GetCount() >= 64 )
        return FALSE;

    byte Nonce[BUILD_CHALLENGE_BYTES];

    for( s32 i = 0; i < BUILD_CHALLENGE_BYTES; i += 8 )
    {
        u64 Random;

        if( !CreateConnectionID( Random ) )
            return FALSE;

        x_memcpy( Nonce + i, &Random, sizeof(Random) );
    }

    challenge& Challenge = m_Challenges.Append();
    Challenge.Remote       = Remote;
    Challenge.ConnectionID = ConnectionID;
    Challenge.Expires      = Now + 10.0;
    x_memcpy( Challenge.Nonce, Nonce, sizeof(Nonce) );
    x_memcpy( pChallenge, Nonce, sizeof(Nonce) );

    return TRUE;
}

//==============================================================================

xbool build_authentication::VerifyResponse( const net_address& Remote, u64 ConnectionID, const byte* pResponse ) const
{
    f64 Now = x_GetTimeSec();

    for( s32 i = 0; i < m_Challenges.GetCount(); i++ )
    {
        const challenge& Challenge = m_Challenges[i];

        if( (Challenge.Remote == Remote) && (Challenge.ConnectionID == ConnectionID) &&
            (Challenge.Expires > Now) )
        {
            byte Expected[BUILD_RESPONSE_BYTES];

            if( !CreateResponse( ConnectionID, Challenge.Nonce, Expected ) )
                return FALSE;

            xbool Matches = (crypto_verify64( Expected, pResponse ) == 0);
            crypto_wipe( Expected, sizeof(Expected) );
            return Matches;
        }
    }

    return FALSE;
}

//==============================================================================

void build_authentication::Clear( void )
{
    m_Challenges.Clear();
}

//==============================================================================
