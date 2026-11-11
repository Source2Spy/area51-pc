//==============================================================================
//
//  a51_cloth_tessellate.hlsl
//
//==============================================================================

#include "common/shader_bindings.hlsl"

A51_VK_BINDING( 0, 0 ) ByteAddressBuffer Vertices : register( t0, space0 );
A51_VK_BINDING( 1, 0 ) ByteAddressBuffer Triangles : register( t1, space0 );
A51_VK_BINDING( 0, 1 ) RWByteAddressBuffer Output : register( u0, space1 );

A51_VK_BINDING( 0, 2 ) cbuffer Tessellation : register( b0, space2 )
{
    uint TriangleCount;
    uint Subdivisions;
    uint VertexStride;
    uint ColorOffset;
    uint NormalOffset;
    uint UVOffset;
    uint Padding0;
    uint Padding1;
    float4 BoundsMin;
    float4 BoundsMax;
};

//==============================================================================

float3 SafeNormal( float3 Normal )
{
    float LengthSquared = dot( Normal, Normal );
    return LengthSquared > 1e-12f ? Normal * rsqrt( LengthSquared ) : float3( 0, 0, 1 );
}

//==============================================================================

float3 EdgeControl( float3 A, float3 B, float3 Normal )
{
    return ( 2.0f * A + B - dot( B - A, Normal ) * Normal ) / 3.0f;
}

//==============================================================================

float3 EdgeNormal( float3 A, float3 B, float3 NA, float3 NB )
{
    float3 Edge = B - A;
    float3 Normal = NA + NB;
    float LengthSquared = dot( Edge, Edge );
    if( LengthSquared > 1e-12f )
    {
        Normal -= Edge * ( 2.0f * dot( Edge, Normal ) / LengthSquared );
    }
    return SafeNormal( Normal );
}

//==============================================================================

[numthreads( 64, 1, 1 )]
void CSMain( uint3 ThreadId : SV_DispatchThreadID )
{
    uint VerticesPerTriangle = ( Subdivisions + 1 ) * ( Subdivisions + 2 ) / 2;
    uint Triangle = ThreadId.x / VerticesPerTriangle;
    if( Triangle >= TriangleCount )
    {
        return;
    }

    uint LocalVertex = ThreadId.x % VerticesPerTriangle;
    uint Row = 0;
    while( LocalVertex >= Subdivisions + 1 - Row )
    {
        LocalVertex -= Subdivisions + 1 - Row;
        ++Row;
    }

    float V = float( Row ) / float( Subdivisions );
    float W = float( LocalVertex ) / float( Subdivisions );
    float U = 1.0f - V - W;
    uint3 Indices = Triangles.Load3( Triangle * 12 );
    uint3 Addresses = Indices * VertexStride;
    float3 P0 = asfloat( Vertices.Load3( Addresses.x ) );
    float3 P1 = asfloat( Vertices.Load3( Addresses.y ) );
    float3 P2 = asfloat( Vertices.Load3( Addresses.z ) );
    float3 N0 = SafeNormal( asfloat( Vertices.Load3( Addresses.x + NormalOffset ) ) );
    float3 N1 = SafeNormal( asfloat( Vertices.Load3( Addresses.y + NormalOffset ) ) );
    float3 N2 = SafeNormal( asfloat( Vertices.Load3( Addresses.z + NormalOffset ) ) );

    float3 B210 = EdgeControl( P0, P1, N0 );
    float3 B120 = EdgeControl( P1, P0, N1 );
    float3 B021 = EdgeControl( P1, P2, N1 );
    float3 B012 = EdgeControl( P2, P1, N2 );
    float3 B102 = EdgeControl( P2, P0, N2 );
    float3 B201 = EdgeControl( P0, P2, N0 );
    float3 EdgeAverage = ( B210 + B120 + B021 + B012 + B102 + B201 ) / 6.0f;
    float3 B111 = EdgeAverage + ( EdgeAverage - ( P0 + P1 + P2 ) / 3.0f ) * 0.5f;
    float3 Position = P0 * U * U * U + P1 * V * V * V + P2 * W * W * W +
        3.0f * ( B210 * U * U * V + B120 * U * V * V + B021 * V * V * W +
                 B012 * V * W * W + B102 * U * W * W + B201 * U * U * W ) +
        6.0f * B111 * U * V * W;
    Position = clamp( Position, BoundsMin.xyz, BoundsMax.xyz );
    float3 Normal = SafeNormal( N0 * U * U + N1 * V * V + N2 * W * W +
        2.0f * ( EdgeNormal( P0, P1, N0, N1 ) * U * V +
                 EdgeNormal( P1, P2, N1, N2 ) * V * W +
                 EdgeNormal( P2, P0, N2, N0 ) * W * U ) );

    float2 UV = asfloat( Vertices.Load2( Addresses.x + UVOffset ) ) * U +
                asfloat( Vertices.Load2( Addresses.y + UVOffset ) ) * V +
                asfloat( Vertices.Load2( Addresses.z + UVOffset ) ) * W;

    uint Address = ThreadId.x * VertexStride;
    for( uint Offset = 0; Offset < VertexStride; Offset += 4 )
    {
        Output.Store( Address + Offset, 0 );
    }
    Output.Store3( Address, asuint( Position ) );
    Output.Store3( Address + NormalOffset, asuint( Normal ) );
    Output.Store2( Address + UVOffset, asuint( UV ) );
    Output.Store( Address + ColorOffset, 0xff000000u );
}
