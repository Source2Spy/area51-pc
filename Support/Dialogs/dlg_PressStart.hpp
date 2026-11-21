// 
// 
// 
// dlg_PressStart.hpp
// Thu Nov 21 09:45:59 2002
//
//

#ifndef __DLG_PRESSSTART__ 
#define __DLG_PRESSSTART__ 

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_bitmap.hpp"

#include "dlg_PopUp.hpp"

#include "Obj_mgr/obj_mgr.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_press_start_register  ( ui_manager* pManager );
extern ui_win*  dlg_press_start_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_press_start : public ui_dialog
{
public:
                        dlg_press_start     ( void );
    virtual            ~dlg_press_start     ( void );

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
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );
    virtual void        OnHelp            ( ui_win* pWin );

    void                DisableStartButton  ( void );
    void                EnableStartButton   ( void );

protected:
    ui_text*            m_pButtonPressStart;
    ui_bitmap*          m_pLogoBitmap;
    ui_bitmap*          m_pFrameBitmap;

    f32                 m_DemoFadeAlpha;
    f32                 m_FadeControl;
    f32                 m_DemoHoldTimer;
    s32                 m_PressStartState;
    f32                 m_FadeStartInAlpha;
    xbool               m_FadeStartIn;
    f32                 m_FadeAdjust;
    f32                 m_WaitTime;
    f32                 m_Timeout;
    xbool               m_bPlayDemo;
    s32                 m_Element;
    s32                 m_BitmapID;

    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;
    s32                 m_PopUpType;

    s32                 m_BlocksRequired;
};

//==============================================================================
#endif // DLG_PRESSSTART
//==============================================================================