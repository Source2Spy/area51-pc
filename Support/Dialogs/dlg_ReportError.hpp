//==============================================================================
//  
//  dlg_OnlineConnect.hpp
//  
//==============================================================================

#ifndef DLG_REPORT_ERROR_HPP
#define DLG_REPORT_ERROR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_listbox.hpp"

#include "Dialogs/dlg_PopUp.hpp"

#include "NetworkMgr/NetworkMgr.hpp"

//==============================================================================
//  dlg_report_error
//==============================================================================

extern void     dlg_report_error_register  ( ui_manager* pManager );
extern ui_win*  dlg_report_error_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;

class dlg_report_error : public ui_dialog
{
public:
                        dlg_report_error  ( void );
    virtual            ~dlg_report_error  ( void );
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
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

protected:
    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
};

//==============================================================================
#endif // DLG_report_error_HPP
//==============================================================================
