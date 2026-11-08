//=========================================================================
//
//  CharacterPhysics.hpp
//
//=========================================================================

#ifndef CHARACTER_PHYSICS_HPP
#define CHARACTER_PHYSICS_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Auxiliary/MiscUtils/Property.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "CharacterMovement.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum class CharacterMotionResponse
{
    None,
    Impulse,
    Launch,
};

//=========================================================================
//  CLASSES
//=========================================================================

class character_physics : public prop_interface, protected CharacterMovementCollision
{
public:
                                character_physics               ( void );
    void                        Init                            ( guid Guid );
    void                        ApplyCollision                  ( void );
#ifndef X_RETAIL
    void                        RenderCollision                 ( void );
#endif
    void                        SetPosition                     ( const vector3& Position );
    void                        Advance                         ( const vector3& MoveTo, f32 DeltaTime, xbool bIsDead = FALSE );
    vector3                     GetPosition                     ( void ) const;
    vector3                     GetContactVelocity              ( void ) const;
    vector3                     GetMotionVelocity               ( void ) const;
    vector3                     GetWorldVelocity                ( void ) const;
    f32                         GetStepOffset                   ( void ) const;
    void                        ApplyImpulse                    ( const vector3& Delta );
    void                        SetMotionVelocity               ( const vector3& Velocity );
    void                        ClearMotionVelocity             ( void );
    virtual void                OnEnumProp                      ( prop_enum& List );
    virtual xbool               OnProperty                      ( prop_query& I );
    f32                         GetColHeight                    ( void ) const;
    void                        SetColHeight                    ( f32 Height );
    f32                         GetColRadius                    ( void ) const;
    void                        SetColRadius                    ( f32 Radius );
    void                        SetColCrouchOffset              ( f32 Offset );
    bbox                        GetBBox                         ( void ) const;
    xbool                       SetCrouchParametric             ( f32 NormalizePercent );
    xbool                       Jump                            ( f32 YVel );
    void                        FlingWithVelocity               ( const vector3& Velocity, f32 AirControl, xbool FlingOnly, xbool ReflingOnly,
                                                                  guid FlingGuid );
    void                        FlingWithAcceleration           ( const vector3& Acceleration, f32 DeltaTime, f32 AirControl, xbool FlingOnly,
                                                                  xbool ReflingOnly );
    xbool                       HasExternalResponse             ( void ) const;
    xbool                       HasLaunchResponse               ( void ) const;
    void                        SetUseGravity                   ( xbool bEnable );
    void                        SetGravityAccel                 ( const f32& GravityAccel );
    f32                         GetGravityAccel                 ( void );
    xbool                       IsGrounded                      ( void ) const;
    xbool                       IsAirborne                      ( void ) const;
    xbool                       IsFalling                       ( void ) const;
    xbool                       HasLanded                       ( void ) const;
    f32                         GetLandingSpeed                 ( void ) const;
    CharacterSupportState       GetSupportState                 ( void ) const;
    void                        SetAirControl                   ( const f32& AirControl );
    void                        SetActorCollisionRadius         ( f32 newRadius );
    f32                         GetActorCollisionRadius         ( void );
    xbool                       GetNavCollided                  ( void ) const;
    guid                        GetGuid                         ( void );

    void                        ResetMovementProgress           ( void );
    xbool                       HasRecoveryFailed               ( void ) const;
    xbool                       IsMovementBlocked               ( void ) const;

    void                        SetLocoGravityOn                ( xbool bValue );
    void                        SetLocoCollisionOn              ( xbool bValue );
    void                        SetHandlePermeable              ( xbool bValue );

    void                        SetSolveActorCollisions         ( xbool doSolve );
    xbool                       IsLocoGravityOn                 ( void ) const;
    xbool                       IsLocoCollisionOn               ( void ) const;
    xbool                       IsSolvingActorCollisions        ( void ) const;

    void                        SolveActorAndPlatformCollisions ( void );
    void                        CatchUpWithRidingPlatform       ( f32 DeltaTime );
    guid                        GetMovingPlatformGuid           ( void ) const;
    void                        WatchForRidingPlatform          ( void );
    void                        ResetRidingPlatforms            ( void );
    void                        Push                            ( const vector3& PushVector );
    void                        SetCollisionIgnoreGuid          ( guid ignoreGuid );

    void                        SetupPlayerCollisionCheck       ( const vector3& Start, const vector3& End );

    void                        DetachFromGround                ( void );
    void                        InitialGroundCheck              ( const vector3& Position );
    xbool                       CanMoveTo                       ( const vector3& Position );
    CharacterMovementSettings   GetMovementSettings             ( void ) const;

