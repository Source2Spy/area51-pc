//=========================================================================
//
//  PlayerInput.hpp
//
//=========================================================================

#ifndef PLAYER_INPUT_HPP
#define PLAYER_INPUT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Input/e_InputActions.hpp"

class ingame_pad;

//=========================================================================
//  TYPES
//=========================================================================

enum class PlayerAction
{
    MoveForward = 0,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    LookHorizontal,
    LookVertical,
    Jump,
    Crouch,
    PrimaryFire,
    SecondaryFire,
    Reload,
    Mutation,
    CycleWeaponRight,
    Use,
    Flashlight,
    SpeakFollowStay,
    SpeakUseActivate,
    SpeakCoverMe,
    SpeakAttackCover,
    ThrowGrenade,
    MeleeAttack,
    CycleWeaponLeft,
    SwitchToScanner,
    SwitchToPistol,
    SwitchToSmp,
    SwitchToShotgun,
    SwitchToSniperRifle,
    SwitchToBbg,
    SwitchToMesonCannon,
    VoteMenuOn,
    VoteMenuOff,
    VoteYes,
    VoteNo,
    VoteAbstain,
    Chat,
    LeanLeft,
    LeanRight,
    TalkModeToggle,
    FireParasites,
    FireContagion,
    DropFlag,
    Scoreboard,
    Count
};

//-------------------------------------------------------------------------

struct PlayerActionState
{
                        PlayerActionState   ( void );

    f32                 Value;
    f32                 PressedValue;
    f32                 GamepadValue;
    f32                 GamepadPressedValue;
    xbool               IsHeld;
    xbool               WasPressed;
    xbool               WasReleased;
    xbool               GamepadIsHeld;
    xbool               GamepadWasPressed;
};

//-------------------------------------------------------------------------

struct PlayerMoveInput
{
                        PlayerMoveInput     ( void );

    f32                 KeyboardForward;
    f32                 KeyboardStrafe;
    f32                 GamepadForward;
    f32                 GamepadStrafe;
};

//-------------------------------------------------------------------------

struct PlayerLookInput
{
                        PlayerLookInput     ( void );

    f32                 MouseYaw;
    f32                 MousePitch;
    f32                 GamepadYaw;
    f32                 GamepadPitch;
};

//-------------------------------------------------------------------------

struct PlayerInputState
{
                        PlayerInputState    ( void );

    void                Clear               ( void );
    PlayerActionState const& GetAction       ( PlayerAction Action ) const;
    xbool               IsHeld              ( PlayerAction Action ) const;
    xbool               WasPressed          ( PlayerAction Action ) const;
    xbool               WasReleased         ( PlayerAction Action ) const;

    PlayerMoveInput     Move;
    PlayerLookInput     Look;

private:

    friend class PlayerInput;
    friend class PlayerInputStateBuilder;

    PlayerActionState   m_Actions[static_cast<s32>( PlayerAction::Count )];
};

//-------------------------------------------------------------------------

// Deterministic construction API used by replays and headless policy tests.
class PlayerInputStateBuilder
{
public:

                        PlayerInputStateBuilder ( void );

    void                Clear                   ( void );
    void                SetAction               ( PlayerAction Action,
                                                   const PlayerActionState& State );
    void                SetMove                 ( const PlayerMoveInput& Move );
    void                SetLook                 ( const PlayerLookInput& Look );
    PlayerInputState const& GetState             ( void ) const;

private:

    PlayerInputState    m_State;
};

//=========================================================================
//  PLAYER INPUT
//=========================================================================

class PlayerInput
{
public:

                        PlayerInput             ( void );

    void                Clear                   ( void );
    void                Sample                  ( ingame_pad const& Input );

    PlayerInputState const& GetState             ( void ) const;

private:

    static f32          GetCombinedActionValue  ( input_action_state const& Action );
    static f32          GetCombinedPressedValue ( input_action_state const& Action );
    static void         SampleKeyboardMove      ( input_action_state const& Forward,
                                                  input_action_state const& Backward,
                                                  input_action_state const& Left,
                                                  input_action_state const& Right,
                                                  PlayerMoveInput& Move );
    static void         SampleGamepadMove       ( input_action_state const& Forward,
                                                  input_action_state const& Backward,
                                                  input_action_state const& Left,
                                                  input_action_state const& Right,
                                                  PlayerMoveInput& Move );
    static void         SampleMouseLook         ( input_action_state const& Horizontal,
                                                  input_action_state const& Vertical,
                                                  PlayerLookInput& Look );
    static void         SampleGamepadLook       ( input_action_state const& Horizontal,
                                                  input_action_state const& Vertical,
                                                  PlayerLookInput& Look );
    static void         SampleLook              ( input_action_state const& Horizontal,
                                                  input_action_state const& Vertical,
                                                  PlayerLookInput& Look );

private:

    PlayerInputState    m_State;
    xbool               m_PreviousHeld[static_cast<s32>( PlayerAction::Count )];
};

//=========================================================================
#endif // PLAYER_INPUT_HPP
//=========================================================================
