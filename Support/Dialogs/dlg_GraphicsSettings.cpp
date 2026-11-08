//==============================================================================
//
//  dlg_GraphicsSettings.cpp
//
//==============================================================================

#include "Entropy.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_slider.hpp"
#include "UI/ui_text.hpp"

#include "dlg_GraphicsSettings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================

ui_manager::control_tem GraphicsSettingsControls[] =
{
    { IDC_GRAPHICS_FIELD_OF_VIEW_TEXT,       "IDS_NULL",                   "text",    40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_FILM_GRAIN_TEXT,          "IDS_NULL",                   "text",    40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_RENDER_SCALE_TEXT,        "IDS_NULL",                   "text",    40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ANTI_ALIASING_TEXT,       "IDS_NULL",                   "text",    40, 145, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_TEXTURE_FILTERING_TEXT,   "IDS_NULL",                   "text",    40, 180, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_GRAPHICS_FIELD_OF_VIEW,            "IDS_NULL",                   "slider", 300,  40, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_FILM_GRAIN,               "IDS_NULL",                   "slider", 300,  75, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_RENDER_SCALE,             "IDS_NULL",                   "combo",  290, 110, 140, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ANTI_ALIASING,            "IDS_NULL",                   "combo",  290, 145, 140, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_TEXTURE_FILTERING,        "IDS_NULL",                   "combo",  290, 180, 140, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_GRAPHICS_ADVANCED,                 "IDS_GRAPHICS_ADVANCED_SETTINGS", "button", 40, 250, 220, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },

    { IDC_GRAPHICS_BUTTON_APPLY,             "IDS_PROFILE_OPTIONS_ACCEPT", "button",  40, 285, 220, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem GraphicsSettingsDialog =
{
    "IDS_OPTIONS_GRAPHICS_SETTINGS",
    1, 9,
    sizeof(GraphicsSettingsControls)/sizeof(ui_manager::control_tem),
    &GraphicsSettingsControls[0],
    0
};

//==============================================================================

void dlg_graphics_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "graphics settings", &GraphicsSettingsDialog, &dlg_graphics_settings_factory );
}

//==============================================================================

ui_win* dlg_graphics_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_graphics_settings* pDialog = new dlg_graphics_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return (ui_win*)pDialog;
}

//==============================================================================

dlg_graphics_settings::dlg_graphics_settings( void )
{
}

//==============================================================================

