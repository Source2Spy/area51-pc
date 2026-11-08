//=========================================================================

//
//  Render platform routines
//
#include "../LightSystem.hpp"
#include "RenderPlatform.hpp"
#include "../ProjectionTextureRegistry.hpp"
#include "../../Decals/DecalMgr.hpp"
#include "../Backend/Storage/StaticGeometryStorage.hpp"
#include "../Backend/Storage/SkinnedGeometryStorage.hpp"
#include "../Backend/Resource/GeometryRepository.hpp"
#include "../Backend/Frame/FrameCompiler.hpp"
#include "../Backend/Decal/DecalEncoder.hpp"
#include "../Backend/Resource/ProjectionAtlas.hpp"
#include "../Features/Shadow/ShadowRenderer.hpp"
#include "../Features/PostProcess/PostProcessFeature.hpp"
#include "../Backend/Primitive/PrimitiveStorage.hpp"
#include "../Backend/Primitive/PrimitiveEncoder.hpp"
#include "../Pipeline/RenderPipeline.hpp"
#include "../Features/Bloom/BloomEffect.hpp"
#include "../Features/Geom/GeomFeature.hpp"
#include "../Features/Shadow/ShadowFeature.hpp"
#include "../FrameGraph/RenderFailure.hpp"
#include "../../UI/ui_renderer.hpp"
#include "../../GameLib/RenderContext.hpp"
#include "e_Engine.hpp"

