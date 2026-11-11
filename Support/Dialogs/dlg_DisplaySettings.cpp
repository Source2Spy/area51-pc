//==============================================================================
//
//  dlg_DisplaySettings.cpp
//
//==============================================================================

#include "Entropy.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_slider.hpp"
#include "UI/ui_text.hpp"

#include "dlg_DisplaySettings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================

enum display_controls
{
    IDC_DISPLAY_MODE_TEXT,
    IDC_DISPLAY_RESOLUTION_TEXT,
    IDC_DISPLAY_PRESENT_MODE_TEXT,
    IDC_DISPLAY_FRAME_LIMIT_TEXT,
    IDC_DISPLAY_BRIGHTNESS_TEXT,
    IDC_DISPLAY_CONTRAST_TEXT,

    IDC_DISPLAY_MODE,
    IDC_DISPLAY_RESOLUTION,
    IDC_DISPLAY_PRESENT_MODE,
    IDC_DISPLAY_FRAME_LIMIT,
    IDC_DISPLAY_BRIGHTNESS,
    IDC_DISPLAY_CONTRAST,
    IDC_DISPLAY_BUTTON_APPLY,
};

//==============================================================================

static const s32 ResolutionWidthDataIndex  = 0;
static const s32 ResolutionHeightDataIndex = 1;

//==============================================================================

static s32 AddResolutionItem( ui_combo* pCombo, s32 Width, s32 Height )
{
    return pCombo->AddItem( xwstring( xfs( "%dx%d", Width, Height ) ), Width, Height );
}

//==============================================================================

static xbool GetSelectedResolution( ui_combo const* pCombo, s32& Width, s32& Height )
{
    s32 const Selection = pCombo->GetSelection();
    if( Selection == -1 )
    {
        return FALSE;
    }

    Width  = static_cast<s32>( pCombo->GetItemData( Selection, ResolutionWidthDataIndex  ) );
    Height = static_cast<s32>( pCombo->GetItemData( Selection, ResolutionHeightDataIndex ) );
    return TRUE;
}

//==============================================================================

static s32 FindResolutionSelection( ui_combo const* pCombo, s32 Width, s32 Height )
{
    for( s32 iItem = 0; iItem < pCombo->GetItemCount(); iItem++ )
    {
        if( (static_cast<s32>( pCombo->GetItemData( iItem, ResolutionWidthDataIndex  ) ) == Width) &&
            (static_cast<s32>( pCombo->GetItemData( iItem, ResolutionHeightDataIndex ) ) == Height) )
        {
            return iItem;
        }
    }
    return -1;
}

//==============================================================================

static void AddPresentModeIfSupported( ui_combo*       pCombo,
                                       eng_present_mode Mode,
                                       const char*      pLabelID )
{
    if( eng_IsPresentModeSupported( Mode ) )
    {
        pCombo->AddItem( g_StringTableMgr( "ui", pLabelID ), Mode );
    }
}

//==============================================================================

ui_manager::control_tem DisplaySettingsControls[] =
{
    { IDC_DISPLAY_MODE_TEXT,       "IDS_NULL",                   "text",    40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_RESOLUTION_TEXT, "IDS_NULL",                   "text",    40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_PRESENT_MODE_TEXT,"IDS_NULL",                  "text",    40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_FRAME_LIMIT_TEXT, "IDS_NULL",                  "text",    40, 145, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_BRIGHTNESS_TEXT, "IDS_NULL",                   "text",    40, 198, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_CONTRAST_TEXT,   "IDS_NULL",                   "text",    40, 233, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_DISPLAY_MODE,            "IDS_NULL",                   "combo",  290,  40, 140, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_RESOLUTION,      "IDS_NULL",                   "combo",  290,  75, 140, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_PRESENT_MODE,    "IDS_NULL",                   "combo",  290, 110, 140, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_FRAME_LIMIT,     "IDS_NULL",                   "combo",  290, 145, 140, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_BRIGHTNESS,      "IDS_NULL",                   "slider", 300, 198, 120, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_DISPLAY_CONTRAST,        "IDS_NULL",                   "slider", 300, 233, 120, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },

    { IDC_DISPLAY_BUTTON_APPLY,    "IDS_PROFILE_OPTIONS_ACCEPT", "button",  40, 285, 220, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem DisplaySettingsDialog =
{
    "IDS_OPTIONS_DISPLAY_SETTINGS",
    1, 9,
    sizeof(DisplaySettingsControls)/sizeof(ui_manager::control_tem),
    &DisplaySettingsControls[0],
    0
};

//==============================================================================

void dlg_display_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "display settings", &DisplaySettingsDialog, &dlg_display_settings_factory );
}

//==============================================================================

ui_win* dlg_display_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_display_settings* pDialog = new dlg_display_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return (ui_win*)pDialog;
}

