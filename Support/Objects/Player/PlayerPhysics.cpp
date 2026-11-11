//=========================================================================
//
//  PlayerPhysics.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "Configuration/GameConfig.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"

//=========================================================================
//  DEFINES
//=========================================================================

f32 HumanSpeedFactor  = 1.000f;
f32 MutantSpeedFactor = 1.000f;

tweak_handle MP_RunSpeedFactor_NormalTweak    ( "MP_RunSpeedFactor_Normal" );
tweak_handle MP_RunSpeedFactor_MutantTweak    ( "MP_RunSpeedFactor_Mutant" );

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void player::EvaluateMovementSpeeds( f32 DeltaTime )
{
    PlayerMovementSettings Settings;
    Settings.MaxSpeed           = m_MaxFowardVelocity;
    Settings.Acceleration       = m_fForwardAccel;
    Settings.DecelerationFactor = m_fDecelerationFactor;

    PlayerMovementSpeeds Speeds;
    m_Movement.Evaluate( m_MoveInput, Settings, DeltaTime, Speeds );
    m_fForwardSpeed = Speeds.Forward;
    m_fStrafeSpeed  = Speeds.Strafe;
}

//==============================================================================

void player::ApplyMovementSpeedFactors( void )
{
    f32 SpeedFactor = g_PerceptionMgr.GetForwardSpeedFactor();

    if( g_MPTweaks.Active )
    {
        if( IsMutated() )
        {
            SpeedFactor *= MP_RunSpeedFactor_MutantTweak.GetF32();
            SpeedFactor *= MutantSpeedFactor;
        }
        else
        {
            SpeedFactor *= MP_RunSpeedFactor_NormalTweak.GetF32();
            SpeedFactor *= HumanSpeedFactor;
        }
    }

    m_ForwardVelocity *= SpeedFactor;
    m_StrafeVelocity  *= SpeedFactor;
}

//==============================================================================

void player::CalculateForwardVelocity( const vector3& ViewZ )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "player::CalculateForwardVelocity");

    m_ForwardVelocity = ViewZ * m_fForwardSpeed;
}

//==============================================================================

void player::CalculateStrafeVelocity( const vector3& ViewX )
{
    m_StrafeVelocity = ViewX * m_fStrafeSpeed;
}

//==============================================================================

f32 player::GetSpeed( void )
{
    return m_Physics.GetMotionVelocity().Length();
}

//==============================================================================

f32 player::GetCurrentVelocity( void )
{
    const vector3 Velocity = m_Physics.GetMotionVelocity();
    return x_sqrt( x_sqr( Velocity.GetX() ) + x_sqr( Velocity.GetZ() ) );
}

//==============================================================================

f32 player::GetMaxVelocity( void )
{ 
    return m_MaxFowardVelocity;
}
