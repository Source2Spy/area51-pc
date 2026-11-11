//==============================================================================
//
//  dlg_UISettings.cpp
//
//==============================================================================

#include "Entropy.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_text.hpp"

#include "dlg_UISettings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================

enum ui_settings_controls
{
    IDC_UI_SETTINGS_UI_SCALE_TEXT,
    IDC_UI_SETTINGS_HUD_SCALE_TEXT,

    IDC_UI_SETTINGS_UI_SCALE,
    IDC_UI_SETTINGS_HUD_SCALE,
    IDC_UI_SETTINGS_BUTTON_APPLY,
};

//==============================================================================

ui_manager::control_tem UISettingsControls[] =
{
    { IDC_UI_SETTINGS_UI_SCALE_TEXT,  "IDS_NULL",                   "text",   40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_UI_SETTINGS_HUD_SCALE_TEXT, "IDS_NULL",                   "text",   40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_UI_SETTINGS_UI_SCALE,       "IDS_NULL",                   "combo", 290,  40, 140, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_UI_SETTINGS_HUD_SCALE,      "IDS_NULL",                   "combo", 290,  75, 140, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_UI_SETTINGS_BUTTON_APPLY,   "IDS_PROFILE_OPTIONS_ACCEPT", "button", 40, 285, 220, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem UISettingsDialog =
{
    "IDS_OPTIONS_UI_SETTINGS",
    1, 5,
    sizeof(UISettingsControls) / sizeof(ui_manager::control_tem),
    &UISettingsControls[0],
    0
};

//==============================================================================

void dlg_ui_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "ui settings", &UISettingsDialog, &dlg_ui_settings_factory );
}

//==============================================================================

ui_win* dlg_ui_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_ui_settings* pDialog = new dlg_ui_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return (ui_win*)pDialog;
}

//==============================================================================

dlg_ui_settings::dlg_ui_settings( void )
{
}

//==============================================================================

dlg_ui_settings::~dlg_ui_settings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_ui_settings::Create( s32                       UserID,
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

    m_pUIScale     = (ui_combo*) FindChildByID( IDC_UI_SETTINGS_UI_SCALE );
    m_pHUDScale    = (ui_combo*) FindChildByID( IDC_UI_SETTINGS_HUD_SCALE );
    m_pButtonApply = (ui_button*)FindChildByID( IDC_UI_SETTINGS_BUTTON_APPLY );

    m_pUIScaleText  = (ui_text*)FindChildByID( IDC_UI_SETTINGS_UI_SCALE_TEXT );
    m_pHUDScaleText = (ui_text*)FindChildByID( IDC_UI_SETTINGS_HUD_SCALE_TEXT );

    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    m_OriginalSettings = PendingSettings;

    m_pUIScale ->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pHUDScale->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );

    for( s32 Percent = UI_SCALE_MIN_PERCENT; Percent <= UI_SCALE_MAX_PERCENT; Percent += 10 )
    {
        m_pUIScale->AddItem( xwstring( xfs( "%d%%", Percent ) ), Percent );
    }
    for( s32 Percent = HUD_SCALE_MIN_PERCENT; Percent <= HUD_SCALE_MAX_PERCENT; Percent += 10 )
    {
        m_pHUDScale->AddItem( xwstring( xfs( "%d%%", Percent ) ), Percent );
    }

    s32 UIScaleSelection = m_pUIScale->FindItemByData( PendingSettings.GetUIScale() );
    if( UIScaleSelection == -1 )
    {
        UIScaleSelection = m_pUIScale->FindItemByData( UI_SCALE_DEFAULT_PERCENT );
    }
    m_pUIScale->SetSelection( UIScaleSelection );

    s32 HUDScaleSelection = m_pHUDScale->FindItemByData( PendingSettings.GetHUDScale() );
    if( HUDScaleSelection == -1 )
    {
        HUDScaleSelection = m_pHUDScale->FindItemByData( HUD_SCALE_DEFAULT_PERCENT );
    }
    m_pHUDScale->SetSelection( HUDScaleSelection );

    m_pUIScaleText ->SetLabel( g_StringTableMgr( "ui", "IDS_DISPLAY_UI_SCALE" ) );
    m_pHUDScaleText->SetLabel( g_StringTableMgr( "ui", "IDS_DISPLAY_HUD_SCALE" ) );
    m_pUIScaleText ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pHUDScaleText->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pButtonApply->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( (ui_control*)m_pUIScale );
        m_CurrentControl = IDC_UI_SETTINGS_UI_SCALE;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    m_pUIScale     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pHUDScale    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonApply ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pUIScaleText ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pHUDScaleText->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_CurrHL = 0;
    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;

    xwstring navText( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) );
    navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    navText += g_StringTableMgr( "ui", "IDS_NAV_RESTORE_DEFAULTS" );
    SetNavText( navText );

    InitScreenScaling( Position );
    m_bRenderBlackout = FALSE;
    m_State = DIALOG_STATE_ACTIVE;
    return Success;
}

