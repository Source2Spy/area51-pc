//=========================================================================
//
//  dlg_resume_game.cpp
//
//=========================================================================

#include "Entropy.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_control.hpp"
#include "UI/ui_combo.hpp"
#include "UI/ui_button.hpp"
#include "UI/ui_listbox.hpp"

#include "dlg_ResumeGame.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "Parsing/TextIn.hpp"
#include "StateMgr/MapList.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "../../Apps/GameApp/Config.hpp"    
#include "Configuration/GameConfig.hpp"


//=========================================================================
//  Main Menu Dialog
//=========================================================================

enum controls
{
    IDC_CHECKPOINT_LISTBOX,
};


ui_manager::control_tem ResumeGameControls[] = 
{
    // Frames.
    { IDC_CHECKPOINT_LISTBOX,    "IDS_NULL",           "listbox",  50, 60, 220, 232, 0, 0, 1, 1, ui_win::WF_VISIBLE },
};


ui_manager::dialog_tem ResumeGameDialog =
{
    "IDS_RESUME_CAMPAIGN_MENU",
    1, 9,
    sizeof(ResumeGameControls)/sizeof(ui_manager::control_tem),
    &ResumeGameControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_resume_game_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "resume game", &ResumeGameDialog, &dlg_resume_game_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_resume_game_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_resume_game* pDialog = new dlg_resume_game;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_resume_game
//=========================================================================

dlg_resume_game::dlg_resume_game( void )
{
}

//=========================================================================

dlg_resume_game::~dlg_resume_game( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_resume_game::Create( s32                        UserID,
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

    m_pCheckpointList    = (ui_listbox*) FindChildByID( IDC_CHECKPOINT_LISTBOX  );
    
    GotoControl( (ui_control*)m_pCheckpointList );
    m_CurrentControl = IDC_CHECKPOINT_LISTBOX;

    FillCheckpointList();
    m_pCheckpointList->SetActive( TRUE );
    m_pCheckpointList->SetFlag(ui_win::WF_VISIBLE, FALSE);
    m_pCheckpointList->SetLabelFlags( ui_font::h_left|ui_font::v_center|ui_font::clip_ellipsis|ui_font::clip_l_justify );
    m_pCheckpointList->SetBackgroundColor( xcolor (39,117,28,128) );
    m_pCheckpointList->DisableFrame();
    m_pCheckpointList->SetExitOnSelect(FALSE);
    m_pCheckpointList->SetExitOnBack(TRUE);
    m_CurrHL = 0;

    // set up nav text
    xwstring navText(g_StringTableMgr( "ui", "IDS_NAV_SELECT" ));
    navText += g_StringTableMgr( "ui", "IDS_NAV_BACK" ); 
    SetNavText( navText );

    if( !(CONFIG_IS_AUTOSERVER || CONFIG_IS_AUTOCLIENT) )
    {
        // clear in-use controller flags
        for( s32 i=0; i<MAX_LOCAL_PLAYERS; i++)
        {
            g_StateMgr.SetControllerRequested(i, FALSE);
        }
    }

    // initialize screen scaling
    InitScreenScaling( Position );

    // make the dialog active
    m_State = DIALOG_STATE_ACTIVE;

    // Return success code
    return Success;
}

//=========================================================================

void dlg_resume_game::Destroy( void )
{
    ui_dialog::Destroy();

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_resume_game::Render( s32 ox, s32 oy )
{
    const s32 offset = (s32)(g_UiMgr->GetAlphaTime() * 60.0f) % 10;
    static s32 gap      =  9;
    static s32 width    =  4;

    irect rb;
        
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

void dlg_resume_game::OnNotify( ui_notification const& Event )
{
    (void)Event.m_pSender;
    (void)Event.m_Type;
    (void)Event.m_pText;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        if (Event.m_Type == ui_notification_type::ListAccepted)
        {
            s32 Selection = m_pCheckpointList->GetSelection();
            if( Selection < 0 || Selection >= m_pCheckpointList->GetItemCount() )
            {
                x_DebugMsg( "Resume campaign: selected checkpoint is out of range.\n" );
                return;
            }

            // Get the checkpoint data
            const level_check_points* pCheckpoint = (level_check_points*)m_pCheckpointList->GetSelectedItemData(0);
            s32 CheckpointIndex = m_pCheckpointList->GetSelectedItemData(1);

            if( !pCheckpoint )
            {
                x_DebugMsg( "Resume campaign: selected checkpoint has no data.\n" );
                return;
            }

            // Set the level name
            const map_entry* pMapEntry = g_MapList.Find( pCheckpoint->MapID, GAME_CAMPAIGN );
            if( !pMapEntry )
            {
                x_DebugMsg( "Resume campaign: checkpoint references missing map ID %d.\n", pCheckpoint->MapID );
                return;
            }

            g_PendingConfig.SetLevelID( pCheckpoint->MapID );
            s32 Index = 0;
            // We need to translate from our index to the index within the maplist.
            while( TRUE )
            {
                if( g_MapList.GetByIndex( Index ) == pMapEntry )
                {
                    break;
                }
                Index++;
                // We should NOT reach the end of the list here.
                ASSERT( g_MapList.GetByIndex( Index )->GetMapID() != -1 );
            }
            g_StateMgr.SetLevelIndex( Index );
            // Now restore the checkpoint from the player profile in to the active checkpoint
            g_CheckPointMgr.m_Level = *pCheckpoint;
            g_CheckPointMgr.SetCheckPointIndex( CheckpointIndex );
            m_State = DIALOG_STATE_SELECT;
        }
    }
}

//=========================================================================

void dlg_resume_game::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    ui_dialog::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

//=========================================================================

void dlg_resume_game::OnAccept( ui_win* pWin )
{
    // store the active controller
    g_StateMgr.SetActiveControllerID( g_UiMgr->GetActiveController() );
    g_StateMgr.SetControllerRequested( g_UiMgr->GetActiveController(), TRUE );

    // load the selected checkpoint
    g_AudioMgr.Play( "Select_Norm" );
    m_pCheckpointList->OnAlternate( pWin );
}

//=========================================================================

void dlg_resume_game::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if ( m_State == DIALOG_STATE_ACTIVE )
    {
        g_AudioMgr.Play("Backup");
        m_State = DIALOG_STATE_BACK;
    }
}

//=========================================================================

void dlg_resume_game::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    // scale window if necessary
    if( g_UiMgr->IsScreenScaling() )
    {
        if( UpdateScreenScaling( DeltaTime ) == FALSE )
        {
            // complete!  turn on the elements
            m_pCheckpointList ->SetFlag(ui_win::WF_VISIBLE, TRUE);

            GotoControl( (ui_control*)m_pCheckpointList );
            g_UiMgr->SetScreenHighlight( m_pCheckpointList->GetPosition() );
        }
    }

    // update the glow bar
    g_UiMgr->UpdateGlowBar(DeltaTime);

    // update list box
    // Level selection within range?
    if( (m_pCheckpointList->GetSelection() >= 0) && (m_pCheckpointList->GetSelection() < m_pCheckpointList->GetItemCount()) )
    {
        // Get Pointer to mission definition
        //fegl.pMissionDef = (mission_def*)m_pMissions->GetSelectedItemData();
        //ASSERT( fegl.pMissionDef );
        //fegl.pNextMissionDef = NULL;

        // Set Name and Game Type
        //x_strcpy( fegl.MissionName, fegl.pMissionDef->Folder );
        //fegl.GameType = fegl.pMissionDef->GameType;
    }

}

//=========================================================================

class checkpoint_sort_compare : public x_compare_functor<const ui_listbox::item&>
{
public:
    s32 operator()( const ui_listbox::item& A, const ui_listbox::item& B )
    {
        const level_check_points& Left = (const level_check_points&)A.Data[0];
        const level_check_points& Right = (const level_check_points&)B.Data[0];
        s32 LeftIndex   = A.Data[1];
        s32 RightIndex  = B.Data[1];

        // We want lowest map id at the top
        if( Left.MapID < Right.MapID )          return -1;
        if( Left.MapID > Right.MapID )          return 1;

        // If map ids are the same, we want the lowest index
        // at the top.
        if( LeftIndex < RightIndex )            return -1;
        if( LeftIndex > RightIndex )            return 1;
        return 0;
    }
};


//=========================================================================

void dlg_resume_game::FillCheckpointList( void )
{
    s32 i,j;
    
    // Clear listbox
    m_pCheckpointList->DeleteAllItems();

    // Go through all the checkpoints in the current active player
    // profile and create a listbox entry for each. The data for each
    // listbox entry will contain a checkpoint level number and a 
    // checkpoint number within that table entry (there are 18 levels of
    // checkpoint per profile). We then sort it using the levelid as the
    // primary key and the checkpointid within that level as the secondary key.

    player_profile& Profile = g_StateMgr.GetActiveProfile(0);

    // First, find the top most level id in the saved list
    for( i=0; i<MAX_SAVED_LEVELS; i++ )
    {
        level_check_points& Checkpoint = Profile.GetCheckpoint(i);

        // If we have no checkpoint data, we're done.
        if( Checkpoint.MapID == -1 )
        {
            break;
        }
        const map_entry* pMapEntry = g_MapList.Find( Checkpoint.MapID, GAME_CAMPAIGN );
        // We *MUST* have that level present to show the details of that checkpoint. We
        // get the name of the level from this mapentry.
        if( !pMapEntry )
        {
            x_DebugMsg( "Resume campaign: skipping checkpoint for missing map ID %d.\n", Checkpoint.MapID );
            continue;
        }

        xwstring MapName;
        if( !DecodeFontText( pMapEntry->GetDisplayName(), MapName ) )
        {
            x_DebugMsg( "Resume campaign: skipping map ID %d with invalid UTF-8 name.\n", Checkpoint.MapID );
            continue;
        }

        // First add the "mapname" checkpoint
        if( Checkpoint.CheckPoints[0].bIsValid )
        {
            xwstring CheckpointName;
            CheckpointName += MapName;
            m_pCheckpointList->AddItem( CheckpointName, (uaddr)&Checkpoint, 0 );
        }

        // Now we construct each individual entry for that level's checkpoints.
        // ***NOTE** the first checkpoint is always the very start of the level
        for( j = 1; j < MAX_CHECKPOINTS; j++ )
        {
            if( Checkpoint.CheckPoints[j].bIsValid )
            {
                xwstring CheckpointName;

#if 0
                // Where do we get this checkpoint data from?????
                if( Checkpoint.CheckPoints[j].TableName != -1 )
                {
                    const char* pStringTable = g_StringMgr.GetString(Checkpoint.CheckPoints[j].TableName);
                    const char* pStringEntry = g_StringMgr.GetString(Checkpoint.CheckPoints[j].TitleName);

                    ASSERT( pStringTable && pStringEntry );
                    CheckpointName += xwstring( g_StringTableMgr( pStringTable, pStringEntry ) );
                }
                else
#endif
                if( j!=0 )
                {
                    CheckpointName += "     ";
                    CheckpointName += g_StringTableMgr( "ui", "IDS_CHECK_POINT" );
                    CheckpointName += xwstring(xfs( " %d", j ));
                }
                
                m_pCheckpointList->AddItem( CheckpointName, (uaddr)&Checkpoint, j );
            }
        }
    }

    if( m_pCheckpointList->GetItemCount() == 0 )
    {
        x_DebugMsg( "Resume campaign: no valid campaign checkpoints are available.\n" );
        return;
    }

    // Now we sort it using the checkpoint data as the sort key
    x_qsort( &m_pCheckpointList->GetItem(0), m_pCheckpointList->GetItemCount(), checkpoint_sort_compare() );
    
    // Set selection to LAST item in list
    m_pCheckpointList->SetSelection( ( m_pCheckpointList->GetItemCount()-1 ) );
}

//=========================================================================
