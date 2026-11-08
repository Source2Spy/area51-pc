//=========================================================================
//
//  GamePad.hpp
//
//=========================================================================

#ifndef GAME_PAD_HPP
#define GAME_PAD_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "Entropy.hpp"
#include "../../Apps/GameApp/Config.hpp"

#include "../Menu/DebugMenuDefine.hpp"
#include "../Objects/Player/PlayerDefines.hpp"

//=========================================================================
// DEFINES
//=========================================================================

enum input_context
{
    INGAME_CONTEXT     = (1 << 0),
    FRONTEND_CONTEXT   = (1 << 1),
#if defined( ENABLE_DEBUG_MENU )
    DEBUG_MENU_CONTEXT = (1 << 2),
#endif
    ALL_CONTEXTS       = INGAME_CONTEXT | FRONTEND_CONTEXT
#if defined( ENABLE_DEBUG_MENU )
                       | DEBUG_MENU_CONTEXT
#endif
};

//=========================================================================
// CLASSES
//=========================================================================

class ingame_pad : public input_action_map
{
public:

    typedef input_action_state logical;

    enum logical_id
    {
        ACTION_NULL = -1,

        GAMEPLAY_ACTION_FIRST = 0,
        MOVE_FORWARD = GAMEPLAY_ACTION_FIRST,
        MOVE_BACKWARD,
        STRAFE_LEFT,
        STRAFE_RIGHT,

        LOOK_HORIZONTAL,
        LOOK_VERTICAL,
        ACTION_JUMP,
        ACTION_CROUCH,
        ACTION_PRIMARY,
        ACTION_SECONDARY,
        ACTION_RELOAD,
        ACTION_MUTATION,
        ACTION_CYCLE_RIGHT,
        ACTION_USE,
        ACTION_FLASHLIGHT,

        ACTION_SPEAK_FOLLOW_STAY,
        ACTION_SPEAK_USE_ACTIVATE,
        ACTION_SPEAK_COVER_ME,
        ACTION_SPEAK_ATTACK_COVER,

        ACTION_THROW_GRENADE,
        ACTION_MELEE_ATTACK,
        ACTION_CYCLE_LEFT,
        ACTION_SWITCH_TO_SCANNER,
        ACTION_SWITCH_TO_PISTOL,
        ACTION_SWITCH_TO_SMP,
        ACTION_SWITCH_TO_SHOTGUN,
        ACTION_SWITCH_TO_SNIPER_RIFLE,
        ACTION_SWITCH_TO_BBG,
        ACTION_SWITCH_TO_MESON_CANNON,

        ACTION_VOTE_MENU_ON,
        ACTION_VOTE_MENU_OFF,
        ACTION_VOTE_YES,
        ACTION_VOTE_NO,
        ACTION_VOTE_ABSTAIN,

        ACTION_CHAT,

        LEAN_LEFT,
        LEAN_RIGHT,

        ACTION_TALK_MODE_TOGGLE,
        ACTION_FIRE_PARASITES,
        ACTION_FIRE_CONTAGION,

        ACTION_DROP_FLAG,
        ACTION_SCOREBOARD,

        GAMEPLAY_ACTION_END,
        MAX_ACTION = GAMEPLAY_ACTION_END
    };

    // Persisted profile order. Changes require a new save format.
    enum keyboard_action
    {
        KEY_MOVE_FORWARD = 0,
        KEY_MOVE_BACKWARD = 1,
        KEY_STRAFE_LEFT = 2,
        KEY_STRAFE_RIGHT = 3,
        KEY_ACTION_JUMP = 4,
        KEY_ACTION_CROUCH = 5,
        KEY_ACTION_RELOAD = 6,
        KEY_ACTION_MUTATION = 7,
        KEY_ACTION_USE = 8,
        KEY_ACTION_FLASHLIGHT = 9,
        KEY_ACTION_THROW_GRENADE = 10,
        KEY_ACTION_MELEE_ATTACK = 11,
        KEY_ACTION_SWITCH_TO_SCANNER = 12,
        KEY_ACTION_SWITCH_TO_PISTOL = 13,
        KEY_ACTION_SWITCH_TO_SMP = 14,
        KEY_ACTION_SWITCH_TO_SHOTGUN = 15,
        KEY_ACTION_SWITCH_TO_SNIPER_RIFLE = 16,
        KEY_ACTION_SWITCH_TO_BBG = 17,
        KEY_ACTION_SWITCH_TO_MESON_CANNON = 18,
        KEY_ACTION_VOTE_MENU_ON = 19,
        KEY_ACTION_VOTE_MENU_OFF = 20,
        KEY_ACTION_VOTE_YES = 21,
        KEY_ACTION_VOTE_NO = 22,
        KEY_ACTION_VOTE_ABSTAIN = 23,
        KEY_ACTION_CHAT = 24,
        KEY_LEAN_LEFT = 25,
        KEY_LEAN_RIGHT = 26,
        KEY_ACTION_TALK_MODE_TOGGLE = 27,
        KEY_ACTION_DROP_FLAG = 28,
        KEY_ACTION_SCOREBOARD = 29,
        KEYBOARD_ACTION_COUNT = 30,
        KEYBOARD_SLOT_COUNT = 2,
    };

    struct keyboard_bindings
    {
        s32 Keys[KEYBOARD_ACTION_COUNT][KEYBOARD_SLOT_COUNT];
    };

