//==============================================================================
//
//  sdleng_window.cpp
//
//==============================================================================

#include "x_target.hpp"

#if defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_window.hpp"
#include "sdleng_private.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

static const char* s_DefaultTitle = "Dreamlnd 51";

//==============================================================================
//  TYPES
//==============================================================================

struct sdleng_window_state
{
    sdleng_native_instance_handle hInstance;
    sdleng_native_window_handle   hWindow;
    sdleng_native_window_handle   hParentWindow;
    sdleng_native_window_handle   hDisplayWindow;
    SDL_Window*                   pWindow;
    s32                           ClientWidth;
    s32                           ClientHeight;
    s32                           ResolutionWidth;
    s32                           ResolutionHeight;
    sdleng_display_mode           DisplayMode;
    xbool                         Active;
    xbool                         Ready;
    xbool                         ModeChange;
    xbool                         CloseRequested;
    xbool                         ResizePending;
    xbool                         VideoInitialized;
    xbool                         OwnVideoSubsystem;
    xbool                         WrappedNativeWindow;
    xbool                         OwnsWindow;
    sdleng_window_resize_fn*      pResizeCallback;
    void*                         pResizeContext;
    sdleng_window_event_fn*       pEventCallback;
    void*                         pEventContext;
    u64                           EventFrameSerial;

    sdleng_window_state( void )
    {
        x_memset( this, 0, sizeof( sdleng_window_state ) );
        ResolutionWidth  = 1024;
        ResolutionHeight = 768;
        DisplayMode      = SDLENG_DISPLAY_WINDOWED;
        Active           = TRUE;
    }
};

//==============================================================================
//  GLOBAL WINDOW HANDLE
//==============================================================================

SDL_Window* g_pSDLWindow = NULL;

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static sdleng_window_state s_Window;

//==============================================================================
//  LOCAL HELPERS
//==============================================================================

static
xbool sdleng_WindowCanSetPosition( void )
{
    const char* pDriver = SDL_GetCurrentVideoDriver();

    return ( pDriver == NULL ) || ( x_stricmp( pDriver, "wayland" ) != 0 );
}

//==============================================================================

static
xbool sdleng_WindowInitVideo( void )
{
    if( s_Window.VideoInitialized )
    {
        return TRUE;
    }

    if( SDL_WasInit( SDL_INIT_VIDEO ) & SDL_INIT_VIDEO )
    {
        s_Window.VideoInitialized  = TRUE;
        s_Window.OwnVideoSubsystem = FALSE;
        return TRUE;
    }

    if( !SDL_InitSubSystem( SDL_INIT_VIDEO ) )
    {
        sdleng_LogError( "SDLWindow", "SDL_InitSubSystem(SDL_INIT_VIDEO)" );
        return FALSE;
    }

    s_Window.VideoInitialized  = TRUE;
    s_Window.OwnVideoSubsystem = TRUE;

    return TRUE;
}

//==============================================================================

static
void sdleng_WindowUpdateNativeHandle( void )
{
    if( !s_Window.pWindow )
    {
        return;
    }

#if defined(TARGET_WINDOWS)
    SDL_PropertiesID Props = SDL_GetWindowProperties( s_Window.pWindow );
    if( !Props )
    {
        return;
    }

    HWND hWindow = (HWND)SDL_GetPointerProperty( Props,
                                                 SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                                 NULL );
    if( hWindow )
    {
        s_Window.hWindow        = hWindow;
        s_Window.hDisplayWindow = hWindow;
    }
#else
    s_Window.hWindow        = (sdleng_native_window_handle)s_Window.pWindow;
    s_Window.hDisplayWindow = s_Window.hWindow;
#endif
}

//==============================================================================

