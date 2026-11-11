//==============================================================================
//
//  sdleng_core.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

#include "e_Composite.hpp"
#include "e_Engine.hpp"
#include "x_stdio.hpp"
#include "x_threads.hpp"

#include <stdarg.h>
#include <stdlib.h>

#if defined(TARGET_POSIX)
#include <time.h>
#endif

//==============================================================================
//  DEFINES
//==============================================================================

#define SCRACH_MEM_SIZE     (2*1024*1024)

#if defined(TARGET_POSIX)
static const u64 FILETIME_UNIX_EPOCH       = 116444736000000000ULL;
static const u64 FILETIME_TICKS_PER_SECOND = 10000000ULL;
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct sdleng_core_locals
{
    u32    Mode;
    xcolor BackColor;
    xbool  bRenderTaskActive;
    xbool  bFrameActive;
    xbool  bProfileTaskActive;
    xbool  bInitialized;
    xbool  bInputInitialized;
    xbool  bRenderTargetsInitialized;
    xbool  bRenderStatesInitialized;
    xbool  bShadersInitialized;
    xbool  bVRAMInitialized;
    xbool  bCompositeInitialized;
    xbool  bScratchInitialized;
    xbool  bPresentTargetPending;
    xbool  bResizePending;

    view           View;
    rdraw_viewport Viewport;
    xbool          bViewportValid;

    xtick CPUFrameIntervals[8];
    xtick SubmittedFrameIntervals[8];
    xtick CPUFrameLastTime;
    xtick SubmittedFrameLastTime;
    s32   CPUFrameIndex;
    s32   SubmittedFrameIndex;
    f32   CPUFrameRate;
    f32   SubmittedFrameRate;
    f32   FramePacingWaitMs;
    f32   RenderSubmitMs;

    s32                   PendingDisplayWidth;
    s32                   PendingDisplayHeight;
    sdleng_display_mode   PendingDisplayMode;
    sdleng_present_policy PendingPresentPolicy;
    xbool                 bDisplaySettingsPending;
    xbool                 bPresentPolicyPending;

    sdleng_core_locals( void )
    {
        x_memset( this, 0, sizeof( sdleng_core_locals ) );
        BackColor = xcolor( 0, 0, 0, 255 );
    }
};

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static sdleng_core_locals s;

//==============================================================================
//  LOCAL HELPERS
//==============================================================================

static
sdleng_display_mode sdleng_core_ToSDLDisplayMode( eng_display_mode Mode )
{
    switch( Mode )
    {
        case ENG_DISPLAY_WINDOWED:   return SDLENG_DISPLAY_WINDOWED;   break;
        case ENG_DISPLAY_BORDERLESS: return SDLENG_DISPLAY_BORDERLESS; break;
        default:                     return SDLENG_DISPLAY_WINDOWED;   break;
    }
}

//==============================================================================

static
xbool sdleng_core_ToSDLPresentPolicy( eng_present_mode       Mode,
                                      sdleng_present_policy& PresentPolicy )
{
    switch( Mode )
    {
        case ENG_PRESENT_VSYNC:
        {
            PresentPolicy = SDLENG_PRESENT_VSYNC;
            return TRUE;
        }

        break;

        case ENG_PRESENT_MAILBOX:
        {
            PresentPolicy = SDLENG_PRESENT_MAILBOX;
            return TRUE;
        }

        break;

        case ENG_PRESENT_IMMEDIATE:
        {
            PresentPolicy = SDLENG_PRESENT_IMMEDIATE;
            return TRUE;
        }

        break;

        default:
        {
            return FALSE;
        }

        break;
    }
}

//==============================================================================

static
eng_display_mode sdleng_core_FromSDLDisplayMode( sdleng_display_mode Mode )
{
    switch( Mode )
    {
        case SDLENG_DISPLAY_WINDOWED:   return ENG_DISPLAY_WINDOWED;   break;
        case SDLENG_DISPLAY_BORDERLESS: return ENG_DISPLAY_BORDERLESS; break;
        default:                        return ENG_DISPLAY_WINDOWED;   break;
    }
}

//==============================================================================

static
void sdleng_core_RecordFrameRate( xtick  CurrentTime,
                                  xtick& LastTime,
                                  xtick  Intervals[8],
                                  s32&   Index,
                                  f32&   FrameRate )
{
    if( LastTime == 0 )
    {
        LastTime = CurrentTime;
        return;
    }

    Intervals[Index] = CurrentTime - LastTime;
    LastTime         = CurrentTime;
    Index            = ( Index + 1 ) & 0x07;

    xtick Sum   = 0;
    s32   Count = 0;

    for( s32 iInterval = 0; iInterval < 8; iInterval++ )
    {
        if( Intervals[iInterval] > 0 )
        {
            Sum += Intervals[iInterval];
            Count++;
        }
    }

    const f32 Milliseconds = x_TicksToMs( Sum );
    FrameRate              = ( Milliseconds > 0.0f )
                                 ? ( (f32)Count * 1000.0f / Milliseconds )
                                 : 0.0f;
}

//==============================================================================

static
void sdleng_core_OnWindowResized( void* pContext, sdleng_native_window_handle hWindow )
{
    (void)pContext;
    (void)hWindow;

    if( !s.bRenderTargetsInitialized )
    {
        return;
    }

    s.bResizePending = TRUE;
}