    //-------------------------------------------------------------------------

protected:
    s32                         Sweep                           ( const vector3& Start, const vector3& End, f32 Radius, f32 Height, CharacterMovementContact* Contacts,
                                                                  s32 MaxContacts ) override;
    xbool                       GetPenetration                  ( const vector3& Position, f32 Radius, f32 Height, vector3& Correction ) override;
    xbool                       FindPenetration                 ( const vector3& Position, f32 Radius, f32 Height, u32 Attributes, vector3& Correction,
                                                                  guid* pObject = NULL );
    xbool                       UpdateSupport                   ( f32 Distance );
    xbool                       UsesGravity                     ( void ) const;
    void                        HandleLanding                   ( f32 ImpactSpeed );
    void                        CommitMovement                  ( const CharacterMovementResult& Result );
    void                        LeavePlatform                   ( xbool InheritVelocity );
    xbool                       MoveDisplacement                ( vector3& Position, const vector3& Delta );
    void                        NotifyContacts                  ( const xarray<CharacterMovementHit>& Objects );
    void                        CollectPermeable                ( object* pObject, const vector3& StartPos, const vector3& EndPos );
    void                        SolveActorCollisions            ( void );
    xbool                       SolvePropCollisions             ( const xarray<CharacterMovementHit>& Contacts );
    void                        HandleRecoveryFailure           ( void );
    void                        SolvePlatformCollisions         ( void );

    xbool                       ApplyFling                      ( const vector3& Velocity, f32 AirControl, xbool FlingOnly, xbool ReflingOnly, xbool ReplaceVelocity,
                                                                  guid FlingGuid );

    //-------------------------------------------------------------------------

protected:
    guid                    m_Guid;
    f32                     m_NavCollisionHeight;
    f32                     m_NavCollisionRadius;
    f32                     m_NavCollisionCrouchOffset;
    f32                     m_NavCollisionCurrentHeight;
    f32                     m_AirControl;
    f32                     m_GroundFriction;
    f32                     m_ExternalAirControl;
    xbool                   m_bHandlePermeable;
    xbool                   m_bNavCollided;
    xbool                   m_bRecoveryFailed;
    CharacterMotionResponse m_MotionResponse;
    xbool                   m_bUseGravity;
    xbool                   m_bLocoGravityOn;
    xbool                   m_bLocoCollisionOn;
    f32                     m_ContactOffset;
    vector3                 m_MotionVelocity;
    vector3                 m_Position;
    f32                     m_GroundTolerance;
    f32                     m_GroundSnapDistance;
    f32                     m_StepHeight;
    f32                     m_GravityAcceleration;
    radian                  m_SlopeAngle;
    guid                    m_IgnoreGuid;

    guid    m_MovingPlatformGuid;
    s32     m_MovingPlatformBone;
    matrix4 m_OldMovingPlatformL2W;
    vector3 m_OldMovingPlatformVelocity;
    guid    m_PlatformCollisionIgnoreList[8];
    s32     m_nPlatformsToIgnore;
    vector3 m_WorldVelocity;
    vector3 m_PlatformDisplacement;
    f32     m_StepOffset;
    guid    m_LastImpulseSource;
    f32     m_FlingAccelerationTime;

    CharacterMovementProgress m_MovementProgress;
    CharacterMovementSupport  m_Support;
    xbool                     m_Landed;
    f32                       m_LandingSpeed;

    xbool m_SolveActorCollisions;
    f32   m_ActorCollisionRadius;

#define CHARACTER_PHYSICS_MAX_TRIGGERS 4
    guid m_TriggerGuid[CHARACTER_PHYSICS_MAX_TRIGGERS];
    s32  m_nTriggerGuids;
};

//=========================================================================
//  INLINE IMPLEMENTATION
//=========================================================================

inline vector3 character_physics::GetWorldVelocity( void ) const
{
    return m_WorldVelocity;
}

//=========================================================================

inline f32 character_physics::GetStepOffset( void ) const
{
    return m_StepOffset;
}

//=========================================================================

inline f32 character_physics::GetColHeight( void ) const
{
    return m_NavCollisionCurrentHeight;
}

//=========================================================================

inline f32 character_physics::GetColRadius( void ) const
{
    return m_NavCollisionRadius;
}

//=========================================================================

inline xbool character_physics::HasExternalResponse( void ) const
{
    return m_MotionResponse != CharacterMotionResponse::None;
}

//=========================================================================

inline xbool character_physics::HasLaunchResponse( void ) const
{
    return m_MotionResponse == CharacterMotionResponse::Launch;
}

//=========================================================================

inline xbool character_physics::HasLanded( void ) const
{
    return m_Landed;
}

//=========================================================================

inline f32 character_physics::GetLandingSpeed( void ) const
{
    return m_LandingSpeed;
}

//=========================================================================

inline CharacterSupportState character_physics::GetSupportState( void ) const
{
    return m_Support.State;
}

//=========================================================================

inline void character_physics::SetAirControl( const f32& AirControl )
{
    m_AirControl = AirControl;
}

//=========================================================================

inline void character_physics::SetActorCollisionRadius( f32 newRadius )
{
    m_ActorCollisionRadius = newRadius;
}

//=========================================================================

inline f32 character_physics::GetActorCollisionRadius( void )
{
    return m_ActorCollisionRadius;
}

//=========================================================================

inline xbool character_physics::GetNavCollided( void ) const
{
    return m_bNavCollided;
}

//=========================================================================

inline guid character_physics::GetGuid( void )
{
    return m_Guid;
}

//=========================================================================

