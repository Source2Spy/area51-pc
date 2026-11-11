//==============================================================================
//
//  audio_command.hpp
//
//==============================================================================

#ifndef AUDIO_COMMAND_HPP
#define AUDIO_COMMAND_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"
#include "e_Audio.hpp"
#include "Audio/audio_package.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum audio_command_type
{
    AUDIO_CMD_NONE,
    AUDIO_CMD_GET_DEBUG_STATS,
    AUDIO_CMD_UPDATE,
    AUDIO_CMD_PERIODIC_UPDATE,
    AUDIO_CMD_STREAM_READ_COMPLETE,
    AUDIO_CMD_STREAM_WARM_COMPLETE,
    AUDIO_CMD_PLAY,
    AUDIO_CMD_LOAD_PACKAGE,
    AUDIO_CMD_UNLOAD_PACKAGE,
    AUDIO_CMD_UNLOAD_ALL_PACKAGES,
    AUDIO_CMD_START,
    AUDIO_CMD_SEGUE,
    AUDIO_CMD_SET_RELEASE_TIME,
    AUDIO_CMD_IS_RELEASING,
    AUDIO_CMD_IS_VALID_VOICE_ID,
    AUDIO_CMD_GET_VOICE_STATE,
    AUDIO_CMD_IS_VOICE_READY,
    AUDIO_CMD_GET_VOICE_DESCRIPTOR,
    AUDIO_CMD_GET_LENGTH_SECONDS_VOICE,
    AUDIO_CMD_GET_VOLUME,
    AUDIO_CMD_GET_PAN,
    AUDIO_CMD_GET_PITCH,
    AUDIO_CMD_GET_POSITION,
    AUDIO_CMD_GET_EFFECT_SEND,
    AUDIO_CMD_HAS_LIP_SYNC,
    AUDIO_CMD_GET_LIP_SYNC,
    AUDIO_CMD_GET_BREAK_POINTS,
    AUDIO_CMD_GET_IS_READY,
    AUDIO_CMD_GET_CURRENT_PLAY_TIME,
    AUDIO_CMD_GET_PRIORITY,
    AUDIO_CMD_PAUSE,
    AUDIO_CMD_PAUSE_ALL,
    AUDIO_CMD_RESUME,
    AUDIO_CMD_RESUME_ALL,
    AUDIO_CMD_RELEASE,
    AUDIO_CMD_RELEASE_ALL,
    AUDIO_CMD_SET_VOLUME,
    AUDIO_CMD_SET_PAN,
    AUDIO_CMD_SET_PITCH,
    AUDIO_CMD_SET_POSITION,
    AUDIO_CMD_SET_FALLOFF,
    AUDIO_CMD_SET_EFFECT_SEND,
    AUDIO_CMD_SET_MASTER_VOLUME,
    AUDIO_CMD_SET_MUSIC_VOLUME,
    AUDIO_CMD_SET_SFX_VOLUME,
    AUDIO_CMD_SET_VOICE_VOLUME,
    AUDIO_CMD_SET_PITCH_FACTOR,
    AUDIO_CMD_ENABLE_DUCKING,
    AUDIO_CMD_DISABLE_DUCKING,
    AUDIO_CMD_SET_EAR,
    AUDIO_CMD_CREATE_EAR,
    AUDIO_CMD_GET_FIRST_EAR,
    AUDIO_CMD_GET_NEXT_EAR,
    AUDIO_CMD_DESTROY_EAR,
    AUDIO_CMD_RESET_CURRENT_EAR,
    AUDIO_CMD_SET_SPEAKER_CONFIG,
    AUDIO_CMD_SET_VOICE_EAR,
    AUDIO_CMD_SET_CLIP,
    AUDIO_CMD_UPDATE_EAR_ZONE_VOLUME,
    AUDIO_CMD_UPDATE_EAR_ZONE_VOLUMES,
    AUDIO_CMD_REMERGE_IDENTIFIER_TABLES,
    AUDIO_CMD_RESIZE_MEMORY,
    AUDIO_CMD_RESERVE_STREAMS,
    AUDIO_CMD_UNRESERVE_STREAMS,
    AUDIO_CMD_SET_PITCH_LOCK,
};

//------------------------------------------------------------------------------

struct audio_command_result
{
    audio_debug_stats DebugStats;
    audio_voice_state VoiceState;
    xbool             Bool;
    voice_id          VoiceID;
    s32               S32;
    const char*       pText;
    f32*              pF32;
    f32               F32;
};

//------------------------------------------------------------------------------

struct audio_command
{
    void                    Init        ( audio_command_type Type );

    audio_command_type      Type;
    xmesgq*                 pCompletion;
    audio_command_result*   pResult;

    audio_request* pRequest;
    io_request*    pIoRequest;

    union
    {
        char Text[AUDIO_PACKAGE_FILENAME_LENGTH];
        f32  ZoneVolumes[MAX_ZONES];
    };
    voice_id    VoiceID;
    voice_id    VoiceID2;
    ear_id      EarID;
    matrix4     Matrix;
    vector3     Position;
    vector3*    pPosition;
    f32*        pF32;
    s32*        pS32;
    s32         S32A;
    s32         S32B;
    s32         S32C;
    s32         S32D;
    s32         S32E;
    f32         F32A;
    f32         F32B;
    f32         F32C;
    f32         F32D;
    xbool       BoolA;
    xbool       BoolB;
};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline 
void audio_command::Init( audio_command_type CommandType )
{
    x_memset( this, 0, sizeof(*this) );
    Type = CommandType;
}


//==============================================================================
#endif // AUDIO_COMMAND_HPP
//==============================================================================
