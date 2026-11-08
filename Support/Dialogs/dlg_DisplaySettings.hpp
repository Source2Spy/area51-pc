//==============================================================================
//
//  dlg_DisplaySettings.hpp
//
//==============================================================================

#ifndef DLG_DISPLAY_SETTINGS_HPP
#define DLG_DISPLAY_SETTINGS_HPP

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
class ui_slider;
class ui_text;

//==============================================================================
//  dlg_display_settings
//==============================================================================

extern void     dlg_display_settings_register  ( ui_manager* pManager );
extern ui_win*  dlg_display_settings_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class dlg_display_settings : public ui_dialog
{
public:
                        dlg_display_settings  ( void );
    virtual            ~dlg_display_settings  ( void );

    xbool               Create                ( s32                       UserID,
                                                ui_manager*               pManager,
                                                ui_manager::dialog_tem*   pDialogTem,
                                                const irect&              Position,
                                                ui_win*                   pParent,
                                                s32                       Flags,
                                                void*                     pUserData );
    virtual void        Destroy               ( void );

    virtual void        Render                ( s32 ox=0, s32 oy=0 );
    virtual void        OnAccept              ( ui_win* pWin );
    virtual void        OnCancel              ( ui_win* pWin );
    virtual void        OnDelete              ( ui_win* pWin );
    virtual void        OnUpdate              ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout        ( void ) { m_bRenderBlackout = TRUE; }

protected:
    void                ApplySettings      ( global_settings& Settings );
    void                BeginSave          ( void );
    void                OpenSavePopup      ( void );
    void                RestoreSettings    ( void );
    void                OnSaveSettingsCB   ( void );

    ui_combo*           m_pDisplayMode;
    ui_combo*           m_pResolution;
    ui_combo*           m_pPresentMode;
    ui_combo*           m_pFrameLimit;
    ui_slider*          m_pBrightness;
    ui_slider*          m_pContrast;
    ui_button*          m_pButtonApply;

    ui_text*            m_pDisplayModeText;
    ui_text*            m_pResolutionText;
    ui_text*            m_pPresentModeText;
    ui_text*            m_pFrameLimitText;
    ui_text*            m_pBrightnessText;
    ui_text*            m_pContrastText;

    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;
    global_settings     m_OriginalSettings;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_DISPLAY_SETTINGS_HPP
//==============================================================================
