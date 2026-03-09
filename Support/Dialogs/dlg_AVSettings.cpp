//=========================================================================
//
//  dlg_av_settings.cpp
//
//=========================================================================

#include "entropy.hpp"

#include "ui\ui_font.hpp"
#include "ui\ui_manager.hpp"
#include "ui\ui_control.hpp"
#include "ui\ui_combo.hpp"
#include "ui\ui_button.hpp"

#include "dlg_AVSettings.hpp"
#include "stringmgr\stringmgr.hpp"
#include "StateMgr\StateMgr.hpp"
#include "MemCardMgr/MemCardMgr.hpp"

//=========================================================================
//  Main Options Dialog
//=========================================================================

enum
{
    POPUP_CONFIRM_SETTINGS,
    POPUP_OVERWRITE_SETTINGS
};

//-------------------------------------------------------------------------

ui_manager::control_tem AVSettingsControls[] = 
{
    // Frames.
    { IDC_AV_VOLUME_SFX_TEXT,       "IDS_OPTIONS_SFX_VOLUME",        "text",      40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_MUSIC_TEXT,     "IDS_OPTIONS_MUSIC_VOLUME",      "text",      40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_SPEECH_TEXT,    "IDS_OPTIONS_SPEECH_VOLUME",     "text",      40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
																	 
    { IDC_AV_VOLUME_SFX,            "IDS_NULL",                      "slider",   320,  40, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_MUSIC,          "IDS_NULL",                      "slider",   320,  75, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_SPEECH,         "IDS_NULL",                      "slider",   320, 110, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },

    { IDC_AV_HEADSET_TEST,          "IDS_OPTIONS_HEADSET_TEST",      "button",    40, 180, 220, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_GRAPHICS_MENU,         "IDS_OPTIONS_GRAPHICS_SETTINGS", "button",    40, 225, 220, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },

    { IDC_AV_BUTTON_ACCEPT,         "IDS_PROFILE_OPTIONS_ACCEPT",    "button",    40, 285, 220, 40, 0, 8, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },

    { IDC_AV_NAV_TEXT,              "IDS_NULL",                      "text",       0,   0,   0,  0, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
};

//-------------------------------------------------------------------------

ui_manager::dialog_tem AVSettingsDialog =
{
    "IDS_AV_SETTINGS",
    1, 9,
    sizeof(AVSettingsControls)/sizeof(ui_manager::control_tem),
    &AVSettingsControls[0],
    0
};

//=========================================================================
//  Defines
//=========================================================================

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

static s32 CurrSettingsSlot = -1;

//=========================================================================
//  Registration function
//=========================================================================

void dlg_av_settings_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "av settings", &AVSettingsDialog, &dlg_av_settings_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_av_settings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_av_settings* pDialog = new dlg_av_settings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_av_settings
//=========================================================================

dlg_av_settings::dlg_av_settings( void )
{
}

//=========================================================================

dlg_av_settings::~dlg_av_settings( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_av_settings::Create( s32                        UserID,
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

    m_pVolumeSFX                = (ui_slider*)  FindChildByID( IDC_AV_VOLUME_SFX           );    
    m_pVolumeMusic	            = (ui_slider*)  FindChildByID( IDC_AV_VOLUME_MUSIC         );
    m_pVolumeSpeech	            = (ui_slider*)  FindChildByID( IDC_AV_VOLUME_SPEECH        );
    m_pHeadsetTest              = (ui_button*)  FindChildByID( IDC_AV_HEADSET_TEST         );
    m_pGraphicsMenu             = (ui_button*)  FindChildByID( IDC_AV_GRAPHICS_MENU        );	
    m_pButtonAccept             = (ui_button*)  FindChildByID( IDC_AV_BUTTON_ACCEPT        );

    m_pVolumeSFXText	        = (ui_text*)    FindChildByID( IDC_AV_VOLUME_SFX_TEXT      );
    m_pVolumeMusicText	        = (ui_text*)    FindChildByID( IDC_AV_VOLUME_MUSIC_TEXT    );
    m_pVolumeSpeechText	        = (ui_text*)    FindChildByID( IDC_AV_VOLUME_SPEECH_TEXT   );
    m_pNavText                  = (ui_text*)    FindChildByID( IDC_AV_NAV_TEXT             );

    GotoControl( (ui_control*)m_pVolumeSFX );
    m_CurrentControl = IDC_AV_VOLUME_SFX;
    m_CurrHL = 0;
    m_PopUp = NULL;

    // set range
    m_pVolumeSFX    ->SetRange( 0, 100 );
    m_pVolumeMusic  ->SetRange( 0, 100 );
    m_pVolumeSpeech ->SetRange( 0, 100 );

    // switch off the controls to start
    m_pVolumeSFX                ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeMusic              ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeSpeech             ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pHeadsetTest              ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pGraphicsMenu             ->SetFlag( ui_win::WF_VISIBLE, FALSE );	
    m_pButtonAccept             ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_pVolumeSFXText            ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeMusicText          ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeSpeechText         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pNavText                  ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_pVolumeSFXText            ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pVolumeMusicText          ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pVolumeSpeechText         ->SetLabelFlags( ui_font::h_left|ui_font::v_center );

#ifdef X_RETAIL
    // Turn it off for retail, for now.
    m_pGraphicsMenu             ->SetFlag(ui_win::WF_DISABLED, TRUE);
#endif

    // set button alignment
    m_pHeadsetTest              ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pGraphicsMenu             ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );	
    m_pButtonAccept             ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    navText += g_StringTableMgr( "ui", "IDS_NAV_RESTORE_DEFAULTS" );
  
    m_pNavText->SetLabel( navText );
    m_pNavText->SetLabelFlags( ui_font::h_center|ui_font::v_top|ui_font::is_help_text );
    m_pNavText->UseSmallText(TRUE);

    // set default values from pending settings
    m_Settings = g_StateMgr.GetPendingSettings();

    m_pVolumeSFX    ->SetValue( m_Settings.GetVolume( VOLUME_SFX    ) );
    m_pVolumeMusic  ->SetValue( m_Settings.GetVolume( VOLUME_MUSIC  ) );
    m_pVolumeSpeech ->SetValue( m_Settings.GetVolume( VOLUME_SPEECH ) );

    // initialize screen scaling
    InitScreenScaling( Position );

    // disable background filter
    m_bRenderBlackout = FALSE;

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // turn off default slider sounds
    m_pVolumeSFX    ->UseDefaultSound(FALSE);
    m_pVolumeMusic  ->UseDefaultSound(FALSE);
    m_pVolumeSpeech ->UseDefaultSound(FALSE);

    // Return success code
    return Success;
}

//=========================================================================

void dlg_av_settings::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_av_settings::Render( s32 ox, s32 oy )
{
    static s32 offset   =  0;
    static s32 gap      =  9;
    static s32 width    =  4;

	irect rb;
    
    if( m_bRenderBlackout )
    {
	    s32 XRes, YRes;
        eng_GetRes(XRes, YRes);
        rb.Set( 0, 0, XRes, YRes );
        g_UiMgr->RenderGouraudRect(rb, xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),FALSE);
    }

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
    
    // increment the offset
    if (++offset > 9)
        offset = 0;

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_av_settings::OnNotify( ui_win* pWin, ui_win* pSender, s32 Command, void* pData )
{
    (void)pWin;
    (void)pSender;
    (void)Command;
    (void)pData;

    static s32 SpeechVoiceID;

    switch (Command)
    {    
        case WN_SLIDER_CHANGE:
        {
            if ( pSender == (ui_win*)m_pVolumeSFX )
            {
                if( m_pVolumeSFX->GetValue() != m_Settings.GetVolume( VOLUME_SFX ) )
                {
                    m_Settings.SetVolume( VOLUME_SFX, m_pVolumeSFX->GetValue() );
                    m_Settings.CommitAudio();
                    g_AudioMgr.Play("Fader_SFX");
                }
            }
            else if ( pSender == (ui_win*)m_pVolumeMusic )
            {
                if( m_pVolumeMusic->GetValue() != m_Settings.GetVolume( VOLUME_MUSIC ) )
                {
                    m_Settings.SetVolume( VOLUME_MUSIC, m_pVolumeMusic->GetValue() );
                    m_Settings.CommitAudio();
                    g_AudioMgr.Play("Music_Slider");
                }
            }
            else if ( pSender == (ui_win*)m_pVolumeSpeech )
            {
                if( m_pVolumeSpeech->GetValue() != m_Settings.GetVolume( VOLUME_SPEECH ) )
                {
                    g_AudioMgr.SetVolume( SpeechVoiceID, (f32)m_Settings.GetVolume( VOLUME_SPEECH ) / 100.0f );
                    m_Settings.SetVolume( VOLUME_SPEECH, m_pVolumeSpeech->GetValue() );
                    m_Settings.CommitAudio();
                    g_AudioMgr.Release( SpeechVoiceID, 1.0f );
                    SpeechVoiceID = g_AudioMgr.Play("Voice_Slider");
                }
            }
        }
        break;

        default:
            break;
    }
}

//=========================================================================

void dlg_av_settings::OnPadSelect( ui_win* pWin )
{
    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pHeadsetTest )
        {
            // get the pending settings
            global_settings& Settings = g_StateMgr.GetPendingSettings();

            // copy changes
            Settings = m_Settings;

            // update the fader volumes
            Settings.CommitAudio();

            // goto headset test screen
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_AV_HEADSET_TEST;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pGraphicsMenu )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_AV_GRAPHICS_MENU;
            m_State = DIALOG_STATE_SELECT;       
        }
        else if( pWin == (ui_win*)m_pButtonAccept )
        {
            g_AudioMgr.Play("Select_Norm");
            
            // get the pending settings
            global_settings& Settings = g_StateMgr.GetPendingSettings();

            // copy changes
            Settings = m_Settings;

            // update the fader volumes
            Settings.CommitAudio();

            // check if anything changed
            if( g_StateMgr.GetPendingSettings().HasChanged() )
            {            
                if( GameMgr.GameInProgress() && g_NetworkMgr.IsOnline() )
                {
                    // don't save whilst online

                    // update the changes in the profile
                    g_StateMgr.ActivatePendingSettings(); 
                    // apply the settings
                    global_settings& Settings = g_StateMgr.GetActiveSettings();
                    Settings.Commit();
                    // Mark Dirty so we prompt to save when we exit the game
                    Settings.MarkDirty();

                    // return to pause menu
                    m_State = DIALOG_STATE_BACK;
                }
                else
                {
                    // ask to save

                    // changes have been made - prompt to save
                    irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL|ui_win::WF_USE_ABSOLUTE );
                    m_PopUpType = POPUP_CONFIRM_SETTINGS;

                    // set nav text
                    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
                    navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
                    m_pNavText->SetFlag(ui_win::WF_VISIBLE, FALSE);

                    // configure message
                    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_SETTINGS_EDIT" ), 
                        TRUE, 
                        TRUE, 
                        FALSE, 
                        g_StringTableMgr( "ui", "IDS_SETTINGS_CHANGED_MSG" ),
                        navText,
                        &m_PopUpResult );

                    m_State = DIALOG_STATE_POPUP;
                    return;
                }
            }
            else
            {
                // no changes - return to previous screen
                g_AudioMgr.Play( "Backup" );
                m_State = DIALOG_STATE_BACK;
            }
        }
    }
}