    //-------------------------------------------------------------------------
    //  KEYBOARD BINDINGS
    //-------------------------------------------------------------------------

    static void         ResetKeyboardBindings   ( keyboard_bindings& Bindings );
    static xbool        KeyboardBindingsAreValid( const keyboard_bindings& Bindings );
    static xbool        IsKeyboardKey           ( input_gadget Key );
    static const char*  GetKeyboardActionLabel  ( s32 Action );
    static xwstring     GetKeyboardKeyLabel     ( input_gadget Key );
    static xbool        HasKeyboardConflict     ( const keyboard_bindings& Bindings, s32 Action, input_gadget Key );
    static void         SetKeyboardBinding      ( keyboard_bindings& Bindings, s32 Action, s32 Slot, input_gadget Key, xbool Replace );
    void                ApplyKeyboardBindings   ( const keyboard_bindings& Bindings );

    //-------------------------------------------------------------------------
    //  ACTION MAP
    //-------------------------------------------------------------------------

                        ingame_pad      ( void );

    logical&            GetFrameLogical ( s32 I );
    const logical&      GetFrameLogical ( s32 I ) const;

    virtual void        Sample          ( input_snapshot const& Snapshot, f32 DeltaTime );

    static  const char* GetLogicalIDName    ( s32 Index );
    static  const char* GetLogicalIDEnum    ( void );
    static  logical_id  GetLogicalIDByName  ( const char* pName );

protected:

    void                OnInitialize    ( void );
    virtual void        OnActionValue   ( s32 ActionID, f32 Value );
};

//=========================================================================

class system_pad : public input_action_map
{
public:

    enum logical_id
    {
        PAUSE = 0,

        MAX_ACTION,
    };

                        system_pad      ( void );

    const input_action_state& GetFrameLogical( s32 I ) const;

private:

    void                OnInitialize    ( void );
    virtual void        OnActionValue   ( s32 ActionID, f32 Value );
};

//=========================================================================

class frontend_pad : public input_action_map
{
public:

    using logical = input_action_state;

    enum logical_id
    {
        UI_UP = 0,
        UI_DOWN,
        UI_LEFT,
        UI_RIGHT,
        UI_SELECT,
        UI_BACK,
        UI_DELETE,
        UI_ACTIVATE,
        UI_SHOULDER_L,
        UI_SHOULDER_R,
        UI_SHOULDER_L2,
        UI_SHOULDER_R2,
        UI_HELP,

#if defined( ENABLE_DEBUG_MENU )
        DEBUG_MENU_NEXT_PAGE,
        DEBUG_MENU_PREV_PAGE,
        DEBUG_MENU_NEXT_ITEM,
        DEBUG_MENU_PREV_ITEM,
        DEBUG_MENU_INCREMENT,
        DEBUG_MENU_DECREMENT,
        DEBUG_MENU_ACTION,
        DEBUG_MENU_EXIT_MODIFIER,
        DEBUG_MENU_EXIT,
#endif

        MAX_ACTION,
    };

                        frontend_pad    ( void );

    logical&            GetFrameLogical ( s32 I );
    const logical&      GetFrameLogical ( s32 I ) const;

private:

    void                OnInitialize    ( void );
    virtual void        OnActionValue   ( s32 ActionID, f32 Value );
};

//=========================================================================

class frontend_input
{
public:

    enum
    {
        MAX_CONTROLLERS = 4,
    };

                        frontend_input  ( void );

    void                Update          ( f32 DeltaTime );
    void                SampleFrame     ( input_snapshot const& Snapshot, f32 DeltaTime, u32 Context );
    frontend_pad&       GetPad          ( s32 ControllerID );
    const frontend_pad& GetPad          ( s32 ControllerID ) const;

private:

    frontend_pad        m_Pads[MAX_CONTROLLERS];
};

//=========================================================================

class game_input
{
public:

                        game_input          ( void );

    xbool               UpdateFrame         ( f32 DeltaTime, u32 Context );
    void                ClearInput          ( void );

    const ingame_pad&   GetPlayer           ( s32 PlayerIndex ) const;
    const ingame_pad&   operator[]          ( s32 PlayerIndex ) const;
    void                SetPlayerDevice     ( s32 PlayerIndex, s32 DeviceID );
    s32                 GetPlayerDevice     ( s32 PlayerIndex ) const;
    void                ClearPlayerDevice   ( s32 PlayerIndex );
    void                ClearPlayerDevices  ( void );
    void                ClearPlayerActions  ( s32 PlayerIndex );

    void                ApplyKeyboardBindings( s32 PlayerIndex, const ingame_pad::keyboard_bindings& Bindings );

    s32                 GetPauseController  ( void ) const;

private:

    ingame_pad          m_Players[MAX_LOCAL_PLAYERS];
    system_pad          m_SystemPads[frontend_input::MAX_CONTROLLERS];
};

//=========================================================================
// FUNCTIONS
//=========================================================================

input_gadget input_GetPromptGadget( const xwchar* pToken, input_platform Platform );

//=========================================================================
// GLOBAL STATE
//=========================================================================

extern frontend_input g_FrontendInput;
extern game_input     g_GameInput;

//=========================================================================
#endif // GAME_PAD_HPP
//=========================================================================
