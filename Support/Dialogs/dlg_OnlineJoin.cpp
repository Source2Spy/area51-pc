//==============================================================================
//
//  dlg_OnlineJoin.cpp
//
//==============================================================================

#include "Entropy.hpp"
#include "dlg_OnlineJoin.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_joinlist.hpp"
#include "UI/ui_text.hpp"
#include "dlg_BrowserOptions.hpp"
#include "StringMgr/StringMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "Apps/GameApp/Config.hpp"

#ifndef CONFIG_RETAIL
extern char g_CmdLineConnectAddress[22];
#endif

//==============================================================================

enum controls
{
    IDC_MATCHLIST,
    IDC_DETAILS,
    IDC_ENDPOINT,
    IDC_MAP,
    IDC_MODE,
    IDC_PLAYERS,
    IDC_PING,
    IDC_PASSWORD,
    IDC_COMPLETE,
    IDC_REFRESH,
    IDC_FILTERS,
    IDC_ADVANCED,
    IDC_JOIN,
};

static ui_manager::control_tem OnlineJoinControls[] =
{
    { IDC_MATCHLIST, "IDS_BROWSER_NO_SERVERS"   , "joinlist",  45,  38, 410, 188, 0, 0, 4, 1, ui_win::WF_VISIBLE },
    { IDC_DETAILS  , "IDS_BROWSER_SELECT_SERVER", "text"    ,  51, 232, 219,  20, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_ENDPOINT , "IDS_NULL"                 , "text"    , 274, 232, 175,  20, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_MAP      , "IDS_NULL"                 , "text"    ,  51, 256, 135,  18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_MODE     , "IDS_NULL"                 , "text"    ,  51, 276, 135,  18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PLAYERS  , "IDS_NULL"                 , "text"    , 194, 256, 112,  18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PASSWORD , "IDS_NULL"                 , "text"    , 194, 276, 112,  18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PING     , "IDS_NULL"                 , "text"    , 314, 256, 135,  18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_COMPLETE , "IDS_NULL"                 , "text"    , 314, 276, 135,  18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_ADVANCED , "IDS_BROWSER_ADVANCED"     , "button"  ,  45, 306,  96,  24, 0, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_FILTERS  , "IDS_BROWSER_FILTERS"      , "button"  , 149, 306,  97,  24, 1, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_REFRESH  , "IDS_REFRESH"              , "button"  , 254, 306,  97,  24, 2, 1, 1, 1, ui_win::WF_VISIBLE },
    { IDC_JOIN     , "IDS_BROWSER_CONNECT"      , "button"  , 359, 306,  96,  24, 3, 1, 1, 1, ui_win::WF_VISIBLE },
};

static ui_manager::dialog_tem OnlineJoinDialog =
{
    "IDS_ONLINE_JOIN_TITLE", 4, 2,
    sizeof( OnlineJoinControls ) / sizeof( ui_manager::control_tem ), OnlineJoinControls, 0
};

//==============================================================================

void dlg_online_join_register( ui_manager* pManager )
{
    dlg_browser_options_register( pManager );
    pManager->RegisterDialogClass( "online join", &OnlineJoinDialog, dlg_online_join_factory );
}

//==============================================================================

