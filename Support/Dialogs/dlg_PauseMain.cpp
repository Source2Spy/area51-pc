//=========================================================================
//
//  dlg_PauseMain.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"

#include "dlg_PauseMain.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "Configuration/GameConfig.hpp"

#include "../../Apps/GameApp/Config.hpp"    

// display build info
#define DISPLAY_BUILD_INFO

#if defined( DISPLAY_DEBUG_INFO )
#include "Objects/Player/Player.hpp"
#include "StateMgr/MapList.hpp"
#endif

extern          s32         g_Changelist;
extern  const   char*       g_pBuildDate;

//=========================================================================
//  Main Menu Dialog
//=========================================================================

ui_manager::control_tem PauseMenuControls[] = 
{
#if CONFIG_IS_DEMO
    { IDC_PAUSE_MENU_RESUME,    "IDS_PAUSE_MENU_RESUME",    "button",   60,  70, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_QUIT,      "IDS_PAUSE_MENU_QUIT",      "button",   60, 130, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_INVERTY,   "IDS_PAUSE_MENU_INVERTY",   "button",   60, 190, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_VIBRATION, "IDS_RUMBLE",               "button",   60, 250, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
#else
    // Frames.
    { IDC_PAUSE_MENU_RESUME,    "IDS_PAUSE_MENU_RESUME",    "button",   60,  60, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_QUIT,      "IDS_PAUSE_MENU_QUIT",      "button",   60, 100, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_OPTIONS,   "IDS_PAUSE_MENU_OPTIONS",   "button",   60, 140, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_SETTINGS,  "IDS_PAUSE_MENU_SETTINGS",  "button",   60, 180, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
#endif

#if defined(DISPLAY_DEBUG_INFO)
    { IDC_PAUSE_MENU_INVERTY,   "IDS_NULL",                 "text",     60,  30, 120, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_ZONE,      "IDS_NULL",                 "text",     60, 260, 120, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_LEVEL,     "IDS_NULL",                 "text",     60, 280, 120, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
#endif
#if defined(DISPLAY_BUILD_INFO)
    { IDC_PAUSE_MENU_CHANGE,    "IDS_NULL",                 "text",     60, 300, 120, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PAUSE_MENU_BUILDDATE, "IDS_NULL",                 "text",     60, 320, 120, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
#endif

};


ui_manager::dialog_tem PauseMenuDialog =
{
    "IDS_PAUSE_MENU",
    1, 11,
    sizeof(PauseMenuControls)/sizeof(ui_manager::control_tem),
    &PauseMenuControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_pause_main_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "pause main", &PauseMenuDialog, &dlg_pause_main_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_pause_main_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_pause_main* pDialog = new dlg_pause_main;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_pause_main
//=========================================================================

dlg_pause_main::dlg_pause_main( void )
{
}

//=========================================================================

dlg_pause_main::~dlg_pause_main( void )
{
    Destroy();
}

//=========================================================================
#if defined( DISPLAY_DEBUG_INFO )
static xwstring zoneText;
#endif 

xbool dlg_pause_main::Create( s32                        UserID,
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
    m_pButtonResume     = (ui_button*)  FindChildByID( IDC_PAUSE_MENU_RESUME    );
    m_pButtonQuit       = (ui_button*)  FindChildByID( IDC_PAUSE_MENU_QUIT      );

#if defined( DISPLAY_DEBUG_INFO )
    m_pInvertYText      = (ui_text*)    FindChildByID( IDC_PAUSE_MENU_INVERTY   );
    m_pZoneText         = (ui_text*)    FindChildByID( IDC_PAUSE_MENU_ZONE      );
    m_pLevelText        = (ui_text*)    FindChildByID( IDC_PAUSE_MENU_LEVEL     );
#endif
#if defined(DISPLAY_BUILD_INFO)
    m_pChangeText       = (ui_text*)    FindChildByID( IDC_PAUSE_MENU_CHANGE    );
    m_pBuildText        = (ui_text*)    FindChildByID( IDC_PAUSE_MENU_BUILDDATE );
#endif

#if CONFIG_IS_DEMO
    m_pButtonInvert     = (ui_button*)  FindChildByID( IDC_PAUSE_MENU_INVERTY   );
    m_pButtonVibration  = (ui_button*)  FindChildByID( IDC_PAUSE_MENU_VIBRATION );
    ASSERT( m_pButtonInvert );
    ASSERT( m_pButtonVibration );
    m_pButtonInvert   ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonVibration->SetFlag( ui_win::WF_VISIBLE, FALSE );
#else
    m_pButtonOptions    = (ui_button*)  FindChildByID( IDC_PAUSE_MENU_OPTIONS   );
    m_pButtonSettings   = (ui_button*)  FindChildByID( IDC_PAUSE_MENU_SETTINGS  );
    m_pButtonOptions ->SetFlag(ui_win::WF_VISIBLE, FALSE);    
    m_pButtonSettings->SetFlag(ui_win::WF_VISIBLE, FALSE);    
#endif



    //s32 iControl = g_StateMgr.GetCurrentControl();
    //if( (iControl == -1) || (GotoControl(iControl)==NULL) )
    //{
        GotoControl( (ui_control*)m_pButtonResume );
        m_CurrentControl = IDC_PAUSE_MENU_RESUME;
    //}
    //else
    //{
    //    m_CurrentControl = iControl;
    //}

    m_CurrHL = 0;
    m_PopUp = NULL;

    // switch off the buttons to start
    m_pButtonResume  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonQuit    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
#if defined( DISPLAY_DEBUG_INFO )
    m_pInvertYText   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pZoneText      ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLevelText     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
#endif
#if defined(DISPLAY_BUILD_INFO)
    m_pChangeText    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pBuildText     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
#endif

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_UNPAUSE" );   
    SetNavText( navText );

    // set up debug info text
#if defined( DISPLAY_DEBUG_INFO )
    // invertY toggle
    player_profile& Profile = g_StateMgr.GetActiveProfile( 0 );
    if( Profile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y ) )
    {
        m_pInvertYText->SetLabel( xwstring( "Y-Axis INVERTED" ) );
    }
    else
    {
        m_pInvertYText->SetLabel( xwstring( "Y-Axis NORMAL" ) );
    }
    m_pInvertYText->SetLabelFlags( ui_font::h_center|ui_font::v_top );
    m_pInvertYText->UseSmallText(TRUE);

    // zone
    player* pPlayer = SMP_UTIL_GetActivePlayer();

    xstring zone = "";
    if( pPlayer )
    {
        s32 playerZone = static_cast<s32>( pPlayer->GetPlayerObjectZone() );
        zone.Format("Zone: %d", playerZone);
    }
    zoneText = zone;
    m_pZoneText->SetLabel( zoneText );
    m_pZoneText->SetLabelFlags( ui_font::h_center|ui_font::v_top );
    m_pZoneText->UseSmallText(TRUE);

    // level name
    s32 levelID = g_ActiveConfig.GetLevelID();
    xwstring levelText;
    if( DecodeFontText( g_MapList.GetDisplayName( levelID ), levelText ) )
    {
        m_pLevelText->SetLabel( levelText );
    }
    m_pLevelText->SetLabelFlags( ui_font::h_center|ui_font::v_top );
    m_pLevelText->UseSmallText(TRUE);
#endif

#if defined(DISPLAY_BUILD_INFO)
    // change list 
    static xwstring changeText  = (const char *)xfs( "build: %d", g_Changelist );
    m_pChangeText->SetLabel( changeText );
    m_pChangeText->SetLabelFlags( ui_font::h_center|ui_font::v_top );
    m_pChangeText->UseSmallText(TRUE);

    // build date
    static xwstring buildText  = (const char *)xfs( "%s", g_pBuildDate );
    m_pBuildText->SetLabel( buildText );
    m_pBuildText->SetLabelFlags( ui_font::h_center|ui_font::v_top );
    m_pBuildText->UseSmallText(TRUE);
#endif

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

void dlg_pause_main::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_pause_main::Render( s32 ox, s32 oy )
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

void dlg_pause_main::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonResume )
        {
            g_AudioMgr.Play("Select_Norm");
            g_UiMgr->DisableScreenHighlight();
            m_CurrentControl = IDC_PAUSE_MENU_RESUME;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonQuit )
        {
          

            if( m_PopUp == NULL )
            {
                // Open a dialog to confirm quitting the online game component
                irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

                // get message text
                xwstring Message( g_StringTableMgr( "ui", "IDS_QUIT_DESTRUCTIVE_MSG" )); 

                // set nav text
                xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
                navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
                SetNavTextVisible( FALSE );

                m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_QUIT_POPUP" ), TRUE, TRUE, FALSE, Message, navText, &m_PopUpResult );
            }
        }
#if CONFIG_IS_DEMO
        else if( pWin == m_pButtonInvert )
        {
            // toggle invert Y
            player_profile& Profile = g_StateMgr.GetActiveProfile(0);
            xbool const IsInverted = Profile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y );
            Profile.SetAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y, !IsInverted );
        }
        else if( pWin == m_pButtonVibration )
        {
            // toggle invert Y
            player_profile& Profile = g_StateMgr.GetActiveProfile(0);
            Profile.SetVibration( !Profile.GetVibration() );
            g_Input.EnableFeedback( Profile.GetVibration() );
        }
#else
        else if( pWin == (ui_win*)m_pButtonOptions )
        {
            g_StateMgr.InitPendingProfile( 0 );
            m_CurrentControl = IDC_PAUSE_MENU_OPTIONS;
            m_State = DIALOG_STATE_SELECT;
            g_AudioMgr.Play("Select_Norm");
        }
        else if( pWin ==(ui_win*)m_pButtonSettings )
        {
            m_CurrentControl = IDC_PAUSE_MENU_SETTINGS;
            m_State = DIALOG_STATE_SELECT;
            g_AudioMgr.Play("Select_Norm");
        }
#endif
    }
}

