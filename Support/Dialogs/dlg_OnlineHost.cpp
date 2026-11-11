//==============================================================================
//
//  dlg_OnlineHost.cpp
//
//==============================================================================

#include "Entropy.hpp"
#include "dlg_OnlineHost.hpp"
#include "dlg_PopUp.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_edit.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_listbox.hpp"
#include "UI/ui_text.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StateMgr/MapList.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "SaveData/SaveDataMgr.hpp"

//==============================================================================

enum host_controls
{
    IDC_NAME_LABEL,
    IDC_NAME,
    IDC_MAPS,
    IDC_ROTATION,
    IDC_ADD,
    IDC_REMOVE,
    IDC_UP,
    IDC_DOWN,
    IDC_GAME_SETUP,
    IDC_ADVANCED,
    IDC_CREATE,
};

static ui_manager::control_tem OnlineHostControls[] =
{
    { IDC_NAME_LABEL   , "IDS_HOST_SERVER_NAME"  , "text"   ,  45,  38, 410, 18, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_NAME         , "IDS_HOST_SERVER_NAME"  , "edit"   ,  45,  58, 410, 24, 0, 0, 4, 1, ui_win::WF_VISIBLE },
    { IDC_MAPS         , "IDS_MAP_NAME"          , "listbox",  45,  88, 187, 210, 0, 1, 1, 4, ui_win::WF_VISIBLE },
    { IDC_ROTATION     , "IDS_MAP_CYCLE"         , "listbox", 268,  88, 187, 210, 3, 1, 1, 4, ui_win::WF_VISIBLE },
    { IDC_ADD          , "IDS_NULL"              , "button" , 238, 136,  24, 24, 1, 1, 2, 1, ui_win::WF_VISIBLE },
    { IDC_REMOVE       , "IDS_NULL"              , "button" , 238, 166,  24, 24, 1, 2, 2, 1, ui_win::WF_VISIBLE },
    { IDC_UP           , "IDS_NULL"              , "button" , 238, 208,  24, 24, 1, 3, 2, 1, ui_win::WF_VISIBLE },
    { IDC_DOWN         , "IDS_NULL"              , "button" , 238, 238,  24, 24, 1, 4, 2, 1, ui_win::WF_VISIBLE },
    { IDC_GAME_SETUP   , "IDS_HOST_GAME_SETUP"   , "button" ,  45, 306, 130, 24, 0, 5, 2, 1, ui_win::WF_VISIBLE },
    { IDC_ADVANCED     , "IDS_BROWSER_ADVANCED"  , "button" , 185, 306, 130, 24, 2, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CREATE       , "IDS_HOST_CREATE"       , "button" , 325, 306, 130, 24, 3, 5, 1, 1, ui_win::WF_VISIBLE },
};

static ui_manager::dialog_tem OnlineHostDialog =
{
    "IDS_ONLINE_HOST_TITLE", 4, 6,
    sizeof( OnlineHostControls ) / sizeof( ui_manager::control_tem ), OnlineHostControls, 1
};

//==============================================================================

void dlg_online_host_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "online host", &OnlineHostDialog, dlg_online_host_factory );
}

//==============================================================================

