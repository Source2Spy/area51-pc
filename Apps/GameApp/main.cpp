//==============================================================================
//  
//  main.cpp
//  
//  Area 51 Main Program 
//  
//==============================================================================
// 
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

//==============================================================================
//  CORE INCLUDES
//==============================================================================

#include "Entropy.hpp"  
#include "Main.hpp"  

//==============================================================================
//  SYSTEM MANAGER INCLUDES
//==============================================================================

#include "ResourceMgr\ResourceMgr.hpp"
#include "Obj_Mgr\Obj_Mgr.hpp"
#include "Render\Render.hpp"
#include "AudioMgr\AudioMgr.hpp"
#include "inputmgr\inputmgr.hpp"
#include "IOManager\io_mgr.hpp"
#include "NetworkMgr\NetworkMgr.hpp"
#include "NetworkMgr\GameMgr.hpp"
#include "NetworkMgr\MsgMgr.hpp"
#include "StateMgr\StateMgr.hpp"
#include "MemCardMgr/MemCardMgr.hpp"

//==============================================================================
//  OBJECT INCLUDES
//==============================================================================

#include "Objects\Player.hpp"  
#include "Objects\Corpse.hpp"
#include "Objects\LevelSettings.hpp"
#include "Objects\PlaySurface.hpp"
#include "Objects\SpawnPoint.hpp"
#include "Objects\ParticleEmiter.hpp"
#include "Objects\AlienGlob.hpp"
#include "Objects\HudObject.hpp"
#include "Objects\Render\PostEffectMgr.hpp"

//==============================================================================
//  GAME SUBSYSTEM INCLUDES
//==============================================================================

#include "Gamelib\Level.hpp"
#include "Gamelib\binLevel.hpp"
#include "Gamelib\Link.hpp"
#include "Gamelib\LevelLoader.hpp"
#include "gamelib\StatsMgr.hpp" 
#include "GameLib\RenderContext.hpp"
#include "ZoneMgr\ZoneMgr.hpp"
#include "PlaySurfaceMgr\PlaySurfaceMgr.hpp"
#include "CollisionMgr\PolyCache.hpp"
#include "Templatemgr\TemplateMgr.hpp"
#include "Decals\DecalMgr.hpp"
#include "TweakMgr\TweakMgr.hpp"
#include "PainMgr\PainMgr.hpp"
#include "PhysicsMgr\PhysicsMgr.hpp"
#include "Debris\debris_mgr.hpp"
#include "PerceptionMgr\PerceptionMgr.hpp"
#include "CheckPointMgr\CheckPointMgr.hpp"

//==============================================================================
//  AUDIO INCLUDES
//==============================================================================

#include "Audio\audio_stream_mgr.hpp"
#include "Audio\audio_hardware.hpp"
#include "Audio\audio_voice_mgr.hpp"
#include "Music_Mgr\Music_mgr.hpp"
#include "MusicStateMgr\MusicStateMgr.hpp"
#include "ConversationMgr\ConversationMgr.hpp"

//==============================================================================
//  SCRIPTING
//==============================================================================

#include "..\Support\ScriptMgr\script_mgr.hpp"

//==============================================================================
//  SUPPORT SYSTEM INCLUDES
//==============================================================================

#include "..\Support\TriggerEx\TriggerEx_Manager.hpp"
#include "..\Support\Tracers\TracerMgr.hpp"
#include "..\Support\Render\LightMgr.hpp"
#include "navigation\nav_map.hpp"
#include "navigation\ng_connection2.hpp"
#include "navigation\ng_node2.hpp"

//==============================================================================
//  UI AND TEXT INCLUDES
//==============================================================================

#include "UI\ui_manager.hpp"
#include "UI\ui_Font.hpp"
#include "StringMgr\StringMgr.hpp"
#include "GameTextMgr\GameTextMgr.hpp"
#include "Menu\DebugMenu2.hpp"

//==============================================================================
//  UTILITY INCLUDES
//==============================================================================

#include "x_files\x_context.hpp"
#include "Auxiliary\Bitmap\aux_Bitmap.hpp"
#include "DataVault\DataVault.hpp"
#include "Config.hpp"
#include "Configuration/GameConfig.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define RELEASE_PATH            "C:\\GameData\\A51\\Release"

static f32 GAME_MAX_DELTA_TIME  = 0.1f;
static f32 GAME_DELTA           = 1.0f / 60.0f; // = 1.0f / 30.0f;

#if CONFIG_IS_DEMO
xtimer g_DemoIdleTimer;
#endif

//==============================================================================
//  STRUCTURES
//==============================================================================

#ifndef WIN32
#if ENABLE_RENDER_STATS
extern render::stats s_RenderStats;
#endif
#endif

struct profile
{
    s32     Rate;
    s32     DoPrint;
};

//==============================================================================
//  GLOBAL VARIABLES - CORE SYSTEM
//==============================================================================

xbool       g_bMemReports           = FALSE;
xbool       g_game_running          = TRUE;
xbool       g_first_person          = TRUE;
s32         g_MemoryLowWater        = 0x7fffffff;
view        g_View;
u32         g_nLogicFramesAfterLoad = 0;
xtimer      g_GameTimer;
char        g_FullPath[256];

//==============================================================================
//  GLOBAL VARIABLES - CAMERA AND RENDERING
//==============================================================================

#if (!CONFIG_IS_DEMO)
xbool       g_FreeCam               = FALSE;
#endif
xbool       g_FreeCamPause          = TRUE;
xbool       g_MagentaColor          = FALSE;
xbool       g_RenderBoneBBoxes      = FALSE;
xbool       g_MirrorWeapon          = FALSE;

//==============================================================================
//  GLOBAL VARIABLES - GAME SETTINGS
//==============================================================================

s32         g_Difficulty            = 1; // Start out on Medium difficulty
const char* DifficultyText[]        = { "Easy", "Medium", "Hard" };
xbool       g_right_stick_swap_xy   = FALSE;
xbool       g_bBloodEnabled         = TRUE;
xbool       g_bRagdollsEnabled      = TRUE;

//==============================================================================
//  GLOBAL VARIABLES - CONTROLLER
//==============================================================================

#if !defined( X_RETAIL ) && !defined(ctetrick)
xbool       g_bControllerCheck      = FALSE;
#else
xbool       g_bControllerCheck      = TRUE;
#endif

//==============================================================================
//  GLOBAL VARIABLES - DEBUG/DEVELOPMENT
//==============================================================================

#if defined( ENABLE_DEBUG_MENU )
stats       g_Stats;
xbool       g_GameLogicDebug        = FALSE;
xbool       g_DevWantsToSave        = FALSE;
xbool       g_DevWantsToLoad        = FALSE;
#endif // defined( ENABLE_DEBUG_MENU )

