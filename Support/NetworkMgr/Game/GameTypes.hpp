#ifndef GAME_TYPES_HPP
#define GAME_TYPES_HPP

//==============================================================================

enum game_type
{
    GAME_CAMPAIGN = -2,
    GAME_MP       = -1, // Only used on client.  Represents ALL online types.

    GAME_DM,    //   0
    GAME_TDM,   //   1
    GAME_CTF,   //   2
    GAME_TAG,   //   3
    GAME_INF,   //   4
    GAME_CNH,   //   5

    GAME_RESERVED_06,
    GAME_RESERVED_07,
    GAME_RESERVED_08,
    GAME_RESERVED_09,
    GAME_RESERVED_10,
    GAME_RESERVED_11,
    GAME_RESERVED_12,
    GAME_RESERVED_13,
    GAME_RESERVED_14,
    GAME_RESERVED_15,

    GAME_LAST,
    GAME_FIRST    = 0,
};

//==============================================================================

enum mutation_mode
{
    MUTATE_CHANGE_AT_WILL,
    MUTATE_HUMAN_LOCK,
    MUTATE_MUTANT_LOCK,
    MUTATE_HUMAN_VS_MUTANT,
    MUTATE_MAN_HUNT,
    MUTATE_MUTANT_HUNT,
};

//==============================================================================

enum skill_level
{
    SKILL_LEVEL_ANY,
    SKILL_LEVEL_BEGINNER,
    SKILL_LEVEL_INTERMEDIATE,
    SKILL_LEVEL_ADVANCED,
};

//==============================================================================


#endif // GAME_TYPES_HPP
