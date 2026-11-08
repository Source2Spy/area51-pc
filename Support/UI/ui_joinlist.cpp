//=========================================================================
//
//  ui_joinlist.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "ui_joinlist.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "StringMgr/StringMgr.hpp"

//=========================================================================

ui_win* ui_joinlist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_joinlist* pList = new ui_joinlist;
    if( !pList->Create( UserID, pManager, Position, pParent, Flags ) )
    {
        delete pList;
        return NULL;
    }
    return pList;
}

//=========================================================================

static irect GetColumnRect( const irect& Row, s32 Column )
{
    static const s32 Offsets[] = { 0, 29, 48, 60, 79, 91, 100 };
    irect Result = Row;
    Result.l = Row.l + Row.GetWidth() * Offsets[Column] / 100 + 4;
    Result.r = Row.l + Row.GetWidth() * Offsets[Column+1] / 100 - 4;
    return Result;
}

//=========================================================================

void ui_joinlist::RenderHeader( irect r )
{
    r.Deflate( 2, 0 );
    r.r -= 2;
    static const char* Labels[] =
    {
        "IDS_HEADER_SERVER",
        "IDS_HEADER_PLAYERS",
        "IDS_BROWSER_MODE",
        "IDS_BROWSER_MAP",
        "IDS_BROWSER_PING",
        "IDS_BROWSER_PASSWORD_SHORT",
    };
    for( s32 i = 0; i < MATCH_SORT_COUNT; i++ )
    {
        xwstring Label( g_StringTableMgr("ui", Labels[i]) );
        if( i == m_Sort )
            Label += m_Descending ? " v" : " ^";
        RenderTitle( GetColumnRect(r, i), ui_font::h_left | ui_font::v_center |
                     ui_font::clip_ellipsis, Label );
    }
}

//=========================================================================

void ui_joinlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_joinlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_joinlist::RenderTitle( irect r, u32 Flags, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, XCOLOR_BLACK, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, m_HeaderColor, pString );
}

//=========================================================================

const server_info* ui_joinlist::GetServerInfo( s32 Index ) const
{
    return ((Index >= 0) && (Index < m_Servers.GetCount())) ? &m_Servers[Index] : NULL;
}

//=========================================================================

void ui_joinlist::RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 )
{
    const server_info* pServer = GetServerInfo( (s32)Item.Data[0] );
    if( !pServer )
        return;

    if( m_ShowFrame )
        r.Deflate( 1, 1 );
    r.Translate( 0, -2 );

    u32 Flags = ui_font::h_left | ui_font::v_center | ui_font::clip_ellipsis;
    RenderString( GetColumnRect(r, 0), Flags, c1, c2, pServer->Name );
    RenderString( GetColumnRect(r, 1), ui_font::h_center | ui_font::v_center |
                  ui_font::clip_ellipsis, c1, c2, xfs("%d/%d", pServer->Players, pServer->MaxPlayers) );
    RenderString( GetColumnRect(r, 2), Flags, c1, c2, pServer->ShortGameType );
    RenderString( GetColumnRect(r, 3), Flags, c1, c2, pServer->MissionName );
    RenderString( GetColumnRect(r, 4), Flags, c1, c2,
                  (pServer->PingDelay < 0.0f) ? "--" : xfs("%d", (s32)pServer->PingDelay) );
    RenderString( GetColumnRect(r, 5), Flags, c1, c2,
                  (pServer->Flags & SERVER_HAS_PASSWORD) ? "+" : "" );
}

//=========================================================================

void ui_joinlist::SetSort( match_sort Sort, xbool Descending )
{
    m_Sort = Sort;
    m_Descending = Descending;
}

//=========================================================================

irect ui_joinlist::GetListRect( void ) const
{
    irect Bounds = ui_listbox::GetListRect();
    Bounds.r -= 1;
    return Bounds;
}

//=========================================================================

void ui_joinlist::Render( s32 ox, s32 oy )
{
    ui_listbox::Render( ox, oy );
    if( (m_Flags & WF_VISIBLE) && !GetItemCount() )
    {
        irect r = GetListRect();
        r.Translate( ox, oy );
        r.t += 26;
        m_pManager->RenderText( m_Font, r, ui_font::h_center | ui_font::v_center |
                               ui_font::clip_ellipsis, m_HeaderColor, m_Label );
    }
}

//=========================================================================

void ui_joinlist::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    s32 LocalX = x;
    s32 LocalY = y;
    ScreenToLocal( LocalX, LocalY );
    if( m_ShowHeaderBar && (LocalY >= 0) && (LocalY < 22) )
    {
        m_LastClickID = -1;
        irect Header = GetListRect();
        Header.Translate( -m_Position.l, -m_Position.t );
        Header.SetHeight( 22 );
        Header.Deflate( 2, 0 );
        Header.r -= 2;
        for( s32 i = 0; i < MATCH_SORT_COUNT; i++ )
        {
            irect Column = GetColumnRect( Header, i );
            if( (LocalX >= Column.l - 4) && (LocalX < Column.r + 4) )
            {
                m_Descending = (m_Sort == i) ? !m_Descending : FALSE;
                m_Sort = (match_sort)i;
                Notify( ui_notification_type::ListSortChanged, i );
                return;
            }
        }
        return;
    }
    UpdateScrollBar();
    if( m_ShowBorders && m_ScrollBar.OnPointerDown(x, y) )
    {
        m_LastClickID = -1;
        m_HoveredItem = -1;
        SetFirstVisibleItem( m_ScrollBar.GetPosition() );
        if( m_ScrollBar.IsInteracting() )
            m_pManager->SetCapture( m_UserID, this );
        return;
    }
    s32 Index = GetItemAt( x, y );
    if( (Index < 0) || !m_Items[Index].Enabled )
    {
        m_LastClickID = -1;
        return;
    }

    xbool const SelectionChanged = SelectUserItem( Index );
    const server_info* pServer = GetServerInfo( (s32)m_Items[Index].Data[0] );
    if( !pServer )
    {
        m_LastClickID = -1;
        return;
    }

    xbool const RepeatClick = !SelectionChanged && (pServer->ID == m_LastClickID) &&
                             (pServer->Remote == m_LastClickAddress);
    m_LastClickID = RepeatClick ? -1 : pServer->ID;
    m_LastClickAddress = pServer->Remote;
    if( RepeatClick )
        OnAccept( pWin );
}

//==============================================================================

void ui_joinlist::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses,
                             s32 Repeats, xbool WrapX, xbool WrapY )
{
    m_LastClickID = -1;
    ui_listbox::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

//==============================================================================

void ui_joinlist::OnFocusLost( ui_win* pWin )
{
    m_LastClickID = -1;
    ui_listbox::OnFocusLost( pWin );
}