#if !defined( CONFIG_RETAIL )
xbool       g_AimAssist_Render_Reticle      = FALSE;
xbool       g_AimAssist_Render_Bullet       = FALSE;
xbool       g_AimAssist_Render_Turn         = FALSE;
xbool       g_AimAssist_Render_Bullet_Angle = FALSE;
xbool       g_AimAssist_Render_Player_Pills = FALSE;
xbool       g_CmdLineAutoServer             = FALSE;
xbool       g_CmdLineAutoClient             = FALSE;
xbool       g_CmdLineRTFHandler             = FALSE;
s32         g_CmdLineLanguage               = -1;
f32         g_WorldTimeDilation             = 1.0f;
#endif // !defined( CONFIG_RETAIL )

#if !defined( X_RETAIL ) || defined( X_QA )
extern f32  g_TimeDilationFactor;
extern xbool g_RenderFrameRateInfo;
extern s32  g_ScreenShotSize;
extern xbool g_ScreenShotModeEnabled;
#endif

//==============================================================================
//  GLOBAL VARIABLES - AUDIO DEBUG
//==============================================================================

xbool       SHOW_STREAM_INFO        = FALSE;
xbool       SHOW_AUDIO_LEVELS       = FALSE;
xbool       SHOW_AUDIO_CHANNELS     = FALSE;

#if !defined(X_RETAIL) || defined(X_QA)
f32         g_PeakTime              = 0.0f;
f32         PEAK_HOLD_TIME          = 1.0f;
f32         g_ClipTime              = 0.0f;
f32         CLIP_HOLD_TIME          = 3.0f;
xbool       g_Clipped               = FALSE;
s32         g_Peak                  = 0;
s32         g_ChannelPeak           = 0;
f32         g_ChannelTime           = 0.0f;
f32         CHANNEL_PEAK_HOLD_TIME  = 5.0f;
f32         CHANNEL_PEAK_CLIP_TIME  = 8.0f;
s32         g_bChannelFlash         = 0;
char*       g_ChannelText[48];
f32         g_ChannelVolume[48];
s32         g_x                     = 5;
s32         g_y                     = 100;
s32         g_limit                 = 30;
#endif // X_RETAIL

//==============================================================================
//  STATIC VARIABLES
//==============================================================================

static xtimer s_FrontEndDelta;
static f32    s_FrontEndLogicAccum = 0.0f;

#if !defined(X_RETAIL)
static xbool s_ForceGameComplete    = FALSE;
#endif

//==============================================================================
//  FUNCTION PROTOTYPES
//==============================================================================

void        LoadCamera              ( void );
void        SaveCamera              ( void );
void        Render                  ( void );

//==============================================================================
//  PLATFORM SPECIFIC INCLUDES
//==============================================================================

#if !defined(X_RETAIL) && !defined(TARGET_PC)
#include "InputMgr\GamePad.hpp"
#endif

#ifdef TARGET_PC
#include "main_pc.inl"
#endif

//==============================================================================
//  INPUT HANDLING FUNCTIONS
//==============================================================================

xbool HandleInput( f32 DeltaTime )
{
    CONTEXT( "HandleInput" );
    g_InputMgr.Update( DeltaTime );

    // Check for exit message
    if( input_IsPressed( INPUT_MSG_EXIT ) )
        return( FALSE );

    #if defined( ENABLE_DEBUG_MENU )
    if( g_DebugMenu.IsActive() )
    {
        return( TRUE );
    }
    #endif // defined( ENABLE_DEBUG_MENU )

    static s32 FreeCamDebounce = 0;

    {
    #if defined( TARGET_PC )
        if( input_WasPressed( INPUT_KBD_GRAVE ) )
        {
            FreeCamDebounce++;
            if( FreeCamDebounce == 1 )
            {
                g_FreeCam ^= 1;
                if( !g_FreeCam )
                {
                    // Move the player.
                    player* pPlayer = SMP_UTIL_GetActivePlayer();
                    if( pPlayer )
                    {
                        vector3 vPos = g_View.GetPosition();
                        pPlayer->OnExitFreeCam( vPos );
                    }
                }
            }
        }
        else
    #endif // defined(TARGET_PC)
        {
            FreeCamDebounce = 0;
        }
        
    #if defined( ENABLE_DEBUG_MENU )
        if( input_IsPressed(INPUT_PS2_BTN_SELECT) && 
            input_WasPressed(INPUT_PS2_BTN_START ) &&
            g_StateMgr.IsPaused() != TRUE ) 
        {
            g_DebugMenu.Enable();
            g_InputMgr.Update( DeltaTime ); // do the update again, otherwise, the update will drop out.
            return( TRUE );
        }
        
        #if !defined(X_RETAIL) && !defined(TARGET_PC)
        // if screenshot enabled, take screenshot on L3 + Select or CROSS on controller 2
        if( (g_ScreenShotModeEnabled == TRUE) &&
            (!eng_ScreenShotActive()) &&
            (g_StateMgr.IsPaused() != TRUE) &&
            (g_NetworkMgr.GetLocalPlayerCount() == 1) &&
            (   (   input_IsPressed(INPUT_PS2_BTN_L_STICK, 0) 
                 && input_IsPressed(INPUT_PS2_BTN_R_STICK, 0)) 
              ||    input_IsPressed(INPUT_PS2_BTN_CROSS,   1) ) )
        {
            eng_ScreenShot(DEBUG_SCREEN_SHOT_DIR, g_ScreenShotSize + 1);

            // these reset the logical inputs associated with the button.
            player* pPlayer = SMP_UTIL_GetActivePlayer();
            if( pPlayer )
            {
                g_IngamePad[pPlayer->GetActivePlayerPad()].SetLogical( INPUT_PLATFORM_PS2, INPUT_PS2_BTN_L_STICK );
                g_IngamePad[pPlayer->GetActivePlayerPad()].SetLogical( INPUT_PLATFORM_PS2, INPUT_PS2_BTN_R_STICK );
            }
            return (TRUE);
        }
        #endif // !defined(X_RETAIL) && !defined(TARGET_PC)

    #endif // defined( ENABLE_DEBUG_MENU )

        if( !g_StateMgr.InSystemError() )
        {
            // Player has hit the Select button.. trigger off the objective text.
            if( input_IsPressed(INPUT_PS2_BTN_SELECT) && !g_StateMgr.IsPaused() && ( GameMgr.GetGameType() == GAME_CAMPAIGN ) ) 
            {
                slot_id SlotID  = g_ObjMgr.GetFirst( object::TYPE_HUD_OBJECT );

                if( SlotID != SLOT_NULL )
                {
                    object* pObj    = g_ObjMgr.GetObjectBySlot( SlotID );
                    hud_object& Hud = hud_object::GetSafeType( *pObj );

                    if( !Hud.m_Initialized )
                    {
                        return FALSE;
                    }

                    Hud.RenderObjectiveText();
                }
            }        

            // check for pause 
            s32 PausingController;

            PausingController = g_InputMgr.WasPausePressed();

            if( PausingController != -1 )
            {
                // Toggle the pause state.
                g_StateMgr.SetPaused( !g_StateMgr.IsPaused(), PausingController );
            #if CONFIG_IS_DEMO
                g_DemoIdleTimer.Reset();
                g_DemoIdleTimer.Start();
            #endif
            }

            //  Check for pulled controllers.
            g_StateMgr.CheckControllers();
        
        }   // !InSystemError()
    }

    // Handle any platform specific input requirements
    if( HandleInputPlatform( DeltaTime ) == FALSE )
        return( FALSE );
 
    return( TRUE );
}

