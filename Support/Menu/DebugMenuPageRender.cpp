//==============================================================================
//  DebugMenuPageRender.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu render page.
//  
//==============================================================================

#include "DebugMenu2.hpp"
#include "CollisionMgr/PolyCache.hpp"
#include "GameLib/StatsMgr.hpp"
#include "Render/Render.hpp"
#include "UI/ui_renderer.hpp"
#include "UI/ui_font.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

//==============================================================================

#if defined( ENABLE_DEBUG_MENU )

//==============================================================================

extern stats        g_Stats;
extern xbool        g_MagentaColor;
extern xbool        g_RenderFrameRateInfo;
extern xbool        g_bShowBackFaces;
extern xbool        g_RenderBoneBBoxes;
extern xbool        g_RenderHUD;


//==============================================================================

static const char*  ColorText[] = { "BLACK", "MAGENTA" };

//==============================================================================

debug_menu_page_render::debug_menu_page_render( ) : debug_menu_page()
{
    m_pTitle = "Render";
    m_pItemPolyCache = NULL;

                            AddItemBool     ( "Log engine stats"           , g_Stats.EngineStats );
                            AddItemBool     ( "Display frame rate info"    , g_RenderFrameRateInfo );
#ifndef X_RETAIL
                            AddItemBool     ( "Show lighting info"         , render::GetRenderPipeline().GetDebugOptions().ShowLightingStats );
#endif
                            AddItemSeperator( );
                            AddItemEnum     ( "Background color"           , g_MagentaColor, ColorText, 2 );

#if !defined(X_RETAIL) || defined(X_QA)
    m_pItemPolyCache     =  AddItemBool     ( "Display polycache"          , g_PolyCache.m_Debug.RENDER );
#endif

#ifndef X_RETAIL
                            AddItemSeperator( );
                            
                            AddItemBool     ( "Display full screen stats"  , stats_mgr::m_bShowNumbers );
#endif


#ifndef X_RETAIL
                            AddItemSeperator( );
                            AddItemBool     ( "Render bone BBoxes"         , g_RenderBoneBBoxes            );
                            AddItemBool     ( "Render rigids only"         , render::GetRenderPipeline().GetDebugOptions().RenderRigidOnly );
                            AddItemBool     ( "Render skins  only"         , render::GetRenderPipeline().GetDebugOptions().RenderSkinOnly );
                            AddItemBool     ( "Render clipped only"        , render::GetRenderPipeline().GetDebugOptions().RenderClippedOnly );
                            AddItemBool     ( "Render shadowed only"       , render::GetRenderPipeline().GetDebugOptions().RenderShadowedOnly );
                            AddItemBool     ( "Dump frame graph"            , render::GetRenderPipeline().GetDebugOptions().DumpFrameGraph );
#endif

#if !defined(X_RETAIL) || defined(X_QA)
                            AddItemBool     ( "Render HUD"                 , g_RenderHUD );
#endif
}

//==============================================================================

debug_menu_page_lighting::debug_menu_page_lighting( void ) : debug_menu_page()
{
    m_pTitle = "Lighting";
#ifndef X_RETAIL
    render::debug_options& Options = render::GetRenderPipeline().GetDebugOptions();
    AddItemBool     ( "Show lighting info"            , Options.ShowLightingStats );
    AddItemSeperator( );
    AddItemBool     ( "Disable all local lights"      , Options.DisableLocalLights );
    AddItemBool     ( "Disable dynamic lights"        , Options.DisableDynamicLights );
    AddItemBool     ( "Disable character-only lights" , Options.DisableCharLights );
    AddItemBool     ( "Disable fading lights"         , Options.DisableFadingLights );
    AddItemBool     ( "Disable light cookies"         , Options.DisableLightCookies );
    AddItemSeperator( );
    AddItemBool     ( "Disable projected lights"      , Options.DisableProjectedLights );
    AddItemBool     ( "Disable projected shadows"     , Options.DisableProjectedShadows );
    AddItemBool     ( "Disable shadow maps"           , Options.DisableShadowMaps );
#endif
}

//==============================================================================

void debug_menu_page_render::OnChangeItem( debug_menu_item* pItem )
{
    if( pItem == m_pItemPolyCache )
    {
#if !defined(X_RETAIL) || defined(X_QA)
        g_PolyCache.m_Debug.RENDER_HIT_CLUSTERS = g_PolyCache.m_Debug.RENDER;
#endif
    }
}

