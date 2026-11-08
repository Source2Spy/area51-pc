//==============================================================================
//
//  dlg_BrowserOptions.cpp
//
//==============================================================================

#include "Entropy.hpp"
#include "dlg_BrowserOptions.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_check.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_edit.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_text.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"
#include "StateMgr/StateMgr.hpp"

//==============================================================================

enum browser_filter_controls
{
    IDC_FILTER_SOURCE_LABEL,
    IDC_FILTER_NAME_LABEL,
    IDC_FILTER_MAP_LABEL,
    IDC_FILTER_MODE_LABEL,
    IDC_FILTER_PING_LABEL,
    IDC_FILTER_SOURCE,
    IDC_FILTER_NAME,
    IDC_FILTER_MAP,
    IDC_FILTER_MODE,
    IDC_FILTER_PING,
    IDC_FILTER_NOT_FULL,
    IDC_FILTER_NOT_EMPTY,
    IDC_FILTER_NO_PASSWORD,
    IDC_FILTER_RESET,
    IDC_FILTER_CANCEL,
    IDC_FILTER_APPLY,
    IDC_FILTER_NOT_FULL_LABEL,
    IDC_FILTER_NOT_EMPTY_LABEL,
    IDC_FILTER_NO_PASSWORD_LABEL,
};