//==============================================================================

static
void ConvertComandLine( s32* pargc, char* argv[], char* lpCmdLine )
{
    s32 argc = 1;

    if( lpCmdLine && *lpCmdLine )
    {
        argv[1] = lpCmdLine;
        argc    = 2;

        do
        {
            if( *lpCmdLine == ' ' )
            {
                do
                {
                    *lpCmdLine = 0;
                    lpCmdLine++;

                } while( *lpCmdLine == ' ' );

                if( *lpCmdLine == 0 )
                {
                    break;
                }

                argv[argc++] = lpCmdLine;
            }

            lpCmdLine++;

        } while( *lpCmdLine );
    }

    *pargc = argc;
}

//==============================================================================

static
void sdleng_core_UpdateDisplayViewport( void )
{
    if( !sdleng_InRenderPass() )
    {
        return;
    }

    s32 XRes = 0;
    s32 YRes = 0;
    eng_GetRes( XRes, YRes );

    if( ( XRes <= 0 ) || ( YRes <= 0 ) )
    {
        return;
    }

    rdraw_viewport Viewport;
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    Viewport.Width    = (f32)XRes;
    Viewport.Height   = (f32)YRes;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    rdraw_SetViewport( Viewport );
}

//==============================================================================

static
void sdleng_core_ApplyViewport( void )
{
    if( !s.bViewportValid || !sdleng_InRenderPass() )
    {
        return;
    }

    if( !rdraw_SetViewport( s.Viewport ) )
    {
        x_DebugMsg( "SDLEngine: failed to apply viewport\n" );
    }
}

//==============================================================================

static
xbool sdleng_core_AbortFrame( const char* pReason )
{
    x_DebugMsg( "SDLEngine: frame lifecycle error: %s\n", pReason );

    if( s.bProfileTaskActive )
    {
        x_GetProfiler().EndScope();
        s.bProfileTaskActive = FALSE;
    }

    if( sdleng_InRenderPass() )
    {
        rtarget_EndPass();
    }

    sdleng_CancelFrame();
    x_GetProfiler().CancelFrame();

    s.bRenderTaskActive = FALSE;
    s.bFrameActive      = FALSE;

    ASSERTS( FALSE, pReason );

    return FALSE;
}

//==============================================================================

static
xbool sdleng_core_ApplyDisplaySettings( s32                 Width,
                                        s32                 Height,
                                        sdleng_display_mode DisplayMode )
{
    if( !sdleng_WindowGetSDLWindow() )
    {
        sdleng_WindowSetResolution( Width, Height );
        sdleng_WindowSetDisplayMode( DisplayMode );
        return TRUE;
    }

    sdleng_WindowSetModeChange( TRUE );

    if( s.bFrameActive || s.bRenderTaskActive || sdleng_InFrame() )
    {
        x_DebugMsg( "SDLEngine: display settings cannot change during an active frame\n" );
        sdleng_WindowSetModeChange( FALSE );
        return FALSE;
    }

    s32 ClientWidth  = 0;
    s32 ClientHeight = 0;
    sdleng_WindowGetClientSize( ClientWidth, ClientHeight );
    const s32 PreviousWidth  = ClientWidth;
    const s32 PreviousHeight = ClientHeight;
    xbool     Success        = sdleng_WindowApplyDisplayMode( Width,
                                                   Height,
                                                   DisplayMode,
                                                   ClientWidth,
                                                   ClientHeight );

    if( Success && s.bRenderTargetsInitialized &&
        ( ( PreviousWidth != ClientWidth ) || ( PreviousHeight != ClientHeight ) ) )
    {
        Success          = rtarget_NotifyResolutionChanged();
        s.bResizePending = !Success;
    }

    sdleng_WindowSetModeChange( FALSE );

    return Success;
}

//==============================================================================

static
void sdleng_core_CapturePendingDisplaySettings( void )
{
    if( s.bDisplaySettingsPending )
    {
        return;
    }

    sdleng_WindowGetResolution( s.PendingDisplayWidth, s.PendingDisplayHeight );
    s.PendingDisplayMode      = sdleng_WindowGetDisplayMode();
    s.bDisplaySettingsPending = TRUE;
}

//==============================================================================

static
void sdleng_core_ApplyPendingSettings( void )
{
    if( s.bDisplaySettingsPending )
    {
        const s32                 Width       = s.PendingDisplayWidth;
        const s32                 Height      = s.PendingDisplayHeight;
        const sdleng_display_mode DisplayMode = s.PendingDisplayMode;
        s.bDisplaySettingsPending             = FALSE;

        if( !sdleng_core_ApplyDisplaySettings( Width, Height, DisplayMode ) )
        {
            x_DebugMsg( "SDLEngine: failed to apply deferred display settings\n" );
        }
    }

    if( s.bPresentPolicyPending )
    {
        const sdleng_present_policy PresentPolicy = s.PendingPresentPolicy;
        s.bPresentPolicyPending                   = FALSE;

        if( !sdleng_SetPresentPolicy( PresentPolicy ) )
        {
            x_DebugMsg( "SDLEngine: failed to apply deferred present mode\n" );
        }
    }
}

//==============================================================================
//  DESKTOP ENTRY POINTS
//==============================================================================

