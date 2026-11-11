//=========================================================================
//
//  dlg_multi_player.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_dlg_vkeyboard.hpp"

#include "dlg_MultiPlayer.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "Configuration/GameConfig.hpp"

extern xbool g_bControllerCheck;

//=========================================================================
//  Main Menu Dialog
//=========================================================================

ui_manager::control_tem MultiPlayerControls[] = 
{
    { IDC_MULTI_PLAYER_ONE_DETAILS,  "IDS_PLAYER_ONE",  "blankbox",   35,  40, 210, 200, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_MULTI_PLAYER_TWO_DETAILS,  "IDS_PLAYER_TWO",  "blankbox",  251,  40, 210, 200, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_MULTI_PLAYER_ONE_COMBO,    "IDS_NULL",        "combo",      42,  80, 196,  40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_MULTI_PLAYER_TWO_COMBO,    "IDS_NULL",        "combo",     258,  80, 196,  40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_MULTI_PLAYER_ONE_TEXT,     "IDS_NULL",        "text",       42, 120, 196,  40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_MULTI_PLAYER_TWO_TEXT,     "IDS_NULL",        "text",      258, 120, 196,  40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
};


ui_manager::dialog_tem MultiPlayerDialog =
{
    "IDS_MULTI_PLAYER",
    1, 9,
    sizeof(MultiPlayerControls)/sizeof(ui_manager::control_tem),
    &MultiPlayerControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_multi_player_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "multiplayer menu", &MultiPlayerDialog, &dlg_multi_player_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_multi_player_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_multi_player* pDialog = new dlg_multi_player;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_multi_player
//=========================================================================

dlg_multi_player::dlg_multi_player( void )
{
    m_PlayerProfileReady[0] = FALSE;
    m_PlayerProfileReady[1] = FALSE;
}

//=========================================================================

dlg_multi_player::~dlg_multi_player( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_multi_player::Create( s32                        UserID,
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

    m_pPlayerOneBox    = (ui_blankbox*)  FindChildByID( IDC_MULTI_PLAYER_ONE_DETAILS );
    m_pPlayerTwoBox    = (ui_blankbox*)  FindChildByID( IDC_MULTI_PLAYER_TWO_DETAILS );

    m_pPlayerOneCombo = (ui_combo*)  FindChildByID( IDC_MULTI_PLAYER_ONE_COMBO );
    m_pPlayerTwoCombo = (ui_combo*)  FindChildByID( IDC_MULTI_PLAYER_TWO_COMBO );

    m_pPlayerOneText = (ui_text*)  FindChildByID( IDC_MULTI_PLAYER_ONE_TEXT );
    m_pPlayerTwoText = (ui_text*)  FindChildByID( IDC_MULTI_PLAYER_TWO_TEXT );

    // initialize text
    m_pPlayerOneText->UseSmallText( TRUE );
    m_pPlayerOneText->SetFlag(ui_win::WF_VISIBLE, FALSE); 
    m_pPlayerOneText->SetLabelColor( xcolor(255,252,204,255) );
    m_pPlayerOneText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );

    m_pPlayerTwoText->UseSmallText( TRUE );
    m_pPlayerTwoText->SetFlag(ui_win::WF_VISIBLE, FALSE); 
    m_pPlayerTwoText->SetLabelColor( xcolor(255,252,204,255) );
    m_pPlayerTwoText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );


    // initialize boxes
    m_pPlayerOneBox->SetFlag(ui_win::WF_VISIBLE, FALSE); 
    m_pPlayerOneBox->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pPlayerOneBox->SetHasTitleBar( TRUE );
    m_pPlayerOneBox->SetLabelColor( xcolor(255,252,204,255) );
    m_pPlayerOneBox->SetTitleBarColor( xcolor(19,59,14,196) );
       
    m_pPlayerTwoBox->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pPlayerTwoBox->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pPlayerTwoBox->SetHasTitleBar( TRUE );
    m_pPlayerTwoBox->SetLabelColor( xcolor(255,252,204,255) );
    m_pPlayerTwoBox->SetTitleBarColor( xcolor(19,59,14,196) );

    // initialize profile combo boxes
    m_pPlayerOneCombo->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pPlayerTwoCombo->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pPlayerOneCombo->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pPlayerTwoCombo->SetFlag(ui_win::WF_VISIBLE, FALSE);

    // initialize nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
  
    SetNavText( navText );

    g_SaveDataMgr.RefreshProfiles( this, &dlg_multi_player::OnPollReturn );

    // get profile data
    RefreshProfileList();
    m_ProfileName = g_StringTableMgr("ui", "IDS_PROFILE_DEFAULT_PLAYER");
    m_ProfileEntered = FALSE;
    m_ProfileOK = FALSE;
    m_bEditProfile = FALSE;
    m_PopUp = NULL;

    // initialize screen scaling
    InitScreenScaling( Position );
    // disable the highlight
    g_UiMgr->DisableScreenHighlight();

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_multi_player::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_multi_player::Render( s32 ox, s32 oy )
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

void dlg_multi_player::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    if( m_State != DIALOG_STATE_ACTIVE )
        return;
    
    if ( g_UiMgr->GetActiveController() == 0 )
    {
        // make sure the player hasn't chosen a profile yet
        if (!m_PlayerProfileReady[0])
        {
            if ( pWin != (ui_win*)m_pPlayerOneCombo )
            {
                if ((Code == ui_navigation::Left)||(Code == ui_navigation::Right))
                {
                    // change player one profile
                    m_pPlayerOneCombo->OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
                }
            }
        }
    }
    else
    {
        // make sure the player hasn't chosen a profile yet
        if (!m_PlayerProfileReady[1])
        {
            if ( pWin != (ui_win*)m_pPlayerTwoCombo )
            {
                if ((Code == ui_navigation::Left)||(Code == ui_navigation::Right))
                {
                    // change player two profile
                    m_pPlayerTwoCombo->OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
                }
            }
        }
    }
}

//=========================================================================

void dlg_multi_player::OnAccept( ui_win* pWin )
{
    (void)pWin;      
    
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if ( g_UiMgr->GetActiveController() == 0 )
        {
            if (!m_PlayerProfileReady[0])
            {
                // check for bad profile
                if( m_pPlayerOneCombo->GetSelectedItemData( 1 ) != PROFILE_OK )
                {
                    g_AudioMgr.Play( "InvalidEntry" );
                    return;
                }

                // get the profile index from the list
                s32 index = m_pPlayerOneCombo->GetSelection();

                // check if enabled
                if( m_pPlayerOneCombo->GetSelectedItemEnabled() == FALSE )
                {
                    // not enabled - in use by another player
                    g_AudioMgr.Play( "InvalidEntry" );

                    // bail out early
                    return;
                }

                // check if this is a new profile
                if( index < m_CreateIndex )
                {
                    // clear the edit flag
                    m_bEditProfile = FALSE;

                    // init the pending profile for player 0
                    g_StateMgr.InitPendingProfile( 0 ); 

                    // get the profile name array
                    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

                    // store the id of the selected profile
                    g_StateMgr.SetSelectedProfile( 0, ProfileNames[index]->Hash );

                    // attempt to load the profile
                    g_SaveDataMgr.LoadProfile( *ProfileNames[index], 0, this, &dlg_multi_player::OnLoadProfileCB );

                    // wait for the save data request
                    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
                }
                else
                {
                    // create a new profile      
                    m_CreatePlayerIndex = 0;

                    // set the profile up with default settings
                    g_StateMgr.ResetProfile( 0 );
                    g_StateMgr.SetProfileNotSaved( 0, TRUE );

                    // init the pending profile for player 0
                    g_StateMgr.InitPendingProfile( 0 ); 

                    // clear the selected profile
                    g_StateMgr.ClearSelectedProfile( 0 );

                    // set the default profile name
                    m_ProfileName = g_StringTableMgr("ui", "IDS_PROFILE_DEFAULT_PLAYER");

                    // open a VK to enter the profile name
                    irect   r = m_pManager->GetUserBounds( m_UserID );
                    ui_dlg_vkeyboard* pVKeyboard = (ui_dlg_vkeyboard*)m_pManager->OpenDialog( m_UserID, "ui_vkeyboard", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_INPUTMODAL );
                    pVKeyboard->Configure( TRUE );
                    pVKeyboard->SetLabel( g_StringTableMgr( "ui", "IDS_PROFILE_CREATE" ) );
                    pVKeyboard->ConnectString( &m_ProfileName, SM_PROFILE_NAME_LENGTH );
                    pVKeyboard->SetReturn( &m_ProfileEntered, &m_ProfileOK );
                    m_State = DIALOG_STATE_POPUP;
                }
                // tag the controller as being in use.
                g_StateMgr.SetControllerRequested(0, TRUE);
            }
            else
            {
                // already selected
                return;
            }
        }
        else
        {
            if (!m_PlayerProfileReady[1])
            {
                // check for bad profile
                if( m_pPlayerTwoCombo->GetSelectedItemData( 1 ) != PROFILE_OK )
                {
                    g_AudioMgr.Play( "InvalidEntry" );
                    return;
                }

                // get the profile index from the list
                s32 index = m_pPlayerTwoCombo->GetSelection();

                // check if enabled
                if( m_pPlayerTwoCombo->GetSelectedItemEnabled() == FALSE )
                {
                    // not enabled - in use by another player
                    g_AudioMgr.Play( "InvalidEntry" );

                    // bail out early
                    return;
                }

                // check if this is a new profile
                if( index < m_CreateIndex )
                {
                    // clear the edit flag
                    m_bEditProfile = FALSE;

                    // init the pending profile for player 1
                    g_StateMgr.InitPendingProfile( 1 ); 

                    // get the profile name array
                    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

                    // store the id of the selected profile
                    g_StateMgr.SetSelectedProfile( 1, ProfileNames[index]->Hash );

                    // attempt to load the profile
                    g_SaveDataMgr.LoadProfile( *ProfileNames[index], 1, this, &dlg_multi_player::OnLoadProfileCB );
                    
                    // wait for the save data request
                    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
                }
                else
                {
                    // create a new profile      
                    m_CreatePlayerIndex = 1;

                    // set the profile up with default settings
                    g_StateMgr.ResetProfile( 1 );
                    g_StateMgr.SetProfileNotSaved( 1, TRUE );

                    // init the pending profile for player 1
                    g_StateMgr.InitPendingProfile( 1 ); 

                    // clear the selected profile
                    g_StateMgr.ClearSelectedProfile( 1 );

                    // set the default profile name
                    m_ProfileName = g_StringTableMgr("ui", "IDS_PROFILE_DEFAULT_PLAYER2");

                    // open a VK to enter the profile name
                    irect   r = m_pManager->GetUserBounds( m_UserID );
                    ui_dlg_vkeyboard* pVKeyboard = (ui_dlg_vkeyboard*)m_pManager->OpenDialog( m_UserID, "ui_vkeyboard", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_INPUTMODAL );
                    pVKeyboard->SetLabel( g_StringTableMgr( "ui", "IDS_PROFILE_CREATE" ) );
                    pVKeyboard->ConnectString( &m_ProfileName, SM_PROFILE_NAME_LENGTH );
                    pVKeyboard->SetReturn( &m_ProfileEntered, &m_ProfileOK );
                    m_State = DIALOG_STATE_POPUP;
                }
                // tag the controller as being in use.
                g_StateMgr.SetControllerRequested(1, TRUE);
            }
            else
            {
                // already selected
                return;
            }
        }

        // play select sound
        g_AudioMgr.Play( "Select_Norm" );
    }
}

