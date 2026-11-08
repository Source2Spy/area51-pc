//==============================================================================
//
//  dlg_UISettings.hpp
//
//==============================================================================

#ifndef DLG_UI_SETTINGS_HPP
#define DLG_UI_SETTINGS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "StateMgr/GlobalSettings.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;
class dlg_popup;
class ui_combo;
class ui_text;

//==============================================================================
//  dlg_ui_settings
//==============================================================================

extern void     dlg_ui_settings_register  ( ui_manager* pManager );
extern ui_win*  dlg_ui_settings_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class dlg_ui_settings : public ui_dialog
{
public:
                        dlg_ui_settings  ( void );
    virtual            ~dlg_ui_settings  ( void );

    xbool               Create            ( s32                       UserID,
                                            ui_manager*               pManager,
                                            ui_manager::dialog_tem*   pDialogTem,
                                            const irect&              Position,
                                            ui_win*                   pParent,
                                            s32                       Flags,
                                            void*                     pUserData );
    virtual void        Destroy           ( void );

    virtual void        Render            ( s32 ox=0, s32 oy=0 );
    virtual void        OnAccept          ( ui_win* pWin );
    virtual void        OnCancel          ( ui_win* pWin );
    virtual void        OnDelete          ( ui_win* pWin );
    virtual void        OnUpdate          ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout    ( void ) { m_bRenderBlackout = TRUE; }

protected:
    void                ApplySettings    ( global_settings& Settings );
    void                BeginSave        ( void );
    void                OpenSavePopup    ( void );
    void                RestoreSettings  ( void );
    void                OnSaveSettingsCB ( void );

    ui_combo*           m_pUIScale;
    ui_combo*           m_pHUDScale;
    ui_button*          m_pButtonApply;

    ui_text*            m_pUIScaleText;
    ui_text*            m_pHUDScaleText;

    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;
    global_settings     m_OriginalSettings;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_UI_SETTINGS_HPP
//==============================================================================