#if defined(TARGET_WINDOWS)
void eng_EntryPoint( s32&                       argc,
                     char**&                    argv,
                     eng_native_instance_handle hInstance,
                     eng_native_instance_handle hPrevInstance,
                     char*                      pCmdLine,
                     s32                        nCmdShow )
{
    (void)hPrevInstance;
    (void)nCmdShow;

    static char* ArgvBuff[256] = { NULL };
    argv                       = ArgvBuff;

    ConvertComandLine( &argc, argv, pCmdLine );
    sdleng_WindowSetInstance( hInstance );

    x_memset( s.CPUFrameIntervals,       0, sizeof( s.CPUFrameIntervals ) );
    x_memset( s.SubmittedFrameIntervals, 0, sizeof( s.SubmittedFrameIntervals ) );

    x_Init( argc, argv );
}

//==============================================================================

s32 eng_ExitPoint( void )
{
#if defined(TARGET_WINDOWS)
    SetThreadExecutionState( ES_CONTINUOUS );
#endif
    x_Kill();

    return 0;
}

#else

s32 eng_ExitPoint( void )
{
    x_Kill();

    return 0;
}
#endif

//==============================================================================
//  ENGINE CONFIGURATION
//==============================================================================

void eng_SetPresets( u32 Mode )
{
    s.Mode = Mode;
}

//==============================================================================

u32 eng_GetMode( void )
{
    return s.Mode;
}

//==============================================================================
//  DISPLAY SETTINGS
//==============================================================================

void eng_SetDisplayMode( eng_display_mode Mode )
{
    if( s.bFrameActive || s.bRenderTaskActive || sdleng_InFrame() )
    {
        sdleng_core_CapturePendingDisplaySettings();
        s.PendingDisplayMode = sdleng_core_ToSDLDisplayMode( Mode );
        return;
    }

    s32 Width  = 0;
    s32 Height = 0;
    sdleng_WindowGetResolution( Width, Height );

    if( !sdleng_core_ApplyDisplaySettings( Width,
                                           Height,
                                           sdleng_core_ToSDLDisplayMode( Mode ) ) )
    {
        x_DebugMsg( "SDLEngine: failed to change display mode\n" );
    }
}

//==============================================================================

eng_display_mode eng_GetDisplayMode( void )
{
    return sdleng_core_FromSDLDisplayMode( sdleng_WindowGetDisplayMode() );
}

//==============================================================================

xbool eng_GetDisplayResolutions( xarray<eng_display_resolution>& Resolutions )
{
    Resolutions.Clear();

    xarray<sdleng_display_resolution> SDLResolutions;

    if( !sdleng_WindowGetResolutions( SDLResolutions ) )
    {
        return FALSE;
    }

    Resolutions.SetCapacity( SDLResolutions.GetCount() );

    for( s32 iResolution = 0; iResolution < SDLResolutions.GetCount(); iResolution++ )
    {
        eng_display_resolution& Resolution = Resolutions.Append();
        Resolution.Set( SDLResolutions[iResolution].GetWidth(),
                        SDLResolutions[iResolution].GetHeight() );
    }

    return TRUE;
}

//==============================================================================

xbool eng_SetPresentMode( eng_present_mode Mode )
{
    sdleng_present_policy PresentPolicy;

    if( !sdleng_core_ToSDLPresentPolicy( Mode, PresentPolicy ) )
    {
        return FALSE;
    }

    if( s.bFrameActive || s.bRenderTaskActive || sdleng_InFrame() )
    {
        s.PendingPresentPolicy  = PresentPolicy;
        s.bPresentPolicyPending = TRUE;
        return TRUE;
    }

    return sdleng_SetPresentPolicy( PresentPolicy );
}

//==============================================================================

xbool eng_IsPresentModeSupported( eng_present_mode Mode )
{
    sdleng_present_policy PresentPolicy;

    if( !sdleng_core_ToSDLPresentPolicy( Mode, PresentPolicy ) )
    {
        return FALSE;
    }

    return sdleng_IsPresentPolicySupported( PresentPolicy );
}

//==============================================================================

eng_present_mode eng_GetPresentMode( void )
{
    switch( sdleng_GetPresentPolicy() )
    {
        case SDLENG_PRESENT_VSYNC:     return ENG_PRESENT_VSYNC;     break;
        case SDLENG_PRESENT_MAILBOX:   return ENG_PRESENT_MAILBOX;   break;
        case SDLENG_PRESENT_IMMEDIATE: return ENG_PRESENT_IMMEDIATE; break;
        default:                       return ENG_PRESENT_VSYNC;     break;
    }
}

//==============================================================================
//  NATIVE WINDOW ACCESS
//==============================================================================

void eng_SetWindowHandle( eng_native_window_handle hWindow )
{
    if( s.bInitialized && hWindow && ( sdleng_WindowGetHandle() != hWindow ) )
    {
        x_DebugMsg( "SDLEngine: window handle cannot be changed after GPU init\n" );
        return;
    }

    sdleng_WindowSetHandle( hWindow );
}

//==============================================================================

void eng_SetParentWindowHandle( eng_native_window_handle hWindow )
{
    sdleng_WindowSetParentHandle( hWindow );
}

//==============================================================================

eng_native_window_handle eng_GetWindowHandle( void )
{
    return sdleng_WindowGetHandle();
}

//==============================================================================

