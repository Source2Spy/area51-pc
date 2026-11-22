//==============================================================================
//  
//  dlg_Headset.hpp
//  
//==============================================================================

#ifndef DLG_HEADSET_HPP
#define DLG_HEADSET_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_slider.hpp"
#include "UI/ui_check.hpp"
#include "UI/ui_button.hpp"

#include "StateMgr/StateMgr.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_headset_register  ( ui_manager* pManager );
extern ui_win*  dlg_headset_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;
class dlg_popup;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_headset : public ui_dialog
{
public:
                        dlg_headset         ( void );
    virtual            ~dlg_headset         ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnDelete         ( ui_win* pWin );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout      ( void )                    { m_bRenderBlackout = TRUE; }

protected:
    void                ApplySettings      ( global_settings& Settings );
    void                BeginSave          ( void );
    void                OpenSavePopup      ( void );
    void                RestoreSettings    ( void );
    void                OnSaveSettingsCB   ( void );

    ui_frame*           m_pFrame1;

    ui_combo*           m_pToggleHeadsetAudio;

    ui_slider*          m_pVolumeSpeaker;
    ui_slider*          m_pVolumeMic;

    ui_text*            m_pToggleHeadsetAudioText;
    ui_text*            m_pVolumeSpeakerText;
    ui_text*            m_pVolumeMicText;
   
    ui_button*          m_pButtonAccept;



    xbool               m_bRenderBlackout;

    global_settings     m_OriginalSettings;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_HEADSET_HPP
//==============================================================================
