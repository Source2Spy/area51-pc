//=========================================================================
//
//  GamePad.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GamePad.hpp"
#include "StringMgr/StringMgr.hpp"

//=========================================================================
//  GLOBAL STATE
//=========================================================================

frontend_input g_FrontendInput;
game_input     g_GameInput;

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
extern xtimer g_DemoIdleTimer;
#endif

//=========================================================================
//  HELPERS
//=========================================================================

static
void RecordDemoInputActivity( f32 Value )
{
#if CONFIG_IS_DEMO && !defined( X_EDITOR )
    if( x_abs( Value ) > 0.2f )
    {
        g_DemoIdleTimer.Trip();
    }
#else
    (void)Value;
#endif
}

//=========================================================================
//  STRUCTS
//=========================================================================

struct logical_definition
{
    s32         LogicalID;
    const char* pName;
};

//-------------------------------------------------------------------------

struct mapping_definition
{
    s32                     LogicalID;
    input_gadget            GadgetID;
    f32                     Scale;
    u32                     ContextMask;
    input_action_value_mode ValueMode = INPUT_ACTION_VALUE_AUTO;
};

//-------------------------------------------------------------------------

enum class prompt_map
{
    Gameplay,
    Frontend,
    System,
};

//-------------------------------------------------------------------------

struct prompt_definition
{
    const char* pToken;
    prompt_map  Map;
    s32         LogicalID;
};

//=========================================================================
//  LOGICAL DEFINITIONS
//=========================================================================

static
const logical_definition s_GameplayLogicals[] =
{
    { ingame_pad::MOVE_FORWARD,                "Move Forward" },
    { ingame_pad::MOVE_BACKWARD,               "Move Backward" },
    { ingame_pad::STRAFE_LEFT,                 "Strafe Left" },
    { ingame_pad::STRAFE_RIGHT,                "Strafe Right" },
    { ingame_pad::LOOK_HORIZONTAL,             "Horizontal Look" },
    { ingame_pad::LOOK_VERTICAL,               "Vertical Look" },
    { ingame_pad::LEAN_LEFT,                   "Lean Left" },
    { ingame_pad::LEAN_RIGHT,                  "Lean Right" },

    { ingame_pad::ACTION_RELOAD,                 "Reload" },
    { ingame_pad::ACTION_PRIMARY,                "Primary Fire" },
    { ingame_pad::ACTION_SECONDARY,              "Secondary Fire" },
    { ingame_pad::ACTION_JUMP,                   "Jump" },
    { ingame_pad::ACTION_CROUCH,                 "Crouch" },
    { ingame_pad::ACTION_MUTATION,               "Toggle Mutation" },
    { ingame_pad::ACTION_FIRE_PARASITES,         "Fire Parasites" },
    { ingame_pad::ACTION_FIRE_CONTAGION,         "Fire Contagion" },
    { ingame_pad::ACTION_CYCLE_RIGHT,            "Cycle Weapons Right" },
    { ingame_pad::ACTION_CYCLE_LEFT,             "Cycle Weapons Left" },
    { ingame_pad::ACTION_USE,                    "Use Object" },
    { ingame_pad::ACTION_FLASHLIGHT,             "Toggle Flashlight" },
    { ingame_pad::ACTION_THROW_GRENADE,          "Throw Grenade" },
    { ingame_pad::ACTION_MELEE_ATTACK,           "Melee Attack" },
    { ingame_pad::ACTION_SWITCH_TO_SCANNER,      "Switch To Scanner" },
    { ingame_pad::ACTION_SWITCH_TO_PISTOL,       "Switch To Pistol" },
    { ingame_pad::ACTION_SWITCH_TO_SMP,          "Switch To SMP" },
    { ingame_pad::ACTION_SWITCH_TO_SHOTGUN,      "Switch To Shotgun" },
    { ingame_pad::ACTION_SWITCH_TO_SNIPER_RIFLE, "Switch To Sniper Rifle" },
    { ingame_pad::ACTION_SWITCH_TO_BBG,          "Switch To BBG" },
    { ingame_pad::ACTION_SWITCH_TO_MESON_CANNON, "Switch To Meson Cannon" },

    { ingame_pad::ACTION_SPEAK_FOLLOW_STAY,    "Speak: Follow Me" },
    { ingame_pad::ACTION_SPEAK_USE_ACTIVATE,   "Speak: Use / Activate" },
    { ingame_pad::ACTION_SPEAK_COVER_ME,       "Speak: Cover Me" },
    { ingame_pad::ACTION_SPEAK_ATTACK_COVER,   "Speak: Attack / Take Cover" },

    { ingame_pad::ACTION_VOTE_MENU_ON,         "Vote: Menu On" },
    { ingame_pad::ACTION_VOTE_MENU_OFF,        "Vote: Menu Off" },
    { ingame_pad::ACTION_VOTE_YES,             "Vote: Yes" },
    { ingame_pad::ACTION_VOTE_NO,              "Vote: No" },
    { ingame_pad::ACTION_VOTE_ABSTAIN,         "Vote: Abstain" },

    { ingame_pad::ACTION_CHAT,                 "Voice Chat" },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE,     "Talk: Mode Toggle" },
    { ingame_pad::ACTION_DROP_FLAG,            "Drop Flag" },
    { ingame_pad::ACTION_SCOREBOARD,           "Show Scoreboard" },
};

//-------------------------------------------------------------------------

static
const logical_definition s_FrontendLogicals[] =
{
    { frontend_pad::UI_UP,                     "UI Up" },
    { frontend_pad::UI_DOWN,                   "UI Down" },
    { frontend_pad::UI_LEFT,                   "UI Left" },
    { frontend_pad::UI_RIGHT,                  "UI Right" },
    { frontend_pad::UI_SELECT,                 "UI Select" },
    { frontend_pad::UI_BACK,                   "UI Back" },
    { frontend_pad::UI_DELETE,                 "UI Delete" },
    { frontend_pad::UI_ACTIVATE,               "UI Activate" },
    { frontend_pad::UI_SHOULDER_L,             "UI Shoulder Left" },
    { frontend_pad::UI_SHOULDER_R,             "UI Shoulder Right" },
    { frontend_pad::UI_SHOULDER_L2,            "UI Shoulder Left 2" },
    { frontend_pad::UI_SHOULDER_R2,            "UI Shoulder Right 2" },
    { frontend_pad::UI_HELP,                   "UI Help" },
};

//-------------------------------------------------------------------------

#if defined( ENABLE_DEBUG_MENU )
static
const logical_definition s_DebugMenuLogicals[] =
{
    { frontend_pad::DEBUG_MENU_NEXT_PAGE,      "Debug Menu Next Page" },
    { frontend_pad::DEBUG_MENU_PREV_PAGE,      "Debug Menu Previous Page" },
    { frontend_pad::DEBUG_MENU_NEXT_ITEM,      "Debug Menu Next Item" },
    { frontend_pad::DEBUG_MENU_PREV_ITEM,      "Debug Menu Previous Item" },
    { frontend_pad::DEBUG_MENU_INCREMENT,      "Debug Menu Increment" },
    { frontend_pad::DEBUG_MENU_DECREMENT,      "Debug Menu Decrement" },
    { frontend_pad::DEBUG_MENU_ACTION,         "Debug Menu Action" },
    { frontend_pad::DEBUG_MENU_EXIT_MODIFIER,  "Debug Menu Exit Modifier" },
    { frontend_pad::DEBUG_MENU_EXIT,           "Debug Menu Exit" },
};
#endif // defined( ENABLE_DEBUG_MENU )