//==============================================================================

void debug_menu_page_render::OnPreRender( void )
{
#ifndef X_RETAIL
    if( render::GetRenderPipeline().GetDebugOptions().ShowLightingStats )
        RenderLightingStats();
#endif
}

//==============================================================================

void debug_menu_page_render::RenderLightingStats( void )
{
#ifndef X_RETAIL
    render::RenderPipeline::lighting_debug_stats const& S = render::GetRenderPipeline().GetLightingDebugStats();
    s32 const Font = g_UiMgr->FindFont( "small" );
    if( Font < 0 ) return;
    irect const Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();
    s32 const LineHeight = g_UiMgr->GetLineHeight( Font );
    s32 Y = Bounds.t + 40;
    auto Line = [&]( char const* Text )
    {
        xwstring const Label( Text );
        irect Rect;
        g_UiMgr->TextSize( Font, Rect, Label, Label.GetLength() );
        Rect.Translate( Bounds.l + 10, Y );
        g_UIRenderer.DrawRect( Rect, xcolor( 0, 0, 0, 128 ) );
        g_UiMgr->RenderText( Font, Rect, ui_font::h_left | ui_font::v_top, XCOLOR_WHITE, Label );
        Y += LineHeight;
    };
    if( !S.Valid )
    {
        Line( "Lighting: waiting for a gameplay frame" );
        return;
    }
    Line( "LIGHTING" );
    Line( xfs( "Lights: dynamic %u | char %u | fading %u", S.DynamicLights, S.CharLights, S.FadingLights ) );
    Line( xfs( "Dynamic: omni %u | spot %u | cookies %u", S.OmniLights, S.SpotLights, S.Cookies ) );
    f32 const Average = S.LightingSets ? static_cast<f32>(S.AssignedLights) / S.LightingSets : 0.0f;
    Line( xfs( "Sets %u | lights/set avg %.2f, max %u/%u", S.LightingSets, Average, S.MaxAssignedLights, MAX_GEOM_LIGHTS ) );
    Line( xfs( "Sets 0/1/2/3/4: %u/%u/%u/%u/%u", S.LightHistogram[0], S.LightHistogram[1], S.LightHistogram[2], S.LightHistogram[3], S.LightHistogram[4] ) );
    Line( xfs( "Scene: queries %u | candidates %u | hits %u", S.Collection.SceneQueries, S.Collection.SceneCandidates, S.Collection.SceneHits ) );
    Line( xfs( "Char: queries %u | candidates %u | hits %u", S.Collection.CharQueries, S.Collection.CharCandidates, S.Collection.CharHits ) );
    Line( xfs( "Projections: lights %u | shadows %u", S.ProjectedLights, S.ProjectedShadows ) );
    char const* Mode = !S.ShadowsEnabled ? "disabled" : S.Mode == ShadowMode::FlashlightOnly ? "flashlight only" : "enabled";
    Line( xfs( "SHADOWS: %s, %s", Mode, S.Filter == ShadowFilterType::Evsm ? "EVSM" : "hard" ) );
    Line( xfs( "Requests %u | point %u (%u faces) | spot %u", S.ShadowRequests, S.PointShadowLights, S.PointShadowFaces, S.SpotShadowSources ) );
    Line( xfs( "Atlas %ux%u | active area %.1f%%", S.AtlasSize, S.AtlasSize, S.ActiveAtlasArea ) );
    Line( xfs( "Tiles: updated %u | reused %u", S.Casters.UpdatedTileCount, S.Casters.ReusedTileCount ) );
    Line( xfs( "Casters: input %u | prepared %u | culled %u", S.Casters.InputDrawCount, S.Casters.PreparedDrawCount, S.Casters.FrustumRejectedDrawCount ) );
    Line( xfs( "GPU draws %u | packets %u | rigid/skin %u/%u", S.Casters.GpuDrawCount, S.Casters.PacketCount, S.Casters.RigidInstanceCount, S.Casters.SkinInstanceCount ) );
    Line( xfs( "Upload %.1f KiB | indices %llu", static_cast<f32>(S.Casters.UploadBytes) / 1024.0f, static_cast<unsigned long long>(S.Casters.SubmittedIndexCount) ) );
#endif
}

//==============================================================================

#endif // defined( ENABLE_DEBUG_MENU )
