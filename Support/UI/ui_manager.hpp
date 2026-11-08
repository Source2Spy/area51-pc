//==============================================================================
//  
//  ui_manager.hpp
//  
//==============================================================================

#ifndef UI_MANAGER_HPP
#define UI_MANAGER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Texture.hpp"
#include "e_Input.hpp"
#include "ui_input.hpp"

//==============================================================================
//  Externals
//==============================================================================

class ui_win;
class ui_font;
class ui_dialog;
class ui_control;

//==============================================================================
//  Types
//==============================================================================
#define BUTTON_SPRITE_WIDTH     18

enum
{
    XBOX_BUTTON_A,
    XBOX_BUTTON_B,
    XBOX_BUTTON_X,
    XBOX_BUTTON_Y,
    XBOX_BUTTON_DPAD_DOWN,
    XBOX_BUTTON_DPAD_LEFT,
    XBOX_BUTTON_DPAD_UP,
    XBOX_BUTTON_DPAD_RIGHT,
    XBOX_BUTTON_DPAD_UPDOWN,
    XBOX_BUTTON_DPAD_LEFTRIGHT,
    XBOX_BUTTON_STICK_RIGHT,
    XBOX_BUTTON_STICK_LEFT,
    XBOX_BUTTON_TRIGGER_L,
    XBOX_BUTTON_TRIGGER_R,
    XBOX_BUTTON_BLACK,
    XBOX_BUTTON_WHITE,
    XBOX_BUTTON_START,

    PS2_BUTTON_CROSS, 
    PS2_BUTTON_SQUARE, 
    PS2_BUTTON_TRIANGLE, 
    PS2_BUTTON_CIRCLE,
    PS2_BUTTON_DPAD_DOWN,
    PS2_BUTTON_DPAD_LEFT,
    PS2_BUTTON_DPAD_UP,
    PS2_BUTTON_DPAD_RIGHT, 
    PS2_BUTTON_DPAD_UPDOWN,
    PS2_BUTTON_DPAD_LEFTRIGHT,
    PS2_BUTTON_STICK_RIGHT, 
    PS2_BUTTON_STICK_LEFT, 
    PS2_BUTTON_L1,
    PS2_BUTTON_L2, 
    PS2_BUTTON_R1, 
    PS2_BUTTON_R2,
    PS2_BUTTON_START,

    KILL_ICON,
    TEAM_KILL_ICON,
    DEATH_ICON,
    FLAG_ICON,
    VOTE_ICON,
    NEW_CREDIT_PAGE,
    CREDIT_TITLE_LINE,
    CREDIT_END,

    NUM_BUTTON_TEXTURES,
};

//==============================================================================
//  Logging
//==============================================================================

extern xstring ui_log;

//==============================================================================
//  ui_manager
//==============================================================================

class ui_manager
{
public:

    enum
    {
        MAX_INPUT_CONTROLLERS = 4,
    };

    //==========================================================================
    //  Templates for dialogs and controls
    //==========================================================================

    struct control_tem
    {
        s32             ControlID;
        const char*     StringID;
        const char*     pClass;
        s32             x, y, w ,h;
        s32             nx, ny, nw, nh;
        s32             Flags;
    };

    struct dialog_tem
    {
        const char*     StringID;
        s32             NavW, NavH;
        s32             nControls;
        control_tem*    pControls;
        s32             FocusControl;
    };

    //==========================================================================
    //  Typedefs for window and dialog factories
    //==========================================================================

