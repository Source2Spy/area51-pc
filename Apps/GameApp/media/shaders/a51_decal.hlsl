//==============================================================================
//
//  a51_decal.hlsl
//
//  Lit forward surface decals.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/material_flags.hlsl"
#include "common/shader_bindings.hlsl"
#define A51_FOG_PALETTE_SDL_BINDING 5
#define A51_DECAL_LIT (A51_DECAL_BLEND == 0 || A51_DECAL_BLEND == 2)
#define A51_RECIPE_PROJECTION A51_DECAL_LIT
#define A51_RECIPE_FLAGS 0
#ifndef A51_DECAL_BLEND
#error "Decal shaders require an offline recipe"
#endif
#if A51_DECAL_FOG
#include "common/fog_constants.hlsl"
#include "common/fog_functions.hlsl"
#endif
#if A51_DECAL_LIT
#define A51_GEOM_LIGHTING_BINDING 6
#define A51_GEOM_LIGHT_PROJECTION_BINDING 9
#define A51_GEOM_AMBIENT_BINDING 10
#define A51_GEOM_SHADOW_MATRICES_BINDING 7
#define A51_GEOM_SHADOW_DATA_BINDING 8
#define A51_GEOM_SURFACE_ONLY 1
#include "common/lighting_constants.hlsl"
#include "common/proj_buffers.hlsl"
#if A51_RECIPE_SHADOWS
#include "common/shadow_buffers.hlsl"
#endif
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

#include "common/decal_frame_constants.hlsl"

//------------------------------------------------------------------------------

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDecal A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samDecal A51_SAMPLER_BIND(0, 0);
#if A51_DECAL_ENV
A51_SAMPLED_TEXTURE_ATTR(3, 1) TextureCube txEnvironmentCube A51_SAMPLED_TEXTURE_BIND(3, 1);
A51_SAMPLER_ATTR(3, 1) SamplerState samEnvironmentCube A51_SAMPLER_BIND(3, 1);
#endif
#if A51_DECAL_LIT
#if A51_RECIPE_SHADOWS
A51_SAMPLED_TEXTURE_ATTR(20, 2) Texture2D<float4> txFaceShadowAtlas A51_SAMPLED_TEXTURE_BIND(20, 2);
#endif
A51_SAMPLED_TEXTURE_ATTR(4, 3) Texture2DArray<float> txProjectionAtlas A51_SAMPLED_TEXTURE_BIND(4, 3);
#if A51_RECIPE_SHADOWS
A51_SAMPLED_TEXTURE_ATTR(29, 4) Texture2D<float> txFaceShadowDepthAtlas A51_SAMPLED_TEXTURE_BIND(29, 4);
A51_SAMPLER_ATTR(20, 2) SamplerState samFaceShadow A51_SAMPLER_BIND(20, 2);
A51_SAMPLER_ATTR(9, 4) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 4);
#endif
A51_SAMPLER_ATTR(4, 3) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 3);
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position      : TEXCOORD0;
    float4 Color         : TEXCOORD1;
    float2 UV            : TEXCOORD2;
    float3 Normal        : TEXCOORD3;
    uint   LightingIndex : TEXCOORD4;
};

//------------------------------------------------------------------------------

#define GEOM_PIXEL_INPUT A51_DECAL_PIXEL_INPUT

#if A51_DECAL_LIT
//==============================================================================
//  FUNCTIONS
//==============================================================================

uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT Input )
{
    return (A51_RECIPE_SHADOWS ? GEOM_INST_FLAG_RECEIVE_LOCAL_SHADOW : 0u) |
        GEOM_INST_FLAG_PROJ_LIGHT | GEOM_INST_FLAG_PROJ_SHADOW;
}

//==============================================================================

uint GeomGetLightingIndex( GEOM_PIXEL_INPUT Input )
{
    return Input.LightingIndex;
}

//==============================================================================

#if A51_RECIPE_LIGHTING
uint GeomGetLightCount( GEOM_PIXEL_INPUT Input )
{
    return min( GeomAmbient[GeomGetLightingIndex( Input )].LightCount, (uint)MAX_GEOM_LIGHTS );
}

