//=========================================================================
//
//  StateMgr.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "StateMgr/MapList.hpp"
#include "StateMgr/LoreList.hpp"
#include "StateMgr/SecretList.hpp"

#include "StateMgr/StateMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "StringMgr/StringMgr.hpp"
#include "Inventory/Inventory2.hpp"
#include "Objects/NewWeapon.hpp"
#include "Objects/Actor/Actor.hpp"
#include "Objects/Player/Player.hpp"
#include "InputMgr/GamePad.hpp"

#include "UI/ui_button.hpp"
#include "UI/ui_dialog.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_renderer.hpp"
#include "Render/Render.hpp"

#include "Dialogs/dlg_MainMenu.hpp"
#include "Dialogs/dlg_Settings.hpp"
#include "Dialogs/dlg_AudioSettings.hpp"
#include "Dialogs/dlg_Headset.hpp"
#include "Dialogs/dlg_GraphicsSettings.hpp"
#include "Dialogs/dlg_GraphicsAdvancedSettings.hpp"
#include "Dialogs/dlg_DisplaySettings.hpp"
#include "Dialogs/dlg_UISettings.hpp"
#include "Dialogs/dlg_LanguageSettings.hpp"
#include "Dialogs/dlg_PressStart.hpp"
#include "Dialogs/dlg_MultiOptions.hpp"
#include "Dialogs/dlg_LevelSelect.hpp"
#include "Dialogs/dlg_ProfileSelect.hpp"
#include "Dialogs/dlg_ProfileOptions.hpp"
#include "Dialogs/dlg_ProfileControls.hpp"
#include "Dialogs/dlg_ProfileMouseControls.hpp"
#include "Dialogs/dlg_ProfileKeyboardBindings.hpp"
#include "Dialogs/dlg_ProfileGamepadControls.hpp"
#include "Dialogs/dlg_OnlineConnect.hpp"
#include "Dialogs/dlg_OnlineHost.hpp"
#include "Dialogs/dlg_OnlineHostOptions.hpp"
#include "Dialogs/dlg_OnlineJoin.hpp"
#include "Dialogs/dlg_OnlineLevelSelect.hpp"
#include "Dialogs/dlg_Players.hpp"
#include "Dialogs/dlg_StartGame.hpp"
#include "Dialogs/dlg_LoadGame.hpp"
#include "Dialogs/dlg_LevelDesc.hpp"
#include "Dialogs/dlg_PauseMain.hpp"
#include "Dialogs/dlg_EndPause.hpp"
#include "Dialogs/dlg_PopUp.hpp"
#include "Dialogs/dlg_ResumeGame.hpp"
#include "Dialogs/dlg_PauseMP.hpp"
#include "Dialogs/dlg_PauseOnline.hpp"
#include "Dialogs/dlg_Leaderboard.hpp"
#include "Dialogs/dlg_TeamLeaderboard.hpp"
#include "Dialogs/dlg_BigLeaderboard.hpp"
#include "Dialogs/dlg_AvatarSelect.hpp"
#include "Dialogs/dlg_ReportError.hpp"
#include "Dialogs/dlg_CampaignMenu.hpp"
#include "Dialogs/dlg_VoteMap.hpp"
#include "Dialogs/dlg_VoteKick.hpp"
#include "Dialogs/dlg_ServerConfig.hpp"
#include "Dialogs/dlg_ChangeMap.hpp"
#include "Dialogs/dlg_KickPlayer.hpp"
#include "Dialogs/dlg_OnlineLogin.hpp"
#include "Dialogs/dlg_LoreMenu.hpp"
#include "Dialogs/dlg_TeamChange.hpp"
#include "Dialogs/dlg_SecretsMenu.hpp"
#include "Dialogs/dlg_Stats.hpp"
#include "Dialogs/dlg_Autosave.hpp"
#include "Dialogs/dlg_SaveData.hpp"
#include "Dialogs/dlg_Credits.hpp"
#include "Dialogs/dlg_Extras.hpp"
#include "Dialogs/dlg_MultiPlayer.hpp"
#include "Dialogs/dlg_OnlineMain.hpp"

#include "../SaveData/SaveDataMgr.hpp"

#ifdef USE_MOVIES
#include "MoviePlayer/MoviePlayer.hpp"
#endif

#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Connection/GameServer.hpp"
#include "NetworkMgr/Connection/GameClient.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"

#include "Parsing/TextIn.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"
#include "GameLib/LevelLoader.hpp"
#include "IOManager/io_mgr.hpp"

#include "../../Apps/GameApp/Config.hpp"    

#if defined(CONFIG_IS_DEMO)
extern xtimer g_DemoIdleTimer;
#define DEMO_ENDGAME_TIMEOUT (15.0f)
#endif

//=========================================================================
//  Defines
//=========================================================================

#define DIALOG_TOP 24
#define DIALOG_BOTTOM (ui_viewport::CONTENT_HEIGHT - 72)

//=========================================================================
//  Structs                                                                
//=========================================================================
//  Globals                                                                
//=========================================================================

extern xbool    g_bControllerCheck;
state_mgr       g_StateMgr;

extern s32      g_Difficulty;

static const f32 MUSIC_RESTART_DELAY = 0.1f;
static f32       s_MusicRestartDelay = 0.0f;
static s32      s_VoiceID  = 0;
static xbool    s_FirstMap = TRUE;

static s32 s_ProfileOptLeft    = 106;
static s32 s_ProfileOptRight   = 406;

static xbool state_mgr_FinishClearOnlyFrame( void )
{
    return render::EndFrame() && eng_EndFrame();
}

//=========================================================================
//  Player Profile Functions
//=========================================================================

void GetMissionName( s32 MapIndex, xwchar* pBuffer )
{
    text_in     TextIn;
    char        Buffer[128];
    s32         i;

    //TextIn.OpenFile( xfs("%s\\%s", g_RscMgr.GetRootDirectory(), "config.txt") );
    TextIn.OpenFile( "config.txt" );
    TextIn.ReadHeader();
    
    s32  nLevel = TextIn.GetHeaderCount();

    for( i=0; i < nLevel; i++ )
    {
        TextIn.ReadFields();
        TextIn.GetString( "Level", Buffer );
        if( i==MapIndex )
            break;
    }
    TextIn.CloseFile();
    x_wstrcpy(pBuffer,xwstring(Buffer));
}

//=========================================================================
//  State Manager Functions
//=========================================================================

state_mgr::state_mgr( void )
{   
    m_bInited = FALSE;
    m_ProfileNames.SetCapacity( 32 );  // Max number of profiles read from both save datas
}

//=========================================================================

state_mgr::~state_mgr( void )
{
    ASSERT( !m_bInited );
}

//=========================================================================

void state_mgr::Kill( )
{
    ASSERT( m_bInited == TRUE );
#ifdef USE_MOVIES
    if( m_bPlayMovie )
    {
        Movie.Close();
        m_bPlayMovie = FALSE;
    }
    Movie.Kill();
#endif
    m_bSimpleMoviePending   = FALSE;
    m_bSimpleMovieCompleted = FALSE;
    m_bRestoreBackgroundAfterSimpleMovie = FALSE;
    m_PendingSimpleMovieName.Clear();
    m_PendingSimpleMovieFallback.Clear();
    m_bInited = FALSE;
}

//=========================================================================