eng_native_instance_handle eng_GetNativeInstance( void )
{
    return sdleng_WindowGetInstance();
}

//==============================================================================

void eng_UpdateDisplayWindow( eng_native_window_handle hWindow )
{
    sdleng_WindowSetDisplayHandle( hWindow );
    sdleng_WindowRefreshClientSize();
    sdleng_core_UpdateDisplayViewport();
}

//==============================================================================
#if defined(TARGET_WINDOWS)
//==============================================================================

LRESULT CALLBACK eng_D3DWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}
#endif

//==============================================================================
//  DISPLAY RESOLUTION
//==============================================================================

void eng_SetResolution( s32 Width, s32 Height )
{
    ASSERT( Width > 0 );
    ASSERT( Width < 40000 );
    ASSERT( Height > 0 );
    ASSERT( Height < 40000 );

    if( s.bFrameActive || s.bRenderTaskActive || sdleng_InFrame() )
    {
        sdleng_core_CapturePendingDisplaySettings();
        s.PendingDisplayWidth  = Width;
        s.PendingDisplayHeight = Height;
        return;
    }

    if( !sdleng_core_ApplyDisplaySettings( Width,
                                           Height,
                                           sdleng_core_ToSDLDisplayMode( eng_GetDisplayMode() ) ) )
    {
        x_DebugMsg( "SDLEngine: failed to change resolution to %dx%d\n", Width, Height );
    }
}

//==============================================================================
//  LIFETIME
//==============================================================================

void eng_Init( void )
{
    x_DebugMsg( "=== SDL ENGINE INITIALIZATION START ===\n" );

    s32 InitXRes = 0;
    s32 InitYRes = 0;
    sdleng_WindowGetResolution( InitXRes, InitYRes );

    sdleng_window_desc WindowDesc;
    x_memset( &WindowDesc, 0, sizeof( WindowDesc ) );
    WindowDesc.hInstance     = sdleng_WindowGetInstance();
    WindowDesc.hWindow       = sdleng_WindowGetHandle();
    WindowDesc.hParentWindow = sdleng_WindowGetParentHandle();
    WindowDesc.Width         = InitXRes;
    WindowDesc.Height        = InitYRes;
    WindowDesc.pTitle        = "Dreamlnd 51";
    WindowDesc.DisplayMode   = sdleng_WindowGetDisplayMode();

    sdleng_WindowSetResizeCallback( sdleng_core_OnWindowResized, NULL );

    if( !sdleng_WindowInit( WindowDesc ) )
    {
        x_DebugMsg( "SDLEngine: window initialization failed\n" );
        return;
    }

    if( !sdleng_CreateDeviceForWindow( sdleng_WindowGetHandle(), InitXRes, InitYRes ) )
    {
        x_DebugMsg( "SDLEngine: GPU device initialization failed\n" );
        sdleng_WindowKill();
        return;
    }

    sdleng_WindowShow();

    rtarget_Init();
    s.bRenderTargetsInitialized = TRUE;

    rstate_Init();
    s.bRenderStatesInitialized = TRUE;

    shader_Init();
    s.bShadersInitialized = TRUE;

    vram_Init();
    s.bVRAMInitialized = TRUE;

    composite_Init();
    s.bCompositeInitialized = TRUE;

    smem_Init( SCRACH_MEM_SIZE );
    s.bScratchInitialized = TRUE;

    input_init_desc InputDesc;
    InputDesc.pWindow = (void*)sdleng_WindowGetInputHandle();

    if( g_Input.Init( InputDesc ) )
    {
        s.bInputInitialized = TRUE;
    }
    else
    {
        x_DebugMsg( "SDLEngine: input initialization failed\n" );
    }

    s.bInitialized       = TRUE;
    s.bFrameActive       = FALSE;
    s.bRenderTaskActive  = FALSE;
    s.bProfileTaskActive = FALSE;
    x_memset( s.CPUFrameIntervals,       0, sizeof( s.CPUFrameIntervals ) );
    x_memset( s.SubmittedFrameIntervals, 0, sizeof( s.SubmittedFrameIntervals ) );
    s.CPUFrameLastTime        = 0;
    s.SubmittedFrameLastTime  = 0;
    s.CPUFrameIndex           = 0;
    s.SubmittedFrameIndex     = 0;
    s.CPUFrameRate            = 0.0f;
    s.SubmittedFrameRate      = 0.0f;
    s.FramePacingWaitMs       = 0.0f;
    s.RenderSubmitMs          = 0.0f;
    s.bDisplaySettingsPending = FALSE;
    s.bPresentPolicyPending   = FALSE;

    sdleng_WindowSetReady( TRUE );

    x_DebugMsg( "=== SDL ENGINE INITIALIZATION COMPLETE ===\n" );
}

//==============================================================================

