//==============================================================================
//  DebugMenuPageGeneral.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu general page.
//  
//==============================================================================

#include "DebugMenu2.hpp"
#include "Objects/Player/Player.hpp"
#include "StateMgr/MapList.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"
#include "Configuration/GameConfig.hpp"
#include "PhysicsMgr/PhysicsMgr.hpp"

#include "GameLib/StatsMgr.hpp"


//==============================================================================

#if defined( ENABLE_DEBUG_MENU )

//==============================================================================
// Data

extern          xbool       g_ShowPlayerPos;
extern          xbool       g_RenderFrameRateInfo;

extern          s32         g_Changelist;
extern  const   char*       g_pBuildDate;

extern          map_list    g_MapList;

                xbool       g_ShowQAInfo      = FALSE;
#if ENABLE_STATS_MGR
        static  s32         s_StatsLegend = 0;
static const char*  StatsLegendText[] = { "Off", "CPU time", "GPU time", "Small Bars" };
#endif

//==============================================================================

debug_menu_page_general::debug_menu_page_general( ) : debug_menu_page()
{
    m_pTitle = "General";
    m_pItemStatsLegend = NULL;

    m_pItemShowQA =         AddItemBool     ( "Show QA info"           , g_ShowQAInfo );
#if ENABLE_STATS_MGR
                            AddItemSeperator( );
                            AddItemBool     ( "Display horizontal bars", stats_mgr::m_bShowHorizontalBars );
    m_pItemStatsLegend =    AddItemEnum     ( "Stats legend"           , s_StatsLegend, StatsLegendText, 4 );
#endif

#ifdef ENABLE_COLLISION_STATS
                            AddItemBool     ( "Display collision stats", g_CollisionMgr.m_DisplayStats );
#endif
                            AddItemBool     ( "Show player position"   , g_ShowPlayerPos );

#ifdef ENABLE_PHYSICS_DEBUG
                            AddItemSeperator( );
                            AddItemBool     ( "Display physics stats"      , g_PhysicsMgr.m_Settings.m_bShowStats );
                            AddItemBool     ( "Render physics debug"       , g_PhysicsMgr.m_Settings.m_bDebugRender );
#endif


                            AddItemSeperator( );
                            AddItemFloat    ( "Debug menu background alpha", g_DebugMenu.m_FadeAlpha, 0.0f, 1.0f, 0.1f );
                            AddItemSeperator( );

}

//==============================================================================

void debug_menu_page_general::OnChangeItem( debug_menu_item* pItem )
{
    if( pItem == m_pItemStatsLegend )
    {
#if ENABLE_STATS_MGR
        stats_mgr* pStatsMgr = stats_mgr::GetStatsMgr();
        if ( pStatsMgr )
        {
            pStatsMgr->m_bShowCPUTimeLegend  = FALSE;
            pStatsMgr->m_bShowGPUTimeLegend  = FALSE;
            pStatsMgr->m_bShowVerticalLegend = FALSE;

            switch( s_StatsLegend )
            {
            case 0:
                break;
            case 1:
                pStatsMgr->m_bShowCPUTimeLegend  = TRUE;
                break;
            case 2:
                pStatsMgr->m_bShowGPUTimeLegend  = TRUE;
                break;
            case 3:
                pStatsMgr->m_bShowVerticalLegend = TRUE;
                break;
            }
        }
#endif
    }

    if( pItem == m_pItemShowQA )
    {
        g_ShowPlayerPos = g_ShowQAInfo;
        g_RenderFrameRateInfo = g_ShowQAInfo;
    }
}

//==============================================================================

