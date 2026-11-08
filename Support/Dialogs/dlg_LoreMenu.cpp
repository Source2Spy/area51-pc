//=========================================================================
//
//  dlg_lore_menu.cpp
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

#include "dlg_LoreMenu.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "StateMgr/MapList.hpp"
#include "StateMgr/LoreList.hpp"
#include "MoviePlayer/MoviePlayer.hpp"

//=========================================================================
//  Main Menu Dialog
//=========================================================================

enum controls
{   
    IDC_LORE_MAIN,
    IDC_LORE_DETAILS,
    IDC_LORE_SELECT,
    IDC_LORE_BUTTON_1,
    IDC_LORE_BUTTON_2,
    IDC_LORE_BUTTON_3,
    IDC_LORE_BUTTON_4,
    IDC_LORE_BUTTON_5,
    IDC_LORE_TEXT_1,
    IDC_LORE_TEXT_2,
    IDC_LORE_TEXT_3,
    IDC_LORE_TEXTBOX,
};

//-------------------------------------------------------------------------

ui_manager::control_tem LoreMenuControls[] = 
{
    // Frames.

    { IDC_LORE_SELECT,      "IDS_NULL",    "combo",      108,  40, 280,  40, 0, 0, 5, 1, ui_win::WF_VISIBLE },

    { IDC_LORE_MAIN,        "IDS_NULL",    "blankbox",    40,  80, 416, 144, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_LORE_BUTTON_1,    "IDS_NULL",    "button",      56, 130,  64,  64, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_LORE_BUTTON_2,    "IDS_NULL",    "button",     136, 130,  64,  64, 1, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_LORE_BUTTON_3,    "IDS_NULL",    "button",     216, 130,  64,  64, 2, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_LORE_BUTTON_4,    "IDS_NULL",    "button",     296, 130,  64,  64, 3, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_LORE_BUTTON_5,    "IDS_NULL",    "button",     376, 130,  64,  64, 4, 1, 1, 1, ui_win::WF_VISIBLE },

    { IDC_LORE_DETAILS,     "IDS_NULL",    "blankbox",    40, 240, 416,  94, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_LORE_TEXT_1,      "IDS_NULL",    "text",        48, 262, 400,  94, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_LORE_TEXT_2,      "IDS_NULL",    "text",        48, 278,  90,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_LORE_TEXT_3,      "IDS_NULL",    "text",        48, 294,  90,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },

    { IDC_LORE_TEXTBOX,     "IDS_NULL",    "textbox",     60, 240, 376,  93, 0, 2, 5, 1, ui_win::WF_VISIBLE },

};

//-------------------------------------------------------------------------

ui_manager::dialog_tem LoreMenuDialog =
{
    "IDS_LORE_MENU_TITLE",
    5, 9,
    sizeof(LoreMenuControls)/sizeof(ui_manager::control_tem),
    &LoreMenuControls[0],
    0
};

//=========================================================================
//  Data
//=========================================================================

static xbool s_Scaled = FALSE;

//=========================================================================
//  Registration function
//=========================================================================

void dlg_lore_menu_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "lore main", &LoreMenuDialog, &dlg_lore_menu_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_lore_menu_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_lore_menu* pDialog = new dlg_lore_menu;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_lore_menu
//=========================================================================

dlg_lore_menu::dlg_lore_menu( void )
{
}

//=========================================================================

