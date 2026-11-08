// Surface composition is shared by every vertex deformation contract.
#include "common/material_flags.hlsl"
#include "common/geom_shader_features.hlsl"
#ifndef A51_RECIPE_FLAGS
#error "Surface programs require an offline recipe"
#endif
#if !A51_GEOM_DEPTH_ONLY || A51_GEOM_ALPHA_TEST || defined(A51_SCENE_DEPTH_TARGET) || A51_GEOM_LINEAR_DEPTH
#include "common/frame_constants.hlsl"
#endif
#include "common/geom_surface_interface.hlsl"
#include "common/pixel_structs.hlsl"
#if A51_RECIPE_DAMAGE
#include "common/cloth_damage.hlsl"
#endif

#if A51_GEOM_DEPTH_ONLY
#if A51_GEOM_ALPHA_TEST
#include "common/geom_textures.hlsl"
#endif
GeomSceneOutput PSMain( GEOM_PIXEL_INPUT Input, bool IsFrontFace : SV_IsFrontFace )
{
#if A51_GEOM_ALPHA_TEST
    const float2 UVDx = ddx( Input.UV );
    const float2 UVDy = ddy( Input.UV );
#if A51_RECIPE_DAMAGE
    ClothDiscardDamagedPixel( txDiffuse.SampleGrad( samDiffuse, Input.UV, UVDx, UVDy ).a,
                             txDamage.SampleGrad( samDamage, Input.UV, UVDx, UVDy ).r,
                             CoverageUsesTextureAlpha != 0 );
#else
    if( txDiffuse.SampleGrad( samDiffuse, Input.UV, UVDx, UVDy ).a < AlphaRef ) discard;
#endif
#endif
    GeomSceneOutput Output;
#if A51_GEOM_LINEAR_DEPTH
    Output.Color = saturate( (Input.ViewDepth - NearZ) / max(FarZ - NearZ, 1e-5f) ).xxxx;
#if A51_GEOM_NORMAL_TARGET
    float3 Normal = normalize( Input.Normal );
    if( SurfaceTwoSided && IsFrontFace )
    {
        Normal = -Normal;
    }
    Normal = normalize( mul( (float3x3)View, Normal ) );
    Output.Normal = float4( Normal * 0.5f + 0.5f, 1.0f );
#endif
#else
    Output.Color = 0;
#endif
    return Output;
}
#elif A51_RECIPE_GLOW_MASK
#if A51_GEOM_ALPHA_TEST
#include "common/geom_textures.hlsl"
#endif
GeomSceneOutput PSMain( GEOM_PIXEL_INPUT Input )
{
#if A51_GEOM_ALPHA_TEST
#if A51_RECIPE_DAMAGE
    ClothDiscardDamagedPixel( txDiffuse.Sample( samDiffuse, Input.UV ).a,
                             ClothDamageCoverage( txDamage, samDamage, Input.UV ), CoverageUsesTextureAlpha != 0 );
#else
    if( txDiffuse.Sample(samDiffuse, Input.UV).a < AlphaRef ) discard;
#endif
#endif
    GeomSceneOutput Output;
    Output.Color = float4(0,0,0,saturate(EnvParams.x));
    return Output;
}
#elif A51_RECIPE_DISTORTION
#include "common/geom_textures.hlsl"
#include "common/geom_pixel_core.hlsl"
#include "common/geom_pixel_distortion.hlsl"
GeomSceneOutput PSMain( GEOM_PIXEL_INPUT Input, bool FrontFace : SV_IsFrontFace )
{
    Input.Normal = normalize(Input.Normal);
    if( SurfaceTwoSided && FrontFace ) Input.Normal = -Input.Normal;
    float4 Color = GeomShadeDistortionPixel(Input, A51_RECIPE_FLAGS, saturate(Input.FadeAlpha));
    const float Depth = GeomEncodeLinearDepth(Input).r;
#if A51_RECIPE_FOG
    Color = ApplyFogToColor(Color, Depth);
#endif
    Color.rgb *= Color.a;
    GeomSceneOutput Output;
    Output.Color = Color;
#if A51_SCENE_DEPTH_TARGET
    Output.Depth = Depth;
#endif
    return Output;
}
#else
#ifndef GEOM_HAS_VERTEX_COLOR
#define GEOM_HAS_VERTEX_COLOR 1
#endif
#include "common/lighting_constants.hlsl"
#if A51_RECIPE_PROJECTION || A51_RECIPE_COOKIES
// Local light cookies also use the atlas dimensions in this buffer.
#include "common/proj_buffers.hlsl"
#endif
#if A51_RECIPE_SHADOWS
#include "common/shadow_buffers.hlsl"
#endif
#include "common/geom_textures.hlsl"
#include "common/geom_pixel_shared.hlsl"
#if A51_RECIPE_SHADOWS
#include "common/geom_local_shadow_maps.hlsl"
#endif
GeomSceneOutput PSMain( GEOM_PIXEL_INPUT Input, bool FrontFace : SV_IsFrontFace )
{
    const float LinearDepth = GeomEncodeLinearDepth(Input).r;
    GeomSceneOutput Output;
    Output.Color = GeomComposeSceneColor(Input, FrontFace, LinearDepth);
#if A51_SCENE_DEPTH_TARGET
    Output.Depth = LinearDepth;
#endif
    return Output;
}
#endif