//=========================================================================
//  MAPPING DEFINITIONS
//=========================================================================

struct keyboard_definition
{
    ingame_pad::logical_id LogicalID;
    input_gadget          DefaultKey;
    const char*           pLabel;
};

//-------------------------------------------------------------------------

static const keyboard_definition s_KeyboardActions[] =
{
    { ingame_pad::MOVE_FORWARD, INPUT_KBD_W, "MOVE_FORWARD" },
    { ingame_pad::MOVE_BACKWARD, INPUT_KBD_S, "MOVE_BACKWARD" },
    { ingame_pad::STRAFE_LEFT, INPUT_KBD_A, "STRAFE_LEFT" },
    { ingame_pad::STRAFE_RIGHT, INPUT_KBD_D, "STRAFE_RIGHT" },
    { ingame_pad::ACTION_JUMP, INPUT_KBD_SPACE, "JUMP" },
    { ingame_pad::ACTION_CROUCH, INPUT_KBD_LCONTROL, "CROUCH" },
    { ingame_pad::ACTION_RELOAD, INPUT_KBD_R, "RELOAD" },
    { ingame_pad::ACTION_MUTATION, INPUT_KBD_X, "TOGGLE_MUTATION" },
    { ingame_pad::ACTION_USE, INPUT_KBD_TAB, "USE" },
    { ingame_pad::ACTION_FLASHLIGHT, INPUT_KBD_F, "TOGGLE_FLASHLIGHT" },
    { ingame_pad::ACTION_THROW_GRENADE, INPUT_KBD_G, "THROW_A_GRENADE" },
    { ingame_pad::ACTION_MELEE_ATTACK, INPUT_KBD_V, "MELEE_ATTACK" },
    { ingame_pad::ACTION_SWITCH_TO_SCANNER, INPUT_KBD_1, "SWITCH_TO_SCANNER" },
    { ingame_pad::ACTION_SWITCH_TO_PISTOL, INPUT_KBD_2, "SWITCH_TO_PISTOL" },
    { ingame_pad::ACTION_SWITCH_TO_SMP, INPUT_KBD_3, "SWITCH_TO_SMP" },
    { ingame_pad::ACTION_SWITCH_TO_SHOTGUN, INPUT_KBD_4, "SWITCH_TO_SHOTGUN" },
    { ingame_pad::ACTION_SWITCH_TO_SNIPER_RIFLE, INPUT_KBD_5, "SWITCH_TO_SNIPER_RIFLE" },
    { ingame_pad::ACTION_SWITCH_TO_BBG, INPUT_KBD_6, "SWITCH_TO_BBG" },
    { ingame_pad::ACTION_SWITCH_TO_MESON_CANNON, INPUT_KBD_7, "SWITCH_TO_MESON_CANNON" },
    { ingame_pad::ACTION_VOTE_MENU_ON, INPUT_KBD_F1, "IDS_BIND_ACTION_VOTE_MENU_ON" },
    { ingame_pad::ACTION_VOTE_MENU_OFF, INPUT_KBD_F1, "IDS_BIND_ACTION_VOTE_MENU_OFF" },
    { ingame_pad::ACTION_VOTE_YES, INPUT_KBD_F2, "VOTE_YES" },
    { ingame_pad::ACTION_VOTE_NO, INPUT_KBD_F3, "VOTE_NO" },
    { ingame_pad::ACTION_VOTE_ABSTAIN, INPUT_KBD_F4, "VOTE_ABSTAIN" },
    { ingame_pad::ACTION_CHAT, INPUT_KBD_T, "CHAT" },
    { ingame_pad::LEAN_LEFT, INPUT_KBD_Q, "LEAN_LEFT" },
    { ingame_pad::LEAN_RIGHT, INPUT_KBD_E, "LEAN_RIGHT" },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE, INPUT_KBD_Y, "IDS_BIND_ACTION_TALK_MODE_TOGGLE" },
    { ingame_pad::ACTION_DROP_FLAG, INPUT_KBD_N, "DROP_FLAG" },
    { ingame_pad::ACTION_SCOREBOARD, INPUT_KBD_TAB, "SHOW_SCORE" },
};

//-------------------------------------------------------------------------

struct keyboard_key_definition
{
    input_gadget Key;
    const char*  pLabel;
};

//-------------------------------------------------------------------------

