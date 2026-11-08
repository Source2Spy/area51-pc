//=========================================================================
//
//  dlg_secrets_menu.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_blankbox.hpp"
#include "UI/ui_textbox.hpp"

#include "dlg_SecretsMenu.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "StateMgr/MapList.hpp"
#include "StateMgr/SecretList.hpp"
#include "MoviePlayer/MoviePlayer.hpp"

extern xstring SelectBestClip( const char* pName );

//=========================================================================
//  Main Menu Dialog
//=========================================================================

enum controls
{   
    IDC_SECRETS_MAIN,
    IDC_SECRETS_DETAILS,
    IDC_SECRETS_SELECT,
    IDC_SECRETS_BUTTON_1,
    IDC_SECRETS_BUTTON_2,
    IDC_SECRETS_BUTTON_3,
    IDC_SECRETS_BUTTON_4,
    IDC_SECRETS_BUTTON_5,
    IDC_SECRETS_TEXT_1,
    IDC_SECRETS_TEXT_2,
    IDC_SECRETS_TEXT_3,
    IDC_SECRETS_TEXTBOX,
};

//-------------------------------------------------------------------------

ui_manager::control_tem SecretsMenuControls[] = 
{
    // Frames.

    { IDC_SECRETS_SELECT,      "IDS_NULL",    "combo",      138,  40, 220,  40, 0, 0, 5, 1, ui_win::WF_VISIBLE },

    { IDC_SECRETS_MAIN,        "IDS_NULL",    "blankbox",    40,  80, 416, 144, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_SECRETS_BUTTON_1,    "IDS_NULL",    "button",      56, 130,  64,  64, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SECRETS_BUTTON_2,    "IDS_NULL",    "button",     136, 130,  64,  64, 1, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SECRETS_BUTTON_3,    "IDS_NULL",    "button",     216, 130,  64,  64, 2, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SECRETS_BUTTON_4,    "IDS_NULL",    "button",     296, 130,  64,  64, 3, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_SECRETS_BUTTON_5,    "IDS_NULL",    "button",     376, 130,  64,  64, 4, 1, 1, 1, ui_win::WF_VISIBLE },

    { IDC_SECRETS_DETAILS,     "IDS_NULL",    "blankbox",    40, 240, 416,  94, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_SECRETS_TEXT_1,      "IDS_NULL",    "text",        48, 262, 400,  94, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_SECRETS_TEXT_2,      "IDS_NULL",    "text",        48, 278,  90,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_SECRETS_TEXT_3,      "IDS_NULL",    "text",        48, 294,  90,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_SECRETS_TEXTBOX,     "IDS_NULL",    "textbox",     60, 240, 376,  93, 0, 2, 5, 1, ui_win::WF_VISIBLE },

};

//-------------------------------------------------------------------------

ui_manager::dialog_tem SecretsMenuDialog =
{
    "IDS_SECRETS_MENU_TITLE",
    5, 9,
    sizeof(SecretsMenuControls)/sizeof(ui_manager::control_tem),
    &SecretsMenuControls[0],
    0
};

//=========================================================================
//  Data
//=========================================================================

static xbool s_Scaled = FALSE;

//=========================================================================
//  Registration function
//=========================================================================

void dlg_secrets_menu_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "secrets", &SecretsMenuDialog, &dlg_secrets_menu_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_secrets_menu_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_secrets_menu* pDialog = new dlg_secrets_menu;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_secrets_menu
//=========================================================================

dlg_secrets_menu::dlg_secrets_menu( void )
{
}

//=========================================================================

