//==============================================================================
//
//  VoicePacket.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "VoicePacket.hpp"
#include "NetworkMgr/NetLimits.hpp"
#include "Network/FiFo.hpp"
#include "Network/NetStream.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void voice_update::Write( netstream& BitStream, fifo& Frames, xbool SendAudio )
{
    s32 const Count = SendAudio ? MIN( Frames.GetBytesUsed() / voice_codec::FRAME_BYTES, MAX_FRAMES ) : 0;
    Length = Count * voice_codec::FRAME_BYTES;
    BitStream.WriteRangedS32( Count, 0, MAX_FRAMES );
    if( Count )
    {
        Frames.Remove( Data, Length, voice_codec::FRAME_BYTES );
        BitStream.WriteBits( Data, Length * 8 );
    }
}

//==============================================================================

xbool voice_update::Read( netstream& BitStream )
{
    s32 Count;
    BitStream.ReadRangedS32( Count, 0, MAX_FRAMES );
    if( BitStream.ReadError() )
    {
        return FALSE;
    }

    Length = Count * voice_codec::FRAME_BYTES;
    BitStream.ReadBits( Data, Length * 8 );
    return !BitStream.ReadError();
}

//==============================================================================

void voice_playback_update::Write( netstream& BitStream, fifo& Frames )
{
    BitStream.WriteRangedS32( Delivery, VOICE_STOP, VOICE_ACTIVE );
    if( (Delivery == VOICE_ACTIVE) || (Delivery == VOICE_TAIL) )
    {
        BitStream.WriteRangedS32( Owner, 0, NET_MAX_PLAYERS - 1 );
        BitStream.WriteRangedS32( Mode, TALK_MODE_FIRST, TALK_MODE_LAST );
        Audio.Write( BitStream, Frames, TRUE );
    }
}

//==============================================================================

xbool voice_playback_update::Read( netstream& BitStream )
{
    s32 Value;
    BitStream.ReadRangedS32( Value, VOICE_STOP, VOICE_ACTIVE );
    if( BitStream.ReadError() )
        return FALSE;

    Delivery = static_cast<voice_delivery>( Value );
    if( (Delivery == VOICE_ACTIVE) || (Delivery == VOICE_TAIL) )
    {
        BitStream.ReadRangedS32( Owner, 0, NET_MAX_PLAYERS - 1 );
        BitStream.ReadRangedS32( Value, TALK_MODE_FIRST, TALK_MODE_LAST );
        Mode = static_cast<actual_talk_mode>( Value );
        return Audio.Read( BitStream );
    }

    Owner = -1;
    Mode  = TALK_NOT_TALKING;
    Audio.Length = 0;
    return TRUE;
}
