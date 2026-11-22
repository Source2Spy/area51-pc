//==============================================================================
//  
//  dlg_AvatarSelect.hpp
//  
//==============================================================================

#ifndef DLG_AVATAR_SELECT_HPP
#define DLG_AVATAR_SELECT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"
#include "StateMgr/PlayerProfile.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_avatar_select_register  ( ui_manager* pManager );
extern ui_win*  dlg_avatar_select_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;
class dlg_popup;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_avatar_select : public ui_dialog
{
public:
                        dlg_avatar_select ( void );
    virtual            ~dlg_avatar_select ( void );

    xbool               Create                  ( s32                       UserID,
                                                  ui_manager*               pManager,
                                                  ui_manager::dialog_tem*   pDialogTem,
                                                  const irect&              Position,
                                                  ui_win*                   pParent,
                                                  s32                       Flags,
                                                  void*                     pUserData);
    virtual void        Destroy                 ( void );

    virtual void        Render                  ( s32 ox=0, s32 oy=0 );

    virtual void        OnNavigate           ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnCancel               ( ui_win* pWin );
    virtual void        OnUpdate                ( ui_win* pWin, f32 DeltaTime );
    virtual void        OnAccept             ( ui_win* pWin );

    void                OnSaveProfileCB          ( void );

    void                EnableBlackout          ( void )                    { m_bRenderBlackout = TRUE; }
protected:
    ui_frame*           m_pFrame1;
    ui_combo*           m_pAvatarSelect;
    ui_button*          m_pButtonAccept;
    xbool               m_bRenderBlackout;
    player_profile      m_OriginalProfile;
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;

private:
    void                BeginSave               ( void );
    void                OpenSavePopup            ( void );
    void                RestoreProfile           ( void );
};

//==============================================================================
#endif // DLG_AVATAR_SELECT_HPP
//==============================================================================
