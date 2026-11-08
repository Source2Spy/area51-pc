//=========================================================================
//
//  CharacterMovement.hpp
//
//=========================================================================

#ifndef CHARACTER_MOVEMENT_HPP
#define CHARACTER_MOVEMENT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "x_array.hpp"

//=========================================================================
//  CONSTANTS
//=========================================================================

static const s32 CHARACTER_MOVEMENT_MAX_PLANES = 5;

//=========================================================================
//  TYPES
//=========================================================================

struct CharacterMovementSettings
{
    f32    Radius;
    f32    Height;
    f32    StepHeight;
    f32    GroundSnapDistance;
    f32    GroundTolerance;
    f32    ContactOffset;
    radian SlopeAngle;
    xbool  WorldCollision;

    static radian                   ClampSlopeAngle     ( radian Angle );
    f32                             GetMinGroundNormalY ( void ) const;
};

//-------------------------------------------------------------------------

struct CharacterMovementContact
{
    f32     T;
    vector3 Point;
    vector3 Normal;
    vector3 SurfaceNormal;
    guid    Object;
    s32     PrimitiveKey;
    xbool   HitTriangleEdge;
};

//-------------------------------------------------------------------------

enum class CharacterSupportState
{
    None,
    Walkable,
    Steep,
    Contact,
};

//-------------------------------------------------------------------------

struct CharacterMovementSupport
{
    CharacterSupportState    State = CharacterSupportState::None;
    CharacterMovementContact Contact;
    vector3                  Position;
    vector3                  Normal;

    xbool                           IsWalkable          ( void ) const
    {
        return State == CharacterSupportState::Walkable;
    }
};

//-------------------------------------------------------------------------

struct CharacterMovementHit
{
    guid    Object;
    vector3 Velocity;
};

//-------------------------------------------------------------------------

struct CharacterMovementResult
{
    vector3                      Position;
    vector3                      SlideVelocity;
    vector3                      RecoveryOffset;
    CharacterMovementSupport     Ground;
    vector3                      Planes[CHARACTER_MOVEMENT_MAX_PLANES];
    s32                          PlaneCount;
    f32                          TimeRemaining;
    xbool                        Landed;
    xbool                        Collided;
    xbool                        Stopped;
    xbool                        RecoveryFailed;
    f32                          StepOffset;
    xarray<CharacterMovementHit> Hits;
};

//=========================================================================
//  CLASSES
//=========================================================================

class CharacterMovementProgress
{
public:
                                    CharacterMovementProgress ( void );

    void                            Clear               ( void );
    void                            Advance             ( const vector3& Requested, const vector3& Actual, f32 DeltaTime );
    xbool                           IsBlocked           ( void ) const;

private:
    f32 m_BlockedTime;
};

//=========================================================================

class CharacterMovementCollision
{
public:
    virtual                         ~CharacterMovementCollision ( void ) = default;

    virtual s32                     Sweep               ( const vector3& Start, const vector3& End, f32 Radius, f32 Height,
                                                          CharacterMovementContact* Contacts, s32 MaxContacts ) = 0;

    virtual xbool                   GetPenetration      ( const vector3& Position, f32 Radius, f32 Height, vector3& Correction ) = 0;
};

//=========================================================================

class CharacterMovement
{
public:
    static xbool                    CanMoveTo           ( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                                                          const vector3& Start, const vector3& End, xbool RequireGround );

    static CharacterMovementResult  Move                ( CharacterMovementCollision&      Collision,
                                                          const CharacterMovementSettings& Settings, const vector3& Position,
                                                          const vector3& Velocity, f32 DeltaTime, xbool Grounded,
                                                          xbool AllowGroundSnap );

    static xbool                    FindSupport         ( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                                                          const vector3& Position, f32 Distance, CharacterMovementSupport& Ground );

    static xbool                    Recover             ( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                                                          vector3& Position );

    static vector3                  ResolveVelocity     ( const vector3& Velocity, const CharacterMovementResult& Result );

    static vector3                  ClipVelocity        ( const vector3& Velocity, const vector3* Normals, s32 Count );

private:
    static CharacterMovementResult  StepMove            ( CharacterMovementCollision&      Collision,
                                                          const CharacterMovementSettings& Settings, const vector3& Position,
                                                          const vector3& Velocity, f32 DeltaTime );

    static CharacterMovementResult  SlideMove           ( CharacterMovementCollision&      Collision,
                                                          const CharacterMovementSettings& Settings, const vector3& Position,
                                                          const vector3& Velocity, f32 DeltaTime, xbool DetectLanding );
};

//=========================================================================
#endif // CHARACTER_MOVEMENT_HPP
//=========================================================================