namespace render
{
namespace platform
{

//=========================================================================
// RenderPipeline owns frame state; this layer adapts legacy entry points.
//=========================================================================

void platform_Kill( void );

void platform_ResetFrameState( void )
{
    GetRenderPipeline().GetPrimitiveEncoder().DiscardSubmission();
    GetRenderPipeline().GetGeomFeature().DiscardQueuedDraws();
    GetRenderPipeline().GetShadowSubmission().Reset();
    GetRenderPipeline().GetClothFeature().AbortFrame();
    GetRenderPipeline().AbortFrame();
}

//=========================================================================

void platform_BeginEngineFrame( void )
{
    GetRenderPipeline().BeginEngineFrame();
}

//=========================================================================

xbool platform_IsFrameAborted( void )
{
    return GetRenderPipeline().IsFrameAborted();
}

//=========================================================================

void platform_AbortFrame( char const* pReason )
{
    xbool const FirstAbort = !GetRenderPipeline().IsFrameAborted();
    platform_ResetFrameState();
    if ( FirstAbort )
        RENDER_THROW( "RenderPlatform", "frame aborted: %s", pReason ? pReason : "unknown reason" );
}

//=========================================================================
// Implementation
//=========================================================================

xbool platform_Init( void )
{
    x_try;

    char const* pFailedStage = NULL;

    if ( !GetRenderPipeline().Init() )
        pFailedStage = "RenderPipeline";

    if ( !pFailedStage && !GetRenderPipeline().GetBackend().Init() )
        pFailedStage = "RenderBackend";

    if ( !pFailedStage )
        GetRenderPipeline().ResolveSampleCountForDevice();

    if ( !pFailedStage )
    {
        GetRenderPipeline().GetFrameCompiler().Init( GetRenderPipeline().GetBackend().GetContext() );
        if ( !GetRenderPipeline().GetFrameCompiler().IsInitialized() )
            pFailedStage = "FrameCompiler";
    }

    if ( !pFailedStage )
    {
        if ( !GetRenderPipeline().GetPostProcessFeatureRef().Init() )
            pFailedStage = "PostProcessFeature";
    }

    if ( !pFailedStage )
    {
        GetRenderPipeline().GetDecalEncoder().Init( GetRenderPipeline().GetBackend().GetContext() );
        if ( !GetRenderPipeline().GetDecalEncoder().IsInitialized() )
            pFailedStage = "DecalEncoder";
    }

    if ( !pFailedStage )
    {
        GetRenderPipeline().GetPrimitiveStorage().Init( GetRenderPipeline().GetBackend().GetContext() );
        if ( !GetRenderPipeline().GetPrimitiveStorage().IsInitialized() )
            pFailedStage = "PrimitiveStorage";
    }

    if ( !pFailedStage )
    {
        GetRenderPipeline().GetPrimitiveEncoder().Init( GetRenderPipeline().GetPrimitiveStorage() );
        if ( !GetRenderPipeline().GetGeomFeature().Bind( GetRenderPipeline().GetBackend(), GetRenderPipeline().GetFrameCompiler(), GetRenderPipeline().GetPrimitiveStorage(), GetRenderPipeline().GetDecalEncoder(),
                                      GetRenderPipeline().GetPostProcessFeatureRef().GetBloomFeature(), GetRenderPipeline().GetShadowFeatureRef().Backend(),
                                      GetRenderPipeline().GetLightSystem() ) )
            pFailedStage = "GeomFeature";
    }

    if ( !pFailedStage )
    {
        if ( !GetRenderPipeline().GetGeometryRepository().Init() )
            pFailedStage = "GeometryRepository";
    }

    if ( !pFailedStage && !GetRenderPipeline().RegisterFeature( GetRenderPipeline().GetClothFeature() ) )
        pFailedStage = "ClothFeature";

    if ( !pFailedStage && !GetRenderPipeline().RegisterFeature( GetRenderPipeline().GetGeomFeature() ) )
        pFailedStage = "GeomFeature";

    if ( !pFailedStage && !GetRenderPipeline().RegisterFeature( GetRenderPipeline().GetShadowFeatureRef() ) )
        pFailedStage = "ShadowFeature";

    if ( !pFailedStage && !GetRenderPipeline().RegisterFeature( GetRenderPipeline().GetPostProcessFeatureRef() ) )
        pFailedStage = "PostProcessFeature";
    if( !pFailedStage && !GetRenderPipeline().RegisterFeature( GetRenderPipeline().GetCmaa2Feature() ) )
    {
        pFailedStage = "Cmaa2Feature";
    }

    if( !pFailedStage && !GetRenderPipeline().RegisterFeature( GetRenderPipeline().GetXeGtaoFeature() ) )
    {
        pFailedStage = "XeGtaoFeature";
    }

    if ( !pFailedStage && !GetRenderPipeline().InitFeatures() )
        pFailedStage = "RenderFeatures";

    if ( pFailedStage )
    {
        platform_Kill();
        RENDER_THROW( "RenderPlatform", "initialization failed at stage '%s'", pFailedStage );
        return FALSE;
    }

    x_DebugMsg( "RenderPlatform: initialized successfully\n" );
    return TRUE;

    x_catch_begin;
    platform_Kill();
    x_catch_end_ret;
}

//=========================================================================

void platform_Kill( void )
{
    eng_ResetAfterException();
    eng_Sync();
    GetRenderPipeline().AbortFrame();
    GetRenderPipeline().KillFeatures();
    GetRenderPipeline().UnregisterFeature( GetRenderPipeline().GetPostProcessFeatureRef() );
    GetRenderPipeline().UnregisterFeature( GetRenderPipeline().GetShadowFeatureRef() );
    GetRenderPipeline().UnregisterFeature( GetRenderPipeline().GetGeomFeature() );
    GetRenderPipeline().GetGeometryRepository().Kill();
    GetRenderPipeline().GetPrimitiveEncoder().Kill();
    GetRenderPipeline().GetPrimitiveStorage().Kill();
    GetRenderPipeline().GetDecalEncoder().Kill();
    GetRenderPipeline().GetFrameCompiler().Kill();
    GetRenderPipeline().GetBackend().Kill();
    GetRenderPipeline().Kill();
}

//=========================================================================

xhandle platform_RegisterRigidGeom( const rigid_geom& Geom )
{
    return GetRenderPipeline().GetBackend().RegisterRigid( Geom );
}

//=========================================================================

void platform_UnregisterRigidGeom( xhandle hGeom )
{
    GetRenderPipeline().GetBackend().UnregisterRigid( hGeom );
}

//=========================================================================

xhandle platform_RegisterSkinGeom( const skin_geom& Geom )
{
    return GetRenderPipeline().GetBackend().RegisterSkin( Geom );
}

//=========================================================================

void platform_UnregisterSkinGeom( xhandle hGeom )
{
    GetRenderPipeline().GetBackend().UnregisterSkin( hGeom );
}

//=========================================================================

xbool platform_SubmitPrimitives( render::primitive_draw_desc const& Desc,
                                 matrix4 const&                     LocalToWorld,
                                 render::primitive_vertex const*    pVertices,
                                 s32                                nVertices,
                                 u16 const*                         pIndices,
                                 s32                                nIndices )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        platform_AbortFrame( "primitive submission has no active view" );
        return FALSE;
    }
    if ( !GetRenderPipeline().GetPrimitiveEncoder().SubmitPrimitives( Desc, LocalToWorld, *pView, pVertices, nVertices,
                                                pIndices, nIndices ) )
    {
        platform_AbortFrame( "primitive submission was rejected" );
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool platform_SetDepthRect( irect const& Rect, f32 Depth )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        platform_AbortFrame( "depth rectangle has no active view" );
        return FALSE;
    }
    if ( !GetRenderPipeline().GetPrimitiveEncoder().SubmitDepthRect( Rect, Depth, *pView ) )
    {
        platform_AbortFrame( "depth rectangle submission was rejected" );
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool platform_SubmitDecalBatch( render::decal_draw_desc const& Desc,
                                 cubemap const*                  pCubeMap,
                                 render::decal_vertex const*     pVertices,
                                 s32                             nVertices,
                                 u16 const*                      pIndices,
                                 s32                             nIndices )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        platform_AbortFrame( "decal submission has no active view" );
        return FALSE;
    }
    if ( !GetRenderPipeline().GetDecalEncoder().Submit( Desc, pCubeMap, *pView, pVertices, nVertices, pIndices, nIndices ) )
    {
        platform_AbortFrame( "decal submission was rejected" );
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

void platform_ReserveDecalSubmissionCapacity( s32 nVertices, s32 nIndices, s32 nDraws )
{
    if ( !GetRenderPipeline().GetDecalEncoder().Reserve( nVertices, nIndices, nDraws ) )
    {
        platform_AbortFrame( "decal submission capacity unavailable" );
    }
}

//=========================================================================

void platform_ResetAfterException( void )
{
    platform_ResetFrameState();
}

//=========================================================================

static
void SetCollectedSceneLights( cb_geom_lighting& Lighting, s32 LightCount )
{
    LightSystem& Lights = GetRenderPipeline().GetLightSystem();

    Lighting.LightCount = LightCount;

    for( s32 i = 0; i < LightCount; i++ )
    {
        vector3 Pos;
        f32     Radius;
        xcolor  Col;
        f32     Falloff;
        s32     Shape;
        vector3 Direction;
        f32     InnerConeCos;
        f32     OuterConeCos;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;

        Lights.GetCollectedLightInfo( i, Pos, Radius, Col, Falloff, Shape, Direction,
                                      InnerConeCos, OuterConeCos );
        Lights.GetCollectedLightCookie( i, CookieIndex, CookieU, CookieV );

        Lighting.LightVec[i].Set( Pos.GetX(),
                                  Pos.GetY(),
                                  Pos.GetZ(),
                                  Radius );

        Lighting.LightCol[i].Set( (f32)Col.R / 255.0f,
                                  (f32)Col.G / 255.0f,
                                  (f32)Col.B / 255.0f,
                                  Falloff );

        Lighting.LightDir[i].Set( Direction.GetX(),
                                  Direction.GetY(),
                                  Direction.GetZ(),
                                  (Shape == LightSystem::LIGHT_SHAPE_SPOT) ? 1.0f : 0.0f );

        Lighting.LightCone[i].Set( InnerConeCos, OuterConeCos, 0.0f, 0.0f );

        Lighting.LightCookieU[i].Set( CookieU.GetX(),
                                      CookieU.GetY(),
                                      CookieU.GetZ(),
                                      (CookieIndex >= 0) ? (f32)(CookieIndex + 1) : 0.0f );

        Lighting.LightCookieV[i].Set( CookieV.GetX(), CookieV.GetY(), CookieV.GetZ(), 0.0f );
        Lighting.DynamicLightIndex[i] = Lights.GetCollectedDynamicLightIndex( i );
    }
}

//=========================================================================

static
void AddCharacterLights( cb_geom_lighting& Lighting, const matrix4& L2W, const bbox& BBox )
{
    LightSystem& Lights = GetRenderPipeline().GetLightSystem();
    s32 NCharLights = Lights.CollectCharLightsOnly( L2W, BBox, MAX_GEOM_LIGHTS );
    s32 LightIndex = Lighting.LightCount;

    for( s32 i = 0; ( i < NCharLights ) && ( LightIndex < MAX_GEOM_LIGHTS ); i++, LightIndex++ )
    {
        vector3 Dir;
        xcolor  Col;

        Lights.GetCollectedCharLight( i, Dir, Col );

        Lighting.LightVec[LightIndex].Set( Dir.GetX(), Dir.GetY(), Dir.GetZ(), 0.0f );

        Lighting.LightCol[LightIndex].Set( (f32)Col.R / 255.0f,
                                           (f32)Col.G / 255.0f,
                                           (f32)Col.B / 255.0f,
                                           0.0f );

        Lighting.LightDir[LightIndex].Set( 0.0f, 0.0f, 0.0f, 2.0f );
        Lighting.DynamicLightIndex[LightIndex] = -1;
    }

    Lighting.LightCount = LightIndex;
}

//=========================================================================

RenderLightingHandle platform_CalculateRigidLighting( const matrix4& L2W,
                                                      const bbox&    WorldBBox,
                                                      xcolor         Ambient )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "platform_CalculateRigidLighting" );
    GetRenderPipeline().GetGeomFeature().RecordRigidLightingQuery();

