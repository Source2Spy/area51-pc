//=========================================================================
//
//  dlg_campaign_menu.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_button.hpp"

#include "dlg_PopUp.hpp"

#include "dlg_CampaignMenu.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "StateMgr/MapList.hpp"

#include "../../Apps/GameApp/Config.hpp"    

#include "Configuration/GameConfig.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================

ui_manager::control_tem CampaignMenuControls[] = 
{
    { IDC_CAMPAIGN_MENU_NEW_CAMPAIGN,       "IDS_CAMPAIGN_MENU_NEW_CAMPAIGN",   "button",   80,  60, 120, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CAMPAIGN_MENU_RESUME_CAMPAIGN,    "IDS_CAMPAIGN_MENU_RESUME_CAMPAIGN","button",   80, 100, 120, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CAMPAIGN_MENU_EDIT_PROFILE,       "IDS_CAMPAIGN_MENU_EDIT_PROFILE",   "button",   80, 140, 120, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CAMPAIGN_MENU_LORE,               "IDS_CAMPAIGN_MENU_LORE",           "button",   80, 180, 120, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CAMPAIGN_MENU_SECRETS,            "IDS_CAMPAIGN_MENU_SECRETS",        "button",   80, 220, 120, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CAMPAIGN_MENU_EXTRAS,             "IDS_CAMPAIGN_MENU_EXTRAS",         "button",   80, 260, 120, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },
#ifndef CONFIG_RETAIL
    { IDC_CAMPAIGN_MENU_LEVEL_SELECT,       "IDS_CAMPAIGN_MENU_LEVEL_SELECT",   "button",   80, 300, 120, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },
#endif
}; 

