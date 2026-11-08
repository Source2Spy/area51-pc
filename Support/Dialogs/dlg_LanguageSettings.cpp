//==============================================================================
//
//  dlg_LanguageSettings.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_text.hpp"

#include "dlg_LanguageSettings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum language_settings_controls
{
    IDC_LANGUAGE_TEXT_TEXT,
    IDC_LANGUAGE_AUDIO_TEXT,
    IDC_LANGUAGE_VIDEO_TEXT,

    IDC_LANGUAGE_TEXT,
    IDC_LANGUAGE_AUDIO,
    IDC_LANGUAGE_VIDEO,
    IDC_LANGUAGE_BUTTON_APPLY,
};

struct language_item
{
    x_language  Language;
    const char* pStringID;
    xbool       bTextOnly;
};

//==============================================================================
//  DATA
//==============================================================================

static language_item const s_LanguageItems[] =
{
    { XL_LANG_ENGLISH,  "IDS_LANGUAGE_ENGLISH",  FALSE },
    { XL_LANG_FRENCH,   "IDS_LANGUAGE_FRENCH",   FALSE },
    { XL_LANG_GERMAN,   "IDS_LANGUAGE_GERMAN",   FALSE },
    { XL_LANG_ITALIAN,  "IDS_LANGUAGE_ITALIAN",  FALSE },
    { XL_LANG_SPANISH,  "IDS_LANGUAGE_SPANISH",  FALSE },
    { XL_LANG_RUSSIAN,  "IDS_LANGUAGE_RUSSIAN",  FALSE },
    { XL_LANG_CZECH,    "IDS_LANGUAGE_CZECH",    TRUE  },
    { XL_LANG_JAPANESE, "IDS_LANGUAGE_JAPANESE", TRUE  },
};

//------------------------------------------------------------------------------

ui_manager::control_tem LanguageSettingsControls[] =
{
    { IDC_LANGUAGE_TEXT_TEXT,    "IDS_LANGUAGE_TEXT",  "text",   40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_LANGUAGE_AUDIO_TEXT,   "IDS_LANGUAGE_AUDIO", "text",   40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_LANGUAGE_VIDEO_TEXT,   "IDS_LANGUAGE_VIDEO", "text",   40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_LANGUAGE_TEXT,         "IDS_NULL",           "combo", 290,  40, 140, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_LANGUAGE_AUDIO,        "IDS_NULL",           "combo", 290,  75, 140, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_LANGUAGE_VIDEO,        "IDS_NULL",           "combo", 290, 110, 140, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_LANGUAGE_BUTTON_APPLY, "IDS_PROFILE_OPTIONS_ACCEPT", "button", 40, 285, 220, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
};

//------------------------------------------------------------------------------

ui_manager::dialog_tem LanguageSettingsDialog =
{
    "IDS_OPTIONS_LANGUAGE_SETTINGS",
    1, 7,
    sizeof(LanguageSettingsControls) / sizeof(ui_manager::control_tem),
    &LanguageSettingsControls[0],
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_language_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "language settings", &LanguageSettingsDialog, &dlg_language_settings_factory );
}

//==============================================================================

ui_win* dlg_language_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_language_settings* pDialog = new dlg_language_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return (ui_win*)pDialog;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_language_settings::dlg_language_settings( void )
{
}

//==============================================================================

dlg_language_settings::~dlg_language_settings( void )
{
    Destroy();
}

//==============================================================================

void dlg_language_settings::PopulateLanguageCombo( ui_combo* pCombo, x_language Language, xbool bIncludeTextOnly )
{
    pCombo->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );

    for( s32 i = 0; i < (s32)(sizeof(s_LanguageItems) / sizeof(language_item)); i++ )
    {
        if( s_LanguageItems[i].bTextOnly && !bIncludeTextOnly )
        {
            continue;
        }

        pCombo->AddItem( g_StringTableMgr( "ui", s_LanguageItems[i].pStringID ),
                         static_cast<uaddr>( s_LanguageItems[i].Language ) );
    }

    s32 Selection = pCombo->FindItemByData( static_cast<uaddr>( Language ) );
    if( Selection == -1 )
    {
        Selection = pCombo->FindItemByData( static_cast<uaddr>( XL_LANG_ENGLISH ) );
    }
    pCombo->SetSelection( Selection );
}

//==============================================================================

x_language dlg_language_settings::GetSelectedLanguage( ui_combo* pCombo )
{
    if( pCombo->GetSelection() == -1 )
    {
        return XL_LANG_ENGLISH;
    }

    return static_cast<x_language>( pCombo->GetSelectedItemData() );
}

//==============================================================================

