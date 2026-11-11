//==============================================================================
//  DebugMenu2.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu classes.
//  
//==============================================================================

#include "DebugMenu2.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"
#include "StateMgr/StateMgr.hpp"
#include "FX/fx_Mgr.hpp"
#include "CollisionMgr/PolyCache.hpp"
#include <cstring>
#if defined( ENABLE_DEBUG_MENU )
#include "InputMgr/GamePad.hpp"
#endif


//==============================================================================

#if defined( ENABLE_DEBUG_MENU )

// implementation for debug menu class.
//==============================================================================

// The one and only debug_menu
debug_menu2 g_DebugMenu;

//==============================================================================

debug_menu2::debug_menu2()
{
    m_bMenuActive       = FALSE;
    m_iActivePage       = 0;
    m_FadeAlpha         = 0.5f;
    m_ItemChangeDelay   = 0;
}

//==============================================================================

debug_menu2::~debug_menu2()
{
    for( s32 i = 0; i < m_Pages.GetCount(); i++ )
        delete m_Pages[i];
}

//==============================================================================

xbool debug_menu2::Init( void )
{
    m_Pages.Append() = new debug_menu_page_general();
    m_Pages.Append() = new debug_menu_page_gameplay();
    m_Pages.Append() = new debug_menu_page_memory();
    m_Pages.Append() = new debug_menu_page_render();
#ifndef X_RETAIL
    m_Pages.Append() = new debug_menu_page_lighting();
#endif

#ifdef DEBUG_FX
    m_Pages.Append() = new debug_menu_page_fx();
#endif

#if !defined(X_RETAIL) 
    m_Pages.Append() = new debug_menu_page_aim_assist();
#endif

#if !defined(X_RETAIL) || defined(X_QA)
    m_Pages.Append() = new debug_menu_page_aiscript();
#endif

#if !defined(X_RETAIL)
    m_Pages.Append() = new debug_menu_page_logging();
#endif 

#if !defined(X_RETAIL) || defined(X_QA)
    m_Pages.Append() = new debug_menu_audio();
#endif

#if !defined(X_RETAIL)
    m_Pages.Append() = new debug_menu_perception();
#endif

#ifdef DEBUG_POLY_CACHE
    m_Pages.Append() = new debug_menu_page_polycache();
#endif

    m_Pages.Append() = new debug_menu_page_adv_checkpoints();
    m_Pages.Append() = new debug_menu_page_multiplayer();

#if !defined(X_RETAIL) || defined(X_QA)
    m_Pages.Append() = new debug_menu_page_localization();
#endif

    return TRUE;
}

//==============================================================================

void debug_menu2::Enable( void )
{
    if( (m_bMenuActive == FALSE) && (m_Pages.GetCount() > 0) )
    {
        // call each page's EnterMenu event.
        for( s32 iPage = 0; iPage < m_Pages.GetCount(); iPage++ )
        {
            m_Pages[iPage]->OnEnterMenu();
        }

        // activate the current page
        m_Pages[m_iActivePage]->OnEnterPage();
        m_Pages[m_iActivePage]->OnFocus();
        m_bMenuActive = TRUE;
    }
}

//==============================================================================

void debug_menu2::Disable( void )
{
    if( m_bMenuActive == TRUE )
    {
        // call each page's LeaveMenu event.
        for( s32 iPage = 0; iPage < m_Pages.GetCount(); iPage++ )
        {
            m_Pages[iPage]->OnLeaveMenu();
        }

        // Deactivate the current page
        m_Pages[m_iActivePage]->OnLoseFocus();
        m_Pages[m_iActivePage]->OnLeavePage();
        m_bMenuActive = FALSE;
    }
}

//==============================================================================

debug_menu_page * debug_menu2::FindPage( const char* pTitle )
{
    // look for the page with a given title string
    for (s32 iPage = 0; iPage < m_Pages.GetCount(); iPage++ )
    {
        if( strcmp(m_Pages[iPage]->GetTitle(), pTitle) == 0 ) return m_Pages[iPage];
    }
    return NULL;
}

//==============================================================================

xbool debug_menu2::IsActive( void )
{
    return m_bMenuActive;
}

//==============================================================================

xbool debug_menu2::WasTogglePressed( void ) const
{
    xbool ExitModifierDown = FALSE;
    xbool ExitPressed      = FALSE;

    for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
    {
        const frontend_pad& Pad = g_FrontendInput.GetPad( i );

        ExitModifierDown |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_EXIT_MODIFIER ).GetIsValue()  > 0.25f);
        ExitPressed      |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_EXIT          ).GetWasValue() > 0.25f);
    }

    return( ExitModifierDown && ExitPressed );
}

//==============================================================================

