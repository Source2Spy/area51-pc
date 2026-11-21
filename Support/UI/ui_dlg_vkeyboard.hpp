//==============================================================================
//  
//  ui_dlg_vkeyboard.hpp
//  
//==============================================================================

#ifndef UI_DLG_VKEYBOARD_HPP
#define UI_DLG_VKEYBOARD_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ui_dialog.hpp"
#include "Dialogs/dlg_PopUp.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern ui_win* ui_dlg_vkeyboard_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_vkString;
class ui_control;

//==============================================================================
//  CLASSES
//==============================================================================

class ui_dlg_vkeyboard : public ui_dialog
{
public:
                    ui_dlg_vkeyboard    ( void );
    virtual        ~ui_dlg_vkeyboard    ( void );

    xbool           Create              ( s32                       UserID,
                                          ui_manager*               pManager,
                                          ui_manager::dialog_tem*   pDialogTem,
                                          const irect&              Position,
                                          ui_win*                   pParent,
                                          s32                       Flags,
                                          void*                     pUserData );

    void            Configure           ( xbool bName ) { m_bName = bName; }
    xbool           IsGamepadLayout     ( void ) const { return m_LayoutDevice == ui_input_device::Gamepad; }

    virtual void    Render              ( s32 ox=0, s32 oy=0 );

    virtual void    OnNavigate          ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void    OnPage              ( ui_win* pWin, s32 Direction );

    virtual void    OnAccept            ( ui_win* pWin );
    virtual void    OnCancel            ( ui_win* pWin );
    virtual void    OnDelete            ( ui_win* pWin );
    virtual void    OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    s32             IsValid             ( const xwstring* pString, xbool bIsName );

    virtual void    OnNotify( ui_notification const& Event );

    void            ConnectString       ( xwstring* pString, s32 BufferSize );
    void            SetReturn           ( xbool* pDone, xbool* pOk );

protected:
    void            ApplyInputLayout    ( ui_input_device Device );

    s32             m_iElement;
    s32             m_MaxCharacters;
    ui_vkString*    m_pStringCtrl;
    ui_control*     m_pGamepadDefault;
    xwstring*       m_pString;
    xwstring        m_BackupString;
    xbool*          m_pResultDone;
    xbool*          m_pResultOk;
    dlg_popup*      m_pPopUp;
    s32             m_PopUpResult;
    xbool           m_bName;            // Whether this dialogue exists to enter in a name (as opposed to a password).
    ui_input_device m_LayoutDevice;
    s32             m_LayoutCenterX;
    s32             m_LayoutCenterY;
    s32             m_RepeatKeyIdx;     // -1=none, >=0=index into s_PCKeyMap
    f32             m_KeyRepeatTimer;
};

//==============================================================================
#endif // UI_DLG_VKEYBOARD_HPP
//==============================================================================