    typedef ui_win* (*ui_pfn_winfact)( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );
    typedef ui_win* (*ui_pfn_dlgfact)( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

    //==========================================================================
    //  Input Button Data
    //==========================================================================

    class button
    {
    public:
        xbool       State;
        f32         RepeatDelay;
        f32         RepeatInterval;
        f32         RepeatTimer;
        s32         nPresses;
        s32         nRepeats;
        s32         nReleases;

    public:
                    button              ( void ) { State = 0; nPresses = 0; nRepeats = 0; nReleases = 0; RepeatDelay = 0.200f; RepeatInterval = 0.060f; RepeatTimer = 0.0f; };
                   ~button              ( void ) {};

        void        Clear               ( void )                    { State = 0; nPresses = 0; nRepeats = 0; nReleases = 0; };

        void        SetupRepeat         ( f32 Delay, f32 Interval ) { RepeatDelay = Delay; RepeatInterval = Interval; };
    };

    //==========================================================================
    //  User Data
    //==========================================================================

    struct user
    {
        xbool                   Enabled;
        s32                     Id;
        s32                     ControllerID;
        irect                   Bounds;
        s32                     Data;
        s32                     Height;
        ui_win*                 pCaptureWindow;
        ui_win*                 pPressedWindow;               // Window that owns the current primary press
        ui_win*                 pFocusedWindow;               // Window that currently has keyboard/gamepad focus
        ui_win*                 pHoveredWindow;               // Window currently under the mouse cursor
        ui_input_device         InputDevice;
        input_platform          InputPlatform;
        xstring                 Background;

        xbool                   MouseVisible;                 // TRUE when mouse cursor is visible
        f32                     MouseX;                       // Mouse cursor X in logical coordinates
        f32                     MouseY;                       // Mouse cursor Y in logical coordinates
        f32                     LastMouseX;                   // Last frame mouse cursor X
        f32                     LastMouseY;                   // Last frame mouse cursor Y
        button                  PointerPrimary;

        button                  NavigateUp        [MAX_INPUT_CONTROLLERS];
        button                  NavigateDown      [MAX_INPUT_CONTROLLERS];
        button                  NavigateLeft      [MAX_INPUT_CONTROLLERS];
        button                  NavigateRight     [MAX_INPUT_CONTROLLERS];
        button                  Accept            [MAX_INPUT_CONTROLLERS];
        button                  Cancel            [MAX_INPUT_CONTROLLERS];
        button                  Delete            [MAX_INPUT_CONTROLLERS];
        button                  Alternate         [MAX_INPUT_CONTROLLERS];
        button                  Help              [MAX_INPUT_CONTROLLERS];
        button                  PagePrevious      [MAX_INPUT_CONTROLLERS];
        button                  PageNext          [MAX_INPUT_CONTROLLERS];
        button                  First             [MAX_INPUT_CONTROLLERS];
        button                  Last              [MAX_INPUT_CONTROLLERS];
        xarray<ui_dialog*>      DialogStack;
        u32                     DialogRevision;
    };

    //==========================================================================
    //  Window Class
    //==========================================================================

    struct winclass
    {
        xstring         ClassName;
        ui_pfn_winfact  pFactory;
    };

    //==========================================================================
    //  Graphic Element for UI
    //==========================================================================

    struct element_desc
    {
        const char*       pName;
        const char*       pBitmapName;
        s32               nStates;
        s32               cx;
        s32               cy;
        f32               TexelsPerUIUnit;
    };

    struct element
    {
        xstring           Name;
        rhandle<texture>  Bitmap;
        s32               nStates;
        s32               cx;
        s32               cy;
        xarray<irect>     TextureRects;
        xarray<irect>     LayoutRects;
    };

    //==========================================================================
    //  Background
    //==========================================================================

    struct background
    {
        xstring           Name;
        xstring           BitmapName;
        rhandle<texture>  Bitmap;
    };

    //==========================================================================
    //  Bitmap
    //==========================================================================

    struct bitmap
    {
        xstring           Name;
        xstring           BitmapName;
        rhandle<texture>  Bitmap;
    };

    //==========================================================================
    //  Font
    //==========================================================================

    struct font
    {
        xstring         Name;
        xstring         PathName;
        ui_font*        pFont;
    };

    //==========================================================================
    //  Dialog Class
    //==========================================================================

    struct dialogclass
    {
        xstring         ClassName;
        ui_pfn_dlgfact  pFactory;
        dialog_tem*     pDialogTem;
    };

//==============================================================================
//  Functions
//==============================================================================

protected:
    void            UpdateViewport          ( void );
    void            UpdateButton            ( ui_manager::button& Button, xbool State, f32 DeltaTime );
    void            SetHoveredWindow        ( user* pUser, ui_win* pWin );
    void            UpdateMousePosition     ( user* pUser, s32 ScreenDeltaX, s32 ScreenDeltaY );
    void            SetInputMode            ( user* pUser, ui_input_device Device, input_platform Platform );
    void            RenderNavText           ( const user* pUser ) const;

public:
                    ui_manager              ( void );
                   ~ui_manager              ( void );

    s32             Init                    ( void );
    void            Kill                    ( void );

    s32             LoadBackground          ( const char* pName, const char* pPathName );
    void            UnloadBackground        ( const char* pName );
    s32             FindBackground          ( const char* pName ) const;
    void            RenderBackground        ( const char* pName ) const;
    void            EnableBackground        ( xbool IsEnabled )                                     { m_EnableBackground = IsEnabled; }

    s32             LoadBitmap              ( const char* pName, const char* pPathName );
    void            UnloadBitmap            ( const char* pName );
    s32             FindBitmap              ( const char* pName );
    void            RenderBitmap            ( s32 iBitmap, const irect& Position, xcolor Color = XCOLOR_WHITE ) const;
    void            RenderBitmapUV          ( s32 iBitmap, const irect& Position, const vector2& UV0, const vector2& UV1, xcolor Color = XCOLOR_WHITE ) const;

    s32             LoadElement             ( const element_desc& Desc );
    s32             FindElement             ( const char* pName ) const;
    void            RenderElement           ( s32 iElement, const irect& Position,       s32 State, const xcolor& Color = XCOLOR_WHITE, xbool IsAdditive = FALSE ) const;
    void            RenderElementUV         ( s32 iElement, const irect& Position, const irect& UV, const xcolor& Color = XCOLOR_WHITE, xbool IsAdditive = FALSE ) const;
    void            RenderElementUV         ( s32 iElement, const irect& Position, const vector2& UV0, const vector2& UV1, const xcolor& Color = XCOLOR_WHITE, xbool IsAdditive = FALSE ) const;

    void            BuildTextData           ( const xwchar* pString, struct ui_text_data& Result,
                                               xarray<s32>* pSourceOffsets = NULL ) const;
    s32             LoadFont                ( const char* pName, const char* pPathName );
    xbool           ReloadFonts             ( void );
    s32             FindFont                ( const char* pName ) const;
    ui_font*        GetFont                 ( const char* pName) const;
    ui_font*        GetFont                 ( s32 Index ) const;
    void            RenderText              ( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color, const   char* pString, xbool IgnoreEmbeddedColor = TRUE, xbool UseGradient = TRUE, f32 FlareAmount = R_0  ) const;
    void            RenderText              ( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color, const xwchar* pString, xbool IgnoreEmbeddedColor = TRUE, xbool UseGradient = TRUE, f32 FlareAmount = R_0  ) const;
    void            RenderText              ( s32 iFont, const irect& Position, s32 Flags,       s32     Alpha, const xwchar* pString, xbool IgnoreEmbeddedColor = TRUE, xbool UseGradient = TRUE, f32 FlareAmount = R_0  ) const;
    void            RenderInputText         ( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color, const xwchar* pString, input_platform Platform ) const;
    void            RenderText_Wrap         ( s32 iFont, const irect& Position, s32 Flags, const xcolor& Color, const xwstring& Text,  xbool IgnoreEmbeddedColor = TRUE, xbool UseGradient = TRUE, f32 FlareAmount = R_0  );

    s32             TextWidth               ( s32 iFont, const xwchar* pString, s32 Count = -1 ) const;
    s32             TextHeight              ( s32 iFont, const xwchar* pString, s32 Count = -1 ) const;
    void            TextSize                ( s32 iFont, irect& Rect, const xwchar* pString, s32 Count ) const;
    s32             GetLineHeight           ( s32 iFont ) const;

    void            RenderRect              ( const irect& r, const xcolor& Color, xbool IsWire=TRUE ) const;
    void            RenderGouraudRect       ( const irect& r, const xcolor& c1, const xcolor& c2, const xcolor& c3, const xcolor& c4, xbool IsWire=TRUE, xbool IsAdditive=FALSE ) const;

    xbool           RegisterWinClass        ( const char* ClassName, ui_pfn_winfact pFactory );
    ui_win*         CreateWin               ( s32 UserID, const char* ClassName, const irect& Position, ui_win* pParent, s32 Flags );

    xbool           RegisterDialogClass     ( const char* ClassName, dialog_tem* pDialogTem, ui_pfn_dlgfact pFactory );
    ui_dialog*      OpenDialog              ( s32 UserID, const char* ClassName, irect Position, ui_win* pParent, s32 Flags, void* pUserData = NULL );
    void            EndDialog               ( s32 UserID, xbool ResetCursor = FALSE );
    void            EndUsersDialogs         ( s32 UserID );
    s32             GetNumUserDialogs       ( s32 UserID );
    ui_dialog*      GetTopmostDialog        ( s32 UserID );

    s32             CreateUser              ( s32 ControllerID, const irect& Bounds, s32 Data = 0 );
    void            DeleteUser              ( s32 UserID );
    void            DeleteAllUsers          ( void );
    user*           GetUser                 ( s32 UserID ) const;
    user*           GetUserById             ( s32 UserID ) const;
    s32             GetUserData             ( s32 UserID ) const;
    ui_win*         GetFocusedWindow        ( s32 UserID ) const;
    ui_input_device GetInputDevice          ( s32 UserID ) const;
    input_platform  GetInputPlatform        ( s32 UserID ) const;
    xbool           DispatchInput           ( ui_win* pTarget, ui_input_event& Event );
    void            SetMouseVisible         ( s32 UserID, xbool State );
    xbool           GetMouseVisible         ( s32 UserID ) const;
    void            SetMousePos             ( s32 UserID, s32  x, s32  y );
    void            GetMousePos             ( s32 UserID, s32& x, s32& y ) const;
    void            SetFocusWindow          ( s32 UserID, ui_win* pWin );
    ui_win*         SetCapture              ( s32 UserID, ui_win* pWin );
    void            ReleaseCapture          ( s32 UserID );
    void            SetUserBackground       ( s32 UserID, const char* pName );
    const irect&    GetUserBounds           ( s32 UserID ) const;
    void            SetUserScale            ( f32 Scale );
    f32             GetUserScale            ( void ) const;
    void            EnableUser              ( s32 UserID, xbool State );
    xbool           IsUserEnabled           ( s32 UserID ) const;
    void            SetUserController       ( s32 UserID, s32 ControllerID );

    void            PushClipWindow          ( const irect &r );
    void            PopClipWindow           ( void );

    ui_win*         GetWindowAtXY           ( user* pUser, s32 x, s32 y );
    xbool           ProcessInput            ( f32 DeltaTime );
    xbool           ProcessInput            ( f32 DeltaTime, s32 UserID );

    void            EnableUserInput         ( void );
    void            DisableUserInput        ( void );
    void            SuppressInputUntilRelease( void );

    void            Update                  ( f32 DeltaTime );
    void            Render                  ( void );

    void            WordWrapString          ( s32 iFont, const irect& r, const char* pString, xwstring& RetVal );
    void            WordWrapString          ( s32 iFont, const irect& r, const xwstring& String, xwstring& RetVal );

    void            CheckForEndDialog       ( s32 UserID );

    u32             GetActiveController     ( void )                { return m_ActiveController; }

    f32             GetAlphaTime            ( void )                { return m_AlphaTime; }

    // button icons
    texture*        GetButtonTexture        ( s32 buttonCode );
    s32             LookUpButtonCode        ( const xwchar* pString, s32 iStart, input_device Device, input_platform Platform ) const;

    // Screen wipe
    void            InitScreenWipe          ( ui_dialog* pOwner );
    void            RenderScreenWipe        ( const ui_dialog* pOwner );
    void            UpdateScreenWipe        ( f32 DeltaTime );
    void            ResetScreenWipe         ( void );
    s32             GetWipeRevealY          ( void ) const          { return m_wipeRevealY; }
    xbool           IsWipeActive            ( void ) const          { return m_wipeActive; }
    xbool           IsWipeActiveFor         ( const ui_dialog* pOwner ) const;

    // Refresh bar
    void            InitRefreshBar          ( void );
    void            RenderRefreshBar        ( void );
    void            UpdateRefreshBar        ( f32 deltaTime );

    // Screen controls
    xbool           IsScreenScaling         ( void )                { return m_isScaling; }
    void            SetScreenScaling        ( xbool val )           { m_isScaling = val; }
    void            GetScreenSize           ( irect& size )         { size = m_CurrScreenSize; }
    void            SetScreenSize           ( const irect& size );  
    xbool           IsScreenOn              ( void )                { return( m_ScreenIsOn ); }
    void            SetScreenOn             ( xbool state)          { m_ScreenIsOn = state; }

    // Screen highlight
    void            InitScreenHighlight     ( void );
    void            SetScreenHighlight      ( const irect& pos );
    void            RenderScreenHighlight   ( void );
    void            RenderScreenGlow        ( void );     
    void            EnableScreenHighlight   ( void )                { m_ScreenHighlightEnabled = TRUE; }
    void            DisableScreenHighlight  ( void )                { m_ScreenHighlightEnabled = FALSE; }
    s32             GetHighlightAlpha       ( s32 cycle );
    f32             GetHighlightAlpha       ( void ) const { return m_HighlightAlpha; }

    // Glow bar
    void            InitGlowBar             ( void );
    void            RenderGlowBar           ( void );
    void            UpdateGlowBar           ( f32 deltaTime );
    void            GetGlowBarPos           ( irect& pos )          { pos = m_GlowPos; }

    // Load progress bar
    f32             GetPercentLoaded        ( void )                { return m_PercentLoaded; }
    void            SetPercentLoaded        ( f32 percent );
    void            AddPercentLoaded        ( f32 percent );
    void            RenderProgressBar       ( xbool mustDraw );

    // debugging tools
    void            EnableSafeArea          ( void );
    void            DisableSafeArea         ( void );

    // ping color coding
    s32             PingToColor             ( f32 ping, xcolor& responsecolor );
    
//==============================================================================
//  Data
//==============================================================================

protected:
    void                    DestroyDeferredDialogs( void );
    void                    ClearUserInput        ( void );

    f32                     m_AlphaTime;

    xarray<user*>           m_Users;

    xarray<winclass>        m_WindowClasses;
    xarray<dialogclass>     m_DialogClasses;

    xarray<background*>     m_Backgrounds;
    xarray<bitmap*>         m_Bitmaps;
    xarray<element*>        m_Elements;
    xarray<font*>           m_Fonts;
    xbool                   m_EnableBackground;

    xarray<ui_dialog*>      m_DeferredDialogs;
    s32                     m_CallbackDepth;

    xbool                   m_EnableUserInput;
    xbool                   m_WaitForInputRelease;

    u32                     m_ActiveController;

    // button icons
    rhandle<texture>        m_ButtonTextures[NUM_BUTTON_TEXTURES];

    xbool                   m_isScaling;

    // screen wipe controls
    xbool                   m_wipeActive;
    xbool                   m_wipeFading;
    ui_dialog*              m_pWipeOwner;
    irect                   m_wipeBounds;
    s32                     m_wipeRevealY;
    f32                     m_wipeSpeed;
    f32                     m_wipeHeadY;
    f32                     m_wipeFade;

    // refresh bar controls
    f32                     m_RefreshSpeed;
    f32                     m_RefreshStepAccumulator;
    u32                     m_RefreshWidth;
    irect                   m_RefreshPos;      

    // screen controls
    irect                   m_CurrScreenSize;
    xbool                   m_ScreenIsOn;

    // screen highlight 
    s32                     m_ScreenHighlightID;
    s32                     m_ScreenGlowID;
    irect                   m_ScreenHighlightPos;
    xbool                   m_ScreenHighlightEnabled;
    f32                     m_HighlightAlpha;
    xbool                   m_HighlightFadeUp;


    // glow bar controls
    s32                     m_GlowID;
    s32                     m_GlowStartX;
    s32                     m_GlowEndX;
    f32                     m_GlowSpeed;
    f32                     m_GlowStepAccumulator;
    irect                   m_GlowPos;
    irect                   m_GlowTrail[8];
    xbool                   m_GlowOnTop;

    // progress bar
    f32                     m_PercentLoaded;
    f32                     m_LastProgressUpdatePercent;

    // debugging tools
    xbool                   m_RenderSafeArea;
    xbool                   m_isInitialized;

public:
    xstring*            m_log;
};

extern ui_manager*    g_UiMgr;
extern s32            g_UiUserID;

//==============================================================================
#endif // UI_MANAGER_HPP
//==============================================================================