ui_win* dlg_online_join_factory( s32                       UserID,
                                 ui_manager*               pManager,
                                 ui_manager::dialog_tem*   pDialogTem,
                                 const irect&              Position,
                                 ui_win*                   pParent,
                                 s32                       Flags,
                                 void*                     pUserData )
{
    dlg_online_join* pDialog = new dlg_online_join;

    if( !pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
    {
        delete pDialog;
        return NULL;
    }

    return pDialog;
}

//==============================================================================

dlg_online_join::dlg_online_join( void )
{
    m_Created         = FALSE;
    m_pFiltersDialog  = NULL;
    m_pAdvancedDialog = NULL;
}

//==============================================================================

dlg_online_join::~dlg_online_join( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_online_join::Create( s32                       UserID,
                               ui_manager*               pManager,
                               ui_manager::dialog_tem*   pDialogTem,
                               const irect&              Position,
                               ui_win*                   pParent,
                               s32                       Flags,
                               void*                     pUserData )
{
    (void)pUserData;

    if( !ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags ) )
        return FALSE;

    g_UiMgr->DisableScreenHighlight();
    m_Created        = TRUE;
    const multiplayer_preferences& Preferences = g_StateMgr.GetActiveProfile(
        g_StateMgr.GetProfileListIndex(0) ).GetMultiplayerPreferences();

    m_Filter         = CONFIG_IS_AUTOCLIENT ? match_browser_filter() : Preferences.GetFilter();
    m_LastRevision   = U32_MAX;
    m_LastSort       = MATCH_SORT_COUNT;
    m_LastDescending = FALSE;
    m_RefreshTimeout = 0.0f;
    m_Rebuilding     = FALSE;

    m_pFilters       = (ui_button*)FindChildByID( IDC_FILTERS );
    m_pMatchList     = (ui_joinlist*)FindChildByID( IDC_MATCHLIST );
    m_pJoin          = (ui_button*)FindChildByID( IDC_JOIN );
    m_pDetails       = (ui_text*)FindChildByID( IDC_DETAILS );
    m_pEndpoint      = (ui_text*)FindChildByID( IDC_ENDPOINT );
    m_pMap           = (ui_text*)FindChildByID( IDC_MAP );
    m_pMode          = (ui_text*)FindChildByID( IDC_MODE );
    m_pPlayers       = (ui_text*)FindChildByID( IDC_PLAYERS );
    m_pPing          = (ui_text*)FindChildByID( IDC_PING );
    m_pPassword      = (ui_text*)FindChildByID( IDC_PASSWORD );
    m_pComplete      = (ui_text*)FindChildByID( IDC_COMPLETE );

    const s32 Buttons[] = { IDC_ADVANCED, IDC_FILTERS, IDC_REFRESH, IDC_JOIN };
    for( s32 i = 0; i < 4; i++ )
    {
        ui_button* pButton = (ui_button*)FindChildByID( Buttons[i] );
        pButton->UseSmallText( TRUE );
        pButton->SetButtonStyle( ui_button::BUTTON_STYLE_FLAT );
    }

    m_pMatchList->SetSort( Preferences.Sort, Preferences.Descending );
    m_pMatchList->SetActive( TRUE );
    m_pMatchList->EnableHeaderBar();
    m_pMatchList->DisableFrame();
    m_pMatchList->EnableParentNavigation();
    m_pMatchList->SetLineHeight( 18 );
    m_pMatchList->SetExitOnSelect( FALSE );
    m_pMatchList->SetExitOnBack( TRUE );
    m_pMatchList->SetBackgroundColor( xcolor( 0, 0, 0, 0 ) );
    m_pMatchList->SetHeaderBarColor( xcolor( 0, 0, 0, 0 ) );
    m_pMatchList->SetHeaderColor( xcolor( 255, 252, 204, 255 ) );

    ui_text* Texts[] = { m_pDetails, m_pEndpoint, m_pMap, m_pMode, m_pPlayers,
                        m_pPing, m_pPassword, m_pComplete };
    for( s32 i = 0; i < (s32)(sizeof( Texts ) / sizeof( Texts[0] )); i++ )
    {
        Texts[i]->UseSmallText( TRUE );
        Texts[i]->UseShadow( TRUE );
        Texts[i]->SetLabelFlags( ui_font::h_left | ui_font::v_center | ui_font::clip_ellipsis );
        Texts[i]->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
    }

    m_pEndpoint->SetLabelFlags( ui_font::h_right | ui_font::v_center | ui_font::clip_ellipsis );

    if( !(CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT) )
    {
        for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
            g_StateMgr.SetControllerRequested( i, FALSE );
    }

    SetNavText( xwstring( g_StringTableMgr( "ui", "IDS_NAV_JOIN" ) ) +
                g_StringTableMgr( "ui", "IDS_NAV_BACK" ) + g_StringTableMgr( "ui", "IDS_NAV_REFRESH" ) );

    if( !CONFIG_IS_AUTOCLIENT && x_strcmp( Preferences.Directory, g_MatchMgr.GetDirectoryAddress() ) )
        g_MatchMgr.ConfigureDirectory( Preferences.Directory );

    Refresh();
    ShowControls( FALSE );
    GotoControl( m_pMatchList );

    InitScreenScaling( Position );
    m_State = DIALOG_STATE_ACTIVE;

    return TRUE;
}

//==============================================================================

void dlg_online_join::Destroy( void )
{
    if( !m_Created )
        return;

    m_Created    = FALSE;
    SavePreferences();

    g_MatchMgr.StopAcquisition();

    ui_dialog::Destroy();
    g_MatchMgr.SetState( MATCH_IDLE );
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_online_join::SavePreferences( void )
{
    if( CONFIG_IS_AUTOCLIENT )
        return;

    multiplayer_preferences& Preferences = g_StateMgr.GetActiveProfile(
        g_StateMgr.GetProfileListIndex(0) ).GetMultiplayerPreferences();
    multiplayer_preferences Updated = Preferences;

    Updated.SetFilter( m_Filter );
    Updated.Sort       = m_pMatchList->GetSort();
    Updated.Descending = m_pMatchList->IsDescending();

    if( x_memcmp( &Preferences, &Updated, sizeof(Preferences) ) == 0 )
        return;

    Preferences = Updated;
    g_StateMgr.SaveMultiplayerPreferences();
}

//==============================================================================

void dlg_online_join::ShowControls( xbool Visible )
{
    for( s32 i = 0; i < m_Children.GetCount(); i++ )
        m_Children[i]->SetFlag( WF_VISIBLE, Visible );
}

//==============================================================================

void dlg_online_join::Render( s32 ox, s32 oy )
{
    if( m_Flags & WF_VISIBLE )
    {
        irect r( m_CurrPos.l + 22, m_CurrPos.t + 28, m_CurrPos.r - 23, m_CurrPos.b - 12 );
        r.Translate( ox, oy );
        g_UiMgr->RenderRect( r, xcolor( 56, 115, 58, 64 ), FALSE );

        s32 Offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
        for( s32 y = r.t + Offset; y < r.b; y += 9 )
            g_UiMgr->RenderRect( irect( r.l, y, r.r, MIN( y + 4, r.b ) ), xcolor( 56, 115, 58, 30 ), FALSE );

        if( !g_UiMgr->IsScreenScaling() )
        {
            const xbool WipeClipped = g_UiMgr->IsWipeActiveFor( this );
            if( WipeClipped )
            {
                irect Clip = m_Position;
                Clip.Translate( ox, oy );
                Clip.b = MAX( Clip.t, MIN( Clip.b, g_UiMgr->GetWipeRevealY() ) );
                g_UiMgr->PushClipWindow( Clip );
            }

            irect Details = m_pMatchList->GetPosition();
            Details.t = 232;
            Details.b = 298;
            irect Panels[] = { m_pMatchList->GetListRect(), Details };
            const s32 HeaderHeights[] = { 22, 20 };
            for( s32 i = 0; i < 2; i++ )
            {
                irect Panel = Panels[i];
                Panel.Translate( m_Position.l + ox, m_Position.t + oy );
                g_UiMgr->RenderRect( Panel, xcolor( 19, 59, 14, 160 ), FALSE );

                irect Header = Panel;
                Header.Deflate( 1, 1 );
                Header.SetHeight( HeaderHeights[i] - 1 );
                g_UiMgr->RenderGouraudRect( Header, xcolor( 39, 117, 28, 224 ), xcolor( 39, 117, 28, 224 ),
                                          xcolor( 19, 59, 14, 128 ), xcolor( 19, 59, 14, 128 ), FALSE );
                g_UiMgr->RenderRect( Panel, xcolor( 79, 214, 60, 224 ), TRUE );
            }

            if( WipeClipped )
                g_UiMgr->PopClipWindow();
        }
    }

    ui_dialog::Render( ox, oy );
    g_UiMgr->RenderGlowBar();
}

//==============================================================================

void dlg_online_join::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses,
                                s32 Repeats, xbool WrapX, xbool WrapY )
{
    if( (pWin == m_pMatchList) && ((Code == ui_navigation::Left) || (Code == ui_navigation::Right)) )
    {
        s32 Direction = (Code == ui_navigation::Right) ? 1 : -1;
        s32 Sort = (m_pMatchList->GetSort() + Direction + MATCH_SORT_COUNT) % MATCH_SORT_COUNT;
        m_pMatchList->SetSort( (match_sort)Sort, FALSE );
        FillMatchList();
        SavePreferences();
        return;
    }

    ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

//==============================================================================

void dlg_online_join::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || m_pFiltersDialog || m_pAdvancedDialog )
        return;

    switch( pWin->GetControlID() )
    {
    case IDC_MATCHLIST:
    case IDC_JOIN:
        JoinSelectedServer();
        break;

    case IDC_REFRESH:
        Refresh();
        break;

    case IDC_FILTERS:
        m_pFiltersDialog = (dlg_browser_filters*)m_pManager->OpenDialog( m_UserID, "browser filters",
            irect( 0, 0, 360, 288 ), NULL, WF_VISIBLE | WF_INPUTMODAL | WF_DLG_CENTER );
        if( m_pFiltersDialog )
            m_pFiltersDialog->Configure( m_Filter );
        break;

    case IDC_ADVANCED:
        m_pAdvancedDialog = (dlg_browser_advanced*)m_pManager->OpenDialog( m_UserID, "browser advanced",
            irect( 0, 0, 360, 288 ), NULL, WF_VISIBLE | WF_INPUTMODAL | WF_DLG_CENTER );
        break;

    default:
        break;
    }
}

