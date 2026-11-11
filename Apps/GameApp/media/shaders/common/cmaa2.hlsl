//==============================================================================
//
//  CMAA2 2.3, adapted for Area 51 / SDL GPU.
//  Copyright (c) 2018, Intel Corporation. Licensed under Apache-2.0.
//  Color packing: Copyright (c) Microsoft. Licensed under MIT.
//  License texts: thirdpartylegalnotices.txt
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  DEFINES
//==============================================================================

#if defined( __spirv__ )
    #define CMAA2_IMAGE_FORMAT( F ) [[vk::image_format( F )]]
#else
    #define CMAA2_IMAGE_FORMAT( F )
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_VK_BINDING( 0, 2 ) cbuffer Cmaa2Constants : register( b0, space2 )
{
    uint2 ImageSize;
    uint CandidateCapacity;
    uint BlendCapacity;
    uint LocationCapacity;
    uint ArgsMode;
    uint2 Padding;
};

//------------------------------------------------------------------------------

#if defined( CMAA2_PASS_EDGES ) || defined( CMAA2_PASS_CANDIDATES ) || defined( CMAA2_PASS_APPLY )
    A51_SAMPLED_TEXTURE_ATTR( 0, 0 ) Texture2D<float4> g_inoutColorReadonly A51_SAMPLED_TEXTURE_BIND( 0, 0 );
    A51_SAMPLER_ATTR( 0, 0 ) SamplerState SourceSampler A51_SAMPLER_BIND( 0, 0 );
#endif

#if defined( CMAA2_PASS_INIT )
    A51_VK_BINDING( 0, 1 ) RWByteAddressBuffer g_workingControlBuffer : register( u0, space1 );
#elif defined( CMAA2_PASS_EDGES )
    CMAA2_IMAGE_FORMAT( "rgba8" )
    A51_VK_BINDING( 0, 1 ) RWTexture2D<unorm float4> g_inoutColorWriteonly : register( u0, space1 );
    CMAA2_IMAGE_FORMAT( "r8" ) A51_VK_BINDING( 1, 1 ) RWTexture2D<unorm float> g_workingEdges : register( u1, space1 );
    A51_VK_BINDING( 2, 1 ) RWStructuredBuffer<uint> g_workingDeferredBlendItemListHeads : register( u2, space1 );
    A51_VK_BINDING( 3, 1 ) RWStructuredBuffer<uint> g_workingShapeCandidates : register( u3, space1 );
    A51_VK_BINDING( 4, 1 ) RWByteAddressBuffer g_workingControlBuffer : register( u4, space1 );
#elif defined( CMAA2_PASS_ARGS )
    A51_VK_BINDING( 0, 1 ) RWByteAddressBuffer g_workingControlBuffer : register( u0, space1 );
    A51_VK_BINDING( 1, 1 ) RWByteAddressBuffer g_workingExecuteIndirectBuffer : register( u1, space1 );
#elif defined( CMAA2_PASS_CANDIDATES )
    A51_SAMPLED_TEXTURE_ATTR( 1, 1 ) Texture2D<float> g_workingEdges A51_SAMPLED_TEXTURE_BIND( 1, 1 );
    A51_SAMPLER_ATTR( 1, 1 ) SamplerState EdgesSampler A51_SAMPLER_BIND( 1, 1 );
    A51_VK_BINDING( 2, 0 ) StructuredBuffer<uint> g_workingShapeCandidates : register( t2, space0 );
    A51_VK_BINDING( 0, 1 ) RWStructuredBuffer<uint> g_workingDeferredBlendItemListHeads : register( u0, space1 );
    A51_VK_BINDING( 1, 1 ) RWStructuredBuffer<uint2> g_workingDeferredBlendItemList : register( u1, space1 );
    A51_VK_BINDING( 2, 1 ) RWStructuredBuffer<uint> g_workingDeferredBlendLocationList : register( u2, space1 );
    A51_VK_BINDING( 3, 1 ) RWByteAddressBuffer g_workingControlBuffer : register( u3, space1 );
    groupshared uint g_groupSharedBlendItemCount;
    groupshared uint2 g_groupSharedBlendItems[768];
    static const uint c_maxLineLength = 86;
    static const float c_symmetryCorrectionOffset = 0.22f;
    static const float c_dampeningEffect = 0.15f;
#elif defined( CMAA2_PASS_APPLY )
    A51_VK_BINDING( 1, 0 ) StructuredBuffer<uint> g_workingDeferredBlendItemListHeads : register( t1, space0 );
    A51_VK_BINDING( 2, 0 ) StructuredBuffer<uint2> g_workingDeferredBlendItemList : register( t2, space0 );
    A51_VK_BINDING( 3, 0 ) StructuredBuffer<uint> g_workingDeferredBlendLocationList : register( t3, space0 );
    A51_VK_BINDING( 4, 0 ) ByteAddressBuffer g_workingControlBuffer : register( t4, space0 );
    CMAA2_IMAGE_FORMAT( "rgba8" )
    A51_VK_BINDING( 0, 1 ) RWTexture2D<unorm float4> g_inoutColorWriteonly : register( u0, space1 );