    RenderLightingHandle Result;
    s32 NLights = GetRenderPipeline().GetLightSystem().CollectLights( WorldBBox, MAX_GEOM_LIGHTS );

    if( NLights || Ambient.R || Ambient.G || Ambient.B )
    {
        RenderLightingTable& LightingTable = GetRenderPipeline().GetLightingTable();
        Result = LightingTable.Allocate();
        cb_geom_lighting* pLighting = LightingTable.GetMutable( Result );

        if( !pLighting )
        {
            return RenderLightingHandle();
        }

        pLighting->AmbCol.Set( (f32)Ambient.R / 255.0f,
                               (f32)Ambient.G / 255.0f,
                               (f32)Ambient.B / 255.0f,
                               1.0f );

        SetCollectedSceneLights( *pLighting, NLights );

        GetRenderPipeline().GetGeomFeature().RecordRigidLightingSetup();
    }

    return Result;
}

//=========================================================================

RenderLightingHandle platform_CalculateSkinLighting( u32            Flags,
                                                     const matrix4& L2W,
                                                     const bbox&    BBox,
                                                     xcolor         Ambient )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "platform_CalculateSkinLighting" );
    GetRenderPipeline().GetGeomFeature().RecordSkinLightingQuery();

    RenderLightingTable& LightingTable = GetRenderPipeline().GetLightingTable();
    RenderLightingHandle Result = LightingTable.Allocate();
    cb_geom_lighting* pLighting = LightingTable.GetMutable( Result );

    if( !pLighting )
    {
        return RenderLightingHandle();
    }

    pLighting->AmbCol.Set( (f32)Ambient.R / 255.0f,
                           (f32)Ambient.G / 255.0f,
                           (f32)Ambient.B / 255.0f,
                           1.0f );

    if( Flags & render::GLOWING )
    {
        return Result;
    }

    bbox WorldBBox = BBox;
    WorldBBox.Transform( L2W );

    s32 NSceneLights = GetRenderPipeline().GetLightSystem().CollectLights( WorldBBox, MAX_GEOM_LIGHTS );

    SetCollectedSceneLights( *pLighting, NSceneLights );
    AddCharacterLights( *pLighting, L2W, BBox );

    return Result;
}