//==============================================================================

void dlg_online_join::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( (m_State == DIALOG_STATE_ACTIVE) && !CONFIG_IS_AUTOCLIENT && !m_pFiltersDialog && !m_pAdvancedDialog )
        m_State = DIALOG_STATE_BACK;
}

//==============================================================================

void dlg_online_join::OnAlternate( ui_win* pWin )
{
    if( pWin == m_pMatchList )
    {
        m_pMatchList->SetSort( m_pMatchList->GetSort(), !m_pMatchList->IsDescending() );
        FillMatchList();
        SavePreferences();
    }
}

//==============================================================================

void dlg_online_join::Refresh( void )
{
    if( m_RefreshTimeout > 0.0f )
        return;

    g_MatchMgr.StartAcquisition( ACQUIRE_SERVERS );
    m_RefreshTimeout = 2.0f;
    m_LastRevision   = U32_MAX;
    FillMatchList();
}

//==============================================================================

void dlg_online_join::OnDelete( ui_win* pWin )
{
    (void)pWin;
    Refresh();
}

//==============================================================================

void dlg_online_join::OnNotify( ui_notification const& Event )
{
    if( m_Rebuilding )
        return;

    if( (Event.m_pSender == m_pMatchList) && (Event.m_Type == ui_notification_type::ListSortChanged) )
    {
        FillMatchList();
        SavePreferences();
    }
    else if( (Event.m_pSender == m_pMatchList) && (Event.m_Type == ui_notification_type::ListSelectionChanged) )
        UpdateDetails();
    else
        ui_dialog::OnNotify( Event );
}

