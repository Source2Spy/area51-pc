//==============================================================================
//
//  a51_composite.hlsl
//
//  Fullscreen composite shaders.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

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

    float2 UV = float2( ( VertexID << 1 ) & 2, VertexID & 2 );
    Output.Position = float4( UV * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f ), 0.5f, 1.0f );
    Output.UV       = UV;

    return Output;
}

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D CompositeSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samCompositeSource A51_SAMPLER_BIND(0, 0);

A51_CBUFFER_ATTR(1, 0) cbuffer CompositeParams A51_CBUFFER_BIND(1, 0)
{
    A51_ABI_FIELDS_CompositeData
};

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSMain( VS_OUTPUT Input ) : SV_Target
{
    float4 Color = CompositeSource.SampleLevel( samCompositeSource, Input.UV, 0.0f );
    Color *= BlendColor;

    if( BlendMode == 3 )
    {
        float3 Overlay;
        Overlay.r = (Color.r < 0.5f) ? (2.0f * Color.r * Color.r) : (1.0f - 2.0f * (1.0f - Color.r) * (1.0f - Color.r));
        Overlay.g = (Color.g < 0.5f) ? (2.0f * Color.g * Color.g) : (1.0f - 2.0f * (1.0f - Color.g) * (1.0f - Color.g));
        Overlay.b = (Color.b < 0.5f) ? (2.0f * Color.b * Color.b) : (1.0f - 2.0f * (1.0f - Color.b) * (1.0f - Color.b));
        Color.rgb = Overlay;
    }

    return Color;
}

//==============================================================================

float4 PSPresent( VS_OUTPUT Input ) : SV_Target
{
    return CompositeSource.SampleLevel( samCompositeSource, Input.UV, 0.0f );
}

//==============================================================================

float4 PSClampNegative( VS_OUTPUT Input ) : SV_Target
{
    return 0.0f.xxxx;
}
