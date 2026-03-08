///////////////////////////////////////////////////////////////////////////////
// 
// Action_CreateProfile.cpp
// Wed Feb 26 11:43:28 2003
// 
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
//
//  Includes
//
///////////////////////////////////////////////////////////////////////////////
#include "MemCardMgr.hpp"
#include "e_Memcard.hpp"
#include "StringMgr/StringMgr.hpp"
#include "Dialogs/dlg_MCMessage.hpp"



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

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_CREATE_PROFILE( void )
{
    condition& Pending = GetPendingCondition(m_PreservedProfile[m_iPlayer].CardID);

    // make sure that we have enough space on the PC
    if( Pending.BytesFree < g_StateMgr.GetProfileSaveSize() )
    {
        ChangeState( __id MC_STATE_CREATE_PROFILE_FAILED );
        return;
    }

    // handle unformatted cards ***********************************************

    if( Pending.bUnformatted )
    {
        //  "Memory card (8MB) (for Playstation®2)\n"
        //  "in MEMORY CARD slot 1 is unformatted.\n"
        //  "Format memory card (8MB)(for Playstation®2)?\n"),

        const xwchar* pText;
        if( ! m_PreservedProfile[m_iPlayer].CardID )
            pText = g_StringTableMgr( "ui", "MC_FORMAT_PROMPT_SLOT1" );
        else
            pText = g_StringTableMgr( "ui", "MC_FORMAT_PROMPT_SLOT2" );

        ChangeState( __id MC_STATE_ASK_FORMAT );
        OptionBox(
            g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER" ),  
            pText,
            g_StringTableMgr( "ui", "IDS_MEMCARD_YES"    ),
            g_StringTableMgr( "ui", "IDS_MEMCARD_NO"     )
        );
        return;
    }

    // create profile *********************************************************

    if( ! Pending.ErrorCode )
    {
        condition& Condition = GetCondition(m_PreservedProfile[m_iPlayer].CardID);
        xbool Found = FALSE;
        s32 i;

        for( i=0; i<Condition.InfoList.GetCount(); i++ )
        {
            m_PreservedProfile[m_iPlayer].ProfileID = i;
            m_PreservedProfile[m_iPlayer].Dir = xfs("%sA51%05d", m_SavePrefix, m_PreservedProfile[m_iPlayer].ProfileID );
            if( Condition.InfoList[i].Dir != m_PreservedProfile[m_iPlayer].Dir )
            {
                Found = TRUE;
                break;
            }
        }
        if( !Found )
        {
            m_PreservedProfile[m_iPlayer].ProfileID = Condition.InfoList.GetCount();
            m_PreservedProfile[m_iPlayer].Dir = xfs( "%sA51%05d", m_SavePrefix, m_PreservedProfile[m_iPlayer].ProfileID );
        }

        ChangeState( __id MC_STATE_CREATE_PROFILE_CREATE_DIR_WAIT );

        player_profile& Profile      = g_StateMgr.GetPendingProfile();
        m_PreservedProfile[m_iPlayer].CardID    = m_iCard;
        m_PreservedProfile[m_iPlayer].Ver       = Profile.GetVersion();
        m_PreservedProfile[m_iPlayer].Name      = xwstring(Profile.GetProfileName());
        m_PreservedProfile[m_iPlayer].Hash      = Profile.GetHash();

        //**NOTE** This will be the index in to the profile list of this newly
        // created profile!
        //

        // Put up warning .....................................................

        const xwchar* pText;
        if( ! m_PreservedProfile[m_iPlayer].CardID )
            pText = g_StringTableMgr( "ui", "MC_SAVING_TO_MEMCARD_SLOT1" );
        else
            pText = g_StringTableMgr( "ui", "MC_SAVING_TO_MEMCARD_SLOT2" );
        WarningBox(
            g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER"   ),  
            pText,
            FALSE
        );

        // Create directory ...................................................
        g_MemcardMgr.AsyncSetDirectory( "" ); //g_MemcardMgr.AsyncCreateDirectory( m_PreservedProfile[m_iPlayer].Dir ); //We dont using settings folders on PC.
        return;
    }
    PopState();
}



//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_CREATE_PROFILE_CREATE_DIR_WAIT( void )
{
    switch( GetMcResult( ))
    {
        case kPENDING:
            return;

        case kSUCCESS:
        {
            ChangeState( __id MC_STATE_CREATE_PROFILE_SET_DIR_WAIT );
            g_MemcardMgr.AsyncSetDirectory( "" ); //g_MemcardMgr.AsyncSetDirectory( m_PreservedProfile[m_iPlayer].Dir ); //We dont using settings folders on PC.
            return;
        }

        case kFAILURE:
        case kRESET:
        {
            // we failed! display a message to the user!
            ChangeState( __id MC_STATE_CREATE_PROFILE_FAILED );
            return;
        }

        default:
            ASSERT(0);
    }
    PopState();
}



