//=========================================================================
//
//  dlg_profile_av.cpp
//
//=========================================================================

#include "entropy.hpp"

#include "ui\ui_font.hpp"
#include "ui\ui_manager.hpp"
#include "ui\ui_control.hpp"
#include "ui\ui_combo.hpp"
#include "ui\ui_button.hpp"

#include "dlg_ProfileAV.hpp"
#include "stringmgr\stringmgr.hpp"
#include "StateMgr\StateMgr.hpp"

//=========================================================================
//  Main Options Dialog
//=========================================================================

enum controls
{
    IDC_AV_VOLUME_SFX_TEXT,
    IDC_AV_VOLUME_MUSIC_TEXT,
    IDC_AV_VOLUME_SPEECH_TEXT,

    IDC_AV_VOLUME_SFX,
    IDC_AV_VOLUME_MUSIC,
    IDC_AV_VOLUME_SPEECH,

    IDC_AV_HEADSET_TEST,
    IDC_AV_RESTORE_DEFAULTS,

    IDC_AV_NAV_TEXT,
};

//-------------------------------------------------------------------------

ui_manager::control_tem ProfileAVControls[] = 
{
    // Frames.
    { IDC_AV_VOLUME_SFX_TEXT,       "IDS_OPTIONS_SFX_VOLUME",       "text",      40,  40, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_MUSIC_TEXT,     "IDS_OPTIONS_MUSIC_VOLUME",     "text",      40,  75, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_SPEECH_TEXT,    "IDS_OPTIONS_SPEECH_VOLUME",    "text",      40, 110, 220, 40, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },

    { IDC_AV_VOLUME_SFX,            "IDS_NULL",                     "slider",   320,  40, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_MUSIC,          "IDS_NULL",                     "slider",   320,  75, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_VOLUME_SPEECH,         "IDS_NULL",                     "slider",   320, 110, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
	
    { IDC_AV_HEADSET_TEST,          "IDS_OPTIONS_HEADSET_TEST",     "button",    40, 180, 220, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
    { IDC_AV_RESTORE_DEFAULTS,      "IDS_OPTIONS_RESTORE_DEFAULTS", "button",    40, 285, 220, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },

    { IDC_AV_NAV_TEXT,              "IDS_NULL",                     "text",       0,   0,   0,  0, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE },
};

//-------------------------------------------------------------------------

ui_manager::dialog_tem ProfileAVDialog =
{
    "IDS_PROFILE_AV",
    1, 9,
    sizeof(ProfileAVControls)/sizeof(ui_manager::control_tem),
    &ProfileAVControls[0],
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

//=========================================================================
//  Registration function
//=========================================================================

void dlg_profile_av_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "profile av", &ProfileAVDialog, &dlg_profile_av_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_profile_av_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_profile_av* pDialog = new dlg_profile_av;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_profile_av
//=========================================================================

dlg_profile_av::dlg_profile_av( void )
{
}

//=========================================================================

dlg_profile_av::~dlg_profile_av( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_profile_av::Create( s32                        UserID,
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
    m_pRestoreDefaults          = (ui_button*)  FindChildByID( IDC_AV_RESTORE_DEFAULTS     );

    m_pVolumeSFXText	        = (ui_text*)    FindChildByID( IDC_AV_VOLUME_SFX_TEXT      );
    m_pVolumeMusicText	        = (ui_text*)    FindChildByID( IDC_AV_VOLUME_MUSIC_TEXT    );
    m_pVolumeSpeechText	        = (ui_text*)    FindChildByID( IDC_AV_VOLUME_SPEECH_TEXT   );
    m_pNavText                  = (ui_text*)    FindChildByID( IDC_AV_NAV_TEXT             );

    GotoControl( (ui_control*)m_pVolumeSFX );
    m_CurrentControl = IDC_AV_VOLUME_SFX;
    m_CurrHL = 0;

    // set range
    m_pVolumeSFX    ->SetRange( 0, 100 );
    m_pVolumeMusic  ->SetRange( 0, 100 );
    m_pVolumeSpeech ->SetRange( 0, 100 );

    // switch off the controls to start
    m_pVolumeSFX                ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeMusic              ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeSpeech             ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pHeadsetTest              ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pRestoreDefaults          ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_pVolumeSFXText            ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeMusicText          ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pVolumeSpeechText         ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pNavText                  ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_pVolumeSFXText            ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pVolumeMusicText          ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pVolumeSpeechText         ->SetLabelFlags( ui_font::h_left|ui_font::v_center );

    // set button alignment
    m_pHeadsetTest              ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pRestoreDefaults          ->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
  
    m_pNavText->SetLabel( navText );
    m_pNavText->SetLabelFlags( ui_font::h_center|ui_font::v_top|ui_font::is_help_text );
    m_pNavText->UseSmallText(TRUE);

    // set default values from pending settings
    global_settings& Settings = g_StateMgr.GetPendingSettings();

    m_pVolumeSFX    ->SetValue( Settings.GetVolume( VOLUME_SFX    ) );
    m_pVolumeMusic  ->SetValue( Settings.GetVolume( VOLUME_MUSIC  ) );
    m_pVolumeSpeech ->SetValue( Settings.GetVolume( VOLUME_SPEECH ) );

    // initialize screen scaling
    InitScreenScaling( Position );

    // disable background filter
    m_bRenderBlackout = FALSE;

    m_pHeadsetTest->SetFlag( ui_win::WF_DISABLED, TRUE );

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

void dlg_profile_av::EnableHeadset( void )
{ 
    m_pHeadsetTest->SetFlag( ui_win::WF_DISABLED, FALSE );
}

//=========================================================================

void dlg_profile_av::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_profile_av::Render( s32 ox, s32 oy )
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

void dlg_profile_av::OnNotify( ui_win* pWin, ui_win* pSender, s32 Command, void* pData )
{
    (void)pWin;
    (void)pSender;
    (void)Command;
    (void)pData;

    static s32 SpeechVoiceID;

    // set default values (should be set from options data)
    global_settings& Settings = g_StateMgr.GetPendingSettings();
   
    switch (Command)
    {    
        case WN_SLIDER_CHANGE:
        {
            if ( pSender == (ui_win*)m_pVolumeSFX )
            {
                if( m_pVolumeSFX->GetValue() != Settings.GetVolume( VOLUME_SFX ) )
                {
                    Settings.SetVolume( VOLUME_SFX, m_pVolumeSFX->GetValue() );
                    Settings.CommitAudio();
                    g_AudioMgr.Play("Fader_SFX");
                }
            }
            else if ( pSender == (ui_win*)m_pVolumeMusic )
            {
                if( m_pVolumeMusic->GetValue() != Settings.GetVolume( VOLUME_MUSIC ) )
                {
                    Settings.SetVolume( VOLUME_MUSIC, m_pVolumeMusic->GetValue() );
                    Settings.CommitAudio();
                    g_AudioMgr.Play("Music_Slider");
                }
            }
            else if ( pSender == (ui_win*)m_pVolumeSpeech )
            {
                if( m_pVolumeSpeech->GetValue() != Settings.GetVolume( VOLUME_SPEECH ) )
                {
                    g_AudioMgr.SetVolume( SpeechVoiceID, (f32)Settings.GetVolume( VOLUME_SPEECH ) / 100.0f );
                    Settings.SetVolume( VOLUME_SPEECH, m_pVolumeSpeech->GetValue() );
                    Settings.CommitAudio();
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

void dlg_profile_av::OnPadSelect( ui_win* pWin )
{
    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pHeadsetTest )
        {
            // goto headset test screen
            g_AudioMgr.Play("Select_Norm");
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pRestoreDefaults )
        {
            g_AudioMgr.Play("Select_Norm");
            
            // get the pending settings
            global_settings& Settings = g_StateMgr.GetPendingSettings();

            // restore default AV settings
            Settings.Reset( RESET_AUDIO );

            // update the fader volumes
            Settings.CommitAudio();

            // update controls with changes
            m_pVolumeSFX    ->SetValue( Settings.GetVolume( VOLUME_SFX    ) );
            m_pVolumeMusic  ->SetValue( Settings.GetVolume( VOLUME_MUSIC  ) );
            m_pVolumeSpeech ->SetValue( Settings.GetVolume( VOLUME_SPEECH ) );
        }
    }
}

//=========================================================================

void dlg_profile_av::OnPadBack( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_profile_av::OnUpdate ( ui_win* pWin, f32 DeltaTime )
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
            m_pRestoreDefaults          ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            m_pVolumeSFXText            ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVolumeMusicText          ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pVolumeSpeechText         ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pNavText                  ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            if( m_pHeadsetTest->GetFlags( ui_win::WF_DISABLED ) == FALSE )
            {
                m_pHeadsetTest          ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            }

            GotoControl( (ui_control*)m_pVolumeSFX );
            m_pVolumeSFX->SetFlag(WF_HIGHLIGHT, TRUE);        
            g_UiMgr->SetScreenHighlight( m_pVolumeSFXText->GetPosition() );
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

    if( m_pRestoreDefaults->GetFlags(WF_HIGHLIGHT) )
    {
        highLight = 6;
        g_UiMgr->SetScreenHighlight( m_pRestoreDefaults->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }
}

//=========================================================================
