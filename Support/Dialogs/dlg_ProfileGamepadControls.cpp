//==============================================================================
//
//  dlg_ProfileGamepadControls.cpp
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
#include "UI/ui_slider.hpp"
#include "UI/ui_text.hpp"

#include "dlg_ProfileGamepadControls.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum profile_gamepad_controls
{
    IDC_GAMEPAD_SENSITIVITY_X_TEXT,
    IDC_GAMEPAD_SENSITIVITY_Y_TEXT,
    IDC_GAMEPAD_INVERT_X_TEXT,
    IDC_GAMEPAD_INVERT_Y_TEXT,
    IDC_GAMEPAD_VIBRATION_TEXT,

    IDC_GAMEPAD_SENSITIVITY_X,
    IDC_GAMEPAD_SENSITIVITY_Y,
    IDC_GAMEPAD_INVERT_X,
    IDC_GAMEPAD_INVERT_Y,
    IDC_GAMEPAD_VIBRATION,
    IDC_GAMEPAD_BUTTON_ACCEPT,
};

//==============================================================================
//  DATA
//==============================================================================

ui_manager::control_tem ProfileGamepadControls[] =
{
    { IDC_GAMEPAD_SENSITIVITY_X_TEXT, "IDS_OPTIONS_SENSITIVITY_X",  "text",    40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_SENSITIVITY_Y_TEXT, "IDS_OPTIONS_SENSITIVITY_Y",  "text",    40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_INVERT_X_TEXT,       "IDS_OPTIONS_TOGGLE_INVERTX", "text",    40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_INVERT_Y_TEXT,       "IDS_OPTIONS_TOGGLE_INVERTY", "text",    40, 145, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_VIBRATION_TEXT,      "IDS_OPTIONS_VIBRATION",      "text",    40, 180, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_GAMEPAD_SENSITIVITY_X,      "IDS_NULL",                    "slider", 300,  40, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_SENSITIVITY_Y,      "IDS_NULL",                    "slider", 300,  75, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_INVERT_X,           "IDS_NULL",                    "check",  300, 110, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_INVERT_Y,           "IDS_NULL",                    "check",  300, 145, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_VIBRATION,          "IDS_NULL",                    "check",  300, 180, 120, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GAMEPAD_BUTTON_ACCEPT,      "IDS_PROFILE_OPTIONS_ACCEPT",  "button",  40, 285, 220, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem ProfileGamepadControlsDialog =
{
    "IDS_PROFILE_GAMEPAD_CONTROLS",
    1, 6,
    sizeof(ProfileGamepadControls) / sizeof(ui_manager::control_tem),
    &ProfileGamepadControls[0],
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_profile_gamepad_controls_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "profile gamepad controls", &ProfileGamepadControlsDialog, &dlg_profile_gamepad_controls_factory );
}

//==============================================================================

ui_win* dlg_profile_gamepad_controls_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_profile_gamepad_controls* pDialog = new dlg_profile_gamepad_controls;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return pDialog;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_profile_gamepad_controls::dlg_profile_gamepad_controls( void )
{
}

//==============================================================================

dlg_profile_gamepad_controls::~dlg_profile_gamepad_controls( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_profile_gamepad_controls::Create( s32                       UserID,
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

    m_pSensitivityX     = static_cast<ui_slider*>( FindChildByID( IDC_GAMEPAD_SENSITIVITY_X ) );
    m_pSensitivityY     = static_cast<ui_slider*>( FindChildByID( IDC_GAMEPAD_SENSITIVITY_Y ) );
    m_pInvertX          = static_cast<ui_check*>( FindChildByID( IDC_GAMEPAD_INVERT_X ) );
    m_pInvertY          = static_cast<ui_check*>( FindChildByID( IDC_GAMEPAD_INVERT_Y ) );
    m_pVibration        = static_cast<ui_check*>( FindChildByID( IDC_GAMEPAD_VIBRATION ) );
    m_pButtonAccept     = static_cast<ui_button*>( FindChildByID( IDC_GAMEPAD_BUTTON_ACCEPT ) );
    m_pSensitivityXText = static_cast<ui_text*>( FindChildByID( IDC_GAMEPAD_SENSITIVITY_X_TEXT ) );
    m_pSensitivityYText = static_cast<ui_text*>( FindChildByID( IDC_GAMEPAD_SENSITIVITY_Y_TEXT ) );
    m_pInvertXText      = static_cast<ui_text*>( FindChildByID( IDC_GAMEPAD_INVERT_X_TEXT ) );
    m_pInvertYText      = static_cast<ui_text*>( FindChildByID( IDC_GAMEPAD_INVERT_Y_TEXT ) );
    m_pVibrationText    = static_cast<ui_text*>( FindChildByID( IDC_GAMEPAD_VIBRATION_TEXT ) );

    m_pSensitivityX->SetRange( 0, GAMEPAD_SENSITIVITY_MAX );
    m_pSensitivityY->SetRange( 0, GAMEPAD_SENSITIVITY_MAX );

    player_profile& Profile = g_StateMgr.GetPendingProfile();
    m_OriginalProfile = Profile;
    m_OriginalProfile.Checksum();
    m_pSensitivityX->SetValue( Profile.GetSensitivity( profile_control_device::Gamepad, profile_control_axis::X ) );
    m_pSensitivityY->SetValue( Profile.GetSensitivity( profile_control_device::Gamepad, profile_control_axis::Y ) );
    m_pInvertX->SetChecked( Profile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::X ) );
    m_pInvertY->SetChecked( Profile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y ) );
    m_pVibration->SetChecked( Profile.GetVibration() );

    m_pSensitivityXText->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pSensitivityYText->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pInvertXText     ->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pInvertYText     ->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pVibrationText   ->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pButtonAccept    ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( static_cast<ui_control*>( m_pSensitivityX ) );
        m_CurrentControl = IDC_GAMEPAD_SENSITIVITY_X;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    m_pSensitivityX    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pSensitivityY    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pInvertX         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pInvertY         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVibration       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonAccept    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pSensitivityXText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pSensitivityYText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pInvertXText     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pInvertYText     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVibrationText   ->SetFlag( ui_win::WF_VISIBLE, FALSE );

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

void dlg_profile_gamepad_controls::Destroy( void )
{
    // Cancel outstanding save callbacks before destroying the dialog.
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_profile_gamepad_controls::Render( s32 ox, s32 oy )
{
    s32 const Offset = static_cast<s32>( g_UiMgr->GetAlphaTime() * 60.0f ) % 10;
    static s32 const Gap = 9;
    static s32 const Width = 4;
    irect Bounds;

    // Dim the background behind the dialog.
    if( m_bRenderBlackout )
    {
        Bounds = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect( Bounds,
                                    xcolor( 0, 0, 0, 180 ), xcolor( 0, 0, 0, 180 ),
                                    xcolor( 0, 0, 0, 180 ), xcolor( 0, 0, 0, 180 ), FALSE );
    }

    // Draw the translucent panel inside the side bars.
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

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//==============================================================================

void dlg_profile_gamepad_controls::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || (pWin != static_cast<ui_win*>( m_pButtonAccept )) )
    {
        return;
    }

    player_profile Profile = m_OriginalProfile;
    ApplyControls( Profile );
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

//==============================================================================

void dlg_profile_gamepad_controls::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State != DIALOG_STATE_ACTIVE )
    {
        return;
    }

    player_profile Profile = m_OriginalProfile;
    ApplyControls( Profile );
    if( !Profile.HasChanged() )
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    g_StateMgr.GetPendingProfile() = Profile;
    OpenSavePopup();
}

