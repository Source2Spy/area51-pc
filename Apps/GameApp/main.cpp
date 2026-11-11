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

#if defined( TARGET_ANDROID )
#include <SDL3/SDL_main.h>
#endif

//==============================================================================
//  SYSTEM MANAGER INCLUDES
//==============================================================================

#include "ResourceMgr/ResourceMgr.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Render.hpp"
#include "e_Engine.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "IOManager/io_mgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "NetworkMgr/Messages/MsgMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "SaveData/Backend/SaveDataBackend.hpp"

//==============================================================================
//  OBJECT INCLUDES
//==============================================================================

#include "Objects/Player/Player.hpp"
#include "Objects/Corpse.hpp"
#include "Objects/LevelSettings.hpp"
#include "Objects/SpawnPoint.hpp"
#include "Objects/ParticleEmiter.hpp"
#include "Objects/AlienGlob.hpp"
#include "Objects/HudObject.hpp"
#include "Objects/Render/PostEffectMgr.hpp"

//==============================================================================
//  GAME SUBSYSTEM INCLUDES
//==============================================================================

#include "GameLib/Level.hpp"
#include "GameLib/BinLevel.hpp"
#include "GameLib/Link.hpp"
#include "GameLib/LevelLoader.hpp"
#include "GameLib/StatsMgr.hpp" 
#include "GameLib/RenderContext.hpp"
#include "ZoneMgr/ZoneMgr.hpp"
#include "PlaySurfaceMgr/PlaySurfaceMgr.hpp"
#include "CollisionMgr/PolyCache.hpp"
#include "TemplateMgr/TemplateMgr.hpp"
#include "Decals/DecalMgr.hpp"
#include "TweakMgr/TweakMgr.hpp"
#include "PainMgr/PainMgr.hpp"
#include "PhysicsMgr/PhysicsMgr.hpp"
#include "Debris/debris_mgr.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"
#include "CheckPointMgr/CheckPointMgr.hpp"

//==============================================================================
//  AUDIO INCLUDES
//==============================================================================

#include "Audio/audio_stream_mgr.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "Music_mgr/music_mgr.hpp"
#include "MusicStateMgr/MusicStateMgr.hpp"
#include "ConversationMgr/ConversationMgr.hpp"

//==============================================================================
//  SCRIPTING
//==============================================================================

#include "../Support/ScriptMgr/ScriptMgr.hpp"

//==============================================================================
//  SUPPORT SYSTEM INCLUDES
//==============================================================================

#include "../Support/TriggerEx/TriggerEx_Manager.hpp"
#include "../Support/Tracers/TracerMgr.hpp"
#include "../Support/Render/Pipeline/RenderPipeline.hpp"
#include "Navigation/Nav_Map.hpp"
#include "Navigation/ng_connection2.hpp"
#include "Navigation/ng_node2.hpp"

//==============================================================================
//  UI AND TEXT INCLUDES
//==============================================================================

#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"
#include "StringMgr/StringMgr.hpp"
#include "GameTextMgr/GameTextMgr.hpp"
#include "Menu/DebugMenu2.hpp"

//==============================================================================
//  UTILITY INCLUDES
//==============================================================================

#include "x_files/x_profile.hpp"
#include "x_files/x_workers.hpp"
#if X_WORKERS_DEBUG && X_THREADS_DEBUG && X_WORKERS_DEBUG_LOG
#include "x_files/x_threads.hpp"
#endif
#include "Auxiliary/Bitmap/aux_Bitmap.hpp"
#include "DataVault/DataVault.hpp"
#include "Config.hpp"
#include "FramePacer.hpp"
#include "FrameTiming.hpp"
#include "GameAppPlatform.hpp"
#include "Configuration/GameConfig.hpp"

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
char        g_FullPath[256];
char        g_DataPath[256];

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
#endif // defined( ENABLE_DEBUG_MENU )

#if !defined( CONFIG_RETAIL )
xbool       g_AimAssist_Render_Reticle      = FALSE;
xbool       g_AimAssist_Render_Bullet       = FALSE;
xbool       g_AimAssist_Render_Turn         = FALSE;
xbool       g_AimAssist_Render_Bullet_Angle = FALSE;
xbool       g_AimAssist_Render_Player_Pills = FALSE;
xbool       g_CmdLineAutoServer             = FALSE;
xbool       g_CmdLineAutoClient             = FALSE;
char        g_CmdLineConnectAddress[22]     = "";
xbool       g_CmdLineRTFHandler             = FALSE;
s32         g_CmdLineLanguage               = -1;
f32         g_WorldTimeDilation             = 1.0f;
#endif // !defined( CONFIG_RETAIL )

#if !defined( X_RETAIL ) || defined( X_QA )
extern f32  g_TimeDilationFactor;
extern xbool g_RenderFrameRateInfo;
#endif

//==============================================================================
//  GLOBAL VARIABLES - AUDIO DEBUG
//==============================================================================

xbool       SHOW_STREAM_INFO        = FALSE;
#if !defined(X_RETAIL)
static xbool s_ForceGameComplete    = FALSE;
#endif

//  FUNCTION PROTOTYPES
//==============================================================================

void        LoadCamera              ( void );
void        SaveCamera              ( void );
void        Render                  ( void );

//==============================================================================
//  PLATFORM SPECIFIC INCLUDES
//==============================================================================

#include "InputMgr/GamePad.hpp"

