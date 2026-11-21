//==============================================================================
//
//  dlg_SaveData.hpp
//
//==============================================================================

#ifndef DLG_SAVE_DATA_HPP
#define DLG_SAVE_DATA_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "dlg_PopUp.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum save_data_dialog_operation
{
    SAVE_DATA_DIALOG_CREATE_PROFILE,
    SAVE_DATA_DIALOG_SAVE_SETTINGS,
};

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void    dlg_save_data_register( ui_manager* pManager );
extern ui_win* dlg_save_data_factory ( s32 UserID,
                                       ui_manager* pManager,
                                       ui_manager::dialog_tem* pDialogTem,
                                       const irect& Position,
                                       ui_win* pParent,
                                       s32 Flags,
                                       void* pUserData );

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_save_data : public ui_dialog
{
public:
                    dlg_save_data ( void );
    virtual        ~dlg_save_data ( void );

    xbool           Create        ( s32 UserID,
                                    ui_manager* pManager,
                                    ui_manager::dialog_tem* pDialogTem,
                                    const irect& Position,
                                    ui_win* pParent,
                                    s32 Flags,
                                    void* pUserData );
    virtual void    Destroy       ( void );
    virtual void    OnUpdate      ( ui_win* pWin, f32 DeltaTime );

    void            Configure     ( save_data_dialog_operation Operation );

private:
    void            BeginRequest  ( void );
    void            OnRequestDone ( void );
    void            ShowError     ( void );

    save_data_dialog_operation m_Operation;
    dlg_popup*                 m_pPopup;
    s32                        m_PopupResult;
    xbool                      m_Configured;
};

//==============================================================================

#endif // DLG_SAVE_DATA_HPP