static ui_manager::control_tem BrowserFilterControls[] =
{
    { IDC_FILTER_SOURCE_LABEL     , "IDS_BROWSER_SOURCE"     , "text"  ,  24,  48, 116, 22, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FILTER_NAME_LABEL       , "IDS_HEADER_SERVER"      , "text"  ,  24,  78, 116, 22, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FILTER_MAP_LABEL        , "IDS_HEADER_MAPNAME"     , "text"  ,  24, 108, 116, 22, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FILTER_MODE_LABEL       , "IDS_BROWSER_MODE"       , "text"  ,  24, 138, 116, 22, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FILTER_PING_LABEL       , "IDS_BROWSER_PING"       , "text"  ,  24, 168, 116, 22, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FILTER_SOURCE           , "IDS_NULL"               , "combo" , 148,  48, 188, 22, 0, 0, 3, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_NAME             , "IDS_BROWSER_SEARCH"     , "edit"  , 158,  78, 168, 22, 0, 1, 3, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_MAP              , "IDS_BROWSER_MAP_SEARCH" , "edit"  , 158, 108, 168, 22, 0, 2, 3, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_MODE             , "IDS_NULL"               , "combo" , 148, 138, 188, 22, 0, 3, 3, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_PING             , "IDS_NULL"               , "combo" , 148, 168, 188, 22, 0, 4, 3, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_NOT_FULL         , "IDS_NULL"               , "check" ,  24, 204,  20, 24, 0, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_NOT_EMPTY        , "IDS_NULL"               , "check" , 118, 204,  20, 24, 1, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_NO_PASSWORD      , "IDS_NULL"               , "check" , 218, 204,  20, 24, 2, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_RESET            , "IDS_BROWSER_RESET"      , "button",  24, 244,  96, 24, 0, 6, 1, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_CANCEL           , "IDS_CANCEL"             , "button", 132, 244,  96, 24, 1, 6, 1, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_APPLY            , "IDS_BROWSER_APPLY"      , "button", 240, 244,  96, 24, 2, 6, 1, 1, ui_win::WF_VISIBLE },
    { IDC_FILTER_NOT_FULL_LABEL   , "IDS_BROWSER_NOT_FULL"   , "text"  ,  48, 204,  64, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FILTER_NOT_EMPTY_LABEL  , "IDS_BROWSER_NOT_EMPTY"  , "text"  , 142, 204,  70, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FILTER_NO_PASSWORD_LABEL, "IDS_BROWSER_NO_PASSWORD", "text"  , 242, 204,  94, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
};

static ui_manager::dialog_tem BrowserFilterDialog =
{
    "IDS_BROWSER_FILTERS", 3, 7,
    sizeof( BrowserFilterControls ) / sizeof( ui_manager::control_tem ), BrowserFilterControls, 5
};

enum browser_advanced_controls
{
    IDC_ADVANCED_DIRECTORY_LABEL,
    IDC_ADVANCED_DIRECTORY_HINT,
    IDC_ADVANCED_ADDRESS_LABEL,
    IDC_ADVANCED_DIRECTORY,
    IDC_ADVANCED_ADDRESS,
    IDC_ADVANCED_CONNECT,
    IDC_ADVANCED_STATUS,
    IDC_ADVANCED_RESET,
    IDC_ADVANCED_CANCEL,
    IDC_ADVANCED_APPLY,
};

static ui_manager::control_tem BrowserAdvancedControls[] =
{
    { IDC_ADVANCED_DIRECTORY_LABEL, "IDS_BROWSER_MASTER_ADDRESS", "text"  ,  24,  42, 312, 18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_DIRECTORY_HINT , "IDS_BROWSER_MASTER_HINT"   , "text"  ,  24,  90, 312, 28, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_ADDRESS_LABEL  , "IDS_DIRECT_ADDRESS"        , "text"  ,  24, 132, 312, 18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_DIRECTORY      , "IDS_BROWSER_MASTER_TITLE"  , "edit"  ,  24,  64, 312, 24, 0, 0, 3, 1, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_ADDRESS        , "IDS_DIRECT_ADDRESS"        , "edit"  ,  24, 154, 208, 24, 0, 1, 2, 1, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_CONNECT        , "IDS_BROWSER_CONNECT"       , "button", 240, 154,  96, 24, 2, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_STATUS         , "IDS_NULL"                  , "text"  ,  24, 190, 312, 32, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_RESET          , "IDS_BROWSER_RESET"         , "button",  24, 244,  96, 24, 0, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_CANCEL         , "IDS_CANCEL"                , "button", 132, 244,  96, 24, 1, 2, 1, 1, ui_win::WF_VISIBLE },
    { IDC_ADVANCED_APPLY          , "IDS_BROWSER_APPLY"         , "button", 240, 244,  96, 24, 2, 2, 1, 1, ui_win::WF_VISIBLE },
};

static ui_manager::dialog_tem BrowserAdvancedDialog =
{
    "IDS_BROWSER_ADVANCED", 3, 3,
    sizeof( BrowserAdvancedControls ) / sizeof( ui_manager::control_tem ), BrowserAdvancedControls, 3
};

//==============================================================================

static ui_win* dlg_browser_filters_factory( s32                       UserID,
                                            ui_manager*               pManager,
                                            ui_manager::dialog_tem*   pDialogTem,
                                            const irect&              Position,
                                            ui_win*                   pParent,
                                            s32                       Flags,
                                            void*                     pUserData )
{
    dlg_browser_filters* pDialog = new dlg_browser_filters;

    if( !pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
    {
        delete pDialog;
        return NULL;
    }

    return pDialog;
}

//==============================================================================

static ui_win* dlg_browser_advanced_factory( s32                       UserID,
                                             ui_manager*               pManager,
                                             ui_manager::dialog_tem*   pDialogTem,
                                             const irect&              Position,
                                             ui_win*                   pParent,
                                             s32                       Flags,
                                             void*                     pUserData )
{
    dlg_browser_advanced* pDialog = new dlg_browser_advanced;

    if( !pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
    {
        delete pDialog;
        return NULL;
    }

    return pDialog;
}

//==============================================================================

void dlg_browser_options_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "browser filters", &BrowserFilterDialog, dlg_browser_filters_factory );
    pManager->RegisterDialogClass( "browser advanced", &BrowserAdvancedDialog, dlg_browser_advanced_factory );
}

//==============================================================================

xbool dlg_browser_options::Create( s32                       UserID,
                                   ui_manager*               pManager,
                                   ui_manager::dialog_tem*   pDialogTem,
                                   const irect&              Position,
                                   ui_win*                   pParent,
                                   s32                       Flags,
                                   void*                     pUserData )
{
    if( !ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags & ~WF_BORDER, pUserData ) )
        return FALSE;

    for( s32 i = 0; i < m_Children.GetCount(); i++ )
    {
        const char* pClass = pDialogTem->pControls[i].pClass;
        if( x_strcmp( pClass, "button" ) == 0 )
        {
            ui_button* pButton = (ui_button*)m_Children[i];
            pButton->UseSmallText( TRUE );
            pButton->SetButtonStyle( ui_button::BUTTON_STYLE_FLAT );
        }
        else if( x_strcmp( pClass, "combo" ) == 0 )
        {
            ui_combo* pCombo = (ui_combo*)m_Children[i];
            pCombo->SetLabelWidth( 0 );
            pCombo->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV | ui_combo::CB_CHANGE_ON_SELECT );
        }
        else if( x_strcmp( pClass, "text" ) == 0 )
        {
            ui_text* pText = (ui_text*)m_Children[i];
            pText->UseSmallText( TRUE );
            pText->SetLabelFlags( ui_font::h_left | ui_font::v_center | ui_font::clip_ellipsis );
            pText->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        }
    }

    SetNavText( xwstring( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) ) + g_StringTableMgr( "ui", "IDS_NAV_BACK" ) );
    m_State = DIALOG_STATE_ACTIVE;

    return TRUE;
}

