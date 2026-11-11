//==============================================================================
//
//  a51_ui.hlsl
//
//  Screen-space UI shader.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbUIDraw A51_CBUFFER_BIND(0, 0)
{
    A51_ABI_FIELDS_UIDrawData
};

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txUI A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samUI A51_SAMPLER_BIND(0, 0);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float2 Position : TEXCOORD0;
    float4 Color    : TEXCOORD1;
    float2 UV       : TEXCOORD2;
};

//------------------------------------------------------------------------------

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  SHADERS
//==============================================================================

PS_INPUT VSMain( VS_INPUT Input )
{
    PS_INPUT Output;
    const float2 Normalized = (Input.Position - LogicalOrigin) * InverseLogicalSize;
    Output.Position = float4( Normalized.x * 2.0f - 1.0f,
                              1.0f - Normalized.y * 2.0f,
                              0.0f,
                              1.0f );
    Output.Color = Input.Color.bgra;
    Output.UV = Input.UV;
    return Output;
}

//==============================================================================

float4 PSMain( PS_INPUT Input ) : SV_Target0
{
    const float4 SampleValue = txUI.Sample( samUI, Input.UV );
    if( Coverage != 0 )
        return float4( Input.Color.rgb, Input.Color.a * SampleValue.r );
    return SampleValue * Input.Color;
}
