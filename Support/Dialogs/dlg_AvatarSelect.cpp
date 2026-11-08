//=========================================================================
//
//  dlg_avatar_select.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"

#include "dlg_PopUp.hpp"
#include "dlg_AvatarSelect.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================

enum controls
{
    IDC_AVATAR_SELECT_COMBO,
    IDC_AVATAR_SELECT_BUTTON_ACCEPT,    
};


ui_manager::control_tem AvatarSelectControls[] = 
{
    // Frames.
    { IDC_AVATAR_SELECT_COMBO,          "IDS_NULL",    "combo",  71,  50, 128, 256, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_AVATAR_SELECT_BUTTON_ACCEPT,  "IDS_ACCEPT",  "button", 71, 315, 128,  22, 0, 1, 1, 1, ui_win::WF_VISIBLE },
};

ui_manager::dialog_tem AvatarSelectDialog =
{
    "IDS_AVATAR_MENU",
    1, 9,
    sizeof(AvatarSelectControls)/sizeof(ui_manager::control_tem),
    &AvatarSelectControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_avatar_select_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "avatar select", &AvatarSelectDialog, &dlg_avatar_select_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_avatar_select_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_avatar_select* pDialog = new dlg_avatar_select;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_level_select
//=========================================================================

dlg_avatar_select::dlg_avatar_select( void )
{
}

//=========================================================================

dlg_avatar_select::~dlg_avatar_select( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_avatar_select::Create( s32                        UserID,
                             ui_manager*                pManager,
                             ui_manager::dialog_tem*    pDialogTem,
                             const irect&               Position,
                             ui_win*                    pParent,
                             s32                        Flags,
                             void*                      pUserData )
{
          xbool   Success  = FALSE;
    const xwchar* pNullStr = g_StringTableMgr( "ui", "IDS_NULL" );

    (void)pUserData;

    ASSERT( pManager );

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    // find controls
    m_pAvatarSelect  = (ui_combo*)   FindChildByID( IDC_AVATAR_SELECT_COMBO    );
    m_pButtonAccept  = (ui_button*)  FindChildByID( IDC_AVATAR_SELECT_BUTTON_ACCEPT );

    // hide them
    m_pAvatarSelect ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pButtonAccept ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_ACCEPT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    navText += g_StringTableMgr( "ui", "IDS_NAV_CYCLE_AVATAR" );
   
    SetNavText( navText );

    // check profile to see if alien avatars are unlocked
    player_profile& Profile = g_StateMgr.GetPendingProfile();

    // load avatars
    s32 ID0  = g_UiMgr->LoadBitmap( "avatar0",  "AVATARS/UI_AVATAR_HAZ_1.XBMP"  );
    s32 ID1  = g_UiMgr->LoadBitmap( "avatar1",  "AVATARS/UI_AVATAR_HAZ_2.XBMP"  );
    s32 ID2  = g_UiMgr->LoadBitmap( "avatar2",  "AVATARS/UI_AVATAR_HAZ_3.XBMP"  );
    s32 ID3  = g_UiMgr->LoadBitmap( "avatar3",  "AVATARS/UI_AVATAR_HAZ_4.XBMP"  );
    s32 ID4  = g_UiMgr->LoadBitmap( "avatar4",  "AVATARS/UI_AVATAR_SPEC_1.XBMP" );
    s32 ID5  = g_UiMgr->LoadBitmap( "avatar5",  "AVATARS/UI_AVATAR_SPEC_2.XBMP" );
    s32 ID6  = g_UiMgr->LoadBitmap( "avatar6",  "AVATARS/UI_AVATAR_SPEC_3.XBMP" );
    s32 ID7  = g_UiMgr->LoadBitmap( "avatar7",  "AVATARS/UI_AVATAR_SPEC_4.XBMP" );
    s32 ID8  = g_UiMgr->LoadBitmap( "avatar8",  "AVATARS/UI_AVATAR_TECH_1.XBMP" );
    s32 ID9  = g_UiMgr->LoadBitmap( "avatar9",  "AVATARS/UI_AVATAR_TECH_2.XBMP" );
    s32 ID10 = g_UiMgr->LoadBitmap( "avatar10", "AVATARS/UI_AVATAR_TECH_3.XBMP" );
    s32 ID11 = g_UiMgr->LoadBitmap( "avatar11", "AVATARS/UI_AVATAR_TECH_4.XBMP" );
    s32 ID12 = g_UiMgr->LoadBitmap( "avatar12", "AVATARS/UI_AVATAR_SCI_1.XBMP"  );
    s32 ID13 = g_UiMgr->LoadBitmap( "avatar13", "AVATARS/UI_AVATAR_SCI_2.XBMP"  );
    s32 ID14 = g_UiMgr->LoadBitmap( "avatar14", "AVATARS/UI_AVATAR_SCI_3.XBMP"  );
    s32 ID15 = g_UiMgr->LoadBitmap( "avatar15", "AVATARS/UI_AVATAR_SCI_4.XBMP"  );

    // set up selections
    m_pAvatarSelect->DeleteAllItems();
    m_pAvatarSelect->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pAvatarSelect->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV);

    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_HAZMAT_0  );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_HAZMAT_1  );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_HAZMAT_2  );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_HAZMAT_3  );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_SPECFOR_0 );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_SPECFOR_1 );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_SPECFOR_2 );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_SPECFOR_3 );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_TECH_0    );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_TECH_1    );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_TECH_2    );
    m_pAvatarSelect->AddItem  ( pNullStr, SKIN_TECH_3    );

    if( Profile.GetAlienAvatarsOn() )
    {
        m_pAvatarSelect->AddItem  ( pNullStr, SKIN_GREY_0    );
        m_pAvatarSelect->AddItem  ( pNullStr, SKIN_GREY_1    );
        m_pAvatarSelect->AddItem  ( pNullStr, SKIN_GREY_2    );
        m_pAvatarSelect->AddItem  ( pNullStr, SKIN_GREY_3    );
    }

    m_pAvatarSelect->SetItemBitmap(  0, ID0  ); 
    m_pAvatarSelect->SetItemBitmap(  1, ID1  );
    m_pAvatarSelect->SetItemBitmap(  2, ID2  );
    m_pAvatarSelect->SetItemBitmap(  3, ID3  );
    m_pAvatarSelect->SetItemBitmap(  4, ID4  );
    m_pAvatarSelect->SetItemBitmap(  5, ID5  );
    m_pAvatarSelect->SetItemBitmap(  6, ID6  );
    m_pAvatarSelect->SetItemBitmap(  7, ID7  );
    m_pAvatarSelect->SetItemBitmap(  8, ID8  );
    m_pAvatarSelect->SetItemBitmap(  9, ID9  );
    m_pAvatarSelect->SetItemBitmap( 10, ID10 );
    m_pAvatarSelect->SetItemBitmap( 11, ID11 );

    if( Profile.GetAlienAvatarsOn() )
    {
        m_pAvatarSelect->SetItemBitmap( 12, ID12 );
        m_pAvatarSelect->SetItemBitmap( 13, ID13 );
        m_pAvatarSelect->SetItemBitmap( 14, ID14 );
        m_pAvatarSelect->SetItemBitmap( 15, ID15 );
    }

    // get the pending profile
    player_profile& CurrentProfile = g_StateMgr.GetPendingProfile();
    s32 AvatarID = CurrentProfile.GetAvatarID();
    ASSERTS( AvatarID < SM_MAX_AVATARS, "AvatarID out of range!" );
    // find the avatar in the list
    for( s32 i=0; i<SM_MAX_AVATARS; i++ )
    {
        if( m_pAvatarSelect->GetItemData(i) == AvatarID )
        {
            m_pAvatarSelect->SetSelection( i ); 
            break;
        }
    }

    // set initial focus
    m_CurrHL = 0;
    GotoControl( (ui_control*)m_pAvatarSelect );
    m_CurrentControl = IDC_AVATAR_SELECT_COMBO;
    m_OriginalProfile = g_StateMgr.GetPendingProfile();
    m_OriginalProfile.Checksum();
    m_PopUp = NULL;
    m_PopUpResult = DLG_POPUP_IDLE;
    m_bRenderBlackout = FALSE;

    // initialize screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_avatar_select::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();

    // unload avatars
    g_UiMgr->UnloadBitmap( "avatar0"  );
    g_UiMgr->UnloadBitmap( "avatar1"  );
    g_UiMgr->UnloadBitmap( "avatar2"  );
    g_UiMgr->UnloadBitmap( "avatar3"  );
    g_UiMgr->UnloadBitmap( "avatar4"  );
    g_UiMgr->UnloadBitmap( "avatar5"  );
    g_UiMgr->UnloadBitmap( "avatar6"  );
    g_UiMgr->UnloadBitmap( "avatar7"  );
    g_UiMgr->UnloadBitmap( "avatar8"  );
    g_UiMgr->UnloadBitmap( "avatar9"  );
    g_UiMgr->UnloadBitmap( "avatar10" );
    g_UiMgr->UnloadBitmap( "avatar11" );
    g_UiMgr->UnloadBitmap( "avatar12" );
    g_UiMgr->UnloadBitmap( "avatar13" );
    g_UiMgr->UnloadBitmap( "avatar14" );
    g_UiMgr->UnloadBitmap( "avatar15" );

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_avatar_select::Render( s32 ox, s32 oy )
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

    // render the avatar
    // do this here, or do it in the UI somewhere.... TBD

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}
//=========================================================================

