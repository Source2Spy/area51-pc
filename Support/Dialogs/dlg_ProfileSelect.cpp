//=========================================================================
//
//  dlg_profile_select.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_listbox.hpp"
#include "UI/ui_dlg_vkeyboard.hpp"
#include "UI/ui_blankbox.hpp"

#include "dlg_PopUp.hpp"
#include "dlg_ProfileSelect.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "Parsing/TextIn.hpp"
#include "../../Apps/GameApp/Config.hpp"    
#include "SaveData/SaveDataMgr.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================

enum popup_type
{
    POPUP_TYPE_DELETE,
    POPUP_TYPE_BADNAME,
    POPUP_TYPE_DAMAGED_PROFILE,
    POPUP_XBOX_FREE_MORE_BLOCKS,
};

enum controls
{
    IDC_PROFILE_SELECT_LISTBOX,
    IDC_PROFILE_SELECT_INFOBOX,

    IDC_PROFILE_CREATE_DATE,
    IDC_PROFILE_MODIFIED_DATE,

    IDC_PROFILE_INFO_CREATE_DATE,
    IDC_PROFILE_INFO_MODIFIED_DATE,

};


ui_manager::control_tem ProfileSelectControls[] = 
{
    // Frames.
    { IDC_PROFILE_SELECT_LISTBOX,       "IDS_PROFILE_PROFILES",         "listbox",  45,  40, 240, 206, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_PROFILE_SELECT_INFOBOX,       "IDS_PROFILE_INFO",             "blankbox", 45, 256, 240,  76, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_PROFILE_CREATE_DATE,          "IDS_PROFILE_CREATE_DATE",      "text",     53, 286, 120,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PROFILE_MODIFIED_DATE,        "IDS_PROFILE_MODIFIED_DATE",    "text",     53, 306, 120,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_PROFILE_INFO_CREATE_DATE,     "IDS_NULL",                     "text",    157, 286,  80,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PROFILE_INFO_MODIFIED_DATE,   "IDS_NULL",                     "text",    157, 306,  80,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },

};