void eng_Kill( void )
{
    x_DebugMsg( "=== SDL ENGINE SHUTDOWN START ===\n" );

    sdleng_WindowSetReady( FALSE );

    if( s.bFrameActive || s.bRenderTaskActive || sdleng_InFrame() )
    {
        eng_ResetAfterException();
    }

    s.bRenderTaskActive = FALSE;
    s.bFrameActive      = FALSE;

    if( sdleng_GetDevice() && !sdleng_WaitForIdle() )
    {
        x_DebugMsg( "SDLEngine: GPU idle wait failed before resource teardown\n" );
    }

    if( s.bInputInitialized )
    {
        g_Input.Kill();
        s.bInputInitialized = FALSE;
    }

    if( s.bScratchInitialized )
    {
        smem_Kill();
        s.bScratchInitialized = FALSE;
    }

    if( s.bCompositeInitialized )
    {
        composite_Kill();
        s.bCompositeInitialized = FALSE;
    }

    if( s.bVRAMInitialized )
    {
        vram_Kill();
        s.bVRAMInitialized = FALSE;
    }

    if( s.bRenderStatesInitialized )
    {
        rstate_Kill();
        s.bRenderStatesInitialized = FALSE;
    }

    if( s.bShadersInitialized )
    {
        shader_Kill();
        s.bShadersInitialized = FALSE;
    }

    if( s.bRenderTargetsInitialized )
    {
        rtarget_Kill();
        s.bRenderTargetsInitialized = FALSE;
    }

    sdleng_DestroyDevice();
    sdleng_WindowKill();

    s.bInitialized = FALSE;

    //x_DebugMsg( "=== SDL ENGINE SHUTDOWN COMPLETE ===\n" );
    x_DebugMsg( "=== НУ БЫЛО И БЫЛО ===\n" );
}

//==============================================================================
//  FRAME LOOP
//==============================================================================

xbool eng_BeginFrame( void )
{
    if( !s.bInitialized || !sdleng_GetDevice() )
    {
        return FALSE;
    }

    if( s.bFrameActive || s.bRenderTaskActive || sdleng_InFrame() )
    {
        return sdleng_core_AbortFrame( "eng_BeginFrame called while a frame is active" );
    }

    x_GetProfiler().BeginFrame();

    {
        X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.Begin" );

        while( TRUE )
        {
            if( s.bResizePending )
            {
                if( !rtarget_NotifyResolutionChanged() )
                {
                    x_GetProfiler().CancelFrame();
                    return FALSE;
                }

                s.bResizePending = FALSE;
            }

            if( !sdleng_AcquireCommandBuffer() )
            {
                x_GetProfiler().CancelFrame();
                return FALSE;
            }

            if( !sdleng_AcquireSwapchainTexture() )
            {
                sdleng_CancelFrame();
                x_GetProfiler().CancelFrame();
                return FALSE;
            }

            if( sdleng_GetSwapchainTexture() )
            {
                {
                    X_PROFILE_SCOPE_CATEGORY( "Frame", "WindowEvents" );

                    // Collect input after the blocking acquire, not before it.
                    if( !sdleng_WindowPumpMessages() )
                    {
                        eng_Reboot( REBOOT_QUIT );
                        sdleng_CancelFrame();
                        x_GetProfiler().CancelFrame();
                        return FALSE;
                    }
                }

                // Target recreation requires no active command buffer. A resize
                // received by this batch must be applied before rendering.
                if( s.bResizePending )
                {
                    sdleng_CancelFrame();
                    continue;
                }

                break;
            }

            sdleng_CancelFrame();

            if( !sdleng_WindowWaitForActivity() )
            {
                eng_Reboot( REBOOT_QUIT );
                x_GetProfiler().CancelFrame();
                return FALSE;
            }
        }
    }

    const xtick FrameBoundary = x_GetTime();
    sdleng_core_RecordFrameRate( FrameBoundary,
                                 s.CPUFrameLastTime,
                                 s.CPUFrameIntervals,
                                 s.CPUFrameIndex,
                                 s.CPUFrameRate );

    s.FramePacingWaitMs = sdleng_GetFramePacingWaitMs();
    s.bFrameActive      = TRUE;

    return TRUE;
}

//==============================================================================

eng_present_target_result eng_AcquirePresentTarget( void )
{
    if( !s.bInitialized || !s.bFrameActive || !sdleng_GetDevice() ||
        !sdleng_InFrame() || sdleng_InRenderPass() )
    {
        return ENG_PRESENT_TARGET_FAILED;
    }

    if( sdleng_GetSwapchainTexture() )
    {
        return ENG_PRESENT_TARGET_READY;
    }

    if( !sdleng_AcquireSwapchainTexture() )
    {
        return ENG_PRESENT_TARGET_FAILED;
    }

    s.FramePacingWaitMs = sdleng_GetFramePacingWaitMs();

    return sdleng_GetSwapchainTexture()
               ? ENG_PRESENT_TARGET_READY
               : ENG_PRESENT_TARGET_SUSPENDED;
}

//==============================================================================

xbool eng_EnsurePresentTarget( void )
{
    while( TRUE )
    {
        switch( eng_AcquirePresentTarget() )
        {
            case ENG_PRESENT_TARGET_READY:
            {
                return TRUE;
            }

            break;

            case ENG_PRESENT_TARGET_FAILED:
            {
                return FALSE;
            }

            break;

            case ENG_PRESENT_TARGET_SUSPENDED:
            default:
            {
                s.bPresentTargetPending   = TRUE;
                const xbool bWindowActive = sdleng_WindowWaitForActivity();
                s.bPresentTargetPending   = FALSE;

                if( !bWindowActive )
                {
                    return FALSE;
                }
            }

            break;
        }
    }
}

//==============================================================================

