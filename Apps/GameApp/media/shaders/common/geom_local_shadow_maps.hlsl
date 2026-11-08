//==============================================================================
//
//  geom_local_shadow_maps.hlsl
//
//  Shared local-light shadow helpers.
//
//==============================================================================

#ifndef GEOM_LOCAL_SHADOW_MAPS_HLSL
#define GEOM_LOCAL_SHADOW_MAPS_HLSL

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/geom_shadow_common.hlsl"
#include "common/geom_shadow_spot.hlsl"
#include "common/geom_shadow_point.hlsl"

//==============================================================================
//  FUNCTIONS
//==============================================================================

float GeomComputeLocalLightShadowVisibility( GEOM_PIXEL_INPUT Input,
                                             uint LightIndex,
                                             float3 GeometricNormal,
                                             float3 PointToLightDir,
                                             float LightDistance )
{
    if( ( GeomGetMaterialFlags( Input ) & GEOM_INST_FLAG_RECEIVE_LOCAL_SHADOW ) == 0u )
    {
        return 1.0f;
    }

    const float4 LightDir = GeomGetLightDir( Input, LightIndex );
    if( GeomIsCharFillLight( LightDir ) )
    {
        return 1.0f;
    }

    const uint ShadowIndex = GeomGetLightShadowIndex( Input, LightIndex );
    if( ShadowIndex == 0xFFFFFFFFu )
    {
        return 1.0f;
    }

    if( LightDir.w >= 0.5f )
    {
        if( ( ShadowIndex >= GetFaceShadowCount() ) || FaceShadowIsPointFace( ShadowIndex ) )
        {
            return 1.0f;
        }

        return SampleFaceShadowSource( ShadowIndex, Input.WorldPos, GeometricNormal, PointToLightDir, LightDistance );
    }

    if( ShadowIndex >= GetPointShadowLightCount() )
    {
        return 1.0f;
    }

    return SamplePointShadowLight( ShadowIndex, Input.WorldPos, GeometricNormal, PointToLightDir, LightDistance );
}

//==============================================================================
#endif // GEOM_LOCAL_SHADOW_MAPS_HLSL
//==============================================================================
