//==============================================================================
//  
//  dlg_PopUp.hpp
//  
//==============================================================================

#ifndef DLG_POPUP_HPP
#define DLG_POPUP_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"
#include "UI/ui_frame.hpp"
#include "UI/ui_text.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

enum
{
    DLG_POPUP_IDLE,
    DLG_POPUP_YES,
    DLG_POPUP_NO,
    DLG_POPUP_MAYBE,
    DLG_POPUP_HELP,
    DLG_POPUP_OTHER,
};

//==============================================================================
//  dlg_popup
//==============================================================================

extern void     dlg_popup_register  ( ui_manager* pManager );
extern ui_win*  dlg_popup_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class ui_button;

class dlg_popup : public ui_dialog
{
public:
                        dlg_popup  ( void );
    virtual            ~dlg_popup  ( void );

    void                Configure           ( const f32                 Timeout,
                                              const xwchar*             Title,
                                              const xwchar*             Message,
                                              const xwchar*             Message2 = NULL,
                                              const xwchar*             Message3 = NULL );

    void                Configure           (       irect&              Position,
                                              const f32                 Timeout,
                                              const xwchar*             Title,
                                              const xwchar*             Message,
                                              const xwchar*             Message2 = NULL,
                                              const xwchar*             Message3 = NULL );

    void                Configure           ( const xwchar*             Title,
                                              const xbool               Yes,
                                              const xbool               No,
                                              const xbool               Maybe,
                                              const xwchar*             Message,
                                              const xwchar*             pNavText,
                                                    s32*                pResult = NULL );

    void                Configure           (       irect&              Position,
                                              const xwchar*             Title,
                                              const xbool               Yes,
                                              const xbool               No,
                                              const xbool               Maybe,
                                              const xwchar*             Message,
                                              const xwchar*             pNavText,
                                                    s32*                pResult = NULL );

    void                Configure           ( const xwchar*             Title,
                                              const xbool               Yes,
                                              const xbool               No,
                                              const xbool               Maybe,
                                              const xbool               Help,
                                              const xwchar*             Message,
                                              const xwchar*             pNavText,
                                                    s32*                pResult = NULL );

    void                Configure           (       irect&              Position,
                                              const xwchar*             Title,
                                              const xbool               Yes,
                                              const xbool               No,
                                              const xbool               Maybe,
                                              const xbool               Help,
                                              const xwchar*             Message,
                                              const xwchar*             pNavText,
                                                    s32*                pResult = NULL );

    void                Configure           ( const xwchar*             Title,
                                              const xbool               Yes,
                                              const xbool               No,
                                              const xbool               Maybe,
                                              const xbool               Help,
                                              const xbool               Other,
                                              const xwchar*             Message,
                                              const xwchar*             pNavText,
                                                    s32*                pResult = NULL );

    void                Configure           (       irect&              Position,
                                              const xwchar*             Title,
                                              const xbool               Yes,
                                              const xbool               No,
                                              const xbool               Maybe,
                                              const xbool               Help,
                                              const xbool               Other,
                                              const xwchar*             Message,
                                              const xwchar*             pNavText,
                                                    s32*                pResult = NULL );


    void                ConfigureAutosaveDialog (   irect&              Position,
                                              const xwchar*             Title,
                                              const xwchar*             Message,
                                              const xwchar*             pNavText,
                                                    s32*                pResult = NULL );


    void                ConfigureMessage    ( const xwchar*             Title,
                                              const xwchar*             Message,
                                                    s32*                pResult = NULL );
    void                ConfigureMessage    (       irect&              Position,
                                              const xwchar*             Title,
                                              const xwchar*             Message,
                                                    s32*                pResult = NULL );

    void                Close               ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData);
    virtual void        Destroy             ( void );

    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );
    virtual void        OnAccept            ( ui_win* pWin );
    virtual void        OnCancel           ( ui_win* pWin );
    virtual void        OnDelete            ( ui_win* pWin );
    virtual void        OnAlternate         ( ui_win* pWin );
    virtual void        OnHelp              ( ui_win* pWin );
    virtual void        OnNavigate          ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void        OnPage              ( ui_win* pWin, s32 Direction );


    void                EnableBlackout      ( xbool bDoBlackout )       { m_bDoBlackout = bDoBlackout; }


    
protected:
    enum popup_action_type
    {
        POPUP_ACTION_NONE = 0,
        POPUP_ACTION_ACCEPT,
        POPUP_ACTION_CANCEL,
        POPUP_ACTION_DELETE,
        POPUP_ACTION_HELP,
        POPUP_ACTION_ALTERNATE,
    };

    enum
    {
        MAX_POPUP_ACTIONS = 5,
    };

    struct popup_action
    {
        popup_action_type   Type;
        xwstring            Prompt;
        xwstring            Label;
        xbool               Enabled;

        popup_action( void ) :
            Type   ( POPUP_ACTION_NONE ),
            Enabled( FALSE             )
        {
        }
    };

    void                ClearActions            ( void );
    void                ConfigureStandardActions( xbool Yes,
                                                  xbool No,
                                                  xbool Maybe,
                                                  xbool Help,
                                                  xbool Other,
                                                  const xwchar* pNavText );
    void                AddAction               ( popup_action_type Type,
                                                  const xwchar*      Prompt,
                                                  const char*        pFallbackString,
                                                  xbool              Enabled = TRUE );
    void                SetAction               ( popup_action_type Type,
                                                  const xwchar*      Prompt,
                                                  xbool              Enabled );
    s32                 FindAction              ( popup_action_type Type ) const;
    s32                 FindAction              ( const ui_win* pWin ) const;
    xbool               IsActionEnabled         ( popup_action_type Type ) const;
    void                ApplyInputLayout        ( ui_input_device Device );
    void                LayoutActionButtons     ( void );
    void                CompleteAction          ( s32 Result, xbool TrackController = FALSE );

    ui_frame*           m_pFrame1;
    ui_text*            m_pNavText;
    ui_button*          m_pActionButtons[MAX_POPUP_ACTIONS];
    popup_action        m_Actions[MAX_POPUP_ACTIONS];
    s32                 m_ActionCount;
    s32                 m_ActionRowCount;
    ui_input_device     m_LastInputDevice;

    s32                 m_iElement;
    xwstring            m_Message;
    xwstring            m_Message2;
    xwstring            m_Message3;
    f32                 m_Timeout;
    xbool               m_bDoTimeout;
    xbool               m_DismissOnCancel;
    s32*                m_pResult;
    s32                 m_CurrHL;
    xbool               m_bDoBlackout;
    xbool               m_HasNavText;
    xbool               m_NavTextIsActionPrompt;


};

//==============================================================================
#endif // DLG_POPUP_HPP
//==============================================================================
