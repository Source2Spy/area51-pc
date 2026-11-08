//==============================================================================
//
//  dlg_Settings.hpp
//
//==============================================================================

#ifndef DLG_SETTINGS_HPP
#define DLG_SETTINGS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "dlg_PopUp.hpp"
#include "StateMgr/GlobalSettings.hpp"

//==============================================================================
//  CONTROLS
//==============================================================================

enum settings_controls
{
    IDC_SETTINGS_AUDIO,
    IDC_SETTINGS_HEADSET,
    IDC_SETTINGS_GRAPHICS,
    IDC_SETTINGS_DISPLAY,
    IDC_SETTINGS_UI,
    IDC_SETTINGS_LANGUAGE,
    IDC_SETTINGS_ACCEPT,
};

//==============================================================================
//  dlg_settings
//==============================================================================

extern void     dlg_settings_register ( ui_manager* pManager );
extern ui_win*  dlg_settings_factory  ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;
class dlg_popup;

class dlg_settings : public ui_dialog
{
public:
                        dlg_settings       ( void );
    virtual            ~dlg_settings       ( void );

    xbool               Create             ( s32                       UserID,
                                             ui_manager*               pManager,
                                             ui_manager::dialog_tem*   pDialogTem,
                                             const irect&              Position,
                                             ui_win*                   pParent,
                                             s32                       Flags,
                                             void*                     pUserData );
    virtual void        Destroy            ( void );

    virtual void        Render             ( s32 ox=0, s32 oy=0 );
    virtual void        OnAccept           ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnUpdate           ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout     ( void ) { m_bRenderBlackout = TRUE; }
    void                OnSaveSettingsCB   ( void );

private:
    void                BeginSave          ( void );
    void                OpenSavePopup      ( void );
    void                RestoreSettings    ( void );

private:
    ui_button*          m_pAudio;
    ui_button*          m_pHeadset;
    ui_button*          m_pGraphics;
    ui_button*          m_pDisplay;
    ui_button*          m_pUI;
    ui_button*          m_pLanguage;
    ui_button*          m_pAccept;

    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;

    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
    s32                 m_PopUpType;

    global_settings     m_OriginalSettings;
};

//==============================================================================
#endif // DLG_SETTINGS_HPP
//==============================================================================
