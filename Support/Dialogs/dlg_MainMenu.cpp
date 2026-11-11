//=========================================================================
//
//  dlg_main_menu.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"

#include "dlg_MainMenu.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "Configuration/GameConfig.hpp"


#include "../../Apps/GameApp/Config.hpp"    

//=========================================================================
//  Main Menu Dialog
//=========================================================================

ui_manager::control_tem MainMenuControls[] = 
{
    { IDC_MAIN_MENU_CAMPAIGN,           "IDS_MAIN_MENU_CAMPAIGN",   "button",   60, 40,  120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_MAIN_MENU_MULTI,              "IDS_MAIN_MENU_MULTI",      "button",   60, 80,  120, 40, 0, 1, 1, 1,  ui_win::WF_VISIBLE },
    { IDC_MAIN_MENU_ONLINE,             "IDS_MAIN_MENU_ONLINE",     "button",   60, 120, 120, 40, 0, 2, 1, 1,  ui_win::WF_VISIBLE },
    { IDC_MAIN_MENU_SETTINGS,           "IDS_MAIN_MENU_SETTINGS",   "button",   60, 160, 120, 40, 0, 3, 1, 1,  ui_win::WF_VISIBLE },
    { IDC_MAIN_MENU_PROFILES,           "IDS_MAIN_MENU_PROFILES",   "button",   60, 200, 120, 40, 0, 4, 1, 1,  ui_win::WF_VISIBLE },
    { IDC_MAIN_MENU_CREDITS,            "IDS_EXTRAS_ITEM_CREDITS",  "button",   60, 240, 120, 40, 0, 5, 1, 1,  ui_win::WF_VISIBLE },
#if defined(TARGET_DESKTOP)                                                                                           
    { IDC_MAIN_MENU_EXIT,               "IDS_MAIN_MENU_QUIT",       "button",   60, 280, 120, 40, 0, 6, 1, 1,  ui_win::WF_VISIBLE },
#endif                                                                                                           
}; 

ui_manager::dialog_tem MainMenuDialog =
{
    "IDS_MAIN_MENU",
    1, 9,
    sizeof(MainMenuControls)/sizeof(ui_manager::control_tem),
    &MainMenuControls[0],
    0
};

//=========================================================================
//  Defines
//=========================================================================

static const s32 MAIN_MENU_LOGO_WIDTH  = 128;
static const s32 MAIN_MENU_LOGO_HEIGHT =  32;
static const s32 MAIN_MENU_LOGO_TOP    =   2;

//=========================================================================
//  Registration function
//=========================================================================

void dlg_main_menu_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "main menu", &MainMenuDialog, &dlg_main_menu_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_main_menu_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_main_menu* pDialog = new dlg_main_menu;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_main_menu
//=========================================================================

dlg_main_menu::dlg_main_menu( void )
{
    m_LogoBitmapID = -1;
}

//=========================================================================

dlg_main_menu::~dlg_main_menu( void )
{
}

//=========================================================================