//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_CREATE_PROFILE_SET_DIR_WAIT( void )
{
    switch( GetMcResult( ))
    {
        case kPENDING:
            return;

        case kSUCCESS:
        {
            ChangeState( __id MC_STATE_PROFILE_WRITE_WAIT );
            s32 RoundedSize = (sizeof(player_profile)+1023) &~1023;
            AllocBuffer( RoundedSize );

            player_profile* pSavedProfile = (player_profile*)m_pLoadBuffer;

            // Preserve profile info ..........................................
            player_profile& Profile = g_StateMgr.GetPendingProfile();

            // copy to intermediate buffer ....................................
            x_memcpy( m_pLoadBuffer, &Profile, sizeof(player_profile) );
            pSavedProfile->Checksum();
            g_MemcardMgr.SetIconDisplayName( pSavedProfile->GetProfileName() );
            x_encrypt( pSavedProfile, sizeof( player_profile ), m_EncryptionKey );

            // write intermediate .............................................
            g_MemcardMgr.AsyncWriteFile( (const char*)m_PreservedProfile[m_iPlayer].Dir, (byte*)m_pLoadBuffer, RoundedSize );
            return;
        }

        case kRESET:
        case kFAILURE:
            // we failed! display a message to the user!
            ChangeState( __id MC_STATE_CREATE_PROFILE_FAILED );
            return;

        default:
            ASSERT(0);
    }
    PopState();
}



//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_CREATE_PROFILE_FAILED( void )
{
    xwstring MessageText;
    xwstring NavText;
    
    condition& Pending = GetPendingCondition(m_PreservedProfile[m_iPlayer].CardID);
    
#if defined(TARGET_XBOX)
    m_BlocksRequired = ( (g_StateMgr.GetProfileSaveSize() - Pending.BytesFree) + 16383 ) / 16384;
        
    MessageText = xwstring( xfs( (const char*)xstring(g_StringTableMgr( "ui", "MC_NOT_ENOUGH_FREE_SPACE_SLOT1_XBOX" )), m_BlocksRequired ) );
    NavText     = g_StringTableMgr( "ui", "IDS_NAV_DONT_FREE_BLOCKS" );
    NavText    += g_StringTableMgr( "ui", "IDS_NAV_FREE_MORE_BLOCKS" );
    
    PopUpBox( 
        g_StringTableMgr( "ui", "IDS_MEMCARD_HEADER" ),
        MessageText, 
        NavText, 
        TRUE, 
        TRUE, 
        FALSE );
#elif defined(TARGET_PC)  
    s32 saveSize = g_StateMgr.GetProfileSaveSize();
    m_BlocksRequired = ( Pending.BytesFree < saveSize ) ? ( (saveSize - Pending.BytesFree) + 1023 ) / 1024 : 0;
   
    MessageText = xwstring(xfs((const char*)xstring(g_StringTableMgr("ui", "MC_NOT_ENOUGH_FREE_SPACE_SLOT1_XBOX")), m_BlocksRequired));
    NavText = g_StringTableMgr("ui", "IDS_OK");
    
    PopUpBox( 
        g_StringTableMgr("ui", "IDS_DISK_SPACE_HEADER"),
        MessageText, 
        NavText, 
        TRUE, 
        FALSE, 
        FALSE);
#endif
     
    FlushStateStack();
    PushState( __id MC_STATE_CREATE_PROFILE_FAILED_WAIT );
    PushState( __id MC_STATE_UNMOUNT                    );
    PushState( __id MC_STATE_FINISH                     );
    return;
}

//==---------------------------------------------------------------------------

void MemCardMgr::MC_STATE_CREATE_PROFILE_FAILED_WAIT( void )
{
    // wait for user response
    condition& Pending = GetPendingCondition(m_PreservedProfile[m_iPlayer].CardID);

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


///////////////////////////////////////////////////////////////////////////////
//
//  Memory card action methods ( entry point )
//
///////////////////////////////////////////////////////////////////////////////



//==---------------------------------------------------------------------------

void MemCardMgr::MC_ACTION_CREATE_PROFILE( void )
{
    InitAction( MEMCARD_CREATE_MODE );

    // setup for writing ******************************************************
    m_iCard = m_PreservedProfile[m_iPlayer].CardID;

    // push states ************************************************************
    PushState( __id MC_STATE_MOUNT                );
    PushState( __id MC_STATE_CREATE_PROFILE       );
    PushState( __id MC_STATE_UNMOUNT              );
    PushState( __id MC_STATE_FINISH               );
}

