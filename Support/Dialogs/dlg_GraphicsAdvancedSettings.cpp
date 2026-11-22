//==============================================================================
//
//  dlg_GraphicsAdvancedSettings.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_check.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_text.hpp"

#include "dlg_GraphicsAdvancedSettings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================
//  DATA
//==============================================================================

ui_manager::control_tem GraphicsAdvancedSettingsControls[] =
{
    { IDC_GRAPHICS_ADVANCED_PHONG_SHADING_TEXT,     "IDS_GRAPHICS_PHONG_SHADING",     "text",  40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_SHADOWS_TEXT,           "IDS_GRAPHICS_SHADOWS",           "text",  40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_SHADOW_FILTER_TEXT,     "IDS_GRAPHICS_SHADOW_FILTER",     "text",  40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_BACKGROUND_BLUR_TEXT,   "IDS_GRAPHICS_BACKGROUND_BLUR",   "text",  40, 145, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_TESSELLATION_TEXT,      "IDS_GRAPHICS_TESSELATION",       "text",  40, 180, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_AMBIENT_OCCLUSION_TEXT, "IDS_GRAPHICS_AMBIENT_OCCLUSION", "text",  40, 215, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_GRAPHICS_ADVANCED_PHONG_SHADING,          "IDS_NULL",                       "combo", 290,  40, 140, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_SHADOWS,                "IDS_NULL",                       "combo", 290,  75, 140, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_SHADOW_FILTER,          "IDS_NULL",                       "combo", 290, 110, 140, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_BACKGROUND_BLUR,        "IDS_NULL",                       "check", 290, 145, 140, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_TESSELLATION,           "IDS_NULL",                       "check", 290, 180, 140, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_AMBIENT_OCCLUSION,      "IDS_NULL",                       "combo", 290, 215, 140, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED_BUTTON_APPLY,           "IDS_PROFILE_OPTIONS_ACCEPT",     "button", 40, 285, 220, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem GraphicsAdvancedSettingsDialog =
{
    "IDS_OPTIONS_GRAPHICS_ADVANCED_SETTINGS",
    1, 7,
    sizeof(GraphicsAdvancedSettingsControls)/sizeof(ui_manager::control_tem),
    &GraphicsAdvancedSettingsControls[0],
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_graphics_advanced_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "graphics advanced settings",
                                   &GraphicsAdvancedSettingsDialog,
                                   &dlg_graphics_advanced_settings_factory );
}

//==============================================================================

ui_win* dlg_graphics_advanced_settings_factory( s32 UserID,
                                                ui_manager* pManager,
                                                ui_manager::dialog_tem* pDialogTem,
                                                const irect& Position,
                                                ui_win* pParent,
                                                s32 Flags,
                                                void* pUserData )
{
    dlg_graphics_advanced_settings* pDialog = new dlg_graphics_advanced_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return pDialog;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_graphics_advanced_settings::dlg_graphics_advanced_settings( void )
{
}

//==============================================================================

dlg_graphics_advanced_settings::~dlg_graphics_advanced_settings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_graphics_advanced_settings::Create( s32                       UserID,
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

    m_pPhongShading      = static_cast<ui_combo*>( FindChildByID( IDC_GRAPHICS_ADVANCED_PHONG_SHADING ) );
    m_pShadows           = static_cast<ui_combo*>( FindChildByID( IDC_GRAPHICS_ADVANCED_SHADOWS ) );
    m_pShadowFilter      = static_cast<ui_combo*>( FindChildByID( IDC_GRAPHICS_ADVANCED_SHADOW_FILTER ) );
    m_pBackgroundBlur    = static_cast<ui_check*>( FindChildByID( IDC_GRAPHICS_ADVANCED_BACKGROUND_BLUR ) );
    m_pTessellation      = static_cast<ui_check*>( FindChildByID( IDC_GRAPHICS_ADVANCED_TESSELLATION ) );
    m_pAmbientOcclusion  = static_cast<ui_combo*>( FindChildByID( IDC_GRAPHICS_ADVANCED_AMBIENT_OCCLUSION ) );
    m_pButtonApply       = static_cast<ui_button*>( FindChildByID( IDC_GRAPHICS_ADVANCED_BUTTON_APPLY ) );

    m_pPhongShadingText      = static_cast<ui_text*>( FindChildByID( IDC_GRAPHICS_ADVANCED_PHONG_SHADING_TEXT ) );
    m_pShadowsText           = static_cast<ui_text*>( FindChildByID( IDC_GRAPHICS_ADVANCED_SHADOWS_TEXT ) );
    m_pShadowFilterText      = static_cast<ui_text*>( FindChildByID( IDC_GRAPHICS_ADVANCED_SHADOW_FILTER_TEXT ) );
    m_pBackgroundBlurText    = static_cast<ui_text*>( FindChildByID( IDC_GRAPHICS_ADVANCED_BACKGROUND_BLUR_TEXT ) );
    m_pTessellationText      = static_cast<ui_text*>( FindChildByID( IDC_GRAPHICS_ADVANCED_TESSELLATION_TEXT ) );
    m_pAmbientOcclusionText  = static_cast<ui_text*>( FindChildByID( IDC_GRAPHICS_ADVANCED_AMBIENT_OCCLUSION_TEXT ) );

    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    m_OriginalSettings = PendingSettings;

    m_pPhongShading->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pPhongShading->AddItem( g_StringTableMgr( "ui", "IDS_OFF" ), 0 );
    m_pPhongShading->AddItem( g_StringTableMgr( "ui", "IDS_ON"  ), 1 );
    m_pPhongShading->SetSelection( PendingSettings.GetPhongShadingEnabled() ? 1 : 0 );

    m_pShadows->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pShadows->AddItem( g_StringTableMgr( "ui", "IDS_OFF" ),
                         static_cast<s32>( ShadowMode::Disabled ) );
    m_pShadows->AddItem( g_StringTableMgr( "ui", "IDS_ON" ),
                         static_cast<s32>( ShadowMode::Enabled ) );
    m_pShadows->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_FLASHLIGHT_SHADOWS" ),
                         static_cast<s32>( ShadowMode::FlashlightOnly ) );
    s32 ShadowModeSelection = m_pShadows->FindItemByData(
        static_cast<s32>( PendingSettings.GetShadowMode() ) );
    if( ShadowModeSelection == -1 )
    {
        ShadowModeSelection = m_pShadows->FindItemByData(
            static_cast<s32>( SHADOW_MODE_DEFAULT ) );
    }
    m_pShadows->SetSelection( ShadowModeSelection );

    m_pShadowFilter->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pShadowFilter->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_SHADOW_FILTER_HARD" ),
                              static_cast<s32>( ShadowFilterType::Hard ) );
    m_pShadowFilter->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_SHADOW_FILTER_EVSM" ),
                              static_cast<s32>( ShadowFilterType::Evsm ) );
    s32 ShadowFilterSelection = m_pShadowFilter->FindItemByData(
        static_cast<s32>( PendingSettings.GetShadowFilterType() ) );
    if( ShadowFilterSelection == -1 )
    {
        ShadowFilterSelection = m_pShadowFilter->FindItemByData(
            static_cast<s32>( SHADOW_FILTER_TYPE_DEFAULT ) );
    }
    m_pShadowFilter->SetSelection( ShadowFilterSelection );

    m_pBackgroundBlur->SetChecked( PendingSettings.GetBackgroundBlurEnabled() );
    m_pTessellation->SetChecked( PendingSettings.GetTessellationEnabled() );

    m_pAmbientOcclusion->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pAmbientOcclusion->AddItem( g_StringTableMgr( "ui", "IDS_OFF" ),
                                 static_cast<s32>( render::AmbientOcclusionQuality::Disabled ) );
    m_pAmbientOcclusion->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_AO_LOW" ),
                                 static_cast<s32>( render::AmbientOcclusionQuality::Low ) );
    m_pAmbientOcclusion->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_AO_MEDIUM" ),
                                 static_cast<s32>( render::AmbientOcclusionQuality::Medium ) );
    m_pAmbientOcclusion->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_AO_HIGH" ),
                                 static_cast<s32>( render::AmbientOcclusionQuality::High ) );
    m_pAmbientOcclusion->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_AO_ULTRA" ),
                                 static_cast<s32>( render::AmbientOcclusionQuality::Ultra ) );
    m_pAmbientOcclusion->SetSelection(
        m_pAmbientOcclusion->FindItemByData( static_cast<s32>( PendingSettings.GetAmbientOcclusionQuality() ) ) );

    m_pBackgroundBlurText->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pTessellationText->SetLabelFlags( ui_font::h_left | ui_font::v_center );
    m_pAmbientOcclusionText->SetLabelFlags( ui_font::h_left | ui_font::v_center );

    m_pPhongShadingText     ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pShadowsText          ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pShadowFilterText     ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pButtonApply          ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( m_pPhongShading );
        m_CurrentControl = IDC_GRAPHICS_ADVANCED_PHONG_SHADING;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    m_pPhongShading        ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pShadows             ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pShadowFilter        ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pBackgroundBlur      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pTessellation        ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAmbientOcclusion    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonApply         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pPhongShadingText    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pShadowsText         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pShadowFilterText    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pBackgroundBlurText  ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pTessellationText    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAmbientOcclusionText->SetFlag( ui_win::WF_VISIBLE, FALSE );

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

