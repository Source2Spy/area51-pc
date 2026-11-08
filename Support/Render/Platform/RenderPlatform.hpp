//=========================================================================
//
//  RenderPlatform.hpp
//
//=========================================================================

#ifndef RENDER_PLATFORM_HPP
#define RENDER_PLATFORM_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Render/Render.hpp"
#include "Render/DecalBatch.hpp"
#include "Render/PrimitiveBatch.hpp"
#include "Render/GeomDraw.hpp"
#include "Render/RenderLighting.hpp"
#include "x_array.hpp"

//=========================================================================
//  INTERNAL PLATFORM ENTRY POINTS
//=========================================================================

// Internal adapters for the public render API.

namespace render
{
namespace platform
{

//=========================================================================
//  INITIALIZATION
//=========================================================================

xbool   platform_Init( void ) X_SECTION( init );
void    platform_Kill( void ) X_SECTION( init );
xhandle platform_RegisterSkinGeom( skin_geom const& Geom ) X_SECTION( init );
xhandle platform_RegisterRigidGeom( rigid_geom const& Geom ) X_SECTION( init );
void    platform_UnregisterSkinGeom( xhandle hGeom ) X_SECTION( init );
void    platform_UnregisterRigidGeom( xhandle hGeom ) X_SECTION( init );

//=========================================================================
//  FRAME STATE
//=========================================================================

void platform_ResetFrameState( void ) X_SECTION( render_infrequent );
void platform_ResetAfterException( void ) X_SECTION( render_infrequent );
void platform_BeginEngineFrame( void ) X_SECTION( render_infrequent );
xbool platform_IsFrameAborted( void ) X_SECTION( render_infrequent );
void platform_AbortFrame( char const* pReason ) X_SECTION( render_infrequent );

//=========================================================================
//  PRIMITIVE SUBMISSION
//=========================================================================

xbool platform_SubmitPrimitives( render::primitive_draw_desc const& Desc,
                                 matrix4 const&                     LocalToWorld,
                                 render::primitive_vertex const*    pVertices,
                                 s32                                nVertices,
                                 u16 const*                         pIndices,
                                 s32                                nIndices )
    X_SECTION( render_primitives );
xbool platform_SetDepthRect( irect const& Rect, f32 Depth ) X_SECTION( render_primitives );
xbool platform_SubmitDecalBatch( render::decal_draw_desc const& Desc,
                                 cubemap const*                  pCubeMap,
                                 render::decal_vertex const*     pVertices,
                                 s32                             nVertices,
                                 u16 const*                      pIndices,
                                 s32                             nIndices )
    X_SECTION( render_primitives );
void platform_ReserveDecalSubmissionCapacity( s32 nVertices, s32 nIndices, s32 nDraws )
    X_SECTION( render_infrequent );

//=========================================================================
//  LIGHTING
//=========================================================================

RenderLightingHandle platform_CalculateRigidLighting( matrix4 const& L2W, bbox const& WorldBBox,
                                                      xcolor Ambient = XCOLOR_BLACK ) X_SECTION( render_add );
RenderLightingHandle platform_CalculateSkinLighting( u32 Flags, matrix4 const& L2W, bbox const& BBox, xcolor Ambient )
    X_SECTION( render_add );
RenderLightingHandle platform_CalculateClothLighting( bbox const& WorldBBox, xcolor Ambient )
    X_SECTION( render_add );

//=========================================================================
//  SHADOW CASTING
//=========================================================================

void platform_ClearShadowSourceList( void ) X_SECTION( render_infrequent );
void platform_FinalizeShadowSourceList( void ) X_SECTION( render_infrequent );
void platform_AddPointShadowMapSource( matrix4 const& L2W, radian FOV, f32 LightRadius, f32 LightFalloff,
                                       s32 ShadowMapResolution, s32 ShadowPriority, f32 ShadowScore,
                                       s32 DynamicLightIndex )
    X_SECTION( render_add_shadow );
void platform_AddSpotShadowMapSource( matrix4 const& L2W, radian FOV, f32 LightRadius, f32 LightFalloff,
                                      s32 ShadowMapResolution, s32 ShadowPriority, f32 ShadowScore,
                                      s32 DynamicLightIndex )
    X_SECTION( render_add_shadow );
xbool platform_RenderShadowCasters( xarray<geom_draw_item const*> const& Draws,
                                   xarray<dynamic_geom_shadow_draw> const& DynamicDraws )
    X_SECTION( render_shadow );

//=========================================================================
//  POST-PROCESSING
//=========================================================================

void platform_SetSceneColorGrade( SceneColorGradeParams const& Params ) X_SECTION( render_post );
SceneColorGradeParams platform_GetSceneColorGrade( void ) X_SECTION( render_infrequent );
void platform_SetCustomFogPalette( texture::handle const& Texture, xbool ImmediateSwitch, s32 PaletteIndex )
    X_SECTION( render_post );
xcolor platform_GetFogValue( vector3 const& WorldPos, s32 PaletteIndex ) X_SECTION( render_infrequent );
void   platform_UpdatePostProcessing( f32 DeltaTime ) X_SECTION( update );
void   platform_AddScreenWarp( vector3 const& WorldPos, f32 Radius, f32 WarpAmount ) X_SECTION( render_post );
void   platform_MotionBlur( f32 Intensity ) X_SECTION( render_post );
void   platform_ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff ) X_SECTION( render_post );
void   platform_RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale ) X_SECTION( render_post );
void   platform_ZFogFilter( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
    X_SECTION( render_post );
void platform_ZFogFilter( render::post_falloff_fn Fn, s32 PaletteIndex ) X_SECTION( render_post );
void platform_MipFilter( s32 nFilters, f32 Offset, render::post_falloff_fn Fn, xcolor Color, f32 Param1,
                         f32 Param2, s32 PaletteIndex )
    X_SECTION( render_post );
void platform_MipFilter( s32 nFilters, f32 Offset, render::post_falloff_fn Fn,
                         texture::handle const& Texture, s32 PaletteIndex )
    X_SECTION( render_post );
void platform_NoiseFilter( xcolor Color ) X_SECTION( render_post );
void platform_ScreenFade( xcolor Color ) X_SECTION( render_post );

//=========================================================================
//  RENDER FRAME
//=========================================================================

xbool platform_BeginRenderFrame( void );
xbool platform_EndRenderFrame( void );

} // namespace platform
} // namespace render

//=========================================================================

#endif // RENDER_PLATFORM_HPP
//=========================================================================