//==============================================================================

void dlg_browser_options::Render( s32 ox, s32 oy )
{
    if( !(m_Flags & WF_VISIBLE) )
        return;

    m_pManager->RenderRect( m_pManager->GetUserBounds( m_UserID ), xcolor( 0, 16, 0, 112 ), FALSE );

    irect r = m_Position;
    r.Translate( ox, oy );
    m_pManager->RenderRect( r, xcolor( 19, 59, 14, 248 ), FALSE );

    irect Title = r;
    Title.Deflate( 1, 1 );
    Title.SetHeight( 32 );
    m_pManager->RenderGouraudRect( Title, xcolor( 39, 117, 28, 255 ), xcolor( 39, 117, 28, 255 ),
                                 xcolor( 19, 59, 14, 255 ), xcolor( 19, 59, 14, 255 ), FALSE );

    Title.Deflate( 12, 2 );
    m_pManager->RenderText( m_pManager->FindFont( "large" ), Title,
                            ui_font::h_left | ui_font::v_center | ui_font::clip_ellipsis,
                            xcolor( 255, 252, 204, 255 ), m_Label );

    ui_dialog::Render( ox, oy );
    m_pManager->RenderRect( r, xcolor( 79, 214, 60, 224 ), TRUE );
}

//==============================================================================

void dlg_browser_options::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
        m_State = DIALOG_STATE_BACK;
}

//==============================================================================

