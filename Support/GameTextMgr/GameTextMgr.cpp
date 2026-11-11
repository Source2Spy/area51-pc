//=========================================================================
// GameTextMgr
//=========================================================================

//=========================================================================
// Includes
//=========================================================================
#include "GameTextMgr.hpp"
#include "StringMgr/StringMgr.hpp"
#include "ConversationMgr/ConversationMgr.hpp"
#include "Objects/HudObject.hpp"
#include "Sound/SimpleSoundEmitter.hpp"
#include "../MiscUtils/SimpleUtils.hpp"

#ifndef X_EDITOR
#include "NetworkMgr/Messages/MsgMgr.hpp"
#endif

//=========================================================================
// GLOBALS
//=========================================================================
game_text_mgr g_GameTextMgr;


//=========================================================================

void game_text_mgr::DisplayMessage( const char* pTableName, const char* pTitleString )
{
    Reset();

    m_Message.MainString       = g_StringTableMgr( pTableName, pTitleString );
    m_Message.SubTitleString   = g_StringTableMgr.GetSubTitleSpeaker( pTableName, pTitleString );
    m_Message.SoundDescString  = g_StringTableMgr.GetSoundDescString( pTableName, pTitleString );
    
    
    // If there is a sound specified start warming it up.
    if( !m_Message.SoundDescString.IsEmpty() )
    {
        m_Message.Guid = g_ObjMgr.CreateObject( simple_sound_emitter::GetObjectType() );
        object* pObj = g_ObjMgr.GetObjectByGuid( m_Message.Guid );
        player* pPlayer = SMP_UTIL_GetActivePlayer();
        if( pObj && pPlayer )
        {
            xstring String( m_Message.SoundDescString );
        
            pObj->OnTransform( pPlayer->GetL2W() );
            
            // Use the player's position but pass in zone 0 so we don't try to propagate the sound.
            m_Message.SoundID = g_ConverseMgr.PlayStream( (const char*)String, 
                                                          pPlayer->GetPosition(), 
                                                          m_Message.Guid, 
                                                          pObj->GetZone1(), 
                                                          IMMEDIATE_PLAY, 
                                                          FALSE );
        }
        
        if( m_Message.SoundID )
        {
            m_Message.State = 0;
            m_Message.State |= AUDIO_WARMING;
        }
    }
    else
    {
        
        // Functionality moved to MsgClient
#ifndef X_EDITOR
        MsgMgr.Message( MSG_GOAL_STRING, 0, (saddr)(const xwchar*)m_Message.MainString );
#endif
    }


} 

//=========================================================================

void game_text_mgr::Init( void )
{   
    m_Message.MainString.Clear();
    m_Message.SoundDescString.Clear();
    m_Message.SubTitleString.Clear();
    m_Message.SoundID           = 0;
    m_Message.Guid              = 0;
    m_Message.State             = 0;
    m_Flags                     = 0;
}

//=========================================================================

void game_text_mgr::Reset( void )
{
    if( m_Message.SoundID )
    {
        if( g_ConverseMgr.IsActive( m_Message.SoundID ) )
            g_ConverseMgr.Stop( m_Message.SoundID );

        m_Message.SoundID = 0;
    }

    if( m_Message.Guid )
    {
        g_ObjMgr.DestroyObject( m_Message.Guid );
        m_Message.Guid = 0;
    }

    m_Message.MainString.Clear();
    m_Message.SoundDescString.Clear();
    m_Message.SubTitleString.Clear();
    m_Message.State             = 0;
    m_Flags                     = 0;
}

//=========================================================================

void game_text_mgr::Update( f32 DeltaTime )
{
    (void)DeltaTime;

    if( m_Message.SoundDescString.IsEmpty() )
        return;

    if( !m_Message.SoundDescString.IsEmpty() )
    {
        // Once the audio start we are no longer dependent on it.
        if( (m_Message.State & AUDIO_PLAYING) || (m_Message.State & AUDIO_FINISHED) )
        {
            ;
        }
        else if( m_Message.State & AUDIO_WARMING )
        {
            // Still warming up.
            if( g_ConverseMgr.IsReadyToPlay( m_Message.SoundID ) == FALSE )
                return;

            // The sound is warmed up so lets start rendering the text and playing the audio.
            m_Message.State = 0;
            m_Message.State |= AUDIO_PLAYING;
            
            g_ConverseMgr.StartSound( m_Message.SoundID );
        }

        if( m_Message.State & AUDIO_PLAYING )
        {
            // If the sound is dead, destory the object.  If its still active update its position to the player position.
            if( g_ConverseMgr.IsActive( m_Message.SoundID ) == FALSE )
            {
                Reset();
            }
            else
            {
                object* pObj    = g_ObjMgr.GetObjectByGuid( m_Message.Guid );
                player* pPlayer = SMP_UTIL_GetActivePlayer();
                if( pObj && pPlayer )
                    pObj->OnTransform( pPlayer->GetL2W() );
            }
        }   
    }
    else
    {
        ;
    }
}

//=========================================================================

void game_text_mgr::Kill( void )
{
    Reset();
}
        
//=========================================================================