//==============================================================================

float4 GeomGetLightVec( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomLighting[GeomGetLightingIndex( Input )].LightVec[LightIndex];
}

//==============================================================================

float4 GeomGetLightCol( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomLighting[GeomGetLightingIndex( Input )].LightCol[LightIndex];
}

//==============================================================================

float2 GeomGetLightRadial( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomLighting[GeomGetLightingIndex( Input )].LightRadial[LightIndex].xy;
}

//==============================================================================

float4 GeomGetLightDir( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomLighting[GeomGetLightingIndex( Input )].LightDir[LightIndex];
}

//==============================================================================

float4 GeomGetLightCone( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomLighting[GeomGetLightingIndex( Input )].LightCone[LightIndex];
}

//==============================================================================

#if A51_RECIPE_COOKIES
float4 GeomGetLightCookieU( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomProjectionLighting[GeomGetLightingIndex( Input )].LightCookieU[LightIndex];
}

//==============================================================================

float4 GeomGetLightCookieV( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomProjectionLighting[GeomGetLightingIndex( Input )].LightCookieV[LightIndex];
}

//==============================================================================

float4 GeomGetLightCookieAtlas( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomProjectionLighting[GeomGetLightingIndex( Input )].LightCookieAtlas[LightIndex];
}

//==============================================================================

float4 GeomGetLightCookieTileBounds( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomProjectionLighting[GeomGetLightingIndex( Input )].LightCookieTileBounds[LightIndex];
}

//==============================================================================

float4 GeomGetLightCookieInfo( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomProjectionLighting[GeomGetLightingIndex( Input )].LightCookieInfo[LightIndex];
}

//==============================================================================

#endif

#if A51_RECIPE_SHADOWS
uint GeomGetLightShadowIndex( GEOM_PIXEL_INPUT Input, uint LightIndex )
{
    return GeomProjectionLighting[GeomGetLightingIndex( Input )].LightShadowIndex[LightIndex];
}

//==============================================================================

#endif

#endif

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT Input )
{
    return GeomAmbient[GeomGetLightingIndex( Input )].LightAmbCol;
}

#define GEOM_HAS_VERTEX_COLOR 0

//------------------------------------------------------------------------------

#include "common/geom_pixel_lighting.hlsl"
#if A51_RECIPE_SHADOWS
#include "common/geom_local_shadow_maps.hlsl"
#endif
#endif

//==============================================================================
//  SHADERS
//==============================================================================

GEOM_PIXEL_INPUT VSMain( VS_INPUT Input )
{
    GEOM_PIXEL_INPUT Output;
    Output.Position = mul( WorldToClip, float4( Input.Position, 1.0f ) );
    Output.Color = Input.Color.bgra;
    Output.UV = Input.UV;
    #if A51_DECAL_LIT
    Output.LightingIndex = Input.LightingIndex;
    Output.WorldPos = Input.Position;
    Output.Normal = Input.Normal;
    #endif
    #if A51_DECAL_ENV
    Output.ViewVector = Input.Position - CameraPosition.xyz;
    #endif
    #if A51_DECAL_FOG
    Output.ViewDepth = mul( WorldToView, float4( Input.Position, 1.0f ) ).z;
    #endif
    return Output;
}

//==============================================================================

