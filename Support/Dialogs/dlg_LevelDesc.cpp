//=========================================================================
//
//  dlg_level_desc.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "dlg_LevelDesc.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StateMgr/MapList.hpp"
#include "StringMgr/StringMgr.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"

static void SetUTF8Label( ui_win* pControl, const char* pText )
{
    xwstring Text;
    if( DecodeFontText( pText, Text ) )
    {
        pControl->SetLabel( Text );
    }
}

xcolor g_TextRectLoadScreen( 0, 0, 0, 128);

//=========================================================================
//  Level Desc Dialog
//=========================================================================

enum controls
{
    IDC_LEVEL_DESC,
    IDC_MAP_TITLE_TEXT,
    IDC_MAP_DESC_TEXT,
    IDC_GAME_TYPE_TEXT,
    IDC_GAME_DESC_TEXT,
    IDC_LEVEL_DESC_LOADING_TEXT,
    IDC_LEVEL_DESC_LOADING_PIPS,
    IDC_LEVEL_DESC_NAV_TEXT
};

ui_manager::control_tem LevelDescControls[] =
{
    { IDC_LEVEL_DESC,               "IDS_NULL",        "text",        0, 308, 480,  30,  0, 0, 1, 1, 0 },
    { IDC_MAP_TITLE_TEXT,           "IDS_NULL",        "text",       40,   8, 426,  22, 0, 0, 0, 0, ui_win::WF_VISIBLE|ui_win::WF_STATIC },
    { IDC_MAP_DESC_TEXT,            "IDS_NULL",        "text",       40,  38, 426,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE|ui_win::WF_STATIC },
    { IDC_GAME_TYPE_TEXT,           "IDS_NULL",        "text",       40, 190, 426,  22, 0, 0, 0, 0, ui_win::WF_VISIBLE|ui_win::WF_STATIC },
    { IDC_GAME_DESC_TEXT,           "IDS_NULL",        "text",       40, 220, 426,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE|ui_win::WF_STATIC },
    { IDC_LEVEL_DESC_LOADING_TEXT,  "IDS_NULL",        "text",      235, 395, 230,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE|ui_win::WF_STATIC },
    { IDC_LEVEL_DESC_LOADING_PIPS,  "IDS_NULL",        "text",      465, 395,  50,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE|ui_win::WF_STATIC },
    { IDC_LEVEL_DESC_NAV_TEXT,      "IDS_NULL",        "text",       25, 395, 200,  16, 0, 0, 0, 0, ui_win::WF_VISIBLE },
};

