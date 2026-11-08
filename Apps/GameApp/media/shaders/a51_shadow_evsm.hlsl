//==============================================================================
//
//  a51_shadow_evsm.hlsl
//
//  EVSM4 depth-to-moment conversion.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"
#include "common/shadow_evsm.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  SHADERS
//==============================================================================

VS_OUTPUT VSMain( uint VertexID : SV_VertexID )
{
    VS_OUTPUT Output;

    const float2 UV = float2( ( VertexID << 1 ) & 2, VertexID & 2 );
    Output.Position = float4( UV * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f ), 0.5f, 1.0f );
    Output.UV = UV;
    return Output;
}

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D<float> txShadowDepth A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samShadowDepth A51_SAMPLER_BIND(0, 0);

A51_CBUFFER_ATTR(1, 0) cbuffer cbShadowFilter A51_CBUFFER_BIND(1, 0)
{
    A51_ABI_FIELDS_ShadowFilterData
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float LinearizeShadowDepth( float Depth )
{
    const float NearZ = max( DepthParams.x, 1.0e-4f );
    const float FarZ = max( DepthParams.y, NearZ + 1.0e-4f );
    return ( NearZ * FarZ ) / max( FarZ - Depth * ( FarZ - NearZ ), 1.0e-4f );
}

//==============================================================================

float ComputeShadowRadialDepth( float Depth, float2 SourceUV )
{
    const float2 FaceNDC = ( SourceUV - SourceProjectionParams.xy ) * SourceProjectionParams.z;
    const float TanHalfFov = SourceProjectionParams.w;
    const float2 FacePlane = FaceNDC * TanHalfFov;
    const float RadialDistance = LinearizeShadowDepth( Depth ) * sqrt( 1.0f + dot( FacePlane, FacePlane ) );
    return saturate( RadialDistance / max( DepthParams.z, 1.0e-4f ) );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSConvert( VS_OUTPUT Input ) : SV_Target
{
    const float2 OutputUV = Input.Position.xy * TextureParams.zw;
    const float2 SampleOffset = TextureParams.zw * 0.25f;
    static const float2 KSampleDirections[4] =
    {
        float2( -1.0f, -1.0f ),
        float2(  1.0f, -1.0f ),
        float2( -1.0f,  1.0f ),
        float2(  1.0f,  1.0f )
    };

    float4 Moments = 0.0f;
    [unroll]
    for( uint I = 0u; I < 4u; ++I )
    {
        const float2 SourceUV =
            clamp( OutputUV + KSampleDirections[I] * SampleOffset, SourceClampRect.xy, SourceClampRect.zw );
        const float Depth = txShadowDepth.SampleLevel( samShadowDepth, SourceUV, 0.0f );
        const float RadialDepth = ComputeShadowRadialDepth( Depth, SourceUV );
        Moments += ComputeEVSM4Moments( RadialDepth, FilterParams.xy );
    }

    return Moments * 0.25f;
}
