//==============================================================================
//  
//  ui_textbox.hpp
//  
//==============================================================================

#ifndef UI_TEXTBOX_HPP
#define UI_TEXTBOX_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_control.hpp"
#include "ui_scrollbar.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern ui_win* ui_textbox_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

//==============================================================================
//  CLASSES
//==============================================================================

class ui_textbox : public ui_control
{
public:
                    ui_textbox          ( void );
    virtual        ~ui_textbox          ( void );

    xbool           Create              ( s32           UserID,
                                          ui_manager*   pManager,
                                          const irect&  Position,
                                          ui_win*       pParent,
                                          s32           Flags );

    virtual void    SetLabel            ( const xwstring&   Text );

    virtual void    Render              ( s32 ox=0, s32 oy=0 );

    virtual void    SetPosition         ( const irect& Position );

    virtual void    OnNavigate          ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void    OnPage              ( ui_win* pWin, s32 Direction );
    virtual void    OnJump              ( ui_win* pWin, s32 Direction );
    virtual void    OnAccept            ( ui_win* pWin );
    virtual void    OnCancel            ( ui_win* pWin );
    virtual void    OnPointerMove       ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnPointerLeave      ( ui_win* pWin );
    virtual void    OnPointerWheel      ( ui_win* pWin, s32 Delta );
    virtual void    OnPointerDown       ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnPointerUp         ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnUpdate            ( ui_win* pWin, f32 DeltaTime );
    virtual void    OnFocusLost         ( ui_win* pWin );

    void            SetExitOnSelect     ( xbool State ) { m_ExitOnSelect = State; }
    void            SetExitOnBack       ( xbool State ) { m_ExitOnBack = State;   }
                                                        
    void            EnableBorders       ( void )        { m_ShowBorders = TRUE;   }
    void            DisableBorders      ( void )        { m_ShowBorders = FALSE;  }
                                                        
    void            EnableFrame         ( void )        { m_ShowFrame = TRUE;     }
    void            DisableFrame        ( void )        { m_ShowFrame = FALSE;    }

    s32             GetLineCount        ( void ) const;

    void            EnsureVisible       ( s32 iLine );

    void            SetBackgroundColor  ( xcolor Color );
    xcolor          GetBackgroundColor  ( void ) const;

protected:
    xbool           SetFirstVisibleLine ( s32 FirstVisibleLine );
    void            UpdateScrollBar     ( void );

    //-------------------------------------------------------------------------

    xbool           m_ExitOnSelect;
    xbool           m_ExitOnBack;
    xbool           m_ShowBorders;
    xbool           m_ShowFrame;
    s32             m_iElementFrame;
    ui_scrollbar    m_ScrollBar;

    irect           m_TextRect;
    s32             m_LineHeight;
    s32             m_nLines;
    s32             m_nVisibleLines;
    s32             m_iFirstVisibleLine;

    s32             m_Font;
    xcolor          m_BackgroundColor;
};

//==============================================================================
#endif // UI_TEXTBOX_HPP
//==============================================================================
