//==============================================================================
//
//  dlg_AudioSettings.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_slider.hpp"
#include "UI/ui_text.hpp"

#include "dlg_AudioSettings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum audio_settings_controls
{
    IDC_AUDIO_VOLUME_SFX_TEXT,
    IDC_AUDIO_VOLUME_MUSIC_TEXT,
    IDC_AUDIO_VOLUME_SPEECH_TEXT,
    IDC_AUDIO_VOLUME_VIDEO_TEXT,

    IDC_AUDIO_VOLUME_SFX,
    IDC_AUDIO_VOLUME_MUSIC,
    IDC_AUDIO_VOLUME_SPEECH,
    IDC_AUDIO_VOLUME_VIDEO,
    IDC_AUDIO_BUTTON_APPLY,
};

//==============================================================================
//  DATA
//==============================================================================

ui_manager::control_tem AudioSettingsControls[] =
{
    { IDC_AUDIO_VOLUME_SFX_TEXT,    "IDS_OPTIONS_SFX_VOLUME",    "text",    40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_AUDIO_VOLUME_MUSIC_TEXT,  "IDS_OPTIONS_MUSIC_VOLUME",  "text",    40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_AUDIO_VOLUME_SPEECH_TEXT, "IDS_OPTIONS_SPEECH_VOLUME", "text",    40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_AUDIO_VOLUME_VIDEO_TEXT,  "IDS_OPTIONS_VIDEO_VOLUME",  "text",    40, 145, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_AUDIO_VOLUME_SFX,         "IDS_NULL",                  "slider", 300,  40, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_AUDIO_VOLUME_MUSIC,       "IDS_NULL",                  "slider", 300,  75, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_AUDIO_VOLUME_SPEECH,      "IDS_NULL",                  "slider", 300, 110, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_AUDIO_VOLUME_VIDEO,       "IDS_NULL",                  "slider", 300, 145, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_AUDIO_BUTTON_APPLY,       "IDS_PROFILE_OPTIONS_ACCEPT", "button", 40, 285, 220, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
};

//------------------------------------------------------------------------------

ui_manager::dialog_tem AudioSettingsDialog =
{
    "IDS_OPTIONS_AUDIO_SETTINGS",
    1, 9,
    sizeof(AudioSettingsControls) / sizeof(ui_manager::control_tem),
    &AudioSettingsControls[0],
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_audio_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "audio settings", &AudioSettingsDialog, &dlg_audio_settings_factory );
}

//==============================================================================

ui_win* dlg_audio_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_audio_settings* pDialog = new dlg_audio_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return pDialog;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_audio_settings::dlg_audio_settings( void )
{
}

//==============================================================================

dlg_audio_settings::~dlg_audio_settings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_audio_settings::Create( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    (void)pUserData;
    ASSERT( pManager );

    xbool const Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_pVolumeSFX        = static_cast<ui_slider*>( FindChildByID( IDC_AUDIO_VOLUME_SFX ) );
    m_pVolumeMusic      = static_cast<ui_slider*>( FindChildByID( IDC_AUDIO_VOLUME_MUSIC ) );
    m_pVolumeSpeech     = static_cast<ui_slider*>( FindChildByID( IDC_AUDIO_VOLUME_SPEECH ) );
    m_pVolumeVideo      = static_cast<ui_slider*>( FindChildByID( IDC_AUDIO_VOLUME_VIDEO ) );
    m_pButtonApply      = static_cast<ui_button*>( FindChildByID( IDC_AUDIO_BUTTON_APPLY ) );
    m_pVolumeSFXText    = static_cast<ui_text*>( FindChildByID( IDC_AUDIO_VOLUME_SFX_TEXT ) );
    m_pVolumeMusicText  = static_cast<ui_text*>( FindChildByID( IDC_AUDIO_VOLUME_MUSIC_TEXT ) );
    m_pVolumeSpeechText = static_cast<ui_text*>( FindChildByID( IDC_AUDIO_VOLUME_SPEECH_TEXT ) );
    m_pVolumeVideoText  = static_cast<ui_text*>( FindChildByID( IDC_AUDIO_VOLUME_VIDEO_TEXT ) );

    m_Settings = g_StateMgr.GetPendingSettings();
    m_OriginalSettings = m_Settings;

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( m_pVolumeSFX );
        m_CurrentControl = IDC_AUDIO_VOLUME_SFX;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    ui_slider* pSliders[] = { m_pVolumeSFX, m_pVolumeMusic, m_pVolumeSpeech, m_pVolumeVideo };
    for( s32 i = 0; i < 4; i++ )
    {
        pSliders[i]->SetRange( VOLUME_MIN_PERCENT, VOLUME_MAX_PERCENT );
        pSliders[i]->UseDefaultSound( FALSE );
        pSliders[i]->SetFlag( ui_win::WF_VISIBLE, FALSE );
    }

    m_pVolumeSFX   ->SetValue( m_Settings.GetVolume( VOLUME_SFX ) );
    m_pVolumeMusic ->SetValue( m_Settings.GetVolume( VOLUME_MUSIC ) );
    m_pVolumeSpeech->SetValue( m_Settings.GetVolume( VOLUME_SPEECH ) );
    m_pVolumeVideo ->SetValue( m_Settings.GetVideoVolume() );

    ui_text* pLabels[] = { m_pVolumeSFXText, m_pVolumeMusicText, m_pVolumeSpeechText, m_pVolumeVideoText };
    for( s32 i = 0; i < 4; i++ )
    {
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

void dlg_audio_settings::Destroy( void )
{
    // Cancel outstanding save callbacks before destroying the dialog.
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_audio_settings::Render( s32 ox, s32 oy )
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

void dlg_audio_settings::CommitPreview( void )
{
    // Preview the volume changes without updating pending settings.
    m_Settings.CommitAudio();
}

//==============================================================================

void dlg_audio_settings::OnNotify( ui_notification const& Event )
{
    if( (Event.m_Type != ui_notification_type::SliderChanged) || (m_State == DIALOG_STATE_INIT) )
    {
        return;
    }

    static s32 SpeechVoiceID = 0;

    if( Event.m_pSender == m_pVolumeSFX )
    {
        m_Settings.SetVolume( VOLUME_SFX, m_pVolumeSFX->GetValue() );
        CommitPreview();
        g_AudioMgr.Play( "Fader_SFX" );
    }
    else if( Event.m_pSender == m_pVolumeMusic )
    {
        m_Settings.SetVolume( VOLUME_MUSIC, m_pVolumeMusic->GetValue() );
        CommitPreview();
        g_AudioMgr.Play( "Music_Slider" );
    }
    else if( Event.m_pSender == m_pVolumeSpeech )
    {
        g_AudioMgr.SetVolume( SpeechVoiceID, static_cast<f32>( m_Settings.GetVolume( VOLUME_SPEECH ) ) / 100.0f );
        m_Settings.SetVolume( VOLUME_SPEECH, m_pVolumeSpeech->GetValue() );
        CommitPreview();
        g_AudioMgr.Release( SpeechVoiceID, 1.0f );
        SpeechVoiceID = g_AudioMgr.Play( "Voice_Slider" );
    }
    else if( Event.m_pSender == m_pVolumeVideo )
    {
        m_Settings.SetVideoVolume( m_pVolumeVideo->GetValue() );
        CommitPreview();
    }
}

//==============================================================================

void dlg_audio_settings::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || (pWin != m_pButtonApply) )
    {
        return;
    }

    if( !m_Settings.HasChanged() )
    {
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    ApplySettings();
    g_AudioMgr.Play( "Select_Norm" );
    OpenSavePopup();
}

//==============================================================================

void dlg_audio_settings::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State != DIALOG_STATE_ACTIVE )
    {
        return;
    }

    if( !m_Settings.HasChanged() )
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    OpenSavePopup();
}