ui_manager::dialog_tem ProfileSelectDialog =
{
    "IDS_PROFILE_MAIN_MENU",
    1, 9,
    sizeof(ProfileSelectControls)/sizeof(ui_manager::control_tem),
    &ProfileSelectControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_profile_select_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "profile select", &ProfileSelectDialog, &dlg_profile_select_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_profile_select_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_profile_select* pDialog = new dlg_profile_select;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_level_select
//=========================================================================

dlg_profile_select::dlg_profile_select( void )
{
}

//=========================================================================

dlg_profile_select::~dlg_profile_select( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_profile_select::Create( s32                        UserID,
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
    m_pProfileList     = (ui_listbox*)    FindChildByID( IDC_PROFILE_SELECT_LISTBOX  );

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
    navText += g_StringTableMgr( "ui", "IDS_NAV_DELETE" );
   
    SetNavText( navText );

    g_SaveDataMgr.RefreshProfiles( this, &dlg_profile_select::OnPollReturn );

    // set up level list
    m_pProfileList->SetActive( TRUE );
    m_pProfileList->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pProfileList->DisableFrame();
    m_pProfileList->SetExitOnSelect(FALSE);
    m_pProfileList->SetExitOnBack(TRUE);
    m_pProfileList->EnableHeaderBar();
    m_pProfileList->SetHeaderBarColor( xcolor(19,59,14,196) );
    m_pProfileList->SetHeaderColor( xcolor(255,252,204,255) );

    // get profile details box
    m_pProfileDetails = (ui_blankbox*)FindChildByID( IDC_PROFILE_SELECT_INFOBOX );
    m_pProfileDetails->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pProfileDetails->SetHasTitleBar( TRUE );
    m_pProfileDetails->SetLabelColor( xcolor(255,252,204,255) );
    m_pProfileDetails->SetTitleBarColor( xcolor(19,59,14,196) );

    // set up profile info text
    m_pCreationDate     = (ui_text*)FindChildByID( IDC_PROFILE_CREATE_DATE        );
    m_pModifiedDate     = (ui_text*)FindChildByID( IDC_PROFILE_MODIFIED_DATE      );
    m_pInfoCreationDate = (ui_text*)FindChildByID( IDC_PROFILE_INFO_CREATE_DATE   );
    m_pInfoModifiedDate = (ui_text*)FindChildByID( IDC_PROFILE_INFO_MODIFIED_DATE );

    m_pCreationDate     ->UseSmallText( TRUE );
    m_pModifiedDate     ->UseSmallText( TRUE );
    m_pInfoCreationDate ->UseSmallText( TRUE );
    m_pInfoModifiedDate ->UseSmallText( TRUE );

    m_pCreationDate     ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pModifiedDate     ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pInfoCreationDate ->SetLabelFlags( ui_font::h_left|ui_font::v_center );
    m_pInfoModifiedDate ->SetLabelFlags( ui_font::h_left|ui_font::v_center );

    m_pCreationDate     ->SetLabelColor( xcolor(255,252,204,255) );
    m_pModifiedDate     ->SetLabelColor( xcolor(255,252,204,255) );
    m_pInfoCreationDate ->SetLabelColor( xcolor(255,252,204,255) );
    m_pInfoModifiedDate ->SetLabelColor( xcolor(255,252,204,255) );


    // Initialize dialog mode
    m_Type = PROFILE_SELECT_NORMAL;
    m_bEditProfile = FALSE;

    // get profile data
    RefreshProfileList();
    m_ProfileName = g_StringTableMgr("ui", "IDS_PROFILE_DEFAULT_PLAYER");
    m_ProfileEntered = FALSE;
    m_ProfileOk = FALSE;

    // clear the selected profile
    //g_StateMgr.ClearSelectedProfile( 0 );

    // set initial focus
    m_CurrHL = 0;
    GotoControl( (ui_control*)m_pProfileList );
    m_CurrentControl = IDC_PROFILE_SELECT_LISTBOX;
    m_PopUp = NULL;
    m_BlocksRequired = 0;

    // BackupPopup
    m_BackupPopup = NULL;

    // initialize screen scaling
    InitScreenScaling( Position );

    // disable blackout
    m_bRenderBlackout = FALSE;

    // switch off the controls to start
    m_pProfileList       ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pProfileDetails    ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pCreationDate      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pModifiedDate      ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pInfoCreationDate  ->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pInfoModifiedDate  ->SetFlag( ui_win::WF_VISIBLE, FALSE );

    // set the frame to be disabled (if coming from off screen)
    if( g_UiMgr->IsScreenOn() == FALSE )
    {
        SetFlag( WF_DISABLED, TRUE );
    }

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_profile_select::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_profile_select::Configure( profile_select_type DialogType )
{
    m_Type = DialogType;

    switch( m_Type )
    {
        case PROFILE_SELECT_MANAGE:
        {
            SetLabel( g_StringTableMgr( "ui", "IDS_PROFILE_MANAGE_PROFILES" ) );
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
            navText += g_StringTableMgr( "ui", "IDS_NAV_DELETE" );
            navText += g_StringTableMgr( "ui", "IDS_NAV_EDIT" );
            SetNavText( navText );
        }
        break;

        case PROFILE_SELECT_NORMAL:
            break;

        case PROFILE_SELECT_OVERWRITE:
            break;
    }
}

//=========================================================================

void dlg_profile_select::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;
    
    if( m_bRenderBlackout )
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
void dlg_profile_select::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    // only allow navigation if active
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
    }
}

//=========================================================================

void dlg_profile_select::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( g_SaveDataMgr.IsBusy() )
        {
            g_AudioMgr.Play( "InvalidEntry" );
            return;
        }
        
        // check for bad profile
        if( m_pProfileList->GetSelectedItemData( 1 ) != PROFILE_OK )
        {
            g_AudioMgr.Play( "InvalidEntry" );
            return;
        }

        // get the profile index from the list
        s32 index = m_pProfileList->GetSelectedItemData();

        // check if this is a new profile
        if( index < m_CreateIndex )
        {
            if( m_Type == PROFILE_SELECT_OVERWRITE )
            {
                // get the profile list
                xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

                // calculate a new hash string for the profile we are overwriting
                player_profile& NewProfile = g_StateMgr.GetPendingProfile();
                NewProfile.SetHash();

                // store the id of the selected profile
                g_StateMgr.SetSelectedProfile( g_StateMgr.GetPendingProfileIndex(), NewProfile.GetHash() );//ProfileNames[index]->Hash );

                // overwrite the selected profile
                g_SaveDataMgr.OverwriteProfile( *ProfileNames[index], g_StateMgr.GetPendingProfileIndex(), this, &dlg_profile_select::OnSaveProfileCB );

                // wait for the save data request
                m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
            }
            else
            {
                // load the selected profile

                // init the pending profile
                g_StateMgr.InitPendingProfile( 0 ); // always player 0 in campaign

                // get the profile list
                xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

                // store the id of the selected profile
                g_StateMgr.SetSelectedProfile( 0, ProfileNames[index]->Hash );

                // attempt to load the selected profile
                g_SaveDataMgr.LoadProfile( *ProfileNames[index], 0, this, &dlg_profile_select::OnLoadProfileCB );

                // wait for the save data request
                m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
            }
        }
        else
        {
            // create a new profile           
            if( m_Type == PROFILE_SELECT_OVERWRITE )
            {
                // calculate the hash string for the new profile
                player_profile& NewProfile = g_StateMgr.GetPendingProfile();
                NewProfile.SetHash();

                // store the id of the selected profile
                g_StateMgr.SetSelectedProfile( g_StateMgr.GetPendingProfileIndex(), NewProfile.GetHash() );

                // save the in-memory profile as a new save data file
                m_State = DIALOG_STATE_CREATE;
            }
            else
            {
                // set up the new profile data
                m_State = DIALOG_STATE_POPUP;

                // set the profile up with default settings
                g_StateMgr.ResetProfile( 0 );
                g_StateMgr.SetProfileNotSaved( 0, TRUE );

                // init the pending profile for player 0
                g_StateMgr.InitPendingProfile( 0 ); 

                // clear the selected profile
                g_StateMgr.ClearSelectedProfile( 0 );

                // open a VK to enter the profile name
                irect   r = m_pManager->GetUserBounds( m_UserID );
                ui_dlg_vkeyboard* pVKeyboard = (ui_dlg_vkeyboard*)m_pManager->OpenDialog( m_UserID, "ui_vkeyboard", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_INPUTMODAL );
                pVKeyboard->Configure( TRUE );
                pVKeyboard->SetLabel( g_StringTableMgr( "ui", "IDS_PROFILE_CREATE" ) );
                pVKeyboard->ConnectString( &m_ProfileName, SM_PROFILE_NAME_LENGTH );
                pVKeyboard->SetReturn( &m_ProfileEntered, &m_ProfileOk );

                // update nav text
                xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
                navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
                navText += g_StringTableMgr( "ui", "IDS_NAV_DELETE" );
                SetNavText( navText );
            }
        }
    }
}

