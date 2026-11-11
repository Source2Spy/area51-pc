//==============================================================================
//  
//  dlg_BigLeaderboard.hpp
//  
//==============================================================================

#ifndef DLG_BIG_LEADERBOARD_HPP
#define DLG_BIG_LEADERBOARD_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"
#include "UI/ui_blankbox.hpp"
#include "dlg_PopUp.hpp"
#include "dlg_Leaderboard.hpp"

#include "StateMgr/StateMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

enum big_leaderboard_controls
{
    IDC_BIG_LEADERBOARD_ONE_DETAILS,
    IDC_BIG_LEADERBOARD_TWO_DETAILS,
    IDC_BIG_LEADERBOARD_FRAME_ONE,
    IDC_BIG_LEADERBOARD_FRAME_TWO,
    IDC_BIG_LEADERBOARD_FRAME_MAIN,
    IDC_BIG_LEADERBOARD_LOADING_TEXT,
    IDC_BIG_LEADERBOARD_LOADING_PIPS,
};

enum player_data_types
{
    TYPE_KILLS = 0,
    TYPE_DEATHS,
    TYPE_TKS,
    TYPE_FLAGS,
    TYPE_VOTES,

    NUM_DATA_TYPES,

    TYPE_NONE
};

//==============================================================================
//  dlg_big_leaderboard
//==============================================================================

extern void     dlg_big_leaderboard_register  ( ui_manager* pManager );
extern ui_win*  dlg_big_leaderboard_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;

class dlg_big_leaderboard : public dlg_mp_score
{
public:
                        dlg_big_leaderboard       ( void );
    virtual            ~dlg_big_leaderboard       ( void );

    virtual xbool       Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnNavigate       ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnDelete         ( ui_win* pWin );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

    void                FillScoreList       ( void );
    void                RenderScoreList     ( s32 ox, s32 oy );
    void                CycleStatDisplay    ( xbool bForward );

protected:
    ui_frame*           m_pFrame1;

    ui_blankbox*        m_pPlayerOneBox;
    ui_blankbox*        m_pPlayerTwoBox;

    ui_frame*           m_pFrameOne;
    ui_frame*           m_pFrameTwo;
    ui_frame*           m_pFrameMain;

    ui_text*            m_pLoadingText;
    ui_text*            m_pLoadingPips;

    player_score        m_PlayerData[NET_MAX_PLAYERS];
    player_score        m_PlayerTotals[2];

    s8                  m_NumTeams;
    s8                  m_SelectedType;
    s32                 m_CurrHL;
    s32                 m_Font;

    dlg_popup*          m_pPopUp;
    s32                 m_PopUpResult;

    xwstring            m_StrLevelDesc;
    xwstring            m_StrPlayer;
    xwstring            m_StrScore;
    xwstring            m_StrIconKills;
    xwstring            m_StrIconDeaths;
    xwstring            m_StrIconTKs;
    xwstring            m_StrIconFlags;
    xwstring            m_StrIconVotes;
};

//==============================================================================
#endif // DLG_BIG_LEADERBOARD_HPP
//==============================================================================
