//==============================================================================
//
//  xegtao.hlsl
//
//  XeGTAO integration for normalized linear scene depth and SDL GPU.
//  Derived portions: Copyright (C) 2016-2021, Intel Corporation. MIT license.
//  See xCore/3rdParty/XeGTAO/LICENSE.
//
//==============================================================================

#ifndef A51_XEGTAO_HLSL
#define A51_XEGTAO_HLSL

#include "shader_bindings.hlsl"
#include "scene_depth.hlsl"

#define XE_GTAO_USE_HALF_FLOAT_PRECISION 0
#define XE_GTAO_FP32_DEPTHS
#define VA_SATURATE saturate
#include "../../../../../xCore/3rdParty/XeGTAO/XeGTAO.hlsli"

#if defined( __spirv__ )
    #define A51_AO_IMAGE_FORMAT( F ) [[vk::image_format( F )]]
#else
    #define A51_AO_IMAGE_FORMAT( F )
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_VK_BINDING( 0, 2 ) cbuffer XeGtaoParams : register( b0, space2 )
{
    GTAOConstants GTAO;
    uint2 FullSize;
    uint2 AOSize;
    uint Downsample;
    float NearZ;
    float FarZ;
    float Strength;
};

#if defined( A51_AO_PREPARE )
    #if A51_AO_MSAA
        A51_VK_BINDING( 0, 0 ) Texture2DMS<float> SceneDepth : register( t0, space0 );
    #else
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 0, 0 ) Texture2D<float> SceneDepth : register( t0, space0 );
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 0, 0 ) SamplerState DepthSampler : register( s0, space0 );
    #endif
    A51_VK_BINDING( 0, 1 ) RWTexture2D<float> Depth0 : register( u0, space1 );
    A51_VK_BINDING( 1, 1 ) RWTexture2D<float> Depth1 : register( u1, space1 );
    A51_VK_BINDING( 2, 1 ) RWTexture2D<float> Depth2 : register( u2, space1 );
    A51_VK_BINDING( 3, 1 ) RWTexture2D<float> Depth3 : register( u3, space1 );
    A51_VK_BINDING( 4, 1 ) RWTexture2D<float> Depth4 : register( u4, space1 );
    #if A51_AO_HALF
        A51_VK_BINDING( 5, 1 ) RWTexture2D<float> FullDepth : register( u5, space1 );
    #endif
#elif defined( A51_AO_MAIN )
    A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 0, 0 ) Texture2D<float> ViewDepth : register( t0, space0 );
    A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 0, 0 ) SamplerState DepthSampler : register( s0, space0 );
    #if A51_AO_MSAA
        A51_VK_BINDING( 1, 0 ) Texture2DMS<float> SurfaceDepth : register( t1, space0 );
        A51_VK_BINDING( 2, 0 ) Texture2DMS<float4> SurfaceNormals : register( t2, space0 );
    #else
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 1, 0 ) Texture2D<float> SurfaceDepth : register( t1, space0 );
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 1, 0 ) SamplerState SurfaceDepthSampler : register( s1, space0 );
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 2, 0 ) Texture2D<float4> SurfaceNormals : register( t2, space0 );
        A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 2, 0 ) SamplerState NormalSampler : register( s2, space0 );
    #endif
    A51_AO_IMAGE_FORMAT( "r8ui" ) A51_VK_BINDING( 0, 1 ) RWTexture2D<uint> WorkingAO : register( u0, space1 );
    A51_AO_IMAGE_FORMAT( "r8" ) A51_VK_BINDING( 1, 1 ) RWTexture2D<unorm float> Edges : register( u1, space1 );
#elif defined( A51_AO_DENOISE )
    A51_VK_BINDING( 0, 0 ) Texture2D<uint> WorkingAO : register( t0, space0 );
    A51_VK_BINDING( 1, 0 ) Texture2D<float> Edges : register( t1, space0 );
    #if A51_AO_FINAL_DENOISE
        A51_AO_IMAGE_FORMAT( "r8" ) A51_VK_BINDING( 0, 1 ) RWTexture2D<unorm float> FilteredAO : register( u0, space1 );
    #else
        A51_AO_IMAGE_FORMAT( "r8ui" ) A51_VK_BINDING( 0, 1 ) RWTexture2D<uint> FilteredAO : register( u0, space1 );
    #endif