void state_mgr::Init( void )
{
    ASSERT( !m_bInited );

    // Register dialogs
    dlg_main_menu_register          ( g_UiMgr );
    dlg_settings_register           ( g_UiMgr );
    dlg_audio_settings_register     ( g_UiMgr );
    dlg_headset_register            ( g_UiMgr );
    dlg_graphics_settings_register  ( g_UiMgr );
    dlg_graphics_advanced_settings_register
                                    ( g_UiMgr );
    dlg_display_settings_register   ( g_UiMgr );
    dlg_ui_settings_register        ( g_UiMgr );
    dlg_language_settings_register  ( g_UiMgr );
    dlg_profile_select_register     ( g_UiMgr );
    dlg_profile_options_register    ( g_UiMgr );
    dlg_profile_controls_register   ( g_UiMgr );
    dlg_profile_keyboard_bindings_register
                                    ( g_UiMgr );
    dlg_profile_mouse_controls_register
                                    ( g_UiMgr );
    dlg_profile_gamepad_controls_register
                                    ( g_UiMgr );
    dlg_press_start_register        ( g_UiMgr );
    dlg_multi_player_register       ( g_UiMgr );
    dlg_multi_options_register      ( g_UiMgr );
#ifndef CONFIG_RETAIL
    dlg_level_select_register       ( g_UiMgr );
#endif
    dlg_online_connect_register     ( g_UiMgr );
    dlg_online_main_register        ( g_UiMgr );
    dlg_online_host_register        ( g_UiMgr );
    dlg_online_host_options_register( g_UiMgr );
    dlg_online_join_register        ( g_UiMgr );
    dlg_online_level_select_register( g_UiMgr );
    dlg_players_register            ( g_UiMgr );
    dlg_start_game_register         ( g_UiMgr );
    dlg_load_game_register          ( g_UiMgr );
    dlg_level_desc_register         ( g_UiMgr );
    dlg_pause_main_register         ( g_UiMgr );
    dlg_pause_mp_register           ( g_UiMgr );
    dlg_pause_online_register       ( g_UiMgr );
    dlg_end_pause_register          ( g_UiMgr );
    dlg_popup_register              ( g_UiMgr );
    dlg_resume_game_register        ( g_UiMgr );
    dlg_leaderboard_register        ( g_UiMgr );
    dlg_team_leaderboard_register   ( g_UiMgr );
    dlg_big_leaderboard_register    ( g_UiMgr );
    dlg_avatar_select_register      ( g_UiMgr );

    dlg_report_error_register       ( g_UiMgr );
    dlg_campaign_menu_register      ( g_UiMgr );
    dlg_lore_menu_register          ( g_UiMgr );
    dlg_vote_map_register           ( g_UiMgr );
    dlg_vote_kick_register          ( g_UiMgr );
    dlg_server_config_register      ( g_UiMgr );
    dlg_change_map_register         ( g_UiMgr );
    dlg_kick_player_register        ( g_UiMgr );
    dlg_team_change_register        ( g_UiMgr );
    dlg_online_login_register       ( g_UiMgr );
    dlg_secrets_menu_register       ( g_UiMgr );
    dlg_stats_register              ( g_UiMgr );
    dlg_autosave_register           ( g_UiMgr );
    dlg_save_data_register          ( g_UiMgr );
    dlg_credits_register            ( g_UiMgr );
    dlg_extras_register             ( g_UiMgr );

    // Setup state mgr view
    m_View.SetZLimits( 10, 100.0f*500.0f );
    m_View.SetPosition( vector3(1356,-1922,-285) );
    m_View.LookAtPoint( vector3(0,0,0) );

    // Set initial states
    m_bInited                   = TRUE;
    m_bDoSystemError            = FALSE;
    m_bIsDuplicateLogin         = FALSE;
    m_bIsPaused                 = FALSE;
    m_bRetrySilentAutoSave      = FALSE;
    m_bRetryAutoSaveMenu        = FALSE;
    m_bStartSaveGame            = FALSE;
    m_State                     = SM_IDLE;
    m_PrevState                 = SM_IDLE;
    m_Exit                      = FALSE;
    m_TimeoutTime               = 30.0f;
    m_StartupInfoTime           = 0.0f;
    m_StartupIntroMovieIndex    = 0;
    m_CurrentDialog             = NULL;
    m_pScoreboardDialog         = NULL;
    m_bPlayMovie                = FALSE;
    m_bSimpleMoviePending       = FALSE;
    m_bSimpleMovieCompleted     = FALSE;
    m_bSimpleMovieWaitForCompletion = FALSE;
    m_bRestoreBackgroundAfterSimpleMovie = FALSE;
    m_PendingSimpleMovieName.Clear();
    m_PendingSimpleMovieFallback.Clear();
    m_LeaderboardID             = SM_LEADERBOARD;
    m_CampaignType              = SM_NEW_CAMPAIGN_GAME;
    m_bInventoryIsStored        = FALSE;
    m_bDoDefaultLoadOut         = FALSE;
    m_bCreatingProfile          = FALSE;
    m_NetworkDisconnectDestination = SM_IDLE;
    m_pLeaderboardDialog        = NULL;
    m_ActiveControllerID        = -1;
    m_bAutosaveInProgress       = FALSE;
    m_bSilentSigninStarted      = FALSE;
    m_StartupInfoVoiceID        = -1;

    // initialize dialog controls
    s32 c;
    for( c=0; c<SM_NUM_STATES; c++ )
    {
        m_CurrentControl[c] = -1;
    }

    // initialize map cycle
    ClearMapCycle();

    // Initialize game controls
    m_PlayerCount     =  0;
    m_LocalPlayerSlot = -1;

    // initialize profiles 
    s32 i;
    for( i=0; i<SM_PROFILE_COUNT; i++ )
    {
        m_Profiles[i].SetProfileName( "Empty" );
        m_ProfileListIndex[i] = -1;
        m_SelectedProfile[i]  = 0;
        m_ProfileNotSaved[i]  = TRUE;
    }

    for ( i=0; i<MAX_LOCAL_PLAYERS; i++ )
    {
        m_bControllerRequested[i] = FALSE;
    }
   
    m_PendingProfileIndex = -1;

    // Enable user
    g_UiMgr->EnableUser(g_UiUserID,TRUE);

    // Set up the front end theme - should be moved to UI
    ui_button::SetTextColorNormal       (xcolor(126,220,60,255));   
    ui_button::SetTextColorHightlight   (xcolor(255,252,204,255));
    ui_button::SetTextColorDisabled     (XCOLOR_GREY);
    ui_button::SetTextColorShadow       (xcolor(0,0,0,0));

    ui_dialog::SetTextColorNormal       (xcolor(255,252,204,255));
    ui_dialog::SetTextColorShadow       (XCOLOR_BLACK);

    // determine what levels we can load
    g_MapList.Init();
    g_MapList.LoadDefault();

    // initialize the lore list
    g_LoreList.Init();

    // initialize the secrets list
    g_SecretList.Init();

    g_RscMgr.Load( PRELOAD_FILE("AUDIO/DX_FRONTEND.AUDIOPKG"    ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/SFX_FRONTEND.AUDIOPKG"   ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/MUSIC_FRONTEND.AUDIOPKG" ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/DREAMLAND.AUDIOPKG" ) );    

    // Let's get started!
#if CONFIG_IS_DEMO
    SetState( SM_STARTUP_INTRO );
#else
    if( CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT || CONFIG_IS_AUTOCAMPAIGN || CONFIG_IS_AUTOSPLITSCREEN )
    {
        // bypass movies and go directly to the MP game using the autoClient/Server
        SetState( SM_PRESS_START_SCREEN );
    }
    else
    {
        SetState( SM_STARTUP_INFO );
    }
#endif

#ifndef X_EDITOR
    switch( x_GetLocale() )
    {    
        case XL_LANG_ENGLISH:    // English uses default
        {
        }    
        break;
        
        default:  // PAL
        {
            // set up new profile options dialog size
            s_ProfileOptLeft    = 63;
            s_ProfileOptRight   = 449;
        }
        break;
    }
#endif
}

//=========================================================================

void state_mgr::ReInit( void )
{
    // re-enable the pause menu
    m_Exit = FALSE;
}


//=========================================================================

void state_mgr::SetState( sm_states State, xbool ForceStateChange )
{
    // ForceStateChange can be set TRUE to make sure the change happens, even to the same state.
    if( ForceStateChange || (State != m_State) )
    {
        x_MemSanity();
        LOG_MESSAGE( "state_mgr::SetState", "From:%s, to:%s, MemFree:%d", GetStateName( m_State ), GetStateName( State ), x_MemGetFree() );
        // Reset current dialog flag
        if( m_CurrentDialog )
        {
            m_CurrentDialog->SetState( DIALOG_STATE_ACTIVE );

            // store the current control id
            m_CurrentControl[m_State] = m_CurrentDialog->GetControl();
        }
#if CONFIG_IS_DEMO
    g_DemoIdleTimer.Trip();
#endif

        // Do clean up for current state
        ExitState   ( m_State );

        // reset dialog pointer
        m_CurrentDialog = NULL;
        
        // Set the current state and reset the timeout
        m_Timeout   = m_TimeoutTime;
        m_PrevState = m_State;
        m_State     = State;

        // Initialize new state
        EnterState  ( State );

    }
}

//=========================================================================
#define LABEL_STRING(x) case x: return ( #x );

const char* state_mgr::GetStateName( sm_states State )
{
    switch( State )
    {
    LABEL_STRING( SM_IDLE );

        LABEL_STRING( SM_STARTUP_INFO );
        LABEL_STRING( SM_STARTUP_INTRO );
        LABEL_STRING( SM_SAVE_DATA_BOOT_CHECK );

        LABEL_STRING( SM_PRESS_START_SCREEN );

        LABEL_STRING( SM_MAIN_MENU );
        LABEL_STRING( SM_SETTINGS_MENU );
        LABEL_STRING( SM_SETTINGS_AUDIO );
        LABEL_STRING( SM_SETTINGS_HEADSET );
        LABEL_STRING( SM_SETTINGS_GRAPHICS );
        LABEL_STRING( SM_SETTINGS_GRAPHICS_ADVANCED );
        LABEL_STRING( SM_SETTINGS_DISPLAY );
        LABEL_STRING( SM_SETTINGS_UI );
        LABEL_STRING( SM_SETTINGS_LANGUAGE );
        LABEL_STRING( SM_SETTINGS_SAVE_DATA_SELECT );
        LABEL_STRING( SM_MANAGE_PROFILES );
        LABEL_STRING( SM_MANAGE_PROFILE_OPTIONS );
        LABEL_STRING( SM_MANAGE_PROFILE_CONTROLS );
        LABEL_STRING( SM_MANAGE_PROFILE_AVATAR );
        LABEL_STRING( SM_MANAGE_PROFILE_SAVE_SELECT );
        LABEL_STRING( SM_MANAGE_PROFILE_SAVE_DATA_RESELECT );

        LABEL_STRING( SM_DEMO_EXIT );
        LABEL_STRING( SM_CAMPAIGN_MENU );
        LABEL_STRING( SM_CAMPAIGN_PROFILE_OPTIONS );
        LABEL_STRING( SM_CAMPAIGN_PROFILE_CONTROLS );
        LABEL_STRING( SM_CAMPAIGN_PROFILE_AVATAR );
        LABEL_STRING( SM_CAMPAIGN_PROFILE_SAVE_SELECT );
        LABEL_STRING( SM_CAMPAIGN_SAVE_DATA_RESELECT );
        LABEL_STRING( SM_LOAD_CAMPAIGN );
        LABEL_STRING( SM_SAVE_CAMPAIGN );

        LABEL_STRING( SM_LORE_MAIN_MENU );
        LABEL_STRING( SM_SECRETS_MENU );
        LABEL_STRING( SM_EXTRAS_MENU );
        LABEL_STRING( SM_CREDITS_SCREEN );

        LABEL_STRING( SM_MULTI_PLAYER_MENU );
        LABEL_STRING( SM_MULTI_PLAYER_OPTIONS );
        LABEL_STRING( SM_MULTI_PLAYER_EDIT );
        LABEL_STRING( SM_MP_LEVEL_SELECT );
        LABEL_STRING( SM_MP_SAVE_SETTINGS );
        LABEL_STRING( SM_PROFILE_CONTROLS_MP );
        LABEL_STRING( SM_PROFILE_AVATAR_MP );
        LABEL_STRING( SM_PROFILE_SAVE_SELECT_MP );
        LABEL_STRING( SM_SAVE_DATA_RESELECT_MP );

        LABEL_STRING( SM_PROFILE_SELECT );
        LABEL_STRING( SM_PROFILE_OPTIONS );
        LABEL_STRING( SM_PROFILE_CONTROLS );
        LABEL_STRING( SM_PROFILE_AVATAR );
        LABEL_STRING( SM_PROFILE_MOUSE_CONTROLS );
        LABEL_STRING( SM_PROFILE_KEYBOARD_BINDINGS );
        LABEL_STRING( SM_PROFILE_GAMEPAD_CONTROLS );

        LABEL_STRING( SM_ONLINE_CONNECT );
        LABEL_STRING( SM_NETWORK_DISCONNECT );
        LABEL_STRING( SM_ONLINE_IDENTITY );
        LABEL_STRING( SM_ONLINE_PROFILE_SELECT );
        LABEL_STRING( SM_ONLINE_PROFILE_OPTIONS );
        LABEL_STRING( SM_ONLINE_PROFILE_CONTROLS );
        LABEL_STRING( SM_ONLINE_PROFILE_AVATAR );
        LABEL_STRING( SM_ONLINE_PROFILE_SAVE_SELECT );
        LABEL_STRING( SM_ONLINE_SAVE_DATA_RESELECT );

        LABEL_STRING( SM_ONLINE_MAIN_MENU );
        LABEL_STRING( SM_ONLINE_JOIN_MENU );
        LABEL_STRING( SM_ONLINE_JOIN_SAVE_SETTINGS );
        LABEL_STRING( SM_ONLINE_STATS );
        LABEL_STRING( SM_ONLINE_EDIT_PROFILE );
        LABEL_STRING( SM_ONLINE_EDIT_CONTROLS );
        LABEL_STRING( SM_ONLINE_EDIT_AVATAR );
        LABEL_STRING( SM_ONLINE_LOGIN );    

        LABEL_STRING( SM_ONLINE_HOST_MENU );
        LABEL_STRING( SM_HOST_SAVE_SETTINGS );

#ifndef CONFIG_RETAIL
        LABEL_STRING( SM_LEVEL_SELECT );
#endif
        LABEL_STRING( SM_START_GAME );
        LABEL_STRING( SM_START_SAVE_GAME );
        LABEL_STRING( SM_SINGLE_PLAYER_LOAD_MISSION );
        LABEL_STRING( SM_MULTI_PLAYER_LOAD_MISSION );
        LABEL_STRING( SM_CLIENT_WAIT_FOR_MISSION );
        LABEL_STRING( SM_SERVER_SYNC );
        LABEL_STRING( SM_CLIENT_SYNC );
        LABEL_STRING( SM_SERVER_COOLDOWN );
        LABEL_STRING( SM_CLIENT_COOLDOWN );
        LABEL_STRING( SM_SERVER_DISCONNECT );
        LABEL_STRING( SM_CLIENT_DISCONNECT );

        LABEL_STRING( SM_RESUME_CAMPAIGN );

        LABEL_STRING( SM_PLAYING_GAME );

        LABEL_STRING( SM_PAUSE_MAIN_MENU );
        LABEL_STRING( SM_PAUSE_OPTIONS );
        LABEL_STRING( SM_PAUSE_CONTROLS );
        LABEL_STRING( SM_PAUSE_SETTINGS );
        LABEL_STRING( SM_PAUSE_AUDIO );
        LABEL_STRING( SM_PAUSE_HEADSET );
        LABEL_STRING( SM_PAUSE_GRAPHICS );
        LABEL_STRING( SM_PAUSE_GRAPHICS_ADVANCED );
        LABEL_STRING( SM_PAUSE_DISPLAY );
        LABEL_STRING( SM_PAUSE_UI );
        LABEL_STRING( SM_PAUSE_LANGUAGE );
        LABEL_STRING( SM_PAUSE_SETTINGS_SELECT );
        LABEL_STRING( SM_PAUSE_PROFILE_SAVE_SELECT );
        LABEL_STRING( SM_PAUSE_SAVE_DATA_RESELECT );

        LABEL_STRING( SM_PAUSE_MP );
        LABEL_STRING( SM_PAUSE_MP_SCORE );
        LABEL_STRING( SM_PAUSE_MP_OPTIONS );
        LABEL_STRING( SM_PAUSE_MP_CONTROLS );
        LABEL_STRING( SM_PAUSE_MP_SETTINGS );
        LABEL_STRING( SM_PAUSE_MP_AUDIO );
        LABEL_STRING( SM_PAUSE_MP_HEADSET );
        LABEL_STRING( SM_PAUSE_MP_GRAPHICS );
        LABEL_STRING( SM_PAUSE_MP_GRAPHICS_ADVANCED );
        LABEL_STRING( SM_PAUSE_MP_DISPLAY );
        LABEL_STRING( SM_PAUSE_MP_UI );
        LABEL_STRING( SM_PAUSE_MP_LANGUAGE );
        LABEL_STRING( SM_PAUSE_MP_SETTINGS_SELECT );
        LABEL_STRING( SM_PAUSE_MP_PROFILE_SAVE_SELECT );
        LABEL_STRING( SM_PAUSE_MP_SAVE_DATA_RESELECT );

        LABEL_STRING( SM_PAUSE_ONLINE );
        LABEL_STRING( SM_PAUSE_ONLINE_VOTE_MAP );
        LABEL_STRING( SM_PAUSE_ONLINE_VOTE_KICK );
        LABEL_STRING( SM_PAUSE_ONLINE_PLAYERS );
        LABEL_STRING( SM_PAUSE_ONLINE_SERVER_CONFIG );
        LABEL_STRING( SM_PAUSE_ONLINE_OPTIONS );
        LABEL_STRING( SM_PAUSE_ONLINE_CONTROLS );
        LABEL_STRING( SM_PAUSE_ONLINE_SETTINGS );
        LABEL_STRING( SM_PAUSE_ONLINE_AUDIO );
        LABEL_STRING( SM_PAUSE_ONLINE_HEADSET );
        LABEL_STRING( SM_PAUSE_ONLINE_GRAPHICS );
        LABEL_STRING( SM_PAUSE_ONLINE_GRAPHICS_ADVANCED );
        LABEL_STRING( SM_PAUSE_ONLINE_DISPLAY );
        LABEL_STRING( SM_PAUSE_ONLINE_UI );
        LABEL_STRING( SM_PAUSE_ONLINE_LANGUAGE );
        LABEL_STRING( SM_PAUSE_ONLINE_SAVE_SELECT );
        LABEL_STRING( SM_PAUSE_ONLINE_SAVE_DATA_RESELECT );
        LABEL_STRING( SM_PAUSE_ONLINE_CHANGE_MAP );
        LABEL_STRING( SM_PAUSE_ONLINE_KICK_PLAYER );

        LABEL_STRING( SM_PAUSE_ONLINE_TEAM_CHANGE );
        LABEL_STRING( SM_PAUSE_ONLINE_RECONFIG );
        LABEL_STRING( SM_PAUSE_ONLINE_SAVE_SETTINGS );

        LABEL_STRING( SM_END_PAUSE );
        LABEL_STRING( SM_EXIT_GAME );
        LABEL_STRING( SM_POST_GAME );

        LABEL_STRING( SM_AUTOSAVE_MENU );
        LABEL_STRING( SM_AUTOSAVE_PROFILE_RESELECT );
        LABEL_STRING( SM_END_AUTOSAVE );

        LABEL_STRING( SM_FINAL_SCORE );
        LABEL_STRING( SM_ADVANCE_LEVEL );
        LABEL_STRING( SM_RELOAD_CHECKPOINT );
        LABEL_STRING( SM_REPORT_ERROR );

        LABEL_STRING( SM_GAME_EXIT_PROMPT_FOR_SAVE );
        LABEL_STRING( SM_GAME_EXIT_SAVE_SELECT );
        LABEL_STRING( SM_GAME_EXIT_SAVE_DATA_RESELECT );
        LABEL_STRING( SM_GAME_EXIT_SAVE_SETTINGS );
        LABEL_STRING( SM_GAME_EXIT_SETTINGS_OVERWRITE );
        LABEL_STRING( SM_GAME_EXIT_SETTINGS_SELECT );
        LABEL_STRING( SM_GAME_EXIT_REDIRECT );
        LABEL_STRING( SM_GAME_OVER );

        default:                                ASSERTS( FALSE, "Unknown state" );
    }

    return NULL;
}

//=========================================================================

void state_mgr::SimplePopUp( f32 Timeout, const xwchar* Title, const xwchar* Message, const xwchar* Message2 )
{
    irect r = g_UiMgr->GetUserBounds(g_UiUserID);
    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog( g_UiUserID, "popup", r, m_CurrentDialog, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER);
    m_PopUp->Configure( Timeout, Title, Message, Message2 );
    
}

//=========================================================================

void state_mgr::SystemError( sm_system_error MessageID, void* pUserData )
{
#ifndef X_EDITOR
    xbool CanExit = TRUE;
    xwstring MessageText;
    xwstring NavText;

    // No controller errors while profile operations are pending.
    if(( MessageID == SM_SYS_ERR_CONTROLLER ) && 
       ( m_CurrentDialog ) && 
       ( m_CurrentDialog->GetState() == DIALOG_STATE_WAIT_FOR_SAVE_DATA ))
    {
        return;
    }

    // Silent auto-save - must wait for dialog to animate
    if(( MessageID == SM_SYS_ERR_CONTROLLER ) &&
        ( GetState() == SM_AUTOSAVE_MENU ) &&
        ( g_UiMgr->IsScreenScaling() ))
    {
        return;
    }

    // enter system error state
    g_StateMgr.m_bDoSystemError    = TRUE;

    // save current paused state, and then set it to pause.
    g_StateMgr.m_bWasPaused        = g_StateMgr.m_bIsPaused;
    g_StateMgr.m_bIsPaused         = TRUE;
    if( (m_State == SM_PLAYING_GAME) && (!g_NetworkMgr.IsOnline()) )
    {
        g_AudioMgr.PauseAll();
    }

    if( g_UiMgr->IsUserEnabled( g_UiUserID ) )
    {
        g_StateMgr.m_bUserWasEnabled = TRUE;
    }
    else
    {
        g_StateMgr.m_bUserWasEnabled = FALSE;
        g_UiMgr->EnableUser( g_UiUserID, TRUE );
    }

    irect r = g_UiMgr->GetUserBounds(g_UiUserID);

    g_StateMgr.m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(
        g_UiUserID,
        "popup",
        r,
        NULL,
        ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|ui_win::WF_INPUTMODAL );

    r.Set( 0, 0, 320, 180 );

    switch( MessageID )
    {
        case SM_SYS_ERR_DISK:
        {
            // Xbox disk error
            MessageText = g_StringTableMgr( "ui", "IDS_HARD_DISK_ERROR" );
            CanExit = FALSE;

            g_StateMgr.m_PopUp->Configure(
                r,
                g_StringTableMgr( "ui", "IDS_SYSTEM_MESSAGE" ), 
                CanExit,
                FALSE,  
                FALSE, 
                MessageText,
                NavText,
                &g_StateMgr.m_PopUpResult );

        }
        break;
        
        case SM_SYS_ERR_CONTROLLER:
        {
            s32 ControllerID = *(s32*)pUserData;

            g_UiMgr->SetUserController(g_UiUserID, ControllerID);

            if( ControllerID == -1 )
            {   // no controllers connected (not port specific).
                MessageText = g_StringTableMgr( "ui", "IDS_NO_CONTROLLERS" );
            }
            else
            {
                MessageText = g_StringTableMgr( "ui", "IDS_LOST_CONTROLLER" );
                s32 Index = MessageText.Find( 0x0023 );
                ASSERT( Index > -1 );

                // !! if we don't find our string, this will crash. "<NULL>" is better.
                if( Index > -1 )
                {
                    char temp[32];
                    x_sprintf( temp,"%d",ControllerID+1 );

                    xwstring Final;
                    Final += MessageText.Left( Index );
                    Final += xwstring( temp );
                    Final += MessageText.Right( MessageText.GetLength( )-Index-1 );

                    MessageText = Final;
                }
            }
            g_StateMgr.m_PopUp->Configure(
                r,
                g_StringTableMgr( "ui", "IDS_SYSTEM_MESSAGE" ), 
                CanExit,
                FALSE,  
                FALSE, 
                MessageText,
                NavText,
                &g_StateMgr.m_PopUpResult );
        }
        break;

        case SM_SYS_ERR_DUPLICATE_LOGIN :
        {

            MessageText = g_StringTableMgr( "ui", "IDS_ONLINE_DUPLICATE_LOGIN" );
            NavText     = g_StringTableMgr( "ui", "IDS_NAV_NETWORK_CONTINUE"   );

            g_StateMgr.m_PopUp->Configure(
                r,
                g_StringTableMgr( "ui", "IDS_ONLINE_MAIN_XBOX" ), 
                TRUE,
                FALSE,  
                FALSE,
                MessageText,
                NavText,
                &g_StateMgr.m_PopUpResult );

        }
        break;

        default:
        {
            ASSERTS( FALSE, "Unexpected system message!" );
        }
        break;
    }

#else // not editor
    (void)MessageID;
    (void)pUserData; 
#endif
}

//=========================================================================
//
//  Check for pulled controllers.
//

void state_mgr::CheckControllers( void )
{
#if !defined(X_EDITOR)

// TODO: GS: Handle THIS normally

    input_gadget ControllerQuery;
    input_gadget AnalogQuery;
    return;

    if( !g_bControllerCheck )
        return;

    if( !InSystemError() )
    {
        s32 ControllerID;

        switch (m_State)
        {
            case SM_START_GAME:
            case SM_START_SAVE_GAME:
            case SM_SINGLE_PLAYER_LOAD_MISSION:
            case SM_MULTI_PLAYER_LOAD_MISSION:
            case SM_SERVER_SYNC:
            case SM_SERVER_COOLDOWN:
            case SM_SERVER_DISCONNECT:
            case SM_CLIENT_SYNC:
            case SM_CLIENT_COOLDOWN:
            case SM_CLIENT_DISCONNECT:
            case SM_EXIT_GAME:
            case SM_POST_GAME:
            {
                // Not Safe! Not Safe! 
            }    
            break;
            
            case SM_PLAYING_GAME:
            {
                // check all ingame pads with valid IDs
                for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
                {
                    ControllerID = g_GameInput.GetPlayerDevice( i );
                    if( ControllerID == -1 ) 
                        continue;
                    if( !(g_Input.GetFrameSnapshot().IsPresent( ControllerQuery, ControllerID ) &&
                         g_Input.GetFrameSnapshot().IsPresent( AnalogQuery, ControllerID ) ) )
                    {
                        SystemError( SM_SYS_ERR_CONTROLLER, &ControllerID );
                        break;
                    }
                }
            }
            break;
            
            default:
            {
                // only check for "locked" controllers
                if( (ControllerID = GetActiveControllerID()) != -1 )
                {
                    if( !(g_Input.GetFrameSnapshot().IsPresent( ControllerQuery, ControllerID ) &&
                          g_Input.GetFrameSnapshot().IsPresent( AnalogQuery, ControllerID ) ) )
                    {
                        SystemError( SM_SYS_ERR_CONTROLLER, &ControllerID );
                    }
                }
            }
            break;
        }
    }

#endif // !X_EDITOR
}

//=========================================================================

void state_mgr::Update( f32 DeltaTime )
{
    if( s_MusicRestartDelay > 0.0f )
    {
        s_MusicRestartDelay -= DeltaTime;
        if( s_MusicRestartDelay < 0.0f )
            s_MusicRestartDelay = 0.0f;
    }

    // update timeout
    m_Timeout -= DeltaTime;

    // Do the memory card update
    g_SaveDataMgr.Update( DeltaTime );

    // Process UI input
    g_UiMgr->ProcessInput( DeltaTime );

    // Update based on state
    UpdateState( m_State, DeltaTime );

    // update UI
    g_UiMgr->Update( DeltaTime );

    if( m_bDoSystemError && m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            if (( m_PopUpResult == DLG_POPUP_YES ) ||
                ( m_PopUpResult == DLG_POPUP_HELP )) // Used for Xbox on lost controllers.
            {
                m_bDoSystemError = FALSE;
                m_bIsPaused = m_bWasPaused;
                m_PopUp = NULL;
                if( !m_bUserWasEnabled )
                {
                    g_UiMgr->EnableUser( g_UiUserID, FALSE );
                }

                if( m_State == SM_PLAYING_GAME )
                {
                    m_bIsPaused = FALSE;
                    g_AudioMgr.ResumeAll(); 
                }

                if( m_bRetrySilentAutoSave )
                {
                    SilentSaveProfile();
                }

                if( m_bRetryAutoSaveMenu )
                {
                    // goto autosave menu
                    m_bRetryAutoSaveMenu = FALSE;
                    SetState( SM_AUTOSAVE_MENU );
                }
                
                if( m_bIsDuplicateLogin == TRUE )
                {
                    m_bIsDuplicateLogin = FALSE;
                    SetState( SM_MAIN_MENU, TRUE );
                }
            }
        }
    }

    // update menu music
    if( ( m_State > SM_PRESS_START_SCREEN ) && ( m_State < SM_END_PAUSE ) && ( !IsPaused() ) )
    {
        switch( m_State ) 
        {
            case SM_PLAYING_GAME:
            case SM_MULTI_PLAYER_LOAD_MISSION:
            case SM_SINGLE_PLAYER_LOAD_MISSION:
            case SM_START_GAME:
            case SM_START_SAVE_GAME:
            {
                // kill background music
                KillFrontEndMusic();
            }
            break;

            //case SM_CREDITS_SCREEN:
            //{
            //    if( m_bPlayMovie )
            //    {
            //        // keep it looping
            //        if( !g_AudioMgr.IsValidVoiceId( s_VoiceID ) )
            //        {
            //            if( !s_Delay )
            //            {
            //                InitFrontEndMusic("MUSIC_CreditsTheme");
            //
            //                s_Delay = 5;
            //            }
            //        }
            //    }
            //}
            //break;
            //
            //case SM_SECRETS_MENU:
            //{
            //    if( m_bPlayMovie )
            //    {
            //        // keep it looping
            //        if( !g_AudioMgr.IsValidVoiceId( s_VoiceID ) )
            //        {
            //            if( !s_Delay )
            //            {
            //                InitFrontEndMusic("MUSIC_SecretsDemo");
            //
            //                s_Delay = 5;
            //            }
            //        }
            //    }
            //}
            //break;

            default:
            {
                // keep it looping
                if( !g_AudioMgr.IsValidVoiceIdCached( s_VoiceID ) )
                {
                    if( s_MusicRestartDelay <= 0.0f )
                    {
                        InitFrontEndMusic("MUSIC_MenuBackground");
                        s_MusicRestartDelay = MUSIC_RESTART_DELAY;
                    }
                }
            }
            break;
        }
    }
    else
    {
        // kill background music
        KillFrontEndMusic();
    }
}

//=========================================================================

void state_mgr::UpdateLevelLoading( f32 DeltaTime )
{
    // A loading frame updates the active dialog but must not advance the
    // state machine until NetworkMgr::LoadMissionComplete has been called.
    g_SaveDataMgr.Update( DeltaTime );
    g_UiMgr->ProcessInput( DeltaTime );
    g_UiMgr->Update( DeltaTime );
}


//=========================================================================

void state_mgr::Render( void )
{
    if( ( m_State != SM_PLAYING_GAME ) || m_bDoSystemError )
        RenderLoading();
}

//=========================================================================

void state_mgr::RenderLoading( void )
{
    // Setup camera view
    eng_MaximizeViewport( m_View );
    eng_SetView ( m_View );
    eng_SetBackColor( xcolor( 0,0,0 ) );

#if defined( USE_MOVIES )
    // render the movie in the background
    if( m_bPlayMovie )
    {
        if( Movie.IsPlaying() )
        {
            Movie.Render();
        }
    }
#endif

    // Render UI
    g_UiMgr->Render();
    if( m_State == SM_PLAYING_GAME && !m_bDoSystemError )
    {
        irect TextRect( 0, 0, ui_viewport::CONTENT_WIDTH, ui_viewport::CONTENT_HEIGHT );
        g_UiMgr->RenderText( g_UiMgr->FindFont( "large" ), TextRect,
                            ui_font::h_center | ui_font::v_center, XCOLOR_WHITE,
                            g_StringTableMgr( "ui", "IDS_LOADING_MSG" ) );
    }
}

//=========================================================================

void state_mgr::DummyScreen( const char* message, xbool canSkip, s32 waitTime )
{
    s32 X,Y;
    eng_GetRes( X,Y );
    f32 YRes = (f32)Y;

#if (defined(bwatson)||defined(jpcossigny)) && defined(X_DEBUG)
    waitTime = 0;
#endif
    //==-----------------------------------------
    //  Run
    //==-----------------------------------------
    xtimer Time;
    xtimer FrameTimer;
    xbool  bSkip = FALSE;

    Time.Start();
    FrameTimer.Start();
    while (Time.ReadSec() < waitTime && !bSkip)
    {
        if( !eng_BeginFrame() )
        {
            break;
        }
        render::BeginFrame();

        const f32 DeltaTime = FrameTimer.TripSec();
        if( !x_isvalid( DeltaTime ) || (DeltaTime <= 0.0f) || (DeltaTime > 0.1f) )
        {
            x_DebugMsg( "StateMgr: discarding DummyScreen frame with invalid delta %.6f\n",
                        DeltaTime );
            if( !state_mgr_FinishClearOnlyFrame() )
            {
                break;
            }
            continue;
        }

        {
            X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.Update" );

            if( canSkip )
            {
                g_FrontendInput.Update( DeltaTime );

                for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
                {
                    const frontend_pad& Pad = g_FrontendInput.GetPad( i );
                    if( (Pad.GetFrameLogical( frontend_pad::UI_SELECT   ).GetIsValue() > 0.25f) ||
                        (Pad.GetFrameLogical( frontend_pad::UI_SELECT   ).GetWasValue() > 0.25f) ||
                        (Pad.GetFrameLogical( frontend_pad::UI_BACK     ).GetIsValue() > 0.25f) ||
                        (Pad.GetFrameLogical( frontend_pad::UI_BACK     ).GetWasValue() > 0.25f) ||
                        (Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetIsValue() > 0.25f) ||
                        (Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetWasValue() > 0.25f) )
                    {
                        bSkip = TRUE;
                    }
                }
            }
        }

        eng_MaximizeViewport( m_View );
        eng_SetView ( m_View );
        eng_SetBackColor( xcolor(0,0,0) );

        if( !eng_Begin("DummyScreen") )
        {
            if( !render::EndFrame() || !eng_EndFrame() )
            {
                break;
            }
            continue;
        }

        f32 Size = YRes;
        f32 HalfSize = Size/2;
        f32 W,H;

        W = HalfSize;
        H = W/1.3f;

        s32     Line = 12;
        x_printfxy( 30-x_strlen(message)/2, Line++, "%s", message );

        eng_End();

        if( !render::EndFrame( FALSE ) || !eng_EndFrame() )
        {
            break;
        }
    }
}

//=========================================================================

void state_mgr::EnterState( sm_states State )
{
    switch( State )
    {
        case SM_STARTUP_INFO:                      EnterStartupInfo();                 break;
        case SM_STARTUP_INTRO:                     EnterStartupIntro();                break;
        case SM_SAVE_DATA_BOOT_CHECK:              EnterSaveDataBootCheck();           break;
        case SM_PRESS_START_SCREEN:                EnterPressStart();                  break;
        case SM_MAIN_MENU:                         EnterMainMenu();                    break;
        case SM_SETTINGS_MENU:                     EnterSettingsMenu();                break;
        case SM_SETTINGS_AUDIO:                    EnterSettingsAudio();               break;
        case SM_SETTINGS_HEADSET:                  EnterSettingsHeadset();             break;
        case SM_SETTINGS_GRAPHICS:                 EnterSettingsGraphics();            break;
        case SM_SETTINGS_GRAPHICS_ADVANCED:        EnterSettingsGraphicsAdvanced();   break;
        case SM_SETTINGS_DISPLAY:                  EnterSettingsDisplay();             break;
        case SM_SETTINGS_UI:                       EnterSettingsUI();                 break;
        case SM_SETTINGS_LANGUAGE:                 EnterSettingsLanguage();            break;
        case SM_SETTINGS_SAVE_DATA_SELECT:         EnterSettingsSaveDataSelect();      break;
        case SM_MANAGE_PROFILES:                   EnterManageProfiles();              break;
        case SM_MANAGE_PROFILE_OPTIONS:            EnterManageProfileOptions();        break;
        case SM_MANAGE_PROFILE_CONTROLS:           EnterManageProfileControls();       break;
        case SM_MANAGE_PROFILE_AVATAR:             EnterManageProfileAvatar();         break;
        case SM_MANAGE_PROFILE_SAVE_SELECT:        EnterManageProfileSaveSelect();     break;
        case SM_MANAGE_PROFILE_SAVE_DATA_RESELECT: EnterManageSaveDataReselect();      break;
        case SM_CAMPAIGN_MENU:                     EnterCampaignMenu();                break;
        case SM_CAMPAIGN_PROFILE_OPTIONS:          EnterCampaignProfileOptions();      break;
        case SM_CAMPAIGN_PROFILE_CONTROLS:         EnterCampaignProfileControls();     break;
        case SM_CAMPAIGN_PROFILE_AVATAR:           EnterCampaignProfileAvatar();       break;
        case SM_CAMPAIGN_PROFILE_SAVE_SELECT:      EnterCampaignProfileSaveSelect();   break;
        case SM_CAMPAIGN_SAVE_DATA_RESELECT:       EnterCampaignSaveDataReselect();    break;
        case SM_LOAD_CAMPAIGN:                     EnterLoadCampaign();                break;
        case SM_SAVE_CAMPAIGN:                     EnterSaveCampaign();                break;
        case SM_LORE_MAIN_MENU:                    EnterLoreMainMenu();                break;
        case SM_SECRETS_MENU:                      EnterSecretsMenu();                 break;
        case SM_EXTRAS_MENU:                       EnterExtrasMenu();                  break;
        case SM_CREDITS_SCREEN:                    EnterCreditsScreen();               break;
        case SM_PROFILE_SELECT:                    EnterProfileSelect();               break;
        case SM_PROFILE_OPTIONS:                   EnterProfileOptions();              break;
        case SM_PROFILE_CONTROLS:                  EnterProfileControls();             break;
        case SM_PROFILE_AVATAR:                    EnterProfileAvatar();               break;
        case SM_PROFILE_MOUSE_CONTROLS:            EnterProfileMouseControls();        break;
        case SM_PROFILE_KEYBOARD_BINDINGS:         EnterProfileKeyboardBindings();     break;
        case SM_PROFILE_GAMEPAD_CONTROLS:          EnterProfileGamepadControls();      break;
        case SM_PROFILE_AVATAR_MP:                 EnterProfileAvatarMP();             break;
        case SM_PROFILE_SAVE_SELECT_MP:            EnterProfileSaveSelectMP();         break;
        case SM_SAVE_DATA_RESELECT_MP:             EnterSaveDataReselectMP();          break;
        case SM_MULTI_PLAYER_MENU:                 EnterMultiPlayer();                 break;
        case SM_MULTI_PLAYER_OPTIONS:              EnterMultiPlayerOptions();          break;
        case SM_MULTI_PLAYER_EDIT:                 EnterMultiPlayerEdit();             break;
        case SM_MP_LEVEL_SELECT:                   EnterMPLevelSelect();               break;
        case SM_MP_SAVE_SETTINGS:                  EnterMPSaveSettings();              break;
        case SM_PROFILE_CONTROLS_MP:               EnterProfileControlsMP();           break;
        case SM_ONLINE_CONNECT:                    EnterOnlineConnect();               break;
        case SM_NETWORK_DISCONNECT:                EnterNetworkDisconnect();           break;
        case SM_ONLINE_IDENTITY:               EnterOnlineIdentity();          break;
        case SM_ONLINE_PROFILE_SELECT:             EnterOnlineProfileSelect();         break;
        case SM_ONLINE_PROFILE_OPTIONS:            EnterOnlineProfileOptions();        break;
        case SM_ONLINE_PROFILE_CONTROLS:           EnterOnlineProfileControls();       break;
        case SM_ONLINE_PROFILE_AVATAR:             EnterOnlineProfileAvatar();         break;
        case SM_ONLINE_PROFILE_SAVE_SELECT:        EnterOnlineProfileSaveSelect();     break;
        case SM_ONLINE_SAVE_DATA_RESELECT:         EnterOnlineSaveDataReselect();      break;
        case SM_ONLINE_MAIN_MENU:                  EnterOnlineMain();                  break;
        case SM_ONLINE_HOST_MENU:                  EnterOnlineHost();                  break;
        case SM_HOST_SAVE_SETTINGS:                EnterHostSaveSettings();            break;
        case SM_ONLINE_JOIN_MENU:                  EnterOnlineJoin();                  break;
        case SM_ONLINE_JOIN_SAVE_SETTINGS:         EnterOnlineJoinSaveSettings();      break;
        case SM_ONLINE_STATS:                      EnterOnlineStats();                 break;
        case SM_ONLINE_EDIT_PROFILE:               EnterOnlineEditProfile();           break;
        case SM_ONLINE_EDIT_CONTROLS:              EnterOnlineEditControls();          break;
        case SM_ONLINE_EDIT_AVATAR:                EnterOnlineEditAvatar();            break;
#ifndef CONFIG_RETAIL                              
        case SM_LEVEL_SELECT:                      EnterLevelSelect();                 break;
#endif                                             
        case SM_RESUME_CAMPAIGN:                   EnterResumeCampaign();              break;
        case SM_START_SAVE_GAME:                   EnterStartSaveGame();               break;
        case SM_START_GAME:                        EnterStartGame();                   break;
        case SM_SERVER_SYNC:                       EnterServerSync();                  break;
        case SM_CLIENT_SYNC:                       EnterClientSync();                  break;
        case SM_MULTI_PLAYER_LOAD_MISSION:         EnterMultiPlayerLoadMission();      break;
        case SM_SINGLE_PLAYER_LOAD_MISSION:        EnterSinglePlayerLoadMission();     break;
        case SM_SERVER_COOLDOWN:                   EnterServerCooldown();              break;
        case SM_CLIENT_COOLDOWN:                   EnterClientCooldown();              break;
        case SM_SERVER_DISCONNECT:                 EnterServerDisconnect();            break;
        case SM_CLIENT_DISCONNECT:                 EnterClientDisconnect();            break;
        case SM_PAUSE_MAIN_MENU:                   EnterPauseMain();                   break;
        case SM_PAUSE_OPTIONS:                     EnterPauseOptions();                break;
        case SM_PAUSE_CONTROLS:                    EnterPauseControls();               break;
        case SM_PAUSE_SETTINGS:                    EnterPauseSettings();               break;
        case SM_PAUSE_AUDIO:                       EnterPauseAudio();                  break;
        case SM_PAUSE_HEADSET:                     EnterPauseHeadset();                break;
        case SM_PAUSE_GRAPHICS:                    EnterPauseGraphics();               break;
        case SM_PAUSE_GRAPHICS_ADVANCED:           EnterPauseGraphicsAdvanced();      break;
        case SM_PAUSE_DISPLAY:                     EnterPauseDisplay();                break;
        case SM_PAUSE_UI:                          EnterPauseUI();                    break;
        case SM_PAUSE_LANGUAGE:                    EnterPauseLanguage();               break;
        case SM_PAUSE_SETTINGS_SELECT:             EnterPauseSettingsSelect();         break;
        case SM_PAUSE_PROFILE_SAVE_SELECT:         EnterPauseSaveDataSaveSelect();     break;
        case SM_PAUSE_SAVE_DATA_RESELECT:          EnterPauseSaveDataReselect();       break;
        case SM_PAUSE_MP:                          EnterPauseMP();                     break;
        case SM_PAUSE_MP_OPTIONS:                  EnterPauseMPOptions();              break;
        case SM_PAUSE_MP_CONTROLS:                 EnterPauseMPControls();             break;
        case SM_PAUSE_MP_SETTINGS:                 EnterPauseMPSettings();             break;
        case SM_PAUSE_MP_AUDIO:                    EnterPauseMPAudio();                break;
        case SM_PAUSE_MP_HEADSET:                  EnterPauseMPHeadset();              break;
        case SM_PAUSE_MP_GRAPHICS:                 EnterPauseMPGraphics();             break;
        case SM_PAUSE_MP_GRAPHICS_ADVANCED:        EnterPauseMPGraphicsAdvanced();    break;
        case SM_PAUSE_MP_DISPLAY:                  EnterPauseMPDisplay();              break;
        case SM_PAUSE_MP_UI:                       EnterPauseMPUI();                  break;
        case SM_PAUSE_MP_LANGUAGE:                 EnterPauseMPLanguage();             break;
        case SM_PAUSE_MP_SETTINGS_SELECT:          EnterPauseMPSettingsSelect();       break;
        case SM_PAUSE_MP_PROFILE_SAVE_SELECT:      EnterPauseMPSaveDataSaveSelect();   break;
        case SM_PAUSE_MP_SAVE_DATA_RESELECT:       EnterPauseMPSaveDataReselect();     break;
        case SM_PAUSE_ONLINE:                      EnterPauseOnline();                 break;
        case SM_PAUSE_ONLINE_VOTE_MAP:             EnterPauseOnlineVoteMap();          break;
        case SM_PAUSE_ONLINE_VOTE_KICK:            EnterPauseOnlineVoteKick();         break;
        case SM_PAUSE_ONLINE_PLAYERS:              EnterPauseOnlinePlayers();          break;
        case SM_PAUSE_ONLINE_SERVER_CONFIG:        EnterPauseServerConfig();           break;
        case SM_PAUSE_ONLINE_OPTIONS:              EnterPauseOnlineOptions();          break;
        case SM_PAUSE_ONLINE_CONTROLS:             EnterPauseOnlineControls();         break;
        case SM_PAUSE_ONLINE_SETTINGS:             EnterPauseOnlineSettings();         break;
        case SM_PAUSE_ONLINE_AUDIO:                EnterPauseOnlineAudio();            break;
        case SM_PAUSE_ONLINE_HEADSET:              EnterPauseOnlineHeadset();          break;
        case SM_PAUSE_ONLINE_GRAPHICS:             EnterPauseOnlineGraphics();         break;
        case SM_PAUSE_ONLINE_GRAPHICS_ADVANCED:    EnterPauseOnlineGraphicsAdvanced(); break;
        case SM_PAUSE_ONLINE_DISPLAY:              EnterPauseOnlineDisplay();          break;
        case SM_PAUSE_ONLINE_UI:                   EnterPauseOnlineUI();              break;
        case SM_PAUSE_ONLINE_LANGUAGE:             EnterPauseOnlineLanguage();         break;
        case SM_PAUSE_ONLINE_SAVE_SELECT:          EnterPauseOnlineSaveSelect();       break;
        case SM_PAUSE_ONLINE_SAVE_DATA_RESELECT:   EnterPauseOnlineSaveDataReselect(); break;
        case SM_PAUSE_ONLINE_CHANGE_MAP:           EnterPauseOnlineChangeMap();        break;
        case SM_PAUSE_ONLINE_KICK_PLAYER:          EnterPauseOnlineKickPlayer();       break;
        case SM_PAUSE_ONLINE_TEAM_CHANGE:          EnterPauseOnlineTeamChange();       break;
        case SM_PAUSE_ONLINE_RECONFIG:            EnterPauseOnlineReconfig();         break;
        case SM_PAUSE_ONLINE_SAVE_SETTINGS:        EnterPauseOnlineSaveSettings();     break;
        case SM_PAUSE_MP_SCORE:                    EnterPauseMPScore();                break;
        case SM_END_PAUSE:                         EnterEndPause();                    break;
        case SM_AUTOSAVE_MENU:                     EnterAutosaveMenu();                break;
        case SM_AUTOSAVE_PROFILE_RESELECT:         EnterAutosaveProfileReselect();     break;
        case SM_END_AUTOSAVE:                      EnterEndAutosave();                 break;
        case SM_POST_GAME:                         EnterPostGame();                    break;
        case SM_EXIT_GAME:                         EnterExitGame();                    break;
        case SM_ADVANCE_LEVEL:                     EnterAdvanceLevel();                break;
        case SM_RELOAD_CHECKPOINT:                 EnterReloadCheckpoint();            break;
        case SM_FINAL_SCORE:                       EnterFinalScore();                  break;
#if CONFIG_IS_DEMO                                 
        case SM_DEMO_EXIT:                         EnterDemoExit();                    break;
#endif                                             
        case SM_REPORT_ERROR:                      EnterReportError();                 break;
        case SM_GAME_EXIT_PROMPT_FOR_SAVE:         EnterGameExitPromptForSave();       break;
        case SM_GAME_EXIT_SAVE_SELECT:             EnterGameExitSaveSelect();          break;
        case SM_GAME_EXIT_SAVE_DATA_RESELECT:      EnterGameExitSaveDataReselect();    break;
        case SM_GAME_EXIT_SAVE_SETTINGS:           EnterGameExitSaveSettings();        break;
        case SM_GAME_EXIT_SETTINGS_OVERWRITE:      EnterGameExitSettingsOverwrite();   break;
        case SM_GAME_EXIT_SETTINGS_SELECT:         EnterGameExitSettingsSelect();      break;
        case SM_GAME_EXIT_REDIRECT:                EnterGameExitRedirect();            break;
        case SM_GAME_OVER:                         EnterGameOver();                    break;
                                                   
        case SM_ONLINE_LOGIN:                      EnterOnlineLogin();                 break;
        case SM_PLAYING_GAME:                      EnterPlayingGame();                 break;

        // No setup required
        default:
        {
        }        
        break;
    }
}

//=========================================================================

void state_mgr::ExitState( sm_states State )
{
    switch( State )
    {
        case SM_STARTUP_INFO:                   ExitStartupInfo();              break;
        case SM_END_PAUSE:                      ExitEndPause();                 break;
        case SM_END_AUTOSAVE:                   ExitEndAutosave();              break;
        case SM_PAUSE_MP:                       ExitPauseMP();                  break;
        case SM_PAUSE_ONLINE:                   ExitPauseOnline();              break;
        case SM_SAVE_CAMPAIGN:                  ExitSaveCampaign();             break;
        case SM_EXIT_GAME:                      ExitExitGame();                 break;
        case SM_MULTI_PLAYER_LOAD_MISSION:      ExitMultiPlayerLoadMission();   break;
        case SM_SINGLE_PLAYER_LOAD_MISSION:     ExitSinglePlayerLoadMission();  break;
        case SM_SERVER_COOLDOWN:                ExitServerCooldown();           break;
        case SM_CLIENT_COOLDOWN:                ExitClientCooldown();           break;
        case SM_SERVER_DISCONNECT:              ExitServerDisconnect();         break;
        case SM_CLIENT_DISCONNECT:              ExitClientDisconnect();         break;
        case SM_SERVER_SYNC:                    ExitServerSync();               break;
        case SM_CLIENT_SYNC:                    ExitClientSync();               break;
        case SM_PRESS_START_SCREEN:             ExitPressStart();               break;
        case SM_ONLINE_LOGIN:                   ExitOnlineLogin();              break;
        case SM_NETWORK_DISCONNECT:             ExitNetworkDisconnect();        break;
        case SM_PLAYING_GAME:                   ExitPlayingGame();              break;

        // No clean up required
        default:
        {
        }        
        break;
    }
}

//=========================================================================

void state_mgr::UpdateState( sm_states State, f32 DeltaTime )
{
    (void)DeltaTime;

    // Update
    switch( State )
    {
        case SM_STARTUP_INFO:                       UpdateStartupInfo( DeltaTime );     break;
        case SM_STARTUP_INTRO:                      UpdateStartupIntro();               break;
        case SM_SAVE_DATA_BOOT_CHECK:               UpdateSaveDataBootCheck();          break;
        case SM_PRESS_START_SCREEN:                 UpdatePressStart();                 break;
        case SM_MAIN_MENU:                          UpdateMainMenu();                   break;
        case SM_SETTINGS_MENU:                      UpdateSettingsMenu();               break;
        case SM_SETTINGS_AUDIO:                     UpdateSettingsAudio();              break;
        case SM_SETTINGS_HEADSET:                   UpdateSettingsHeadset();            break;
        case SM_SETTINGS_GRAPHICS:                  UpdateSettingsGraphics();           break;
        case SM_SETTINGS_GRAPHICS_ADVANCED:         UpdateSettingsGraphicsAdvanced();  break;
        case SM_SETTINGS_DISPLAY:                   UpdateSettingsDisplay();            break;
        case SM_SETTINGS_UI:                        UpdateSettingsUI();                break;
        case SM_SETTINGS_LANGUAGE:                  UpdateSettingsLanguage();           break;
        case SM_SETTINGS_SAVE_DATA_SELECT:          UpdateSettingsSaveDataSelect();     break;
                                                    
        case SM_MANAGE_PROFILES:                    UpdateManageProfiles();             break;
        case SM_MANAGE_PROFILE_OPTIONS:             UpdateManageProfileOptions();       break;
        case SM_MANAGE_PROFILE_CONTROLS:            UpdateManageProfileControls();      break;
        case SM_MANAGE_PROFILE_AVATAR:              UpdateManageProfileAvatar();        break;
        case SM_MANAGE_PROFILE_SAVE_SELECT:         UpdateManageProfileSaveSelect();    break;
        case SM_MANAGE_PROFILE_SAVE_DATA_RESELECT:  UpdateManageSaveDataReselect();     break;

        case SM_CAMPAIGN_MENU:                      UpdateCampaignMenu();               break;
        case SM_CAMPAIGN_PROFILE_OPTIONS:           UpdateCampaignProfileOptions();     break;
        case SM_CAMPAIGN_PROFILE_CONTROLS:          UpdateCampaignProfileControls();    break;
        case SM_CAMPAIGN_PROFILE_AVATAR:            UpdateCampaignProfileAvatar();      break;
        case SM_CAMPAIGN_PROFILE_SAVE_SELECT:       UpdateCampaignProfileSaveSelect();  break;
        case SM_CAMPAIGN_SAVE_DATA_RESELECT:        UpdateCampaignSaveDataReselect();   break;
        case SM_LOAD_CAMPAIGN:                      UpdateLoadCampaign();               break;
        case SM_SAVE_CAMPAIGN:                      UpdateSaveCampaign();               break;
        case SM_LORE_MAIN_MENU:                     UpdateLoreMainMenu();               break;
        case SM_SECRETS_MENU:                       UpdateSecretsMenu();                break;
        case SM_EXTRAS_MENU:                        UpdateExtrasMenu();                 break;
        case SM_CREDITS_SCREEN:                     UpdateCreditsScreen();              break;
        case SM_PROFILE_SELECT:                     UpdateProfileSelect();              break;
        case SM_PROFILE_OPTIONS:                    UpdateProfileOptions();             break;
        case SM_PROFILE_CONTROLS:                   UpdateProfileControls();            break;
        case SM_PROFILE_AVATAR:                     UpdateProfileAvatar();              break;
        case SM_PROFILE_MOUSE_CONTROLS:             UpdateProfileDeviceControls();      break;
        case SM_PROFILE_KEYBOARD_BINDINGS:          UpdateProfileDeviceControls();      break;
        case SM_PROFILE_GAMEPAD_CONTROLS:           UpdateProfileDeviceControls();      break;
        case SM_MULTI_PLAYER_MENU:                  UpdateMultiPlayer();                break;
        case SM_MULTI_PLAYER_OPTIONS:               UpdateMultiPlayerOptions();         break;
        case SM_MULTI_PLAYER_EDIT:                  UpdateMultiPlayerEdit();            break;
        case SM_MP_LEVEL_SELECT:                    UpdateMPLevelSelect();              break;
        case SM_MP_SAVE_SETTINGS:                   UpdateMPSaveSettings();             break;
        case SM_PROFILE_CONTROLS_MP:                UpdateProfileControlsMP();          break;
        case SM_PROFILE_AVATAR_MP:                  UpdateProfileAvatarMP();            break;
        case SM_PROFILE_SAVE_SELECT_MP:             UpdateProfileSaveSelectMP();        break;
        case SM_SAVE_DATA_RESELECT_MP:              UpdateSaveDataReselectMP();         break;
        case SM_ONLINE_CONNECT:                     UpdateOnlineConnect();              break;
        case SM_NETWORK_DISCONNECT:                 UpdateNetworkDisconnect();          break;
        case SM_ONLINE_IDENTITY:                UpdateOnlineIdentity();         break;
        case SM_ONLINE_PROFILE_SELECT:              UpdateOnlineProfileSelect();        break;
        case SM_ONLINE_PROFILE_OPTIONS:             UpdateOnlineProfileOptions();       break;
        case SM_ONLINE_PROFILE_CONTROLS:            UpdateOnlineProfileControls();      break;
        case SM_ONLINE_PROFILE_AVATAR:              UpdateOnlineProfileAvatar();        break;
        case SM_ONLINE_PROFILE_SAVE_SELECT:         UpdateOnlineProfileSaveSelect();    break;
        case SM_ONLINE_SAVE_DATA_RESELECT:          UpdateOnlineSaveDataReselect();     break;
        case SM_ONLINE_MAIN_MENU:                   UpdateOnlineMain();                 break;
        case SM_ONLINE_HOST_MENU:                   UpdateOnlineHost();                 break;
        case SM_HOST_SAVE_SETTINGS:                 UpdateHostSaveSettings();           break;
        case SM_ONLINE_JOIN_MENU:                   UpdateOnlineJoin();                 break;
        case SM_ONLINE_JOIN_SAVE_SETTINGS:          UpdateOnlineJoinSaveSettings();     break;
        case SM_ONLINE_STATS:                       UpdateOnlineStats();                break;
        case SM_ONLINE_EDIT_PROFILE:                UpdateOnlineEditProfile();          break;
        case SM_ONLINE_EDIT_CONTROLS:               UpdateOnlineEditControls();         break;
        case SM_ONLINE_EDIT_AVATAR:                 UpdateOnlineEditAvatar();           break;
#ifndef CONFIG_RETAIL                               
        case SM_LEVEL_SELECT:                       UpdateLevelSelect();                break;
#endif                                              
        case SM_RESUME_CAMPAIGN:                    UpdateResumeCampaign();             break;
        case SM_START_SAVE_GAME:                    UpdateStartSaveGame();              break;
        //
        //  Loading and Unloading
        //
        case SM_START_GAME:                     UpdateStartGame();                  break;     
        case SM_SERVER_SYNC:                    UpdateServerSync();                 break;
        case SM_CLIENT_SYNC:                    UpdateClientSync();                 break;
        case SM_MULTI_PLAYER_LOAD_MISSION:      UpdateMultiPlayerLoadMission();     break;
        case SM_SINGLE_PLAYER_LOAD_MISSION:     UpdateSinglePlayerLoadMission();    break;
        case SM_PLAYING_GAME:                   UpdatePlayingGame();                break;
        case SM_SERVER_COOLDOWN:                UpdateServerCooldown();             break;
        case SM_CLIENT_COOLDOWN:                UpdateClientCooldown();             break;
        case SM_SERVER_DISCONNECT:              UpdateServerDisconnect();           break;
        case SM_CLIENT_DISCONNECT:              UpdateClientDisconnect();           break;

        //
        // In game pause menu
        //
        case SM_PAUSE_MAIN_MENU:                  UpdatePauseMain();                    break;
        case SM_PAUSE_OPTIONS:                    UpdatePauseOptions();                 break;
        case SM_PAUSE_CONTROLS:                   UpdatePauseControls();                break;
        case SM_PAUSE_SETTINGS:                   UpdatePauseSettings();                break;
        case SM_PAUSE_AUDIO:                      UpdatePauseAudio();                   break;
        case SM_PAUSE_HEADSET:                    UpdatePauseHeadset();                 break;
        case SM_PAUSE_GRAPHICS:                   UpdatePauseGraphics();                break;
        case SM_PAUSE_GRAPHICS_ADVANCED:          UpdatePauseGraphicsAdvanced();       break;
        case SM_PAUSE_DISPLAY:                    UpdatePauseDisplay();                 break;
        case SM_PAUSE_UI:                         UpdatePauseUI();                     break;
        case SM_PAUSE_LANGUAGE:                   UpdatePauseLanguage();                break;
        case SM_PAUSE_SETTINGS_SELECT:            UpdatePauseSettingsSelect();          break;
        case SM_PAUSE_PROFILE_SAVE_SELECT:        UpdatePauseSaveDataSaveSelect();      break;
        case SM_PAUSE_SAVE_DATA_RESELECT:         UpdatePauseSaveDataReselect();        break;
        case SM_PAUSE_MP:                         UpdatePauseMP();                      break;
        case SM_PAUSE_MP_OPTIONS:                 UpdatePauseMPOptions();               break;
        case SM_PAUSE_MP_CONTROLS:                UpdatePauseMPControls();              break;
        case SM_PAUSE_MP_SETTINGS:                UpdatePauseMPSettings();              break;
        case SM_PAUSE_MP_AUDIO:                   UpdatePauseMPAudio();                 break;
        case SM_PAUSE_MP_HEADSET:                 UpdatePauseMPHeadset();               break;
        case SM_PAUSE_MP_GRAPHICS:                UpdatePauseMPGraphics();              break;
        case SM_PAUSE_MP_GRAPHICS_ADVANCED:       UpdatePauseMPGraphicsAdvanced();     break;
        case SM_PAUSE_MP_DISPLAY:                 UpdatePauseMPDisplay();               break;
        case SM_PAUSE_MP_UI:                      UpdatePauseMPUI();                   break;
        case SM_PAUSE_MP_LANGUAGE:                UpdatePauseMPLanguage();              break;
        case SM_PAUSE_MP_SETTINGS_SELECT:         UpdatePauseMPSettingsSelect();        break;
        case SM_PAUSE_MP_PROFILE_SAVE_SELECT:     UpdatePauseMPSaveDataSaveSelect();    break;
        case SM_PAUSE_MP_SAVE_DATA_RESELECT:      UpdatePauseMPSaveDataReselect();      break;
                                                                                        
        case SM_PAUSE_ONLINE:                     UpdatePauseOnline();                  break;
        case SM_PAUSE_ONLINE_VOTE_MAP:            UpdatePauseOnlineVoteMap();           break;
        case SM_PAUSE_ONLINE_VOTE_KICK:           UpdatePauseOnlineVoteKick();          break;
        case SM_PAUSE_ONLINE_PLAYERS:             UpdatePauseOnlinePlayers();           break;
        case SM_PAUSE_ONLINE_SERVER_CONFIG:       UpdatePauseServerConfig();            break;
        case SM_PAUSE_ONLINE_OPTIONS:             UpdatePauseOnlineOptions();           break;
        case SM_PAUSE_ONLINE_CONTROLS:            UpdatePauseOnlineControls();          break;
        case SM_PAUSE_ONLINE_SETTINGS:            UpdatePauseOnlineSettings();          break;
        case SM_PAUSE_ONLINE_AUDIO:               UpdatePauseOnlineAudio();             break;
        case SM_PAUSE_ONLINE_HEADSET:             UpdatePauseOnlineHeadset();           break;
        case SM_PAUSE_ONLINE_GRAPHICS:            UpdatePauseOnlineGraphics();          break;
        case SM_PAUSE_ONLINE_GRAPHICS_ADVANCED:   UpdatePauseOnlineGraphicsAdvanced(); break;
        case SM_PAUSE_ONLINE_DISPLAY:             UpdatePauseOnlineDisplay();           break;
        case SM_PAUSE_ONLINE_UI:                  UpdatePauseOnlineUI();               break;
        case SM_PAUSE_ONLINE_LANGUAGE:            UpdatePauseOnlineLanguage();          break;
        case SM_PAUSE_ONLINE_SAVE_SELECT:         UpdatePauseOnlineSaveSelect();        break;
        case SM_PAUSE_ONLINE_SAVE_DATA_RESELECT:  UpdatePauseOnlineSaveDataReselect();  break;
        case SM_PAUSE_ONLINE_CHANGE_MAP:          UpdatePauseOnlineChangeMap();         break;
        case SM_PAUSE_ONLINE_KICK_PLAYER:         UpdatePauseOnlineKickPlayer();        break;
        case SM_PAUSE_ONLINE_TEAM_CHANGE:         UpdatePauseOnlineTeamChange();        break;
        case SM_PAUSE_ONLINE_RECONFIG:           UpdatePauseOnlineReconfig();          break;
        case SM_PAUSE_ONLINE_SAVE_SETTINGS:       UpdatePauseOnlineSaveSettings();      break;
        case SM_PAUSE_MP_SCORE:                   UpdatePauseMPScore();                 break;
        case SM_END_PAUSE:                        UpdateEndPause();                     break;
        case SM_AUTOSAVE_MENU:                    UpdateAutosaveMenu();                 break;
        case SM_AUTOSAVE_PROFILE_RESELECT:        UpdateAutosaveProfileReselect();      break;
        case SM_END_AUTOSAVE:                     UpdateEndAutosave();                  break;
        case SM_EXIT_GAME:                        UpdateExitGame();                     break;
        case SM_POST_GAME:                        UpdatePostGame();                     break;
        case SM_ADVANCE_LEVEL:                    UpdateAdvanceLevel();                 break;
        case SM_RELOAD_CHECKPOINT:                UpdateReloadCheckpoint();             break;
        case SM_FINAL_SCORE:                      UpdateFinalScore();                   break;
        case SM_GAME_OVER:                        UpdateGameOver();                     break;
#if CONFIG_IS_DEMO                                                                      
        case SM_DEMO_EXIT:                        UpdateDemoExit();                     break;
#endif                                                                                  
        case SM_REPORT_ERROR:                     UpdateReportError();                  break;
        case SM_GAME_EXIT_PROMPT_FOR_SAVE:        UpdateGameExitPromptForSave();        break;
        case SM_GAME_EXIT_SAVE_SELECT:            UpdateGameExitSaveSelect();           break;
        case SM_GAME_EXIT_SAVE_DATA_RESELECT:     UpdateGameExitSaveDataReselect();     break;
        case SM_GAME_EXIT_SAVE_SETTINGS:          UpdateGameExitSaveSettings();         break;
        case SM_GAME_EXIT_SETTINGS_OVERWRITE:     UpdateGameExitSettingsOverwrite();    break;
        case SM_GAME_EXIT_SETTINGS_SELECT:        UpdateGameExitSettingsSelect();       break;
        case SM_GAME_EXIT_REDIRECT:               UpdateGameExitRedirect();             break;
        case SM_ONLINE_LOGIN:                     UpdateOnlineLogin();                  break;

        // no update required
        default:
        {
        }
        break;
    }
#if CONFIG_IS_DEMO
    // **** Coverdisk timeout ****
    // If no input is received for 1 minute, then we will
    // bail out. **NOTE** this timer will only be started
    // when the load level is complete.

    s32         IdleTime;
    extern s32  g_DemoInactiveTimeout;

    IdleTime = (s32)g_DemoIdleTimer.ReadSec();
    if( IdleTime > g_DemoInactiveTimeout )
    {
        if( GameMgr.GameInProgress() )
        {
            SetState( SM_EXIT_GAME );
        }
        else
        {
            SetState( SM_DEMO_EXIT );
        }
    }
#endif
}

//=========================================================================

void state_mgr::SetPaused( xbool bPause, s32 PausingController )
{
    // check if we are autosaving
    if( m_bAutosaveInProgress )
    {
        // no can do!
        return;
    }

    if( g_SaveDataMgr.IsBusy() )
    {
        return;
    }

    player* pPlayer = SMP_UTIL_GetActivePlayer();
    if (pPlayer)
    {
        if (pPlayer->IsCinemaRunning())
        {
            // We no longer allow pausing during in game
            // cinematics
            return;
        }
#ifndef X_EDITOR
        if (!GameMgr.IsGameMultiplayer())
        {
            if (pPlayer->IsDead())
            {
                // We no longer allow pausing while you are dead.
                return;
            }
        }
#endif
    }

    if( !GameMgr.IsGameOnline() )
    {
        g_Input.SuppressFeedback(bPause);
    }

    // check we are not exiting to the main menu
    if( m_Exit )
    {
        return;
    }

    // check we are not in the save menu
    if( GameMgr.GetGameType() == GAME_CAMPAIGN )
    {
        if( m_State == SM_SAVE_CAMPAIGN )
        {
            return;
        }
    }

    // ok, see what we want to do
    if( bPause )
    {
        ASSERT( PausingController != -1 );
        CloseScoreboardOverlay();

        // set the initial state
        if( GameMgr.GetGameType() == GAME_CAMPAIGN )
        {
            if( m_State != SM_END_PAUSE )
            {
                // set up the screen bars off screen
                irect initSize( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );
                initSize.Translate( 0, SAFE_ZONE );

                g_UiMgr->SetScreenSize(initSize);       
            }
            // force main pause menu to the active controller
            // (returns to all controllers in the EndPause state transition)
            SetState( SM_PAUSE_MAIN_MENU );
        }
        else
        {
            if( g_NetworkMgr.IsOnline() )
            {
                SetState( SM_PAUSE_ONLINE );
            }
            else
            {
                SetState( SM_PAUSE_MP_SCORE );
            }

            // Pause Audio
        //  g_AudioMgr.PauseAll();
        }
        g_UiMgr->EnableUser( g_UiUserID, TRUE );

        // set pause state
        m_bIsPaused = bPause;
        g_UiMgr->SetUserController( g_UiUserID, PausingController );
        SetActiveControllerID( PausingController );
    }
    else
    {
        //
        // Someone MUST be in control of the pause menu. Only they can deactivate it.
        //
        if( (PausingController!=-1) && (PausingController != GetActiveControllerID()) )
        {
            return;
        }

        SetActiveControllerID( PausingController );
        g_UiMgr->SetUserController( g_UiUserID, -1 );
        //#ifdef mbillington
        //#error This is a HACK and needs to be fixed.
        //#endif
        if( (m_State == SM_PLAYING_GAME) && (m_bIsPaused) )
        {
            m_bIsPaused = FALSE;
            g_AudioMgr.ResumeAll(); 
            return;
        }

        if( m_State != SM_PAUSE_MP_SCORE )
        {
            if( m_State != SM_END_PAUSE )
            {
                // clean up the screen gracefully
                SetState( SM_END_PAUSE );
            }
        }
        else
        {
            // Kill the leaderboard and return to the game immediately
            g_UiMgr->EndDialog( g_UiUserID, TRUE );
            m_CurrentDialog = NULL;
            // Disable ui
            g_UiMgr->EnableUser( g_UiUserID, FALSE );

            // Set unpaused
            m_bIsPaused = FALSE;

            // back to playing the game
            SetState( SM_PLAYING_GAME );
        }

        // Resume Audio
        g_AudioMgr.ResumeAll();
    }
}

//=========================================================================

void state_mgr::SetProfileNotSaved( s32 playerID, xbool bNotSaved )
{
    ASSERT( playerID >= 0 );
    ASSERT( playerID < SM_PROFILE_COUNT );
    // flag the profile in memory as not saved
    m_ProfileNotSaved[playerID] = bNotSaved; 
}

//=========================================================================

void state_mgr::ResetProfile( s32 index )
{
    ASSERT( index >= 0 );
    ASSERT( index < SM_PROFILE_COUNT );
    player_profile Pp;
    x_memcpy( m_Profiles+index, &Pp, sizeof( Pp ) );
}

//=========================================================================

void state_mgr::InitPendingProfile( s32 index )
{ 
    // make a copy of the actual profile data selected
    ASSERT(index >= 0 );
    ASSERT(index < SM_PROFILE_COUNT); 
    m_PendingProfile = m_Profiles[index]; 
    m_PendingProfileIndex = index;
}

//=========================================================================

void state_mgr::ActivatePendingProfile( xbool MarkDirty )
{
    // copy the pending profile into the actual profile data
    ASSERT( m_PendingProfileIndex >= 0 );
    ASSERT( m_PendingProfileIndex < SM_PROFILE_COUNT );
    m_Profiles[m_PendingProfileIndex] = m_PendingProfile; 
    m_Profiles[m_PendingProfileIndex].Checksum();
    for( s32 LocalIndex = 0; LocalIndex < MAX_LOCAL_PLAYERS; LocalIndex++ )
    {
        if( m_ProfileListIndex[LocalIndex] == m_PendingProfileIndex )
            g_GameInput.ApplyKeyboardBindings( LocalIndex, m_PendingProfile.GetKeyboardBindings() );
    }

    if( MarkDirty )
    {
        m_Profiles[m_PendingProfileIndex].MarkDirty();
    }

    // set nickname
    g_MatchMgr.SetNickname( m_PendingProfile.GetProfileName() );

    // set difficulty
    g_Difficulty = m_PendingProfile.GetDifficultyLevel();

    // set autosave flag
    m_bAutosaveProfile = m_PendingProfile.GetAutosaveOn();

    // finally clear the pending profile index
    m_PendingProfileIndex = -1;
}

//=========================================================================

void state_mgr::ResetProfileListIndex( void )
{
    // reset profile index list
    s32 i;
    for( i=0; i<SM_PROFILE_COUNT; i++ )
    {
        m_ProfileListIndex[i] = -1;
    }
}

//=========================================================================
// whatever this list's original purpose was, it was never implemented.
// The list is used to gather selected profile indexes into an order that
// will work with the game server/client code.
// it's primary use is for split screen mode where the profiles occupy the
// same slots as the initiating controller.
// This needs to be called after all players have selected profiles. 

void state_mgr::SetupProfileListIndex( void )
{
    ResetProfileListIndex();

    s32 i;
    s32 p = 0;
    s32 count = 0;

    for( i = 0; i< SM_PROFILE_COUNT; i++ )
    {
        if( m_SelectedProfile[i] != 0 )
        {
            count++;
        }
    }

    ASSERT(count > 0);

    // setup profile index list
    for( i = 0; (i < SM_PROFILE_COUNT) && (i < count); i++ )
    {
        // find a requested controller
        for( ; p < SM_PROFILE_COUNT; p++)
        {
            if( m_SelectedProfile[p] != 0 )
            {
                m_ProfileListIndex[i] = p++;    // increment p here too.
                break;
            }
        }
    }
}

//=========================================================================

void state_mgr::InitPendingSettings( void )
{
     m_PendingSettings = m_ActiveSettings;
}

//=========================================================================

void state_mgr::ActivatePendingSettings( xbool MarkDirty )
{
    m_ActiveSettings = m_PendingSettings;
    m_ActiveSettings.Commit();

    if( MarkDirty )
    {
        m_ActiveSettings.MarkDirty();
    }
}

//=========================================================================

// profile game exit save callback
void state_mgr::OnGameExitSaveProfileCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        global_settings& ActiveSettings = GetActiveSettings();

        // update the changes in the profile
        g_StateMgr.ActivatePendingProfile();

        // save successful - finish end game processing
        g_AudioMgr.Play( "Select_Norm" );        

        // check settings
        if( ActiveSettings.HasChanged() )
        {
            SetState( SM_GAME_EXIT_SAVE_SETTINGS );
        }
        else
        {
            SetState( SM_GAME_EXIT_REDIRECT );
        }
    }
    else
    {
        // save failed - goto save select
        g_AudioMgr.Play( "Select_Norm" );
        SetState( SM_GAME_EXIT_SAVE_SELECT );
    }

    // get the profile list
    xarray<profile_info*>& ProfileNames = g_StateMgr.GetProfileList();
    // get the current list from the save data manager
    g_SaveDataMgr.GetProfileNames( ProfileNames );
}

