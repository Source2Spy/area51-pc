//==============================================================================
//
//  dlg_ProfileKeyboardBindings.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_listbox.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"

#include "dlg_ProfileKeyboardBindings.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum profile_keyboard_bindings
{
    IDC_KEYBOARD_BINDINGS,
    IDC_KEYBOARD_RESET,
    IDC_KEYBOARD_ACCEPT,
};

//==============================================================================

class keyboard_binding_list : public ui_listbox
{
public:
    keyboard_binding_list( void ) : m_Slot( 0 ), m_HoveredSlot( -1 ) {}

    virtual irect GetListRect( void ) const;
    virtual void Render( s32 ox=0, s32 oy=0 );
    virtual void RenderHeader( irect r );
    virtual void RenderSelection( irect r );
    virtual void RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 );
    virtual void OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY );
    virtual void OnAccept( ui_win* pWin );
    virtual void OnCancel( ui_win* pWin );
    virtual void OnPage( ui_win* pWin, s32 Direction );
    virtual void OnJump( ui_win* pWin, s32 Direction );
    virtual void OnPointerWheel( ui_win* pWin, s32 Delta );
    virtual void OnPointerMove( ui_win* pWin, s32 x, s32 y );
    virtual void OnPointerLeave( ui_win* pWin );
    virtual void OnPointerDown( ui_win* pWin, s32 x, s32 y );

    s32 GetSlot( void ) const { return m_Slot; }

private:
    s32 m_Slot;
    s32 m_HoveredSlot;
};

//==============================================================================
//  DATA
//==============================================================================

