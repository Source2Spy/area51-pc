//=========================================================================
//
//  dlg_profile_options.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_blankbox.hpp"

#include "dlg_PopUp.hpp"
#include "dlg_ProfileOptions.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================

enum
{
    OPTIONS_POPUP_CANCEL_CREATE,
    OPTIONS_POPUP_XBOX_FREE_BLOCKS,
    OPTIONS_POPUP_PROFILE_HAS_CHANGED,
};

//-------------------------------------------------------------------------

// x, y, w, h
s32 s_EDX = 80;
s32 s_EDW = 140;

s32 s_PDX = 100;
s32 s_PDW = 185;

//-------------------------------------------------------------------------

ui_manager::control_tem ProfileOptionsControls_PAL[] = 
{
    { IDC_PROFILE_OPTIONS_CONTROLS,         "IDS_PROFILE_OPTIONS_CONTROLS",     "button",   s_PDX,   40,    s_PDW, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_AVATAR,           "IDS_PROFILE_OPTIONS_AVATAR",       "button",   s_PDX,   80,    s_PDW, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_DIFFICULTY,       "IDS_PROFILE_OPTIONS_DIFFICULTY",   "button",   s_PDX,  120,    s_PDW, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_AUTOSAVE,         "IDS_PROFILE_OPTIONS_AUTOSAVE",     "button",   s_PDX,  160,    s_PDW, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_ACCEPT,           "IDS_PROFILE_OPTIONS_ACCEPT",       "button",   s_PDX,  285,    s_PDW, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },

    { IDC_PROFILE_DIFFICULTY_BBOX,          "IDS_PROFILE_SELECT_DIFFICULTY",    "blankbox", 63,      120,     265, 70, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_PROFILE_DIFFICULTY_SELECT,        "IDS_NULL",                         "combo",    100,     140,   s_PDW, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },


    { IDC_PROFILE_AUTOSAVE_BBOX,            "IDS_PROFILE_SELECT_AUTOSAVE",      "blankbox", 30,      160,     326, 70, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_PROFILE_AUTOSAVE_SELECT,          "IDS_NULL",                         "combo",    45,      180,     295, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },


};

//-------------------------------------------------------------------------

ui_manager::control_tem ProfileOptionsControls_ENG[] =
{
    { IDC_PROFILE_OPTIONS_CONTROLS,         "IDS_PROFILE_OPTIONS_CONTROLS",     "button",   s_EDX,   40,    s_EDW, 40, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_AVATAR,           "IDS_PROFILE_OPTIONS_AVATAR",       "button",   s_EDX,   80,    s_EDW, 40, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_DIFFICULTY,       "IDS_PROFILE_OPTIONS_DIFFICULTY",   "button",   s_EDX,  120,    s_EDW, 40, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_AUTOSAVE,         "IDS_PROFILE_OPTIONS_AUTOSAVE",     "button",   s_EDX,  160,    s_EDW, 40, 0, 4, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_OPTIONS_ACCEPT,           "IDS_PROFILE_OPTIONS_ACCEPT",       "button",   s_EDX,  285,    s_EDW, 40, 0, 6, 1, 1, ui_win::WF_VISIBLE },

    { IDC_PROFILE_DIFFICULTY_BBOX,          "IDS_PROFILE_SELECT_DIFFICULTY",    "blankbox", 50,      120,     200,   70, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_PROFILE_DIFFICULTY_SELECT,        "IDS_NULL",                         "combo",    s_EDX,   140,   s_EDW, 40, 0, 3, 1, 1, ui_win::WF_VISIBLE },


    { IDC_PROFILE_AUTOSAVE_BBOX,            "IDS_PROFILE_SELECT_AUTOSAVE",      "blankbox", 45,      160,     210, 70, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_PROFILE_AUTOSAVE_SELECT,          "IDS_NULL",                         "combo",    55,      180,     190, 40, 0, 5, 1, 1, ui_win::WF_VISIBLE },

};

