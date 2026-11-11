//=========================================================================
//
//  dlg_Autosave.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"

#include "dlg_Autosave.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "../../Apps/GameApp/Config.hpp"    

//=========================================================================
//  Autosave Dialog
//=========================================================================

ui_manager::dialog_tem AutosaveDialog =
{
    "IDS_AUTOSAVE_MENU",
    1, 9,
    0,
    NULL,
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_autosave_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "autosave", &AutosaveDialog, &dlg_autosave_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_autosave_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_autosave* pDialog = new dlg_autosave;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_autosave
//=========================================================================

dlg_autosave::dlg_autosave( void )
{
}

//=========================================================================

dlg_autosave::~dlg_autosave( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_autosave::Create( s32                        UserID,
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

    // intialize popup pointer
    m_PopUp = NULL;

    // initialize screen scaling
    InitScreenScaling( Position );

    // set the frame to be disabled (if coming from off screen)
    if (g_UiMgr->IsScreenOn() == FALSE)
        SetFlag( WF_DISABLED, TRUE );

    g_UiMgr->DisableScreenHighlight();

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_autosave::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_autosave::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;


    // render background filter
    rb = g_UiMgr->GetUserBounds( m_UserID );
    g_UiMgr->RenderGouraudRect(rb, xcolor(0,0,0,180),
                                   xcolor(0,0,0,180),
                                   xcolor(0,0,0,180),
                                   xcolor(0,0,0,180),FALSE);
    
    // render the screen (if we're correctly sized)
    if (g_UiMgr->IsScreenOn())
    {
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
    }

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_autosave::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;


    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            if (g_UiMgr->IsScreenOn() == FALSE)
            {
                // enable the frame
                SetFlag( WF_DISABLED, FALSE );
                g_UiMgr->SetScreenOn(TRUE);
            }

            // open a popup
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_RETRY" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL_AUTOSAVE" );

            // set message
            xwstring messageText;
            if( g_StateMgr.GetProfileNotSaved( 0 ) )
            {
                // could not save because we haven't selected a profile on a memory card/hdd                
                messageText = g_StringTableMgr( "ui", "IDS_AUTOSAVE_FAILED_SELECT_PROFILE" );
            }
            else
            {
                messageText = g_StringTableMgr( "ui", "IDS_SAVE_DATA_ERROR" );
            }

            if( x_GetLocale() == XL_LANG_ENGLISH )
            {
                r.Set( 0, 0, 270, 200 );
            }
            else
            {
                r.Set( 0, 0, 340, 220 );
            }

            // configure message
            m_PopUp->ConfigureAutosaveDialog( 
                r,
                g_StringTableMgr( "ui", "IDS_AUTOSAVE_FAILED_HEADER" ), 
                messageText,
                navText,
                &m_PopUpResult );
    
            m_PopUp->EnableBlackout( FALSE );

            return;
        }
    }

    // check popup dialog
    if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            if ( m_PopUpResult == DLG_POPUP_YES )
            {
                // retry - goto select profile screen
                m_State = DIALOG_STATE_SELECT;
            }
            else
            {
                // continue without saving
                m_State = DIALOG_STATE_BACK;
            }

            // clear popup 
            m_PopUp = NULL;
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);
}

//=========================================================================