xbool dlg_browser_filters::Create( s32                       UserID,
                                   ui_manager*               pManager,
                                   ui_manager::dialog_tem*   pDialogTem,
                                   const irect&              Position,
                                   ui_win*                   pParent,
                                   s32                       Flags,
                                   void*                     pUserData )
{
    if( !dlg_browser_options::Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
        return FALSE;

    m_pSource     = (ui_combo*)FindChildByID( IDC_FILTER_SOURCE );
    m_pMode       = (ui_combo*)FindChildByID( IDC_FILTER_MODE );
    m_pPing       = (ui_combo*)FindChildByID( IDC_FILTER_PING );
    m_pName       = (ui_edit*)FindChildByID( IDC_FILTER_NAME );
    m_pMap        = (ui_edit*)FindChildByID( IDC_FILTER_MAP );
    m_pNotFull    = (ui_check*)FindChildByID( IDC_FILTER_NOT_FULL );
    m_pNotEmpty   = (ui_check*)FindChildByID( IDC_FILTER_NOT_EMPTY );
    m_pNoPassword = (ui_check*)FindChildByID( IDC_FILTER_NO_PASSWORD );

    m_pSource->AddItem( g_StringTableMgr( "ui", "IDS_BROWSER_ALL" ), MATCH_SOURCE_ALL );
    if( g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY )
        m_pSource->AddItem( g_StringTableMgr( "ui", "IDS_BROWSER_INTERNET" ), MATCH_SOURCE_INTERNET );
    if( g_MatchMgr.GetCapabilities() & MATCH_CAP_LAN_DISCOVERY )
        m_pSource->AddItem( g_StringTableMgr( "ui", "IDS_BROWSER_LAN" ), MATCH_SOURCE_LAN );

    m_pMode->AddItem( g_StringTableMgr( "ui", "IDS_BROWSER_MODE_ANY" ), (uaddr)GAME_MP );
    const char* Modes[] = { "IDS_GAMETYPE_DM", "IDS_GAMETYPE_TDM", "IDS_GAMETYPE_CTF",
                           "IDS_GAMETYPE_TAG", "IDS_GAMETYPE_INF", "IDS_GAMETYPE_CNH" };
    for( s32 i = GAME_DM; i <= GAME_CNH; i++ )
        m_pMode->AddItem( g_StringTableMgr( "ui", Modes[i] ), i );

    m_pPing->AddItem( g_StringTableMgr( "ui", "IDS_BROWSER_PING_ANY" ), (uaddr)-1 );
    const s32 Pings[] = { 50, 100, 150, 250 };
    for( s32 i = 0; i < 4; i++ )
        m_pPing->AddItem( xwstring( xfs( "<= %d ms", Pings[i] ) ), Pings[i] );

    m_pName->Configure( FALSE );
    m_pName->SetBufferSize( 64 );
    m_pName->SetPlaceholder( g_StringTableMgr( "ui", "IDS_BROWSER_SEARCH" ) );

    m_pMap->Configure( FALSE );
    m_pMap->SetBufferSize( 32 );
    m_pMap->SetPlaceholder( g_StringTableMgr( "ui", "IDS_BROWSER_MAP_SEARCH" ) );

    Configure( match_browser_filter() );

    return TRUE;
}

//==============================================================================

void dlg_browser_filters::Configure( const match_browser_filter& Filter )
{
    m_Filter = Filter;

    s32 Source = m_pSource->FindItemByData( Filter.Source );
    m_pSource->SetSelection( Source >= 0 ? Source : 0 );
    m_pMode->SetSelection( m_pMode->FindItemByData( (uaddr)Filter.GameType ) );
    m_pPing->SetSelection( m_pPing->FindItemByData( (uaddr)Filter.MaxPing ) );

    m_pName->SetLabel( Filter.Name );
    m_pMap->SetLabel( Filter.Map );

    m_pNotFull->SetChecked( Filter.HideFull );
    m_pNotEmpty->SetChecked( Filter.HideEmpty );
    m_pNoPassword->SetChecked( Filter.HidePassword );
}

//==============================================================================

void dlg_browser_filters::OnAccept( ui_win* pWin )
{
    if( m_State != DIALOG_STATE_ACTIVE )
        return;

    switch( pWin->GetControlID() )
    {
    case IDC_FILTER_RESET:
        Configure( match_browser_filter() );
        break;

    case IDC_FILTER_CANCEL:
        OnCancel( pWin );
        break;

    case IDC_FILTER_APPLY:
        m_Filter.Source = (u32)m_pSource->GetSelectedItemData();
        m_Filter.GameType = (s32)m_pMode->GetSelectedItemData();
        m_Filter.MaxPing = (s32)m_pPing->GetSelectedItemData();
        m_Filter.Name = m_pName->GetLabel();
        m_Filter.Map = m_pMap->GetLabel();
        m_Filter.HideFull = m_pNotFull->IsChecked();
        m_Filter.HideEmpty = m_pNotEmpty->IsChecked();
        m_Filter.HidePassword = m_pNoPassword->IsChecked();
        m_State = DIALOG_STATE_SELECT;
        break;

    default:
        break;
    }
}

//==============================================================================

xbool dlg_browser_advanced::Create( s32                       UserID,
                                    ui_manager*               pManager,
                                    ui_manager::dialog_tem*   pDialogTem,
                                    const irect&              Position,
                                    ui_win*                   pParent,
                                    s32                       Flags,
                                    void*                     pUserData )
{
    if( !dlg_browser_options::Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
        return FALSE;

    m_pDirectory = (ui_edit*)FindChildByID( IDC_ADVANCED_DIRECTORY );
    m_pAddress   = (ui_edit*)FindChildByID( IDC_ADVANCED_ADDRESS );
    m_pStatus    = (ui_text*)FindChildByID( IDC_ADVANCED_STATUS );
    m_Connected  = FALSE;

    const multiplayer_preferences& Preferences = g_StateMgr.GetActiveProfile(
        g_StateMgr.GetProfileListIndex(0) ).GetMultiplayerPreferences();

    m_pDirectory->Configure( FALSE );
    m_pDirectory->SetBufferSize( MATCH_ENDPOINT_LENGTH );
    m_pDirectory->SetLabel( xwstring( Preferences.Directory ) );
    m_pDirectory->SetFlag( WF_DISABLED, !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY) );

    m_pAddress->Configure( FALSE );
    m_pAddress->SetBufferSize( MATCH_ENDPOINT_LENGTH );
    m_pAddress->SetLabel( xwstring( Preferences.Address ) );
    m_pAddress->SetFlag( WF_DISABLED, !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECT_CONNECT) );
    FindChildByID( IDC_ADVANCED_CONNECT )->SetFlag( WF_DISABLED,
        !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECT_CONNECT) );

    m_pStatus->SetLabelColor( xcolor( 255, 180, 96, 255 ) );

    return TRUE;
}