xbool eng_EndFrame( void )
{
    if( !s.bFrameActive || !sdleng_InFrame() )
    {
        x_DebugMsg( "SDLEngine: eng_EndFrame called without an active frame\n" );
        ASSERT( FALSE );
        return FALSE;
    }

    if( s.bRenderTaskActive )
    {
        return sdleng_core_AbortFrame( "eng_EndFrame called with an active render task" );
    }

    if( sdleng_InRenderPass() )
    {
        return sdleng_core_AbortFrame( "eng_EndFrame requires the render pass to be closed" );
    }

    if( !sdleng_WasBackBufferRendered() )
    {
        return sdleng_core_AbortFrame( "eng_EndFrame requires an explicit backbuffer pass" );
    }

    static xprofile_gauge LivePipelineMetric =
        x_GetProfiler().RegisterGauge( "LivePipelines", XPROFILE_UNIT_COUNT, "Renderer" );

    static xprofile_gauge PipelineHandleMetric =
        x_GetProfiler().RegisterGauge( "PipelineHandles", XPROFILE_UNIT_COUNT, "Renderer" );

    static xprofile_gauge BufferMetric =
        x_GetProfiler().RegisterGauge( "SDLResourceBuffers", XPROFILE_UNIT_COUNT, "RendererMemory" );

    static xprofile_gauge TextureMetric =
        x_GetProfiler().RegisterGauge( "SDLResourceTextures", XPROFILE_UNIT_COUNT, "RendererMemory" );

    static xprofile_gauge ShaderMetric =
        x_GetProfiler().RegisterGauge( "SDLResourceShaders", XPROFILE_UNIT_COUNT, "RendererMemory" );

    static xprofile_gauge RenderTargetMetric =
        x_GetProfiler().RegisterGauge( "SDLResourceRenderTargets", XPROFILE_UNIT_COUNT, "RendererMemory" );

    LivePipelineMetric.Set( sdleng_GetPipelineCount() );
    PipelineHandleMetric.Set( sdleng_GetPipelineHandleCount() );
    BufferMetric.Set( sdleng_GetBufferCount() );
    TextureMetric.Set( vram_GetNTextures() );
    ShaderMetric.Set( sdleng_GetShaderCount() );
    RenderTargetMetric.Set( sdleng_GetRenderTargetCount() );

    xbool bSubmitted;
    {
        X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.SubmitPresent" );
        bSubmitted = sdleng_EndFrame();
    }
    const xtick SubmitTime = x_GetTime();
    s.RenderSubmitMs       = sdleng_GetRenderSubmitMs();

    s.bFrameActive      = FALSE;
    s.bRenderTaskActive = FALSE;

    if( !bSubmitted )
    {
        if( sdleng_InFrame() )
        {
            sdleng_CancelFrame();
        }

        x_GetProfiler().CancelFrame();
        return FALSE;
    }

    sdleng_core_RecordFrameRate( SubmitTime,
                                 s.SubmittedFrameLastTime,
                                 s.SubmittedFrameIntervals,
                                 s.SubmittedFrameIndex,
                                 s.SubmittedFrameRate );

    static xprofile_gauge CPUFrameRateMetric =
        x_GetProfiler().RegisterGauge( "CPUFrameRate", XPROFILE_UNIT_NONE, "Frame" );

    static xprofile_gauge SubmittedFrameRateMetric =
        x_GetProfiler().RegisterGauge( "SubmittedFrameRate", XPROFILE_UNIT_NONE, "Frame" );

    static xprofile_gauge FramePacingWaitMetric =
        x_GetProfiler().RegisterGauge( "FramePacingWaitMs", XPROFILE_UNIT_MILLISECONDS, "Frame" );

    static xprofile_gauge RenderSubmitMetric =
        x_GetProfiler().RegisterGauge( "RenderSubmitMs", XPROFILE_UNIT_MILLISECONDS, "Frame" );

    CPUFrameRateMetric.Set( s.CPUFrameRate );
    SubmittedFrameRateMetric.Set( s.SubmittedFrameRate );
    FramePacingWaitMetric.Set( s.FramePacingWaitMs );
    RenderSubmitMetric.Set( s.RenderSubmitMs );

    if( s.bScratchInitialized )
    {
        smem_Toggle();
    }

    x_GetProfiler().EndFrame();
    sdleng_core_ApplyPendingSettings();

    return TRUE;
}

//==============================================================================

xbool eng_Begin( const char* pTaskName )
{
    ASSERT( s.bRenderTaskActive == FALSE );

    if( !s.bInitialized || !s.bFrameActive || !sdleng_InFrame() )
    {
        x_DebugMsg( "SDLEngine: eng_Begin requires an active frame lifecycle\n" );
        return FALSE;
    }

    if( s.bRenderTaskActive )
    {
        return FALSE;
    }

    const char*         pProfileTaskName = ( pTaskName && pTaskName[0] ) ? pTaskName : "unnamed";
    const xprofile_zone ProfileTask =
        x_GetProfiler().RegisterZone( pProfileTaskName, "RenderTask" );

    s.bProfileTaskActive = x_GetProfiler().BeginScope( ProfileTask );
    s.bRenderTaskActive  = TRUE;

    return TRUE;
}

//==============================================================================