//==============================================================================

dlg_display_settings::dlg_display_settings( void )
{
}

//==============================================================================

dlg_display_settings::~dlg_display_settings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_display_settings::Create( s32                       UserID,
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

    m_pDisplayMode       = (ui_combo*) FindChildByID( IDC_DISPLAY_MODE );
    m_pResolution        = (ui_combo*) FindChildByID( IDC_DISPLAY_RESOLUTION );
    m_pPresentMode       = (ui_combo*) FindChildByID( IDC_DISPLAY_PRESENT_MODE );
    m_pFrameLimit        = (ui_combo*) FindChildByID( IDC_DISPLAY_FRAME_LIMIT );
    m_pBrightness        = (ui_slider*)FindChildByID( IDC_DISPLAY_BRIGHTNESS );
    m_pContrast          = (ui_slider*)FindChildByID( IDC_DISPLAY_CONTRAST );
    m_pButtonApply       = (ui_button*)FindChildByID( IDC_DISPLAY_BUTTON_APPLY );

    m_pDisplayModeText   = (ui_text*)FindChildByID( IDC_DISPLAY_MODE_TEXT );
    m_pResolutionText    = (ui_text*)FindChildByID( IDC_DISPLAY_RESOLUTION_TEXT );
    m_pPresentModeText   = (ui_text*)FindChildByID( IDC_DISPLAY_PRESENT_MODE_TEXT );
    m_pFrameLimitText    = (ui_text*)FindChildByID( IDC_DISPLAY_FRAME_LIMIT_TEXT );
    m_pBrightnessText    = (ui_text*)FindChildByID( IDC_DISPLAY_BRIGHTNESS_TEXT );
    m_pContrastText      = (ui_text*)FindChildByID( IDC_DISPLAY_CONTRAST_TEXT );

    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    m_OriginalSettings = PendingSettings;

    m_pDisplayMode->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pResolution ->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pPresentMode->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pFrameLimit ->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );

    m_pBrightness->SetRange( BRIGHTNESS_MIN_PERCENT, BRIGHTNESS_MAX_PERCENT );
    m_pBrightness->SetStep( 5 );
    m_pBrightness->SetValue( PendingSettings.GetBrightness() );
    m_pBrightness->UseDefaultSound( FALSE );

    m_pContrast->SetRange( CONTRAST_MIN_PERCENT, CONTRAST_MAX_PERCENT );
    m_pContrast->SetStep( 5 );
    m_pContrast->SetValue( PendingSettings.GetContrast() );
    m_pContrast->UseDefaultSound( FALSE );

    m_pDisplayMode->AddItem( g_StringTableMgr( "ui", "IDS_DISPLAY_MODE_BORDERLESS" ), ENG_DISPLAY_BORDERLESS );
    m_pDisplayMode->AddItem( g_StringTableMgr( "ui", "IDS_DISPLAY_MODE_WINDOWED"   ), ENG_DISPLAY_WINDOWED );
    s32 DisplayModeSelection = m_pDisplayMode->FindItemByData( PendingSettings.GetDisplayMode() );
    m_pDisplayMode->SetSelection( (DisplayModeSelection == -1) ? 0 : DisplayModeSelection );

    s32 SelectedWidth  = PendingSettings.GetDisplayWidth();
    s32 SelectedHeight = PendingSettings.GetDisplayHeight();
    if( (SelectedWidth <= 0) || (SelectedHeight <= 0) )
    {
        eng_GetRes( SelectedWidth, SelectedHeight );
    }

    xarray<eng_display_resolution> DisplayResolutions;
    if( !eng_GetDisplayResolutions( DisplayResolutions ) )
    {
        x_DebugMsg( "Display settings: failed to enumerate display resolutions\n" );
    }

    for( s32 iResolution = 0; iResolution < DisplayResolutions.GetCount(); iResolution++ )
    {
        eng_display_resolution const& Resolution = DisplayResolutions[iResolution];
        AddResolutionItem( m_pResolution, Resolution.GetWidth(), Resolution.GetHeight() );
    }

    s32 ResolutionSelection = FindResolutionSelection( m_pResolution, SelectedWidth, SelectedHeight );
    if( (ResolutionSelection == -1) && (SelectedWidth > 0) && (SelectedHeight > 0) )
    {
        ResolutionSelection = AddResolutionItem( m_pResolution, SelectedWidth, SelectedHeight );
    }
    if( (ResolutionSelection == -1) && (m_pResolution->GetItemCount() > 0) )
    {
        ResolutionSelection = 0;
    }
    m_pResolution->SetSelection( ResolutionSelection );

    AddPresentModeIfSupported( m_pPresentMode, ENG_PRESENT_VSYNC,     "IDS_DISPLAY_PRESENT_MODE_VSYNC" );
    AddPresentModeIfSupported( m_pPresentMode, ENG_PRESENT_MAILBOX,   "IDS_DISPLAY_PRESENT_MODE_MAILBOX" );
    AddPresentModeIfSupported( m_pPresentMode, ENG_PRESENT_IMMEDIATE, "IDS_DISPLAY_PRESENT_MODE_IMMEDIATE" );

    ASSERT( m_pPresentMode->GetItemCount() > 0 );
    s32 PresentModeSelection = m_pPresentMode->FindItemByData( PendingSettings.GetPresentMode() );
    if( PresentModeSelection == -1 )
    {
        x_DebugMsg( "Display settings: saved present mode is unsupported by the current window; selecting VSync\n" );
        PresentModeSelection = m_pPresentMode->FindItemByData( ENG_PRESENT_VSYNC );
    }
    m_pPresentMode->SetSelection( PresentModeSelection );

    m_pFrameLimit->AddItem( g_StringTableMgr( "ui", "IDS_DISPLAY_FRAME_LIMIT_AUTO" ),
                            static_cast<s32>( FrameRateLimit::Auto ) );
    m_pFrameLimit->AddItem( xwstring( "30"  ), static_cast<s32>( FrameRateLimit::Fps30  ) );
    m_pFrameLimit->AddItem( xwstring( "60"  ), static_cast<s32>( FrameRateLimit::Fps60  ) );
    m_pFrameLimit->AddItem( xwstring( "90"  ), static_cast<s32>( FrameRateLimit::Fps90  ) );
    m_pFrameLimit->AddItem( xwstring( "120" ), static_cast<s32>( FrameRateLimit::Fps120 ) );
    m_pFrameLimit->AddItem( xwstring( "144" ), static_cast<s32>( FrameRateLimit::Fps144 ) );
    m_pFrameLimit->AddItem( xwstring( "165" ), static_cast<s32>( FrameRateLimit::Fps165 ) );
    m_pFrameLimit->AddItem( xwstring( "240" ), static_cast<s32>( FrameRateLimit::Fps240 ) );

    s32 FrameLimitSelection = m_pFrameLimit->FindItemByData(
        static_cast<s32>( PendingSettings.GetFrameRateLimit() ) );
    if( FrameLimitSelection == -1 )
    {
        FrameLimitSelection = 0;
    }
    m_pFrameLimit->SetSelection( FrameLimitSelection );

    m_pDisplayModeText->SetLabel( g_StringTableMgr( "ui", "IDS_DISPLAY_MODE" ) );
    m_pResolutionText ->SetLabel( g_StringTableMgr( "ui", "IDS_DISPLAY_RESOLUTION" ) );
    m_pPresentModeText->SetLabel( g_StringTableMgr( "ui", "IDS_DISPLAY_PRESENT_MODE" ) );
    m_pFrameLimitText ->SetLabel( g_StringTableMgr( "ui", "IDS_DISPLAY_FRAME_LIMIT" ) );
    m_pBrightnessText ->SetLabel( g_StringTableMgr( "ui", "IDS_OPTIONS_BRIGHTNESS" ) );
    m_pContrastText   ->SetLabel( g_StringTableMgr( "ui", "IDS_OPTIONS_CONTRAST" ) );

    m_pDisplayModeText->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pResolutionText ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pPresentModeText->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pFrameLimitText ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pBrightnessText ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pContrastText   ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pButtonApply    ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( (ui_control*)m_pDisplayMode );
        m_CurrentControl = IDC_DISPLAY_MODE;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    m_pDisplayMode    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pResolution     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pPresentMode    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFrameLimit     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pBrightness     ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pContrast       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pButtonApply    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pDisplayModeText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pResolutionText ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pPresentModeText->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pFrameLimitText ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pBrightnessText ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pContrastText   ->SetFlag( ui_win::WF_VISIBLE, FALSE );

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

