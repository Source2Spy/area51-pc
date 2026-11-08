//=========================================================================
//
//  Render.hpp
//
//=========================================================================

#ifndef RENDER_HPP
#define RENDER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "x_time.hpp"

#include "../ResourceMgr/ResourceMgr.hpp"

#include "Material.hpp"
#include "GeomDraw.hpp"
#include "PrimitiveBatch.hpp"
#include "PrimitiveDebug.hpp"
#include "RigidGeom.hpp"
#include "SkinGeom.hpp"
#include "RenderScope.hpp"

//=========================================================================
//  TYPES
//=========================================================================

namespace render
{

//-------------------------------------------------------------------------
//  DEBUG OPTIONS
//-------------------------------------------------------------------------

#ifndef X_RETAIL
struct debug_options
{
    xbool RenderRigidOnly;
    xbool RenderSkinOnly;
    xbool RenderClippedOnly;
    xbool RenderShadowedOnly;
    xbool DumpFrameGraph;
    xbool ShowLightingStats;
    xbool DisableLocalLights;
    xbool DisableDynamicLights;
    xbool DisableCharLights;
    xbool DisableFadingLights;
    xbool DisableLightCookies;
    xbool DisableProjectedLights;
    xbool DisableProjectedShadows;
    xbool DisableShadowMaps;
};
#endif

//=========================================================================
//  LIFETIME
//=========================================================================

void  Init      ( void ) X_SECTION( init );
void  Kill      ( void ) X_SECTION( init );
void  BeginFrame( void ) X_SECTION( render_infrequent );
xbool EndFrame  ( xbool RenderEmptyFrame = TRUE ) X_SECTION( render_infrequent );
void  Update    ( f32 DeltaTime ) X_SECTION( update );
geom_render_pass ClassifyGeomPass( material const& Material, u32 Flags, xbool FadingAlpha );

//=========================================================================
//  CONSTANTS
//=========================================================================

//-------------------------------------------------------------------------
//  RENDER FLAGS
//-------------------------------------------------------------------------

// Public geometry submission options.
using render_flags   = u32;

enum render_flag_bits : u32
{
    WIREFRAME                 = 0x00000001,
    WIREFRAME_NO_CULL         = 0x00000002,
    GLOWING                   = 0x00000010,
    FADING_ALPHA              = 0x00000020,
    CLIPPED                   = 0x00000040, // Request frustum culling.
    FIRST_PERSON              = 0x00000080,
    FORCE_LAST                = 0x01000000,
    DISABLE_PROJ_LIGHT        = 0x02000000,
    DISABLE_FILTER_LIGHT      = 0x04000000,
    DISABLE_PROJ_SHADOWS      = 0x08000000
};

// Debug display modes. They are not lighting or material state.
enum render_debug_flag_bits : u32
{
    DEBUG_RENDER_PULSED = 0x00000004,
};

//=========================================================================
//  GEOMETRY REGISTRATION
//=========================================================================

using GeometryInstanceHandle = xhandle;

GeometryInstanceHandle RegisterRigidInstance( rigid_geom& geom ) X_SECTION( init );
void                   UnregisterRigidInstance( GeometryInstanceHandle hInst ) X_SECTION( init );
GeometryInstanceHandle RegisterSkinInstance( skin_geom& geom ) X_SECTION( init );
void                   UnregisterSkinInstance( GeometryInstanceHandle hInst ) X_SECTION( init );
geom const*            GetGeom( GeometryInstanceHandle hInst ) X_SECTION( init );

//=========================================================================
//  SHADOW CASTING
//=========================================================================


//=========================================================================
//  SCENE SUBMISSION
//=========================================================================

xbool BeginRenderScope           ( void ) X_SECTION( render_infrequent );
xbool SubmitRenderScope          ( void ) X_SECTION( render_geometry );
xbool EndRenderScope             ( void ) X_SECTION( render_infrequent );
void  ResetAfterException  ( void ) X_SECTION( render_infrequent );

// Submit geometry through the active RenderPipeline scope.

//=========================================================================
//  RENDER UTILITIES
//=========================================================================

xbool SetDepthRect( irect const& rect, f32 depth ) X_SECTION( render_primitives );
s32   GetHardwareBufferSize( void ) X_SECTION( render_infrequent );

//=========================================================================
//  MATERIAL AND TEXTURE ACCESS
//=========================================================================

material& GetMaterial( GeometryInstanceHandle hInst, s32 iSubMesh ) X_SECTION( render_infrequent );
texture*  GetVTexture( geom const* pGeom, s32 iMaterial, s32 vTextureMask ) X_SECTION( render_infrequent );
void      SetAreaCubeMap( cubemap::handle const& CubeMap ) X_SECTION( render_infrequent );

//=========================================================================
//  POST-PROCESSING
//=========================================================================

// Persistent scene grading runs after post-processing and before UI.
struct SceneColorGradeParams
{
    f32    Brightness;
    f32    Contrast;

