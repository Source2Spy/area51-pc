//==============================================================================
//
//  VoicePacket.hpp
//
//==============================================================================

#ifndef VOICE_PACKET_HPP
#define VOICE_PACKET_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "VoiceCodec.hpp"

//==============================================================================
//  TYPES
//==============================================================================

class fifo;
class netstream;

enum desired_talk_mode
{
    TALK_TEAM,
    TALK_GLOBAL,
    TALK_LOCAL,
    TALK_NONE,
};

//------------------------------------------------------------------------------

enum actual_talk_mode
{
    TALK_NEW_TEAM,
    TALK_POT_TEAM,
    TALK_OLD_TEAM,

    TALK_NEW_LOCAL,
    TALK_POT_LOCAL,
    TALK_OLD_LOCAL,

    TALK_NEW_GLOBAL,
    TALK_POT_GLOBAL,
    TALK_OLD_GLOBAL,

    TALK_NOT_TALKING,
    TALK_MODE_FIRST = TALK_NEW_TEAM,
    TALK_MODE_LAST  = TALK_OLD_GLOBAL,
};

//------------------------------------------------------------------------------

enum voice_delivery
{
    VOICE_STOP,     // Discard any pending audio.
    VOICE_IDLE,     // No owner; let previously authorized audio play out.
    VOICE_TAIL,     // The last frames from an owner who released the channel.
    VOICE_ACTIVE,
};

//------------------------------------------------------------------------------

struct voice_update
{
    // 280 ms at 20 ms/frame, including headroom over the 250 ms ship interval.
    static const s32 MAX_FRAMES = 14;
    static const s32 MAX_BYTES  = MAX_FRAMES * voice_codec::FRAME_BYTES;
    static const s32 FIFO_BYTES = 2 * MAX_BYTES;

    void Write( netstream& BitStream, fifo& Frames, xbool SendAudio );
    xbool Read( netstream& BitStream );

    byte Data[MAX_BYTES];
    s32  Length;
};

//------------------------------------------------------------------------------

struct voice_playback_update
{
    void Write( netstream& BitStream, fifo& Frames );
    xbool Read( netstream& BitStream );

    voice_delivery   Delivery;
    s32              Owner;
    actual_talk_mode Mode;
    voice_update     Audio;
};

//==============================================================================
#endif // VOICE_PACKET_HPP
//==============================================================================
