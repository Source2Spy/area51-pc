//==============================================================================
//
//  a51_shadow_evsm_blur.hlsl
//
//  Separable EVSM4 moment-atlas filtering.
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
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D<float4> txShadowMoments A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samShadowMoments A51_SAMPLER_BIND(0, 0);

A51_CBUFFER_ATTR(1, 0) cbuffer cbShadowFilter A51_CBUFFER_BIND(1, 0)
{
    A51_ABI_FIELDS_ShadowFilterData
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float4 SampleShadowMoments( float2 UV )
{
    return txShadowMoments.SampleLevel( samShadowMoments,
                                        clamp( UV, SourceClampRect.xy, SourceClampRect.zw ), 0.0f );
}

float4 SampleGaussian9( float2 UV, float2 Direction )
{
    float4 Result = SampleShadowMoments( UV ) * 0.2270270270f;
    Result +=
        SampleShadowMoments( UV + Direction * 1.3846153846f ) * 0.3162162162f;
    Result +=
        SampleShadowMoments( UV - Direction * 1.3846153846f ) * 0.3162162162f;
    Result +=
        SampleShadowMoments( UV + Direction * 3.2307692308f ) * 0.0702702703f;
    Result +=
        SampleShadowMoments( UV - Direction * 3.2307692308f ) * 0.0702702703f;
    return Result;
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSBlurHorizontal( VS_OUTPUT Input ) : SV_Target
{
    const float2 UV = Input.Position.xy * TextureParams.xy;
    return SampleGaussian9( UV, float2( TextureParams.x, 0.0f ) * FilterParams.z );
}

//==============================================================================

float4 PSBlurVertical( VS_OUTPUT Input ) : SV_Target
{
    const float2 UV = Input.Position.xy * TextureParams.xy;
    return SampleGaussian9( UV, float2( 0.0f, TextureParams.y ) * FilterParams.z );
}
