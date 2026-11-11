//==============================================================================
//
//  sdleng_window.hpp
//
//==============================================================================

#ifndef SDLENG_WINDOW_HPP
#define SDLENG_WINDOW_HPP

#include "x_target.hpp"

#if defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#if defined(TARGET_WINDOWS)
#include <windows.h>
#endif
#include "SDL3/SDL.h"

//==============================================================================
//  TYPES
//==============================================================================

enum sdleng_display_mode
{
    SDLENG_DISPLAY_WINDOWED,
    SDLENG_DISPLAY_BORDERLESS
};

//------------------------------------------------------------------------------

class sdleng_display_resolution
{
public:
                    sdleng_display_resolution( void );

    void            Set                      ( s32 Width, s32 Height );
    s32             GetWidth                 ( void ) const;
    s32             GetHeight                ( void ) const;

private:
    s32 m_Width;
    s32 m_Height;
};

//==============================================================================
//  NATIVE HANDLES AND CALLBACKS
//==============================================================================

#if defined(TARGET_WINDOWS)
typedef HINSTANCE sdleng_native_instance_handle;
typedef HWND      sdleng_native_window_handle;
#else
// Linux uses an SDL_Window pointer as the opaque desktop window handle.
typedef void* sdleng_native_instance_handle;
typedef void* sdleng_native_window_handle;
#endif

typedef void sdleng_window_resize_fn( void* pContext, sdleng_native_window_handle hWindow );
typedef void sdleng_window_event_fn( void* pContext, const SDL_Event& Event, SDL_WindowID WindowID );

//==============================================================================
//  WINDOW DESCRIPTION
//==============================================================================

struct sdleng_window_desc
{
    sdleng_native_instance_handle hInstance;
    sdleng_native_window_handle   hWindow;
    sdleng_native_window_handle   hParentWindow;
    s32                           Width;
    s32                           Height;
    const char*                   pTitle;
    sdleng_display_mode           DisplayMode;
};

//==============================================================================
//  WINDOW API
//==============================================================================

void                sdleng_WindowSetInstance        ( sdleng_native_instance_handle hInstance );
sdleng_native_instance_handle
sdleng_WindowGetInstance( void );

xbool               sdleng_WindowInit               ( const sdleng_window_desc& Desc );
void                sdleng_WindowKill               ( void );
void                sdleng_WindowShow               ( void );
void                sdleng_WindowProcessEvent       ( const SDL_Event& Event,
                                                      SDL_WindowID     WindowID );
xbool               sdleng_WindowPumpMessages       ( void );
xbool               sdleng_WindowWaitForActivity    ( void );

SDL_Window*         sdleng_WindowGetSDLWindow       ( void );
void                sdleng_WindowSetHandle          ( sdleng_native_window_handle hWindow );
sdleng_native_window_handle
sdleng_WindowGetHandle( void );
void                sdleng_WindowSetParentHandle    ( sdleng_native_window_handle hWindow );
sdleng_native_window_handle
sdleng_WindowGetParentHandle( void );
sdleng_native_window_handle
sdleng_WindowGetInputHandle( void );
void                sdleng_WindowSetDisplayHandle   ( sdleng_native_window_handle hWindow );
sdleng_native_window_handle
sdleng_WindowGetDisplayHandle( void );

void                sdleng_WindowRefreshClientSize  ( void );
void                sdleng_WindowGetClientSize      ( s32& Width, s32& Height );

void                sdleng_WindowSetDisplayMode     ( sdleng_display_mode Mode );
sdleng_display_mode sdleng_WindowGetDisplayMode     ( void );
void                sdleng_WindowSetResolution      ( s32 Width, s32 Height );
void                sdleng_WindowGetResolution      ( s32& Width, s32& Height );
xbool               sdleng_WindowGetResolutions     ( xarray<sdleng_display_resolution>& Resolutions );
const char*         sdleng_WindowGetDisplayModeName ( sdleng_display_mode Mode );
xbool               sdleng_WindowApplyDisplayMode   ( s32                 Width,
                                                      s32                 Height,
                                                      sdleng_display_mode Mode,
                                                      s32&                ClientWidth,
                                                      s32&                ClientHeight );

void                sdleng_WindowSetReady           ( xbool Ready );
xbool               sdleng_WindowIsReady            ( void );
xbool               sdleng_WindowIsActive           ( void );
void                sdleng_WindowSetModeChange      ( xbool Changing );
void                sdleng_WindowSetResizeCallback  ( sdleng_window_resize_fn* pCallback,
                                                      void*                    pContext );
void                sdleng_WindowSetEventCallback   ( sdleng_window_event_fn* pCallback,
                                                      void*                   pContext );
xbool               sdleng_WindowIsCloseRequested   ( void );

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline sdleng_display_resolution::sdleng_display_resolution( void ) :
    m_Width ( 0 ),
    m_Height( 0 )
{
}

//==============================================================================

inline void sdleng_display_resolution::Set( s32 Width, s32 Height )
{
    m_Width  = Width;
    m_Height = Height;
}

//==============================================================================

inline s32 sdleng_display_resolution::GetWidth( void ) const
{
    return m_Width;
}

//==============================================================================

inline s32 sdleng_display_resolution::GetHeight( void ) const
{
    return m_Height;
}

#endif // defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)

//==============================================================================
#endif // SDLENG_WINDOW_HPP
//==============================================================================
