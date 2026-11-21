//==============================================================================
//  
//  ui_win.hpp
//  
//==============================================================================

#ifndef UI_WIN_HPP
#define UI_WIN_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_input.hpp"
#include "ui_notify.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_manager;

//==============================================================================
//  CLASSES
//==============================================================================

class ui_win
{
    friend ui_manager;

public:
    enum flags
    {
        WF_VISIBLE      = 0x00000001,                   // Is visible
        WF_STATIC       = 0x00000002,                   // Is static, should not respond to input
        WF_BORDER       = 0x00000004,                   // Has Border
        WF_TAB          = 0x00000008,                   // Is a page of a tabbed dialog
        
        WF_NO_ACTIVATE  = 0x00000010,                   // Do not activate first control of dialog
        WF_TITLE        = 0x00010000,                   // Has a title.
        WF_DLG_CENTER   = 0x00000020,                   // Center Dialog when it is opened

        WF_DISABLED     = 0x00000100,                   // Is disabled
        WF_INPUTMODAL   = 0x00001000,                   // Is input modal, input stops here
        WF_RENDERMODAL  = 0x00002000,                   // Is render modal, rendering stops here

        WF_BUTTON_LEFT  = 0x00004000,                   // Button needs to be left just.
        WF_BUTTON_RIGHT = 0x00008000,                   // Button needs to be right just.

    };

public:
                            ui_win              ( void );
    virtual                ~ui_win              ( void );

    xbool                   Create              ( s32           UserID,
                                                  ui_manager*   pManager,
                                                  const irect&  Position,
                                                  ui_win*       pParent,
                                                  s32           Flags );

    virtual void            Render              ( s32 ox=0, s32 oy=0 );

    void                    UpdateTree          ( f32 DeltaTime );

    virtual void            SetPosition         ( const irect& Position );
    virtual const irect&    GetPosition         ( void ) const;
    s32                     GetWidth            ( void ) const;
    s32                     GetHeight           ( void ) const;
    ui_win*                 GetWindowAtXY       ( s32 x, s32 y ) const;

    void                    SetFlags            ( s32 Flags );
    s32                     GetFlags            ( void ) const;
    void                    SetFlag             ( s32 Flag, s32 State );
    s32                     GetFlags            ( s32 Flag ) const;

    void                    SetActive           ( xbool State );
    xbool                   IsActive            ( void ) const;
    xbool                   IsFocused           ( void ) const;
    xbool                   IsHovered           ( void ) const;
    xbool                   IsPressed           ( void ) const;
    virtual xbool           CanFocus            ( void ) const;

    virtual void            SetLabel            ( const xwstring&   Text );
    virtual void            SetLabel            ( const xwchar*     Text );
    virtual void            SetLabelColor       ( const xcolor&     color);
    virtual const xcolor&   GetLabelColor       ( void ) const;
    virtual const xwstring& GetLabel            ( void ) const;
    virtual void            SetLabelFlags       ( u32 Flags );
    u32                     GetLabelFlags       ( void ) const;

    void                    SetControlID        ( s32 ID );
    s32                     GetControlID        ( void ) const;

    void                    SetParent           ( ui_win* pParent );
    ui_win*                 GetParent           ( void ) const;

    // Finding Children
    ui_win*                 FindChildByID       ( s32 ID ) const;
    xbool                   IsChildOf           ( ui_win* pParent ) const;

    // Coordinate system conversions
    void                    LocalToScreen       ( s32& x, s32& y ) const;
    void                    ScreenToLocal       ( s32& x, s32& y ) const;
    void                    LocalToScreen       ( irect& r ) const;
    void                    ScreenToLocal       ( irect& r ) const;

    // Messaging functions
    void                    Notify              ( ui_notification_type Type, s32 Value = 0 );
    void                    Notify              ( ui_notification_type Type, xwstring const& Text );
    virtual xbool           OnInput             ( ui_input_event& Event );
    virtual void            OnUpdate            ( ui_win* pWin, f32 DeltaTime );
    virtual void            OnNotify            ( ui_notification const& Event );
    virtual void            OnPointerDown       ( ui_win* pWin, s32 x, s32 y );
    virtual void            OnPointerUp         ( ui_win* pWin, s32 x, s32 y );
    virtual void            OnPointerMove       ( ui_win* pWin, s32 x, s32 y );
    virtual void            OnPointerLeave      ( ui_win* pWin );
    virtual void            OnPointerWheel      ( ui_win* pWin, s32 Delta );
    virtual void            OnFocusGained       ( ui_win* pWin );
    virtual void            OnFocusLost         ( ui_win* pWin );
    virtual void            OnFocusWithin       ( ui_win* pWin );
    virtual void            OnNavigate          ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void            OnAccept            ( ui_win* pWin );
    virtual void            OnCancel            ( ui_win* pWin );
    virtual void            OnDelete            ( ui_win* pWin );
    virtual void            OnHelp              ( ui_win* pWin );
    virtual void            OnAlternate         ( ui_win* pWin );
    virtual void            OnPage              ( ui_win* pWin, s32 Direction );
    virtual void            OnJump              ( ui_win* pWin, s32 Direction );

protected:
    void                Destroy             ( void );

    ui_manager*         m_pManager;         // Pointer to ui manager
    s32                 m_UserID;           // UserID that owns this window

    ui_win*             m_pParent;          // Pointer to parent window
    xarray<ui_win*>     m_Children;         // List of child windows
    s32                 m_Flags;            // Window flags
    xbool               m_IsActive;
    xbool               m_IsFocused;
    xbool               m_IsHovered;
    xbool               m_IsPressed;

    s32                 m_ID;               // Window ID
    irect               m_Position;         // Position of window
    xwstring            m_Label;            // Window Label
    u32                 m_LabelFlags;       // Window Label Flags
    xcolor              m_LabelColor;       // Window label color
    s32                 m_Font;             // Window Font
};

//==============================================================================
#endif // UI_WIN_HPP
//==============================================================================