//==============================================================================
//  AUDIO MANAGEMENT FUNCTIONS
//==============================================================================

void UpdateAudio( f32 DeltaTime )
{
    (void)DeltaTime;
    STAT_LOGGER( temp, k_stats_Sound );

    CONTEXT( "UpdateAudio" );

    #ifdef AUDIO_ENABLE
    
    g_ConverseMgr.Update( DeltaTime );
    g_MusicStateMgr.Update();
    g_MusicMgr.Update( DeltaTime );
    g_AudioMgr.Update( DeltaTime );

    #endif // AUDIO_ENABLE
}

//==============================================================================

#if !defined(X_RETAIL) || defined(X_QA)

void AudioStats( f32 DeltaTime )
{
    (void)DeltaTime;
}

#endif // X_RETAIL

//==============================================================================
//  MAIN UPDATE FUNCTIONS
//==============================================================================

void Update( f32 DeltaTime )
{
    CONTEXT( "Update" );
    DeltaTime = fMin( GAME_MAX_DELTA_TIME, DeltaTime );

    #ifndef X_RETAIL
    g_PolyCache.Update();
    #endif

    if( HandleInput( DeltaTime )==FALSE )
    {
        g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
    }

    #if !defined(X_RETAIL)
    if( s_ForceGameComplete )
    {
        g_ActiveConfig.SetExitReason( GAME_EXIT_GAME_COMPLETE );
        s_ForceGameComplete = FALSE;
    }
    #endif

    #if defined( ENABLE_DEBUG_MENU )
    // run debug menu
    g_DebugMenu.Update( DeltaTime );
    #endif // defined( ENABLE_DEBUG_MENU )

    ASSERT( g_StateMgr.IsBackgroundThreadRunning()==FALSE );
    // run pause menu
    g_StateMgr.Update( DeltaTime );

    // should not pause the game logic for multiplayer or online games!
    if( 
    #if (!CONFIG_IS_DEMO)
        (!g_FreeCamPause || (g_FreeCam == FALSE)) && 
    #endif
    #if defined( ENABLE_DEBUG_MENU )
        (g_DebugMenu.IsActive() == FALSE) &&
        (!eng_ScreenShotActive()) &&
    #endif // defined( ENABLE_DEBUG_MENU )
        ( (g_StateMgr.IsPaused() == FALSE) || 
          ( g_NetworkMgr.IsOnline() == TRUE ) ) 
      )
    {
        g_nLogicFramesAfterLoad++;

        render::Update( DeltaTime );
        g_TracerMgr.OnUpdate( DeltaTime );
        
        // Limit dynamic dead bodies before advancing physics so that it doesn't get overloaded 
        // and or/run out of constraints
        corpse::LimitCount();
        
        g_PhysicsMgr.Advance( DeltaTime );
        {
            STAT_LOGGER( temp, k_stats_OnAdvance );
            g_ObjMgr.AdvanceAllLogic( DeltaTime );
        }
        g_LightMgr.OnUpdate( DeltaTime );
        g_PostEffectMgr.OnUpdate( DeltaTime );
        g_DecalMgr.OnUpdate( DeltaTime );
        g_AlienGlobMgr.Advance( DeltaTime );
        g_ScriptMgr.Update( DeltaTime );		
    }

    if(
    #if defined( ENABLE_DEBUG_MENU )
        (g_DebugMenu.IsActive() == FALSE) &&
        (!eng_ScreenShotActive()) &&
    #endif // defined( ENABLE_DEBUG_MENU )
        ( ( g_StateMgr.IsPaused() == FALSE ) || 
          ( g_NetworkMgr.IsOnline() == TRUE ) )
      )
    { 
        g_TriggerExMgr.OnUpdate( DeltaTime );
    }

    g_NetworkMgr.Update( DeltaTime );
    g_GameTextMgr.Update( DeltaTime );
    UpdateAudio( DeltaTime );

    //handle save/load
    if ( g_BinLevelMgr.WantsToSave() )
    {
        g_BinLevelMgr.SaveRuntimeDynamicData();
        g_VarMgr.SaveRuntimeData();
    }
    else if ( g_BinLevelMgr.WantsToLoad() )
    {
        g_BinLevelMgr.LoadRuntimeDynamicData();
        g_VarMgr.LoadRuntimeData();
    }
}

//==============================================================================

void UpdateFrontEnd( void )
{
    // Accumulate real elapsed time
    f32 DeltaTime = s_FrontEndDelta.TripSec();
    if( DeltaTime > 0.25f )
        DeltaTime = GAME_DELTA;
    s_FrontEndLogicAccum += DeltaTime;

    ASSERT( g_StateMgr.IsBackgroundThreadRunning() == FALSE );

    // run frontend logic at fixed fps, same as game logic.
    // Input is polled inside the tick so mouse deltas and key events are
    // consumed exactly once per update, polling every real frame would
    // cause the second call to overwrite the delta before Update() sees it,
    // making the cursor half-speed and dropping fast keypresses
    if( s_FrontEndLogicAccum >= GAME_DELTA )
    {
        s_FrontEndLogicAccum -= GAME_DELTA;
        input_UpdateState();
        g_StateMgr.CheckControllers();

        #ifdef TARGET_PC
            // Bail if the app is closed
            if( input_IsPressed( INPUT_MSG_EXIT ) )
                return;
        #endif

        g_NetworkMgr.Update( GAME_DELTA );
        g_StateMgr.Update( GAME_DELTA );
        UpdateAudio( GAME_DELTA );
    }

    // render and flip every real frame regardless of logic rate
    g_StateMgr.Render();
    eng_PageFlip();
}

//==============================================================================
//  VIEW AND RENDERING FUNCTIONS
//==============================================================================

void SetupViewAndFog( zone_mgr::zone_id StartZone )
{
    CONTEXT( "SetupView" );

    texture::handle FogPalette;

    // get the default fog palette and z buffer settings
    xbool   QuickFog = FALSE;
    slot_id SlotID = g_ObjMgr.GetFirst( object::TYPE_LEVEL_SETTINGS );
    if ( SlotID != SLOT_NULL )
    {
        object* pObject = g_ObjMgr.GetObjectBySlot( SlotID );
        ASSERT( pObject );
        level_settings& Settings = level_settings::GetSafeType( *pObject );
        g_View.SetZLimits( 10.0f, Settings.GetFarPlane() );

        FogPalette = Settings.GetFogPalette();
    }
    else
    {
        g_View.SetZLimits( 10.0f, 8000.0f );
    }

    // if the zone we're in has a different fog from the default, use
    // that one instead
    const char* pFog = g_ZoneMgr.GetZoneFog(StartZone,QuickFog);
    if ( *pFog != '\0' )
    {
        FogPalette.SetName( pFog );
    }

    // set the pixel scale (aspect ratio)
    #ifndef X_RETAIL
    if ( eng_ScreenShotActive() )
    {   
        g_View.SetPixelScale( 1.0f );
    }
    else
    #endif // X_RETAIL
    {
        g_View.SetPixelScale();
    }

    // Set the viewport
    eng_SetView( g_View );

    // Set the fog
    render::SetCustomFogPalette( FogPalette, QuickFog, g_RenderContext.LocalPlayerIndex );
}

