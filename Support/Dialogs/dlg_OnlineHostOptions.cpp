//==============================================================================
//
//  dlg_OnlineHostOptions.cpp
//
//==============================================================================

#include "Entropy.hpp"
#include "dlg_OnlineHostOptions.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_check.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_edit.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_text.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"

//==============================================================================

enum host_option_controls
{
    IDC_MODE_LABEL,
    IDC_PLAYERS_LABEL,
    IDC_MODE,
    IDC_PLAYERS,
    IDC_SCORE_LABEL,
    IDC_TIME_LABEL,
    IDC_MUTATION_LABEL,
    IDC_VOTE_LABEL,
    IDC_FIRE_LABEL,
    IDC_SCALING_LABEL,
    IDC_SCORE,
    IDC_TIME,
    IDC_MUTATION,
    IDC_VOTE,
    IDC_FIRE,
    IDC_SCALING,
    IDC_ADDRESS_LABEL,
    IDC_ADDRESS,
    IDC_LAN_ONLY,
    IDC_LAN_ONLY_LABEL,
    IDC_CHEATS,
    IDC_CHEATS_LABEL,
    IDC_PASSWORD_LABEL,
    IDC_PORT_LABEL,
    IDC_DIRECTORY_LABEL,
    IDC_PASSWORD,
    IDC_PORT,
    IDC_DIRECTORY,
    IDC_VOICE,
    IDC_PRIVATE,
    IDC_VOICE_LABEL,
    IDC_PRIVATE_LABEL,
    IDC_STATUS,
    IDC_RESET,
    IDC_CANCEL,
    IDC_APPLY,
};

static ui_manager::control_tem HostGameSetupControls[] =
{
    { IDC_MODE_LABEL    , "IDS_BROWSER_MODE"      , "text"  ,  24,  44, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PLAYERS_LABEL , "IDS_HOST_NUM_PLAYERS"  , "text"  ,  24,  72, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_SCORE_LABEL   , "IDS_HOST_SCORE"        , "text"  ,  24, 100, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_TIME_LABEL    , "IDS_HOST_TIME_LIMIT"   , "text"  ,  24, 128, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_MUTATION_LABEL, "IDS_HOST_MUTATION"     , "text"  ,  24, 156, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_VOTE_LABEL    , "IDS_HOST_VOTE_PASS"    , "text"  ,  24, 184, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_FIRE_LABEL    , "IDS_HOST_FRIENDLY_FIRE", "text"  ,  24, 212, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_SCALING_LABEL , "IDS_HOST_MAP_SCALING"  , "text"  ,  48, 244, 288, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_MODE          , "IDS_NULL"              , "combo" , 148,  44, 188, 24, 0, 0, 3, 1, ui_win::WF_VISIBLE },
    { IDC_PLAYERS       , "IDS_NULL"              , "combo" , 148,  72, 188, 24, 0, 1, 3, 1, ui_win::WF_VISIBLE },
    { IDC_SCORE         , "IDS_NULL"              , "combo" , 148, 100, 188, 24, 0, 2, 3, 1, ui_win::WF_VISIBLE },
    { IDC_TIME          , "IDS_NULL"              , "combo" , 148, 128, 188, 24, 0, 3, 3, 1, ui_win::WF_VISIBLE },
    { IDC_MUTATION      , "IDS_NULL"              , "combo" , 148, 156, 188, 24, 0, 4, 3, 1, ui_win::WF_VISIBLE },
    { IDC_VOTE          , "IDS_NULL"              , "combo" , 148, 184, 188, 24, 0, 5, 3, 1, ui_win::WF_VISIBLE },
    { IDC_FIRE          , "IDS_NULL"              , "combo" , 148, 212, 188, 24, 0, 6, 3, 1, ui_win::WF_VISIBLE },
    { IDC_SCALING       , "IDS_NULL"              , "check" ,  24, 244,  20, 24, 0, 7, 3, 1, ui_win::WF_VISIBLE },
    { IDC_RESET         , "IDS_BROWSER_RESET"     , "button",  24, 282,  96, 24, 0, 8, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CANCEL        , "IDS_CANCEL"            , "button", 132, 282,  96, 24, 1, 8, 1, 1, ui_win::WF_VISIBLE },
    { IDC_APPLY         , "IDS_BROWSER_APPLY"     , "button", 240, 282,  96, 24, 2, 8, 1, 1, ui_win::WF_VISIBLE },
};

