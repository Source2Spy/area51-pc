//==============================================================================
//
//  dlg_OnlineHostOptions.hpp
//
//==============================================================================

#ifndef DLG_ONLINE_HOST_OPTIONS_HPP
#define DLG_ONLINE_HOST_OPTIONS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "StateMgr/GlobalSettings.hpp"

class ui_combo;
class ui_edit;
class ui_check;
class ui_text;

struct host_options
{
    host_settings       Settings;
    s32                 Port;
    xstring             Directory;
    xbool               Reconfigure;
};

void dlg_online_host_options_register( ui_manager* pManager );

//==============================================================================

class dlg_online_host_options : public ui_dialog
{
public:
    xbool               Create                  ( s32                       UserID,
                                                  ui_manager*               pManager,
                                                  ui_manager::dialog_tem*   pDialogTem,
                                                  const irect&              Position,
                                                  ui_win*                   pParent,
                                                  s32                       Flags,
                                                  void*                     pUserData );
    virtual void        Render                  ( s32 ox=0, s32 oy=0 );
    virtual void        OnNotify                ( ui_notification const& Event );
    virtual void        OnAccept                ( ui_win* pWin );
    virtual void        OnCancel                ( ui_win* pWin );

private:
    void                UpdateDirectoryControl  ( void );
    void                ConfigureGameType       ( s32 Type );
    void                ReadSettings            ( const host_settings& Settings );

    host_options*       m_pOptions;
    xbool               m_Advanced;
    xbool               m_Rebuilding;
    ui_combo*           m_pMode;
    ui_combo*           m_pPlayers;
    ui_combo*           m_pScore;
    ui_combo*           m_pTime;
    ui_combo*           m_pMutation;
    ui_combo*           m_pVote;
    ui_combo*           m_pFire;
    ui_check*           m_pScaling;
    ui_edit*            m_pAddress;
    ui_edit*            m_pPassword;
    ui_edit*            m_pPort;
    ui_edit*            m_pDirectory;
    ui_check*           m_pVoice;
    ui_check*           m_pPrivate;
    ui_check*           m_pLanOnly;
    ui_check*           m_pCheats;
    ui_text*            m_pStatus;
};

//==============================================================================
#endif // DLG_ONLINE_HOST_OPTIONS_HPP
//==============================================================================
