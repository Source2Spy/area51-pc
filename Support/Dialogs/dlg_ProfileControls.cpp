//==============================================================================
//
//  dlg_ProfileControls.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_check.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_text.hpp"

#include "dlg_ProfileControls.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum profile_controls_internal
{
    IDC_CONTROLS_CROUCH_TEXT = IDC_CONTROLS_BUTTON_ACCEPT + 1,
    IDC_CONTROLS_AIM_TEXT,
    IDC_CONTROLS_AUTO_SWITCH_TEXT,
};

//==============================================================================
//  DATA
//==============================================================================

ui_manager::control_tem ProfileControlsControls[] =
{
    { IDC_CONTROLS_MOUSE_MENU,             "IDS_PROFILE_MOUSE_CONTROLS",   "button",  40, 165, 220, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_GAMEPAD_MENU,           "IDS_PROFILE_GAMEPAD_CONTROLS", "button",  40, 200, 220, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_KEYBOARD_MENU,          "IDS_PROFILE_KEYBOARD_BINDINGS", "button",  40, 235, 220, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_TOGGLE_CROUCH,          "IDS_NULL",                     "check",  300,  40, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_TOGGLE_AIM,             "IDS_NULL",                     "check",  300,  75, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_TOGGLE_AUTO_SWITCH,     "IDS_NULL",                     "check",  300, 110, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_BUTTON_ACCEPT,          "IDS_PROFILE_OPTIONS_ACCEPT",   "button",  40, 285, 220, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_CROUCH_TEXT,            "IDS_OPTIONS_TOGGLE_CROUCH",    "text",    40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_AIM_TEXT,               "IDS_OPTIONS_TOGGLE_AIM",       "text",    40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_CONTROLS_AUTO_SWITCH_TEXT,       "IDS_OPTIONS_AUTO_SWITCH",      "text",    40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem ProfileControlsDialog =
{
    "IDS_PROFILE_CONTROLS",
    1, 7,
    sizeof(ProfileControlsControls) / sizeof(ui_manager::control_tem),
    &ProfileControlsControls[0],
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_profile_controls_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "profile controls", &ProfileControlsDialog, &dlg_profile_controls_factory );
}

//==============================================================================

ui_win* dlg_profile_controls_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_profile_controls* pDialog = new dlg_profile_controls;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return static_cast<ui_win*>( pDialog );
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_profile_controls::dlg_profile_controls( void )
{
}

//==============================================================================

dlg_profile_controls::~dlg_profile_controls( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_profile_controls::Create( s32                       UserID,
                                    ui_manager*               pManager,
                                    ui_manager::dialog_tem*   pDialogTem,
                                    const irect&              Position,
                                    ui_win*                   pParent,
                                    s32                       Flags,
                                    void*                     pUserData )
{
    (void)pUserData;
    ASSERT( pManager );

    xbool const Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_pMouseMenu        = static_cast<ui_button*>( FindChildByID( IDC_CONTROLS_MOUSE_MENU ) );
    m_pGamepadMenu      = static_cast<ui_button*>( FindChildByID( IDC_CONTROLS_GAMEPAD_MENU ) );
    m_pKeyboardMenu     = static_cast<ui_button*>( FindChildByID( IDC_CONTROLS_KEYBOARD_MENU ) );
    m_pToggleCrouch     = static_cast<ui_check*>( FindChildByID( IDC_CONTROLS_TOGGLE_CROUCH ) );
    m_pToggleAim        = static_cast<ui_check*>( FindChildByID( IDC_CONTROLS_TOGGLE_AIM ) );
    m_pToggleAutoSwitch = static_cast<ui_check*>( FindChildByID( IDC_CONTROLS_TOGGLE_AUTO_SWITCH ) );
    m_pButtonAccept     = static_cast<ui_button*>( FindChildByID( IDC_CONTROLS_BUTTON_ACCEPT ) );
    m_pCrouchText       = static_cast<ui_text*>( FindChildByID( IDC_CONTROLS_CROUCH_TEXT ) );
    m_pAimText          = static_cast<ui_text*>( FindChildByID( IDC_CONTROLS_AIM_TEXT ) );
    m_pAutoSwitchText   = static_cast<ui_text*>( FindChildByID( IDC_CONTROLS_AUTO_SWITCH_TEXT ) );

    player_profile& Profile = g_StateMgr.GetPendingProfile();
    m_OriginalProfile = Profile;
    m_OriginalProfile.Checksum();
    m_pToggleCrouch->SetChecked( Profile.GetCrouchOn() );
    m_pToggleAim->SetChecked( Profile.IsAimToggleEnabled() );
    m_pToggleAutoSwitch->SetChecked( Profile.GetWeaponAutoSwitch() );

    m_pMouseMenu       ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pGamepadMenu     ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pKeyboardMenu    ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pButtonAccept    ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pCrouchText      ->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pAimText         ->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pAutoSwitchText  ->SetLabelFlags( ui_font::h_left | ui_font::v_center );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( static_cast<ui_control*>( m_pToggleCrouch ) );
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    m_pMouseMenu       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pGamepadMenu     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pKeyboardMenu    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pToggleCrouch    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pToggleAim       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pToggleAutoSwitch->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonAccept    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pCrouchText      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAimText         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAutoSwitchText  ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_CurrHL = 0;
    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;

    xwstring NavText( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) );
    NavText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    NavText += g_StringTableMgr( "ui", "IDS_NAV_RESTORE_DEFAULTS" );
    SetNavText( NavText );

    InitScreenScaling( Position );
    m_bRenderBlackout = FALSE;
    m_State = DIALOG_STATE_ACTIVE;
    return Success;
}

//==============================================================================

void dlg_profile_controls::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_profile_controls::Render( s32 ox, s32 oy )
{
    s32 const Offset = static_cast<s32>( g_UiMgr->GetAlphaTime() * 60.0f ) % 10;
    static s32 const Gap = 9;
    static s32 const Width = 4;
    irect Bounds;

    if( m_bRenderBlackout )
    {
        Bounds = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect( Bounds,
                                    xcolor( 0, 0, 0, 180 ), xcolor( 0, 0, 0, 180 ),
                                    xcolor( 0, 0, 0, 180 ), xcolor( 0, 0, 0, 180 ), FALSE );
    }

    Bounds.l = m_CurrPos.l + 22;
    Bounds.t = m_CurrPos.t;
    Bounds.r = m_CurrPos.r - 23;
    Bounds.b = m_CurrPos.b;
    g_UiMgr->RenderGouraudRect( Bounds,
                                xcolor( 56, 115, 58, 64 ), xcolor( 56, 115, 58, 64 ),
                                xcolor( 56, 115, 58, 64 ), xcolor( 56, 115, 58, 64 ), FALSE );

    for( s32 Y = Bounds.t + Offset; Y < Bounds.b; Y += Gap )
    {
        irect const Bar( Bounds.l, Y, Bounds.r, MIN( Y + Width, Bounds.b ) );
        g_UiMgr->RenderGouraudRect( Bar,
                                    xcolor( 56, 115, 58, 30 ), xcolor( 56, 115, 58, 30 ),
                                    xcolor( 56, 115, 58, 30 ), xcolor( 56, 115, 58, 30 ), FALSE );
    }

    ui_dialog::Render( ox, oy );
    g_UiMgr->RenderGlowBar();
}

//==============================================================================

void dlg_profile_controls::ApplyCommonControls( player_profile& Profile ) const
{
    Profile.SetCrouchOn( m_pToggleCrouch->IsChecked() );
    Profile.SetAimToggleEnabled( m_pToggleAim->IsChecked() );
    Profile.SetWeaponAutoSwitch( m_pToggleAutoSwitch->IsChecked() );
}

//==============================================================================

void dlg_profile_controls::OnAccept( ui_win* pWin )
{
    if( m_State != DIALOG_STATE_ACTIVE )
    {
        return;
    }

    if( (pWin == static_cast<ui_win*>( m_pMouseMenu )) ||
        (pWin == static_cast<ui_win*>( m_pGamepadMenu )) ||
        (pWin == static_cast<ui_win*>( m_pKeyboardMenu )) )
    {
        ApplyCommonControls( g_StateMgr.GetPendingProfile() );
        g_AudioMgr.Play( "Select_Norm" );
        GotoControl( static_cast<ui_control*>( pWin ) );
        m_State = DIALOG_STATE_SELECT;
    }
    else if( pWin == static_cast<ui_win*>( m_pButtonAccept ) )
    {
        player_profile Profile = m_OriginalProfile;
        ApplyCommonControls( Profile );
        if( !Profile.HasChanged() )
        {
            g_AudioMgr.Play( "Select_Norm" );
            m_State = DIALOG_STATE_BACK;
            return;
        }

        g_StateMgr.GetPendingProfile() = Profile;
        g_AudioMgr.Play( "Select_Norm" );
        OpenSavePopup();
    }
}

//==============================================================================

void dlg_profile_controls::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        player_profile Profile = m_OriginalProfile;
        ApplyCommonControls( Profile );
        if( !Profile.HasChanged() )
        {
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
            return;
        }

        g_StateMgr.GetPendingProfile() = Profile;
        OpenSavePopup();
    }
}

