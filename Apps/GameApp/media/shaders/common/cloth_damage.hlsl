//==============================================================================
//
//  cloth_damage.hlsl
//
//  Shared runtime cloth cutout and frayed-edge helpers.
//
//==============================================================================

#ifndef CLOTH_DAMAGE_HLSL
#define CLOTH_DAMAGE_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

void ClothDiscardDamagedPixel( float DiffuseAlpha, float Damage, bool UsesTextureAlpha )
{
    // Opaque cloth textures may have an unused alpha channel filled with zero.
    float const Coverage = UsesTextureAlpha ? DiffuseAlpha : 1.0f;
    clip( min( Coverage - 0.05f, Damage - 0.5f ) );
}

//==============================================================================

float ClothDamageCoverage( Texture2D DamageTexture, SamplerState DamageSampler, float2 UV )
{
    return DamageTexture.Sample( DamageSampler, UV ).r;
}

//==============================================================================

float ClothDamageEdge( Texture2D DamageTexture, SamplerState DamageSampler, float2 UV, float Coverage, float2 EdgeStep )
{
    float Neighbour = 1.0f;
    Neighbour = min( Neighbour, DamageTexture.SampleLevel( DamageSampler, UV + float2( EdgeStep.x, 0.0f ), 0.0f ).r );
    Neighbour = min( Neighbour, DamageTexture.SampleLevel( DamageSampler, UV - float2( EdgeStep.x, 0.0f ), 0.0f ).r );
    Neighbour = min( Neighbour, DamageTexture.SampleLevel( DamageSampler, UV + float2( 0.0f, EdgeStep.y ), 0.0f ).r );
    Neighbour = min( Neighbour, DamageTexture.SampleLevel( DamageSampler, UV - float2( 0.0f, EdgeStep.y ), 0.0f ).r );

    // Shade the surviving cloth beside the cut, not the discarded hole interior.
    float const Lip = 1.0f - smoothstep( 0.5f, 0.95f, Coverage );
    float const Edge = smoothstep( 0.08f, 0.55f, Coverage - Neighbour );
    return saturate( max( Lip, Edge * 0.65f ) );
}

//==============================================================================
#endif // CLOTH_DAMAGE_HLSL
//==============================================================================
