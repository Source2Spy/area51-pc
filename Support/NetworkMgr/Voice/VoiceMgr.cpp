//==============================================================================
//
//  VoiceMgr.cpp
//
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "VoiceMgr.hpp"
#include "VoiceProxy.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Connection/GameServer.hpp"

#include "Objects/Actor/Actor.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"

voice_mgr g_VoiceMgr;

//==============================================================================
//  FUNCTIONS
//==============================================================================

voice_mgr::voice_mgr( void )
{
    m_Initialized         = FALSE;
    m_LocalMutedPlayers   = 0;
    m_bGameIsVoiceEnabled = TRUE;
}

//==============================================================================

voice_mgr::~voice_mgr( void )
{
    ASSERT(!m_Initialized);
}

//==============================================================================

void voice_mgr::Init( xbool EnableHeadset )
{
    ASSERT(!m_Initialized);
    m_Initialized            = TRUE;
    m_bGameIsVoiceEnabled    = TRUE;
    m_HeadsetEnabled         = EnableHeadset;
    m_LocalVoiceOwner        = -1;
    m_LocalVoiceTalkType     = TALK_NOT_TALKING;
    m_CurrentTalkType        = TALK_GLOBAL;
    m_LocalDesiredTalkMode   = TALK_NONE;
    m_LocalMutedPlayers      = 0;
    m_LocalDirtyMutedPlayers = TRUE;

    m_Headset.Init( m_HeadsetEnabled );

    m_MaxSpeakers = 1;

    for( s32 i = 0; i < NET_MAX_PLAYERS; i++ )
    {
        m_Speakers[ i ].PlayerNum        = i;
        m_Speakers[ i ].ActualTalkMode   = TALK_NOT_TALKING;
        m_Speakers[ i ].TalkTime         = 0.0f;
        m_Speakers[ i ].MoveToEndOfQueue = FALSE;

        m_SpeakerQueue[ i ] = m_Speakers + i;

        GameMgr.SetSpeaking( i, FALSE );
    }

}

//==============================================================================

void voice_mgr::Kill( void )
{
    ASSERT(m_Initialized);
    if( m_Initialized )
    {
        m_Headset.Kill();
        m_HeadsetEnabled     = FALSE;
        m_LocalVoiceOwner    = -1;
        m_LocalVoiceTalkType = TALK_NOT_TALKING;
        m_Initialized        = FALSE;
    }
}

//==============================================================================

void voice_mgr::Update( f32 DeltaTime )
{
    if( m_HeadsetEnabled )
    {
        m_Headset.Update( DeltaTime );
    }

    if( g_NetworkMgr.IsServer() )
    {
        DoArbitration( DeltaTime );
    }
}

//==============================================================================

s32 voice_mgr::GetLocalVoiceOwner( void )
{
    return m_LocalVoiceOwner;
}

//==============================================================================