//==============================================================================

const server_info* dlg_online_join::GetSelectedServer( void ) const
{
    if( m_pMatchList->GetSelection() < 0 )
        return NULL;

    return m_pMatchList->GetServerInfo( (s32)m_pMatchList->GetSelectedItemData() );
}

//==============================================================================

void dlg_online_join::JoinSelectedServer( void )
{
    const server_info* pSelected = GetSelectedServer();
    if( !pSelected )
        return;

    g_MatchMgr.LockLists();
    s32 Index = g_MatchMgr.FindServerInfo( *pSelected );
    if( Index >= 0 )
    {
        g_PendingConfig.GetConfig() = *g_MatchMgr.GetServerInfo( Index );
        g_PendingConfig.SetPlayerCount( 1 );
        g_StateMgr.SetActiveControllerID( g_UiMgr->GetActiveController() );
        g_StateMgr.SetControllerRequested( g_UiMgr->GetActiveController(), TRUE );
        m_State = DIALOG_STATE_SELECT;
    }

    g_MatchMgr.UnlockLists();
}

//==============================================================================

void dlg_online_join::FillMatchList( void )
{
    u32 Revision = g_MatchMgr.GetServerListRevision();
    match_sort Sort = m_pMatchList->GetSort();
    xbool Descending = m_pMatchList->IsDescending();

    if( (Revision != m_LastRevision) || !(m_Filter == m_LastFilter) ||
        (Sort != m_LastSort) || (Descending != m_LastDescending) )
    {
        server_info Selected;
        const server_info* pSelected = GetSelectedServer();
        xbool HadSelection = pSelected != NULL;
        if( HadSelection )
            Selected = *pSelected;

        s32 Offset = m_pMatchList->GetCursorOffset();

        xarray<server_info> Servers;
        g_MatchMgr.LockLists();
        s32 TotalCount = g_MatchMgr.GetServerCount();
        for( s32 i = 0; i < TotalCount; i++ )
        {
            const server_info& Info = *g_MatchMgr.GetServerInfo( i );
            if( m_Filter.Matches( Info ) )
                Servers.Append( Info );
        }
        g_MatchMgr.UnlockLists();

        for( s32 i = 1; i < Servers.GetCount(); i++ )
        {
            server_info Item = Servers[i];
            s32 j = i;
            while( (j > 0) && (CompareMatchServers( Item, Servers[j-1], Sort, Descending ) < 0) )
            {
                Servers[j] = Servers[j-1];
                j--;
            }
            Servers[j] = Item;
        }

        m_Rebuilding = TRUE;
        m_pMatchList->DeleteAllItems();
        m_pMatchList->SetServers( Servers );

        s32 Selection = -1;
        for( s32 i = 0; i < Servers.GetCount(); i++ )
        {
            s32 Item = m_pMatchList->AddItem( Servers[i].Name, i );
            m_pMatchList->SetItemColor( Item, xcolor( 126, 220, 60, 255 ) );
            if( HadSelection && (Servers[i] == Selected) )
                Selection = i;
        }

        if( !HadSelection && Servers.GetCount() )
            Selection = 0;

        m_pMatchList->SetSelectionWithOffset( Selection, Offset );
        m_Rebuilding = FALSE;

        m_LastRevision   = Revision;
        m_LastFilter     = m_Filter;
        m_LastSort       = Sort;
        m_LastDescending = Descending;
        m_pMatchList->SetLabel( g_StringTableMgr( "ui", TotalCount ? "IDS_BROWSER_NO_MATCHES" : "IDS_BROWSER_NO_SERVERS" ) );
    }

    xbool Filtered = (m_Filter.Source != MATCH_SOURCE_ALL) || m_Filter.Name.GetLength() ||
                     (m_Filter.GameType != GAME_MP) || m_Filter.Map.GetLength() ||
                     (m_Filter.MaxPing >= 0) || m_Filter.HideFull || m_Filter.HideEmpty || m_Filter.HidePassword;
    m_pFilters->SetLabel( xwstring( g_StringTableMgr( "ui", "IDS_BROWSER_FILTERS" ) ) + xwstring( Filtered ? " *" : "" ) );
    UpdateDetails();
}