float4 PSMain( GEOM_PIXEL_INPUT Input ) : SV_Target0
{
    static const uint DECAL_BLEND_ALPHA = 0u;
    static const uint DECAL_BLEND_ADDITIVE = 1u;
    static const uint DECAL_BLEND_SUBTRACTIVE = 2u;
    static const uint DECAL_BLEND_INTENSITY = 3u;
    static const uint DECAL_DRAW_FLAG_GLOW = 1u;
    static const uint DECAL_DRAW_FLAG_ENV_MAPPED = 2u;

    float4 TextureSample = txDecal.Sample( samDecal, Input.UV );
    float4 Surface = TextureSample * Input.Color;

    const bool IsIntensity = A51_DECAL_BLEND == DECAL_BLEND_INTENSITY;
    const bool IsAdditive = A51_DECAL_BLEND == DECAL_BLEND_ADDITIVE;

    #if A51_DECAL_LIT
    GeomWorldGradients Gradients;
    Gradients.Dx = ddx( Input.WorldPos );
    Gradients.Dy = ddy( Input.WorldPos );
#if A51_RECIPE_LIGHTING
    float3 GeometricNormal = cross( Gradients.Dx, Gradients.Dy );
    float NormalLengthSquared = dot( GeometricNormal, GeometricNormal );
    if( NormalLengthSquared > 1e-12f )
    {
        GeometricNormal *= rsqrt( NormalLengthSquared );
        if( dot( GeometricNormal, Input.Normal ) < 0.0f )
        {
            GeometricNormal = -GeometricNormal;
        }
    }
    else
    {
        GeometricNormal = normalize( Input.Normal );
    }
    Input.Normal = GeometricNormal;

#endif

    if( !IsIntensity && !IsAdditive )
    {
#if A51_RECIPE_LIGHTING
        GeomLightingResult Lighting =
            GeomComputeLighting( Input, GeomGetMaterialFlags( Input ), Surface.a, GeometricNormal, Gradients );
        Surface.rgb *= Lighting.Diffuse;
        Surface.rgb += Lighting.Specular;
#else
        Surface.rgb *= GeomGetLightAmbCol( Input ).rgb;
#endif
        Surface.rgb = ApplyProjLights( Surface.rgb, Input.WorldPos, Gradients );
        Surface.rgb = ApplyProjShadows( Surface.rgb, Input.WorldPos, Gradients );
    }

    #endif
    #if A51_DECAL_ENV
    if( ( A51_DECAL_ENV ? DECAL_DRAW_FLAG_ENV_MAPPED : 0u ) != 0u )
    {
        float3 CubeDirection = normalize( Input.ViewVector );
        float3 Environment = txEnvironmentCube.Sample( samEnvironmentCube, CubeDirection ).rgb;
        float EnvBlend = saturate( TextureSample.a * EnvParams.w );
        if( IsAdditive )
        {
            Surface.rgb += Environment * EnvBlend;
        }
        else
        {
            Surface.rgb = lerp( Surface.rgb, Environment, EnvBlend );
        }
    }

    #endif
    if( IsIntensity )
    {
        // Intensity textures encode the multiplier in RGB; vertex alpha carries the fade.
        Surface.rgb = lerp( 0.5f, Surface.rgb, saturate(Input.Color.a) );
    }
    else
    {
        Surface.rgb = saturate( Surface.rgb );
    }

    #if A51_DECAL_FOG
    const float LinearDepth = saturate( ( Input.ViewDepth - FogDepth.x ) /
                                        max( FogDepth.y - FogDepth.x, 1e-5f ) );
    float4 Color = IsIntensity ? ApplyFogToIntensity( Surface, LinearDepth ) : ApplyFogToColor( Surface, LinearDepth );
    #else
    float4 Color = Surface;
    #endif
    const bool NeedsPremultipliedColor = ( A51_DECAL_BLEND == DECAL_BLEND_ALPHA ) ||
                                         ( A51_DECAL_BLEND == DECAL_BLEND_ADDITIVE ) ||
                                         ( A51_DECAL_BLEND == DECAL_BLEND_SUBTRACTIVE );
    if( NeedsPremultipliedColor )
    {
        Color.rgb *= Surface.a;
    }
    Color.a = ( A51_DECAL_BLEND == DECAL_BLEND_ALPHA ) ? Surface.a
                  : ( ( A51_DECAL_GLOW ? DECAL_DRAW_FLAG_GLOW : 0u ) != 0u )
#if A51_DECAL_FOG
                        ? ApplyFogToEmission( Surface, LinearDepth ).a
#else
                        ? Surface.a
#endif
                        : 0.0f;
    return Color;
}