dlg_lore_menu::~dlg_lore_menu( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_lore_menu::Create( s32                        UserID,
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
    m_LoreIconID[LORE_TYPE_VIDEO]   = g_UiMgr->LoadBitmap( "LoreVideo",  "LORE/INTERFACE/UI_LOREVIDEO.XBMP"   );
    m_LoreIconID[LORE_TYPE_AUDIO]   = g_UiMgr->LoadBitmap( "LoreAudio",  "VARIANTS/COMMON/LORE/INTERFACE/UI_LOREAUDIO.XBMP"   );
    m_LoreIconID[LORE_TYPE_STILL]   = g_UiMgr->LoadBitmap( "LoreStill",  "LORE/INTERFACE/UI_LORESTILL.XBMP"   );
    m_LoreIconID[LORE_TYPE_TEXT]    = g_UiMgr->LoadBitmap( "LoreText",   "LORE/INTERFACE/UI_LORETEXT.XBMP"    );
    m_LoreIconID[LORE_TYPE_UNKNOWN] = g_UiMgr->LoadBitmap( "LoreNull",   "VARIANTS/COMMON/LORE/INTERFACE/UI_LOREUNKNOWN.XBMP" );

    // set up nav text 
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_BACK" ));
    //navText += g_StringTableMgr( "ui", "IDS_NAV_CYCLE_VAULT" );
    SetNavText( navText );
    // setup lore main box
    m_pLoreMain = (ui_blankbox*)FindChildByID( IDC_LORE_MAIN );
    m_pLoreMain->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreMain->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pLoreMain->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pLoreMain->SetHasTitleBar( TRUE );
    m_pLoreMain->SetLabel( g_StringTableMgr( "ui", "IDS_LORE_VAULT" ) );
    m_pLoreMain->SetLabelColor( xcolor(255,252,204,255) );
    m_pLoreMain->SetTitleBarColor( xcolor(19,59,14,196) );

    // setup lore details box
    m_pLoreDetails = (ui_blankbox*)FindChildByID( IDC_LORE_DETAILS );
    m_pLoreDetails->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreDetails->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pLoreDetails->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pLoreDetails->SetHasTitleBar( TRUE );
    m_pLoreDetails->SetLabel( g_StringTableMgr( "ui", "IDS_LORE_DETAILS" ) );
    m_pLoreDetails->SetLabelColor( xcolor(255,252,204,255) );
    m_pLoreDetails->SetTitleBarColor( xcolor(19,59,14,196) );

    // set up textbox
    m_pTextBox = (ui_textbox*)FindChildByID( IDC_LORE_TEXTBOX );
    
    m_pTextBox->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_pTextBox->SetFlag( ui_win::WF_DISABLED, TRUE );
    m_pTextBox->SetExitOnBack( TRUE );
    m_pTextBox->SetExitOnSelect( TRUE );
    m_pTextBox->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pTextBox->DisableFrame();
    m_pTextBox->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    
    // set up lore combo
    m_pLoreSelect   = (ui_combo*)FindChildByID( IDC_LORE_SELECT );
    m_pLoreSelect   ->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV | ui_combo::CB_CHANGE_ON_SELECT | ui_combo::CB_NOTIFY_PARENT );
    m_pLoreSelect   ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    // get the active player profile
    player_profile& Profile = g_StateMgr.GetActiveProfile( 0 );

    s32 Count = 0;
    for( s32 i=0; i<g_MapList.GetCount(); i++ )
    {
        const map_entry& Entry = *g_MapList.GetByIndex( i );
        s32 MapID = Entry.GetMapID();

        // check if of the correct game type
        if( ( Entry.GetGameType() == GAME_CAMPAIGN ) && ( MapID < 2000 ) )
        {
            if( Count < NUM_VAULTS )
            {
                // look up the vault by the mapID
                s32 VaultIndex;
                g_LoreList.GetVaultByMapID( MapID, VaultIndex );

#if 0 //defined (mbillington) || (jhowa)
                // unlock it all!
                if( 1 )
#else
                // see if we unlocked ANYTHING in this vault
                if( Profile.GetLoreAcquired( VaultIndex, -1 ) )
#endif
                {
                    // add an entry to the list
                    xwstring DisplayName;
                    if( !DecodeFontText( Entry.GetDisplayName(), DisplayName ) )
                    {
                        x_DebugMsg( "Lore menu: skipping map ID %d with invalid UTF-8 name.\n", Entry.GetMapID() );
                        continue;
                    }
                    m_pLoreSelect->AddItem( DisplayName, (uaddr)&Entry );
                    // increment count
                    Count++;
                }
            }
        }
    }

    ASSERT( Count ); // Should have at least 1!

    // clear new lore flag
    Profile.ClearNewLoreUnlocked();
    // checksum profile to prevent unwanted "changed" messages appearing
    Profile.Checksum();

    // set up buttons
    m_pLoreButton[0]  = (ui_button*) FindChildByID( IDC_LORE_BUTTON_1 );
    m_pLoreButton[1]  = (ui_button*) FindChildByID( IDC_LORE_BUTTON_2 );
    m_pLoreButton[2]  = (ui_button*) FindChildByID( IDC_LORE_BUTTON_3 );
    m_pLoreButton[3]  = (ui_button*) FindChildByID( IDC_LORE_BUTTON_4 );
    m_pLoreButton[4]  = (ui_button*) FindChildByID( IDC_LORE_BUTTON_5 );

    m_pLoreButton[0]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreButton[1]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreButton[2]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreButton[3]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreButton[4]  ->SetFlag(ui_win::WF_VISIBLE, FALSE);


    // set up server info text
    m_pLoreLine1    = (ui_text*)FindChildByID( IDC_LORE_TEXT_1 );
    m_pLoreLine2    = (ui_text*)FindChildByID( IDC_LORE_TEXT_2 );
    m_pLoreLine3    = (ui_text*)FindChildByID( IDC_LORE_TEXT_3 );
    
    m_pLoreLine1    ->UseSmallText( TRUE );
    m_pLoreLine2    ->UseSmallText( TRUE );
    m_pLoreLine3    ->UseSmallText( TRUE );

    m_pLoreLine1    ->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    m_pLoreLine2    ->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    m_pLoreLine3    ->SetLabelFlags( ui_font::h_left|ui_font::v_top );

    m_pLoreLine1    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreLine2    ->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pLoreLine3    ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    m_pLoreLine1    ->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pLoreLine2    ->SetFlag(ui_win::WF_STATIC, TRUE);
    m_pLoreLine3    ->SetFlag(ui_win::WF_STATIC, TRUE);

    m_pLoreLine1    ->SetLabelColor( xcolor(255,252,204,255) );
    m_pLoreLine2    ->SetLabelColor( xcolor(255,252,204,255) );
    m_pLoreLine3    ->SetLabelColor( xcolor(255,252,204,255) );


    m_pLoreSelect->SetSelection( 0 );
    PopulateLoreDetails( TRUE );

    // set focus
    GotoControl( (ui_control*)m_pLoreSelect );

    // Initialize popup
    m_PopUp = NULL;

    // Initialize icon scaling
    m_scaleCount        = 0.0f;
    m_bScreenIsOn       = FALSE;
    m_bScaleDown        = FALSE;
    m_TimeOut           = 0.0f;
    m_bCycleBitmap      = FALSE;
    m_bFullScreenMode   = FALSE;
    m_CurrentType       = LORE_TYPE_UNKNOWN;
    
    // initialize screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_lore_menu::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();

    // unload lore bitmaps
    g_UiMgr->UnloadBitmap( "LoreVideo" );
    g_UiMgr->UnloadBitmap( "LoreAudio" );
    g_UiMgr->UnloadBitmap( "LoreStill" );
    g_UiMgr->UnloadBitmap( "LoreText" );
    g_UiMgr->UnloadBitmap( "LoreNull" );
    g_UiMgr->UnloadBitmap( "Still" );
}