//=========================================================================

void dlg_profile_select::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)x;
    (void)y;
    OnAccept( pWin );
}

//=========================================================================

void dlg_profile_select::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( m_Type == PROFILE_SELECT_OVERWRITE )
        {
            // no backing out in this state
            return;
        }

        // check for backing out during online connect phase
        if( g_StateMgr.GetState() == SM_ONLINE_PROFILE_SELECT )
        {
            CreateBackupPopup();
            return;
        }

        // Clear the poll callback
        g_AudioMgr.Play("Backup");
        g_SaveDataMgr.CancelCallbacks( this );
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_profile_select::OnDelete( ui_win* pWin )
{
    (void)pWin;
    // delete the profile

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( g_SaveDataMgr.IsBusy() )
        {
            g_AudioMgr.Play( "InvalidEntry" );
            return;
        }
        
        // get the profile index from the list
        s32 index = m_pProfileList->GetSelectedItemData();

        // make sure the profile is valid
        if( index < m_CreateIndex )
        {
            // open delete confirmation dialog
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
            m_PopUpType = POPUP_TYPE_DELETE;
            m_State = DIALOG_STATE_POPUP;

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
            SetNavTextVisible( FALSE );
            

            m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_PROFILE_DELETE" ), 
                                TRUE, 
                                TRUE, 
                                FALSE, 
                                g_StringTableMgr( "ui", "IDS_PROFILE_DELETE_MSG" ),
                                navText,
                                &m_PopUpResult );
        }
        else
        {
            g_AudioMgr.Play( "InvalidEntry" );
        }
    }
}

