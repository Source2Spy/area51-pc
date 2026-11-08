//==============================================================================
//
//  a51_scene_region_copy.hlsl
//
//  Scaled scene capture used by scope and other output-space consumers.
//
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D SceneColorSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samSceneColorSource A51_SAMPLER_BIND(0, 0);

A51_CBUFFER_ATTR(1, 0) cbuffer SceneRegionParams A51_CBUFFER_BIND(1, 0)
{
    A51_ABI_FIELDS_SceneRegionCopyData
};

//==============================================================================

float4 PSMain( VS_OUTPUT Input ) : SV_Target0
{
    const float2 UV = SourceUV.xy + Input.UV * SourceUV.zw;
    return SceneColorSource.SampleLevel( samSceneColorSource, UV, 0.0f );
}