void dlg_display_settings::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_display_settings::Render( s32 ox, s32 oy )
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

void dlg_display_settings::OnAccept( ui_win* pWin )
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

void dlg_display_settings::OnCancel( ui_win* pWin )
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

void dlg_display_settings::ApplySettings( global_settings& Settings )
{
    if( m_pDisplayMode->GetSelection() != -1 )
    {
        Settings.SetDisplayMode( static_cast<eng_display_mode>( m_pDisplayMode->GetSelectedItemData() ) );
    }

    s32 Width  = 0;
    s32 Height = 0;
    if( GetSelectedResolution( m_pResolution, Width, Height ) )
    {
        Settings.SetDisplayResolution( Width, Height );
    }

    if( m_pPresentMode->GetSelection() != -1 )
    {
        Settings.SetPresentMode( static_cast<eng_present_mode>( m_pPresentMode->GetSelectedItemData() ) );
    }

    if( m_pFrameLimit->GetSelection() != -1 )
    {
        Settings.SetFrameRateLimit( static_cast<FrameRateLimit>( m_pFrameLimit->GetSelectedItemData() ) );
    }

    Settings.SetBrightness( m_pBrightness->GetValue() );
    Settings.SetContrast( m_pContrast->GetValue() );

}

//==============================================================================

void dlg_display_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_display_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_display_settings::OpenSavePopup( void )
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