xbool dlg_language_settings::Create( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    (void)pUserData;
    ASSERT( pManager );

    xbool const Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_pTextLanguage      = (ui_combo*)FindChildByID( IDC_LANGUAGE_TEXT );
    m_pAudioLanguage     = (ui_combo*)FindChildByID( IDC_LANGUAGE_AUDIO );
    m_pVideoLanguage     = (ui_combo*)FindChildByID( IDC_LANGUAGE_VIDEO );
    m_pButtonApply       = (ui_button*)FindChildByID( IDC_LANGUAGE_BUTTON_APPLY );
    m_pTextLanguageText  = (ui_text*)FindChildByID( IDC_LANGUAGE_TEXT_TEXT );
    m_pAudioLanguageText = (ui_text*)FindChildByID( IDC_LANGUAGE_AUDIO_TEXT );
    m_pVideoLanguageText = (ui_text*)FindChildByID( IDC_LANGUAGE_VIDEO_TEXT );

    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    m_OriginalSettings = PendingSettings;
    PopulateLanguageCombo( m_pTextLanguage, PendingSettings.GetTextLanguage(), TRUE );
    PopulateLanguageCombo( m_pAudioLanguage, PendingSettings.GetAudioLanguage(), FALSE );
    PopulateLanguageCombo( m_pVideoLanguage, PendingSettings.GetVideoLanguage(), FALSE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( (ui_control*)m_pTextLanguage );
        m_CurrentControl = IDC_LANGUAGE_TEXT;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    ui_combo* pCombos[] = { m_pTextLanguage, m_pAudioLanguage, m_pVideoLanguage };
    ui_text* pLabels[] = { m_pTextLanguageText, m_pAudioLanguageText, m_pVideoLanguageText };
    for( s32 i = 0; i < 3; i++ )
    {
        pCombos[i]->SetFlag( ui_win::WF_VISIBLE, FALSE );
        pLabels[i]->SetLabelFlags( ui_font::h_left | ui_font::v_center );
        pLabels[i]->SetFlag( ui_win::WF_VISIBLE, FALSE );
    }

    m_pButtonApply->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pButtonApply->SetFlag( ui_win::WF_VISIBLE, FALSE );

    xwstring NavText( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) );
    NavText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    NavText += g_StringTableMgr( "ui", "IDS_NAV_RESTORE_DEFAULTS" );
    SetNavText( NavText );

    m_CurrHL = 0;
    m_bRenderBlackout = FALSE;
    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;

    InitScreenScaling( Position );
    m_State = DIALOG_STATE_ACTIVE;
    return Success;
}

//==============================================================================

void dlg_language_settings::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_language_settings::Render( s32 ox, s32 oy )
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

void dlg_language_settings::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || (pWin != (ui_win*)m_pButtonApply) )
    {
        return;
    }

    global_settings Settings = m_OriginalSettings;

    if( !ApplySettings( Settings ) )
    {
        return;
    }

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

void dlg_language_settings::OnCancel( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        global_settings Settings = m_OriginalSettings;

        if( !ApplySettings( Settings ) )
        {
            return;
        }

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

xbool dlg_language_settings::ApplySettings( global_settings& Settings )
{
    x_language Language = GetSelectedLanguage( m_pTextLanguage );

    if( Language != x_GetLocale() && !g_StringTableMgr.ValidateLocalizedTables( Language ) )
    {
        m_pTextLanguage->SetSelection( m_pTextLanguage->FindItemByData( static_cast<uaddr>( x_GetLocale() ) ) );
        g_AudioMgr.Play( "Backup" );
        return FALSE;
    }

    Settings.SetTextLanguage ( Language );
    Settings.SetAudioLanguage( GetSelectedLanguage( m_pAudioLanguage ) );
    Settings.SetVideoLanguage( GetSelectedLanguage( m_pVideoLanguage ) );
    return TRUE;
}

//==============================================================================

void dlg_language_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_language_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_language_settings::OpenSavePopup( void )
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

void dlg_language_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitLocalization();
}

//==============================================================================

void dlg_language_settings::OnSaveSettingsCB( void )
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

void dlg_language_settings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        x_language const DefaultLanguage = global_settings::GetDefaultLocalizationLanguage();
        m_pTextLanguage ->SetSelection( m_pTextLanguage ->FindItemByData( static_cast<uaddr>( DefaultLanguage ) ) );
        m_pAudioLanguage->SetSelection( m_pAudioLanguage->FindItemByData( static_cast<uaddr>( DefaultLanguage ) ) );
        m_pVideoLanguage->SetSelection( m_pVideoLanguage->FindItemByData( static_cast<uaddr>( DefaultLanguage ) ) );
        g_AudioMgr.Play( "Select_Norm" );
    }
}

//==============================================================================

void dlg_language_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
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
            RestoreSettings();
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        ui_win* pControls[] =
        {
            m_pTextLanguage, m_pAudioLanguage, m_pVideoLanguage, m_pButtonApply,
            m_pTextLanguageText, m_pAudioLanguageText, m_pVideoLanguageText,
        };
        for( s32 i = 0; i < 7; i++ )
        {
            pControls[i]->SetFlag( ui_win::WF_VISIBLE, TRUE );
        }
        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( (ui_control*)m_pTextLanguage );
            m_CurrentControl = IDC_LANGUAGE_TEXT;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    ui_combo* pCombos[] = { m_pTextLanguage, m_pAudioLanguage, m_pVideoLanguage };
    ui_text* pLabels[] = { m_pTextLanguageText, m_pAudioLanguageText, m_pVideoLanguageText };
    for( s32 i = 0; i < 3; i++ )
    {
        xbool const Focused = pCombos[i]->IsFocused();
        pLabels[i]->SetLabelColor( Focused ? xcolor(255,252,204,255) : xcolor(126,220,60,255) );
        if( Focused )
        {
            Highlight = i;
            g_UiMgr->SetScreenHighlight( pLabels[i]->GetPosition() );
        }
    }

    if( m_pButtonApply->IsFocused() )
    {
        Highlight = 3;
        g_UiMgr->SetScreenHighlight( m_pButtonApply->GetPosition() );
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