//=========================================================================

void dlg_lore_menu::Render( s32 ox, s32 oy )
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

    // render full screen?
    if( m_bFullScreenMode )
    {
        irect r = g_UiMgr->GetUserBounds( g_UiUserID );
        m_pManager->RenderBitmap( m_StillBitmapID, r, XCOLOR_WHITE );
        return;
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
            switch( m_CurrentType )
            {
                case LORE_TYPE_VIDEO:
                case LORE_TYPE_AUDIO:
                    if( Movie.IsPlaying() )
                    {               
                        Movie.Render( TRUE );
                    }
                    break;

                default:
                    irect r = m_DrawPos;
                    r.t += 2;
                    r.l += 2;
                    r.b -= 2;
                    r.r -= 2;

                    // change the bitmap
                    if( m_bCycleBitmap )
                    {
                        m_bCycleBitmap = FALSE;
                        g_UiMgr->UnloadBitmap( "Still" );
                        if( m_CurrentType == LORE_TYPE_TEXT )
                        {
                            m_StillBitmapID = g_UiMgr->LoadBitmap( "Still", xfs( "LORE/IMAGES/%s.xbmp", m_FileName ) );
                        }
                        else
                        {
                            m_StillBitmapID = g_UiMgr->LoadBitmap( "Still", xfs( "LORE/GALLERY/%s%d.xbmp", m_FileName, m_CurrItem+1 ) );
                        }
                    }
                    m_pManager->RenderBitmap( m_StillBitmapID, r, XCOLOR_WHITE );
            }
        }

        if( IsTextBoxVisible )
        {
            m_pTextBox->Render( m_Position.l + ox, m_Position.t + oy );
        }
    }
}


