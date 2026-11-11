//=========================================================================
//
//  CharacterPhysics.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Render/PrimitiveDebug.hpp"
#include "CharacterPhysics.hpp"
#include "CollisionMgr/CollisionMgr.hpp"
#include "CollisionMgr/CollisionPrimatives.hpp"
#include "Entropy.hpp"
#include "x_math.hpp"
#include "Objects/AnimSurface.hpp"
#include "Objects/Player/Player.hpp"
#include "PainMgr/Pain.hpp"
#include "CollisionMgr/PolyCache.hpp"
#include "Characters/Character.hpp"
#include "Objects/Door.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Objects/CokeCan.hpp"

#ifndef X_EDITOR
#include "NetworkMgr/NetworkMgr.hpp"
#endif

//=========================================================================
//  CONSTANTS
//=========================================================================

static const f32 IMPULSE_SETTLE_SPEED  = 1.0f;
static const f32 REALLY_SMALL          = 0.00001f;
static const f32 MAX_MOVEMENT_INTERVAL = 1.0f / 60.0f;

//=========================================================================
//  GLOBAL STATE
//=========================================================================

xbool g_UpdateRidingPlatform   = TRUE;
xbool g_ApplyPlatformCollision = TRUE;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
void ComputePlatformMotion( const matrix4& OldPlatformM, const matrix4& NewPlatformM, const vector3& CurrentPos,
                                   vector3& DeltaPos, radian& DeltaYaw )
{
    // Compute motion matrix
    matrix4 DeltaM = OldPlatformM;
    DeltaM.InvertSRT();
    DeltaM = NewPlatformM * DeltaM;

    // Compute DeltaPos
    vector3 NewPos = DeltaM * CurrentPos;
    DeltaPos       = NewPos - CurrentPos;

    // Compute DeltaYaw
    radian3 OldPlatformYaw = OldPlatformM.GetRotation();
    radian3 NewPlatformYaw = NewPlatformM.GetRotation();
    DeltaYaw               = NewPlatformYaw.Yaw - OldPlatformYaw.Yaw;
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

character_physics::character_physics( void )
{
    m_Guid                      = 0;
    m_NavCollisionHeight        = 180.0f;
    m_NavCollisionCurrentHeight = m_NavCollisionHeight;
    m_NavCollisionRadius        = 30.0f;
    m_NavCollisionCrouchOffset  = 40.0f;
    m_AirControl                = 0.04f;
    m_ExternalAirControl        = 0.0f;
    m_bHandlePermeable          = FALSE;
    m_bNavCollided              = FALSE;
    m_bRecoveryFailed           = FALSE;
    m_MotionResponse            = CharacterMotionResponse::None;
    m_bUseGravity               = TRUE;
    m_bLocoGravityOn            = TRUE;
    m_bLocoCollisionOn          = TRUE;
    m_ContactOffset             = 1.0f;
    m_GroundTolerance           = 5.0f;
    m_GroundSnapDistance        = 45.0f;
    m_StepHeight                = 60.0f;
    m_GravityAcceleration       = -1000.0f;
    m_SlopeAngle                = R_60;
    m_MovingPlatformGuid        = 0;
    m_MovingPlatformBone        = -1;
    m_ActorCollisionRadius      = 50.0f;
    m_SolveActorCollisions      = FALSE;
    m_IgnoreGuid                = 0;
    m_LastImpulseSource         = 0;
    m_FlingAccelerationTime     = 0.0f;
    m_OldMovingPlatformVelocity.Zero();
    m_OldMovingPlatformL2W.Identity();

    m_nPlatformsToIgnore = 0;

    m_MotionVelocity.Set( 0, 0, 0 );
    m_Position.Set( 0, 0, 0 );

    m_WorldVelocity.Set( 0.0f, 0.0f, 0.0f );
    m_PlatformDisplacement.Zero();
    m_StepOffset     = 0.0f;
    m_GroundFriction = 8.0f;
    m_Support        = {};
    m_Support.Normal.Set( 0.0f, 1.0f, 0.0f );
    m_Landed       = FALSE;
    m_LandingSpeed = 0.0f;

    ResetMovementProgress();

    x_memset( m_TriggerGuid, 0, sizeof( m_TriggerGuid ) );
    m_nTriggerGuids = 0;
}

//=========================================================================

void character_physics::Init( guid Guid )
{
    m_Guid = Guid;
}

//=========================================================================

void character_physics::SolveActorAndPlatformCollisions( void )
{
    if( m_bLocoCollisionOn == FALSE )
    {
        return;
    }

    if( m_SolveActorCollisions )
    {
        SolveActorCollisions();
    }

    // Loop through list and process animated surfaces
    SolvePlatformCollisions();
}

//=========================================================================

void character_physics::SolveActorCollisions( void )
{
    actor* pOwner = (actor*)g_ObjMgr.GetObjectByGuid( m_Guid );
    if( !pOwner )
    {
        return;
    }
    ASSERT( pOwner->IsKindOf( actor::GetRTTI() ) );

    const f32 Radius = x_min( m_ActorCollisionRadius, m_NavCollisionCurrentHeight * 0.5f );
    for( actor* pActor = actor::m_pFirstActive; pActor; pActor = pActor->m_pNextActive )
    {
        if( ( pActor == pOwner ) || ( pActor->GetGuid() == m_IgnoreGuid ) || pActor->IsDead() )
        {
            continue;
        }
        const f32     OtherHeight   = pActor->GetCollisionHeight();
        const f32     OtherRadius   = x_min( pActor->GetActorCollisionRadius(), OtherHeight * 0.5f );
        const vector3 OtherPosition = pActor->GetPosition();
        const vector3 CoincidentNormal( m_Guid > pActor->GetGuid() ? 1.0f : -1.0f, 0.0f, 0.0f );
        vector3       Correction;
        if( !ComputeCapsuleCapsulePenetration( m_Position + vector3( 0.0f, Radius, 0.0f ),
                                               m_Position + vector3( 0.0f, m_NavCollisionCurrentHeight - Radius, 0.0f ),
                                               Radius, OtherPosition + vector3( 0.0f, OtherRadius, 0.0f ),
                                               OtherPosition + vector3( 0.0f, OtherHeight - OtherRadius, 0.0f ),
                                               OtherRadius, CoincidentNormal, Correction ) )
        {
            continue;
        }
        pOwner->SetCollidedActor( pActor->GetGuid() );
        if( pOwner->CanBePushedBy( *pActor ) )
        {
            Push( Correction );
            if( m_bRecoveryFailed )
            {
                return;
            }
        }
    }
}

//=========================================================================

void character_physics::Advance( const vector3& MoveTo, f32 DeltaTime, xbool bIsDead )
{
    if( !x_isvalid( DeltaTime ) || ( DeltaTime < 0.0f ) || !MoveTo.IsValid() )
    {
        ASSERT( FALSE );
        return;
    }
    if( DeltaTime <= F32_MIN )
    {
        m_Landed       = FALSE;
        m_LandingSpeed = 0.0f;
        m_StepOffset   = 0.0f;
        m_WorldVelocity.Zero();
        return;
    }

    xarray<CharacterMovementHit> ContactObjects;
    const vector3 StartPosition     = m_Position;
    const vector3 RequestedVelocity = ( MoveTo - StartPosition ) / DeltaTime;
    const s32     Steps             = MAX( 1, (s32)x_ceil( DeltaTime / MAX_MOVEMENT_INTERVAL ) );
    const f32     Interval          = DeltaTime / Steps;
    object*       pOwner            = g_ObjMgr.GetObjectByGuid( m_Guid );
    const xbool   IsPlayer          = pOwner && pOwner->IsKindOf( player::GetRTTI() );
    m_bNavCollided                  = FALSE;
    m_bRecoveryFailed               = FALSE;
    m_StepOffset                    = 0.0f;
    m_Landed                        = FALSE;
    m_LandingSpeed                  = 0.0f;

    for( s32 Step = 0; Step < Steps; Step++ )
    {
        if( UsesGravity() && ( m_MotionVelocity.GetY() <= 0.0f ) )
        {
            const xbool WasGrounded = IsGrounded();
            const f32   Distance    = WasGrounded ? m_GroundSnapDistance : m_ContactOffset;
            if( UpdateSupport( Distance ) )
            {
                if( !WasGrounded )
                {
                    HandleLanding( -m_MotionVelocity.GetY() );
                }
                m_MotionVelocity.GetY() = 0.0f;
            }
            else if( WasGrounded )
            {
                LeavePlatform( TRUE );
            }
        }
        else
        {
            m_Support.State = CharacterSupportState::None;
        }

        f32 RemainingTime = Interval;
        for( s32 Phase = 0; ( Phase < 3 ) && ( RemainingTime > F32_MIN ); Phase++ )
        {
            const f32     PhaseTime       = m_FlingAccelerationTime > 0.0f ?
                                               x_min( RemainingTime, m_FlingAccelerationTime ) : RemainingTime;
            const xbool   Grounded        = IsGrounded();
            const vector3 OldPosition     = m_Position;
            const vector3 InitialVelocity = m_MotionVelocity;
            f32           Gravity         = 0.0f;
            f32           ResponseBase    = 1.0f;
            f32           ResponseRate    = 0.0f;

            if( UsesGravity() )
            {
                if( Grounded && !HasExternalResponse() )
                {
                    m_MotionVelocity.GetX() = RequestedVelocity.GetX();
                    m_MotionVelocity.GetZ() = RequestedVelocity.GetZ();
                }
                else
                {
                    f32 Response;
                    if( Grounded )
                    {
                        ResponseBase = x_exp( -m_GroundFriction );
                        ResponseRate = 1.0f;
                        Response     = 1.0f - x_pow( ResponseBase, PhaseTime );
                    }
                    else
                    {
                        f32 AirControl = HasExternalResponse() ? m_ExternalAirControl : m_AirControl;
                        if( g_MPTweaks.Active && !HasExternalResponse() )
                        {
                            AirControl = g_MPTweaks.AirControl;
                        }
                        ResponseBase = 1.0f - x_clamp( AirControl, 0.0f, 1.0f );
                        ResponseRate = 60.0f;
                        Response     = 1.0f - x_pow( ResponseBase, PhaseTime * ResponseRate );
                    }

                    m_MotionVelocity.GetX() += Response * ( RequestedVelocity.GetX() - m_MotionVelocity.GetX() );
                    m_MotionVelocity.GetZ() += Response * ( RequestedVelocity.GetZ() - m_MotionVelocity.GetZ() );
                }

                if( !Grounded && ( m_FlingAccelerationTime <= 0.0f ) )
                {
                    Gravity = m_GravityAcceleration * ( g_MPTweaks.Active ? g_MPTweaks.Gravity : 1.0f );
                }
            }
            else
            {
                m_MotionVelocity = RequestedVelocity;
            }

            vector3 PathVelocity = m_MotionVelocity;
            if( UsesGravity() )
            {
                PathVelocity.GetY() += RequestedVelocity.GetY() + Gravity * PhaseTime * 0.5f;
                if( Grounded )
                {
                    PathVelocity.GetY() =
                        RequestedVelocity.GetY() - ( m_Support.Normal.GetX() * m_MotionVelocity.GetX() +
                                                     m_Support.Normal.GetZ() * m_MotionVelocity.GetZ() ) /
                                                       m_Support.Normal.GetY();
                }
            }

            if( IsPlayer && Grounded && UsesGravity() && m_bLocoCollisionOn && !HasExternalResponse() &&
                ( RequestedVelocity.GetY() == 0.0f ) )
            {
                // Preserve input speed along the support plane.
                const f32 SpeedSquared     = SQR( m_MotionVelocity.GetX() ) + SQR( m_MotionVelocity.GetZ() );
                const f32 PathSpeedSquared = PathVelocity.LengthSquared();
                if( PathSpeedSquared > SpeedSquared )
                {
                    PathVelocity *= x_sqrt( SpeedSquared / PathSpeedSquared );
                    m_MotionVelocity.GetX() = PathVelocity.GetX();
                    m_MotionVelocity.GetZ() = PathVelocity.GetZ();
                }
            }

            const CharacterMovementResult Result =
                CharacterMovement::Move( *this, GetMovementSettings(), m_Position, PathVelocity, PhaseTime,
                                         Grounded, UsesGravity() && ( Grounded || m_MotionVelocity.GetY() <= 0.0f ) );
            if( Result.RecoveryFailed )
            {
                HandleRecoveryFailure();
                break;
            }

            const f32 ConsumedTime = PhaseTime - Result.TimeRemaining;
            const f32 ImpactSpeed  = x_max( 0.0f, -( m_MotionVelocity.GetY() + Gravity * ConsumedTime ) );
            if( UsesGravity() && ( ResponseRate > 0.0f ) && ( ConsumedTime < PhaseTime ) )
            {
                const f32 Response =
                    ConsumedTime > F32_MIN ? 1.0f - x_pow( ResponseBase, ConsumedTime * ResponseRate ) : 0.0f;
                m_MotionVelocity.GetX() =
                    InitialVelocity.GetX() + Response * ( RequestedVelocity.GetX() - InitialVelocity.GetX() );
                m_MotionVelocity.GetZ() =
                    InitialVelocity.GetZ() + Response * ( RequestedVelocity.GetZ() - InitialVelocity.GetZ() );
            }
            m_MotionVelocity.GetY() += Gravity * ConsumedTime;
            m_FlingAccelerationTime = x_max( 0.0f, m_FlingAccelerationTime - ConsumedTime );
            m_MotionVelocity = CharacterMovement::ResolveVelocity( m_MotionVelocity, Result );
            CommitMovement( Result );

            if( Result.Landed )
            {
                HandleLanding( ImpactSpeed );
            }
            if( Grounded && !IsGrounded() )
            {
                LeavePlatform( TRUE );
            }

            if( IsGrounded() && HasExternalResponse() )
            {
                vector3 Difference = m_MotionVelocity - RequestedVelocity;
                Difference.GetY()  = 0.0f;
                if( Difference.LengthSquared() <= x_sqr( IMPULSE_SETTLE_SPEED ) )
                {
                    m_MotionResponse    = CharacterMotionResponse::None;
                    m_LastImpulseSource = NULL_GUID;
                }
            }

            for( s32 Contact = 0; Contact < Result.Hits.GetCount(); Contact++ )
            {
                const CharacterMovementHit& Hit = Result.Hits[Contact];
                xbool Found = FALSE;
                for( s32 i = 0; i < ContactObjects.GetCount(); i++ )
                {
                    Found |= ContactObjects[i].Object == Hit.Object;
                }
                if( !Found )
                {
                    ContactObjects.Append( Hit );
                }
            }
            if( m_bHandlePermeable && g_ObjMgr.GetObjectByGuid( m_Guid ) )
            {
                CollectPermeable( g_ObjMgr.GetObjectByGuid( m_Guid ), OldPosition, m_Position );
            }
            RemainingTime = x_max( 0.0f, RemainingTime - ConsumedTime );
        }

        if( m_bRecoveryFailed )
        {
            break;
        }
    }

    if( !m_bRecoveryFailed )
    {
        const vector3 BeforeExternal = m_Position;
        xbool         PropsChanged   = FALSE;
#ifndef X_EDITOR
        if( !g_NetworkMgr.IsOnline() || !bIsDead )
#endif
        {
            SolveActorAndPlatformCollisions();
            if( !m_bRecoveryFailed )
            {
                PropsChanged = SolvePropCollisions( ContactObjects );
            }
        }

        if( !m_bRecoveryFailed && ( ( m_Position != BeforeExternal ) || PropsChanged ) )
        {
            const xbool WasGrounded = IsGrounded();
            if( UsesGravity() && ( m_MotionVelocity.GetY() <= 0.0f ) &&
                UpdateSupport( WasGrounded ? m_GroundSnapDistance : m_ContactOffset ) )
            {
                m_StepOffset += WasGrounded ? m_Support.Position.GetY() - m_Position.GetY() : 0.0f;
                m_Position              = m_Support.Position;
                if( !WasGrounded )
                {
                    HandleLanding( -m_MotionVelocity.GetY() );
                }
                m_MotionVelocity.GetY() = 0.0f;
            }
            else if( WasGrounded && !IsGrounded() )
            {
                LeavePlatform( TRUE );
            }
        }
    }

    const vector3 ActualDelta    = m_Position - StartPosition;
    vector3       RequestedDelta = MoveTo - StartPosition;
    vector3       ProgressDelta  = ActualDelta;
    if( UsesGravity() )
    {
        RequestedDelta.GetY() = 0.0f;
        ProgressDelta.GetY()  = 0.0f;
    }
    m_MovementProgress.Advance( RequestedDelta, ProgressDelta, DeltaTime );
    m_WorldVelocity = ( ActualDelta + m_PlatformDisplacement ) / DeltaTime;
    m_PlatformDisplacement.Zero();
    NotifyContacts( ContactObjects );
}

//=========================================================================

void character_physics::HandleLanding( f32 ImpactSpeed )
{
    m_Landed       = TRUE;
    m_LandingSpeed = x_max( m_LandingSpeed, ImpactSpeed );
    if( HasLaunchResponse() )
    {
        m_MotionResponse    = CharacterMotionResponse::None;
        m_LastImpulseSource = NULL_GUID;
    }
}

//=========================================================================

void character_physics::CommitMovement( const CharacterMovementResult& Result )
{
    m_Position = Result.Position;
    m_Support  = Result.Ground;
    m_bNavCollided |= Result.Collided;
    m_StepOffset += Result.StepOffset;
}

//=========================================================================

xbool character_physics::SolvePropCollisions( const xarray<CharacterMovementHit>& Contacts )
{
    object* pOwner = g_ObjMgr.GetObjectByGuid( m_Guid );
    if( !pOwner || !pOwner->IsKindOf( actor::GetRTTI() ) ||
        ( g_ObjMgr.GetFirst( object::TYPE_COKE_CAN ) == SLOT_NULL ) )
    {
        return FALSE;
    }

    bbox Bounds = GetBBox();
    Bounds.Translate( m_Position );
    Bounds.Inflate( m_ContactOffset, m_ContactOffset, m_ContactOffset );
    xarray<guid> Props;
    g_ObjMgr.SelectBBox( object::ATTR_COLLIDABLE, Bounds, object::TYPE_COKE_CAN );
    for( slot_id Slot = g_ObjMgr.StartLoop(); Slot != SLOT_NULL; Slot = g_ObjMgr.GetNextResult( Slot ) )
    {
        Props.Append( g_ObjMgr.GetObjectBySlot( Slot )->GetGuid() );
    }
    g_ObjMgr.EndLoop();

    const vector3 Velocity = GetContactVelocity();
    xbool         Changed  = FALSE;
    for( s32 i = 0; i < Props.GetCount(); i++ )
    {
        object* pObject = g_ObjMgr.GetObjectByGuid( Props[i] );
        if( pObject )
        {
            coke_can& Can             = coke_can::GetSafeType( *pObject );
            vector3   ContactVelocity = Velocity;
            for( s32 j = 0; j < Contacts.GetCount(); j++ )
            {
                if( Contacts[j].Object == Props[i] )
                {
                    ContactVelocity = Contacts[j].Velocity;
                    break;
                }
            }
            Changed |= Can.ApplyActorConstraints( m_Position, m_NavCollisionRadius, m_NavCollisionCurrentHeight,
                                                   m_ContactOffset, ContactVelocity );
        }
    }
    return Changed;
}

//=========================================================================

void character_physics::CollectPermeable( object* pActor, const vector3& StartPos, const vector3& EndPos )
{
    ASSERT( pActor );
    ASSERT( pActor->IsKindOf( actor::GetRTTI() ) );

    if( pActor->IsKindOf( character::GetRTTI() ) )
    {
        // We are assuming this actor is a character!

        s32 i, j;

        guid NewGuid[CHARACTER_PHYSICS_MAX_TRIGGERS] = { 0 };
        s32  nNewGuids                               = 0;

        // Do select bbox around actor using an inflated bbox (necessary for doors)
        bbox BBox = GetBBox();
        BBox.Translate( EndPos );

        bbox InflatedBBox = BBox;
        InflatedBBox.Inflate( 350.0f, 0.0f, 350.0f );

        // Handle special-case permeable object interaction
        g_ObjMgr.SelectBBox( object::ATTR_COLLIDABLE | object::ATTR_COLLISION_PERMEABLE, InflatedBBox,
                             object::TYPE_ALL_TYPES );
        s32 ObjectSlot;
        for( ObjectSlot = g_ObjMgr.StartLoop(); ObjectSlot != SLOT_NULL;
             ObjectSlot = g_ObjMgr.GetNextResult( ObjectSlot ) )
        {
            object* pObject = g_ObjMgr.GetObjectBySlot( ObjectSlot );
            ASSERT( pObject );

            switch( pObject->GetType() )
            {
            case object::TYPE_TRIGGER_EX:
                // triggers just get added to a guid list if they have intersected with
                // our non-inflated bbox...we'll deal with the actual collision in a moment
                if( BBox.Intersect( pObject->GetBBox() ) )
                {
                    if( nNewGuids < CHARACTER_PHYSICS_MAX_TRIGGERS )
                    {
                        NewGuid[nNewGuids++] = pObject->GetGuid();
                    }
                }
                break;
            case object::TYPE_DOOR:
            {
                // wake up the door so it knows to start looking for us
                door& DoorObj = door::GetSafeType( *pObject );
                DoorObj.WakeUp();
            }
            break;
            case object::TYPE_DAMAGE_FIELD:
                // handle the damage field collision
                pObject->OnColNotify( *pActor );
                break;
            default:
                // nothing to do
                break;
            }
        }
        g_ObjMgr.EndLoop();

        // Look for any NEW triggers
        for( i = 0; i < nNewGuids; i++ )
        {
            for( j = 0; j < m_nTriggerGuids; j++ )
            {
                if( m_TriggerGuid[j] == NewGuid[i] )
                {
                    break;
                }
            }

            // if we did not find the new guid in the triggerguid list
            // then we have just entered it and should notify the object
            if( j == m_nTriggerGuids )
            {
                object* pObject = g_ObjMgr.GetObjectByGuid( NewGuid[i] );
                if( pObject )
                {
                    pObject->OnColNotify( *pActor );
                }
            }
        }

        // Copy new triggers into old triggers
        for( i = 0; i < nNewGuids; i++ )
        {
            m_TriggerGuid[i] = NewGuid[i];
        }
        m_nTriggerGuids = nNewGuids;
    }
    else
    {
        // We are assuming this actor is a player!

        // Walk from where we were to where we are.
        vector3 Start = StartPos;
        vector3 Stop  = EndPos;

        SetupPlayerCollisionCheck( Start, Stop );
        g_CollisionMgr.SetMaxCollisions( MAX_COLLISION_MGR_COLLISIONS );

        // Check the pickups here.
        g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_COLLISION_PERMEABLE );

        for( s32 i = 0; i < g_CollisionMgr.m_nCollisions; i++ )
        {
            object* pObj = g_ObjMgr.GetObjectByGuid( g_CollisionMgr.m_Collisions[i].ObjectHitGuid );
            pObj->OnColNotify( *pActor );
        }
    }
}
//=========================================================================