void dlg_avatar_select::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        // check for changing sort key
        switch( Code )
        {
            case ui_navigation::Left:
                // get the previous avatar
                break;
            case ui_navigation::Right:
                // get the next avatar
                break;
        }
    }
}

//=========================================================================

void dlg_avatar_select::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        player_profile Candidate = g_StateMgr.GetPendingProfile();
        Candidate.SetAvatarID( m_pAvatarSelect->GetSelectedItemData( 0 ) );

        if( Candidate.HasChanged() )
        {
            g_StateMgr.GetPendingProfile() = Candidate;
            OpenSavePopup();
            return;
        }

        g_AudioMgr.Play("Backup");
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_avatar_select::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pButtonAccept )
        {
            player_profile Candidate = g_StateMgr.GetPendingProfile();
            Candidate.SetAvatarID( m_pAvatarSelect->GetSelectedItemData( 0 ) );

            if( !Candidate.HasChanged() )
            {
                g_AudioMgr.Play( "Select_Norm" );
                m_State = DIALOG_STATE_BACK;
                return;
            }

            g_StateMgr.GetPendingProfile() = Candidate;
            g_AudioMgr.Play("Select_Norm");
            OpenSavePopup();
        }       
    }
}

//=========================================================================

void dlg_avatar_select::BeginSave( void )
{
    s32 const ProfileIndex = g_StateMgr.GetPendingProfileIndex();
    if( (ProfileIndex < 0) || (ProfileIndex >= SM_PROFILE_COUNT) )
    {
        RestoreProfile();
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
        return;
    }

    if( g_StateMgr.GetProfileNotSaved( ProfileIndex ) )
    {
        g_StateMgr.GetPendingProfile().SetHash();
        g_StateMgr.SetSelectedProfile( ProfileIndex, g_StateMgr.GetPendingProfile().GetHash() );
        g_SaveDataMgr.CreateProfile( ProfileIndex, this, &dlg_avatar_select::OnSaveProfileCB );
    }
    else
    {
        profile_info* pProfileInfo = &g_SaveDataMgr.GetProfileInfo( ProfileIndex );
        g_SaveDataMgr.SaveProfile( *pProfileInfo,
                                   ProfileIndex,
                                   this,
                                   &dlg_avatar_select::OnSaveProfileCB );
    }

    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//=========================================================================

void dlg_avatar_select::OpenSavePopup( void )
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
    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_PROFILE_EDIT" ),
                        TRUE,
                        TRUE,
                        FALSE,
                        g_StringTableMgr( "ui", "IDS_PROFILE_EDIT_MSG" ),
                        PopupNavText,
                        &m_PopUpResult );
    m_State = DIALOG_STATE_POPUP;
}