//=========================================================================

void dlg_av_settings::OnPadBack( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        // no changes - return to previous screen
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_av_settings::OnPadDelete( ui_win* pWin )
{
    (void)pWin;

    // restore defaults
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play("Select_Norm");

        // get default AV settings
        m_Settings.Reset( RESET_AUDIO );

        // update the fader volumes
        m_Settings.CommitAudio();

        // update controls with changes
        m_pVolumeSFX    ->SetValue( m_Settings.GetVolume( VOLUME_SFX    ) );
        m_pVolumeMusic  ->SetValue( m_Settings.GetVolume( VOLUME_MUSIC  ) );
        m_pVolumeSpeech ->SetValue( m_Settings.GetVolume( VOLUME_SPEECH ) );
    }
}

//=========================================================================

void dlg_av_settings::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    s32 highLight = -1;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the buttons
            m_pVolumeSFX                ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVolumeMusic              ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVolumeSpeech             ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pGraphicsMenu             ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pButtonAccept             ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            m_pVolumeSFXText            ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVolumeMusicText          ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVolumeSpeechText         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pNavText                  ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            if( m_pHeadsetTest->GetFlags( ui_win::WF_DISABLED ) == FALSE )
            {
                m_pHeadsetTest          ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            }
			
            if( m_pGraphicsMenu->GetFlags( ui_win::WF_DISABLED ) == FALSE )
            {
                m_pGraphicsMenu         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            }			

            GotoControl( (ui_control*)m_pVolumeSFX );
            m_pVolumeSFX->SetFlag(WF_HIGHLIGHT, TRUE);        
            g_UiMgr->SetScreenHighlight( m_pVolumeSFXText->GetPosition() );
        }
    }

    // check for result of popup box
    if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            if( m_PopUpType == POPUP_CONFIRM_SETTINGS )
            {
                // save changes?
                if ( m_PopUpResult == DLG_POPUP_YES )
                {
                    // check if the settings are saved 
                    if( g_StateMgr.GetSettingsCardSlot() == -1 )
                    {
                        // check for corrupt settings
                        if( g_UIMemCardMgr.FoundSettings() )
                        {
                            // ask permission to overwrite old settings
                            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL|ui_win::WF_USE_ABSOLUTE );
                            m_PopUpType = POPUP_OVERWRITE_SETTINGS;

                            // set nav text
                            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
                            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
                            m_pNavText->SetFlag(ui_win::WF_VISIBLE, FALSE);

                            // configure message
                            m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_SETTINGS_EDIT" ), 
                                TRUE, 
                                TRUE, 
                                FALSE, 
                                g_StringTableMgr( "ui", "IDS_SETTINGS_CHANGED_OVERWRITE" ),
                                navText,
                                &m_PopUpResult );

                            return;
                        }
                        else
                        {
                            // attempt to create settings
                            g_StateMgr.SetSettingsCardSlot( 0 );
                            g_UIMemCardMgr.CreateSettings( this, &dlg_av_settings::OnSaveSettingsCB );
                            m_State = DIALOG_STATE_WAIT_FOR_MEMCARD;
                        }
                    }
                    else
                    {
                        // OK. attempt to save changes to memcard
                        g_AudioMgr.Play("Select_Norm");
                        CurrSettingsSlot = g_StateMgr.GetSettingsCardSlot();
                        g_UIMemCardMgr.SaveSettings(this, &dlg_av_settings::OnSaveSettingsCB );
                        m_State = DIALOG_STATE_WAIT_FOR_MEMCARD;
                    }
                }
                else
                {
                    // Activate the pending settings right now. Even though the player opted not to
                    // save, the settings should be preserved locally.
                    g_StateMgr.ActivatePendingSettings();
                    // apply settings
                    global_settings& Settings = g_StateMgr.GetActiveSettings();
                    Settings.Commit();
                    g_AudioMgr.Play("Backup");
                    m_State = DIALOG_STATE_BACK;
                }

                // clear popup 
                m_PopUp = NULL;

                // turn on nav text
                m_pNavText->SetFlag(ui_win::WF_VISIBLE, TRUE);
            }
            else
            {
                if( m_PopUpResult == DLG_POPUP_YES )
                {
                    // overwrite the old settings
                    g_AudioMgr.Play("Select_Norm");
                    g_StateMgr.SetSettingsCardSlot( 0 );
                    g_UIMemCardMgr.SaveSettings( this, &dlg_av_settings::OnSaveSettingsCB );
                    m_State = DIALOG_STATE_WAIT_FOR_MEMCARD;
                }
                else
                {
                    // Activate the pending settings right now. Even though the player opted not to
                    // save, the settings should be preserved locally.
                    g_StateMgr.ActivatePendingSettings();
                    // apply settings
                    global_settings& Settings = g_StateMgr.GetActiveSettings();
                    Settings.Commit();
                    g_AudioMgr.Play("Backup");
                    m_State = DIALOG_STATE_BACK;
                }

                // clear popup 
                m_PopUp = NULL;

                // turn on nav text
                m_pNavText->SetFlag(ui_win::WF_VISIBLE, TRUE);
            }
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    // update labels
    if( m_pVolumeSFX->GetFlags(WF_HIGHLIGHT) )
    {
        highLight = 0;
        m_pVolumeSFXText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pVolumeSFXText->GetPosition() );
    }
    else
        m_pVolumeSFXText->SetLabelColor( xcolor(126,220,60,255) );

    if( m_pVolumeMusic->GetFlags(WF_HIGHLIGHT) )
    {
        highLight = 1;
        m_pVolumeMusicText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pVolumeMusicText->GetPosition() );
    }
    else
        m_pVolumeMusicText->SetLabelColor( xcolor(126,220,60,255) );
    
    if( m_pVolumeSpeech->GetFlags(WF_HIGHLIGHT) )
    {
        highLight = 2;

        m_pVolumeSpeechText->SetLabelColor( xcolor(255,252,204,255) );
        g_UiMgr->SetScreenHighlight( m_pVolumeSpeechText->GetPosition() );
    }
    else
        m_pVolumeSpeechText->SetLabelColor( xcolor(126,220,60,255) );
       
    if( m_pHeadsetTest->GetFlags(WF_HIGHLIGHT) )
    {
        highLight = 4;
        g_UiMgr->SetScreenHighlight( m_pHeadsetTest->GetPosition() );
    }

    if( m_pGraphicsMenu->GetFlags(WF_HIGHLIGHT) )
    {
        highLight = 5;
        g_UiMgr->SetScreenHighlight( m_pGraphicsMenu->GetPosition() );
    }

    if( m_pButtonAccept->GetFlags(WF_HIGHLIGHT) )
    {
        highLight = 6;
        g_UiMgr->SetScreenHighlight( m_pButtonAccept->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }
}