dlg_graphics_settings::~dlg_graphics_settings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_graphics_settings::Create( s32                       UserID,
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

    m_pFieldOfView        = (ui_slider*)FindChildByID( IDC_GRAPHICS_FIELD_OF_VIEW );
    m_pRenderScale        = (ui_combo*) FindChildByID( IDC_GRAPHICS_RENDER_SCALE );
    m_pFilmGrain          = (ui_slider*)FindChildByID( IDC_GRAPHICS_FILM_GRAIN );
    m_pTextureFiltering     = (ui_combo*) FindChildByID( IDC_GRAPHICS_TEXTURE_FILTERING );
    m_pAntiAliasing       = (ui_combo*) FindChildByID( IDC_GRAPHICS_ANTI_ALIASING );
    m_pAdvanced           = (ui_button*)FindChildByID( IDC_GRAPHICS_ADVANCED );
    m_pButtonApply        = (ui_button*)FindChildByID( IDC_GRAPHICS_BUTTON_APPLY );

    m_pFieldOfViewText    = (ui_text*)FindChildByID( IDC_GRAPHICS_FIELD_OF_VIEW_TEXT );
    m_pRenderScaleText    = (ui_text*)FindChildByID( IDC_GRAPHICS_RENDER_SCALE_TEXT );
    m_pFilmGrainText      = (ui_text*)FindChildByID( IDC_GRAPHICS_FILM_GRAIN_TEXT );
    m_pTextureFilteringText = (ui_text*)FindChildByID( IDC_GRAPHICS_TEXTURE_FILTERING_TEXT );
    m_pAntiAliasingText   = (ui_text*)FindChildByID( IDC_GRAPHICS_ANTI_ALIASING_TEXT );

    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    m_OriginalSettings = PendingSettings;

    m_pFieldOfView->SetRange( FIELD_OF_VIEW_MIN_DEGREES, FIELD_OF_VIEW_MAX_DEGREES );
    m_pFieldOfView->SetStep( 5 );
    m_pFieldOfView->SetValue( PendingSettings.GetFieldOfView() );
    m_pFieldOfView->UseDefaultSound( FALSE );

    m_pRenderScale->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    for( s32 Percent = RENDER_SCALE_MIN_PERCENT;
         Percent <= RENDER_SCALE_MAX_PERCENT;
         Percent += 25 )
    {
        m_pRenderScale->AddItem( xwstring( xfs( "%d%%", Percent ) ), Percent );
    }
    s32 RenderScaleSelection = m_pRenderScale->FindItemByData( PendingSettings.GetRenderScale() );
    if( RenderScaleSelection == -1 )
    {
        RenderScaleSelection = m_pRenderScale->FindItemByData( RENDER_SCALE_DEFAULT_PERCENT );
    }
    m_pRenderScale->SetSelection( RenderScaleSelection );

    m_pFilmGrain->SetRange( FILM_GRAIN_MIN_STRENGTH, FILM_GRAIN_MAX_STRENGTH );
    m_pFilmGrain->SetStep( 5, 10 );
    m_pFilmGrain->SetValue( PendingSettings.GetFilmGrainStrength() );
    m_pFilmGrain->UseDefaultSound( FALSE );

    m_pTextureFiltering->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pTextureFiltering->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_FILTER_BILINEAR" ),
                                 RSTATE_TEXTURE_FILTER_BILINEAR );
    m_pTextureFiltering->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_FILTER_TRILINEAR" ),
                                 RSTATE_TEXTURE_FILTER_TRILINEAR );
    for( s32 Filtering = RSTATE_TEXTURE_FILTER_ANISOTROPIC_2;
         Filtering <= RSTATE_TEXTURE_FILTER_ANISOTROPIC_16; ++Filtering )
    {
        xwstring Label( g_StringTableMgr( "ui", "IDS_GRAPHICS_FILTER_ANISOTROPIC" ) );
        Label += xwstring( xfs( " %dx", 2 << (Filtering - RSTATE_TEXTURE_FILTER_ANISOTROPIC_2) ) );
        m_pTextureFiltering->AddItem( Label, Filtering );
    }
    m_pTextureFiltering->SetSelection(
        m_pTextureFiltering->FindItemByData( PendingSettings.GetTextureFiltering() ) );

    m_pAntiAliasing->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pAntiAliasing->AddItem( g_StringTableMgr( "ui", "IDS_OFF" ),
                              static_cast<uaddr>( AntiAliasingType::None ) );
    m_pAntiAliasing->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_ANTI_ALIASING_MSAA" ),
                              static_cast<uaddr>( AntiAliasingType::Msaa ) );
    m_pAntiAliasing->AddItem( g_StringTableMgr( "ui", "IDS_GRAPHICS_ANTI_ALIASING_CMAA2" ),
                              static_cast<uaddr>( AntiAliasingType::Cmaa2 ) );
    s32 AntiAliasingSelection =
        m_pAntiAliasing->FindItemByData( static_cast<uaddr>( PendingSettings.GetAntiAliasingType() ) );
    if( AntiAliasingSelection == -1 )
    {
        AntiAliasingSelection =
            m_pAntiAliasing->FindItemByData( static_cast<uaddr>( ANTI_ALIASING_TYPE_DEFAULT ) );
    }
    m_pAntiAliasing->SetSelection( AntiAliasingSelection );

    m_pFieldOfViewText   ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_FIELD_OF_VIEW" ) );
    m_pRenderScaleText   ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_RENDER_SCALE" ) );
    m_pFilmGrainText     ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_FILM_GRAIN" ) );
    m_pTextureFilteringText->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_TEXTURE_FILTERING" ) );
    m_pAntiAliasingText  ->SetLabel( g_StringTableMgr( "ui", "IDS_GRAPHICS_ANTI_ALIASING" ) );

    m_pFieldOfViewText   ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pRenderScaleText   ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pFilmGrainText     ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pTextureFilteringText->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pAntiAliasingText  ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pButtonApply       ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pAdvanced          ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( (ui_control*)m_pFieldOfView );
        m_CurrentControl = IDC_GRAPHICS_FIELD_OF_VIEW;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    m_pFieldOfView       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pRenderScale       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFilmGrain         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pTextureFiltering    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAntiAliasing      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAdvanced          ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonApply       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFieldOfViewText   ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pRenderScaleText   ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFilmGrainText     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pTextureFilteringText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pAntiAliasingText  ->SetFlag( ui_win::WF_VISIBLE, FALSE );

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

void dlg_graphics_settings::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_graphics_settings::Render( s32 ox, s32 oy )
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

