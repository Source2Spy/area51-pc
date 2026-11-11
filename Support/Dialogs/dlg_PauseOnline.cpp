//=========================================================================
//
//  dlg_PauseOnline.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"

#include "dlg_PauseOnline.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Objects/Player/Player.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================

ui_manager::control_tem PauseOnlineControls[] = 
{
    // Frames.
    { IDC_PAUSE_ONLINE_SUICIDE,        "IDS_ONLINE_PAUSE_SUICIDE",         "button",  110,  40, 120, 30, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_SWITCH_TEAM,    "IDS_ONLINE_PAUSE_SWITCH_TEAM",     "button",  110,  70, 120, 30, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_VOTE_MAP,    "IDS_ONLINE_PAUSE_VOTE_MAP",        "button",  110, 100, 120, 30, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_VOTE_KICK,   "IDS_ONLINE_PAUSE_VOTE_KICK",       "button",  110, 130, 120, 30, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_PLAYERS,     "IDS_ONLINE_PAUSE_PLAYERS",         "button",  110, 160, 120, 30, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_OPTIONS,        "IDS_ONLINE_PAUSE_OPTIONS",         "button",  110, 190, 120, 30, 0, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_SETTINGS,    "IDS_ONLINE_PAUSE_SETTINGS",        "button",  110, 220, 120, 30, 0, 6, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_CONFIG,      "IDS_ONLINE_PAUSE_SERVER_CONFIG",   "button",  110, 250, 120, 30, 0, 7, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_ONLINE_QUIT,        "IDS_PAUSE_MENU_QUIT",              "button",  110, 280, 120, 30, 0, 8, 1, 1, ui_win::WF_VISIBLE },
};


ui_manager::dialog_tem PauseOnlineDialog =
{
    "IDS_ONLINE_PAUSE_MENU",
    1, 9,
    sizeof(PauseOnlineControls)/sizeof(ui_manager::control_tem),
    &PauseOnlineControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_pause_online_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "pause online", &PauseOnlineDialog, &dlg_pause_online_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_pause_online_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_pause_online* pDialog = new dlg_pause_online;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_pause_online
//=========================================================================

dlg_pause_online::dlg_pause_online( void )
{
}

//=========================================================================

dlg_pause_online::~dlg_pause_online( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_pause_online::Create( s32                        UserID,
                             ui_manager*                pManager,
                             ui_manager::dialog_tem*    pDialogTem,
                             const irect&               Position,
                             ui_win*                    pParent,
                             s32                        Flags,
                             void*                      pUserData )
{
    xbool   Success = FALSE;

    (void)pUserData;

    ASSERT( pManager );

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    // get button handles
    m_pButtonSuicide    = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_SUICIDE     );
    m_pButtonSwitchTeam = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_SWITCH_TEAM );     
    m_pButtonVoteMap    = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_VOTE_MAP    );     
    m_pButtonVoteKick   = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_VOTE_KICK   );     
    m_pButtonPlayers    = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_PLAYERS     );     
    m_pButtonOptions    = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_OPTIONS     );     
    m_pButtonSettings   = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_SETTINGS    );
    m_pButtonConfig     = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_CONFIG      );     
    m_pButtonQuit       = (ui_button*)  FindChildByID( IDC_PAUSE_ONLINE_QUIT        );
 
    s32 iControl = g_StateMgr.GetCurrentControl();

    if( iControl > IDC_PAUSE_ONLINE_QUIT )
        iControl = 0;

    if( (iControl == -1) || (GotoControl( iControl )==NULL) )
    {
        GotoControl( (ui_control*)m_pButtonSuicide );
        m_CurrentControl = IDC_PAUSE_ONLINE_SUICIDE;
    }
    else
    {
        m_CurrentControl = iControl;
    }

    m_CurrHL = 0;
    m_PopUp = NULL;

    // switch off the buttons to start
    m_pButtonSuicide    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonSwitchTeam ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonVoteMap    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonVoteKick   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonPlayers    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonOptions    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonSettings   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonConfig     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonQuit       ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_QUIT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_SELECT" );   
    navText += g_StringTableMgr( "ui", "IDS_NAV_UNPAUSE" );   
    SetNavText( navText );


    // check for team based game
    const game_score& ScoreData = GameMgr.GetScore();
    if( !ScoreData.IsTeamBased )
    {
        m_pButtonSwitchTeam->SetFlag(ui_win::WF_DISABLED, TRUE);
    }

    // check if we have admin rights
    if( !g_NetworkMgr.IsServer() )
    {
        m_pButtonConfig->SetFlag(ui_win::WF_DISABLED, TRUE);
    }


    // initialize screen scaling
    InitScreenScaling( Position );

    // set the frame to be disabled (if coming from off screen)
    if (g_UiMgr->IsScreenOn() == FALSE)
        SetFlag( WF_DISABLED, TRUE );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_pause_online::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_pause_online::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;


    // render background filter
    rb = g_UiMgr->GetUserBounds( m_UserID );
    g_UiMgr->RenderGouraudRect(rb, xcolor(0,0,0,180),
                                   xcolor(0,0,0,180),
                                   xcolor(0,0,0,180),
                                   xcolor(0,0,0,180),FALSE);
    
    // render the screen (if we're correctly sized)
    if (g_UiMgr->IsScreenOn())
    {
        // render transparent screen
        rb.l = m_CurrPos.l + 22;
        rb.t = m_CurrPos.t;
        rb.r = m_CurrPos.r - 23;
        rb.b = m_CurrPos.b;

        g_UiMgr->RenderGouraudRect(rb, xcolor(56,115,58,64),
                                       xcolor(56,115,58,64),
                                       xcolor(56,115,58,64),
                                       xcolor(56,115,58,64),FALSE);


        // render the screen bars
        s32 y = rb.t + offset;    

        while (y < rb.b)
        {
            irect bar;

            if ((y+width) > rb.b)
            {
                bar.Set(rb.l, y, rb.r, rb.b);
            }
            else
            {
                bar.Set(rb.l, y, rb.r, y+width);
            }

            // draw the bar
            g_UiMgr->RenderGouraudRect(bar, xcolor(56,115,58,30),
                                            xcolor(56,115,58,30),
                                            xcolor(56,115,58,30),
                                            xcolor(56,115,58,30),FALSE);

            y+=gap;
        }
    }

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_pause_online::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonSwitchTeam )
        {
            // switch team?
            m_CurrentControl = IDC_PAUSE_ONLINE_SWITCH_TEAM;

            // open confirmation dialog
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
            SetNavTextVisible( FALSE );
            

            m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_GAMEMGR_POPUP" ), 
                                TRUE, 
                                TRUE, 
                                FALSE, 
                                g_StringTableMgr( "ui", "IDS_ONLINE_SWITCH_TEAM_MSG" ),
                                navText,
                                &m_PopUpResult );
        }
        else if( pWin == (ui_win*)m_pButtonSuicide )
        {
            // suicide?
            m_CurrentControl = IDC_PAUSE_ONLINE_SUICIDE;

            // open confirmation dialog
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
            SetNavTextVisible( FALSE );
            

            m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_GAMEMGR_POPUP" ), 
                                TRUE, 
                                TRUE, 
                                FALSE, 
                                g_StringTableMgr( "ui", "IDS_ONLINE_SUICIDE_MSG" ),
                                navText,
                                &m_PopUpResult );
        }
        else if( pWin == (ui_win*)m_pButtonVoteMap )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_ONLINE_VOTE_MAP;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonVoteKick )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_ONLINE_VOTE_KICK;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonPlayers )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_ONLINE_PLAYERS;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonOptions )
        {
            g_AudioMgr.Play("Select_Norm");
            g_StateMgr.InitPendingProfile( 0 );
            m_CurrentControl = IDC_PAUSE_ONLINE_OPTIONS;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonSettings )
        {            
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_ONLINE_SETTINGS;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonConfig )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_ONLINE_CONFIG;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonQuit )
        {
            OnDelete( pWin );
        }
    }
}