static
SDL_Window* sdleng_WindowCreateWrapped( sdleng_native_window_handle hWindow )
{
#if defined(TARGET_WINDOWS)
    SDL_PropertiesID Props = SDL_CreateProperties();
    if( !Props )
    {
        sdleng_LogError( "SDLWindow", "SDL_CreateProperties" );
        return NULL;
    }

    if( !SDL_SetPointerProperty( Props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, hWindow ) )
    {
        sdleng_LogError( "SDLWindow", "SDL_SetPointerProperty(SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER)" );
        SDL_DestroyProperties( Props );
        return NULL;
    }

    SDL_Window* pWindow = SDL_CreateWindowWithProperties( Props );
    SDL_DestroyProperties( Props );

    if( !pWindow )
    {
        sdleng_LogError( "SDLWindow", "SDL_CreateWindowWithProperties" );
    }

    return pWindow;
#else
    // On Linux the opaque handle is an SDL_Window supplied by the host.
    return (SDL_Window*)hWindow;
#endif
}

//==============================================================================

static
SDL_Window* sdleng_WindowCreateOwned( const sdleng_window_desc& Desc )
{
    SDL_PropertiesID Props = SDL_CreateProperties();
    if( !Props )
    {
        sdleng_LogError( "SDLWindow", "SDL_CreateProperties" );
        return NULL;
    }

    const s32 Width  = ( Desc.Width > 0 ) ? Desc.Width : 1024;
    const s32 Height = ( Desc.Height > 0 ) ? Desc.Height : 768;

    if( !SDL_SetStringProperty( Props,
                                SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                                Desc.pTitle ? Desc.pTitle : s_DefaultTitle ) ||
        !SDL_SetNumberProperty( Props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, Width ) ||
        !SDL_SetNumberProperty( Props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, Height ) ||
        !SDL_SetBooleanProperty( Props,
                                 SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN,
                                 Desc.DisplayMode == SDLENG_DISPLAY_BORDERLESS ) )
    {
        sdleng_LogError( "SDLWindow", "SDL_Set*Property(window create)" );
        SDL_DestroyProperties( Props );
        return NULL;
    }

    SDL_Window* pWindow = SDL_CreateWindowWithProperties( Props );
    SDL_DestroyProperties( Props );

    if( !pWindow )
    {
        sdleng_LogError( "SDLWindow", "SDL_CreateWindowWithProperties" );
    }

    return pWindow;
}

//==============================================================================

