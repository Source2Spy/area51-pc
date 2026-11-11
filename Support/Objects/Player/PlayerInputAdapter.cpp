//=========================================================================
//
//  PlayerInputAdapter.cpp
//
//=========================================================================

#include "PlayerInput.hpp"
#include "InputMgr/GamePad.hpp"

//=========================================================================
//  ACTION MAPPING
//=========================================================================

static const ingame_pad::logical_id s_PlayerActionToLogical[] =
{
    ingame_pad::MOVE_FORWARD,
    ingame_pad::MOVE_BACKWARD,
    ingame_pad::STRAFE_LEFT,
    ingame_pad::STRAFE_RIGHT,
    ingame_pad::LOOK_HORIZONTAL,
    ingame_pad::LOOK_VERTICAL,
    ingame_pad::ACTION_JUMP,
    ingame_pad::ACTION_CROUCH,
    ingame_pad::ACTION_PRIMARY,
    ingame_pad::ACTION_SECONDARY,
    ingame_pad::ACTION_RELOAD,
    ingame_pad::ACTION_MUTATION,
    ingame_pad::ACTION_CYCLE_RIGHT,
    ingame_pad::ACTION_USE,
    ingame_pad::ACTION_FLASHLIGHT,
    ingame_pad::ACTION_SPEAK_FOLLOW_STAY,
    ingame_pad::ACTION_SPEAK_USE_ACTIVATE,
    ingame_pad::ACTION_SPEAK_COVER_ME,
    ingame_pad::ACTION_SPEAK_ATTACK_COVER,
    ingame_pad::ACTION_THROW_GRENADE,
    ingame_pad::ACTION_MELEE_ATTACK,
    ingame_pad::ACTION_CYCLE_LEFT,
    ingame_pad::ACTION_SWITCH_TO_SCANNER,
    ingame_pad::ACTION_SWITCH_TO_PISTOL,
    ingame_pad::ACTION_SWITCH_TO_SMP,
    ingame_pad::ACTION_SWITCH_TO_SHOTGUN,
    ingame_pad::ACTION_SWITCH_TO_SNIPER_RIFLE,
    ingame_pad::ACTION_SWITCH_TO_BBG,
    ingame_pad::ACTION_SWITCH_TO_MESON_CANNON,
    ingame_pad::ACTION_VOTE_MENU_ON,
    ingame_pad::ACTION_VOTE_MENU_OFF,
    ingame_pad::ACTION_VOTE_YES,
    ingame_pad::ACTION_VOTE_NO,
    ingame_pad::ACTION_VOTE_ABSTAIN,
    ingame_pad::ACTION_CHAT,
    ingame_pad::LEAN_LEFT,
    ingame_pad::LEAN_RIGHT,
    ingame_pad::ACTION_TALK_MODE_TOGGLE,
    ingame_pad::ACTION_FIRE_PARASITES,
    ingame_pad::ACTION_FIRE_CONTAGION,
    ingame_pad::ACTION_DROP_FLAG,
    ingame_pad::ACTION_SCOREBOARD,
};

static_assert( (sizeof( s_PlayerActionToLogical ) / sizeof( s_PlayerActionToLogical[0] )) ==
               static_cast<s32>( PlayerAction::Count ),
               "Every PlayerAction must have an explicit logical mapping" );

//=========================================================================

static ingame_pad::logical_id GetLogicalID( PlayerAction Action )
{
    s32 const Index = static_cast<s32>( Action );
    ASSERT( (Index >= 0) && (Index < static_cast<s32>( PlayerAction::Count )) );
    return s_PlayerActionToLogical[Index];
}

//=========================================================================

static input_action_state const& GetLogical( ingame_pad const& Input, PlayerAction Action )
{
    return Input.GetFrameLogical( GetLogicalID( Action ) );
}

//=========================================================================

PlayerInput::PlayerInput( void )
{
    Clear();
}

//=========================================================================

void PlayerInput::Clear( void )
{
    m_State.Clear();

    for( s32 i = 0; i < static_cast<s32>( PlayerAction::Count ); i++ )
        m_PreviousHeld[i] = FALSE;
}

//=========================================================================

f32 PlayerInput::GetCombinedActionValue( input_action_state const& Action )
{
    f32 Value = Action.GetIsValue( INPUT_DEVICE_KEYBOARD );

    f32 const MouseValue = Action.GetIsValue( INPUT_DEVICE_MOUSE );
    if( x_abs( MouseValue ) > x_abs( Value ) )
        Value = MouseValue;

    f32 const GamepadValue = Action.GetIsValue( INPUT_DEVICE_GAMEPAD );
    if( x_abs( GamepadValue ) > x_abs( Value ) )
        Value = GamepadValue;

    return Value;
}

//=========================================================================

f32 PlayerInput::GetCombinedPressedValue( input_action_state const& Action )
{
    f32 Value = Action.GetWasValue( INPUT_DEVICE_KEYBOARD );

    f32 const MouseValue = Action.GetWasValue( INPUT_DEVICE_MOUSE );
    if( x_abs( MouseValue ) > x_abs( Value ) )
        Value = MouseValue;

    f32 const GamepadValue = Action.GetWasValue( INPUT_DEVICE_GAMEPAD );
    if( x_abs( GamepadValue ) > x_abs( Value ) )
        Value = GamepadValue;

    return Value;
}