//=========================================================================

void dlg_pause_main::OnCancel( ui_win* pWin )
{
    (void)pWin;
}

//=========================================================================

void dlg_pause_main::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    s32 highLight = -1;

    //
    // Figure out if we have invitations to be friends or join games
    //

#if CONFIG_IS_DEMO
    player_profile& Profile = g_StateMgr.GetActiveProfile( 0 );

    xfs Vibration( "Vibration %s", Profile.GetVibration() ? "Enabled" : "Disabled" );
    xfs InvertY  ( "Invert Y %s",
                   Profile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y )
                       ? "Enabled"
                       : "Disabled" );

    m_pButtonVibration->SetLabel( (const xwchar*)xwstring( (const char*)Vibration ) );
    m_pButtonInvert->SetLabel( (const xwchar*)xwstring( (const char*)InvertY ) );

#endif

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the buttons
            m_pButtonResume     ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonQuit       ->SetFlag(ui_win::WF_VISIBLE, TRUE);
#if CONFIG_IS_DEMO
            m_pButtonVibration  ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pButtonInvert     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
#else
            m_pButtonOptions    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonSettings   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
#endif

#if defined( DISPLAY_DEBUG_INFO )
            m_pInvertYText      ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pZoneText         ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pLevelText        ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            
            // update zone information (may have chnaged).
            player* pPlayer = SMP_UTIL_GetActivePlayer();
            xstring zone = "";
            if( pPlayer )
            {
                s32 playerZone = static_cast<s32>( pPlayer->GetPlayerObjectZone() );
                zone.Format("Zone: %d", playerZone);
            }
            zoneText = zone;
            m_pZoneText->SetLabel( zoneText );
