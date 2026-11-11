//==============================================================================
//
//  e_Engine.hpp
//
//==============================================================================

#ifndef E_ENGINE_HPP
#define E_ENGINE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_View.hpp"
#include "e_Text.hpp"
#include "e_VRAM.hpp"
#include "e_Shader.hpp"
#include "e_RenderBuffer.hpp"
#include "e_RenderDraw.hpp"
#include "e_RenderState.hpp"
#include "e_RenderTarget.hpp"
#include "e_Composite.hpp"
#include "e_Input.hpp"
#include "Input/e_InputActions.hpp"
#include "e_ScratchMem.hpp"
#include "e_Audio.hpp"

#include "x_files.hpp"

//==============================================================================
//  TYPES
//==============================================================================

typedef u64 datestamp;

struct split_date
{
    u16     Year;
    u8      Month;
    u8      Day;
    u8      Hour;
    u8      Minute;
    u8      Second;
    u8      CentiSecond;
};

enum eng_mode
{
    ENG_ACT_DEFAULT             = (0),
    ENG_ACT_SOFTWARE            = (1<<1)
};

enum eng_display_mode
{
    ENG_DISPLAY_WINDOWED        = 0,
    ENG_DISPLAY_BORDERLESS      = 1
};

class eng_display_resolution
{
public:
                    eng_display_resolution  ( void );
                    eng_display_resolution  ( s32 Width, s32 Height );

    void            Set                     ( s32 Width, s32 Height );
    s32             GetWidth                ( void ) const;
    s32             GetHeight               ( void ) const;

private:
    s32             m_Width;
    s32             m_Height;
};

enum eng_present_mode
{
    ENG_PRESENT_VSYNC       = 0,
    ENG_PRESENT_MAILBOX     = 1,
    ENG_PRESENT_IMMEDIATE   = 2
};

enum eng_present_target_result
{
    ENG_PRESENT_TARGET_READY     = 0,
    ENG_PRESENT_TARGET_SUSPENDED = 1,
    ENG_PRESENT_TARGET_FAILED    = 2
};

enum reboot_reason
{
    REBOOT_HALT,
    REBOOT_QUIT,
    REBOOT_MANAGE,
    REBOOT_NEWUSER,
    REBOOT_MESSAGE,
    REBOOT_UPDATE,
};

#if defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)
#if defined(TARGET_WINDOWS)
struct HWND__;
struct HINSTANCE__;

typedef HWND__*      eng_native_window_handle;
typedef HINSTANCE__* eng_native_instance_handle;
#else
typedef void*        eng_native_window_handle;
typedef void*        eng_native_instance_handle;
#endif
#endif

//==============================================================================
//  DEFINES
//==============================================================================

#define ENG_MAX_VIEWS   8

//==============================================================================
//  CORE FUNCTIONS
//==============================================================================

void            eng_Init                ( void );
void            eng_Kill                ( void );
void            eng_SetPipelineCacheDirectory( const char* pDirectory );
xbool           eng_SavePipelineCache   ( void );

void            eng_GetRes              ( s32& XRes, s32& YRes );
void            eng_GetPALMode          ( xbool& PALMode );
void            eng_SetBackColor        ( xcolor Color );

void            eng_MaximizeViewport    ( view& View );

void            eng_SetView             ( const view& View );
const view*     eng_GetView             ( void );

#if !defined(X_RETAIL) || defined(X_QA)
void            eng_ScreenShot          ( const char* pFileName = NULL, s32 Size = 1 );
xbool           eng_ScreenShotActive    ( void );
s32             eng_ScreenShotSize      ( void );
s32             eng_ScreenShotX         ( void );
s32             eng_ScreenShotY         ( void );
#endif

xbool           eng_BeginFrame          ( void );
xbool           eng_EndFrame            ( void );
eng_present_target_result
                eng_AcquirePresentTarget( void );