#elif defined( A51_AO_RESOLVE )
    A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 0, 0 ) Texture2D<float> ViewDepth : register( t0, space0 );
    A51_VK_COMBINED_SAMPLER A51_VK_BINDING( 0, 0 ) SamplerState DepthSampler : register( s0, space0 );
    A51_VK_BINDING( 1, 0 ) Texture2D<float> FilteredAO : register( t1, space0 );
    A51_VK_BINDING( 2, 0 ) Texture2D<float> FullDepth : register( t2, space0 );
    A51_AO_IMAGE_FORMAT( "r8" ) A51_VK_BINDING( 0, 1 ) RWTexture2D<unorm float> AmbientVisibility : register( u0, space1 );
#endif

//==============================================================================
//  DEPTH PREPARATION
//==============================================================================

#if defined( A51_AO_PREPARE )
float LoadSceneViewDepth( uint2 pixel )
{
    pixel = min( pixel, FullSize - 1 );
#if A51_AO_MSAA
    uint width, height, samples;
    SceneDepth.GetDimensions( width, height, samples );
    const float depth = SceneDepthNearestSample( SceneDepth, int2( pixel ), samples );
#else
    const float depth = SceneDepth.SampleLevel( DepthSampler, ( float2( pixel ) + 0.5f ) / float2( FullSize ), 0 );
#endif
    return depth >= 1.0f ? FarZ : NearZ + depth * ( FarZ - NearZ );
}

//==============================================================================

float PreparePixelDepth( uint2 pixel )
{
#if A51_AO_HALF
    const uint2 source = min( pixel, AOSize - 1 ) * 2;
    float nearestDepth = FarZ;

    [unroll]
    for( uint y = 0; y < 2; ++y )
    {
        [unroll]
        for( uint x = 0; x < 2; ++x )
        {
            const uint2 fullPixel = source + uint2( x, y );
            const float depth = LoadSceneViewDepth( fullPixel );
            nearestDepth = min( nearestDepth, depth );
            if( all( pixel < AOSize ) && all( fullPixel < FullSize ) )
            {
                FullDepth[fullPixel] = depth;
            }
        }
    }

    return nearestDepth;
#else
    return LoadSceneViewDepth( min( pixel, AOSize - 1 ) );
#endif
}

//==============================================================================

[numthreads( 8, 8, 1 )]
void CSPrepare( uint2 thread : SV_DispatchThreadID, uint2 local : SV_GroupThreadID )
{
    const uint2 pixel = thread * 2;
    const float d0 = PreparePixelDepth( pixel );
    const float d1 = PreparePixelDepth( pixel + uint2( 1, 0 ) );
    const float d2 = PreparePixelDepth( pixel + uint2( 0, 1 ) );
    const float d3 = PreparePixelDepth( pixel + uint2( 1, 1 ) );
    Depth0[pixel] = d0;
    Depth0[pixel + uint2( 1, 0 )] = d1;
    Depth0[pixel + uint2( 0, 1 )] = d2;
    Depth0[pixel + uint2( 1, 1 )] = d3;

    float depth = XeGTAO_DepthMIPFilter( d0, d1, d2, d3, GTAO );
    Depth1[thread] = depth;
    g_scratchDepths[local.x][local.y] = depth;
    GroupMemoryBarrierWithGroupSync();

    if( all( ( local % 2 ) == 0 ) )
    {
        depth = XeGTAO_DepthMIPFilter( g_scratchDepths[local.x][local.y],
                                      g_scratchDepths[local.x + 1][local.y],
                                      g_scratchDepths[local.x][local.y + 1],
                                      g_scratchDepths[local.x + 1][local.y + 1], GTAO );
        Depth2[thread / 2] = depth;
        g_scratchDepths[local.x][local.y] = depth;
    }
    GroupMemoryBarrierWithGroupSync();

    if( all( ( local % 4 ) == 0 ) )
    {
        depth = XeGTAO_DepthMIPFilter( g_scratchDepths[local.x][local.y],
                                      g_scratchDepths[local.x + 2][local.y],
                                      g_scratchDepths[local.x][local.y + 2],
                                      g_scratchDepths[local.x + 2][local.y + 2], GTAO );
        Depth3[thread / 4] = depth;
        g_scratchDepths[local.x][local.y] = depth;
    }
    GroupMemoryBarrierWithGroupSync();

    if( all( ( local % 8 ) == 0 ) )
    {
        Depth4[thread / 8] = XeGTAO_DepthMIPFilter( g_scratchDepths[local.x][local.y],
                                                   g_scratchDepths[local.x + 4][local.y],
                                                   g_scratchDepths[local.x][local.y + 4],
                                                   g_scratchDepths[local.x + 4][local.y + 4], GTAO );
    }
}
#endif

