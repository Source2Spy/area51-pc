//=========================================================================
//
//  PlayerCombatPolicy.cpp
//
//=========================================================================

#include "PlayerCombat.hpp"

//=========================================================================

PlayerCombatInput::PlayerCombatInput( void ) :
    PrimaryHeld      ( FALSE ),
    PrimaryPressed   ( FALSE ),
    SecondaryHeld    ( FALSE ),
    SecondaryPressed ( FALSE ),
    ReloadPressed    ( FALSE ),
    MeleeHeld        ( FALSE ),
    GrenadeHeld      ( FALSE ),
    UsesGamepad      ( FALSE )
{
}

//=========================================================================

PlayerCombatInput PlayerCombat::BuildInput( const PlayerInputState& Input,
                                            xbool                   IsMutated ) const
{
    PlayerCombatInput CombatInput;

    PlayerAction const PrimaryAction = IsMutated
                                     ? PlayerAction::FireParasites
                                     : PlayerAction::PrimaryFire;
    PlayerAction const SecondaryAction = IsMutated
                                       ? PlayerAction::FireContagion
                                       : PlayerAction::SecondaryFire;
    PlayerAction const MeleeAction = PlayerAction::MeleeAttack;

    CombatInput.PrimaryHeld      = Input.IsHeld( PrimaryAction );
    CombatInput.PrimaryPressed   = Input.WasPressed( PrimaryAction );
    CombatInput.SecondaryHeld    = Input.IsHeld( SecondaryAction );
    CombatInput.SecondaryPressed = Input.WasPressed( SecondaryAction );
    CombatInput.ReloadPressed    = Input.WasPressed( PlayerAction::Reload );
    CombatInput.MeleeHeld        = Input.IsHeld( MeleeAction );
    CombatInput.GrenadeHeld      = Input.IsHeld( PlayerAction::ThrowGrenade );
    CombatInput.UsesGamepad      = Input.GetAction( PrimaryAction ).GamepadIsHeld ||
                                   Input.GetAction( SecondaryAction ).GamepadIsHeld ||
                                   Input.GetAction( MeleeAction ).GamepadIsHeld ||
                                   Input.GetAction( PlayerAction::ThrowGrenade ).GamepadIsHeld;

    return CombatInput;
}

//=========================================================================

xbool PlayerCombat::IsAimAssistActive( const PlayerLookSample& Look,
                                       const PlayerInputState& Input,
                                       xbool                   IsMutated ) const
{
    return Look.HasGamepadInput() || BuildInput( Input, IsMutated ).UsesGamepad;
}

//=========================================================================

void PlayerCombat::ApplyAimAssistDampening( PlayerLookSample& Look,
                                            f32               Multiplier ) const
{
    Look.GamepadYaw   *= Multiplier;
    Look.GamepadPitch *= Multiplier;
}
