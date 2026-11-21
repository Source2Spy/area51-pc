//==============================================================================
//
//  dlg_SaveData.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"

#include "dlg_SaveData.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "SaveData/SaveDataMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "UI/ui_manager.hpp"

//==============================================================================
//  DATA
//==============================================================================

static ui_manager::dialog_tem SaveDataDialog =
{
    "IDS_SAVE_DATA_TITLE",
    1, 1,
    0,
    NULL,
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void dlg_save_data_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "save data", &SaveDataDialog, &dlg_save_data_factory );
}

//==============================================================================

ui_win* dlg_save_data_factory( s32 UserID,
                               ui_manager* pManager,
                               ui_manager::dialog_tem* pDialogTem,
                               const irect& Position,
                               ui_win* pParent,
                               s32 Flags,
                               void* pUserData )
{
    dlg_save_data* pDialog = new dlg_save_data;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );
    return pDialog;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dlg_save_data::dlg_save_data( void ) :
    m_Operation  ( SAVE_DATA_DIALOG_CREATE_PROFILE ),
    m_pPopup     ( NULL ),
    m_PopupResult( DLG_POPUP_IDLE ),
    m_Configured ( FALSE )
{
}

//==============================================================================

dlg_save_data::~dlg_save_data( void )
{
    Destroy();
}

//==============================================================================

xbool dlg_save_data::Create( s32 UserID,
                             ui_manager* pManager,
                             ui_manager::dialog_tem* pDialogTem,
                             const irect& Position,
                             ui_win* pParent,
                             s32 Flags,
                             void* pUserData )
{
    (void)pUserData;
    const xbool Success = ui_dialog::Create( UserID,
                                             pManager,
                                             pDialogTem,
                                             Position,
                                             pParent,
                                             Flags );
    m_State = DIALOG_STATE_ACTIVE;
    return Success;
}

//==============================================================================

void dlg_save_data::Destroy( void )
{
    // Cancel outstanding save callbacks before destroying the dialog.
    g_SaveDataMgr.CancelCallbacks( this );
    ui_dialog::Destroy();
}

//==============================================================================

void dlg_save_data::Configure( save_data_dialog_operation Operation )
{
    ASSERT( !m_Configured );
    m_Operation  = Operation;
    m_Configured = TRUE;
    BeginRequest();
}

//==============================================================================

void dlg_save_data::BeginRequest( void )
{
    ASSERT( m_Configured );
    if( m_Operation == SAVE_DATA_DIALOG_CREATE_PROFILE )
    {
        g_SaveDataMgr.CreateProfile( g_StateMgr.GetPendingProfileIndex(),
                                     this,
                                     &dlg_save_data::OnRequestDone );
    }
    else
    {
        g_SaveDataMgr.SaveSettings( this, &dlg_save_data::OnRequestDone );
    }
}

//==============================================================================

void dlg_save_data::OnRequestDone( void )
{
    if( g_SaveDataMgr.GetLastResult().Succeeded() )
    {
        // Activate pending data only after the save succeeds.
        if( m_Operation == SAVE_DATA_DIALOG_CREATE_PROFILE )
        {
            g_StateMgr.SetProfileNotSaved( g_StateMgr.GetPendingProfileIndex(), FALSE );
            g_StateMgr.ActivatePendingProfile();
        }
        else
        {
            g_StateMgr.ActivatePendingSettings();
        }

        g_AudioMgr.Play( "Select_Norm" );
        m_State = DIALOG_STATE_SELECT;
        return;
    }

    ShowError();
}

//==============================================================================

void dlg_save_data::ShowError( void )
{
    irect Bounds = g_UiMgr->GetUserBounds( m_UserID );
    m_pPopup = static_cast<dlg_popup*>( g_UiMgr->OpenDialog(
        m_UserID,
        "popup",
        Bounds,
        NULL,
        ui_win::WF_VISIBLE | ui_win::WF_BORDER |
        ui_win::WF_DLG_CENTER | ui_win::WF_INPUTMODAL ) );

    xwstring Navigation( g_StringTableMgr( "ui", "IDS_NAV_RETRY" ) );
    Navigation += g_StringTableMgr( "ui", "IDS_NAV_CANCEL" );
    m_PopupResult = DLG_POPUP_IDLE;
    m_pPopup->Configure( g_StringTableMgr( "ui", "IDS_SAVE_DATA_TITLE" ),
                         TRUE,
                         TRUE,
                         FALSE,
                         g_StringTableMgr( "ui", "IDS_SAVE_DATA_ERROR" ),
                         Navigation,
                         &m_PopupResult );
}

//==============================================================================

void dlg_save_data::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;

    if( !m_pPopup || (m_PopupResult == DLG_POPUP_IDLE) )
    {
        return;
    }

    // Clear the old popup state before a retry can open another one.
    const s32 Result = m_PopupResult;
    m_pPopup      = NULL;
    m_PopupResult = DLG_POPUP_IDLE;
    if( Result == DLG_POPUP_YES )
    {
        BeginRequest();
    }
    else
    {
        m_State = DIALOG_STATE_BACK;
    }
}