static ui_manager::dialog_tem HostGameSetupDialog =
{
    "IDS_HOST_GAME_SETUP", 3, 9,
    sizeof( HostGameSetupControls ) / sizeof( ui_manager::control_tem ), HostGameSetupControls, 8
};

static ui_manager::control_tem HostAdvancedControls[] =
{
    { IDC_ADDRESS_LABEL  , "IDS_HOST_LAN_ADDRESS"      , "text"  ,  24,  44, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PORT_LABEL     , "IDS_HOST_PORT"             , "text"  ,  24,  76, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PASSWORD_LABEL , "IDS_HOST_PASSWORD"         , "text"  ,  24, 108, 124, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_DIRECTORY_LABEL, "IDS_BROWSER_MASTER_ADDRESS", "text"  ,  24, 144, 312, 20, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_ADDRESS        , "IDS_NULL"                  , "edit"  , 158,  44, 168, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
    { IDC_PORT           , "IDS_HOST_PORT"             , "edit"  , 158,  76, 168, 24, 0, 0, 4, 1, ui_win::WF_VISIBLE },
    { IDC_PASSWORD       , "IDS_HOST_PASSWORD"         , "edit"  , 158, 108, 168, 24, 0, 1, 4, 1, ui_win::WF_VISIBLE },
    { IDC_DIRECTORY      , "IDS_BROWSER_MASTER_TITLE"  , "edit"  ,  24, 168, 312, 24, 0, 2, 4, 1, ui_win::WF_VISIBLE },
    { IDC_VOICE          , "IDS_NULL"                  , "check" ,  24, 206,  20, 24, 0, 3, 2, 1, ui_win::WF_VISIBLE },
    { IDC_PRIVATE        , "IDS_NULL"                  , "check" , 180, 206,  20, 24, 2, 3, 2, 1, ui_win::WF_VISIBLE },
    { IDC_LAN_ONLY       , "IDS_NULL"                  , "check" ,  24, 236,  20, 24, 0, 4, 2, 1, ui_win::WF_VISIBLE },
    { IDC_CHEATS         , "IDS_NULL"                  , "check" , 180, 236,  20, 24, 2, 4, 2, 1, ui_win::WF_VISIBLE },
    { IDC_VOICE_LABEL    , "IDS_HOST_VOICE_ENABLED"    , "text"  ,  48, 206, 126, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_PRIVATE_LABEL  , "IDS_HOST_PRIVATE_SERVER"   , "text"  , 204, 206, 132, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_LAN_ONLY_LABEL , "IDS_HOST_LAN_ONLY"         , "text"  ,  48, 236, 126, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_CHEATS_LABEL   , "IDS_HOST_ALLOW_CHEATS"     , "text"  , 204, 236, 132, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_STATUS         , "IDS_NULL"                  , "text"  ,  24, 266, 312, 24, 0, 0, 0, 0, ui_win::WF_VISIBLE },
    { IDC_RESET          , "IDS_BROWSER_RESET"         , "button",  24, 298,  96, 24, 0, 5, 1, 1, ui_win::WF_VISIBLE },
    { IDC_CANCEL         , "IDS_CANCEL"                , "button", 132, 298,  96, 24, 1, 5, 2, 1, ui_win::WF_VISIBLE },
    { IDC_APPLY          , "IDS_BROWSER_APPLY"         , "button", 240, 298,  96, 24, 3, 5, 1, 1, ui_win::WF_VISIBLE },
};

static ui_manager::dialog_tem HostAdvancedDialog =
{
    "IDS_BROWSER_ADVANCED", 4, 6,
    sizeof( HostAdvancedControls ) / sizeof( ui_manager::control_tem ), HostAdvancedControls, 5
};

//==============================================================================

static ui_win* dlg_online_host_options_factory( s32                       UserID,
                                               ui_manager*               pManager,
                                               ui_manager::dialog_tem*   pDialogTem,
                                               const irect&              Position,
                                               ui_win*                   pParent,
                                               s32                       Flags,
                                               void*                     pUserData )
{
    dlg_online_host_options* pDialog = new dlg_online_host_options;

    if( !pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData ) )
    {
        delete pDialog;
        return NULL;
    }

    return pDialog;
}