//=========================================================================

void dlg_profile_select::OnAlternate( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( g_SaveDataMgr.IsBusy() )
        {
            g_AudioMgr.Play( "InvalidEntry" );
            return;
        }
        
        switch( m_Type )
        {
            // continue without saving
            case PROFILE_SELECT_OVERWRITE:
            {
                // Clear the poll callback
                g_SaveDataMgr.CancelCallbacks( this );
                // flag the profile as not saved
                g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), TRUE ); 
                // Continue without saving
                g_StateMgr.ActivatePendingProfile();
                m_State = DIALOG_STATE_ACTIVATE;            
            }
            break;

            // load profile to edit
            case PROFILE_SELECT_MANAGE:
            {
                // check for bad profile
                if( m_pProfileList->GetSelectedItemData( 1 ) != PROFILE_OK )
                {
                    g_AudioMgr.Play( "InvalidEntry" );
                    return;
                }

                // get the profile index from the list
                s32 index = m_pProfileList->GetSelectedItemData();

                // check if this is a new profile
                if( index < m_CreateIndex )
                {
                    // edit the selected profile
                    m_bEditProfile = TRUE;

                    // init the pending profile
                    g_StateMgr.InitPendingProfile( 0 ); // always player 0 in campaign

                    // get the profile list
                    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

                    // store the id of the selected profile
                    g_StateMgr.SetSelectedProfile( 0, ProfileNames[index]->Hash );

                    // attempt to load the selected profile
                    g_SaveDataMgr.LoadProfile( *ProfileNames[index], 0, this, &dlg_profile_select::OnLoadProfileCB );

                    // wait for the save data request
                    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
                }
                else
                {
                    // can't edit the create option!
                    g_AudioMgr.Play( "InvalidEntry" );
                    return;
                }
            }
            break;
        }
    }
}

//=========================================================================

void dlg_profile_select::OnPollReturn( void )
{
    RefreshProfileList();
}

//=========================================================================