//==============================================================================

void RenderGame( void )
{
    s32 i;

    CONTEXT( "Render" );
    LOG_STAT( k_stats_OtherRender );

    ASSERT( g_StateMgr.IsBackgroundThreadRunning() == FALSE );

    // Set background clear color
    #if (!CONFIG_IS_DEMO)
    if(g_MagentaColor)
    {

        static int count = 0;
        count++;
        if( count%2 )
            eng_SetBackColor( XCOLOR_BLACK );
        else
            eng_SetBackColor( xcolor(255,0,255) );
    }
    else
    #endif
    {
        eng_SetBackColor( XCOLOR_BLACK );
    }

    // Make sure we have full access to the framebuffer so that the splitscreen 
    // cleansing rects can render properly
    {
        view TempView;
        {
            eng_MaximizeViewport( TempView );
            eng_SetViewport     ( TempView );        
        }
    }
    
    // Get pointers to each of the players, we'll need them for setting up the
    // viewports.

    player* pPlayers[MAX_LOCAL_PLAYERS] = { 0 };
    slot_id ID                          = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
    s32     nPlayers                    = 0;

    while( ID != SLOT_NULL )
    {
        ASSERT( nPlayers < g_ActiveConfig.GetPlayerCount() );

        object* pObj    = g_ObjMgr.GetObjectBySlot(ID);
        player* pPlayer = &player::GetSafeType( *pObj );

        if( pPlayer && (pPlayer->GetLocalSlot() != -1) )
        {
            pPlayers[ pPlayer->GetLocalSlot() ] = pPlayer;
            nPlayers++;
        }

        ID = g_ObjMgr.GetNext(ID);
    }
    
    // If we don't have all of the local players yet, then we don't want to try
    // to render.  So, just return.
    if( nPlayers != g_NetworkMgr.GetLocalPlayerCount() )
        return;

    s32 XRes, YRes;
    eng_GetRes( XRes, YRes );
    switch( nPlayers )
    {
        case 0:
        default:
            break;
    
        case 1:
        if ( MAX_LOCAL_PLAYERS >= nPlayers )
        {
            // one view, set it to the entire screen
            view& rView0 = pPlayers[0]->GetView();
            rView0.SetViewport( 0, 0, XRes, YRes );
            pPlayers[0]->ComputeView( rView0 );
        }
        break;

        case 2:
        if ( MAX_LOCAL_PLAYERS >= nPlayers )
        {
            // two views, set them to a horizontal split
            view& rView0 = pPlayers[0]->GetView();
            view& rView1 = pPlayers[1]->GetView();
            rView0.SetViewport( 0,0       ,XRes,YRes/2-1 ); // top
            rView1.SetViewport( 0,YRes/2+1,XRes,YRes     ); // bottom
            pPlayers[0]->ComputeView( rView0 );
            pPlayers[1]->ComputeView( rView1 );

            if( !g_StateMgr.IsPaused() && eng_Begin( "Kill last rect" ) )
            {
                draw_Rect( irect( 0,YRes/2-2,XRes,YRes/2+2 ),XCOLOR_BLACK,FALSE,DRAW_UI_RTARGET);
                eng_End( );
            }
        }
        break;

        case 3:
        if ( MAX_LOCAL_PLAYERS >= nPlayers )
        {
            // four views, set them to a 4-way split
            view& rView0 = pPlayers[0]->GetView();
            view& rView1 = pPlayers[1]->GetView();
            view& rView2 = pPlayers[2]->GetView();
            rView0.SetViewport( 0       ,0       ,XRes/2-1,YRes/2-1 );   // upper-left
            rView1.SetViewport( XRes/2+1,0       ,XRes    ,YRes/2-1 );   // upper-right
            rView2.SetViewport( 0       ,YRes/2+1,XRes    ,YRes     );   // bottom

            pPlayers[0]->ComputeView( rView0 );
            pPlayers[1]->ComputeView( rView1 );
            pPlayers[2]->ComputeView( rView2 );

            if( !g_StateMgr.IsPaused() && eng_Begin( "Kill last rect" ) )
            {
                //draw_Rect( irect( XRes/2  ,YRes/2  ,XRes    ,YRes     ),XCOLOR_BLACK,FALSE,DRAW_UI_RTARGET ); // missing square
                draw_Rect( irect( 0       ,YRes/2-2,XRes    ,YRes/2+2 ),XCOLOR_BLACK,FALSE,DRAW_UI_RTARGET ); // horizontal line
                draw_Rect( irect( XRes/2-2,0       ,XRes/2+2,YRes/2+2 ),XCOLOR_BLACK,FALSE,DRAW_UI_RTARGET ); // vertical line
                eng_End( );
            }
        }
        break;

        case 4:
        if ( MAX_LOCAL_PLAYERS >= nPlayers )
        {
            // four views, set them to a 4-way split
            view& rView0 = pPlayers[0]->GetView();
            view& rView1 = pPlayers[1]->GetView();
            view& rView2 = pPlayers[2]->GetView();
            view& rView3 = pPlayers[3]->GetView(); 

            rView0.SetViewport( 0       ,0,       XRes/2-1,YRes/2-1 );   // upper-left
            rView1.SetViewport( XRes/2+1,0,       XRes    ,YRes/2-1 );   // upper-right
            rView2.SetViewport( 0       ,YRes/2+1,XRes/2-1,YRes     );   // lower-left
            rView3.SetViewport( XRes/2+1,YRes/2+1,XRes    ,YRes     );   // lower-right

            pPlayers[0]->ComputeView( rView0 );
            pPlayers[1]->ComputeView( rView1 );
            pPlayers[2]->ComputeView( rView2 );
            pPlayers[3]->ComputeView( rView3 );

            if( !g_StateMgr.IsPaused() && eng_Begin( "Kill last rect" ) )
            {
                draw_Rect( irect( 0       ,YRes/2-2,XRes    ,YRes/2+2 ),XCOLOR_BLACK,FALSE,DRAW_UI_RTARGET ); // horizontal line
                draw_Rect( irect( XRes/2-2,0       ,XRes/2+2,YRes     ),XCOLOR_BLACK,FALSE,DRAW_UI_RTARGET ); // vertical line
                eng_End( );
            }
        }
        break;
    }

    // Make all the players inactive in anticipation of the render...
    for( i = 0; i < nPlayers; i++ )
        pPlayers[i]->SetAsActivePlayer( FALSE );

    for( i = 0; i < nPlayers; i++ )
    {
        g_RenderContext.Set( i,                                 // Local slot.
                             pPlayers[i]->net_GetSlot(),        // Net slot.
                             pPlayers[i]->net_GetTeamBits(), 
                             pPlayers[i]->IsMutantVisionOn(),
                             FALSE );

        // set this player as the active one
        pPlayers[i]->SetAsActivePlayer( TRUE );

    #if (!CONFIG_IS_DEMO)
        if ( g_FreeCam == FALSE )
    #endif
        {
            view& rView0 = pPlayers[i]->GetView();
            pPlayers[i]->ComputeView( rView0, player::VIEW_NULL );
            g_View = rView0;
        }

        SetupViewAndFog(pPlayers[i]->GetPlayerViewZone());

        // Perform any platform specific render initialization
        InitRenderPlatform();

        // render all objects
        xbool DoPortalWalk = TRUE;
    #if (!CONFIG_IS_DEMO)
        if( g_FreeCam )
            DoPortalWalk = FALSE;
    #endif
        g_ObjMgr.Render( DoPortalWalk, g_View, pPlayers[i]->GetPlayerViewZone() );

        EndRenderPlatform();
        pPlayers[i]->SetAsActivePlayer( FALSE );
    }

    // Make all the players active again so their input will function.
    for( i = 0; i < nPlayers; i++ )
        pPlayers[i]->SetAsActivePlayer( TRUE );

    #if defined( ENABLE_DEBUG_MENU )
    // Debug menu rendering
    g_DebugMenu.Render();
    #endif // defined( ENABLE_DEBUG_MENU )
}