//=========================================================================

RenderLightingHandle platform_CalculateClothLighting( const bbox& WorldBBox, xcolor Ambient )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "platform_CalculateClothLighting" );

    RenderLightingTable& LightingTable = GetRenderPipeline().GetLightingTable();
    RenderLightingHandle Result = LightingTable.Allocate();
    cb_geom_lighting* pLighting = LightingTable.GetMutable( Result );

    if( !pLighting )
    {
        return RenderLightingHandle();
    }

    pLighting->AmbCol.Set( (f32)Ambient.R / 255.0f,
                           (f32)Ambient.G / 255.0f,
                           (f32)Ambient.B / 255.0f,
                           1.0f );

    s32 NSceneLights = GetRenderPipeline().GetLightSystem().CollectLights( WorldBBox, MAX_GEOM_LIGHTS );
    matrix4 Identity;
    Identity.Identity();

    SetCollectedSceneLights( *pLighting, NSceneLights );
    AddCharacterLights( *pLighting, Identity, WorldBBox );

    return Result;
}

//=========================================================================

void platform_SetSceneColorGrade( SceneColorGradeParams const& Params )
{
    GetRenderPipeline().GetPostProcessFeatureRef().SetSceneColorGrade( Params );
}

//=========================================================================

SceneColorGradeParams platform_GetSceneColorGrade( void )
{
    return GetRenderPipeline().GetPostProcessFeatureRef().GetSceneColorGrade();
}

