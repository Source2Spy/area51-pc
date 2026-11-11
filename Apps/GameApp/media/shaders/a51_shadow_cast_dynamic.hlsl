//==============================================================================
//
//  a51_shadow_cast_dynamic.hlsl
//
//  Shadow depth caster shader for runtime cloth geometry.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"
#include "common/cloth_damage.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbShadowCast A51_CBUFFER_BIND(0, 0)
{
    A51_ABI_FIELDS_ShadowCastData
};

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txDamage A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);
A51_SAMPLER_ATTR(1, 1) SamplerState samDamage A51_SAMPLER_BIND(1, 1);

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float2 UV       : TEXCOORD1;
};

//------------------------------------------------------------------------------

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
    nointerpolation uint UsesTextureAlpha : TEXCOORD1;
};

//==============================================================================
//  SHADERS
//==============================================================================

VS_OUTPUT VSMain( VS_INPUT Input )
{
    VS_OUTPUT Output;
    Output.Position = mul( ShadowViewProjection, float4( Input.Position - ShadowOrigin.xyz, 1.0f ) );
    Output.UV = Input.UV;
    Output.UsesTextureAlpha = CoverageUsesTextureAlpha;
    return Output;
}

//==============================================================================

void PSMain( VS_OUTPUT Input )
{
    float const DiffuseAlpha = txDiffuse.Sample( samDiffuse, Input.UV ).a;
    float const Damage = ClothDamageCoverage( txDamage, samDamage, Input.UV );
    ClothDiscardDamagedPixel( DiffuseAlpha, Damage, Input.UsesTextureAlpha != 0 );
}