void dlg_graphics_advanced_settings::Destroy( void )
{
    // Cancel outstanding save callbacks before destroying the dialog.
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_graphics_advanced_settings::Render( s32 ox, s32 oy )
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
    Bounds.Set( m_CurrPos.l + 22, m_CurrPos.t, m_CurrPos.r - 23, m_CurrPos.b );
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

void dlg_graphics_advanced_settings::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || (pWin != m_pButtonApply) )
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

void dlg_graphics_advanced_settings::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State != DIALOG_STATE_ACTIVE )
    {
        return;
    }

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

//==============================================================================

void dlg_graphics_advanced_settings::ApplySettings( global_settings& Settings )
{
    Settings.SetBackgroundBlurEnabled( m_pBackgroundBlur->IsChecked() );
    Settings.SetTessellationEnabled( m_pTessellation->IsChecked() );
    if( m_pAmbientOcclusion->GetSelection() != -1 )
    {
        Settings.SetAmbientOcclusionQuality(
            static_cast<render::AmbientOcclusionQuality>( m_pAmbientOcclusion->GetSelectedItemData() ) );
    }
    if( m_pPhongShading->GetSelection() != -1 )
    {
        Settings.SetPhongShadingEnabled(
            static_cast<xbool>( m_pPhongShading->GetSelectedItemData() ) );
    }
    if( m_pShadows->GetSelection() != -1 )
    {
        Settings.SetShadowMode(
            static_cast<ShadowMode>( m_pShadows->GetSelectedItemData() ) );
    }
    if( m_pShadowFilter->GetSelection() != -1 )
    {
        Settings.SetShadowFilterType(
            static_cast<ShadowFilterType>( m_pShadowFilter->GetSelectedItemData() ) );
    }
}