//=========================================================================

void platform_SetCustomFogPalette( const texture::handle& Texture, xbool ImmediateSwitch, s32 PaletteIndex )
{
    GetRenderPipeline().GetPostProcessFeatureRef().SetCustomFogPalette( Texture, ImmediateSwitch, PaletteIndex, eng_GetView() );
}

//=========================================================================

xcolor platform_GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    return GetRenderPipeline().GetPostProcessFeatureRef().GetFogValue( WorldPos, PaletteIndex, eng_GetView() );
}

//=========================================================================

void platform_AddScreenWarp( const vector3& WorldPos, f32 Radius, f32 WarpAmount )
{
    GetRenderPipeline().GetPostProcessFeatureRef().AddScreenWarp( WorldPos, Radius, WarpAmount );
}

//=========================================================================

void platform_ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff )
{
    GetRenderPipeline().GetPostProcessFeatureRef().ApplySelfIllumGlows( MotionBlurIntensity, GlowCutoff );
}

//=========================================================================

void platform_UpdatePostProcessing( f32 DeltaTime )
{
    GetRenderPipeline().GetPostProcessFeatureRef().Update( DeltaTime );
}

//=========================================================================

void platform_MotionBlur( f32 Intensity )
{
    GetRenderPipeline().GetPostProcessFeatureRef().MotionBlur( Intensity );
}

//=========================================================================

void platform_ZFogFilter( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    GetRenderPipeline().GetPostProcessFeatureRef().ZFogFilter( Fn, Color, Param1, Param2 );
}

//=========================================================================

void platform_ZFogFilter( render::post_falloff_fn Fn, s32 PaletteIndex )
{
    GetRenderPipeline().GetPostProcessFeatureRef().ZFogFilter( Fn, PaletteIndex );
}

//=========================================================================

void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         xcolor                     Color,
                         f32                        Param1,
                         f32                        Param2,
                         s32                        PaletteIndex )
{
    GetRenderPipeline().GetPostProcessFeatureRef().MipFilter( nFilters, Offset, Fn, Color, Param1, Param2, PaletteIndex );
}

//=========================================================================

void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         const texture::handle&     Texture,
                         s32                        PaletteIndex )
{
    GetRenderPipeline().GetPostProcessFeatureRef().MipFilter( nFilters, Offset, Fn, Texture, PaletteIndex );
}

//=========================================================================

void platform_NoiseFilter( xcolor Color )
{
    GetRenderPipeline().GetPostProcessFeatureRef().NoiseFilter( Color );
}

//=========================================================================