ui_manager::control_tem ProfileKeyboardBindings[] =
{
    { IDC_KEYBOARD_BINDINGS, "IDS_NULL",                   "keyboard binding list",  45,  40, 370, 222, 0, 0, 1, 1, ui_win::WF_VISIBLE },
    { IDC_KEYBOARD_RESET,    "IDS_BIND_RESET",             "button",                 45, 270, 220,  30, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_KEYBOARD_ACCEPT,   "IDS_PROFILE_OPTIONS_ACCEPT", "button",                 45, 305, 220,  30, 0, 2, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem ProfileKeyboardBindingsDialog =
{
    "IDS_PROFILE_KEYBOARD_BINDINGS",
    1, 3,
    sizeof(ProfileKeyboardBindings) / sizeof(ui_manager::control_tem),
    &ProfileKeyboardBindings[0],
    0
};

//==============================================================================
//  HELPERS
//==============================================================================

static
ui_win* KeyboardBindingListFactory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    keyboard_binding_list* pList = new keyboard_binding_list;
    pList->Create( UserID, pManager, Position, pParent, Flags );
    return pList;
}

//==============================================================================

static
xbool BindingInputsReleased( const input_snapshot& Snapshot )
{
    for( s32 Key = INPUT_KBD_ESCAPE; Key <= INPUT_KBD_MEDIASELECT; Key++ )
        if( Snapshot.IsPressed( (input_gadget)Key, 0 ) )
            return FALSE;
    if( Snapshot.IsPressed( INPUT_MOUSE_BTN_L, 0 ) )
        return FALSE;
    for( s32 Pad = 0; Pad < frontend_input::MAX_CONTROLLERS; Pad++ )
        for( s32 Action = 0; Action <= frontend_pad::UI_HELP; Action++ )
            if( g_FrontendInput.GetPad( Pad ).GetFrameLogical( Action ).GetIsValue() != 0.0f )
                return FALSE;

    return TRUE;
}

//==============================================================================

static
irect BindingColumn( irect r, s32 Column )
{
    s32 Width = r.GetWidth();
    if( Column == 0 )
        r.r = r.l + Width / 2;
    else
    {
        r.l += Width / 2 + (Column - 1) * Width / 4;
        r.r = r.l + Width / 4;
    }
    r.Deflate( 3, 0 );
    return r;
}

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_profile_keyboard_bindings_register( ui_manager* pManager )
{
    pManager->RegisterWinClass( "keyboard binding list", &KeyboardBindingListFactory );
    pManager->RegisterDialogClass( "profile keyboard bindings", &ProfileKeyboardBindingsDialog, &dlg_profile_keyboard_bindings_factory );
}

//==============================================================================

ui_win* dlg_profile_keyboard_bindings_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_profile_keyboard_bindings* pDialog = new dlg_profile_keyboard_bindings;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return static_cast<ui_win*>( pDialog );
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_profile_keyboard_bindings::dlg_profile_keyboard_bindings( void )
{
}

//==============================================================================

dlg_profile_keyboard_bindings::~dlg_profile_keyboard_bindings( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_profile_keyboard_bindings::Create( s32                       UserID,
                                          ui_manager*               pManager,
                                          ui_manager::dialog_tem*   pDialogTem,
                                          const irect&              Position,
                                          ui_win*                   pParent,
                                          s32                       Flags,
                                          void*                     pUserData )
{
    (void)pUserData;
    ASSERT( pManager );

    xbool const Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    g_UiMgr->DisableScreenHighlight();

    m_pBindings     = static_cast<keyboard_binding_list*>( FindChildByID( IDC_KEYBOARD_BINDINGS ) );
    m_pButtonReset  = static_cast<ui_button*>( FindChildByID( IDC_KEYBOARD_RESET ) );
    m_pButtonAccept = static_cast<ui_button*>( FindChildByID( IDC_KEYBOARD_ACCEPT ) );

    m_OriginalProfile = g_StateMgr.GetPendingProfile();
    m_OriginalProfile.Checksum();
    m_EditingProfile = m_OriginalProfile;

    m_pBindings->SetLineHeight( 24 );
    m_pBindings->EnableHeaderBar();
    m_pBindings->SetHeaderBarColor( xcolor( 0, 0, 0, 0 ) );
    m_pBindings->SetBackgroundColor( xcolor( 19, 59, 14, 160 ) );
    m_pBindings->DisableFrame();
    m_pBindings->EnableParentNavigation();
    m_pBindings->SetExitOnSelect( FALSE );

    for( s32 Action = 0; Action < ingame_pad::KEYBOARD_ACTION_COUNT; Action++ )
        m_pBindings->AddItem( g_StringTableMgr( "ui", ingame_pad::GetKeyboardActionLabel( Action ) ), Action );

    m_pBindings->SetSelection( 0 );
    m_pBindings->SetActive( TRUE );

    m_pButtonReset->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );
    m_pButtonAccept->SetFlag( ui_win::WF_BUTTON_LEFT, TRUE );

    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
    if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
    {
        GotoControl( static_cast<ui_control*>( m_pBindings ) );
        m_CurrentControl = IDC_KEYBOARD_BINDINGS;
    }
    else
    {
        m_CurrentControl = CurrentControl;
    }

    ui_win* Controls[] = { m_pBindings, m_pButtonReset, m_pButtonAccept };
    for( ui_win* pControl : Controls )
        pControl->SetFlag( ui_win::WF_VISIBLE, FALSE );

    m_CurrHL          = 0;
    m_PopUp           = NULL;
    m_PopUpResult     = DLG_POPUP_IDLE;
    m_InputState      = INPUT_WAIT_RELEASE;
    m_CaptureAction   = -1;
    m_CaptureSlot     = -1;
    m_CapturedKey     = INPUT_UNDEFINED;
    m_ConflictPending = FALSE;
    m_ConflictPopup   = FALSE;
    SetNavText( g_StringTableMgr( "ui", "IDS_BIND_NAV" ) );

    InitScreenScaling( Position );
    m_bRenderBlackout = FALSE;
    m_State           = DIALOG_STATE_ACTIVE;
    return Success;
}

//==============================================================================

void dlg_profile_keyboard_bindings::Destroy( void )
{
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_profile_keyboard_bindings::Render( s32 ox, s32 oy )
{
    s32 const Offset = static_cast<s32>( g_UiMgr->GetAlphaTime() * 60.0f ) % 10;
    static s32 const Gap = 9;
    static s32 const Width = 4;
    irect Bounds;

    if( m_bRenderBlackout )
    {
        Bounds = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderGouraudRect( Bounds,
                                    xcolor( 0, 0, 0, 180 ), xcolor( 0, 0, 0, 180 ),
                                    xcolor( 0, 0, 0, 180 ), xcolor( 0, 0, 0, 180 ), FALSE );
    }

    Bounds.l = m_CurrPos.l + 22;
    Bounds.t = m_CurrPos.t + 28;
    Bounds.r = m_CurrPos.r - 23;
    Bounds.b = m_CurrPos.b - 12;
    Bounds.Translate( ox, oy );
    g_UiMgr->RenderGouraudRect( Bounds,
                                xcolor( 56, 115, 58, 64 ), xcolor( 56, 115, 58, 64 ),
                                xcolor( 56, 115, 58, 64 ), xcolor( 56, 115, 58, 64 ), FALSE );

    for( s32 Y = Bounds.t + Offset; Y < Bounds.b; Y += Gap )
    {
        irect const Bar( Bounds.l, Y, Bounds.r, MIN( Y + Width, Bounds.b ) );
        g_UiMgr->RenderGouraudRect( Bar,
                                    xcolor( 56, 115, 58, 30 ), xcolor( 56, 115, 58, 30 ),
                                    xcolor( 56, 115, 58, 30 ), xcolor( 56, 115, 58, 30 ), FALSE );
    }

    ui_dialog::Render( ox, oy );
    g_UiMgr->RenderGlowBar();
}

//==============================================================================

void dlg_profile_keyboard_bindings::OnAccept( ui_win* pWin )
{

    if( !InputEnabled() )
        return;

    if( pWin == m_pBindings )
    {
        m_CaptureAction = m_pBindings->GetSelection();
        m_CaptureSlot = m_pBindings->GetSlot();
        m_InputState = INPUT_WAIT_CAPTURE;
        SetNavText( g_StringTableMgr( "ui", "IDS_BIND_CAPTURE" ) );
        SetButtonsEnabled( FALSE );
        g_AudioMgr.Play( "Select_Norm" );
    }
    else if( pWin == m_pButtonReset )
    {
        ingame_pad::ResetKeyboardBindings( m_EditingProfile.GetKeyboardBindings() );
        g_AudioMgr.Play( "Select_Norm" );
    }
    else if( pWin == m_pButtonAccept )
    {
        g_AudioMgr.Play( "Select_Norm" );
        if( !m_EditingProfile.HasChanged() )
        {
            m_State = DIALOG_STATE_BACK;
            return;
        }

        g_StateMgr.GetPendingProfile() = m_EditingProfile;
        OpenSavePopup();
    }
}

//==============================================================================

void dlg_profile_keyboard_bindings::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( !InputEnabled() )
        return;
    if( !m_EditingProfile.HasChanged() )
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
        return;
    }
    g_StateMgr.GetPendingProfile() = m_EditingProfile;
    OpenSavePopup();
}

