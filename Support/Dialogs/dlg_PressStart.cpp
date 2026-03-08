//=========================================================================
// 
// dlg_PressStart.cpp
// 
//=========================================================================

//=========================================================================
// Includes
//=========================================================================

#include "entropy.hpp"

#include "ui\ui_manager.hpp"
#include "ui\ui_button.hpp"
#include "ui\ui_bitmap.hpp"

#include "dlg_PressStart.hpp"   
#include "dlg_PopUp.hpp"
#include "stringmgr\stringmgr.hpp"
#include "ResourceMgr\ResourceMgr.hpp"
#include "memcardmgr\memcardmgr.hpp"
#include "stateMgr/StateMgr.hpp"
#include "MemCardMgr/MemCardMgr.hpp"

#include "MoviePlayer/MoviePlayer.hpp"

#ifdef TARGET_PS2
#include "ps2\ps2_misc.hpp"
#endif

extern xstring SelectBestClip( const char* pName );


//=========================================================================
//  Press Start Dialog
//=========================================================================

enum
{
    POPUP_NO_SPACE,
    POPUP_BAD_SETTINGS,
};

enum controls
{
    IDC_A51_LOGO,
    IDC_START_BOX,
	IDC_PRESS_START,
};

ui_manager::control_tem Press_StartControls[] =
{
//  { IDC_A51_LOGO,    "IDS_NULL",             "bitmap",  40,  10, 400, 100, 0, 0, 0, 0, ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE  },
    { IDC_A51_LOGO,    "IDS_NULL",             "bitmap", 280,  10, 200,  50, 0, 0, 0, 0, ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE  },
    { IDC_START_BOX,   "IDS_NULL",             "bitmap",  90, 312, 300,  30, 0, 0, 0, 0, ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE  },
    { IDC_PRESS_START, "IDS_PRESS_START_TEXT", "text",     0, 307, 480,  30, 0, 0, 1, 1, ui_win::WF_SCALE_XPOS | ui_win::WF_SCALE_XSIZE  },
};

ui_manager::dialog_tem Press_StartDialog =
{
    "IDS_PRESS_START_SCREEN",
    1, 9,
    sizeof(Press_StartControls)/sizeof(ui_manager::control_tem),
    &Press_StartControls[0],
    0
};

const f32 TIMEOUT_TIME = 60.0f;

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

void dlg_press_start_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "press start", &Press_StartDialog, &dlg_press_start_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_press_start_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_press_start* pDialog = new dlg_press_start;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_press_start
//=========================================================================

dlg_press_start::dlg_press_start( void )
{
}

//=========================================================================

dlg_press_start::~dlg_press_start( void )
{
    Destroy();
}


//=========================================================================

void dlg_press_start::DisableStartButton( void )
{
    m_State = DIALOG_STATE_TIMEOUT;
}

//=========================================================================

void dlg_press_start::EnableStartButton( void )
{
    m_WaitTime = 0.2f;
}

//=========================================================================

s32 s_PressStartVoiceID = 0;

//=========================================================================

xbool dlg_press_start::Create( s32                        UserID,
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

    m_pLogoBitmap       = (ui_bitmap*) FindChildByID( IDC_A51_LOGO    );
    m_pFrameBitmap      = (ui_bitmap*) FindChildByID( IDC_START_BOX   );
    m_pButtonPressStart	= (ui_text*)   FindChildByID( IDC_PRESS_START );

    // initialize logo bitmap
    m_pLogoBitmap->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_BitmapID = g_UiMgr->LoadBitmap( "logo",  "UI_A51_Logo.xbmp" );
    m_pLogoBitmap->SetBitmap( m_BitmapID );

    // initialize start button frame
    //m_pFrameBitmap->SetFlag(ui_win::WF_VISIBLE, TRUE);
    m_pFrameBitmap->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pFrameBitmap->SetBitmap( g_UiMgr->FindElement( "button_combo1" ), TRUE, 0 );

    // initialize start button
    m_pButtonPressStart ->SetFlag(ui_win::WF_VISIBLE, TRUE);
    m_pButtonPressStart ->SetLabelColor( xcolor(230, 230, 230, 255) );
	GotoControl( (ui_control*)m_pButtonPressStart );

    m_PressStartState   = 0;
    m_DemoHoldTimer     = 0.0f;
    m_DemoFadeAlpha     = 255.0f;
    m_FadeControl       = -1.0f;

    m_FadeStartInAlpha  = 0;
    m_FadeStartIn       = TRUE;
    m_FadeAdjust        = 4;
    m_WaitTime          = 0;
    m_Timeout           = TIMEOUT_TIME;
    m_bPlayDemo         = FALSE;
    m_PopUp             = NULL;
    m_BlocksRequired    = 0;

    // start up the start movie
    g_StateMgr.PlayMovie( "StartScreen", TRUE, TRUE );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    s_PressStartVoiceID = g_AudioMgr.Play( "MUSIC_StartScreen" );

	// Return success code
    return Success;
}