xbool           eng_EnsurePresentTarget ( void );
xbool           eng_Begin               ( const char* pTaskName=NULL );
void            eng_End                 ( void );
xbool           eng_InBeginEnd          ( void );
void            eng_ResetAfterException ( void );
void            eng_Sync                ( void );
u32             eng_GetCurrentFrameSlot ( void );
u64             eng_GetCurrentFrameSerial( void );
u64             eng_GetFrameLifetimeSerial( void );
u32             eng_GetFramesInFlight    ( void );
xbool           eng_IsFrameComplete     ( u64 FrameSerial );

void            eng_SetViewport         ( const view& View );

f32             eng_GetCPUFrameRate     ( void );
void            eng_PrintStats          ( void );

datestamp       eng_GetDate             ( void );
split_date      eng_SplitDate           ( datestamp DateStamp );
datestamp       eng_JoinDate            ( const split_date& DateStamp );

void            eng_Reboot              ( reboot_reason ExitCode );
s32             eng_GetProductCode      ( void );
const char*     eng_GetProductKey       ( void );
xbool           eng_OpenURL             ( const char* pURL );

//==============================================================================
//  DESKTOP HOST FUNCTIONS
//==============================================================================

#if defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)

#if defined(TARGET_WINDOWS)
void            eng_EntryPoint              ( s32&                       argc,
                                              char**&                    argv,
                                              eng_native_instance_handle hInstance,
                                              eng_native_instance_handle hPrevInstance,
                                              char*                      pCmdLine,
                                              s32                        nCmdShow );
#endif

s32             eng_ExitPoint               ( void );

void            eng_SetPresets              ( u32 Mode = ENG_ACT_DEFAULT );
u32             eng_GetMode                 ( void );
void            eng_SetDisplayMode          ( eng_display_mode Mode );
eng_display_mode
                eng_GetDisplayMode          ( void );
xbool           eng_GetDisplayResolutions   ( xarray<eng_display_resolution>& Resolutions );
xbool           eng_IsPresentModeSupported  ( eng_present_mode Mode );
xbool           eng_SetPresentMode          ( eng_present_mode Mode );
eng_present_mode
                eng_GetPresentMode          ( void );

eng_native_window_handle
                eng_GetWindowHandle         ( void );
void            eng_UpdateDisplayWindow     ( eng_native_window_handle hWindow );
void            eng_SetWindowHandle         ( eng_native_window_handle hWindow );
void            eng_SetParentWindowHandle   ( eng_native_window_handle hWindow );
void            eng_SetResolution           ( s32 Width, s32 Height );
eng_native_instance_handle
                eng_GetNativeInstance       ( void );

#if defined(TARGET_WINDOWS)
#define AppMain AppMain( s32 argc, char* argv[] );                           \
                                                                              \
s32 __stdcall WinMain( eng_native_instance_handle hInstance,                  \
                       eng_native_instance_handle hPrevInstance,              \
                       char*                      pCmdLine,                   \
                       s32                        nCmdShow )                  \
{                                                                             \
    s32    argc;                                                              \
    char** argv;                                                              \
                                                                              \
    eng_EntryPoint( argc, argv, hInstance, hPrevInstance, pCmdLine, nCmdShow );\
    x_StartMain( AppMain, argc, argv );                                       \
                                                                              \
    return eng_ExitPoint();                                                   \
}                                                                             \
                                                                              \
void AppMain
#endif

#endif // TARGET_DESKTOP || TARGET_MOBILE

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline eng_display_resolution::eng_display_resolution( void ) :
    m_Width ( 0 ),
    m_Height( 0 )
{
}

//==============================================================================

inline eng_display_resolution::eng_display_resolution( s32 Width, s32 Height ) :
    m_Width ( Width  ),
    m_Height( Height )
{
}

//==============================================================================

inline void eng_display_resolution::Set( s32 Width, s32 Height )
{
    m_Width  = Width;
    m_Height = Height;
}

//==============================================================================

inline s32 eng_display_resolution::GetWidth( void ) const
{
    return m_Width;
}

//==============================================================================

inline s32 eng_display_resolution::GetHeight( void ) const
{
    return m_Height;
}

//==============================================================================
#endif // E_ENGINE_HPP
//==============================================================================
