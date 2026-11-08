#ifndef DLG_ONLINE_CONNECT_HPP
#define DLG_ONLINE_CONNECT_HPP

#include "UI/ui_dialog.hpp"

extern void    dlg_online_connect_register( ui_manager* pManager );
extern ui_win* dlg_online_connect_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class dlg_online_connect : public ui_dialog
{
public:
    virtual xbool       Create                ( s32                       UserID,
                                                ui_manager*               pManager,
                                                ui_manager::dialog_tem*   pDialogTem,
                                                const irect&              Position,
                                                ui_win*                   pParent,
                                                s32                       Flags,
                                                void*                     pUserData );
    virtual void        Render                ( s32 ox=0, s32 oy=0 );
    virtual void        OnCancel              ( ui_win* pWin );
    virtual void        OnUpdate              ( ui_win* pWin, f32 DeltaTime );

private:
    void                Failed                ( void );

    xbool               m_Cancelled;
    xbool               m_Failed;
    f32                 m_Timeout;
};

#endif // DLG_ONLINE_CONNECT_HPP