void voice_mgr::SetVoiceOwner( s32 Listener, s32 Owner, actual_talk_mode TalkMode )
{
    ASSERT( g_NetworkMgr.IsServer() );

    if( !IN_RANGE( 0, Listener, NET_MAX_PLAYERS - 1 ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( (Owner != -1) && !IN_RANGE( 0, Owner, NET_MAX_PLAYERS - 1 ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( Owner == -1 )
    {
        TalkMode = TALK_NOT_TALKING;
    }
    else if( !IN_RANGE( TALK_MODE_FIRST, TalkMode, TALK_MODE_LAST ) )
    {
        ASSERT( FALSE );
        return;
    }

    s32 ClientIndex = g_NetworkMgr.GetClientIndex( Listener );

    // Is this a client we're trying to make listen?
    if( ClientIndex >= 0 )
    {
        game_server& Server = g_NetworkMgr.GetServerObject();

        if( Server.IsClientConnected( ClientIndex ) )
        {
            voice_proxy& Proxy = Server.GetVoiceProxy( ClientIndex );
            Proxy.SetVoiceOwner( Owner, TalkMode );
        }
    }

    // The local player belongs to the server process.
    else if( ClientIndex == -1 )
    {
        SetLocalVoiceOwner( Owner, TalkMode );
    }
}

//==============================================================================

void voice_mgr::SetLocalVoiceOwner( s32 Owner, actual_talk_mode TalkMode )
{
    if( (Owner != -1) && !IN_RANGE( 0, Owner, NET_MAX_PLAYERS - 1 ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( Owner == -1 )
    {
        TalkMode = TALK_NOT_TALKING;
    }
    else if( !IN_RANGE( TALK_MODE_FIRST, TalkMode, TALK_MODE_LAST ) )
    {
        ASSERT( FALSE );
        return;
    }

    // If there is a change of voice ownership from the current local player.
    if( m_LocalVoiceOwner != Owner )
    {
        if( m_LocalVoiceOwner >= 0 )
        {
            GameMgr.SetSpeaking( m_LocalVoiceOwner, FALSE );
            LOG_MESSAGE( "voice_mgr::SetLocalVoiceOwner", "Released from player %d", m_LocalVoiceOwner );
        }

        if( Owner >= 0 )
        {
            LOG_MESSAGE( "voice_mgr::SetLocalVoiceOwner", "Granted to player %d", Owner );
        }

        m_Headset.ClearWriteFifo();
    }

    m_LocalVoiceOwner    = Owner;
    m_LocalVoiceTalkType = TalkMode;

    if( m_LocalVoiceOwner >= 0 )
    {
        GameMgr.SetSpeaking( m_LocalVoiceOwner, TRUE );
    }
}

//==============================================================================

// This will read a chunk of data from the 'write' fifo. This is because, on a
// client, the write fifo is just used to store voice data pending to go out
// to each client.

s32 voice_mgr::ReadFromVoiceFifo( byte* pBuffer, s32 MaxLength )
{
    return m_Headset.Read( pBuffer, MaxLength );
}

//==============================================================================

void voice_mgr::WriteToVoiceFifo( const byte* pBuffer, s32 Length )
{
    m_Headset.Write( pBuffer, Length );
}

//==============================================================================


s32 voice_mgr::GetBytesInWriteFifo( void )
{
    return( m_Headset.GetNumBytesInWriteFifo() );
}

//==============================================================================

void voice_mgr::SetTalking( xbool bTalking )
{
    desired_talk_mode OldMode = m_LocalDesiredTalkMode;
    if( bTalking && m_bGameIsVoiceEnabled )
    {
        m_LocalDesiredTalkMode = m_CurrentTalkType;
    }
    else
    {
        m_LocalDesiredTalkMode = TALK_NONE;
    }

    if( m_LocalDesiredTalkMode != OldMode )
    {
#if defined(X_DEBUG)
        LOG_MESSAGE( "voice_mgr::SetTalking", "Voice mode change. Old Mode:%s, New Mode:%s",
            GetTalkModeName(OldMode),
            GetTalkModeName(m_LocalDesiredTalkMode) );
#endif

        if( m_LocalDesiredTalkMode == TALK_NONE )
        {
            m_Headset.SetTalking( FALSE );
        }
        else
        {
            m_Headset.SetTalking( TRUE );
        }
    }
}

//==============================================================================

void voice_mgr::SetIsGameVoiceEnabled( xbool Enabled )
{
    m_bGameIsVoiceEnabled = Enabled;
    if( !Enabled && m_Initialized )
    {
        m_LocalDesiredTalkMode = TALK_NONE;
        m_Headset.SetTalking( FALSE );
        m_Headset.ClearReadFifo();
    }
}

//==============================================================================

xbool voice_mgr::IsValidTarget( s32 Speaker, s32 Listener, actual_talk_mode TalkMode )
{
    if( !IN_RANGE( 0, Speaker, NET_MAX_PLAYERS - 1 ) ||
        !IN_RANGE( 0, Listener, NET_MAX_PLAYERS - 1 ) )
    {
        return FALSE;
    }

    s32 SpeakerClientIndex  = g_NetworkMgr.GetClientIndex( Speaker );
    s32 ListenerClientIndex = g_NetworkMgr.GetClientIndex( Listener );
    if( (SpeakerClientIndex < -1) || (ListenerClientIndex < -1) )
    {
        return FALSE;
    }

    actor* pSpeaker  = (actor*)NetObjMgr.GetObjFromSlot( Speaker  );
    actor* pListener = (actor*)NetObjMgr.GetObjFromSlot( Listener );

    if( !pSpeaker || !pListener )
    {
        LOG_WARNING( "voice_mgr::IsValidTarget", "Bad Speaker (%d) or Listener (%d)", Speaker, Listener );
        return FALSE;
    }

    // Use the local mute list for the server process and the proxy list for a remote client.
    u32 MutedPlayers;
    if( ListenerClientIndex == -1 )
    {
        MutedPlayers = m_LocalMutedPlayers;
    }
    else if( ListenerClientIndex >= 0 )
    {
        MutedPlayers = g_NetworkMgr.GetVoiceProxy( ListenerClientIndex ).GetMutedPlayers();
    }
    else
    {
        return FALSE;
    }

    if( MutedPlayers & (u32( 1 ) << Speaker) )
    {
        return FALSE;
    }

    // Check if the speaker is voice banned.
    if( GameMgr.GetScore().Player[ Speaker ].IsVoiceAllowed == FALSE )
        return FALSE;

    // Check if the listener is voice banned.
    if( GameMgr.GetScore().Player[ Listener ].IsVoiceAllowed == FALSE )
        return FALSE;

    switch( TalkMode )
    {
        case TALK_NEW_GLOBAL:
        case TALK_POT_GLOBAL:
        case TALK_OLD_GLOBAL:
        {
            return TRUE;
        }
        break;

        case TALK_NEW_TEAM:
        case TALK_POT_TEAM:
        case TALK_OLD_TEAM:
        {
            return (pSpeaker->net_GetTeamBits() & pListener->net_GetTeamBits()) != 0;
        }
        break;

        case TALK_NEW_LOCAL:
        case TALK_POT_LOCAL:
        case TALK_OLD_LOCAL:
        {
            return ( (pSpeaker->GetPosition() - pListener->GetPosition()).LengthSquared() < 2560000.0f );
        }
        break;

        default:
        {
            return FALSE;
        }
        break;
    }
}

//==============================================================================

xbool voice_mgr::IsSpeaking( s32 Speaker )
{
    ASSERT( IN_RANGE( 0, Speaker, NET_MAX_PLAYERS - 1 ) );
    if( !IN_RANGE( 0, Speaker, NET_MAX_PLAYERS - 1 ) )
    {
        return FALSE;
    }

    switch( m_Speakers[ Speaker ].ActualTalkMode )
    {
        case TALK_NEW_TEAM:
        case TALK_OLD_TEAM:
        case TALK_NEW_LOCAL:
        case TALK_OLD_LOCAL:
        case TALK_NEW_GLOBAL:
        case TALK_OLD_GLOBAL:
        {
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
        break;
    }
}

//==============================================================================

void voice_mgr::AgeSpeaker( s32 Speaker, f32 DeltaTime )
{
    const f32 ProtectedTime = 5.0f;

    ASSERT( IN_RANGE( 0, Speaker, NET_MAX_PLAYERS - 1 ) );
    if( !IN_RANGE( 0, Speaker, NET_MAX_PLAYERS - 1 ) )
    {
        return;
    }

    m_Speakers[ Speaker ].TalkTime += DeltaTime;
    if( m_Speakers[ Speaker ].TalkTime > ProtectedTime )
    {
        switch( m_Speakers[ Speaker ].ActualTalkMode )
        {

            case TALK_NEW_TEAM:
            {
                m_Speakers[ Speaker ].ActualTalkMode = TALK_OLD_TEAM;
            }
            break;

            case TALK_NEW_LOCAL:
            {
                m_Speakers[ Speaker ].ActualTalkMode = TALK_OLD_LOCAL;
            }
            break;

            case TALK_NEW_GLOBAL:
            {
                m_Speakers[ Speaker ].ActualTalkMode = TALK_OLD_GLOBAL;
            }
            break;

            default:
            {
            }
            break;
        }
    }
}

//==============================================================================

desired_talk_mode voice_mgr::GetDesiredTalkMode( s32 PlayerIndex )
{
    s32 ClientIndex = g_NetworkMgr.GetClientIndex( PlayerIndex );

    if( ClientIndex >= 0 )
    {
        game_server& Server = g_NetworkMgr.GetServerObject();

        if( Server.IsClientConnected( ClientIndex ) )
        {
            voice_proxy& Proxy = Server.GetVoiceProxy( ClientIndex );
            return Proxy.GetDesiredTalkMode();
        }
        else
        {
            return TALK_NONE;
        }
    }
    else if( ClientIndex == -1 )
    {
        return GetLocalDesiredTalkMode();
    }

    return TALK_NONE;
}

//==============================================================================

void voice_mgr::ToggleTalkMode( void )
{
    // We don't want them changing their talk type in the middle of talking!
    if( GetLocalDesiredTalkMode() == TALK_NONE )
    {
        if( g_NetworkMgr.IsOnline() )
        {
            const game_score& ScoreData = GameMgr.GetScore();

            switch( m_CurrentTalkType )
            {
                case TALK_GLOBAL:
                {
                    m_CurrentTalkType = TALK_LOCAL;
                }
                break;

                case TALK_LOCAL:
                {
                    m_CurrentTalkType = ScoreData.IsTeamBased ? TALK_TEAM : TALK_GLOBAL;
                }
                break;

                case TALK_TEAM:
                {
                    m_CurrentTalkType = TALK_GLOBAL;
                }
                break;

                default:
                {
                }
                break;
            }
        }
    }
}

//==============================================================================

void voice_mgr::DoArbitration( f32 DeltaTime )
{
    s32 CurrentMax = 1;

    // First, create an array to hold how many people
    // each person is currently listening to.

    s32 NumSpeakers          [ NET_MAX_PLAYERS ];
    u32 WantedListeners      [ NET_MAX_PLAYERS ];
    s32 NumWantedListeners   [ NET_MAX_PLAYERS ];

    u32 PotentialListeners   [ NET_MAX_PLAYERS ];
    s32 NumPotentialListeners[ NET_MAX_PLAYERS ];
    s32 NewOwners            [ NET_MAX_PLAYERS ];
    actual_talk_mode NewTalkModes[ NET_MAX_PLAYERS ];

    // Clear everything out.
    for( s32 i = 0; i < NET_MAX_PLAYERS; i++ )
    {
        NumSpeakers             [ i ] = 0;

        PotentialListeners      [ i ] = 0;
        NumPotentialListeners   [ i ] = 0;

        WantedListeners         [ i ] = 0;
        NumWantedListeners      [ i ] = 0;

        NewOwners               [ i ] = -1;
        NewTalkModes            [ i ] = TALK_NOT_TALKING;

        if( IsSpeaking( i ) )
        {
            AgeSpeaker( i, DeltaTime );
        }
    }

    //
    // For the people who desire to speak, but aren't, set them to potential talkers.
    //
    {
        for( s32 i = 0; i < NET_MAX_PLAYERS; i++ )
        {
            desired_talk_mode DesiredTalkMode = GetDesiredTalkMode( i );
            if( DesiredTalkMode != TALK_NONE )
            {
                // If they're not speaking now, but they desire to be,
                // set them as potential speakers.
                if( m_Speakers[ i ].ActualTalkMode == TALK_NOT_TALKING )
                {
                    switch( DesiredTalkMode )
                    {
                        case TALK_GLOBAL:
                        {
                            m_Speakers[ i ].ActualTalkMode = TALK_POT_GLOBAL;
                        }
                        break;

                        case TALK_TEAM:
                        {
                            m_Speakers[ i ].ActualTalkMode = TALK_POT_TEAM;
                        }
                        break;

                        case TALK_LOCAL:
                        {
                            m_Speakers[ i ].ActualTalkMode = TALK_POT_LOCAL;
                        }
                        break;

                        default:
                        {
                        }
                        break;
                    }
                }
            }

            // If they don't desire to speak,
            // we certainly aren't going to make them.
            else
            {
                if( IsSpeaking( i ) )
                {
                    // They willingly gave up their speaking rights,
                    // but they're still going to the end of the queue.
                    m_Speakers[ i ].MoveToEndOfQueue = TRUE;
                }

                m_Speakers[ i ].TalkTime = 0.0f;
                m_Speakers[ i ].ActualTalkMode = TALK_NOT_TALKING;
            }
        }
    }

    //
    // Go through and set up the speaker bitfields with
    // the desired recipients.
    //
    for( s32 i = 0; i < NET_MAX_PLAYERS; i++ )
    {
        if( m_Speakers[ i ].ActualTalkMode != TALK_NOT_TALKING )
        {
            for( s32 j = 0; j < NET_MAX_PLAYERS; j++ )
            {
                if( IsValidTarget( i, j, m_Speakers[ i ].ActualTalkMode ) )
                {
                    // Turn on the bit, this speaker wants to talk to this player.
                    WantedListeners[ i ] |= (u32( 1 ) << j);
                    NumWantedListeners[ i ]++;
                }
            }
        }
    }

    //
    // We know who wants to talk to whom,
    // now figure out who can talk to whom.
    // Priority is determined by talk mode, then priority queue.
    //
    for( s32 TalkMode = TALK_MODE_FIRST; TalkMode <= TALK_MODE_LAST; TalkMode++ )
    {
        // This is defined so we don't cover the same entry twice
        // when the queue is rearranged.
        s32 Top = NET_MAX_PLAYERS;

        // Tie breaks are by priority queue.
        for( s32 iSpeaker = 0; iSpeaker < Top; iSpeaker++ )
        {
            speaker& Speaker = *m_SpeakerQueue[ iSpeaker ];
            if( (Speaker.ActualTalkMode == TalkMode) )
            {
                // Go through the listener bitfield here and look for listeners
                // who aren't listening to anyone else, and that we also decided
                // we wanted to talk to above.
                for( s32 ListenerNum = 0; ListenerNum < NET_MAX_PLAYERS; ListenerNum++ )
                {
                    if( (NumSpeakers[ ListenerNum ] < m_MaxSpeakers) &&
                        (WantedListeners[ Speaker.PlayerNum ] & (u32( 1 ) << ListenerNum)) )
                    {
                        if( NumSpeakers[ ListenerNum ] < CurrentMax )
                        {
                            NumPotentialListeners[ Speaker.PlayerNum ]++;
                            PotentialListeners[ Speaker.PlayerNum ] |= (u32( 1 ) << ListenerNum);
                        }
                    }
                }

                // Now check to see that we can talk to at least half the
                // people we wanted to originally.  Also, you need to be
                // able to talk to yourself, but not just yourself, cause
                // that's a sure sign of madness.
                if( ((NumPotentialListeners[ Speaker.PlayerNum ] * 2) >= NumWantedListeners[ Speaker.PlayerNum ]) &&
                    (NumPotentialListeners[ Speaker.PlayerNum ] > 1) &&
                    (PotentialListeners[ Speaker.PlayerNum ] & (u32( 1 ) << Speaker.PlayerNum)) )
                {
                    // If this is his first frame talking, upgrade him from a potential talker.
                    switch( Speaker.ActualTalkMode )
                    {
                        case TALK_POT_GLOBAL:
                        {
                            Speaker.ActualTalkMode = TALK_NEW_GLOBAL;
                        }
                        break;

                        case TALK_POT_LOCAL:
                        {
                            Speaker.ActualTalkMode = TALK_NEW_LOCAL;
                        }
                        break;

                        case TALK_POT_TEAM:
                        {
                            Speaker.ActualTalkMode = TALK_NEW_TEAM;
                        }
                        break;

                        default:
                        {
                        }
                        break;
                    }

                    for( s32 ListenerNum = 0; ListenerNum < NET_MAX_PLAYERS; ListenerNum++ )
                    {
                        if( (PotentialListeners[ Speaker.PlayerNum ] & (u32( 1 ) << ListenerNum)) )
                        {
                            NumSpeakers[ ListenerNum ]++;
                            NewOwners[ ListenerNum ] = Speaker.PlayerNum;
                            NewTalkModes[ ListenerNum ] = m_Speakers[ Speaker.PlayerNum ].ActualTalkMode;
                        }
                    }
                }

                // Guess you don't get to speak this time, better luck next frame.
                else
                {
                    // If he still desires to speak, it will get sorted out at the beginning
                    // of the next DoArbitration, but he'll have to start from scratch.

                    if( IsSpeaking( Speaker.PlayerNum ) )
                    {
                        // Move them to the end of the queue.
                        Speaker.MoveToEndOfQueue = TRUE;
                    }

                    Speaker.TalkTime = 0.0f;
                    Speaker.ActualTalkMode = TALK_NOT_TALKING;
                }
            }
        }
    }

    // Apply only ownership changes. Clearing and reassigning every frame would
    // flush queued voice data even while the same speaker remains selected.
    for( s32 ListenerNum = 0; ListenerNum < NET_MAX_PLAYERS; ListenerNum++ )
    {
        SetVoiceOwner( ListenerNum, NewOwners[ ListenerNum ], NewTalkModes[ ListenerNum ] );
    }

    // Move people who finished speaking
    // this frame to the end of the priority queue.
    s32 NumMoves = 0;
    for( s32 i = 0; i + NumMoves < NET_MAX_PLAYERS; )
    {
        if( m_SpeakerQueue[ i ]->MoveToEndOfQueue )
        {
            // Put this entry at the end and shift everything else forward one.
            speaker* pTempSpeaker = m_SpeakerQueue[ i ];

            for( s32 j = i; j < (NET_MAX_PLAYERS - 1); j++ )
            {
                m_SpeakerQueue[ j ] = m_SpeakerQueue[ j + 1 ];
            }

            pTempSpeaker->MoveToEndOfQueue = FALSE;
            m_SpeakerQueue[ NET_MAX_PLAYERS - 1 ] = pTempSpeaker;
            NumMoves++;
        }
        else
        {
            i++;
        }
    }
}

//==============================================================================

void voice_mgr::Distribute( s32 TalkerIndex, const byte* pBuffer, s32 Length )
{
    s32     PlayerIndex;
    s32     ClientIndex;
    s32     EncodeBlockSize;

    ASSERT( g_NetworkMgr.IsServer() );

    EncodeBlockSize = GetEncodeBlockSize();

    if( !IN_RANGE( 0, TalkerIndex, NET_MAX_PLAYERS - 1 ) ||
        (pBuffer == NULL) ||
        (Length <= 0) ||
        (EncodeBlockSize <= 0) ||
        ((Length % EncodeBlockSize) != 0) )
    {
        return;
    }

    for( PlayerIndex=0; PlayerIndex < NET_MAX_PLAYERS; PlayerIndex++ )
    {
        ClientIndex = g_NetworkMgr.GetClientIndex( PlayerIndex );
        if( ClientIndex >= 0 )
        {
            voice_proxy& Proxy = g_NetworkMgr.GetVoiceProxy( ClientIndex );

            if( (Proxy.GetVoiceOwner() == TalkerIndex) && (TalkerIndex != PlayerIndex) )
            {
                LOG_MESSAGE( "voice_mgr::Distribute","Voice data sent. Player:%d, Client:%d, Length:%d", TalkerIndex, ClientIndex, Length );
                Proxy.Write( pBuffer, Length );
            }
        }
    }
    // If the 'local' voice channel is owned by this player, then send the data to the local headset.
    if( (GetLocalVoiceOwner() == TalkerIndex) && (g_NetworkMgr.GetLocalPlayerSlot(0) != TalkerIndex) )
    {
        LOG_MESSAGE( "voice_mgr::Distribute","Voice data queued locally. Player:%d, Length:%d", TalkerIndex, Length );
        WriteToVoiceFifo( pBuffer, Length );
    }
}

//==============================================================================

// The format of the data sent out here should mirror the voice_proxy::AcceptUpdate
// function. This can be used by any client to send to the server.

void voice_mgr::ProvideUpdate( netstream& BitStream )
{
    ASSERT( g_NetworkMgr.IsClient() );

    // Update the voice peripheral status
    BitStream.WriteFlag( !IsVoiceBanned()  );
    BitStream.WriteFlag(  IsVoiceCapable() );

    if( BitStream.WriteFlag( m_LocalDirtyMutedPlayers ) )
    {
        BitStream.WriteU32( m_LocalMutedPlayers );
        m_LocalDirtyMutedPlayers = FALSE;
    }

    // If we are the owner, or we want to be the owner, then we send an update
    // otherwise, we send nothing (except a flag saying there is no data).
    if( m_LocalDesiredTalkMode != TALK_NONE )
    {
        if( GetLocalVoiceOwner() == g_NetworkMgr.GetLocalPlayerSlot( 0 ) )
        {
#if defined(X_DEBUG)
            LOG_MESSAGE( "voice_mgr::ProvideUpdate", "Requesting voice ownership. Talk Mode:%s", GetTalkModeName(m_LocalDesiredTalkMode) );
#endif
            // Write a flag to say data is present.
            BitStream.WriteRangedS32( m_LocalDesiredTalkMode, TALK_TEAM, TALK_NONE );
            m_Headset.ProvideUpdate( BitStream, VOICE_MAX_UPDATE_BYTES );
        }
        else
        {
            // Keep the ownership request active while another player owns the channel.
            BitStream.WriteRangedS32( m_LocalDesiredTalkMode, TALK_TEAM, TALK_NONE );
            m_Headset.ProvideUpdate( BitStream, 0 );
        }
    }
    else
    {
        //
        // No one from this machine is interested in speaking!
        //
        BitStream.WriteRangedS32( TALK_NONE, TALK_TEAM, TALK_NONE );
    }
}

//==============================================================================

// Data came in from the server. This tells us who actually has control of the
// headset. It *may* be us! The format of this data should mirror
// voice_proxy::ProvideUpdate()

void voice_mgr::AcceptUpdate( netstream& BitStream )
{
    xbool   DataPresent;
    s32     Owner;
    s32     TalkMode;

    ASSERT( g_NetworkMgr.IsClient() );

    DataPresent = BitStream.ReadFlag();
    if( BitStream.ReadError() )
        return;
    //
    // We have the data coming in from a bitstream. This is only called on a client
    // and means that the server has sent some data to it. This is responsible for
    // distributing this data to the headset. Since this is the client, they should
    // play anything that's sent down this channel since the server just won't send
    // any useful data to the client that is talking.
    //
    if( !DataPresent )
    {
        //
        // Clear out the voice owner.
        //
        if( GetLocalVoiceOwner() != -1 )
        {
            LOG_MESSAGE( "voice_mgr::AcceptUpdate", "Released voice. Old speaker:%d", GetLocalVoiceOwner() );
        }
        SetLocalVoiceOwner( -1, TALK_NOT_TALKING );
        return;
    }


    BitStream.ReadRangedS32( Owner, 0, NET_MAX_PLAYERS - 1 );
    BitStream.ReadRangedS32( TalkMode, TALK_MODE_FIRST, TALK_MODE_LAST );

    if( BitStream.ReadError() )
        return;

    fifo::update Update;
    if( !m_Headset.ReadUpdate( BitStream, Update ) )
        return;

    if( (GetLocalVoiceOwner() != Owner) ||
        (m_LocalVoiceTalkType != static_cast<actual_talk_mode>( TalkMode )) )
    {
        LOG_MESSAGE( "voice_mgr::AcceptUpdate", "New voice owner, old speaker:%d, new speaker:%d", GetLocalVoiceOwner(), Owner );
        SetLocalVoiceOwner( Owner, static_cast<actual_talk_mode>( TalkMode ) );
    }

    m_Headset.AcceptUpdate( Update );
}

//==============================================================================

headset& voice_mgr::GetHeadset( void )
{
    return( m_Headset );
}

//==============================================================================

xbool voice_mgr::IsVoiceCapable( void )
{
    xbool   IsCapable = IsHeadsetPresent() & IsVoiceEnabled();
    return( IsCapable );
}

//==============================================================================

#if defined(X_DEBUG)
const char* GetTalkModeName( s32 Mode )
{
    switch(Mode)
    {
    case TALK_NONE:             return "TALK_NONE";
    case TALK_GLOBAL:           return "TALK_GLOBAL";
    case TALK_LOCAL:            return "TALK_LOCAL";
    case TALK_TEAM:             return "TALK_TEAM";
    default:                    ASSERT(FALSE);
    }
    return "<unknown>";
}
#endif

//==============================================================================

#if defined(X_DEBUG) && (defined(bwatson) || defined(jpcossigny) || defined(Biscuit))
void VoiceTestCode( void )
{
}
#endif