//==============================================================================
//  HORIZON SEARCH
//==============================================================================

#if defined( A51_AO_MAIN )
[numthreads( 8, 8, 1 )]
void CSMain( uint2 pixel : SV_DispatchThreadID )
{
    if( any( pixel >= AOSize ) )
    {
        return;
    }

    if( ViewDepth.Load( int3( pixel, 0 ) ) >= FarZ )
    {
        WorkingAO[pixel] = uint( 255.0f / XE_GTAO_OCCLUSION_TERM_SCALE + 0.5f );
        Edges[pixel] = 0.0f;
        return;
    }

    const uint index = HilbertIndex( pixel.x, pixel.y );
    const float2 noise = frac( 0.5f + float( index ) * float2( 0.75487766624669276f, 0.56984029099805327f ) );
    const uint2 source = pixel * Downsample;
    uint2 nearestPixel = min( source, FullSize - 1 );
    float nearestDepth = 1.0f;
#if A51_AO_MSAA
    uint width, height, samples;
    SurfaceDepth.GetDimensions( width, height, samples );
    uint nearestSample = 0;
    for( uint y = 0; y < Downsample; ++y )
    {
        for( uint x = 0; x < Downsample; ++x )
        {
            const uint2 fullPixel = min( source + uint2( x, y ), FullSize - 1 );
            for( uint sampleIndex = 0; sampleIndex < samples; ++sampleIndex )
            {
                const float depth = SurfaceDepth.Load( int2( fullPixel ), sampleIndex );
                if( depth < nearestDepth )
                {
                    nearestDepth = depth;
                    nearestPixel = fullPixel;
                    nearestSample = sampleIndex;
                }
            }
        }
    }
    float3 normal = SurfaceNormals.Load( int2( nearestPixel ), nearestSample ).xyz * 2.0f - 1.0f;
#else
    [branch]
    if( Downsample > 1 )
    {
        for( uint y = 0; y < Downsample; ++y )
        {
            for( uint x = 0; x < Downsample; ++x )
            {
                const uint2 fullPixel = min( source + uint2( x, y ), FullSize - 1 );
                const float depth = SurfaceDepth.SampleLevel( SurfaceDepthSampler,
                    ( float2( fullPixel ) + 0.5f ) / float2( FullSize ), 0 );
                if( depth < nearestDepth )
                {
                    nearestDepth = depth;
                    nearestPixel = fullPixel;
                }
            }
        }
    }
    float3 normal = SurfaceNormals.SampleLevel( NormalSampler,
        ( float2( nearestPixel ) + 0.5f ) / float2( FullSize ), 0 ).xyz * 2.0f - 1.0f;
#endif
    normal = normalize( normal );
    normal.x = -normal.x;
    XeGTAO_MainPass( pixel, A51_AO_SLICES, A51_AO_STEPS, noise, normal, GTAO,
                     ViewDepth, DepthSampler, WorkingAO, Edges );
}
#endif

//==============================================================================
//  SPATIAL DENOISE
//==============================================================================

#if defined( A51_AO_DENOISE )
int2 ClampAOPixel( int2 pixel )
{
    return clamp( pixel, int2( 0, 0 ), int2( AOSize ) - 1 );
}

float4 LoadEdges( int2 pixel )
{
    return XeGTAO_UnpackEdges( Edges.Load( int3( ClampAOPixel( pixel ), 0 ) ) );
}

float LoadVisibility( int2 pixel )
{
    return float( WorkingAO.Load( int3( ClampAOPixel( pixel ), 0 ) ) );
}

//==============================================================================

