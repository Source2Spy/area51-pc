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
//  CONTROLS
//==============================================================================

enum audio_settings_controls
{
    IDC_AUDIO_VOLUME_SFX_TEXT,
    IDC_AUDIO_VOLUME_MUSIC_TEXT,
    IDC_AUDIO_VOLUME_SPEECH_TEXT,
    IDC_AUDIO_VOLUME_VIDEO_TEXT,

    IDC_AUDIO_VOLUME_SFX,
    IDC_AUDIO_VOLUME_MUSIC,
    IDC_AUDIO_VOLUME_SPEECH,
    IDC_AUDIO_VOLUME_VIDEO,
    IDC_AUDIO_HEADSET,
    IDC_AUDIO_BUTTON_APPLY,
};

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
    void                ApplySettings      ( global_settings& Settings );
    void                BeginSave          ( void );
    void                OpenSavePopup      ( void );
    void                RestoreSettings    ( void );
    void                OnSaveSettingsCB   ( void );

private:
    ui_slider*          m_pVolumeSFX;
    ui_slider*          m_pVolumeMusic;
    ui_slider*          m_pVolumeSpeech;
    ui_slider*          m_pVolumeVideo;
    ui_button*          m_pHeadset;
    ui_button*          m_pButtonApply;

    ui_text*            m_pVolumeSFXText;
    ui_text*            m_pVolumeMusicText;
    ui_text*            m_pVolumeSpeechText;
    ui_text*            m_pVolumeVideoText;

    global_settings     m_Settings;
    global_settings     m_OriginalSettings;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
    xbool               m_bRenderBlackout;
};

//==============================================================================
#endif // DLG_AUDIO_SETTINGS_HPP
//==============================================================================