void dlg_display_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitGraphics();
}

//==============================================================================

void dlg_display_settings::OnSaveSettingsCB( void )
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

void dlg_display_settings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Select_Norm" );
        global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
        PendingSettings.Reset( RESET_DISPLAY );

        m_pDisplayMode->SetSelection(
            m_pDisplayMode->FindItemByData( PendingSettings.GetDisplayMode() ) );

        s32 CurrentWidth  = 0;
        s32 CurrentHeight = 0;
        CurrentWidth  = PendingSettings.GetDisplayWidth();
        CurrentHeight = PendingSettings.GetDisplayHeight();
        if( (CurrentWidth <= 0) || (CurrentHeight <= 0) )
        {
            eng_GetRes( CurrentWidth, CurrentHeight );
        }
        m_pResolution->SetSelection( FindResolutionSelection( m_pResolution, CurrentWidth, CurrentHeight ) );

        m_pPresentMode->SetSelection(
            m_pPresentMode->FindItemByData( PendingSettings.GetPresentMode() ) );
        m_pFrameLimit->SetSelection(
            m_pFrameLimit->FindItemByData( static_cast<s32>( PendingSettings.GetFrameRateLimit() ) ) );
        m_pBrightness->SetValue( PendingSettings.GetBrightness() );
        m_pContrast->SetValue( PendingSettings.GetContrast() );
    }
}

//==============================================================================

void dlg_display_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
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
        m_pDisplayMode    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pResolution     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pPresentMode    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFrameLimit     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pBrightness     ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pContrast       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonApply    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pDisplayModeText->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pResolutionText ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pPresentModeText->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pFrameLimitText ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pBrightnessText ->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pContrastText   ->SetFlag( ui_win::WF_VISIBLE, TRUE );

        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( (ui_control*)m_pDisplayMode );
            m_CurrentControl = IDC_DISPLAY_MODE;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
        g_UiMgr->SetScreenHighlight( m_pDisplayModeText->GetPosition() );
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    xbool const IsBorderless = (m_pDisplayMode->GetSelection() != -1) &&
                               (m_pDisplayMode->GetSelectedItemData() == ENG_DISPLAY_BORDERLESS);
    if( IsBorderless && (m_pResolution->GetItemCount() > 0) )
    {
        m_pResolution->SetSelection( m_pResolution->GetItemCount() - 1 );
    }
    m_pResolution    ->SetFlag( ui_win::WF_DISABLED, IsBorderless );
    m_pResolutionText->SetFlag( ui_win::WF_DISABLED, IsBorderless );

    xbool const IsImmediate = (m_pPresentMode->GetSelection() != -1) &&
                              (m_pPresentMode->GetSelectedItemData() == ENG_PRESENT_IMMEDIATE);
    if( !IsImmediate )
    {
        m_pFrameLimit->SetSelection( 0 );
    }
    m_pFrameLimit    ->SetFlag( ui_win::WF_DISABLED, !IsImmediate );
    m_pFrameLimitText->SetFlag( ui_win::WF_DISABLED, !IsImmediate );

    if( m_pDisplayMode->IsFocused() )
    {
        highLight = 0;
        m_pDisplayModeText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pDisplayModeText->GetPosition() );
    }
    else
    {
        m_pDisplayModeText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pResolution->IsFocused() )
    {
        highLight = 1;
        m_pResolutionText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pResolutionText->GetPosition() );
    }
    else
    {
        m_pResolutionText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pPresentMode->IsFocused() )
    {
        highLight = 2;
        m_pPresentModeText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pPresentModeText->GetPosition() );
    }
    else
    {
        m_pPresentModeText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pFrameLimit->IsFocused() )
    {
        highLight = 3;
        m_pFrameLimitText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pFrameLimitText->GetPosition() );
    }
    else
    {
        m_pFrameLimitText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pBrightness->IsFocused() )
    {
        highLight = 4;
        m_pBrightnessText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pBrightnessText->GetPosition() );
    }
    else
    {
        m_pBrightnessText->SetLabelColor( xcolor(126,220,60,255) );
    }

    if( m_pContrast->IsFocused() )
    {
        highLight = 5;
        m_pContrastText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pContrastText->GetPosition() );
    }
    else
    {
        m_pContrastText->SetLabelColor( xcolor(126,220,60,255) );
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