//=========================================================================

void PlayerInput::SampleKeyboardMove( input_action_state const& Forward,
                                      input_action_state const& Backward,
                                      input_action_state const& Left,
                                      input_action_state const& Right,
                                      PlayerMoveInput&          Move )
{
    Move.KeyboardForward = x_clamp( Forward.GetIsValue( INPUT_DEVICE_KEYBOARD ) -
                                    Backward.GetIsValue( INPUT_DEVICE_KEYBOARD ),
                                    -1.0f,
                                     1.0f );
    Move.KeyboardStrafe = x_clamp( Left.GetIsValue( INPUT_DEVICE_KEYBOARD ) -
                                   Right.GetIsValue( INPUT_DEVICE_KEYBOARD ),
                                   -1.0f,
                                    1.0f );
}

//=========================================================================

void PlayerInput::SampleGamepadMove( input_action_state const& Forward,
                                     input_action_state const& Backward,
                                     input_action_state const& Left,
                                     input_action_state const& Right,
                                     PlayerMoveInput&          Move )
{
    Move.GamepadForward = x_clamp( Forward.GetIsValue( INPUT_DEVICE_GAMEPAD ) -
                                   Backward.GetIsValue( INPUT_DEVICE_GAMEPAD ),
                                   -1.0f,
                                    1.0f );
    Move.GamepadStrafe = x_clamp( Left.GetIsValue( INPUT_DEVICE_GAMEPAD ) -
                                  Right.GetIsValue( INPUT_DEVICE_GAMEPAD ),
                                  -1.0f,
                                   1.0f );
}

//=========================================================================

void PlayerInput::SampleMouseLook( input_action_state const& Horizontal,
                                   input_action_state const& Vertical,
                                   PlayerLookInput&          Look )
{
    Look.MouseYaw   = -Horizontal.GetIsValue( INPUT_DEVICE_MOUSE );
    Look.MousePitch =  Vertical.GetIsValue( INPUT_DEVICE_MOUSE );
}

//=========================================================================

void PlayerInput::SampleGamepadLook( input_action_state const& Horizontal,
                                     input_action_state const& Vertical,
                                     PlayerLookInput&          Look )
{
    Look.GamepadYaw   = -Horizontal.GetIsValue( INPUT_DEVICE_GAMEPAD );
    Look.GamepadPitch =  Vertical.GetIsValue( INPUT_DEVICE_GAMEPAD );
}

//=========================================================================

void PlayerInput::SampleLook( input_action_state const& Horizontal,
                              input_action_state const& Vertical,
                              PlayerLookInput&          Look )
{
    SampleMouseLook( Horizontal, Vertical, Look );
    SampleGamepadLook( Horizontal, Vertical, Look );
}

//=========================================================================

void PlayerInput::Sample( ingame_pad const& Input )
{
    for( s32 i = 0; i < static_cast<s32>( PlayerAction::Count ); i++ )
    {
        PlayerAction const Action = static_cast<PlayerAction>( i );
        input_action_state const& Source = GetLogical( Input, Action );
        PlayerActionState& Destination = m_State.m_Actions[i];

        Destination.Value        = GetCombinedActionValue( Source );
        Destination.PressedValue = GetCombinedPressedValue( Source );
        Destination.GamepadValue = Source.GetIsValue( INPUT_DEVICE_GAMEPAD );
        Destination.GamepadPressedValue = Source.GetWasValue( INPUT_DEVICE_GAMEPAD );
        Destination.IsHeld       = x_abs( Destination.Value ) > 0.25f;
        Destination.WasPressed   = x_abs( Destination.PressedValue ) > 0.25f;
        Destination.WasReleased  = m_PreviousHeld[i] && !Destination.IsHeld;
        Destination.GamepadIsHeld = x_abs( Destination.GamepadValue ) > 0.25f;
        Destination.GamepadWasPressed = x_abs( Destination.GamepadPressedValue ) > 0.25f;
        m_PreviousHeld[i]        = Destination.IsHeld;
    }

    input_action_state const& MoveForward  = GetLogical( Input, PlayerAction::MoveForward );
    input_action_state const& MoveBackward = GetLogical( Input, PlayerAction::MoveBackward );
    input_action_state const& StrafeLeft   = GetLogical( Input, PlayerAction::StrafeLeft );
    input_action_state const& StrafeRight  = GetLogical( Input, PlayerAction::StrafeRight );

    SampleKeyboardMove( MoveForward,
                        MoveBackward,
                        StrafeLeft,
                        StrafeRight,
                        m_State.Move );
    SampleGamepadMove( MoveForward,
                       MoveBackward,
                       StrafeLeft,
                       StrafeRight,
                       m_State.Move );

    SampleLook( GetLogical( Input, PlayerAction::LookHorizontal ),
                GetLogical( Input, PlayerAction::LookVertical ),
                m_State.Look );
}

//=========================================================================

PlayerInputState const& PlayerInput::GetState( void ) const
{
    return m_State;
}
