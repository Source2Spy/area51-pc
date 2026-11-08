//==============================================================================
//
//  dlg_Settings.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_manager.hpp"

#include "dlg_Settings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum
{
    POPUP_CONFIRM_SETTINGS,
};

//------------------------------------------------------------------------------

ui_manager::control_tem SettingsControls[] =
{
    { IDC_SETTINGS_AUDIO,    "IDS_OPTIONS_AUDIO_SETTINGS",    "button", 40,  40, 240, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SETTINGS_HEADSET,  "IDS_OPTIONS_HEADSET_TEST",      "button", 40,  75, 240, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SETTINGS_GRAPHICS, "IDS_OPTIONS_GRAPHICS_SETTINGS", "button", 40, 110, 240, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SETTINGS_DISPLAY,  "IDS_OPTIONS_DISPLAY_SETTINGS",  "button", 40, 145, 240, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SETTINGS_UI,       "IDS_OPTIONS_UI_SETTINGS",        "button", 40, 180, 240, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SETTINGS_LANGUAGE, "IDS_OPTIONS_LANGUAGE_SETTINGS", "button", 40, 215, 240, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SETTINGS_ACCEPT,   "IDS_PROFILE_OPTIONS_ACCEPT",    "button", 40, 285, 240, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },
};

//------------------------------------------------------------------------------

ui_manager::dialog_tem SettingsDialog =
{
    "IDS_SETTINGS",
    1, 7,
    sizeof(SettingsControls) / sizeof(ui_manager::control_tem),
    &SettingsControls[0],
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "settings", &SettingsDialog, &dlg_settings_factory );
}

//==============================================================================

ui_win* dlg_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_settings* pDialog = new dlg_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return (ui_win*)pDialog;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_settings::dlg_settings( void )
{
}

//==============================================================================

dlg_settings::~dlg_settings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_settings::Create( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    (void)pUserData;
    ASSERT( pManager );

    xbool const Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_pAudio    = (ui_button*)FindChildByID( IDC_SETTINGS_AUDIO );
    m_pHeadset  = (ui_button*)FindChildByID( IDC_SETTINGS_HEADSET );
    m_pGraphics = (ui_button*)FindChildByID( IDC_SETTINGS_GRAPHICS );
    m_pDisplay  = (ui_button*)FindChildByID( IDC_SETTINGS_DISPLAY );
    m_pUI         = (ui_button*)FindChildByID( IDC_SETTINGS_UI );
    m_pLanguage = (ui_button*)FindChildByID( IDC_SETTINGS_LANGUAGE );
    m_pAccept   = (ui_button*)FindChildByID( IDC_SETTINGS_ACCEPT );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( (ui_control*)m_pAudio );
        m_CurrentControl = IDC_SETTINGS_AUDIO;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    ui_button* pButtons[] = { m_pAudio, m_pHeadset, m_pGraphics, m_pDisplay, m_pUI, m_pLanguage, m_pAccept };
    for( s32 i = 0; i < 7; i++ )
    {
        pButtons[i]->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
        pButtons[i]->SetFlag( ui_win::WF_VISIBLE, FALSE );
    }

    xwstring NavText( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) );
    NavText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    SetNavText( NavText );

    m_CurrHL = 0;
    m_bRenderBlackout = FALSE;
    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;
    m_PopUpType = POPUP_CONFIRM_SETTINGS;
    m_OriginalSettings = g_StateMgr.GetPendingSettings();

    InitScreenScaling( Position );
    m_State = DIALOG_STATE_ACTIVE;
    return Success;
}

//==============================================================================

void dlg_settings::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_settings::Render( s32 ox, s32 oy )
{
    s32 const Offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    irect Bounds;

    if( m_bRenderBlackout )
    {
        Bounds = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect( Bounds, xcolor(0,0,0,180), xcolor(0,0,0,180), xcolor(0,0,0,180), xcolor(0,0,0,180), FALSE );
    }

    Bounds.Set( m_CurrPos.l + 22, m_CurrPos.t, m_CurrPos.r - 23, m_CurrPos.b );
    g_UiMgr->RenderGouraudRect( Bounds, xcolor(56,115,58,64), xcolor(56,115,58,64), xcolor(56,115,58,64), xcolor(56,115,58,64), FALSE );
    for( s32 y = Bounds.t + Offset; y < Bounds.b; y += 9 )
    {
        irect Bar( Bounds.l, y, Bounds.r, MIN( y + 4, Bounds.b ) );
        g_UiMgr->RenderGouraudRect( Bar, xcolor(56,115,58,30), xcolor(56,115,58,30), xcolor(56,115,58,30), xcolor(56,115,58,30), FALSE );
    }

    ui_dialog::Render( ox, oy );
    g_UiMgr->RenderGlowBar();
}