static const keyboard_key_definition s_KeyboardKeys[] =
{
    { INPUT_KBD_1, "1" },
    { INPUT_KBD_2, "2" },
    { INPUT_KBD_3, "3" },
    { INPUT_KBD_4, "4" },
    { INPUT_KBD_5, "5" },
    { INPUT_KBD_6, "6" },
    { INPUT_KBD_7, "7" },
    { INPUT_KBD_8, "8" },
    { INPUT_KBD_9, "9" },
    { INPUT_KBD_0, "0" },
    { INPUT_KBD_MINUS, "MINUS" },
    { INPUT_KBD_EQUALS, "EQUALS" },
    { INPUT_KBD_BACK, "BACK" },
    { INPUT_KBD_TAB, "TAB" },
    { INPUT_KBD_Q, "Q" },
    { INPUT_KBD_W, "W" },
    { INPUT_KBD_E, "E" },
    { INPUT_KBD_R, "R" },
    { INPUT_KBD_T, "T" },
    { INPUT_KBD_Y, "Y" },
    { INPUT_KBD_U, "U" },
    { INPUT_KBD_I, "I" },
    { INPUT_KBD_O, "O" },
    { INPUT_KBD_P, "P" },
    { INPUT_KBD_LBRACKET, "LBRACKET" },
    { INPUT_KBD_RBRACKET, "RBRACKET" },
    { INPUT_KBD_RETURN, "RETURN" },
    { INPUT_KBD_LCONTROL, "LCONTROL" },
    { INPUT_KBD_A, "A" },
    { INPUT_KBD_S, "S" },
    { INPUT_KBD_D, "D" },
    { INPUT_KBD_F, "F" },
    { INPUT_KBD_G, "G" },
    { INPUT_KBD_H, "H" },
    { INPUT_KBD_J, "J" },
    { INPUT_KBD_K, "K" },
    { INPUT_KBD_L, "L" },
    { INPUT_KBD_SEMICOLON, "SEMICOLON" },
    { INPUT_KBD_APOSTROPHE, "APOSTROPHE" },
    { INPUT_KBD_GRAVE, "GRAVE" },
    { INPUT_KBD_LSHIFT, "LSHIFT" },
    { INPUT_KBD_BACKSLASH, "BACKSLASH" },
    { INPUT_KBD_Z, "Z" },
    { INPUT_KBD_X, "X" },
    { INPUT_KBD_C, "C" },
    { INPUT_KBD_V, "V" },
    { INPUT_KBD_B, "B" },
    { INPUT_KBD_N, "N" },
    { INPUT_KBD_M, "M" },
    { INPUT_KBD_COMMA, "COMMA" },
    { INPUT_KBD_PERIOD, "PERIOD" },
    { INPUT_KBD_SLASH, "SLASH" },
    { INPUT_KBD_RSHIFT, "RSHIFT" },
    { INPUT_KBD_MULTIPLY, "MULTIPLY" },
    { INPUT_KBD_LMENU, "LMENU" },
    { INPUT_KBD_SPACE, "SPACE" },
    { INPUT_KBD_CAPITAL, "CAPITAL" },
    { INPUT_KBD_F1, "F1" },
    { INPUT_KBD_F2, "F2" },
    { INPUT_KBD_F3, "F3" },
    { INPUT_KBD_F4, "F4" },
    { INPUT_KBD_F5, "F5" },
    { INPUT_KBD_F6, "F6" },
    { INPUT_KBD_F7, "F7" },
    { INPUT_KBD_F8, "F8" },
    { INPUT_KBD_F9, "F9" },
    { INPUT_KBD_F10, "F10" },
    { INPUT_KBD_NUMLOCK, "NUMLOCK" },
    { INPUT_KBD_SCROLL, "SCROLL" },
    { INPUT_KBD_NUMPAD7, "NUMPAD7" },
    { INPUT_KBD_NUMPAD8, "NUMPAD8" },
    { INPUT_KBD_NUMPAD9, "NUMPAD9" },
    { INPUT_KBD_SUBTRACT, "SUBTRACT" },
    { INPUT_KBD_NUMPAD4, "NUMPAD4" },
    { INPUT_KBD_NUMPAD5, "NUMPAD5" },
    { INPUT_KBD_NUMPAD6, "NUMPAD6" },
    { INPUT_KBD_ADD, "ADD" },
    { INPUT_KBD_NUMPAD1, "NUMPAD1" },
    { INPUT_KBD_NUMPAD2, "NUMPAD2" },
    { INPUT_KBD_NUMPAD3, "NUMPAD3" },
    { INPUT_KBD_NUMPAD0, "NUMPAD0" },
    { INPUT_KBD_DECIMAL, "DECIMAL" },
    { INPUT_KBD_OEM_102, "OEM_102" },
    { INPUT_KBD_F11, "F11" },
    { INPUT_KBD_F12, "F12" },
    { INPUT_KBD_F13, "F13" },
    { INPUT_KBD_F14, "F14" },
    { INPUT_KBD_F15, "F15" },
    { INPUT_KBD_KANA, "KANA" },
    { INPUT_KBD_ABNT_C1, "ABNT_C1" },
    { INPUT_KBD_CONVERT, "CONVERT" },
    { INPUT_KBD_NOCONVERT, "NOCONVERT" },
    { INPUT_KBD_YEN, "YEN" },
    { INPUT_KBD_ABNT_C2, "ABNT_C2" },
    { INPUT_KBD_NUMPADEQUALS, "NUMPADEQUALS" },
    { INPUT_KBD_PREVTRACK, "PREVTRACK" },
    { INPUT_KBD_AT, "AT" },
    { INPUT_KBD_COLON, "COLON" },
    { INPUT_KBD_UNDERLINE, "UNDERLINE" },
    { INPUT_KBD_KANJI, "KANJI" },
    { INPUT_KBD_STOP, "STOP" },
    { INPUT_KBD_AX, "AX" },
    { INPUT_KBD_UNLABELED, "UNLABELED" },
    { INPUT_KBD_NEXTTRACK, "NEXTTRACK" },
    { INPUT_KBD_NUMPADENTER, "NUMPADENTER" },
    { INPUT_KBD_RCONTROL, "RCONTROL" },
    { INPUT_KBD_MUTE, "MUTE" },
    { INPUT_KBD_CALCULATOR, "CALCULATOR" },
    { INPUT_KBD_PLAYPAUSE, "PLAYPAUSE" },
    { INPUT_KBD_MEDIASTOP, "MEDIASTOP" },
    { INPUT_KBD_VOLUMEDOWN, "VOLUMEDOWN" },
    { INPUT_KBD_VOLUMEUP, "VOLUMEUP" },
    { INPUT_KBD_WEBHOME, "WEBHOME" },
    { INPUT_KBD_NUMPADCOMMA, "NUMPADCOMMA" },
    { INPUT_KBD_DIVIDE, "DIVIDE" },
    { INPUT_KBD_SYSRQ, "SYSRQ" },
    { INPUT_KBD_RMENU, "RMENU" },
    { INPUT_KBD_PAUSE, "PAUSE_GAME" },
    { INPUT_KBD_HOME, "HOME" },
    { INPUT_KBD_UP, "UP" },
    { INPUT_KBD_PRIOR, "PRIOR" },
    { INPUT_KBD_LEFT, "LEFT" },
    { INPUT_KBD_RIGHT, "RIGHT" },
    { INPUT_KBD_END, "END" },
    { INPUT_KBD_DOWN, "DOWN" },
    { INPUT_KBD_NEXT, "NEXT" },
    { INPUT_KBD_INSERT, "INSERT" },
    { INPUT_KBD_DELETE, "DELETE" },
    { INPUT_KBD_LWIN, "LWIN" },
    { INPUT_KBD_RWIN, "RWIN" },
    { INPUT_KBD_APPS, "APPS" },
    { INPUT_KBD_POWER, "POWER" },
    { INPUT_KBD_SLEEP, "SLEEP" },
    { INPUT_KBD_WAKE, "WAKE" },
    { INPUT_KBD_WEBSEARCH, "WEBSEARCH" },
    { INPUT_KBD_WEBFAVORITES, "WEBFAVORITES" },
    { INPUT_KBD_WEBREFRESH, "WEBREFRESH" },
    { INPUT_KBD_WEBSTOP, "WEBSTOP" },
    { INPUT_KBD_WEBFORWARD, "WEBFORWARD" },
    { INPUT_KBD_WEBBACK, "WEBBACK" },
    { INPUT_KBD_MYCOMPUTER, "MYCOMPUTER" },
    { INPUT_KBD_MAIL, "MAIL" },
    { INPUT_KBD_MEDIASELECT, "MEDIASELECT" },
};

//-------------------------------------------------------------------------