static
xbool sdleng_WindowGetDisplay( SDL_DisplayID& DisplayID )
{
    DisplayID = SDL_GetDisplayForWindow( s_Window.pWindow );
    if( !DisplayID )
    {
        DisplayID = SDL_GetPrimaryDisplay();
    }

    if( !DisplayID )
    {
        sdleng_LogError( "SDLWindow", "SDL_GetDisplayForWindow" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdleng_WindowGetDisplayBounds( SDL_Rect& Bounds )
{
    SDL_DisplayID DisplayID = 0;
    if( !sdleng_WindowGetDisplay( DisplayID ) )
    {
        return FALSE;
    }

    if( !SDL_GetDisplayBounds( DisplayID, &Bounds ) )
    {
        sdleng_LogError( "SDLWindow", "SDL_GetDisplayBounds" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
void sdleng_WindowAddResolution( xarray<sdleng_display_resolution>& Resolutions,
                                 s32                                Width,
                                 s32                                Height )
{
    if( ( Width <= 0 ) || ( Height <= 0 ) )
    {
        return;
    }

    s32 InsertIndex = 0;
    while( InsertIndex < Resolutions.GetCount() )
    {
        sdleng_display_resolution const& Resolution = Resolutions[InsertIndex];
        if( ( Resolution.GetWidth() == Width ) && ( Resolution.GetHeight() == Height ) )
        {
            return;
        }

        if( ( Resolution.GetWidth() > Width ) ||
            ( ( Resolution.GetWidth() == Width ) && ( Resolution.GetHeight() > Height ) ) )
        {
            break;
        }

        InsertIndex++;
    }

    sdleng_display_resolution& Resolution = Resolutions.Insert( InsertIndex );
    Resolution.Set( Width, Height );
}

//==============================================================================

static
void sdleng_WindowNotifyResize( void )
{
    if( s_Window.Active &&
        s_Window.Ready &&
        !s_Window.ModeChange &&
        s_Window.pResizeCallback )
    {
        s_Window.pResizeCallback( s_Window.pResizeContext, s_Window.hWindow );
    }
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================
//  NATIVE INSTANCE ACCESS
//==============================================================================

void sdleng_WindowSetInstance( sdleng_native_instance_handle hInstance )
{
    s_Window.hInstance = hInstance;
}

//==============================================================================

sdleng_native_instance_handle sdleng_WindowGetInstance( void )
{
    return s_Window.hInstance;
}

//==============================================================================
//  WINDOW LIFETIME
//==============================================================================

xbool sdleng_WindowInit( const sdleng_window_desc& Desc )
{
    if( s_Window.pWindow )
    {
        return TRUE;
    }

    if( !sdleng_WindowInitVideo() )
    {
        return FALSE;
    }

    if( Desc.hInstance )
    {
        s_Window.hInstance = Desc.hInstance;
    }

    if( Desc.hWindow )
    {
        s_Window.hWindow = Desc.hWindow;
    }

    if( Desc.hParentWindow )
    {
        s_Window.hParentWindow = Desc.hParentWindow;
    }

    s_Window.DisplayMode         = Desc.DisplayMode;
    s_Window.ResolutionWidth     = ( Desc.Width > 0 ) ? Desc.Width : 1024;
    s_Window.ResolutionHeight    = ( Desc.Height > 0 ) ? Desc.Height : 768;
    s_Window.Ready               = FALSE;
    s_Window.CloseRequested      = FALSE;
    s_Window.WrappedNativeWindow = ( s_Window.hWindow != NULL );
    s_Window.OwnsWindow          = FALSE;
    s_Window.EventFrameSerial    = 0;

    if( s_Window.hWindow )
    {
        s_Window.pWindow = sdleng_WindowCreateWrapped( s_Window.hWindow );
#if defined(TARGET_WINDOWS)
        s_Window.OwnsWindow = TRUE;
#endif
    }
    else
    {
        s_Window.pWindow    = sdleng_WindowCreateOwned( Desc );
        s_Window.OwnsWindow = TRUE;
    }

    if( !s_Window.pWindow )
    {
        sdleng_WindowKill();
        return FALSE;
    }

    g_pSDLWindow = s_Window.pWindow;
    sdleng_WindowUpdateNativeHandle();
    sdleng_WindowRefreshClientSize();

    if( !s_Window.WrappedNativeWindow )
    {
        s32 ClientWidth  = 0;
        s32 ClientHeight = 0;
        if( !sdleng_WindowApplyDisplayMode( s_Window.ResolutionWidth,
                                            s_Window.ResolutionHeight,
                                            s_Window.DisplayMode,
                                            ClientWidth,
                                            ClientHeight ) )
        {
            sdleng_WindowKill();
            return FALSE;
        }
    }
    else
    {
        s_Window.ResolutionWidth  = s_Window.ClientWidth;
        s_Window.ResolutionHeight = s_Window.ClientHeight;
        s_Window.DisplayMode      = SDLENG_DISPLAY_WINDOWED;
    }

    return TRUE;
}

//==============================================================================

void sdleng_WindowKill( void )
{
    if( s_Window.pWindow && s_Window.OwnsWindow )
    {
        SDL_DestroyWindow( s_Window.pWindow );
    }

    s_Window.pWindow = NULL;

    g_pSDLWindow = NULL;

    if( s_Window.VideoInitialized && s_Window.OwnVideoSubsystem )
    {
        SDL_QuitSubSystem( SDL_INIT_VIDEO );
    }

    s_Window.VideoInitialized  = FALSE;
    s_Window.OwnVideoSubsystem = FALSE;

    s_Window.hWindow             = NULL;
    s_Window.hParentWindow       = NULL;
    s_Window.hDisplayWindow      = NULL;
    s_Window.ClientWidth         = 0;
    s_Window.ClientHeight        = 0;
    s_Window.Ready               = FALSE;
    s_Window.ModeChange          = FALSE;
    s_Window.CloseRequested      = FALSE;
    s_Window.WrappedNativeWindow = FALSE;
    s_Window.OwnsWindow          = FALSE;
    s_Window.EventFrameSerial    = 0;
}

//==============================================================================

void sdleng_WindowShow( void )
{
    if( s_Window.pWindow )
    {
        SDL_ShowWindow( s_Window.pWindow );
    }
}

//==============================================================================
//  WINDOW EVENTS
//==============================================================================

void sdleng_WindowProcessEvent( const SDL_Event& Event, SDL_WindowID WindowID )
{
    switch( Event.type )
    {
        case SDL_EVENT_QUIT:
        {
            s_Window.CloseRequested = TRUE;
        }

        break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        {
            if( !WindowID || ( Event.window.windowID == WindowID ) )
            {
                s_Window.CloseRequested = TRUE;
            }
        }

        break;

        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        {
            if( !WindowID || ( Event.window.windowID == WindowID ) )
            {
                s_Window.Active = FALSE;
            }
        }

        break;

        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        {
            if( !WindowID || ( Event.window.windowID == WindowID ) )
            {
                s_Window.Active = TRUE;
            }
        }

        break;

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            if( !WindowID || ( Event.window.windowID == WindowID ) )
            {
                s_Window.Active        = TRUE;
                s_Window.ResizePending = TRUE;
            }
        }

        break;
    }
}

//==============================================================================

static
void sdleng_WindowDispatchEvent( const SDL_Event& Event, SDL_WindowID WindowID )
{
    sdleng_WindowProcessEvent( Event, WindowID );

    if( s_Window.pEventCallback )
    {
        s_Window.pEventCallback( s_Window.pEventContext, Event, WindowID );
    }
}

//==============================================================================

static
xbool sdleng_WindowPollMessages( void )
{
    if( !s_Window.VideoInitialized )
    {
        return TRUE;
    }

    SDL_Event    Event;
    SDL_WindowID WindowID = 0;

    if( s_Window.pWindow )
    {
        WindowID = SDL_GetWindowID( s_Window.pWindow );
    }

    while( SDL_PollEvent( &Event ) )
    {
        sdleng_WindowDispatchEvent( Event, WindowID );
    }

    if( s_Window.ResizePending )
    {
        s_Window.ResizePending = FALSE;
        const s32 Width        = s_Window.ClientWidth;
        const s32 Height       = s_Window.ClientHeight;
        sdleng_WindowRefreshClientSize();

        if( ( Width != s_Window.ClientWidth ) || ( Height != s_Window.ClientHeight ) )
        {
            sdleng_WindowNotifyResize();
        }
    }

    return !s_Window.CloseRequested;
}

//==============================================================================

xbool sdleng_WindowPumpMessages( void )
{
    // Render-only frames still service the window; input consumes the same batch.
    // Without an active frame (editor logic, for example), each call stays fresh.
    const u64 FrameSerial = sdleng_GetCurrentFrameSerial();
    if( sdleng_InFrame() && FrameSerial )
    {
        if( s_Window.EventFrameSerial == FrameSerial )
        {
            return !s_Window.CloseRequested;
        }

        s_Window.EventFrameSerial = FrameSerial;
    }

    return sdleng_WindowPollMessages();
}

//==============================================================================

xbool sdleng_WindowWaitForActivity( void )
{
    if( !s_Window.VideoInitialized )
    {
        return TRUE;
    }

    SDL_Event Event;
    if( SDL_WaitEventTimeout( &Event, 100 ) )
    {
        SDL_WindowID WindowID = 0;
        if( s_Window.pWindow )
        {
            WindowID = SDL_GetWindowID( s_Window.pWindow );
        }

        sdleng_WindowDispatchEvent( Event, WindowID );
    }

    // A suspended frame must keep receiving restore/resize/quit events even when
    // its normal event batch has already been collected.
    return sdleng_WindowPollMessages();
}

//==============================================================================
//  NATIVE WINDOW ACCESS
//==============================================================================

SDL_Window* sdleng_WindowGetSDLWindow( void )
{
    return s_Window.pWindow;
}

//==============================================================================

void sdleng_WindowSetHandle( sdleng_native_window_handle hWindow )
{
    s_Window.hWindow = hWindow;
}

//==============================================================================

sdleng_native_window_handle sdleng_WindowGetHandle( void )
{
    return s_Window.hWindow;
}

//==============================================================================

void sdleng_WindowSetParentHandle( sdleng_native_window_handle hWindow )
{
    s_Window.hParentWindow = hWindow;
}

//==============================================================================

sdleng_native_window_handle sdleng_WindowGetParentHandle( void )
{
    return s_Window.hParentWindow;
}

//==============================================================================

sdleng_native_window_handle sdleng_WindowGetInputHandle( void )
{
    return s_Window.hParentWindow ? s_Window.hParentWindow : s_Window.hWindow;
}

//==============================================================================

void sdleng_WindowSetDisplayHandle( sdleng_native_window_handle hWindow )
{
    s_Window.hDisplayWindow = hWindow;
}

//==============================================================================

sdleng_native_window_handle sdleng_WindowGetDisplayHandle( void )
{
    return s_Window.hDisplayWindow;
}

//==============================================================================
//  CLIENT SIZE
//==============================================================================

void sdleng_WindowRefreshClientSize( void )
{
    if( !s_Window.pWindow )
    {
        s_Window.ClientWidth  = 0;
        s_Window.ClientHeight = 0;
        return;
    }

    s32 Width  = 0;
    s32 Height = 0;
    if( !SDL_GetWindowSizeInPixels( s_Window.pWindow, &Width, &Height ) )
    {
        sdleng_LogError( "SDLWindow", "SDL_GetWindowSizeInPixels" );
        Width  = 0;
        Height = 0;
    }

    s_Window.ClientWidth  = Width;
    s_Window.ClientHeight = Height;
    sdleng_WindowUpdateNativeHandle();
}

//==============================================================================

void sdleng_WindowGetClientSize( s32& Width, s32& Height )
{
    Width  = s_Window.ClientWidth;
    Height = s_Window.ClientHeight;
}

//==============================================================================
//  DISPLAY CONFIGURATION
//==============================================================================

void sdleng_WindowSetDisplayMode( sdleng_display_mode Mode )
{
    if( Mode < SDLENG_DISPLAY_WINDOWED || Mode > SDLENG_DISPLAY_BORDERLESS )
    {
        s_Window.DisplayMode = SDLENG_DISPLAY_WINDOWED;
    }
    else
    {
        s_Window.DisplayMode = Mode;
    }
}

//==============================================================================

sdleng_display_mode sdleng_WindowGetDisplayMode( void )
{
    if( s_Window.pWindow )
    {
        SDL_WindowFlags const Flags = SDL_GetWindowFlags( s_Window.pWindow );
        return ( Flags & ( SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN ) )
                   ? SDLENG_DISPLAY_BORDERLESS
                   : SDLENG_DISPLAY_WINDOWED;
    }

    return s_Window.DisplayMode;
}

//==============================================================================

void sdleng_WindowSetResolution( s32 Width, s32 Height )
{
    if( Width > 0 )
    {
        s_Window.ResolutionWidth = Width;
    }

    if( Height > 0 )
    {
        s_Window.ResolutionHeight = Height;
    }
}

//==============================================================================

void sdleng_WindowGetResolution( s32& Width, s32& Height )
{
    Width  = s_Window.ResolutionWidth;
    Height = s_Window.ResolutionHeight;
}

//==============================================================================

xbool sdleng_WindowGetResolutions( xarray<sdleng_display_resolution>& Resolutions )
{
    Resolutions.Clear();

    SDL_DisplayID DisplayID = 0;
    if( !sdleng_WindowGetDisplay( DisplayID ) )
    {
        return FALSE;
    }

    s32               ModeCount = 0;
    SDL_DisplayMode** ppModes   = SDL_GetFullscreenDisplayModes( DisplayID, &ModeCount );
    if( ppModes )
    {
        Resolutions.SetCapacity( ModeCount + 1 );

        for( s32 iMode = 0; iMode < ModeCount; iMode++ )
        {
            SDL_DisplayMode const* pMode = ppModes[iMode];
            if( !pMode )
            {
                continue;
            }

            f32 const PixelDensity = ( pMode->pixel_density > 0.0f ) ? pMode->pixel_density : 1.0f;
            s32 const Width        = static_cast<s32>( ( pMode->w * PixelDensity ) + 0.5f );
            s32 const Height       = static_cast<s32>( ( pMode->h * PixelDensity ) + 0.5f );
            sdleng_WindowAddResolution( Resolutions, Width, Height );
        }

        SDL_free( ppModes );
    }

    SDL_DisplayMode const* pDesktopMode = SDL_GetDesktopDisplayMode( DisplayID );
    if( pDesktopMode )
    {
        f32 const PixelDensity = ( pDesktopMode->pixel_density > 0.0f ) ? pDesktopMode->pixel_density : 1.0f;
        s32 const Width        = static_cast<s32>( ( pDesktopMode->w * PixelDensity ) + 0.5f );
        s32 const Height       = static_cast<s32>( ( pDesktopMode->h * PixelDensity ) + 0.5f );
        sdleng_WindowAddResolution( Resolutions, Width, Height );
    }

    if( Resolutions.GetCount() == 0 )
    {
        sdleng_LogError( "SDLWindow", "SDL display mode enumeration" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

const char* sdleng_WindowGetDisplayModeName( sdleng_display_mode Mode )
{
    switch( Mode )
    {
        case SDLENG_DISPLAY_WINDOWED:   return "Windowed";   break;
        case SDLENG_DISPLAY_BORDERLESS: return "Borderless"; break;
        default:                        return "Unknown";    break;
    }
}

//==============================================================================

xbool sdleng_WindowApplyDisplayMode( s32                 Width,
                                     s32                 Height,
                                     sdleng_display_mode Mode,
                                     s32&                ClientWidth,
                                     s32&                ClientHeight )
{
    if( !s_Window.pWindow )
    {
        return FALSE;
    }

#if defined(TARGET_ANDROID)
    (void)Width;
    (void)Height;
    (void)Mode;

    sdleng_WindowRefreshClientSize();
    ClientWidth               = s_Window.ClientWidth;
    ClientHeight              = s_Window.ClientHeight;
    s_Window.ResolutionWidth  = ClientWidth;
    s_Window.ResolutionHeight = ClientHeight;
    s_Window.DisplayMode      = SDLENG_DISPLAY_WINDOWED;

    return ( ClientWidth > 0 ) && ( ClientHeight > 0 );
#endif

    if( ( Width <= 0 ) || ( Height <= 0 ) ||
        ( Mode < SDLENG_DISPLAY_WINDOWED ) || ( Mode > SDLENG_DISPLAY_BORDERLESS ) )
    {
        return FALSE;
    }

    if( s_Window.WrappedNativeWindow && ( Mode != SDLENG_DISPLAY_WINDOWED ) )
    {
        x_DebugMsg( "SDLWindow: wrapped native windows only support windowed display mode\n" );
        return FALSE;
    }

    if( Mode == SDLENG_DISPLAY_BORDERLESS )
    {
#if defined(TARGET_POSIX)
        if( !SDL_SetWindowFullscreenMode( s_Window.pWindow, NULL ) ||
            !SDL_SetWindowFullscreen( s_Window.pWindow, true ) )
        {
            sdleng_LogError( "SDLWindow", "SDL borderless fullscreen display mode" );
            return FALSE;
        }
#else
        SDL_Rect DisplayBounds;
        if( !sdleng_WindowGetDisplayBounds( DisplayBounds ) )
        {
            return FALSE;
        }

        const xbool bCanSetPosition = sdleng_WindowCanSetPosition();

        if( !SDL_SetWindowBordered( s_Window.pWindow, false ) ||
            ( bCanSetPosition &&
              !SDL_SetWindowPosition( s_Window.pWindow, DisplayBounds.x, DisplayBounds.y ) ) ||
            !SDL_SetWindowSize( s_Window.pWindow, DisplayBounds.w, DisplayBounds.h ) )
        {
            sdleng_LogError( "SDLWindow", "SDL borderless display mode" );
            return FALSE;
        }
#endif
    }
    else
    {
#if defined(TARGET_POSIX)
        const xbool bCanSetPosition = sdleng_WindowCanSetPosition();
        if( !SDL_SetWindowFullscreen( s_Window.pWindow, false ) ||
            !SDL_SetWindowBordered( s_Window.pWindow, true ) ||
            !SDL_SetWindowSize( s_Window.pWindow, Width, Height ) ||
            ( bCanSetPosition &&
              !SDL_SetWindowPosition( s_Window.pWindow,
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED ) ) )
        {
            sdleng_LogError( "SDLWindow", "SDL windowed display mode" );
            return FALSE;
        }
#else
        SDL_DisplayID DisplayID = 0;
        if( !sdleng_WindowGetDisplay( DisplayID ) )
        {
            return FALSE;
        }

        const xbool bCanSetPosition  = sdleng_WindowCanSetPosition();
        s32 const   CenteredPosition = SDL_WINDOWPOS_CENTERED_DISPLAY( DisplayID );
        if( !SDL_SetWindowBordered( s_Window.pWindow, true ) ||
            !SDL_SetWindowSize( s_Window.pWindow, Width, Height ) ||
            ( bCanSetPosition &&
              !SDL_SetWindowPosition( s_Window.pWindow, CenteredPosition, CenteredPosition ) ) )
        {
            sdleng_LogError( "SDLWindow", "SDL windowed display mode" );
            return FALSE;
        }
#endif
    }

    if( !SDL_SyncWindow( s_Window.pWindow ) )
    {
        sdleng_LogError( "SDLWindow", "SDL_SyncWindow" );
        return FALSE;
    }

    sdleng_WindowRefreshClientSize();

    if( sdleng_WindowGetDisplayMode() != Mode )
    {
        x_DebugMsg( "SDLWindow: display mode request was not applied\n" );
        return FALSE;
    }

    s_Window.DisplayMode      = Mode;
    s_Window.ResolutionWidth  = Width;
    s_Window.ResolutionHeight = Height;

    ClientWidth  = s_Window.ClientWidth;
    ClientHeight = s_Window.ClientHeight;

    return TRUE;
}

//==============================================================================
//  WINDOW STATUS AND CALLBACKS
//==============================================================================

void sdleng_WindowSetReady( xbool Ready )
{
    s_Window.Ready = Ready;
}

//==============================================================================

xbool sdleng_WindowIsReady( void )
{
    return s_Window.Ready;
}

//==============================================================================

xbool sdleng_WindowIsActive( void )
{
    return s_Window.Active;
}

//==============================================================================

void sdleng_WindowSetModeChange( xbool Changing )
{
    s_Window.ModeChange = Changing;
}

//==============================================================================

void sdleng_WindowSetResizeCallback( sdleng_window_resize_fn* pCallback, void* pContext )
{
    s_Window.pResizeCallback = pCallback;
    s_Window.pResizeContext  = pContext;
}

//==============================================================================

void sdleng_WindowSetEventCallback( sdleng_window_event_fn* pCallback, void* pContext )
{
    s_Window.pEventCallback = pCallback;
    s_Window.pEventContext  = pContext;
}

//==============================================================================

xbool sdleng_WindowIsCloseRequested( void )
{
    return s_Window.CloseRequested;
}

//==============================================================================
#endif // defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)
//==============================================================================