CharacterMovementSettings character_physics::GetMovementSettings( void ) const
{
    CharacterMovementSettings Settings;
    Settings.Radius             = m_NavCollisionRadius;
    Settings.Height             = m_NavCollisionCurrentHeight;
    Settings.StepHeight         = m_StepHeight;
    Settings.GroundSnapDistance = m_GroundSnapDistance;
    Settings.GroundTolerance    = m_GroundTolerance;
    Settings.ContactOffset      = m_ContactOffset;
    Settings.SlopeAngle         = m_SlopeAngle;
    Settings.WorldCollision     = m_bLocoCollisionOn;
    return Settings;
}

//=========================================================================

s32 character_physics::Sweep( const vector3& Start, const vector3& End, f32 Radius, f32 Height,
                              CharacterMovementContact* Contacts, s32 MaxContacts )
{
    g_CollisionMgr.CapsuleSetup( m_Guid, Start, End, Radius, Height );
    g_CollisionMgr.SetMaxCollisions( MaxContacts );
    g_CollisionMgr.DoNotRemoveDuplicateGuids();
    g_CollisionMgr.AddToIgnoreList( m_PlatformCollisionIgnoreList, m_nPlatformsToIgnore );
    if( m_IgnoreGuid )
    {
        g_CollisionMgr.AddToIgnoreList( &m_IgnoreGuid, 1 );
    }

    object*   pObject = g_ObjMgr.GetObjectByGuid( m_Guid );
    const u32 Attributes =
        pObject && pObject->IsKindOf( player::GetRTTI() ) ? object::ATTR_BLOCKS_PLAYER : object::ATTR_BLOCKS_CHARACTER;
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, Attributes,
                                    object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );
    for( s32 i = 0; i < g_CollisionMgr.m_nCollisions; i++ )
    {
        const collision_mgr::collision& Hit = g_CollisionMgr.m_Collisions[i];
        Contacts[i].T                       = Hit.T;
        Contacts[i].Point                   = Hit.Point;
        Contacts[i].Normal                  = Hit.SlipPlane.Normal;
        Contacts[i].SurfaceNormal           = Hit.Plane.Normal;
        Contacts[i].Object                  = Hit.ObjectHitGuid;
        Contacts[i].PrimitiveKey            = Hit.PrimitiveKey;
        Contacts[i].HitTriangleEdge         = Hit.HitTriangleEdge;
    }
    return g_CollisionMgr.m_nCollisions;
}

