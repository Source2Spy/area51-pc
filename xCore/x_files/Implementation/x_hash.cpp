//==============================================================================
//
//  x_hash.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_hash.hpp"
#include "x_plus.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

static 
const u32 s_RoundConstants[64] =
{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

//==============================================================================
//  LOCAL FUNCTIONS
//==============================================================================

static
u32 Rotate( u32 Value, u32 Bits )
{
    return ( Value >> Bits ) | ( Value << ( 32 - Bits ) );
}

//==============================================================================

static
void Transform( u32 State[8], const byte* pBlock )
{
    // Expand the block into the message schedule.
    u32 Words[64];

    for( u32 i = 0; i < 16; ++i )
    {
        const byte* pWord = pBlock + i * 4;
        Words[i]          = ( static_cast<u32>( pWord[0] ) << 24 ) |
                            ( static_cast<u32>( pWord[1] ) << 16 ) |
                            ( static_cast<u32>( pWord[2] ) << 8 ) | pWord[3];
    }

    for( u32 i = 16; i < 64; ++i )
    {
        const u32 S0 = Rotate( Words[i - 15], 7 ) ^ Rotate( Words[i - 15], 18 ) ^ ( Words[i - 15] >> 3 );
        const u32 S1 = Rotate( Words[i - 2], 17 ) ^ Rotate( Words[i - 2], 19 ) ^ ( Words[i - 2] >> 10 );

        Words[i] = Words[i - 16] + S0 + Words[i - 7] + S1;
    }

    // Apply the 64 compression rounds.
    u32 A = State[0], B = State[1], C = State[2], D = State[3];
    u32 E = State[4], F = State[5], G = State[6], H = State[7];

    for( u32 i = 0; i < 64; ++i )
    {
        const u32 S1       = Rotate( E, 6 ) ^ Rotate( E, 11 ) ^ Rotate( E, 25 );
        const u32 Choose   = ( E & F ) ^ ( ~E & G );
        const u32 T1       = H + S1 + Choose + s_RoundConstants[i] + Words[i];
        const u32 S0       = Rotate( A, 2 ) ^ Rotate( A, 13 ) ^ Rotate( A, 22 );
        const u32 Majority = ( A & B ) ^ ( A & C ) ^ ( B & C );

        H = G;
        G = F;
        F = E;
        E = D + T1;
        D = C;
        C = B;
        B = A;
        A = T1 + S0 + Majority;
    }

    // Accumulate the compressed state.
    State[0] += A;
    State[1] += B;
    State[2] += C;
    State[3] += D;
    State[4] += E;
    State[5] += F;
    State[6] += G;
    State[7] += H;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

x_content_hash::x_content_hash( void )
    : m_nBytes( 0 ),
      m_nTailBytes( 0 )
{
    const u32 Initial[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                             0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    x_memcpy( m_State, Initial, sizeof( Initial ) );
}

//==============================================================================

void x_content_hash::Update( const byte* pData, u32 Size )
{
    ASSERT( pData || !Size );
    m_nBytes += Size;

    if( m_nTailBytes )
    {
        const u32 Count = MIN( Size, 64 - m_nTailBytes );

        if( Count )
        {
            x_memcpy( m_Tail + m_nTailBytes, pData, Count );
        }

        m_nTailBytes += Count;

        if( Count )
        {
            pData += Count;
        }

        Size -= Count;

        if( m_nTailBytes == 64 )
        {
            Transform( m_State, m_Tail );
            m_nTailBytes = 0;
        }
    }

    while( Size >= 64 )
    {
        Transform( m_State, pData );
        pData += 64;
        Size -= 64;
    }

    if( Size )
    {
        x_memcpy( m_Tail + m_nTailBytes, pData, Size );
        m_nTailBytes += Size;
    }
}

//==============================================================================

x_content_id x_content_hash::Finish( void ) const
{
    u32 State[8];

    x_memcpy( State, m_State, sizeof( State ) );

    byte Tail[128] = {};

    x_memcpy( Tail, m_Tail, m_nTailBytes );
    Tail[m_nTailBytes] = 0x80;

    const u32 TailSize = m_nTailBytes < 56 ? 64 : 128;
    const u64 Bits     = m_nBytes * 8;

    for( u32 i = 0; i < 8; ++i )
    {
        Tail[TailSize - 1 - i] = static_cast<byte>( Bits >> ( i * 8 ) );
    }

    Transform( State, Tail );

    if( TailSize == 128 )
    {
        Transform( State, Tail + 64 );
    }

    x_content_id Result;

    for( u32 i = 0; i < 8; ++i )
    {
        for( u32 j = 0; j < 4; ++j )
        {
            Result[i * 4 + j] = static_cast<byte>( State[i] >> ( 24 - j * 8 ) );
        }
    }

    return Result;
}

//==============================================================================

x_content_id x_HashContent( const byte* pData, u32 Size )
{
    x_content_hash Hash;

    Hash.Update( pData, Size );
    return Hash.Finish();
}
