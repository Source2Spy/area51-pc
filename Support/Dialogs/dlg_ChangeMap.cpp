//=========================================================================
//
//  dlg_change_map.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_maplist.hpp"

#include "dlg_PopUp.hpp"
#include "dlg_ChangeMap.hpp"

#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "StateMgr/MapList.hpp"

//=========================================================================
//  Vote Map Dialog
//=========================================================================

enum controls
{
    IDC_CHANGE_MAP_LISTBOX,
};


ui_manager::control_tem ChangeMapControls[] = 
{
    // Frames.
    { IDC_CHANGE_MAP_LISTBOX,   "IDS_CHANGE_MAP",   "maplist",  45,  60, 260, 238, 0, 0, 1, 1, ui_win::WF_VISIBLE },
};

ui_manager::dialog_tem ChangeMapDialog =
{
    "IDS_CHANGE_MAP_MENU",
    1, 9,
    sizeof(ChangeMapControls)/sizeof(ui_manager::control_tem),
    &ChangeMapControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_change_map_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "change map", &ChangeMapDialog, &dlg_change_map_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_change_map_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_change_map* pDialog = new dlg_change_map;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_level_select
//=========================================================================

dlg_change_map::dlg_change_map( void )
{
}

//=========================================================================

dlg_change_map::~dlg_change_map( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_change_map::Create( s32                        UserID,
                             ui_manager*                pManager,
                             ui_manager::dialog_tem*    pDialogTem,
                             const irect&               Position,
                             ui_win*                    pParent,
                             s32                        Flags,
                             void*                      pUserData )
{
    xbool   Success = FALSE;

    (void)pUserData;

    ASSERT( pManager );

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    // find controls
    m_pMapList         = (ui_maplist*)    FindChildByID( IDC_CHANGE_MAP_LISTBOX  );

    // hide them
    m_pMapList        ->SetFlag(ui_win::WF_VISIBLE, FALSE);

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" );
   
    SetNavText( navText );

    // set up map list
    m_pMapList->SetActive( TRUE );
    m_pMapList->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pMapList->DisableFrame();
    m_pMapList->SetExitOnSelect(FALSE);
    m_pMapList->SetExitOnBack(TRUE);
    m_pMapList->EnableHeaderBar();
    m_pMapList->SetHeaderBarColor( xcolor(19,59,14,196) );
    m_pMapList->SetHeaderColor( xcolor(255,252,204,255) );
    m_pMapList->DeleteAllItems();

    for( s32 i=0; i <g_MapList.GetCount(); i++ )
    {
        const map_entry& Entry = *g_MapList.GetByIndex( i );

        // check if loaded
        if( Entry.IsAvailable() )
        {
            switch ( Entry.GetGameType() )
            {
                case GAME_CAMPAIGN:
                    // Don't add campaign maps
                    break;

                default:
                    // build the full map name (includes the game type)
                    xwstring FullMapName;
                    if( DecodeFontText( xfs( "%s %s", Entry.GetShortGameTypeName(), Entry.GetDisplayName() ), FullMapName ) )
                    {
                        m_pMapList->AddItem( FullMapName, (uaddr)&Entry );
                    }
                    break;
            }
        }
    }
    // sort the list
    m_pMapList->AlphaSortList();
    // set initial selection
    m_pMapList->SetSelection( 0 );



    // set initial focus
    m_CurrHL = 0;
    GotoControl( (ui_control*)m_pMapList );
    m_CurrentControl = IDC_CHANGE_MAP_LISTBOX;
    m_PopUp = NULL;

    // initialize screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_change_map::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_change_map::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;
    
    // render background filter
    rb = g_UiMgr->GetUserBounds( m_UserID );
    g_UiMgr->RenderGouraudRect(rb, xcolor(0,0,0,180),
        xcolor(0,0,0,180),
        xcolor(0,0,0,180),
        xcolor(0,0,0,180),FALSE);

    // render transparent screen
    rb.l = m_CurrPos.l + 22;
    rb.t = m_CurrPos.t;
    rb.r = m_CurrPos.r - 23;
    rb.b = m_CurrPos.b;

    g_UiMgr->RenderGouraudRect(rb, xcolor(56,115,58,64),
                                   xcolor(56,115,58,64),
                                   xcolor(56,115,58,64),
                                   xcolor(56,115,58,64),FALSE);


    // render the screen bars
    s32 y = rb.t + offset;    

    while (y < rb.b)
    {
        irect bar;

        if ((y+width) > rb.b)
        {
            bar.Set(rb.l, y, rb.r, rb.b);
        }
        else
        {
            bar.Set(rb.l, y, rb.r, y+width);
        }

        // draw the bar
        g_UiMgr->RenderGouraudRect(bar, xcolor(56,115,58,30),
                                        xcolor(56,115,58,30),
                                        xcolor(56,115,58,30),
                                        xcolor(56,115,58,30),FALSE);

        y+=gap;
    }

    // render the normal dialog stuff
    ui_dialog::Render( ox, oy );

    // render the glow bar
    g_UiMgr->RenderGlowBar();
}

//=========================================================================

void dlg_change_map::OnAccept( ui_win* pWin )
{

    (void)pWin;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        // display confirmation popup
        irect r = g_UiMgr->GetUserBounds( g_UiUserID );
        m_PopUp = (dlg_popup*)g_UiMgr->OpenDialog(  m_UserID, "popup", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_BORDER|ui_win::WF_DLG_CENTER|WF_INPUTMODAL );

        // set nav text
        xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_YES" ));
        navText += g_StringTableMgr( "ui", "IDS_NAV_NO" );
        SetNavTextVisible( FALSE );

        m_PopUp->Configure( g_StringTableMgr( "ui", "IDS_CHANGE_MAP" ), 
            TRUE, 
            TRUE, 
            FALSE, 
            g_StringTableMgr( "ui", "IDS_CHANGE_MAP_MSG" ),
            navText,
            &m_PopUpResult );

        m_State = DIALOG_STATE_POPUP;
    }
}

//=========================================================================

void dlg_change_map::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play("Backup");
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_change_map::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // turn on the controls
            m_pMapList  ->SetFlag(ui_win::WF_VISIBLE, TRUE);
            GotoControl( (ui_control*)m_pMapList );
            g_UiMgr->SetScreenHighlight( m_pMapList->GetPosition() );
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    if ( m_PopUp )
    {
        if ( m_PopUpResult != DLG_POPUP_IDLE )
        {
            if ( m_PopUpResult == DLG_POPUP_YES )
            {
                // TODO: Change to the selected map here!
                // get the map table entry from the list
                map_entry*  pTableEntry = (map_entry*)m_pMapList->GetSelectedItemData();
                ASSERT( pTableEntry );
                m_State = DIALOG_STATE_SELECT;

                if( g_PendingConfig.GetGameTypeID() != pTableEntry->GetGameType() )
                    g_PendingConfig.SetScoreLimit( -1 );

                g_PendingConfig.SetLevelID( pTableEntry->GetMapID() );
                g_PendingConfig.SetGameTypeID( pTableEntry->GetGameType() );

                g_ActiveConfig.SetExitReason( GAME_EXIT_RELOAD_LEVEL );
            }
            else
            {
                // re-enable the dialog
                m_State = DIALOG_STATE_ACTIVE;
            }

            // clear popup 
            m_PopUp = NULL;

            // turn on nav text
            SetNavTextVisible( TRUE );
        }
    }
}

//=========================================================================
