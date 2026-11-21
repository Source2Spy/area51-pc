//==============================================================================
//  
//  dlg_Extras.hpp
//  
//==============================================================================

#ifndef DLG_EXTRAS_HPP
#define DLG_EXTRAS_HPP

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

extern void     dlg_extras_register  ( ui_manager* pManager );
extern ui_win*  dlg_extras_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_extras : public ui_dialog
{
public:
                        dlg_extras       ( void );
    virtual            ~dlg_extras       ( void );

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
    
private:    
    void                FillExtrasList      ( void );
    void                PlaySelectedMovie   ( void );

protected:
    ui_listbox*         m_pExtrasList;
    s32                 m_CurrHL;
};

//==============================================================================
#endif // DLG_EXTRAS_HPP
//==============================================================================
