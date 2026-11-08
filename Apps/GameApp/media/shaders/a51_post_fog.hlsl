//==============================================================================
//
//  a51_post_fog.hlsl
//
//  Depth-based fog composite shader.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#define A51_FOG_CONSTANTS_SDL_BINDING 0
#define A51_FOG_PALETTE_SDL_BINDING 1
#include "common/fog_constants.hlsl"
#include "common/fog_functions.hlsl"

//==============================================================================
//  RESOURCES
//==============================================================================

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D<float> SceneDepthSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLER_ATTR(0, 0) SamplerState samSceneDepthSource A51_SAMPLER_BIND(0, 0);

//==============================================================================
//  FUNCTIONS
//==============================================================================

float SampleLinearDepth( float2 UV )
{
    return saturate( SceneDepthSource.SampleLevel( samSceneDepthSource, UV, 0.0f ) );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PSMain( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float  LinearDepth = SampleLinearDepth( UV );
    return GetFogSample( LinearDepth );
}