xbool debug_menu2::Update( f32 DeltaTime )
{
    if( WasTogglePressed() )
    {
        if( IsActive() )
        {
            Disable();
            return FALSE;
        }

        if( g_StateMgr.IsPaused() != TRUE )
        {
            Enable();
        }

        return TRUE;
    }

    if( IsActive() )
    {
        static const f32 fInitialDelay = 0.5f;
        static const f32 fRepeatDelay = 0.25f;
        static const f32 fRepeatAccl = 0.05f;
        static const f32 fDelayMin = 0.05f;
        static f32 fElapsedTime = 0.0f;
        static f32 fDelay;

        // item change execution is delayed so that we can render the menu flash.
        if( m_ItemChangeDelay ) // we get in here only if it's not zero.
        {
            if( --m_ItemChangeDelay == 0 )  // if it does reach zero, fire the event
            {
                debug_menu_page* pPage = m_Pages[m_iActivePage];
                if( pPage )
                {
                    debug_menu_item& Item = pPage->GetActiveItem();
                    m_Pages[m_iActivePage]->OnChangeItem( &Item );
                }
            }
            else
                return TRUE;
        }

        xbool NextPagePressed  = FALSE;
        xbool PrevPagePressed  = FALSE;
        xbool NextItemPressed  = FALSE;
        xbool PrevItemPressed  = FALSE;
        xbool IncrementPressed = FALSE;
        xbool DecrementPressed = FALSE;
        xbool ActionPressed    = FALSE;
        xbool IncrementHeld    = FALSE;
        xbool DecrementHeld    = FALSE;

        for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
        {
            const frontend_pad& Pad = g_FrontendInput.GetPad( i );

            NextPagePressed  |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_NEXT_PAGE     ).GetWasValue() > 0.25f);
            PrevPagePressed  |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_PREV_PAGE     ).GetWasValue() > 0.25f);
            NextItemPressed  |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_NEXT_ITEM     ).GetWasValue() > 0.25f);
            PrevItemPressed  |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_PREV_ITEM     ).GetWasValue() > 0.25f);
            IncrementPressed |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_INCREMENT     ).GetWasValue() > 0.25f);
            DecrementPressed |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_DECREMENT     ).GetWasValue() > 0.25f);
            ActionPressed    |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_ACTION        ).GetWasValue() > 0.25f);
            IncrementHeld    |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_INCREMENT     ).GetIsValue()  > 0.25f);
            DecrementHeld    |= (Pad.GetFrameLogical( frontend_pad::DEBUG_MENU_DECREMENT     ).GetIsValue()  > 0.25f);
        }

        if( NextPagePressed )
        {
            // Deactivate the current page
            m_Pages[m_iActivePage]->OnLoseFocus();

            // Change the page
            m_iActivePage++;
            if( m_iActivePage >= m_Pages.GetCount() )
                m_iActivePage = 0;

            // Activate the new page
            m_Pages[m_iActivePage]->OnFocus();
        }
        else if( PrevPagePressed )
        {
            // Deactivate the current page
            m_Pages[m_iActivePage]->OnLoseFocus();

            // Change the page
            m_iActivePage--;
            if( m_iActivePage < 0 )
                m_iActivePage = m_Pages.GetCount()-1;

            // Activate the new page
            m_Pages[m_iActivePage]->OnFocus();
        }
        else if( NextItemPressed )
        {
            debug_menu_page* pPage = m_Pages[m_iActivePage];
            pPage->NextItem();
        }
        else if( PrevItemPressed )
        {
            debug_menu_page* pPage = m_Pages[m_iActivePage];
            pPage->PrevItem();
        }
        else if( IncrementPressed )
        {
            debug_menu_page* pPage = m_Pages[m_iActivePage];
            if( pPage )
            {
                debug_menu_item& Item = pPage->GetActiveItem();
                Item.Increment();
                m_ItemChangeDelay = 2; // OnChangeItem() will fire when 0.
            }
            fDelay = fInitialDelay;
            fElapsedTime = 0.0f;
        }
        else if( DecrementPressed )
        {
            debug_menu_page* pPage = m_Pages[m_iActivePage];
            if( pPage )
            {
                debug_menu_item& Item = pPage->GetActiveItem();
                Item.Decrement();
                m_ItemChangeDelay = 2;  // OnChangeItem() will fire when 0.
            }
            fDelay = fInitialDelay;
            fElapsedTime = 0.0f;
        }
        else if( ActionPressed )
        {
            debug_menu_page* pPage = m_Pages[m_iActivePage];
            if( pPage )
            {
                debug_menu_item& Item = pPage->GetActiveItem();
                if( Item.GetType() == debug_menu_item::TYPE_BUTTON )
                {
                    Item.Increment();
                    m_ItemChangeDelay = 2;  // OnChangeItem() will fire when 0.
                }
            }
        }
        else if( IncrementHeld )
        {
            debug_menu_page* pPage = m_Pages[m_iActivePage];
            if( pPage )
            {
                debug_menu_item& Item = pPage->GetActiveItem();
                if( Item.GetType() == debug_menu_item::TYPE_FLOAT )
                {
                    fElapsedTime += DeltaTime;
                    if( fElapsedTime > fDelay )
                    {
                        Item.Increment();
                        m_ItemChangeDelay = 2;  // OnChangeItem() will fire when 0.
                        if( fDelay == fInitialDelay )
                        {
                            fDelay = fRepeatDelay;
                        }
                        else if( fDelay > fDelayMin )
                        {
                            fDelay -= fRepeatAccl;
                        }
                        fElapsedTime = 0.0f;
                    }
                }
            }
        }
        else if( DecrementHeld )
        {
            debug_menu_page* pPage = m_Pages[m_iActivePage];
            if( pPage )
            {
                debug_menu_item& Item = pPage->GetActiveItem();
                if( Item.GetType() == debug_menu_item::TYPE_FLOAT )
                {
                    fElapsedTime += DeltaTime;
                    if( fElapsedTime > fDelay )
                    {
                        Item.Decrement();
                        m_ItemChangeDelay = 2;  // OnChangeItem() will fire when 0.
                        if( fDelay == fInitialDelay )
                        {
                            fDelay = fRepeatDelay;
                        }
                        else if(fDelay > fDelayMin )
                        {
                            fDelay -= fRepeatAccl;
                        }
                        fElapsedTime = 0.0f;
                    }
                }
            }
        }
    }   // IsActive() 

    return TRUE;
}