//=========================================================================

// settings game exit save callback
void state_mgr::OnGameExitSaveSettingsCB( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        // activate the new settings
        g_StateMgr.ActivatePendingSettings();

        // save successful - return to main menu
        g_AudioMgr.Play( "Select_Norm" );

        SetState( SM_GAME_EXIT_REDIRECT );
    }
    else
    {
        // save failed! - goto save data select dialog
        g_AudioMgr.Play( "Backup" );
        SetState( SM_GAME_EXIT_SETTINGS_SELECT );
    }
}

//=========================================================================

void state_mgr::SetLoreAcquired( s32 LoreID )
{
    s32 VaultIndex;
    s32 LoreIndex;

    // find out which vault the lore is in
    if( g_LoreList.GetVaultByLoreID( LoreID, VaultIndex, LoreIndex ) )
    {
        // get the active profile for player 0
        player_profile& Profile = GetActiveProfile( GetProfileListIndex(0) );
        // set the lore as acquired
        Profile.SetLoreAcquired( VaultIndex, LoreIndex );
    }
    else
    {
        ASSERTS( FALSE, "Error: could not look up vault from lore ID acquired" );
    }
}

//=========================================================================

s32 state_mgr::SetLoreAcquired( s32 MapID, s32 LoreIndex )
{
    s32 VaultIndex;    
    s32 LoreID;

    lore_vault* pVault = g_LoreList.GetVaultByMapID( MapID, VaultIndex );

    // find out which vault the lore is in
    if( pVault )
    {
        LoreID = g_LoreList.GetLoreIDByVault(pVault, LoreIndex);

        // valid lore id?
        if( LoreID != -1 )
        {
            // get the active profile for player 0
            player_profile& Profile = GetActiveProfile( GetProfileListIndex(0) );

            // set the lore as acquired
            Profile.SetLoreAcquired( VaultIndex, LoreIndex );

            // this is the real index, not just the 0-n id per map
            return LoreID;
        }
    }

    return -1;
 
    ASSERTS( FALSE, "Error: could not look up vault from Lore Index acquired" );
}

//=========================================================================
u32 state_mgr::GetLevelLoreAcquired( s32 MapID )
{
    s32 VaultIndex;    
    u32 LoreCount = 0;
    u32 i = 0;

    // in a campaign, the profile will be zero, so the indirection should not change anything.
    ASSERT(GetProfileListIndex(0) == 0);

    player_profile& Profile = GetActiveProfile( GetProfileListIndex(0) );

    // loop through all the items in this vault and see if they've been acquired
    for( i = 0; i < NUM_PER_VAULT; i++ )
    {
        g_LoreList.GetVaultByMapID( MapID, VaultIndex );

        if( Profile.GetLoreAcquired(VaultIndex, i) )
        {
            LoreCount++;
        }
    }

    return LoreCount;
}

//=========================================================================
u32 state_mgr::GetTotalLoreAcquired( void )
{
    player_profile& Profile = GetActiveProfile( GetProfileListIndex(0) );

    return Profile.GetTotalLoreAcquired();
}

//=========================================================================

void state_mgr::EnterStartupInfo( void )
{
    g_UiMgr->LoadBackground( "startup info", "SCREENS/UI_WARNINGSCREEN.XBMP" );
    g_UiMgr->SetUserBackground( g_UiUserID, "startup info" );

    m_StartupInfoTime = 10.0f;
    //m_StartupInfoVoiceID = g_AudioMgr.Play( "JanDemo" );
}

//=========================================================================

void state_mgr::UpdateStartupInfo( f32 DeltaTime )
{
    m_StartupInfoTime -= DeltaTime;

    if( m_StartupInfoTime <= 0.0f )
    {
        SetState( SM_STARTUP_INTRO );
    }
}

//=========================================================================

void state_mgr::ExitStartupInfo( void )
{
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
    g_UiMgr->UnloadBackground( "startup info" );

    //if( g_AudioMgr.IsValidVoiceId( m_StartupInfoVoiceID ) )
    //{
    //    g_AudioMgr.Release( m_StartupInfoVoiceID, 0.0f );
    //    m_StartupInfoVoiceID = -1;
    //}

}

//=========================================================================

void state_mgr::EnterStartupIntro( void )
{
    m_StartupIntroMovieIndex = 0;
}

//=========================================================================

xstring SelectBestClip( const char* pName )
{
    return (const char*)xfs( "%s_640x480_%d", pName, 30 );
}

//=========================================================================

void state_mgr::QueueSimpleMovie( const char* pFilename,
                                  const char* pFallbackMessage,
                                  xbool       WaitForCompletion,
                                  xbool       RestoreBackground )
{
    ASSERT( pFilename && pFilename[0] );
    ASSERT( !m_bSimpleMoviePending );
    ASSERT( !m_bSimpleMovieCompleted );

    m_PendingSimpleMovieName          = pFilename;
    m_PendingSimpleMovieFallback      = pFallbackMessage ? pFallbackMessage : "";
    m_bSimpleMoviePending             = TRUE;
    m_bSimpleMovieWaitForCompletion   = WaitForCompletion;
    m_bRestoreBackgroundAfterSimpleMovie = RestoreBackground;
}

//=========================================================================

xbool state_mgr::WaitForSimpleMovie( const char* pFilename,
                                     const char* pFallbackMessage )
{
    if( m_bSimpleMovieCompleted )
    {
        if( x_strcmp( m_PendingSimpleMovieName, pFilename ) != 0 )
        {
            x_DebugMsg( "StateMgr: completed simple movie '%s' while waiting for '%s'\n",
                        (const char*)m_PendingSimpleMovieName,
                        pFilename );
            ASSERT( FALSE );
            return FALSE;
        }

        m_bSimpleMovieCompleted = FALSE;
        m_PendingSimpleMovieName.Clear();
        m_PendingSimpleMovieFallback.Clear();
        return TRUE;
    }

    if( !m_bSimpleMoviePending )
    {
        QueueSimpleMovie( pFilename, pFallbackMessage, TRUE );
    }
    else
    {
        ASSERT( x_strcmp( m_PendingSimpleMovieName, pFilename ) == 0 );
    }

    return FALSE;
}

