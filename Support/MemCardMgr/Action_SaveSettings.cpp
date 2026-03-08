///////////////////////////////////////////////////////////////////////////////
//
//  Action_SaveSettings.cpp
//  Wed Feb 26 11:43:28 2003
//
//  Quick note on the state machine:
//
//      The queue_machine class is an improvement over the old array and switch
//      mechanism used in the Hobbit. What it does is maintain an array of ptrs
//      to member functions inside MemCardMgr. These methods all follow similar
//      naming conventions to the previous implementation.
//
//      The only difference between the paradigms are we constantly execute the
//      top of the stack. This gets rid of any requirement for a switch. I have
//      arranged each of the actions into separate files for readability.
//
///////////////////////////////////////////////////////////////////////////////
#include "MemCardMgr.hpp"
#include "e_Memcard.hpp"
#include "StringMgr/StringMgr.hpp"
#include "Dialogs/dlg_MCMessage.hpp"


///////////////////////////////////////////////////////////////////////////////
//
//  Includes
//
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
//
// Globals
//
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
//
//  Memory card state methods
//
///////////////////////////////////////////////////////////////////////////////

void MemCardMgr::MC_STATE_OVERWRITE_SETTINGS_CONFIRM( void )
{
    const xwchar* pText = g_StringTableMgr( "ui", "MC_PROMPT_FOR_OVERWRITE_SETTINGS" );

    ChangeState( __id MC_STATE_OVERWRITE_SETTINGS_CONFIRM_WAIT );

    OptionBox(
        g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER" ),  
        pText,
        g_StringTableMgr( "ui", "IDS_MEMCARD_YES"    ),
        g_StringTableMgr( "ui", "IDS_MEMCARD_NO"     )
        );

    return;
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_OVERWRITE_SETTINGS_CONFIRM_WAIT( void )
{
    switch( m_MessageResult )
    {
    case DLG_MCMESSAGE_YES:
        {
            // recheck the card before starting the format (make sure it wasn't changed etc)
            ChangeState( __id MC_STATE_OVERWRITE_SETTINGS_RECHECK );
            g_MemcardMgr.AsyncMount( m_iCard );
            break;
        }

    case DLG_MCMESSAGE_IDLE:
        break;

    case DLG_MCMESSAGE_NO:
        {
            // Cancel overwrite
            condition& Pending = GetPendingCondition( m_iCard );
            Pending.bCancelled = TRUE;
            FlushStateStack();
            PushState( __id MC_STATE_UNMOUNT );
            PushState( __id MC_STATE_FINISH  );
            break;
        }

    default:
        break;
    }
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_OVERWRITE_SETTINGS_RECHECK( void )
{
    switch( GetMcResult() )
    {
    case kPENDING:
        return;

    case kRESET:
    case kFAILURE:
        {
            // Card was changed, or other error, abort operation
            ChangeState(  __id MC_STATE_SAVE_SETTINGS_FAILED );
            return;
        }

    case kSUCCESS:
        {
            ChangeState(  __id MC_STATE_SAVE_SETTINGS );
            return;
        }
    }
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_CREATE_SETTINGS( void )
{
    condition& Pending = GetPendingCondition( m_iCard );

    // make sure that we have enough space on the PC
    if( Pending.BytesFree < g_StateMgr.GetSettingsSaveSize() )
    {
        ChangeState( __id MC_STATE_CREATE_PROFILE_FAILED );
        return;
    }

    if( ! Pending.ErrorCode )
    {

        // display warning message
        const xwchar* pText;

#ifdef TARGET_PC
        pText = g_StringTableMgr( "ui", "MC_SAVING_SETTINGS_XBOX" );
#else   
        pText = g_StringTableMgr( "ui", "MC_SAVING_SETTINGS_XBOX" );
#endif
        WarningBox(
            g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER"   ),  
            pText,
            FALSE
            );
			
        //g_MemcardMgr.AsyncCreateDirectory( "Game Settings" );
        g_MemcardMgr.AsyncSetDirectory( "" ); //We dont using settings folders on PC.
        ChangeState( __id MC_STATE_CREATE_SETTINGS_CREATE_DIR_WAIT );

        return;
    }
    PopState();
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_CREATE_SETTINGS_CREATE_DIR_WAIT( void )
{
    condition& Pending = GetPendingCondition(m_iCard);

    switch( GetMcResult( ))
    {
    case kPENDING:
        return;

    case kSUCCESS:
        ChangeState( __id MC_STATE_SAVE_SETTINGS_SET_DIR_WAIT );
        return;
    case kFAILURE:
    case kRESET:
        if( Pending.bFileAlreadyExists )
        {
            Pending.ErrorCode = 0;
            ChangeState( __id MC_STATE_SAVE_SETTINGS );
            return;
        }
        else
        {
            ChangeState( __id MC_STATE_SAVE_SETTINGS_FAILED );
            return;
        }

    default:
        ASSERT(0);
    }
    PopState();
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_SAVE_SETTINGS( void )
{
    condition& Pending = GetPendingCondition(m_iCard);

    if( ! Pending.ErrorCode )
    {
        //  Display overwrite message
        const xwchar* pText;
        if( ! m_iCard )
            pText = g_StringTableMgr( "ui", "MC_OVERWRITE_DATA_SLOT1" );
        else
            pText = g_StringTableMgr( "ui", "MC_OVERWRITE_DATA_SLOT2" );
        WarningBox(
            g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER"   ),  
            pText,
            FALSE
            );

        // Before we can save settings, or a patch, we need to switch to that directory and
        // create it if necessary.
        //g_MemcardMgr.AsyncSetDirectory( "Game Settings" );
        g_MemcardMgr.AsyncSetDirectory( "" ); //We dont using settings folders on PC.
        ChangeState( __id MC_STATE_SAVE_SETTINGS_SET_DIR_WAIT );
    }
    else
    {
        ChangeState( __id MC_STATE_SAVE_SETTINGS_FAILED );
    }
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_SAVE_SETTINGS_SET_DIR_WAIT( void )
{
    condition& Pending = GetPendingCondition(m_iCard);
    (void)Pending;


    switch( GetMcResult( ))
    {
    case kPENDING:
        return;

    case kSUCCESS:
        {
            s32 RoundedSize = AllocBuffer( sizeof(global_settings) );
            global_settings* pSettings = (global_settings*)m_pLoadBuffer;

            *pSettings = g_StateMgr.GetPendingSettings();
            pSettings->Checksum();
            g_MemcardMgr.SetIconDisplayName( "Settings" );
            
            g_MemcardMgr.AsyncWriteFile( xfs("%s%s", m_SavePrefix, m_OptionsPostfix), m_pLoadBuffer, RoundedSize );    
            ChangeState( __id MC_STATE_SAVE_SETTINGS_WRITE_WAIT );
            return;
        }

    case kFAILURE:
    case kRESET:
        Pending.ErrorCode = 0;
        ChangeState( __id MC_STATE_CREATE_SETTINGS       );
        return;

    default:
        ASSERT(0);
    }
    PopState();
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_SAVE_SETTINGS_WRITE_WAIT( void )
{

    switch( GetMcResult( ))
    {
    case kPENDING:
        return;

    case kSUCCESS:
        FreeBuffer();
        g_StateMgr.GetPendingSettings().Checksum();
        g_StateMgr.GetActiveSettings().Checksum();

        m_bForcePoll[m_iCard] = true;
        ChangeState( __id MC_STATE_SAVE_SETTINGS_SUCCESS );
        return;

    case kFAILURE:
    case kRESET:
        FreeBuffer();
        ChangeState( __id MC_STATE_SAVE_SETTINGS_FAILED );
        return;

    default:
        ASSERT(0);
    }
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_SAVE_SETTINGS_FAILED(void)
{
    xwstring MessageText;
    xwstring NavText;

    condition& Pending = GetPendingCondition(m_iCard);

#if defined(TARGET_XBOX)
    m_BlocksRequired = ( (g_StateMgr.GetSettingsSaveSize() - Pending.BytesFree) + 16383 ) / 16384;
    MessageText = xwstring( xfs( (const char*)xstring(g_StringTableMgr( "ui", "MC_NOT_ENOUGH_FREE_SPACE_SLOT1_SETTINGS_XBOX" )), m_BlocksRequired ) );
    NavText     = g_StringTableMgr( "ui", "IDS_NAV_DONT_FREE_BLOCKS" );

    xbool SecondOption = FALSE;
    if( GameMgr.GameInProgress() == FALSE )
    {
        NavText    += g_StringTableMgr( "ui", "IDS_NAV_FREE_MORE_BLOCKS" );
        SecondOption = TRUE;
    }

    PopUpBox( 
        g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER" ),
        MessageText, 
        NavText, 
        TRUE, 
        SecondOption, 
        FALSE );
#elif defined(TARGET_PC)
    s32 saveSize = g_StateMgr.GetProfileSaveSize();
    m_BlocksRequired = ( Pending.BytesFree < saveSize ) ? ( (saveSize - Pending.BytesFree) + 1023 ) / 1024 : 0;

    MessageText = xwstring( xfs( (const char*)xstring(g_StringTableMgr( "ui", "MC_NOT_ENOUGH_FREE_SPACE_SLOT1_SETTINGS_XBOX" )), m_BlocksRequired ) );
    NavText     = g_StringTableMgr( "ui", "IDS_OK" );

    PopUpBox(
        g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER" ),
        MessageText,
        NavText,
        TRUE,
        FALSE,
        FALSE );
#endif

    FlushStateStack();
    PushState( __id MC_STATE_SAVE_SETTINGS_FAILED_WAIT );
    PushState( __id MC_STATE_UNMOUNT                   );
    PushState( __id MC_STATE_FINISH                    );
    return;
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_SAVE_SETTINGS_FAILED_WAIT(void)
{
    // wait for user response
    condition& Pending = GetPendingCondition( m_iCard );

    switch( m_MessageResult )
    {
    case DLG_MCMESSAGE_IDLE:
        return;

    case DLG_MCMESSAGE_NO:
        // continue without saving
        Pending.bCancelled = TRUE;
        break;

    case DLG_MCMESSAGE_YES:
        // retry
        break;
    }

    // finish processing
    PopState();
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_SAVE_SETTINGS_SUCCESS( void )
{
    // reset timer
    m_CardWait = 0;

    if( m_MemcardMode == MEMCARD_CREATE_MODE )
    {
        WarningBox(
            g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER" ),  
            g_StringTableMgr( "ui", "MC_SAVE_SUCCESS"  ),
            FALSE
            );
    }
    else
    {
        WarningBox(
            g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER" ),  
            g_StringTableMgr( "ui", "MC_OVERWRITE_SUCCESS"  ),
            FALSE
            );
    }

    ChangeState( __id MC_STATE_SAVE_SETTINGS_SUCCESS_WAIT );
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_SAVE_SETTINGS_SUCCESS_WAIT( void )
{
    // wait for message to timeout
    m_CardWait++;

    // timeout?
    if( m_CardWait >= 5 )
        PopState();
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_ACTION_SAVE_SETTINGS( void )
{
    InitAction( MEMCARD_SAVE_MODE );

    // setup for writing
    m_CardDataMode = SM_CARDMODE_SETTINGS;
    m_iCard = g_StateMgr.GetSettingsCardSlot();
    // range check
    ASSERT( m_iCard >= 0 );
    ASSERT( m_iCard < MAX_CARD_SLOTS  );

    // push states
    PushState( __id MC_STATE_MOUNT                 );
    PushState( __id MC_STATE_SAVE_SETTINGS         );
    PushState( __id MC_STATE_UNMOUNT               );
    PushState( __id MC_STATE_FINISH                );
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_ACTION_CREATE_SETTINGS( void )
{
    InitAction( MEMCARD_CREATE_MODE );

    // setup for writing
    m_CardDataMode = SM_CARDMODE_SETTINGS;
    m_iCard = g_StateMgr.GetSettingsCardSlot();
    // range check
    ASSERT( m_iCard >= 0 );
    ASSERT( m_iCard < MAX_CARD_SLOTS  );

    // push states
    PushState( __id MC_STATE_MOUNT                 );
    PushState( __id MC_STATE_CREATE_SETTINGS       );
    PushState( __id MC_STATE_UNMOUNT               );
    PushState( __id MC_STATE_FINISH                );
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_ACTION_OVERWRITE_SETTINGS( void )
{
    InitAction( MEMCARD_SAVE_MODE );

    // setup for writing
    m_CardDataMode = SM_CARDMODE_SETTINGS;
    m_iCard = g_StateMgr.GetSettingsCardSlot();
    // range check
    ASSERT( m_iCard >= 0 );
    ASSERT( m_iCard < MAX_CARD_SLOTS );

    // push states
    PushState( __id MC_STATE_MOUNT                      );
    PushState( __id MC_STATE_OVERWRITE_SETTINGS_CONFIRM );
    PushState( __id MC_STATE_UNMOUNT                    );
    PushState( __id MC_STATE_FINISH                     );
}

//==---------------------------------------------------------------------------
