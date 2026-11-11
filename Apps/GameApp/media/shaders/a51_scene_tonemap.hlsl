//==============================================================================
//
//  a51_scene_tonemap.hlsl
//
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D SceneColor A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState SceneColorSampler A51_SAMPLER_BIND(0, 0);

A51_CBUFFER_ATTR(1, 0) cbuffer SceneColorGradeParams A51_CBUFFER_BIND(1, 0)
{
    A51_ABI_FIELDS_SceneColorGradeData
};

float3 ApplySceneColorGrade( float4 Scene )
{
    // Scene colors are already display-space values; preserve them for SDR output.
    float3 Color = saturate( Scene.rgb );

    [branch]
    if( Adjustments.x != 0.0f )
    {
        Color = pow( Color, exp2( -2.0f * Adjustments.x ) );
    }

    [branch]
    if( Adjustments.y != 1.0f )
    {
        const float Contrast = max( Adjustments.y, 1e-4f );
        const float3 Lower = 0.5f * pow( 2.0f * min( Color, 1.0f - Color ), Contrast );
        Color = lerp( Lower, 1.0f - Lower, step( 0.5f, Color ) );
    }

    return saturate( Color );
}

float4 PSMain( float4 Position : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 Scene = SceneColor.SampleLevel( SceneColorSampler, UV, 0.0f );
    return float4( ApplySceneColorGrade( Scene ), Scene.a );
}