//==============================================================================

void dlg_ui_settings::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_ui_settings::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap   = 9;
    static s32 width = 4;
    irect rb;

    if( m_bRenderBlackout )
    {
        rb = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect( rb,
                                    xcolor(0,0,0,180), xcolor(0,0,0,180),
                                    xcolor(0,0,0,180), xcolor(0,0,0,180), FALSE );
    }

    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;
    g_UiMgr->RenderGouraudRect( rb,
                                xcolor(56,115,58,64), xcolor(56,115,58,64),
                                xcolor(56,115,58,64), xcolor(56,115,58,64), FALSE );

    for( s32 y = rb.t + offset; y < rb.b; y += gap )
    {
        irect bar( rb.l, y, rb.r, MIN( y + width, rb.b ) );
        g_UiMgr->RenderGouraudRect( bar,
                                    xcolor(56,115,58,30), xcolor(56,115,58,30),
                                    xcolor(56,115,58,30), xcolor(56,115,58,30), FALSE );
    }

    ui_dialog::Render( ox, oy );
    g_UiMgr->RenderGlowBar();
}

//==============================================================================

void dlg_ui_settings::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || (pWin != (ui_win*)m_pButtonApply) )
    {
        return;
    }

    global_settings Settings = m_OriginalSettings;
    ApplySettings( Settings );
    if( !Settings.HasChanged() )
    {
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    g_StateMgr.GetPendingSettings() = Settings;
    g_AudioMgr.Play( "Select_Norm" );
    OpenSavePopup();
}

//==============================================================================

void dlg_ui_settings::OnCancel( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        global_settings Settings = m_OriginalSettings;
        ApplySettings( Settings );
        if( !Settings.HasChanged() )
        {
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
            return;
        }

        g_StateMgr.GetPendingSettings() = Settings;
        OpenSavePopup();
    }
}

//==============================================================================

void dlg_ui_settings::ApplySettings( global_settings& Settings )
{
    if( m_pUIScale->GetSelection() != -1 )
    {
        Settings.SetUIScale( static_cast<s32>( m_pUIScale->GetSelectedItemData() ) );
    }
    if( m_pHUDScale->GetSelection() != -1 )
    {
        Settings.SetHUDScale( static_cast<s32>( m_pHUDScale->GetSelectedItemData() ) );
    }
}

//==============================================================================

void dlg_ui_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_ui_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_ui_settings::OpenSavePopup( void )
{
    const irect Bounds = g_UiMgr->GetUserBounds( g_UiUserID );
    m_PopUp = static_cast<dlg_popup*>( g_UiMgr->OpenDialog( m_UserID,
                                                            "popup",
                                                            Bounds,
                                                            NULL,
                                                            ui_win::WF_VISIBLE | ui_win::WF_BORDER |
                                                            ui_win::WF_DLG_CENTER | ui_win::WF_INPUTMODAL ) );
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

void dlg_ui_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitGraphics();
}

//==============================================================================

void dlg_ui_settings::OnSaveSettingsCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        g_StateMgr.ActivatePendingSettings();
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

void dlg_ui_settings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Select_Norm" );
        global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
        PendingSettings.Reset( RESET_INTERFACE );
        m_pUIScale->SetSelection( m_pUIScale->FindItemByData( PendingSettings.GetUIScale() ) );
        m_pHUDScale->SetSelection( m_pHUDScale->FindItemByData( PendingSettings.GetHUDScale() ) );
    }
}

//==============================================================================

void dlg_ui_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    s32 highLight = -1;

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
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        m_pUIScale     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pHUDScale    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonApply ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pUIScaleText ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pHUDScaleText->SetFlag( ui_win::WF_VISIBLE, TRUE );

        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( (ui_control*)m_pUIScale );
            m_CurrentControl = IDC_UI_SETTINGS_UI_SCALE;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
        g_UiMgr->SetScreenHighlight( m_pUIScaleText->GetPosition() );
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    if( m_pUIScale->IsFocused() )
    {
        highLight = 0;
        m_pUIScaleText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pUIScaleText->GetPosition() );
    }
    else
    {
        m_pUIScaleText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pHUDScale->IsFocused() )
    {
        highLight = 1;
        m_pHUDScaleText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pHUDScaleText->GetPosition() );
    }
    else
    {
        m_pHUDScaleText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pButtonApply->IsFocused() )
    {
        highLight = 2;
        g_UiMgr->SetScreenHighlight( m_pButtonApply->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
        {
            g_AudioMgr.Play( "Cusor_Norm" );
        }
        m_CurrHL = highLight;
    }
}

//==============================================================================