ui_manager::dialog_tem CampaignMenuDialog =
{
    "IDS_CAMPAIGN_MENU",
    1, 9,
    sizeof(CampaignMenuControls)/sizeof(ui_manager::control_tem),
    &CampaignMenuControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_campaign_menu_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "campaign menu", &CampaignMenuDialog, &dlg_campaign_menu_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_campaign_menu_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_campaign_menu* pDialog = new dlg_campaign_menu;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_campaign_menu
//=========================================================================

dlg_campaign_menu::dlg_campaign_menu( void )
{
}

//=========================================================================

dlg_campaign_menu::~dlg_campaign_menu( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_campaign_menu::Create( s32                        UserID,
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

    m_pButtonNewCampaign    = (ui_button*)  FindChildByID( IDC_CAMPAIGN_MENU_NEW_CAMPAIGN  );
    m_pButtonResumeCampaign = (ui_button*)  FindChildByID( IDC_CAMPAIGN_MENU_RESUME_CAMPAIGN );
    m_pButtonEditProfile    = (ui_button*)  FindChildByID( IDC_CAMPAIGN_MENU_EDIT_PROFILE  );
    m_pButtonLore           = (ui_button*)  FindChildByID( IDC_CAMPAIGN_MENU_LORE          );
    m_pButtonSecrets        = (ui_button*)  FindChildByID( IDC_CAMPAIGN_MENU_SECRETS       );
    m_pButtonExtras         = (ui_button*)  FindChildByID( IDC_CAMPAIGN_MENU_EXTRAS        );
#ifndef CONFIG_RETAIL
    m_pButtonLevelSelect    = (ui_button*)  FindChildByID( IDC_CAMPAIGN_MENU_LEVEL_SELECT  );
#endif

    m_CurrHL            = 0;
    m_PopUp             = NULL;
    m_PopUpResult       = DLG_POPUP_IDLE;
    m_bCheckKeySequence = FALSE;

    // switch off the buttons to start
    m_pButtonNewCampaign    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonResumeCampaign ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonEditProfile    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonLore           ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonSecrets        ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonExtras         ->SetFlag(ui_win::WF_VISIBLE, FALSE);
#ifndef CONFIG_RETAIL
    m_pButtonLevelSelect    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
#endif
    // We cannot resume a campaign if we have no checkpoints
    player_profile& Profile = g_StateMgr.GetActiveProfile(0);
    xbool DisableCheckpoints=TRUE;

    s32 i;
    for( i=0; i<MAX_SAVED_LEVELS; i++ )
    {
        level_check_points& Checkpoint = Profile.GetCheckpoint(i);

        if( Checkpoint.MapID != -1 )
        {
            DisableCheckpoints = FALSE;
            break;
        }
    }

    m_pButtonResumeCampaign->SetFlag( ui_win::WF_DISABLED, DisableCheckpoints );

#ifdef OPM_REVIEW_BUILD
    // no lore or secrets for OPM review
    m_pButtonLore    ->SetFlag( ui_win::WF_DISABLED, TRUE );
    m_pButtonSecrets ->SetFlag( ui_win::WF_DISABLED, TRUE );
#else

#if 0 //defined (mbillington) || (jhowa) || (ksaffel) || (sbroumley)
#else
    xbool bFoundLore = FALSE;
    s32 m=0;
    while ( ( m < g_MapList.GetCount() ) && !bFoundLore )
    {
        const map_entry& Entry = *g_MapList.GetByIndex( m );
        s32 MapID = Entry.GetMapID();

        // check if of the correct game type
        if( ( Entry.GetGameType() == GAME_CAMPAIGN ) && ( MapID < 2000 ) )
        {
            // look up the vault by the mapID
            s32 VaultIndex;
            g_LoreList.GetVaultByMapID( MapID, VaultIndex );

            // see if we unlocked ANYTHING in this vault
            if( Profile.GetLoreAcquired( VaultIndex, -1 ) )
            {
                // found some lore!
                bFoundLore = TRUE;
            }
        }

        // check next map
        m++;
    }


    // check if any lore or secrets are unlocked
    if( !bFoundLore )
    {
        // no lore, therefore no secrets either!
        m_pButtonLore    ->SetFlag( ui_win::WF_DISABLED, TRUE );
        m_pButtonSecrets ->SetFlag( ui_win::WF_DISABLED, TRUE );
    }
    else
    {
        // check for new lore unlocked
        if( Profile.IsNewLoreUnlocked() )
        {
            m_pButtonLore->EnablePulse();
        }

        // some lore is unlocked, check for secrets
        if( Profile.GetNumSecretsUnlocked() == 0 )
        {
            m_pButtonSecrets ->SetFlag( ui_win::WF_DISABLED, TRUE );
        }
        else if( Profile.IsNewSecretUnlocked() )
        {
            m_pButtonSecrets->EnablePulse();
        }
    }
#endif

#endif // OPM



    s32 iControl = g_StateMgr.GetCurrentControl();
    if( (iControl == -1) || (GotoControl( iControl )==NULL) )
    {
        if( DisableCheckpoints )
        {
            GotoControl( (ui_control*)m_pButtonNewCampaign );
            m_CurrentControl =  IDC_CAMPAIGN_MENU_NEW_CAMPAIGN;
        }
        else
        {
            GotoControl( (ui_control*)m_pButtonResumeCampaign );
            m_CurrentControl =  IDC_CAMPAIGN_MENU_RESUME_CAMPAIGN;
        }
    }
    else
    {
        m_CurrentControl = iControl;
    }


    // set up nav text 
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );

    SetNavText( xwstring(navText) );

    // set the number of players to 1
    g_PendingConfig.SetPlayerCount( 1 );

    if( !(CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT) )
    {
        // clear in-use controller flags
        for( s32 i=0; i<MAX_LOCAL_PLAYERS; i++)
        {
            g_StateMgr.SetControllerRequested(i, FALSE);
        }
    }

    // initialize the screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_campaign_menu::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_campaign_menu::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;
    
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
  
    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();

}

//=========================================================================

void dlg_campaign_menu::OnAccept( ui_win* pWin )
{
    if( g_UiMgr->IsScreenScaling() )
        return;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonNewCampaign )
        {
            g_StateMgr.SetActiveControllerID( g_UiMgr->GetActiveController() );
            g_StateMgr.SetControllerRequested( g_UiMgr->GetActiveController(), TRUE );

            player_profile& Profile = g_StateMgr.GetActiveProfile(0);

            // check if we beat the campaign on hard (or we have no checkpoints)
            if( Profile.GetAlienAvatarsOn() || ( m_pButtonResumeCampaign->GetFlags() & ui_win::WF_DISABLED ) )
            {
                // store the active controller
                m_CurrentControl =  IDC_CAMPAIGN_MENU_NEW_CAMPAIGN;
            }
            else
            {
                // warn the player that their checkpoint are about to be reset!
                irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

                // set nav text
                xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
                navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
                SetNavTextVisible( FALSE );

                // configure message
                m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_CAMPAIGN_MENU_NEW_CAMPAIGN" ), 
                    TRUE, 
                    TRUE, 
                    FALSE, 
                    g_StringTableMgr( "ui", "IDS_RESET_CHECKPOINTS_MSG" ),
                    navText,
                    &m_PopUpResult );

                return;
            }
        }
        else if( pWin == (ui_win*)m_pButtonResumeCampaign )
        {
            m_CurrentControl = IDC_CAMPAIGN_MENU_RESUME_CAMPAIGN;
        }
        else if( pWin == (ui_win*)m_pButtonEditProfile )
        {
            g_StateMgr.InitPendingProfile( 0 );
            m_CurrentControl = IDC_CAMPAIGN_MENU_EDIT_PROFILE;
        }
        else if( pWin == (ui_win*)m_pButtonLore )
        {
            m_CurrentControl = IDC_CAMPAIGN_MENU_LORE;
        }
        else if( pWin == (ui_win*)m_pButtonSecrets )
        {
            m_CurrentControl = IDC_CAMPAIGN_MENU_SECRETS;
        }
        else if( pWin == (ui_win*)m_pButtonExtras )
        {
            m_CurrentControl = IDC_CAMPAIGN_MENU_EXTRAS;
        }