//==============================================================================

void dlg_profile_gamepad_controls::ApplyControls( player_profile& Profile )
{
    Profile.SetSensitivity( profile_control_device::Gamepad, profile_control_axis::X, m_pSensitivityX->GetValue() );
    Profile.SetSensitivity( profile_control_device::Gamepad, profile_control_axis::Y, m_pSensitivityY->GetValue() );
    Profile.SetAxisInverted( profile_control_device::Gamepad, profile_control_axis::X, m_pInvertX->IsChecked() );
    Profile.SetAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y, m_pInvertY->IsChecked() );
    Profile.SetVibration( m_pVibration->IsChecked() );
}

//==============================================================================

void dlg_profile_gamepad_controls::BeginSave( void )
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
        g_SaveDataMgr.CreateProfile( ProfileIndex, this, &dlg_profile_gamepad_controls::OnSaveProfileCB );
    }
    else
    {
        profile_info* pProfileInfo = &g_SaveDataMgr.GetProfileInfo( ProfileIndex );
        g_SaveDataMgr.SaveProfile( *pProfileInfo,
                                   ProfileIndex,
                                   this,
                                   &dlg_profile_gamepad_controls::OnSaveProfileCB );
    }
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_profile_gamepad_controls::OpenSavePopup( void )
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

void dlg_profile_gamepad_controls::RestoreProfile( void )
{
    g_StateMgr.GetPendingProfile() = m_OriginalProfile;
}

//==============================================================================

void dlg_profile_gamepad_controls::OnSaveProfileCB( void )
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

void dlg_profile_gamepad_controls::OnDelete( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        player_profile DefaultProfile;
        DefaultProfile.RestoreGamepadControlDefaults();
        m_pSensitivityX->SetValue( DefaultProfile.GetSensitivity( profile_control_device::Gamepad, profile_control_axis::X ) );
        m_pSensitivityY->SetValue( DefaultProfile.GetSensitivity( profile_control_device::Gamepad, profile_control_axis::Y ) );
        m_pInvertX->SetChecked( DefaultProfile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::X ) );
        m_pInvertY->SetChecked( DefaultProfile.IsAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y ) );
        m_pVibration->SetChecked( DefaultProfile.GetVibration() );
        g_AudioMgr.Play( "Select_Norm" );
    }
}

//==============================================================================

void dlg_profile_gamepad_controls::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    s32 Highlight = -1;

    // Consume the popup result once before resuming the dialog.
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

    // Show the controls only after the opening animation finishes.
    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        m_pSensitivityX    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pSensitivityY    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pInvertX         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pInvertY         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pVibration       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonAccept    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pSensitivityXText->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pSensitivityYText->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pInvertXText     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pInvertYText     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pVibrationText   ->SetFlag( ui_win::WF_VISIBLE, TRUE );

        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( static_cast<ui_control*>( m_pSensitivityX ) );
            m_CurrentControl = IDC_GAMEPAD_SENSITIVITY_X;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
        g_UiMgr->SetScreenHighlight( m_pSensitivityXText->GetPosition() );
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    ui_control* Controls[] =
    {
        m_pSensitivityX,
        m_pSensitivityY,
        m_pInvertX,
        m_pInvertY,
        m_pVibration,
        m_pButtonAccept,
    };
    ui_win* Labels[] =
    {
        m_pSensitivityXText,
        m_pSensitivityYText,
        m_pInvertXText,
        m_pInvertYText,
        m_pVibrationText,
        m_pButtonAccept,
    };

    for( s32 iControl = 0; iControl < 6; ++iControl )
    {
        if( Controls[iControl]->IsFocused() )
        {
            Highlight = iControl;
            Labels[iControl]->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
            g_UiMgr->SetScreenHighlight( Labels[iControl]->GetPosition() );
        }
        else
        {
            Labels[iControl]->SetLabelColor( xcolor( 126, 220, 60, 255 ) );
        }
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