//==============================================================================

void dlg_online_host_options_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "host game setup", &HostGameSetupDialog, dlg_online_host_options_factory );
    pManager->RegisterDialogClass( "host advanced", &HostAdvancedDialog, dlg_online_host_options_factory );
}

//==============================================================================

xbool dlg_online_host_options::Create( s32                       UserID,
                                       ui_manager*               pManager,
                                       ui_manager::dialog_tem*   pDialogTem,
                                       const irect&              Position,
                                       ui_win*                   pParent,
                                       s32                       Flags,
                                       void*                     pUserData )
{
    if( !ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags & ~WF_BORDER ) )
        return FALSE;

    m_Rebuilding = TRUE;
    m_pOptions = (host_options*)pUserData;
    m_Advanced = pDialogTem == &HostAdvancedDialog;
    ASSERT( m_pOptions );

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
            pText->UseShadow( TRUE );
            pText->SetLabelFlags( ui_font::h_left | ui_font::v_center | ui_font::clip_ellipsis );
            pText->SetLabelColor( xcolor( 255, 252, 204, 255 ) );
        }
    }

    if( m_Advanced )
    {
        m_pAddress   = (ui_edit*)FindChildByID( IDC_ADDRESS );
        m_pLanOnly   = (ui_check*)FindChildByID( IDC_LAN_ONLY );
        m_pCheats    = (ui_check*)FindChildByID( IDC_CHEATS );
        m_pPassword  = (ui_edit*)FindChildByID( IDC_PASSWORD );
        m_pPort      = (ui_edit*)FindChildByID( IDC_PORT );
        m_pDirectory = (ui_edit*)FindChildByID( IDC_DIRECTORY );
        m_pVoice     = (ui_check*)FindChildByID( IDC_VOICE );
        m_pPrivate   = (ui_check*)FindChildByID( IDC_PRIVATE );
        m_pStatus    = (ui_text*)FindChildByID( IDC_STATUS );

        m_pPassword->Configure( FALSE );
        m_pPassword->SetBufferSize( NET_PASSWORD_LENGTH );
        m_pPort->Configure( FALSE );
        m_pPort->SetBufferSize( 6 );
        m_pPort->SetFlag( WF_DISABLED, m_pOptions->Reconfigure );
        m_pDirectory->Configure( FALSE );
        m_pDirectory->SetBufferSize( MATCH_ENDPOINT_LENGTH );
        m_pStatus->SetLabelColor( xcolor( 255, 180, 96, 255 ) );
    }
    else
    {
        m_pMode     = (ui_combo*)FindChildByID( IDC_MODE );
        m_pPlayers  = (ui_combo*)FindChildByID( IDC_PLAYERS );
        m_pScore    = (ui_combo*)FindChildByID( IDC_SCORE );
        m_pTime     = (ui_combo*)FindChildByID( IDC_TIME );
        m_pMutation = (ui_combo*)FindChildByID( IDC_MUTATION );
        m_pVote     = (ui_combo*)FindChildByID( IDC_VOTE );
        m_pFire     = (ui_combo*)FindChildByID( IDC_FIRE );
        m_pScaling  = (ui_check*)FindChildByID( IDC_SCALING );

        const char* Modes[] = { "IDS_GAMETYPE_DM", "IDS_GAMETYPE_TDM", "IDS_GAMETYPE_CTF",
                               "IDS_GAMETYPE_TAG", "IDS_GAMETYPE_INF", "IDS_GAMETYPE_CNH" };
        const s32 Types[] = { GAME_DM, GAME_TDM, GAME_CTF, GAME_INF, GAME_CNH };
        for( s32 i = 0; i < 5; i++ )
            m_pMode->AddItem( g_StringTableMgr( "ui", Modes[Types[i]] ), Types[i] );
        m_pMode->SetNavFlags( ui_combo::CB_CHANGE_ON_NAV | ui_combo::CB_CHANGE_ON_SELECT | ui_combo::CB_NOTIFY_PARENT );

        for( s32 i = 2; i <= 16; i++ )
            m_pPlayers->AddItem( xwstring( xfs( "%d", i ) ), i );

        m_pTime->AddItem( g_StringTableMgr( "ui", "IDS_NO_LIMIT" ), (uaddr)-1 );
        for( s32 i = 1; i <= 60; i += (i < 5) ? 1 : 5 )
            m_pTime->AddItem( xwstring( xfs( "%d:00", i ) ), i * 60 );

        m_pVote->AddItem( g_StringTableMgr( "ui", "IDS_VOTE_DISABLED" ), 0 );
        for( s32 i = 5; i <= 100; i += 5 )
            m_pVote->AddItem( xwstring( xfs( "%d%%", i ) ), i );
        for( s32 i = 0; i <= 100; i += 5 )
            m_pFire->AddItem( xwstring( xfs( "%d%%", i ) ), i );
    }

    ReadSettings( m_pOptions->Settings );
    m_Rebuilding = FALSE;
    SetNavText( xwstring( g_StringTableMgr( "ui", "IDS_NAV_SELECT" ) ) + g_StringTableMgr( "ui", "IDS_NAV_BACK" ) );
    m_State = DIALOG_STATE_ACTIVE;

    return TRUE;
}

