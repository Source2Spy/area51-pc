//=========================================================================
//
//  dlg_OnlineConnect.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_text.hpp"
#include "dlg_OnlineConnect.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Match/MatchMgr.hpp"
#include "StringMgr/StringMgr.hpp"

//=========================================================================

static ui_manager::control_tem OnlineConnectControls[] =
{
    { 0, "IDS_ONLINE_PLEASE_WAIT", "text", 30, 100, 300, 80, 0, 0, 1, 1, ui_win::WF_VISIBLE | ui_win::WF_STATIC },
};

static ui_manager::dialog_tem OnlineConnectDialog =
{
    "IDS_ONLINE_CONNECT", 1, 1,
    sizeof( OnlineConnectControls )/sizeof( ui_manager::control_tem ),
    OnlineConnectControls, 0
};

//=========================================================================

void dlg_online_connect_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "connect info", &OnlineConnectDialog, &dlg_online_connect_factory );
}

//=========================================================================

ui_win* dlg_online_connect_factory( s32                       UserID,
                                    ui_manager*               pManager,
                                    ui_manager::dialog_tem*   pDialogTem,
                                    const irect&              Position,
                                    ui_win*                   pParent,
                                    s32                       Flags,
                                    void*                     pUserData )
{
    dlg_online_connect* pDialog = new dlg_online_connect;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return pDialog;
}

//=========================================================================

xbool dlg_online_connect::Create( s32                       UserID,
                                  ui_manager*               pManager,
                                  ui_manager::dialog_tem*   pDialogTem,
                                  const irect&              Position,
                                  ui_win*                   pParent,
                                  s32                       Flags,
                                  void*                     pUserData )
{
    (void)pUserData;

    if( !ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags ) )
        return FALSE;

    m_Cancelled = FALSE;
    m_Failed    = FALSE;
    m_Timeout   = 0.0f;
    m_State     = DIALOG_STATE_ACTIVE;

    SetNavText( g_StringTableMgr( "ui", "IDS_NAV_CANCEL" ) );
    InitScreenScaling( Position );
    g_UiMgr->DisableScreenHighlight();

    if( !g_NetworkMgr.BeginOnlineStateChange( TRUE ) )
        Failed();

    return TRUE;
}

//=========================================================================

void dlg_online_connect::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb = g_UiMgr->GetUserBounds( m_UserID );

    g_UiMgr->RenderGouraudRect( rb, xcolor( 0, 0, 0, 180 ),
        xcolor( 0, 0, 0, 180 ),
        xcolor( 0, 0, 0, 180 ),
        xcolor( 0, 0, 0, 180 ), FALSE );

    // render transparent screen
    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;

    g_UiMgr->RenderGouraudRect( rb, xcolor( 56, 115, 58, 64 ),
        xcolor( 56, 115, 58, 64 ),
        xcolor( 56, 115, 58, 64 ),
        xcolor( 56, 115, 58, 64 ), FALSE );

    // render the screen bars
    s32 y = rb.t + offset;

    while ( y < rb.b )
    {
        irect bar;

        if ( ( y + width ) > rb.b )
        {
            bar.Set( rb.l, y, rb.r, rb.b );
        }
        else
        {
            bar.Set( rb.l, y, rb.r, ( y + width ) );
        }

        // draw the bar
        g_UiMgr->RenderGouraudRect( bar, xcolor( 56, 115, 58, 30 ),
            xcolor( 56, 115, 58, 30 ),
            xcolor( 56, 115, 58, 30 ),
            xcolor( 56, 115, 58, 30 ), FALSE );

        y += gap;
    }

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_online_connect::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( m_State != DIALOG_STATE_ACTIVE )
        return;

    m_Cancelled = TRUE;
    if( !g_NetworkMgr.IsOnlineStateChanging() )
        m_State = DIALOG_STATE_BACK;
}

//=========================================================================

void dlg_online_connect::Failed( void )
{
    m_Failed = TRUE;
    FindChildByID( 0 )->SetLabel( g_StringTableMgr( "ui", "IDS_ONLINE_NETWORK_DOWN" ) );
}

//=========================================================================

void dlg_online_connect::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    if( m_State != DIALOG_STATE_ACTIVE )
        return;

    if( g_UiMgr->IsScreenScaling() )
        UpdateScreenScaling( DeltaTime );

    g_UiMgr->UpdateGlowBar( DeltaTime );

    if( g_NetworkMgr.IsOnlineStateChanging() )
    {
        if( !g_NetworkMgr.IsOnlineStateChangeDone() )
            return;

        g_NetworkMgr.FinishOnlineStateChange();
    }

    if( m_Cancelled )
    {
        m_State = DIALOG_STATE_BACK;
        return;
    }

    if( m_Failed )
        return;

    interface_info Info;
    net_GetInterfaceInfo( -1, Info );
    if( !Info.Address )
    {
        m_Timeout += DeltaTime;
        if( m_Timeout >= 10.0f )
            Failed();
        return;
    }

    net_socket& Socket = g_NetworkMgr.GetSocket();
    if( Socket.IsEmpty() && !Socket.Bind( 0, NET_FLAGS_BROADCAST ) )
    {
        Failed();
        return;
    }

    if( !g_MatchMgr.IsReady() && !g_MatchMgr.Init( Socket, net_address( Info.Broadcast, Socket.GetPort() ) ) )
    {
        Socket.Close();
        Failed();
        return;
    }

    m_State = DIALOG_STATE_EXIT;
}
