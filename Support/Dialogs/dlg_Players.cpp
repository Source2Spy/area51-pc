//=========================================================================
//
//  dlg_Players.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_listbox.hpp"
#include "dlg_Players.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"

//=========================================================================

static ui_manager::control_tem PlayersControls[] =
{
    { 0, "IDS_ONLINE_PLAYERS", "listbox", 40, 40, 416, 260, 0, 0, 1, 1, ui_win::WF_VISIBLE },
};

static ui_manager::dialog_tem PlayersDialog =
{
    "IDS_ONLINE_PLAYERS", 1, 1,
    sizeof( PlayersControls )/sizeof( ui_manager::control_tem ),
    PlayersControls, 0
};

//=========================================================================

void dlg_players_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "players list", &PlayersDialog, &dlg_players_factory );
}

//=========================================================================

ui_win* dlg_players_factory( s32                       UserID,
                             ui_manager*               pManager,
                             ui_manager::dialog_tem*   pDialogTem,
                             const irect&              Position,
                             ui_win*                   pParent,
                             s32                       Flags,
                             void*                     pUserData )
{
    dlg_players* pDialog = new dlg_players;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return pDialog;
}

//=========================================================================

xbool dlg_players::Create( s32                       UserID,
                           ui_manager*               pManager,
                           ui_manager::dialog_tem*   pDialogTem,
                           const irect&              Position,
                           ui_win*                   pParent,
                           s32                       Flags,
                           void*                     pUserData )
{
    (void)pUserData;

    if( !ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags ) )
        return FALSE;

    m_pPlayerList = (ui_listbox*)FindChildByID( 0 );
    m_pPlayerList->SetActive( TRUE );
    m_pPlayerList->SetLineHeight( 24 );
    m_pPlayerList->SetBackgroundColor( xcolor( 39, 117, 28, 128 ) );
    m_pPlayerList->EnableHeaderBar();
    m_pPlayerList->SetHeaderBarColor( xcolor( 19, 59, 14, 196 ) );
    m_pPlayerList->SetHeaderColor( xcolor( 255, 252, 204, 255 ) );
    m_pPlayerList->SetExitOnSelect( FALSE );
    m_pPlayerList->SetExitOnBack( TRUE );
    m_pPlayerList->DisableFrame();
    m_pPlayerList->SetFlag( WF_VISIBLE, FALSE );

    m_bRenderBlackout = FALSE;
    x_memset( m_Identifiers, 0, sizeof( m_Identifiers ) );
    m_State = DIALOG_STATE_ACTIVE;

    xwstring NavText( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) );
    NavText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
    SetNavText( NavText );

    FillPlayersList();
    GotoControl( m_pPlayerList );

    InitScreenScaling( Position );

    return TRUE;
}

//=========================================================================

void dlg_players::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;

    if( m_bRenderBlackout )
    {
        rb = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect( rb, xcolor( 0, 0, 0, 180 ),
            xcolor( 0, 0, 0, 180 ),
            xcolor( 0, 0, 0, 180 ),
            xcolor( 0, 0, 0, 180 ), FALSE );
    }

    // render transparent screen
    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;

    g_UiMgr->RenderGouraudRect( rb, xcolor( 56, 115, 58, 64 ),
                                   xcolor( 56, 115, 58, 64 ),
                                   xcolor( 56, 115, 58, 64 ),
                                   xcolor( 56, 115, 58, 64 ), FALSE );

    // render the screen bars
    s32 y = rb.t + offset;

    while ( y < rb.b )
    {
        irect bar;

        if ( (y+width) > rb.b )
        {
            bar.Set( rb.l, y, rb.r, rb.b );
        }
        else
        {
            bar.Set( rb.l, y, rb.r, y+width );
        }

        // draw the bar
        g_UiMgr->RenderGouraudRect( bar, xcolor( 56, 115, 58, 30 ),
                                        xcolor( 56, 115, 58, 30 ),
                                        xcolor( 56, 115, 58, 30 ),
                                        xcolor( 56, 115, 58, 30 ), FALSE );

        y+=gap;
    }

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_players::OnAccept( ui_win* pWin )
{
    (void)pWin;

    s32 Selection = m_pPlayerList->GetSelection();
    if( (m_State != DIALOG_STATE_ACTIVE) || (Selection < 0) )
        return;

    s32 Slot = m_pPlayerList->GetSelectedItemData();
    const player_score& Player = GameMgr.GetScore().Player[Slot];
    if( !Player.IsConnected || Player.IsBot ||
        (Slot == g_NetworkMgr.GetLocalPlayerSlot( 0 )) ||
        (Player.Identifier != m_Identifiers[Slot]) )
        return;

    g_MatchMgr.SetIsPlayerMuted( Player.Identifier, !g_MatchMgr.IsPlayerMuted( Player.Identifier ) );
    FillPlayersList();
}

//=========================================================================

void dlg_players::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
        m_State = DIALOG_STATE_BACK;
}

//=========================================================================

void dlg_players::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    if( m_State != DIALOG_STATE_ACTIVE )
        return;

    if( g_UiMgr->IsScreenScaling() && !UpdateScreenScaling( DeltaTime ) )
    {
        m_pPlayerList->SetFlag( WF_VISIBLE, TRUE );
        g_UiMgr->SetScreenHighlight( m_pPlayerList->GetPosition() );
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );
    FillPlayersList();
}

//=========================================================================

void dlg_players::FillPlayersList( void )
{
    const game_score& Score = GameMgr.GetScore();

    u64 Selected = 0;
    if( m_pPlayerList->GetSelection() >= 0 )
        Selected = m_Identifiers[m_pPlayerList->GetSelectedItemData()];

    s32 Count = 0;
    xbool Changed = FALSE;

    for( s32 Slot = 0; Slot < NET_MAX_PLAYERS; Slot++ )
    {
        if( !Score.Player[Slot].IsConnected )
            continue;

        if( (Count >= m_pPlayerList->GetItemCount()) ||
            (m_pPlayerList->GetItemData( Count ) != (uaddr)Slot) ||
            (m_Identifiers[Slot] != Score.Player[Slot].Identifier) )
            Changed = TRUE;

        Count++;
    }

    Changed |= Count != m_pPlayerList->GetItemCount();

    if( Changed )
        m_pPlayerList->DeleteAllItems();

    s32 Row = 0;
    for( s32 Slot = 0; Slot < NET_MAX_PLAYERS; Slot++ )
    {
        const player_score& Player = Score.Player[Slot];
        if( !Player.IsConnected )
            continue;

        m_Identifiers[Slot] = Player.Identifier;
        xwstring Label( Player.Name );
        if( g_MatchMgr.IsPlayerMuted( Player.Identifier ) )
        {
            Label += "  ";
            Label += g_StringTableMgr( "ui", "IDS_STATUS_VOICE_MUTED" );
        }

        xbool Enabled = !Player.IsBot && (Slot != g_NetworkMgr.GetLocalPlayerSlot( 0 ));
        if( Changed )
        {
            m_pPlayerList->AddItem( Label, Slot, 0, Enabled );
            if( Player.Identifier == Selected )
                m_pPlayerList->SetSelection( Row );
        }
        else
        {
            m_pPlayerList->SetItemLabel( Row, Label );
            m_pPlayerList->EnableItem( Row, Enabled );
        }

        Row++;
    }
}
