//=========================================================================
//
//  dlg_PauseMP.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"

#include "dlg_PauseMP.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================

ui_manager::control_tem PauseMPControls[] = 
{
    // Frames.
    { IDC_PAUSE_MP_QUIT,        "IDS_PAUSE_MENU_QUIT",      "button",   60,  60, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MP_SCORE,       "IDS_PAUSE_MENU_SCORE",     "button",   60, 100, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MP_OPTIONS,     "IDS_PAUSE_MENU_OPTIONS",   "button",   60, 140, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MP_SETTINGS,    "IDS_PAUSE_MENU_SETTINGS",  "button",   60, 180, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
};


ui_manager::dialog_tem PauseMPDialog =
{
    "IDS_PAUSE_MENU",
    1, 9,
    sizeof(PauseMPControls)/sizeof(ui_manager::control_tem),
    &PauseMPControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_pause_mp_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "pause multiplayer", &PauseMPDialog, &dlg_pause_mp_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_pause_mp_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_pause_mp* pDialog = new dlg_pause_mp;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_pause_mp
//=========================================================================

dlg_pause_mp::dlg_pause_mp( void )
{
}

//=========================================================================

dlg_pause_mp::~dlg_pause_mp( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_pause_mp::Create( s32                        UserID,
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
    m_pButtonQuit        = (ui_button*)  FindChildByID( IDC_PAUSE_MP_QUIT     );
    m_pButtonScore         = (ui_button*)  FindChildByID( IDC_PAUSE_MP_SCORE    );
    m_pButtonOptions    = (ui_button*)  FindChildByID( IDC_PAUSE_MP_OPTIONS  );
    m_pButtonSettings   = (ui_button*)  FindChildByID( IDC_PAUSE_MP_SETTINGS );

    s32 iControl = g_StateMgr.GetCurrentControl();
    if( (iControl == -1) || (GotoControl(iControl)==NULL) )
    {
        GotoControl( (ui_control*)m_pButtonQuit );
        m_CurrentControl = IDC_PAUSE_MP_QUIT;
    }
    else
    {
        m_CurrentControl = iControl;
    }

    m_CurrHL = 0;
    m_PopUp = NULL;

    // switch off the buttons to start
    m_pButtonQuit    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonScore   ->SetFlag(ui_win::WF_VISIBLE, FALSE);    
    m_pButtonOptions ->SetFlag(ui_win::WF_VISIBLE, FALSE);    
    m_pButtonSettings->SetFlag(ui_win::WF_VISIBLE, FALSE);    



    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_UNPAUSE" );   
    SetNavText( navText );

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

void dlg_pause_mp::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_pause_mp::Render( s32 ox, s32 oy )
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

void dlg_pause_mp::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonQuit )
        {


            if( m_PopUp == NULL )
            {
                // Open a dialog to confirm quitting the online game component
                irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

                // get message text
                xwstring Message( g_StringTableMgr( "ui", "IDS_QUIT_MSG" ));

                // set nav text
                xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
                navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
                SetNavTextVisible( FALSE );

                m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_QUIT_POPUP" ), TRUE, TRUE, FALSE, Message, navText, &m_PopUpResult );
            }
        }
        else if( pWin == (ui_win*)m_pButtonScore )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_MP_SCORE;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonOptions )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_MP_OPTIONS;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonSettings )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PAUSE_MP_SETTINGS;
            m_State = DIALOG_STATE_SELECT;
        }
    }
}

//=========================================================================

void dlg_pause_mp::OnCancel( ui_win* pWin )
{
    (void)pWin;
}

//=========================================================================

void dlg_pause_mp::OnUpdate ( ui_win* pWin, f32 DeltaTime )
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
            m_pButtonQuit    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonScore   ->SetFlag(ui_win::WF_VISIBLE, TRUE);    
            m_pButtonOptions ->SetFlag(ui_win::WF_VISIBLE, TRUE);    
            m_pButtonSettings->SetFlag(ui_win::WF_VISIBLE, TRUE);    
    

            s32 iControl = g_StateMgr.GetCurrentControl();
            if( (iControl == -1) || (GotoControl( iControl )==NULL) )
            {
                GotoControl( (ui_control*)m_pButtonQuit );
                g_UiMgr->SetScreenHighlight( m_pButtonQuit->GetPosition() );
                m_CurrentControl =  IDC_PAUSE_MP_QUIT;
            }
            else
            {
                ui_control* pControl = GotoControl( iControl );
                g_UiMgr->SetScreenHighlight( pControl->GetPosition() );
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

    // check popup dialog
    if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            if ( m_PopUpResult == DLG_POPUP_YES )
            {
                // quit game
                g_UiMgr->DisableScreenHighlight();
                m_CurrentControl = IDC_PAUSE_MP_QUIT;
                m_State = DIALOG_STATE_SELECT;
            }

            // clear popup 
            m_PopUp = NULL;

            // turn on nav text
            SetNavTextVisible( TRUE );
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    if( m_pButtonQuit->IsFocused() )
    {
        highLight = 0;
        g_UiMgr->SetScreenHighlight( m_pButtonQuit->GetPosition() );
    }
    else if( m_pButtonScore->IsFocused() )
    {
        highLight = 1;
        g_UiMgr->SetScreenHighlight( m_pButtonScore->GetPosition() );
    }
    else if( m_pButtonOptions->IsFocused() )
    {
        highLight = 2;
        g_UiMgr->SetScreenHighlight( m_pButtonOptions->GetPosition() );
    }
    else if( m_pButtonSettings->IsFocused() )
    {
        highLight = 3;
        g_UiMgr->SetScreenHighlight( m_pButtonSettings->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }
}

//=========================================================================
