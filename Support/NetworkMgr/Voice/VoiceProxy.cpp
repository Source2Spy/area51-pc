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
    m_VoiceTalkMode         = TALK_NOT_TALKING;
    m_MutedPlayers          = 0;
    m_Outgoing.Init( m_OutgoingBuffer, 256 );
    m_Incoming.Init( m_IncomingBuffer, 256 );
}

//==============================================================================

void voice_proxy::Kill( void )
{
    ASSERT(m_Initialized);
    m_Initialized = FALSE;
    m_Outgoing.Kill();
    m_Incoming.Kill();
}

//==============================================================================

// This bitstream format should mirror voice_mgr::AcceptUpdate(). This goes from
// the server to the client. The client needs to know if someone is talking, and
// if so, whom.

void voice_proxy::ProvideUpdate( netstream& BitStream )
{
    xbool DataPresent;
    s32   EncodeBlockSize;

    ASSERT( g_NetworkMgr.IsServer() );

    EncodeBlockSize = g_VoiceMgr.GetEncodeBlockSize();
    DataPresent = (m_VoiceOwner >= 0) && (EncodeBlockSize > 0);
    // If there is some data waiting to go from the server to the client, then
    // send it. Each client has an independent voice owner.
    BitStream.WriteFlag( DataPresent );
    if( DataPresent )
    {
#if defined(X_DEBUG)
        s32 OriginalLength;
        s32 ClientIndex;
        OriginalLength = m_Outgoing.GetBytesUsed();
        ClientIndex = g_NetworkMgr.GetClientIndex( m_PlayerNetSlot );

#endif
        BitStream.WriteRangedS32( m_VoiceOwner, 0, NET_MAX_PLAYERS - 1 );
        BitStream.WriteRangedS32( m_VoiceTalkMode, TALK_MODE_FIRST, TALK_MODE_LAST );

        // If this client's player is the voice owner, we really shouldn't pull any data
        // from it's outgoing buffer. However, in most cases, this should be empty anyway
        // so sending this data will do no harm.
        m_Outgoing.ProvideUpdate( BitStream, VOICE_MAX_UPDATE_BYTES, EncodeBlockSize );
#if defined(X_DEBUG)
        LOG_MESSAGE( "voice_proxy::ProvideUpdate", "Sending voice data to client:%d, Owner:%d, Length:%d", ClientIndex, m_VoiceOwner, OriginalLength - m_Outgoing.GetBytesUsed() );
#endif
    }
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
    desired_talk_mode   DesiredTalkMode;
    byte                Buffer[512];
    s32                 Length;
    s32                 EncodeBlockSize;
    fifo::update        Update;

    ASSERT( g_NetworkMgr.IsServer() );

    xbool IsVoiceAllowed;
    xbool IsVoiceCapable;
    u32 MutedPlayers = m_MutedPlayers;
    BitStream.ReadFlag( IsVoiceAllowed );
    BitStream.ReadFlag( IsVoiceCapable );
    if( BitStream.ReadFlag() )
        BitStream.ReadU32( MutedPlayers );
    BitStream.ReadRangedS32( DesiredTalkModeValue, TALK_TEAM, TALK_NONE );
    if( BitStream.ReadError() )
        return;

    DesiredTalkMode = static_cast<desired_talk_mode>( DesiredTalkModeValue );
    EncodeBlockSize = g_VoiceMgr.GetEncodeBlockSize();

    if( DesiredTalkMode != TALK_NONE )
    {
        if( EncodeBlockSize <= 0 )
        {
            BitStream.SetReadError();
            return;
        }
        if( !m_Incoming.ReadUpdate( BitStream, Update, EncodeBlockSize ) )
            return;
    }

    GameMgr.SetVoiceAllowed( m_PlayerNetSlot, IsVoiceAllowed );
    GameMgr.SetVoiceCapable( m_PlayerNetSlot, IsVoiceCapable );
    m_MutedPlayers    = MutedPlayers;
    m_DesiredTalkMode = DesiredTalkMode;

    if( m_DesiredTalkMode != TALK_NONE )
    {
        m_Incoming.AcceptUpdate( Update, EncodeBlockSize );

        Length = Read( Buffer, sizeof(Buffer) );
        if( Length > 0 )
        {
            g_VoiceMgr.Distribute( m_PlayerNetSlot, Buffer, Length );
        }
    }
    else
    {
        m_Incoming.Clear();
    }
}

//==============================================================================

// This will read a chunk of data from the incoming data fifo. This fifo will be
// filled by AcceptUpdate above when called on a client instance.

s32 voice_proxy::Read( byte* pBuffer, s32 MaxLength )
{
    s32 EncodeBlockSize;
    s32 Length;

    if( (pBuffer == NULL) || (MaxLength <= 0) )
    {
        return 0;
    }

    EncodeBlockSize = g_VoiceMgr.GetEncodeBlockSize();
    if( EncodeBlockSize <= 0 )
    {
        return 0;
    }

    Length = MIN( MaxLength, m_Incoming.GetBytesUsed() );
    Length -= Length % EncodeBlockSize;
    if( Length > 0 )
    {
        m_Incoming.Remove( pBuffer, Length, EncodeBlockSize );
    }

    return Length;
}

//==============================================================================

// This will write a block of data to the outgoing fifo. This will be consumed by
// ProvideUpdate when called from a client instance.

void voice_proxy::Write( const byte* pBuffer, s32 Length )
{
    s32 EncodeBlockSize;

    if( (pBuffer == NULL) ||
        (Length <= 0) )
    {
        return;
    }

    EncodeBlockSize = g_VoiceMgr.GetEncodeBlockSize();
    if( (EncodeBlockSize <= 0) || ((Length % EncodeBlockSize) != 0) )
    {
        return;
    }

    m_Outgoing.Insert( pBuffer, Length, EncodeBlockSize );
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

void voice_proxy::SetVoiceOwner( s32 PlayerSlot, actual_talk_mode TalkMode )
{
    if( (PlayerSlot != -1) && !IN_RANGE( 0, PlayerSlot, NET_MAX_PLAYERS - 1 ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( PlayerSlot == -1 )
    {
        TalkMode = TALK_NOT_TALKING;
    }
    else if( !IN_RANGE( TALK_MODE_FIRST, TalkMode, TALK_MODE_LAST ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( m_VoiceOwner != PlayerSlot )
    {
        m_Outgoing.Clear();
    }

    m_VoiceOwner      = PlayerSlot;
    m_VoiceTalkMode   = TalkMode;
}

//==============================================================================

s32 voice_proxy::GetVoiceOwner( void )
{
    return m_VoiceOwner;
}