void eng_End( void )
{
    if( !s.bRenderTaskActive )
    {
        x_DebugMsg( "SDLEngine: eng_End called without an active render task\n" );
        ASSERT( FALSE );
        return;
    }

    if( s.bProfileTaskActive )
    {
        x_GetProfiler().EndScope();
        s.bProfileTaskActive = FALSE;
    }

    s.bRenderTaskActive = FALSE;
}

//==============================================================================

xbool eng_InBeginEnd( void )
{
    return s.bRenderTaskActive;
}

//==============================================================================

void eng_ResetAfterException( void )
{
    if( s.bProfileTaskActive )
    {
        x_GetProfiler().EndScope();
        s.bProfileTaskActive = FALSE;
    }

    if( sdleng_InRenderPass() )
    {
        rtarget_EndPass();
    }

    sdleng_CancelFrame();
    x_GetProfiler().CancelFrame();

    s.bRenderTaskActive = FALSE;
    s.bFrameActive      = FALSE;

    if( s.bScratchInitialized )
    {
        smem_ResetAfterException();
    }
}

//==============================================================================

void eng_Sync( void )
{
    if( sdleng_InFrame() )
    {
        x_DebugMsg( "SDLEngine: eng_Sync is not valid during an active frame\n" );
        ASSERT( FALSE );
        return;
    }

    if( !sdleng_WaitForIdle() )
    {
        x_DebugMsg( "SDLEngine: eng_Sync failed to reach GPU idle\n" );
    }
}

//==============================================================================
//  FRAME STATUS
//==============================================================================

u32 eng_GetCurrentFrameSlot( void )
{
    return sdleng_GetCurrentFrameSlot();
}

//==============================================================================

u64 eng_GetCurrentFrameSerial( void )
{
    return sdleng_GetCurrentFrameSerial();
}

//==============================================================================

u64 eng_GetFrameLifetimeSerial( void )
{
    return sdleng_GetFrameLifetimeSerial();
}

//==============================================================================

u32 eng_GetFramesInFlight( void )
{
    return sdleng_GetFramesInFlight();
}

//==============================================================================

xbool eng_IsFrameComplete( u64 FrameSerial )
{
    return sdleng_IsFrameComplete( FrameSerial );
}

//==============================================================================
//  VIEW AND DISPLAY
//==============================================================================

void eng_GetRes( s32& XRes, s32& YRes )
{
    sdleng_WindowRefreshClientSize();
    sdleng_WindowGetClientSize( XRes, YRes );

    if( ( XRes <= 0 ) || ( YRes <= 0 ) )
    {
        sdleng_GetBackBufferSize( XRes, YRes );
    }
}

//==============================================================================

void eng_GetPALMode( xbool& PALMode )
{
    PALMode = FALSE;
}

//==============================================================================

void eng_SetBackColor( xcolor Color )
{
    s.BackColor = Color;
}

//==============================================================================

void eng_MaximizeViewport( view& View )
{
    s32 XRes;
    s32 YRes;
    eng_GetRes( XRes, YRes );
    View.SetViewport( 0, 0, XRes, YRes );
}

//==============================================================================

void eng_SetView( const view& View )
{
    s.View = View;
    eng_SetViewport( View );
}

//==============================================================================

const view* eng_GetView( void )
{
    return &s.View;
}

//==============================================================================

void eng_SetViewport( const view& View )
{
    s32 L;
    s32 T;
    s32 R;
    s32 B;
    View.GetViewport( L, T, R, B );

    rdraw_viewport Viewport;
    Viewport.TopLeftX = (f32)L;
    Viewport.TopLeftY = (f32)T;
    Viewport.Width    = (f32)( R - L );
    Viewport.Height   = (f32)( B - T );
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    if( ( Viewport.Width <= 0.0f ) || ( Viewport.Height <= 0.0f ) )
    {
        s.bViewportValid = FALSE;
        x_DebugMsg( "SDLEngine: invalid viewport\n" );
        return;
    }

    s.Viewport       = Viewport;
    s.bViewportValid = TRUE;

    sdleng_core_ApplyViewport();
}

//==============================================================================

f32 eng_GetCPUFrameRate( void )
{
    return s.CPUFrameRate;
}

//==============================================================================

void eng_PrintStats( void )
{
    x_DebugMsg( "CPUFrameRate:%1.1f  SubmittedFrameRate:%1.1f  FramePacingWaitMs:%1.3f  RenderSubmitMs:%1.3f\n",
                s.CPUFrameRate,
                s.SubmittedFrameRate,
                s.FramePacingWaitMs,
                s.RenderSubmitMs );
}

//  HOST SERVICE FUNCTIONS
//==============================================================================

void DebugMessage( const char* FormatStr, ... )
{
    va_list Args;
    va_start( Args, FormatStr );
    xvfs Message( FormatStr, Args );
    x_DebugMsg( "%s", (const char*)Message );
    va_end( Args );
}

//==============================================================================

void eng_Reboot( reboot_reason Reason )
{
    exit( Reason );
}

//==============================================================================

datestamp eng_GetDate( void )
{
#if defined(TARGET_POSIX)
    struct timespec Time;

    if( clock_gettime( CLOCK_REALTIME, &Time ) != 0 )
    {
        return 0;
    }

    return FILETIME_UNIX_EPOCH + ( (datestamp)Time.tv_sec * FILETIME_TICKS_PER_SECOND ) + ( (datestamp)Time.tv_nsec / 100 );
#else
    FILETIME Time;
    GetSystemTimeAsFileTime( &Time );
    ULARGE_INTEGER Value;
    Value.LowPart  = Time.dwLowDateTime;
    Value.HighPart = Time.dwHighDateTime;

    return Value.QuadPart;
#endif
}

