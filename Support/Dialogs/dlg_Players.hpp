#ifndef DLG_PLAYERS_HPP
#define DLG_PLAYERS_HPP

#include "UI/ui_dialog.hpp"
#include "NetworkMgr/NetLimits.hpp"

extern void    dlg_players_register( ui_manager* pManager );
extern ui_win* dlg_players_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_listbox;

class dlg_players : public ui_dialog
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
    virtual void        OnAccept              ( ui_win* pWin );
    virtual void        OnCancel              ( ui_win* pWin );
    virtual void        OnUpdate              ( ui_win* pWin, f32 DeltaTime );
    void                EnableBlackout        ( void ) { m_bRenderBlackout = TRUE; }

private:
    void                FillPlayersList       ( void );

    ui_listbox*         m_pPlayerList;
    xbool               m_bRenderBlackout;
    u64                 m_Identifiers[NET_MAX_PLAYERS];
};

#endif // DLG_PLAYERS_HPP
