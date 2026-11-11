//==============================================================================
//  DebugMenuPage.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu page.
//  
//==============================================================================

#include "DebugMenu2.hpp"
#include <cstring>
#include "UI/ui_manager.hpp"
#include "UI/ui_renderer.hpp"

//==============================================================================

#if defined( ENABLE_DEBUG_MENU )

//==============================================================================

debug_menu_page::debug_menu_page( void )
{
    m_pTitle        = "<null>";
    m_iActiveItem   = 0;
    m_iFirstVisibleItem = 0;
}

//==============================================================================

debug_menu_page::~debug_menu_page( void )
{
    for( s32 i = 0; i < m_Items.GetCount(); i++ )
        delete m_Items[i];
}

//==============================================================================

debug_menu_item& debug_menu_page::GetActiveItem( void )
{
    ASSERT( m_Items.GetCount() > 0 );

    return *m_Items[m_iActiveItem];
}

//==============================================================================

void debug_menu_page::NextItem( void )
{
    ASSERT( m_Items.GetCount() > 0 );

    s32 m_iOldItem = m_iActiveItem;
    do
    {
        m_iActiveItem = (m_iActiveItem + 1);
        if( m_iActiveItem >= m_Items.GetCount() )
            m_iActiveItem = 0;
    } while( (m_iOldItem != m_iActiveItem) && (m_Items[m_iActiveItem]->GetType() == debug_menu_item::TYPE_SEPERATOR) );
}

//==============================================================================

void debug_menu_page::PrevItem( void )
{
    ASSERT( m_Items.GetCount() > 0 );

    s32 m_iOldItem = m_iActiveItem;
    do
    {
        m_iActiveItem = (m_iActiveItem - 1);
        if( m_iActiveItem < 0 )
            m_iActiveItem = m_Items.GetCount()-1;
    } while( (m_iOldItem != m_iActiveItem) && (m_Items[m_iActiveItem]->GetType() == debug_menu_item::TYPE_SEPERATOR) );
}

//==============================================================================

s32 debug_menu_page::GetItemCount( void )
{
    return m_Items.GetCount();
}

//==============================================================================

debug_menu_item* debug_menu_page::GetItem( s32 ItemNum )
{
    ASSERT( (ItemNum >= 0) && (ItemNum < m_Items.GetCount()) );

    return m_Items[ItemNum];
}

//==============================================================================
debug_menu_item* debug_menu_page::FindItem( const char* pName )
{
    // look for the page with a given title string
    for (s32 iItem = 0; iItem < m_Items.GetCount(); iItem++ )
    {
        if( strcmp(m_Items[iItem]->GetName(), pName) == 0 ) return m_Items[iItem];
    }
    return NULL;
}

//==============================================================================

const char* debug_menu_page::GetTitle( void )
{
    return m_pTitle;
}

//==============================================================================