//=========================================================================

xbool character_physics::UpdateSupport( f32 Distance )
{
    if( !UsesGravity() || ( m_MotionVelocity.GetY() > 0.0f ) )
    {
        m_Support.State = CharacterSupportState::None;
        return FALSE;
    }

    return CharacterMovement::FindSupport( *this, GetMovementSettings(), m_Position, Distance, m_Support );
}

//=========================================================================

xbool character_physics::SetCrouchParametric( f32 NormalizePercent )
{
    ASSERT( ( NormalizePercent >= 0.0f ) && ( NormalizePercent <= 1.0f ) );
    const f32 Height = m_NavCollisionHeight - NormalizePercent * m_NavCollisionCrouchOffset;
    ASSERT( Height >= m_NavCollisionRadius * 2.0f );
    vector3 Position = m_Position;
    if( IsAirborne() )
    {
        Position.GetY() += m_NavCollisionCurrentHeight - Height;
    }

    vector3 Correction;
    if( GetPenetration( Position, m_NavCollisionRadius, Height, Correction ) )
    {
        return FALSE;
    }

    if( Height > m_NavCollisionCurrentHeight )
    {
        const vector3 Start = m_Position + vector3( 0.0f, m_NavCollisionCurrentHeight - m_NavCollisionRadius, 0.0f );
        const vector3 End   = Position + vector3( 0.0f, Height - m_NavCollisionRadius, 0.0f );
        g_CollisionMgr.CapsuleSetup( m_Guid, Start - vector3( 0.0f, m_NavCollisionRadius, 0.0f ),
                                     End - vector3( 0.0f, m_NavCollisionRadius, 0.0f ), m_NavCollisionRadius,
                                     m_NavCollisionRadius * 2.0f );
        g_CollisionMgr.UseLowPoly();
        object*   pObject    = g_ObjMgr.GetObjectByGuid( m_Guid );
        const u32 Attributes = pObject && pObject->IsKindOf( player::GetRTTI() ) ? object::ATTR_BLOCKS_PLAYER
                                                                                 : object::ATTR_BLOCKS_CHARACTER;
        g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, Attributes,
                                        object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );
        if( g_CollisionMgr.m_nCollisions )
        {
            return FALSE;
        }
    }

    m_NavCollisionCurrentHeight = Height;
    m_Position                  = Position;
    return TRUE;
}
//=========================================================================