void dlg_graphics_settings::OnAccept( ui_win* pWin )
{
    if( m_State != DIALOG_STATE_ACTIVE )
    {
        return;
    }

    if( pWin == (ui_win*)m_pAdvanced )
    {
        // Keep edits from the basic page when opening Advanced.
        ApplySettings( g_StateMgr.GetPendingSettings() );
        g_AudioMgr.Play( "Select_Norm" );
        m_CurrentControl = IDC_GRAPHICS_ADVANCED;
        m_State = DIALOG_STATE_SELECT;
        return;
    }

    if( pWin != (ui_win*)m_pButtonApply )
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

void dlg_graphics_settings::OnCancel( ui_win* pWin )
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

void dlg_graphics_settings::ApplySettings( global_settings& Settings )
{
    Settings.SetFieldOfView( m_pFieldOfView->GetValue() );
    if( m_pRenderScale->GetSelection() != -1 )
    {
        Settings.SetRenderScale( static_cast<s32>( m_pRenderScale->GetSelectedItemData() ) );
    }
    Settings.SetFilmGrainStrength( m_pFilmGrain->GetValue() );
    if( m_pTextureFiltering->GetSelection() != -1 )
    {
        Settings.SetTextureFiltering(
            static_cast<rstate_texture_filtering>( m_pTextureFiltering->GetSelectedItemData() ) );
    }
    if( m_pAntiAliasing->GetSelection() != -1 )
    {
        Settings.SetAntiAliasingType(
            static_cast<AntiAliasingType>( m_pAntiAliasing->GetSelectedItemData() ) );
    }
}

//==============================================================================

void dlg_graphics_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_graphics_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_graphics_settings::OpenSavePopup( void )
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

void dlg_graphics_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitGraphics();
}

//==============================================================================

void dlg_graphics_settings::OnSaveSettingsCB( void )
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

void dlg_graphics_settings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Select_Norm" );
        global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
        PendingSettings.Reset( RESET_GRAPHICS );

        m_pFieldOfView->SetValue( PendingSettings.GetFieldOfView() );
        s32 const RenderScaleSelection =
            m_pRenderScale->FindItemByData( PendingSettings.GetRenderScale() );
        m_pRenderScale->SetSelection( RenderScaleSelection );
        m_pFilmGrain->SetValue( PendingSettings.GetFilmGrainStrength() );
        m_pTextureFiltering->SetSelection(
            m_pTextureFiltering->FindItemByData( PendingSettings.GetTextureFiltering() ) );
        s32 const AntiAliasingSelection =
            m_pAntiAliasing->FindItemByData(
                static_cast<uaddr>( ANTI_ALIASING_TYPE_DEFAULT ) );
        m_pAntiAliasing->SetSelection( AntiAliasingSelection );
    }
}

//==============================================================================

void dlg_graphics_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
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
        m_pFieldOfView       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pRenderScale       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFilmGrain         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pTextureFiltering    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAntiAliasing      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAdvanced          ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonApply       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFieldOfViewText   ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pRenderScaleText   ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFilmGrainText     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pTextureFilteringText->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pAntiAliasingText  ->SetFlag( ui_win::WF_VISIBLE, TRUE );

        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( (ui_control*)m_pFieldOfView );
            m_CurrentControl = IDC_GRAPHICS_FIELD_OF_VIEW;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
        g_UiMgr->SetScreenHighlight( m_pFieldOfViewText->GetPosition() );
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    if( m_pFieldOfView->IsFocused() )
    {
        highLight = 0;
        m_pFieldOfViewText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pFieldOfViewText->GetPosition() );
    }
    else
    {
        m_pFieldOfViewText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pFilmGrain->IsFocused() )
    {
        highLight = 1;
        m_pFilmGrainText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pFilmGrainText->GetPosition() );
    }
    else
    {
        m_pFilmGrainText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pTextureFiltering->IsFocused() )
    {
        highLight = 4;
        m_pTextureFilteringText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pTextureFilteringText->GetPosition() );
    }
    else
    {
        m_pTextureFilteringText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pRenderScale->IsFocused() )
    {
        highLight = 2;
        m_pRenderScaleText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pRenderScaleText->GetPosition() );
    }
    else
    {
        m_pRenderScaleText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pAntiAliasing->IsFocused() )
    {
        highLight = 3;
        m_pAntiAliasingText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pAntiAliasingText->GetPosition() );
    }
    else
    {
        m_pAntiAliasingText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pAdvanced->IsFocused() )
    {
        highLight = 5;
        g_UiMgr->SetScreenHighlight( m_pAdvanced->GetPosition() );
    }

    if( m_pButtonApply->IsFocused() )
    {
        highLight = 6;
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
