//==============================================================================
//  
//  dlg_ProfileOptions.hpp
//  
//==============================================================================

#ifndef DLG_PROFILE_OPTIONS_HPP
#define DLG_PROFILE_OPTIONS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_blankbox.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_check.hpp"
#include "StateMgr/PlayerProfile.hpp"

#include "dlg_PopUp.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

enum profile_options_controls
{
    IDC_PROFILE_OPTIONS_CONTROLS,
    IDC_PROFILE_OPTIONS_AVATAR,
    IDC_PROFILE_OPTIONS_DIFFICULTY,
    IDC_PROFILE_OPTIONS_AUTOSAVE,
    IDC_PROFILE_OPTIONS_ACCEPT,

    IDC_PROFILE_DIFFICULTY_BBOX,
    IDC_PROFILE_DIFFICULTY_SELECT,


    IDC_PROFILE_AUTOSAVE_BBOX,
    IDC_PROFILE_AUTOSAVE_SELECT,

};


enum profile_dlg_types
{
    PROFILE_OPTIONS_NORMAL,
    PROFILE_OPTIONS_PAUSE,
};

//==============================================================================
//  dlg_profile_options
//==============================================================================

extern void     dlg_profile_options_register  ( ui_manager* pManager );
extern ui_win*  dlg_profile_options_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;

class dlg_profile_options : public ui_dialog
{
public:
                        dlg_profile_options       ( void );
    virtual            ~dlg_profile_options       ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    
    void                Configure           ( profile_dlg_types         Type,
                                              xbool                     bCreate );

    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );

    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    void                OnSaveProfileCB     ( void );
    void                OnProfileCreateCB   ( void );

private:
    void                OpenProfileChangedPopup( void );

protected:
    ui_frame*           m_pFrame1;

    ui_button*            m_pButtonControls;
    ui_button*          m_pButtonAvatar;
    ui_button*          m_pButtonDifficulty;
    ui_button*          m_pButtonAutosave;
    ui_button*          m_pButtonCreate;

    ui_blankbox*        m_pDifficultyBBox;
    ui_blankbox*        m_pAutosaveBBox;

    ui_combo*           m_pDifficultySelect;
    ui_combo*           m_pAutosaveSelect;


    s32                 m_CurrHL;

    profile_dlg_types   m_Type;
    xbool               m_bCreate;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpType;
    s32                 m_PopUpResult;
    player_profile      m_OriginalProfile;
};

//==============================================================================
#endif // DLG_PROFILE_OPTIONS_HPP
//==============================================================================