//=========================================================================

void dlg_lore_menu::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    // only allow navigation if active
    if( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pLoreSelect )
        {
            switch( Code )
            {
                case ui_navigation::Left:
                case ui_navigation::Right:
                    PopulateLoreDetails( TRUE );
                    return;
            }
        }
        ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
    }
    else if( m_State == DIALOG_STATE_ACTIVATE )
    {
        // check for cycling images
        if ( m_CurrentType == LORE_TYPE_STILL )
        {
            if( m_NumItems > 1 )
            {
                switch( Code )
                {
                    case ui_navigation::Left:
                        if( --m_CurrItem < 0 )
                            m_CurrItem = m_NumItems-1;
                        break;

                    case ui_navigation::Right:
                        if( ++m_CurrItem == m_NumItems )
                            m_CurrItem = 0;
                        break;
                }

                // change the bitmap
                m_bCycleBitmap = TRUE;

                // change the text
                m_pTextBox->SetLabel( g_StringTableMgr( "lore", xfs( "%s_%d", m_FullDesc, m_CurrItem+1 ) ) );
            }
        }
    }
}

//=========================================================================

void dlg_lore_menu::OnNotify( ui_notification const& Event )
{
    (void)Event.m_pText;

    if( Event.m_pSender == (ui_win*)m_pLoreSelect )
    {
        if( Event.m_Type == ui_notification_type::ComboSelectionChanged )
        {
            if( !s_Scaled && (m_State == DIALOG_STATE_ACTIVE) )
            {
                PopulateLoreDetails( TRUE );
            }
        }
    }
}

//=========================================================================

