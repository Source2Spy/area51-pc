//==============================================================================
//
//  dlg_BrowserOptions.hpp
//
//==============================================================================

#ifndef DLG_BROWSER_OPTIONS_HPP
#define DLG_BROWSER_OPTIONS_HPP

#include "UI/ui_dialog.hpp"
#include "NetworkMgr/Match/MatchBrowser.hpp"
#include "NetworkMgr/Match/MatchProvider.hpp"

class ui_check;
class ui_combo;
class ui_edit;
class ui_text;

void dlg_browser_options_register( ui_manager* pManager );

//==============================================================================

class dlg_browser_options : public ui_dialog
{
public:
    xbool               Create                ( s32                       UserID,
                                                ui_manager*               pManager,
                                                ui_manager::dialog_tem*   pDialogTem,
                                                const irect&              Position,
                                                ui_win*                   pParent,
                                                s32                       Flags,
                                                void*                     pUserData );
    virtual void        Render                ( s32 ox=0, s32 oy=0 );
    virtual void        OnCancel              ( ui_win* pWin );
};

//==============================================================================

class dlg_browser_filters : public dlg_browser_options
{
public:
    xbool               Create                ( s32                       UserID,
                                                ui_manager*               pManager,
                                                ui_manager::dialog_tem*   pDialogTem,
                                                const irect&              Position,
                                                ui_win*                   pParent,
                                                s32                       Flags,
                                                void*                     pUserData );
    void                Configure             ( const match_browser_filter& Filter );
    const match_browser_filter& GetFilter      ( void ) const { return m_Filter; }
    virtual void        OnAccept              ( ui_win* pWin );

private:
    match_browser_filter    m_Filter;
    ui_combo*               m_pSource;
    ui_combo*               m_pMode;
    ui_combo*               m_pPing;
    ui_edit*                m_pName;
    ui_edit*                m_pMap;
    ui_check*               m_pNotFull;
    ui_check*               m_pNotEmpty;
    ui_check*               m_pNoPassword;
};

//==============================================================================

class dlg_browser_advanced : public dlg_browser_options
{
public:
    virtual            ~dlg_browser_advanced  ( void );
    xbool               Create                ( s32                       UserID,
                                                ui_manager*               pManager,
                                                ui_manager::dialog_tem*   pDialogTem,
                                                const irect&              Position,
                                                ui_win*                   pParent,
                                                s32                       Flags,
                                                void*                     pUserData );
    xbool               HasConnection         ( void ) const { return m_Connected; }
    virtual void        OnAccept              ( ui_win* pWin );
    virtual void        OnCancel              ( ui_win* pWin );
    virtual void        OnUpdate              ( ui_win* pWin, f32 DeltaTime );

private:
    void                HandleConnectionResult( match_result Result );
    void                SavePreferences       ( void );

    ui_edit*            m_pDirectory;
    ui_edit*            m_pAddress;
    ui_text*            m_pStatus;
    xbool               m_Connected;
    xbool               m_Resolving = FALSE;
};

#endif // DLG_BROWSER_OPTIONS_HPP