//==============================================================================
//  STATISTICS AND DEBUG FUNCTIONS
//==============================================================================

#if defined( ENABLE_DEBUG_MENU )
void Stats( f32 DeltaTime )
{
    static f32 s_StatTimer = 0.0f;
    static s32 s_AmountFree = 0;
    static s32 s_LargestFree = 0;
    static s32 s_NFragments = 0;
    static s32 s_ObjectCount = 0;
    s_StatTimer += DeltaTime;

    xbool bPrint = FALSE;
    if( s_StatTimer > g_Stats.Interval )
    {
        s_StatTimer = 0.0f;

        #ifndef X_RETAIL
        if( g_Stats.EngineStats == TRUE )
        {
            eng_PrintStats();       
            bPrint = TRUE;
        }
        #endif
    }

    if( g_Stats.RenderStats == TRUE )
    {
        #if ENABLE_RENDER_STATS
        s32 Mode = render::stats::OUTPUT_TO_SCREEN;
        s32 Flags = 0;
        
        //if( g_Stats.RenderVerbose == TRUE )
        //    Flags = render::stats::FLAG_VERBOSE;

        render::GetStats().Print( Mode, Flags );
        bPrint = TRUE;
        #endif
    }

    if (bPrint)
    {
        PrintStatsPlatform();
    }

    #if ENABLE_STATS_MGR
    stats_mgr::GetStatsMgr()->OnGameUpdate(DeltaTime);

    if ( !eng_ScreenShotActive() )
    {
        if( eng_Begin( "StatsMgr" ) )
        {
            stats_mgr::GetStatsMgr()->DrawFPS();
            stats_mgr::GetStatsMgr()->DrawCPULegend();
            stats_mgr::GetStatsMgr()->DrawGPULegend();
            if( g_Stats.MemVertBars )
            {
                stats_mgr::GetStatsMgr()->DrawSmallBars();
                stats_mgr::GetStatsMgr()->DrawSmallBarLegend();
            }
            eng_End();
        }
    }
    #endif

}
#endif // !defined(X_RETAIL) || defined(CONFIG_PROFILE)

//==============================================================================
//  CAMERA MANAGEMENT FUNCTIONS
//==============================================================================

void SaveCamera( void )
{
    X_FILE* fp;
    
    const view* pView = eng_GetView();

    if( !(fp = x_fopen( xfs( "%s\\camera.dat", g_FullPath ), "wb" ))) ASSERT( FALSE );
    x_fwrite( pView, sizeof( view ), 1, fp );
    x_fclose( fp );
    x_DebugMsg( "Camera saved\n" );
}

//==============================================================================

void LoadCamera( void )
{
    X_FILE* fp;
    view    TheView;

    if( !(fp = x_fopen( xfs( "%s\\camera.dat", g_FullPath ), "rb" ))) return;
    x_fread( &TheView, sizeof( view ), 1, fp );
    g_View = TheView;
    x_fclose( fp );
    x_DebugMsg( "Camera loaded\n" );

    // Move the player.
    player* pPlayer = SMP_UTIL_GetActivePlayer();
    if ( pPlayer )
    {
        pPlayer->OnMoveFreeCam( g_View );
    }
}

//==============================================================================
//  LANGUAGE SUPPORT FUNCTIONS
//==============================================================================

x_language CheckLanguageSupport( x_language lang )
{

    #if defined (X_EDITOR)
    (void)lang;
    return XL_LANG_ENGLISH;
    #else

    //// temp fix for programmers
    //#if defined (TARGET_DEV)
    //switch( lang ) 
    //{
    //case XL_LANG_ENGLISH:
    //case XL_LANG_FRENCH:
    //case XL_LANG_ITALIAN:
    //case XL_LANG_SPANISH:
    //case XL_LANG_GERMAN:
    //case XL_LANG_RUSSIAN:
    //    return lang;
    //default:
    //    return XL_LANG_ENGLISH;
    //}
    //#else
    //
    //switch( x_GetTerritory() )
    //{
    //    case XL_TERRITORY_AMERICA:
    //        return XL_LANG_ENGLISH;
    //
    //    case XL_TERRITORY_EUROPE:
    //        switch( lang ) 
    //        {
    //            case XL_LANG_ENGLISH:
    //            case XL_LANG_FRENCH:
    //            case XL_LANG_ITALIAN:
    //            case XL_LANG_SPANISH:
    //            case XL_LANG_GERMAN:
    //            case XL_LANG_RUSSIAN:
    //                return lang;
    //            default:
    //                return XL_LANG_ENGLISH;
    //        }
    //
    //    default:
    //        return XL_LANG_ENGLISH;
    //}
    //#endif

    switch( lang ) 
    {
    case XL_LANG_ENGLISH:
    case XL_LANG_FRENCH:
    case XL_LANG_ITALIAN:
    case XL_LANG_SPANISH:
    case XL_LANG_GERMAN:
    case XL_LANG_RUSSIAN:
        return lang;
    default:
        return XL_LANG_ENGLISH;
    }

    #endif   // !defined (X_EDITOR)
}

//==============================================================================
//  STARTUP AND SHUTDOWN FUNCTIONS
//==============================================================================