ui_manager::dialog_tem LevelDescDialog =
{
    "IDS_LEVEL_DESC",
    1, 9,
    sizeof(LevelDescControls)/sizeof(ui_manager::control_tem),
    &LevelDescControls[0],
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_level_desc_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "level desc", &LevelDescDialog, &dlg_level_desc_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_level_desc_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_level_desc* pDialog = new dlg_level_desc;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_level_desc
//=========================================================================

dlg_level_desc::dlg_level_desc( void )
{
}

//=========================================================================

dlg_level_desc::~dlg_level_desc( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_level_desc::Create( s32                        UserID,
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

    m_MapTitleText      = (ui_text*) FindChildByID( IDC_MAP_TITLE_TEXT          );
    m_MapDescText       = (ui_text*) FindChildByID( IDC_MAP_DESC_TEXT           );
    m_GameTypeText      = (ui_text*) FindChildByID( IDC_GAME_TYPE_TEXT          );
    m_GameDescText      = (ui_text*) FindChildByID( IDC_GAME_DESC_TEXT          );
    m_pLoadingText      = (ui_text*) FindChildByID( IDC_LEVEL_DESC_LOADING_TEXT );
    m_pLoadingPips      = (ui_text*) FindChildByID( IDC_LEVEL_DESC_LOADING_PIPS );
    m_pNavText          = (ui_text*) FindChildByID( IDC_LEVEL_DESC_NAV_TEXT     );

    // initialize text strings    
    m_MapTitleText->SetFlag( ui_win::WF_VISIBLE, TRUE );
    m_MapTitleText->SetLabelFlags( ui_font::h_right|ui_font::v_top );//|ui_font::clip_r_justify );

    m_MapDescText->SetFlag( ui_win::WF_VISIBLE, TRUE );
    
    // descriptions are longer in most PAL languages, so, left justify the text
    if( x_GetTerritory() == XL_TERRITORY_EUROPE )
    {
        m_MapDescText->SetLabelFlags( ui_font::h_left|ui_font::v_top ); //|ui_font::clip_r_justify );
    }
    else
    {
        m_MapDescText->SetLabelFlags( ui_font::h_right|ui_font::v_top ); //|ui_font::clip_r_justify );
    }

    m_MapDescText->UseSmallText(TRUE);

    m_GameTypeText->SetFlag( ui_win::WF_VISIBLE, TRUE );
    m_GameTypeText->SetLabelFlags( ui_font::h_left|ui_font::v_top );

    m_GameDescText->SetFlag( ui_win::WF_VISIBLE, TRUE );
    m_GameDescText->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    m_GameDescText->UseSmallText(TRUE);

    // get text from map list
    const map_entry* pEntry = g_MapList.Find( g_ActiveConfig.GetLevelID(), g_ActiveConfig.GetGameTypeID() );
    if( pEntry )
    {
        SetUTF8Label( m_MapTitleText, pEntry->GetDisplayName() );
        SetUTF8Label( m_MapDescText, pEntry->GetDescription() );
        SetUTF8Label( m_GameTypeText, pEntry->GetGameTypeName() );
        SetUTF8Label( m_GameDescText, pEntry->GetGameRules() );
    }

    // initialize loading text
    m_pLoadingText->SetFlag( ui_win::WF_VISIBLE, TRUE );
    m_pLoadingText->SetLabelFlags( ui_font::h_right|ui_font::v_top );
    m_pLoadingText->UseSmallText(TRUE);

    // build loading text string
    xwstring LoadText;
    LoadText = g_StringTableMgr( "ui", "IDS_LOADING_MSG" );
    if( x_GetTerritory() == XL_TERRITORY_AMERICA )
    {
        LoadText += " ";
        LoadText += g_ActiveConfig.GetShortGameType();
        LoadText += ":";
        LoadText += g_ActiveConfig.GetLevelName();
    }
    m_pLoadingText->SetLabel( LoadText );

    // initialize loading pips    
    m_pLoadingPips->SetFlag( ui_win::WF_VISIBLE, TRUE );
    m_pLoadingPips->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    m_pLoadingPips->UseSmallText(TRUE);

    m_pNavText->SetLabel( g_StringTableMgr( "ui", "IDS_NULL") );
    m_pNavText->SetFlag( ui_win::WF_VISIBLE, TRUE );
    m_pNavText->SetLabelFlags( ui_font::h_left|ui_font::v_top );
    m_pNavText->UseSmallText(TRUE);

    // load the appropriate background
    xwstring LoadName( "LOADING/UI_LoadScreen_MP_" );

    if( IN_RANGE( 2000, g_ActiveConfig.GetLevelID(), 2999 ) )
    {
        LoadName += (const char*)xfs("%d", g_ActiveConfig.GetLevelID());
    }
    LoadName += ".xbmp";
    g_UiMgr->LoadBackground ( "mp_load", xstring(LoadName) );

    // disable the highlight
    g_UiMgr->DisableScreenHighlight();

    // make the dialog active
    m_State           = DIALOG_STATE_ACTIVE;
    m_LoadingComplete = FALSE;
    m_ForceExit       = FALSE;
    m_Mode            = LEVEL_DESC_INITIAL;
    m_LoadTimeElapsed = 0.0f;

    g_UiMgr->SetUserBackground( g_UiUserID, "mp_load" );

    // Return success code
    return Success;
}

//=========================================================================

void dlg_level_desc::Destroy( void )
{
    ui_dialog::Destroy();

    g_UiMgr->UnloadBackground( "mp_load" );
    g_UiMgr->SetUserBackground( g_UiUserID, "" );

    // kill screen wipe
    g_UiMgr->ResetScreenWipe();
}

//=========================================================================

void dlg_level_desc::Render( s32 ox, s32 oy )
{
    (void)ox;
    (void)oy;

    if( (GetFlags() & ui_win::WF_VISIBLE)==0 )
    {
        return;
    }

    // descriptions are longer in most PAL languages, so, since the text is left justified
    // we'll need a dark rect to cover the screen to darken the map bitmap so the white text
    // won't bleed into it.
    if( x_GetTerritory() == XL_TERRITORY_EUROPE )
    {
        const view* pView = eng_GetView();
        rect r;
        pView->GetViewport( r );
        irect ir( (s32)r.Min.X, (s32)r.Min.Y, (s32)r.Max.X, (s32)r.Max.Y );
        g_UIRenderer.DrawRect( ir, g_TextRectLoadScreen );
    }

    // render the normal ui dialog
    ui_dialog::Render( ox, oy );

    // render loading text
    xwstring LoadText("");

    switch( (s32)(m_LoadTimeElapsed*4.0f) % 4 )
    {
    case 0: LoadText += "   ";   break;
    case 1: LoadText += ".  ";   break;
    case 2: LoadText += ".. ";   break;
    case 3: LoadText += "...";   break;
    }
    m_pLoadingPips->SetLabel( LoadText );
}

//=========================================================================

void dlg_level_desc::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if( m_Mode != LEVEL_DESC_INTERLEVEL )
        return;

    if( m_State == DIALOG_STATE_ACTIVE )
    {
        m_State = DIALOG_STATE_SELECT;
    }
}

//=========================================================================

void dlg_level_desc::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    m_LoadTimeElapsed += DeltaTime;

    // get text from map list
    const map_entry* pEntry = g_MapList.Find( g_ActiveConfig.GetLevelID(), g_ActiveConfig.GetGameTypeID() );
    if( pEntry )
    {
        SetUTF8Label( m_MapTitleText, pEntry->GetDisplayName() );
        SetUTF8Label( m_MapDescText, pEntry->GetDescription() );
        SetUTF8Label( m_GameTypeText, pEntry->GetGameTypeName() );
        SetUTF8Label( m_GameDescText, pEntry->GetGameRules() );
    }

    if( m_LoadingComplete )
    {
        m_State = DIALOG_STATE_EXIT;
    }

    if( m_Mode == LEVEL_DESC_INTERLEVEL )
    {
        m_pNavText->SetLabel( g_StringTableMgr( "ui", "IDS_NAV_LEADERBOARD" ) );
    }

}

//=========================================================================

void dlg_level_desc::LoadingComplete( void )
{
    m_LoadingComplete = TRUE;
}

//=========================================================================

void dlg_level_desc::Configure( level_desc_mode Mode )
{
    m_Mode = Mode;
}