//==============================================================================

void dlg_settings::OnAccept( ui_win* pWin )
{
    if( m_State != DIALOG_STATE_ACTIVE )
    {
        return;
    }

    ui_button* pMenus[] = { m_pAudio, m_pHeadset, m_pGraphics, m_pDisplay, m_pUI, m_pLanguage };
    s32 const Controls[] = { IDC_SETTINGS_AUDIO, IDC_SETTINGS_HEADSET, IDC_SETTINGS_GRAPHICS, IDC_SETTINGS_DISPLAY, IDC_SETTINGS_UI, IDC_SETTINGS_LANGUAGE };
    for( s32 i = 0; i < 6; i++ )
    {
        if( pWin == (ui_win*)pMenus[i] )
        {
            g_AudioMgr.Play( "Select_Norm" );
            m_CurrentControl = Controls[i];
            m_State = DIALOG_STATE_SELECT;
            return;
        }
    }

    if( pWin != (ui_win*)m_pAccept )
    {
        return;
    }

    g_AudioMgr.Play( "Select_Norm" );
    if( !g_StateMgr.GetPendingSettings().HasChanged() )
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    if( GameMgr.GameInProgress() && g_NetworkMgr.IsOnline() )
    {
        g_StateMgr.ActivatePendingSettings();
        g_StateMgr.GetActiveSettings().MarkDirty();
        m_State = DIALOG_STATE_BACK;
        return;
    }

    OpenSavePopup();
}

//==============================================================================

void dlg_settings::OnCancel( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( g_StateMgr.GetPendingSettings().HasChanged() )
        {
            OpenSavePopup();
            return;
        }

        global_settings& ActiveSettings = g_StateMgr.GetActiveSettings();
        g_StateMgr.GetPendingSettings() = m_OriginalSettings;
        ActiveSettings.CommitAudio();
        ActiveSettings.CommitLocalization();
        ActiveSettings.CommitGraphics();
        ActiveSettings.UpdateHeadsetMode( ActiveSettings.GetHeadsetMode() );
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
    }
}

//==============================================================================

void dlg_settings::OpenSavePopup( void )
{
    irect const Bounds = g_UiMgr->GetUserBounds( g_UiUserID );
    m_PopUp = static_cast<dlg_popup*>( g_UiMgr->OpenDialog( m_UserID,
                                                            "popup",
                                                            Bounds,
                                                            NULL,
                                                            ui_win::WF_VISIBLE | ui_win::WF_BORDER |
                                                            ui_win::WF_DLG_CENTER | ui_win::WF_INPUTMODAL ) );
    m_PopUpType = POPUP_CONFIRM_SETTINGS;

    xwstring PopupNavText( g_StringTableMgr( "ui", "IDS_NAV_YES" ) );
    PopupNavText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
    SetNavTextVisible( FALSE );
    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_SETTINGS_EDIT" ),
                        TRUE,
                        TRUE,
                        FALSE,
                        g_StringTableMgr( "ui", "IDS_SETTINGS_CHANGED_MSG" ),
                        PopupNavText,
                        &m_PopUpResult );
    m_State = DIALOG_STATE_POPUP;
}

//==============================================================================

void dlg_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitAudio();
    PendingSettings.CommitLocalization();
    PendingSettings.CommitGraphics();
    PendingSettings.UpdateHeadsetMode( PendingSettings.GetHeadsetMode() );
}

//==============================================================================

void dlg_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    s32 Highlight = -1;

    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        ui_button* pButtons[] = { m_pAudio, m_pHeadset, m_pGraphics, m_pDisplay, m_pUI, m_pLanguage, m_pAccept };
        for( s32 i = 0; i < 7; i++ )
        {
            pButtons[i]->SetFlag( ui_win::WF_VISIBLE, TRUE );
        }
        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( (ui_control*)m_pAudio );
            m_CurrentControl = IDC_SETTINGS_AUDIO;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
    }

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
            RestoreSettings();
            m_State = DIALOG_STATE_BACK;
        }
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    ui_button* pButtons[] = { m_pAudio, m_pHeadset, m_pGraphics, m_pDisplay, m_pUI, m_pLanguage, m_pAccept };
    for( s32 i = 0; i < 7; i++ )
    {
        if( pButtons[i]->IsFocused() )
        {
            Highlight = i;
            g_UiMgr->SetScreenHighlight( pButtons[i]->GetPosition() );
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

void dlg_settings::OnSaveSettingsCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        g_StateMgr.ActivatePendingSettings();
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;
    }
    else
    {
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_SAVE_DATA_ERROR;
    }
}

//==============================================================================