//==============================================================================

void dlg_profile_keyboard_bindings::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    if( InputEnabled() )
        ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

//==============================================================================

void dlg_profile_keyboard_bindings::SetButtonsEnabled( xbool Enabled )
{
    m_pButtonReset->SetFlag( ui_win::WF_DISABLED, !Enabled );
    m_pButtonAccept->SetFlag( ui_win::WF_DISABLED, !Enabled );
}

//==============================================================================

void dlg_profile_keyboard_bindings::EndCapture( void )
{
    m_InputState = INPUT_WAIT_RELEASE;
    SetNavText( g_StringTableMgr( "ui", "IDS_BIND_NAV" ) );
}

//==============================================================================

void dlg_profile_keyboard_bindings::OpenConflictPopup( void )
{
    irect const Bounds = g_UiMgr->GetUserBounds( m_UserID );
    m_PopUp = static_cast<dlg_popup*>( g_UiMgr->OpenDialog( m_UserID, "popup", Bounds, NULL,
        ui_win::WF_VISIBLE | ui_win::WF_BORDER | ui_win::WF_DLG_CENTER | ui_win::WF_INPUTMODAL ) );
    xwstring Message = ingame_pad::GetKeyboardKeyLabel( m_CapturedKey );
    Message += g_StringTableMgr( "ui", "IDS_BIND_CONFLICT_MSG" );
    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_BIND_CONFLICT" ), TRUE, TRUE, TRUE,
        Message, g_StringTableMgr( "ui", "IDS_BIND_CONFLICT_NAV" ), &m_PopUpResult );
    m_ConflictPopup   = TRUE;
    m_ConflictPending = FALSE;
    SetNavTextVisible( FALSE );
    m_State = DIALOG_STATE_POPUP;
}

//==============================================================================

