//==============================================================================
//  
//  dlg_LevelSelect.hpp
//  
//==============================================================================

#ifndef DLG_RESUME_GAME_HPP
#define DLG_RESUME_GAME_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_listbox.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_resume_game_register  ( ui_manager* pManager );
extern ui_win*  dlg_resume_game_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_resume_game : public ui_dialog
{
public:
                        dlg_resume_game       ( void );
    virtual            ~dlg_resume_game       ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnNotify( ui_notification const& Event );
    virtual void        OnNavigate       ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );
    
    void                FillCheckpointList  ( void );

protected:
    ui_listbox*         m_pCheckpointList;
    s32                 m_CurrHL;
};

//==============================================================================
#endif // DLG_RESUME_GAME_HPP
//==============================================================================