//=========================================================================

void dlg_avatar_select::RestoreProfile( void )
{
    g_StateMgr.GetPendingProfile() = m_OriginalProfile;
}

//=========================================================================

void dlg_avatar_select::OnSaveProfileCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        s32 const ProfileIndex = g_StateMgr.GetPendingProfileIndex();
        g_StateMgr.SetProfileNotSaved( ProfileIndex, FALSE );
        g_StateMgr.ActivatePendingProfile();
        g_StateMgr.InitPendingProfile( ProfileIndex );
        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_BACK;
    }
    else
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_SAVE_DATA_ERROR;
    }
}

//=========================================================================

void dlg_avatar_select::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    s32 highLight = -1;

    if( m_PopUp && (m_PopUpResult != DLG_POPUP_IDLE) )
    {
        s32 const Result = m_PopUpResult;
        m_PopUp = NULL;
        m_PopUpResult = DLG_POPUP_IDLE;
        SetNavTextVisible( TRUE );

        if( Result == DLG_POPUP_YES )
        {
            BeginSave();
        }
        else
        {
            RestoreProfile();
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the controls
            m_pAvatarSelect ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pButtonAccept ->SetFlag(ui_win::WF_VISIBLE, TRUE); 
            GotoControl( (ui_control*)m_pAvatarSelect );
            g_UiMgr->SetScreenHighlight( m_pAvatarSelect->GetPosition() );
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);
    
    // update labels    
    if( m_pAvatarSelect->IsFocused() )
    {
        highLight = 0;
        g_UiMgr->SetScreenHighlight( m_pAvatarSelect->GetPosition() );
    }    
    
    if( m_pButtonAccept->IsFocused() )
    {
        highLight = 1;
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