void dlg_profile_keyboard_bindings::BeginSave( void )
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
        g_SaveDataMgr.CreateProfile( ProfileIndex, this, &dlg_profile_keyboard_bindings::OnSaveProfileCB );
    }
    else
    {
        profile_info* pProfileInfo = &g_SaveDataMgr.GetProfileInfo( ProfileIndex );
        g_SaveDataMgr.SaveProfile( *pProfileInfo,
                                   ProfileIndex,
                                   this,
                                   &dlg_profile_keyboard_bindings::OnSaveProfileCB );
    }
    m_State = DIALOG_STATE_WAIT_FOR_SAVE_DATA;
}

//==============================================================================

void dlg_profile_keyboard_bindings::OpenSavePopup( void )
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

//==============================================================================

void dlg_profile_keyboard_bindings::RestoreProfile( void )
{
    g_StateMgr.GetPendingProfile() = m_OriginalProfile;
}

//==============================================================================

void dlg_profile_keyboard_bindings::OnSaveProfileCB( void )
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

//==============================================================================

void dlg_profile_keyboard_bindings::OnDelete( ui_win* pWin )
{
    (void)pWin;
    if( InputEnabled() && m_pBindings->IsFocused() )
    {
        ingame_pad::SetKeyboardBinding( m_EditingProfile.GetKeyboardBindings(),
            m_pBindings->GetSelection(), m_pBindings->GetSlot(), INPUT_UNDEFINED, FALSE );
        g_AudioMgr.Play( "Select_Norm" );
    }
}

//==============================================================================

void dlg_profile_keyboard_bindings::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    s32 Highlight = -1;
    const input_snapshot& Snapshot = g_Input.GetFrameSnapshot();

    if( m_PopUp && (m_PopUpResult != DLG_POPUP_IDLE) )
    {
        s32 const Result = m_PopUpResult;
        m_PopUp = NULL;
        m_PopUpResult = DLG_POPUP_IDLE;
        SetNavTextVisible( TRUE );
        if( m_ConflictPopup )
        {
            if( (Result == DLG_POPUP_YES) || (Result == DLG_POPUP_MAYBE) )
                ingame_pad::SetKeyboardBinding( m_EditingProfile.GetKeyboardBindings(),
                    m_CaptureAction, m_CaptureSlot, m_CapturedKey, Result == DLG_POPUP_YES );
            m_ConflictPopup = FALSE;
            m_State = DIALOG_STATE_ACTIVE;
            EndCapture();
        }
        else if( Result == DLG_POPUP_YES )
            BeginSave();
        else
        {
            RestoreProfile();
            g_AudioMgr.Play( "Backup" );
            m_State = DIALOG_STATE_BACK;
        }
    }

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( (m_InputState == INPUT_WAIT_CAPTURE) || (m_InputState == INPUT_WAIT_RELEASE) )
        {
            if( BindingInputsReleased( Snapshot ) )
            {
                if( m_InputState == INPUT_WAIT_CAPTURE )
                    m_InputState = INPUT_CAPTURE;
                else if( m_ConflictPending )
                    OpenConflictPopup();
                else
                {
                    m_InputState = INPUT_READY;
                    SetButtonsEnabled( TRUE );
                }
            }
        }
        else if( m_InputState == INPUT_CAPTURE )
        {
            if( Snapshot.WasPressed( INPUT_KBD_ESCAPE, 0 ) )
                EndCapture();
            else
                for( s32 Key = INPUT_KBD_ESCAPE + 1; Key <= INPUT_KBD_MEDIASELECT; Key++ )
                {
                    input_gadget Gadget = (input_gadget)Key;
                    if( ingame_pad::IsKeyboardKey( Gadget ) && Snapshot.WasPressed( Gadget, 0 ) )
                    {
                        m_CapturedKey = Gadget;
                        m_ConflictPending = ingame_pad::HasKeyboardConflict( m_EditingProfile.GetKeyboardBindings(), m_CaptureAction, Gadget );
                        if( !m_ConflictPending )
                            ingame_pad::SetKeyboardBinding( m_EditingProfile.GetKeyboardBindings(), m_CaptureAction, m_CaptureSlot, Gadget, FALSE );

                        EndCapture();
                        break;
                    }
                }
        }
    }

    if( g_UiMgr->IsScreenScaling() && !UpdateScreenScaling( DeltaTime ) )
    {
        m_pBindings->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonReset->SetFlag( ui_win::WF_VISIBLE, TRUE );
        m_pButtonAccept->SetFlag( ui_win::WF_VISIBLE, TRUE );
    
    s32 const CurrentControl = g_StateMgr.GetCurrentControl();
        if( (CurrentControl == -1) || (GotoControl( CurrentControl ) == NULL) )
        {
            GotoControl( static_cast<ui_control*>( m_pBindings ) );
            m_CurrentControl = IDC_KEYBOARD_BINDINGS;
        }
        else
        {
            m_CurrentControl = CurrentControl;
        }
    }

    g_UiMgr->UpdateGlowBar( DeltaTime );

    ui_win* Controls[] = { m_pBindings, m_pButtonReset, m_pButtonAccept };
    for( s32 iControl = 0; iControl < 3; ++iControl )
    {
        if( Controls[iControl]->IsFocused() )
        {
            Highlight = iControl;
            if( Controls[iControl] == m_pBindings )
                g_UiMgr->DisableScreenHighlight();
            else
                g_UiMgr->SetScreenHighlight( Controls[iControl]->GetPosition() );
        }
    }

    if( Highlight != m_CurrHL )
    {
        if( Highlight != -1 )
            g_AudioMgr.Play( "Cusor_Norm" );
        m_CurrHL = Highlight;
    }
}