//=========================================================================

void dlg_av_settings::OnSaveSettingsCB( void )
{
    // check if the save was successful (OR user wants to continue without saving)
#ifdef TARGET_PC
    MemCardMgr::condition& Condition1 = g_UIMemCardMgr.GetCondition( 0 );
#else
    s32 CheckCardSlot;
    if( g_StateMgr.GetSettingsCardSlot() == -1 )
    {
        CheckCardSlot = CurrSettingsSlot;
    }
    else
    {
        CheckCardSlot = g_StateMgr.GetSettingsCardSlot();
    }
    MemCardMgr::condition& Condition1 = g_UIMemCardMgr.GetCondition( CheckCardSlot );	
#endif
    if( Condition1.SuccessCode )
    {
        // activate the new settings
        g_StateMgr.ActivatePendingSettings();

        // continue without saving?
        if( Condition1.bCancelled )
        {
            // clear settings card slot
            g_StateMgr.SetSettingsCardSlot(-1);
        }

        // save successful - return to main menu
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;            
    }
    else
    {
        // save failed! - goto memcard select dialog
        g_AudioMgr.Play( "Select_Norm" );

        // clear settings card slot
        g_StateMgr.SetSettingsCardSlot(-1);

        // handle error state
        m_State = DIALOG_STATE_MEMCARD_ERROR;
    }
}

//=========================================================================