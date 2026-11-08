//==============================================================================
//
//  lighting_constants.hlsl
//
//  Shared geometry lighting storage.
//
//==============================================================================

#ifndef LIGHTING_CONSTANTS_HLSL
#define LIGHTING_CONSTANTS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================
//  TYPES
//==============================================================================

#include "a51_shader_contract.hlsl"

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #ifndef A51_GEOM_LIGHTING_BINDING
        #define A51_GEOM_LIGHTING_BINDING 11
    #endif
#else
    #ifndef A51_GEOM_LIGHTING_BINDING
        #define A51_GEOM_LIGHTING_BINDING 2
    #endif
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

#if A51_RECIPE_LIGHTING || !defined(A51_RECIPE_FLAGS)
    A51_STORAGE_BUFFER_ATTR(26, A51_GEOM_LIGHTING_BINDING)
    StructuredBuffer<GeomLightingData> GeomLighting A51_STORAGE_BUFFER_BIND(26, A51_GEOM_LIGHTING_BINDING);
    #ifndef A51_GEOM_LIGHT_PROJECTION_BINDING
        #define A51_GEOM_LIGHT_PROJECTION_BINDING 14
    #endif
    #if A51_RECIPE_COOKIES || A51_RECIPE_SHADOWS || !defined(A51_RECIPE_FLAGS)
    A51_STORAGE_BUFFER_ATTR(31, A51_GEOM_LIGHT_PROJECTION_BINDING)
    StructuredBuffer<GeomProjectionLightingData> GeomProjectionLighting A51_STORAGE_BUFFER_BIND(31, A51_GEOM_LIGHT_PROJECTION_BINDING);
    #endif
#endif

#ifndef A51_GEOM_AMBIENT_BINDING
    #define A51_GEOM_AMBIENT_BINDING 15
#endif

A51_STORAGE_BUFFER_ATTR(32, A51_GEOM_AMBIENT_BINDING)
StructuredBuffer<GeomAmbientData> GeomAmbient A51_STORAGE_BUFFER_BIND(32, A51_GEOM_AMBIENT_BINDING);

//==============================================================================
#endif // LIGHTING_CONSTANTS_HLSL
//==============================================================================