//==============================================================================

xbool dlg_profile_keyboard_bindings::IsCapturing( s32 Action, s32 Slot ) const
{
    return ((m_InputState == INPUT_WAIT_CAPTURE) || (m_InputState == INPUT_CAPTURE)) &&
           (Action == m_CaptureAction) && (Slot == m_CaptureSlot);
}

//==============================================================================

irect keyboard_binding_list::GetListRect( void ) const
{
    irect Bounds = ui_listbox::GetListRect();
    Bounds.r -= 1;
    return Bounds;
}

//==============================================================================

void keyboard_binding_list::Render( s32 ox, s32 oy )
{
    ui_listbox::Render( ox, oy );
    if( m_Flags & WF_VISIBLE )
    {
        irect Bounds = GetListRect();
        Bounds.Translate( ox, oy );
        m_pManager->RenderRect( Bounds, xcolor( 79, 214, 60, 224 ), TRUE );
    }
}

//==============================================================================

void keyboard_binding_list::RenderHeader( irect r )
{
    irect Header = r;
    Header.Deflate( 1, 1 );
    Header.SetHeight( r.GetHeight() - 1 );
    m_pManager->RenderGouraudRect( Header,
        xcolor( 39, 117, 28, 224 ), xcolor( 39, 117, 28, 224 ),
        xcolor( 19, 59, 14, 128 ), xcolor( 19, 59, 14, 128 ), FALSE );
    const char* Labels[] = { "IDS_BIND_ACTION", "IDS_BIND_PRIMARY", "IDS_BIND_SECONDARY" };
    for( s32 Column = 0; Column < 3; Column++ )
        m_pManager->RenderText( m_Font, BindingColumn( r, Column ),
            ((Column == 0) ? ui_font::h_left : ui_font::h_center) | ui_font::v_center,
            m_HeaderColor, g_StringTableMgr( "ui", Labels[Column] ) );
}

//==============================================================================

void keyboard_binding_list::RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 )
{
    (void)c1;
    (void)c2;

    const dlg_profile_keyboard_bindings* pDialog = static_cast<dlg_profile_keyboard_bindings*>( m_pParent );
    s32 Action = (s32)Item.Data[0];
    xbool Selected = (Action == GetSelection()) && IsFocused();
    s32 TextFlags = ui_font::h_left | ui_font::v_center | ui_font::clip_ellipsis | ui_font::clip_l_justify;

    m_pManager->RenderText( m_Font, BindingColumn( r, 0 ), TextFlags, Item.Color, Item.Label );

    TextFlags = ui_font::h_center | ui_font::v_center | ui_font::clip_ellipsis;
    for( s32 Slot = 0; Slot < ingame_pad::KEYBOARD_SLOT_COUNT; Slot++ )
    {
        irect Bounds = BindingColumn( r, Slot + 1 );
        input_gadget Key = (input_gadget)pDialog->GetBindings().Keys[Action][Slot];
        xcolor Color = (Key == INPUT_UNDEFINED) ? xcolor( 126, 166, 126, 255 ) : Item.Color;

        if( Selected && (Slot == m_Slot) )
        {
            s32 Alpha = 128 + m_pManager->GetHighlightAlpha( 8 ) * 8;
            m_pManager->RenderRect( Bounds, xcolor( 79, 214, 60, Alpha ), FALSE );
            Color = XCOLOR_BLACK;
        }
        else if( (Action == m_HoveredItem) && (Slot == m_HoveredSlot) )
        {
            m_pManager->RenderRect( Bounds, xcolor( 79, 214, 60, 48 ), FALSE );
        }

        xwstring Label;
        if( pDialog->IsCapturing( Action, Slot ) )
            Label = g_StringTableMgr( "ui", "IDS_BIND_PRESS_KEY" );
        else
            Label = ingame_pad::GetKeyboardKeyLabel( Key );

        m_pManager->RenderText( m_Font, Bounds, TextFlags, Color, Label );
    }
}