#if defined( TARGET_DESKTOP )
#include "main_desktop.inl"
#elif defined( TARGET_MOBILE )
#include "main_mobile.inl"
#endif

//==============================================================================
//  INPUT HANDLING FUNCTIONS
//==============================================================================

//==============================================================================

static u32 GetGameInputContext( void )
{
#if defined( ENABLE_DEBUG_MENU )
    if( g_DebugMenu.IsActive() )
        return DEBUG_MENU_CONTEXT;
#endif

    if( g_StateMgr.IsPaused() || g_StateMgr.InSystemError() )
        return FRONTEND_CONTEXT;

    if( g_StateMgr.GetState() == SM_PLAYING_GAME )
        return INGAME_CONTEXT;

    return FRONTEND_CONTEXT;
}

//==============================================================================

static s32 WasPausePressed( void )
{
    return g_GameInput.GetPauseController();
}

//==============================================================================

xbool HandleInput( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "HandleInput" );

    // Check for exit message
    if( g_Input.GetFrameSnapshot().IsPressed( INPUT_MSG_EXIT ) )
        return( FALSE );

    #if defined( ENABLE_DEBUG_MENU )
    if( g_DebugMenu.IsActive() )
    {
        return( TRUE );
    }
    #endif // defined( ENABLE_DEBUG_MENU )

    static s32 FreeCamDebounce = 0;

    {
    #if defined( TARGET_DESKTOP )
        #if defined( X_DEBUG )
            if( g_Input.GetFrameSnapshot().WasPressed( INPUT_KBD_GRAVE ) )
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
        #endif
    #endif // defined(TARGET_DESKTOP)
            {
                FreeCamDebounce = 0;
            }
        
#if defined( ENABLE_DEBUG_MENU )
        if( g_DebugMenu.WasTogglePressed() &&
            g_StateMgr.IsPaused() != TRUE )
        {
            return( TRUE );
        }
    #endif // defined( ENABLE_DEBUG_MENU )

        if( !g_StateMgr.InSystemError() )
        {
            // check for pause 
            s32 PausingController;

            PausingController = WasPausePressed();

            if( PausingController != -1 )
            {
                // Toggle the pause state.
                g_StateMgr.SetPaused( !g_StateMgr.IsPaused(), PausingController );
                g_GameInput.ClearInput();
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
    STAT_LOGGER( temp, k_stats_Sound );

    X_PROFILE_SCOPE_CATEGORY( "Context", "UpdateAudio" );

    #ifdef AUDIO_ENABLE

    g_ConverseMgr.Update( DeltaTime );
    g_MusicStateMgr.Update();
    g_MusicMgr.Update( DeltaTime );
    g_AudioMgr.Update( DeltaTime );

    #endif // AUDIO_ENABLE
}

//==============================================================================

#if !defined(X_RETAIL) || defined(X_QA)

void AudioStats( void )
{
    if( !SHOW_STREAM_INFO )
        return;
    audio_debug_stats Stats;
    if( !g_AudioMgr.GetDebugStats( Stats ) )
        return;
    x_printfxy( 1, 2, "Audio channels: %d", Stats.ActiveChannels );
    x_printfxy( 1, 3, "Streams: %d / Running: %d", Stats.ActiveStreams, Stats.RunningStreams );
    x_printfxy( 1, 4, "Opening: %d / Reserved: %d", Stats.OpeningStreams, Stats.ReservedStreams );
}

#endif // X_RETAIL

//==============================================================================
//  MAIN UPDATE FUNCTIONS
//==============================================================================

#if !defined(X_RETAIL) && X_WORKERS_DEBUG && X_THREADS_DEBUG && X_WORKERS_DEBUG_LOG
static void LogThreadDebugStats( f32 DeltaTime );
#endif

static xbool ShouldAdvanceWorld( void )
{
    return
    #if (!CONFIG_IS_DEMO)
        (!g_FreeCamPause || (g_FreeCam == FALSE)) &&
    #endif
    #if defined( ENABLE_DEBUG_MENU )
        (g_DebugMenu.IsActive() == FALSE) &&
        (!eng_ScreenShotActive()) &&
    #endif
        ((g_StateMgr.IsPaused() == FALSE) || (g_NetworkMgr.IsOnline() == TRUE));
}

//==============================================================================

void AdvanceSimulation( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "AdvanceSimulation" );


    #if !defined(X_RETAIL)
    if( s_ForceGameComplete )
    {
        g_ActiveConfig.SetExitReason( GAME_EXIT_GAME_COMPLETE );
        s_ForceGameComplete = FALSE;
    }

    #if X_WORKERS_DEBUG && X_THREADS_DEBUG && X_WORKERS_DEBUG_LOG
    LogThreadDebugStats( DeltaTime );
    #endif
    #endif

    if( ShouldAdvanceWorld() )
    {
        g_nLogicFramesAfterLoad++;
        
        // Limit dynamic dead bodies before advancing physics so that it doesn't get overloaded 
        // and or/run out of constraints
        corpse::LimitCount();
        
        g_PhysicsMgr.Advance( DeltaTime );
        slot_id const HudSlot = g_ObjMgr.GetFirst( object::TYPE_HUD_OBJECT );
        hud_object* pHud = (HudSlot != SLOT_NULL)
                         ? (hud_object*)g_ObjMgr.GetObjectBySlot( HudSlot )
                         : NULL;
        if( pHud )
        {
            pHud->BeginIconSnapshot();
        }
        {
            STAT_LOGGER( temp, k_stats_OnAdvance );
            g_ObjMgr.AdvanceSimulation( DeltaTime );
        }
        if( pHud )
        {
            pHud->CommitIconSnapshot();
        }
        g_AlienGlobMgr.Advance( DeltaTime );
        g_ScriptMgr.Update( DeltaTime );
    }

    if(
    #if defined( ENABLE_DEBUG_MENU )
        (g_DebugMenu.IsActive() == FALSE) &&
        (!eng_ScreenShotActive()) &&
    #endif // defined( ENABLE_DEBUG_MENU )
        ((g_StateMgr.IsPaused() == FALSE) || (g_NetworkMgr.IsOnline() == TRUE))
      )
    { 
        g_TriggerExMgr.OnUpdate( DeltaTime );
    }

    g_NetworkMgr.AdvanceSimulation( DeltaTime );
}

//==============================================================================

void UpdateFrameServices( f32 FrameDeltaTime )
{
    // Wall-clock and platform-facing services. These run once per presented
    // frame and must not integrate authoritative world motion.
    X_PROFILE_SCOPE_CATEGORY( "Context", "UpdateFrameServices" );

    #ifndef X_RETAIL
    g_PolyCache.Update();
    #endif

    #if defined( ENABLE_DEBUG_MENU )
    g_DebugMenu.Update( FrameDeltaTime );
    #endif

    g_StateMgr.Update( FrameDeltaTime );
    g_GameTextMgr.Update( FrameDeltaTime );
    UpdateAudio( FrameDeltaTime );

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

void UpdateRenderServices( f32 FrameDeltaTime )
{
    // Advance render-side services once per frame.
    X_PROFILE_SCOPE_CATEGORY( "Context", "UpdateRenderServices" );

    render::Update( FrameDeltaTime );
    g_TracerMgr.OnUpdate( FrameDeltaTime );
    render::GetRenderPipeline().GetLightSystem().OnUpdate( FrameDeltaTime );
    g_PostEffectMgr.OnUpdate( FrameDeltaTime );
    g_DecalMgr.OnUpdate( FrameDeltaTime );
}

//==============================================================================

static xbool FinishClearOnlyFrame( void )
{
    return render::EndFrame() && eng_EndFrame();
}

//==============================================================================

static xbool SampleFrameTiming( FrameTiming& FrameClock,
                                f32&         FrameDeltaSeconds,
                                const char*  pFrameName,
                                xbool&       bFrameAccepted )
{
    FrameTimingSample const Sample = FrameClock.Sample();
    bFrameAccepted = (Sample.Status == FrameTimingStatus::Valid);

    if( bFrameAccepted )
    {
        FrameDeltaSeconds = Sample.AcceptedDeltaSeconds;
        return TRUE;
    }

    FrameDeltaSeconds = 0.0f;
    if( Sample.Status == FrameTimingStatus::Hitch )
    {
        x_DebugMsg( "GameApp: %s hitch delta %f; discarding frame\n", pFrameName, Sample.RawDeltaSeconds );
    }
    else
    {
        x_DebugMsg( "GameApp: invalid %s delta %f; discarding frame\n", pFrameName, Sample.RawDeltaSeconds );
    }

    return FinishClearOnlyFrame();
}

//==============================================================================

static xbool BeginTimedFrame( FramePacer& Pacer,
                              FrameTiming& FrameClock,
                              f32&        FrameDeltaSeconds )
{
    while( TRUE )
    {
        global_settings const& Settings = g_StateMgr.GetActiveSettings();
        Pacer.Configure( Settings.GetFrameRateLimit(), eng_GetPresentMode() );
        Pacer.WaitForNextFrame();

        if( !eng_BeginFrame() )
        {
            return FALSE;
        }
        render::BeginFrame();

        xbool bFrameAccepted = FALSE;
        if( !SampleFrameTiming( FrameClock, FrameDeltaSeconds, "frame", bFrameAccepted ) )
        {
            return FALSE;
        }

        if( bFrameAccepted )
        {
            return TRUE;
        }
    }
}

//==============================================================================

static xbool BeginLoadingFrame( FrameTiming& FrameClock,
                                f32&    FrameDeltaSeconds )
{
    while( TRUE )
    {
        if( !eng_BeginFrame() )
        {
            return FALSE;
        }
        render::BeginFrame();

        xbool bFrameAccepted = FALSE;
        if( !SampleFrameTiming( FrameClock, FrameDeltaSeconds, "loading frame", bFrameAccepted ) )
        {
            return FALSE;
        }

        if( bFrameAccepted )
        {
            return TRUE;
        }
    }
}

//==============================================================================

xbool UpdateFrontEnd( FramePacer& Pacer, FrameTiming& FrontEndClock )
{
    f32 DeltaTime = 0.0f;
    if( !BeginTimedFrame( Pacer, FrontEndClock, DeltaTime ) )
    {
        return FALSE;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.Update" );

        g_FrontendInput.Update( DeltaTime );

        g_StateMgr.CheckControllers();

        #ifdef TARGET_DESKTOP
        if( g_Input.GetFrameSnapshot().IsPressed( INPUT_MSG_EXIT ) )
        {
            FinishClearOnlyFrame();
            return FALSE;
        }
        #endif
        g_NetworkMgr.UpdateFrame( DeltaTime );
        g_StateMgr.Update( DeltaTime );
        g_StateMgr.Render();

        UpdateAudio( DeltaTime );
    }

    return render::EndFrame() && eng_EndFrame();
}

//==============================================================================

xbool UpdateLevelLoadingFrame( FrameTiming& FrontEndClock )
{
    static const f32 k_LevelLoadTimeBudgetSeconds = 0.008f;

    f32 DeltaTime = 0.0f;
    if( !BeginLoadingFrame( FrontEndClock, DeltaTime ) )
    {
        return FALSE;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.Update" );

        g_LevelLoader.UpdateLevelLoad( k_LevelLoadTimeBudgetSeconds );
        g_FrontendInput.Update( DeltaTime );
        g_NetworkMgr.UpdateFrame( DeltaTime );
        g_StateMgr.UpdateLevelLoading( DeltaTime );
        g_StateMgr.RenderLoading();
        UpdateAudio( DeltaTime );
    }

    return render::EndFrame() && eng_EndFrame();
}

//==============================================================================
//  VIEW AND RENDERING FUNCTIONS
//==============================================================================

void SetupViewAndFog( zone_mgr::zone_id StartZone )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "SetupView" );

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

#if !defined(X_RETAIL) && X_WORKERS_DEBUG && X_THREADS_DEBUG && X_WORKERS_DEBUG_LOG

static xbool s_LogThreadDebugStats = TRUE;

static void LogThreadDebugStats( f32 DeltaTime )
{
    static f32 s_LogTimer = 0.0f;

    if( !s_LogThreadDebugStats )
        return;

    s_LogTimer += DeltaTime;
    if( s_LogTimer < 1.0f )
        return;

    s_LogTimer = 0.0f;

    x_thread_debug_snapshot ThreadSnapshot;
    x_worker_debug_snapshot WorkerSnapshot;
    s32                     i;

    x_GetThreadDebugSnapshot( ThreadSnapshot );
    x_WorkersGetDebugSnapshot( WorkerSnapshot );

    x_DebugMsg( "x_workers: %s n:%d kill:%d jobs F/P/R/D:%d/%d/%d/%d sub/done:%d/%d\n",
                WorkerSnapshot.IsInitialized ? "ON" : "OFF",
                WorkerSnapshot.nWorkers,
                WorkerSnapshot.IsKilling,
                WorkerSnapshot.nJobsFree,
                WorkerSnapshot.nJobsPending,
                WorkerSnapshot.nJobsRunning,
                WorkerSnapshot.nJobsDone,
                WorkerSnapshot.nJobsSubmitted,
                WorkerSnapshot.nJobsCompleted );

    x_DebugMsg( "x_worker_services: F/R/D:%d/%d/%d start/done:%d/%d\n",
                WorkerSnapshot.nServicesFree,
                WorkerSnapshot.nServicesRunning,
                WorkerSnapshot.nServicesDone,
                WorkerSnapshot.nServicesStarted,
                WorkerSnapshot.nServicesCompleted );

    if( !ThreadSnapshot.IsInitialized )
    {
        x_DebugMsg( "x_threads: OFF\n" );
        return;
    }

    x_DebugMsg( "x_threads: n:%d active:%d\n", ThreadSnapshot.nThreads, ThreadSnapshot.ActiveThreadId );

    for( i=0; i<ThreadSnapshot.nThreads; i++ )
    {
        const x_thread_debug_info& Info    = ThreadSnapshot.Threads[i];
        const char*                pActive = (Info.ThreadId == ThreadSnapshot.ActiveThreadId) ? "*" : " ";

        x_DebugMsg( "  %s id:%02d sys:%05d pri:%2d %s %s%s\n",
                    pActive,
                    Info.ThreadId,
                    Info.SystemId,
                    Info.Priority,
                    x_GetThreadStateName( Info.Status ),
                    Info.pName,
                    Info.NeedToTerminate ? " TERM" : "" );
    }
}

#endif // !defined(X_RETAIL) && X_WORKERS_DEBUG && X_THREADS_DEBUG && X_WORKERS_DEBUG_LOG

//==============================================================================

void RenderGame( void )
{
    s32 i;

    X_PROFILE_SCOPE_CATEGORY( "Context", "Render" );
    LOG_STAT( k_stats_OtherRender );

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
            pPlayers[0]->SetViewViewport( 0, 0, XRes, YRes );
        }
        break;

        case 2:
        if ( MAX_LOCAL_PLAYERS >= nPlayers )
        {
            // two views, set them to a horizontal split
            pPlayers[0]->SetViewViewport( 0,0       ,XRes,YRes/2-1 ); // top
            pPlayers[1]->SetViewViewport( 0,YRes/2+1,XRes,YRes     ); // bottom

            if( !g_StateMgr.IsPaused() && eng_Begin( "Kill last rect" ) )
            {
                g_UIRenderer.DrawRect( irect( 0,YRes/2-2,XRes,YRes/2+2 ), XCOLOR_BLACK );
                eng_End( );
            }
        }
        break;

        case 3:
        if ( MAX_LOCAL_PLAYERS >= nPlayers )
        {
            // four views, set them to a 4-way split
            pPlayers[0]->SetViewViewport( 0       ,0       ,XRes/2-1,YRes/2-1 );   // upper-left
            pPlayers[1]->SetViewViewport( XRes/2+1,0       ,XRes    ,YRes/2-1 );   // upper-right
            pPlayers[2]->SetViewViewport( 0       ,YRes/2+1,XRes    ,YRes     );   // bottom


            if( !g_StateMgr.IsPaused() && eng_Begin( "Kill last rect" ) )
            {
                g_UIRenderer.DrawRect( irect( 0,        YRes/2-2, XRes,     YRes/2+2 ), XCOLOR_BLACK ); // horizontal line
                g_UIRenderer.DrawRect( irect( XRes/2-2, 0,        XRes/2+2, YRes/2+2 ), XCOLOR_BLACK ); // vertical line
                eng_End( );
            }
        }
        break;

        case 4:
        if ( MAX_LOCAL_PLAYERS >= nPlayers )
        {
            // four views, set them to a 4-way split
            pPlayers[0]->SetViewViewport( 0       ,0,       XRes/2-1,YRes/2-1 );   // upper-left
            pPlayers[1]->SetViewViewport( XRes/2+1,0,       XRes    ,YRes/2-1 );   // upper-right
            pPlayers[2]->SetViewViewport( 0       ,YRes/2+1,XRes/2-1,YRes     );   // lower-left
            pPlayers[3]->SetViewViewport( XRes/2+1,YRes/2+1,XRes    ,YRes     );   // lower-right


            if( !g_StateMgr.IsPaused() && eng_Begin( "Kill last rect" ) )
            {
                g_UIRenderer.DrawRect( irect( 0,        YRes/2-2, XRes,     YRes/2+2 ), XCOLOR_BLACK ); // horizontal line
                g_UIRenderer.DrawRect( irect( XRes/2-2, 0,        XRes/2+2, YRes     ), XCOLOR_BLACK ); // vertical line
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
        render::RenderScopeDesc Scope;
        Scope.ScopeId = i;
        render::ScopedRenderView RenderViewScope( Scope );

        g_RenderContext.Set( i,                                 // Local slot.
                             pPlayers[i]->net_GetSlot(),        // Net slot.
                             pPlayers[i]->net_GetTeamBits(), 
                             pPlayers[i]->IsMutantVisionOn() );

        // set this player as the active one
        pPlayers[i]->SetAsActivePlayer( TRUE );

    #if (!CONFIG_IS_DEMO)
        if ( g_FreeCam == FALSE )
    #endif
        {
            g_View = pPlayers[i]->GetRenderView();
        }

        zone_mgr::zone_id const PlayerViewZone =
            pPlayers[i]->GetPlayerViewZone();
        SetupViewAndFog( PlayerViewZone );

        // render all objects
        xbool DoPortalWalk = TRUE;
        g_ObjMgr.Render( DoPortalWalk, g_View, pPlayers[i]->GetPlayerViewZoneTracker() );

        if( !render::EndRenderScope() )
            return;
        pPlayers[i]->SetAsActivePlayer( FALSE );
    }

    // Make all the players active again so their input will function.
    for( i = 0; i < nPlayers; i++ )
        pPlayers[i]->SetAsActivePlayer( TRUE );

    #if defined( ENABLE_DEBUG_MENU )
    // Debug menu rendering
    AudioStats();
    g_DebugMenu.Render();
    #endif // defined( ENABLE_DEBUG_MENU )
}

//==============================================================================
//  STATISTICS AND DEBUG FUNCTIONS
//==============================================================================

#if defined( ENABLE_DEBUG_MENU )
void Stats( f32 DeltaTime )
{
    static f32 StatTimer = 0.0f;
    StatTimer += DeltaTime;
    if( g_Stats.EngineStats && StatTimer >= g_Stats.Interval )
    {
        eng_PrintStats();
        StatTimer = 0.0f;
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
            stats_mgr::GetStatsMgr()->DrawSmallBars();
            stats_mgr::GetStatsMgr()->DrawSmallBarLegend();
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

    if( !(fp = x_fopen( xfs( "%s/camera.dat", g_FullPath ), "wb" ))) ASSERT( FALSE );
    x_fwrite( pView, sizeof( view ), 1, fp );
    x_fclose( fp );
    x_DebugMsg( "Camera saved\n" );
}

//==============================================================================

void LoadCamera( void )
{
    X_FILE* fp;
    view    TheView;

    if( !(fp = x_fopen( xfs( "%s/camera.dat", g_FullPath ), "rb" ))) return;
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
    case XL_LANG_JAPANESE:
    case XL_LANG_CZECH:
        return lang;
    default:
        return XL_LANG_ENGLISH;
    }

    #endif   // !defined (X_EDITOR)
}

//==============================================================================

x_language CheckAudioVideoLanguageSupport( x_language lang )
{
    if( (lang == XL_LANG_JAPANESE) || (lang == XL_LANG_CZECH) )
    {
        return XL_LANG_ENGLISH;
    }

    return CheckLanguageSupport( lang );
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

    if( !GameAppGetExecutableDirectory( g_FullPath, sizeof( g_FullPath ) ) )
        x_strcpy( g_FullPath, "." );

    if( !GameAppGetDataDirectory( g_DataPath, sizeof( g_DataPath ) ) )
        x_strcpy( g_DataPath, g_FullPath );

    x_DebugMsg( "Executable directory: %s\n", g_FullPath );
    x_DebugMsg( "Data directory: %s\n", g_DataPath );

    char SavePath[256];
    if( !GameAppGetSaveDirectory( SavePath, sizeof( SavePath ) ) )
        x_strcpy( SavePath, g_DataPath );

    x_DebugMsg( "Save directory: %s\n", SavePath );
    SaveDataBackend_SetRootDirectory( SavePath );
    eng_SetPipelineCacheDirectory( xfs( "%s/Cache/Vulkan", SavePath ) );

    // get language setting and check for default language.
    x_language DefaultLanguage = CheckLanguageSupport( x_GetConsoleLanguage() );

    #if !defined( CONFIG_RETAIL )
    if( g_CmdLineLanguage != -1 )
    {
        DefaultLanguage = CheckLanguageSupport( static_cast<x_language>( g_CmdLineLanguage ) );
    }
    #endif

    x_language const DefaultAudioVideoLanguage = CheckAudioVideoLanguageSupport( DefaultLanguage );
    global_settings::SetDefaultLocalizationLanguages( DefaultLanguage,
                                                      DefaultAudioVideoLanguage,
                                                      DefaultAudioVideoLanguage );
    x_SetLocale( DefaultLanguage );

    g_StateMgr.GetActiveSettings().Reset( RESET_LOCALIZATION );
    g_StateMgr.GetPendingSettings().Reset( RESET_LOCALIZATION );

    g_SaveDataMgr.Init();

    // Restore startup consumers before the engine creates its display window.
    xbool const StartupSettingsLoaded = g_SaveDataMgr.LoadStartupSettings();
    g_StateMgr.GetActiveSettings().CommitStartup();

    guid_Init();

    x_DebugMsg( "Initialize io system\n" );

    g_IoMgr.Init();

    // Mount the default file system.
    g_IoMgr.SetDevicePathPrefix( xfs( "%s/", g_DataPath ), IO_DEVICE_HOST );

    // Xbox: cache to the utility partition.
    g_LevelLoader.MountDefaultFilesystems();

    // Load up the global configuration options from the ini file.
    // This MUST happen AFTER the io system has been initialized. Otherwise we can't
    // load the config.ini file on a viewer build.
    #ifndef CONFIG_RETAIL
    g_Config.Load( xfs( "%s/Config.ini", g_DataPath ) );
    if( g_CmdLineAutoClient )
        g_Config.AutoClient = TRUE;
    if( g_CmdLineConnectAddress[0] )
        x_strcpy( g_Config.AutoClientAddress, g_CmdLineConnectAddress );
    if( g_CmdLineAutoServer )
        g_Config.AutoServer = TRUE;
    #endif

    eng_Init();
    GameAppSetWindowIcon();

    //
    // Fire up some of the major system managers.
    //

    g_ObjMgr.Init();
    g_SpatialDBase.Init( 400.0f );
    g_PostEffectMgr.Init();
    g_PlaySurfaceMgr.Init();
    g_DecalMgr.Init();
    g_AudioManager.Init( 5512*1024 );
    g_StateMgr.GetActiveSettings().CommitLocalization();

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
    g_RscMgr.SetRootDirectory( g_DataPath );
    g_RscMgr.SetOnDemandLoading( FALSE );
    x_DebugMsg( "Finished initializing resource manager\n" );

    g_LevelLoader.LoadDFS( "PRELOAD" );
    g_LevelLoader.LoadResourceList( "PRELOAD_RESOURCES.TXT" );

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

    // load strings for inventory items.
    VERIFY( g_StringTableMgr.LoadTable( "Inventory", "ENG_INVENTORY_STRINGS.string" ) );

    if( !StartupSettingsLoaded )
    {
        g_StateMgr.GetActiveSettings().Reset( RESET_ALL );
    }
    g_StateMgr.GetActiveSettings().Commit();

    // initialize state manager
    // MUST be done AFTER resource manager init
    g_StateMgr.Init();
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
    g_ScriptMgr.RunFile( xfs( "%s/SCRIPTS/main.lua", g_DataPath ) );
}

//==============================================================================

void DoShutdown( void )
{
    eng_ResetAfterException();
    eng_Sync();
    g_ScriptMgr.Kill();    
    g_SaveDataMgr.Kill();
    g_NetworkMgr.Kill();
    g_GameTextMgr.Kill();
    g_DecalMgr.Kill();
    g_PlaySurfaceMgr.Kill();
    #ifdef USE_MOVIES
    g_StateMgr.CloseMovie();
    #endif
    #ifdef AUDIO_ENABLE
    g_ConverseMgr.Kill();
    g_AudioMgr.UnloadAllPackages();
    g_AudioMgr.Kill();
    #endif
    g_StateMgr.Kill();
    g_TracerMgr.Kill();
    g_PhysicsMgr.Kill();
    delete g_UiMgr;
    g_UiMgr = NULL;
    render::Kill();
    eng_Kill();
    g_LevelLoader.UnmountDefaultFilesystems();
    g_IoMgr.Kill();
}

//==============================================================================
//  MAIN LOOP FUNCTIONS
//==============================================================================

void RunFrontEnd( void )
{
    MEMORY_OWNER( "RunFrontEnd()" );

    FramePacer Pacer;

    // load lore strings
    VERIFY( g_StringTableMgr.LoadTable( "lore", "ENG_LORE_STRINGS.string" ) );

    FrameTiming FrontEndTiming;
    FrontEndTiming.Start();

    while( (g_StateMgr.GetState() != SM_MULTI_PLAYER_LOAD_MISSION) &&
           (g_StateMgr.GetState() != SM_SINGLE_PLAYER_LOAD_MISSION) &&
           (g_StateMgr.GetState() != SM_RELOAD_CHECKPOINT) &&
           (g_StateMgr.GetState() != SM_DEMO_EXIT) )
    {
        if( g_StateMgr.HasPendingSimpleMovie() )
        {
            g_StateMgr.ProcessPendingSimpleMovie();
            FrontEndTiming.Restart();
            continue;
        }

        if( !UpdateFrontEnd( Pacer, FrontEndTiming ) )
        {
            break;
        }

#ifdef TARGET_DESKTOP
        if( g_Input.GetFrameSnapshot().IsPressed( INPUT_MSG_EXIT ) )
            break;
#endif
    }

    // unload string tables
    g_StringTableMgr.UnloadTable( "lore" );
}

//==============================================================================

void RunGame( void )
{
    MEMORY_OWNER( "INGAME" );

    FramePacer Pacer;

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
            Trigger.OnAdvanceSimulation( 0.033f );

            // Now NUKE it!
            g_ObjMgr.DestroyObjectEx( GUID, TRUE );
        }
    }

    // Level is fully loaded and startup trigger has run, notify scripts.
    g_ScriptMgr.NotifyLevelStart();

    FrameTiming GameTiming;
    GameTiming.Start();
    f32 StartupSimulationTime = 0.0f;

    // Run!  At least until we stop, that is.
    while( TRUE )
    {
        if( g_StateMgr.HasPendingSimpleMovie() )
        {
            g_StateMgr.ProcessPendingSimpleMovie();
            GameTiming.Restart();
            continue;
        }

        LOG_STAT( k_stats_CPU_Time );

        #if !defined( CONFIG_RETAIL )
        s32 LowWater = x_MemGetFree();
        if( LowWater < g_MemoryLowWater )
            g_MemoryLowWater = LowWater;
        #endif // !defined( CONFIG_RETAIL )

        f32 FrameDeltaSeconds = 0.0f;
        if( !BeginTimedFrame( Pacer, GameTiming, FrameDeltaSeconds ) )
        {
            break;
        }

        {
            X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.Update" );

            if( g_GameInput.UpdateFrame( FrameDeltaSeconds, GetGameInputContext() ) )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
            }

            if( HandleInput( FrameDeltaSeconds ) == FALSE )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
            }

            if( g_StateMgr.IsPaused() )
            {
                g_GameInput.ClearInput();
            }

            if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
            {
                g_PerceptionMgr.Update( FrameDeltaSeconds );

                f32 TimeScale = g_PerceptionMgr.GetGlobalTimeDialation();

                #if !defined( CONFIG_RETAIL )
                TimeScale *= g_WorldTimeDilation;
                #endif

                if( !x_isvalid( TimeScale ) || (TimeScale < 0.0f) )
                {
                    ASSERT( FALSE );
                    TimeScale = 0.0f;
                }

                g_NetworkMgr.BeginFrame( FrameDeltaSeconds );
                UpdateFrameServices( FrameDeltaSeconds );

                const f32 SimulationDeltaSeconds = ShouldAdvanceWorld()
                                                   ? (FrameDeltaSeconds * TimeScale)
                                                   : 0.0f;
                if( SimulationDeltaSeconds > 0.0f )
                {
                    AdvanceSimulation( SimulationDeltaSeconds );
                    StartupSimulationTime += SimulationDeltaSeconds;
                }

                g_NetworkMgr.EndFrame( FrameDeltaSeconds );
                UpdateRenderServices( FrameDeltaSeconds );
            }
        }

        if( g_ActiveConfig.GetExitReason() != GAME_EXIT_CONTINUE )
        {
            FinishClearOnlyFrame();
            break;
        }

        //
        // During the logic, the game could have "ended".  If it did, get out
        // NOW!  Do not pass GO.  Do not attempt to render.
        //

        #ifdef TARGET_DESKTOP
        if( g_Input.GetFrameSnapshot().IsPressed( INPUT_MSG_EXIT ) )
        {
            FinishClearOnlyFrame();
            break; // GAME OVER, DUDE!
        }
        #endif

        if( g_ActiveConfig.GetExitReason() != GAME_EXIT_CONTINUE )
        {
            FinishClearOnlyFrame();
            break; // GAME OVER, DUDE!
        }

        //
        // We are now reasonably caught up time-wise.  Lets show the situation.
        //

        // give the level a few frames for triggers and other objects to get
        // started before trying to render anything
        if( GameMgr.GameInProgress() )
        {
            if( g_LevelLoader.NeedsPipelinePrewarm() && !g_LevelLoader.BeginPipelinePrewarm() )
                g_ActiveConfig.SetExitReason( GAME_EXIT_INVALID_MISSION );
            if( !g_LevelLoader.UpdatePipelinePrewarm() )
                g_ActiveConfig.SetExitReason( GAME_EXIT_INVALID_MISSION );
            if( g_nLogicFramesAfterLoad > 10 && StartupSimulationTime >= (10.0f / 30.0f) &&
                g_LevelLoader.IsPipelinePrewarmComplete() &&
                g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
            {
                RenderGame();
            }
            // render the pause
            g_StateMgr.Render();

            // Some extra stuff...
            {
                #if defined( ENABLE_DEBUG_MENU )
                Stats( FrameDeltaSeconds );
                #endif // X_RETAIL
            }
            if( !render::EndFrame() || !eng_EndFrame() )
            {
                break;
            }
        }
        else
        {
            if( g_ActiveConfig.GetExitReason()==GAME_EXIT_CONTINUE )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_ADVANCE_LEVEL );
            }
            FinishClearOnlyFrame();
            break;
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

            if( x_stricmp( argv[i], "connect" ) == 0 )
            {
                net_address Address;
                if( (i + 1 >= argc) || !Address.Parse( argv[i+1], NET_GAME_PORT ) )
                {
                    x_DebugMsg( "Usage: connect IPv4[:port]\n" );
                    return;
                }
                x_strcpy( g_CmdLineConnectAddress, Address.GetStrAddress() );
                g_CmdLineAutoClient = TRUE;
                i++;
            }

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
                    if( x_stricmp( argv[i], "cze" ) == 0 )
                    {
                        g_CmdLineLanguage = XL_LANG_CZECH;
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
    // Initialize editable settings from the values restored during startup.
    g_StateMgr.InitPendingSettings();

    FramePacer AppFramePacer;

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

        while( g_StateMgr.HasPendingSimpleMovie() )
        {
            g_StateMgr.ProcessPendingSimpleMovie();
        }

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
#ifdef TARGET_DESKTOP
        if( g_Input.GetFrameSnapshot().IsPressed( INPUT_MSG_EXIT ) )
            break;
#endif

        xbool LoadEntireLevel = bFullLevelLoad;

        // Is this a new game or are we restoring from save data?
        if( g_StateMgr.IsRestoredGame() )
        {
            // Not a full level load...
            bFullLevelLoad = FALSE;
            LoadEntireLevel = TRUE;
        }

        // Keep the front end, slideshow, audio, input and network alive while
        // the level loader advances one bounded stage at a time.
        g_LevelLoader.BeginLevelLoad( LoadEntireLevel );

        FrameTiming LevelLoadFrontEndTiming;
        LevelLoadFrontEndTiming.Start();

        while( !g_LevelLoader.IsLevelLoadComplete() &&
               g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
        {
            if( !UpdateLevelLoadingFrame( LevelLoadFrontEndTiming ) )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
                break;
            }
        }

        if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
        {
            // Tell network manager load is complete!
            g_NetworkMgr.LoadMissionComplete();
        }


        FrameTiming SyncFrontEndTiming;
        SyncFrontEndTiming.Start();
        //
        // We have to wait until the statemgr has said that everything is ready to go. This is what detects whether or
        // not the sync phase has completed.
        //
        while( (g_StateMgr.GetState() != SM_PLAYING_GAME) && (g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE) )
        {
            if( !UpdateFrontEnd( AppFramePacer, SyncFrontEndTiming ) )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
                break;
            }
        }

        // Finish the level initialization that depends on front-end sync.
        if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
        {
            g_LevelLoader.LoadLevelFinish();
        }

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

        if( g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
        {
            const exit_reason LoadFailure = GameMgr.GetGameType() == GAME_CAMPAIGN
                                                ? GAME_EXIT_INVALID_CAMPAIGN_MISSION
                                                : GAME_EXIT_INVALID_MISSION;
            if( !g_LevelLoader.BeginPipelinePrewarm() )
                g_ActiveConfig.SetExitReason( LoadFailure );
            while( !g_LevelLoader.IsPipelinePrewarmComplete() &&
                   g_ActiveConfig.GetExitReason() == GAME_EXIT_CONTINUE )
            {
                if( !g_LevelLoader.UpdatePipelinePrewarm() ||
                    !UpdateLevelLoadingFrame( LevelLoadFrontEndTiming ) )
                    g_ActiveConfig.SetExitReason( LoadFailure );
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
        g_Input.ClearFeedback();

        // Bail if the app is closed
#ifdef TARGET_DESKTOP
        if( g_Input.GetFrameSnapshot().IsPressed( INPUT_MSG_EXIT ) )
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

        FrameTiming CooldownFrontEndTiming;
        CooldownFrontEndTiming.Start();

        //
        // We have to wait until the statemgr has said all the subsystems have cooled down. This makes sure
        // that we do not have any game traffic going on while the level is unloading.
        //
        if( g_StateMgr.GetState() != CooldownState )
        {
            g_StateMgr.SetState( CooldownState );
        }

        g_NetworkMgr.UpdateFrame( 0.01f );
        g_NetworkMgr.UpdateFrame( 0.01f );
        g_NetworkMgr.UpdateFrame( 0.01f );

        //
        // Unload the level
        //
        LOG_MEMMARK( "UnloadLevel" );
        g_LevelLoader.UnloadLevel( bFullLevelLoad );
        LOG_FLUSH();
        while( g_StateMgr.GetState() == CooldownState )
        {
            if( !UpdateFrontEnd( AppFramePacer, CooldownFrontEndTiming ) )
            {
                g_ActiveConfig.SetExitReason( GAME_EXIT_PLAYER_QUIT );
                break;
            }
        }
    }

    //
    // Shutdown systems involved in Startup
    //
    DoShutdown();
}

//==============================================================================

#if defined( TARGET_POSIX )
int main( int argc, char* argv[] )
{
    x_Init( argc, argv );
    AppMain( (s32)argc, argv );
    return eng_ExitPoint();
}
#endif // defined( TARGET_POSIX )

//==============================================================================