//==============================================================================

void dlg_online_host_options::ConfigureGameType( s32 Type )
{
    m_pScore->DeleteAllItems();
    m_pMutation->DeleteAllItems();

    const s32 Limits[6][16] =
    {
        {   5,  10,  15,  20,  25,   30,   35,   40,   45,   50,   -1 },
        {  10,  20,  30,  40,  50,   60,   70,   80,   90,  100,  110, 120, 130, 140, 150, -1 },
        {   1,   2,   3,   4,   5,    6,    7,    8,    9,   10,   -1 },
        {  10,  20,  30,  40,  50,   60,   70,   80,   90,  100,  110, 120, 130, 140, 150, -1 },
        {  50,  60,  70,  80,  90,  100,  120,  140,  160,  180,  200,  -1 },
        { 500, 600, 700, 800, 900, 1000, 1200, 1400, 1600, 1800, 2000,  -1 },
    };
    m_pScore->AddItem( g_StringTableMgr( "ui", "IDS_NO_LIMIT" ), (uaddr)-1 );
    for( s32 i = 0; Limits[Type][i] != -1; i++ )
        m_pScore->AddItem( xwstring( xfs( "%d", Limits[Type][i] ) ), Limits[Type][i] );

    if( Type != GAME_INF )
    {
        m_pMutation->AddItem( g_StringTableMgr( "ui", "IDS_MUTATION_MODE_CHANGE_AT_WILL" ), MUTATE_CHANGE_AT_WILL );
        m_pMutation->AddItem( g_StringTableMgr( "ui", "IDS_MUTATION_MODE_HUMAN_LOCK" ), MUTATE_HUMAN_LOCK );
        m_pMutation->AddItem( g_StringTableMgr( "ui", "IDS_MUTATION_MODE_MUTANT_LOCK" ), MUTATE_MUTANT_LOCK );
    }
    if( Type != GAME_DM )
        m_pMutation->AddItem( g_StringTableMgr( "ui", "IDS_MUTATION_MODE_HUMAN_VS_MUTANT" ), MUTATE_HUMAN_VS_MUTANT );
    m_pMutation->SetFlag( WF_DISABLED, m_pMutation->GetItemCount() <= 1 );

    m_pFire->SetFlag( WF_DISABLED, Type == GAME_DM );
}