void DoStartup( void )
{
    MEMORY_OWNER( "STARTUP" );

    ForceLink();
  
    //
    // Initialize general systems.
    //

    x_DebugMsg( "Entered app.\n" );

    // get language setting and check for default language.
    x_language DefaultLanguage = x_GetConsoleLanguage();
    x_SetLocale( CheckLanguageSupport( DefaultLanguage ) );

    #if !defined( CONFIG_RETAIL )
    if( g_CmdLineLanguage != -1 )
    {
        x_SetLocale( (x_language)g_CmdLineLanguage );
    }
    #endif

    g_UIMemCardMgr.Init();

    eng_Init();
    guid_Init();

    // Enable rtf handler based on a command-line parameter.
    // By default, the rtf handler will be disabled. We need
    // to be fixing ASSERTs, not ignoring them.
    #if !defined(X_RETAIL) && !defined(X_QA)
    if( g_CmdLineRTFHandler )
    {
        extern void InstallRTFHandler( void );
        InstallRTFHandler();
    }
    #endif

    x_DebugMsg( "Initialize io system\n" );

    g_IoMgr.Init();

    #ifdef TARGET_PC
    // Setup the full path to the PC exe
    char exePath[256];
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));

    x_strcpy(g_FullPath, exePath);
    char* pLastSlash = x_strrchr(g_FullPath, '\\');
    if (pLastSlash)
        *pLastSlash = '\0';
    
    x_DebugMsg( "Executable directory: %s\n", g_FullPath );

    // Mount the default file system.
    g_IoMgr.SetDevicePathPrefix( xfs("%s\\", g_FullPath ), IO_DEVICE_HOST );
    #else
    // Mount the default file system.
    g_IoMgr.SetDevicePathPrefix( "C:\\",IO_DEVICE_HOST );	
    // Setup the full path to the platform specific release data
    x_strcpy( g_FullPath, xfs( "%s\\%s", RELEASE_PATH, PLATFORM_PATH ) );
    #endif

    // Xbox: cache to the utility partition.
    g_LevelLoader.MountDefaultFilesystems();

    // Load up the global configuration options from the ini file.
    // This MUST happen AFTER the io system has been initialized. Otherwise we can't
    // load the config.ini file on a viewer build.
    #ifndef CONFIG_RETAIL
    g_Config.Load( xfs( "%s\\%s\\Config.ini", RELEASE_PATH, PLATFORM_PATH ) );
    if( g_CmdLineAutoClient )
        g_Config.AutoClient = TRUE;
    if( g_CmdLineAutoServer )
        g_Config.AutoServer = TRUE;
    #endif

    //
    // Fire up some of the major system managers.
    //

    g_ObjMgr.Init();
    g_SpatialDBase.Init( 400.0f );
    g_PostEffectMgr.Init();
    g_PlaySurfaceMgr.Init();
    g_DecalMgr.Init();
    g_AudioManager.Init( 5512*1024 );

    // Initialize animation system
    anim_event::Init();

    #if defined(AUDIO_ENABLE)
    g_MusicMgr.Init();
    g_ConverseMgr.Init();
    #endif

    g_NetworkMgr.Init();
    g_GameTextMgr.Init();

    #if defined( ENABLE_DEBUG_MENU )
    // Init stats
    x_memset( &g_Stats, 0, sizeof( g_Stats ) );
    g_Stats.Interval = 1;
    #endif

    // Initialize the resource system
    x_DebugMsg( "Starting to initialize resource manager\n" );
    g_RscMgr.Init();
    g_RscMgr.SetRootDirectory( g_FullPath );
    g_RscMgr.SetOnDemandLoading( FALSE );
    x_DebugMsg( "Finished initializing resource manager\n" );

    g_LevelLoader.LoadDFS( "BOOT" );
    g_LevelLoader.LoadDFS( "PRELOAD" );

    // Initialize the render system
    render::Init();
    x_DebugMsg( "Finished initializing rendering\n" );

    // Init systems
    g_TracerMgr.Init();  
    g_PhysicsMgr.Init();

    x_DebugMsg( "Loaded projected textures\n" );

    // Load the debug camera
    LoadCamera();
    x_DebugMsg( "Loaded camera\n" );

    // initialize ui manager
    g_UiMgr =  new ui_manager;
    g_UiMgr->Init();
    g_UiMgr->SetRes();

    // load strings for inventory items.
    //g_StringTableMgr.LoadTable( "Inventory", xfs("%s\\%s", g_RscMgr.GetRootDirectory(), "ENG_Inventory_strings.stringbin" ) );
    g_StringTableMgr.LoadTable( "Inventory", "ENG_Inventory_strings.stringbin" );

    // initialize state manager
    // MUST be done AFTER resource manager init
    g_StateMgr.Init();

    // initialize memcard manager
    //g_MemcardMgr.Init();
    //g_UIMemCardMgr.Init();

    g_RscMgr.TagResources();

    #if !defined(X_RETAIL) && defined(RSC_MGR_COLLECT_STATS)
    {
        MEMORY_OWNER( "STATS" );
        g_RscMgr.DumpStats();
    }
    #endif // X_RETAIL

    // initialize save manager
    g_CheckPointMgr.Init(0);

    // Init the debug menu
    #if defined( ENABLE_DEBUG_MENU )
    g_DebugMenu.Init();
    #endif
	
    // Init scripting
    g_ScriptMgr.Init();
    g_ScriptMgr.RunFile( "scripts\\main.lua" );	
}

//==============================================================================

void DoShutdown( void )
{
    g_ScriptMgr.Kill();	
    g_UIMemCardMgr.Kill();
    g_NetworkMgr.Kill();
    g_GameTextMgr.Kill();
    g_DecalMgr.Kill();
    g_PlaySurfaceMgr.Kill();
    #ifdef AUDIO_ENABLE
    g_ConverseMgr.Kill();
    g_AudioMgr.UnloadAllPackages();
    g_AudioMgr.Kill();
    #endif
    g_StateMgr.Kill();
    g_TracerMgr.Kill();
    g_PhysicsMgr.Kill();
    g_IoMgr.Kill();
    render::Kill();
}

//==============================================================================
//  MAIN LOOP FUNCTIONS
//==============================================================================

void RunFrontEnd( void )
{
    MEMORY_OWNER( "RunFrontEnd()" );

    // load lore strings
    //g_StringTableMgr.LoadTable( "lore", xfs("%s\\%s", g_RscMgr.GetRootDirectory(), "ENG_lore_strings.stringbin") );
    g_StringTableMgr.LoadTable( "lore", "ENG_lore_strings.stringbin" );

    // Run the FrontEnd
    s_FrontEndDelta.Reset();
    s_FrontEndDelta.Start();
    s_FrontEndLogicAccum = 0.0f;

    while( (g_StateMgr.GetState() != SM_MULTI_PLAYER_LOAD_MISSION) &&
           (g_StateMgr.GetState() != SM_SINGLE_PLAYER_LOAD_MISSION) &&
           (g_StateMgr.GetState() != SM_RELOAD_CHECKPOINT) &&
           (g_StateMgr.GetState() != SM_DEMO_EXIT) )
    {
        UpdateFrontEnd( );

#ifdef TARGET_PC
        if( input_IsPressed( INPUT_MSG_EXIT ) )
            break;
#endif
    }

    // update input - flush last keypress
    input_UpdateState();

    // unload string tables
    g_StringTableMgr.UnloadTable( "lore" );
}