#endif
#if defined(DISPLAY_BUILD_INFO)
            m_pChangeText    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pBuildText     ->SetFlag(ui_win::WF_VISIBLE, TRUE);
#endif
    
            //s32 iControl = g_StateMgr.GetCurrentControl();
            //if( (iControl == -1) || (GotoControl(iControl)==NULL) )
            //{
                GotoControl( (ui_control*)m_pButtonResume );
                g_UiMgr->SetScreenHighlight( m_pButtonResume->GetPosition() );
                m_CurrentControl =  IDC_PAUSE_MENU_RESUME;
            //}
            //else
            //{
            //    ui_control* pControl = GotoControl( iControl );
            //    g_UiMgr->SetScreenHighlight(pControl->GetPosition() );
            //    m_CurrentControl = iControl;
            //}

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
                m_CurrentControl = IDC_PAUSE_MENU_QUIT;
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

    if( m_pButtonResume->IsFocused() )
    {
        highLight = 0;
        g_UiMgr->SetScreenHighlight( m_pButtonResume->GetPosition() );
    }
    else if( m_pButtonQuit->IsFocused() )
    {
        highLight = 1;
        g_UiMgr->SetScreenHighlight( m_pButtonQuit->GetPosition() );
    }
#if CONFIG_IS_DEMO
    else if( m_pButtonInvert->IsFocused() )
    {
        highLight = 2;
        g_UiMgr->SetScreenHighlight( m_pButtonInvert->GetPosition() );

    }
    else if( m_pButtonVibration->IsFocused() )
    {
        highLight = 3;
        g_UiMgr->SetScreenHighlight( m_pButtonVibration->GetPosition() );
    }
#else
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
#endif

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }
}

//=========================================================================