//=========================================================================

void dlg_multi_player::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        // clear the profiles and controller requests
        // this will prevent issues associated with default profiles.
        for(s32 p=0; p < SM_MAX_PLAYERS; p++)
        {
            g_StateMgr.ClearSelectedProfile( p );
            g_StateMgr.SetControllerRequested(p, FALSE);
        }

        g_AudioMgr.Play("Backup");
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_multi_player::OnDelete( ui_win* pWin )
{
    (void)pWin;
    // deselect profile

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if ( g_UiMgr->GetActiveController() == 0 )
        {
            if (!m_PlayerProfileReady[0])
            {
                // not selected
                return;
            }
            else
            {
                // deselect player one profile
                m_PlayerProfileReady[0] = FALSE;
                m_pPlayerOneText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );

                // clear the selected profile
                g_StateMgr.ClearSelectedProfile( 0 );

                // update the profile lists
                RefreshProfileList();

                // clear controller flag
                g_StateMgr.SetControllerRequested(0, FALSE);
            }
        }
        else
        {
            if (!m_PlayerProfileReady[1])
            {
                // not selected
                return;
            }
            else
            {
                // deselect player two profile
                m_PlayerProfileReady[1] = FALSE;
                m_pPlayerTwoText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );

                // clear the selected profile
                g_StateMgr.ClearSelectedProfile( 1 );

                // update the profile lists
                RefreshProfileList();

                // clear controller flag
                g_StateMgr.SetControllerRequested(1, FALSE);
            }
        }

        // play select sound
        g_AudioMgr.Play( "Select_Norm" );
    }
}