//=========================================================================

void dlg_press_start::Destroy( void )
{
    ui_dialog::Destroy();

    g_AudioMgr.Release( s_PressStartVoiceID, 0.0f );

    // unload logo bitmap
    g_UiMgr->UnloadBitmap( "logo" );
}

//=========================================================================

void dlg_press_start::Render( s32 ox, s32 oy )
{
    m_FadeStartInAlpha+=m_FadeAdjust;
    
    if( m_FadeStartInAlpha > 255 )
    {
        m_FadeStartInAlpha = 255;
        m_FadeAdjust = -m_FadeAdjust;
    }

    if( m_FadeStartInAlpha < 64 )
    {
        m_FadeStartInAlpha = 64;
        m_FadeAdjust = -m_FadeAdjust;
    }

    m_pButtonPressStart->SetLabelColor( xcolor(230, 230, 230, m_FadeStartInAlpha) );

    // finally render all the normal dialog stuff
	ui_dialog::Render( ox, oy );
}

//=========================================================================

void dlg_press_start::OnPadSelect( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        // check for movie
        if( m_bPlayDemo )
        {
            m_bPlayDemo = FALSE;
            m_Timeout = TIMEOUT_TIME;
            g_StateMgr.CloseMovie();

            // restart the start movie
            g_StateMgr.PlayMovie( "StartScreen", TRUE, TRUE );
            // set text
            m_pButtonPressStart->SetLabel( g_StringTableMgr( "ui", "IDS_PRESS_START_TEXT" ) );
            m_pLogoBitmap->SetFlag(ui_win::WF_VISIBLE, FALSE);
        }
        else
        {
            // set state
            m_State             = DIALOG_STATE_SELECT;
            m_CurrentControl    = IDC_PRESS_START;

            g_StateMgr.CloseMovie();
            g_StateMgr.PlayMovie( "MenuBackground", TRUE, TRUE );
        }
     }
}

//=========================================================================
void dlg_press_start::OnPadHelp( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        // check for movie
        if( m_bPlayDemo )
        {
            m_bPlayDemo = FALSE;
            m_Timeout = TIMEOUT_TIME;
            g_StateMgr.CloseMovie();

            // restart the start movie
            g_StateMgr.PlayMovie( "StartScreen", TRUE, TRUE );
            // set text
            m_pButtonPressStart->SetLabel( g_StringTableMgr( "ui", "IDS_PRESS_START_TEXT" ) );
            m_pLogoBitmap->SetFlag(ui_win::WF_VISIBLE, FALSE);
        }
        else
        {
            // stop the movie
            g_StateMgr.CloseMovie();

            // set state
            m_State             = DIALOG_STATE_SELECT;
            m_CurrentControl    = IDC_PRESS_START;

            g_StateMgr.PlayMovie( "MenuBackGround", TRUE, TRUE );
        }
    }
}

//=========================================================================

void dlg_press_start::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

#if defined( TARGET_PC )
    if( m_bPlayDemo )
    {
        if( !Movie.IsPlaying() )
        {
            m_bPlayDemo = FALSE;
            m_Timeout = TIMEOUT_TIME;
            g_StateMgr.CloseMovie();
            g_StateMgr.PlayMovie( "StartScreen", TRUE, TRUE );
            m_pButtonPressStart->SetLabel( g_StringTableMgr( "ui", "IDS_PRESS_START_TEXT" ) );
            m_pLogoBitmap->SetFlag(ui_win::WF_VISIBLE, FALSE);
        }
    }
    else
    {
        m_Timeout -= DeltaTime;

        if( m_Timeout < 0 )
        {
            m_bPlayDemo = TRUE;
            m_pButtonPressStart->SetLabel( g_StringTableMgr( "ui", "IDS_DEMO_MODE" ) );
            m_pLogoBitmap->SetFlag(ui_win::WF_VISIBLE, TRUE);
            g_StateMgr.CloseMovie();
            g_StateMgr.PlayMovie( "attract", FALSE, FALSE );
        }
    }
#endif
}

//=========================================================================
