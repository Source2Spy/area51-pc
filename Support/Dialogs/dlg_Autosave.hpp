//==============================================================================
//  
//  dlg_Autosave.hpp
//  
//==============================================================================

#ifndef DLG_AUTOSAVE_HPP
#define DLG_AUTOSAVE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"

#include "dlg_PopUp.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_autosave_register  ( ui_manager* pManager );
extern ui_win*  dlg_autosave_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_autosave : public ui_dialog
{
public:
                        dlg_autosave       ( void );
    virtual            ~dlg_autosave       ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

protected:
    ui_frame*           m_pFrame1;

    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;

};

//==============================================================================
#endif // DLG_AUTOSAVE_HPP
//==============================================================================
