#ifndef MATCH_SERVER_HPP
#define MATCH_SERVER_HPP

//==============================================================================

#include "e_Network.hpp"
#include "NetworkMgr/Game/GameTypes.hpp"
#include "NetworkMgr/NetLimits.hpp"

const s32 MAX_MATCH_RESULTS = 50;

enum match_source
{
    MATCH_SOURCE_LAN      = ( 1 << 0 ),
    MATCH_SOURCE_INTERNET = ( 1 << 1 ),
    MATCH_SOURCE_ALL      = MATCH_SOURCE_LAN | MATCH_SOURCE_INTERNET,
};

enum server_flags
{
    SERVER_HAS_PASSWORD        = ( 1 << 0 ),  // 0x001
    SERVER_VOICE_ENABLED       = ( 1 << 1 ),  // 0x002
    SERVER_IS_DEDICATED        = ( 1 << 2 ),  // 0x004
    SERVER_IS_A_FAVORITE       = ( 1 << 4 ),  // 0x010
    SERVER_MUTATION_ENABLED    = ( 1 << 5 ),  // 0x020
    SERVER_ENABLE_TEAM_BALANCE = ( 1 << 6 ),  // 0x040
    SERVER_FRIENDLY_FIRE       = ( 1 << 7 ),  // 0x080
    SERVER_ELIMINATION_MODE    = ( 1 << 8 ),  // 0x100
    SERVER_ENABLE_MAP_SCALING  = ( 1 << 9 ),  // 0x200
    SERVER_IS_PRIVATE          = ( 1 << 10 ), // 0x400
    SERVER_LAN_ONLY            = ( 1 << 11 ), // 0x800
    SERVER_ALLOW_CHEATS        = ( 1 << 12 ), // 0x1000
};

enum connect_type
{
    CONNECT_DIRECT,
    CONNECT_NAT,
    CONNECT_INDIRECT,
};

struct server_info
{
    s32           ID;
    u32           Sources;
    s32           Retries;
    f32           RetryTimeout;
    net_address   Remote;
    net_address   External;
    s32           Version;
    s32           MaxPlayers;
    s32           Players;
    s32           Level;
    f32           PingDelay;
    xwchar        Name[NET_SERVER_NAME_LENGTH];
    char          Password[NET_PASSWORD_LENGTH];
    xwchar        MissionName[NET_MISSION_NAME_LENGTH];
    xwchar        GameType[NET_GAME_TYPE_LENGTH];
    xwchar        ShortGameType[NET_SHORT_GAME_TYPE_LENGTH];
    s32           GameTypeID;
    s32           FragLimit;
    s32           GameTime;
    s32           EliminationMode;
    mutation_mode MutationMode;
    skill_level   SkillLevel;
    s32           PercentComplete;
    s32           VotePassPercent;
    s32           AIType;
    s32           ScoreLimit;
    s32           CapCook;
    s32           NumRounds;
    s32           ConvertTime;
    s32           FirePercent;
    s32           Flags;
    connect_type  ConnectionType;
    s32           PasswordEnabled; //klkl: needed for optimatch filtering
    s32           VoiceEnabled;    //klkl: needed for optimatch filtering

    xbool operator==( const server_info& Right ) const
    {
        return ( ID == Right.ID ) && ( Remote == Right.Remote );
    }

    xbool operator!=( const server_info& Right ) const
    {
        return !( *this == Right );
    }
};

#endif // MATCH_SERVER_HPP