//==============================================================================

void dlg_online_join::UpdateDetails( void )
{
    const server_info* pServer = GetSelectedServer();
    m_pJoin->SetFlag( WF_DISABLED, pServer == NULL );

    ui_text* Values[] = { m_pEndpoint, m_pMap, m_pMode, m_pPlayers, m_pPing, m_pPassword, m_pComplete };
    if( !pServer )
    {
        m_pDetails->SetLabel( g_StringTableMgr( "ui", "IDS_BROWSER_SELECT_SERVER" ) );
        for( s32 i = 0; i < (s32)(sizeof( Values ) / sizeof( Values[0] )); i++ )
            Values[i]->SetLabel( xwstring( "" ) );
        return;
    }

    m_pDetails->SetLabel( pServer->Name );
    m_pEndpoint->SetLabel( xwstring( pServer->Remote.GetStrAddress() ) );
    m_pMap->SetLabel( xwstring( g_StringTableMgr( "ui", "IDS_BROWSER_MAP" ) ) + xwstring( ": " ) + pServer->MissionName );
    m_pMode->SetLabel( xwstring( g_StringTableMgr( "ui", "IDS_BROWSER_MODE" ) ) + xwstring( ": " ) + pServer->GameType );
    m_pPlayers->SetLabel( xwstring( g_StringTableMgr( "ui", "IDS_HEADER_PLAYERS" ) ) +
                         xwstring( xfs( ": %d/%d", pServer->Players, pServer->MaxPlayers ) ) );
    m_pPing->SetLabel( xwstring( g_StringTableMgr( "ui", "IDS_BROWSER_PING" ) ) +
                      xwstring( (pServer->PingDelay < 0.0f) ? ": --" : xfs( ": %d ms", (s32)pServer->PingDelay ) ) );
    m_pPassword->SetLabel( xwstring( g_StringTableMgr( "ui", "IDS_JOIN_PASSWORD" ) ) + xwstring( " " ) +
                           g_StringTableMgr( "ui", (pServer->Flags & SERVER_HAS_PASSWORD) ? "IDS_YES" : "IDS_NO" ) );
    m_pComplete->SetLabel( xwstring( g_StringTableMgr( "ui", "IDS_BROWSER_COMPLETE" ) ) +
                          xwstring( xfs( ": %d%%", pServer->PercentComplete ) ) );
}