//-------------------------------------------------------------------------

ui_manager::dialog_tem ProfileOptionsDialog =
{
    "IDS_PROFILE_OPTIONS_TITLE_EDIT",
        1, 10,
        sizeof(ProfileOptionsControls_ENG)/sizeof(ui_manager::control_tem),
        &ProfileOptionsControls_ENG[0],
        0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_profile_options_register( ui_manager* pManager )
{
#ifndef X_EDITOR
    switch( x_GetLocale() )
    {        
    case XL_LANG_ENGLISH:    // English uses default
        break;

    default:  // PAL
        {
            // set up new profile options dialog controls
            ProfileOptionsDialog.StringID = "IDS_PROFILE_OPTIONS_TITLE_EDIT";
            ProfileOptionsDialog.NavW = 1;
            ProfileOptionsDialog.NavH = 10;
            ProfileOptionsDialog.nControls = sizeof(ProfileOptionsControls_PAL)/sizeof(ui_manager::control_tem);
            ProfileOptionsDialog.pControls = &ProfileOptionsControls_PAL[0];
            ProfileOptionsDialog.FocusControl = 0;
        }
        break;

    }
#endif    

    pManager->RegisterDialogClass( "profile options", &ProfileOptionsDialog, &dlg_profile_options_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_profile_options_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_profile_options* pDialog = new dlg_profile_options;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_profile_options
//=========================================================================

dlg_profile_options::dlg_profile_options( void )
{
}

//=========================================================================

dlg_profile_options::~dlg_profile_options( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_profile_options::Create( s32                        UserID,
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

    m_pButtonControls       = (ui_button*)  FindChildByID( IDC_PROFILE_OPTIONS_CONTROLS     );
    m_pButtonAvatar         = (ui_button*)  FindChildByID( IDC_PROFILE_OPTIONS_AVATAR       );
    m_pButtonDifficulty     = (ui_button*)  FindChildByID( IDC_PROFILE_OPTIONS_DIFFICULTY   );
    m_pButtonAutosave       = (ui_button*)  FindChildByID( IDC_PROFILE_OPTIONS_AUTOSAVE     );
    m_pButtonCreate         = (ui_button*)  FindChildByID( IDC_PROFILE_OPTIONS_ACCEPT       );

    m_pDifficultyBBox       = (ui_blankbox*)FindChildByID( IDC_PROFILE_DIFFICULTY_BBOX      );
    m_pDifficultySelect     = (ui_combo*)   FindChildByID( IDC_PROFILE_DIFFICULTY_SELECT    );


    m_pAutosaveBBox         = (ui_blankbox*)FindChildByID( IDC_PROFILE_AUTOSAVE_BBOX        );
    m_pAutosaveSelect       = (ui_combo*)   FindChildByID( IDC_PROFILE_AUTOSAVE_SELECT      );


    s32 iControl = g_StateMgr.GetCurrentControl();
    if( (iControl == -1) || (GotoControl(iControl)==NULL) )
    {
        GotoControl( (ui_control*)m_pButtonControls );
        m_CurrentControl =  IDC_PROFILE_OPTIONS_CONTROLS;
    }
    else
    {
        m_CurrentControl = iControl;
    }

    m_CurrHL = 0;
    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;
    m_OriginalProfile = g_StateMgr.GetPendingProfile();
    m_OriginalProfile.Checksum();

    // switch off the buttons to start
    m_pButtonControls       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonAvatar         ->SetFlag(ui_win::WF_VISIBLE, FALSE);    
    m_pButtonDifficulty     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonAutosave       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonCreate         ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pDifficultyBBox       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pDifficultySelect     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonCreate         ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pAutosaveBBox         ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pAutosaveSelect       ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    // disable pop up selections
    m_pDifficultySelect     ->SetFlag(ui_win::WF_DISABLED, TRUE);
    m_pAutosaveSelect       ->SetFlag(ui_win::WF_DISABLED, TRUE);

    // set up difficulty blankbox   
    m_pDifficultyBBox->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pDifficultyBBox->SetHasTitleBar( TRUE );
    m_pDifficultyBBox->SetLabelColor( xcolor(255,252,204,255) );
    m_pDifficultyBBox->SetTitleBarColor( xcolor(19,59,14,196) );

    // set up difficulty combo
    m_pDifficultySelect->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pDifficultySelect->AddItem  ( g_StringTableMgr( "ui", "IDS_DIFFICULTY_EASY"   ), DIFFICULTY_EASY   );
    m_pDifficultySelect->AddItem  ( g_StringTableMgr( "ui", "IDS_DIFFICULTY_MEDIUM" ), DIFFICULTY_MEDIUM );
    // check if hard is available
    player_profile& Profile = g_StateMgr.GetPendingProfile();
    if( Profile.GetHardUnlocked() )
    {
        m_pDifficultySelect->AddItem  ( g_StringTableMgr( "ui", "IDS_DIFFICULTY_HARD"   ), DIFFICULTY_HARD   );
    }

    // set up status blankbox

    // set up status combo

    // set up autosave blankbox
    m_pAutosaveBBox->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pAutosaveBBox->SetHasTitleBar( TRUE );
    m_pAutosaveBBox->SetLabelColor( xcolor(255,252,204,255) );
    m_pAutosaveBBox->SetTitleBarColor( xcolor(19,59,14,196) );

    // set up the autosave combo
    m_pAutosaveSelect->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV );
    m_pAutosaveSelect->AddItem( g_StringTableMgr( "ui", "IDS_PROFILE_AUTOSAVE_OFF" ), 0 );
    m_pAutosaveSelect->AddItem( g_StringTableMgr( "ui", "IDS_PROFILE_AUTOSAVE_ON" ),  1 );

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
  
    SetNavText( navText );
    
    // initialize screen scaling
    InitScreenScaling( Position );

    // set the frame to be disabled (if coming from off screen)
    if (g_UiMgr->IsScreenOn() == FALSE)
        SetFlag( WF_DISABLED, TRUE );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // set the type
    m_Type = PROFILE_OPTIONS_NORMAL;

    // clear create flag
    m_bCreate = FALSE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_profile_options::Configure( profile_dlg_types Type, xbool bCreate )
{
    // set create flag
    m_bCreate = bCreate;

    if( m_bCreate )
    {
        // change title bar
        SetLabel( g_StringTableMgr( "ui", "IDS_PROFILE_OPTIONS_TITLE_CREATE" ) );
    }

    // set type
    m_Type = Type;

    // reconfigure for pause state
    if( Type != PROFILE_OPTIONS_NORMAL )
    {
        // can't change avatar during pause
        m_pButtonAvatar ->SetFlag(ui_win::WF_DISABLED, TRUE);
    }
}

//=========================================================================

void dlg_profile_options::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_profile_options::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;
    

    // render background filter
    if( m_Type != PROFILE_OPTIONS_NORMAL )
    {
        rb = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect(rb, xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),
                                    xcolor(0,0,0,180),FALSE);
    }


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

void dlg_profile_options::OnSaveProfileCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        s32 const ProfileIndex = g_StateMgr.GetPendingProfileIndex();
        // update the changes in the profile
        g_StateMgr.ActivatePendingProfile();
        g_StateMgr.InitPendingProfile( ProfileIndex );

        // continue onward
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_ACTIVATE;            
    }
    else
    {
        // save failed!
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_SAVE_DATA_ERROR;
    }

    // get the profile list
    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();
    // get the current list from the save data manager
    g_SaveDataMgr.GetProfileNames( ProfileNames );
}

//=========================================================================

void dlg_profile_options::OnAccept( ui_win* pWin )
{
    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonControls )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl =  IDC_PROFILE_OPTIONS_CONTROLS;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonAvatar )
        {
            g_AudioMgr.Play("Select_Norm");
            m_CurrentControl = IDC_PROFILE_OPTIONS_AVATAR;
            m_State = DIALOG_STATE_SELECT;
        }
        else if( pWin == (ui_win*)m_pButtonDifficulty )
        {
            // disable other options
            m_pButtonControls       ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonAvatar         ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonAutosave       ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_DISABLED, TRUE);

            // hide some buttons
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_VISIBLE, FALSE);

            // display combo box
            m_pDifficultyBBox       ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pDifficultySelect     ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pDifficultySelect     ->SetFlag(ui_win::WF_DISABLED, FALSE);

            // update nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_ACCEPT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
            SetNavText( navText );

            player_profile& Profile = g_StateMgr.GetPendingProfile();
            m_pDifficultySelect->SetSelection( Profile.GetDifficultyLevel() );

            // set highlight
            g_UiMgr->SetScreenHighlight( m_pDifficultyBBox->GetPosition() );

            GotoControl( (ui_control*)m_pDifficultySelect );
        }
        else if( pWin == (ui_win*)m_pDifficultySelect )
        {
            // set difficulty level
            player_profile& Profile = g_StateMgr.GetPendingProfile();

            // did the setting change?
            if( Profile.GetDifficultyLevel() != m_pDifficultySelect->GetSelectedItemData() )
            {
                // if we've started a campaign, then flag that the difficulty level changed
                level_check_points& Checkpoint = Profile.GetCheckpoint(0);
                if( Checkpoint.MapID != -1 )
                {
                    // we've got a checkpoint, so we must have started a campaign
                    Profile.SetDifficultyChanged( TRUE );
                }

                Profile.SetDifficultyLevel( m_pDifficultySelect->GetSelectedItemData() );
            }

            // hide combo box
            m_pDifficultyBBox       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pDifficultySelect     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pDifficultySelect     ->SetFlag(ui_win::WF_DISABLED, TRUE);

            // show buttons
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_VISIBLE, TRUE);

            // enable other options
            m_pButtonControls       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonAutosave       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_DISABLED, FALSE);    

            // update nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            SetNavText( navText );

            if( m_Type != PROFILE_OPTIONS_PAUSE )
                m_pButtonAvatar     ->SetFlag(ui_win::WF_DISABLED, FALSE);


            // set highlight
            g_UiMgr->SetScreenHighlight( m_pButtonDifficulty->GetPosition() );

            GotoControl( (ui_control*)m_pButtonDifficulty );
        }
        else if( pWin == (ui_win*)m_pButtonAutosave )
        {
            // disable other options
            m_pButtonControls       ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonAvatar         ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonAutosave       ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_DISABLED, TRUE);

            // hide autosave button
            m_pButtonAutosave       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_VISIBLE, FALSE);

            // display combo box
            m_pAutosaveBBox         ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pAutosaveSelect       ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pAutosaveSelect       ->SetFlag(ui_win::WF_DISABLED, FALSE);

            // update nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_ACCEPT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
            SetNavText( navText );

            player_profile& Profile = g_StateMgr.GetPendingProfile();
            m_pAutosaveSelect->SetSelection( Profile.GetAutosaveOn() );

            // set highlight
            g_UiMgr->SetScreenHighlight( m_pAutosaveBBox->GetPosition() );

            GotoControl( (ui_control*)m_pAutosaveSelect );
        }
        else if( pWin == (ui_win*)m_pAutosaveSelect )
        {
            // set autosave status
            player_profile& Profile = g_StateMgr.GetPendingProfile();
            Profile.SetAutosaveOn( m_pAutosaveSelect->GetSelectedItemData() );

            // hide combo box
            m_pAutosaveBBox         ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pAutosaveSelect       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pAutosaveSelect       ->SetFlag(ui_win::WF_DISABLED, TRUE);

            // show hidden buttons
            m_pButtonAutosave       ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_VISIBLE, TRUE);

            // enable other options
            m_pButtonControls       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonAutosave       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_DISABLED, FALSE);    

            // update nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            SetNavText( navText );

            if( m_Type != PROFILE_OPTIONS_PAUSE )
                m_pButtonAvatar     ->SetFlag(ui_win::WF_DISABLED, FALSE);

            // set highlight
            g_UiMgr->SetScreenHighlight( m_pButtonAutosave->GetPosition() );

            GotoControl( (ui_control*)m_pButtonAutosave );
        }
        else if( pWin == (ui_win*)m_pButtonCreate )
        {
            // are we creating this profile?
            if( m_bCreate && g_StateMgr.GetProfileNotSaved( g_StateMgr.GetPendingProfileIndex() ) )
            {
                g_AudioMgr.Play("Select_Norm");
                m_CurrentControl = IDC_PROFILE_OPTIONS_ACCEPT;

                // this is a NEW profile that we just created

                // calculate the hash string for the new profile
                player_profile& NewProfile = g_StateMgr.GetPendingProfile();
                NewProfile.SetHash();

                // select this new profile
                g_StateMgr.SetSelectedProfile( g_StateMgr.GetPendingProfileIndex(), NewProfile.GetHash() );

                g_SaveDataMgr.CreateProfile( g_StateMgr.GetPendingProfileIndex(), this, &dlg_profile_options::OnProfileCreateCB );

                // wait for the save data request
                m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
            }
            else // Editing existing profile
            {
                // check for any changes made to the profile
                player_profile PendingProfile = g_StateMgr.GetPendingProfile();

                if( PendingProfile.HasChanged() )
                {
                    OpenProfileChangedPopup();
                    return;
                }
                else
                {
                    // no changes
                    g_AudioMgr.Play("Select_Norm");
                    m_State = DIALOG_STATE_BACK;
                }
            }
        }
    }
}