dlg_secrets_menu::~dlg_secrets_menu( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_secrets_menu::Create( s32                        UserID,
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

    m_CurrHL = 0;
    s_Scaled = FALSE;

    // load bitmaps
    m_SecretsIconID[SECRET_TYPE_VIDEO]   = g_UiMgr->LoadBitmap( "SecretsVideo",  "LORE/INTERFACE/UI_LOREVIDEO.XBMP"   );  // TODO: get some new bitmaps
    m_SecretsIconID[SECRET_TYPE_CHEAT]   = g_UiMgr->LoadBitmap( "SecretsCheat",  "VARIANTS/COMMON/LORE/INTERFACE/UI_LOREAUDIO.XBMP"   );  // for the secrets icons
    m_SecretsIconID[SECRET_TYPE_UNKNOWN] = g_UiMgr->LoadBitmap( "SecretsNull",   "VARIANTS/COMMON/LORE/INTERFACE/UI_LOREUNKNOWN.XBMP" );

    // set up nav text 
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_BACK" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_CYCLE_VAULT" );
    SetNavText( navText );

    // setup secrets main box
    m_pSecretsMain = (ui_blankbox*)FindChildByID( IDC_SECRETS_MAIN );
    m_pSecretsMain->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsMain->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pSecretsMain->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pSecretsMain->SetHasTitleBar( TRUE );
    m_pSecretsMain->SetLabel( g_StringTableMgr( "ui", "IDS_SECRETS_VAULT" ) );
    m_pSecretsMain->SetLabelColor( xcolor(255,252,204,255) );
    m_pSecretsMain->SetTitleBarColor( xcolor(19,59,14,196) );

    // setup secrets details box
    m_pSecretsDetails = (ui_blankbox*)FindChildByID( IDC_SECRETS_DETAILS );
    m_pSecretsDetails->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsDetails->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pSecretsDetails->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pSecretsDetails->SetHasTitleBar( TRUE );
    m_pSecretsDetails->SetLabel( g_StringTableMgr( "ui", "IDS_SECRETS_DETAILS" ) );
    m_pSecretsDetails->SetLabelColor( xcolor(255,252,204,255) );
    m_pSecretsDetails->SetTitleBarColor( xcolor(19,59,14,196) );

    // set up textbox
    m_pTextBox = (ui_textbox*)FindChildByID( IDC_SECRETS_TEXTBOX );
    
    m_pTextBox->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pTextBox->SetFlag( ui_win::WF_DISABLED, TRUE );
    m_pTextBox->SetExitOnBack( TRUE );
    m_pTextBox->SetExitOnSelect( TRUE );
    m_pTextBox->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pTextBox->DisableFrame();
    m_pTextBox->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    
    // set up secrets combo
    m_pSecretsSelect = (ui_combo*)FindChildByID( IDC_SECRETS_SELECT );
    m_pSecretsSelect->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV | ui_combo::CB_CHANGE_ON_SELECT | ui_combo::CB_NOTIFY_PARENT );
    m_pSecretsSelect->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsSelect->AddItem( g_StringTableMgr( "ui", "IDS_SECRETS_VAULT_1" ), 0 );
    m_pSecretsSelect->AddItem( g_StringTableMgr( "ui", "IDS_SECRETS_VAULT_2" ), 1 );
    m_pSecretsSelect->AddItem( g_StringTableMgr( "ui", "IDS_SECRETS_VAULT_3" ), 2 );
    m_pSecretsSelect->AddItem( g_StringTableMgr( "ui", "IDS_SECRETS_VAULT_4" ), 3 );
    m_pSecretsSelect->AddItem( g_StringTableMgr( "ui", "IDS_SECRETS_VAULT_5" ), 4 );

    // set up buttons
    m_pSecretsButton[0]  = (ui_button*) FindChildByID( IDC_SECRETS_BUTTON_1 );
    m_pSecretsButton[1]  = (ui_button*) FindChildByID( IDC_SECRETS_BUTTON_2 );
    m_pSecretsButton[2]  = (ui_button*) FindChildByID( IDC_SECRETS_BUTTON_3 );
    m_pSecretsButton[3]  = (ui_button*) FindChildByID( IDC_SECRETS_BUTTON_4 );
    m_pSecretsButton[4]  = (ui_button*) FindChildByID( IDC_SECRETS_BUTTON_5 );

    m_pSecretsButton[0]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsButton[1]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsButton[2]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsButton[3]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsButton[4]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);


    m_pSecretsLine1    = (ui_text*)FindChildByID( IDC_SECRETS_TEXT_1 );
    m_pSecretsLine2    = (ui_text*)FindChildByID( IDC_SECRETS_TEXT_2 );
    m_pSecretsLine3    = (ui_text*)FindChildByID( IDC_SECRETS_TEXT_3 );
    
    m_pSecretsLine1    ->UseSmallText( TRUE );
    m_pSecretsLine2    ->UseSmallText( TRUE );
    m_pSecretsLine3    ->UseSmallText( TRUE );

    m_pSecretsLine1    ->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    m_pSecretsLine2    ->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    m_pSecretsLine3    ->SetLabelFlags( ui_font::h_left|ui_font::v_top );

    m_pSecretsLine1    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsLine2    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pSecretsLine3    ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    m_pSecretsLine1    ->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pSecretsLine2    ->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pSecretsLine3    ->SetFlag(ui_win::WF_STATIC, TRUE);

    m_pSecretsLine1    ->SetLabelColor( xcolor(255,252,204,255) );
    m_pSecretsLine2    ->SetLabelColor( xcolor(255,252,204,255) );
    m_pSecretsLine3    ->SetLabelColor( xcolor(255,252,204,255) );

    m_pSelectedIcon     = m_pSecretsButton[0];

    // set focus
    GotoControl( (ui_control*)m_pSecretsSelect );

    // Initialize popup
    m_PopUp = NULL;

    // Initialize icon scaling
    m_scaleCount  = 0.0f;
    m_bScreenIsOn = FALSE;
    m_bScaleDown  = FALSE;
    m_TimeOut     = 0.0f;

    // get the active player profile
    player_profile& Profile = g_StateMgr.GetActiveProfile( 0 );
    // clear the new secret flag
    Profile.ClearNewSecretUnlocked();
    // checksum profile to prevent unwanted "changed" messages appearing
    Profile.Checksum();

    // initialize screen scaling
    InitScreenScaling( Position );

    m_pSecretsSelect->SetSelection( 0 );
    PopulateSecretsDetails( TRUE );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_secrets_menu::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();

    // unload secrets bitmaps
    g_UiMgr->UnloadBitmap( "SecretsVideo" );
    g_UiMgr->UnloadBitmap( "SecretsCheat" );
    g_UiMgr->UnloadBitmap( "SecretsNull" );
    g_UiMgr->UnloadBitmap( "Still" );
}