//==============================================================================

void dlg_online_host_options::OnNotify( ui_notification const& Event )
{
    if( m_Rebuilding )
        return;

    if( m_Advanced )
    {
        if( (Event.m_pSender == m_pLanOnly) && (Event.m_Type == ui_notification_type::CheckChanged) )
            UpdateDirectoryControl();
        return;
    }

    if( (Event.m_pSender == m_pMode) && (Event.m_Type == ui_notification_type::ComboSelectionChanged) )
    {
        s32 Score = (s32)m_pScore->GetSelectedItemData();
        s32 Mutation = (s32)m_pMutation->GetSelectedItemData();
        ConfigureGameType( (s32)m_pMode->GetSelectedItemData() );

        s32 Selection = m_pScore->FindItemByData( (uaddr)Score );
        m_pScore->SetSelection( Selection >= 0 ? Selection : 0 );
        Selection = m_pMutation->FindItemByData( (uaddr)Mutation );
        m_pMutation->SetSelection( Selection >= 0 ? Selection : 0 );
    }
    else
        ui_dialog::OnNotify( Event );
}

//==============================================================================

void dlg_online_host_options::UpdateDirectoryControl( void )
{
    m_pDirectory->SetFlag( WF_DISABLED, m_pLanOnly->IsChecked() ||
                          !(g_MatchMgr.GetCapabilities() & MATCH_CAP_DIRECTORY) );
}

//==============================================================================

void dlg_online_host_options::ReadSettings( const host_settings& Settings )
{
    if( m_Advanced )
    {
        interface_info Info;
        net_GetInterfaceInfo( -1, Info );
        net_address Address( Info.Address, 0 );
        m_pAddress->SetLabel( xwstring( Info.IsAvailable ? Address.GetStrIP() : "--" ) );
        m_pLanOnly->SetChecked( (Settings.m_Flags & SERVER_LAN_ONLY) != 0 );
        m_pCheats->SetChecked( (Settings.m_Flags & SERVER_ALLOW_CHEATS) != 0 );
        UpdateDirectoryControl();

        m_pPassword->SetLabel( xwstring( Settings.m_Password ) );
        m_pVoice->SetChecked( (Settings.m_Flags & SERVER_VOICE_ENABLED) != 0 );
        m_pPrivate->SetChecked( (Settings.m_Flags & SERVER_IS_PRIVATE) != 0 );
        m_pPort->SetLabel( xwstring( xfs( "%d", m_pOptions->Port ) ) );
        m_pDirectory->SetLabel( xwstring( m_pOptions->Directory ) );
        m_pStatus->SetLabel( xwstring( "" ) );
    }
    else
    {
        m_Rebuilding = TRUE;
        s32 Mode = m_pMode->FindItemByData( Settings.m_GameTypeID );
        m_pMode->SetSelection( Mode >= 0 ? Mode : 0 );
        s32 Players = m_pPlayers->FindItemByData( Settings.m_MaxPlayers );
        m_pPlayers->SetSelection( Players >= 0 ? Players : 14 );
        ConfigureGameType( (s32)m_pMode->GetSelectedItemData() );

        ui_combo* Combos[] = { m_pScore, m_pTime, m_pMutation, m_pVote, m_pFire };
        const s32 Values[] = { Settings.m_ScoreLimit, Settings.m_TimeLimit, Settings.m_MutationMode,
                               Settings.m_VotePassPct, Settings.m_FFirePct };
        for( s32 i = 0; i < 5; i++ )
        {
            s32 Selection = Combos[i]->FindItemByData( (uaddr)Values[i] );
            Combos[i]->SetSelection( Selection >= 0 ? Selection : 0 );
        }
        m_pScaling->SetChecked( (Settings.m_Flags & SERVER_ENABLE_MAP_SCALING) != 0 );
        m_Rebuilding = FALSE;
    }
}

