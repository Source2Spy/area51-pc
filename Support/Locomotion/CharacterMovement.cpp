//=========================================================================
//
//  CharacterMovement.cpp
//
//=========================================================================
//  INCLUDES
//=========================================================================

#include "CharacterMovement.hpp"
#include "CollisionMgr/CollisionPrimatives.hpp"

//=========================================================================
//  CONSTANTS
//=========================================================================

static const s32 MAX_MOVE_CONTACTS        = 32;
static const s32 MAX_MOVE_BUMPS           = 5;
static const s32 MAX_RECOVERY             = 8;
static const f32 MOVE_EPSILON             = 0.0001f;
static const f32 PLANE_MERGE_COSINE       = 0.999f;
static const f32 CONTACT_POSITION_EPSILON = CAPSULE_CAST_EPSILON * 2.0f;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
void AddHit( CharacterMovementResult& Result, guid Object, const vector3& Velocity )
{
    if( Object == NULL_GUID )
    {
        return;
    }

    for( s32 i = 0; i < Result.Hits.GetCount(); i++ )
    {
        if( Result.Hits[i].Object == Object )
        {
            return;
        }
    }
    CharacterMovementHit& Hit = Result.Hits.Append();
    Hit.Object               = Object;
    Hit.Velocity             = Velocity;
}

//=========================================================================

static
xbool IsWalkable( const CharacterMovementContact& Contact, f32 MinGroundNormalY )
{
    return ( Contact.SurfaceNormal.GetY() >= MinGroundNormalY ) && ( Contact.Normal.GetY() > MOVE_EPSILON );
}

//=========================================================================

static
vector3 GetSlideNormal( const CharacterMovementContact& Contact )
{
    vector3 Normal = Contact.Normal;
    if( x_abs( Contact.SurfaceNormal.GetY() ) <= MOVE_EPSILON && Normal.GetY() > 0.0f )
    {
        Normal = Contact.SurfaceNormal;
    }
    return Normal;
}

//=========================================================================

static
vector3 LimitSlopeVelocity( const CharacterMovementContact& Contact, const vector3& Velocity,
                                   f32 MinGroundNormalY )
{
    if( ( Contact.SurfaceNormal.GetY() >= 0.0f ) &&
        ( ( Contact.SurfaceNormal.GetY() < MinGroundNormalY ) || Contact.HitTriangleEdge ) &&
        ( Contact.Normal.GetY() > 0.0f ) )
    {
        vector3 Normal = Contact.Normal;
        Normal.GetY()  = 0.0f;
        if( Normal.LengthSquared() > MOVE_EPSILON )
        {
            Normal.Normalize();
        }
        else
        {
            Normal = Contact.SurfaceNormal;
        }

        vector3 Horizontal = Velocity;
        Horizontal.GetY()  = 0.0f;
        Horizontal         = CharacterMovement::ClipVelocity( Horizontal, &Normal, 1 );
        const vector3 Vertical( 0.0f, Velocity.GetY(), 0.0f );
        const vector3 SlideNormal = GetSlideNormal( Contact );
        return Horizontal + CharacterMovement::ClipVelocity( Vertical, &SlideNormal, 1 );
    }
    return Velocity;
}

//=========================================================================

static
xbool SatisfiesPlanes( const vector3& Velocity, const vector3* Normals, s32 Count )
{
    for( s32 i = 0; i < Count; i++ )
    {
        if( Velocity.Dot( Normals[i] ) < -MOVE_EPSILON )
        {
            return FALSE;
        }
    }
    return TRUE;
}

//=========================================================================

static
vector3 SweepPosition( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                              const vector3& Start, const vector3& End )
{
    const vector3 Delta    = End - Start;
    const f32     Distance = Delta.Length();
    if( Distance <= MOVE_EPSILON )
    {
        return Start;
    }

    CharacterMovementContact Contacts[MAX_MOVE_CONTACTS];
    const s32 Hits = Collision.Sweep( Start, End, Settings.Radius, Settings.Height, Contacts, MAX_MOVE_CONTACTS );
    f32       T    = 1.0f;
    for( s32 i = 0; i < Hits; i++ )
    {
        T = x_min( T, x_max( 0.0f, Contacts[i].T - Settings.ContactOffset /
                                                       x_max( MOVE_EPSILON, -Contacts[i].Normal.Dot( Delta ) ) ) );
    }
    return Start + Delta * T;
}

//=========================================================================