//==============================================================================

void dlg_graphics_advanced_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_graphics_advanced_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_graphics_advanced_settings::OpenSavePopup( void )
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

void dlg_graphics_advanced_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitGraphics();
}

//==============================================================================

void dlg_graphics_advanced_settings::OnSaveSettingsCB( void )
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

void dlg_graphics_advanced_settings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Select_Norm" );
        global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
        PendingSettings.Reset( RESET_GRAPHICS );

        m_pBackgroundBlur->SetChecked( PendingSettings.GetBackgroundBlurEnabled() );
        m_pTessellation->SetChecked( PendingSettings.GetTessellationEnabled() );
        m_pAmbientOcclusion->SetSelection(
            m_pAmbientOcclusion->FindItemByData( static_cast<s32>( PendingSettings.GetAmbientOcclusionQuality() ) ) );
        m_pPhongShading->SetSelection( PendingSettings.GetPhongShadingEnabled() ? 1 : 0 );
        m_pShadows->SetSelection(
            m_pShadows->FindItemByData( static_cast<s32>( PendingSettings.GetShadowMode() ) ) );
        m_pShadowFilter->SetSelection(
            m_pShadowFilter->FindItemByData( static_cast<s32>( PendingSettings.GetShadowFilterType() ) ) );
    }
}

//==============================================================================

void dlg_graphics_advanced_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

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
            RestoreSettings();
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    // Show the controls only after the opening animation finishes.
    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        m_pPhongShading        ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pShadows             ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pShadowFilter        ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pBackgroundBlur      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pTessellation        ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAmbientOcclusion    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonApply         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pPhongShadingText    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pShadowsText         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pShadowFilterText    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pBackgroundBlurText  ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pTessellationText    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAmbientOcclusionText->SetFlag( ui_win::WF_VISIBLE, TRUE );

        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( m_pPhongShading );
            m_CurrentControl = IDC_GRAPHICS_ADVANCED_PHONG_SHADING;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
    }

    xbool const ShadowFilterEnabled =
        (m_pShadows->GetSelectedItemData() != static_cast<s32>( ShadowMode::Disabled ));
    m_pShadowFilter->SetFlag( ui_win::WF_DISABLED, !ShadowFilterEnabled );
    m_pShadowFilterText->SetFlag( ui_win::WF_DISABLED, !ShadowFilterEnabled );
    if( !ShadowFilterEnabled && m_pShadowFilter->IsFocused() )
    {
        GotoControl( m_pShadows );
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    if( m_pPhongShading->IsFocused() )
    {
        m_pPhongShadingText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pPhongShadingText->GetPosition() );
    }
    else
    {
        m_pPhongShadingText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pShadows->IsFocused() )
    {
        m_pShadowsText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pShadowsText->GetPosition() );
    }
    else
    {
        m_pShadowsText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pShadowFilter->IsFocused() )
    {
        m_pShadowFilterText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pShadowFilterText->GetPosition() );
    }
    else
    {
        m_pShadowFilterText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pBackgroundBlur->IsFocused() )
    {
        m_pBackgroundBlurText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pBackgroundBlurText->GetPosition() );
    }
    else
    {
        m_pBackgroundBlurText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pTessellation->IsFocused() )
    {
        m_pTessellationText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pTessellationText->GetPosition() );
    }
    else
    {
        m_pTessellationText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pAmbientOcclusion->IsFocused() )
    {
        m_pAmbientOcclusionText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pAmbientOcclusionText->GetPosition() );
    }
    else
    {
        m_pAmbientOcclusionText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pButtonApply->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonApply->GetPosition() );
    }

}

//==============================================================================