void platform_ScreenFade( xcolor Color )
{
    GetRenderPipeline().GetPostProcessFeatureRef().ScreenFade( Color );
}

//=========================================================================

void platform_RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale  )
{
    GetRenderPipeline().GetPostProcessFeatureRef().RadialBlur( Zoom, Angle, AlphaSub, AlphaScale );
}

//=========================================================================

void platform_ClearShadowSourceList( void )
{
    GetRenderPipeline().GetShadowMapAtlas().ClearSources();
}

//=========================================================================

void platform_FinalizeShadowSourceList( void )
{
    GetRenderPipeline().GetShadowMapAtlas().FinalizeSources();
}

//=========================================================================

void platform_AddPointShadowMapSource( const matrix4& L2W,
                                       radian         FOV,
                                       f32            LightRadius,
                                       f32            LightFalloff,
                                       s32            ShadowMapResolution,
                                       s32            ShadowPriority,
                                       f32            ShadowScore,
                                       s32            DynamicLightIndex )
{
    GetRenderPipeline().GetShadowMapAtlas().AddPointSource( L2W,
                                   FOV,
                                   LightRadius,
                                   LightFalloff,
                                   ShadowMapResolution,
                                   ShadowPriority,
                                   ShadowScore,
                                   DynamicLightIndex );
}

//=========================================================================

void platform_AddSpotShadowMapSource( const matrix4& L2W,
                                      radian         FOV,
                                      f32            LightRadius,
                                      f32            LightFalloff,
                                      s32            ShadowMapResolution,
                                      s32            ShadowPriority,
                                      f32            ShadowScore,
                                      s32            DynamicLightIndex )
{
    GetRenderPipeline().GetShadowMapAtlas().AddSpotSource( L2W,
                                  FOV,
                                  LightRadius,
                                  LightFalloff,
                                  ShadowMapResolution,
                                  ShadowPriority,
                                  ShadowScore,
                                  DynamicLightIndex );
}

//=========================================================================