static
xbool SweepGround( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                          const vector3& Start, const vector3& End, CharacterMovementSupport& Ground )
{
    CharacterMovementContact Contacts[MAX_MOVE_CONTACTS];
    const s32 Hits  = Collision.Sweep( Start, End, Settings.Radius, Settings.Height, Contacts, MAX_MOVE_CONTACTS );
    s32       First = -1;
    const f32 TimeTolerance = CONTACT_POSITION_EPSILON / x_max( MOVE_EPSILON, ( End - Start ).Length() );
    for( s32 i = 0; i < Hits; i++ )
    {
        if( Contacts[i].Normal.GetY() <= MOVE_EPSILON )
        {
            continue;
        }
        if( !Settings.WorldCollision && !IsWalkable( Contacts[i], Settings.GetMinGroundNormalY() ) )
        {
            continue;
        }
        if( ( First == -1 ) || ( Contacts[i].T < Contacts[First].T - TimeTolerance ) ||
            ( ( x_abs( Contacts[i].T - Contacts[First].T ) <= TimeTolerance ) &&
              ( Contacts[i].SurfaceNormal.GetY() > Contacts[First].SurfaceNormal.GetY() ) ) )
        {
            First = i;
        }
    }
    if( First == -1 )
    {
        return FALSE;
    }

    Ground.Contact  = Contacts[First];
    Ground.Position = Start + ( End - Start ) * Ground.Contact.T;
    Ground.Position.GetY() += Settings.ContactOffset / Ground.Contact.Normal.GetY();
    vector3 Correction;
    return !Settings.WorldCollision ||
           !Collision.GetPenetration( Ground.Position, Settings.Radius, Settings.Height, Correction );
}

//=========================================================================

static
xbool FindEdgeSupport( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                              CharacterMovementSupport& Ground )
{
    if( !Ground.Contact.HitTriangleEdge || ( Settings.StepHeight <= 0.0f ) ||
        ( Ground.Contact.SurfaceNormal.GetY() < -MOVE_EPSILON ) )
    {
        return FALSE;
    }

    vector3 Direction = -Ground.Contact.Normal;
    Direction.GetY()  = 0.0f;
    if( Direction.LengthSquared() <= MOVE_EPSILON )
    {
        Direction        = -Ground.Contact.SurfaceNormal;
        Direction.GetY() = 0.0f;
        if( Direction.LengthSquared() <= MOVE_EPSILON )
        {
            return FALSE;
        }
    }
    Direction.Normalize();

    const vector3 Up      = SweepPosition( Collision, Settings, Ground.Position,
                                           Ground.Position + vector3( 0.0f, Settings.StepHeight, 0.0f ) );
    const vector3 Forward = Direction * ( Settings.Radius * 2.0f + Settings.ContactOffset );
    const vector3 Probe   = SweepPosition( Collision, Settings, Up, Up + Forward );
    if( ( Probe - Ground.Contact.Point ).Dot( Direction ) <= CONTACT_POSITION_EPSILON )
    {
        return FALSE;
    }

    CharacterMovementSupport Support;
    const vector3            End( Probe.GetX(), Ground.Position.GetY() - Settings.GroundSnapDistance - Settings.Radius,
                                  Probe.GetZ() );
    if( !SweepGround( Collision, Settings, Probe, End, Support ) ||
        !IsWalkable( Support.Contact, Settings.GetMinGroundNormalY() ) ||
        ( ( Support.Contact.Point - Ground.Contact.Point ).Dot( Direction ) <= CONTACT_POSITION_EPSILON ) ||
        ( x_abs( Ground.Contact.Point.GetY() - Support.Contact.Point.GetY() ) >
          Settings.StepHeight + CONTACT_POSITION_EPSILON ) ||
        ( Ground.Position.GetY() - Support.Position.GetY() > Settings.GroundSnapDistance + MOVE_EPSILON ) )
    {
        return FALSE;
    }

    // Keep the actual contact; the forward probe only classifies support.
    Ground.Normal = Support.Contact.SurfaceNormal;
    return TRUE;
}

//=========================================================================

