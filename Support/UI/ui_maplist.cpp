//=========================================================================
//
//  ui_maplist.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_listbox.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "ui_maplist.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "StateMgr/MapList.hpp"

#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"

//=========================================================================
//  Defines
//=========================================================================

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

//=========================================================================
//  Factory function
//=========================================================================

ui_win* ui_maplist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_maplist* pList = new ui_maplist;
    pList->Create( UserID, pManager, Position, pParent, Flags );

    return (ui_win*)pList;
}

//=========================================================================
//  ui_maplist
//=========================================================================

ui_maplist::ui_maplist( void )
{
}

//=========================================================================

ui_maplist::~ui_maplist( void )
{
}

//=========================================================================

void ui_maplist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_maplist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_maplist::RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 )
{
    r.Deflate( 4, 0 );
    r.Translate( 1, -2 );
    irect rt = r;

    map_entry* pEntry = (map_entry*)Item.Data[0];

    rt.r = rt.l + 50;
    RenderString( rt, ui_font::h_right|ui_font::v_center, c1, c2, pEntry->GetShortGameTypeName() );

    r.l += 70;
    RenderString( r, ui_font::h_left|ui_font::v_center|ui_font::clip_ellipsis, c1, c2, pEntry->GetDisplayName() );
}

//=========================================================================