//==============================================================================

void debug_menu2::Render( void )
{
    if( eng_Begin( "debug menu" ))
    {
        if( !g_StateMgr.IsPaused())
        {
            // render debug code that happens whether menu is active or not
            for( s32 p = 0; p < m_Pages.GetCount(); p++)
            {
                m_Pages[p]->OnPreRender();
            }
            m_Pages[m_iActivePage]->OnPreRenderActive();
        }

        // display current page if not taking screen-shot.
        if(    (m_bMenuActive)
#ifndef X_RETAIL
            && (!eng_ScreenShotActive())
#endif
        )
        {
            const irect rb = g_UIRenderer.GetViewport().GetLogicalClipBounds();
            g_UIRenderer.DrawRect( rb, xcolor(0,0,0,(u8)(255 * m_FadeAlpha)) );

            m_Pages[m_iActivePage]->Render();

            // render debug code that happens when menu is active
            // this is the overridable code for drawing over debug menus
            // I'm not sure it really makes sense, but doesn't hurt to be here.
            for( s32 p = 0; p < m_Pages.GetCount(); p++)
            {
                m_Pages[p]->OnRender();
            }
            m_Pages[m_iActivePage]->OnRenderActive();
        }
        eng_End();
    }
}

//==============================================================================
// This is our own rendering helper.

void debug_menu2::RenderLine( s32 iFontNum, const char* pLine, irect iRect, xbool bHighlight, xbool bFlash)
{
#ifdef TEXT_SHADOW
    irect Shadow;
    xcolor TextShadowColor( 0, 0, 0 );
#endif
    static f32 Flare = 0.0f;
    static f32 FlareIncr = 0.005f;

    // This is NOT an error, just don't bother to render it.
    if( pLine == NULL ) 
        return;

    xwstring text( pLine );

#ifdef TEXT_SHADOW
    Shadow = iRect;
    Shadow.Translate( 0, 2 );
    g_UiMgr->RenderText( iFontNum, Shadow, ui_font::h_left|ui_font::v_top, TextShadowColor, text, TRUE, FALSE );

    Shadow = iRect;
    Shadow.Translate( -2, 0 );
    g_UiMgr->RenderText( iFontNum, Shadow, ui_font::h_left|ui_font::v_top, TextShadowColor, text, TRUE, FALSE );

    Shadow = iRect;
    Shadow.Translate( 2, 0 );
    g_UiMgr->RenderText( iFontNum, Shadow, ui_font::h_left|ui_font::v_top, TextShadowColor, text, TRUE, FALSE );
    
    Shadow = iRect;
    Shadow.Translate( 0, -2 );
    g_UiMgr->RenderText( iFontNum, Shadow, ui_font::h_left|ui_font::v_top, TextShadowColor, text, TRUE, FALSE );
#endif

    xcolor TextColor        ( 250, 250,   0 );
    xcolor HighlightColor   ( 150, 150, 150 );
    xcolor FlashColor       ( 255,   0,   0 );

    if( bFlash )
    {
        g_UiMgr->RenderText( iFontNum, iRect, ui_font::h_left|ui_font::v_top, FlashColor, text, TRUE, TRUE );
    }
    else if( bHighlight )
    {
        g_UiMgr->RenderText( iFontNum, iRect, ui_font::h_left|ui_font::v_top, HighlightColor, text, TRUE, TRUE, Flare );
    }
    else
    {
        g_UiMgr->RenderText( iFontNum, iRect, ui_font::h_left|ui_font::v_top, TextColor, text, TRUE, TRUE );
    }

    Flare += FlareIncr;
    if( Flare > 3.0f )
    {
        Flare = 3.0f;
        FlareIncr = -FlareIncr;
    }
    if( Flare < 0.0f )
    {
        Flare = 0.0f;
        FlareIncr = -FlareIncr;
    }
}

//==============================================================================

#endif // defined( ENABLE_DEBUG_MENU )