//=========================================================================

void dlg_pause_online::OnCancel( ui_win* pWin )
{
    (void)pWin;
}

//=========================================================================

void dlg_pause_online::OnDelete( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( m_PopUp == NULL )
        {


            // Open a dialog to confirm quitting the online game component
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

            // get message text
            xwstring Message;
            if( g_NetworkMgr.IsOnline() && g_NetworkMgr.IsServer() )
            {
                Message = g_StringTableMgr( "ui", "IDS_QUIT_ONLINE_MSG" );
            }
            else
            {
                Message = g_StringTableMgr( "ui", "IDS_QUIT_MSG" );
            }            

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
            SetNavTextVisible( FALSE );

            m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_QUIT_POPUP" ), TRUE, TRUE, FALSE, Message, navText, &m_PopUpResult );

            m_CurrentControl = IDC_PAUSE_ONLINE_QUIT;
        }
    }
}

//=========================================================================

void dlg_pause_online::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    s32 highLight = -1;

    //
    // Figure out if we have invitations to be friends or join games
    //

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the buttons
            m_pButtonSuicide    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonSwitchTeam ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonVoteMap    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonVoteKick   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonPlayers    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonOptions    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonSettings   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonQuit       ->SetFlag(ui_win::WF_VISIBLE, TRUE);


            if( m_pButtonConfig->GetFlags(WF_DISABLED) == FALSE )
            {
                m_pButtonConfig     ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            }

            s32 iControl = g_StateMgr.GetCurrentControl();

            if( iControl > IDC_PAUSE_ONLINE_QUIT )
                iControl = 0;

            if( (iControl == -1) || (GotoControl(iControl)==NULL) )
            {
                GotoControl( (ui_control*)m_pButtonSuicide );
                g_UiMgr->SetScreenHighlight( m_pButtonSuicide->GetPosition() );
                m_CurrentControl = IDC_PAUSE_ONLINE_SUICIDE;
            }
            else
            {
                ui_control* pControl = GotoControl( iControl );
                g_UiMgr->SetScreenHighlight(pControl->GetPosition() );
                m_CurrentControl = iControl;
            }

            if (g_UiMgr->IsScreenOn() == FALSE)
            {
                // enable the frame
                SetFlag( WF_DISABLED, FALSE );
                g_UiMgr->SetScreenOn(TRUE);
            }
        }
    }

    if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            switch( m_CurrentControl )
            {
                case IDC_PAUSE_ONLINE_SWITCH_TEAM:
                    if ( m_PopUpResult == DLG_POPUP_YES )
                    {
                        // switch teams here
                        s32     NetSlot = g_NetworkMgr.GetLocalPlayerSlot(0);
                        player* pPlayer = (player*)NetObjMgr.GetObjFromSlot( NetSlot );
                        ASSERT( pPlayer );
                        s32     Team = GameMgr.GetScore().Player[NetSlot].Team;
                        pPlayer->net_RequestTeam( 1 - Team );

                        // exit pause menu
                        m_State = DIALOG_STATE_SELECT;
                    }
                    break;

                case IDC_PAUSE_ONLINE_SUICIDE:
                    if ( m_PopUpResult == DLG_POPUP_YES )
                    {
                        // suicide player here
                        GameMgr.PlayerSuicide();

                        // exit pause menu
                        m_State = DIALOG_STATE_SELECT;
                    }
                    break;

                case IDC_PAUSE_ONLINE_QUIT:
                    if ( m_PopUpResult == DLG_POPUP_YES )
                    {
                        // quit game
                        m_State = DIALOG_STATE_DELETE;
                        m_CurrentControl = IDC_PAUSE_ONLINE_SUICIDE;
                    }
            }

            // clear popup 
            m_PopUp = NULL;

            // turn on nav text
            SetNavTextVisible( TRUE );
        }
    }

    // Update the state of the two vote items.
    {
        s32     NetSlot = g_NetworkMgr.GetLocalPlayerSlot(0);
        player* pPlayer = (player*)NetObjMgr.GetObjFromSlot( NetSlot );
        ASSERT( pPlayer );
        u32     KickMask     = pPlayer->GetVoteCanStartKick();
        xbool   CanStartMap  = pPlayer->GetVoteCanStartMap();
        xbool   CanStartKick = !!(KickMask);

        m_pButtonVoteMap ->SetFlag( ui_win::WF_DISABLED, !CanStartMap  );
        m_pButtonVoteKick->SetFlag( ui_win::WF_DISABLED, !CanStartKick );

        if( (!CanStartMap) && (m_pButtonVoteMap->IsFocused()) )
        {
            OnNavigate( this, ui_navigation::Up, 1, 0 );
        }

        if( (!CanStartKick) && (m_pButtonVoteKick->IsFocused()) )
        {
            OnNavigate( this, ui_navigation::Down, 1, 0 );
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    if( m_pButtonSuicide->IsFocused() )
    {
        highLight = 0;
        g_UiMgr->SetScreenHighlight( m_pButtonSuicide->GetPosition() );
    }
    else if( m_pButtonSwitchTeam->IsFocused() )
    {
        highLight = 1;
        g_UiMgr->SetScreenHighlight( m_pButtonSwitchTeam->GetPosition() );
    }
    else if( m_pButtonVoteMap->IsFocused() )
    {
        highLight = 2;
        g_UiMgr->SetScreenHighlight( m_pButtonVoteMap->GetPosition() );
    }
    else if( m_pButtonVoteKick->IsFocused() )
    {
        highLight = 3;
        g_UiMgr->SetScreenHighlight( m_pButtonVoteKick->GetPosition() );
    }
    else if( m_pButtonOptions->IsFocused() )
    {
        highLight = 4;
        g_UiMgr->SetScreenHighlight( m_pButtonOptions->GetPosition() );
    }
    else if( m_pButtonPlayers->IsFocused() )
    {
        highLight = 6;
        g_UiMgr->SetScreenHighlight( m_pButtonPlayers->GetPosition() );
    }
    else if( m_pButtonSettings->IsFocused() )
    {
        highLight = 7;
        g_UiMgr->SetScreenHighlight( m_pButtonSettings->GetPosition() );
    }
    else if( m_pButtonConfig->IsFocused() )
    {
        highLight = 8;
        g_UiMgr->SetScreenHighlight( m_pButtonConfig->GetPosition() );
    }
    else if( m_pButtonQuit->IsFocused() )
    {
        highLight = 9;
        g_UiMgr->SetScreenHighlight( m_pButtonQuit->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }
}

//=========================================================================