//=========================================================================

void dlg_secrets_menu::Render( s32 ox, s32 oy )
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

    xbool const IsTextBoxVisible = (m_pTextBox->GetFlags( ui_win::WF_VISIBLE ) != 0);
    if( IsTextBoxVisible )
    {
        m_pTextBox->SetFlag( ui_win::WF_VISIBLE, FALSE );
    }

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    if( IsTextBoxVisible )
    {
        m_pTextBox->SetFlag( ui_win::WF_VISIBLE, TRUE );
    }

    // render the glow bar
    g_UiMgr->RenderGlowBar();

    if( m_bScreenIsOn )
    {
        irect const UserBounds = g_UiMgr->GetUserBounds( m_UserID );
        g_UiMgr->RenderRect( UserBounds, xcolor( 0, 0, 0, m_FadeLevel ), FALSE );
    }

    // render the popup screen (if any)
    if( m_bScreenIsOn )
    {                    
        // render border
        g_UiMgr->RenderRect( m_DrawPos, xcolor(255,252,204,255), FALSE );

        // render movie/button bitmap
        if( m_TimeOut || (m_scaleCount && m_bScaleDown) )
        { 
            irect r = m_DrawPos;
            r.t += 2;
            r.l += 2;
            r.b -= 2;
            r.r -= 2;

            m_pManager->RenderBitmap( m_pSelectedIcon->GetBitmap(), r, xcolor(255,252,204,255) );
        }
        else
        {
            irect r = m_DrawPos;
            r.t += 2;
            r.l += 2;
            r.b -= 2;
            r.r -= 2;

            m_pManager->RenderBitmap( m_StillBitmapID, r, XCOLOR_WHITE );
        }

        if( IsTextBoxVisible )
        {
            m_pTextBox->Render( m_Position.l + ox, m_Position.t + oy );
        }
    }
}