void debug_menu_page::Render( void )
{
    const s32 Font = g_UiMgr->FindFont( "large" );
    if( Font < 0 )
        return;
    const irect Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();
    const s32 LineHeight = g_UiMgr->GetLineHeight( Font );
    const s32 RowHeight = MAX( 18, LineHeight );
    const s32 TitleGap = 34;
    const s32 Margin = 30;
    const s32 AvailableHeight = MAX( RowHeight, Bounds.GetHeight() - LineHeight - TitleGap - Margin * 2 );
    m_iFirstVisibleItem = MIN( m_iFirstVisibleItem, m_iActiveItem );
    s32 ActiveBottom = 0;
    for( s32 i = m_iFirstVisibleItem; i <= m_iActiveItem && i < m_Items.GetCount(); i++ )
        ActiveBottom += m_Items[i]->GetType() == debug_menu_item::TYPE_SEPERATOR &&
                        !x_strlen( m_Items[i]->GetName() ) ? 14 : RowHeight;
    while( ActiveBottom > AvailableHeight && m_iFirstVisibleItem < m_iActiveItem )
    {
        debug_menu_item* pItem = m_Items[m_iFirstVisibleItem++];
        ActiveBottom -= pItem->GetType() == debug_menu_item::TYPE_SEPERATOR &&
                        !x_strlen( pItem->GetName() ) ? 14 : RowHeight;
    }

    s32 BodyHeight = 0;
    s32 LastItem = m_iFirstVisibleItem;
    while( LastItem < m_Items.GetCount() )
    {
        debug_menu_item* pItem = m_Items[LastItem];
        const s32 Height = pItem->GetType() == debug_menu_item::TYPE_SEPERATOR &&
                           !x_strlen( pItem->GetName() ) ? 14 : RowHeight;
        if( BodyHeight + Height > AvailableHeight )
            break;
        BodyHeight += Height;
        LastItem++;
    }

    while( LastItem > m_iFirstVisibleItem )
    {
        debug_menu_item* pItem = m_Items[LastItem - 1];
        if( pItem->GetType() != debug_menu_item::TYPE_SEPERATOR || x_strlen( pItem->GetName() ) )
            break;
        BodyHeight -= 14;
        LastItem--;
    }

    s32 BodyWidth = 0;
    for( s32 i = m_iFirstVisibleItem; i < LastItem; i++ )
    {
        debug_menu_item* pItem = m_Items[i];
        irect TextBounds;
        const xwstring Label( pItem->GetName() );
        g_UiMgr->TextSize( Font, TextBounds, Label, Label.GetLength() );
        BodyWidth = MAX( BodyWidth, TextBounds.GetWidth() );
        const xwstring Value( pItem->GetValueAsString() );
        if( Value.GetLength() )
        {
            g_UiMgr->TextSize( Font, TextBounds, Value, Value.GetLength() );
            BodyWidth = MAX( BodyWidth, 300 + TextBounds.GetWidth() );
        }
    }

    const s32 Left = Bounds.l + ( Bounds.GetWidth() - BodyWidth ) / 2;
    const s32 Top = Bounds.t + ( Bounds.GetHeight() - LineHeight - TitleGap - BodyHeight ) / 2;
    const xwstring TitleText( m_pTitle );
    irect Title;
    g_UiMgr->TextSize( Font, Title, TitleText, TitleText.GetLength() );
    Title.Translate( Bounds.l + ( Bounds.GetWidth() - Title.GetWidth() ) / 2, Top );
    debug_menu2::RenderLine( Font, m_pTitle, Title );
    s32 Y = Top + LineHeight + TitleGap;
    for( s32 i = m_iFirstVisibleItem; i < LastItem; i++ )
    {
        debug_menu_item* pItem = m_Items[i];
        pItem->Render( Left, Y, i == m_iActiveItem );
        Y += pItem->GetType() == debug_menu_item::TYPE_SEPERATOR &&
             !x_strlen( pItem->GetName() ) ? 14 : RowHeight;
    }
}

//==============================================================================

debug_menu_item* debug_menu_page::AddItemSeperator( const char* pName )
{
    m_Items.Append() = new debug_menu_item();
    m_Items[m_Items.GetCount()-1]->InitSeperator( pName );
    return m_Items[m_Items.GetCount()-1];
}

//==============================================================================

debug_menu_item* debug_menu_page::AddItemButton( const char* pName )
{
    m_Items.Append() = new debug_menu_item();
    m_Items[m_Items.GetCount()-1]->InitButton( pName );
    return m_Items[m_Items.GetCount()-1];
}

//==============================================================================

debug_menu_item* debug_menu_page::AddItemBool( const char* pName, xbool& Var, const char** pValues )
{
    m_Items.Append() = new debug_menu_item();
    m_Items[m_Items.GetCount()-1]->InitBool( pName, Var, pValues );
    return m_Items[m_Items.GetCount()-1];
}

//==============================================================================

debug_menu_item* debug_menu_page::AddItemInt( const char* pName, s32& Var, s32 Min, s32 Max )
{
    m_Items.Append() = new debug_menu_item();
    m_Items[m_Items.GetCount()-1]->InitInt( pName, Var, Min, Max );
    return m_Items[m_Items.GetCount()-1];
}

//==============================================================================

debug_menu_item* debug_menu_page::AddItemFloat( const char* pName, f32& Var, f32 Min, f32 Max, f32 Increment )
{
    m_Items.Append() = new debug_menu_item();
    m_Items[m_Items.GetCount()-1]->InitFloat( pName, Var, Min, Max, Increment );
    return m_Items[m_Items.GetCount()-1];
}

//==============================================================================

debug_menu_item* debug_menu_page::AddItemEnum( const char* pName, s32& Var, const char** pValues, s32 nValues )
{
    m_Items.Append() = new debug_menu_item();
    m_Items[m_Items.GetCount()-1]->InitEnum( pName, Var, pValues, nValues );
    return m_Items[m_Items.GetCount()-1];
}

//==============================================================================

#endif // defined( ENABLE_DEBUG_MENU )