xbool dlg_main_menu::Create( s32                        UserID,
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

    // Place Area 51 logo here
    m_Label.Clear();
    m_LogoBitmapID = g_UiMgr->FindBitmap( "a51_logo" );
    ASSERT( m_LogoBitmapID >= 0 );

    m_pButtonCampaign       = (ui_button*)  FindChildByID( IDC_MAIN_MENU_CAMPAIGN           );
    m_pButtonMultiPlayer    = (ui_button*)  FindChildByID( IDC_MAIN_MENU_MULTI              );
    m_pButtonOnline         = (ui_button*)  FindChildByID( IDC_MAIN_MENU_ONLINE             );
    m_pButtonSettings       = (ui_button*)  FindChildByID( IDC_MAIN_MENU_SETTINGS           );
    m_pButtonProfiles       = (ui_button*)  FindChildByID( IDC_MAIN_MENU_PROFILES           );
    m_pButtonCredits        = (ui_button*)  FindChildByID( IDC_MAIN_MENU_CREDITS            );
#if defined(TARGET_DESKTOP)        
    m_pButtonExit           = (ui_button*)  FindChildByID( IDC_MAIN_MENU_EXIT               );
#endif    

    s32 iControl = g_StateMgr.GetCurrentControl();
    if( (iControl == -1) || (GotoControl(iControl)==NULL) )
    {
        GotoControl( (ui_control*)m_pButtonCampaign );
        m_CurrentControl =  IDC_MAIN_MENU_CAMPAIGN;
    }
    else
    {
        GotoControl( iControl );
        m_CurrentControl = iControl;
    }

    m_CurrHL = 0;
    m_bCheckKeySequence = FALSE;

    // switch off the buttons to start
    m_pButtonCampaign     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonMultiPlayer  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonOnline       ->SetFlag(ui_win::WF_VISIBLE, FALSE);    
    m_pButtonSettings     ->SetFlag(ui_win::WF_VISIBLE, FALSE);    
    m_pButtonProfiles     ->SetFlag(ui_win::WF_VISIBLE, FALSE);    
    m_pButtonCredits      ->SetFlag(ui_win::WF_VISIBLE, FALSE);
#if defined(TARGET_DESKTOP)    
    m_pButtonExit         ->SetFlag(ui_win::WF_VISIBLE, FALSE);
#endif    
    m_pButtonMultiPlayer  ->SetFlag(ui_win::WF_DISABLED, TRUE);


    // set up nav text 
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));

    SetNavText( xwstring(navText) );

    // set the number of players to 0
    g_PendingConfig.SetPlayerCount( 0 );

    // initialize the screen scaling
    InitScreenScaling( Position );

    // set the frame to be disabled (if coming from off screen)
    if (g_UiMgr->IsScreenOn() == FALSE)
        SetFlag( WF_DISABLED, TRUE );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;

    // nuke any temporary profiles 
    //for( s32 i=0; i<SM_PROFILE_COUNT; i++ )
    //{
    //    g_StateMgr.ClearSelectedProfile( i );
    //}

    // Return success code
    return Success;
}

//=========================================================================

void dlg_main_menu::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_main_menu::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;
    
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

    if( !g_UiMgr->IsScreenScaling() && (m_LogoBitmapID >= 0) )
    {
        const s32 LogoLeft = m_Position.l + ox + (m_Position.GetWidth() - MAIN_MENU_LOGO_WIDTH) / 2;
        const s32 LogoTop  = m_Position.t + oy + MAIN_MENU_LOGO_TOP;
        const irect LogoPosition( LogoLeft,
                                  LogoTop,
                                  LogoLeft + MAIN_MENU_LOGO_WIDTH,
                                  LogoTop  + MAIN_MENU_LOGO_HEIGHT );
        g_UiMgr->RenderBitmap( m_LogoBitmapID, LogoPosition, XCOLOR_WHITE );
    }

    // render the glow bar
    g_UiMgr->RenderGlowBar();

}

//=========================================================================

void dlg_main_menu::OnAccept( ui_win* pWin )
{
    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonCampaign )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl =  IDC_MAIN_MENU_CAMPAIGN;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonMultiPlayer )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_MAIN_MENU_MULTI;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonOnline )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_MAIN_MENU_ONLINE;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonSettings )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_MAIN_MENU_SETTINGS;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonProfiles )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_MAIN_MENU_PROFILES;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonCredits )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_MAIN_MENU_CREDITS;
            m_State = DIALOG_STATE_SELECT;
        }
#if defined(TARGET_DESKTOP)            
        else if( pWin == (ui_win*)m_pButtonExit )
        {
            g_AudioMgr.Play("Select_Norm");
            if( m_PopUp == NULL )
            {
                m_PopUpResult = DLG_POPUP_IDLE;
                irect r = g_UiMgr->GetUserBounds( m_UserID );
                m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog( m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|ui_win::WF_INPUTMODAL );

                xwstring navText( g_StringTableMgr( "ui", "IDS_NAV_YES" ) );
                navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
                SetNavTextVisible( FALSE );

                m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_APP_EXIT_VERIFY_TITLE" ),
                                    TRUE,
                                    TRUE,
                                    FALSE,
                                    g_StringTableMgr( "ui", "IDS_APP_EXIT_VERIFY_MSG" ),
                                    navText,
                                    &m_PopUpResult );
            }
        }
