//==============================================================================
//  
//  ui_dialog.hpp
//  
//==============================================================================

#ifndef UI_DIALOG_HPP
#define UI_DIALOG_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_win.hpp"
#include "ui_manager.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum dialog_states
{
    DIALOG_STATE_INIT = 0,
    DIALOG_STATE_ACTIVE,
    DIALOG_STATE_SELECT,
    DIALOG_STATE_BACK,
    DIALOG_STATE_ACTIVATE,
    DIALOG_STATE_DELETE,
    DIALOG_STATE_CANCEL,
    DIALOG_STATE_RETRY,
    DIALOG_STATE_TIMEOUT,
    DIALOG_STATE_EXIT,
    DIALOG_STATE_EDIT,
    DIALOG_STATE_CREATE,
    DIALOG_STATE_WAIT_FOR_SAVE_DATA,
    DIALOG_STATE_SAVE_DATA_ERROR,
    DIALOG_STATE_POPUP,
    NUM_DIALOG_STATES
};

//==============================================================================
//  CONSTANTS
//==============================================================================

static s32 const SAFE_ZONE = 0;

//==============================================================================
//  REGISTRATION
//==============================================================================

extern ui_win* ui_dialog_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  CLASSES
//==============================================================================

class ui_dialog : public ui_win
{
    friend ui_manager;

public:
                            ui_dialog           ( void );
    virtual                ~ui_dialog           ( void );
                            
    xbool                   Create              ( s32                       UserID,
                                                  ui_manager*               pManager,
                                                  ui_manager::dialog_tem*   pDialogTem,
                                                  const irect&              Position,
                                                  ui_win*                   pParent,
                                                  s32                       Flags,
                                                  void*                     pUserData = NULL );

    virtual void            Render              ( s32 ox=0, s32 oy=0 );

    virtual void            OnNavigate       ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE);
    virtual void            OnFocusWithin        ( ui_win* pWin );
                            
    void                    SetBackgroundColor  ( xcolor Color );
    xcolor                  GetBackgroundColor  ( void ) const;
                            
    xbool                   GotoControl         ( ui_control* pControl );
    ui_control*             GotoControl         ( s32 iControl = 0 );
                            
    s32                     GetNumControls      ( void ) const;

    ui_manager::dialog_tem* GetTemplate         ( void );

    void                    SetNavText          ( const xwstring& Text );
    void                    SetNavText          ( const xwchar* pText );
    void                    SetNavTextVisible   ( xbool IsVisible );
    const xwstring&         GetNavText          ( void ) const;
    xbool                   IsNavTextVisible    ( void ) const;

    void                    SetState            ( dialog_states State)      { m_State = State; }
    dialog_states           GetState            ( void )                    { return m_State; }
    s32                     GetControl          ( void )                    { return m_CurrentControl; }

    // screen scaling functions
    void                    InitScreenScaling   ( const irect& Position );
    xbool                   UpdateScreenScaling ( f32 DeltaTime, xbool DoWipe = TRUE );

    // global dialog theme functions
    static void             SetTextColorNormal  ( xcolor color)             { m_TextColorNormal    = color; }
    static void             SetTextColorShadow  ( xcolor color)             { m_TextColorShadow    = color; }

protected:
    xbool                   NavigateFocus       ( ui_navigation Code, xbool WrapX, xbool WrapY );

    s32                     m_iElement;
    ui_manager::dialog_tem* m_pDialogTem;
    s32                     m_NavW;
    s32                     m_NavH;
    s32                     m_NavX;
    s32                     m_NavY;
    xarray<ui_win*>         m_NavGraph;
    xcolor                  m_BackgroundColor;
    xbool                   m_InputEnabled;
    void*                   m_pUserData;
    dialog_states           m_State;
    s32                     m_CurrentControl;
    xwstring                m_NavText;
    xbool                   m_IsNavTextVisible;

    // set once for all dialogs
    static xcolor           m_TextColorNormal;
    static xcolor           m_TextColorShadow;

    // dialog scaling controls
    irect                   m_CurrPos;
    irect                   m_RequestedPos;
    irect                   m_StartPos;
    f32                     m_scaleX;
    f32                     m_scaleY;
    f32                     m_totalX;
    f32                     m_scaleCount;
    f32                     m_scaleAngle;

};

extern xcolor   m_TextColorNormal;
extern xcolor   m_TextColorShadow;

//==============================================================================
#endif // UI_DIALOG_HPP
//==============================================================================
