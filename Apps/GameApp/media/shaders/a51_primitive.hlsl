//==============================================================================
//
//  a51_primitive.hlsl
//
//  Basic renderer-owned primitive shader.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"
#ifndef A51_PRIMITIVE_OUTPUT
    #error "Primitive shaders require an offline recipe"
#endif
#ifndef A51_PRIMITIVE_VIEW_DEPTH
    #if A51_PRIMITIVE_FOG || defined(A51_SCENE_DEPTH_TARGET)
    #define A51_PRIMITIVE_VIEW_DEPTH 1
#else
    #define A51_PRIMITIVE_VIEW_DEPTH 0
#endif
#endif
#include "common/pixel_structs.hlsl"
#ifndef A51_FOG_CONSTANTS_SDL_BINDING
    #define A51_FOG_CONSTANTS_SDL_BINDING 1
#endif
#define A51_FOG_PALETTE_SDL_BINDING 2
#if A51_PRIMITIVE_FOG || defined(A51_SCENE_DEPTH_TARGET)
    #define A51_FOG_PALETTE_ENABLED A51_PRIMITIVE_FOG
    #include "common/fog_constants.hlsl"
#endif
#if A51_PRIMITIVE_FOG
    #include "common/fog_functions.hlsl"
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(0, 0) cbuffer cbPrimitiveDraw A51_CBUFFER_BIND(0, 0)
{
    A51_ABI_FIELDS_PrimitiveDrawData
};

//------------------------------------------------------------------------------

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txPrimitive A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samPrimitive A51_SAMPLER_BIND(0, 0);

#if A51_PRIMITIVE_OUTPUT == 2
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txPrimitiveScene A51_SAMPLED_TEXTURE_BIND(1, 1);
A51_SAMPLER_ATTR(1, 1) SamplerState samPrimitiveScene A51_SAMPLER_BIND(1, 1);
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct VS_INPUT
{
    float3 Position : TEXCOORD0;
    float4 Color    : TEXCOORD1;
#if A51_PRIMITIVE_SPRITE
    float3 Right    : TEXCOORD2;
    float3 Up       : TEXCOORD3;
    float4 UVRect   : TEXCOORD4;
#else
    float2 UV       : TEXCOORD2;
#endif
};

//------------------------------------------------------------------------------

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
    float2 UV       : TEXCOORD0;
#if A51_PRIMITIVE_VIEW_DEPTH
    float  ViewDepth : TEXCOORD1;
#endif
};

//==============================================================================
//  SHADERS
//==============================================================================

PS_INPUT VSMain( VS_INPUT Input
#if A51_PRIMITIVE_SPRITE
                 , uint VertexIndex : SV_VertexID
#endif
               )
{
    PS_INPUT Output;

#if A51_PRIMITIVE_SPRITE
    static const float2 Corners[4] = {
        float2(0, 0), float2(0, 1), float2(1, 1), float2(1, 0)
    };
    float2 Corner = Corners[VertexIndex];
    float3 Position = Input.Position + Input.Right * (Corner.x * 2.0f - 1.0f) +
                                      Input.Up * (Corner.y * 2.0f - 1.0f);
    Output.UV = lerp( Input.UVRect.xy, Input.UVRect.zw, Corner );
#else
    float3 Position = Input.Position;
    Output.UV = Input.UV;
#endif
    Output.Position = mul( LocalToClip, float4( Position, 1.0f ) );
#if A51_PRIMITIVE_VIEW_DEPTH
    Output.ViewDepth = mul( LocalToView, float4( Position, 1.0f ) ).z;
#endif
    Output.Color    = Input.Color.bgra;

    return Output;
}

//==============================================================================

float4 ShadePrimitive( PS_INPUT Input )
{
    float4 Surface = txPrimitive.Sample( samPrimitive, Input.UV ) * Input.Color;

    static const uint PRIMITIVE_OUTPUT_DISTORTION = 2u;
    static const uint PRIMITIVE_OUTPUT_GLOW = 1u;
    static const uint PRIMITIVE_BLEND_ALPHA = 1u;
    static const uint PRIMITIVE_BLEND_ADDITIVE = 2u;
    static const uint PRIMITIVE_BLEND_SUBTRACTIVE = 3u;
    static const uint PRIMITIVE_BLEND_INTENSITY = 4u;

#if A51_PRIMITIVE_OUTPUT == 2
    const float2 InverseSize = PassDimensions.zw;
    const float2 ScreenUV = Input.Position.xy * InverseSize;
    const float2 Displacement = (Surface.rg * 2.0f - 1.0f) * Surface.a * DistortionScale * InverseSize;
    float4 Distorted = txPrimitiveScene.Sample(samPrimitiveScene, ScreenUV + Displacement);
    Distorted.rgb *= Surface.a;
    Distorted.a = Surface.a;
    return Distorted;
#else
    const bool IsIntensityBlend = A51_PRIMITIVE_BLEND == PRIMITIVE_BLEND_INTENSITY;
    if( IsIntensityBlend )
    {
        // Intensity uses RGB as a multiplier; vertex alpha fades it toward no change.
        Surface.rgb = lerp( 0.5f, Surface.rgb, saturate(Input.Color.a) );
    }
#if A51_PRIMITIVE_FOG
    float LinearDepth = saturate( ( Input.ViewDepth - FogDepth.x ) / max( FogDepth.y - FogDepth.x, 1e-5f ) );
#endif
    bool IsEmissionBlend = ( A51_PRIMITIVE_BLEND == PRIMITIVE_BLEND_ADDITIVE ) ||
                           ( A51_PRIMITIVE_BLEND == PRIMITIVE_BLEND_SUBTRACTIVE );
#if A51_PRIMITIVE_FOG
    float4 Color = IsIntensityBlend ? ApplyFogToIntensity(Surface, LinearDepth)
                                   : (IsEmissionBlend ? ApplySceneFogToEmission(Surface, LinearDepth)
                                                      : ApplyFogToColor(Surface, LinearDepth));
#else
    float4 Color = Surface;
#endif
    const bool NeedsPremultipliedColor = ( A51_PRIMITIVE_BLEND == PRIMITIVE_BLEND_ALPHA ) ||
                                         ( A51_PRIMITIVE_BLEND == PRIMITIVE_BLEND_ADDITIVE ) ||
                                         ( A51_PRIMITIVE_BLEND == PRIMITIVE_BLEND_SUBTRACTIVE );
    if( NeedsPremultipliedColor )
    {
        Color.rgb *= Surface.a;
    }
    Color.a = ( A51_PRIMITIVE_BLEND == PRIMITIVE_BLEND_ALPHA ) ? Surface.a
                  : ( A51_PRIMITIVE_OUTPUT == PRIMITIVE_OUTPUT_GLOW )
#if A51_PRIMITIVE_FOG
                        ? ApplyFogToEmission( Surface, LinearDepth ).a
#else
                        ? Surface.a
#endif
                        : 0.0f;
    return Color;
#endif
}

//==============================================================================

GeomSceneOutput PSMain( PS_INPUT Input )
{
    GeomSceneOutput Output;
    Output.Color = ShadePrimitive( Input );
#if defined(A51_SCENE_DEPTH_TARGET)
    Output.Depth = saturate( (Input.ViewDepth - FogDepth.x) / max( FogDepth.y - FogDepth.x, 1e-5f ) );
#endif
    return Output;
}
