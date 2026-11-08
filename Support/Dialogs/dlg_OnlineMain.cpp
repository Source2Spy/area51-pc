//=========================================================================
//
//  dlg_online_main.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"

#include "dlg_OnlineMain.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================
ui_manager::control_tem OnlineMenuControls[] = 
{
    // Frames.
    { IDC_ONLINE_JOIN,          "IDS_ONLINE_JOIN",          "button",   90,  80, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_ONLINE_HOST,          "IDS_ONLINE_HOST",          "button",   90, 120, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_ONLINE_EDIT_PROFILE,  "IDS_ONLINE_EDIT_PROFILE",  "button",   90, 160, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_ONLINE_VIEW_STATS,    "IDS_ONLINE_VIEW_STATS",    "button",   90, 200, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_ONLINE_SIGN_OUT,      "IDS_SIGN_OUT",             "button",   90, 240, 120, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
};


ui_manager::dialog_tem OnlineMenuDialog =
{
    "IDS_ONLINE_MAIN",
    1, 9,
    sizeof(OnlineMenuControls)/sizeof(ui_manager::control_tem),
    &OnlineMenuControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_online_main_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "online menu", &OnlineMenuDialog, &dlg_online_main_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_online_main_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_online_main* pDialog = new dlg_online_main;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_online_main
//=========================================================================

dlg_online_main::dlg_online_main( void )
{
}

//=========================================================================

dlg_online_main::~dlg_online_main( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_online_main::Create( s32                        UserID,
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

    m_pButtonJoin               = (ui_button*)  FindChildByID( IDC_ONLINE_JOIN         );
    m_pButtonHost               = (ui_button*)  FindChildByID( IDC_ONLINE_HOST         );
    m_pButtonEditProfile        = (ui_button*)  FindChildByID( IDC_ONLINE_EDIT_PROFILE );
    m_pButtonViewStats          = (ui_button*)  FindChildByID( IDC_ONLINE_VIEW_STATS   );
    m_pButtonSignOut            = (ui_button*)  FindChildByID( IDC_ONLINE_SIGN_OUT     );
    m_pPopUp                    = NULL;

    s32 iControl = g_StateMgr.GetCurrentControl();
    if( (iControl == -1) || (GotoControl( iControl )==NULL) )
    {
        GotoControl( (ui_control*)m_pButtonJoin );
        m_CurrentControl = IDC_ONLINE_HOST;
    }
    else
    {
        m_CurrentControl = iControl;
    }

    m_CurrHL = 0;

    // switch off the buttons to start
    m_pButtonJoin        ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonHost        ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonEditProfile ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonViewStats   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonSignOut     ->SetFlag(ui_win::WF_VISIBLE, FALSE);



    // initialize nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
  
    SetNavText( navText );
    
    // initialize screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_online_main::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_online_main::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;
    
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

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_online_main::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonJoin )
        {
            m_CurrentControl = IDC_ONLINE_JOIN;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonHost )
        {
            m_CurrentControl = IDC_ONLINE_HOST;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonEditProfile )
        {
            g_StateMgr.InitPendingProfile( 0 );
            m_CurrentControl = IDC_ONLINE_EDIT_PROFILE;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin ==(ui_win*)m_pButtonViewStats )
        {
            m_CurrentControl = IDC_ONLINE_VIEW_STATS;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonSignOut )
        {
            // Open a dialog to confirm quitting the online game component
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_pPopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

            // get message text
            xwstring Message( g_StringTableMgr( "ui", "IDS_ONLINE_DISCONNECT" ));

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
            SetNavTextVisible( FALSE );

            m_pPopUp->Configure( g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ), TRUE, TRUE, FALSE, Message, navText, &m_PopUpResult );

        }
        g_AudioMgr.Play("Select_Norm");
    }
}

//=========================================================================

void dlg_online_main::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( m_pPopUp == NULL )
        {


            // Open a dialog to confirm quitting the online game component
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_pPopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

            // get message text
            xwstring Message( g_StringTableMgr( "ui", "IDS_ONLINE_DISCONNECT" ));

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
            SetNavTextVisible( FALSE );

            m_pPopUp->Configure( g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ), TRUE, TRUE, FALSE, Message, navText, &m_PopUpResult );
        }
    }
}

//=========================================================================

void dlg_online_main::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    s32 highLight = -1;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the controls
            m_pButtonJoin        ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonHost        ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonViewStats   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonEditProfile ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonSignOut     ->SetFlag(ui_win::WF_VISIBLE, TRUE);

            s32 iControl = g_StateMgr.GetCurrentControl();
            if( (iControl == -1) || (GotoControl(iControl)==NULL) )
            {
                GotoControl( (ui_control*)m_pButtonJoin );
                g_UiMgr->SetScreenHighlight( m_pButtonJoin->GetPosition() );
                m_CurrentControl = IDC_ONLINE_HOST;
            }
            else
            {
                ui_control* pControl = GotoControl( iControl );
                ASSERT( pControl );
                g_UiMgr->SetScreenHighlight(pControl->GetPosition() );
                m_CurrentControl = iControl;
            }
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    if( m_pButtonJoin->IsFocused() )
    {
        highLight = 0;
        g_UiMgr->SetScreenHighlight( m_pButtonJoin->GetPosition() );
    }
    else if( m_pButtonHost->IsFocused() )
    {
        highLight = 1;
        g_UiMgr->SetScreenHighlight( m_pButtonHost->GetPosition() );
    }
    else if( m_pButtonEditProfile->IsFocused() )
    {
        highLight = 4;
        g_UiMgr->SetScreenHighlight( m_pButtonEditProfile->GetPosition() );
    }
    else if( m_pButtonViewStats->IsFocused() )
    {
        highLight = 5;
        g_UiMgr->SetScreenHighlight( m_pButtonViewStats->GetPosition() );
    }
    else if( m_pButtonSignOut->IsFocused() )
    {
        highLight = 6;
        g_UiMgr->SetScreenHighlight( m_pButtonSignOut->GetPosition() );
    }
    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }

    if( m_pPopUp )
    {
        if( m_PopUpResult != DLG_POPUP_IDLE )
        {
            if( m_PopUpResult == DLG_POPUP_YES )
            {
                m_State = DIALOG_STATE_BACK;
            }
            else
            {
                SetNavTextVisible( TRUE );
                m_pPopUp = NULL;
            }
        }
    }
}