//=========================================================================

void dlg_profile_options::OnProfileCreateCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        s32 const ProfileIndex = g_StateMgr.GetPendingProfileIndex();
        g_StateMgr.SetProfileNotSaved( ProfileIndex, FALSE );

        // update the changes in the profile
        g_StateMgr.ActivatePendingProfile();
        g_StateMgr.InitPendingProfile( ProfileIndex );
        g_AudioMgr.Play( "Select_Norm" );

        // continue to campaign menu or next logical step
        m_State = DIALOG_STATE_SELECT; 
    }
    else
    {
        // save unsuccessful - return to profile select screen or previous menu
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_profile_options::OpenProfileChangedPopup( void )
{
    irect const Bounds = g_UiMgr->GetUserBounds( g_UiUserID );
    m_PopUp = static_cast<dlg_popup*>( g_UiMgr->OpenDialog( m_UserID,
                                                            "popup",
                                                            Bounds,
                                                            NULL,
                                                            ui_win::WF_VISIBLE | ui_win::WF_BORDER |
                                                            ui_win::WF_DLG_CENTER | ui_win::WF_INPUTMODAL ) );
    m_PopUpType = OPTIONS_POPUP_PROFILE_HAS_CHANGED;

    xwstring PopupNavText( g_StringTableMgr( "ui", "IDS_NAV_YES" ) );
    PopupNavText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
    SetNavTextVisible( FALSE );
    m_State = DIALOG_STATE_POPUP;
    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_PROFILE_EDIT" ),
                        TRUE,
                        TRUE,
                        FALSE,
                        g_StringTableMgr( "ui", "IDS_PROFILE_EDIT_MSG" ),
                        PopupNavText,
                        &m_PopUpResult );
}

