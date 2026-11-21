//==============================================================================
//  
//  dlg_MultiPlayer.hpp
//  
//==============================================================================

#ifndef DLG_MULTI_PLAYER_HPP
#define DLG_MULTI_PLAYER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_blankbox.hpp"
#include "dlg_PopUp.hpp"

#include "StateMgr/StateMgr.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum multi_player_controls
{
    IDC_MULTI_PLAYER_ONE_DETAILS,
    IDC_MULTI_PLAYER_TWO_DETAILS,
    IDC_MULTI_PLAYER_ONE_COMBO,
    IDC_MULTI_PLAYER_TWO_COMBO,
    IDC_MULTI_PLAYER_ONE_TEXT,
    IDC_MULTI_PLAYER_TWO_TEXT,
};

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_multi_player_register  ( ui_manager* pManager );
extern ui_win*  dlg_multi_player_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_multi_player : public ui_dialog
{
public:
                        dlg_multi_player       ( void );
    virtual            ~dlg_multi_player       ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnNavigate       ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnDelete         ( ui_win* pWin );
    virtual void        OnAlternate       ( ui_win* pWin );
    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    void                RefreshProfileList  ( void );

    void                OnPollReturn        ( void );
    void                OnLoadProfileCB     ( void );

protected:
    ui_frame*           m_pFrame1;

    ui_blankbox*        m_pPlayerOneBox;
    ui_blankbox*        m_pPlayerTwoBox;

    ui_combo*           m_pPlayerOneCombo;
    ui_combo*           m_pPlayerTwoCombo;

    ui_text*            m_pPlayerOneText;
    ui_text*            m_pPlayerTwoText;


    s32                 m_CreatePlayerIndex;                // player who is creating the profile
    s32                 m_CreateIndex;                      // index in the list where the create profile option is located

    s32                 m_CurrHL;

    xwstring            m_ProfileName;
    xbool               m_ProfileEntered;
    xbool               m_ProfileOK;
    xbool               m_PlayerProfileReady[2];

    dlg_popup*          m_PopUp;
    s32                 m_PopUpResult;

    xbool               m_bEditProfile;
};

//==============================================================================
#endif // DLG_MULTI_PLAYER_HPP
//==============================================================================
