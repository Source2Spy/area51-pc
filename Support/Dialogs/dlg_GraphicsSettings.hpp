//==============================================================================
//
//  dlg_GraphicsSettings.hpp
//
//==============================================================================

#ifndef DLG_GRAPHICS_SETTINGS_HPP
#define DLG_GRAPHICS_SETTINGS_HPP

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
//  dlg_graphics_settings
//==============================================================================

extern void     dlg_graphics_settings_register  ( ui_manager* pManager );
extern ui_win*  dlg_graphics_settings_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

enum graphics_settings_controls
{
    IDC_GRAPHICS_FIELD_OF_VIEW_TEXT,
    IDC_GRAPHICS_RENDER_SCALE_TEXT,
    IDC_GRAPHICS_FILM_GRAIN_TEXT,
    IDC_GRAPHICS_TEXTURE_FILTERING_TEXT,
    IDC_GRAPHICS_ANTI_ALIASING_TEXT,

    IDC_GRAPHICS_FIELD_OF_VIEW,
    IDC_GRAPHICS_RENDER_SCALE,
    IDC_GRAPHICS_FILM_GRAIN,
    IDC_GRAPHICS_TEXTURE_FILTERING,
    IDC_GRAPHICS_ANTI_ALIASING,
    IDC_GRAPHICS_ADVANCED,
    IDC_GRAPHICS_BUTTON_APPLY,
};

class dlg_graphics_settings : public ui_dialog
{
public:
                        dlg_graphics_settings ( void );
    virtual            ~dlg_graphics_settings ( void );

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

    ui_slider*          m_pFieldOfView;
    ui_combo*           m_pRenderScale;
    ui_slider*          m_pFilmGrain;
    ui_combo*           m_pTextureFiltering;
    ui_combo*           m_pAntiAliasing;
    ui_button*          m_pAdvanced;
    ui_button*          m_pButtonApply;

    ui_text*            m_pFieldOfViewText;
    ui_text*            m_pRenderScaleText;
    ui_text*            m_pFilmGrainText;
    ui_text*            m_pTextureFilteringText;
    ui_text*            m_pAntiAliasingText;

    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;
    global_settings     m_OriginalSettings;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_GRAPHICS_SETTINGS_HPP
//==============================================================================