xbool character_physics::Jump( f32 Vel )
{
    if( !UsesGravity() || !IsGrounded() || !x_isvalid( Vel ) || ( Vel <= 0.0f ) )
    {
        return FALSE;
    }

    m_MotionVelocity.GetY() = Vel;
    DetachFromGround();
    return TRUE;
}

//=========================================================================

void character_physics::SetPosition( const vector3& Position )
{
    ASSERT( Position.IsValid() );
    if( Position != m_Position )
    {
        m_Position      = Position;
        m_Support.State = CharacterSupportState::None;
        LeavePlatform( FALSE );
    }
}

//=========================================================================

void character_physics::SetUseGravity( xbool Enable )
{
    if( m_bUseGravity != Enable )
    {
        m_bUseGravity           = Enable;
        m_Support.State         = CharacterSupportState::None;
        m_MotionVelocity.GetY() = 0.0f;
        LeavePlatform( FALSE );
    }
}

//=========================================================================

void character_physics::SetLocoGravityOn( xbool Enable )
{
    if( m_bLocoGravityOn != Enable )
    {
        m_bLocoGravityOn        = Enable;
        m_Support.State         = CharacterSupportState::None;
        m_MotionVelocity.GetY() = 0.0f;
        LeavePlatform( FALSE );
    }
}

