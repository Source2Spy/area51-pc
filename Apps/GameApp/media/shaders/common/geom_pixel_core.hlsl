//==============================================================================
//
//  geom_pixel_core.hlsl
//
//  Shared geometry pixel structs and depth helpers.
//
//==============================================================================

#ifndef GEOM_PIXEL_CORE_HLSL
#define GEOM_PIXEL_CORE_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#if A51_RECIPE_FOG
#include "common/fog_constants.hlsl"
#include "common/fog_functions.hlsl"
#endif

//==============================================================================
//  TYPES
//==============================================================================

#include "geom_world_gradients.hlsl"

struct GeomSurfaceGradients
{
    float3 NormalDx;
    float3 NormalDy;
};

//-----------------------------------------------------------------------------

struct GeomDiffuseResult
{
    A51Color4 Color;
    float4 Sample; // Alpha-test thresholds must use the full-precision texture sample.
    float  Alpha;
    float  Damage;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

float3 GeomNormalizedGradient( float3 Direction, float3 Gradient )
{
    const float InverseLength = rsqrt( dot( Direction, Direction ) );
    const float3 Normal = Direction * InverseLength;
    return ( Gradient - Normal * dot( Normal, Gradient ) ) * InverseLength;
}

//==============================================================================

static
float4 GeomEncodeLinearDepth( GEOM_PIXEL_INPUT Input )
{
#if A51_SURFACE_INTERFACE & 32
    const float  InvRange    = rcp( max( FarZ - NearZ, 1e-5f ) );
    const float  LinearDepth = saturate( (Input.ViewDepth - NearZ) * InvRange );
    return LinearDepth.xxxx;
#else
    return 0;
#endif
}

//==============================================================================

// Lens filter is applied after regular lighting, before fog and bloom.
static
float3 GeomApplyFilterLight( float3 Color, uint MaterialFlags )
{
    return ( ( MaterialFlags & GEOM_INST_FLAG_FILTER_LIGHT ) != 0u ) && ( FilterLightColor.w > 0.5f )
        ? ( Color * FilterLightColor.rgb )
        : Color;
}

//==============================================================================
#if A51_GEOM_SCREEN_AO
float GeomAmbientVisibility( GEOM_PIXEL_INPUT Input )
{
    [branch]
    if( ScreenAOParams.z > 0.0f )
    {
        return txScreenAO.SampleLevel( samScreenAO, Input.Pos.xy * ScreenAOParams.xy, 0 );
    }

    return 1.0f;
}

//==============================================================================
#endif

#endif // GEOM_PIXEL_CORE_HLSL
//==============================================================================
