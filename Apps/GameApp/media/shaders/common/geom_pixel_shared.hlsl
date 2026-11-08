//==============================================================================
//
//  geom_pixel_shared.hlsl
//
//  Shared shading routine for rigid and skinned geometry.
//
//==============================================================================

#ifndef GEOM_PIXEL_SHARED_HLSL
#define GEOM_PIXEL_SHARED_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/geom_pixel_core.hlsl"
#include "common/geom_pixel_instances.hlsl"
#if A51_RECIPE_DISTORTION
    #include "common/geom_pixel_distortion.hlsl"
#endif
#include "common/geom_pixel_surface.hlsl"
#if A51_RECIPE_LIGHTING || A51_RECIPE_PROJECTION
    #include "common/geom_pixel_lighting.hlsl"
#endif
#include "common/geom_pixel_shading.hlsl"

//==============================================================================
#endif // GEOM_PIXEL_SHARED_HLSL
//==============================================================================
