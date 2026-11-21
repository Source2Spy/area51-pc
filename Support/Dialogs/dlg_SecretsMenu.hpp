//==============================================================================
//  
//  dlg_SecretsMenu.hpp
//  
//==============================================================================

#ifndef DLG_SECRETS_MENU_HPP
#define DLG_SECRETS_MENU_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_blankbox.hpp"
#include "UI/ui_textbox.hpp"

#include "StateMgr/SecretList.hpp"

#include "Dialogs/dlg_PopUp.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern void     dlg_secrets_menu_register  ( ui_manager* pManager );
extern ui_win*  dlg_secrets_menu_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_button;

//==============================================================================
//  CLASSES
//==============================================================================

class dlg_secrets_menu : public ui_dialog
{
public:
                        dlg_secrets_menu       ( void );
    virtual            ~dlg_secrets_menu       ( void );

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
    virtual void        OnPointerDown       ( ui_win* pWin, s32 x, s32 y );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    void                InitIconScaling     ( xbool ScaleDown );
    xbool               UpdateIconScaling   ( f32 DeltaTime );

    void                PopulateSecretsDetails ( xbool bVaultDetails );

protected:
    ui_frame*               m_pFrame1;

    ui_blankbox*            m_pSecretsMain;
    ui_blankbox*            m_pSecretsDetails;

    ui_combo*               m_pSecretsSelect;

    ui_button*              m_pSecretsButton[5];

    ui_text*                m_pSecretsLine1;
    ui_text*                m_pSecretsLine2;
    ui_text*                m_pSecretsLine3;

    ui_textbox*             m_pTextBox;


    dlg_popup*              m_PopUp;

    s32                     m_SecretsIconID[5];

    s32                     m_CurrHL;

    f32                     m_ScreenScaleX;
    f32                     m_ScreenScaleY;

    // icon scaling controls
    irect                   m_DrawPos;
    irect                   m_RequestedPos;
    irect                   m_StartPos;
    irect                   m_DiffPos;
    irect                   m_TotalMoved;
    f32                     m_scaleX;
    f32                     m_scaleY;
    f32                     m_totalX;
    f32                     m_scaleCount;
    f32                     m_scaleAngle;
    xbool                   m_bScreenIsOn;
    xbool                   m_bScaleDown;
    ui_button*              m_pSelectedIcon;

    // secrets list related
    s32                     m_SelectedIndex;
    //secrets_vault*          m_pSelectedVault;
    //s32                     m_VaultIndex;
    char                    m_FileName[32];
    char                    m_FullDesc[32];
    s32                     m_NumItems;
    s32                     m_CurrItem;
    s32                     m_StillBitmapID;

    // fade controls
    u8                      m_FadeLevel;
    f32                     m_TimeOut;
};

//==============================================================================
#endif // DLG_SECRETS_MENU_HPP
//==============================================================================