//=========================================================================

void dlg_multi_player::OnAlternate( ui_win* pWin )
{
    (void)pWin;

    // start game!
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( m_PlayerProfileReady[0] )
        {
            if( m_PlayerProfileReady[1] )
            {
                g_AudioMgr.Play( "Select_Norm" );
                g_PendingConfig.SetPlayerCount( 2 );
                m_State = DIALOG_STATE_ACTIVATE;
                // reset the active controller
                g_StateMgr.SetActiveControllerID( -1 );
                // re-select the player's controller 
                for (s32 iPad = 0; iPad < SM_MAX_PLAYERS; iPad++ )
                {
                    if( g_StateMgr.GetSelectedProfile(iPad) != 0 )
                    {
                        g_StateMgr.SetControllerRequested(iPad, TRUE );
                    }
                }
                return;
            }
        }
    }
}

//=========================================================================

void dlg_multi_player::OnPollReturn( void )
{
    RefreshProfileList();
}

//=========================================================================
void dlg_multi_player::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    if (g_UiMgr->IsScreenScaling())
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // complete!  turn on the elements
            m_pPlayerOneBox->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pPlayerTwoBox->SetFlag( ui_win::WF_VISIBLE, TRUE );

            m_pPlayerOneCombo->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pPlayerTwoCombo->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_PlayerProfileReady[0] = FALSE;
            m_PlayerProfileReady[1] = FALSE;
    
            m_pPlayerOneText->SetFlag( ui_win::WF_VISIBLE, TRUE ); 
            m_pPlayerTwoText->SetFlag( ui_win::WF_VISIBLE, TRUE ); 
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);


    // check for profile name entry
    if( m_ProfileEntered )
    {
        m_ProfileEntered = FALSE;

        if( m_ProfileOK )
        {
            m_ProfileOK = FALSE;

            // check for duplicate name entry
            for( s32 p=0; p<m_pPlayerOneCombo->GetItemCount(); p++ )
            {
                if( x_wstrcmp( m_pPlayerOneCombo->GetItemLabel(p), m_ProfileName ) == 0 )
                {
                    // open duplicate name error popup
                    irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

                    // set nav text
                    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_OK" ));
                    SetNavTextVisible( FALSE );

                    m_PopUp->ConfigureMessage( g_StringTableMgr( "ui", "IDS_PROFILE_DUPLICATE_NAME" ),
                                               g_StringTableMgr( "ui", "IDS_PROFILE_DUPLICATE_NAME_MSG" ),
                                               &m_PopUpResult );
                    return;
                }
            }

            // store the new profile name
            g_StateMgr.InitPendingProfile( m_CreatePlayerIndex );
            player_profile& NewProfile = g_StateMgr.GetPendingProfile();
            NewProfile.SetProfileName( xstring(m_ProfileName) );

            // go to the MP profile options screen
            m_State = DIALOG_STATE_CREATE;
        }
        else
        {
            // Re-enable dialog
            m_State = DIALOG_STATE_ACTIVE;
        }
    }
    else if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            // clear popup 
            m_PopUp = NULL;

            // turn on nav text
            SetNavTextVisible( TRUE );

            // re-enable dialog
            m_State = DIALOG_STATE_ACTIVE;
        }
    }
    else
    {
        // don't update if waiting for another operation
        if( m_State != DIALOG_STATE_ACTIVE )
            return;

        // check for pulled controllers
        if( g_bControllerCheck )
        {
            for (s32 p = 0; p < SM_MAX_PLAYERS; p++)
            {
                // Has a requested controller been pulled?
                if(g_StateMgr.GetSelectedProfile(p) && !g_Input.GetFrameSnapshot().IsPresent(INPUT_PS2_QRY_PAD_PRESENT, p ))
                {
                    // deselect player profile
                    switch(p)
                    {
                    case 0:
                        m_PlayerProfileReady[0] = FALSE;
                        m_pPlayerOneText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );
                        break;
                    case 1:
                        m_PlayerProfileReady[1] = FALSE;
                        m_pPlayerTwoText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );
                        break;
                    }

                    // clear the selected profile
                    g_StateMgr.ClearSelectedProfile( p );

                    // update the profile lists
                    RefreshProfileList();

                    // clear controller flag
                    g_StateMgr.SetControllerRequested(p, FALSE);
                }
            }
        }

        // update the profile lists
        RefreshProfileList();

        // update nav text
        if( m_PlayerProfileReady[0] && m_PlayerProfileReady[1] )
        {
            // display continue option
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            navText += g_StringTableMgr( "ui", "IDS_NAV_DESELECT" );
            navText += g_StringTableMgr( "ui", "IDS_NAV_MP_CONTINUE" );
            SetNavText( navText );
        }
        else if( m_PlayerProfileReady[0] || m_PlayerProfileReady[1] )
        {
            // display profile options text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            navText += g_StringTableMgr( "ui", "IDS_NAV_DESELECT" );
            SetNavText( navText );
        }
        else
        {
            // standard options
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            SetNavText( navText );
        }
    }
}