static
void SnapToGround( xbool Grounded, CharacterMovementResult& Result )
{
    if( Result.Ground.IsWalkable() )
    {
        if( Grounded )
        {
            Result.StepOffset += Result.Ground.Position.GetY() - Result.Position.GetY();
        }
        Result.Position             = Result.Ground.Position;
        Result.SlideVelocity.GetY() = 0.0f;
    }
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

radian CharacterMovementSettings::ClampSlopeAngle( radian Angle )
{
    ASSERT( x_isvalid( Angle ) );
    return x_clamp( Angle, 0.0f, R_89 );
}

//=========================================================================

f32 CharacterMovementSettings::GetMinGroundNormalY( void ) const
{
    return x_cos( ClampSlopeAngle( SlopeAngle ) );
}

//=========================================================================

CharacterMovementProgress::CharacterMovementProgress( void ) : m_BlockedTime( 0.0f )
{
}

//=========================================================================

void CharacterMovementProgress::Clear( void )
{
    m_BlockedTime = 0.0f;
}

//=========================================================================

void CharacterMovementProgress::Advance( const vector3& Requested, const vector3& Actual, f32 DeltaTime )
{
    const f32 DistanceSquared = Requested.LengthSquared();
    if( ( DistanceSquared > F32_MIN ) && ( Actual.Dot( Requested ) < DistanceSquared * 0.05f ) )
    {
        m_BlockedTime = x_min( 0.5f, m_BlockedTime + DeltaTime );
    }
    else
    {
        m_BlockedTime = 0.0f;
    }
}

//=========================================================================

xbool CharacterMovementProgress::IsBlocked( void ) const
{
    return m_BlockedTime >= 0.5f;
}

//=========================================================================

xbool CharacterMovement::CanMoveTo( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                                    const vector3& Start, const vector3& End, xbool RequireGround )
{
    vector3 Current = Start;
    vector3 Delta   = End - Start;
    if( RequireGround )
    {
        Delta.GetY() = 0.0f;
    }
    const s32     Steps    = MAX( 1, (s32)x_ceil( Delta.Length() / Settings.Radius ) );
    const vector3 Interval = Delta / Steps;
    for( s32 i = 0; i < Steps; i++ )
    {
        const CharacterMovementResult Result =
            Move( Collision, Settings, Current, Interval, 1.0f, RequireGround, RequireGround );
        if( ( RequireGround && !Result.Ground.IsWalkable() ) || Result.RecoveryFailed )
        {
            return FALSE;
        }
        Current = Result.Position;
    }
    vector3 Remaining = End - Current;
    if( RequireGround )
    {
        Remaining.GetY() = 0.0f;
    }
    return ( Remaining.LengthSquared() <= x_sqr( Settings.GroundTolerance ) ) &&
           ( !RequireGround || ( x_abs( Current.GetY() - End.GetY() ) <= Settings.StepHeight ) );
}

//=========================================================================

vector3 CharacterMovement::ClipVelocity( const vector3& Velocity, const vector3* Normals, s32 Count )
{
    if( SatisfiesPlanes( Velocity, Normals, Count ) )
    {
        return Velocity;
    }

    vector3 Best( 0.0f, 0.0f, 0.0f );
    f32     BestDistance = Velocity.LengthSquared();
    for( s32 i = 0; i < Count; i++ )
    {
        vector3 Candidate = Velocity - Normals[i] * x_min( 0.0f, Velocity.Dot( Normals[i] ) );
        f32     Distance  = ( Candidate - Velocity ).LengthSquared();
        if( SatisfiesPlanes( Candidate, Normals, Count ) && ( Distance < BestDistance ) )
        {
            Best         = Candidate;
            BestDistance = Distance;
        }

        for( s32 j = i + 1; j < Count; j++ )
        {
            vector3   Crease        = Normals[i].Cross( Normals[j] );
            const f32 LengthSquared = Crease.LengthSquared();
            if( LengthSquared <= MOVE_EPSILON )
            {
                continue;
            }

            Candidate = Crease * ( Velocity.Dot( Crease ) / LengthSquared );
            Distance  = ( Candidate - Velocity ).LengthSquared();
            if( SatisfiesPlanes( Candidate, Normals, Count ) && ( Distance < BestDistance ) )
            {
                Best         = Candidate;
                BestDistance = Distance;
            }
        }
    }

    return Best;
}

//=========================================================================

CharacterMovementResult CharacterMovement::SlideMove( CharacterMovementCollision&      Collision,
                                                      const CharacterMovementSettings& Settings,
                                                      const vector3& Position, const vector3& Velocity, f32 DeltaTime,
                                                      xbool DetectLanding )
{
    CharacterMovementResult Result = {};
    Result.Position                = Position;
    Result.SlideVelocity           = Velocity;

    f32       RemainingTime    = DeltaTime;
    const f32 MinGroundNormalY = Settings.GetMinGroundNormalY();
    s32       Bump             = 0;

    for( ; Bump < MAX_MOVE_BUMPS; Bump++ )
    {
        const vector3 Delta    = Result.SlideVelocity * RemainingTime;
        const f32     Distance = Delta.Length();
        if( Distance <= MOVE_EPSILON )
        {
            break;
        }

        CharacterMovementContact Contacts[MAX_MOVE_CONTACTS];
        s32 Hits = Collision.Sweep( Result.Position, Result.Position + Delta, Settings.Radius, Settings.Height,
                                    Contacts, MAX_MOVE_CONTACTS );
        s32 Blocking = 0;
        for( s32 i = 0; i < Hits; i++ )
        {
            if( GetSlideNormal( Contacts[i] ).Dot( Delta ) < -MOVE_EPSILON )
            {
                Contacts[Blocking++] = Contacts[i];
            }
        }
        Hits = Blocking;
        if( Hits == 0 )
        {
            Result.Position += Delta;
            break;
        }

        s32 First = 0;
        for( s32 i = 1; i < Hits; i++ )
        {
            if( Contacts[i].T < Contacts[First].T )
            {
                First = i;
            }
        }

        const CharacterMovementContact& Contact = Contacts[First];
        const f32 T = x_clamp( Contact.T - Settings.ContactOffset / x_max( MOVE_EPSILON, -Contact.Normal.Dot( Delta ) ),
                               0.0f, 1.0f );
        Result.Position += Delta * T;
        RemainingTime *= 1.0f - T;
        Result.Collided = TRUE;

        const f32 TimeTolerance = CONTACT_POSITION_EPSILON / Distance;
        xbool     Duplicate     = TRUE;
        for( s32 i = 0; i < Hits; i++ )
        {
            if( Contacts[i].T > Contact.T + TimeTolerance )
            {
                continue;
            }

            AddHit( Result, Contacts[i].Object, Result.SlideVelocity );
            const vector3 Normal = GetSlideNormal( Contacts[i] );
            xbool Found = FALSE;
            for( s32 j = 0; j < Result.PlaneCount; j++ )
            {
                Found |= Result.Planes[j].Dot( Normal ) > PLANE_MERGE_COSINE;
            }

            if( !Found && ( Result.PlaneCount < CHARACTER_MOVEMENT_MAX_PLANES ) )
            {
                Result.Planes[Result.PlaneCount++] = Normal;
                Duplicate                          = FALSE;
            }
        }

        if( DetectLanding && ( Result.SlideVelocity.GetY() <= 0.0f ) &&
            FindSupport( Collision, Settings, Result.Position, Settings.ContactOffset, Result.Ground ) )
        {
            Result.Position             = Result.Ground.Position;
            Result.SlideVelocity.GetY() = 0.0f;
            Result.Landed               = TRUE;
            Result.TimeRemaining        = RemainingTime;
            break;
        }

        for( s32 i = 0; i < Hits; i++ )
        {
            if( Contacts[i].T <= Contact.T + TimeTolerance )
            {
                Result.SlideVelocity = LimitSlopeVelocity( Contacts[i], Result.SlideVelocity, MinGroundNormalY );
            }
        }
        Result.SlideVelocity = ClipVelocity( Result.SlideVelocity, Result.Planes, Result.PlaneCount );

        if( ( Result.PlaneCount == CHARACTER_MOVEMENT_MAX_PLANES ) ||
            ( Result.SlideVelocity.Dot( Velocity ) <= 0.0f ) || ( Duplicate && ( T <= MOVE_EPSILON ) ) )
        {
            Result.SlideVelocity.Zero();
            Result.Stopped = TRUE;
            break;
        }
    }

    if( Bump == MAX_MOVE_BUMPS )
    {
        Result.SlideVelocity.Zero();
        Result.Stopped = TRUE;
    }
    return Result;
}

//=========================================================================

xbool CharacterMovement::FindSupport( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                                      const vector3& Position, f32 Distance, CharacterMovementSupport& Ground )
{
    Ground          = {};
    Ground.Position = Position;
    Ground.Normal.Set( 0.0f, 1.0f, 0.0f );

    vector3 Start = Position + vector3( 0.0f, Settings.GroundTolerance, 0.0f );
    if( Settings.WorldCollision )
    {
        Start = SweepPosition( Collision, Settings, Position, Start );
    }

    const vector3 End = Position - vector3( 0.0f, Distance + Settings.Radius, 0.0f );
    if( !SweepGround( Collision, Settings, Start, End, Ground ) )
    {
        return FALSE;
    }

    const f32 RiseTolerance  = Settings.ContactOffset / x_max( MOVE_EPSILON, Ground.Contact.Normal.GetY() );
    const f32 GroundDistance = Position.GetY() - Ground.Position.GetY();
    if( ( GroundDistance < -RiseTolerance ) || ( GroundDistance > Distance + MOVE_EPSILON ) )
    {
        Ground.State = CharacterSupportState::None;
        return FALSE;
    }

    Ground.State = CharacterSupportState::Contact;
    if( IsWalkable( Ground.Contact, Settings.GetMinGroundNormalY() ) )
    {
        if( GroundDistance < 0.0f )
        {
            const f32 Clearance = Settings.ContactOffset / Ground.Contact.Normal.GetY();
            if( GroundDistance < -Clearance - CONTACT_POSITION_EPSILON )
            {
                return FALSE;
            }
            Ground.Position = Position;
        }

        Ground.Normal = Ground.Contact.SurfaceNormal;
        Ground.State  = CharacterSupportState::Walkable;
        return TRUE;
    }

    if( Settings.WorldCollision && FindEdgeSupport( Collision, Settings, Ground ) )
    {
        Ground.State = CharacterSupportState::Walkable;
        return TRUE;
    }

    if( Ground.Contact.Normal.GetY() > MOVE_EPSILON )
    {
        Ground.State = CharacterSupportState::Steep;
    }
    return FALSE;
}

//=========================================================================

xbool CharacterMovement::Recover( CharacterMovementCollision& Collision, const CharacterMovementSettings& Settings,
                                  vector3& Position )
{
    vector3 RecoveredPosition = Position;
    vector3 Correction;

    for( s32 i = 0; i < MAX_RECOVERY; i++ )
    {
        if( !Collision.GetPenetration( RecoveredPosition, Settings.Radius, Settings.Height, Correction ) )
        {
            Position = RecoveredPosition;
            return TRUE;
        }

        if( Correction.LengthSquared() <= x_sqr( MOVE_EPSILON ) )
        {
            return FALSE;
        }
        RecoveredPosition += Correction;
    }

    if( Collision.GetPenetration( RecoveredPosition, Settings.Radius, Settings.Height, Correction ) )
    {
        return FALSE;
    }

    Position = RecoveredPosition;
    return TRUE;
}

//=========================================================================

vector3 CharacterMovement::ResolveVelocity( const vector3& Velocity, const CharacterMovementResult& Result )
{
    if( Result.Stopped )
    {
        return vector3( 0.0f, 0.0f, 0.0f );
    }

    vector3 Resolved = Velocity;
    if( Result.Ground.IsWalkable() )
    {
        Resolved.GetY() =
            -( Result.Ground.Normal.GetX() * Resolved.GetX() + Result.Ground.Normal.GetZ() * Resolved.GetZ() ) /
            Result.Ground.Normal.GetY();
    }

    Resolved        = ClipVelocity( Resolved, Result.Planes, Result.PlaneCount );
    Resolved.GetY() = x_clamp( Resolved.GetY(), x_min( 0.0f, Velocity.GetY() ), x_max( 0.0f, Velocity.GetY() ) );
    if( Result.Ground.IsWalkable() )
    {
        Resolved.GetY() = 0.0f;
    }
    return Resolved;
}

//=========================================================================

CharacterMovementResult CharacterMovement::StepMove( CharacterMovementCollision&      Collision,
                                                     const CharacterMovementSettings& Settings, const vector3& Position,
                                                     const vector3& Velocity, f32 DeltaTime )
{
    const vector3 Up =
        SweepPosition( Collision, Settings, Position, Position + vector3( 0.0f, Settings.StepHeight, 0.0f ) );
    vector3 Horizontal           = Velocity;
    Horizontal.GetY()            = 0.0f;
    CharacterMovementResult Step = SlideMove( Collision, Settings, Up, Horizontal, DeltaTime, FALSE );
    const f32               Drop = Up.GetY() - Position.GetY() + Settings.GroundSnapDistance;
    FindSupport( Collision, Settings, Step.Position, Drop, Step.Ground );
    if( !Step.Ground.IsWalkable() )
    {
        return Step;
    }
    if( ( Step.Ground.Position.GetY() - Position.GetY() > Settings.StepHeight + MOVE_EPSILON ) ||
        ( Step.Ground.Contact.Point.GetY() + Settings.ContactOffset - Position.GetY() >
          Settings.StepHeight + MOVE_EPSILON ) )
    {
        Step.Ground.State = CharacterSupportState::None;
        return Step;
    }
    Step.Position             = Step.Ground.Position;
    Step.StepOffset           = Step.Position.GetY() - Position.GetY() - Velocity.GetY() * DeltaTime;
    Step.SlideVelocity.GetY() = 0.0f;
    Step.Collided             = TRUE;
    AddHit( Step, Step.Ground.Contact.Object, Velocity );
    return Step;
}

//=========================================================================

CharacterMovementResult CharacterMovement::Move( CharacterMovementCollision&      Collision,
                                                 const CharacterMovementSettings& Settings, const vector3& Position,
                                                 const vector3& Velocity, f32 DeltaTime, xbool Grounded,
                                                 xbool AllowGroundSnap )
{
    vector3 Start = Position;
    if( Settings.WorldCollision && !Recover( Collision, Settings, Start ) )
    {
        CharacterMovementResult Failed = {};
        Failed.Position                = Position;
        Failed.RecoveryFailed          = TRUE;
        return Failed;
    }

    CharacterMovementResult Result = {};
    if( Settings.WorldCollision )
    {
        Result = SlideMove( Collision, Settings, Start, Velocity, DeltaTime, AllowGroundSnap && !Grounded );
    }
    else
    {
        Result.Position      = Start + Velocity * DeltaTime;
        Result.SlideVelocity = Velocity;
    }
    Result.RecoveryOffset = Start - Position;

    vector3 Horizontal       = Velocity;
    Horizontal.GetY()        = 0.0f;
    const xbool SupportKnown = Settings.WorldCollision && Grounded && AllowGroundSnap;
    const xbool OrdinarySupported =
        SupportKnown && FindSupport( Collision, Settings, Result.Position, Settings.GroundSnapDistance, Result.Ground );

    const f32 OrdinaryProgress  = ( Result.Position - Start ).Dot( Horizontal );
    const f32 RequestedProgress = Horizontal.LengthSquared() * DeltaTime;
    if( SupportKnown && ( Settings.StepHeight > 0.0f ) && ( Horizontal.LengthSquared() > MOVE_EPSILON ) &&
        ( !OrdinarySupported || ( Result.Collided && ( OrdinaryProgress < RequestedProgress - MOVE_EPSILON ) ) ) )
    {
        CharacterMovementResult Step         = StepMove( Collision, Settings, Start, Velocity, DeltaTime );
        const f32               StepProgress = ( Step.Position - Start ).Dot( Horizontal );
        if( Step.Ground.IsWalkable() &&
            ( ( StepProgress > OrdinaryProgress + MOVE_EPSILON ) ||
              ( !OrdinarySupported && ( StepProgress >= OrdinaryProgress - MOVE_EPSILON ) ) ) )
        {
            Step.RecoveryOffset = Result.RecoveryOffset;
            Result              = Step;
        }
    }

    const vector3 SupportPosition = Result.Position;
    Result.RecoveryFailed         = Settings.WorldCollision && !Recover( Collision, Settings, Result.Position );
    if( Result.RecoveryFailed )
    {
        Result.Position      = Start;
        Result.Ground.State  = CharacterSupportState::None;
        Result.TimeRemaining = 0.0f;
        Result.Landed        = FALSE;
        Result.StepOffset    = 0.0f;
        Result.Hits.Clear();
        return Result;
    }
    Result.RecoveryOffset += Result.Position - SupportPosition;

    if( AllowGroundSnap && ( Grounded || ( Velocity.GetY() <= 0.0f ) ) )
    {
        if( ( !SupportKnown && !Result.Landed ) || ( Result.Position != SupportPosition ) )
        {
            f32     Distance    = Grounded ? Settings.GroundSnapDistance : Settings.ContactOffset;
            vector3 GroundStart = Result.Position;
            if( !Settings.WorldCollision )
            {
                GroundStart.GetY() = x_max( Start.GetY(), Result.Position.GetY() );
                Distance += GroundStart.GetY() - Result.Position.GetY();
            }
            FindSupport( Collision, Settings, GroundStart, Distance, Result.Ground );
        }
        Result.Landed &= Result.Ground.IsWalkable();
        SnapToGround( Grounded, Result );

        if( !Grounded && Result.Ground.IsWalkable() )
        {
            Result.Landed = TRUE;
        }
    }
    return Result;
}