//=========================================================================

void dlg_profile_options::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pDifficultySelect )
        {
            // hide combo box
            m_pDifficultyBBox       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pDifficultySelect     ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pDifficultySelect     ->SetFlag(ui_win::WF_DISABLED, TRUE);

            // show difficulty button
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_VISIBLE, TRUE);

            // enable other options
            m_pButtonControls       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonAutosave       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_DISABLED, FALSE);    

            // update nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            SetNavText( navText );

            if( m_Type != PROFILE_OPTIONS_PAUSE )
                m_pButtonAvatar     ->SetFlag(ui_win::WF_DISABLED, FALSE);


            // set highlight
            g_UiMgr->SetScreenHighlight( m_pButtonDifficulty->GetPosition() );

            GotoControl( (ui_control*)m_pButtonDifficulty );

            return;
        }
        else if( pWin == (ui_win*)m_pAutosaveSelect )
        {
            // hide combo box
            m_pAutosaveBBox         ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pAutosaveSelect       ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pAutosaveSelect       ->SetFlag(ui_win::WF_DISABLED, TRUE);

            // show hidden buttons
            m_pButtonAutosave       ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_VISIBLE, TRUE);

            // enable other options
            m_pButtonControls       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonAutosave       ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_DISABLED, FALSE);    

            // update nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            SetNavText( navText );

            if( m_Type != PROFILE_OPTIONS_PAUSE )
                m_pButtonAvatar     ->SetFlag(ui_win::WF_DISABLED, FALSE);

            // set highlight
            g_UiMgr->SetScreenHighlight( m_pButtonAutosave->GetPosition() );

            GotoControl( (ui_control*)m_pButtonAutosave );

            return;
        }

        // are we creating a new profile or editing an existing one?
        if( m_bCreate && g_StateMgr.GetProfileNotSaved( g_StateMgr.GetPendingProfileIndex() ) )
        {
            // Confirm abandoning an unsaved profile.
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
            m_PopUpType = OPTIONS_POPUP_CANCEL_CREATE;
            m_State = DIALOG_STATE_POPUP;

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
            SetNavTextVisible( FALSE );

            // configure message
            m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_PROFILE_CREATE" ), 
                TRUE, 
                TRUE, 
                FALSE, 
                g_StringTableMgr( "ui", "IDS_PROFILE_CANCEL_CREATE_MSG" ),
                navText,
                &m_PopUpResult );

            return;
        }

        if( g_StateMgr.GetPendingProfile().HasChanged() )
        {
            OpenProfileChangedPopup();
            return;
        }

        // Cancel changes and exit to the previous screen.
        g_AudioMgr.Play("Backup");
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_profile_options::OnUpdate ( ui_win* pWin, f32 DeltaTime )
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
            m_pButtonControls       ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonAvatar         ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonDifficulty     ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonAutosave       ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonCreate         ->SetFlag(ui_win::WF_VISIBLE, TRUE);

            s32 iControl = g_StateMgr.GetCurrentControl();

            if( m_bCreate )
            {
                if( (iControl == -1) || (GotoControl(iControl)==NULL) )
                {
                    GotoControl( (ui_control*)m_pButtonCreate );
                    g_UiMgr->SetScreenHighlight( m_pButtonCreate->GetPosition() );
                }
                else
                {
                    ui_control* pControl = GotoControl( iControl );
                    g_UiMgr->SetScreenHighlight(pControl->GetPosition() );
                    m_CurrentControl = iControl;
                }
            }
            else
            {
                if( (iControl == -1) || (GotoControl(iControl)==NULL) )
                {
                    GotoControl( (ui_control*)m_pButtonControls );
                    g_UiMgr->SetScreenHighlight( m_pButtonControls->GetPosition() );
                }
                else
                {
                    // range check control ID
                    if( iControl > 4 ) // Assuming max control index is 4 for edit mode (Controls, Avatar, Difficulty, Online Status, Autosave)
                    {
                        iControl = 0;
                    }

                    ui_control* pControl = GotoControl( iControl );
                    ASSERT( pControl );
                    g_UiMgr->SetScreenHighlight(pControl->GetPosition() );
                    m_CurrentControl = iControl;
                }
            }

            if (g_UiMgr->IsScreenOn() == FALSE)
            {
                // enable the frame
                SetFlag( WF_DISABLED, FALSE );
                g_UiMgr->SetScreenOn(TRUE);
            }
        }
    }

    // check for result of popup box
    if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            switch( m_PopUpType )
            {
                case OPTIONS_POPUP_CANCEL_CREATE:
                {
                    // abandon create?
                    if ( m_PopUpResult == DLG_POPUP_YES )
                    {
                        // abandon changes and return to profile select screen
                        g_AudioMgr.Play("Backup");
                        m_State = DIALOG_STATE_BACK;
                    }
                    else
                    {
                        // re-enable dialog
                        m_State = DIALOG_STATE_ACTIVE;
                    }
                }
                break;

                case OPTIONS_POPUP_PROFILE_HAS_CHANGED:
                {
                    // save changes?
                    if ( m_PopUpResult == DLG_POPUP_YES )
                    {
                        s32 const ProfileIndex = g_StateMgr.GetPendingProfileIndex();
                        if( (ProfileIndex < 0) || (ProfileIndex >= SM_PROFILE_COUNT) )
                        {
                            g_StateMgr.GetPendingProfile() = m_OriginalProfile;
                            g_AudioMgr.Play( "Backup" );
                            m_State = DIALOG_STATE_BACK;
                            break;
                        }

                        // check if this profile exists on disk already
                        if( g_StateMgr.GetProfileNotSaved( ProfileIndex ) )
                        {
                            g_SaveDataMgr.CreateProfile( ProfileIndex, this, &dlg_profile_options::OnProfileCreateCB );
                            m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
                        }
                        else
                        {
                            // Profile already exists on disk. Save the changes.
                            g_AudioMgr.Play("Select_Norm");

                            m_CurrentControl = IDC_PROFILE_OPTIONS_ACCEPT;

                            profile_info* pProfileInfo = &g_SaveDataMgr.GetProfileInfo( ProfileIndex );
                            g_SaveDataMgr.SaveProfile( *pProfileInfo, ProfileIndex, this, &dlg_profile_options::OnSaveProfileCB );

                            m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
                        }
                    }
                    else // User chose "No" to saving changes
                    {
                        // Abandon changes.
                        g_StateMgr.GetPendingProfile() = m_OriginalProfile;
                        g_AudioMgr.Play("Backup");
                        m_State = DIALOG_STATE_BACK;
                    }
                }
                break;
            }
            // clear popup
            m_PopUp = NULL;
            m_PopUpResult = DLG_POPUP_IDLE;

            // turn on nav text
            SetNavTextVisible( TRUE );
        }
    }


    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    if( m_pButtonControls->IsFocused() )
    {
        highLight = 0;
        g_UiMgr->SetScreenHighlight( m_pButtonControls->GetPosition() );
    }
    else if( m_pButtonAvatar->IsFocused() )
    {
        highLight = 1;
        g_UiMgr->SetScreenHighlight( m_pButtonAvatar->GetPosition() );
    }
    else if( m_pButtonDifficulty->IsFocused() )
    {
        highLight = 2;
        g_UiMgr->SetScreenHighlight( m_pButtonDifficulty->GetPosition() );
    }
    else if( m_pButtonAutosave->IsFocused() )
    {
        highLight = 4;
        g_UiMgr->SetScreenHighlight( m_pButtonAutosave->GetPosition() );
    }
    else if( m_pButtonCreate->IsFocused() )
    {
        // Assuming 'Create/Accept' button corresponds to highlight index 6
        // (Indices seem to skip based on control layout/disabling)
        highLight = 6;
        g_UiMgr->SetScreenHighlight( m_pButtonCreate->GetPosition() );
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;
    }
}

//=========================================================================
