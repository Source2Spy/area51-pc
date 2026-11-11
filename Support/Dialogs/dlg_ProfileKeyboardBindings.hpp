//==============================================================================
//
//  dlg_ProfileKeyboardBindings.hpp
//
//==============================================================================

#ifndef DLG_PROFILE_KEYBOARD_BINDINGS_HPP
#define DLG_PROFILE_KEYBOARD_BINDINGS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "StateMgr/PlayerProfile.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;
class dlg_popup;
class keyboard_binding_list;

//==============================================================================
//  dlg_profile_keyboard_bindings
//==============================================================================

extern void     dlg_profile_keyboard_bindings_register  ( ui_manager* pManager );
extern ui_win*  dlg_profile_keyboard_bindings_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class dlg_profile_keyboard_bindings : public ui_dialog
{
public:
                        dlg_profile_keyboard_bindings ( void );
    virtual            ~dlg_profile_keyboard_bindings ( void );

    xbool               Create                     ( s32                       UserID,
                                                     ui_manager*               pManager,
                                                     ui_manager::dialog_tem*   pDialogTem,
                                                     const irect&              Position,
                                                     ui_win*                   pParent,
                                                     s32                       Flags,
                                                     void*                     pUserData );
    virtual void        Destroy                    ( void );

    virtual void        Render                     ( s32 ox=0, s32 oy=0 );
    virtual void        OnAccept                   ( ui_win* pWin );
    virtual void        OnCancel                   ( ui_win* pWin );
    virtual void        OnDelete                   ( ui_win* pWin );
    virtual void        OnUpdate                   ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout             ( void ) { m_bRenderBlackout = TRUE; }

    xbool               InputEnabled( void ) const { return (m_State == DIALOG_STATE_ACTIVE) && (m_InputState == INPUT_READY); }
    const ingame_pad::keyboard_bindings& GetBindings( void ) const { return m_EditingProfile.GetKeyboardBindings(); }
    xbool               IsCapturing( s32 Action, s32 Slot ) const;
    virtual void        OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );

private:
    enum input_state
    {
        INPUT_READY,
        INPUT_WAIT_CAPTURE,
        INPUT_CAPTURE,
        INPUT_WAIT_RELEASE,
    };

    void                SetButtonsEnabled      ( xbool Enabled );
    void                EndCapture             ( void );
    void                OpenConflictPopup      ( void );
    void                BeginSave              ( void );
    void                OpenSavePopup          ( void );
    void                RestoreProfile         ( void );
    void                OnSaveProfileCB        ( void );

    keyboard_binding_list* m_pBindings;
    ui_button*          m_pButtonReset;
    ui_button*          m_pButtonAccept;
    input_state         m_InputState;
    s32                 m_CaptureAction;
    s32                 m_CaptureSlot;
    input_gadget        m_CapturedKey;
    xbool               m_ConflictPending;
    xbool               m_ConflictPopup;
    player_profile      m_EditingProfile;
    xbool               m_bRenderBlackout;
    s32                 m_CurrHL;
    player_profile      m_OriginalProfile;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_PROFILE_KEYBOARD_BINDINGS_HPP
//==============================================================================
