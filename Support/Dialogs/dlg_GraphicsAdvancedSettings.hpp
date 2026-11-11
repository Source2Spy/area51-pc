//==============================================================================
//
//  dlg_GraphicsAdvancedSettings.hpp
//
//==============================================================================

#ifndef DLG_GRAPHICS_ADVANCED_SETTINGS_HPP
#define DLG_GRAPHICS_ADVANCED_SETTINGS_HPP

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
class ui_check;
class ui_text;

//==============================================================================
//  dlg_graphics_advanced_settings
//==============================================================================

extern void     dlg_graphics_advanced_settings_register ( ui_manager* pManager );
extern ui_win*  dlg_graphics_advanced_settings_factory  ( s32 UserID,
                                                           ui_manager* pManager,
                                                           ui_manager::dialog_tem* pDialogTem,
                                                           const irect& Position,
                                                           ui_win* pParent,
                                                           s32 Flags,
                                                           void* pUserData );

enum graphics_advanced_settings_controls
{
    IDC_GRAPHICS_ADVANCED_PHONG_SHADING_TEXT,
    IDC_GRAPHICS_ADVANCED_SHADOWS_TEXT,
    IDC_GRAPHICS_ADVANCED_SHADOW_FILTER_TEXT,
    IDC_GRAPHICS_ADVANCED_BACKGROUND_BLUR_TEXT,
    IDC_GRAPHICS_ADVANCED_TESSELLATION_TEXT,
    IDC_GRAPHICS_ADVANCED_AMBIENT_OCCLUSION_TEXT,

    IDC_GRAPHICS_ADVANCED_PHONG_SHADING,
    IDC_GRAPHICS_ADVANCED_SHADOWS,
    IDC_GRAPHICS_ADVANCED_SHADOW_FILTER,
    IDC_GRAPHICS_ADVANCED_BACKGROUND_BLUR,
    IDC_GRAPHICS_ADVANCED_TESSELLATION,
    IDC_GRAPHICS_ADVANCED_AMBIENT_OCCLUSION,
    IDC_GRAPHICS_ADVANCED_BUTTON_APPLY,
};

class dlg_graphics_advanced_settings : public ui_dialog
{
public:
                        dlg_graphics_advanced_settings ( void );
    virtual            ~dlg_graphics_advanced_settings ( void );

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

protected:
    void                ApplySettings                 ( global_settings& Settings );
    void                BeginSave                     ( void );
    void                OpenSavePopup                 ( void );
    void                RestoreSettings               ( void );
    void                OnSaveSettingsCB              ( void );

    ui_combo*           m_pPhongShading;
    ui_combo*           m_pShadows;
    ui_combo*           m_pShadowFilter;
    ui_check*           m_pBackgroundBlur;
    ui_check*           m_pTessellation;
    ui_combo*           m_pAmbientOcclusion;
    ui_button*          m_pButtonApply;

    ui_text*            m_pPhongShadingText;
    ui_text*            m_pShadowsText;
    ui_text*            m_pShadowFilterText;
    ui_text*            m_pBackgroundBlurText;
    ui_text*            m_pTessellationText;
    ui_text*            m_pAmbientOcclusionText;

    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;
    global_settings     m_OriginalSettings;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_GRAPHICS_ADVANCED_SETTINGS_HPP
//==============================================================================