//=========================================================================

void dlg_secrets_menu::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    // only allow navigation if active
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pSecretsSelect )
        {
            switch( Code )
            {
                case ui_navigation::Left:
                case ui_navigation::Right:
                    PopulateSecretsDetails( TRUE );                   
                    return;
            }
        }
        ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
    }
}

//=========================================================================

void dlg_secrets_menu::OnNotify( ui_notification const& Event )
{
    (void)Event.m_pText;

    if( Event.m_pSender == (ui_win*)m_pSecretsSelect )
    {
        if( Event.m_Type == ui_notification_type::ComboSelectionChanged )
        {
            if( !s_Scaled && (m_State == DIALOG_STATE_ACTIVE) )
            {
                PopulateSecretsDetails( TRUE );
            }
        }
    }
}

//=========================================================================

void dlg_secrets_menu::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pSecretsSelect )
        {
            PopulateSecretsDetails( TRUE );
            return;
        }

        // handle secrets item selection here!
        if( pWin == (ui_win*)m_pSecretsButton[0] )
        {
            if( m_pSecretsButton[0]->GetBitmap() == m_SecretsIconID[SECRET_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pSecretsButton[0];
            m_SelectedIndex = 0;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pSecretsButton[1] )
        {
            if( m_pSecretsButton[1]->GetBitmap() == m_SecretsIconID[SECRET_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pSecretsButton[1];
            m_SelectedIndex = 1;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pSecretsButton[2] )
        {
            if( m_pSecretsButton[2]->GetBitmap() == m_SecretsIconID[SECRET_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pSecretsButton[2];
            m_SelectedIndex = 2;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pSecretsButton[3] )
        {
            if( m_pSecretsButton[3]->GetBitmap() == m_SecretsIconID[SECRET_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pSecretsButton[3];
            m_SelectedIndex = 3;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pSecretsButton[4] )
        {
            if( m_pSecretsButton[4]->GetBitmap() == m_SecretsIconID[SECRET_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pSecretsButton[4];
            m_SelectedIndex = 4;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
    }
    else if( m_State == DIALOG_STATE_ACTIVATE )
    {
        if( m_scaleCount == 0 )
        {
            const secret_entry* Entry = g_SecretList.GetByIndex( m_pSelectedIcon->GetData() );
            ASSERT( Entry );

            // if this is a movie, then play it
            if( Entry->SecretType == SECRET_TYPE_VIDEO )
            {
                g_StateMgr.RequestSimpleMovie( SelectBestClip(m_FileName) );
                InitIconScaling( TRUE );
            }

            // if this is cheat activate it?
            if( Entry->SecretType == SECRET_TYPE_CHEAT )
            {
                // activate/deactivate cheat here???
            }
        }
    }
}

//=========================================================================

void dlg_secrets_menu::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_scaleCount )
    {
        // currently scaling an icon
        return;
    }

    switch( m_State )
    {
        case DIALOG_STATE_ACTIVE:
        {
            // exit main dialog
            g_AudioMgr.Play("Backup");
            m_State = DIALOG_STATE_BACK;
        }
        break;

        case DIALOG_STATE_ACTIVATE:
        {
            // exit sub menu / movie player
            InitIconScaling( TRUE );            
        }
        break;
    }
    
}

//=========================================================================

void dlg_secrets_menu::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    if( m_bScreenIsOn && (m_scaleCount == 0) )
    {
        if( m_DrawPos.PointInRect( x, y ) )
        {
            // Click on popup image - activate (play video / cheat)
            OnAccept( pWin );
        }
        else
        {
            // Click outside popup - close it
            g_AudioMgr.Play("Backup");
            InitIconScaling( TRUE );
        }
        return;
    }
    ui_dialog::OnPointerDown( pWin, x, y );
}

//=========================================================================

void dlg_secrets_menu::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    s32 highLight = -1;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the controls
            m_pSecretsMain      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pSecretsDetails   ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pSecretsSelect    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pSecretsButton[0] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pSecretsButton[1] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pSecretsButton[2] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pSecretsButton[3] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pSecretsButton[4] ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            GotoControl( (ui_control*)m_pSecretsSelect );
            irect Pos = m_pSecretsSelect->GetPosition();
            Pos.Translate( 0, -8 );
            g_UiMgr->SetScreenHighlight( Pos );

            // activate text
            m_pSecretsLine1    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pSecretsLine2    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pSecretsLine3    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
        }
    }
    else
    {
        // update any icon scaling
        UpdateIconScaling( DeltaTime );
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    // update everything else
    ui_dialog::OnUpdate( pWin, DeltaTime );

    if( !s_Scaled && (m_State == DIALOG_STATE_ACTIVE) )
    {
        // update highlight
        if( m_pSecretsSelect->IsFocused() )
        {
            highLight = 0;
            irect Pos = m_pSecretsSelect->GetPosition();
            Pos.Translate( 0, -8 );
            g_UiMgr->SetScreenHighlight( Pos );
        }
        else if( m_pSecretsButton[0]->IsFocused() )
        {
            highLight = 1;
            g_UiMgr->SetScreenHighlight( m_pSecretsMain->GetPosition() );
            m_pSelectedIcon = m_pSecretsButton[0];
        }
        else if( m_pSecretsButton[1]->IsFocused() )
        {
            highLight = 2;
            g_UiMgr->SetScreenHighlight( m_pSecretsMain->GetPosition() );
            m_pSelectedIcon = m_pSecretsButton[1];
        }
        else if( m_pSecretsButton[2]->IsFocused() )
        {
            highLight = 3;
            g_UiMgr->SetScreenHighlight( m_pSecretsMain->GetPosition() );
            m_pSelectedIcon = m_pSecretsButton[2];
        }
        else if( m_pSecretsButton[3]->IsFocused() )
        {
            highLight = 4;
            g_UiMgr->SetScreenHighlight( m_pSecretsMain->GetPosition() );
            m_pSelectedIcon = m_pSecretsButton[3];
        }
        else if( m_pSecretsButton[4]->IsFocused() )
        {
            highLight = 5;
            g_UiMgr->SetScreenHighlight( m_pSecretsMain->GetPosition() );
            m_pSelectedIcon = m_pSecretsButton[4];
        }
    }

    if( highLight != -1 )
    {
        if( highLight != m_CurrHL )
        {
            if( highLight != -1 )
                g_AudioMgr.Play("Cusor_Norm");

            m_CurrHL = highLight;

            if( highLight > 0 )
            {
                PopulateSecretsDetails( FALSE );
            }
            else
            {
                PopulateSecretsDetails( TRUE );
            }
        }
    }
}

//=========================================================================

void dlg_secrets_menu::InitIconScaling ( xbool ScaleDown )
{
    // scaling up or down
    m_bScaleDown  = ScaleDown;

    // store requested frame size
    if( m_bScaleDown )
    {
        m_FadeLevel = 205;
        m_RequestedPos.Set( 0, 0, 64, 64 );
        m_pSelectedIcon->LocalToScreen( m_RequestedPos );

        m_StartPos = m_DrawPos;
        m_DrawPos = m_StartPos;
        m_TimeOut = 0.0f;

        // restart background movie
        Movie.Close();
        g_StateMgr.EnableBackgroundMovie();

        // turn off textbox (if any)
        m_pTextBox->SetFlag( ui_win::WF_VISIBLE, FALSE );
        m_pTextBox->SetFlag( ui_win::WF_DISABLED, TRUE );
        m_pTextBox->SetActive( FALSE );
        
        g_UiMgr->EnableScreenHighlight();
        // turn on secrets details
        m_pSecretsDetails ->SetFlag(ui_win::WF_VISIBLE, TRUE);
        m_pSecretsLine1   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
        m_pSecretsLine2   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
        m_pSecretsLine3   ->SetFlag(ui_win::WF_VISIBLE, TRUE);

        xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
        navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
        SetNavText( navText );

        // goto previous control
        GotoControl( (ui_control*)m_pSelectedIcon );
        s_Scaled = FALSE;
        
        for( s32 i = 0; i < 5; i++ )
        {
            m_pSecretsButton[i]->SetFlag( ui_win::WF_DISABLED, FALSE );
        }        
    }
    else
    {
        s_Scaled = TRUE;

        m_FadeLevel = 0;

        // TODO: GS: Do it better :L
        const f32 virtHW = 122.5f;
        const f32 virtHH = 107.0f;
        const f32 virtYO = 72.5f;

        s32 const cx = 256;
        s32 const cy = 224;
        m_RequestedPos = irect( cx - (s32)virtHW,
                                cy - (s32)(virtHH + virtYO),
                                cx + (s32)virtHW,
                                cy + (s32)(virtHH - virtYO) );

        m_StartPos.Set( 0, 0, 64, 64 );
        m_pSelectedIcon->LocalToScreen( m_StartPos );
        m_DrawPos = m_StartPos;
        m_pSelectedIcon->SetFlag( ui_win::WF_VISIBLE, FALSE );

        for( s32 i = 0; i < 5; i++ )
        {
            m_pSecretsButton[i]->SetFlag( ui_win::WF_DISABLED, TRUE );
        }

        // disable vault selector while file is expanded
        m_pSecretsSelect->SetFlag( ui_win::WF_DISABLED, TRUE );

        // disable the highlight
        g_UiMgr->DisableScreenHighlight();

        // set filename for still
        const secret_entry* PreEntry = g_SecretList.GetByIndex( m_pSelectedIcon->GetData() );
        if( PreEntry )
        {
            g_UiMgr->UnloadBitmap( "Still" );
            m_NumItems = 1;
            m_CurrItem = 0;
            x_strcpy( m_FileName, PreEntry->FileName );
            m_StillBitmapID = g_UiMgr->LoadBitmap( "Still", xfs( "LORE/SECRETS/%s.xbmp", m_FileName ) );
        }
    }

    
    // set up scaling
    m_scaleCount = 0.3f; // time to scale in seconds
    m_scaleAngle = 180.0f / m_scaleCount;

    m_DiffPos.t = (s32)((m_RequestedPos.t - m_DrawPos.t) / 2.0f);
    m_DiffPos.l = (s32)((m_RequestedPos.l - m_DrawPos.l) / 2.0f);
    m_DiffPos.b = (s32)((m_RequestedPos.b - m_DrawPos.b) / 2.0f);
    m_DiffPos.r = (s32)((m_RequestedPos.r - m_DrawPos.r) / 2.0f);

    m_TotalMoved.Set( 0, 0, 0, 0 );
    
    // turn screen on
    m_bScreenIsOn = TRUE;

    // play scaling sound
    if( m_DiffPos.b > 0 )
    {
        g_AudioMgr.Play( "ResizeLarge" ); 
    }
    else
    {
        g_AudioMgr.Play( "ResizeSmall" );
    }
}

//=========================================================================

xbool dlg_secrets_menu::UpdateIconScaling( f32 DeltaTime )
{
    // scale window if necessary
    if (m_scaleCount)
    {
        // apply delta time
        m_scaleCount -= DeltaTime;

        if (m_scaleCount <= 0)
        {
            // last one - make sure window is correct size
            m_scaleCount = 0;
            m_DrawPos = m_RequestedPos;

            if( m_bScaleDown )
            {
                m_FadeLevel = 0;
                m_bScreenIsOn = FALSE;
                m_State = DIALOG_STATE_ACTIVE;
                m_pSelectedIcon->SetFlag( ui_win::WF_VISIBLE, TRUE );

                // re-enable vault selector
                m_pSecretsSelect->SetFlag( ui_win::WF_DISABLED, FALSE );
            }
            else
            {
                m_FadeLevel = 205;

                // get the secrets description
                const secret_entry* Entry = g_SecretList.GetByIndex( m_pSelectedIcon->GetData() );
                ASSERT( Entry );

                // what type of secrets item do we have
                if( Entry->SecretType == SECRET_TYPE_VIDEO )
                {
                    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_PLAY_MOVIE" ));
                    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
                    SetNavText( navText );
                }

                // turn off secrets details
                m_pSecretsDetails ->SetFlag(ui_win::WF_VISIBLE, FALSE);
                m_pSecretsLine1   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
                m_pSecretsLine2   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
                m_pSecretsLine3   ->SetFlag(ui_win::WF_VISIBLE, FALSE);

                // make text box visible and fill with text
                m_pTextBox->SetFlag( ui_win::WF_VISIBLE, TRUE );
                m_pTextBox->SetFlag( ui_win::WF_DISABLED, FALSE );
                m_pTextBox->SetActive( TRUE );
                x_strcpy( m_FullDesc, Entry->FullDesc );
                m_pTextBox->SetLabel( g_StringTableMgr( "lore", m_FullDesc ) );

                GotoControl( (ui_control*)m_pTextBox );
            }
        }
        else
        {
            f32 const ScaleCurve = x_cos( DEG_TO_RAD( m_scaleAngle * m_scaleCount ) );

            m_TotalMoved.t = m_DiffPos.t + (s32)( m_DiffPos.t * ScaleCurve );
            m_TotalMoved.l = m_DiffPos.l + (s32)( m_DiffPos.l * ScaleCurve );
            m_TotalMoved.r = m_DiffPos.r + (s32)( m_DiffPos.r * ScaleCurve );
            m_TotalMoved.b = m_DiffPos.b + (s32)( m_DiffPos.b * ScaleCurve );

            m_DrawPos.t = m_StartPos.t + m_TotalMoved.t;
            m_DrawPos.l = m_StartPos.l + m_TotalMoved.l;
            m_DrawPos.r = m_StartPos.r + m_TotalMoved.r;
            m_DrawPos.b = m_StartPos.b + m_TotalMoved.b;

            f32 const FadeProgress = 0.5f * (1.0f + ScaleCurve);
            if( m_bScaleDown)
            {
                m_FadeLevel = static_cast<u8>( 205.0f * (1.0f - FadeProgress) );
            }
            else
            {
                m_FadeLevel = static_cast<u8>( 205.0f * FadeProgress );
            }

            // still more to do!
            return TRUE;
        }
    }

    // we're done!
    return FALSE;
}
//=========================================================================

void dlg_secrets_menu::PopulateSecretsDetails( xbool bVaultDetails )
{
    if( bVaultDetails )
    {
        player_profile& Profile = g_StateMgr.GetActiveProfile( 0 );
        s32 VaultIdx = m_pSecretsSelect->GetSelectedItemData();

#if defined (mbillington) || (jhowa) || (ksaffel) || (sbroumley)
        s32 NumUnlocked = 20;
#else
        s32 NumUnlocked = Profile.GetNumSecretsUnlocked() - 1;
#endif
        s32 Min = VaultIdx * 5;
        s32 Max = Min + 5;
        s32 Count = 0;
        s32 j=0;
    
        // check for last vault
        if( VaultIdx == 4 )
        {
            // turn off outer buttons (only three items)
            m_pSecretsButton[0]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pSecretsButton[1]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pSecretsButton[3]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            m_pSecretsButton[4]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
            
            m_pSecretsButton[0]  ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pSecretsButton[1]  ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pSecretsButton[3]  ->SetFlag(ui_win::WF_DISABLED, TRUE);
            m_pSecretsButton[4]  ->SetFlag(ui_win::WF_DISABLED, TRUE);

            // limit selection
            j = 2;

            // we must make this bigger than Min or it won't go through the for loop
            // below which will cause page 5's button data to not be set properly.
            Max = Min + 1;  

            m_pSecretsButton[j]->SetBitmap( m_SecretsIconID[SECRET_TYPE_UNKNOWN] );
            m_pSecretsButton[j]->SetData( -1 );
        }
        else
        {
            // enable all five buttons
            if( g_UiMgr->IsScreenScaling() == FALSE )
            {
                m_pSecretsButton[0]  ->SetFlag(ui_win::WF_VISIBLE, TRUE);
                m_pSecretsButton[1]  ->SetFlag(ui_win::WF_VISIBLE, TRUE);
                m_pSecretsButton[3]  ->SetFlag(ui_win::WF_VISIBLE, TRUE);
                m_pSecretsButton[4]  ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            }
            m_pSecretsButton[0]  ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pSecretsButton[1]  ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pSecretsButton[3]  ->SetFlag(ui_win::WF_DISABLED, FALSE);
            m_pSecretsButton[4]  ->SetFlag(ui_win::WF_DISABLED, FALSE);
        }

        for( s32 i=Min; i<Max; i++, j++ )
        {
            if( i > NumUnlocked )
            {
                m_pSecretsButton[j]->SetBitmap( m_SecretsIconID[SECRET_TYPE_UNKNOWN] );
                m_pSecretsButton[j]->SetData( -1 );
            }
            else
            {
                // set the bitmap based on the secrets type      
                secret_entry *pSecret = g_SecretList.GetByIndex(i); 
                m_pSecretsButton[j]->SetBitmap( m_SecretsIconID[ pSecret->SecretType ] );
                m_pSecretsButton[j]->SetData( i );
                Count++;
            }
        }

        m_pSecretsLine1->SetLabel( m_pSecretsSelect->GetSelectedItemLabel() );

        // TODO: Align separators across locales.
        xwstring UnlockedCount( g_StringTableMgr( "ui", "IDS_UNLOCKED" ) );
        UnlockedCount += xwstring( " : " );
        UnlockedCount += xwstring( xfs( "%d/%d", Count, (VaultIdx == 4) ? 1 : 5 ) );
        m_pSecretsLine2->SetLabel( UnlockedCount );

        xwstring OverallCount( g_StringTableMgr( "ui", "IDS_OVERALL" ) );
        OverallCount += xwstring( "  : " );
        OverallCount += xwstring( xfs( "%d/21", Profile.GetNumSecretsUnlocked() ) );
        m_pSecretsLine3->SetLabel( OverallCount );

        xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_CYCLE_VAULT" ));
        navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
        SetNavText( navText );
    }
    else
    {
        // update details text based on selected icon
        const xwchar* pSecretsText;

        if( m_pSelectedIcon->GetBitmap() == m_SecretsIconID[SECRET_TYPE_UNKNOWN] )
        {
            pSecretsText = g_StringTableMgr( "lore", "IDS_SECRET_UNLOCK_INFO" );
        }
        else
        {
            // get the secrets info
            const secret_entry* pEntry = g_SecretList.GetByIndex( m_pSelectedIcon->GetData() );
            ASSERT( pEntry );
            pSecretsText = g_StringTableMgr( "lore", pEntry->ShortDesc );
        }

        ui_font* pFont      = g_UiMgr->GetFont( "small" );
        xwstring Wrapped;
        pFont->TextWrap( pSecretsText, m_pSecretsLine1->GetPosition(), Wrapped );
        m_pSecretsLine1->SetLabel( Wrapped );
        m_pSecretsLine2->SetLabel( g_StringTableMgr( "ui", "IDS_NULL" ) );
        m_pSecretsLine3->SetLabel( g_StringTableMgr( "ui", "IDS_NULL" ) );

        xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
        navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
        SetNavText( navText );
    }
}

//=========================================================================