//=========================================================================

xbool state_mgr::HasPendingSimpleMovie( void ) const
{
    return m_bSimpleMoviePending;
}

//=========================================================================

void state_mgr::RequestSimpleMovie( const char* pFilename )
{
    QueueSimpleMovie( pFilename, NULL, FALSE, TRUE );
}

//=========================================================================

void state_mgr::ProcessPendingSimpleMovie( void )
{
    if( !m_bSimpleMoviePending )
    {
        return;
    }

    if( m_bRestoreBackgroundAfterSimpleMovie )
    {
        DisableBackgoundMovie();
    }

    const xbool bPlayed = PlaySimpleMovie( m_PendingSimpleMovieName );
    if( !bPlayed && (m_PendingSimpleMovieFallback.GetLength() > 0) )
    {
        DummyScreen( m_PendingSimpleMovieFallback, TRUE, 5 );
    }

    if( m_bRestoreBackgroundAfterSimpleMovie )
    {
        EnableBackgroundMovie();
        m_bRestoreBackgroundAfterSimpleMovie = FALSE;
    }

    g_UiMgr->SuppressInputUntilRelease();

    m_bSimpleMoviePending = FALSE;
    if( m_bSimpleMovieWaitForCompletion )
    {
        m_bSimpleMovieCompleted = TRUE;
    }
    else
    {
        m_PendingSimpleMovieName.Clear();
        m_PendingSimpleMovieFallback.Clear();
    }
}

//=========================================================================

void state_mgr::UpdateStartupIntro( void )
{
#ifdef USE_MOVIES
    while( TRUE )
    {
        const char* pMovieName      = NULL;
        const char* pFallbackMessage = NULL;

    #if CONFIG_IS_DEMO
        if( m_StartupIntroMovieIndex == 0 )
        {
            pMovieName       = "trailer";
            pFallbackMessage = "Trailer Movie Here";
        }
    #else
        switch( m_StartupIntroMovieIndex )
        {
            case 0:
                pMovieName       = "Inevitable";
                pFallbackMessage = "Inevitable Movie Here";
                break;

            case 1:
                pMovieName       = "Midway";
                pFallbackMessage = "Midway Movie Here";
                break;

            case 2:
                if( x_GetLocale() == XL_LANG_RUSSIAN )
                {
                    pMovieName       = "NewDisk";
                    pFallbackMessage = "NewDisk Movie Here";
                }
                else
                {
                    m_StartupIntroMovieIndex++;
                    continue;
                }
                break;

            default:
                break;
        }
    #endif

        if( !pMovieName )
        {
            break;
        }

        if( !WaitForSimpleMovie( SelectBestClip( pMovieName ), pFallbackMessage ) )
        {
            return;
        }

        m_StartupIntroMovieIndex++;
    }
#endif

#if CONFIG_IS_DEMO
    SetState( SM_PRESS_START_SCREEN );
#else
    // change the state
    SetState( SM_SAVE_DATA_BOOT_CHECK );
//  SetState( SM_PRESS_START_SCREEN );
#endif
}

//=========================================================================

void state_mgr::ExitStartupIntro( void )
{
}

//=========================================================================

void state_mgr::EnterSaveDataBootCheck( void )
{
    g_SaveDataMgr.RefreshProfiles();
}

//=========================================================================

void state_mgr::UpdateSaveDataBootCheck( void )
{

    if( !g_SaveDataMgr.IsBusy() )
    {
        // change the state
        SetState( SM_PRESS_START_SCREEN );
    }
}

//=========================================================================

void state_mgr::ExitSaveDataBootCheck( void )
{
}

//=========================================================================

void state_mgr::EnterPressStart( void )
{
    // Create the press start screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    g_UiMgr->EnableUser( g_UiUserID, TRUE );
    irect mainarea( 16, 16, ui_viewport::CONTENT_WIDTH - 16, ui_viewport::CONTENT_HEIGHT - 16 );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "press start", mainarea, NULL, ui_win::WF_VISIBLE);//|ui_win::WF_BORDER );
//    g_UiMgr->SetUserBackground( g_UiUserID, "titlescreen" );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
#if CONFIG_IS_DEMO
    extern s32 g_DemoInactiveTimeout;

    m_TimeoutTime = g_DemoInactiveTimeout;
#endif
}

//=========================================================================

void state_mgr::UpdatePressStart( void )
{
#if CONFIG_IS_DEMO
    // check for timeout
    if( m_Timeout < 0 )
    {      
        // This is really missnamed, this displays the post game splash
        SetState( SM_DEMO_EXIT );
    }
#endif

    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        if( ( DialogState == DIALOG_STATE_SELECT ) || ( CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT || CONFIG_IS_AUTOCAMPAIGN || CONFIG_IS_AUTOSPLITSCREEN) )
        {
            irect initSize( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );
            initSize.Translate( 0, SAFE_ZONE );

            g_UiMgr->SetScreenOn( FALSE );
            g_UiMgr->SetScreenSize( initSize );
            g_UiMgr->InitGlowBar();
            g_UiMgr->InitRefreshBar();

#if CONFIG_IS_DEMO
            // This is where we load the level in demo mode
            g_PendingConfig.SetLevelID          ( 1100 ); // Lies of the Past
            g_PendingConfig.SetPlayerCount      ( 1 );
            g_PendingConfig.SetMaxPlayerCount   ( 1 );
            g_PendingConfig.SetGameTypeID       ( GAME_CAMPAIGN );
            g_NetworkMgr.BecomeServer();

            extern s32 g_LastControllerID;
            ASSERT( g_LastControllerID != -1 );
            SetActiveControllerID( g_LastControllerID );
            g_UiMgr->SetUserController( g_UiUserID, g_LastControllerID );
            SetState( SM_LOAD_GAME );
#else
            SetState( SM_MAIN_MENU );
#endif
        }
    }
}

//=========================================================================

void state_mgr::ExitPressStart( void )
{
}

//=========================================================================

#if CONFIG_IS_DEMO
void state_mgr::EnterDemoExit( void )
{
    g_UiMgr->EnableUser( g_UiUserID, TRUE );
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    g_UiMgr->LoadBackground ( "background1", "PostGameSplash.xbmp" );
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
    m_TimeoutTime = DEMO_ENDGAME_TIMEOUT;
}

//=========================================================================

void state_mgr::UpdateDemoExit( void )
{
    if( m_Timeout <= 0.0f )
    {
        Reboot( REBOOT_QUIT );
    }

    if( m_Timeout < (DEMO_ENDGAME_TIMEOUT-4.0f) )
    {
        for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
        {
            const frontend_pad& Pad = g_FrontendInput.GetPad( i );
            if( (Pad.GetFrameLogical( frontend_pad::UI_HELP ).GetIsValue() > 0.25f) ||
                (Pad.GetFrameLogical( frontend_pad::UI_HELP ).GetWasValue() > 0.25f) )
            {
                Reboot( REBOOT_QUIT );
            }
        }
    }
}
//=========================================================================

void state_mgr::ExitDemoExit( void )
{
}
#endif

//=========================================================================