    SceneColorGradeParams()
        : Brightness( 0.0f ), Contrast( 1.0f )
    {
    }

    xbool IsNeutral( void ) const
    {
        return ( Brightness == 0.0f ) && ( Contrast == 1.0f );
    }
};

void SetSceneColorGrade( SceneColorGradeParams const& Params ) X_SECTION( render_post );
SceneColorGradeParams GetSceneColorGrade( void ) X_SECTION( render_infrequent );

enum post_falloff_fn
{
    FALLOFF_CONSTANT = 0,
    FALLOFF_LINEAR,
    FALLOFF_EXP,
    FALLOFF_EXP2,
    FALLOFF_CUSTOM,
};

// Colors use 128 as 1.0 and 255 as 2.0. The frame graph orders post requests.

void SetCustomFogPalette( texture::handle const& texture, xbool ImmediateSwitch, s32 PaletteIndex )
    X_SECTION( render_post );
xcolor GetFogValue( vector3 const& WorldPos, s32 PaletteIndex ) X_SECTION( render_infrequent );

void AddScreenWarp( vector3 const& WorldPos, f32 radius, f32 WarpAmount ) X_SECTION( render_post );
void MotionBlur( f32 intensity ) X_SECTION( render_post );
void ApplySelfIllumGlows( f32 MotionBlurIntensity = 0.0f, s32 GlowCutoff = 255 ) X_SECTION( render_post );
void RadialBlur( f32 zoom, radian angle, f32 AlphaSub, f32 AlphaScale ) X_SECTION( render_post );
void ZFogFilter( post_falloff_fn fn, xcolor color, f32 param1, f32 param2 ) X_SECTION( render_post );
void ZFogFilter( post_falloff_fn fn, s32 PaletteIndex ) X_SECTION( render_post );
void MipFilter( s32 nFilters, f32 offset, post_falloff_fn fn, xcolor color, f32 param1, f32 param2,
                s32 PaletteIndex ) X_SECTION( render_post );
void MipFilter( s32 nFilters, f32 offset, post_falloff_fn fn, texture::handle const& texture, s32 PaletteIndex )
    X_SECTION( render_post );
void NoiseFilter( xcolor color ) X_SECTION( render_post );
void ScreenFade( xcolor color ) X_SECTION( render_post );

//=========================================================================
//  FILTER LIGHTING
//=========================================================================

void   EnableFilterLight( xbool bEnable ) X_SECTION( render_infrequent );
xbool  IsFilterLightEnabled( void ) X_SECTION( render_infrequent );
void   SetFilterLightColor( xcolor color ) X_SECTION( render_infrequent );
xcolor GetFilterLightColor( void ) X_SECTION( render_infrequent );

} // namespace render

//=========================================================================
//  EXTERNAL DEBUG STATE
//=========================================================================

//=========================================================================
#endif // RENDER_HPP
//=========================================================================
