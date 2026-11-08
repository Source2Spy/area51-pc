//==============================================================================
//
//  geom_textures.hlsl
//
//  Shared texture and sampler bindings for geometry shaders.
//
//==============================================================================

#ifndef GEOM_TEXTURES_HLSL
#define GEOM_TEXTURES_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

#if A51_GEOM_SCREEN_AO
    A51_SAMPLED_TEXTURE_ATTR(30, 10) Texture2D<float> txScreenAO A51_SAMPLED_TEXTURE_BIND(30, 10);
    A51_SAMPLER_ATTR(30, 10) SamplerState samScreenAO A51_SAMPLER_BIND(30, 10);
#endif

#if (!A51_GEOM_DEPTH_ONLY && !A51_RECIPE_GLOW_MASK && !A51_RECIPE_DISTORTION) || A51_GEOM_ALPHA_TEST
    A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D txDiffuse A51_SAMPLED_TEXTURE_BIND(0, 0);
#endif
#if A51_RECIPE_DETAIL && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D txDetail A51_SAMPLED_TEXTURE_BIND(1, 1);
#endif
#if (A51_RECIPE_ENV == 1) && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLED_TEXTURE_ATTR(2, 2) Texture2D txEnvironment A51_SAMPLED_TEXTURE_BIND(2, 2);
#endif
#if (A51_RECIPE_ENV == 2) && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLED_TEXTURE_ATTR(3, 3) TextureCube txEnvironmentCube A51_SAMPLED_TEXTURE_BIND(3, 3);
#endif
#if A51_RECIPE_DISTORTION && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLED_TEXTURE_ATTR(21, 5) Texture2D txDistortionScene A51_SAMPLED_TEXTURE_BIND(21, 5);
#endif
#if A51_RECIPE_SHADOWS && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLED_TEXTURE_ATTR(20, A51_GEOM_FACE_SHADOW_BINDING) Texture2D<float4> txFaceShadowAtlas A51_SAMPLED_TEXTURE_BIND(20, A51_GEOM_FACE_SHADOW_BINDING);
#endif
#if (A51_RECIPE_COOKIES || A51_RECIPE_PROJECTION) && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLED_TEXTURE_ATTR(4, A51_GEOM_PROJECTION_BINDING) Texture2DArray<float> txProjectionAtlas A51_SAMPLED_TEXTURE_BIND(4, A51_GEOM_PROJECTION_BINDING);
#endif
#if A51_RECIPE_SHADOWS && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLED_TEXTURE_ATTR(29, A51_GEOM_FACE_DEPTH_BINDING) Texture2D<float> txFaceShadowDepthAtlas A51_SAMPLED_TEXTURE_BIND(29, A51_GEOM_FACE_DEPTH_BINDING);
#endif

#if defined(A51_SHADER_BINDING_SDL)
#if (!A51_GEOM_DEPTH_ONLY && !A51_RECIPE_GLOW_MASK && !A51_RECIPE_DISTORTION) || A51_GEOM_ALPHA_TEST
    A51_SAMPLER_ATTR(0, 0) SamplerState samDiffuse A51_SAMPLER_BIND(0, 0);
#endif
#if A51_RECIPE_DETAIL && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLER_ATTR(1, 1) SamplerState samDetail A51_SAMPLER_BIND(1, 1);
#endif
#if (A51_RECIPE_ENV == 1) && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLER_ATTR(2, 2) SamplerState samEnvironment A51_SAMPLER_BIND(2, 2);
#endif
#if (A51_RECIPE_ENV == 2) && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLER_ATTR(3, 3) SamplerState samEnvironmentCube A51_SAMPLER_BIND(3, 3);
#endif
#if A51_RECIPE_DISTORTION && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLER_ATTR(21, 5) SamplerState samDistortion A51_SAMPLER_BIND(21, 5);
#endif
#if A51_RECIPE_SHADOWS && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLER_ATTR(20, A51_GEOM_FACE_SHADOW_BINDING) SamplerState samFaceShadow A51_SAMPLER_BIND(20, A51_GEOM_FACE_SHADOW_BINDING);
#endif
#if (A51_RECIPE_COOKIES || A51_RECIPE_PROJECTION) && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLER_ATTR(4, A51_GEOM_PROJECTION_BINDING) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, A51_GEOM_PROJECTION_BINDING);
#endif
#if A51_RECIPE_SHADOWS && !A51_GEOM_DEPTH_ONLY
    A51_SAMPLER_ATTR(9, A51_GEOM_FACE_DEPTH_BINDING) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, A51_GEOM_FACE_DEPTH_BINDING);
#endif
#else
    A51_SAMPLER_ATTR(0, 0) SamplerState samLinear A51_SAMPLER_BIND(0, 0);
    A51_SAMPLER_ATTR(4, 4) SamplerState samProjectionAtlas A51_SAMPLER_BIND(4, 4);
    A51_SAMPLER_ATTR(8, 8) SamplerState samFaceShadow A51_SAMPLER_BIND(8, 8);
    A51_SAMPLER_ATTR(9, 9) SamplerState samFaceShadowDepth A51_SAMPLER_BIND(9, 9);
    
    #define samDiffuse         samLinear
    #define samDetail          samLinear
    #define samEnvironment     samLinear
    #define samEnvironmentCube samLinear
    #define samDistortion      samLinear
#endif

#if A51_RECIPE_DAMAGE
A51_SAMPLED_TEXTURE_ATTR(10, 9) Texture2D txDamage A51_SAMPLED_TEXTURE_BIND(10, 9);
A51_SAMPLER_ATTR(10, 9) SamplerState samDamage A51_SAMPLER_BIND(10, 9);
#endif

//==============================================================================
#endif // GEOM_TEXTURES_HLSL
//==============================================================================