inline void character_physics::ResetMovementProgress( void )
{
    m_MovementProgress.Clear();
}

//=========================================================================

inline xbool character_physics::HasRecoveryFailed( void ) const
{
    return m_bRecoveryFailed;
}

//=========================================================================

inline xbool character_physics::IsMovementBlocked( void ) const
{
    return m_MovementProgress.IsBlocked() || m_bRecoveryFailed;
}

//=========================================================================

inline void character_physics::SetLocoCollisionOn( xbool bValue )
{
    m_bLocoCollisionOn = bValue;
}

//=========================================================================

inline void character_physics::SetHandlePermeable( xbool bValue )
{
    m_bHandlePermeable = bValue;
}

//=========================================================================

inline void character_physics::SetSolveActorCollisions( xbool doSolve )
{
    m_SolveActorCollisions = doSolve;
}

//=========================================================================

inline xbool character_physics::IsLocoGravityOn( void ) const
{
    return m_bLocoGravityOn;
}

//=========================================================================

inline xbool character_physics::IsLocoCollisionOn( void ) const
{
    return m_bLocoCollisionOn;
}

//=========================================================================

inline xbool character_physics::IsSolvingActorCollisions( void ) const
{
    return m_SolveActorCollisions;
}

//=========================================================================

inline guid character_physics::GetMovingPlatformGuid( void ) const
{
    return m_MovingPlatformGuid;
}

//=========================================================================

inline vector3 character_physics::GetPosition( void ) const
{
    return m_Position;
}

//=========================================================================

inline vector3 character_physics::GetContactVelocity( void ) const
{
    vector3 Velocity = m_MotionVelocity;
    if( IsGrounded() )
    {
        Velocity.GetY() = -( m_Support.Normal.GetX() * Velocity.GetX() + m_Support.Normal.GetZ() * Velocity.GetZ() ) /
                          m_Support.Normal.GetY();
        Velocity += m_OldMovingPlatformVelocity;
    }
    return Velocity;
}

//=========================================================================

inline vector3 character_physics::GetMotionVelocity( void ) const
{
    return m_MotionVelocity;
}

//=========================================================================

inline void character_physics::ClearMotionVelocity( void )
{
    m_MotionVelocity.Zero();
    m_FlingAccelerationTime = 0.0f;
    m_MotionResponse        = CharacterMotionResponse::None;
    m_LastImpulseSource     = NULL_GUID;
}

//=========================================================================

inline void character_physics::SetColHeight( f32 Height )
{
    ASSERT( x_isvalid( Height ) );
    m_NavCollisionHeight        = x_max( Height, m_NavCollisionRadius * 2.0f );
    m_NavCollisionCurrentHeight = m_NavCollisionHeight;
    m_NavCollisionCrouchOffset =
        x_clamp( m_NavCollisionCrouchOffset, 0.0f, m_NavCollisionHeight - m_NavCollisionRadius * 2.0f );
}

//=========================================================================

inline void character_physics::SetColRadius( f32 Radius )
{
    ASSERT( x_isvalid( Radius ) );
    m_NavCollisionRadius = x_clamp( Radius, 1.0f, m_NavCollisionHeight * 0.5f );
    m_NavCollisionCrouchOffset =
        x_clamp( m_NavCollisionCrouchOffset, 0.0f, m_NavCollisionHeight - m_NavCollisionRadius * 2.0f );
    m_NavCollisionCurrentHeight = x_max( m_NavCollisionCurrentHeight, m_NavCollisionRadius * 2.0f );
}

//=========================================================================

inline void character_physics::SetColCrouchOffset( f32 Offset )
{
    ASSERT( x_isvalid( Offset ) );
    m_NavCollisionCrouchOffset = x_clamp( Offset, 0.0f, m_NavCollisionHeight - m_NavCollisionRadius * 2.0f );
}

//=========================================================================

inline bbox character_physics::GetBBox( void ) const
{
    bbox BBox;
    BBox.Max.GetZ() = BBox.Max.GetX() = GetColRadius();
    BBox.Min.GetZ() = BBox.Min.GetX() = -BBox.Max.GetX();
    BBox.Max.GetY()                   = GetColHeight();
    BBox.Min.GetY()                   = 0;
    return BBox;
}

//=========================================================================

inline void character_physics::SetGravityAccel( const f32& fGravityAccel )
{
    m_GravityAcceleration = fGravityAccel;
}

//=========================================================================

inline f32 character_physics::GetGravityAccel( void )
{
    return m_GravityAcceleration;
}

//=========================================================================

inline xbool character_physics::UsesGravity( void ) const
{
    return m_bUseGravity && m_bLocoGravityOn;
}

//=========================================================================

inline xbool character_physics::IsGrounded( void ) const
{
    return m_Support.IsWalkable();
}

//=========================================================================

inline xbool character_physics::IsAirborne( void ) const
{
    return UsesGravity() && !IsGrounded();
}

//=========================================================================

inline xbool character_physics::IsFalling( void ) const
{
    return IsAirborne() && ( m_MotionVelocity.GetY() < 0.0f );
}

//=========================================================================
#endif // CHARACTER_PHYSICS_HPP
//=========================================================================
