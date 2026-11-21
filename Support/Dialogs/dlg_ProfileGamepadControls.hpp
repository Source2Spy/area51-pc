//==============================================================================
//
//  dlg_ProfileGamepadControls.hpp
//
//==============================================================================

#ifndef DLG_PROFILE_GAMEPAD_CONTROLS_HPP
#define DLG_PROFILE_GAMEPAD_CONTROLS_HPP

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
class ui_check;
class ui_slider;
class ui_text;

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_profile_gamepad_controls_register  ( ui_manager* pManager );
extern ui_win*  dlg_profile_gamepad_controls_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_profile_gamepad_controls : public ui_dialog
{
public:
                        dlg_profile_gamepad_controls ( void );
    virtual            ~dlg_profile_gamepad_controls ( void );

    xbool               Create                       ( s32                       UserID,
                                                       ui_manager*               pManager,
                                                       ui_manager::dialog_tem*   pDialogTem,
                                                       const irect&              Position,
                                                       ui_win*                   pParent,
                                                       s32                       Flags,
                                                       void*                     pUserData );
    virtual void        Destroy                      ( void );

    virtual void        Render                       ( s32 ox=0, s32 oy=0 );
    virtual void        OnAccept                     ( ui_win* pWin );
    virtual void        OnCancel                     ( ui_win* pWin );
    virtual void        OnDelete                     ( ui_win* pWin );
    virtual void        OnUpdate                     ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout               ( void ) { m_bRenderBlackout = TRUE; }

private:
    void                ApplyControls         ( player_profile& Profile );
    void                BeginSave              ( void );
    void                OpenSavePopup          ( void );
    void                RestoreProfile         ( void );
    void                OnSaveProfileCB        ( void );

    ui_slider*          m_pSensitivityX;
    ui_slider*          m_pSensitivityY;
    ui_check*           m_pInvertX;
    ui_check*           m_pInvertY;
    ui_check*           m_pVibration;
    ui_button*          m_pButtonAccept;

    ui_text*            m_pSensitivityXText;
    ui_text*            m_pSensitivityYText;
    ui_text*            m_pInvertXText;
    ui_text*            m_pInvertYText;
    ui_text*            m_pVibrationText;

    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;
    player_profile      m_OriginalProfile;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_PROFILE_GAMEPAD_CONTROLS_HPP
//==============================================================================