//==============================================================================

void RunGame( void )
{
    MEMORY_OWNER( "INGAME" );
    f32 GameTime   = 0.0f;
    f32 GameLogicAccum = 0.0f;

    #if !defined( CONFIG_RETAIL )
    g_MemoryLowWater = 0x7fffffff;
    #endif

    // Commit the settings again. Some of the fields require knowledge as to whether or
    // not we're a server.
    g_StateMgr.GetActiveSettings().Commit();
    // Find the level settings.
    slot_id         ID        = g_ObjMgr.GetFirst( object::TYPE_LEVEL_SETTINGS );
    level_settings* pSettings = (level_settings*)g_ObjMgr.GetObjectBySlot( ID );

    // All good?
    if( pSettings && pSettings->IsKindOf(level_settings::GetRTTI()) )
    {
        // Get the startup trigger.
        guid    GUID    = pSettings->GetStartupGuid();
        object* pObject = g_ObjMgr.GetObjectByGuid( GUID );

        // All good?
        if( pObject && pObject->IsKindOf(trigger_ex_object::GetRTTI()) )
        {
            trigger_ex_object &Trigger = trigger_ex_object::GetSafeType( *pObject );

            // Force it to be active and such!
            Trigger.ForceStartTrigger();

            // Run the trigger logic once.
            Trigger.OnAdvanceLogic( 0.033f );

            // Now NUKE it!
            g_ObjMgr.DestroyObjectEx( GUID, TRUE );
        }
    }

    // Level is fully loaded and startup trigger has run, notify scripts.
    g_ScriptMgr.NotifyLevelStart();

    f32 DeltaTime = MAX( g_GameTimer.ReadSec(), 0.001f );

    // Run!  At least until we stop, that is.
    while( TRUE )
    {
        LOG_STAT( k_stats_CPU_Time );

        #if !defined( CONFIG_RETAIL )
        s32 LowWater = x_MemGetFree();
        if( LowWater < g_MemoryLowWater )
            g_MemoryLowWater = LowWater;
        #endif // !defined( CONFIG_RETAIL )

        //
        // Delta time and game timer management.
        //
        {
            // Compute the duration of the last frame.
            DeltaTime = MAX( g_GameTimer.ReadSec(), 0.001f );

            g_PerceptionMgr.Update( DeltaTime );

            DeltaTime *= g_PerceptionMgr.GetGlobalTimeDialation();

            #if !defined( CONFIG_RETAIL )
            // add in world time dialation
            DeltaTime *= g_WorldTimeDilation;
            #endif

            // Beware negative delta time!
            if( DeltaTime < 0.0f )
            {
                LOG_ERROR( "RunGame", "NEGATIVE DELTA TIME" );
                DeltaTime = GAME_DELTA;
            }

            // Never advance more than 100ms at once
            DeltaTime = fMin( DeltaTime, GAME_MAX_DELTA_TIME );

            g_GameTimer.Reset();
            g_GameTimer.Start();

            // Accumulate real elapsed time.
            GameLogicAccum += DeltaTime;
        }

        //
        // Drain the accumulator in a loop so that at low
        // frame rates (< GAME_DELTA) logic still fires the correct number of times
        // and the game doesn't run in slow-motion. The GAME_MAX_DELTA_TIME cap
        // applied above limits how much time can acumulate per real frame, so
        // this loop is bounded to at most ~6 iterations even at 10 fps - no
        // spiral of death risk
        //
        while( GameLogicAccum >= GAME_DELTA )
        {
            GameLogicAccum -= GAME_DELTA;
            GameTime   += GAME_DELTA;
            Update( GAME_DELTA );
        }

        //
        // During the logic, the game could have "ended".  If it did, get out
        // NOW!  Do not pass GO.  Do not attempt to render.
        //

        #ifdef TARGET_PC
        if( input_IsPressed( INPUT_MSG_EXIT ) )
            break; // GAME OVER, DUDE!
        #endif

        if( g_ActiveConfig.GetExitReason() != GAME_EXIT_CONTINUE )
            break; // GAME OVER, DUDE!

        //
        // We are now reasonably caught up time-wise.  Lets show the situation.
        //

        // give the level a few frames for triggers and other objects to get
        // started before trying to render anything
        if( GameMgr.GameInProgress() )
        {
            if( g_nLogicFramesAfterLoad > 10 )
            {
                RenderGame();
            }

            // render the pause
            g_StateMgr.Render();

            // Some extra stuff...
            {
                // Update context stats.
                x_ContextPrintProfile();
                x_ContextResetProfile();

                #if defined( ENABLE_DEBUG_MENU )
                Stats( DeltaTime );
                //AudioStats( DeltaTime );
                #endif // X_RETAIL
            }
            eng_PageFlip();
        }
        else
        {
            if( g_ActiveConfig.GetExitReason()==GAME_EXIT_CONTINUE )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_ADVANCE_LEVEL );
            }
        }
    }
    //
    // Was the user in the pause menu - unexpected exit condition.
    //
    if( g_StateMgr.IsPaused() )
    {
        // reset the pause flag
        g_StateMgr.ClearPause();
        // set the state to idle whilst we wait
        g_StateMgr.SetState( SM_IDLE );
    }

    // Notify scripts that the level is ending.
    g_ScriptMgr.NotifyLevelEnd();

    //
    // Enable the user interface for the primary user.
    //
    g_UiMgr->EnableUser( g_UiUserID, TRUE );

    #ifndef CONFIG_RETAIL
    // Stop our automated client/server stuff while trying to test the game exit 
    // code.
    g_Config.AutoClient = FALSE;
    g_Config.AutoServer = FALSE;
    #endif
}

//==============================================================================
//  MAIN APPLICATION ENTRY POINT
//==============================================================================