//==============================================================================

void dlg_profile_controls::BeginSave( void )
{
    s32 const ProfileIndex = g_StateMgr.GetPendingProfileIndex();
    if( (ProfileIndex < 0) || (ProfileIndex >= SM_PROFILE_COUNT) )
    {
        RestoreProfile();
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    if( g_StateMgr.GetProfileNotSaved( ProfileIndex ) )
    {
        g_StateMgr.GetPendingProfile().SetHash();
        g_StateMgr.SetSelectedProfile( ProfileIndex, g_StateMgr.GetPendingProfile().GetHash() );
        g_SaveDataMgr.CreateProfile( ProfileIndex, this, &dlg_profile_controls::OnSaveProfileCB );
    }
    else
    {
        profile_info* pProfileInfo = &g_SaveDataMgr.GetProfileInfo( ProfileIndex );
        g_SaveDataMgr.SaveProfile( *pProfileInfo,
                                   ProfileIndex,
                                   this,
                                   &dlg_profile_controls::OnSaveProfileCB );
    }
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_profile_controls::OpenSavePopup( void )
{
    irect const Bounds = g_UiMgr->GetUserBounds( g_UiUserID );
    m_PopUp = static_cast<dlg_popup*>( g_UiMgr->OpenDialog( m_UserID,
                                                            "popup",
                                                            Bounds,
                                                            NULL,
                                                            ui_win::WF_VISIBLE | ui_win::WF_BORDER |
                                                            ui_win::WF_DLG_CENTER | ui_win::WF_INPUTMODAL ) );
    xwstring PopupNavText( g_StringTableMgr( "ui", "IDS_NAV_YES" ) );
    PopupNavText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
    SetNavTextVisible( FALSE );
    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_PROFILE_EDIT" ),
                        TRUE,
                        TRUE,
                        FALSE,
                        g_StringTableMgr( "ui", "IDS_PROFILE_EDIT_MSG" ),
                        PopupNavText,
                        &m_PopUpResult );
    m_State = DIALOG_STATE_POPUP;
}

//==============================================================================

void dlg_profile_controls::RestoreProfile( void )
{
    g_StateMgr.GetPendingProfile() = m_OriginalProfile;
}

//==============================================================================

void dlg_profile_controls::OnSaveProfileCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        s32 const ProfileIndex = g_StateMgr.GetPendingProfileIndex();
        g_StateMgr.SetProfileNotSaved( ProfileIndex, FALSE );
        g_StateMgr.ActivatePendingProfile();
        g_StateMgr.InitPendingProfile( ProfileIndex );
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;
    }
    else
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_SAVE_DATA_ERROR;
    }
}

