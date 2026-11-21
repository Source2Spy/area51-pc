//==============================================================================
//  
//  ui_dlg_list.hpp
//  
//==============================================================================

#ifndef UI_DLG_LIST_HPP
#define UI_DLG_LIST_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ui_dialog.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void ui_dlg_list_register( ui_manager* pManager );
extern ui_win* ui_dlg_list_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_listbox;

//==============================================================================
//  CLASSES
//==============================================================================

class ui_dlg_list : public ui_dialog
{
public:
                    ui_dlg_list         ( void );
    virtual        ~ui_dlg_list         ( void );

    xbool           Create              ( s32                       UserID,
                                          ui_manager*               pManager,
                                          ui_manager::dialog_tem*   pDialogTem,
                                          const irect&              Position,
                                          ui_win*                   pParent,
                                          s32                       Flags,
                                          void*                     pUserData );

    virtual void    Render              ( s32 ox=0, s32 oy=0 );

    virtual void    OnPointerDown       ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnNotify( ui_notification const& Event );

    ui_listbox*     GetListBox          ( void );
    void            SetResultPtr        ( s32* pResultPtr );

protected:
    ui_listbox*     m_pList;
    s32*            m_pResultPtr;
};

//==============================================================================
#endif // UI_DLG_LIST_HPP
//==============================================================================
