//==============================================================================
//
//  geom_pixel_instances.hlsl
//
//  Geometry instance accessors for rigid and skinned shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_INSTANCES_HLSL
#define GEOM_PIXEL_INSTANCES_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

uint GeomGetMaterialFlags( GEOM_PIXEL_INPUT Input )
{
#if A51_SURFACE_INTERFACE & 64
    return A51_RECIPE_FLAGS | (Input.DrawFlags & (GEOM_INST_FLAG_FILTER_LIGHT |
        (A51_RECIPE_PROJECTION ? (GEOM_INST_FLAG_PROJ_LIGHT | GEOM_INST_FLAG_PROJ_SHADOW) : 0u)));
#else
    return A51_RECIPE_FLAGS;
#endif
}

float GeomGetFadeAlpha( GEOM_PIXEL_INPUT Input )
{
    return saturate( Input.FadeAlpha );
}

uint GeomGetLightingIndex( GEOM_PIXEL_INPUT Input )
{
#if A51_SURFACE_INTERFACE & 128
    return Input.LightingIndex;
#else
    return 0;
#endif
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
#endif

//==============================================================================

#endif

float4 GeomGetLightAmbCol( GEOM_PIXEL_INPUT Input )
{
    return GeomAmbient[GeomGetLightingIndex( Input )].LightAmbCol;
}

//==============================================================================
#endif // GEOM_PIXEL_INSTANCES_HLSL
//==============================================================================