//==============================================================================

void dlg_browser_advanced::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || (m_Resolving && (pWin->GetControlID() != IDC_ADVANCED_CANCEL)) )
        return;

    switch( pWin->GetControlID() )
    {
    case IDC_ADVANCED_CANCEL:
        OnCancel( pWin );
        break;

    case IDC_ADVANCED_RESET:
        m_pDirectory->SetLabel( xwstring( "" ) );
        m_pAddress->SetLabel( xwstring( "" ) );
        m_pStatus->SetLabel( xwstring( "" ) );
        break;

    case IDC_ADVANCED_CONNECT:
    {
        match_endpoint Directory;

        if( (g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY) &&
            m_pDirectory->GetLabel().GetLength() &&
            !Directory.Parse( xstring( m_pDirectory->GetLabel() ), MATCH_DIRECTORY_PORT ) )
        {
            m_pStatus->SetLabel( g_StringTableMgr( "ui", "IDS_BROWSER_INVALID_ADDRESS" ) );
            break;
        }

        HandleConnectionResult( g_MatchMgr.PrepareDirectConnection( xstring( m_pAddress->GetLabel() ) ) );
        break;
    }

    case IDC_ADVANCED_APPLY:
        if( !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY) ||
            (xstring( m_pDirectory->GetLabel() ) == g_MatchMgr.GetDirectoryAddress()) ||
            (g_MatchMgr.ConfigureDirectory( xstring( m_pDirectory->GetLabel() ) ) == MATCH_RESULT_OK) )
        {
            SavePreferences();
            m_State = DIALOG_STATE_SELECT;
        }
        else
            m_pStatus->SetLabel( g_StringTableMgr( "ui", "IDS_BROWSER_INVALID_ADDRESS" ) );
        break;

    default:
        break;
    }
}

//==============================================================================

dlg_browser_advanced::~dlg_browser_advanced( void )
{
    if( m_Resolving )
        g_MatchMgr.CancelDirectConnection();
}

//==============================================================================