void debug_menu_page_general::OnPreRender ( void )
{
    // Set variables
    player* pPlayer = SMP_UTIL_GetActivePlayer();
    s32 font = g_UiMgr->FindFont("small");
    const irect Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();

    if( g_RenderFrameRateInfo )
    {
        const f32 FPS = eng_GetCPUFrameRate();
        const xwstring Text( (const char*)xfs( "FPS %.1f / %.2f ms", FPS, FPS > 0.0f ? 1000.0f / FPS : 0.0f ) );
        irect Rect;
        g_UiMgr->TextSize( font, Rect, Text, Text.GetLength() );
        Rect.Translate( Bounds.l + 10, Bounds.t + 10 );
        g_UIRenderer.DrawRect( Rect, xcolor( 0, 0, 0, 128 ) );
        g_UiMgr->RenderText( font, Rect, ui_font::h_left | ui_font::v_top, XCOLOR_WHITE, Text );
    }

    if ( g_ShowPlayerPos && pPlayer )
    {
        vector3 Pos = pPlayer->GetPosition();
        irect Rect;

        xwstring posText = (const char *)xfs( "Player( %7.1f, %7.1f, %7.1f )", Pos.GetX(), Pos.GetY(), Pos.GetZ() );
        g_UiMgr->TextSize( font, Rect, posText, posText.GetLength());
        Rect.Translate( Bounds.r - Rect.r - 10, Bounds.t - Rect.t + 10 );

        g_UIRenderer.DrawRect( Rect, xcolor(0,0,0,128) );
        g_UiMgr->RenderText( font, Rect, ui_font::h_left|ui_font::v_top, xcolor(XCOLOR_WHITE), posText );
    }

    if( g_ShowQAInfo )
    {
        irect Rect;
        s32 x = Bounds.r - 240;
        s32 y = Bounds.t + 40;
        Rect.Set(x - 4, y, x + 230, y + g_UiMgr->GetLineHeight(font) * 4);
        g_UIRenderer.DrawRect( Rect, xcolor(0,0,0,128) );

        //zone #
        if( pPlayer )
        {
            s32 PlayerZone = pPlayer->GetZone1();
            xwstring Zone = (const char *)xfs( "Zone: %d", PlayerZone);
            g_UiMgr->TextSize( font, Rect, Zone, Zone.GetLength());
            Rect.Translate(x, y);
            g_UiMgr->RenderText( font, Rect, ui_font::h_left|ui_font::v_top, xcolor(XCOLOR_WHITE), Zone );
            y += g_UiMgr->GetLineHeight(font);
        }

        // level name
        s32 LevelID = g_ActiveConfig.GetLevelID();
        xwstring Level  = (const char *)xfs( "Lvl:  %s", g_MapList.GetDisplayName(LevelID));
        g_UiMgr->TextSize( font, Rect, Level, Level.GetLength());
        Rect.Translate(x, y);
        g_UiMgr->RenderText( font, Rect, ui_font::h_left|ui_font::v_top, xcolor(XCOLOR_WHITE), Level );
        y += g_UiMgr->GetLineHeight(font);

        // change list 
        xwstring Change  = (const char *)xfs( "Chg: %d", g_Changelist );
        g_UiMgr->TextSize( font, Rect, Change, Change.GetLength());
        Rect.Translate(x, y);
        g_UiMgr->RenderText( font, Rect, ui_font::h_left|ui_font::v_top, xcolor(XCOLOR_WHITE), Change );
        y += g_UiMgr->GetLineHeight(font);

        // build date
        xwstring Build  = (const char *)xfs( "Bld: %s", g_pBuildDate );
        g_UiMgr->TextSize( font, Rect, Build, Build.GetLength());
        Rect.Translate(x, y);
        g_UiMgr->RenderText( font, Rect, ui_font::h_left|ui_font::v_top, xcolor(XCOLOR_WHITE), Build );
        y += g_UiMgr->GetLineHeight(font);

        // build low water mark
        extern s32 g_MemoryLowWater;
        xwstring Mem  = (const char *)xfs( "Mem: %d", g_MemoryLowWater );
        g_UiMgr->TextSize( font, Rect, Mem, Mem.GetLength());
        Rect.Translate(x, y);
        g_UiMgr->RenderText( font, Rect, ui_font::h_left|ui_font::v_top, xcolor(XCOLOR_WHITE), Mem );
        y += g_UiMgr->GetLineHeight(font);
    }

}

#endif // defined( ENABLE_DEBUG_MENU )
