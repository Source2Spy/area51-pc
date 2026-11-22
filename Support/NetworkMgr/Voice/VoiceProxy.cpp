//==============================================================================
//
//  VoiceProxy.cpp
//
// This contains a local instance, on a server, of the data that is needed to
// be sent to that client. The server will issue data to this fifo, this will then
// be output on the next available update cycle.
//
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "VoiceProxy.hpp"
#include "Network/NetStream.hpp"

#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/NetLimits.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

voice_proxy::voice_proxy( void )
{
    m_Initialized = FALSE;
}

//==============================================================================

voice_proxy::~voice_proxy( void )
{
    ASSERT(!m_Initialized);
}

//==============================================================================

void voice_proxy::Init( s32 PlayerNetSlot )
{
    ASSERT(!m_Initialized);
    m_Initialized           = TRUE;
    m_PlayerNetSlot         = PlayerNetSlot;
    m_DesiredTalkMode       = TALK_NONE;
    m_VoiceOwner            = -1;
    m_OutgoingOwner         = -1;
    m_VoiceTalkMode         = TALK_NOT_TALKING;
    m_MutedPlayers          = 0;
    m_Outgoing.Init( m_OutgoingBuffer, sizeof( m_OutgoingBuffer ) );
}

//==============================================================================

void voice_proxy::Kill( void )
{
    ASSERT(m_Initialized);
    m_Initialized = FALSE;
    m_Outgoing.Kill();
}

//==============================================================================

// This bitstream format should mirror voice_mgr::AcceptUpdate(). This goes from
// the server to the client. The client needs to know if someone is talking, and
// if so, whom.

void voice_proxy::ProvideUpdate( netstream& BitStream )
{
    ASSERT( g_NetworkMgr.IsServer() );

    voice_playback_update Update;
    Update.Delivery = VOICE_STOP;
    if( m_VoiceOwner >= 0 )
    {
        Update.Delivery = VOICE_ACTIVE;
    }
    else if( m_OutgoingOwner >= 0 )
    {
        Update.Delivery = m_Outgoing.GetBytesUsed() ? VOICE_TAIL : VOICE_IDLE;
    }

    Update.Owner = m_OutgoingOwner;
    Update.Mode  = m_VoiceTalkMode;
    Update.Write( BitStream, m_Outgoing );
}

//==============================================================================

// Data has been received from the client. This function will strip out any
// voice data that may be in the bitstream. It will also attempt to acquire
// control of a voice channel.

// The server can determine who wants to have charge of the headset from the client
// ID that it comes from. This bitstream format should mirror voice_mgr::ProvideUpdate()

void voice_proxy::AcceptUpdate( netstream& BitStream )
{
    s32                 DesiredTalkModeValue;
    voice_update        Update;

    ASSERT( g_NetworkMgr.IsServer() );

    xbool IsVoiceAllowed;
    xbool IsVoiceCapable;
    u32 MutedPlayers = m_MutedPlayers;
    BitStream.ReadFlag( IsVoiceAllowed );
    BitStream.ReadFlag( IsVoiceCapable );
    if( BitStream.ReadFlag() )
        BitStream.ReadU32( MutedPlayers );
    BitStream.ReadRangedS32( DesiredTalkModeValue, TALK_TEAM, TALK_NONE );
    if( !Update.Read( BitStream ) )
        return;

    GameMgr.SetVoiceAllowed( m_PlayerNetSlot, IsVoiceAllowed );
    GameMgr.SetVoiceCapable( m_PlayerNetSlot, IsVoiceCapable );
    m_MutedPlayers    = MutedPlayers;
    m_DesiredTalkMode = static_cast<desired_talk_mode>( DesiredTalkModeValue );

    // The final packet can carry audio even though it releases the channel.
    if( Update.Length && IsVoiceAllowed )
    {
        g_VoiceMgr.Distribute( m_PlayerNetSlot, Update.Data, Update.Length );
    }
}

//==============================================================================

// This will write a block of data to the outgoing fifo. This will be consumed by
// ProvideUpdate when called from a client instance.

void voice_proxy::Write( const byte* pBuffer, s32 Length )
{
    if( (pBuffer == NULL) ||
        (Length <= 0) ||
        (Length > voice_update::MAX_BYTES) ||
        ((Length % voice_codec::FRAME_BYTES) != 0) )
    {
        return;
    }

    m_Outgoing.Insert( pBuffer, Length, voice_codec::FRAME_BYTES );
}

//==============================================================================

void voice_proxy::SetPlayerSlot( s32 PlayerSlot )
{
    m_PlayerNetSlot = PlayerSlot;
}

//==============================================================================

s32 voice_proxy::GetPlayerSlot( void )
{
    return m_PlayerNetSlot;
}

//==============================================================================

void voice_proxy::SetVoiceOwner( s32 PlayerSlot, actual_talk_mode TalkMode, xbool Drain )
{
    if( (PlayerSlot != -1) && !IN_RANGE( 0, PlayerSlot, NET_MAX_PLAYERS - 1 ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( (PlayerSlot >= 0) && !IN_RANGE( TALK_MODE_FIRST, TalkMode, TALK_MODE_LAST ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( PlayerSlot == -1 )
    {
        if( !Drain )
        {
            m_Outgoing.Clear();
            m_OutgoingOwner = -1;
            m_VoiceTalkMode = TALK_NOT_TALKING;
        }
    }
    else
    {
        if( m_VoiceOwner != PlayerSlot )
        {
            m_Outgoing.Clear();
        }
        m_OutgoingOwner = PlayerSlot;
        m_VoiceTalkMode = TalkMode;
    }

    m_VoiceOwner = PlayerSlot;
}

//==============================================================================

s32 voice_proxy::GetVoiceOwner( void )
{
    return m_VoiceOwner;
}
