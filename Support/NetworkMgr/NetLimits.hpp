//==============================================================================
//
//  NetLimits.hpp
//
//==============================================================================

#ifndef NET_LIMITS_HPP
#define NET_LIMITS_HPP

//==============================================================================
//  CONSTANTS
//==============================================================================

#if defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)
const s32   NET_MAX_PER_CLIENT          =     1;        // No split-screen local client.
const s32   NET_MAX_LOCAL_CLIENTS       =    31;        // Client-hosted servers can host 31 remote clients.
#endif

#ifdef DEDICATED_SERVER
const s32   NET_MAX_LOCAL_CLIENTS       =    32;        // Dedicated servers host 32 clients.
#endif

const s32   NET_NAME_LENGTH             =    16;        // Player names.
const s32   NET_MAX_TOTAL_CLIENTS       =    32;        // Absolute maximum clients.
const s32   NET_MAX_PLAYERS             =    32;        // Absolute maximum players.
const s32   NET_MAX_OBJECTS_ON_SERVER   =   384;
const s32   NET_MAX_OBJECTS_ON_CLIENT   =    64;
const s32   NET_MAX_OBJECTS             =   NET_MAX_OBJECTS_ON_SERVER  +
                                           (NET_MAX_OBJECTS_ON_CLIENT * NET_MAX_TOTAL_CLIENTS);

const s32   NET_SERVER_NAME_LENGTH     =    24;
const s32   NET_PASSWORD_LENGTH        =    16;
const s32   NET_MAX_ID_LENGTH          =    64;
const s32   NET_MISSION_NAME_LENGTH    =    32;
const s32   NET_GAME_TYPE_LENGTH       =    32;
const s32   NET_SHORT_GAME_TYPE_LENGTH =     8;
const s32   NET_MAX_PATCH_SIZE         = 16384;

//==============================================================================
#endif // NET_LIMITS_HPP
//==============================================================================
