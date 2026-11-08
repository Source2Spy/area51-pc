//==============================================================================
//
//  dlg_OnlineJoin.hpp
//
//==============================================================================

#ifndef DLG_ONLINE_JOIN_HPP
#define DLG_ONLINE_JOIN_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "NetworkMgr/Match/MatchBrowser.hpp"

class ui_button;
class ui_text;
class ui_joinlist;
class dlg_browser_filters;
class dlg_browser_advanced;

extern void dlg_online_join_register( ui_manager* pManager );
extern ui_win* dlg_online_join_factory( s32 UserID, ui_manager* pManager,
                                        ui_manager::dialog_tem* pDialogTem, const irect& Position,
                                        ui_win* pParent, s32 Flags, void* pUserData );

//==============================================================================

class dlg_online_join : public ui_dialog
{
public:
                        dlg_online_join ( void );
    virtual            ~dlg_online_join ( void );

    xbool               Create                ( s32                       UserID,
                                                ui_manager*               pManager,
                                                ui_manager::dialog_tem*   pDialogTem,
                                                const irect&              Position,
                                                ui_win*                   pParent,
                                                s32                       Flags,
                                                void*                     pUserData );
    virtual void        Destroy               ( void );
    virtual void        Render                ( s32 ox=0, s32 oy=0 );
    virtual void        OnNavigate            ( ui_win* pWin, ui_navigation Code, s32 Presses,
                                                s32 Repeats, xbool WrapX=FALSE, xbool WrapY=FALSE );
    virtual void        OnAccept              ( ui_win* pWin );
    virtual void        OnCancel              ( ui_win* pWin );
    virtual void        OnAlternate           ( ui_win* pWin );
    virtual void        OnDelete              ( ui_win* pWin );
    virtual void        OnNotify              ( ui_notification const& Event );
    virtual void        OnUpdate              ( ui_win* pWin, f32 DeltaTime );
    void                FillMatchList         ( void );

protected:
    const server_info*   GetSelectedServer     ( void ) const;
    void                JoinSelectedServer    ( void );
    void                Refresh               ( void );
    void                UpdateDetails         ( void );
    void                ShowControls          ( xbool Visible );
    void                SavePreferences       ( void );

    ui_joinlist*            m_pMatchList;
    ui_button*              m_pFilters;
    ui_button*              m_pJoin;
    ui_text*                m_pDetails;
    ui_text*                m_pEndpoint;
    ui_text*                m_pMap;
    ui_text*                m_pMode;
    ui_text*                m_pPlayers;
    ui_text*                m_pPing;
    ui_text*                m_pPassword;
    ui_text*                m_pComplete;
    dlg_browser_filters*    m_pFiltersDialog;
    dlg_browser_advanced*   m_pAdvancedDialog;
    match_browser_filter    m_Filter;
    match_browser_filter    m_LastFilter;
    match_sort              m_LastSort;
    xbool                   m_LastDescending;
    u32                     m_LastRevision;
    f32                     m_RefreshTimeout;
    xbool                   m_Rebuilding;
    xbool                   m_Created;
};
//==============================================================================
#endif // DLG_ONLINE_JOIN_HPP
//==============================================================================