//=========================================================================

void dlg_multi_player::RefreshProfileList()
{
    xbool Found1 = FALSE;
    xbool Found2 = FALSE;

    u32 Profile1 = g_StateMgr.GetSelectedProfile( 0 );
    u32 Profile2 = g_StateMgr.GetSelectedProfile( 1 );

    // get the profile list
    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

    // store the current selections
    s32 CurrentSelection1 = m_pPlayerOneCombo->GetSelection();
    s32 CurrentSelection2 = m_pPlayerTwoCombo->GetSelection();

    // clear the lists
    m_pPlayerOneCombo->DeleteAllItems();
    m_pPlayerTwoCombo->DeleteAllItems();

    // get the current list from the save data manager
    g_SaveDataMgr.GetProfileNames( ProfileNames );

    // fill it with the profile information
    for (s32 i=0; i<ProfileNames.GetCount(); i++)
    {
        // add the profile to the list
        if( ProfileNames[i]->bDamaged )
        {
            m_pPlayerOneCombo->AddItem( g_StringTableMgr( "ui", "IDS_CORRUPT" ), i, PROFILE_CORRUPT );
            m_pPlayerTwoCombo->AddItem( g_StringTableMgr( "ui", "IDS_CORRUPT" ), i, PROFILE_CORRUPT );
            m_pPlayerOneCombo->SetItemColor( i, XCOLOR_RED );
            m_pPlayerTwoCombo->SetItemColor( i, XCOLOR_RED );
        }
        else
        {
            m_pPlayerOneCombo->AddItem( ProfileNames[i]->Name, i );
            m_pPlayerTwoCombo->AddItem( ProfileNames[i]->Name, i );
        }

        // look for match for selected profiles
        if( Profile1 != 0 )
        {
            if( ProfileNames[i]->Hash == Profile1 )
            {
                CurrentSelection1 = i;
                m_pPlayerTwoCombo->SetItemEnabled( i, FALSE );
                Found1 = TRUE;
            }
        }
        
        if( Profile2 != 0 )
        {
            if( ProfileNames[i]->Hash == Profile2 )
            {
                CurrentSelection2 = i;
                m_pPlayerOneCombo->SetItemEnabled( i, FALSE );
                Found2 = TRUE;
            }
        }
    }

    // get the current count
    m_CreateIndex = ProfileNames.GetCount();

    // add any profiles created that aren't on the memory card
    if(  g_StateMgr.GetSelectedProfile( 0 ) && g_StateMgr.GetProfileNotSaved( 0 ) )
    {
        m_pPlayerOneCombo->AddItem( g_StateMgr.GetProfileName( 0 ), m_CreateIndex );
        m_pPlayerTwoCombo->AddItem( g_StateMgr.GetProfileName( 0 ), m_CreateIndex );
        m_pPlayerTwoCombo->SetItemEnabled( m_CreateIndex, FALSE );
        CurrentSelection1 = m_CreateIndex;
        m_CreateIndex++;
    }
    if(  g_StateMgr.GetSelectedProfile( 1 ) && g_StateMgr.GetProfileNotSaved( 1 ) )
    {
        m_pPlayerTwoCombo->AddItem( g_StateMgr.GetProfileName( 1 ), m_CreateIndex );
        m_pPlayerOneCombo->AddItem( g_StateMgr.GetProfileName( 1 ), m_CreateIndex );
        m_pPlayerOneCombo->SetItemEnabled( m_CreateIndex, FALSE );
        CurrentSelection2 = m_CreateIndex;
        m_CreateIndex++;
    }

    // add a create a profile option
    m_pPlayerOneCombo->AddItem( g_StringTableMgr("ui", "IDS_PROFILE_CREATE_NEW"), m_CreateIndex );
    m_pPlayerTwoCombo->AddItem( g_StringTableMgr("ui", "IDS_PROFILE_CREATE_NEW"), m_CreateIndex );

    // determine if profile selected
    if( g_StateMgr.GetSelectedProfile( 0 ) )
    {
        if( Found1 )
        {
            // set the selected profile as the active profile
            m_PlayerProfileReady[0] = TRUE;
            m_pPlayerOneText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_READY") );
            m_pPlayerOneCombo->SetSelection( CurrentSelection1 );
        }
        else if(  g_StateMgr.GetProfileNotSaved( 0 ) )
        {
            // set the unsaved profile as the active profile
            m_PlayerProfileReady[0] = TRUE;
            m_pPlayerOneText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_READY") );
            m_pPlayerOneCombo->SetSelection( CurrentSelection1 );
        }
        else
        {
            // profile not found, deselect it
            m_PlayerProfileReady[0] = FALSE;
            m_pPlayerOneText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );

            // set default selection
            m_pPlayerOneCombo->SetSelection( m_CreateIndex );

            // check if we're polling
            if( !g_SaveDataMgr.IsBusy() )
            {
                // clear the selected profile
                g_StateMgr.ClearSelectedProfile( 0 );
            }
        }
    }
    else
    {
        // restore selection for player 1
        if( ( m_pPlayerOneCombo->GetItemCount() > CurrentSelection1 ) && ( CurrentSelection1 >= 0 ) )
        {
            m_pPlayerOneCombo->SetSelection( CurrentSelection1 );
        }
        else
        {
            m_pPlayerOneCombo->SetSelection( m_CreateIndex );
        }
    }
    
    if( g_StateMgr.GetSelectedProfile( 1 ) )
    {
        if( Found2 )
        {
            // set the selected profile as the active profile
            m_PlayerProfileReady[1] = TRUE;
            m_pPlayerTwoText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_READY") );
            m_pPlayerTwoCombo->SetSelection( CurrentSelection2 );
        }
        else if(  g_StateMgr.GetProfileNotSaved( 1 ) )
        {
            // set the unsaved profile as the active profile
            m_PlayerProfileReady[1] = TRUE;
            m_pPlayerTwoText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_READY") );
            m_pPlayerTwoCombo->SetSelection( CurrentSelection2 );
        }
        else
        {
            // profile not found, deselect it
            m_PlayerProfileReady[1] = FALSE;
            m_pPlayerTwoText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_SELECT") );

            // set default selection
            m_pPlayerTwoCombo->SetSelection( m_CreateIndex );

            // check if we're polling
            if( !g_SaveDataMgr.IsBusy() )
            {
                // clear the selected profile
                g_StateMgr.ClearSelectedProfile( 1 );
            }
        }
    }
    else
    {
        // restore selection for player 2
        if( ( m_pPlayerTwoCombo->GetItemCount() > CurrentSelection2 ) && ( CurrentSelection2 >= 0 ) )
        {
            m_pPlayerTwoCombo->SetSelection( CurrentSelection2 );
        }
        else
        {
            m_pPlayerTwoCombo->SetSelection( m_CreateIndex );
        }
    }
}

