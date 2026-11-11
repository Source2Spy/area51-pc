//==============================================================================
//
//  shadow_buffers.hlsl
//
//  Shadow map matrices shared by geometry shaders.
//
//==============================================================================

#ifndef SHADOW_BUFFERS_HLSL
#define SHADOW_BUFFERS_HLSL

//==============================================================================
//  DEFINES
//==============================================================================

#define MAX_SHADOW_SOURCES 64
#define MAX_SHADOW_LIGHTS 8
#define POINT_SHADOW_FACE_COUNT 6

//==============================================================================
//  INCLUDES
//==============================================================================

#include "shader_bindings.hlsl"

//==============================================================================

#if defined(A51_SHADER_BINDING_SDL) && defined(A51_SHADER_STAGE_PIXEL)
    #ifndef A51_GEOM_SHADOW_MATRICES_BINDING
        #define A51_GEOM_SHADOW_MATRICES_BINDING 12
    #endif
    #ifndef A51_GEOM_SHADOW_DATA_BINDING
        #define A51_GEOM_SHADOW_DATA_BINDING 13
    #endif
#else
    #ifndef A51_GEOM_SHADOW_MATRICES_BINDING
        #define A51_GEOM_SHADOW_MATRICES_BINDING 3
    #endif
    #ifndef A51_GEOM_SHADOW_DATA_BINDING
        #define A51_GEOM_SHADOW_DATA_BINDING 4
    #endif
#endif

//==============================================================================
//  RESOURCES
//==============================================================================

A51_STORAGE_BUFFER_ATTR(27, A51_GEOM_SHADOW_MATRICES_BINDING)
StructuredBuffer<float4x4> FaceShadowMatrices
    A51_STORAGE_BUFFER_BIND(27, A51_GEOM_SHADOW_MATRICES_BINDING);

A51_STORAGE_BUFFER_ATTR(28, A51_GEOM_SHADOW_DATA_BINDING)
StructuredBuffer<float4> ShadowMapData
    A51_STORAGE_BUFFER_BIND(28, A51_GEOM_SHADOW_DATA_BINDING);

#define SHADOW_FACE_POS_OFFSET        0u
#define SHADOW_FACE_DIR_OFFSET       ( SHADOW_FACE_POS_OFFSET + MAX_SHADOW_SOURCES )
#define SHADOW_FACE_DATA_OFFSET      ( SHADOW_FACE_DIR_OFFSET + MAX_SHADOW_SOURCES )
#define SHADOW_FACE_PROJECTION_OFFSET ( SHADOW_FACE_DATA_OFFSET + MAX_SHADOW_SOURCES )
#define SHADOW_FACE_RECT_OFFSET      ( SHADOW_FACE_PROJECTION_OFFSET + MAX_SHADOW_SOURCES )
#define SHADOW_POINT_POS_OFFSET      ( SHADOW_FACE_RECT_OFFSET + MAX_SHADOW_SOURCES )
#define SHADOW_POINT_DATA_OFFSET     ( SHADOW_POINT_POS_OFFSET + MAX_SHADOW_LIGHTS )
#define SHADOW_POINT_PARAMS_OFFSET   ( SHADOW_POINT_DATA_OFFSET + MAX_SHADOW_LIGHTS )
#define SHADOW_COUNTS_OFFSET         ( SHADOW_POINT_PARAMS_OFFSET + MAX_SHADOW_LIGHTS )
#define SHADOW_PARAMS_OFFSET         ( SHADOW_COUNTS_OFFSET + 1u )
#define SHADOW_FILTER_PARAMS_OFFSET  ( SHADOW_PARAMS_OFFSET + 1u )

float4 GetFaceShadowLightPosRadius( uint Index )
{
    return ShadowMapData[SHADOW_FACE_POS_OFFSET + Index];
}

//==============================================================================

float4 GetFaceShadowLightDirRadial( uint Index )
{
    return ShadowMapData[SHADOW_FACE_DIR_OFFSET + Index];
}

//==============================================================================

float4 GetFaceShadowLightData( uint Index )
{
    return ShadowMapData[SHADOW_FACE_DATA_OFFSET + Index];
}

//==============================================================================

float4 GetFaceShadowProjectionData( uint Index )
{
    return ShadowMapData[SHADOW_FACE_PROJECTION_OFFSET + Index];
}

//==============================================================================

float4 GetFaceShadowAtlasRect( uint Index )
{
    return ShadowMapData[SHADOW_FACE_RECT_OFFSET + Index];
}

//==============================================================================

float4 GetPointShadowLightPosRadius( uint Index )
{
    return ShadowMapData[SHADOW_POINT_POS_OFFSET + Index];
}

//==============================================================================

float4 GetPointShadowLightData( uint Index )
{
    return ShadowMapData[SHADOW_POINT_DATA_OFFSET + Index];
}

//==============================================================================

float4 GetPointShadowLightParams( uint Index )
{
    return ShadowMapData[SHADOW_POINT_PARAMS_OFFSET + Index];
}

//==============================================================================

uint GetFaceShadowCount()
{
    return asuint( ShadowMapData[SHADOW_COUNTS_OFFSET].x );
}

//==============================================================================

uint GetPointShadowLightCount()
{
    return asuint( ShadowMapData[SHADOW_COUNTS_OFFSET].y );
}

//==============================================================================

float4 GetShadowParams()
{
    return ShadowMapData[SHADOW_PARAMS_OFFSET];
}

//==============================================================================

float4 GetShadowFilterParams()
{
    return ShadowMapData[SHADOW_FILTER_PARAMS_OFFSET];
}

//==============================================================================

#endif // SHADOW_BUFFERS_HLSL
//==============================================================================
