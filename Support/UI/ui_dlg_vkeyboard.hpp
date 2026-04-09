//==============================================================================
//  
//  ui_dlg_vkeyboard.hpp
//  
//==============================================================================

#ifndef UI_DLG_VKEYBOARD_HPP
#define UI_DLG_VKEYBOARD_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ui_dialog.hpp"
#include "dialogs\dlg_popup.hpp"

//==============================================================================
//  ui_dlg_vkeyboard
//==============================================================================

extern ui_win* ui_dlg_vkeyboard_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_vkString;

class ui_dlg_vkeyboard : public ui_dialog
{
public:
                    ui_dlg_vkeyboard    ( void );
    virtual        ~ui_dlg_vkeyboard    ( void );

    xbool           Create              ( s32                       UserID,
                                          ui_manager*               pManager,
                                          ui_manager::dialog_tem*   pDialogTem,
                                          const irect&              Position,
                                          ui_win*                   pParent,
                                          s32                       Flags,
                                          void*                     pUserData );

    void            Configure           ( xbool bName ) { m_bName = bName; }

#ifdef TARGET_PC
    void            SetGamepadMode      ( xbool bGamepad );
    xbool           IsGamepadMode       ( void ) const { return m_bGamepadMode; }
#endif

    virtual void    Render              ( s32 ox=0, s32 oy=0 );

    virtual void    OnPadNavigate       ( ui_win* pWin, s32 Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void    OnPadShoulder       ( ui_win* pWin, s32 Direction );

    virtual void    OnPadSelect         ( ui_win* pWin );
    virtual void    OnPadBack           ( ui_win* pWin );
    virtual void    OnPadDelete         ( ui_win* pWin );
    virtual void    OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    s32             IsValid             ( const xwstring* pString, xbool bIsName );

    virtual void    OnNotify            ( ui_win* pWin, ui_win* pSender, s32 Command, void* pData );

    void            ConnectString       ( xwstring* pString, s32 BufferSize );
    void            SetReturn           ( xbool* pDone, xbool* pOk );

protected:
    s32             m_iElement;
    s32             m_MaxCharacters;
    ui_vkString*    m_pStringCtrl;
    xwstring*       m_pString;
    xwstring        m_BackupString;
    xbool*          m_pResultDone;
    xbool*          m_pResultOk;
    dlg_popup*      m_pPopUp;
    s32             m_PopUpResult;
    xbool           m_bName;            // Whether this dialogue exists to enter in a name (as opposed to a password).
#ifdef TARGET_PC
    xbool           m_bGamepadMode;     // If TURE on screen keyboard will be shown.
    s32             m_FullPositionB;
    s32             m_RepeatKeyIdx;     // -1=none, >=0=index into s_PCKeyMap
    f32             m_KeyRepeatTimer;
#endif
};

//==============================================================================
#endif // UI_DLG_VKEYBOARD_HPP
//==============================================================================