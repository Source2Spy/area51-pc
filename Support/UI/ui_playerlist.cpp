//=========================================================================
//
//  ui_playerlist.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_listbox.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "ui_playerlist.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"

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

ui_win* ui_playerlist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_playerlist* pList = new ui_playerlist;
    pList->Create( UserID, pManager, Position, pParent, Flags );

    return (ui_win*)pList;
}

//=========================================================================
//  ui_listbox
//=========================================================================

ui_playerlist::ui_playerlist( void )
{
    m_ScoreFieldMask = 0;
    m_MaxPlayerWidth = 250;
}

//=========================================================================

ui_playerlist::~ui_playerlist( void )
{
}

//=========================================================================

void ui_playerlist::RenderHeader( irect r )
{
    r.l += 8;
    RenderTitle( r, ui_font::h_left | ui_font::v_center, g_StringTableMgr( "ui", "IDS_HEADER_PLAYER" ) );

    r.l += m_PlayerWidth;
    r.r = r.l + 35;

    if( m_ScoreFieldMask & SCORE_POINTS )
    {
        RenderTitle( r, ui_font::h_right | ui_font::v_center, g_StringTableMgr( "ui", "IDS_SCORE" ) );
    }

    if( m_ScoreFieldMask & SCORE_KILLS )
    {
        r.Translate( 35, 0 );
        RenderTitle( r, ui_font::h_right | ui_font::v_center, g_StringTableMgr( "ui", "IDS_ICON_KILLS" ) );
    }

    if( m_ScoreFieldMask & SCORE_DEATHS )
    {
        r.Translate( 35, 0 );
        RenderTitle( r, ui_font::h_right | ui_font::v_center, g_StringTableMgr( "ui", "IDS_ICON_DEATHS" ) );
    }

    if( m_ScoreFieldMask & SCORE_TKS )
    {
        r.Translate( 35, 0 );
        RenderTitle( r, ui_font::h_right | ui_font::v_center, g_StringTableMgr( "ui", "IDS_ICON_TEAM_KILLS" ) );
    }

    if( m_ScoreFieldMask & SCORE_FLAGS )
    {
        r.Translate( 35, 0 );
        RenderTitle( r, ui_font::h_right | ui_font::v_center, g_StringTableMgr( "ui", "IDS_ICON_FLAGS" ) );
    }

    if( m_ScoreFieldMask & SCORE_VOTES )
    {
        r.Translate( 35, 0 );
        RenderTitle( r, ui_font::h_right | ui_font::v_center, g_StringTableMgr( "ui", "IDS_ICON_VOTES" ) );
    }
}

//=========================================================================

void ui_playerlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_playerlist::RenderString( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, c2, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, c1, pString );
}

//=========================================================================

void ui_playerlist::RenderTitle( irect r, u32 Flags, const xwchar* pString )
{
    m_pManager->RenderText( m_Font, r, Flags, XCOLOR_BLACK, pString );
    r.Translate( -1, -1 );
    m_pManager->RenderText( m_Font, r, Flags, m_HeaderColor, pString );
}

//=========================================================================

void ui_playerlist::RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 )
{
    r.Deflate( 4, 0 );
    r.Translate( 1, -2 );

    const player_score* pScoreData = ( const player_score*)Item.Data[0];

    r.r = r.l + (m_PlayerWidth - 5);
    RenderString( r, ui_font::h_left|ui_font::v_center|ui_font::clip_ellipsis, c1, c2, pScoreData->NName );

    r.l += m_PlayerWidth;
    r.r = r.l + 35;
    if( m_ScoreFieldMask & SCORE_POINTS )
    {
        RenderString( r, ui_font::h_right|ui_font::v_center, c1, c2, xfs("%d",pScoreData->Score) );
    }
            
    if( m_ScoreFieldMask & SCORE_KILLS )
    {
        r.l += 35;
        r.r += 35;
        RenderString( r, ui_font::h_right|ui_font::v_center, c1, c2, xfs("%d",pScoreData->Kills) );
    }

    if( m_ScoreFieldMask & SCORE_DEATHS )
    {
        r.l += 35;
        r.r += 35;
        RenderString( r, ui_font::h_right|ui_font::v_center, c1, c2, xfs("%d",pScoreData->Deaths) );
    }

    if( m_ScoreFieldMask & SCORE_TKS )
    {
        r.l += 35;
        r.r += 35;
        RenderString( r, ui_font::h_right|ui_font::v_center, c1, c2, xfs("%d",pScoreData->TKs) );
    }

    if( m_ScoreFieldMask & SCORE_FLAGS )
    {
        r.l += 35;
        r.r += 35;
        RenderString( r, ui_font::h_right|ui_font::v_center, c1, c2, xfs("%d",pScoreData->Flags) );
    }

    if( m_ScoreFieldMask & SCORE_VOTES )
    {
        r.l += 35;
        r.r += 35;
        RenderString( r, ui_font::h_right|ui_font::v_center, c1, c2, xfs("%d",pScoreData->Votes) );
    }
}

//=========================================================================
