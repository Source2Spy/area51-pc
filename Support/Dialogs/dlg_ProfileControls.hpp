//==============================================================================
//
//  dlg_ProfileControls.hpp
//
//==============================================================================

#ifndef DLG_PROFILE_CONTROLS_HPP
#define DLG_PROFILE_CONTROLS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "StateMgr/PlayerProfile.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;
class ui_check;
class ui_text;
class dlg_popup;

//==============================================================================
//  ENUMS
//==============================================================================

enum profile_controls
{
    IDC_CONTROLS_MOUSE_MENU,
    IDC_CONTROLS_GAMEPAD_MENU,
    IDC_CONTROLS_KEYBOARD_MENU,
    IDC_CONTROLS_TOGGLE_CROUCH,
    IDC_CONTROLS_TOGGLE_AIM,
    IDC_CONTROLS_TOGGLE_AUTO_SWITCH,
    IDC_CONTROLS_BUTTON_ACCEPT,
};

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_profile_controls_register  ( ui_manager* pManager );
extern ui_win*  dlg_profile_controls_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_profile_controls : public ui_dialog
{
public:
                        dlg_profile_controls   ( void );
    virtual            ~dlg_profile_controls   ( void );

    xbool               Create                 ( s32                       UserID,
                                                 ui_manager*               pManager,
                                                 ui_manager::dialog_tem*   pDialogTem,
                                                 const irect&              Position,
                                                 ui_win*                   pParent,
                                                 s32                       Flags,
                                                 void*                     pUserData );
    virtual void        Destroy                ( void );

    virtual void        Render                 ( s32 ox=0, s32 oy=0 );
    virtual void        OnAccept               ( ui_win* pWin );
    virtual void        OnCancel               ( ui_win* pWin );
    virtual void        OnDelete               ( ui_win* pWin );
    virtual void        OnUpdate               ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout         ( void ) { m_bRenderBlackout = TRUE; }

private:
    void                ApplyCommonControls   ( player_profile& Profile ) const;
    void                BeginSave               ( void );
    void                OpenSavePopup           ( void );
    void                RestoreProfile          ( void );
    void                OnSaveProfileCB         ( void );

private:
    ui_button*          m_pMouseMenu;
    ui_button*          m_pGamepadMenu;
    ui_button*          m_pKeyboardMenu;
    ui_check*           m_pToggleCrouch;
    ui_check*           m_pToggleAim;
    ui_check*           m_pToggleAutoSwitch;
    ui_button*          m_pButtonAccept;

    ui_text*            m_pCrouchText;
    ui_text*            m_pAimText;
    ui_text*            m_pAutoSwitchText;

    xbool               m_bRenderBlackout;
    player_profile      m_OriginalProfile;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_PROFILE_CONTROLS_HPP
//==============================================================================