ui_win* dlg_online_host_factory( s32                       UserID,
                                ui_manager*               pManager,
                                ui_manager::dialog_tem*   pDialogTem,
                                const irect&              Position,
                                ui_win*                   pParent,
                                s32                       Flags,
                                void*                     pUserData )
{
    dlg_online_host* pDialog = new dlg_online_host;

    if( !pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
    {
        delete pDialog;
        return NULL;
    }

    return pDialog;
}

//==============================================================================

dlg_online_host::~dlg_online_host( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_online_host::Create( s32                       UserID,
                               ui_manager*               pManager,
                               ui_manager::dialog_tem*   pDialogTem,
                               const irect&              Position,
                               ui_win*                   pParent,
                               s32                       Flags,
                               void*                     pUserData )
{
    if( !ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
        return FALSE;

    const multiplayer_preferences& Preferences = g_StateMgr.GetActiveProfile(
        g_StateMgr.GetProfileListIndex(0) ).GetMultiplayerPreferences();

    m_Created = TRUE;
    m_Options.Settings = Preferences.Host;
    m_Options.Directory = Preferences.Directory;
    m_Options.Reconfigure = g_StateMgr.IsPaused();
    m_Options.Port = m_Options.Reconfigure ? g_NetworkMgr.GetSocket().GetPort() : Preferences.HostPort;
    m_pOptionsDialog = NULL;
    m_pPopup = NULL;
    m_ReconfigurePrompt = FALSE;
    m_Rebuilding = TRUE;

    m_pName     = (ui_edit*)FindChildByID( IDC_NAME );
    m_pMaps     = (ui_listbox*)FindChildByID( IDC_MAPS );
    m_pRotation = (ui_listbox*)FindChildByID( IDC_ROTATION );

    if( m_Options.Reconfigure )
        FindChildByID( IDC_CREATE )->SetLabel( g_StringTableMgr( "ui", "IDS_BROWSER_APPLY" ) );

    for( s32 i = 0; i < m_Children.GetCount(); i++ )
    {
        const char* pClass = pDialogTem->pControls[i].pClass;
        if( x_strcmp( pClass, "button" ) == 0 )
        {
            ui_button* pButton = (ui_button*)m_Children[i];
            pButton->UseSmallText( TRUE );
            pButton->SetButtonStyle( ui_button::BUTTON_STYLE_FLAT );
        }
        else if( x_strcmp( pClass, "text" ) == 0 )
        {
            ui_text* pText = (ui_text*)m_Children[i];
            pText->UseSmallText( TRUE );
            pText->UseShadow( TRUE );
            pText->SetLabelFlags( ui_font::h_left | ui_font::v_center | ui_font::clip_ellipsis );
            pText->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        }
    }

    FindChildByID( IDC_ADD )->SetLabel( xwstring( ">" ) );
    FindChildByID( IDC_REMOVE )->SetLabel( xwstring( "<" ) );
    FindChildByID( IDC_UP )->SetLabel( xwstring( (xwchar)0x2191 ) );
    FindChildByID( IDC_DOWN )->SetLabel( xwstring( (xwchar)0x2193 ) );

    m_pName->Configure( FALSE );
    m_pName->SetBufferSize( NET_SERVER_NAME_LENGTH );
    m_pName->SetLabel( m_Options.Settings.m_ServerName );
    if( !m_Options.Settings.m_ServerName[0] )
        m_pName->SetLabel( g_StringTableMgr( "ui", "IDS_HOST_SERVER_NAME" ) );

    ui_listbox* Lists[] = { m_pMaps, m_pRotation };
    for( s32 i = 0; i < 2; i++ )
    {
        Lists[i]->DisableFrame();
        Lists[i]->EnableHeaderBar();
        Lists[i]->EnableParentNavigation();
        Lists[i]->SetLineHeight( 18 );
        Lists[i]->SetExitOnSelect( FALSE );
        Lists[i]->SetAcceptOnRepeatClick( TRUE );
        Lists[i]->SetExitOnBack( TRUE );
        Lists[i]->SetBackgroundColor( xcolor( 0, 0, 0, 0 ) );
        Lists[i]->SetHeaderBarColor( xcolor( 0, 0, 0, 0 ) );
        Lists[i]->SetHeaderColor( xcolor( 255, 252, 204, 255 ) );
    }

    FillMaps( m_Options.Settings.m_MapSettings.m_bUseDefault );
    m_Rebuilding = FALSE;

    SetNavText( xwstring( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) ) + g_StringTableMgr( "ui", "IDS_NAV_BACK" ) );
    ShowControls( FALSE );
    InitScreenScaling( Position );
    m_State = DIALOG_STATE_ACTIVE;

    return TRUE;
}

//==============================================================================

void dlg_online_host::Destroy( void )
{
    if( !m_Created )
        return;

    m_Created = FALSE;
    SavePreferences();
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
    g_UiMgr->ResetScreenWipe();
}

//==============================================================================

void dlg_online_host::ShowControls( xbool Visible )
{
    for( s32 i = 0; i < m_Children.GetCount(); i++ )
        m_Children[i]->SetFlag( WF_VISIBLE, Visible );
}

//==============================================================================

void dlg_online_host::Render( s32 ox, s32 oy )
{
    if( m_Flags & WF_VISIBLE )
    {
        if( m_Options.Reconfigure )
            g_UiMgr->RenderRect( g_UiMgr->GetUserBounds( m_UserID ), xcolor( 0, 0, 0, 180 ), FALSE );

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

            irect Panels[] = { m_pMaps->GetListRect(), m_pRotation->GetListRect() };
            for( s32 i = 0; i < 2; i++ )
            {
                irect Panel = Panels[i];
                Panel.Translate( m_Position.l + ox, m_Position.t + oy );
                g_UiMgr->RenderRect( Panel, xcolor( 19, 59, 14, 160 ), FALSE );

                irect Header = Panel;
                Header.Deflate( 1, 1 );
                Header.SetHeight( 22 );
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

void dlg_online_host::FillMaps( xbool Reset )
{
    host_settings& Settings = m_Options.Settings;
    map_settings& Rotation = Settings.m_MapSettings;
    m_MapGameType = Settings.m_GameTypeID;
    m_MapPlayerLimit = Settings.m_MaxPlayers;
    m_pMaps->DeleteAllItems();

    for( s32 i = 0; i < g_MapList.GetCount(); i++ )
    {
        const map_entry& Entry = *g_MapList.GetByIndex( i );
        if( (Entry.GetGameType() == Settings.m_GameTypeID) &&
            (Entry.GetMaxPlayers() >= Settings.m_MaxPlayers) && Entry.IsAvailable() )
        {
            xwstring Name;
            if( DecodeFontText( Entry.GetDisplayName(), Name ) )
                m_pMaps->AddItem( Name, Entry.GetMapID() );
        }
    }
    m_pMaps->SetSelection( m_pMaps->GetItemCount() ? 0 : -1 );

    s32 Count = 0;
    s32 Maps[MAP_CYCLE_SIZE];
    if( Reset )
    {
        for( s32 i = 0; (i < m_pMaps->GetItemCount()) && (Count < MAP_CYCLE_SIZE); i++ )
            Maps[Count++] = (s32)m_pMaps->GetItemData( i );
    }
    else
    {
        for( s32 i = 0; i < Rotation.m_MapCycleCount; i++ )
        {
            s32 Index = (Rotation.m_MapCycleIdx + i) % Rotation.m_MapCycleCount;
            s32 MapID = Rotation.m_MapCycle[Index];
            if( m_pMaps->FindItemByData( MapID ) >= 0 )
                Maps[Count++] = MapID;
        }
    }

    x_memset( Rotation.m_MapCycle, -1, sizeof( Rotation.m_MapCycle ) );
    for( s32 i = 0; i < Count; i++ )
        Rotation.m_MapCycle[i] = Maps[i];
    Rotation.m_MapCycleCount = Count;
    Rotation.m_MapCycleIdx = 0;
    FillRotation( 0 );
}

//==============================================================================

void dlg_online_host::FillRotation( s32 Selection )
{
    m_pRotation->DeleteAllItems();
    const map_settings& Rotation = m_Options.Settings.m_MapSettings;
    for( s32 i = 0; i < Rotation.m_MapCycleCount; i++ )
    {
        s32 Map = m_pMaps->FindItemByData( Rotation.m_MapCycle[i] );
        if( Map >= 0 )
            m_pRotation->AddItem( xwstring( xfs( "%d. ", i + 1 ) ) + m_pMaps->GetItemLabel( Map ), Rotation.m_MapCycle[i] );
    }
    m_pRotation->SetSelection( MIN( Selection, m_pRotation->GetItemCount() - 1 ) );
}

//==============================================================================

void dlg_online_host::AddMap( void )
{
    map_settings& Rotation = m_Options.Settings.m_MapSettings;
    if( (m_pMaps->GetSelection() < 0) || (Rotation.m_MapCycleCount >= MAP_CYCLE_SIZE) )
        return;

    Rotation.m_MapCycle[Rotation.m_MapCycleCount++] = (s32)m_pMaps->GetSelectedItemData();
    Rotation.m_bUseDefault = FALSE;
    FillRotation( Rotation.m_MapCycleCount - 1 );
    SavePreferences();
}

//==============================================================================

void dlg_online_host::RemoveMap( void )
{
    s32 Selection = m_pRotation->GetSelection();
    if( Selection < 0 )
        return;

    map_settings& Rotation = m_Options.Settings.m_MapSettings;
    for( s32 i = Selection; i < Rotation.m_MapCycleCount - 1; i++ )
        Rotation.m_MapCycle[i] = Rotation.m_MapCycle[i + 1];
    Rotation.m_MapCycle[--Rotation.m_MapCycleCount] = -1;
    Rotation.m_bUseDefault = FALSE;
    FillRotation( Selection );
    SavePreferences();
}

//==============================================================================

void dlg_online_host::MoveMap( s32 Direction )
{
    s32 Selection = m_pRotation->GetSelection();
    map_settings& Rotation = m_Options.Settings.m_MapSettings;
    s32 Target = Selection + Direction;
    if( (Selection < 0) || (Target < 0) || (Target >= Rotation.m_MapCycleCount) )
        return;

    s32 MapID = Rotation.m_MapCycle[Target];
    Rotation.m_MapCycle[Target] = Rotation.m_MapCycle[Selection];
    Rotation.m_MapCycle[Selection] = MapID;
    Rotation.m_bUseDefault = FALSE;
    FillRotation( Target );
    SavePreferences();
}

//==============================================================================

void dlg_online_host::OnNotify( ui_notification const& Event )
{
    if( m_Rebuilding || (m_State != DIALOG_STATE_ACTIVE) || m_pOptionsDialog || m_pPopup )
        return;

    if( Event.m_Type == ui_notification_type::ListAccepted )
    {
        if( Event.m_pSender == m_pMaps )
            AddMap();
        else if( Event.m_pSender == m_pRotation )
            RemoveMap();
    }
    else
        ui_dialog::OnNotify( Event );
}

//==============================================================================

xbool dlg_online_host::FocusMapControl( s32 Start, s32 Direction )
{
    const s32 Controls[] = { IDC_ADD, IDC_REMOVE, IDC_UP, IDC_DOWN };

    for( s32 i = Start; (i >= 0) && (i < 4); i += Direction )
    {
        ui_control* pControl = (ui_control*)FindChildByID( Controls[i] );
        if( GotoControl( pControl ) )
            return TRUE;
    }

    return FALSE;
}

//==============================================================================

void dlg_online_host::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses,
                                 s32 Repeats, xbool WrapX, xbool WrapY )
{
    if( !m_InputEnabled || (m_State != DIALOG_STATE_ACTIVE) || m_pOptionsDialog || m_pPopup )
        return;

    ui_win* pFocused = m_pManager->GetFocusedWindow( m_UserID );
    if( !pFocused )
        return;

    s32 Control = pFocused->GetControlID();
    xbool Footer = (Control == IDC_GAME_SETUP) || (Control == IDC_ADVANCED) || (Control == IDC_CREATE);

    if( ((pFocused == m_pName) && (Code == ui_navigation::Down)) ||
        (pFocused == m_pMaps) || (pFocused == m_pRotation) )
    {
        if( FocusMapControl( 0, 1 ) )
            return;
    }
    else if( Footer && (Code == ui_navigation::Up) )
    {
        if( FocusMapControl( 3, -1 ) )
            return;
    }

    const s32 Controls[] = { IDC_ADD, IDC_REMOVE, IDC_UP, IDC_DOWN };
    for( s32 i = 0; i < 4; i++ )
    {
        if( Control != Controls[i] )
            continue;

        if( Code == ui_navigation::Left )
        {
            GotoControl( m_pMaps );
            return;
        }
        if( Code == ui_navigation::Right )
        {
            GotoControl( m_pRotation );
            return;
        }
        if( Code == ui_navigation::Up )
        {
            if( !FocusMapControl( i - 1, -1 ) )
                GotoControl( m_pName );
            return;
        }
        if( Code == ui_navigation::Down )
        {
            if( !FocusMapControl( i + 1, 1 ) )
                GotoControl( (ui_control*)FindChildByID( IDC_GAME_SETUP ) );
            return;
        }
    }

    ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

//==============================================================================

void dlg_online_host::OnAccept( ui_win* pWin )
{
    if( (m_State != DIALOG_STATE_ACTIVE) || m_pOptionsDialog || m_pPopup )
        return;

    switch( pWin->GetControlID() )
    {
    case IDC_GAME_SETUP:
    case IDC_ADVANCED:
    {
        xbool Advanced = pWin->GetControlID() == IDC_ADVANCED;
        irect Bounds( 0, 0, 360, Advanced ? 338 : 322 );
        m_pOptionsDialog = (dlg_online_host_options*)m_pManager->OpenDialog( m_UserID,
            Advanced ? "host advanced" : "host game setup", Bounds, NULL,
            WF_VISIBLE | WF_INPUTMODAL | WF_DLG_CENTER, &m_Options );
        break;
    }

    case IDC_ADD:       AddMap();          break;
    case IDC_REMOVE:    RemoveMap();       break;
    case IDC_UP:        MoveMap( -1 );     break;
    case IDC_DOWN:      MoveMap( 1 );      break;
    case IDC_CREATE:    LaunchServer();    break;
    default:                              break;
    }
}

//==============================================================================

void dlg_online_host::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( (m_State == DIALOG_STATE_ACTIVE) && !m_pOptionsDialog && !m_pPopup )
    {
        g_AudioMgr.Play( "Backup" );
        m_State = DIALOG_STATE_BACK;
    }
}

//==============================================================================

void dlg_online_host::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    if( m_pOptionsDialog )
    {
        s32 State = m_pOptionsDialog->GetState();
        if( (State == DIALOG_STATE_SELECT) || (State == DIALOG_STATE_BACK) )
        {
            if( State == DIALOG_STATE_SELECT )
            {
                const host_settings& Settings = m_Options.Settings;
                xbool ModeChanged = m_MapGameType != Settings.m_GameTypeID;
                if( ModeChanged || (m_MapPlayerLimit != Settings.m_MaxPlayers) )
                    FillMaps( ModeChanged || Settings.m_MapSettings.m_bUseDefault );
                SavePreferences();
            }

            m_pManager->EndDialog( m_UserID, TRUE );
            m_pOptionsDialog = NULL;
        }
    }

    if( m_pPopup && (m_PopupResult != DLG_POPUP_IDLE) )
    {
        m_pPopup = NULL;
        if( m_ReconfigurePrompt )
        {
            m_ReconfigurePrompt = FALSE;
            if( ((m_PopupResult == DLG_POPUP_YES) || (m_PopupResult == DLG_POPUP_MAYBE)) && ApplySettings() )
            {
                g_StateMgr.ActivatePendingSettings();
                m_State = (m_PopupResult == DLG_POPUP_YES) ? DIALOG_STATE_SELECT : DIALOG_STATE_ACTIVATE;
            }
        }
    }

    if( g_UiMgr->IsScreenScaling() && !UpdateScreenScaling( DeltaTime ) )
    {
        ShowControls( TRUE );
        GotoControl( m_pName );
    }

    s32 Selection = m_pRotation->GetSelection();
    FindChildByID( IDC_ADD )->SetFlag( WF_DISABLED, (m_pMaps->GetSelection() < 0) ||
        (m_Options.Settings.m_MapSettings.m_MapCycleCount >= MAP_CYCLE_SIZE) );
    FindChildByID( IDC_REMOVE )->SetFlag( WF_DISABLED, Selection < 0 );
    FindChildByID( IDC_UP )->SetFlag( WF_DISABLED, Selection <= 0 );
    FindChildByID( IDC_DOWN )->SetFlag( WF_DISABLED, (Selection < 0) || (Selection >= m_pRotation->GetItemCount() - 1) );
    FindChildByID( IDC_CREATE )->SetFlag( WF_DISABLED, m_pRotation->GetItemCount() == 0 );
    g_UiMgr->DisableScreenHighlight();
    g_UiMgr->UpdateGlowBar( DeltaTime );
}

//==============================================================================

void dlg_online_host::ShowMessage( const char* pMessage )
{
    irect Bounds = m_pManager->GetUserBounds( m_UserID );
    m_pPopup = (dlg_popup*)m_pManager->OpenDialog( m_UserID, "popup", Bounds, NULL,
        WF_VISIBLE | WF_BORDER | WF_DLG_CENTER | WF_INPUTMODAL );
    m_pPopup->ConfigureMessage( g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ),
                                g_StringTableMgr( "ui", pMessage ), &m_PopupResult );
}

//==============================================================================

void dlg_online_host::LaunchServer( void )
{
    if( m_pRotation->GetItemCount() == 0 )
    {
        ShowMessage( "IDS_EMPTY_CYCLE_MSG" );
        return;
    }

    const xwstring& Name = m_pName->GetLabel();
    xbool HasName = FALSE;
    for( s32 i = 0; i < Name.GetLength(); i++ )
        HasName |= Name[i] > ' ';
    if( !HasName || (Name == xwstring( "---" )) )
    {
        ShowMessage( "IDS_ENTER_SERVER_NAME" );
        return;
    }

    if( m_Options.Reconfigure )
    {
        irect Bounds( 0, 0, 280, 240 );
        m_pPopup = (dlg_popup*)m_pManager->OpenDialog( m_UserID, "popup", Bounds, NULL,
            WF_VISIBLE | WF_BORDER | WF_DLG_CENTER | WF_INPUTMODAL );
        xwstring NavText( g_StringTableMgr( "ui", "IDS_NAV_APPLY_NOW" ) );
        NavText += g_StringTableMgr( "ui", "IDS_NAV_APPLY_AFTER" );
        NavText += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
        m_pPopup->Configure( Bounds, g_StringTableMgr( "ui", "IDS_RECONFIGURE_POPUP" ), TRUE, TRUE, TRUE,
                            g_StringTableMgr( "ui", "IDS_CONFIRM_RECONFIG_MSG" ), NavText, &m_PopupResult );
        m_ReconfigurePrompt = TRUE;
        return;
    }

    if( !ApplySettings() )
        return;

    g_StateMgr.ActivatePendingSettings();
    m_State = DIALOG_STATE_ACTIVATE;
}

//==============================================================================

xbool dlg_online_host::ApplySettings( void )
{
    if( !(m_Options.Settings.m_Flags & SERVER_LAN_ONLY) &&
        (g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY) &&
        (m_Options.Directory != g_MatchMgr.GetDirectoryAddress()) &&
        (g_MatchMgr.ConfigureDirectory( m_Options.Directory ) != MATCH_RESULT_OK) )
    {
        ShowMessage( "IDS_BROWSER_INVALID_ADDRESS" );
        return FALSE;
    }
    if( !m_Options.Reconfigure && !g_MatchMgr.PrepareHost( m_Options.Port ) )
    {
        ShowMessage( "IDS_DIRECT_PORT_BUSY" );
        return FALSE;
    }

    host_settings& Settings = m_Options.Settings;
    x_wstrcpy( Settings.m_ServerName, m_pName->GetLabel() );
    SavePreferences();
    g_StateMgr.GetPendingSettings().GetHostSettings() = Settings;

    g_PendingConfig.SetServerName( Settings.m_ServerName );
    g_PendingConfig.SetPassword( Settings.m_Password );
    g_PendingConfig.SetPasswordEnabled( Settings.m_Password[0] != 0 );
    g_PendingConfig.SetGameTypeID( (game_type)Settings.m_GameTypeID );
    const char* Modes[] = { "IDS_GAMETYPE_DM", "IDS_GAMETYPE_TDM", "IDS_GAMETYPE_CTF",
                           "IDS_GAMETYPE_TAG", "IDS_GAMETYPE_INF", "IDS_GAMETYPE_CNH" };
    g_PendingConfig.SetGameType( g_StringTableMgr( "ui", Modes[Settings.m_GameTypeID] ) );
    const char* ShortModes[] = { "IDS_SHORT_GAMETYPE_DM", "IDS_SHORT_GAMETYPE_TDM", "IDS_SHORT_GAMETYPE_CTF",
                                "IDS_SHORT_GAMETYPE_TAG", "IDS_SHORT_GAMETYPE_INF", "IDS_SHORT_GAMETYPE_CNH" };
    g_PendingConfig.SetShortGameType( g_StringTableMgr( "ui", ShortModes[Settings.m_GameTypeID] ) );
    g_PendingConfig.SetPlayerCount( m_Options.Reconfigure ? g_ActiveConfig.GetPlayerCount() : 1 );
    g_PendingConfig.SetMaxPlayerCount( Settings.m_MaxPlayers );
    g_PendingConfig.SetScoreLimit( Settings.m_ScoreLimit );
    g_PendingConfig.SetGameTime( Settings.m_TimeLimit );
    g_PendingConfig.SetMutationMode( Settings.m_MutationMode );
    g_PendingConfig.SetVotePercent( Settings.m_VotePassPct );
    g_PendingConfig.SetFirePercent( Settings.m_FFirePct );
    g_PendingConfig.SetMapScalingEnabled( (Settings.m_Flags & SERVER_ENABLE_MAP_SCALING) != 0 );
    g_PendingConfig.SetVoiceEnabled( (Settings.m_Flags & SERVER_VOICE_ENABLED) != 0 );
    g_PendingConfig.SetPrivateServer( (Settings.m_Flags & SERVER_IS_PRIVATE) != 0 );
    g_PendingConfig.SetLanOnly( (Settings.m_Flags & SERVER_LAN_ONLY) != 0 );
    g_PendingConfig.SetCheatsAllowed( (Settings.m_Flags & SERVER_ALLOW_CHEATS) != 0 );

    g_StateMgr.ClearMapCycle();
    for( s32 i = 0; i < Settings.m_MapSettings.m_MapCycleCount; i++ )
        g_StateMgr.InsertMapInCycle( Settings.m_MapSettings.m_MapCycle[i] );
    g_StateMgr.SetMapCycleIndex( 0 );
    g_StateMgr.SetUseDefaultMapCycle( Settings.m_MapSettings.m_bUseDefault );
    s32 MapID = Settings.m_MapSettings.m_MapCycle[0];
    g_PendingConfig.SetLevelID( MapID );
    for( s32 i = 0; i < g_MapList.GetCount(); i++ )
    {
        const map_entry& Entry = *g_MapList.GetByIndex( i );
        if( (Entry.GetMapID() == MapID) && (Entry.GetGameType() == Settings.m_GameTypeID) )
        {
            g_StateMgr.SetLevelIndex( i );
            break;
        }
    }

    g_StateMgr.SetActiveControllerID( m_pManager->GetActiveController() );
    g_StateMgr.SetControllerRequested( m_pManager->GetActiveController(), TRUE );
    g_AudioMgr.Play( "Select_Norm" );

    return TRUE;
}

//==============================================================================

void dlg_online_host::SavePreferences( void )
{
    multiplayer_preferences& Preferences = g_StateMgr.GetActiveProfile(
        g_StateMgr.GetProfileListIndex(0) ).GetMultiplayerPreferences();
    multiplayer_preferences Updated = Preferences;

    Updated.Host = m_Options.Settings;
    x_wstrncpy( Updated.Host.m_ServerName, m_pName->GetLabel(), NET_SERVER_NAME_LENGTH - 1 );
    Updated.Host.m_ServerName[NET_SERVER_NAME_LENGTH - 1] = 0;
    Updated.HostPort = m_Options.Port;
    x_strncpy( Updated.Directory, m_Options.Directory, MATCH_ENDPOINT_LENGTH - 1 );
    Updated.Directory[MATCH_ENDPOINT_LENGTH - 1] = 0;

    if( x_memcmp( &Preferences, &Updated, sizeof(Preferences) ) == 0 )
        return;

    Preferences = Updated;
    g_StateMgr.SaveMultiplayerPreferences();
}