//=========================================================================

void character_physics::ApplyImpulse( const vector3& Delta )
{
    ASSERT( Delta.IsValid() );
    m_MotionVelocity += Delta;
    if( Delta.LengthSquared() > F32_MIN )
    {
        if( !HasLaunchResponse() )
        {
            m_MotionResponse     = CharacterMotionResponse::Impulse;
            m_ExternalAirControl = m_AirControl;
            m_LastImpulseSource  = NULL_GUID;
        }
    }
    if( Delta.GetY() > 0.0f )
    {
        DetachFromGround();
    }
}

//=========================================================================

void character_physics::SetMotionVelocity( const vector3& Velocity )
{
    ASSERT( Velocity.IsValid() );
    m_MotionVelocity = Velocity;
    if( Velocity.LengthSquared() <= F32_MIN )
    {
        ClearMotionVelocity();
    }
    else if( !HasLaunchResponse() )
    {
        m_MotionResponse     = CharacterMotionResponse::Impulse;
        m_ExternalAirControl = m_AirControl;
        m_LastImpulseSource  = NULL_GUID;
    }
    if( Velocity.GetY() > 0.0f )
    {
        DetachFromGround();
    }
}

//=========================================================================

void character_physics::FlingWithVelocity( const vector3& Velocity, f32 AirControl, xbool FlingOnly, xbool ReflingOnly,
                                           guid FlingGuid )
{
    ApplyFling( Velocity, AirControl, FlingOnly, ReflingOnly, TRUE, FlingGuid );
}

//=========================================================================

void character_physics::FlingWithAcceleration( const vector3& Acceleration, f32 DeltaTime, f32 AirControl,
                                               xbool FlingOnly, xbool ReflingOnly )
{
    if( !x_isvalid( DeltaTime ) || ( DeltaTime < 0.0f ) )
    {
        ASSERT( FALSE );
        return;
    }

    if( DeltaTime <= F32_MIN )
    {
        return;
    }

    if( ApplyFling( Acceleration * DeltaTime, AirControl, FlingOnly, ReflingOnly, FALSE, NULL_GUID ) )
    {
        // Field acceleration replaces world gravity for this interval.
        m_FlingAccelerationTime = x_max( m_FlingAccelerationTime, DeltaTime );
    }
}

//=========================================================================

xbool character_physics::ApplyFling( const vector3& Velocity, f32 AirControl, xbool FlingOnly, xbool ReflingOnly,
                                    xbool ReplaceVelocity, guid FlingGuid )
{
    if( FlingOnly && HasLaunchResponse() )
    {
        return FALSE;
    }

    if( ReflingOnly && !HasLaunchResponse() )
    {
        return FALSE;
    }

    if( ReplaceVelocity && ( FlingGuid != NULL_GUID ) && HasLaunchResponse() && ( m_LastImpulseSource == FlingGuid ) )
    {
        return FALSE;
    }

    if( ReplaceVelocity )
    {
        m_MotionVelocity        = Velocity;
        m_LastImpulseSource     = FlingGuid;
        m_FlingAccelerationTime = 0.0f;
    }
    else
    {
        m_MotionVelocity += Velocity;
    }

    m_ExternalAirControl = x_clamp( AirControl, 0.0f, 1.0f );
    m_MotionResponse     = CharacterMotionResponse::Launch;
    if( m_MotionVelocity.GetY() > 0.0f )
    {
        DetachFromGround();
    }
    return TRUE;
}

//=========================================================================