//==============================================================================

void dlg_audio_settings::ApplySettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings.SetVolume( VOLUME_SFX, m_Settings.GetVolume( VOLUME_SFX ) );
    PendingSettings.SetVolume( VOLUME_MUSIC, m_Settings.GetVolume( VOLUME_MUSIC ) );
    PendingSettings.SetVolume( VOLUME_SPEECH, m_Settings.GetVolume( VOLUME_SPEECH ) );
    PendingSettings.SetVideoVolume( m_Settings.GetVideoVolume() );
    PendingSettings.CommitAudio();
}

//==============================================================================

void dlg_audio_settings::BeginSave( void )
{
    g_SaveDataMgr.SaveSettings( this, &dlg_audio_settings::OnSaveSettingsCB );
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_audio_settings::OpenSavePopup( void )
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

void dlg_audio_settings::RestoreSettings( void )
{
    global_settings& PendingSettings = g_StateMgr.GetPendingSettings();
    PendingSettings = m_OriginalSettings;
    PendingSettings.CommitAudio();
}

//==============================================================================

void dlg_audio_settings::OnSaveSettingsCB( void )
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

void dlg_audio_settings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        m_Settings.Reset( RESET_AUDIO );
        CommitPreview();
        m_pVolumeSFX   ->SetValue( m_Settings.GetVolume( VOLUME_SFX ) );
        m_pVolumeMusic ->SetValue( m_Settings.GetVolume( VOLUME_MUSIC ) );
        m_pVolumeSpeech->SetValue( m_Settings.GetVolume( VOLUME_SPEECH ) );
        m_pVolumeVideo ->SetValue( m_Settings.GetVideoVolume() );
        g_AudioMgr.Play( "Select_Norm" );
    }
}

//==============================================================================

void dlg_audio_settings::OnUpdate( ui_win* pWin, f32 DeltaTime )
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
            ApplySettings();
            BeginSave();
        }
        else
        {
            RestoreSettings();
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );
    ui_slider* pSliders[] = { m_pVolumeSFX, m_pVolumeMusic, m_pVolumeSpeech, m_pVolumeVideo };
    ui_text* pLabels[] = { m_pVolumeSFXText, m_pVolumeMusicText, m_pVolumeSpeechText, m_pVolumeVideoText };
    for( s32 i = 0; i < 4; i++ )
    {
        xbool const Focused = pSliders[i]->IsFocused();
        pLabels[i]->SetLabelColor( Focused ? xcolor(255,252,204,255) : xcolor(126,220,60,255) );
        if( Focused )
        {
            Highlight = i;
            g_UiMgr->SetScreenHighlight( pLabels[i]->GetPosition() );
        }
    }

    if( m_pButtonApply->IsFocused() )
    {
        Highlight = 4;
        g_UiMgr->SetScreenHighlight( m_pButtonApply->GetPosition() );
    }

    // Show the controls only after the opening animation finishes.
    if( g_UiMgr->IsScreenScaling() && (UpdateScreenScaling( DeltaTime ) == FALSE) )
    {
        ui_win* pControls[] =
        {
            m_pVolumeSFX, m_pVolumeMusic, m_pVolumeSpeech, m_pVolumeVideo, m_pButtonApply,
            m_pVolumeSFXText, m_pVolumeMusicText, m_pVolumeSpeechText, m_pVolumeVideoText,
        };
        for( s32 i = 0; i < 9; i++ )
        {
            pControls[i]->SetFlag( ui_win::WF_VISIBLE, TRUE );
        }
        s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( m_pVolumeSFX );
            m_CurrentControl = IDC_AUDIO_VOLUME_SFX;
        }
        else
        {
            m_CurrentControl = CurrentControl;
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