void dlg_browser_advanced::OnCancel( ui_win* pWin )
{
    if( m_Resolving )
    {
        g_MatchMgr.CancelDirectConnection();
        m_Resolving = FALSE;
    }
    dlg_browser_options::OnCancel( pWin );
}

//==============================================================================

void dlg_browser_advanced::SavePreferences( void )
{
    multiplayer_preferences& Preferences = g_StateMgr.GetActiveProfile(
        g_StateMgr.GetProfileListIndex(0) ).GetMultiplayerPreferences();
    multiplayer_preferences Updated = Preferences;

    x_strncpy( Updated.Directory, xstring( m_pDirectory->GetLabel() ), MATCH_ENDPOINT_LENGTH - 1 );
    Updated.Directory[MATCH_ENDPOINT_LENGTH - 1] = 0;
    x_strncpy( Updated.Address, xstring( m_pAddress->GetLabel() ), MATCH_ENDPOINT_LENGTH - 1 );
    Updated.Address[MATCH_ENDPOINT_LENGTH - 1] = 0;

    if( x_memcmp( &Preferences, &Updated, sizeof(Preferences) ) == 0 )
        return;

    Preferences = Updated;
    g_StateMgr.SaveMultiplayerPreferences();
}

//==============================================================================

void dlg_browser_advanced::HandleConnectionResult( match_result Result )
{
    m_Resolving = Result == MATCH_RESULT_PENDING;
    const s32 Controls[] = { IDC_ADVANCED_DIRECTORY, IDC_ADVANCED_ADDRESS, IDC_ADVANCED_CONNECT,
                             IDC_ADVANCED_APPLY, IDC_ADVANCED_RESET };
    for( s32 i = 0; i < 5; i++ )
        FindChildByID( Controls[i] )->SetFlag( WF_DISABLED, m_Resolving );
    m_pDirectory->SetFlag( WF_DISABLED, m_Resolving || !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY) );
    m_pAddress->SetFlag( WF_DISABLED, m_Resolving || !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECT_CONNECT) );
    FindChildByID( IDC_ADVANCED_CONNECT )->SetFlag( WF_DISABLED,
        m_Resolving || !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECT_CONNECT) );

    if( Result == MATCH_RESULT_OK )
    {
        if( (g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY) &&
            (xstring( m_pDirectory->GetLabel() ) != g_MatchMgr.GetDirectoryAddress()) &&
            (g_MatchMgr.ConfigureDirectory( xstring( m_pDirectory->GetLabel() ) ) != MATCH_RESULT_OK) )
        {
            m_pStatus->SetLabel( g_StringTableMgr( "ui", "IDS_BROWSER_INVALID_ADDRESS" ) );
            return;
        }

        SavePreferences();
        m_Connected = TRUE;
        m_State = DIALOG_STATE_SELECT;
        return;
    }

    const char* pMessage = "IDS_ONLINE_NETWORK_DOWN";
    switch( Result )
    {
    case MATCH_RESULT_PENDING:          pMessage = "IDS_DIRECT_RESOLVING";         break;
    case MATCH_RESULT_INVALID_ADDRESS:  pMessage = "IDS_DIRECT_INVALID_ADDRESS";    break;
    case MATCH_RESULT_NAME_NOT_FOUND:   pMessage = "IDS_DIRECT_RESOLVE_FAILED";     break;
    case MATCH_RESULT_TIMEOUT:          pMessage = "IDS_DIRECT_RESOLVE_TIMEOUT";    break;
    default:                                                                      break;
    }
    m_pStatus->SetLabel( g_StringTableMgr( "ui", pMessage ) );
}

//==============================================================================

void dlg_browser_advanced::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    if( m_Resolving )
    {
        match_result Result = g_MatchMgr.PollDirectConnection( DeltaTime );
        if( Result != MATCH_RESULT_PENDING )
            HandleConnectionResult( Result );
    }
    dlg_browser_options::OnUpdate( pWin, DeltaTime );
}