//==============================================================================

void dlg_profile_controls::OnDelete( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        player_profile DefaultProfile;
        DefaultProfile.RestoreCommonControlDefaults();
        m_pToggleCrouch->SetChecked( DefaultProfile.GetCrouchOn() );
        m_pToggleAim->SetChecked( DefaultProfile.IsAimToggleEnabled() );
        m_pToggleAutoSwitch->SetChecked( DefaultProfile.GetWeaponAutoSwitch() );
        g_AudioMgr.Play( "Select_Norm" );
    }
}

//==============================================================================

void dlg_profile_controls::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    s32 Highlight = -1;

    if( m_PopUp && (m_PopUpResult != DLG_POPUP_IDLE) )
    {
        s32 const Result = m_PopUpResult;
        m_PopUp = NULL;
        m_PopUpResult = DLG_POPUP_IDLE;
        SetNavTextVisible( TRUE );

        if( Result == DLG_POPUP_YES )
        {
            BeginSave();
        }
        else
        {
            RestoreProfile();
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );
    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        m_pMouseMenu       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pGamepadMenu     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pKeyboardMenu    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pToggleCrouch    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pToggleAim       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pToggleAutoSwitch->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonAccept    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pCrouchText      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAimText         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAutoSwitchText  ->SetFlag( ui_win::WF_VISIBLE, TRUE );

        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( static_cast<ui_control*>( m_pToggleCrouch ) );
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
        g_UiMgr->SetScreenHighlight( m_pCrouchText->GetPosition() );
    }

    if( m_pMouseMenu->IsFocused() )
    {
        Highlight = 3;
        m_pMouseMenu->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        g_UiMgr->SetScreenHighlight( m_pMouseMenu->GetPosition() );
    }
    else
    {
        m_pMouseMenu->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
    }

    if( m_pGamepadMenu->IsFocused() )
    {
        Highlight = 4;
        m_pGamepadMenu->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        g_UiMgr->SetScreenHighlight( m_pGamepadMenu->GetPosition() );
    }
    else
    {
        m_pGamepadMenu->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
    }

    if( m_pKeyboardMenu->IsFocused() )
    {
        Highlight = 5;
        m_pKeyboardMenu->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        g_UiMgr->SetScreenHighlight( m_pKeyboardMenu->GetPosition() );
    }
    else
    {
        m_pKeyboardMenu->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
    }

    if( m_pToggleCrouch->IsFocused() )
    {
        Highlight = 0;
        m_pCrouchText->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        g_UiMgr->SetScreenHighlight( m_pCrouchText->GetPosition() );
    }
    else
    {
        m_pCrouchText->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
    }

    if( m_pToggleAutoSwitch->IsFocused() )
    {
        Highlight = 2;
        m_pAutoSwitchText->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        g_UiMgr->SetScreenHighlight( m_pAutoSwitchText->GetPosition() );
    }
    else
    {
        m_pAutoSwitchText->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
    }

    if( m_pToggleAim->IsFocused() )
    {
        Highlight = 1;
        m_pAimText->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        g_UiMgr->SetScreenHighlight( m_pAimText->GetPosition() );
    }
    else
    {
        m_pAimText->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
    }

    if( m_pButtonAccept->IsFocused() )
    {
        Highlight = 6;
        m_pButtonAccept->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        g_UiMgr->SetScreenHighlight( m_pButtonAccept->GetPosition() );
    }
    else
    {
        m_pButtonAccept->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
    }

    if( Highlight != m_CurrHL )
    {
        if( Highlight != -1 )
        {
            g_AudioMgr.Play( "Cusor_Norm" );
        }
        m_CurrHL = Highlight;
    }
}

//==============================================================================
