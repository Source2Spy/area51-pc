//=========================================================================
//
//  dlg_ReportError.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_text.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_listbox.hpp"

#include "dlg_ReportError.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"
#include "AudioMgr/AudioMgr.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================
ui_manager::dialog_tem ReportErrorDialog =
{
    "IDS_NULL",
    1, 9,
    0,
    NULL,
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_report_error_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "report error", &ReportErrorDialog, &dlg_report_error_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_report_error_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_report_error* pDialog = new dlg_report_error;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_report_error
//=========================================================================

dlg_report_error::dlg_report_error( void )
{
}

//=========================================================================

dlg_report_error::~dlg_report_error( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_report_error::Create( s32                        UserID,
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

    // find controls

    // hide them

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_OK" ));
   
    SetNavText( navText );

    // set initial focus
    m_PopUp = NULL;

    // disable highlight
    g_UiMgr->DisableScreenHighlight();
    m_Position = Position;

    // initialize screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;


    // Return success code
    return Success;
}

//=========================================================================

void dlg_report_error::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_report_error::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect   rb;
    rb = g_UiMgr->GetUserBounds( m_UserID );
    g_UiMgr->RenderGouraudRect( rb, xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),
                                    xcolor(0,0,0,180), FALSE);
    
    
    // render transparent screen
    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;

    g_UiMgr->RenderGouraudRect( rb, xcolor(56,115,58,64),
                                    xcolor(56,115,58,64),
                                    xcolor(56,115,58,64),
                                    xcolor(56,115,58,64), FALSE);


    // render the screen bars
    s32 y = rb.t + offset;    

    while ( y < rb.b )
    {
        irect bar;

        if ( ( y + width ) > rb.b )
        {
            bar.Set( rb.l, y, rb.r, rb.b );
        }
        else
        {
            bar.Set( rb.l, y, rb.r, ( y + width ) );
        }

        // draw the bar
        g_UiMgr->RenderGouraudRect( bar, xcolor(56,115,58,30),
                                         xcolor(56,115,58,30),
                                         xcolor(56,115,58,30),
                                         xcolor(56,115,58,30), FALSE);

        y += gap;
    }
    
    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_report_error::OnAccept( ui_win* pWin )
{
    (void)pWin;
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        //g_AudioMgr.Play( "OptionBack" );
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_report_error::OnCancel( ui_win* pWin )
{
    (void)pWin;

}

//=========================================================================

void dlg_report_error::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    MEMORY_OWNER("dlg_report_error::OnUpdate");
    (void)pWin;
    (void)DeltaTime;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {            

            irect r = g_UiMgr->GetUserBounds( g_UiUserID );

            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, 
                ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

            r.Set( 0, 0, 300, 200 );

            const char* pTitle = "IDS_NETWORK_POPUP";
            const char* pLabel = NULL;

            switch( g_ActiveConfig.GetExitReason() )
            {
            case GAME_EXIT_NETWORK_DOWN:
            {
                interface_info Info;
                net_GetInterfaceInfo( -1, Info );
                if( Info.IsAvailable )
                {
                    pLabel = "IDS_ONLINE_NETWORK_DOWN";
                }
                else
                {
                    pLabel = "IDS_ONLINE_CHECK_CABLE";
                }
                break;
            }
            case GAME_EXIT_DUPLICATE_LOGIN:     pLabel = "IDS_ONLINE_DUPLICATE_LOGIN";          break;
            case GAME_EXIT_PLAYER_KICKED:       pLabel = "IDS_ONLINE_KICKED";                   break;
            case GAME_EXIT_PLAYER_DROPPED:      pLabel = "IDS_ONLINE_DROPPED";                  break;
            case GAME_EXIT_SERVER_SHUTDOWN:     pLabel = "IDS_ONLINE_SERVER_SHUTDOWN";          break;
            case GAME_EXIT_SERVER_BUSY:         pLabel = "IDS_ONLINE_DROPPED";                  break;
            case GAME_EXIT_CONNECTION_LOST:     pLabel = "IDS_ONLINE_CONNECTION_LOST";          break;
            case GAME_EXIT_INVALID_MISSION:     pLabel = "IDS_ONLINE_INVALID_MISSION";          break;
            case GAME_EXIT_INVALID_CAMPAIGN_MISSION:
            {
                pTitle = "IDS_LOAD_CAMPAIGN_MENU";
                pLabel = "IDS_CAMPAIGN_INVALID_MISSION";
            }
            break;
            case GAME_EXIT_SERVER_FULL:         pLabel = "IDS_ONLINE_LOGIN_SERVER_FULL";        break;
            case GAME_EXIT_BAD_PASSWORD:        pLabel = "IDS_ONLINE_LOGIN_BAD_PASSWORD";       break;
            case GAME_EXIT_CANNOT_CONNECT:      pLabel = "IDS_ONLINE_LOGIN_CANNOT_CONNECT";     break;
            case GAME_EXIT_LOGIN_REFUSED:       pLabel = "IDS_ONLINE_LOGIN_REFUSED";            break;
            case GAME_EXIT_CLIENT_BANNED:       pLabel = "IDS_ONLINE_LOGIN_BANNED";             break;
            case GAME_EXIT_SESSION_ENDED:       pLabel = "IDS_ONLINE_SESSION_ENDED";            break;
            default:                            ASSERT( FALSE );                                break;
            }
            m_PopUp->ConfigureMessage( r,
                                       g_StringTableMgr( "ui", pTitle ),
                                       g_StringTableMgr( "ui", pLabel ),
                                       &m_PopUpResult );
            g_UiMgr->SetScreenOn(TRUE);
        }
    }
    else
    {
        if( m_PopUpResult != DLG_POPUP_IDLE )
        {
            m_State = DIALOG_STATE_BACK;
            g_UiMgr->EndDialog( g_UiUserID, TRUE );
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    // update everything else
    ui_dialog::OnUpdate( pWin, DeltaTime );

}
