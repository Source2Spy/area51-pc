//==============================================================================
//
//  a51_ui_video.hlsl
//
//  I420 movie frames in the UI pass.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D<float> txVideoY A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D<float> txVideoU A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLED_TEXTURE_ATTR(2, 2) Texture2D<float> txVideoV A51_SAMPLED_TEXTURE_BIND(2, 2);
A51_SAMPLER_ATTR(0, 0) SamplerState samVideoY A51_SAMPLER_BIND(0, 0);
A51_SAMPLER_ATTR(1, 1) SamplerState samVideoU A51_SAMPLER_BIND(1, 1);
A51_SAMPLER_ATTR(2, 2) SamplerState samVideoV A51_SAMPLER_BIND(2, 2);

//==============================================================================
//  TYPES
//==============================================================================

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
    float2 UV       : TEXCOORD0;
};

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSMain( PS_INPUT Input ) : SV_Target0
{
    uint Width, Height, ChromaWidth, ChromaHeight;
    txVideoY.GetDimensions( Width, Height );
    txVideoU.GetDimensions( ChromaWidth, ChromaHeight );

    const float2 ChromaUV = Input.UV * (float2( Width, Height ) / (2.0f * float2( ChromaWidth, ChromaHeight )));
    const float Y = txVideoY.Sample( samVideoY, Input.UV ) - 16.0f / 255.0f;
    const float U = txVideoU.Sample( samVideoU, ChromaUV ) - 128.0f / 255.0f;
    const float V = txVideoV.Sample( samVideoV, ChromaUV ) - 128.0f / 255.0f;

    const float3 RGB = (298.0f * Y + float3( 409.0f * V, -100.0f * U - 208.0f * V, 516.0f * U )) / 256.0f;
    return float4( saturate( RGB ), 1.0f ) * Input.Color;
}
