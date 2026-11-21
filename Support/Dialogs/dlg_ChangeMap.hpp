//==============================================================================
//  
//  dlg_ChangeMap.hpp
//  
//==============================================================================

#ifndef DLG_CHANGE_MAP_HPP
#define DLG_CHANGE_MAP_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_maplist.hpp"

#include "dlg_PopUp.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_change_map_register  ( ui_manager* pManager );
extern ui_win*  dlg_change_map_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_change_map : public ui_dialog
{
public:
                        dlg_change_map ( void );
    virtual            ~dlg_change_map ( void );

    xbool               Create                  ( s32                       UserID,
                                                  ui_manager*               pManager,
                                                  ui_manager::dialog_tem*   pDialogTem,
                                                  const irect&              Position,
                                                  ui_win*                   pParent,
                                                  s32                       Flags,
                                                  void*                     pUserData);
    virtual void        Destroy                 ( void );

    virtual void        Render                  ( s32 ox=0, s32 oy=0 );

    virtual void        OnAccept             ( ui_win* pWin );
    virtual void        OnCancel               ( ui_win* pWin );
    virtual void        OnUpdate                ( ui_win* pWin, f32 DeltaTime );

protected:
    ui_frame*           m_pFrame1;
    ui_maplist*         m_pMapList;


    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;

    s32                 m_CurrHL;
};

//==============================================================================
#endif // DLG_CHANGE_MAP_HPP
//==============================================================================