void dlg_profile_select::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the controls
            m_pProfileList       ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pProfileDetails    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pCreationDate      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pModifiedDate      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pInfoCreationDate  ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pInfoModifiedDate  ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            GotoControl( (ui_control*)m_pProfileList );
            g_UiMgr->SetScreenHighlight( m_pProfileList->GetPosition() );

            if( g_UiMgr->IsScreenOn() == FALSE )
            {
                // enable the frame
                SetFlag( WF_DISABLED, FALSE );
                g_UiMgr->SetScreenOn( TRUE );
            }
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    // check for profile name entry
    if( m_ProfileEntered )
    {
        m_ProfileEntered = FALSE;
    
        if( m_ProfileOk )
        {
            m_ProfileOk = FALSE;
            
            // check for duplicate name entry
            for( s32 p=0; p<m_pProfileList->GetItemCount(); p++ )
            {
                if( x_wstrcmp( m_pProfileList->GetItemLabel(p), m_ProfileName ) == 0 )
                {
                    // open duplicate name error popup
                    irect r = g_UiMgr->GetUserBounds( g_UiUserID );
                    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
                    m_PopUpType = POPUP_TYPE_BADNAME;

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
            g_StateMgr.InitPendingProfile( 0 );
            player_profile& NewProfile = g_StateMgr.GetPendingProfile();
            NewProfile.SetProfileName( xstring(m_ProfileName) );

            // go to the profile options screen
            m_State = DIALOG_STATE_ACTIVATE;
            return;
        }
        else
        {
            // re-enable dialog
            m_State = DIALOG_STATE_ACTIVE;
        }
    }

    // get the profile index from the list
    s32 index = m_pProfileList->GetSelectedItemData();

    if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            switch( m_PopUpType )
            {
                ///////////////////////////////////////////////////////////////

                case POPUP_TYPE_DELETE:
                {
                    if ( m_PopUpResult == DLG_POPUP_YES )
                    {
                        // get the profile list
                        xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

                        // Bye bye profile, bye bye!
                        g_SaveDataMgr.DeleteProfile( *ProfileNames[index], this, &dlg_profile_select::OnDeleteProfileCB );

                        // wait for the save data request
                        m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
                    }
                    else
                    {
                        m_State = DIALOG_STATE_ACTIVE;
                    }
                    break;
                }

                case POPUP_TYPE_BADNAME:
                    // re-enable dialog
                    m_State = DIALOG_STATE_ACTIVE;
                    break;

                ///////////////////////////////////////////////////////////////

                default:
                    ASSERT(0);
                    break;
            }

            // clear popup 
            m_PopUp = NULL;

            // turn on nav text
            SetNavTextVisible( TRUE );
        }
    }
    else if( m_BackupPopup )
    {
        UpdateBackupPopup();
    }
    else
    {
        if( m_State == DIALOG_STATE_ACTIVE )
        {
            if( CONFIG_IS_AUTOCLIENT || CONFIG_IS_AUTOSERVER )
            {
                if( m_pProfileList->GetItemCount() )
                {
                    g_StateMgr.ResetProfile( 0 );
                    g_StateMgr.InitPendingProfile(0);
                    // copy the data read into the profile
                    g_StateMgr.ActivatePendingProfile();

                    // tell the state mgr that we finished
                    m_State = DIALOG_STATE_SELECT;
                }
            }

            // update the profile lists
            RefreshProfileList();

            // get current index
            index = m_pProfileList->GetSelectedItemData();

            // make sure we're not in the vkeyboard before changing the nav text.
            if( (dlg_profile_select*)m_pManager->GetTopmostDialog( g_UiUserID ) == this )
            {
                // check if this is a new profile
                if( index < m_CreateIndex )
                {
                    // update nav text
                    switch( m_Type )
                    {
                        case PROFILE_SELECT_OVERWRITE:
                        {
                            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SAVE_CHANGES" ));
                            navText += g_StringTableMgr( "ui", "IDS_NAV_DELETE" );
                            navText += g_StringTableMgr( "ui", "IDS_NAV_CONT_NO_SAVE" );
                            SetNavText( navText );
                        }
                        break;

                        case PROFILE_SELECT_NORMAL:
                        {
                            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
                            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
                            navText += g_StringTableMgr( "ui", "IDS_NAV_DELETE" );
                            SetNavText( navText );
                        }
                        break;

                        case PROFILE_SELECT_MANAGE:
                        {
                            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
                            navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
                            navText += g_StringTableMgr( "ui", "IDS_NAV_DELETE" );
                            navText += g_StringTableMgr( "ui", "IDS_NAV_EDIT" );
                            SetNavText( navText );
                        }
                        break;
                    }
                }
                else
                {
                    // update nav text
                    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));

                    if( m_Type == PROFILE_SELECT_OVERWRITE )
                    {
                        navText += g_StringTableMgr( "ui", "IDS_NAV_CONT_NO_SAVE" );
                    }
                    else
                    {
                        navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
                    }

                    SetNavText( navText );
                }
            }
        }
    }
}

//=========================================================================