#ifndef CONFIG_RETAIL
        else if( pWin == (ui_win*)m_pButtonLevelSelect )
        {
            m_CurrentControl = IDC_CAMPAIGN_MENU_LEVEL_SELECT;
        }
#endif
        m_State = DIALOG_STATE_SELECT;
        g_AudioMgr.Play("Select_Norm");
    }
}

//=========================================================================

void dlg_campaign_menu::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( g_UiMgr->IsScreenScaling() )
        return;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        m_State = DIALOG_STATE_BACK;
        g_AudioMgr.Play("Backup");
    }
}

//=========================================================================

void dlg_campaign_menu::OnUpdate ( ui_win* pWin, f32 DeltaTime )
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
            m_pButtonNewCampaign    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonResumeCampaign ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonEditProfile    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonLore           ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonSecrets        ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonExtras         ->SetFlag(ui_win::WF_VISIBLE, TRUE);
#ifndef CONFIG_RETAIL
            m_pButtonLevelSelect    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
#endif


            s32 iControl = g_StateMgr.GetCurrentControl();
            if( (iControl == -1) || (GotoControl( iControl )==NULL) )
            {
                if( m_pButtonResumeCampaign->GetFlags() & ui_win::WF_DISABLED )
                {
                    GotoControl( (ui_control*)m_pButtonNewCampaign );
                    g_UiMgr->SetScreenHighlight( m_pButtonNewCampaign->GetPosition() );
                    m_CurrentControl = IDC_CAMPAIGN_MENU_NEW_CAMPAIGN;
                }
                else
                {
                    GotoControl( (ui_control*)m_pButtonResumeCampaign );
                    g_UiMgr->SetScreenHighlight( m_pButtonResumeCampaign->GetPosition() );
                    m_CurrentControl = IDC_CAMPAIGN_MENU_RESUME_CAMPAIGN;
                }
            }
            else
            {
                ui_control* pControl = GotoControl( iControl );
                ASSERT( pControl );
                g_UiMgr->SetScreenHighlight(pControl->GetPosition() );
                m_CurrentControl = iControl;
            }
        }
    }

    if( m_PopUp )
    {
        switch( m_PopUpResult )
        {
            case DLG_POPUP_IDLE:
                break;

            case DLG_POPUP_YES:
            {
                // clear the checkpoints
                player_profile& Profile = g_StateMgr.GetActiveProfile(0);
                for( s32 i=0; i<MAX_SAVED_LEVELS; i++ )
                {
                    // reinitialize checkpoint data
                    Profile.GetCheckpoint(i).Init( -1 );
                }
                m_CurrentControl =  IDC_CAMPAIGN_MENU_NEW_CAMPAIGN;
                m_State = DIALOG_STATE_SELECT;
                m_PopUp = NULL;
                return;
            }

            default:
                // Abort new campaign
                m_PopUp = NULL;
               
                // set controllers back to all
                if( g_StateMgr.GetActiveControllerID() != -1 )
                {
                    g_StateMgr.SetControllerRequested( g_StateMgr.GetActiveControllerID(), FALSE );
                    g_StateMgr.SetActiveControllerID( -1 );
                }

                break;
        }
    }

    // update button pulse
    m_pButtonLore    ->OnUpdate( pWin, DeltaTime );
    m_pButtonSecrets ->OnUpdate( pWin, DeltaTime );

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    if( m_pButtonNewCampaign->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonNewCampaign->GetPosition() );
        highLight = 0;
    }
    else if( m_pButtonResumeCampaign->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonResumeCampaign->GetPosition() );
        highLight = 1;
    }
    else if( m_pButtonEditProfile->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonEditProfile->GetPosition() );
        highLight = 2;
    }
    else if( m_pButtonLore->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonLore->GetPosition() );
        highLight = 3;
    }
    else if( m_pButtonSecrets->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonSecrets->GetPosition() );
        highLight = 4;
    }
    else if( m_pButtonExtras->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonExtras->GetPosition() );
        highLight = 5;
    }
#ifndef CONFIG_RETAIL
    else if( m_pButtonLevelSelect->IsFocused() )
    {
        g_UiMgr->SetScreenHighlight( m_pButtonLevelSelect->GetPosition() );
        highLight = 6;
    }
#endif

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }
}

//=========================================================================