//==============================================================================

void keyboard_binding_list::RenderSelection( irect r )
{
    m_pManager->RenderRect( r, xcolor( 79, 214, 60, IsFocused() ? 96 : 48 ), FALSE );
}

//==============================================================================

void keyboard_binding_list::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    if( !static_cast<dlg_profile_keyboard_bindings*>( m_pParent )->InputEnabled() )
        return;
    if( (Code == ui_navigation::Left) || (Code == ui_navigation::Right) )
    {
        s32 PreviousSlot = m_Slot;
        m_Slot = (Code == ui_navigation::Left) ? 0 : 1;
        if( m_Slot != PreviousSlot )
            g_AudioMgr.Play( "Cusor_Norm" );
    }
    else
        ui_listbox::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

//==============================================================================

void keyboard_binding_list::OnAccept( ui_win* pWin )
{
    (void)pWin;
    m_pParent->OnAccept( this );
}

//==============================================================================

void keyboard_binding_list::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    if( !static_cast<dlg_profile_keyboard_bindings*>( m_pParent )->InputEnabled() )
        return;
    irect Bounds = GetListRect();
    Bounds.Translate( -m_Position.l, -m_Position.t );
    LocalToScreen( Bounds );
    if( Bounds.PointInRect( x, y ) && (GetItemAt( x, y ) >= 0) )
        m_Slot = (x >= Bounds.l + Bounds.GetWidth() * 3 / 4) ? 1 : 0;
    ui_listbox::OnPointerDown( pWin, x, y );
}

//==============================================================================

void keyboard_binding_list::OnCancel( ui_win* pWin )
{
    m_pParent->OnCancel( pWin );
}

//==============================================================================

void keyboard_binding_list::OnPage( ui_win* pWin, s32 Direction )
{
    if( static_cast<dlg_profile_keyboard_bindings*>( m_pParent )->InputEnabled() )
        ui_listbox::OnPage( pWin, Direction );
}

//==============================================================================

void keyboard_binding_list::OnJump( ui_win* pWin, s32 Direction )
{
    if( static_cast<dlg_profile_keyboard_bindings*>( m_pParent )->InputEnabled() )
        ui_listbox::OnJump( pWin, Direction );
}

//==============================================================================

void keyboard_binding_list::OnPointerWheel( ui_win* pWin, s32 Delta )
{
    if( static_cast<dlg_profile_keyboard_bindings*>( m_pParent )->InputEnabled() )
        ui_listbox::OnPointerWheel( pWin, Delta );
}

//==============================================================================

void keyboard_binding_list::OnPointerMove( ui_win* pWin, s32 x, s32 y )
{
    m_HoveredSlot = -1;
    if( static_cast<dlg_profile_keyboard_bindings*>( m_pParent )->InputEnabled() )
    {
        ui_listbox::OnPointerMove( pWin, x, y );
        irect Bounds = GetListRect();
        Bounds.Translate( -m_Position.l, -m_Position.t );
        LocalToScreen( Bounds );
        if( (m_HoveredItem != -1) && Bounds.PointInRect( x, y ) &&
            (x >= Bounds.l + Bounds.GetWidth() / 2) )
        {
            m_HoveredSlot = (x >= Bounds.l + Bounds.GetWidth() * 3 / 4) ? 1 : 0;
        }
    }
}

//==============================================================================

void keyboard_binding_list::OnPointerLeave( ui_win* pWin )
{
    m_HoveredSlot = -1;
    ui_listbox::OnPointerLeave( pWin );
}

//==============================================================================