void character_physics::OnEnumProp( prop_enum& List )
{
    List.PropEnumHeader ( "CharacterPhysics",                        "This is were the physics of the character get descrive", 0 );
    List.PropEnumBool   ( "CharacterPhysics\\IsCollided",            "Tells the system that the object encounter a collision", PROP_TYPE_READ_ONLY );
    List.PropEnumBool   ( "CharacterPhysics\\IsFalling",             "Tells the system that the object is in a falling mode", PROP_TYPE_READ_ONLY );
    List.PropEnumVector3( "CharacterPhysics\\Position",              "The position of the object", PROP_TYPE_READ_ONLY );
    List.PropEnumVector3( "CharacterPhysics\\Velocity",              "The Velocity of the object", PROP_TYPE_READ_ONLY );
    List.PropEnumFloat  ( "CharacterPhysics\\ColHeight",             "This is the physical height of the object in cm. It descrives a cylinder.", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\ColRadius",             "This is the physical width of the object in cm. It descrives a cylinder.", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\ColCrouchOffset",       "This is when in couch how much the Y off the bbox nneds to be subtracted", 0 );
    List.PropEnumBool   ( "CharacterPhysics\\DoPermeable",           "When true objects that are Permeable will be notify of collisions", 0 );
    List.PropEnumBool   ( "CharacterPhysics\\UseGravity",            "Tells the system whether it should use gravery or not", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\GravityAcc",            "This is the gravety acceleration for the physics", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\ContactOffset",         "This is the minimum distance in cm that the character maintains from collision surfaces.", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\GroundTolerance",       "This variable tells the system which height the system will start chking for the ground", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\StepHeight",            "This is the maximum height in cm that the character can step up without jumping.", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\GroundSnapDistance",    "This is the maximum distance in cm that the character can move down to maintain contact with the ground.", 0 );
    List.PropEnumAngle  ( "CharacterPhysics\\SlopeAngle",            "This is the maximum slope angle that the character can stand and walk on. From 0 to 89 degrees.", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\GroundFriction",        "This controls how quickly the character returns to the requested movement after an external impulse while on the ground. In inverse seconds.", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\AirControl",            "While in the jump or fall how much control does the character has in the air. From 0 to 1.", 0 );
    List.PropEnumFloat  ( "CharacterPhysics\\ActorCollisionRadius",  "Our collision radius vs other actors", 0 );
    List.PropEnumBool   ( "CharacterPhysics\\RecoveryFailed",        "Tells the system that the character could not be moved out of an overlap with collision geometry.", PROP_TYPE_READ_ONLY );
}

//=========================================================================

xbool character_physics::OnProperty( prop_query& I )
{
    if( I.IsVar( "CharacterPhysics\\RecoveryFailed" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarBool( m_bRecoveryFailed );
        }
        return TRUE;
    }
    if( I.IsVar( "CharacterPhysics\\IsFalling" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarBool( IsFalling() );
        }
        return TRUE;
    }

    vector3 Position = m_Position;
    if( I.VarVector3( "CharacterPhysics\\Position", Position ) )
    {
        if( !I.IsRead() )
        {
            InitialGroundCheck( Position );
        }
        return TRUE;
    }

    vector3 Velocity = m_MotionVelocity;
    if( I.VarVector3( "CharacterPhysics\\Velocity", Velocity ) )
    {
        if( !I.IsRead() )
        {
            SetMotionVelocity( Velocity );
        }
        return TRUE;
    }

    xbool UseGravity = m_bUseGravity;
    if( I.VarBool( "CharacterPhysics\\UseGravity", UseGravity ) )
    {
        if( !I.IsRead() )
        {
            SetUseGravity( UseGravity );
        }
        return TRUE;
    }

    if( I.VarBool( "CharacterPhysics\\IsCollided", m_bNavCollided ) ||
        I.VarBool( "CharacterPhysics\\DoPermeable", m_bHandlePermeable ) ||
        I.VarFloat( "CharacterPhysics\\GravityAcc", m_GravityAcceleration ) )
    {
        return TRUE;
    }

    if( I.VarFloat( "CharacterPhysics\\ColHeight", m_NavCollisionHeight ) )
    {
        if( !I.IsRead() )
        {
            SetColHeight( m_NavCollisionHeight );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\ColRadius", m_NavCollisionRadius ) )
    {
        if( !I.IsRead() )
        {
            SetColRadius( m_NavCollisionRadius );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\ColCrouchOffset", m_NavCollisionCrouchOffset ) )
    {
        if( !I.IsRead() )
        {
            SetColCrouchOffset( m_NavCollisionCrouchOffset );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\ContactOffset", m_ContactOffset ) )
    {
        if( !I.IsRead() )
        {
            m_ContactOffset = x_clamp( m_ContactOffset, 0.1f, m_NavCollisionRadius * 0.25f );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\GroundFriction", m_GroundFriction ) )
    {
        if( !I.IsRead() )
        {
            m_GroundFriction = x_max( 0.0f, m_GroundFriction );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\GroundTolerance", m_GroundTolerance ) )
    {
        if( !I.IsRead() )
        {
            m_GroundTolerance = x_max( m_GroundTolerance, m_ContactOffset );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\StepHeight", m_StepHeight ) )
    {
        if( !I.IsRead() )
        {
            m_StepHeight = x_max( m_StepHeight, 0.0f );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\GroundSnapDistance", m_GroundSnapDistance ) )
    {
        if( !I.IsRead() )
        {
            m_GroundSnapDistance = x_max( m_GroundSnapDistance, 0.0f );
        }
    }
    else if( I.IsVar( "CharacterPhysics\\SlopeAngle" ) )
    {
        radian Angle = m_SlopeAngle;
        I.VarAngle( "CharacterPhysics\\SlopeAngle", Angle );
        if( !I.IsRead() )
        {
            m_SlopeAngle = CharacterMovementSettings::ClampSlopeAngle( Angle );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\AirControl", m_AirControl ) )
    {
        if( !I.IsRead() )
        {
            m_AirControl = x_clamp( m_AirControl, 0.0f, 1.0f );
        }
    }
    else if( I.VarFloat( "CharacterPhysics\\ActorCollisionRadius", m_ActorCollisionRadius ) )
    {
        if( !I.IsRead() )
        {
            m_ActorCollisionRadius = x_max( m_ActorCollisionRadius, 0.0f );
        }
    }
    else
    {
        return FALSE;
    }
    return TRUE;
}
//=========================================================================

void character_physics::ApplyCollision( void )
{
    g_CollisionMgr.StartApply( m_Guid );
    g_CollisionMgr.ApplyCapsule( m_Position + vector3( 0.0f, m_NavCollisionRadius, 0.0f ),
                                 m_Position + vector3( 0.0f, m_NavCollisionCurrentHeight - m_NavCollisionRadius, 0.0f ),
                                 m_NavCollisionRadius, object::MAT_TYPE_FLESH );
    g_CollisionMgr.EndApply();
}

//=========================================================================

#ifndef X_RETAIL
void character_physics::RenderCollision( void )
{
    const f32 Radius = m_NavCollisionRadius;
    const f32 Length = m_NavCollisionCurrentHeight - Radius * 2.0f;
    render::debug::Sphere( m_Position + vector3( 0.0f, Radius, 0.0f ), Radius );
    if( Length > 0.0f )
    {
        render::debug::Sphere( m_Position + vector3( 0.0f, Radius + Length, 0.0f ), Radius );
        render::debug::Cylinder( m_Position + vector3( 0.0f, Radius + Length * 0.5f, 0.0f ), Radius, Length, 16,
                                 XCOLOR_WHITE, FALSE );
    }
}
#endif

//=========================================================================

void character_physics::CatchUpWithRidingPlatform( f32 DeltaTime )
{
    if( !g_UpdateRidingPlatform )
    {
        return;
    }

    // Check that we are standing on a platform
    if( m_MovingPlatformGuid == 0 )
    {
        return;
    }

    // Get access to the previous surface.  Does it still exist?
    anim_surface* pPlatform = (anim_surface*)g_ObjMgr.GetObjectByGuid( m_MovingPlatformGuid );
    if( !pPlatform )
    {
        LeavePlatform( FALSE );
        m_Support.State = CharacterSupportState::None;
        return;
    }

    // Get access to the actor
    actor* pActor = (actor*)g_ObjMgr.GetObjectByGuid( m_Guid );
    if( !pActor )
    {
        return;
    }

    // We need to move the amount that the current platform did.
    {
        // Get new matrix
        matrix4 NewMovingPlatformL2W;
        pPlatform->GetBoneL2W( m_MovingPlatformBone, NewMovingPlatformL2W );

        vector3 DeltaPos;
        radian  DeltaYaw;
        ComputePlatformMotion( m_OldMovingPlatformL2W, NewMovingPlatformL2W, pActor->GetPosition(), DeltaPos,
                               DeltaYaw );

        // Remember velocity
        if( x_isvalid( DeltaTime ) && ( DeltaTime > F32_MIN ) )
        {
            m_OldMovingPlatformVelocity = DeltaPos / DeltaTime;
        }
        else
        {
            m_OldMovingPlatformVelocity.Zero();
        }

        // Remember new platform L2W
        m_OldMovingPlatformL2W = NewMovingPlatformL2W;

        // Get current position
        const matrix4& L2W     = pActor->GetL2W();
        vector3        CurrPos = L2W.GetTranslation();

        // Update position
        m_PlatformCollisionIgnoreList[0] = m_MovingPlatformGuid;
        m_nPlatformsToIgnore             = 1;

        const xbool Moved    = MoveDisplacement( CurrPos, DeltaPos );
        m_nPlatformsToIgnore = 0;
        if( !Moved )
        {
            return;
        }

        m_PlatformDisplacement += CurrPos - m_Position;
        m_Support.Position += CurrPos - m_Position;
        m_Position = CurrPos;
        pActor->OnRidingPlatformMove( CurrPos, DeltaYaw );
    }
}

//=========================================================================

void character_physics::ResetRidingPlatforms( void )
{
    m_MovingPlatformGuid = 0;
    m_MovingPlatformBone = -1;
    m_OldMovingPlatformVelocity.Zero();
    m_OldMovingPlatformL2W.Identity();
}

//=========================================================================

void character_physics::WatchForRidingPlatform( void )
{
    if( !g_UpdateRidingPlatform )
    {
        return;
    }

    anim_surface* pPlatform     = NULL;
    s32           iPlatformBone = -1;
    guid          PlatformGuid  = 0;

    // Now check and see if we are still standing on any platform
    if( IsGrounded() && m_Support.Contact.Object )
    {
        object* pObject = g_ObjMgr.GetObjectByGuid( m_Support.Contact.Object );
        if( pObject && ( pObject->GetAttrBits() & object::ATTR_ACTOR_RIDEABLE ) )
        {
            PlatformGuid  = m_Support.Contact.Object;
            pPlatform     = (anim_surface*)pObject;
            iPlatformBone = GetBoneIndexFromPolyCachePrimKey( m_Support.Contact.PrimitiveKey );
        }
    }

    // Handle transition if there is one
    if( ( PlatformGuid == 0 ) || ( PlatformGuid != m_MovingPlatformGuid ) || ( iPlatformBone != m_MovingPlatformBone ) )
    {
        // Detach from old platform
        if( ( PlatformGuid != m_MovingPlatformGuid ) || ( iPlatformBone != m_MovingPlatformBone ) )
        {
            LeavePlatform( !IsGrounded() );
        }

        // Is new ground a moving platform?
        if( PlatformGuid != 0 )
        {
            // Attach to platform
            m_MovingPlatformGuid = PlatformGuid;
            m_MovingPlatformBone = iPlatformBone;
            pPlatform->GetBoneL2W( iPlatformBone, m_OldMovingPlatformL2W );
        }
    }
}

//=========================================================================

void character_physics::SolvePlatformCollisions( void )
{
    if( !g_ApplyPlatformCollision )
    {
        return;
    }

    vector3 Correction;
    for( s32 i = 0; i < 8; i++ )
    {
        guid Platform;
        if( !FindPenetration( m_Position, m_NavCollisionRadius, m_NavCollisionCurrentHeight,
                              object::ATTR_ACTOR_RIDEABLE, Correction, &Platform ) )
        {
            return;
        }

        const vector3 OldPosition        = m_Position;
        m_PlatformCollisionIgnoreList[0] = Platform;
        m_nPlatformsToIgnore             = 1;
        const xbool Moved                = MoveDisplacement( m_Position, Correction );
        m_nPlatformsToIgnore             = 0;
        if( !Moved )
        {
            return;
        }

        if( ( m_Position - OldPosition ).LengthSquared() <= 0.01f )
        {
            object* pObject = g_ObjMgr.GetObjectByGuid( m_Guid );
            if( pObject )
            {
                pain PainEvent;
                PainEvent.Setup( "GENERIC_LETHAL", 0, pObject->GetBBox().GetCenter() );
                PainEvent.SetDirectHitGuid( m_Guid );
                PainEvent.ApplyToObject( pObject );
            }
            return;
        }
    }
}

//=========================================================================

void character_physics::NotifyContacts( const xarray<CharacterMovementHit>& Objects )
{
    object* pActor = g_ObjMgr.GetObjectByGuid( m_Guid );
    if( !pActor )
    {
        return;
    }
    for( s32 i = 0; i < Objects.GetCount(); i++ )
    {
        object* pHit = g_ObjMgr.GetObjectByGuid( Objects[i].Object );
        if( pHit )
        {
            pHit->OnColNotify( *pActor );
        }
    }
}

//=========================================================================

xbool character_physics::MoveDisplacement( vector3& Position, const vector3& Delta )
{
    const vector3 StartPosition = Position;
    const CharacterMovementResult Result =
        CharacterMovement::Move( *this, GetMovementSettings(), Position, Delta, 1.0f, FALSE, FALSE );
    Position = Result.Position;
    if( Result.RecoveryFailed )
    {
        HandleRecoveryFailure();
        return FALSE;
    }
    m_bNavCollided |= Result.Collided;
    NotifyContacts( Result.Hits );

    object* pActor = g_ObjMgr.GetObjectByGuid( m_Guid );
    if( m_bHandlePermeable && pActor )
    {
        CollectPermeable( pActor, StartPosition, Position );
    }
    return TRUE;
}

//=========================================================================

void character_physics::Push( const vector3& PushVector )
{
    if( !MoveDisplacement( m_Position, PushVector ) )
    {
        return;
    }
    if( m_bLocoCollisionOn && !CharacterMovement::Recover( *this, GetMovementSettings(), m_Position ) )
    {
        HandleRecoveryFailure();
    }
}
//=========================================================================

void character_physics::SetCollisionIgnoreGuid( guid ignoreGuid )
{
    m_IgnoreGuid = ignoreGuid;
}

//=========================================================================

xbool character_physics::GetPenetration( const vector3& Position, f32 Radius, f32 Height, vector3& Correction )
{
    object*   pObject = g_ObjMgr.GetObjectByGuid( m_Guid );
    const u32 Attributes =
        pObject && pObject->IsKindOf( player::GetRTTI() ) ? object::ATTR_BLOCKS_PLAYER : object::ATTR_BLOCKS_CHARACTER;
    return FindPenetration( Position, Radius, Height, Attributes, Correction );
}

//=========================================================================

xbool character_physics::FindPenetration( const vector3& Position, f32 Radius, f32 Height, u32 Attributes,
                                          vector3& Correction, guid* pObject )
{
    const vector3 A = Position + vector3( 0.0f, Radius, 0.0f );
    const vector3 B = Position + vector3( 0.0f, Height - Radius, 0.0f );
    bbox          Bounds( vector3( -Radius, 0.0f, -Radius ), vector3( Radius, Height, Radius ) );
    Bounds.Translate( Position );
    g_PolyCache.BuildClusterList( Bounds, object::TYPE_ALL_TYPES, Attributes,
                                  object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );
    f32 Deepest = 0.0f;
    for( s32 i = 0; i < g_PolyCache.m_nClusters; i++ )
    {
        const poly_cache::cluster& Cluster = *g_PolyCache.m_ClusterList[i];
        if( ( Cluster.Guid == m_Guid ) || ( Cluster.Guid == m_IgnoreGuid ) )
        {
            continue;
        }
        xbool Ignore = FALSE;
        for( s32 j = 0; j < m_nPlatformsToIgnore; j++ )
        {
            Ignore |= ( Cluster.Guid == m_PlatformCollisionIgnoreList[j] );
        }
        if( Ignore )
        {
            continue;
        }

        for( s32 j = 0; j < (s32)Cluster.nQuads; j++ )
        {
            const bbox& QuadBounds = *(const bbox*)&Cluster.pBounds[j];
            if( !Bounds.Intersect( QuadBounds ) )
            {
                continue;
            }
            const poly_cache::cluster::quad& Quad      = Cluster.pQuad[j];
            const s32                        Triangles = ( Cluster.pBounds[j].Flags & BOUNDS_IS_QUAD ) ? 2 : 1;
            vector3                          Points[4];
            for( s32 k = 0; k < Triangles + 2; k++ )
            {
                ASSERT( Quad.iP[k] < Cluster.nPoints );
                Points[k] = Cluster.pPoint[Quad.iP[k]];
            }
            for( s32 k = 0; k < Triangles; k++ )
            {
                const vector3 Triangle[3] = { Points[0], Points[k + 1], Points[k + 2] };
                vector3       Move;
                if( ComputeCapsuleTriPenetration( Triangle, A, B, Radius, Move ) && ( Move.LengthSquared() > Deepest ) )
                {
                    Deepest    = Move.LengthSquared();
                    Correction = Move;
                    if( pObject )
                    {
                        *pObject = Cluster.Guid;
                    }
                }
            }
        }
    }
    return Deepest > 0.0f;
}

//=========================================================================

xbool character_physics::CanMoveTo( const vector3& Position )
{
    const xbool RequireGround = IsGrounded();
    return CharacterMovement::CanMoveTo( *this, GetMovementSettings(), m_Position, Position, RequireGround );
}

//=========================================================================

void character_physics::SetupPlayerCollisionCheck( const vector3& Start, const vector3& End )
{
    g_CollisionMgr.CapsuleSetup( m_Guid, Start, End, m_NavCollisionRadius, m_NavCollisionCurrentHeight );
}

//=========================================================================

void character_physics::LeavePlatform( xbool InheritVelocity )
{
    if( InheritVelocity && ( m_MovingPlatformGuid != NULL_GUID ) )
    {
        vector3 Inherited = m_OldMovingPlatformVelocity;
        Inherited.GetY()  = x_max( 0.0f, Inherited.GetY() );
        m_MotionVelocity += Inherited;
    }
    ResetRidingPlatforms();
}

//=========================================================================

void character_physics::DetachFromGround( void )
{
    m_Support.State = CharacterSupportState::None;
    LeavePlatform( TRUE );
}

//=========================================================================

void character_physics::InitialGroundCheck( const vector3& Position )
{
    m_Position = Position;
    m_Support  = {};
    ClearMotionVelocity();
    ResetMovementProgress();
    ResetRidingPlatforms();
    m_WorldVelocity.Zero();
    m_PlatformDisplacement.Zero();
    m_bRecoveryFailed = FALSE;
    m_StepOffset      = 0.0f;
    m_Landed          = FALSE;
    m_LandingSpeed    = 0.0f;
    UpdateSupport( m_ContactOffset );
}

//=========================================================================

void character_physics::HandleRecoveryFailure( void )
{
    m_bRecoveryFailed = TRUE;
    m_bNavCollided    = TRUE;
    ClearMotionVelocity();
    m_Support    = {};
    m_StepOffset = 0.0f;
    LeavePlatform( FALSE );
}