//==============================================================================

split_date eng_SplitDate( datestamp DateStamp )
{
#if defined(TARGET_POSIX)
    split_date SplitDate;
    struct tm  Time;
    datestamp  Ticks;
    time_t     Seconds;

    x_memset( &SplitDate, 0, sizeof( SplitDate ) );

    if( DateStamp < FILETIME_UNIX_EPOCH )
    {
        return SplitDate;
    }

    Ticks   = DateStamp - FILETIME_UNIX_EPOCH;
    Seconds = (time_t)( Ticks / FILETIME_TICKS_PER_SECOND );

    if( localtime_r( &Seconds, &Time ) == NULL )
    {
        return SplitDate;
    }

    SplitDate.Year        = (u16)( Time.tm_year + 1900 );
    SplitDate.Month       = (u8)( Time.tm_mon + 1 );
    SplitDate.Day         = (u8)Time.tm_mday;
    SplitDate.Hour        = (u8)Time.tm_hour;
    SplitDate.Minute      = (u8)Time.tm_min;
    SplitDate.Second      = (u8)Time.tm_sec;
    SplitDate.CentiSecond = (u8)( ( Ticks % FILETIME_TICKS_PER_SECOND ) / 100000 );

    return SplitDate;
#else
    SYSTEMTIME     Time;
    SYSTEMTIME     UtcTime;
    ULARGE_INTEGER Value;
    Value.QuadPart           = DateStamp;
    const FILETIME FileTime  = { Value.LowPart, Value.HighPart };
    split_date     SplitDate = {};

    if( !FileTimeToSystemTime( &FileTime, &UtcTime ) ||
        !SystemTimeToTzSpecificLocalTime( NULL, &UtcTime, &Time ) )
    {
        return SplitDate;
    }

    SplitDate.Year        = Time.wYear;
    SplitDate.Month       = (u8)Time.wMonth;
    SplitDate.Day         = (u8)Time.wDay;
    SplitDate.Hour        = (u8)Time.wHour;
    SplitDate.Minute      = (u8)Time.wMinute;
    SplitDate.Second      = (u8)Time.wSecond;
    SplitDate.CentiSecond = (u8)( Time.wMilliseconds / 10 );

    return SplitDate;
#endif
}

//==============================================================================

datestamp eng_JoinDate( const split_date& SplitDate )
{
#if defined(TARGET_POSIX)
    struct tm Time;
    time_t    Seconds;

    x_memset( &Time, 0, sizeof( Time ) );
    Time.tm_year  = SplitDate.Year - 1900;
    Time.tm_mon   = SplitDate.Month - 1;
    Time.tm_mday  = SplitDate.Day;
    Time.tm_hour  = SplitDate.Hour;
    Time.tm_min   = SplitDate.Minute;
    Time.tm_sec   = SplitDate.Second;
    Time.tm_isdst = -1;

    Seconds = mktime( &Time );

    if( Seconds == (time_t)-1 )
    {
        return 0;
    }

    return FILETIME_UNIX_EPOCH + ( (datestamp)Seconds * FILETIME_TICKS_PER_SECOND ) + ( (datestamp)SplitDate.CentiSecond * 100000 );
#else
    SYSTEMTIME Time = {};
    SYSTEMTIME UtcTime;
    FILETIME   FileTime;

    Time.wYear         = SplitDate.Year;
    Time.wMonth        = SplitDate.Month;
    Time.wDay          = SplitDate.Day;
    Time.wHour         = SplitDate.Hour;
    Time.wMinute       = SplitDate.Minute;
    Time.wSecond       = SplitDate.Second;
    Time.wMilliseconds = SplitDate.CentiSecond * 10;

    if( !TzSpecificLocalTimeToSystemTime( NULL, &Time, &UtcTime ) ||
        !SystemTimeToFileTime( &UtcTime, &FileTime ) )
    {
        return 0;
    }

    ULARGE_INTEGER Value;
    Value.LowPart  = FileTime.dwLowDateTime;
    Value.HighPart = FileTime.dwHighDateTime;

    return Value.QuadPart;
#endif
}

//==============================================================================

s32 eng_GetProductCode( void )
{
    return 0;
}

//==============================================================================

const char* eng_GetProductKey( void )
{
    return NULL;
}

//==============================================================================

#if !defined(X_RETAIL) || defined(X_QA)

//==============================================================================
//  SCREENSHOTS
//==============================================================================

void eng_ScreenShot( const char* pFileName, s32 Size )
{
    (void)pFileName;
    (void)Size;
    x_DebugMsg( "SDLEngine: screenshot capture is not implemented\n" );
}

//==============================================================================

xbool eng_ScreenShotActive( void )
{
    return FALSE;
}

//==============================================================================

s32 eng_ScreenShotSize( void )
{
    return 0;
}

//==============================================================================

s32 eng_ScreenShotX( void )
{
    return 0;
}

//==============================================================================

s32 eng_ScreenShotY( void )
{
    return 0;
}

#endif // !defined(X_RETAIL) || defined(X_QA)

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