xbool platform_RenderShadowCasters( const xarray<const geom_draw_item*>& Draws,
                                   const xarray<dynamic_geom_shadow_draw>& DynamicDraws )
{
    // Tag pre-collected shadow draws with the scope that will consume them.
    RenderScopeDesc const& scope = GetActiveRenderScopeDesc();
    s32 const ScopeId = ( scope.ScopeId != -1 ) ? scope.ScopeId : g_RenderContext.LocalPlayerIndex;
    ShadowRenderer& Shadows = GetRenderPipeline().GetShadowFeatureRef().Backend();
    ClothFeature&   Cloth   = GetRenderPipeline().GetClothFeature();
    if( !Cloth.CollectShadowRequests( DynamicDraws, ScopeId ) ||
        !Shadows.PrepareCasters( Draws, DynamicDraws, ScopeId ) )
    {
        Cloth.AbortFrame();
        Shadows.AbortFrame();
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool platform_BeginRenderFrame( void )
{
    render_ResetFailureState();
    if ( !GetRenderPipeline().IsInitialized() || !GetRenderPipeline().GetFrameCompiler().IsInitialized() ||
         !GetRenderPipeline().GetShadowFeatureRef().Backend().IsInitialized() || !GetRenderPipeline().GetPostProcessFeatureRef().IsInitialized() ||
         !GetRenderPipeline().GetDecalEncoder().IsInitialized() || !GetRenderPipeline().GetPrimitiveStorage().IsInitialized() ||
         !GetRenderPipeline().GetProjectionAtlas().IsInitialized() )
    {
        platform_AbortFrame( "one or more renderer subsystems are not initialized" );
        return FALSE;
    }

    GetRenderPipeline().GetPrimitiveStorage().BeginRender();
    GetRenderPipeline().GetDecalEncoder().BeginRender();

    const view* pView = eng_GetView();
    if( !pView )
    {
        platform_AbortFrame( "active render view is unavailable" );
        return FALSE;
    }

    RenderView FrameView;
    FrameView.View        = pView->GetW2V();
    FrameView.Projection  = pView->GetV2C();
    FrameView.SourceView = *pView;
    FrameView.HasSourceView = TRUE;
    s32 left, top, right, bottom;
    pView->GetViewport( left, top, right, bottom );
    FrameView.Viewport.TopLeftX = static_cast<f32>( left );
    FrameView.Viewport.TopLeftY = static_cast<f32>( top );
    FrameView.Viewport.Width    = static_cast<f32>( right - left );
    FrameView.Viewport.Height   = static_cast<f32>( bottom - top );
    // ScopeId separates render histories; ViewId is stable within the scope.
    FrameView.ViewId      = 1;

    RenderFrameDesc FrameDesc;
    RenderQualityProfile const quality = RenderQualityProfile::PlatformDefault();
    FrameDesc.Views.pViews    = &FrameView;
    FrameDesc.Views.ViewCount = 1;
    FrameDesc.Views.ViewMask  = 1;
    RenderScopeDesc const& scope = GetActiveRenderScopeDesc();
    if ( ( scope.Purpose == RENDER_VIEW_SCENE_CAPTURE ) && !scope.pColorTarget )
    {
        platform_AbortFrame( "scene capture has no output target" );
        return FALSE;
    }
    if ( scope.pColorTarget )
    {
        FrameDesc.Output.pColor      = scope.pColorTarget;
        FrameDesc.Output.pDepth      = scope.pDepthTarget;
        FrameDesc.Output.ColorFormat = scope.pColorTarget->Desc.Format;
        FrameDesc.Output.Extent      = RenderExtent( scope.pColorTarget->Desc.Width,
                                                     scope.pColorTarget->Desc.Height );
        FrameDesc.Output.SampleCount = scope.pColorTarget->Desc.SampleCount;
        FrameDesc.Output.ArrayLayers = 1;
        FrameDesc.Output.ViewMask    = 1;
        if ( scope.pDepthTarget )
            FrameDesc.Output.DepthFormat = scope.pDepthTarget->Desc.Format;
    }
    FrameDesc.Purpose       = scope.Purpose;
    FrameDesc.Formats         = quality.Formats;
    FrameDesc.RenderScale     = GetRenderPipeline().GetRenderScale();
    FrameDesc.SampleCount     = scope.SampleCount ? scope.SampleCount
                               : scope.pColorTarget ? scope.pColorTarget->Desc.SampleCount
                                                    : GetRenderPipeline().GetSampleCount();
    FrameDesc.EnableShadows   = scope.EnableShadows;
    FrameDesc.EnablePostProcessing = scope.EnablePostProcessing;
    FrameDesc.EnableTemporalHistory = scope.EnableTemporalHistory;
    FrameDesc.ScopeId         = ( scope.ScopeId != -1 ) ? scope.ScopeId : g_RenderContext.LocalPlayerIndex;
    FrameDesc.SceneTextureRequirements = SCENE_TEXTURE_DEPTH |
                                         GetRenderPipeline().GetFeatureSceneTextureRequirements();
    if ( !GetRenderPipeline().BeginFrame( FrameDesc ) )
    {
        platform_AbortFrame( "failed to begin render frame" );
        return FALSE;
    }
    if ( !GetRenderPipeline().BeginFeatureFrames() )
    {
        platform_AbortFrame( "failed to begin feature frames" );
        return FALSE;
    }
    if ( !GetRenderPipeline().GetPrimitiveEncoder().BeginSubmission() )
    {
        platform_AbortFrame( "failed to initialize frame submission" );
        return FALSE;
    }

    GetRenderPipeline().GetProjectionTextureRegistry().ClearProjTextures();
    return TRUE;
}

//=========================================================================

xbool platform_EndRenderFrame( void )
{
    if ( GetRenderPipeline().IsFrameAborted() || !GetRenderPipeline().GetFrame() )
    {
        RENDER_THROW( "RenderPlatform", "cannot end an unavailable render frame" );
        return FALSE;
    }
    if ( !GetRenderPipeline().FinishRenderScope() )
    {
        platform_AbortFrame( "failed to finish render scope" );
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

} // namespace platform
} // namespace render