void dlg_lore_menu::OnAccept( ui_win* pWin )
{
    if ( m_State == DIALOG_STATE_ACTIVATE )
    {
        if( (!m_bFullScreenMode) && (m_scaleCount == 0) )
        {
            // go full screen
            m_bFullScreenMode = TRUE;
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_BACK" ));
            SetNavText( navText );
            g_AudioMgr.Play("Select_Norm");
            return;
        }
    }

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if( pWin == (ui_win*)m_pLoreSelect )
        {
            // change selected vault
            PopulateLoreDetails( TRUE );
            return;
        }

        // handle lore item selection here!
        if( pWin == (ui_win*)m_pLoreButton[0] )
        {
            if( m_pLoreButton[0]->GetBitmap() == m_LoreIconID[LORE_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pLoreButton[0];
            m_SelectedIndex = 0;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pLoreButton[1] )
        {
            if( m_pLoreButton[1]->GetBitmap() == m_LoreIconID[LORE_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pLoreButton[1];
            m_SelectedIndex = 1;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pLoreButton[2] )
        {
            if( m_pLoreButton[2]->GetBitmap() == m_LoreIconID[LORE_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pLoreButton[2];
            m_SelectedIndex = 2;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pLoreButton[3] )
        {
            if( m_pLoreButton[3]->GetBitmap() == m_LoreIconID[LORE_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pLoreButton[3];
            m_SelectedIndex = 3;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
        else if( pWin == (ui_win*)m_pLoreButton[4] )
        {
            if( m_pLoreButton[4]->GetBitmap() == m_LoreIconID[LORE_TYPE_UNKNOWN] )
            {
                g_AudioMgr.Play( "InvalidEntry" );
                return;
            }

            g_AudioMgr.Play("Select_Norm");
            m_pSelectedIcon = m_pLoreButton[4];
            m_SelectedIndex = 4;
            InitIconScaling( FALSE );
            m_State = DIALOG_STATE_ACTIVATE;
        }
    }
}

//=========================================================================

void dlg_lore_menu::OnCancel( ui_win* pWin )
{
    (void)pWin;

    // check if we're scaling an icon
    if( m_scaleCount )
    {
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
            if( m_bFullScreenMode )
            {
                // exit full screen mode
                m_bFullScreenMode = FALSE;
                xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
                navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
                SetNavText( navText );
            }
            else
            {
                // exit sub menu / movie player
                InitIconScaling( TRUE );            
            }
            g_AudioMgr.Play("Backup");
        }
        break;
    }
    
}

//=========================================================================

void dlg_lore_menu::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    if( m_bFullScreenMode )
    {
        // exit full screen mode
        m_bFullScreenMode = FALSE;
        xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
        navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
        SetNavText( navText );
        g_AudioMgr.Play("Backup");
        return;
        
    }

    if( m_bScreenIsOn && !m_bFullScreenMode && (m_scaleCount == 0) )
    {
        if( m_DrawPos.PointInRect( x, y ) )
        {
            // Click on popup image - go full screen
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

void dlg_lore_menu::OnUpdate ( ui_win* pWin, f32 DeltaTime )
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
            m_pLoreMain      ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pLoreDetails   ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pLoreSelect    ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pLoreButton[0] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pLoreButton[1] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pLoreButton[2] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pLoreButton[3] ->SetFlag( ui_win::WF_VISIBLE, TRUE );
            m_pLoreButton[4] ->SetFlag( ui_win::WF_VISIBLE, TRUE );

            GotoControl( (ui_control*)m_pLoreSelect );
            irect Pos = m_pLoreSelect->GetPosition();
            Pos.Translate( 0, -8 );
            g_UiMgr->SetScreenHighlight( Pos );
            //m_CurrHL = 0;

            // activate text
            m_pLoreLine1    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pLoreLine2    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            m_pLoreLine3    ->SetFlag(ui_win::WF_VISIBLE, TRUE);
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
        if( m_pLoreSelect->IsFocused() )
        {
            highLight = 0;
            irect Pos = m_pLoreSelect->GetPosition();
            Pos.Translate( 0, -8 );
            g_UiMgr->SetScreenHighlight( Pos );
        }
        else if( m_pLoreButton[0]->IsFocused() )
        {
            highLight = 1;
            g_UiMgr->SetScreenHighlight( m_pLoreMain->GetPosition() );
            m_pSelectedIcon = m_pLoreButton[0];
        }
        else if( m_pLoreButton[1]->IsFocused() )
        {
            highLight = 2;
            g_UiMgr->SetScreenHighlight( m_pLoreMain->GetPosition() );
            m_pSelectedIcon = m_pLoreButton[1];
        }
        else if( m_pLoreButton[2]->IsFocused() )
        {
            highLight = 3;
            g_UiMgr->SetScreenHighlight( m_pLoreMain->GetPosition() );
            m_pSelectedIcon = m_pLoreButton[2];
        }
        else if( m_pLoreButton[3]->IsFocused() )
        {
            highLight = 4;
            g_UiMgr->SetScreenHighlight( m_pLoreMain->GetPosition() );
            m_pSelectedIcon = m_pLoreButton[3];
        }
        else if( m_pLoreButton[4]->IsFocused() )
        {
            highLight = 5;
            g_UiMgr->SetScreenHighlight( m_pLoreMain->GetPosition() );
            m_pSelectedIcon = m_pLoreButton[4];
        }
    }

    if( highLight != m_CurrHL )
    {
        if( highLight != -1 )
            g_AudioMgr.Play("Cusor_Norm");

        m_CurrHL = highLight;

        if( highLight > 0 )
        {
            PopulateLoreDetails( FALSE );
        }
        else
        {
            PopulateLoreDetails( TRUE );
        }
    }
}

//=========================================================================

void dlg_lore_menu::InitIconScaling ( xbool ScaleDown )
{
    // scaling up or down
    m_bScaleDown  = ScaleDown;

    const lore_entry* pSelectedEntry = NULL;
    if( !m_bScaleDown )
    {
        pSelectedEntry = g_LoreList.Find( m_pSelectedIcon->GetData() );
        ASSERT( pSelectedEntry );
        if( !pSelectedEntry )
        {
            return;
        }

        // the opened item owns the playback state. Do not depend on a prior
        // focus or hover update to establish its type
        m_CurrentType = pSelectedEntry->LoreType;
    }

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

        // turn on lore details
        m_pLoreDetails ->SetFlag(ui_win::WF_VISIBLE, TRUE);
        m_pLoreLine1   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
        m_pLoreLine2   ->SetFlag(ui_win::WF_VISIBLE, TRUE);
        m_pLoreLine3   ->SetFlag(ui_win::WF_VISIBLE, TRUE);

        // goto previous control
        GotoControl( (ui_control*)m_pSelectedIcon );
        s_Scaled = FALSE;

        for( s32 i = 0; i < 5; i++ )
        {
            m_pLoreButton[i]->SetFlag( ui_win::WF_DISABLED, FALSE );
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
            m_pLoreButton[i]->SetFlag( ui_win::WF_DISABLED, TRUE );
        }

        // disable vault selector while file is expanded
        m_pLoreSelect->SetFlag( ui_win::WF_DISABLED, TRUE );

        // disable the highlight
        g_UiMgr->DisableScreenHighlight();

        // set filename for still 
        if( (m_CurrentType == LORE_TYPE_STILL) || (m_CurrentType == LORE_TYPE_TEXT) )
        {
            g_UiMgr->UnloadBitmap( "Still" );
            m_NumItems = pSelectedEntry->NumItems;
            m_CurrItem = 0;
            x_strcpy( m_FileName, pSelectedEntry->FileName );
            if( m_NumItems > 1 )
                m_StillBitmapID = g_UiMgr->LoadBitmap( "Still", xfs( "LORE/GALLERY/%s%d.xbmp", m_FileName, m_CurrItem+1 ) );
            else
                m_StillBitmapID = g_UiMgr->LoadBitmap( "Still", xfs( "LORE/IMAGES/%s.xbmp", m_FileName ) );
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

xbool dlg_lore_menu::UpdateIconScaling( f32 DeltaTime )
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
                m_pLoreSelect->SetFlag( ui_win::WF_DISABLED, FALSE );
            }
            else
            {
                m_FadeLevel = 205;

                // get the lore description
                const lore_entry* Entry = g_LoreList.Find( m_pSelectedIcon->GetData() );
                ASSERT( Entry );

                // what type of lore item do we have
                switch( Entry->LoreType )
                {
                    case LORE_TYPE_VIDEO:
                    case LORE_TYPE_AUDIO:
                        // set filename
                        x_strcpy( m_FileName, Entry->FileName );
                        // set timeout
                        m_TimeOut = 0.5f;
                        break;
                    case LORE_TYPE_STILL:
                    case LORE_TYPE_TEXT:

                        // turn off lore details
                        m_pLoreDetails ->SetFlag(ui_win::WF_VISIBLE, FALSE);
                        m_pLoreLine1   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
                        m_pLoreLine2   ->SetFlag(ui_win::WF_VISIBLE, FALSE);
                        m_pLoreLine3   ->SetFlag(ui_win::WF_VISIBLE, FALSE);

                        // make text box visible and fill with text
                        m_pTextBox->SetFlag( ui_win::WF_VISIBLE, TRUE );
                        m_pTextBox->SetFlag( ui_win::WF_DISABLED, FALSE );
                        m_pTextBox->SetActive( TRUE );
                        x_strcpy( m_FullDesc, Entry->FullDesc );
                        if( Entry->NumItems > 1 )
                        {
                            m_pTextBox->SetLabel( g_StringTableMgr( "lore", xfs( "%s_%d", m_FullDesc, m_CurrItem+1 ) ) );
                        }
                        else
                        {
                            m_pTextBox->SetLabel( g_StringTableMgr( "lore", m_FullDesc ) );
                        }
                        GotoControl( (ui_control*)m_pTextBox );
                        break;
                }
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
    else
    {
        // update movie timeout
        if( m_TimeOut )
        {
            m_TimeOut -= DeltaTime;

            if (m_TimeOut <= 0)
            {
                // shut down background movie
                g_StateMgr.DisableBackgoundMovie();
                // open lore movie (or sound file, or text)
                Movie.Open(m_FileName, FALSE, TRUE);
                // reset timeout
                m_TimeOut = 0;
            }
        }
    }

    // we're done!
    return FALSE;
}
//=========================================================================

void dlg_lore_menu::PopulateLoreDetails( xbool bVaultDetails )
{
    if( bVaultDetails )
    {
        player_profile& Profile = g_StateMgr.GetActiveProfile( 0 );
        map_entry* pEntry = (map_entry*)m_pLoreSelect->GetSelectedItemData();

        lore_vault* m_pSelectedVault = g_LoreList.GetVaultByMapID( pEntry->GetMapID(), m_VaultIndex );

        u32 Count = 0;
        for( s32 i=0; i<NUM_PER_VAULT; i++ )
        {
#if defined (mbillington) || (jhowa) || (shird)
            if( 1 )
#else
            if( Profile.GetLoreAcquired( m_VaultIndex, i ) )
#endif
            {
                // set the bitmap based on the lore type            
                m_pLoreButton[i]->SetBitmap( m_LoreIconID[ g_LoreList.GetType( m_pSelectedVault->LoreID[i] ) ] );
                m_pLoreButton[i]->SetData( m_pSelectedVault->LoreID[i] );
                Count++;
            }
            else
            {
                m_pLoreButton[i]->SetBitmap( m_LoreIconID[LORE_TYPE_UNKNOWN] );
                m_pLoreButton[i]->SetData( m_pSelectedVault->LoreID[i] );
            }
        }

        xwstring DisplayName;
        if( DecodeFontText( pEntry->GetDisplayName(), DisplayName ) )
        {
            m_pLoreLine1->SetLabel( DisplayName );
        }
        else
        {
            x_DebugMsg( "Lore menu: invalid UTF-8 map name for ID %d.\n", pEntry->GetMapID() );
        }

        xwstring LevelCount( g_StringTableMgr( "ui", "IDS_LEVEL" ) );
        LevelCount += xwstring( "   : " );
        LevelCount += xwstring( xfs( "%d/%d", Count, NUM_PER_VAULT ) );
        m_pLoreLine2->SetLabel( LevelCount );

        xwstring OverallCount( g_StringTableMgr( "ui", "IDS_OVERALL" ) );
        OverallCount += xwstring( " : " );
        OverallCount += xwstring( xfs( "%d/90", Profile.GetTotalLoreAcquired() ) );
        m_pLoreLine3->SetLabel( OverallCount );

        //xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_CYCLE_VAULT" ));
        //navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
        xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
        navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
        SetNavText( navText );
    }
    else
    {
        // get the lore info
        const lore_entry* pLoreEntry = g_LoreList.Find( m_pSelectedIcon->GetData() );
        ASSERT( pLoreEntry );

        // update details text based on selected icon
        const xwchar* pLoreText;

#if defined (mbillington) || (jhowa) || (shird)
        if( 1 )
#else
        // get the active profile
        player_profile& Profile = g_StateMgr.GetActiveProfile( 0 );

        if( Profile.GetLoreAcquired( m_VaultIndex, m_CurrHL-1 ) )
#endif
        {
            pLoreText = g_StringTableMgr( "lore", pLoreEntry->ShortDesc );
        }
        else
        {
            pLoreText = g_StringTableMgr( "lore", pLoreEntry->Clue );
        }

        ui_font* pFont      = g_UiMgr->GetFont( "small" );
        xwstring Wrapped;
        pFont->TextWrap( pLoreText, m_pLoreLine1->GetPosition(), Wrapped );
        m_pLoreLine1->SetLabel( Wrapped );
        m_pLoreLine2->SetLabel( g_StringTableMgr( "ui", "IDS_NULL" ) );
        m_pLoreLine3->SetLabel( g_StringTableMgr( "ui", "IDS_NULL" ) );

        xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
        navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
        SetNavText( navText );
    }
}

//=========================================================================