static
const mapping_definition s_GameplayMappings[] =
{
    { ingame_pad::MOVE_FORWARD,                          INPUT_XBOX_STICK_LEFT_Y,   1.0f,   INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::MOVE_BACKWARD,                         INPUT_XBOX_STICK_LEFT_Y,   -1.0f,  INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::STRAFE_LEFT,                           INPUT_XBOX_STICK_LEFT_X,   -1.0f,  INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::STRAFE_RIGHT,                          INPUT_XBOX_STICK_LEFT_X,   1.0f,   INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::LOOK_HORIZONTAL,                       INPUT_XBOX_STICK_RIGHT_X,  1.0f,   INGAME_CONTEXT, INPUT_ACTION_VALUE_SIGNED_AXIS },
    { ingame_pad::LOOK_VERTICAL,                         INPUT_XBOX_STICK_RIGHT_Y,  -1.0f,  INGAME_CONTEXT, INPUT_ACTION_VALUE_SIGNED_AXIS },
    { ingame_pad::ACTION_JUMP,                           INPUT_XBOX_BTN_A,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CROUCH,                         INPUT_XBOX_BTN_L_STICK,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_PRIMARY,                        INPUT_XBOX_R_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SECONDARY,                      INPUT_XBOX_L_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_RELOAD,                         INPUT_XBOX_BTN_X,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTATION,                       INPUT_XBOX_BTN_UP,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_RIGHT,                    INPUT_XBOX_BTN_Y,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_USE,                            INPUT_XBOX_BTN_X,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FLASHLIGHT,                     INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_THROW_GRENADE,                  INPUT_XBOX_BTN_B,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MELEE_ATTACK,                   INPUT_XBOX_BTN_R_STICK,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_LEFT,                     INPUT_XBOX_BTN_BLACK,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_ON,                   INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_OFF,                  INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_YES,                       INPUT_XBOX_BTN_RIGHT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_NO,                        INPUT_XBOX_BTN_LEFT,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_ABSTAIN,                   INPUT_XBOX_BTN_UP,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CHAT,                           INPUT_XBOX_BTN_WHITE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_LEFT,                             INPUT_XBOX_BTN_LEFT,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_RIGHT,                            INPUT_XBOX_BTN_RIGHT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE,               INPUT_XBOX_BTN_WHITE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_PARASITES,                 INPUT_XBOX_R_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_CONTAGION,                 INPUT_XBOX_L_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_DROP_FLAG,                      INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SCOREBOARD,                     INPUT_XBOX_BTN_BACK,       1.0f,   INGAME_CONTEXT },

    { ingame_pad::LOOK_HORIZONTAL,                       INPUT_MOUSE_X_REL,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::LOOK_VERTICAL,                         INPUT_MOUSE_Y_REL,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_PRIMARY,                        INPUT_MOUSE_BTN_L,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SECONDARY,                      INPUT_MOUSE_BTN_R,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_RIGHT,                    INPUT_MOUSE_WHEEL_REL,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_LEFT,                     INPUT_MOUSE_WHEEL_REL,     -1.0f,  INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_PARASITES,                 INPUT_MOUSE_BTN_L,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_CONTAGION,                 INPUT_MOUSE_BTN_R,         1.0f,   INGAME_CONTEXT },

    { ingame_pad::MOVE_FORWARD,                          INPUT_PS2_STICK_LEFT_Y,    1.0f,   INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::MOVE_BACKWARD,                         INPUT_PS2_STICK_LEFT_Y,    -1.0f,  INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::STRAFE_LEFT,                           INPUT_PS2_STICK_LEFT_X,    -1.0f,  INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::STRAFE_RIGHT,                          INPUT_PS2_STICK_LEFT_X,    1.0f,   INGAME_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { ingame_pad::LOOK_HORIZONTAL,                       INPUT_PS2_STICK_RIGHT_X,   1.0f,   INGAME_CONTEXT, INPUT_ACTION_VALUE_SIGNED_AXIS },
    { ingame_pad::LOOK_VERTICAL,                         INPUT_PS2_STICK_RIGHT_Y,   -1.0f,  INGAME_CONTEXT, INPUT_ACTION_VALUE_SIGNED_AXIS },
    { ingame_pad::ACTION_JUMP,                           INPUT_PS2_BTN_L1,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CROUCH,                         INPUT_PS2_BTN_L2,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_PRIMARY,                        INPUT_PS2_BTN_R1,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SECONDARY,                      INPUT_PS2_BTN_R2,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_RELOAD,                         INPUT_PS2_BTN_CROSS,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTATION,                       INPUT_PS2_BTN_L_UP,        1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_RIGHT,                    INPUT_PS2_BTN_CIRCLE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_USE,                            INPUT_PS2_BTN_CROSS,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FLASHLIGHT,                     INPUT_PS2_BTN_L_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SPEAK_USE_ACTIVATE,             INPUT_PS2_BTN_CROSS,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_THROW_GRENADE,                  INPUT_PS2_BTN_SQUARE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MELEE_ATTACK,                   INPUT_PS2_BTN_R_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_LEFT,                     INPUT_PS2_BTN_TRIANGLE,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_ON,                   INPUT_PS2_BTN_L_DOWN,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_OFF,                  INPUT_PS2_BTN_L_DOWN,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_YES,                       INPUT_PS2_BTN_L_RIGHT,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_NO,                        INPUT_PS2_BTN_L_LEFT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_ABSTAIN,                   INPUT_PS2_BTN_L_UP,        1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CHAT,                           INPUT_PS2_BTN_L_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_LEFT,                             INPUT_PS2_BTN_L_LEFT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_RIGHT,                            INPUT_PS2_BTN_L_RIGHT,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE,               INPUT_PS2_BTN_SELECT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_PARASITES,                 INPUT_PS2_BTN_R1,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_CONTAGION,                 INPUT_PS2_BTN_R2,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_DROP_FLAG,                      INPUT_PS2_BTN_L_DOWN,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SCOREBOARD,                     INPUT_PS2_BTN_SELECT,      1.0f,   INGAME_CONTEXT },
};

//-------------------------------------------------------------------------

static
const mapping_definition s_SystemMappings[] =
{
    { system_pad::PAUSE,                                  INPUT_XBOX_BTN_START,      1.0f,   ALL_CONTEXTS },
    { system_pad::PAUSE,                                  INPUT_KBD_ESCAPE,          1.0f,   ALL_CONTEXTS },
    { system_pad::PAUSE,                                  INPUT_PS2_BTN_START,       1.0f,   ALL_CONTEXTS },
};

//-------------------------------------------------------------------------

static
const mapping_definition s_FrontendMappings[] =
{
    { frontend_pad::UI_UP,                               INPUT_XBOX_BTN_UP,         1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_DOWN,                             INPUT_XBOX_BTN_DOWN,       1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_LEFT,                             INPUT_XBOX_BTN_LEFT,       1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_RIGHT,                            INPUT_XBOX_BTN_RIGHT,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_UP,                               INPUT_XBOX_STICK_LEFT_Y,   1.0f,   FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_DOWN,                             INPUT_XBOX_STICK_LEFT_Y,   -1.0f,  FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_LEFT,                             INPUT_XBOX_STICK_LEFT_X,   -1.0f,  FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_RIGHT,                            INPUT_XBOX_STICK_LEFT_X,   1.0f,   FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_SELECT,                           INPUT_XBOX_BTN_A,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_BACK,                             INPUT_XBOX_BTN_B,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_BACK,                             INPUT_XBOX_BTN_BACK,       1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_DELETE,                           INPUT_XBOX_BTN_X,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_ACTIVATE,                         INPUT_XBOX_BTN_Y,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_L,                       INPUT_XBOX_BTN_WHITE,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_R,                       INPUT_XBOX_BTN_BLACK,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_L2,                      INPUT_XBOX_L_TRIGGER,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_R2,                      INPUT_XBOX_R_TRIGGER,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_HELP,                             INPUT_XBOX_BTN_START,      1.0f,   FRONTEND_CONTEXT },

    { frontend_pad::UI_UP,                               INPUT_KBD_UP,              1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_DOWN,                             INPUT_KBD_DOWN,            1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_LEFT,                             INPUT_KBD_LEFT,            1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_RIGHT,                            INPUT_KBD_RIGHT,           1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SELECT,                           INPUT_KBD_RETURN,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_BACK,                             INPUT_KBD_ESCAPE,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_DELETE,                           INPUT_KBD_DELETE,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_DELETE,                           INPUT_KBD_BACK,            1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_ACTIVATE,                         INPUT_KBD_R,               1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_L,                       INPUT_KBD_PRIOR,           1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_R,                       INPUT_KBD_NEXT,            1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_L2,                      INPUT_KBD_HOME,            1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_R2,                      INPUT_KBD_END,             1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_HELP,                             INPUT_KBD_RETURN,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_UP,                               INPUT_PS2_BTN_L_UP,        1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_DOWN,                             INPUT_PS2_BTN_L_DOWN,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_LEFT,                             INPUT_PS2_BTN_L_LEFT,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_RIGHT,                            INPUT_PS2_BTN_L_RIGHT,     1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_UP,                               INPUT_PS2_STICK_LEFT_Y,    1.0f,   FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_DOWN,                             INPUT_PS2_STICK_LEFT_Y,    -1.0f,  FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_LEFT,                             INPUT_PS2_STICK_LEFT_X,    -1.0f,  FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_RIGHT,                            INPUT_PS2_STICK_LEFT_X,    1.0f,   FRONTEND_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::UI_SELECT,                           INPUT_PS2_BTN_CROSS,       1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_BACK,                             INPUT_PS2_BTN_SQUARE,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_DELETE,                           INPUT_PS2_BTN_CIRCLE,      1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_ACTIVATE,                         INPUT_PS2_BTN_TRIANGLE,    1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_L,                       INPUT_PS2_BTN_L1,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_R,                       INPUT_PS2_BTN_R1,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_L2,                      INPUT_PS2_BTN_L2,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_SHOULDER_R2,                      INPUT_PS2_BTN_R2,          1.0f,   FRONTEND_CONTEXT },
    { frontend_pad::UI_HELP,                             INPUT_PS2_BTN_START,       1.0f,   FRONTEND_CONTEXT },
};

//-------------------------------------------------------------------------

#if defined( ENABLE_DEBUG_MENU )
static
const mapping_definition s_DebugMenuMappings[] =
{
    { frontend_pad::DEBUG_MENU_EXIT_MODIFIER,            INPUT_XBOX_BTN_BACK,       1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_EXIT,                     INPUT_XBOX_BTN_START,      1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_PAGE,                INPUT_XBOX_BTN_BLACK,      1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_PAGE,                INPUT_XBOX_R_TRIGGER,      1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_PREV_PAGE,                INPUT_XBOX_BTN_WHITE,      1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_PREV_PAGE,                INPUT_XBOX_L_TRIGGER,      1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_ITEM,                INPUT_XBOX_BTN_DOWN,       1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_ITEM,                INPUT_XBOX_STICK_LEFT_Y,  -1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_PREV_ITEM,                INPUT_XBOX_BTN_UP,         1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_PREV_ITEM,                INPUT_XBOX_STICK_LEFT_Y,   1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_INCREMENT,                INPUT_XBOX_BTN_RIGHT,      1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_INCREMENT,                INPUT_XBOX_STICK_LEFT_X,   1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_DECREMENT,                INPUT_XBOX_BTN_LEFT,       1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_DECREMENT,                INPUT_XBOX_STICK_LEFT_X,  -1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_ACTION,                   INPUT_XBOX_BTN_A,          1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_ACTION,                   INPUT_XBOX_BTN_X,          1.0f,   DEBUG_MENU_CONTEXT },

    { frontend_pad::DEBUG_MENU_EXIT_MODIFIER,            INPUT_KBD_TAB,             1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_EXIT,                     INPUT_KBD_RETURN,          1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_ITEM,                INPUT_KBD_DOWN,            1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_PREV_ITEM,                INPUT_KBD_UP,              1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_INCREMENT,                INPUT_KBD_RIGHT,           1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_DECREMENT,                INPUT_KBD_LEFT,            1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_ACTION,                   INPUT_KBD_RETURN,          1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_ACTION,                   INPUT_KBD_DELETE,          1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_ACTION,                   INPUT_KBD_BACK,            1.0f,   DEBUG_MENU_CONTEXT },

    { frontend_pad::DEBUG_MENU_EXIT_MODIFIER,            INPUT_PS2_BTN_SELECT,      1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_EXIT,                     INPUT_PS2_BTN_START,       1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_PAGE,                INPUT_PS2_BTN_R1,          1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_PAGE,                INPUT_PS2_BTN_R2,          1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_PREV_PAGE,                INPUT_PS2_BTN_L1,          1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_PREV_PAGE,                INPUT_PS2_BTN_L2,          1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_ITEM,                INPUT_PS2_BTN_L_DOWN,      1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_NEXT_ITEM,                INPUT_PS2_STICK_LEFT_Y,   -1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_PREV_ITEM,                INPUT_PS2_BTN_L_UP,        1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_PREV_ITEM,                INPUT_PS2_STICK_LEFT_Y,    1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_INCREMENT,                INPUT_PS2_BTN_L_RIGHT,     1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_INCREMENT,                INPUT_PS2_STICK_LEFT_X,    1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_DECREMENT,                INPUT_PS2_BTN_L_LEFT,      1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_DECREMENT,                INPUT_PS2_STICK_LEFT_X,   -1.0f,   DEBUG_MENU_CONTEXT, INPUT_ACTION_VALUE_POSITIVE_AXIS },
    { frontend_pad::DEBUG_MENU_ACTION,                   INPUT_PS2_BTN_CROSS,       1.0f,   DEBUG_MENU_CONTEXT },
    { frontend_pad::DEBUG_MENU_ACTION,                   INPUT_PS2_BTN_CIRCLE,      1.0f,   DEBUG_MENU_CONTEXT },
};
#endif // defined( ENABLE_DEBUG_MENU )

//=========================================================================
//  PROMPT DEFINITIONS
//=========================================================================

static
const prompt_definition s_PromptDefinitions[] =
{
    { "x",          prompt_map::Frontend, frontend_pad::UI_SELECT             },
    { "q",          prompt_map::Frontend, frontend_pad::UI_DELETE             },
    { "a",          prompt_map::Frontend, frontend_pad::UI_BACK               },
    { "o",          prompt_map::Frontend, frontend_pad::UI_ACTIVATE           },
    { "X",          prompt_map::Frontend, frontend_pad::UI_DELETE             },
    { "Y",          prompt_map::Frontend, frontend_pad::UI_ACTIVATE           },
    { "A",          prompt_map::Frontend, frontend_pad::UI_SELECT             },
    { "B",          prompt_map::Frontend, frontend_pad::UI_BACK               },
    { "S",          prompt_map::Frontend, frontend_pad::UI_HELP               },
    { "SQUARE",     prompt_map::Frontend, frontend_pad::UI_DELETE             },
    { "CROSS",      prompt_map::Frontend, frontend_pad::UI_SELECT             },
    { "TRIANGLE",   prompt_map::Frontend, frontend_pad::UI_ACTIVATE           },
    { "CIRCLE",     prompt_map::Frontend, frontend_pad::UI_BACK               },
    { "DOWN",       prompt_map::Frontend, frontend_pad::UI_DOWN               },
    { "LEFT",       prompt_map::Frontend, frontend_pad::UI_LEFT               },
    { "UP",         prompt_map::Frontend, frontend_pad::UI_UP                 },
    { "RIGHT",      prompt_map::Frontend, frontend_pad::UI_RIGHT              },
    { "UPDOWN",     prompt_map::Frontend, frontend_pad::UI_UP                 },
    { "LEFTRIGHT",  prompt_map::Frontend, frontend_pad::UI_LEFT               },
    { "PAUSE",      prompt_map::System,   system_pad::PAUSE                   },
    { "START",      prompt_map::Frontend, frontend_pad::UI_HELP               },
    { "SELECT",     prompt_map::Frontend, frontend_pad::UI_SELECT             },
    { "BACK",       prompt_map::Frontend, frontend_pad::UI_BACK               },
    { "DELETE",     prompt_map::Frontend, frontend_pad::UI_DELETE             },
    { "ACTIVATE",   prompt_map::Frontend, frontend_pad::UI_ACTIVATE           },
    { "GRENADE",    prompt_map::Gameplay, ingame_pad::ACTION_THROW_GRENADE    },
    { "RELOAD",     prompt_map::Gameplay, ingame_pad::ACTION_RELOAD           },
    { "USE",        prompt_map::Gameplay, ingame_pad::ACTION_USE              },
    { "PREVWEAPON", prompt_map::Gameplay, ingame_pad::ACTION_CYCLE_LEFT       },
    { "NEXTWEAPON", prompt_map::Gameplay, ingame_pad::ACTION_CYCLE_RIGHT      },
    { "LEANLEFT",   prompt_map::Gameplay, ingame_pad::LEAN_LEFT               },
    { "LEANRIGHT",  prompt_map::Gameplay, ingame_pad::LEAN_RIGHT              },
    { "LEAN",       prompt_map::Gameplay, ingame_pad::LEAN_LEFT               },
    { "MUTATE",     prompt_map::Gameplay, ingame_pad::ACTION_MUTATION         },
    { "TRANSFORM",  prompt_map::Gameplay, ingame_pad::ACTION_MUTATION         },
    { "JUMP",       prompt_map::Gameplay, ingame_pad::ACTION_JUMP             },
    { "CROUCH",     prompt_map::Gameplay, ingame_pad::ACTION_CROUCH           },
    { "FLASHLIGHT", prompt_map::Gameplay, ingame_pad::ACTION_FLASHLIGHT       },
    { "FIRE",       prompt_map::Gameplay, ingame_pad::ACTION_PRIMARY          },
    { "SECONDARY",  prompt_map::Gameplay, ingame_pad::ACTION_SECONDARY        },
    { "MELEE",      prompt_map::Gameplay, ingame_pad::ACTION_MELEE_ATTACK     },
    { "PARASITE",   prompt_map::Gameplay, ingame_pad::ACTION_FIRE_PARASITES   },
    { "CONTAGION",  prompt_map::Gameplay, ingame_pad::ACTION_FIRE_CONTAGION   },
};

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

template <class T, s32 Count>
static
s32 CountOf( const T ( & )[Count] )
{
    return Count;
}

//=========================================================================

template <typename TPad, s32 Count>
static
void RegisterLogicalNames( TPad& Pad, const logical_definition ( &Definitions )[Count] )
{
    for( s32 i = 0; i < Count; i++ )
    {
        Pad.SetActionName( Definitions[i].LogicalID, Definitions[i].pName );
    }
}

//=========================================================================

template <typename TPad, s32 Count>
static
void RegisterInputMappings( TPad& Pad, const mapping_definition ( &Definitions )[Count] )
{
    for( s32 i = 0; i < Count; i++ )
    {
        const mapping_definition& Mapping = Definitions[i];
        Pad.AddBinding( Mapping.LogicalID,
                        Mapping.GadgetID,
                        Mapping.Scale,
                        Mapping.ContextMask,
                        Mapping.ValueMode );
    }
}

//=========================================================================

template <s32 Count>
static
const char* FindRegisteredLogicalName( const logical_definition ( &Definitions )[Count], s32 LogicalID )
{
    for( s32 i = 0; i < Count; i++ )
    {
        if( Definitions[i].LogicalID == LogicalID )
            return Definitions[i].pName;
    }

    return NULL;
}

//=========================================================================

template <s32 Count>
static
ingame_pad::logical_id FindRegisteredLogicalID( const logical_definition ( &Definitions )[Count], const char* pName )
{
    ASSERT( pName );

    if( !pName )
        return ingame_pad::ACTION_NULL;

    for( s32 i = 0; i < Count; i++ )
    {
        if( x_stricmp( pName, Definitions[i].pName ) == 0 )
            return static_cast<ingame_pad::logical_id>( Definitions[i].LogicalID );
    }

    return ingame_pad::ACTION_NULL;
}

//=========================================================================

static
xbool PromptTokenMatches( const xwchar* pToken, const char* pReference )
{
    ASSERT( pToken );
    ASSERT( pReference );

    if( !pToken || !pReference )
        return FALSE;

    while( *pReference )
    {
        if( *pToken == 0 )
            return FALSE;

        if( (char)*pToken != *pReference )
            return FALSE;

        pToken++;
        pReference++;
    }

    return ( *pToken == 0 );
}

//=========================================================================

static
xbool IsPromptTokenComposite( const xwchar* pToken )
{
    if( !pToken )
        return FALSE;

    return ( PromptTokenMatches( pToken, "UPDOWN" ) || PromptTokenMatches( pToken, "LEFTRIGHT" ) || PromptTokenMatches( pToken, "LEAN" ) );
}

//=========================================================================

static
const prompt_definition* FindPromptToken( const xwchar* pToken )
{
    if( !pToken )
        return NULL;

    for( s32 i = 0; i < CountOf( s_PromptDefinitions ); i++ )
    {
        if( PromptTokenMatches( pToken, s_PromptDefinitions[i].pToken ) )
            return &s_PromptDefinitions[i];
    }

    return NULL;
}

//=========================================================================

template <s32 Count>
static
const mapping_definition* FindLogicalMapping( const mapping_definition ( &Mappings )[Count], s32 LogicalID, input_platform Platform )
{
    for( s32 i = 0; i < Count; i++ )
    {
        if( ( Mappings[i].LogicalID == LogicalID ) &&
            ( Mappings[i].GadgetID != INPUT_UNDEFINED ) &&
            ( input_system::GetGadgetPlatform( Mappings[i].GadgetID ) == Platform ) )
        {
            return &Mappings[i];
        }
    }

    return NULL;
}

//=========================================================================

static
const mapping_definition* FindPromptLogicalMapping( const prompt_definition& Prompt, input_platform Platform )
{
    if( Prompt.Map == prompt_map::Frontend )
    {
        return FindLogicalMapping( s_FrontendMappings, Prompt.LogicalID, Platform );
    }

    if( Prompt.Map == prompt_map::System )
    {
        return FindLogicalMapping( s_SystemMappings, Prompt.LogicalID, Platform );
    }

    return FindLogicalMapping( s_GameplayMappings, Prompt.LogicalID, Platform );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

ingame_pad::ingame_pad( void )
{
    SetActiveContext( INGAME_CONTEXT );
    OnInitialize();
}

//=========================================================================

ingame_pad::logical& ingame_pad::GetFrameLogical( s32 I )
{
    return GetActionState( I );
}

//=========================================================================

const ingame_pad::logical& ingame_pad::GetFrameLogical( s32 I ) const
{
    return GetActionState( I );
}

//=========================================================================

void ingame_pad::Sample( input_snapshot const& Snapshot, f32 DeltaTime )
{
    if( !HasDeviceID() )
    {
        ClearAllActions();
        return;
    }

    input_action_map::Sample( Snapshot, DeltaTime );
}

//=========================================================================

void ingame_pad::OnActionValue( s32 ActionID, f32 Value )
{
    (void)ActionID;
    RecordDemoInputActivity( Value );
}

//=========================================================================

void ingame_pad::OnInitialize( void )
{
    ASSERT( CountOf( s_GameplayLogicals ) == ( GAMEPLAY_ACTION_END - GAMEPLAY_ACTION_FIRST ) );

    SetActionCount( MAX_ACTION );

    RegisterLogicalNames( *this, s_GameplayLogicals );

    RegisterInputMappings( *this, s_GameplayMappings );

    keyboard_bindings Bindings;
    ResetKeyboardBindings( Bindings );
    ApplyKeyboardBindings( Bindings );
}

//=========================================================================

const char* ingame_pad::GetLogicalIDName( s32 Index )
{
    const char* pName = FindRegisteredLogicalName( s_GameplayLogicals, Index );

    if( pName )
        return pName;

    ASSERTS( 0, "Unknown logical input id" );
    return "";
}

//===========================================================================

const char* ingame_pad::GetLogicalIDEnum( void )
{
    static xarray<char> s_Enum;

    if( s_Enum.GetCount() )
        return s_Enum.GetPtr();

    for( s32 i = 0; i < CountOf( s_GameplayLogicals ); i++ )
    {
        const char* pName  = s_GameplayLogicals[i].pName;
        const s32   Length = x_strlen( pName );

        for( s32 j = 0; j <= Length; j++ )
        {
            s_Enum.Append( pName[j] );
        }
    }

    s_Enum.Append( 0 );
    return s_Enum.GetPtr();
}

//=========================================================================

ingame_pad::logical_id ingame_pad::GetLogicalIDByName( const char* pName )
{
    return FindRegisteredLogicalID( s_GameplayLogicals, pName );
}

//=========================================================================

void ingame_pad::ResetKeyboardBindings( keyboard_bindings& Bindings )
{
    static_assert( sizeof( s_KeyboardActions ) / sizeof( s_KeyboardActions[0] ) == KEYBOARD_ACTION_COUNT );

    x_memset( &Bindings, 0, sizeof( Bindings ) );

    for( s32 Action = 0; Action < KEYBOARD_ACTION_COUNT; Action++ )
        Bindings.Keys[Action][0] = s_KeyboardActions[Action].DefaultKey;
}

//=========================================================================

xbool ingame_pad::IsKeyboardKey( input_gadget Key )
{
    for( const keyboard_key_definition& Definition : s_KeyboardKeys )
        if( Definition.Key == Key )
            return TRUE;

    return FALSE;
}

//=========================================================================

xbool ingame_pad::KeyboardBindingsAreValid( const keyboard_bindings& Bindings )
{
    for( s32 Action = 0; Action < KEYBOARD_ACTION_COUNT; Action++ )
        for( s32 Slot = 0; Slot < KEYBOARD_SLOT_COUNT; Slot++ )
        {
            input_gadget Key = (input_gadget)Bindings.Keys[Action][Slot];

            if( ( Key != INPUT_UNDEFINED ) && !IsKeyboardKey( Key ) )
                return FALSE;
        }

    return TRUE;
}

//=========================================================================

const char* ingame_pad::GetKeyboardActionLabel( s32 Action )
{
    ASSERT( ( Action >= 0 ) && ( Action < KEYBOARD_ACTION_COUNT ) );
    return s_KeyboardActions[Action].pLabel;
}

//=========================================================================

xwstring ingame_pad::GetKeyboardKeyLabel( input_gadget Key )
{
    if( Key == INPUT_UNDEFINED )
        return xwstring( g_StringTableMgr( "ui", "IDS_BIND_UNBOUND" ) );

    for( const keyboard_key_definition& Definition : s_KeyboardKeys )
        if( Definition.Key == Key )
            return xwstring( g_StringTableMgr( "ui", Definition.pLabel ) );

    ASSERT( FALSE );
    return xwstring( "" );
}

//=========================================================================

xbool ingame_pad::HasKeyboardConflict( const keyboard_bindings& Bindings, s32 Action, input_gadget Key )
{
    if( Key == INPUT_UNDEFINED )
        return FALSE;

    for( s32 Other = 0; Other < KEYBOARD_ACTION_COUNT; Other++ )
        for( s32 Slot = 0; Slot < KEYBOARD_SLOT_COUNT; Slot++ )
            if( ( Other != Action ) && ( Bindings.Keys[Other][Slot] == Key ) )
                return TRUE;

    return FALSE;
}

//=========================================================================

void ingame_pad::SetKeyboardBinding( keyboard_bindings& Bindings, s32 Action, s32 Slot, input_gadget Key, xbool Replace )
{
    ASSERT( ( Action >= 0 ) && ( Action < KEYBOARD_ACTION_COUNT ) );
    ASSERT( ( Slot >= 0 ) && ( Slot < KEYBOARD_SLOT_COUNT ) );
    ASSERT( ( Key == INPUT_UNDEFINED ) || IsKeyboardKey( Key ) );

    if( ( Action < 0 ) || ( Action >= KEYBOARD_ACTION_COUNT ) ||
        ( Slot < 0 ) || ( Slot >= KEYBOARD_SLOT_COUNT ) ||
        ( ( Key != INPUT_UNDEFINED ) && !IsKeyboardKey( Key ) ) )
        return;

    // Replacing a binding removes the key from other actions before assigning it.
    if( Replace && ( Key != INPUT_UNDEFINED ) )
        for( s32 Other = 0; Other < KEYBOARD_ACTION_COUNT; Other++ )
            for( s32 OtherSlot = 0; OtherSlot < KEYBOARD_SLOT_COUNT; OtherSlot++ )
                if( ( Other != Action ) && ( Bindings.Keys[Other][OtherSlot] == Key ) )
                    Bindings.Keys[Other][OtherSlot] = INPUT_UNDEFINED;

    Bindings.Keys[Action][Slot] = Key;
}

//=========================================================================

void ingame_pad::ApplyKeyboardBindings( const keyboard_bindings& Bindings )
{
    ASSERT( KeyboardBindingsAreValid( Bindings ) );

    if( !KeyboardBindingsAreValid( Bindings ) )
        return;

    // Replace only keyboard bindings; gamepad mappings remain registered.
    for( s32 Action = 0; Action < KEYBOARD_ACTION_COUNT; Action++ )
    {
        s32 LogicalID = s_KeyboardActions[Action].LogicalID;
        RemoveBindings( LogicalID, INPUT_DEVICE_KEYBOARD );

        for( s32 Slot = 0; Slot < KEYBOARD_SLOT_COUNT; Slot++ )
        {
            input_gadget Key = (input_gadget)Bindings.Keys[Action][Slot];

            if( Key != INPUT_UNDEFINED )
                AddBinding( LogicalID, Key, 1.0f, INGAME_CONTEXT );
        }
    }

    // Do not carry held or pressed state across a binding change.
    ClearAllActions();
}

//=========================================================================

system_pad::system_pad( void )
{
    SetActiveContext( INGAME_CONTEXT );
    OnInitialize();
}

//=========================================================================

const input_action_state& system_pad::GetFrameLogical( s32 I ) const
{
    return GetActionState( I );
}

//=========================================================================

void system_pad::OnInitialize( void )
{
    SetActionCount( MAX_ACTION );
    SetActionName( PAUSE, "Pause" );
    RegisterInputMappings( *this, s_SystemMappings );
}

//=========================================================================

void system_pad::OnActionValue( s32 ActionID, f32 Value )
{
    (void)ActionID;
    RecordDemoInputActivity( Value );
}

//=========================================================================

frontend_pad::frontend_pad( void )
{
    SetActiveContext( FRONTEND_CONTEXT );
    OnInitialize();
}

//=========================================================================

frontend_pad::logical& frontend_pad::GetFrameLogical( s32 I )
{
    return GetActionState( I );
}

//=========================================================================

const frontend_pad::logical& frontend_pad::GetFrameLogical( s32 I ) const
{
    return GetActionState( I );
}

//=========================================================================

void frontend_pad::OnInitialize( void )
{
    ASSERT( CountOf( s_FrontendLogicals ) == UI_HELP + 1 );
#if defined( ENABLE_DEBUG_MENU )
    ASSERT( CountOf( s_DebugMenuLogicals ) == ( MAX_ACTION - DEBUG_MENU_NEXT_PAGE ) );
#endif

    SetActionCount( MAX_ACTION );

    RegisterLogicalNames( *this, s_FrontendLogicals );
    RegisterInputMappings( *this, s_FrontendMappings );

#if defined( ENABLE_DEBUG_MENU )
    RegisterLogicalNames( *this, s_DebugMenuLogicals );
    RegisterInputMappings( *this, s_DebugMenuMappings );
#endif
}

//=========================================================================

void frontend_pad::OnActionValue( s32 ActionID, f32 Value )
{
    (void)ActionID;
    RecordDemoInputActivity( Value );
}

//=========================================================================

frontend_input::frontend_input( void )
{
    for( s32 i = 0; i < MAX_CONTROLLERS; i++ )
    {
        m_Pads[i].SetDeviceID( i );
    }
}

//=========================================================================

void frontend_input::Update( f32 DeltaTime )
{
    g_Input.CaptureFrameInput();
    SampleFrame( g_Input.GetFrameSnapshot(), DeltaTime, FRONTEND_CONTEXT );
}

//=========================================================================

void frontend_input::SampleFrame( input_snapshot const& Snapshot, f32 DeltaTime, u32 Context )
{
    for( s32 i = 0; i < MAX_CONTROLLERS; i++ )
    {
        m_Pads[i].SetActiveContext( Context );
        m_Pads[i].Sample( Snapshot, DeltaTime );
    }
}

//=========================================================================

frontend_pad& frontend_input::GetPad( s32 ControllerID )
{
    ASSERT( ( ControllerID >= 0 ) && ( ControllerID < MAX_CONTROLLERS ) );
    return m_Pads[ControllerID];
}

//=========================================================================

const frontend_pad& frontend_input::GetPad( s32 ControllerID ) const
{
    ASSERT( ( ControllerID >= 0 ) && ( ControllerID < MAX_CONTROLLERS ) );
    return m_Pads[ControllerID];
}

//=========================================================================

game_input::game_input( void )
{
    for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
    {
        m_SystemPads[i].SetDeviceID( i );
    }
}

//=========================================================================

xbool game_input::UpdateFrame( f32 DeltaTime, u32 Context )
{
    xbool const           ExitRequested = g_Input.CaptureFrameInput();
    input_snapshot const& Snapshot      = g_Input.GetFrameSnapshot();

    // All action maps consume the same captured snapshot for this frame.
    for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
    {
        m_Players[i].SetActiveContext( Context );
        m_Players[i].Sample( Snapshot, DeltaTime );
    }

    for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
    {
        m_SystemPads[i].SetActiveContext( Context );
        m_SystemPads[i].Sample( Snapshot, DeltaTime );
    }

    g_FrontendInput.SampleFrame( Snapshot, DeltaTime, Context );
    return ExitRequested;
}

//=========================================================================

void game_input::ClearInput( void )
{
    for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
    {
        m_Players[i].ClearAllActions();
    }
}

//=========================================================================

const ingame_pad& game_input::GetPlayer( s32 PlayerIndex ) const
{
    ASSERT( ( PlayerIndex >= 0 ) && ( PlayerIndex < MAX_LOCAL_PLAYERS ) );
    return m_Players[PlayerIndex];
}

//=========================================================================

const ingame_pad& game_input::operator[]( s32 PlayerIndex ) const
{
    return GetPlayer( PlayerIndex );
}

//=========================================================================

void game_input::ApplyKeyboardBindings( s32 PlayerIndex, const ingame_pad::keyboard_bindings& Bindings )
{
    ASSERT( ( PlayerIndex >= 0 ) && ( PlayerIndex < MAX_LOCAL_PLAYERS ) );

    if( ( PlayerIndex >= 0 ) && ( PlayerIndex < MAX_LOCAL_PLAYERS ) )
        m_Players[PlayerIndex].ApplyKeyboardBindings( Bindings );
}

//=========================================================================

void game_input::SetPlayerDevice( s32 PlayerIndex, s32 DeviceID )
{
    ASSERT( ( PlayerIndex >= 0 ) && ( PlayerIndex < MAX_LOCAL_PLAYERS ) );
    ASSERT( ( DeviceID >= 0 ) && ( DeviceID < frontend_input::MAX_CONTROLLERS ) );
    m_Players[PlayerIndex].SetDeviceID( DeviceID );
}

//=========================================================================

s32 game_input::GetPlayerDevice( s32 PlayerIndex ) const
{
    return GetPlayer( PlayerIndex ).GetDeviceID();
}

//=========================================================================

void game_input::ClearPlayerDevice( s32 PlayerIndex )
{
    ASSERT( ( PlayerIndex >= 0 ) && ( PlayerIndex < MAX_LOCAL_PLAYERS ) );
    m_Players[PlayerIndex].SetDeviceID( -1 );
    m_Players[PlayerIndex].ClearAllActions();
}

//=========================================================================

void game_input::ClearPlayerDevices( void )
{
    for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
    {
        ClearPlayerDevice( i );
    }
}

//=========================================================================

void game_input::ClearPlayerActions( s32 PlayerIndex )
{
    ASSERT( ( PlayerIndex >= 0 ) && ( PlayerIndex < MAX_LOCAL_PLAYERS ) );
    m_Players[PlayerIndex].ClearAllActions();
}

//=========================================================================

s32 game_input::GetPauseController( void ) const
{
    for( s32 i = 0; i < MAX_LOCAL_PLAYERS; i++ )
    {
        s32 const DeviceID = GetPlayerDevice( i );

        if( ( DeviceID >= 0 ) && ( DeviceID < frontend_input::MAX_CONTROLLERS ) &&
            ( m_SystemPads[DeviceID].GetFrameLogical( system_pad::PAUSE ).GetWasValue() > 0.25f ) )
        {
            return DeviceID;
        }
    }

    return -1;
}

//=========================================================================

input_gadget input_GetPromptGadget( const xwchar* pToken, input_platform Platform )
{
    ASSERT( pToken );

    if( !pToken )
        return INPUT_UNDEFINED;

    const prompt_definition* pPrompt = FindPromptToken( pToken );

    if( !pPrompt || IsPromptTokenComposite( pToken ) )
        return INPUT_UNDEFINED;

    if( ( Platform != INPUT_PLATFORM_XBOX ) && ( Platform != INPUT_PLATFORM_PS2 ) )
        return INPUT_UNDEFINED;

    const mapping_definition* pMapping = FindPromptLogicalMapping( *pPrompt, Platform );
    return pMapping ? pMapping->GadgetID : INPUT_UNDEFINED;
}

//=========================================================================
