//==============================================================================
//
//  BuildAuthentication.hpp
//
//==============================================================================

#ifndef BUILD_AUTHENTICATION_HPP
#define BUILD_AUTHENTICATION_HPP

#include "x_files.hpp"
#include "e_Network.hpp"

//==============================================================================

const s32 BUILD_CHALLENGE_BYTES = 32;
const s32 BUILD_RESPONSE_BYTES  = 64;

class build_authentication
{
public:
    static xbool        HasKey          ( void );
    static xbool        CreateResponse  ( u64 ConnectionID, const byte* pChallenge, byte* pResponse );

    xbool               IssueChallenge  ( const net_address& Remote, u64 ConnectionID, byte* pChallenge );
    xbool               VerifyResponse  ( const net_address& Remote, u64 ConnectionID, const byte* pResponse ) const;
    void                Clear           ( void );

private:
    struct challenge
    {
        net_address     Remote;
        u64             ConnectionID;
        f64             Expires;
        byte            Nonce[BUILD_CHALLENGE_BYTES];
    };

    xarray<challenge>   m_Challenges;
};

//==============================================================================
#endif // BUILD_AUTHENTICATION_HPP