#endif
    }
}

//=========================================================================

void dlg_main_menu::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    s32 highLight = -1;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the buttons
            m_pButtonCampaign     ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonMultiPlayer  ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonOnline       ->SetFlag(ui_win::WF_VISIBLE, TRUE); 
            m_pButtonSettings     ->SetFlag(ui_win::WF_VISIBLE, TRUE);    
            m_pButtonProfiles     ->SetFlag(ui_win::WF_VISIBLE, TRUE);    
            m_pButtonCredits      ->SetFlag(ui_win::WF_VISIBLE, TRUE);
#if defined(TARGET_DESKTOP)            
            m_pButtonExit         ->SetFlag(ui_win::WF_VISIBLE, TRUE);
#endif            

            s32 iControl = g_StateMgr.GetCurrentControl();
            if( (iControl == -1) || (GotoControl(iControl)==NULL) )
            {
                GotoControl( (ui_control*)m_pButtonCampaign );
                g_UiMgr->SetScreenHighlight( m_pButtonCampaign->GetPosition() );
                m_CurrentControl =  IDC_MAIN_MENU_CAMPAIGN;
            }
            else
            {
                ui_control* pControl = GotoControl( iControl );
                ASSERT( pControl );
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

#if defined(TARGET_DESKTOP)
    // check exit popup result
    if( m_PopUp )
    {
        if( m_PopUpResult != DLG_POPUP_IDLE )
        {
            if( m_PopUpResult == DLG_POPUP_YES )
            {
                g_StateMgr.Reboot( REBOOT_QUIT );
            }
            else
            {
                // re-enable the dialog
                m_State = DIALOG_STATE_ACTIVE;
            }

            // clear popup 
            m_PopUp = NULL;

            // turn on nav text
            SetNavTextVisible( TRUE );
        }
    }
#endif    

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    if( m_pButtonCampaign->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonCampaign->GetPosition() );
        highLight = 0;
    }
    else if( m_pButtonMultiPlayer->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonMultiPlayer->GetPosition() );
        highLight = 1;
    }
    else if( m_pButtonOnline->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonOnline->GetPosition() );
        highLight = 2;
    }
    else if( m_pButtonSettings->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonSettings->GetPosition() );
        highLight = 3;
    }
    else if( m_pButtonProfiles->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonProfiles->GetPosition() );
        highLight = 4;
    }
    else if( m_pButtonCredits->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonCredits->GetPosition() );
        highLight = 5;
    }
#if defined(TARGET_DESKTOP)    
    else if( m_pButtonExit->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonExit->GetPosition() );
        highLight = 6;
    }
#endif
    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }

#ifndef CONFIG_RETAIL
#if defined(TARGET_DESKTOP)
    // check for enabling autoclient/server
    if( !m_bCheckKeySequence )
    {
        if( g_Input.GetFrameSnapshot().IsPressed( INPUT_PS2_BTN_START,   0 ) &&
            g_Input.GetFrameSnapshot().IsPressed( INPUT_PS2_BTN_SELECT,  0 ) )
        {
            // enable escape sequence checking
            m_bCheckKeySequence = TRUE;
        }
    }
    else
    {
        if( g_Input.GetFrameSnapshot().WasPressed( INPUT_PS2_BTN_L_UP,  0 ) )
        {
            g_Config.AutoServer = TRUE;
        }
        else if( g_Input.GetFrameSnapshot().WasPressed( INPUT_PS2_BTN_L_DOWN, 0 ) )
        {
            g_Config.AutoClient = TRUE;
        }
    }
#endif
#endif
}

//=========================================================================