void dlg_profile_select::RefreshProfileList( void )
{
    xbool Found = FALSE;
    u32 ProfileHashToSelect;

    // get the hash for the selected profile
    if( m_Type == PROFILE_SELECT_OVERWRITE )
    {
        ProfileHashToSelect = g_StateMgr.GetSelectedProfile( g_StateMgr.GetPendingProfileIndex() );
    }
    else
    {
        ProfileHashToSelect = g_StateMgr.GetSelectedProfile( 0 );
    }

    // get the profile list
    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();

    // store the current selection
    s32 CurrentSelection = m_pProfileList->GetSelection();

    // clear the list
    m_pProfileList->DeleteAllItems();

    // get the current list from the save data manager
    g_SaveDataMgr.GetProfileNames( ProfileNames );

    // fill it with the profile information
    for( s32 i = 0; i < ProfileNames.GetCount(); i++ )
    {
        if( ProfileNames[i]->bDamaged )
        {
            // add the profile to the list
            m_pProfileList->AddItem( g_StringTableMgr( "ui", "IDS_CORRUPT" ), i, PROFILE_CORRUPT );
            m_pProfileList->SetItemColor( i, XCOLOR_RED );
        }
        else
        {
            // add the profile to the list
            m_pProfileList->AddItem( ProfileNames[i]->Name, i, PROFILE_OK );
        }

        // look for a match for the selected profile hash
        if( ProfileHashToSelect != 0 )
        {
            if( ProfileNames[i]->Hash == ProfileHashToSelect )
            {
                if( CurrentSelection == -1 )
                    CurrentSelection = i;
                Found = TRUE;
            }
        }
    }

    // add a create option
    m_CreateIndex = ProfileNames.GetCount();
    m_pProfileList->AddItem( g_StringTableMgr( "ui", "IDS_PROFILE_CREATE_NEW" ), m_CreateIndex );

    // determine if profile selected
    if( ( CurrentSelection >= 0 ) && ( CurrentSelection < m_pProfileList->GetItemCount() ) )
    {
        m_pProfileList->SetSelection( CurrentSelection );
    }
    else
    {
        m_pProfileList->SetSelection( 0 );
    }

    // populate profile info
    s32 SelIndex = m_pProfileList->GetSelection();

    if( SelIndex == m_CreateIndex )
    {
        m_pProfileDetails->SetLabel( g_StringTableMgr( "ui", "IDS_PROFILE_INFO" ) );
        m_pCreationDate     ->SetFlag( WF_VISIBLE, FALSE );
        m_pModifiedDate     ->SetFlag( WF_VISIBLE, FALSE );
        m_pInfoCreationDate ->SetFlag( WF_VISIBLE, FALSE );
        m_pInfoModifiedDate ->SetFlag( WF_VISIBLE, FALSE );
    }
    else
    {
        if( SelIndex >= 0 && SelIndex < ProfileNames.GetCount() )
        {
            if( !g_UiMgr->IsScreenScaling() )
            {
                m_pCreationDate     ->SetFlag( WF_VISIBLE, TRUE );
                m_pModifiedDate     ->SetFlag( WF_VISIBLE, TRUE );
                m_pInfoCreationDate ->SetFlag( WF_VISIBLE, TRUE );
                m_pInfoModifiedDate ->SetFlag( WF_VISIBLE, TRUE );
            }

            m_pProfileDetails->SetLabel( g_StringTableMgr( "ui", "IDS_PROFILE_INFO" ) );

            split_date TimeStamp = eng_SplitDate( ProfileNames[SelIndex]->CreationDate );
            const xwchar* Month = g_StringTableMgr( "ui", (const char*)xfs("IDS_MONTH%d", TimeStamp.Month));
            xwstring CreateStamp(xfs("%02i:%02i:%02i ",TimeStamp.Hour, TimeStamp.Minute, TimeStamp.Second));
            CreateStamp += Month;
            CreateStamp += (const char*)xfs("%02i", TimeStamp.Day);
            m_pInfoCreationDate->SetLabel(CreateStamp);
            
            TimeStamp = eng_SplitDate( ProfileNames[SelIndex]->ModifiedDate );
            Month = g_StringTableMgr( "ui", (const char*)xfs("IDS_MONTH%d", TimeStamp.Month));
            xwstring ModStamp(xfs("%02i:%02i:%02i ",TimeStamp.Hour, TimeStamp.Minute, TimeStamp.Second));
            ModStamp += Month;
            ModStamp += (const char*)xfs("%02i", TimeStamp.Day);
            m_pInfoModifiedDate->SetLabel(ModStamp);
        }
        else
        {
            m_pProfileDetails   ->SetLabel( g_StringTableMgr( "ui", "IDS_ERROR" ) );
            m_pInfoCreationDate ->SetLabel( xwstring("") );
            m_pInfoModifiedDate ->SetLabel( xwstring("") );
        }
    }
}