#endif

#if defined( CMAA2_PASS_EDGES ) || defined( CMAA2_PASS_CANDIDATES ) || defined( CMAA2_PASS_APPLY )
uint BlendHeadAddress( uint2 QuadPosition )
{
    return QuadPosition.y * ( ( ImageSize.x + 1 ) / 2 ) + QuadPosition.x;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
    groupshared float4 g_groupShared2x2FracEdgesH[256];
    groupshared float4 g_groupShared2x2FracEdgesV[256];
#endif

//==============================================================================
//  FUNCTIONS
//==============================================================================

#if defined( CMAA2_PASS_INIT )
[numthreads( 1, 1, 1 )]
void InitializeCS( uint3 ThreadId : SV_DispatchThreadID )
{
    for( uint i = 0; i < 16; ++i )
    {
        g_workingControlBuffer.Store( i * 4, 0 );
    }
    g_workingControlBuffer.Store2( 0, ImageSize );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES ) || defined( CMAA2_PASS_EDGES )
float3 DecodeColor( float3 Color )
{
    return lerp( Color / 12.92f, pow( ( Color + 0.055f ) / 1.055f, 2.4f ), step( 0.04045f, Color ) );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES ) || defined( CMAA2_PASS_EDGES )
float3 LoadSourceColor( uint2 pixelPos, int2 offset )
{
    int2 Position = clamp( int2( pixelPos ) + offset, int2( 0, 0 ), int2( ImageSize ) - 1 );
    return DecodeColor( g_inoutColorReadonly.Load( int3( Position, 0 ) ).rgb );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
float RGBToLumaForEdges( float3 linearRGB )
{

    float luma = dot( sqrt( linearRGB.rgb ), float3( 0.299, 0.587, 0.114 ) );

    return luma;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
float2 ComputeEdgeLuma( int x, int y, float pixelLumas[3 * 3 - 1] )
{
    float2 temp;
    temp.x = abs( pixelLumas[x + y * 3] - pixelLumas[x + 1 + y * 3] );
    temp.y = abs( pixelLumas[x + y * 3] - pixelLumas[x + ( y + 1 ) * 3] );
    return temp;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
void GroupsharedLoadQuadHV( uint addr, out float2 e00, out float2 e10, out float2 e01, out float2 e11 )
{
    float4 valH = g_groupShared2x2FracEdgesH[addr];
    e00.y       = valH.x;
    e10.y       = valH.y;
    e01.y       = valH.z;
    e11.y       = valH.w;
    float4 valV = g_groupShared2x2FracEdgesV[addr];
    e00.x       = valV.x;
    e10.x       = valV.y;
    e01.x       = valV.z;
    e11.x       = valV.w;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
float ComputeLocalContrastH( int x, int y, in float2 neighbourhood[4][4] )
{

    return max( max( neighbourhood[x + 0][y + 1].x, neighbourhood[x + 1][y + 1].x ),
                max( neighbourhood[x + 0][y + 2].x, neighbourhood[x + 1][y + 2].x ) ) *
           0.10f;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
float GetActualEdgeThreshold()
{
    float retVal = float( 0.05 );
    return retVal;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
float ComputeLocalContrastV( int x, int y, in float2 neighbourhood[4][4] )
{

    return max( max( neighbourhood[x + 1][y + 0].y, neighbourhood[x + 1][y + 1].y ),
                max( neighbourhood[x + 2][y + 0].y, neighbourhood[x + 2][y + 1].y ) ) *
           0.10f;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
uint PackEdges( float4 edges )
{
    return (uint)dot( edges, float4( 1, 2, 4, 8 ) );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_EDGES )
[numthreads( 16, 16, 1 )]
void EdgesColor2x2CS( uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID )
{
    uint2 pixelPos = groupID.xy * int2( ( 16 - 2 ), ( 16 - 2 ) ) + groupThreadID.xy - int2( 1, 1 );
    pixelPos *= int2( 2, 2 );

    const uint2 qeOffsets[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
    const uint rowStride2x2  = 16;
    const uint centerAddr2x2 = groupThreadID.x + groupThreadID.y * rowStride2x2;

    const bool inOutputKernel =
        all( pixelPos < ImageSize ) && !any( bool4( groupThreadID.x == ( 16 - 1 ), groupThreadID.x == 0,
                                                    groupThreadID.y == ( 16 - 1 ), groupThreadID.y == 0 ) );

    uint i;
    float2 qe0, qe1, qe2, qe3;
    uint4 outEdges = { 0, 0, 0, 0 };

    {

        float pixelLumas[3 * 3 - 1];
        [unroll] for( i = 0; i < 3 * 3 - 1; i++ )
        {
            float3 color  = LoadSourceColor( pixelPos, int2( i % 3, i / 3 ) ).rgb;
            pixelLumas[i] = RGBToLumaForEdges( color );
        }
        qe0 = ComputeEdgeLuma( 0, 0, pixelLumas );
        qe1 = ComputeEdgeLuma( 1, 0, pixelLumas );
        qe2 = ComputeEdgeLuma( 0, 1, pixelLumas );
        qe3 = ComputeEdgeLuma( 1, 1, pixelLumas );

        g_groupShared2x2FracEdgesV[centerAddr2x2 + rowStride2x2 * 0] = float4( qe0.x, qe1.x, qe2.x, qe3.x );
        g_groupShared2x2FracEdgesH[centerAddr2x2 + rowStride2x2 * 0] = float4( qe0.y, qe1.y, qe2.y, qe3.y );

        GroupMemoryBarrierWithGroupSync();

        [branch] if( inOutputKernel )
        {
            float2 topRow     = g_groupShared2x2FracEdgesH[centerAddr2x2 - rowStride2x2].zw;
            float2 leftColumn = g_groupShared2x2FracEdgesV[centerAddr2x2 - 1].yw;

            bool someNonZeroEdges = any( float4( qe0, qe1 ) + float4( qe2, qe3 ) +
                                         float4( topRow[0], topRow[1], leftColumn[0], leftColumn[1] ) );

            [branch] if( someNonZeroEdges )
            {

                float4 ce[4];

                float2 dummyd0, dummyd1, dummyd2;
                float2 neighbourhood[4][4];

                GroupsharedLoadQuadHV( centerAddr2x2 - rowStride2x2 - 1, dummyd0, dummyd1, dummyd2,
                                       neighbourhood[0][0] );
                GroupsharedLoadQuadHV( centerAddr2x2 - rowStride2x2, dummyd0, dummyd1, neighbourhood[1][0],
                                       neighbourhood[2][0] );
                GroupsharedLoadQuadHV( centerAddr2x2 - rowStride2x2 + 1, dummyd0, dummyd1, neighbourhood[3][0],
                                       dummyd2 );
                GroupsharedLoadQuadHV( centerAddr2x2 - 1, dummyd0, neighbourhood[0][1], dummyd1, neighbourhood[0][2] );
                GroupsharedLoadQuadHV( centerAddr2x2 + 1, neighbourhood[3][1], dummyd0, neighbourhood[3][2], dummyd1 );
                GroupsharedLoadQuadHV( centerAddr2x2 - 1 + rowStride2x2, dummyd0, neighbourhood[0][3], dummyd1,
                                       dummyd2 );
                GroupsharedLoadQuadHV( centerAddr2x2 + rowStride2x2, neighbourhood[1][3], neighbourhood[2][3], dummyd0,
                                       dummyd1 );
                neighbourhood[1][0].y = topRow[0];
                neighbourhood[2][0].y = topRow[1];
                neighbourhood[0][1].x = leftColumn[0];
                neighbourhood[0][2].x = leftColumn[1];
                neighbourhood[1][1]   = qe0;
                neighbourhood[2][1]   = qe1;
                neighbourhood[1][2]   = qe2;
                neighbourhood[2][2]   = qe3;

                topRow[0] = ( topRow[0] - ComputeLocalContrastH( 0, -1, neighbourhood ) ) > GetActualEdgeThreshold();
                topRow[1] = ( topRow[1] - ComputeLocalContrastH( 1, -1, neighbourhood ) ) > GetActualEdgeThreshold();
                leftColumn[0] =
                    ( leftColumn[0] - ComputeLocalContrastV( -1, 0, neighbourhood ) ) > GetActualEdgeThreshold();
                leftColumn[1] =
                    ( leftColumn[1] - ComputeLocalContrastV( -1, 1, neighbourhood ) ) > GetActualEdgeThreshold();

                ce[0].x = ( qe0.x - ComputeLocalContrastV( 0, 0, neighbourhood ) ) > GetActualEdgeThreshold();
                ce[0].y = ( qe0.y - ComputeLocalContrastH( 0, 0, neighbourhood ) ) > GetActualEdgeThreshold();
                ce[1].x = ( qe1.x - ComputeLocalContrastV( 1, 0, neighbourhood ) ) > GetActualEdgeThreshold();
                ce[1].y = ( qe1.y - ComputeLocalContrastH( 1, 0, neighbourhood ) ) > GetActualEdgeThreshold();
                ce[2].x = ( qe2.x - ComputeLocalContrastV( 0, 1, neighbourhood ) ) > GetActualEdgeThreshold();
                ce[2].y = ( qe2.y - ComputeLocalContrastH( 0, 1, neighbourhood ) ) > GetActualEdgeThreshold();
                ce[3].x = ( qe3.x - ComputeLocalContrastV( 1, 1, neighbourhood ) ) > GetActualEdgeThreshold();
                ce[3].y = ( qe3.y - ComputeLocalContrastH( 1, 1, neighbourhood ) ) > GetActualEdgeThreshold();

                ce[0].z = leftColumn[0];
                ce[1].z = ce[0].x;
                ce[2].z = leftColumn[1];
                ce[3].z = ce[2].x;

                ce[0].w = topRow[0];
                ce[1].w = topRow[1];
                ce[2].w = ce[0].y;
                ce[3].w = ce[1].y;

                [unroll] for( i = 0; i < 4; i++ )
                {
                    const uint2 localPixelPos = pixelPos + qeOffsets[i];

                    const float4 edges = ce[i];

                    bool isCandidate =
                        ( edges.x * edges.y + edges.y * edges.z + edges.z * edges.w + edges.w * edges.x ) != 0;
                    if( isCandidate && all( localPixelPos < ImageSize ) )
                    {
                        uint counterIndex;
                        g_workingControlBuffer.InterlockedAdd( 4 * 4, 1, counterIndex );
                        g_workingShapeCandidates[counterIndex] =
                            ( localPixelPos.x << 18 ) | localPixelPos.y;
                    }

                    uint packedEdge = PackEdges( edges );
                    outEdges[i]     = packedEdge;
                }
            }
        }
    }

    [branch] if( inOutputKernel )
    {
        g_workingEdges[int2( pixelPos.x / 2, pixelPos.y + 0 )] = float( ( outEdges[1] << 4 ) | outEdges[0] ) / 255.0f;
        if( pixelPos.y + 1 < ImageSize.y )
        {
            g_workingEdges[int2( pixelPos.x / 2, pixelPos.y + 1 )] = float( ( outEdges[3] << 4 ) | outEdges[2] ) / 255.0f;
        }

        g_workingDeferredBlendItemListHeads[BlendHeadAddress( pixelPos / 2 )] = 0xffffffffu;
        for( uint Pixel = 0; Pixel < 4; ++Pixel )
        {
            uint2 Position = pixelPos + qeOffsets[Pixel];
            if( all( Position < ImageSize ) )
            {
                g_inoutColorWriteonly[Position] = g_inoutColorReadonly.Load( int3( Position, 0 ) );
            }
        }
    }
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_ARGS )
    [numthreads( 1, 1, 1 )]
void ComputeDispatchArgsCS( uint3 ThreadId : SV_DispatchThreadID )
{
    uint Count = g_workingControlBuffer.Load( ArgsMode == 0 ? 16 : 32 );
    Count      = min( Count, ArgsMode == 0 ? CandidateCapacity : LocationCapacity );
    if( g_workingControlBuffer.Load( 52 ) != 0 )
    {
        Count = 0;
    }

    uint Threads = ArgsMode == 0 ? 128 : 32;
    uint Groups  = max( ( Count + Threads - 1 ) / Threads, 1u );
    uint GroupsX = min( Groups, 65535u );
    uint GroupsY = ( Groups + GroupsX - 1 ) / GroupsX;
    g_workingExecuteIndirectBuffer.Store3( 0, uint3( GroupsX, GroupsY, 1 ) );
    g_workingControlBuffer.Store( 12, Count );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
uint LoadEdge( int2 pixelPos, int2 offset )
{
    int2 Position = pixelPos + offset;
    if( any( Position < 0 ) || any( Position >= int2( ImageSize ) ) )
    {
        return 0;
    }

    uint Packed = uint( g_workingEdges.Load( int3( Position.x / 2, Position.y, 0 ) ) * 255.0f + 0.5f );
    return ( Packed >> ( ( Position.x & 1 ) * 4 ) ) & 15;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
float4 UnpackEdgesFlt( uint value )
{
    float4 ret;
    ret.x = ( value & 0x01 ) != 0;
    ret.y = ( value & 0x02 ) != 0;
    ret.z = ( value & 0x04 ) != 0;
    ret.w = ( value & 0x08 ) != 0;
    return ret;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
float4 ComputeSimpleShapeBlendValues( float4 edges, float4 edgesLeft, float4 edgesRight, float4 edgesTop,
                                      float4 edgesBottom, uniform bool dontTestShapeValidity )
{

    float fromRight = edges.r;
    float fromBelow = edges.g;
    float fromLeft  = edges.b;
    float fromAbove = edges.a;

    float blurCoeff = 0.10f;

    float numberOfEdges = dot( edges, float4( 1, 1, 1, 1 ) );

    float numberOfEdgesAllAround =
        dot( edgesLeft.bga + edgesRight.rga + edgesTop.rba + edgesBottom.rgb, float3( 1, 1, 1 ) );

    if( !dontTestShapeValidity )
    {

        if( numberOfEdges == 1 )
        {
            blurCoeff = 0;
        }

        if( numberOfEdges == 2 )
        {
            blurCoeff *= ( ( float( 1.0 ) - fromBelow * fromAbove ) * ( float( 1.0 ) - fromRight * fromLeft ) );
        }
    }

    if( numberOfEdges == 2 )
    {
        blurCoeff *= 0.75;

        float k = 0.9f;
        fromRight +=
            k * ( edges.g * edgesTop.r * ( 1.0 - edgesLeft.g ) + edges.a * edgesBottom.r * ( 1.0 - edgesLeft.a ) );
        fromBelow +=
            k * ( edges.b * edgesRight.g * ( 1.0 - edgesTop.b ) + edges.r * edgesLeft.g * ( 1.0 - edgesTop.r ) );
        fromLeft +=
            k * ( edges.a * edgesBottom.b * ( 1.0 - edgesRight.a ) + edges.g * edgesTop.b * ( 1.0 - edgesRight.g ) );
        fromAbove +=
            k * ( edges.r * edgesLeft.a * ( 1.0 - edgesBottom.r ) + edges.b * edgesRight.a * ( 1.0 - edgesBottom.b ) );
    }

    blurCoeff *= saturate( 1.30 - numberOfEdgesAllAround / 10.0 );

    return float4( fromLeft, fromAbove, fromRight, fromBelow ) * blurCoeff;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
uint Pack_R11G11B10_E4_FLOAT( float3 rgb )
{

    rgb    = clamp( rgb, 0.0, asfloat( 0x3FFFFFFF ) );
    uint r = ( ( f32tof16( rgb.r ) + 4 ) >> 3 ) & 0x000007FF;
    uint g = ( ( f32tof16( rgb.g ) + 4 ) << 8 ) & 0x003FF800;
    uint b = ( ( f32tof16( rgb.b ) + 8 ) << 18 ) & 0xFFC00000;
    return r | g | b;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
uint InternalPackColor( float3 color )
{
    return Pack_R11G11B10_E4_FLOAT( color );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
void StoreColorSample( uint2 pixelPos, float3 color, bool isComplexShape )
{
    if( any( pixelPos >= ImageSize ) )
    {
        return;
    }

    uint counterIndex;
    g_workingControlBuffer.InterlockedAdd( 4 * 12, 1, counterIndex );
    if( counterIndex >= BlendCapacity )
    {
        uint Previous;
        g_workingControlBuffer.InterlockedOr( 52, 1, Previous );
        return;
    }

    uint2 quadPos = pixelPos / uint2( 2, 2 );

    uint offsetXY = ( pixelPos.y % 2 ) * 2 + ( pixelPos.x % 2 );

    uint header = ( offsetXY << 30 ) | ( isComplexShape << 26 );

    uint counterIndexWithHeader = counterIndex | header;

    uint originalIndex;
    InterlockedExchange( g_workingDeferredBlendItemListHeads[BlendHeadAddress( quadPos )], counterIndexWithHeader, originalIndex );
    g_workingDeferredBlendItemList[counterIndex] = uint2( originalIndex, InternalPackColor( color ) );

    if( originalIndex == 0xFFFFFFFF )
    {

        uint edgeListCounter;
        g_workingControlBuffer.InterlockedAdd( 4 * 8, 1, edgeListCounter );
        g_workingDeferredBlendLocationList[edgeListCounter] = ( quadPos.x << 16 ) | quadPos.y;
    }
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
void DetectZsHorizontal( in float4 edges, in float4 edgesM1P0, in float4 edgesP1P0, in float4 edgesP2P0,
                         out float invertedZScore, out float normalZScore )
{

    {
        invertedZScore = edges.r * edges.g * edgesP1P0.a;
        invertedZScore *= 2.0 + ( ( edgesM1P0.g + edgesP2P0.a ) ) - ( edges.a + edgesP1P0.g ) -
                          0.7 * ( edgesP2P0.g + edgesM1P0.a + edges.b + edgesP1P0.r );
    }

    {
        normalZScore = edges.r * edges.a * edgesP1P0.g;
        normalZScore *= 2.0 + ( ( edgesM1P0.a + edgesP2P0.g ) ) - ( edges.g + edgesP1P0.a ) -
                        0.7 * ( edgesP2P0.a + edgesM1P0.g + edges.b + edgesP1P0.r );
    }
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
void FindZLineLengths( out float lineLengthLeft, out float lineLengthRight, uint2 screenPos, uniform bool horizontal,
                       uniform bool invertedZShape, const float2 stepRight )
{

    uint maskLeft, bitsContinueLeft, maskRight, bitsContinueRight;
    {

        uint maskTraceLeft, maskTraceRight;
        if( horizontal )
        {
            maskTraceLeft  = 0x08;
            maskTraceRight = 0x02;
        }
        else
        {
            maskTraceLeft  = 0x04;
            maskTraceRight = 0x01;
        }
        if( invertedZShape )
        {
            uint temp      = maskTraceLeft;
            maskTraceLeft  = maskTraceRight;
            maskTraceRight = temp;
        }
        maskLeft          = maskTraceLeft;
        bitsContinueLeft  = maskTraceLeft;
        maskRight         = maskTraceRight;
        bitsContinueRight = maskTraceRight;
    }

    bool continueLeft  = true;
    bool continueRight = true;
    lineLengthLeft     = 1;
    lineLengthRight    = 1;
    [loop] for( ;; )
    {
        uint edgeLeft = LoadEdge( screenPos.xy - stepRight * float( lineLengthLeft ), int2( 0, 0 ) );
        uint edgeRight =
            LoadEdge( screenPos.xy + stepRight * ( float( lineLengthRight ) + 1 ), int2( 0, 0 ) );

        continueLeft  = continueLeft && ( ( edgeLeft & maskLeft ) == bitsContinueLeft );
        continueRight = continueRight && ( ( edgeRight & maskRight ) == bitsContinueRight );

        lineLengthLeft += continueLeft;
        lineLengthRight += continueRight;

        float maxLR = max( lineLengthRight, lineLengthLeft );

        if( !continueLeft && !continueRight )
        {
            maxLR = (float)c_maxLineLength;
        }

        if( maxLR >= min( (float)c_maxLineLength, ( 1.25 * min( lineLengthRight, lineLengthLeft ) - 0.25 ) ) )
        {
            break;
        }
    }
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
bool CollectBlendZs( uint2 screenPos, bool horizontal, bool invertedZShape, float shapeQualityScore,
                     float lineLengthLeft, float lineLengthRight, float2 stepRight )
{
    float leftOdd  = c_symmetryCorrectionOffset * float( lineLengthLeft % 2 );
    float rightOdd = c_symmetryCorrectionOffset * float( lineLengthRight % 2 );

    float dampenEffect = saturate( float( lineLengthLeft + lineLengthRight - shapeQualityScore ) * c_dampeningEffect );

    float loopFrom = -floor( ( lineLengthLeft + 1 ) / 2 ) + 1.0;
    float loopTo   = floor( ( lineLengthRight + 1 ) / 2 );

    uint itemIndex;
    const uint blendItemCount = loopTo - loopFrom + 1;
    InterlockedAdd( g_groupSharedBlendItemCount, blendItemCount, itemIndex );

    if( ( itemIndex + blendItemCount ) > 768 )
    {
        return false;
    }

    float totalLength = float( loopTo - loopFrom ) + 1 - leftOdd - rightOdd;
    float lerpStep    = float( 1.0 ) / totalLength;

    float lerpFromK = ( 0.5 - leftOdd - loopFrom ) * lerpStep;

    uint itemHeader    = ( screenPos.x << 18 ) | screenPos.y;
    uint itemValStatic = ( horizontal << 31 ) | ( invertedZShape << 30 );

    for( float i = loopFrom; i <= loopTo; i++ )
    {

        float secondPart = ( i > 0 );
        float srcOffset  = 1.0 - secondPart * 2.0;

        float lerpK = ( lerpStep * i + lerpFromK ) * srcOffset + secondPart;
        lerpK *= dampenEffect;

        int2 encodedItem;
        encodedItem.x = itemHeader;
        encodedItem.y = itemValStatic | ( ( uint( i + 256 ) ) << 20 ) | ( ( uint( srcOffset + 256 ) ) << 10 ) |
                        uint( saturate( lerpK ) * 1023 + 0.5 );
        g_groupSharedBlendItems[itemIndex++] = encodedItem;
    }
    return true;
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
void BlendZs( uint2 screenPos, bool horizontal, bool invertedZShape, float shapeQualityScore, float lineLengthLeft,
              float lineLengthRight, float2 stepRight )
{
    float2 blendDir = ( horizontal ) ? ( float2( 0, -1 ) ) : ( float2( -1, 0 ) );

    if( invertedZShape )
    {
        blendDir = -blendDir;
    }

    float leftOdd  = c_symmetryCorrectionOffset * float( lineLengthLeft % 2 );
    float rightOdd = c_symmetryCorrectionOffset * float( lineLengthRight % 2 );

    float dampenEffect = saturate( float( lineLengthLeft + lineLengthRight - shapeQualityScore ) * c_dampeningEffect );

    float loopFrom = -floor( ( lineLengthLeft + 1 ) / 2 ) + 1.0;
    float loopTo   = floor( ( lineLengthRight + 1 ) / 2 );

    float totalLength = float( loopTo - loopFrom ) + 1 - leftOdd - rightOdd;
    float lerpStep    = float( 1.0 ) / totalLength;

    float lerpFromK = ( 0.5 - leftOdd - loopFrom ) * lerpStep;

    for( float i = loopFrom; i <= loopTo; i++ )
    {

        float secondPart = ( i > 0 );
        float srcOffset  = 1.0 - secondPart * 2.0;

        float lerpK = ( lerpStep * i + lerpFromK ) * srcOffset + secondPart;
        lerpK *= dampenEffect;

        float2 pixelPos = screenPos + stepRight * float( i );

        float3 colorCenter = LoadSourceColor( pixelPos, int2( 0, 0 ) ).rgb;
        float3 colorFrom =
            LoadSourceColor( pixelPos.xy + blendDir * float( srcOffset ).xx, int2( 0, 0 ) ).rgb;

        float3 output = lerp( colorCenter.rgb, colorFrom.rgb, lerpK );

        StoreColorSample( pixelPos.xy, output, true );
    }
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_CANDIDATES )
[numthreads( 128, 1, 1 )]
void ProcessCandidatesCS( uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID )
{
    uint CandidateIndex = ( groupID.y * 65535u + groupID.x ) * 128u + groupThreadID.x;
    if( groupThreadID.x == 0 )
    {
        g_groupSharedBlendItemCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    const uint numCandidates = g_workingControlBuffer.Load( 4 * 3 );
    if( CandidateIndex < numCandidates )
    {

        uint pixelID = g_workingShapeCandidates[CandidateIndex];

        uint2 pixelPos = uint2( ( pixelID >> 18 ), pixelID & 0x3FFF );


        uint edgesCenterPacked = LoadEdge( pixelPos, int2( 0, 0 ) );
        float4 edges           = UnpackEdgesFlt( edgesCenterPacked );
        float4 edgesLeft       = UnpackEdgesFlt( LoadEdge( pixelPos, int2( -1, 0 ) ) );
        float4 edgesRight      = UnpackEdgesFlt( LoadEdge( pixelPos, int2( 1, 0 ) ) );
        float4 edgesBottom     = UnpackEdgesFlt( LoadEdge( pixelPos, int2( 0, 1 ) ) );
        float4 edgesTop        = UnpackEdgesFlt( LoadEdge( pixelPos, int2( 0, -1 ) ) );

        {
            float4 blendVal =
                ComputeSimpleShapeBlendValues( edges, edgesLeft, edgesRight, edgesTop, edgesBottom, true );

            const float fourWeightSum = dot( blendVal, float4( 1, 1, 1, 1 ) );
            const float centerWeight  = 1.0 - fourWeightSum;

            float3 outColor = LoadSourceColor( pixelPos, int2( 0, 0 ) ).rgb * centerWeight;
            [flatten] if( blendVal.x > 0.0 )
            {
                float3 pixelL = LoadSourceColor( pixelPos, int2( -1, 0 ) ).rgb;
                outColor.rgb += blendVal.x * pixelL;
            }
            [flatten] if( blendVal.y > 0.0 )
            {
                float3 pixelT = LoadSourceColor( pixelPos, int2( 0, -1 ) ).rgb;
                outColor.rgb += blendVal.y * pixelT;
            }
            [flatten] if( blendVal.z > 0.0 )
            {
                float3 pixelR = LoadSourceColor( pixelPos, int2( 1, 0 ) ).rgb;
                outColor.rgb += blendVal.z * pixelR;
            }
            [flatten] if( blendVal.w > 0.0 )
            {
                float3 pixelB = LoadSourceColor( pixelPos, int2( 0, 1 ) ).rgb;
                outColor.rgb += blendVal.w * pixelB;
            }

            StoreColorSample( pixelPos.xy, outColor, false );
        }

        {
            float invertedZScore;
            float normalZScore;
            float maxScore;
            bool horizontal = true;
            bool invertedZ  = false;

            {
                float4 edgesM1P0 = edgesLeft;
                float4 edgesP1P0 = edgesRight;
                float4 edgesP2P0 = UnpackEdgesFlt( LoadEdge( pixelPos, int2( 2, 0 ) ) );

                DetectZsHorizontal( edges, edgesM1P0, edgesP1P0, edgesP2P0, invertedZScore, normalZScore );
                maxScore = max( invertedZScore, normalZScore );

                if( maxScore > 0 )
                {
                    invertedZ = invertedZScore > normalZScore;
                }
            }

            {

                float4 edgesM1P0 = edgesBottom;
                float4 edgesP1P0 = edgesTop;
                float4 edgesP2P0 = UnpackEdgesFlt( LoadEdge( pixelPos, int2( 0, -2 ) ) );

                DetectZsHorizontal( edges.argb, edgesM1P0.argb, edgesP1P0.argb, edgesP2P0.argb, invertedZScore,
                                    normalZScore );
                float vertScore = max( invertedZScore, normalZScore );

                if( vertScore > maxScore )
                {
                    maxScore   = vertScore;
                    horizontal = false;
                    invertedZ  = invertedZScore > normalZScore;
                }
            }

            if( maxScore > 0 )
            {
                float shapeQualityScore = floor( clamp( 4.0 - maxScore, 0.0, 3.0 ) );

                const float2 stepRight = ( horizontal ) ? ( float2( 1, 0 ) ) : ( float2( 0, -1 ) );
                float lineLengthLeft, lineLengthRight;
                FindZLineLengths( lineLengthLeft, lineLengthRight, pixelPos, horizontal, invertedZ, stepRight );

                lineLengthLeft -= shapeQualityScore;
                lineLengthRight -= shapeQualityScore;

                if( ( lineLengthLeft + lineLengthRight ) >= ( 5.0 ) )
                {

                    if( !CollectBlendZs( pixelPos, horizontal, invertedZ, shapeQualityScore, lineLengthLeft,
                                         lineLengthRight, stepRight ) )
                    {
                        BlendZs( pixelPos, horizontal, invertedZ, shapeQualityScore, lineLengthLeft, lineLengthRight,
                                 stepRight );
                    }
                }
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint totalItemCount = min( 768, g_groupSharedBlendItemCount );

    uint loops = ( totalItemCount + ( 128 - 1 ) - groupThreadID.x ) / 128;

    for( uint loop = 0; loop < loops; loop++ )
    {
        uint index = loop * 128 + groupThreadID.x;

        uint2 itemVal = g_groupSharedBlendItems[index];

        uint2 startingPos        = uint2( ( itemVal.x >> 18 ), itemVal.x & 0x3FFF );

        bool itemHorizontal = ( itemVal.y >> 31 ) & 1;
        bool itemInvertedZ  = ( itemVal.y >> 30 ) & 1;
        float itemStepIndex = float( ( itemVal.y >> 20 ) & 0x3FF ) - 256.0;
        float itemSrcOffset = ( ( itemVal.y >> 10 ) & 0x3FF ) - 256.0;
        float itemLerpK     = ( itemVal.y & 0x3FF ) / 1023.0;

        float2 itemStepRight = ( itemHorizontal ) ? ( float2( 1, 0 ) ) : ( float2( 0, -1 ) );
        float2 itemBlendDir  = ( itemHorizontal ) ? ( float2( 0, -1 ) ) : ( float2( -1, 0 ) );
        if( itemInvertedZ )
        {
            itemBlendDir = -itemBlendDir;
        }

        uint2 itemPixelPos = startingPos + itemStepRight * float( itemStepIndex );

        float3 colorCenter = LoadSourceColor( itemPixelPos, int2( 0, 0 ) ).rgb;
        float3 colorFrom   = LoadSourceColor( itemPixelPos.xy + itemBlendDir * float( itemSrcOffset ).xx, int2( 0, 0 ) )
                               .rgb;

        float3 outputColor = lerp( colorCenter.rgb, colorFrom.rgb, itemLerpK );

        StoreColorSample( itemPixelPos.xy, outputColor, true );
    }
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_APPLY )
float3 Unpack_R11G11B10_E4_FLOAT( uint rgb )
{
    float r = f16tof32( ( rgb << 3 ) & 0x3FF8 );
    float g = f16tof32( ( rgb >> 8 ) & 0x3FF8 );
    float b = f16tof32( ( rgb >> 18 ) & 0x3FF0 );
    return float3( r, g, b );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_APPLY )
float3 InternalUnpackColor( uint packedColor )
{
    return Unpack_R11G11B10_E4_FLOAT( packedColor );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_APPLY )
float3 EncodeColor( float3 Color )
{
    Color = saturate( Color );
    return lerp( Color * 12.92f, 1.055f * pow( Color, 1.0f / 2.4f ) - 0.055f, step( 0.0031308f, Color ) );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_APPLY )
void FinalUAVStore( uint2 pixelPos, float3 color )
{
    float Alpha                     = g_inoutColorReadonly.Load( int3( pixelPos, 0 ) ).a;
    g_inoutColorWriteonly[pixelPos] = float4( EncodeColor( color ), Alpha );
}
#endif

//==============================================================================

#if defined( CMAA2_PASS_APPLY )
[numthreads( 4, 32, 1 )]
void DeferredColorApply2x2CS( uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID )
{
    const uint numCandidates       = g_workingControlBuffer.Load( 4 * 3 );
    const uint currentCandidate    = ( groupID.y * 65535u + groupID.x ) * 32u + groupThreadID.y;
    const uint currentQuadOffsetXY = groupThreadID.x;

    if( currentCandidate >= numCandidates )
    {
        return;
    }

    uint pixelID            = g_workingDeferredBlendLocationList[currentCandidate];
    uint2 quadPos           = uint2( ( pixelID >> 16 ), pixelID & 0xFFFF );
    const int2 qeOffsets[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
    uint2 pixelPos          = quadPos * 2 + qeOffsets[currentQuadOffsetXY];

    if( any( pixelPos >= ImageSize ) )
    {
        return;
    }

    uint counterIndexWithHeader = g_workingDeferredBlendItemListHeads[BlendHeadAddress( quadPos )];

    float4 outColors = float4( 0, 0, 0, 0 );

    const uint maxLoops = 32;
    {
        for( uint i = 0; ( counterIndexWithHeader != 0xFFFFFFFF ) && ( i < maxLoops ); i++ )
        {

            uint offsetXY = ( counterIndexWithHeader >> 30 ) & 0x03;

            bool isComplexShape = ( counterIndexWithHeader >> 26 ) & 0x01;

            uint2 val = g_workingDeferredBlendItemList[counterIndexWithHeader & ( ( 1 << 26 ) - 1 )];

            counterIndexWithHeader = val.x;

            if( offsetXY == currentQuadOffsetXY )
            {
                float3 color = InternalUnpackColor( val.y );
                float weight = 0.8 + 1.0 * float( isComplexShape );
                outColors += float4( color * weight, weight );
            }
        }
    }

    if( outColors.a == 0 )
    {
        return;
    }

    {
        float4 outColor = outColors;
        outColor.rgb /= outColor.a;
        FinalUAVStore( pixelPos, float3( outColor.rgb ) );
    }
}
#endif