//==============================================================================

void dlg_online_host_options::Render( s32 ox, s32 oy )
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

void dlg_online_host_options::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
        m_State = DIALOG_STATE_BACK;
}

//==============================================================================

void dlg_online_host_options::OnAccept( ui_win* pWin )
{
    if( m_State != DIALOG_STATE_ACTIVE )
        return;

    switch( pWin->GetControlID() )
    {
    case IDC_CANCEL:
        OnCancel( pWin );
        break;

    case IDC_RESET:
    {
        global_settings Defaults;
        ReadSettings( Defaults.GetHostSettings() );
        if( m_Advanced )
        {
            m_pPort->SetLabel( xwstring( xfs( "%d", m_pOptions->Reconfigure ? m_pOptions->Port : NET_GAME_PORT ) ) );
            m_pDirectory->SetLabel( xwstring( "" ) );
        }
        break;
    }

    case IDC_APPLY:
    {
        host_settings& Settings = m_pOptions->Settings;
        if( m_Advanced )
        {
            xstring PortText( m_pPort->GetLabel() );
            s32 Port = 0;
            xbool Valid = PortText.GetLength() > 0;
            for( s32 i = 0; i < PortText.GetLength(); i++ )
            {
                if( (PortText[i] < '0') || (PortText[i] > '9') )
                {
                    Valid = FALSE;
                    break;
                }
                Port = Port * 10 + PortText[i] - '0';
            }
            if( !Valid || (Port < 1) || (Port > 65535) )
            {
                m_pStatus->SetLabel( g_StringTableMgr( "ui", "IDS_HOST_INVALID_PORT" ) );
                return;
            }

            xstring Directory( m_pDirectory->GetLabel() );
            match_endpoint Address;
            if( !m_pLanOnly->IsChecked() && Directory.GetLength() && !Address.Parse( Directory, MATCH_DIRECTORY_PORT ) )
            {
                m_pStatus->SetLabel( g_StringTableMgr( "ui", "IDS_BROWSER_INVALID_ADDRESS" ) );
                return;
            }

            x_mstrcpy( Settings.m_Password, m_pPassword->GetLabel() );
            Settings.m_Flags &= ~(SERVER_VOICE_ENABLED | SERVER_IS_PRIVATE | SERVER_LAN_ONLY | SERVER_ALLOW_CHEATS);
            if( m_pVoice->IsChecked() )
                Settings.m_Flags |= SERVER_VOICE_ENABLED;
            if( m_pPrivate->IsChecked() )
                Settings.m_Flags |= SERVER_IS_PRIVATE;
            if( m_pLanOnly->IsChecked() )
                Settings.m_Flags |= SERVER_LAN_ONLY;
            if( m_pCheats->IsChecked() )
                Settings.m_Flags |= SERVER_ALLOW_CHEATS;
            m_pOptions->Port = Port;
            m_pOptions->Directory = Directory;
        }
        else
        {
            Settings.m_GameTypeID   = (s32)m_pMode->GetSelectedItemData();
            Settings.m_MaxPlayers   = (s32)m_pPlayers->GetSelectedItemData();
            Settings.m_ScoreLimit   = (s32)m_pScore->GetSelectedItemData();
            Settings.m_TimeLimit    = (s32)m_pTime->GetSelectedItemData();
            Settings.m_MutationMode = (mutation_mode)m_pMutation->GetSelectedItemData();
            Settings.m_VotePassPct  = (s32)m_pVote->GetSelectedItemData();
            Settings.m_FFirePct     = (s32)m_pFire->GetSelectedItemData();
            Settings.m_Flags &= ~SERVER_ENABLE_MAP_SCALING;
            if( m_pScaling->IsChecked() )
                Settings.m_Flags |= SERVER_ENABLE_MAP_SCALING;
        }

        m_State = DIALOG_STATE_SELECT;
        break;
    }

    default:
        break;
    }
}
