//==============================================================================
//
//  PlayerStats.hpp
//
//==============================================================================

#ifndef PLAYER_STATS_HPP
#define PLAYER_STATS_HPP

#include "x_types.hpp"

//==============================================================================

struct player_stats
{
    s32     KillsAsHuman;
    s32     KillsAsMutant;
    s32     Deaths;
    s32     PlayTime;   // In minutes.
    s32     Games;      // Only count games completed.
    s32     Wins;       // Only count 'valid' participation on winning team.
    s32     Gold;
    s32     Silver;
    s32     Bronze;
    s32     Kicks;
    s32     VotesStarted;
};

//==============================================================================
#endif // PLAYER_STATS_HPP