[numthreads( 8, 8, 1 )]
void CSDenoise( uint2 pixel : SV_DispatchThreadID )
{
    if( any( pixel >= AOSize ) )
    {
        return;
    }

    const int2 p = int2( pixel );
    const float4 left = LoadEdges( p + int2( -1, 0 ) );
    const float4 right = LoadEdges( p + int2( 1, 0 ) );
    const float4 top = LoadEdges( p + int2( 0, -1 ) );
    const float4 bottom = LoadEdges( p + int2( 0, 1 ) );
    float4 edges = LoadEdges( p ) * float4( left.y, right.x, top.w, bottom.z );
    const float edginess = saturate( 1.5f - dot( edges, 1.0f.xxxx ) ) / 1.5f * 0.5f;
    edges = saturate( edges + edginess );

    const float4 diagonal = 0.425f * float4( edges.x * left.z + edges.z * top.x,
                                            edges.z * top.y + edges.y * right.z,
                                            edges.w * bottom.x + edges.x * left.w,
                                            edges.y * right.w + edges.w * bottom.y );
    float weight = A51_AO_FINAL_DENOISE ? GTAO.DenoiseBlurBeta : GTAO.DenoiseBlurBeta / 5.0f;
    float sum = LoadVisibility( p ) * weight;
    sum += LoadVisibility( p + int2( -1, 0 ) ) * edges.x;
    sum += LoadVisibility( p + int2( 1, 0 ) ) * edges.y;
    sum += LoadVisibility( p + int2( 0, -1 ) ) * edges.z;
    sum += LoadVisibility( p + int2( 0, 1 ) ) * edges.w;
    sum += LoadVisibility( p + int2( -1, -1 ) ) * diagonal.x;
    sum += LoadVisibility( p + int2( 1, -1 ) ) * diagonal.y;
    sum += LoadVisibility( p + int2( -1, 1 ) ) * diagonal.z;
    sum += LoadVisibility( p + int2( 1, 1 ) ) * diagonal.w;
    weight += dot( edges, 1.0f.xxxx ) + dot( diagonal, 1.0f.xxxx );
    const float visibility = sum / weight;

#if A51_AO_FINAL_DENOISE
    const float filtered = saturate( visibility * ( XE_GTAO_OCCLUSION_TERM_SCALE / 255.0f ) );
    FilteredAO[pixel] = Downsample == 1 ? lerp( 1.0f, filtered, Strength ) : filtered;
#else
    FilteredAO[pixel] = uint( visibility + 0.5f );
#endif
}
#endif

//==============================================================================
//  FULL RESOLUTION MASK
//==============================================================================

#if defined( A51_AO_RESOLVE )
[numthreads( 8, 8, 1 )]
void CSResolve( uint2 pixel : SV_DispatchThreadID )
{
    if( any( pixel >= FullSize ) )
    {
        return;
    }

    const float depth = FullDepth.Load( int3( pixel, 0 ) );
    float visibility = 1.0f;
    if( depth < FarZ )
    {
        if( Downsample == 1 )
        {
            visibility = FilteredAO.Load( int3( pixel, 0 ) );
        }
        else
        {
            const float2 position = ( float2( pixel ) + 0.5f ) / float( Downsample ) - 0.5f;
            const int2 base = int2( floor( position ) );
            const float2 blend = frac( position );
            float sum = 0.0f;
            float sumWeight = 0.0f;

            [unroll]
            for( int y = 0; y < 2; ++y )
            {
                [unroll]
                for( int x = 0; x < 2; ++x )
                {
                    const int2 tap = clamp( base + int2( x, y ), int2( 0, 0 ), int2( AOSize ) - 1 );
                    const float tapDepth = ViewDepth.SampleLevel( DepthSampler,
                        ( float2( tap ) + 0.5f ) * GTAO.ViewportPixelSize, 0 );
                    const float difference = abs( tapDepth - depth ) / max( depth * 0.02f, 1e-4f );
                    const float2 bilinear = lerp( 1.0f - blend, blend, float2( x, y ) );
                    const float weight = bilinear.x * bilinear.y * exp2( -difference * difference );
                    sum += FilteredAO.Load( int3( tap, 0 ) ) * weight;
                    sumWeight += weight;
                }
            }

            visibility = sumWeight > 1e-4f ? sum / sumWeight : 1.0f;
        }
    }

    AmbientVisibility[pixel] = saturate( lerp( 1.0f, visibility, Strength ) );
}
#endif

#endif // A51_XEGTAO_HLSL