//=========================================================================

void dlg_multi_player::OnLoadProfileCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        if( m_bEditProfile )
        {
            // edit profile
            m_State = DIALOG_STATE_EDIT;
        }
        else
        {
            s32 PlayerID = g_StateMgr.GetPendingProfileIndex();

            if( PlayerID == 0 )
            {
                // set the selected profile as the active profile
                m_PlayerProfileReady[0] = TRUE;
                m_pPlayerOneText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_READY") );
            }
            else
            {
                // set the selected profile as the active profile
                m_PlayerProfileReady[1] = TRUE;
                m_pPlayerTwoText->SetLabel( g_StringTableMgr("ui", "IDS_MULTI_PLAYER_READY") );
            }

            // flag the profile as saved
            g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), FALSE );

            // copy the data read into the profile
            g_StateMgr.ActivatePendingProfile();

            // make the dialog active again
            m_State = DIALOG_STATE_ACTIVE;
        }
    }
    else
    {
        // deactivate
        // clear the selected profile
        g_StateMgr.ClearSelectedProfile( g_StateMgr.GetPendingProfileIndex() );

        // update the profile lists
        RefreshProfileList();

        //continue with the status quo of polling the memory cards
        m_State = DIALOG_STATE_ACTIVE;
    }
}

//=========================================================================