void state_mgr::EnterMainMenu( void )
{
    // Make sure all pads are inactive for the game. They will be re-activated when
    // the game is actually started.
    g_GameInput.ClearPlayerDevices();

#ifdef USE_MOVIES
    // initialize movie player
    if( !m_bPlayMovie )
    {
        Movie.Init();
        //Movie.SetLanguage(XL_LANG_ENGLISH);
        m_bPlayMovie = Movie.Open( SelectBestClip("MenuBackground"),TRUE,TRUE );
    }
#else
    // Load the background
    g_UiMgr->LoadBackground ( "background1", "A51Background.xbmp" );
#endif

    // Create main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );

    irect mainarea(136, DIALOG_TOP, 376, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "main menu", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifdef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
#else
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateMainMenu( void )
{
    // check for menu automation
    if( CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT )
    {
        if( GetSelectedProfile( 0 ) == 0 )
        {
            m_Profiles[0].SetHash();
            SetSelectedProfile( 0, m_Profiles[0].GetHash() );
        }

        // activate controller 0
        SetControllerRequested( 0, TRUE );

        if( g_NetworkMgr.IsOnline() )
        {
            SetState( SM_ONLINE_MAIN_MENU );
        }
        else
        {
            // Can't have movie playing, or anything else, while
            // trying to go online.
#if defined(USE_MOVIES)
            if( m_bPlayMovie )
            {
                Movie.Close();
                Movie.Kill();
                m_bPlayMovie = FALSE;
            }
#endif
            SetState( SM_ONLINE_CONNECT );
            
        }
    }
    else if ( CONFIG_IS_AUTOCAMPAIGN )
    {
        // activate controller 0
        SetControllerRequested( 0, TRUE );
 
        g_PendingConfig.SetPlayerCount( 1 );
        g_PendingConfig.SetGameTypeID( GAME_CAMPAIGN );
        SetState( SM_PROFILE_SELECT );
    }
    else if ( CONFIG_IS_AUTOSPLITSCREEN )
    {
        // activate controller 0
        SetControllerRequested( 0, TRUE );
 
        SetState( SM_MULTI_PLAYER_MENU );
    }
    else
    {
        // Get the current dialog state
        if( m_CurrentDialog != NULL )
        {
            u32 DialogState = m_CurrentDialog->GetState();

            switch( DialogState )
            {
                case DIALOG_STATE_SELECT:
                {
                    s32 Control = m_CurrentDialog->GetControl();

                    switch( Control )
                    {
                        case IDC_MAIN_MENU_CAMPAIGN:
                        {
                            g_PendingConfig.SetGameTypeID( GAME_CAMPAIGN );
                            g_PendingConfig.SetPlayerCount( 1 );

                            if( GetSelectedProfile( 0 ) != 0 )
                            {
                                SetState( SM_CAMPAIGN_MENU );
                            }
                            else
                            {
                                SetState( SM_PROFILE_SELECT );
                            }
                        }
                        break;

                        case IDC_MAIN_MENU_MULTI:
                        {
                            // Clear the profiles and controller requests.
                            // This will prevent issues associated with default profiles.
                            for( s32 p = 0; p < SM_MAX_PLAYERS; p++ )
                            {
                                ClearSelectedProfile( p );
                                SetControllerRequested( p, FALSE );
                            }

                            SetState( SM_MULTI_PLAYER_MENU );
                        }
                        break;

                        case IDC_MAIN_MENU_ONLINE:
                        {
                            SetState( SM_ONLINE_CONNECT );
                        }
                        break;

                        case IDC_MAIN_MENU_SETTINGS:
                        {
                            // Initialize pending settings.
                            InitPendingSettings();
                            SetState( SM_SETTINGS_MENU );
                        }
                        break;

                        case IDC_MAIN_MENU_PROFILES:
                        {
                            SetState( SM_MANAGE_PROFILES );
                        }
                        break;

                        case IDC_MAIN_MENU_CREDITS:
                        {
                            SetState( SM_CREDITS_SCREEN );
                        }
                        break;
                    }
                }
                break;
            }
        }
    }
}

//=========================================================================

void state_mgr::ExitMainMenu( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsMenu( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(16, DIALOG_TOP, 496, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateSettingsMenu( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_SETTINGS_AUDIO:
                    {
                        SetState( SM_SETTINGS_AUDIO );
                    }
                    break;

                    case IDC_SETTINGS_HEADSET:
                    {
                        SetState( SM_SETTINGS_HEADSET );
                    }
                    break;

                    case IDC_SETTINGS_GRAPHICS:
                    {
                        SetState( SM_SETTINGS_GRAPHICS );
                    }
                    break;

                    case IDC_SETTINGS_DISPLAY:
                    {
                        SetState( SM_SETTINGS_DISPLAY );
                    }
                    break;

                    case IDC_SETTINGS_UI:
                    {
                        SetState( SM_SETTINGS_UI );
                    }
                    break;

                    case IDC_SETTINGS_LANGUAGE:
                    {
                        SetState( SM_SETTINGS_LANGUAGE );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsMenu( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsAudio( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "audio settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateSettingsAudio( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_SETTINGS_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsAudio( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsHeadset( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "headset", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateSettingsHeadset( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_SETTINGS_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsHeadset( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsGraphics( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "graphics settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateSettingsGraphics( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_SETTINGS_MENU );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                if( m_CurrentDialog->GetControl() == IDC_GRAPHICS_ADVANCED )
                {
                    SetState( SM_SETTINGS_GRAPHICS_ADVANCED );
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsGraphics( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsGraphicsAdvanced( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID,
                                           "graphics advanced settings",
                                           mainarea,
                                           NULL,
                                           ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateSettingsGraphicsAdvanced( void )
{
    if( m_CurrentDialog != NULL )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_SETTINGS_GRAPHICS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsGraphicsAdvanced( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsDisplay( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "display settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateSettingsDisplay( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_SETTINGS_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsDisplay( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsUI( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "ui settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateSettingsUI( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_SETTINGS_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsUI( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsLanguage( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "language settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateSettingsLanguage( void )
{
    if( m_CurrentDialog )
    {
        u32 const DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_SETTINGS_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_SETTINGS_SAVE_DATA_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsLanguage( void )
{
}

//=========================================================================

void state_mgr::EnterSettingsSaveDataSelect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdateSettingsSaveDataSelect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // save settings was successful
                SetState( SM_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // save failed or was aborted
                SetState( SM_MAIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSettingsSaveDataSelect( void )
{
}

//=========================================================================

void state_mgr::EnterManageProfiles( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_MANAGE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateManageProfiles( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected a profile
                SetState( SM_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // do nothing
                SetState( SM_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // create a profile
                m_bCreatingProfile = TRUE;
                SetState( SM_MANAGE_PROFILE_OPTIONS );
            }
            break;

            case DIALOG_STATE_EDIT:
            {
                // edit a profile
                InitPendingProfile(0);
                m_bCreatingProfile = FALSE;
                SetState( SM_MANAGE_PROFILE_OPTIONS );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitManageProfiles( void )
{
}

//=========================================================================
void state_mgr::EnterManageProfileOptions( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // configure the dialog for creating a profile-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_NORMAL, m_bCreatingProfile );
}

//=========================================================================

void state_mgr::UpdateManageProfileOptions( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_MANAGE_PROFILE_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_AVATAR:
                    {
                        SetState( SM_MANAGE_PROFILE_AVATAR );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_MAIN_MENU );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // abort create/edit profile
                SetState( SM_MANAGE_PROFILES );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // save success - current profile
                SetState( SM_MANAGE_PROFILES );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_MANAGE_PROFILE_SAVE_SELECT ); 
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitManageProfileOptions( void )
{
}

//=========================================================================

void state_mgr::EnterManageProfileControls( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateManageProfileControls( void )
{
    UpdateProfileControlsMenu( SM_MANAGE_PROFILE_OPTIONS, FALSE );
}

//=========================================================================

void state_mgr::ExitManageProfileControls( void )
{
}

//=========================================================================

void state_mgr::EnterManageProfileAvatar( void )
{
    // Create avatar dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(121, DIALOG_TOP, 391, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "avatar select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateManageProfileAvatar( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_MANAGE_PROFILE_OPTIONS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_MANAGE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitManageProfileAvatar( void )
{
}

//=========================================================================

void state_mgr::EnterManageProfileSaveSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateManageProfileSaveSelect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                SetState( SM_MANAGE_PROFILES );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // continue without saving
                SetState( SM_MANAGE_PROFILES );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_MANAGE_PROFILE_SAVE_DATA_RESELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitManageProfileSaveSelect( void )
{
}

//=========================================================================

void state_mgr::EnterManageSaveDataReselect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdateManageSaveDataReselect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                SetState( SM_MANAGE_PROFILES );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // Go back to the profile select menu.
                SetState( SM_MANAGE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitManageSaveDataReselect( void )
{
}

//=========================================================================

void state_mgr::EnterCampaignMenu( void )
{
    // Create main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(116, DIALOG_TOP, 396, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "campaign menu", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifdef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
#else
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateCampaignMenu( void )
{
#ifndef CONFIG_RETAIL
    // check for auto campaign
    if( CONFIG_IS_AUTOCAMPAIGN )
    {
        // use profile defaults
        SetState( SM_LEVEL_SELECT );        
    }
#endif

    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_CAMPAIGN_MENU_NEW_CAMPAIGN:
                    {
                        // Start new campaign.
                        player_profile& ActiveProfile = GetActiveProfile(0);
                        // Flag difficulty as unchanged.
                        ActiveProfile.SetDifficultyChanged( FALSE );

                        // Set initial level.
                        const map_entry* pMapEntry = g_MapList.Find( -1, GAME_CAMPAIGN);
                        ASSERTS( pMapEntry, "Cannot find a campaign map in the maplist" );
                        g_PendingConfig.SetLevelID( pMapEntry->GetMapID() );
                        g_PendingConfig.SetMaxPlayerCount( 1 );
                        g_StateMgr.SetLevelIndex( 0 );

                        ASSERT( g_NetworkMgr.IsOnline()==FALSE );
                        g_NetworkMgr.BecomeServer();

                        // Set campaign game type.
                        m_CampaignType = SM_NEW_CAMPAIGN_GAME;

                        // Set state to load it.
                        SetState( SM_START_GAME );
                    }
                    break;

                    case IDC_CAMPAIGN_MENU_RESUME_CAMPAIGN:
                    {
                        // Set campaign game type.
                        m_CampaignType = SM_LOAD_CAMPAIGN_GAME;
                        // Go to the campaign load screen.
    //                    SetState( SM_LOAD_CAMPAIGN );
                        SetState( SM_RESUME_CAMPAIGN );
                    }
                    break;

                    case IDC_CAMPAIGN_MENU_EDIT_PROFILE:
                    {
                        // Edit the currently active profile.
                        SetState( SM_CAMPAIGN_PROFILE_OPTIONS );
                    }
                    break;

                    case IDC_CAMPAIGN_MENU_LORE:
                    {
                        // Go to the lore menu.
                        SetState( SM_LORE_MAIN_MENU );
                    }
                    break;

                    case IDC_CAMPAIGN_MENU_SECRETS:
                    {
                        // Go to the secrets menu.
                        SetState( SM_SECRETS_MENU );
                    }
                    break;

                    case IDC_CAMPAIGN_MENU_EXTRAS:
                    {
                        // Go to the extras menu.
                        SetState( SM_EXTRAS_MENU );
                    }
                    break;
#ifndef CONFIG_RETAIL
                    case IDC_CAMPAIGN_MENU_LEVEL_SELECT:
                    {
                        // Set campaign game type.
                        m_CampaignType = SM_DEBUG_SELECT_GAME;
                        // Debug only - select from the available levels.
                        SetState( SM_LEVEL_SELECT );
                    }
                    break;
#endif
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                if( GetSelectedProfile(0) != 0 )
                {
                    SetState( SM_MAIN_MENU );
                }
                else
                {
                    SetState( SM_PROFILE_SELECT );
                }
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitCampaignMenu( void )
{
}

//=========================================================================

void state_mgr::EnterLoadCampaign( void )
{
    //  Create load game screen 
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 96, DIALOG_TOP, 416, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "load game", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateLoadCampaign( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Exit menu and restore save game data.
                g_PendingConfig.SetMaxPlayerCount( 1 );

                g_NetworkMgr.BecomeServer();
                SetState( SM_START_SAVE_GAME );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_CAMPAIGN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitLoadCampaign( void )
{
}

//=========================================================================

void state_mgr::EnterSaveCampaign( void )
{
    //  Create save game screen 
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 96, DIALOG_TOP, 416, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save game", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );

    mainarea.Translate( 0, SAFE_ZONE );

    g_UiMgr->SetScreenSize(mainarea);       

    // enabled user input
    g_UiMgr->EnableUser( g_UiUserID, TRUE );

    // pause the game
    m_bIsPaused = TRUE;
}

//=========================================================================

void state_mgr::UpdateSaveCampaign( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Saving complete.
                SetState( SM_PLAYING_GAME );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // Cancel saving and return to game.
                SetState( SM_PLAYING_GAME );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSaveCampaign( void )
{
    // Kill the save menu and return to the game
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;

    // Disable ui
    g_UiMgr->EnableUser( g_UiUserID, FALSE );

    // unpause the game
    m_bIsPaused = FALSE;
}

//=========================================================================

void state_mgr::EnterResumeCampaign( void )
{
    //  Create load game screen 
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 96, DIALOG_TOP, 416, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "resume game", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateResumeCampaign( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // The checkpoint is actually restored within the dialog code. But this is where
                // we need to tell the statemgr that we're going to restore a level from a save.
                m_bStartSaveGame = TRUE;
                g_PendingConfig.SetMaxPlayerCount( 1 );

                g_NetworkMgr.BecomeServer();

                SetState( SM_START_SAVE_GAME );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_CAMPAIGN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitResumeCampaign( void )
{
}

//=========================================================================

void state_mgr::EnterLoreMainMenu ( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "lore main", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateLoreMainMenu( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_CAMPAIGN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitLoreMainMenu  ( void )
{
}

//=========================================================================

void state_mgr::EnterSecretsMenu( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "secrets", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateSecretsMenu( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_CAMPAIGN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSecretsMenu( void )
{
}

//=========================================================================

void state_mgr::EnterExtrasMenu( void )
{
    //  Create extras menu dialog 
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 96, DIALOG_TOP, 416, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "extras", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateExtrasMenu( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_CAMPAIGN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitExtrasMenu( void )
{
}

//=========================================================================

void state_mgr::EnterCreditsScreen( void )
{
    // Create the press start screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 16, DIALOG_TOP, ui_viewport::CONTENT_WIDTH - 16, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "credits screen", mainarea, NULL, ui_win::WF_VISIBLE);//|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
}

//=========================================================================

void state_mgr::UpdateCreditsScreen( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_MAIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitCreditsScreen( void )
{
}

//=========================================================================

void state_mgr::EnterCampaignProfileOptions( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateCampaignProfileOptions( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_CAMPAIGN_PROFILE_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_AVATAR:
                    {
                        SetState( SM_CAMPAIGN_PROFILE_AVATAR );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_CAMPAIGN_MENU );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_CAMPAIGN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // save success - current profile
                SetState( SM_CAMPAIGN_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_CAMPAIGN_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitCampaignProfileOptions( void )
{
}

//=========================================================================

void state_mgr::EnterCampaignProfileControls( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateCampaignProfileControls( void )
{
    UpdateProfileControlsMenu( SM_CAMPAIGN_PROFILE_OPTIONS, FALSE );
}

//=========================================================================

void state_mgr::ExitCampaignProfileControls( void )
{
}

//=========================================================================

void state_mgr::EnterCampaignProfileAvatar( void )
{
    // Create avatar dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(121, DIALOG_TOP, 391, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "avatar select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateCampaignProfileAvatar( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_CAMPAIGN_PROFILE_OPTIONS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_CAMPAIGN_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitCampaignProfileAvatar( void )
{
}

//=========================================================================

void state_mgr::EnterCampaignProfileSaveSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateCampaignProfileSaveSelect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                SetState( SM_CAMPAIGN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // continue without saving
                SetState( SM_CAMPAIGN_MENU );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_CAMPAIGN_SAVE_DATA_RESELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitCampaignProfileSaveSelect( void )
{
}

//=========================================================================

void state_mgr::EnterCampaignSaveDataReselect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdateCampaignSaveDataReselect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                // Go to the campaign menu.
                SetState( SM_CAMPAIGN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // Go back to the profile select menu.
                SetState( SM_CAMPAIGN_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitCampaignSaveDataReselect( void )
{
}

//=========================================================================

void state_mgr::EnterProfileSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateProfileSelect( void )
{
    // check for auto campaign
    if( CONFIG_IS_AUTOCAMPAIGN )
    {
        // use profile defaults
        SetState( SM_CAMPAIGN_MENU );        
    }

    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                SetState( SM_CAMPAIGN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_PROFILE_OPTIONS );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitProfileSelect( void )
{
}

//=========================================================================

void state_mgr::EnterProfileOptions( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // configure the dialog for creating a profile-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_NORMAL, TRUE );
}

//=========================================================================

void state_mgr::UpdateProfileOptions( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_PROFILE_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_AVATAR:
                    {
                        SetState( SM_PROFILE_AVATAR );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_CAMPAIGN_MENU );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // abort create/edit profile
                SetState( SM_PROFILE_SELECT );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // save success - current profile
                SetState( SM_PROFILE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitProfileOptions( void )
{
}

//=========================================================================

void state_mgr::EnterProfileControls( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================
    
void state_mgr::UpdateProfileControls( void )
{
    UpdateProfileControlsMenu( SM_PROFILE_OPTIONS, FALSE );
}

//=========================================================================

void state_mgr::ExitProfileControls( void )
{
}

//=========================================================================

void state_mgr::EnterProfileMouseControls( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile mouse controls", mainarea, NULL, ui_win::WF_VISIBLE | ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif

    if( IsPaused() )
    {
        static_cast<dlg_profile_mouse_controls*>( m_CurrentDialog )->EnableBlackout();
    }
}

//=========================================================================

void state_mgr::EnterProfileKeyboardBindings( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile keyboard bindings", mainarea, NULL, ui_win::WF_VISIBLE | ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif

    if( IsPaused() )
    {
        static_cast<dlg_profile_keyboard_bindings*>( m_CurrentDialog )->EnableBlackout();
    }
}

//=========================================================================

void state_mgr::EnterProfileGamepadControls( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile gamepad controls", mainarea, NULL, ui_win::WF_VISIBLE | ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif

    if( IsPaused() )
    {
        static_cast<dlg_profile_gamepad_controls*>( m_CurrentDialog )->EnableBlackout();
    }
}

//=========================================================================

void state_mgr::UpdateProfileDeviceControls( void )
{
    xbool const IsOnlineControls =
        (m_PrevState == SM_ONLINE_PROFILE_CONTROLS) ||
        (m_PrevState == SM_ONLINE_EDIT_CONTROLS) ||
        (m_PrevState == SM_PAUSE_ONLINE_CONTROLS);

    if( IsOnlineControls && CheckForDisconnect() )
    {
        return;
    }

    if( m_CurrentDialog == NULL )
    {
        return;
    }

    switch( m_CurrentDialog->GetState() )
    {
        case DIALOG_STATE_BACK:
        {
            SetState( m_PrevState );
        }
        break;

        case DIALOG_STATE_SAVE_DATA_ERROR:
        {
            switch( m_PrevState )
            {
                case SM_PROFILE_CONTROLS:
                {
                    SetState( SM_PROFILE_SELECT );
                }
                break;

                case SM_MANAGE_PROFILE_CONTROLS:
                {
                    SetState( SM_MANAGE_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_CAMPAIGN_PROFILE_CONTROLS:
                {
                    SetState( SM_CAMPAIGN_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PROFILE_CONTROLS_MP:
                {
                    SetState( SM_PROFILE_SAVE_SELECT_MP );
                }
                break;

                case SM_ONLINE_PROFILE_CONTROLS:
                case SM_ONLINE_EDIT_CONTROLS:
                {
                    SetState( SM_ONLINE_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PAUSE_CONTROLS:
                {
                    SetState( SM_PAUSE_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PAUSE_MP_CONTROLS:
                {
                    SetState( SM_PAUSE_MP_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PAUSE_ONLINE_CONTROLS:
                {
                    SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
                }
                break;

                default:
                {
                    SetState( m_PrevState );
                }
                break;
            }
        }
        break;
    }
}

//=========================================================================

void state_mgr::UpdateProfileControlsMenu( sm_states BackState, xbool CheckDisconnect )
{
    if( CheckDisconnect && CheckForDisconnect() )
    {
        return;
    }

    if( m_CurrentDialog == NULL )
    {
        return;
    }

    switch( m_CurrentDialog->GetState() )
    {
        case DIALOG_STATE_SELECT:
        {
            s32 const Control = m_CurrentDialog->GetControl();

            switch( Control )
            {
                case IDC_CONTROLS_KEYBOARD_MENU:
                {
                    SetState( SM_PROFILE_KEYBOARD_BINDINGS );
                }
                break;

                case IDC_CONTROLS_MOUSE_MENU:
                {
                    SetState( SM_PROFILE_MOUSE_CONTROLS );
                }
                break;

                case IDC_CONTROLS_GAMEPAD_MENU:
                {
                    SetState( SM_PROFILE_GAMEPAD_CONTROLS );
                }
                break;
            }
        }
        break;

        case DIALOG_STATE_BACK:
        {
            SetState( BackState );
        }
        break;

        case DIALOG_STATE_SAVE_DATA_ERROR:
        {
            switch( BackState )
            {
                case SM_MANAGE_PROFILE_OPTIONS:
                {
                    SetState( SM_MANAGE_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PROFILE_OPTIONS:
                {
                    SetState( SM_PROFILE_SELECT );
                }
                break;

                case SM_CAMPAIGN_PROFILE_OPTIONS:
                {
                    SetState( SM_CAMPAIGN_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_MULTI_PLAYER_EDIT:
                {
                    SetState( SM_PROFILE_SAVE_SELECT_MP );
                }
                break;

                case SM_ONLINE_PROFILE_OPTIONS:
                case SM_ONLINE_EDIT_PROFILE:
                {
                    SetState( SM_ONLINE_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PAUSE_OPTIONS:
                {
                    SetState( SM_PAUSE_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PAUSE_MP_OPTIONS:
                {
                    SetState( SM_PAUSE_MP_PROFILE_SAVE_SELECT );
                }
                break;

                case SM_PAUSE_ONLINE_OPTIONS:
                {
                    SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
                }
                break;

                default:
                {
                    SetState( BackState );
                }
                break;
            }
        }
        break;
    }
}

//=========================================================================

void state_mgr::EnterProfileAvatar( void )
{
    // Create avatar dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(121, DIALOG_TOP, 391, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "avatar select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateProfileAvatar( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PROFILE_OPTIONS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PROFILE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitProfileAvatar( void )
{
}

//=========================================================================

void state_mgr::EnterMultiPlayer( void )
{
    // Create multi player menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "multiplayer menu", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateMultiPlayer( void )
{
    if ( CONFIG_IS_AUTOSPLITSCREEN )
    {
        SetState( SM_MULTI_PLAYER_OPTIONS );
    }

    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_EDIT:
            {
                // profile options
                m_bCreatingProfile = FALSE;
                SetState( SM_MULTI_PLAYER_EDIT );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // profile options
                m_bCreatingProfile = TRUE;
                SetState( SM_MULTI_PLAYER_EDIT );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // game options
                InitPendingSettings();
                SetState( SM_MULTI_PLAYER_OPTIONS );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // back to main menu
                SetState( SM_MAIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitMultiPlayer( void )
{
}

//=========================================================================

void state_mgr::EnterMultiPlayerOptions( void )
{
    // Create online host menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "multi options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateMultiPlayerOptions( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                SetState( SM_MP_LEVEL_SELECT ); 
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_MULTI_PLAYER_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitMultiPlayerOptions( void )
{
}

//=========================================================================

void state_mgr::EnterMultiPlayerEdit( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif

    // configure the dialog for MP options-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_NORMAL, m_bCreatingProfile );
}

//=========================================================================

void state_mgr::UpdateMultiPlayerEdit( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_PROFILE_CONTROLS_MP );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_AVATAR:
                    {
                        SetState( SM_PROFILE_AVATAR_MP );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_MULTI_PLAYER_MENU );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // abort create/edit profile
                SetState( SM_MULTI_PLAYER_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // save success - current profile
                SetState( SM_MULTI_PLAYER_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_PROFILE_SAVE_SELECT_MP );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitMultiPlayerEdit( void )
{
}

//=========================================================================

void state_mgr::EnterMPLevelSelect( void )
{
    //  Create level select screen 
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "online level select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_online_level_select*)m_CurrentDialog)->Configure( MAP_SELECT_MP );
}

//=========================================================================

void state_mgr::UpdateMPLevelSelect( void )
{

    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_MULTI_PLAYER_OPTIONS );
            }
            break;
            
            case DIALOG_STATE_SELECT:
            {
                // save to save data and return to menu
                SetState( SM_MP_SAVE_SETTINGS );
            }
            break;
            
            case DIALOG_STATE_ACTIVATE:
            {
                ASSERT( g_NetworkMgr.IsOnline()==FALSE );
            
                g_NetworkMgr.BecomeServer();
                SetState( SM_START_GAME );
            }
            break;
            
            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_MP_SAVE_SETTINGS );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitMPLevelSelect( void )
{
}

//=========================================================================

void state_mgr::EnterMPSaveSettings( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdateMPSaveSettings( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save settings was successful.
                ASSERT( g_NetworkMgr.IsOnline()==FALSE );

                g_NetworkMgr.BecomeServer();
                SetState( SM_START_GAME );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // Save settings failed or was aborted.
                ASSERT( g_NetworkMgr.IsOnline()==FALSE );

                g_NetworkMgr.BecomeServer();
                SetState( SM_START_GAME );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitMPSaveSettings( void )
{
}

//=========================================================================

void state_mgr::EnterProfileControlsMP( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateProfileControlsMP( void )
{
    UpdateProfileControlsMenu( SM_MULTI_PLAYER_EDIT, FALSE );
}

//=========================================================================

void state_mgr::ExitProfileControlsMP( void )
{
}

//=========================================================================

void state_mgr::EnterProfileAvatarMP( void )
{
    // Create avatar dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(121, DIALOG_TOP, 391, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "avatar select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateProfileAvatarMP( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_MULTI_PLAYER_EDIT );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PROFILE_SAVE_SELECT_MP );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitProfileAvatarMP( void )
{
}

//=========================================================================

void state_mgr::EnterProfileSaveSelectMP( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateProfileSaveSelectMP( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                SetState( SM_MULTI_PLAYER_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // continue without saving
                SetState( SM_MULTI_PLAYER_MENU );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_SAVE_DATA_RESELECT_MP );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitProfileSaveSelectMP( void )
{
}

//=========================================================================

void state_mgr::EnterSaveDataReselectMP( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdateSaveDataReselectMP( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                // Go to the multiplayer main menu.
                SetState( SM_MULTI_PLAYER_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // Go back to the profile select menu.
                SetState( SM_PROFILE_SAVE_SELECT_MP );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitSaveDataReselectMP( void )
{
}

//=========================================================================


//=========================================================================

void state_mgr::EnterOnlineConnect( void )
{
    // Create online main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(76, DIALOG_TOP, 436, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "connect info", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineConnect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_EXIT:
            {
                // We got connected, so we make sure that we know we've not
                // been notified of a disconnect due to network being down.
                g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );

                if( (GetProfileListIndex(0) != -1) && 
                    (GetSelectedProfile( GetProfileListIndex(0) ) != 0 ) )
                {
                    // set nickname
                    player_profile& ActiveProfile = GetActiveProfile( GetProfileListIndex(0) );
                    g_MatchMgr.SetNickname( ActiveProfile.GetProfileName() );
                    SetState( SM_ONLINE_IDENTITY );
                }
                else
                {
                    SetState( SM_ONLINE_PROFILE_SELECT );
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                BeginNetworkDisconnect( SM_MAIN_MENU );
            }
            break;
        }
    }    
}

//=========================================================================

void state_mgr::ExitOnlineConnect( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineProfileSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineProfileSelect( void )
{
    if( m_CurrentDialog->GetState() != DIALOG_STATE_WAIT_FOR_SAVE_DATA )
    {
        if( CheckForDisconnect() )
        {
            return;
        }
    }

    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                SetState( SM_ONLINE_IDENTITY );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                BeginNetworkDisconnect( SM_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_ONLINE_PROFILE_OPTIONS );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineProfileSelect( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineProfileOptions( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif

    // configure the dialog for MP options-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_NORMAL, TRUE );
}

//=========================================================================

void state_mgr::UpdateOnlineProfileOptions( void )
{
    if( CheckForDisconnect() )
    {
        return;
    }

    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_ONLINE_PROFILE_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_AVATAR:
                    {
                        SetState( SM_ONLINE_PROFILE_AVATAR );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_ONLINE_IDENTITY );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // abort create/edit profile
                SetState( SM_ONLINE_PROFILE_SELECT );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineProfileOptions( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineProfileControls( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineProfileControls( void )
{
    UpdateProfileControlsMenu( SM_ONLINE_PROFILE_OPTIONS, TRUE );
}

//=========================================================================

void state_mgr::ExitOnlineProfileControls( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineProfileAvatar( void )
{
    // Create avatar dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(121, DIALOG_TOP, 391, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "avatar select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineProfileAvatar( void )
{
    if( CheckForDisconnect() )
    {
        return;
    }

    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_ONLINE_PROFILE_OPTIONS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_ONLINE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineProfileAvatar( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineIdentity( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;
}

//=========================================================================

void state_mgr::UpdateOnlineIdentity( void )
{
    SetupProfileListIndex();
    player_profile& Profile = GetActiveProfile( GetProfileListIndex(0) );
    s32 IdLength = 0;
    const byte* pId = Profile.GetUniqueId( IdLength );
    if( IdLength == 0 )
    {
        guid Id = guid_New();
        Profile.SetUniqueId( (const byte*)&Id, sizeof(Id) );
        Profile.MarkDirty();
        pId = Profile.GetUniqueId( IdLength );
    }
    g_MatchMgr.SetLocalIdentity( Profile.GetProfileName(), pId, IdLength );
    SetState( SM_ONLINE_MAIN_MENU );
}

//=========================================================================

void state_mgr::ExitOnlineIdentity( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineProfileSaveSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineProfileSaveSelect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // continue without saving
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_ONLINE_SAVE_DATA_RESELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineProfileSaveSelect( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineSaveDataReselect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdateOnlineSaveDataReselect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                // Go to the online main menu.
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // Go back to the profile select menu.
                SetState( SM_ONLINE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineSaveDataReselect( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineMain( void )
{
#ifdef USE_MOVIES
    if( !m_bPlayMovie )
    {
        Movie.Init();
        //Movie.SetLanguage(XL_LANG_ENGLISH);
        m_bPlayMovie = Movie.Open( SelectBestClip("MenuBackground"),TRUE,TRUE );
    }
#else
    s32 Background = g_UiMgr->FindBackground("background1");
    // Load the background. This *may* not have already been loaded depending on where entry to the online ui
    // happens. If it comes from main menu, then it will have been loaded but if it comes from post-game exit,
    // then it will have been unloaded so we need to load it up again.
    if( Background == -1 )
    {
        Background = g_UiMgr->LoadBackground ( "background1", "A51Background.xbmp" );
    }
#endif

    // Create online main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(106, DIALOG_TOP, 406, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "online menu", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifdef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
#else
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineMain( void )
{
    if( CheckForDisconnect() )
        return;

    if( CONFIG_IS_AUTOSERVER )
    {
        SetState( SM_ONLINE_HOST_MENU );
    }
    else if( CONFIG_IS_AUTOCLIENT )
    {
        SetState( SM_ONLINE_JOIN_MENU );
    }
    else if( m_CurrentDialog != NULL )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_SELECT:
            {
                switch( m_CurrentDialog->GetControl() )
                {
                    case IDC_ONLINE_JOIN:
                        SetState( SM_ONLINE_JOIN_MENU );
                        break;
                    case IDC_ONLINE_HOST:
                        InitPendingSettings();
                        SetState( SM_ONLINE_HOST_MENU );
                        break;
                    case IDC_ONLINE_EDIT_PROFILE:
                        SetState( SM_ONLINE_EDIT_PROFILE );
                        break;
                    case IDC_ONLINE_VIEW_STATS:
                        SetState( SM_ONLINE_STATS );
                        break;
                    default:
                        ASSERT( FALSE );
                        break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
                BeginNetworkDisconnect( SM_MAIN_MENU );
                break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineMain( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineHost( void )
{
    // Create online host menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(6, DIALOG_TOP, 506, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "online host", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineHost( void )
{
    if( CheckForDisconnect() )
    {
        return;
    }

    // check for menu automation
    if( CONFIG_IS_AUTOSERVER )
    {
#ifndef CONFIG_RETAIL
        if( !g_MatchMgr.PrepareHost( NET_GAME_PORT ) )
        {
            g_Config.AutoServer = FALSE;
            return;
        }
        g_PendingConfig.SetGameTypeID( (game_type)g_Config.AutoServerType );
        g_PendingConfig.SetPlayerCount(1);
        g_PendingConfig.SetLevelID( g_Config.AutoLevel );
        g_PendingConfig.SetMutationMode( (mutation_mode)g_Config.AutoMutateMode );
        g_PendingConfig.SetMaxPlayerCount( 16 );

        switch( g_Config.AutoServerType )
        {
            case GAME_DM:   g_PendingConfig.SetGameType( "Deathmatch" );        g_PendingConfig.SetShortGameType( "DM" );       break;
            case GAME_TDM:  g_PendingConfig.SetGameType( "Team Deathmatch" );   g_PendingConfig.SetShortGameType( "TDM" );      break;
            case GAME_CTF:  g_PendingConfig.SetGameType( "Capture the Flag" );  g_PendingConfig.SetShortGameType( "CTF" );      break;
            case GAME_TAG:  g_PendingConfig.SetGameType( "Tag" );               g_PendingConfig.SetShortGameType( "Tag" );      break;
            case GAME_INF:  g_PendingConfig.SetGameType( "Infection" );         g_PendingConfig.SetShortGameType( "Inf" );      break;
            case GAME_CNH:  g_PendingConfig.SetGameType( "Capture and Hold" );  g_PendingConfig.SetShortGameType( "CNH" );      break;
        }

        // tell the network manager that we want to be a server
        g_NetworkMgr.BecomeServer();

        SetState( SM_START_GAME );
#endif
    }
    else if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_ACTIVATE:
            {
                g_NetworkMgr.BecomeServer();
                SetState( SM_START_GAME );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_HOST_SAVE_SETTINGS );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineHost( void )
{
}

//=========================================================================

void state_mgr::EnterHostSaveSettings( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdateHostSaveSettings( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Start up the server.
                g_NetworkMgr.BecomeServer();
                SetState( SM_START_GAME );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                g_NetworkMgr.BecomeServer();
                SetState( SM_START_GAME );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitHostSaveSettings( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineJoin( void )
{
    // Create online join menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(6, DIALOG_TOP, 506, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "online join", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineJoin( void )
{
    if( CheckForDisconnect() )
    {
        return;
    }

    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
                SetState( SM_ONLINE_MAIN_MENU );
                break;


            case DIALOG_STATE_SELECT:
            {
                StartLogin( &g_PendingConfig.GetConfig(), LOGIN_FROM_JOIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineJoin( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineJoinSaveSettings( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdateOnlineJoinSaveSettings( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save was successful.
                // Go to the online join (server list).
                SetState( SM_ONLINE_JOIN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // Save failed or was aborted.
                // Go to the online join (server list).
                SetState( SM_ONLINE_JOIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineJoinSaveSettings( void )
{
}

//=========================================================================


//=========================================================================


//=========================================================================


//=========================================================================

void state_mgr::EnterOnlineStats( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "stats", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateOnlineStats( void )
{
    if( CheckForDisconnect() )
    {
        return;
    }

    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineStats( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineEditProfile( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif

    // configure the dialog for MP options-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_NORMAL, FALSE );
}

//=========================================================================

void state_mgr::UpdateOnlineEditProfile( void )
{
    if( CheckForDisconnect() )
    {
        return;
    }

    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_ONLINE_EDIT_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_AVATAR:
                    {
                        SetState( SM_ONLINE_EDIT_AVATAR );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_ONLINE_MAIN_MENU );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_ONLINE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_ONLINE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineEditProfile( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineEditControls( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineEditControls( void )
{
    UpdateProfileControlsMenu( SM_ONLINE_EDIT_PROFILE, TRUE );
}

//=========================================================================

void state_mgr::ExitOnlineEditControls( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineEditAvatar( void )
{
    // Create avatar dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(121, DIALOG_TOP, 391, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "avatar select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateOnlineEditAvatar( void )
{
    if( CheckForDisconnect() )
    {
        return;
    }

    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_ONLINE_EDIT_PROFILE );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_ONLINE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineEditAvatar( void )
{
}

//=========================================================================

void state_mgr::EnterLevelSelect( void )
{
    //  Create level select screen 
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(116, DIALOG_TOP, 396, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "level select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateLevelSelect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                g_PendingConfig.SetMaxPlayerCount( 1 );

                g_NetworkMgr.BecomeServer();
                EnableDefaultLoadOut();
                SetState( SM_START_GAME );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( m_PrevState );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitLevelSelect( void )
{
}

//=========================================================================

void state_mgr::EnterStartSaveGame( void )
{
    //  Start new game option
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "start game", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateStartSaveGame( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                m_bStartSaveGame = TRUE;
                SetState( SM_SINGLE_PLAYER_LOAD_MISSION );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitStartSaveGame( void )
{
}
    
//=========================================================================

void state_mgr::EnterStartGame( void )
{
    // prepare for loading the level
    m_bShowingScores = FALSE;
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
    g_RscMgr.Unload( "AUDIO/DX_FRONTEND.AUDIOPKG"    );
    g_RscMgr.Unload( "AUDIO/SFX_FRONTEND.AUDIOPKG"   );
    g_RscMgr.Unload( "AUDIO/MUSIC_FRONTEND.AUDIOPKG" );
    g_RscMgr.Unload( "AUDIO/DREAMLAND.AUDIOPKG" );
    g_UiMgr->UnloadBackground( "titlescreen" );

#ifdef USE_MOVIES
    // Kill the background movie
    Movie.Close();
    Movie.Kill();
    m_bPlayMovie = FALSE;
#endif

    irect mainarea( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "start game", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateStartGame( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Calculate checksum for profile prior to starting game. This will mark the profile
                // as having not been modified.
                player_profile& ActiveProfile = GetActiveProfile( GetProfileListIndex(0) );
                ActiveProfile.Checksum();

                m_bStartSaveGame = FALSE;
                s_FirstMap = TRUE;

                // Advance to either a load screen, or a level description
                // screen (which also does loading in the background).
                if( GameMgr.GetGameType() == GAME_CAMPAIGN )
                {
                    SetState( SM_SINGLE_PLAYER_LOAD_MISSION );
                }
                else
                {
                    SetState( SM_MULTI_PLAYER_LOAD_MISSION );
                }
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitStartGame( void )
{
}

//=========================================================================
void state_mgr::EnterSinglePlayerLoadMission( void )
{
    //**BW** Bit of a hack here. We don't want to render anything new since
    //we're trying to display the score at this time.
    //if( m_State == SM_INTER_LEVEL_SCORE )
    //{
    //    m_bShowingScores = TRUE;
    //    return;
    //}

    // prepare for loading the level
    m_bShowingScores = FALSE;
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
    g_RscMgr.Unload( "AUDIO/DX_FRONTEND.AUDIOPKG"    );
    g_RscMgr.Unload( "AUDIO/SFX_FRONTEND.AUDIOPKG"   );
    g_RscMgr.Unload( "AUDIO/MUSIC_FRONTEND.AUDIOPKG" );
    g_RscMgr.Unload( "AUDIO/DREAMLAND.AUDIOPKG" );
    g_UiMgr->UnloadBackground( "titlescreen" );

#ifdef USE_MOVIES
    // Kill the background movie
    Movie.Close();
    Movie.Kill();
    m_bPlayMovie = FALSE;
#endif

#if !defined( X_EDITOR )
    // are we starting a new campaign?
    if( g_PendingConfig.GetGameTypeID() ==  GAME_CAMPAIGN )
    {
#ifdef USE_MOVIES
        const map_entry* pEntry = g_MapList.GetByIndex( m_LevelIndex );
        s32 iCheckpoint = g_CheckPointMgr.GetCheckPointIndex();

        // iCheckpoint == -1 when entering the level for the very first time in campaign
        // iCheckpoint == 0 when using Resume Campaign and selecting the level itself instead of a checkpoint
        // In BOTH cases, we want to play the intro cine.
        // If the player has died since the level was initially loaded, don't play the cine.
        if ( ((iCheckpoint == -1) || (iCheckpoint == 0)) && (!player::s_bPlayerDied) )
        {
            switch( pEntry->GetMapID() )
            {
                case 1000:
                {
                    // Play intro movie before starting "One of them".
                    QueueSimpleMovie( SelectBestClip("CinemaIntro"),
                                      "Intro Movie Here",
                                      FALSE );
                }
                break;

                case 1020:
                {
                    // unlock a secret after deep underground
                    player_profile& ActiveProfile = GetActiveProfile(0);                    
                    ActiveProfile.AcquireSecret();
                    x_memcpy( &m_PendingProfile, &ActiveProfile, sizeof(ActiveProfile) );
                }
                break;

                case 1060:
                {
                    // Play infection movie before starting "One of them".
                    QueueSimpleMovie( SelectBestClip( "CinemaInfection" ),
                                      "Infection Movie Here",
                                      FALSE );
                }
                break;

                case 1115:
                {
                    // Play Edgar movie before starting "The Grays".
                    QueueSimpleMovie( SelectBestClip( "CinemaEdgar" ),
                                      "Edgar Movie Here",
                                      FALSE );
                }
                break;

                default:
                {
                    // nothing to do!
                }
                break;
            }
        }
#endif // USE_MOVIES
    }

#endif

    // Create a load game dialog
    irect mainarea(116, DIALOG_TOP, 396, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "load game", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateSinglePlayerLoadMission( void )
{
    dlg_load_game*  pLoadGame   = (dlg_load_game*)g_UiMgr->GetTopmostDialog(g_UiUserID);
    server_state    ServerState = g_NetworkMgr.GetServerObject().GetState();
    dialog_states   State       = pLoadGame->GetState();

    if( ServerState != STATE_SERVER_LOAD_MISSION )
    {
        if( State == DIALOG_STATE_EXIT )
        {
            SetState( SM_SERVER_SYNC );
        }
        // let the dialog know that we have finished loading the game
        pLoadGame->LoadingComplete();
    }
}

//=========================================================================

void state_mgr::ExitSinglePlayerLoadMission( void )
{
    // end the load game dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );

#if CONFIG_IS_DEMO
    g_DemoIdleTimer.Reset();
    g_DemoIdleTimer.Start();
#endif
}

//=========================================================================
void state_mgr::EnterPlayingGame( void )
{
    m_bIsPaused      = FALSE;        
    m_bDoSystemError = FALSE;
    m_PopUp          = NULL;
    s_FirstMap       = FALSE;

    // This means that we have actually entered the game so if there is any sort of
    // failure from this point on, we need to go back to the main menu. This will be
    // used in ExitReportError to decide where to go next.
    m_LoginFailureDestination = LOGIN_FROM_MAIN;
}

//=========================================================================
void state_mgr::UpdatePlayingGame( void )
{
    UpdateScoreboardOverlay();

    if( g_ActiveConfig.GetExitReason()!=GAME_EXIT_CONTINUE )
    {
        if( g_NetworkMgr.IsServer() )
        {
            SetState( SM_SERVER_COOLDOWN );
        }
        else
        {
            SetState( SM_CLIENT_COOLDOWN );
        }
    }
}

//=========================================================================
void state_mgr::ExitPlayingGame( void )
{
    CloseScoreboardOverlay();
}

//=========================================================================

void state_mgr::UpdateScoreboardOverlay( void )
{
    if( !g_NetworkMgr.IsOnline() )
    {
        CloseScoreboardOverlay();
        return;
    }

    const xbool bShowScoreboard =
        g_GameInput[0].GetFrameLogical( ingame_pad::ACTION_SCOREBOARD ).GetIsValue() > 0.25f;

    if( bShowScoreboard )
    {
        OpenScoreboardOverlay();
    }
    else
    {
        CloseScoreboardOverlay();
    }
}

//=========================================================================

void state_mgr::OpenScoreboardOverlay( void )
{
    if( m_pScoreboardDialog != NULL )
    {
        return;
    }

    const game_score& ScoreData = GameMgr.GetScore();
    const s32 PlayerCount = ScoreData.NPlayers;

    if( PlayerCount > 16 )
    {
        irect MainArea( 8, DIALOG_TOP, 504, DIALOG_BOTTOM );
        m_pScoreboardDialog = g_UiMgr->OpenDialog( g_UiUserID, "big leaderboard", MainArea, NULL, ui_win::WF_VISIBLE );
    }
    else if( ScoreData.IsTeamBased )
    {
        irect MainArea( 8, DIALOG_TOP, 504, ui_viewport::CONTENT_HEIGHT - 36 );
        m_pScoreboardDialog = g_UiMgr->OpenDialog( g_UiUserID, "team leaderboard", MainArea, NULL, ui_win::WF_VISIBLE );
    }
    else
    {
        irect MainArea( 8, DIALOG_TOP, 504, DIALOG_BOTTOM );
        m_pScoreboardDialog = g_UiMgr->OpenDialog( g_UiUserID, "leaderboard", MainArea, NULL, ui_win::WF_VISIBLE );
    }

    if( m_pScoreboardDialog == NULL )
    {
        return;
    }

    ((dlg_mp_score*)m_pScoreboardDialog)->Configure( LEADERBOARD_OVERLAY );
}

//=========================================================================

void state_mgr::CloseScoreboardOverlay( void )
{
    if( m_pScoreboardDialog == NULL )
    {
        return;
    }

    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_pScoreboardDialog = NULL;
}

//=========================================================================
// This is the state transition that occurs after a level has completed loading
// and we want to enter the game. It is the responsibility of this UpdateLoadSync
// function to make sure all the ducks are lined up and ready to go prior to jumping
// in to the game.

void state_mgr::EnterServerSync( void )
{
}

//=========================================================================

void state_mgr::UpdateServerSync( void )
{
    server_state ServerState = g_NetworkMgr.GetServerObject().GetState();
    if( ServerState == STATE_SERVER_INGAME )
    {
        SetState( SM_PLAYING_GAME );
    }
    else if( ServerState != STATE_SERVER_SYNC )
    {
        SetState( SM_SERVER_COOLDOWN );
    }
}

//=========================================================================

void state_mgr::ExitServerSync( void )
{
}

//=========================================================================
// This is the state transition that occurs after a level has completed loading
// and we want to enter the game. It is the responsibility of this UpdateLoadSync
// function to make sure all the ducks are lined up and ready to go prior to jumping
// in to the game.

void state_mgr::EnterClientSync( void )
{
}

//=========================================================================

void state_mgr::UpdateClientSync( void )
{
    client_state ClientState = g_NetworkMgr.GetClientObject().GetState();
    if( ClientState == STATE_CLIENT_INGAME )
    {
        SetState( SM_PLAYING_GAME );
    }
    else if( ClientState != STATE_CLIENT_SYNC )
    {
        SetState( SM_MULTI_PLAYER_LOAD_MISSION );
    }
}

//=========================================================================

void state_mgr::ExitClientSync( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMain( void )
{
    // Create the main pause dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 136, DIALOG_TOP, 376, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "pause main", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );

    // Pause Audio
    g_AudioMgr.PauseAll();
}

//=========================================================================

void state_mgr::UpdatePauseMain( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PAUSE_MENU_RESUME:
                    {
                        // Return to game.
                        SetState( SM_END_PAUSE );
                    }
                    break;

                    case IDC_PAUSE_MENU_QUIT:
                    {
                        // Exit from game to main menu.
                        SetState( SM_EXIT_GAME );
                    }
                    break;

                    case IDC_PAUSE_MENU_OPTIONS:
                    {
                        // Edit profile.
                        g_StateMgr.InitPendingProfile( 0 );
                        SetState( SM_PAUSE_OPTIONS );
                    }
                    break;

                    case IDC_PAUSE_MENU_SETTINGS:
                    {
                        // Edit settings.
                        InitPendingSettings();
                        SetState( SM_PAUSE_SETTINGS );
                    }
                    break;
                }
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMain( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOptions( void )
{
    // Pause options screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // configure the dialog for MP options-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_PAUSE, FALSE );
}

//=========================================================================

void state_mgr::UpdatePauseOptions( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_PAUSE_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_PAUSE_MAIN_MENU );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_PAUSE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOptions( void )
{
}

//=========================================================================

void state_mgr::EnterPauseControls( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // enable background filter
    ((dlg_profile_controls*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseControls( void )
{
    UpdateProfileControlsMenu( SM_PAUSE_OPTIONS, FALSE );
}

//=========================================================================

void state_mgr::ExitPauseControls( void )
{
}

//=========================================================================

void state_mgr::EnterPauseSettings ( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(16, DIALOG_TOP, 496, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseSettings( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_SETTINGS_AUDIO:
                    {
                        SetState( SM_PAUSE_AUDIO );
                    }
                    break;

                    case IDC_SETTINGS_HEADSET:
                    {
                        SetState( SM_PAUSE_HEADSET );
                    }
                    break;

                    case IDC_SETTINGS_GRAPHICS:
                    {
                        SetState( SM_PAUSE_GRAPHICS );
                    }
                    break;

                    case IDC_SETTINGS_DISPLAY:
                    {
                        SetState( SM_PAUSE_DISPLAY );
                    }
                    break;

                    case IDC_SETTINGS_UI:
                    {
                        SetState( SM_PAUSE_UI );
                    }
                    break;

                    case IDC_SETTINGS_LANGUAGE:
                    {
                        SetState( SM_PAUSE_LANGUAGE );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseSettings  ( void )
{
}

//=========================================================================

void state_mgr::EnterPauseAudio( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "audio settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_audio_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseAudio( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseAudio( void )
{
}

//=========================================================================

void state_mgr::EnterPauseHeadset ( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "headset", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_headset*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseHeadset ( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseHeadset ( void )
{
}

//=========================================================================

void state_mgr::EnterPauseGraphics( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "graphics settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_graphics_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseGraphics( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                if( m_CurrentDialog->GetControl() == IDC_GRAPHICS_ADVANCED )
                {
                    SetState( SM_PAUSE_GRAPHICS_ADVANCED );
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseGraphics( void )
{
}

//=========================================================================

void state_mgr::EnterPauseGraphicsAdvanced( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID,
                                           "graphics advanced settings",
                                           mainarea,
                                           NULL,
                                           ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_graphics_advanced_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseGraphicsAdvanced( void )
{
    if( m_CurrentDialog != NULL )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_GRAPHICS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseGraphicsAdvanced( void )
{
}

//=========================================================================

void state_mgr::EnterPauseDisplay( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "display settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_display_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseDisplay( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseDisplay( void )
{
}

//=========================================================================

void state_mgr::EnterPauseUI( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "ui settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_ui_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseUI( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseUI( void )
{
}

//=========================================================================

void state_mgr::EnterPauseLanguage( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "language settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_language_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseLanguage( void )
{
    if( m_CurrentDialog )
    {
        u32 const DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseLanguage( void )
{
}

//=========================================================================

void state_mgr::EnterPauseSettingsSelect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdatePauseSettingsSelect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // save settings was successful
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // save failed or was aborted
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseSettingsSelect( void )
{
}

//=========================================================================

void state_mgr::EnterPauseSaveDataSaveSelect ( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_profile_select*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseSaveDataSaveSelect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // continue without saving
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_PAUSE_SAVE_DATA_RESELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseSaveDataSaveSelect  ( void )
{
}

//=========================================================================

void state_mgr::EnterPauseSaveDataReselect ( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdatePauseSaveDataReselect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                // Go to the campaign menu.
                SetState( SM_PAUSE_MAIN_MENU );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // Go back to the profile select menu.
                SetState( SM_PAUSE_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseSaveDataReselect  ( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMP( void )
{
    // Create the main pause dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 136, DIALOG_TOP, 376, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "pause multiplayer", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
}

//=========================================================================

void state_mgr::UpdatePauseMP( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PAUSE_MP_QUIT:
                    {
                        // Exit from game to main menu.
                        SetState( SM_EXIT_GAME );
                    }
                    break;

                    case IDC_PAUSE_MP_SCORE:
                    {
                        SetState( SM_PAUSE_MP_SCORE );
                    }
                    break;

                    case IDC_PAUSE_MP_OPTIONS:
                    {
                        // Initialize pending profile for the player that paused the game.
                        g_StateMgr.InitPendingProfile( GetActiveControllerID() );
                        SetState( SM_PAUSE_MP_OPTIONS );
                    }
                    break;

                    case IDC_PAUSE_MP_SETTINGS:
                    {
                        // Edit settings.
                        InitPendingSettings();
                        SetState( SM_PAUSE_MP_SETTINGS );
                    }
                    break;
                }
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMP( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPSettings( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(16, DIALOG_TOP, 496, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPSettings( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_SETTINGS_AUDIO:
                    {
                        SetState( SM_PAUSE_MP_AUDIO );
                    }
                    break;

                    case IDC_SETTINGS_HEADSET:
                    {
                        SetState( SM_PAUSE_MP_HEADSET );
                    }
                    break;

                    case IDC_SETTINGS_GRAPHICS:
                    {
                        SetState( SM_PAUSE_MP_GRAPHICS );
                    }
                    break;

                    case IDC_SETTINGS_DISPLAY:
                    {
                        SetState( SM_PAUSE_MP_DISPLAY );
                    }
                    break;

                    case IDC_SETTINGS_UI:
                    {
                        SetState( SM_PAUSE_MP_UI );
                    }
                    break;

                    case IDC_SETTINGS_LANGUAGE:
                    {
                        SetState( SM_PAUSE_MP_LANGUAGE );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPSettings( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPAudio( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "audio settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_audio_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPAudio( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPAudio( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPHeadset ( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "headset", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_headset*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPHeadset ( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPHeadset ( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPGraphics( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "graphics settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_graphics_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPGraphics( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP_SETTINGS );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                if( m_CurrentDialog->GetControl() == IDC_GRAPHICS_ADVANCED )
                {
                    SetState( SM_PAUSE_MP_GRAPHICS_ADVANCED );
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPGraphics( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPGraphicsAdvanced( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID,
                                           "graphics advanced settings",
                                           mainarea,
                                           NULL,
                                           ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_graphics_advanced_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPGraphicsAdvanced( void )
{
    if( m_CurrentDialog != NULL )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP_GRAPHICS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPGraphicsAdvanced( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPDisplay( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "display settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_display_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPDisplay( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPDisplay( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPUI( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "ui settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_ui_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPUI( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPUI( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPLanguage( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "language settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_language_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPLanguage( void )
{
    if( m_CurrentDialog )
    {
        u32 const DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_MP_SETTINGS_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPLanguage( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPSettingsSelect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdatePauseMPSettingsSelect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // save settings was successful
                SetState( SM_PAUSE_MP );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // save failed or was aborted
                SetState( SM_PAUSE_MP );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPSettingsSelect( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPOptions( void )
{
    // Pause options screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // configure the dialog for MP options-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_PAUSE, FALSE );
}

//=========================================================================

void state_mgr::UpdatePauseMPOptions( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_PAUSE_MP_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_PAUSE_MP );
                    }
                    break;
                }
            }
            break;

        case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_PAUSE_MP );
            }
            break;

        case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_MP );
            }
            break;

        case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_PAUSE_MP_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPOptions( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPControls( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // enable background filter
    ((dlg_profile_controls*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPControls( void )
{
    UpdateProfileControlsMenu( SM_PAUSE_MP_OPTIONS, FALSE );
}

//=========================================================================

void state_mgr::ExitPauseMPControls( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPSaveDataSaveSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_profile_select*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseMPSaveDataSaveSelect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                SetState( SM_PAUSE_MP );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // continue without saving
                SetState( SM_PAUSE_MP );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_PAUSE_MP_SAVE_DATA_RESELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPSaveDataSaveSelect( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPSaveDataReselect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdatePauseMPSaveDataReselect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                SetState( SM_PAUSE_MP );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                SetState( SM_PAUSE_MP_PROFILE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPSaveDataReselect( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnline( void )
{
    // Create the main pause dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 86, DIALOG_TOP, 426, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "pause online", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );

    // Pause Audio
//  g_AudioMgr.PauseAll();
}

//=========================================================================

void state_mgr::UpdatePauseOnline( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PAUSE_ONLINE_SWITCH_TEAM:
                    case IDC_PAUSE_ONLINE_SUICIDE:
                    {
                        // exit pause and rejoin game
                        SetPaused( FALSE, GetActiveControllerID() );
                    }
                    break;

                    case IDC_PAUSE_ONLINE_VOTE_MAP:
                    {
                        SetState( SM_PAUSE_ONLINE_VOTE_MAP );
                    }
                    break;

                    case IDC_PAUSE_ONLINE_VOTE_KICK:
                    {
                        SetState( SM_PAUSE_ONLINE_VOTE_KICK );
                    }
                    break;


                    case IDC_PAUSE_ONLINE_PLAYERS:
                    {
                        SetState( SM_PAUSE_ONLINE_PLAYERS );
                    }
                    break;

                    case IDC_PAUSE_ONLINE_OPTIONS:
                    {
                        // init pending profile
                        g_StateMgr.InitPendingProfile( 0 );
                        SetState( SM_PAUSE_ONLINE_OPTIONS );
                    }  
                    break;

                    case IDC_PAUSE_ONLINE_SETTINGS:
                    {
                        // init pending settings
                        InitPendingSettings();
                        SetState( SM_PAUSE_ONLINE_SETTINGS );
                    }
                    break;

                    case IDC_PAUSE_ONLINE_CONFIG:
                    {
                        SetState( SM_PAUSE_ONLINE_SERVER_CONFIG );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_DELETE:
            {
                // quit game
                g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
                SetPaused( FALSE, -1 );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnline( void )
{
    // Resume Audio
    g_AudioMgr.ResumeAll();
}

//=========================================================================

void state_mgr::EnterPauseOnlineVoteMap( void )
{
    // Create vote map menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(81, DIALOG_TOP, 431, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "vote map", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdatePauseOnlineVoteMap( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Initiated vote for map change - exit pause and rejoin game
                SetPaused( FALSE, GetActiveControllerID() );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineVoteMap( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineVoteKick( void )
{
    // Create vote kick menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(51, DIALOG_TOP, 461, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "vote kick", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdatePauseOnlineVoteKick( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Initiated vote kick player - exit pause and rejoin game
                SetPaused( FALSE, GetActiveControllerID() );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineVoteKick( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlinePlayers( void )
{
    // Create players menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "players list", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // enable background filter
    ((dlg_players*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlinePlayers( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE );
            }
            break;

        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlinePlayers( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineOptions( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(s_ProfileOptLeft, DIALOG_TOP, s_ProfileOptRight, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile options", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // configure the dialog for MP options-
    ((dlg_profile_options*)m_CurrentDialog)->Configure( PROFILE_OPTIONS_PAUSE, FALSE );
}

//=========================================================================

void state_mgr::UpdatePauseOnlineOptions( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_PROFILE_OPTIONS_CONTROLS:
                    {
                        SetState( SM_PAUSE_ONLINE_CONTROLS );
                    }
                    break;

                    case IDC_PROFILE_OPTIONS_ACCEPT:
                    {
                        SetState( SM_PAUSE_MAIN_MENU );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_PAUSE_ONLINE );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                // save failed - select a profile to save to
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineOptions( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineControls( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile controls", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // enable background filter
    ((dlg_profile_controls*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineControls( void )
{
    UpdateProfileControlsMenu( SM_PAUSE_ONLINE_OPTIONS, FALSE );
}

//=========================================================================

void state_mgr::ExitPauseOnlineControls( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineSettings( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(16, DIALOG_TOP, 496, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineSettings( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_SETTINGS_AUDIO:
                    {
                        SetState( SM_PAUSE_ONLINE_AUDIO );
                    }
                    break;

                    case IDC_SETTINGS_HEADSET:
                    {
                        SetState( SM_PAUSE_ONLINE_HEADSET );
                    }
                    break;

                    case IDC_SETTINGS_GRAPHICS:
                    {
                        SetState( SM_PAUSE_ONLINE_GRAPHICS );
                    }
                    break;

                    case IDC_SETTINGS_DISPLAY:
                    {
                        SetState( SM_PAUSE_ONLINE_DISPLAY );
                    }
                    break;

                    case IDC_SETTINGS_UI:
                    {
                        SetState( SM_PAUSE_ONLINE_UI );
                    }
                    break;

                    case IDC_SETTINGS_LANGUAGE:
                    {
                        SetState( SM_PAUSE_ONLINE_LANGUAGE );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;

        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineSettings( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineAudio( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "audio settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_audio_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineAudio( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineAudio( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineHeadset ( void )
{
    //  create options main menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "headset", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_headset*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineHeadset ( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineHeadset ( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineGraphics( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "graphics settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_graphics_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineGraphics( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                if( m_CurrentDialog->GetControl() == IDC_GRAPHICS_ADVANCED )
                {
                    SetState( SM_PAUSE_ONLINE_GRAPHICS_ADVANCED );
                }
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineGraphics( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineGraphicsAdvanced( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID,
                                           "graphics advanced settings",
                                           mainarea,
                                           NULL,
                                           ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_graphics_advanced_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineGraphicsAdvanced( void )
{
    if( m_CurrentDialog != NULL )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_GRAPHICS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineGraphicsAdvanced( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineDisplay( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "display settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_display_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineDisplay( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineDisplay( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineUI( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "ui settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_ui_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineUI( void )
{
    if( m_CurrentDialog )
    {
        switch( m_CurrentDialog->GetState() )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineUI( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineLanguage( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "language settings", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_language_settings*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineLanguage( void )
{
    if( m_CurrentDialog )
    {
        u32 const DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SETTINGS );
            }
            break;

            case DIALOG_STATE_SAVE_DATA_ERROR:
            {
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineLanguage( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineSaveSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    ((dlg_profile_select*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdatePauseOnlineSaveSelect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                SetState( SM_PAUSE_ONLINE );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                // continue without saving
                SetState( SM_PAUSE_ONLINE );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_PAUSE_ONLINE_SAVE_DATA_RESELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineSaveSelect( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineSaveDataReselect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdatePauseOnlineSaveDataReselect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                SetState( SM_PAUSE_ONLINE );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // Go back to the profile select menu.
                SetState( SM_PAUSE_ONLINE_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineSaveDataReselect( void )
{
}

//=========================================================================

void state_mgr::EnterPauseServerConfig( void )
{
    // Create the server config dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 86, DIALOG_TOP, 426, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "server config", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
}

//=========================================================================

void state_mgr::UpdatePauseServerConfig( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                s32 Control = m_CurrentDialog->GetControl();

                switch( Control )
                {
                    case IDC_SERVER_CONFIG_CHANGE_MAP:
                    {
                        SetState( SM_PAUSE_ONLINE_CHANGE_MAP );
                    }
                    break;

                    case IDC_SERVER_CONFIG_KICK_PLAYER:
                    {
                        SetState( SM_PAUSE_ONLINE_KICK_PLAYER );
                    }
                    break;

                    case IDC_SERVER_CONFIG_CHANGE_TEAM:
                    {
                        SetState( SM_PAUSE_ONLINE_TEAM_CHANGE );
                    }
                    break;

                    case IDC_SERVER_CONFIG_RECONFIGURE:
                    {
                        SetState( SM_PAUSE_ONLINE_RECONFIG );
                    }
                    break;

                    case IDC_SERVER_CONFIG_SHUTDOWN:
                    {
                        g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
                        // Shutdown the server.
                    }
                    break;

                    case IDC_SERVER_CONFIG_RESTART_MAP:
                    {
                        g_ActiveConfig.SetExitReason( GAME_EXIT_RELOAD_LEVEL );
                    }
                    break;

                    default:
                    {
                        ASSERT( FALSE );
                    }
                    break;
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseServerConfig( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineChangeMap( void )
{
    // Create change map dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(81, DIALOG_TOP, 431, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "change map", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdatePauseOnlineChangeMap( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Change the map
                // anything to set here?
                SetState( SM_PAUSE_ONLINE_SERVER_CONFIG );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SERVER_CONFIG );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineChangeMap( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineKickPlayer( void )
{
    // Create vote kick menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(51, DIALOG_TOP, 461, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "kick player", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdatePauseOnlineKickPlayer( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Kicked a player
                // anything to set here?
                SetState( SM_PAUSE_ONLINE_SERVER_CONFIG );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SERVER_CONFIG );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineKickPlayer( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineTeamChange( void )
{
    //  Create level select screen 
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "team change", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdatePauseOnlineTeamChange( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SERVER_CONFIG );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineTeamChange( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineReconfig( void )
{
    // Create online host menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(6, DIALOG_TOP, 506, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "online host", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdatePauseOnlineReconfig( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                g_PendingConfig.Commit();
                g_ActiveConfig.SetExitReason( GAME_EXIT_RELOAD_LEVEL );
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                SetState( SM_END_PAUSE );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetState( SM_PAUSE_ONLINE_SERVER_CONFIG );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineReconfig( void )
{
}

//=========================================================================

void state_mgr::EnterPauseOnlineSaveSettings( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdatePauseOnlineSaveSettings( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                SetState( SM_END_PAUSE );
                // Save profile was successful.
                // TODO: Restart the game with the new settings.
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // TODO: Restart the game with the new settings.
                SetState( SM_END_PAUSE );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseOnlineSaveSettings( void )
{
}

//=========================================================================

void state_mgr::EnterPauseMPScore( void )
{
    // Create score dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    
    // determine which dialog to create
    const game_score& ScoreData = GameMgr.GetScore();
    
    s32 playerCount = ScoreData.NPlayers;

    if( playerCount > 16 )
    {
        irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
        m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "big leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
        m_LeaderboardID = SM_BIG_LEADERBOARD;
    }
    else
    {
        if( ScoreData.IsTeamBased ) 
        {
            irect mainarea( 8, DIALOG_TOP, 504, ui_viewport::CONTENT_HEIGHT - 36 );
            m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "team leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
            m_LeaderboardID = SM_TEAM_LEADERBOARD;
        }
        else
        {        
            irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
            m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
            m_LeaderboardID = SM_LEADERBOARD;
        }
    }
}

//=========================================================================

void state_mgr::UpdatePauseMPScore( void )
{
    const game_score& ScoreData = GameMgr.GetScore();
    
    s32 playerCount = ScoreData.NPlayers;

    // check if we need to switch the dialog
    switch( m_LeaderboardID )
    {
        case SM_BIG_LEADERBOARD:
        {
            if( playerCount < 17 )
            {
                g_UiMgr->EndDialog( g_UiUserID, TRUE );
                if( ScoreData.IsTeamBased ) 
                {
                    irect mainarea( 8, DIALOG_TOP, 504, ui_viewport::CONTENT_HEIGHT - 36 );
                    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "team leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
                    m_LeaderboardID = SM_TEAM_LEADERBOARD;
                }
                else
                {        
                    irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
                    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
                    m_LeaderboardID = SM_LEADERBOARD;
                }
            }
        }
        break;
        
        default:
        {
            if( playerCount > 16 )
            {
                g_UiMgr->EndDialog( g_UiUserID, TRUE );
                irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
                m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "big leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
                m_LeaderboardID = SM_BIG_LEADERBOARD;
            }
        }
        break;
    }

    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                if( g_NetworkMgr.IsOnline() )
                {
                    // goto online pause main menu
                    SetState( SM_PAUSE_ONLINE );
                }
                else
                {
                    // goto multi player pause main menu
                    SetState( SM_PAUSE_MP );
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                SetPaused( FALSE, -1 );
            }
            break;

            case DIALOG_STATE_EXIT:                
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
                SetPaused( FALSE, -1 );
            }
            break;

            default:
            {
            }    
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitPauseMPScore( void )
{
}

//=========================================================================

void state_mgr::EnterEndPause( void )
{
    // gracefully exit from the pause menu (kill all dialogs)
    g_UiMgr->EndUsersDialogs( g_UiUserID );

    irect mainarea( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );

    //restore UI control to all controllers
    g_UiMgr->SetUserController( g_UiUserID, -1 );

    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "end pause", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
}

//=========================================================================

void state_mgr::UpdateEndPause( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                SetState( SM_PLAYING_GAME );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitEndPause( void )
{
    // Kill the pause menu and return to the game
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;
    // Disable ui
    g_UiMgr->EnableUser( g_UiUserID, FALSE );

    // Resume Audio
    g_AudioMgr.ResumeAll();

    // Set unpaused
    g_Input.SuppressFeedback( FALSE ); // SetPaused(xbool) is not always called!
    m_bIsPaused = FALSE;
    m_bDoSystemError = FALSE;
    m_PopUp = NULL;

}

//=========================================================================

void state_mgr::EnterExitGame( void )
{
    // gracefully exit from the pause menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );
    g_UiMgr->EnableUser( g_UiUserID, TRUE );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "end pause", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );
    m_Exit = TRUE;
    m_TimeoutTime = 0.25f;
}

//=========================================================================

void state_mgr::UpdateExitGame( void )
{
    // Do not tear down state while a save data request owns captured state.
#if (!CONFIG_IS_DEMO)
    if( g_SaveDataMgr.IsBusy() )
    {
        LOG_MESSAGE( "state_mgr::UpdateExitGame", "Delaying game exit due to a save data operation." );
        return;
    }
#endif

    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                SetState( SM_IDLE );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitExitGame( void )
{
    // Kill the end pause dialog and exit the game
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;
    // Set unpaused
    m_bIsPaused = FALSE;
    m_Exit      = FALSE;
    g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
    m_bDoSystemError = FALSE;
    m_PopUp = NULL;

    for( s32 i=0; i< SM_MAX_PLAYERS; i++ )
    {
        g_Input.Feedback( 0.0f, 0.0f, i );
    }
}

//=========================================================================

void state_mgr::EnterPostGame( void )
{
    for( s32 i=0; i< SM_MAX_PLAYERS; i++ )
    {
        g_Input.Feedback( 0.0f, 0.0f, i );
    }

    g_SaveDataMgr.ClearCachedProfiles();
    g_UiMgr->EnableUser( g_UiUserID, TRUE );
}

//=========================================================================

void state_mgr::UpdatePostGame( void )
{
    //=============================================================================
    // The ExitReason() is used to drive the starting point of the front end ui and
    // state machine. When we first enter the game loop, the GAME_EXIT_CONTINUE code is
    // set. This tells us that we've never been anywhere so far. When RunGame() exits,
    // (see below), the reason for the exit drives where we go next. The target is 
    // determined at this point.
    //
    // Note: Other networked players may still be connected to the game at this point.
    // In that case, we should only be going through the ADVANCE_LEVEL and RELOAD_LEVEL
    // exit paths. Otherwise, everyone should be disconnected.

    // handle any system errors
    if( m_bDoSystemError )
        return;

    xbool bClearControllers = FALSE;

    switch( g_ActiveConfig.GetExitReason() )
    {
        case GAME_EXIT_ADVANCE_LEVEL:
        {
        #if CONFIG_IS_DEMO
            g_StateMgr.SetState( SM_DEMO_EXIT );
        #else 
            // set the movie play flag to false!
            m_bPlayMovie = FALSE;
            g_StateMgr.SetState( SM_ADVANCE_LEVEL );
        #endif
        }
        break;
    
        case GAME_EXIT_RELOAD_CHECKPOINT:
        {
            // Tell the networkmgr we're about to re-enter the game. Then
            // tell the statemgr we're going to do that too.
            g_NetworkMgr.ReenterGame();
            SetState( SM_RELOAD_CHECKPOINT );
        }
        break;

        case GAME_EXIT_RELOAD_LEVEL:
        {
            g_StateMgr.SetState( SM_ADVANCE_LEVEL );
        }
        break;

        case GAME_EXIT_PLAYER_DROPPED:
        case GAME_EXIT_PLAYER_KICKED:
        case GAME_EXIT_SESSION_ENDED:
        case GAME_EXIT_CONNECTION_LOST:
        case GAME_EXIT_SERVER_BUSY:
        case GAME_EXIT_SERVER_SHUTDOWN:
        case GAME_EXIT_NETWORK_DOWN:
        case GAME_EXIT_DUPLICATE_LOGIN:
        {
            g_StateMgr.SetState( SM_REPORT_ERROR );
            bClearControllers = TRUE;        
        }
        break;

        case GAME_EXIT_GAME_COMPLETE:
        {
            g_StateMgr.SetState( SM_GAME_OVER );
            bClearControllers = TRUE;        
        }
        break;

        case GAME_EXIT_PLAYER_QUIT:
        {
            g_RscMgr.Load( PRELOAD_FILE("AUDIO/DX_FRONTEND.AUDIOPKG"    ) );
            g_RscMgr.Load( PRELOAD_FILE("AUDIO/SFX_FRONTEND.AUDIOPKG"   ) );
            g_RscMgr.Load( PRELOAD_FILE("AUDIO/MUSIC_FRONTEND.AUDIOPKG" ) );
            g_RscMgr.Load( PRELOAD_FILE("AUDIO/DREAMLAND.AUDIOPKG" ) );
            // initialize audio volumes from global settings
            global_settings& Settings = g_StateMgr.GetActiveSettings();
            Settings.CommitAudio();
            bClearControllers = TRUE;        

            EnableBackgroundMovie();

            if( g_NetworkMgr.IsOnline() )
            {
                SetState( SM_FINAL_SCORE );
            }
            else
            {
            #if CONFIG_IS_DEMO
                SetState( SM_DEMO_EXIT );
            #else
                if( GameMgr.GetGameType() == GAME_CAMPAIGN )
                {
                    // check if profile needs to be saved
                    SetState( SM_GAME_EXIT_PROMPT_FOR_SAVE );
                }
                else
                {
                    // split screen justs return to the split screen menu
                    SetState( SM_MULTI_PLAYER_MENU );
                }
            #endif
            }
        }
        break;

        case GAME_EXIT_BAD_PASSWORD:
        case GAME_EXIT_SERVER_FULL:
        case GAME_EXIT_CLIENT_BANNED:
        case GAME_EXIT_CANNOT_CONNECT:
        case GAME_EXIT_INVALID_MISSION:
        case GAME_EXIT_INVALID_CAMPAIGN_MISSION:
        case GAME_EXIT_LOGIN_REFUSED:
        {
            SetState( SM_REPORT_ERROR );
            bClearControllers = TRUE;        
        }
        break;


        default:
        {
            // NOTE: DO NOT remove this assert. If we come out of the game with an unexpected error code,
            // then we need to handle it gracefully and understand WHY this error code appeared.        
            ASSERT(FALSE);
        } 
        break;
    }

    if( bClearControllers )
    {
        // clear the locked controller ID
        SetActiveControllerID(-1);

        // clear the controller requests
        for(s32 p=0; p < SM_MAX_PLAYERS; p++)
        {
            g_StateMgr.SetControllerRequested(p, FALSE);
            g_GameInput.ClearPlayerDevice( p );
        }
    }
}

//=========================================================================

void state_mgr::ExitPostGame( void )
{
}

//=========================================================================

void state_mgr::EnterAutosaveMenu( void )
{
    g_Input.SuppressFeedback( TRUE );

    // pause the ingame audio
    g_AudioMgr.PauseAll();

    // set up the screen bars off screen
    irect initSize( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );
    initSize.Translate( 0, SAFE_ZONE );

    g_UiMgr->SetScreenSize(initSize);       
    g_UiMgr->EnableUser( g_UiUserID, TRUE );

    // set controller states
    //g_UiMgr->SetUserController( g_UiUserID, PausingController );
    //SetActiveControllerID( PausingController );

    // intialize the pending profile
    InitPendingProfile(0);

    // enable save data dialogs

    // Create the main pause dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 106, DIALOG_TOP, 406, DIALOG_BOTTOM );

    if( x_GetLocale() != XL_LANG_ENGLISH )
    {
        mainarea.l = 71;
        mainarea.r = 441;
    }

    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "autosave", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );

    // Pause Audio
    g_AudioMgr.PauseAll();
    // pause game
    m_bIsPaused = TRUE;
}

//=========================================================================

void state_mgr::UpdateAutosaveMenu( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Pick a profile to save to.
                SetState( SM_AUTOSAVE_PROFILE_RESELECT );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // Continue without saving for now.
                g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), TRUE );
                // Update the changes in the profile.
                g_StateMgr.ActivatePendingProfile();
                // Return to game.
                SetState( SM_END_AUTOSAVE );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitAutosaveMenu( void )
{
}

//=========================================================================

void state_mgr::EnterAutosaveProfileReselect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
    // enable background filter
    ((dlg_profile_select*)m_CurrentDialog)->EnableBlackout();
}

//=========================================================================

void state_mgr::UpdateAutosaveProfileReselect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            // selected profile was overwritten
            case DIALOG_STATE_SELECT: 
            {
                // return to game
                SetState( SM_END_AUTOSAVE );
            }
            break;

            // continue without saving
            case DIALOG_STATE_ACTIVATE:     
            {               
                // return to game
                SetState( SM_END_AUTOSAVE );
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // create a new profile
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitAutosaveProfileReselect( void )
{
}

//=========================================================================

void state_mgr::EnterEndAutosave( void )
{
    // gracefully exit from the pause menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( -75, DIALOG_TOP, ui_viewport::CONTENT_WIDTH + 75, DIALOG_BOTTOM );

    //restore UI control to all controllers
    g_UiMgr->SetUserController( g_UiUserID, -1 );

    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "end pause", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );

    g_Input.SuppressFeedback( FALSE );
}

//=========================================================================

void state_mgr::UpdateEndAutosave( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                SetState( SM_PLAYING_GAME );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitEndAutosave( void )
{
    // Kill the pause menu and return to the game
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;
    // Disable ui
    g_UiMgr->EnableUser( g_UiUserID, FALSE );

    // Resume Audio
    g_AudioMgr.ResumeAll();

    // Set unpaused
    m_bIsPaused = FALSE;
    m_bDoSystemError = FALSE;
    m_PopUp = NULL;

    // reset save in progress flag
    m_bAutosaveInProgress = FALSE;
}

//=========================================================================

void state_mgr::EnterFinalScore( void )
{
    // Create score dialog
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    
    // determine which dialog to create
    const game_score& ScoreData = GameMgr.GetScore();
    
    s32 playerCount = ScoreData.NPlayers;

    if( playerCount > 16 )
    {
        irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
        m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "big leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
        m_LeaderboardID = SM_BIG_LEADERBOARD;
        // configure for final score
        ((dlg_big_leaderboard*)m_CurrentDialog)->Configure( LEADERBOARD_FINAL );
    }
    else
    {
        if( ScoreData.IsTeamBased ) 
        {
            irect mainarea( 8, DIALOG_TOP, 504, ui_viewport::CONTENT_HEIGHT - 36 );
            m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "team leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
            m_LeaderboardID = SM_TEAM_LEADERBOARD;
            // configure for final score
            ((dlg_team_leaderboard*)m_CurrentDialog)->Configure( LEADERBOARD_FINAL );
        }
        else
        {        
            irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
            m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
            m_LeaderboardID = SM_LEADERBOARD;
            // configure for final score
            ((dlg_leaderboard*)m_CurrentDialog)->Configure( LEADERBOARD_FINAL );
        }
    }
}

//=========================================================================

void state_mgr::UpdateFinalScore( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_ACTIVE:
            {
            }
            break;

            case DIALOG_STATE_BACK:
            case DIALOG_STATE_SELECT:
            {
                // check if profile needs to be saved
                SetState( SM_GAME_EXIT_PROMPT_FOR_SAVE );
            }
            break;
            
            default:
            {
                ASSERT(FALSE);
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitFinalScore( void )
{
}

//=========================================================================

void state_mgr::EnterAdvanceLevel( void )
{
}

//=========================================================================

void state_mgr::UpdateAdvanceLevel( void )
{
    xbool IsReloading;

    exit_reason Reason = g_ActiveConfig.GetExitReason();

    if( (Reason!=GAME_EXIT_RELOAD_LEVEL) && 
        (Reason!=GAME_EXIT_ADVANCE_LEVEL) )
    {
        g_StateMgr.SetState( SM_REPORT_ERROR );
        return;
    }

    IsReloading= (Reason == GAME_EXIT_RELOAD_LEVEL);

    //
    // We're playing campaign. This means we always go through the level list in sequence
    //
    if( g_PendingConfig.GetGameTypeID() == GAME_CAMPAIGN )
    {
        // Reset the death flag
        player::s_bPlayerDied = FALSE;

        const map_entry* pEntry;
        s32 OriginalIndex;

        OriginalIndex = m_LevelIndex;

        while( TRUE )
        {
            m_LevelIndex++;

            if( m_LevelIndex >= g_MapList.GetCount() )
            {
                m_LevelIndex = 0;
            }
            pEntry = g_MapList.GetByIndex( m_LevelIndex );
            ASSERT( pEntry );


            // If we hit a different map index, or we have gone through the list looking
            // for a new one and not found any others loaded, then we quit looking.
            xbool IsSameLevelId     = ( pEntry->GetMapID() == g_PendingConfig.GetLevelID() );
            xbool IsSameGameType    = (pEntry->GetGameType() == g_PendingConfig.GetGameTypeID() );
            xbool HasWrapped        = (m_LevelIndex == OriginalIndex );
            xbool IsPresent         = pEntry->IsAvailable();
            if( (!IsSameLevelId && IsSameGameType && IsPresent) || HasWrapped )
            {
                g_PendingConfig.SetLevelID( pEntry->GetMapID() );
                break;
            }
        }
        LOG_MESSAGE("state_mgr::UpdateAdvanceLevel","Campaign Default map cycle: GameType:%d, New Map:%s", g_PendingConfig.GetGameTypeID(), (const char*)xstring(g_PendingConfig.GetLevelName()) );
        game_config::Commit();
        g_NetworkMgr.ReenterGame();
        m_bStartSaveGame = FALSE;
        g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
        SetState( SM_SINGLE_PLAYER_LOAD_MISSION );
        return;
    }

    //
    // We're following a player in to a game
    //

    //
    // We're a client. We need to wait until the RequestingMission is complete.
    //
    if( g_NetworkMgr.IsClient() )
    {
        if( g_NetworkMgr.IsRequestingMission() == FALSE )
        {
            xbool MapPresent;

            // Check for disconnect while requesting mission.
            if( g_NetworkMgr.GetClientObject().IsConnected()== FALSE )
            {
                ASSERT( g_ActiveConfig.GetExitReason()!=GAME_EXIT_CONTINUE );
                ASSERT( g_ActiveConfig.GetExitReason()!=GAME_EXIT_ADVANCE_LEVEL );
                g_StateMgr.SetState( SM_REPORT_ERROR );
                return;
            }

            MapPresent = g_PendingConfig.SetLevelID( g_ActiveConfig.GetLevelID() );

            if( !MapPresent )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_INVALID_MISSION );
                g_StateMgr.SetState( SM_REPORT_ERROR );

            }
            else
            {
                game_config::Commit();
                g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
                SetState( SM_MULTI_PLAYER_LOAD_MISSION ); // was SM_LOAD_GAME
            }
        }
        return;
    }

    // Now we know we must be a server. This is either multiplayer mode or online
    //
    // At the moment, we're not storing the game type as an index in the server configuration structure. This
    // is because we want to display a gametype. But, what happens if we have a custom game type?
    //
    //
    // Only advance the level if we didn't try to reload a map. If we do reload, it's the UI responsibility
    // to make sure that the **Pending** configuration has been updated.
    //

    s32 OriginalIndex = m_LevelIndex;
    const map_entry* pEntry = NULL;

    // If the user has changed gametypes, don't use the map cycle list.
    if( g_PendingConfig.GetGameTypeID() != g_ActiveConfig.GetGameTypeID() )
    {
        LOG_WARNING( "state_mgr::UpdateAdvanceLevel","Disabled user defined map cycling due to game type change." );
        SetUseDefaultMapCycle( TRUE );
    }

    // Are we reloading a level?
    if( IsReloading )
    {
        // Since we're reloading, let's see if the map we desire is in the current map cycle list. If it is,
        // advance our index for the map cycle list to that point.
        if( GetUseDefaultMapCycle() )
        {
            s32 NewLevelIndex;
            NewLevelIndex = m_LevelIndex;

            while( TRUE )
            {
                NewLevelIndex++;
                if( NewLevelIndex >= g_MapList.GetCount() )
                {
                    NewLevelIndex = 0;
                }
                pEntry = g_MapList.GetByIndex( NewLevelIndex );

                if( (pEntry->GetMapID() == g_PendingConfig.GetLevelID()) &&
                    (pEntry->GetGameType() == g_PendingConfig.GetGameTypeID()) )
                {
                    LOG_MESSAGE("state_mgr::UpdateAdvanceLevel","Reload, Default map cycle: Old level index:%d, New Level Index:%d", m_LevelIndex, NewLevelIndex );
                    m_LevelIndex = NewLevelIndex;
                    break;
                }
                // If we wrapped, then we did not find it in the table. This is bad. Very, Very bad.
                // so we assert.
                ASSERT( NewLevelIndex != m_LevelIndex );
            }
        }
        else
        {
            // This is similar code to above. Except we go through the user defined map redirection table.
            OriginalIndex = m_MapCycleIdx;
            s32 NewMapIndex;

            NewMapIndex = OriginalIndex;
            while( TRUE )
            {
                if( m_MapCycle[NewMapIndex] == g_PendingConfig.GetLevelID() )
                {
                    LOG_MESSAGE("state_mgr::UpdateAdvanceLevel","Reload, user map cycle: Old cycle index:%d, New cycle Index:%d", m_MapCycleIdx, NewMapIndex );
                    m_MapCycleIdx = NewMapIndex;
                    break;
                }
                NewMapIndex++;
                if( NewMapIndex >= m_MapCycleCount )
                {
                    NewMapIndex = 0;
                }
                if( NewMapIndex == OriginalIndex )
                {
                    break;
                }
            }
        }
    }
    else
    {
        //
        // Not reloading a level. We're just doing a standard advance. 
        //
        // If we have not defined a user map list, then just go through the map table in order making sure
        // that we select a new map with the same game type. We don't want to be switching from Deathmatch
        // to campaign!
        if( GetUseDefaultMapCycle() )
        {

            while( TRUE )
            {
                m_LevelIndex++;

                if( m_LevelIndex >= g_MapList.GetCount() )
                {
                    m_LevelIndex = 0;
                }
                pEntry = g_MapList.GetByIndex( m_LevelIndex );


                // If we hit a different map index, or we have gone through the list looking
                // for a new one and not found any others loaded, then we quit looking.
                xbool IsSameLevelId     = ( pEntry->GetMapID() == g_PendingConfig.GetLevelID() );
                xbool IsSameGameType    = (pEntry->GetGameType() == g_PendingConfig.GetGameTypeID() );
                xbool HasWrapped        = (m_LevelIndex == OriginalIndex );
                xbool IsPresent         = pEntry->IsAvailable();
                if( (!IsSameLevelId && IsSameGameType && IsPresent) || HasWrapped )
                {
                    g_PendingConfig.SetLevelID( pEntry->GetMapID() );
                    break;
                }
            }
            LOG_MESSAGE("state_mgr::UpdateAdvanceLevel","Default map cycle: GameType:%d, New Map:%s", g_PendingConfig.GetGameTypeID(), (const char*)xstring(g_PendingConfig.GetLevelName()) );
        }
        else
        {
            // We've defined a user map list. Now make sure that we advance properly through it.
            IncrementMapCycle();

            g_PendingConfig.SetLevelID( GetMapCycleMapID( GetMapCycleIndex() ) );
            LOG_MESSAGE("state_mgr::UpdateAdvanceLevel","User map cycle: GameType:%d, New Map:%s", g_PendingConfig.GetGameTypeID(), (const char*)xstring(g_PendingConfig.GetLevelName()) );
        }
    }

    game_config::Commit();
    // Commit any changes to the server configuration. These need to be applied now so it is properly
    // reflected when you transition a level.
    if( g_NetworkMgr.IsServer() )
    {
        ActivatePendingSettings(FALSE);
    }
    g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
    //
    // A client will enter request map, and a server will start to load the next map. 
    // Campaign should do nothing.
    //
    g_NetworkMgr.ReenterGame();
    //
    // Start to load next level
    //
    SetState( SM_MULTI_PLAYER_LOAD_MISSION );
}

//=========================================================================

void state_mgr::ExitAdvanceLevel( void )
{
    g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
}

//=========================================================================

void state_mgr::EnterReloadCheckpoint( void )
{
    m_bStartSaveGame = FALSE;
}

//=========================================================================

void state_mgr::UpdateReloadCheckpoint( void )
{
    SetState( SM_SINGLE_PLAYER_LOAD_MISSION );
}

//=========================================================================

void state_mgr::ExitReloadCheckpoint( void )
{
}

//=========================================================================
// These states display the score table between online level loads.
//
void state_mgr::EnterMultiPlayerLoadMission( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    m_CurrentDialog = NULL;
    g_UiMgr->EnableUser( g_UiUserID, TRUE );
    ASSERT( m_pLeaderboardDialog == NULL );
    //
    // Create score dialog
    //

    // determine which dialog to create
    const game_score& ScoreData = GameMgr.GetScore();

    m_Timeout = 0.25f;
    s32 playerCount = ScoreData.NPlayers;

    if( playerCount > 16 )
    {         
        irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
        m_pLeaderboardDialog = g_UiMgr->OpenDialog( g_UiUserID, "big leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
        m_LeaderboardID = SM_BIG_LEADERBOARD;
        ((dlg_big_leaderboard*)m_pLeaderboardDialog)->Configure( LEADERBOARD_INTERLEVEL );
    }
    else
    {
        if( ScoreData.IsTeamBased ) 
        {           
            irect mainarea( 8, DIALOG_TOP, 504, ui_viewport::CONTENT_HEIGHT - 36 );
            m_pLeaderboardDialog = g_UiMgr->OpenDialog( g_UiUserID, "team leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
            m_LeaderboardID = SM_TEAM_LEADERBOARD;
            // configure for final score
            ((dlg_team_leaderboard*)m_pLeaderboardDialog)->Configure( LEADERBOARD_INTERLEVEL );
        }
        else
        {                    
            irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM );
            m_pLeaderboardDialog = g_UiMgr->OpenDialog( g_UiUserID, "leaderboard", mainarea, NULL, ui_win::WF_VISIBLE );
            m_LeaderboardID = SM_LEADERBOARD;
            // configure for final score
            ((dlg_leaderboard*)m_pLeaderboardDialog)->Configure( LEADERBOARD_INTERLEVEL );
        }
    }

    // 
    // Create the level description dialog
    //
    // prepare for loading the level
    ASSERT( m_CurrentDialog == NULL );

    // Create a level description dialog
    irect mainarea(8, DIALOG_TOP, 504, DIALOG_BOTTOM);
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "level desc", mainarea, NULL, ui_win::WF_VISIBLE );
    ((dlg_level_desc*)m_CurrentDialog)->Configure( s_FirstMap ? LEVEL_DESC_INITIAL : LEVEL_DESC_INTERLEVEL );
    g_UiMgr->EnableBackground( FALSE );

    //
    // The description dialog does not show for now.
    //
    m_CurrentDialog->SetFlag( ui_win::WF_VISIBLE, FALSE );
    m_CurrentDialog->SetFlag( ui_win::WF_DISABLED, TRUE );
    if( s_FirstMap )
    {
        // Make the background go away for now.
        g_UiMgr->EnableBackground( TRUE );
        // goto the level description
        m_pLeaderboardDialog->SetState( DIALOG_STATE_ACTIVE );                   // Reset the pad pressed flag
        m_pLeaderboardDialog->SetFlag( ui_win::WF_VISIBLE, FALSE );                      // Make this dialog invisible
        m_pLeaderboardDialog->SetFlag( ui_win::WF_DISABLED, TRUE );

        m_CurrentDialog->SetFlag( ui_win::WF_VISIBLE, TRUE );                           // Make the description visible
        m_CurrentDialog->SetFlag( ui_win::WF_DISABLED, FALSE );
        m_bShowingScores = FALSE;
    }
    else
    {
        // goto the level description screen
        m_CurrentDialog->SetState( DIALOG_STATE_ACTIVE );                   // Reset the pad pressed flag
        m_CurrentDialog->SetFlag( ui_win::WF_VISIBLE, FALSE );                      // Make this dialog invisible
        m_CurrentDialog->SetFlag( ui_win::WF_DISABLED, TRUE );

        m_pLeaderboardDialog->SetFlag( ui_win::WF_VISIBLE, TRUE );                  // Make the leaderboard visible
        m_pLeaderboardDialog->SetFlag( ui_win::WF_DISABLED, FALSE );
        g_UiMgr->EnableBackground( FALSE );
        m_bShowingScores = TRUE;
    }

    if( g_MatchMgr.GetState() == MATCH_SERVER_ACTIVE )
    {
        // Force an update of the server state since we are loading a new mission
        g_MatchMgr.SetState( MATCH_UPDATE_SERVER );
    }
}

//=========================================================================

void state_mgr::UpdateMultiPlayerLoadMission( void )
{
    if( m_bShowingScores==FALSE )
    {
        ASSERT( m_CurrentDialog != NULL );
        ASSERT( (m_CurrentDialog->GetFlags() & ui_win::WF_VISIBLE)  != 0 );
        ASSERT( (m_CurrentDialog->GetFlags() & ui_win::WF_DISABLED) == 0 );
        dialog_states DialogState = m_CurrentDialog->GetState();
        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                if( !s_FirstMap )
                {
                    // Goto the level description screen
                    m_CurrentDialog->SetState( DIALOG_STATE_ACTIVE );                   // Reset the pad pressed flag
                    m_CurrentDialog->SetFlag( ui_win::WF_VISIBLE, FALSE );              // Make this dialog invisible
                    m_CurrentDialog->SetFlag( ui_win::WF_DISABLED, TRUE );

                    m_pLeaderboardDialog->SetFlag( ui_win::WF_VISIBLE, TRUE );          // Make the leaderboard visible
                    m_pLeaderboardDialog->SetFlag( ui_win::WF_DISABLED, FALSE );
                    g_UiMgr->EnableBackground( FALSE );
                    m_bShowingScores = TRUE;
                }
            }
            break;
        }
    }
    else
    {
        dialog_states DialogState = m_pLeaderboardDialog->GetState();
        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Make the background go away for now.
                g_UiMgr->EnableBackground( TRUE );
                // Goto the level description
                m_pLeaderboardDialog->SetState( DIALOG_STATE_ACTIVE );                 // Reset the pad pressed flag
                m_pLeaderboardDialog->SetFlag( ui_win::WF_VISIBLE, FALSE );             // Make this dialog invisible
                m_pLeaderboardDialog->SetFlag( ui_win::WF_DISABLED, TRUE );

                m_CurrentDialog->SetFlag( ui_win::WF_VISIBLE, TRUE );                   // Make the description visible
                m_CurrentDialog->SetFlag( ui_win::WF_DISABLED, FALSE );
                m_bShowingScores = FALSE;
            }
            break;
        }
    }

    if( g_NetworkMgr.IsServer() )
    {
        server_state ServerState = g_NetworkMgr.GetServerObject().GetState();
        if( ServerState != STATE_SERVER_LOAD_MISSION )
        {
            SetState( SM_SERVER_SYNC );
        }
    }
    if( g_NetworkMgr.IsClient() )
    {
        client_state ClientState = g_NetworkMgr.GetClientObject().GetState();
        if( (ClientState != STATE_CLIENT_LOAD_MISSION) &&
            (ClientState != STATE_CLIENT_VERIFY_MISSION) &&
            (ClientState != STATE_CLIENT_REQUEST_MISSION) )
        {
            SetState( SM_CLIENT_SYNC );
        }
    }
}

//=========================================================================

void state_mgr::ExitMultiPlayerLoadMission( void )
{
    if( m_bShowingScores )
    {
        dlg_mp_score* pScoreDlg = (dlg_mp_score*)g_UiMgr->GetTopmostDialog(g_UiUserID);

        // let the dialog know that we have finished loading the game
        pScoreDlg->LoadingComplete();
    }

    // end the dialog
    ASSERT( g_UiMgr->GetTopmostDialog(g_UiUserID) == m_CurrentDialog );
    g_UiMgr->EndDialog( g_UiUserID, TRUE );                                     // Close level description
    ASSERT( g_UiMgr->GetTopmostDialog(g_UiUserID) == m_pLeaderboardDialog );
    g_UiMgr->EndDialog( g_UiUserID, TRUE );                                     // Close the score dialog

    m_CurrentDialog         = NULL;
    m_pLeaderboardDialog    = NULL;
    g_UiMgr->UnloadBackground( "background1" );

#if CONFIG_IS_DEMO
    g_DemoIdleTimer.Reset();
    g_DemoIdleTimer.Start();
#endif
}

//=========================================================================
void state_mgr::EnterServerCooldown( void )
{
    game_server& Server = g_NetworkMgr.GetServerObject();
    if( Server.GetState()==STATE_SERVER_INGAME )
    {
        Server.SetState( STATE_SERVER_COOLDOWN );
    }
}

//=========================================================================
void state_mgr::UpdateServerCooldown( void )
{   
    server_state ServerState = g_NetworkMgr.GetServerObject().GetState();
    exit_reason Reason = g_ActiveConfig.GetExitReason();
    
    if( ServerState != STATE_SERVER_COOLDOWN )
    {
        switch( Reason )
        {
            case GAME_EXIT_ADVANCE_LEVEL:
            case GAME_EXIT_RELOAD_LEVEL:
            case GAME_EXIT_RELOAD_CHECKPOINT:
            {
                SetState( SM_POST_GAME );
            }
            break;
            
            case GAME_EXIT_NETWORK_DOWN:
            case GAME_EXIT_DUPLICATE_LOGIN:
            case GAME_EXIT_CONNECTION_LOST:
            case GAME_EXIT_SERVER_BUSY:
            case GAME_EXIT_SERVER_SHUTDOWN:
            case GAME_EXIT_SESSION_ENDED:
            case GAME_EXIT_PLAYER_QUIT:
            case GAME_EXIT_GAME_COMPLETE:
            case GAME_EXIT_INVALID_MISSION:
            case GAME_EXIT_INVALID_CAMPAIGN_MISSION:
            {
                SetState( SM_SERVER_DISCONNECT );
            }
            break;

            default:
            {
                // NOTE: DO NOT remove this assert. If we come out of the game with an unexpected error code,
                // then we need to handle it gracefully and understand WHY this error code appeared.
                ASSERT(FALSE);
            }
            break;
        }
    }
}

//=========================================================================
void state_mgr::ExitServerCooldown( void )
{
}

//=========================================================================
void state_mgr::EnterClientCooldown( void )
{
    game_client& Client= g_NetworkMgr.GetClientObject();
    switch( Client.GetState() )
    {
    case STATE_CLIENT_IDLE:
    case STATE_CLIENT_COOLDOWN:
    case STATE_CLIENT_DISCONNECT:
    case STATE_CLIENT_SHUTDOWN:
    case STATE_CLIENT_KILL:
        break;
    default:
        Client.SetState( STATE_CLIENT_COOLDOWN );
        break;
    }
}

//=========================================================================
void state_mgr::UpdateClientCooldown( void )
{
    client_state ClientState = g_NetworkMgr.GetClientObject().GetState();
    exit_reason Reason = g_ActiveConfig.GetExitReason();

    if( ClientState != STATE_CLIENT_COOLDOWN )
    {
        switch( Reason )
        {
            case GAME_EXIT_ADVANCE_LEVEL:
            case GAME_EXIT_RELOAD_LEVEL:
            case GAME_EXIT_RELOAD_CHECKPOINT:
            {
                SetState( SM_POST_GAME );
            }
            break;

            case GAME_EXIT_NETWORK_DOWN:
            case GAME_EXIT_DUPLICATE_LOGIN:
            case GAME_EXIT_PLAYER_DROPPED:
            case GAME_EXIT_PLAYER_KICKED:
            case GAME_EXIT_CONNECTION_LOST:
            case GAME_EXIT_SERVER_BUSY:
            case GAME_EXIT_SERVER_SHUTDOWN:
            case GAME_EXIT_SESSION_ENDED:
            case GAME_EXIT_PLAYER_QUIT:
            case GAME_EXIT_GAME_COMPLETE:
            case GAME_EXIT_LOGIN_REFUSED:
            case GAME_EXIT_BAD_PASSWORD:
            case GAME_EXIT_CLIENT_BANNED:
            case GAME_EXIT_SERVER_FULL:
            case GAME_EXIT_INVALID_MISSION:
            case GAME_EXIT_CANNOT_CONNECT:
            case GAME_EXIT_INVALID_CAMPAIGN_MISSION:
            {
                SetState( SM_CLIENT_DISCONNECT );
            }
            break;

            default:
            {
                // NOTE: DO NOT remove this assert. If we come out of the game with an unexpected error code,
                // then we need to handle it gracefully and understand WHY this error code appeared.
                ASSERTS( FALSE, GetExitReasonName( Reason ) );
            }
            break;
        }
    }
}

//=========================================================================
void state_mgr::ExitClientCooldown( void )
{
}


//=========================================================================
void state_mgr::EnterServerDisconnect( void )
{

    game_server& Server = g_NetworkMgr.GetServerObject();
    Server.SetState( STATE_SERVER_SHUTDOWN );
    if( g_NetworkMgr.IsOnline() )
    {
        g_MatchMgr.SetState( MATCH_UNREGISTER_SERVER );
    }
    else
    {
        g_MatchMgr.SetState( MATCH_IDLE );
    }
}

//=========================================================================
void state_mgr::UpdateServerDisconnect( void )
{
    game_server& Server = g_NetworkMgr.GetServerObject();
    if( (g_MatchMgr.GetState() == MATCH_IDLE) && (Server.GetState()==STATE_SERVER_IDLE) )
    {
        g_NetworkMgr.Disconnect();
        SetState( SM_POST_GAME );
    }
}

//=========================================================================
void state_mgr::ExitServerDisconnect( void )
{
}

//=========================================================================
void state_mgr::EnterClientDisconnect( void )
{
}

//=========================================================================
// Need to wait until disconnect is complete then we kill the client.
void state_mgr::UpdateClientDisconnect( void )
{
    client_state ClientState = g_NetworkMgr.GetClientObject().GetState();
    if( ClientState==STATE_CLIENT_IDLE )
    {
        g_NetworkMgr.Disconnect();
        SetState( SM_POST_GAME );
    }
}

//=========================================================================
void state_mgr::ExitClientDisconnect( void )
{
}


//=========================================================================
//  Background Movie Functions
//=========================================================================

void state_mgr::EnableBackgroundMovie( void )
{
#if !defined( X_EDITOR ) && (!CONFIG_IS_DEMO)
    m_bPlayMovie = Movie.Open( SelectBestClip("MenuBackground"),TRUE,TRUE );
#endif
}

//=========================================================================

void state_mgr::DisableBackgoundMovie( void )
{
#if !defined( X_EDITOR ) && (!CONFIG_IS_DEMO)
    Movie.Close();
    m_bPlayMovie = FALSE;
#endif
    // kill background music
    KillFrontEndMusic();
}

//=========================================================================

void state_mgr::PlayMovie( const char* pFilename, xbool bResident, xbool bLooped )
{
    if( !m_bPlayMovie )
    {
        // startup movie player
        Movie.Init();
    }
    else
    {
        // close current movie
        Movie.Close();
    }
    m_bPlayMovie = Movie.Open( SelectBestClip(pFilename), bResident, bLooped );
}

//=========================================================================

void state_mgr::CloseMovie( void )
{
    if( m_bPlayMovie )
    {
        // close movie and shutdown player
        m_bPlayMovie = FALSE;
        Movie.Close();
        Movie.Kill();
    }
}

//=========================================================================
//  Player Inventory Functions
//=========================================================================

void state_mgr::BackupPlayerInventory( void )
{
    ASSERT( GameMgr.GetGameType() == GAME_CAMPAIGN );

    // get the player object
    slot_id ID = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
    player* pPlayer = (player*)g_ObjMgr.GetObjectBySlot( ID );
    ASSERT( pPlayer );

    // store player settings
    inventory2& PlayerInventory = pPlayer->GetInventory2();
    m_StoredPlayerInventory = PlayerInventory;

    // store weapon ammo counts
    for( s32 i=0; i<INVEN_WEAPON_LAST; i++)
    {
        if( PlayerInventory.HasItem( (inven_item)(i+1) ) )
        {
            new_weapon* Weapon = ((actor*)(pPlayer))->GetWeaponPtr( (inven_item)(i+1) );

            if( Weapon )
            {
                Weapon->GetAmmoState( new_weapon::AMMO_PRIMARY, m_AmmoAmount[i], m_AmmoMax[i], m_AmmoPerClip[i], m_AmmoInCurrentClip[i] );
            }
        }
    }

    // store player health
    m_PlayerHealth    = ((actor*)pPlayer)->GetHealth();
    m_PlayerMaxHealth = ((actor*)pPlayer)->GetMaxHealth();

    // store player mutant abilities
    m_PlayerMutantMelee     = pPlayer->m_bMutationMeleeEnabled;
    m_PlayerMutantPrimary   = pPlayer->m_bPrimaryMutationFireEnabled;
    m_PlayerMutantSecondary = pPlayer->m_bSecondaryMutationFireEnabled;
    m_CurrentWeaponItem     = pPlayer->m_CurrentWeaponItem;
    m_PrevWeaponItem        = pPlayer->m_PrevWeaponItem;
    m_NextWeaponItem        = pPlayer->m_NextWeaponItem; 
    m_Mutagen               = pPlayer->m_Mutagen;

    // Switch to the correct weapon.
    pPlayer->SetNextWeapon2( pPlayer->m_CurrentWeaponItem, TRUE );

    // set flag
    m_bInventoryIsStored = TRUE;
}

//=========================================================================

void state_mgr::RestorePlayerInventory( void )
{
    // are we advancing levels?
    if( m_bInventoryIsStored )
    {
        // get the player object
        slot_id ID = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
        player* pPlayer = (player*)g_ObjMgr.GetObjectBySlot( ID );
        ASSERT( pPlayer );

        // create the weapon objects
        pPlayer->InitInventory();

        // restore player settings
        inventory2& PlayerInventory = pPlayer->GetInventory2();
        PlayerInventory = m_StoredPlayerInventory;

        // restore weapon ammo counts
        for( s32 i=0; i<INVEN_WEAPON_LAST; i++)
        {
            if( PlayerInventory.HasItem( (inven_item)(i+1) ) )
            {
                new_weapon* Weapon = pPlayer->GetWeaponPtr( (inven_item)(i+1) );

                if( Weapon )
                {
                    Weapon->SetAmmoState( new_weapon::AMMO_PRIMARY, m_AmmoAmount[i], m_AmmoMax[i], m_AmmoPerClip[i], m_AmmoInCurrentClip[i] );
                }
            }
        }

        // restore player health
        pPlayer->ResetHealth ( m_PlayerHealth    );
        pPlayer->SetMaxHealth( m_PlayerMaxHealth );

        // store player mutant abilities
        pPlayer->m_bMutationMeleeEnabled         = m_PlayerMutantMelee;
        pPlayer->m_bPrimaryMutationFireEnabled   = m_PlayerMutantPrimary;
        pPlayer->m_bSecondaryMutationFireEnabled = m_PlayerMutantSecondary;

        //
        // We don't want to start off with the mutation weapon
        //
        if ( m_CurrentWeaponItem == INVEN_WEAPON_MUTATION )
        {
            m_CurrentWeaponItem = INVEN_WEAPON_DESERT_EAGLE;
        }
        pPlayer->m_CurrentWeaponItem             = m_CurrentWeaponItem;

        pPlayer->m_PrevWeaponItem                = m_PrevWeaponItem;
        pPlayer->m_NextWeaponItem                = m_NextWeaponItem; 
        pPlayer->m_Mutagen                       = m_Mutagen;

        // clear flag
        m_bInventoryIsStored = FALSE;
    }
}

//=========================================================================
//  Map Cycle Functions
//=========================================================================

void state_mgr::ClearMapCycle( void )
{
    for( s32 m=0; m<MAP_CYCLE_SIZE; m++ )
    {
        m_MapCycle[m] = -1; 
    }
    m_bUseDefault   = TRUE;
    m_MapCycleCount = 0;
    m_MapCycleIdx   = 0;
}

//=========================================================================

void state_mgr::IncrementMapCycle( void )
{
    // move the map cycle index to the next map in the cycle
    if( m_MapCycleCount > 1 )
    {
        // increment index
        m_MapCycleIdx++;

        // check for wrapping
        if( m_MapCycleIdx >= m_MapCycleCount )
        {
            // wrap
            m_MapCycleIdx = 0;
        }

        // user changed the list so flag it so
        m_bUseDefault = FALSE;
    }
}

//=========================================================================

void state_mgr::DecrementMapCycle( void )
{
    // move the map cycle index to the previous map in the cycle
    if( m_MapCycleCount > 1 )
    {
        // decrement index
        m_MapCycleIdx--;

        // check for wrapping
        if( m_MapCycleIdx < 0 )
        {
            // wrap
            m_MapCycleIdx = m_MapCycleCount-1;
        }

        // user changed the list so flag it so
        m_bUseDefault = FALSE;
    }
}

//=========================================================================

void state_mgr::InsertMapInCycle( s32 MapID )
{
    ASSERTS( m_MapCycleCount < MAP_CYCLE_SIZE, "Map cycle is FULL - can not insert new map!" );

    if( m_MapCycleIdx == 0 )
    {
        // add the new map ID to the end of the map cycle
        m_MapCycle[m_MapCycleCount] = MapID;
    }
    else
    {
        // insert it after the last "logical" entry
        for( s32 m = MAP_CYCLE_SIZE-1; m>m_MapCycleIdx; m-- )
        {
            m_MapCycle[m] = m_MapCycle[m-1];
        }
        // insert the new map ID in the space we just created
        m_MapCycle[m_MapCycleIdx] = MapID;
        
        // move the idx to its new position
        m_MapCycleIdx++;
    }

    // increment the count
    m_MapCycleCount++;
}

//=========================================================================

void state_mgr::DeleteMapFromCycle( s32 Index )
{
    ASSERTS( Index < m_MapCycleCount, "Cannot delete - index is past end of map cycle!" );

    // move the list up to fill the gap
    if( m_MapCycleIdx == 0 )
    {
        for( s32 m=Index; m<MAP_CYCLE_SIZE-1; m++ )
        {
            m_MapCycle[m] = m_MapCycle[m+1];
        }
    }
    else
    {
        // determine the actual index of the map to be deleted
        s32 Count = Index;
        s32 CurrIdx = m_MapCycleIdx;

        while( Count > 0 )
        {
            // increment the index
            CurrIdx++;

            // check for wrapping
            if( CurrIdx >= m_MapCycleCount )
                CurrIdx = 0;

            // decrement count
            Count--;
        }

        // delete this entry
        for( s32 m=CurrIdx; m<MAP_CYCLE_SIZE-1; m++ )
        {
            m_MapCycle[m] = m_MapCycle[m+1];
        }

        // fix up cycle index
        if( m_MapCycleIdx >= CurrIdx )
        {
            // move it up a slot
            if( m_MapCycleIdx > 0 )
            {
                m_MapCycleIdx--;
            }
        }
    }

    // set last entry (always will be -1 )
    m_MapCycle[MAP_CYCLE_SIZE-1] = -1;

    // decrement count
    m_MapCycleCount--;
}

//=========================================================================

s32 state_mgr::GetMapCycleMapID( s32 Index )
{
    ASSERTS( Index >= 0, "Index out of range!" );
    ASSERTS( Index < MAP_CYCLE_SIZE, "Index out of range!" );

    return m_MapCycle[Index];
}

//=========================================================================
//=========================================================================

void state_mgr::EnterReportError( void )
{
    // reload the front-end sound effects
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/DX_FRONTEND.AUDIOPKG"    ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/SFX_FRONTEND.AUDIOPKG"   ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/MUSIC_FRONTEND.AUDIOPKG" ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/DREAMLAND.AUDIOPKG" ) );
    // initialize audio volumes from global settings
    global_settings& Settings = g_StateMgr.GetActiveSettings();
    Settings.CommitAudio();

    //
    //**NOTE** ReportError state is being used in two scenarios now. Initially,
    // it would just report a problem when we exited the game. As such, it needed
    // to do a couple of things to re-enable the user interface. However, it is
    // also used to report a login failure which will be reported prior to starting
    // up the game. So, we might introduce some problems if we try to restart the
    // movie and re-enable the user. However, they should be benign
    if( m_CurrentDialog )
    {
        g_UiMgr->EndDialog( g_UiUserID, TRUE );
        m_CurrentDialog = NULL;
    }
    else
    {
        g_UiMgr->EnableUser( g_UiUserID, TRUE );

    #ifdef USE_MOVIES
        if( !m_bPlayMovie )
        {
            Movie.Init();
            //Movie.SetLanguage(XL_LANG_ENGLISH);
            m_bPlayMovie = Movie.Open( SelectBestClip("MenuBackground"),TRUE,TRUE );
        }
    #endif
    }

    irect mainarea(126, DIALOG_TOP, 386, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "report error", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateReportError( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        if( DialogState != DIALOG_STATE_ACTIVE )
        {
            switch( g_ActiveConfig.GetExitReason() )
            {
                case GAME_EXIT_NETWORK_DOWN:
                case GAME_EXIT_DUPLICATE_LOGIN:
                {
                    BeginNetworkDisconnect( SM_GAME_EXIT_PROMPT_FOR_SAVE );
                }
                break;
                
                case GAME_EXIT_INVALID_CAMPAIGN_MISSION:
                {
                    g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
                    SetState( SM_CAMPAIGN_MENU );
                }
                break;
                
                case GAME_EXIT_PLAYER_DROPPED:
                case GAME_EXIT_PLAYER_KICKED:
                case GAME_EXIT_PLAYER_QUIT:
                case GAME_EXIT_INVALID_MISSION:
                case GAME_EXIT_SERVER_BUSY:
                case GAME_EXIT_SERVER_SHUTDOWN:
                case GAME_EXIT_CONNECTION_LOST:
                {
                    if( m_LoginFailureDestination == LOGIN_FROM_MAIN )
                    {
                        // If the failure destination is the online main menu, then we need to go through
                        // prompt for save to make sure any in-game data got saved before aborting.
                        SetState( SM_GAME_EXIT_PROMPT_FOR_SAVE );
                        break;
                    }
                }
                
                //***** WARNING WARNING WARNING *****
                //***** DELIBERATE FALLTHROUGH *****

                case GAME_EXIT_SESSION_ENDED:
                case GAME_EXIT_CANNOT_CONNECT:
                case GAME_EXIT_BAD_PASSWORD:
                case GAME_EXIT_SERVER_FULL:
                case GAME_EXIT_CLIENT_BANNED:
                case GAME_EXIT_LOGIN_REFUSED:
                {
                    switch( m_LoginFailureDestination )
                    {
                        case LOGIN_FROM_JOIN_MENU:      SetState( SM_ONLINE_JOIN_MENU );                break;
                        case LOGIN_FROM_MAIN:           SetState( SM_ONLINE_MAIN_MENU );                break;
                        default:                        SetState( SM_ONLINE_MAIN_MENU ); ASSERT(FALSE); break;             
                    }
                }
                break;

                default:
                {
                    // NOTE: DO NOT remove this assert. If we come out of the game with an unexpected error code,
                    // then we need to handle it gracefully and understand WHY this error code appeared.
                    ASSERT(FALSE);
                }
                break;
            }
        }
    }
}

//=========================================================================

void state_mgr::ExitReportError( void )
{
}

//=========================================================================

void state_mgr::EnterGameExitPromptForSave( void )
{
    // We only try to prompt for save if we have a profile. The only time we
    // may not have a profile if the network is disconnected prior to going
    // online properly.
    if( GetProfileListIndex(0)!=-1 )
    {
        // calculate checksum for profile prior to starting game
        player_profile&     ActiveProfile  = GetActiveProfile( GetProfileListIndex(0) );

        // check for changes
        if( ActiveProfile.HasChanged() ) 
        {
            g_UiMgr->EndDialog( g_UiUserID, TRUE );

            // changes have been made - prompt to save
            irect r = g_UiMgr->GetUserBounds( g_UiUserID );
            m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|ui_win::WF_INPUTMODAL );

            // set nav text
            xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
            navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );

            // configure message
            m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_PROFILE_CHANGED_TITLE" ), 
                TRUE, 
                TRUE, 
                FALSE, 
                g_StringTableMgr( "ui", "IDS_PROFILE_CHANGED_MSG" ),
                navText,
                &m_PopUpResult );
        }
        else
        {
            ASSERT( m_PopUp == NULL );
            m_PopUp = NULL;
        }
    }
}

//=========================================================================

void state_mgr::UpdateGameExitPromptForSave( void )
{
    if( m_PopUp == NULL )
    {
        global_settings& ActiveSettings = GetActiveSettings();
        if( ActiveSettings.HasChanged() )
        {
            SetState( SM_GAME_EXIT_SAVE_SETTINGS );
        }
        else
        {
            // No changes to profile - continue on
            SetState( SM_GAME_EXIT_REDIRECT );
        }
        return;
    }
    // check for user response
    if ( m_PopUpResult != DLG_POPUP_IDLE )
    {
        // save changes?
        if ( m_PopUpResult == DLG_POPUP_YES )
        {
            // save changes.  Copy the active profile to the pending profile for saving
            InitPendingProfile(0);

            // check if profile is saved            
            if( g_StateMgr.GetProfileNotSaved( g_StateMgr.GetPendingProfileIndex() ) )
            {
                // Select profile to save to.
                SetState( SM_GAME_EXIT_SAVE_SELECT );
            }
            else
            {
                // attempt to save to the preserved profile
                profile_info* pProfileInfo = &g_SaveDataMgr.GetProfileInfo( 0 );
                g_SaveDataMgr.SaveProfile( *pProfileInfo, 0, this, &state_mgr::OnGameExitSaveProfileCB );
                // goto idle whilst we wait for the callback
                SetState( SM_IDLE );
            }
        }
        else
        {
            global_settings& ActiveSettings = GetActiveSettings();
            if( ActiveSettings.HasChanged() )
            {
                SetState( SM_GAME_EXIT_SAVE_SETTINGS );
            }
            else
            {
                // continue without saving
                SetState( SM_GAME_EXIT_REDIRECT );
            }
        }
    }
}

//=========================================================================

void state_mgr::ExitGameExitPromptForSave( void )
{
}

//=========================================================================

void state_mgr::EnterGameExitSaveSelect( void )
{
    // Create profile menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea(91, DIALOG_TOP, 421, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "profile select", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_profile_select*)m_CurrentDialog)->Configure( PROFILE_SELECT_OVERWRITE );
#ifndef USE_MOVIES
    g_UiMgr->SetUserBackground( g_UiUserID, "background1" );
#endif
}

//=========================================================================

void state_mgr::UpdateGameExitSaveSelect( void )
{
    // Get the current dialog state
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // selected profile was overwritten
                global_settings& ActiveSettings = GetActiveSettings();
                if( ActiveSettings.HasChanged() )
                {
                    SetState( SM_GAME_EXIT_SAVE_SETTINGS );
                }
                else
                {
                    SetState( SM_GAME_EXIT_REDIRECT );
                }
            }
            break;

            case DIALOG_STATE_ACTIVATE:
            {
                global_settings& ActiveSettings = GetActiveSettings();
                if( ActiveSettings.HasChanged() )
                {
                    SetState( SM_GAME_EXIT_SAVE_SETTINGS );
                }
                else
                {
                    // continue without saving
                    SetState( SM_GAME_EXIT_REDIRECT );
                }
            }
            break;

            case DIALOG_STATE_CREATE:
            {
                // Select a card to save the profile to
                SetState( SM_GAME_EXIT_SAVE_DATA_RESELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitGameExitSaveSelect( void )
{
}

//=========================================================================

void state_mgr::EnterGameExitSaveDataReselect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_CREATE_PROFILE );
}

//=========================================================================

void state_mgr::UpdateGameExitSaveDataReselect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save profile was successful.
                global_settings& ActiveSettings = GetActiveSettings();
                if( ActiveSettings.HasChanged() )
                {
                    SetState( SM_GAME_EXIT_SAVE_SETTINGS );
                }
                else
                {
                    // Go to redirect.
                    SetState( SM_GAME_EXIT_REDIRECT );
                }
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // The profile save failed or was aborted.
                // Go back to the profile select menu.
                SetState( SM_GAME_EXIT_SAVE_SELECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitGameExitSaveDataReselect( void )
{
}

//=========================================================================

void state_mgr::EnterGameExitSaveSettings( void )
{
    g_UiMgr->EndDialog( g_UiUserID, TRUE );

    // changes have been made to settings - prompt to save
    irect r = g_UiMgr->GetUserBounds( g_UiUserID );
    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|ui_win::WF_INPUTMODAL );

    // set nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );

    // configure message
    if( g_NetworkMgr.IsServer() )
    {
        m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_GAMEMGR_POPUP" ), 
            TRUE, 
            TRUE, 
            FALSE, 
            g_StringTableMgr( "ui", "IDS_SAVE_SETTINGS_MSG" ),
            navText,
            &m_PopUpResult );
    }
    else
    {
        m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_GAMEMGR_POPUP" ), 
            TRUE, 
            TRUE, 
            FALSE, 
            g_StringTableMgr( "ui", "IDS_SETTINGS_CHANGED_MSG" ),
            navText,
            &m_PopUpResult );
    }
}

//=========================================================================

void state_mgr::UpdateGameExitSaveSettings( void )
{
    switch( m_PopUpResult )
    {
        case DLG_POPUP_YES:
        {
            // save changes
            g_AudioMgr.Play("Select_Norm");

            g_SaveDataMgr.SaveSettings( this, &state_mgr::OnGameExitSaveSettingsCB );
            SetState( SM_IDLE );
        }
        break;

        case DLG_POPUP_NO:
        {
            // continue without saving
            // Activate the pending settings right now. Even though the player opted not to
            // save, the settings should be preserved locally.
            g_StateMgr.ActivatePendingSettings();
            // goto redirect
            SetState( SM_GAME_EXIT_REDIRECT );
        }
        break;
    }
}

//=========================================================================

void state_mgr::ExitGameExitSaveSettings( void )
{
}

//=========================================================================

void state_mgr::EnterGameExitSettingsOverwrite( void )
{
    // ask permission to overwrite old settings
    irect r = g_UiMgr->GetUserBounds( g_UiUserID );
    m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  g_UiUserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|ui_win::WF_INPUTMODAL );

    // set nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );

    // configure message
    m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_GAMEMGR_POPUP" ), 
        TRUE, 
        TRUE, 
        FALSE, 
        g_StringTableMgr( "ui", "IDS_SETTINGS_CHANGED_OVERWRITE" ),
        navText,
        &m_PopUpResult );
}

//=========================================================================

void state_mgr::UpdateGameExitSettingsOverwrite( void )
{
    switch( m_PopUpResult )
    {
        case DLG_POPUP_YES:
        {
            // overwrite old settings file
            g_AudioMgr.Play("Select_Norm");
            g_SaveDataMgr.SaveSettings( this, &state_mgr::OnGameExitSaveSettingsCB );
            // goto idle whilst we wait for the callback
            SetState( SM_IDLE );
        }
        break;

        case DLG_POPUP_NO:
        {
            // continue without saving
            // Activate the pending settings right now. Even though the player opted not to
            // save, the settings should be preserved locally.
            g_StateMgr.ActivatePendingSettings();
            // goto redirect
            SetState( SM_GAME_EXIT_REDIRECT );
        }
        break;
    }
}

//=========================================================================

void state_mgr::ExitGameExitSettingsOverwrite( void )
{
}

//=========================================================================

void state_mgr::EnterGameExitSettingsSelect( void )
{
    //  Create save data select screen
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
    irect mainarea( 46, DIALOG_TOP, 466, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "save data", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
    ((dlg_save_data*)m_CurrentDialog)->Configure( SAVE_DATA_DIALOG_SAVE_SETTINGS );
}

//=========================================================================

void state_mgr::UpdateGameExitSettingsSelect( void )
{
    if( m_CurrentDialog != NULL )
    {
        u32 DialogState = m_CurrentDialog->GetState();

        switch( DialogState )
        {
            case DIALOG_STATE_SELECT:
            {
                // Save settings was successful.
                SetState( SM_GAME_EXIT_REDIRECT );
            }
            break;

            case DIALOG_STATE_BACK:
            {
                // Save failed or was aborted.
                SetState( SM_GAME_EXIT_REDIRECT );
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitGameExitSettingsSelect( void )
{
}

//=========================================================================

void state_mgr::EnterGameExitRedirect( void )
{
    // kill any old dialogs
    g_UiMgr->EndDialog( g_UiUserID, TRUE );
}

//=========================================================================

void state_mgr::UpdateGameExitRedirect( void )
{
    // redirect the flow based on the exit reason
    switch( g_ActiveConfig.GetExitReason() )
    {
        case GAME_EXIT_PLAYER_QUIT:
        case GAME_EXIT_GAME_COMPLETE:
        {
            if( g_NetworkMgr.IsOnline() )
            {
                SetState( SM_ONLINE_MAIN_MENU );
            }
            else
            {
                SetState( SM_CAMPAIGN_MENU );
            }
        }
        break;
        
        case GAME_EXIT_NETWORK_DOWN:
        case GAME_EXIT_DUPLICATE_LOGIN:
        {
            // This will only happen if the cable is pulled while quitting
            // a game.
            if( g_NetworkMgr.IsOnline() )
            {
                SetState( SM_REPORT_ERROR );
                return;
            }
            else
            {
                SetState( SM_MAIN_MENU );
            }
        }
        break;
        
        case GAME_EXIT_PLAYER_DROPPED:
        case GAME_EXIT_PLAYER_KICKED:
        case GAME_EXIT_SESSION_ENDED:
        case GAME_EXIT_CONNECTION_LOST:
        case GAME_EXIT_SERVER_BUSY:
        case GAME_EXIT_SERVER_SHUTDOWN:
        case GAME_EXIT_INVALID_MISSION:
        {
            SetState( SM_ONLINE_MAIN_MENU );
        }
        break;
        
        case GAME_EXIT_INVALID_CAMPAIGN_MISSION:
        {
            SetState( SM_CAMPAIGN_MENU );
        }
        break;
        
        default:
        {
            ASSERTS( FALSE, "Unexpected exit reason in redirect!" );
        }
        break;
    }
    g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
}

//=========================================================================

void state_mgr::ExitGameExitRedirect( void )
{
}

//=========================================================================

void state_mgr::EnterOnlineLogin( void )
{
    // Create online host menu
    g_UiMgr->EndDialog( g_UiUserID, TRUE );

    irect mainarea(26, DIALOG_TOP, 486, DIALOG_BOTTOM );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID, "login", mainarea, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER );
}

//=========================================================================

void state_mgr::UpdateOnlineLogin( void )
{
    s32     LevelID;

    if( CheckForDisconnect() )
    {
        return;
    }

    if( m_CurrentDialog != NULL )
    {
        dialog_states DialogState = (dialog_states)m_CurrentDialog->GetState();
        switch( DialogState )
        {
            case DIALOG_STATE_ACTIVE:
            {
            }
            break;

            case DIALOG_STATE_SELECT:
            {
                LevelID = g_ActiveConfig.GetLevelID();
                ASSERT( LevelID );
                g_PendingConfig.SetLevelID( LevelID );
                game_config::Commit();
                SetState( SM_START_GAME );
            }
            break;
            
                // The child dialog sets the BACK state if any sort of error occurs. 
                // The ReportError dialogs can deal with all the explicit messages.

            case DIALOG_STATE_BACK:
            {
                ASSERT( g_ActiveConfig.GetExitReason() != GAME_EXIT_CONTINUE );
                // Although this should be SM_REPORT_ERROR, SM_POST_GAME
                // performs some cleanup of game objects that still needs
                // to be performed when we drop out of the game early.
                SetState( SM_POST_GAME );
            }
            break;

            case DIALOG_STATE_CANCEL:
            {
                // 'Cancel' (or back) was pressed
                switch( m_LoginFailureDestination )
                {
                    case LOGIN_FROM_JOIN_MENU:              SetState( SM_ONLINE_JOIN_MENU );            break;
                    default:                                ASSERT( FALSE );                            break;
                }
            }
            break;

            default:
            {
                ASSERT(FALSE);
            }
            break;
        }
    }
}

//=========================================================================

void state_mgr::ExitOnlineLogin( void )
{
    if( g_UiMgr->GetTopmostDialog( g_UiUserID ) == m_CurrentDialog )
    {
        g_UiMgr->EndDialog( g_UiUserID, TRUE );
    }
    m_CurrentDialog = NULL;
}

//=========================================================================

void state_mgr::StartLogin( const server_info* pInfo, login_source LoginSource )
{
    m_LoginFailureDestination = LoginSource;

    server_info& Config = g_PendingConfig.GetConfig();
    
    Config = *pInfo;

    // clear server name if we don't have the data yet.
    if( LoginSource != LOGIN_FROM_JOIN_MENU )
    {
        Config.Name[0] = '\0';
    }

    SetState( SM_ONLINE_LOGIN );
}

//=========================================================================
void state_mgr::EnterGameOver( void )
{
}

//=========================================================================
void state_mgr::UpdateGameOver( void )
{
#ifdef USE_MOVIES
    // play the outro movie
    if( !WaitForSimpleMovie( SelectBestClip( "CinemaOutro" ),
                             "End Credits Here (just don't make them lame)" ) )
    {
        return;
    }
#endif

    player_profile& ActiveProfile = GetActiveProfile( GetProfileListIndex(0) );

    // Set that player has finished the game. ( used to open up extra movie )
    ActiveProfile.SetGameFinished( TRUE );

    // check that the difficulty level was unchanged for the entire campaign
    if( !ActiveProfile.GetDifficultyChanged() )
    {
        switch( ActiveProfile.GetDifficultyLevel() )
        {
            case DIFFICULTY_MEDIUM:
            {
                // unlock difficulty hard for completing the game on medium
                ActiveProfile.SetHardUnlocked( TRUE );
            }
            break;

            case DIFFICULTY_HARD:
            {
                // unlock alien avatars for completing the game on hard
                ActiveProfile.SetAlienAvatarsOn( TRUE );
            }
            break;

            default:
            {
                // you get nothing, nada, zip!
            }
            break;
        }
    }

    // save changes to profile
    SilentSaveProfile();

    // load front end audio
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/DX_FRONTEND.AUDIOPKG"    ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/SFX_FRONTEND.AUDIOPKG"   ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/MUSIC_FRONTEND.AUDIOPKG" ) );
    g_RscMgr.Load( PRELOAD_FILE("AUDIO/DREAMLAND.AUDIOPKG" ) );    
    // initialize audio volumes from global settings
    global_settings& Settings = g_StateMgr.GetActiveSettings();
    Settings.CommitAudio();
    EnableBackgroundMovie();

    SetState( SM_CAMPAIGN_MENU );
}

//=========================================================================
void state_mgr::ExitGameOver( void )
{
}

//=========================================================================

xbool state_mgr::CheckForDisconnect( void )
{
    if( g_SaveDataMgr.IsBusy() || m_bDoSystemError )
    {
        // if this is the case, don't check for a disconnect to force dialog 
        // shutdown or you'll hose everything.
        return FALSE;
    }


    // Ok, we noticed a disconnect occurred, let's just close every open
    // dialog and then kick them back to the main menu, after, of course,
    // telling them what just happened.
    exit_reason Reason = g_ActiveConfig.GetExitReason();
    if( (Reason == GAME_EXIT_NETWORK_DOWN) || (Reason==GAME_EXIT_DUPLICATE_LOGIN) )
    {
        g_UiMgr->EndUsersDialogs( g_UiUserID );
        SetState( SM_REPORT_ERROR );
        return TRUE;
    }
    return FALSE;
}

//=========================================================================

void state_mgr::SilentSaveProfileReturn( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        player_profile& ActiveProfile = GetActiveProfile( 0 );
        ActiveProfile.Checksum();

        // reset save in progress flag
        m_bAutosaveInProgress = FALSE;
    }
    else
    {
        // defer menu if a controller is pulled.
        m_bRetryAutoSaveMenu = m_bDoSystemError;
        if( m_bDoSystemError )
        {
            return;
        }

        // goto autosave menu
        SetState( SM_AUTOSAVE_MENU );
    }
}

//=========================================================================
// Make sure we package everything up that's needed for a save, then silently
// save the player profile.
void state_mgr::SilentSaveProfile( void )
{
    // if we've detected a pulled controller, defer the save until it's back in.
    // state_mgr::Update() will detect when we exit the controller error, and will
    // call this again, and cause this to reset.
    m_bRetrySilentAutoSave = m_bDoSystemError;
    if( m_bDoSystemError )
    {
        return;
    }

    // First, copy the checkpoint data from the checkpoint manager to the active
    // player profile.
    s32                     i;
    level_check_points*     pOldestCheckpoint;


    player_profile& ActiveProfile = GetActiveProfile( GetProfileListIndex(0) );

    pOldestCheckpoint = &ActiveProfile.GetCheckpoint(0);

    for( i=0; i<MAX_SAVED_LEVELS;i++ )
    {
        level_check_points& ThisCheckpoint = ActiveProfile.GetCheckpoint(i);

        // Did we find one the same as the current map id? If so, bail out.
        if( (ThisCheckpoint.MapID == g_CheckPointMgr.m_Level.MapID) || (ThisCheckpoint.MapID == -1) )
        {
            pOldestCheckpoint = &ThisCheckpoint;
            break;
        }
        // If the mapid on the checkpoint is less than the current oldest, then we can
        // overwrite that one.
        if( ThisCheckpoint.MapID < pOldestCheckpoint->MapID )
        {
            pOldestCheckpoint = &ThisCheckpoint;
        }
    }

    ASSERTS( i<MAX_SAVED_LEVELS, "Max saved levels reached" );

    // This can only happen if we have, for some reason, tried to save a checkpoint that is for a level we
    // have not yet played but it is older than all those in the saved checkpoint list.
    ASSERT( pOldestCheckpoint->MapID <= g_CheckPointMgr.m_Level.MapID );

    // Store it and then start to save the profile
    *pOldestCheckpoint = g_CheckPointMgr.m_Level;

    // attempt to save the changes to the save data
    if( m_bAutosaveProfile)
    {
        // set save in progress flag
        m_bAutosaveInProgress = TRUE;

        profile_info* pProfileInfo=NULL;
        // This is the last saved profile information stored in the
        // save data subsystem.
        pProfileInfo = &g_SaveDataMgr.GetProfileInfo( 0 );

        LOG_MESSAGE( "state_mgr::SilentSaveProfile", "Profile '%s' save initiated", (const char*)xstring(pProfileInfo->Name) );
        // We need to copy the active profile to the pending profile. This is what will
        // be saved to memory card.

        x_memcpy( &m_PendingProfile, &ActiveProfile, sizeof(ActiveProfile) );

        if( GetProfileNotSaved( 0 ) )
        {
            // goto autosave menu
            SetState( SM_AUTOSAVE_MENU );
        }
        else
        {
            // attempt to save the changes to the save data
            g_SaveDataMgr.SaveProfile( *pProfileInfo, 0, this, &state_mgr::SilentSaveProfileReturn );
        }
    }
    else
    {
        LOG_WARNING( "state_mgr::SilentSaveProfile", "Profile NOT saved (autosave was disabled)" );
    }
}

//=========================================================================

void state_mgr::BeginNetworkDisconnect( sm_states Destination )
{
    ASSERT( Destination != SM_NETWORK_DISCONNECT );

    if( !g_NetworkMgr.IsOnline() )
    {
        SetState( Destination );
        return;
    }

    m_NetworkDisconnectDestination = Destination;
    SetState( SM_NETWORK_DISCONNECT );
}

//=========================================================================

void state_mgr::EnterNetworkDisconnect( void )
{
    g_UiMgr->EndUsersDialogs( g_UiUserID );

    irect r = g_UiMgr->GetUserBounds( g_UiUserID );
    m_CurrentDialog = g_UiMgr->OpenDialog( g_UiUserID,
                                          "popup",
                                          r,
                                          NULL,
                                          ui_win::WF_VISIBLE |
                                          ui_win::WF_BORDER |
                                          ui_win::WF_DLG_CENTER |
                                          ui_win::WF_INPUTMODAL );

    r.Set( 0, 0, 300, 160 );

    dlg_popup* pPopUp = (dlg_popup*)m_CurrentDialog;
    pPopUp->Configure( r,
                       1000.0f,
                       g_StringTableMgr( "ui", "IDS_NETWORK_POPUP" ),
                       g_StringTableMgr( "ui", "IDS_ONLINE_DISCONNECTING" ),
                       NULL );
    pPopUp->EnableBlackout( FALSE );

    g_NetworkMgr.BeginOnlineStateChange( FALSE );
}

//=========================================================================

void state_mgr::UpdateNetworkDisconnect( void )
{
    if( !g_NetworkMgr.IsOnlineStateChanging() )
    {
        g_NetworkMgr.BeginOnlineStateChange( FALSE );
        return;
    }

    if( !g_NetworkMgr.IsOnlineStateChangeDone() )
    {
        return;
    }

    g_NetworkMgr.FinishOnlineStateChange();
    SetState( m_NetworkDisconnectDestination );
}

//=========================================================================

void state_mgr::ExitNetworkDisconnect( void )
{
    g_UiMgr->EndUsersDialogs( g_UiUserID );
}

//=========================================================================

void state_mgr::Reboot( reboot_reason Reason )
{
    g_AudioMgr.Kill();
    g_LevelLoader.UnmountDefaultFilesystems();
    g_IoMgr.Kill();
    g_Input.Kill();
    eng_Reboot( Reason );
}

//=========================================================================

void state_mgr::SetActiveControllerID( s32 ID )
{
    m_ActiveControllerID = ID;
    g_VoiceMgr.GetHeadset().SetActiveHeadset( ID );
}

//=========================================================================

void state_mgr::KillFrontEndMusic( void )
{
    if( g_AudioMgr.IsValidVoiceIdCached( s_VoiceID ) )
    {
        g_AudioMgr.Release( s_VoiceID, 0.0f );
        s_VoiceID = -1;
    }
}

//=========================================================================

void state_mgr::InitFrontEndMusic( const char* pMusicName )
{
    s_VoiceID = g_AudioMgr.Play( pMusicName );
}

//=========================================================================

void state_mgr::SaveMatchStats( const player_stats& Stats )
{
    s32 Index = GetProfileListIndex( 0 );
    if( Index < 0 )
        return;
    player_profile& Profile = GetActiveProfile( Index );
    Profile.AddCareerStats( Stats );
    if( !Profile.GetAutosaveOn() || GetProfileNotSaved(Index) )
        return;
    g_SaveDataMgr.SaveProfileSnapshot( g_SaveDataMgr.GetProfileInfo(Index), Index, Profile,
                                       this, &state_mgr::SaveMatchStatsReturn );
}

//=========================================================================

void state_mgr::SaveMatchStatsReturn( void )
{
    if( !g_SaveDataMgr.GetLastResult().Succeeded() )
        LOG_WARNING( "state_mgr::SaveMatchStatsReturn", "Career statistics could not be saved." );
}

//=========================================================================

void state_mgr::SaveMultiplayerPreferences( void )
{
    s32 Index = GetProfileListIndex( 0 );

    if( (Index < 0) || GetProfileNotSaved( Index ) )
        return;

    player_profile& Profile = GetActiveProfile( Index );
    g_SaveDataMgr.SaveProfileSnapshot( g_SaveDataMgr.GetProfileInfo(Index), Index, Profile,
                                       this, &state_mgr::SaveMultiplayerPreferencesReturn );
}

//=========================================================================

void state_mgr::SaveMultiplayerPreferencesReturn( void )
{
    const SaveDataResult& Result = g_SaveDataMgr.GetLastResult();

    if( !Result.Succeeded() )
        x_DebugMsg( "Multiplayer preferences save failed: profile %d, status %d.\n",
                    Result.ProfileID, (s32)Result.Status );
}