void AppMain( s32 argc, char* argv[] )
{
    // Parse out the command line arguments
#ifndef X_RETAIL

    extern xbool AUDIO_TWEAK;
    {
        for( s32 i=1; i<argc; i++ )
        {
            if( x_stricmp( argv[i], "autoserver" ) == 0 )
                g_CmdLineAutoServer = TRUE;

            if( x_stricmp( argv[i], "autoclient" ) == 0 )
                g_CmdLineAutoClient = TRUE;

            if( x_stricmp( argv[i], "audiotweak" ) == 0 )
                AUDIO_TWEAK = TRUE;

            if( x_stricmp( argv[i], "rtfhandler" ) == 0 )
                g_CmdLineRTFHandler = TRUE;

            if( x_stricmp( argv[i], "language" ) == 0 )
            {
                i++;
                if( i<argc )
                {
                    if( x_stricmp( argv[i], "eng" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_ENGLISH;
                    }
                    if( x_stricmp( argv[i], "fre" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_FRENCH;
                    }
                    if( x_stricmp( argv[i], "ger" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_GERMAN;
                    }
                    if( x_stricmp( argv[i], "ita" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_ITALIAN;
                    }
                    if( x_stricmp( argv[i], "spa" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_SPANISH;
                    }
                    if( x_stricmp( argv[i], "dut" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_DUTCH;
                    }
                    if( x_stricmp( argv[i], "jpn" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_JAPANESE;
                    }
                    if( x_stricmp( argv[i], "kor" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_KOREAN;
                    }
                    if( x_stricmp( argv[i], "por" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_PORTUGUESE;
                    }
                    if( x_stricmp( argv[i], "chi" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_TCHINESE;
                    }
                }
            }
        }
    }
#endif

    (void)argc;
    (void)argv;
    xbool bFullLevelLoad = TRUE;

    MEMORY_OWNER( "DYNAMIC" );

    //
    // Do core startup
    //
    DoStartup();

    // We have to reset these here as we should now have the stringtable
    g_StateMgr.GetActiveSettings().Reset( RESET_ALL );
    g_StateMgr.GetPendingSettings().Reset( RESET_ALL );

    //
    // We're starting off with no exit condition. This will force the 'RunFrontEnd' to
    // start afresh. RunFrontEnd will decide where we need to go next.
    //
    g_ActiveConfig.SetExitReason( GAME_EXIT_CONTINUE );
    //
    // Loop forever playing our fabulous game
    //
    s32 nLoops = 0;
    sm_states CooldownState;

    while( TRUE )
    {
        nLoops++;

        //
        // Did a level trigger request a new level or do we
        // need to talk to the frontend?
        //
        LOG_MEMMARK( "RunFrontEnd" );
        RunFrontEnd();

        if( g_NetworkMgr.IsServer() )
        {
            CooldownState = SM_SERVER_COOLDOWN;
        }
        else
        {
            CooldownState = SM_CLIENT_COOLDOWN;
        }

        LOG_MEMMARK( "LoadLevel" );
        // Bail if the app is closed
#ifdef TARGET_PC
        if( input_IsPressed( INPUT_MSG_EXIT ) )
            break;
#endif

        // Is this a new game or are we restoring from save data?
        if( g_StateMgr.IsRestoredGame() )
        {
            // Not a full level load...
            bFullLevelLoad = FALSE;

            // Load the selected level (load the entire thing however.)
            g_LevelLoader.LoadLevel( TRUE );
        }
        else
        {
            // Load the selected level.
            g_LevelLoader.LoadLevel( bFullLevelLoad );
        }

        //*BW*
        // This causes us to bail early in this special case as it would mean we do not have
        // the mission on the memory card. However, ideally LoadLevel would abort early should
        // something like a disconnect happen and so we may have to end up fixing UnloadLevel
        // so it will deal with partially, or no, loaded level data.
        if( g_ActiveConfig.GetExitReason() == GAME_EXIT_INVALID_MISSION )
        {
            g_StateMgr.SetState( CooldownState );
            continue;
        }
        // Tell network manager load is complete!
        g_NetworkMgr.LoadMissionComplete();


        s_FrontEndDelta.Reset();
        s_FrontEndDelta.Start();
        s_FrontEndLogicAccum = 0.0f;
        //
        // We have to wait until the statemgr has said that everything is ready to go. This is what detects whether or
        // not the sync phase has completed.
        //
        while( (g_StateMgr.GetState() != SM_PLAYING_GAME) && (g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE) )
        {
            UpdateFrontEnd();
        }

        // Finish off the last-minute loading stuff that cannot occur
        // from a background thread or that would cause a pause
        // to happen in the slide show.
        g_LevelLoader.LoadLevelFinish();

        // During the level load process it is possible for an error such as duplicate logon to occur.
        // When this happens the exit reason will be set to something other than GAME_EXIT_CONTINUE.
        // This has the effect of denying the game from creating all the necessary objects such as players etc.
        // We must therefore only attempt a checkpoint restore if we are sure there were no errors on load.
        if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
        {
            // Full level load?
            if( bFullLevelLoad )
            {
                // Restore the player inventory (if necessary)
                g_StateMgr.RestorePlayerInventory();
            }
            else
            {
                // Suck in the check point info.
                g_CheckPointMgr.Restore( FALSE );
            }
        }

        LOG_MEMMARK( "RunGame" );
        if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
        {
            // Only for campaign and full level loads set the initial checkpoint.
            if( (GameMgr.GetGameType() == GAME_CAMPAIGN) && bFullLevelLoad )
            {
                // Start again!
                g_CheckPointMgr.Reinit( g_ActiveConfig.GetLevelID() );

                // Set the initial checkpoint!
                g_CheckPointMgr.SetCheckPoint( NULL_GUID, NULL_GUID, -1, -1 );
                g_StateMgr.SilentSaveProfile();
            }

            // Play the game until we hit an exit condition
            RunGame();
        }

        // Clean out any existing feedback info.
        // This prevents us from rumbling when input updates next time.
        input_ClearFeedback();

        // Bail if the app is closed
#ifdef TARGET_PC
        if( input_IsPressed( INPUT_MSG_EXIT ) )
        {
            g_LevelLoader.UnloadLevel( TRUE );
            break;
        }
#endif

        // Default to complete level load.
        bFullLevelLoad = TRUE;

        g_StateMgr.SetPaused( FALSE, g_StateMgr.GetActiveControllerID() );
        // Special cases for campaign games.
        if( GameMgr.GetGameType() == GAME_CAMPAIGN )
        {
            // Decide what to do based on the exit reason!
            switch( g_ActiveConfig.GetExitReason() )
            {
                case GAME_EXIT_ADVANCE_LEVEL:                                          
                    // Backup the player inventory!
                    g_StateMgr.BackupPlayerInventory();
                    break;

                case GAME_EXIT_RELOAD_CHECKPOINT:
                    // Just load the objects.
                    bFullLevelLoad = FALSE;
                    break;

                default:
                    break;
            }
        }

        s_FrontEndDelta.Reset();
        s_FrontEndDelta.Start();
        s_FrontEndLogicAccum = 0.0f;

        //
        // We have to wait until the statemgr has said all the subsystems have cooled down. This makes sure
        // that we do not have any game traffic going on while the level is unloading.
        //
        if( g_StateMgr.GetState() != CooldownState )
        {
            g_StateMgr.SetState( CooldownState );
        }

        g_NetworkMgr.Update( 0.01f );
        g_NetworkMgr.Update( 0.01f );
        g_NetworkMgr.Update( 0.01f );

        //
        // Unload the level
        //
        LOG_MEMMARK( "UnloadLevel" );
        g_LevelLoader.UnloadLevel( bFullLevelLoad );
        LOG_FLUSH();
        while( g_StateMgr.GetState() == CooldownState )
        {
            UpdateFrontEnd();
        }
    }

    //
    // Shutdown systems involved in Startup
    //
    DoShutdown();
}

//==============================================================================