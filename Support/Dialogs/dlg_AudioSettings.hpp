//==============================================================================
//
//  dlg_AudioSettings.hpp
//
//==============================================================================

#ifndef DLG_AUDIO_SETTINGS_HPP
#define DLG_AUDIO_SETTINGS_HPP

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
class ui_slider;
class ui_text;

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_audio_settings_register ( ui_manager* pManager );
extern ui_win*  dlg_audio_settings_factory  ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_audio_settings : public ui_dialog
{
public:
                        dlg_audio_settings ( void );
    virtual            ~dlg_audio_settings ( void );

    xbool               Create             ( s32                       UserID,
                                             ui_manager*               pManager,
                                             ui_manager::dialog_tem*   pDialogTem,
                                             const irect&              Position,
                                             ui_win*                   pParent,
                                             s32                       Flags,
                                             void*                     pUserData );
    virtual void        Destroy            ( void );

    virtual void        Render             ( s32 ox=0, s32 oy=0 );
    virtual void        OnNotify           ( ui_notification const& Event );
    virtual void        OnAccept           ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnDelete           ( ui_win* pWin );
    virtual void        OnUpdate           ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout     ( void ) { m_bRenderBlackout = TRUE; }

private:
    void                CommitPreview      ( void );
    void                ApplySettings      ( void );
    void                BeginSave          ( void );
    void                OpenSavePopup      ( void );
    void                RestoreSettings    ( void );
    void                OnSaveSettingsCB   ( void );

private:
    ui_slider*          m_pVolumeSFX;
    ui_slider*          m_pVolumeMusic;
    ui_slider*          m_pVolumeSpeech;
    ui_slider*          m_pVolumeVideo;
    ui_button*          m_pButtonApply;

    ui_text*            m_pVolumeSFXText;
    ui_text*            m_pVolumeMusicText;
    ui_text*            m_pVolumeSpeechText;
    ui_text*            m_pVolumeVideoText;

    global_settings     m_Settings;
    global_settings     m_OriginalSettings;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;
};

//==============================================================================
#endif // DLG_AUDIO_SETTINGS_HPP
//==============================================================================
