//==============================================================================
//
//  dlg_OnlineHost.hpp
//
//==============================================================================

#ifndef DLG_ONLINE_HOST_HPP
#define DLG_ONLINE_HOST_HPP

#include "UI/ui_dialog.hpp"
#include "dlg_OnlineHostOptions.hpp"

class ui_button;
class ui_edit;
class ui_listbox;
class dlg_popup;

void dlg_online_host_register( ui_manager* pManager );
ui_win* dlg_online_host_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================

class dlg_online_host : public ui_dialog
{
public:
    virtual            ~dlg_online_host         ( void );

    xbool               Create                  ( s32                       UserID,
                                                  ui_manager*               pManager,
                                                  ui_manager::dialog_tem*   pDialogTem,
                                                  const irect&              Position,
                                                  ui_win*                   pParent,
                                                  s32                       Flags,
                                                  void*                     pUserData );
    virtual void        Destroy                 ( void );
    virtual void        Render                  ( s32 ox=0, s32 oy=0 );
    virtual void        OnNotify                ( ui_notification const& Event );
    virtual void        OnNavigate              ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnAccept                ( ui_win* pWin );
    virtual void        OnCancel                ( ui_win* pWin );
    virtual void        OnUpdate                ( ui_win* pWin, f32 DeltaTime );

private:
    void                ShowControls            ( xbool Visible );
    xbool               FocusMapControl         ( s32 Start, s32 Direction );
    void                FillMaps                ( xbool Reset );
    void                FillRotation            ( s32 Selection );
    void                AddMap                  ( void );
    void                RemoveMap               ( void );
    void                MoveMap                 ( s32 Direction );
    void                LaunchServer            ( void );
    xbool               ApplySettings           ( void );
    void                ShowMessage             ( const char* pMessage );
    void                SavePreferences         ( void );

    host_options        m_Options;

    dlg_online_host_options* m_pOptionsDialog;

    ui_edit*            m_pName;
    ui_listbox*         m_pMaps;
    ui_listbox*         m_pRotation;
    dlg_popup*          m_pPopup;
    s32                 m_PopupResult;
    s32                 m_MapGameType;
    s32                 m_MapPlayerLimit;
    xbool               m_Created = FALSE;
    xbool               m_ReconfigurePrompt;
    xbool               m_Rebuilding;
};

//==============================================================================
#endif // DLG_ONLINE_HOST_HPP
