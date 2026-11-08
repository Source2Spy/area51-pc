//=========================================================================
//
//  PlayerMovementPolicy.cpp
//
//=========================================================================

#include "PlayerMovement.hpp"

//=========================================================================

PlayerMovementSpeeds::PlayerMovementSpeeds( void ) :
    Forward( 0.0f ),
    Strafe ( 0.0f )
{
}


//=========================================================================

PlayerMovement::PlayerMovement( void )
{
    Clear();
}

//=========================================================================

void PlayerMovement::Clear( void )
{
    m_GamepadVelocity.Zero();
}

//=========================================================================

void PlayerMovement::ShapeGamepadMoveVector( vector2& Move )
{
    f32 Magnitude = Move.Length();
    if( Magnitude <= F32_MIN )
    {
        Move.Zero();
        return;
    }

    if( Magnitude > 1.0f )
    {
        Move /= Magnitude;
        Magnitude = 1.0f;
    }

    f32 const CurvedMagnitude = Magnitude * x_sqrt( Magnitude );
    Move *= CurvedMagnitude / Magnitude;
}

//=========================================================================

void PlayerMovement::BuildInput( PlayerMoveInput const& Source,
                                 PlayerMoveInput&       Input ) const
{
    Input = Source;

    vector2 GamepadMove( Source.GamepadStrafe, Source.GamepadForward );
    ShapeGamepadMoveVector( GamepadMove );
    Input.GamepadStrafe  = GamepadMove.X;
    Input.GamepadForward = GamepadMove.Y;
}

//=========================================================================

void PlayerMovement::BuildKeyboardSpeeds( PlayerMoveInput const& Input,
                                          f32                    MaxSpeed,
                                          PlayerMovementSpeeds& Speeds )
{
    Speeds.Forward = MaxSpeed * Input.KeyboardForward;
    Speeds.Strafe  = MaxSpeed * Input.KeyboardStrafe;
}

//=========================================================================

void PlayerMovement::ApproachGamepadVelocity( vector2 const& TargetVelocity,
                                              f32            Acceleration,
                                              f32            DecelerationFactor,
                                              f32            DeltaTime,
                                              vector2&       Velocity )
{
    Acceleration       = MAX( 0.0f, Acceleration );
    DecelerationFactor = MAX( 0.0f, DecelerationFactor );
    DeltaTime          = MAX( 0.0f, DeltaTime );

    f32 const TargetSpeedSqr = TargetVelocity.LengthSquared();
    f32 const CurrentSpeedSqr = Velocity.LengthSquared();
    xbool const IsDecelerating = (TargetSpeedSqr <= F32_MIN) ||
                                 (TargetSpeedSqr < CurrentSpeedSqr) ||
                                 (v2_Dot( Velocity, TargetVelocity ) <= 0.0f);
    f32 const Rate = Acceleration * (IsDecelerating ? DecelerationFactor : 1.0f);

    vector2 Delta = TargetVelocity - Velocity;
    f32 const Distance = Delta.Length();
    if( Distance <= F32_MIN )
    {
        Velocity = TargetVelocity;
        return;
    }

    f32 const Step = Rate * DeltaTime;
    if( Step >= Distance )
    {
        Velocity = TargetVelocity;
        return;
    }

    Velocity += Delta * (Step / Distance);
}

//=========================================================================

void PlayerMovement::AdvanceGamepadSpeed( PlayerMoveInput const&        Input,
                                          PlayerMovementSettings const& Settings,
                                          f32                           DeltaTime,
                                          PlayerMovementSpeeds&         Speeds )
{
    f32 const MaxSpeed = MAX( x_abs( Settings.MaxSpeed ), 0.0f );
    vector2 const TargetVelocity( Input.GamepadStrafe * MaxSpeed,
                                  Input.GamepadForward * MaxSpeed );

    ApproachGamepadVelocity( TargetVelocity,
                             Settings.Acceleration,
                             Settings.DecelerationFactor,
                             DeltaTime,
                             m_GamepadVelocity );

    f32 const Speed = m_GamepadVelocity.Length();
    if( Speed > MaxSpeed )
    {
        m_GamepadVelocity *= MaxSpeed / Speed;
    }

    Speeds.Forward = m_GamepadVelocity.Y;
    Speeds.Strafe  = m_GamepadVelocity.X;
}

//=========================================================================

void PlayerMovement::CombineDeviceSpeeds( PlayerMovementSpeeds const& KeyboardSpeeds,
                                          PlayerMovementSpeeds const& GamepadSpeeds,
                                          f32                         MaxSpeed,
                                          PlayerMovementSpeeds&       Speeds )
{
    Speeds.Forward = x_clamp( KeyboardSpeeds.Forward + GamepadSpeeds.Forward,
                              -MaxSpeed,
                               MaxSpeed );
    Speeds.Strafe = x_clamp( KeyboardSpeeds.Strafe + GamepadSpeeds.Strafe,
                             -MaxSpeed,
                              MaxSpeed );
    const f32 Speed = x_sqrt( x_sqr( Speeds.Forward ) + x_sqr( Speeds.Strafe ) );
    if( Speed > MaxSpeed )
    {
        Speeds.Forward *= MaxSpeed / Speed;
        Speeds.Strafe *= MaxSpeed / Speed;
    }
}

//=========================================================================

void PlayerMovement::Evaluate( PlayerMoveInput const&        Input,
                               PlayerMovementSettings const& Settings,
                               f32                          DeltaTime,
                               PlayerMovementSpeeds&        Speeds )
{
    f32 const MaxSpeed = MAX( x_abs( Settings.MaxSpeed ), 0.0f );

    PlayerMovementSpeeds KeyboardSpeeds;
    BuildKeyboardSpeeds( Input, MaxSpeed, KeyboardSpeeds );

    PlayerMovementSpeeds GamepadSpeeds;
    AdvanceGamepadSpeed( Input, Settings, DeltaTime, GamepadSpeeds );

    // Apply the speed limit after combining device velocities.
    CombineDeviceSpeeds( KeyboardSpeeds, GamepadSpeeds, MaxSpeed, Speeds );
}