//=========================================================================

void dlg_profile_select::OnLoadProfileCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        // copy the data read into the profile
        g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), FALSE );
        g_StateMgr.ActivatePendingProfile();

        // tell the state manager that we finished
        if( m_bEditProfile )
        {
            m_State = DIALOG_STATE_EDIT;
        }
        else
        {
            m_State = DIALOG_STATE_SELECT;
        }
    }
    else
    {
        // else we just continue with the status quo of polling the memory cards
        m_bEditProfile = FALSE;
        g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), TRUE );
        g_StateMgr.ClearSelectedProfile(g_StateMgr.GetPendingProfileIndex() );

        // refresh the profile selection LB
        RefreshProfileList();

        // re-enable dialog
        m_State = DIALOG_STATE_ACTIVE;
    }
}

//=========================================================================

void dlg_profile_select::OnDeleteProfileCB( void )
{
    m_State = DIALOG_STATE_ACTIVE;

    // turn off autosave and reset profile selection
    g_StateMgr.ClearSelectedProfile( 0 );
    g_StateMgr.SetProfileNotSaved( 0, TRUE ); 

    // refresh the profile selection LB
    RefreshProfileList();
}

//=========================================================================

void dlg_profile_select::OnSaveProfileCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), FALSE );
        g_StateMgr.ActivatePendingProfile();
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_SELECT;
    }
    else
    {
        // clear selected profile
        g_StateMgr.ClearSelectedProfile( 0 );
        g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), TRUE ); 

        // refresh the profile selection LB
        RefreshProfileList();

        // return to polling
        m_State = DIALOG_STATE_ACTIVE;
    }

    // get the profile list
    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();
    // get the current list from the save data manager
    g_SaveDataMgr.GetProfileNames( ProfileNames );
}

//=========================================================================

void dlg_profile_select::UpdateBackupPopup(void)
{
    if ( m_BackupPopupResult != DLG_POPUP_IDLE )
    {
        if( m_BackupPopupResult == DLG_POPUP_NO )
        {
            // stay in this dialog
            SetNavTextVisible( TRUE );
            m_State = DIALOG_STATE_ACTIVE;
        }
        else if ( m_BackupPopupResult == DLG_POPUP_YES )
        {
            // Clear the poll callback
            g_SaveDataMgr.CancelCallbacks( this );
            m_State = DIALOG_STATE_BACK;
        }

        m_BackupPopup = NULL;
    }
}

//=========================================================================

void dlg_profile_select::CreateBackupPopup( void )
{
    // open duplicate name error popup
    irect r = g_UiMgr->GetUserBounds( g_UiUserID );
    m_BackupPopup = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );
    m_State = DIALOG_STATE_POPUP;

    // set nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
    navText += g_StringTableMgr("ui", "IDS_NAV_NO" );
    SetNavTextVisible( FALSE );
    m_BackupPopup->Configure( g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ), TRUE, TRUE, FALSE, g_StringTableMgr( "ui", "IDS_ONLINE_DISCONNECT" ), navText, &m_BackupPopupResult );
}

//=========================================================================