//==============================================================================

void dlg_online_join::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    if( m_pFiltersDialog )
    {
        s32 State = m_pFiltersDialog->GetState();
        if( (State == DIALOG_STATE_SELECT) || (State == DIALOG_STATE_BACK) )
        {
            if( State == DIALOG_STATE_SELECT )
            {
                m_Filter = m_pFiltersDialog->GetFilter();
                SavePreferences();
            }
            m_pManager->EndDialog( m_UserID, TRUE );
            m_pFiltersDialog = NULL;
        }
    }

    if( m_pAdvancedDialog )
    {
        s32 State = m_pAdvancedDialog->GetState();
        if( (State == DIALOG_STATE_SELECT) || (State == DIALOG_STATE_BACK) )
        {
            if( (State == DIALOG_STATE_SELECT) && m_pAdvancedDialog->HasConnection() )
            {
                g_StateMgr.SetActiveControllerID( g_UiMgr->GetActiveController() );
                g_StateMgr.SetControllerRequested( g_UiMgr->GetActiveController(), TRUE );
                m_State = DIALOG_STATE_SELECT;
            }
            m_pManager->EndDialog( m_UserID, TRUE );
            m_pAdvancedDialog = NULL;
        }
    }

    if( g_UiMgr->IsScreenScaling() && !UpdateScreenScaling( DeltaTime ) )
        ShowControls( TRUE );

    m_RefreshTimeout = MAX( 0.0f, m_RefreshTimeout - DeltaTime );
    FindChildByID( IDC_REFRESH )->SetFlag( WF_DISABLED, m_RefreshTimeout > 0.0f );

    g_UiMgr->UpdateGlowBar( DeltaTime );
    FillMatchList();

    if( CONFIG_IS_AUTOCLIENT && (m_State == DIALOG_STATE_ACTIVE) && !m_pFiltersDialog && !m_pAdvancedDialog )
    {
#ifndef CONFIG_RETAIL
        if( g_CmdLineConnectAddress[0] )
        {
            if( g_MatchMgr.PrepareDirectConnection( g_Config.AutoClientAddress ) == MATCH_RESULT_OK )
                m_State = DIALOG_STATE_SELECT;
        }
        else
#endif
        if( GetSelectedServer() )
            JoinSelectedServer();
    }

    ui_dialog::OnUpdate( pWin, DeltaTime );
}
