//=========================================================================
//
//  PlayerMovement.cpp
//
//=========================================================================

#include "PlayerMovement.hpp"
#include "Player.hpp"
#include "CollisionMgr/CollisionPrimatives.hpp"
#include "Objects/Ladders/Ladder_Field.hpp"
#include "Objects/Turret.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"

//=========================================================================

static const f32 ZERO = 0.00001f;
static const f32 s_ArmsDampen                 = 0.2f;
static const f32 s_ArmReturnForceMultiplier   = 75.0f;
static const f32 s_MovementReferenceFrameRate = 30.0f;

static f32  ComputeSoftLean             ( f32 LeanAmount );
static f32  ComputeLean                 ( f32 SoftLeanAmount );
static f32  ComputeLeanValueForPosition ( const vector3& Start,
                                           const vector3& HitPoint,
                                           xbool bLeaningRight );
static void HideObject                  ( object* pObject );
static void UnhideObject                ( object* pObject );

void player::Push( const vector3& PushVector )
{
    if ( !m_bInTurret )
    {
        m_Physics.SetPosition( GetPosition() );
        m_Physics.Push(PushVector);
        OnMove( m_Physics.GetPosition() );
    }
}

void player::UpdateWeaponPullback( void )
{
    if ( m_CurrentAnimState == ANIM_STATE_DEATH )
    {
        m_WeaponCollisionOffset.Zero();
    }
    else{
        //
        // Check to see if the gun will collide with something. If so, move 
        // it back using m_WeaponCollisionOffset.
        //
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if ( pWeapon && (pWeapon->GetType() != TYPE_WEAPON_MUTATION) )
        {
            vector3 FirePos;
            vector3 SingleCollisionOffset( 0.0f, 0.0f, 0.0f );
            vector3 LeftCollisionOffset  ( 0.0f, 0.0f, 0.0f );
            vector3 RightCollisionOffset  ( 0.0f, 0.0f, 0.0f );
            f32     SingleCollisionScalar = 0.0f;
            f32     LeftCollisionScalar   = 0.0f;
            f32     RightCollisionScalar  = 0.0f;
            const vector3 ToWeapon             ( m_EyesPosition - pWeapon->GetPosition() );

            if ( pWeapon->GetFiringBonePosition( FirePos, new_weapon::FIRE_POINT_DEFAULT ) )
            {
                FirePos += ToWeapon;
                SingleCollisionOffset = GetWeaponCollisionOffset( pWeapon->GetGuid(), FirePos );
                SingleCollisionScalar = m_WeaponCollisionOffsetScalar;
            }

            if ( pWeapon->GetFiringBonePosition( FirePos, new_weapon::FIRE_POINT_LEFT ) )
            {
                FirePos += ToWeapon;
                LeftCollisionOffset = GetWeaponCollisionOffset( pWeapon->GetGuid(), FirePos );
                LeftCollisionScalar = m_WeaponCollisionOffsetScalar;
            }

            if ( pWeapon->GetFiringBonePosition( FirePos, new_weapon::FIRE_POINT_RIGHT ) )
            {
                FirePos += ToWeapon;
                RightCollisionOffset = GetWeaponCollisionOffset( pWeapon->GetGuid(), FirePos );
                RightCollisionScalar = m_WeaponCollisionOffsetScalar;
            }

            if ( SingleCollisionScalar > LeftCollisionScalar )
            {
                if ( SingleCollisionScalar > RightCollisionScalar )
                {
                    m_WeaponCollisionOffset         = SingleCollisionOffset;
                    m_WeaponCollisionOffsetScalar   = SingleCollisionScalar;
                }
                else
                {
                    m_WeaponCollisionOffset         = RightCollisionOffset;
                    m_WeaponCollisionOffsetScalar   = RightCollisionScalar;
                }
            }
            else
            {
                if ( LeftCollisionScalar > RightCollisionScalar )
                {
                    m_WeaponCollisionOffset         = LeftCollisionOffset;
                    m_WeaponCollisionOffsetScalar   = LeftCollisionScalar;
                }
                else
                {
                    m_WeaponCollisionOffset         = RightCollisionOffset;
                    m_WeaponCollisionOffsetScalar   = RightCollisionScalar;
                }
            }
        }
        else
        {
            m_WeaponCollisionOffset.Zero();
        }
    }
    m_LastWeaponCollisionOffsetScalar = m_WeaponCollisionOffsetScalar;


}

void player::MoveAnimPlayer( const vector3& Pos )
{
    m_AnimPlayer.SetPosition( Pos + GetAnimPlayerOffset() + vector3( 0.0f, m_StepView.GetOffset(), 0.0f ) );
}

void player::OnMove( const vector3& NewPosition )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "player::OnMove" );
    if( GetAttrBits() & object::ATTR_DESTROY )
        return;

    m_DeltaPos = NewPosition - GetPosition();
    actor::OnMove( NewPosition );
    m_Physics.SetPosition( NewPosition );
    OnMoveViewPosition( NewPosition );
}

void player::OnMoveViewPosition( const vector3& rNewPos )
{
    (void)rNewPos;
    vector3 EyesOffSet = m_EyesOffset;

    EyesOffSet.RotateY( m_Yaw );

    vector3 vViewPosition;
    f32 Height = GetBBox().Max.GetY();

    vViewPosition.Set( rNewPos.GetX(), Height, rNewPos.GetZ() );
    vViewPosition += EyesOffSet;

    MoveAnimPlayer( vViewPosition );
}

void player::OnTransform( const matrix4& L2W )
{
    m_StepView.Clear();
    //==========================================================================================
    // Begin code from ghost.cpp
    //==========================================================================================

    actor::OnTransform(L2W);

    //update physics
    m_Physics.SetPosition( L2W.GetTranslation() );

    //==========================================================================================
    // End code from ghost.cpp
    //==========================================================================================

    m_Yaw = L2W.GetRotation().Yaw;
    m_AnimPlayer.SetYaw( m_fCurrentYawOffset + m_Yaw );
    
    OnMoveViewPosition( L2W.GetTranslation() );
    MoveAnimPlayer( m_EyesPosition );
}

void player::OnRidingPlatformMove( const vector3& NewPos, radian DeltaYaw )
{
    SetYaw( x_ModAngle2( m_Yaw + DeltaYaw ) );
    OnMove( NewPos );
}

void player::UpdateMovement( f32 DeltaTime )
{
    if( !x_isvalid( DeltaTime ) || (DeltaTime < 0.0f) )
    {
        ASSERT( FALSE );
        m_DeltaPos.Zero();
        m_StepView.Clear();
        return;
    }

    if( DeltaTime <= F32_MIN )
    {
        m_DeltaPos.Zero();
        return;
    }

    if( m_LockedView.IsActive() ||
        (IsCinemaRunning() && (GetCinemaCameraGuid() != 0)) )
    {
        m_DeltaPos.Zero();
        m_StepView.Clear();
        OnMoveViewPosition( GetPosition() );
        return;
    }

    if( UpdateLadderMovement( DeltaTime ) )
    {
        m_Movement.Clear();
        m_StepView.Clear();
        OnMoveViewPosition( GetPosition() );
        return;
    }

    vector3 ViewX( 1.0f, 0.0f, 0.0f );
    vector3 ViewZ( 0.0f, 0.0f, 1.0f );
    ViewX.RotateY( m_Yaw );
    ViewZ.RotateY( m_Yaw );

    CalculateRigOffset( DeltaTime );
    EvaluateMovementSpeeds( DeltaTime );
    CalculateStrafeVelocity( ViewX );
    CalculateForwardVelocity( ViewZ );
    ApplyMovementSpeedFactors();

    vector3 MovementVelocity = m_StrafeVelocity + m_ForwardVelocity;
    MovementVelocity *= 1.0f - 0.55f * m_fCurrentCrouchFactor;

    m_DeltaPos = MovementVelocity * DeltaTime;

    if( !m_bInTurret )
    {
        m_Physics.Advance( m_Physics.GetPosition() + m_DeltaPos, DeltaTime );
        m_StepView.Advance( m_Physics.GetStepOffset(), DeltaTime, m_Physics.GetMovementSettings().StepHeight );
    }
    else
        m_StepView.Clear();

    ASSERT( m_ArmsOffset.IsValid() );
    ASSERT( m_ArmsVelocity.IsValid() );

    vector3 ReturnForce( -m_ArmsOffset );
    ReturnForce *= s_ArmReturnForceMultiplier;
    m_ArmsVelocity += ReturnForce * DeltaTime;

    m_ArmsVelocity *= x_pow( 1.0f - s_ArmsDampen,
                             DeltaTime * s_MovementReferenceFrameRate );
    m_ArmsOffset += m_ArmsVelocity * DeltaTime;

    ASSERT( m_ArmsOffset.IsValid() );
    ASSERT( m_ArmsVelocity.IsValid() );

    UpdateArmsOffsetForLean( DeltaTime );
    OnMove( m_Physics.GetPosition() );
}

void player::UpdateCharacterRotation( const f32& DeltaTime )
{
    CalculateLookHorozOffset( DeltaTime );
    CalculateLookVertOffset( DeltaTime );


    //set the pitch and yaw of the rig.
    radian AnimPitch = -m_Pitch - m_CurrentVertRigOffset - m_ShakePitch;
    AnimPitch = MIN( R_89, MAX( -R_89, AnimPitch ) );

    m_AnimPlayer.SetYaw( m_Yaw + m_CurrentHorozRigOffset - m_ShakeYaw );
    m_AnimPlayer.SetPitch( AnimPitch );
    m_AnimPlayer.SetRoll( m_SoftLeanAmount * DEG_TO_RAD( GetTweakF32( "LeanMaxDegrees" ) ) );
}

void player::UpdateCrouchHeight( const f32& rDeltaTime )
{
    if ( m_bIsCrouching  )
    {
        //trying to crouch
        f32 NewCrouchFactor = MIN( 1.f ,  m_fCurrentCrouchFactor + m_fCrouchChangeRate * rDeltaTime );
        if ( m_Physics.SetCrouchParametric( NewCrouchFactor ) )
        {
            m_fCurrentCrouchFactor = NewCrouchFactor;
        }
    }
    else
    if ( m_fCurrentCrouchFactor > 0.f )
    {
        f32 NewCrouchFactor = MAX( 0.f ,  m_fCurrentCrouchFactor - m_fCrouchChangeRate * rDeltaTime );
        if ( m_Physics.SetCrouchParametric( NewCrouchFactor ) )
        {
            m_fCurrentCrouchFactor = NewCrouchFactor;
        }
    }

}

vector3 player::GetPositionWithOffset( eOffsetPos offset )
{
    switch( offset ) 
    {
    case OFFSET_NONE:
        return GetPosition();
        break;
    case OFFSET_CENTER:
        return GetBBox().GetCenter();
        break;
    case OFFSET_AIM_AT:
        return GetBBox().GetCenter() + GetLeanOffset();
        break;
    case OFFSET_EYES:
        return GetEyesPosition();
        break;
    case OFFSET_TOP_OF_BBOX:
        return GetPosition() + vector3( 0.0f, GetBBox().Max.GetY(), 0.0f );
        break;
    default:
        return GetPosition();
        break;
    }
}

const matrix4& player::GetL2W( void ) const
{
    matrix4& L2W = *(matrix4*)(&actor::GetL2W()); // de-constification
    const vector3 Pos( GetPosition() );
    L2W.Identity();
    L2W.RotateY( m_Yaw );
    L2W.Translate( Pos );
    return L2W;
}

bbox player::GetLocalBBox( void ) const
{
    bbox BBox = m_Physics.GetBBox();

    // Take lean into account so leaning ghosts/players can be hit in MP
    f32  LeanDist = x_abs( GetLeanAmount() * 100.0f );
    BBox.Inflate( LeanDist, 0.0f, LeanDist );

    return BBox;
}

bbox player::GetColBBox( void )
{
    // Start with physics bbox
    bbox BBox = m_Physics.GetBBox();

    // Take lean into account so leaning ghosts/players can be hit in MP
    f32  LeanDist = x_abs( GetLeanAmount() * 100.0f );
    BBox.Inflate( LeanDist, 0.0f, LeanDist );

    // Convert into world space
    BBox.Transform( GetL2W() );
    return BBox;
}

void player::OnColCheck( void )
{
#ifndef X_EDITOR
    // For multi-player use bone bboxes so call base class
    if( GameMgr.IsGameMultiplayer() )
    {
        actor::OnColCheck();
        return;
    }
#endif

    vector3 Pos = GetPosition();
    vector3 Offset  ( GetLeanOffset() );
    Offset.GetY() = 0.0f;
    Pos += Offset;

    const f32 Radius = m_Physics.GetColRadius();
    g_CollisionMgr.StartApply( GetGuid() );
    g_CollisionMgr.ApplyCapsule( Pos + vector3( 0.0f, Radius, 0.0f ),
        Pos + vector3( 0.0f, m_Physics.GetColHeight() - Radius, 0.0f ),
        Radius, object::MAT_TYPE_FLESH );
    g_CollisionMgr.EndApply();
}

void player::UpdateLean( f32 LeanValue, f32 DeltaTime )
{
    const lean_state OldLeanState      = m_LeanState;
    const f32        OldLeanAmount     = m_LeanAmount;
    const f32        OldSoftLeanAmount = m_SoftLeanAmount;

    if ( (LeanValue > GetTweakF32( "LeanThreshold" )) && (m_LeanAmount >= 0.0f) )
    {
        // Leaning left
        const f32 ElapsedTime       = m_LeanAmount * GetTweakF32( "LeanTime" );
        const f32 NewElapsedTime    = ElapsedTime + DeltaTime;
        m_LeanAmount                = NewElapsedTime / GetTweakF32( "LeanTime" );
        m_LeanState                 = LEAN_LEFT;
    }
    else if ( (LeanValue < -GetTweakF32( "LeanThreshold" )) && (m_LeanAmount <= 0.0f) )
    {
        // Leaning right
        const f32 ElapsedTime       = (-m_LeanAmount) * GetTweakF32( "LeanTime" );
        const f32 NewElapsedTime    = ElapsedTime + DeltaTime;
        m_LeanAmount                = -(NewElapsedTime / GetTweakF32( "LeanTime" ));
        m_LeanState                 = LEAN_RIGHT;
    }
    else
    {
        // Returning to upright
        if ( m_LeanAmount > 0.0f )
        {
            // from left
            const f32 ElapsedTime       = m_LeanAmount * GetTweakF32( "LeanTime" );
            const f32 NewElapsedTime    = ElapsedTime - DeltaTime;
            m_LeanAmount                = NewElapsedTime / GetTweakF32( "LeanTime" );
            m_LeanAmount                = MAX( 0.0f, m_LeanAmount );
            m_LeanState                 = LEAN_RETURN_FROM_LEFT;
        }
        else if ( m_LeanAmount < 0.0f )
        {
            // from right
            const f32 ElapsedTime       = (-m_LeanAmount) * GetTweakF32( "LeanTime" );
            const f32 NewElapsedTime    = ElapsedTime - DeltaTime;
            m_LeanAmount                = -(NewElapsedTime / GetTweakF32( "LeanTime" ));
            m_LeanAmount                = MIN( 0.0f, m_LeanAmount );
            m_LeanState                 = LEAN_RETURN_FROM_RIGHT;
        }
        else
        {
            m_LeanState = LEAN_NONE;
        }

    }
    m_LeanAmount = MAX( -1.0f, m_LeanAmount );
    m_LeanAmount = MIN( 1.0f, m_LeanAmount );

    m_SoftLeanAmount = ComputeSoftLean( m_LeanAmount );

    // check to make sure we aren't violating any collision
    if ( (m_LeanState == LEAN_RIGHT) || (m_LeanState == LEAN_LEFT) )
    {
        // New position
        vector3 StartPos( GetPosition() );
        vector3 Offset  ( GetAnimPlayerOffset() );
        Offset.GetY() = 0.0f;
        vector3 EndPos  ( GetPosition() + Offset );

        //
        // We need to elevate our collision check to account for uneven floors
        // that cause us to collide at our feet, when we're mostly just
        // concerned about the player's head and weapon
        //
        static const f32 TweakLeanElevate = 10.0f;
        StartPos.GetY() += TweakLeanElevate;
        EndPos.GetY() += TweakLeanElevate;

        const vector3 Delta( EndPos - StartPos );

        if ( Delta.LengthSquared() > ZERO )
        {
            m_Physics.SetupPlayerCollisionCheck( StartPos, EndPos );
            g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, 
                                            object::ATTR_BLOCKS_PLAYER_LOS, 
                                            object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );

            if ( g_CollisionMgr.m_nCollisions > 0 )
            {
                vector3 StopPos = StartPos + (Delta * g_CollisionMgr.m_Collisions[0].T);
                m_SoftLeanAmount = ComputeLeanValueForPosition( StartPos, StopPos, m_LeanState == LEAN_RIGHT );
                m_LeanAmount = ComputeLean( m_SoftLeanAmount );
            }
        }
    }

#ifndef X_EDITOR
    // Send across net?    
    if(     ( OldLeanState      != m_LeanState      )
        ||  ( OldLeanAmount     != m_LeanAmount     )
        ||  ( OldSoftLeanAmount != m_SoftLeanAmount ) )
    {
        m_NetDirtyBits |= LEAN_BIT;  // NETWORK
    }        
#endif // X_EDITOR
    
}

vector3 player::GetLeanOffset( void )
{
    vector3 Forward ( radian3( 0.0f, m_Yaw, 0.0f ) );
    vector3 Lean    ( 0.0f, 0.0f, 0.0f );

    if ( m_SoftLeanAmount != 0.0f )
    {
        Lean = Forward;
        Lean.RotateY( R_90 );
        Lean *= (m_SoftLeanAmount * GetTweakF32( "LeanX" )); // Scale according to our horiz offset

        f32 Vertical = (PI / 2) * x_abs( m_SoftLeanAmount) * GetTweakF32( "LeanY" );

        Lean += vector3( 0.0f, -Vertical, 0.0f );
    }

    return Lean;
}

void player::UpdateArmsOffsetForLean( f32 DeltaTime )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if ( pWeapon )
    {
        //const bbox BBox( pWeapon->GetBBox() );
        const vector3 CenterPos( pWeapon->GetBBox().GetCenter() );
        radian        Pitch, Yaw;
        GetSimulationView().GetPitchYaw( Pitch, Yaw );
        const vector3 LookDir( radian3( Pitch, Yaw, 0.0f ) );
        const vector3 ViewPos(GetSimulationView().GetPosition() - (LookDir * 10000.0f) );
        const vector3 ViewEnd( ViewPos + (LookDir * 10000.0f) );
        vector3 Closest( CenterPos.GetClosestPToLSeg( ViewPos, ViewEnd ) );
        const f32 DistanceThisFrame = GetTweakF32( "LeanWeaponOffsetSpeed" ) * DeltaTime;

        switch ( m_LeanState )
        {
        case LEAN_LEFT:
        case LEAN_RIGHT:
            {
                // Move the rig under our look direction
                vector3 Dir = Closest - CenterPos;
                const f32 TotalDistanceSquared = Dir.LengthSquared();
                if ( x_sqr( DistanceThisFrame ) >= TotalDistanceSquared )
                {
                    m_LeanWeaponOffset += Dir; // Just go the remaining distance
                }
                else
                {
                    Dir.Normalize();
                    Dir *= (m_LeanWeaponOffset.Length() + DistanceThisFrame);
                    m_LeanWeaponOffset = Dir;
                }
            }
            break;

        case LEAN_NONE:
        case LEAN_RETURN_FROM_LEFT:
        case LEAN_RETURN_FROM_RIGHT:
            {
                vector3 Dir = -m_LeanWeaponOffset;
                // Move the rig back
                if ( x_sqr( DistanceThisFrame ) > Dir.LengthSquared() )
                {
                    m_LeanWeaponOffset.Zero();
                }
                else
                {
                    Dir.Normalize();
                    Dir *= DistanceThisFrame;
                    m_LeanWeaponOffset += Dir;
                }
            }
            break;
        default:
            ASSERTS( FALSE, xfs( "Invalid lean_state: %d", m_LeanState ) );
        }
    }
    else
    {
        m_LeanWeaponOffset.Zero();
    }
    
    ASSERT( m_LeanWeaponOffset.IsValid() );
}

vector3 player::GetAnimPlayerOffset( void )
{
    return m_ArmsOffset + GetLeanOffset() + m_LeanWeaponOffset;
}

void player::ManTurret( guid TurretGuid, 
                        guid Turret2Guid, 
                        guid Turret3Guid, 
                        guid AnchorGuid,
                        guid LeftBoundaryGuid,
                        guid RightBoundaryGuid,
                        guid UpperBoundaryGuid,
                        guid LowerBoundaryGuid )
{
    LOG_MESSAGE( "Turret", "Man Turret" );

    if ( m_bIsMutated )
    {
        SetupMutationChange( FALSE );
        m_bIsMutantVisionOn = FALSE;
        ForceNextWeapon();
    }

    m_Turret.PreviousWeapon = m_CurrentWeaponItem;
    SetNextWeapon2( INVEN_WEAPON_TRA );

    m_Turret.TurretGuid         = TurretGuid;
    m_Turret.Turret2Guid        = Turret2Guid;
    m_Turret.Turret3Guid        = Turret3Guid;
    m_Turret.PreviousL2W        = GetL2W();
    m_Turret.PreviousZone1      = m_ZoneTracker.GetMainZone();
    m_Turret.PreviousZone2      = m_ZoneTracker.GetZone2();
    m_Turret.PreviousWeapon     = m_CurrentWeaponItem;
    m_Turret.LeftBoundaryGuid   = LeftBoundaryGuid;
    m_Turret.RightBoundaryGuid  = RightBoundaryGuid;
    m_Turret.UpperBoundaryGuid  = UpperBoundaryGuid;
    m_Turret.LowerBoundaryGuid  = LowerBoundaryGuid;

    // Hide the turret and parts
    turret* pTurret = (turret*)g_ObjMgr.GetObjectByGuid( TurretGuid );
    ASSERT( pTurret->IsKindOf( turret::GetRTTI() ) );
    if ( pTurret )
    {
        pTurret->Hide();
    }
    HideObject( g_ObjMgr.GetObjectByGuid( Turret2Guid ) );
    HideObject( g_ObjMgr.GetObjectByGuid( Turret3Guid ) );

    object* pAnchor = g_ObjMgr.GetObjectByGuid( AnchorGuid );
    if ( pAnchor )
    {
        m_Turret.AnchorL2W = pAnchor->GetL2W();
        const radian3 Rotation( m_Turret.AnchorL2W.GetRotation() );
        Teleport( m_Turret.AnchorL2W.GetTranslation(),
                  Rotation.Pitch,
                  Rotation.Yaw,
                  static_cast<zone_mgr::zone_id>( pAnchor->GetZone1() ),
                  static_cast<zone_mgr::zone_id>( pAnchor->GetZone2() ),
                  PlayerTeleportVelocityPolicy::Clear );
    }
    else
    {
        m_Turret.AnchorL2W.Identity();
    }

    SetIsCrouching( FALSE );
    m_bInTurret  = TRUE;
}

void player::ExitTurret( void )
{
    if ( m_bInTurret )
    {
        LOG_MESSAGE( "Turret", "Exit Turret" );

        const radian3 Rotation( m_Turret.PreviousL2W.GetRotation() );
        Teleport( m_Turret.PreviousL2W.GetTranslation(),
                  Rotation.Pitch,
                  Rotation.Yaw,
                  m_Turret.PreviousZone1,
                  m_Turret.PreviousZone2,
                  PlayerTeleportVelocityPolicy::Clear );
        loco* pLoco = GetLocoPointer();
        if( pLoco )
            pLoco->m_Physics.SetMotionVelocity( vector3(0.0f,0.0f,0.0f) );
        SwitchWeapon2( m_Turret.PreviousWeapon );
        m_Inventory2.RemoveAmount( INVEN_WEAPON_TRA, 1 );
        m_bInTurret  = FALSE;

        // unhide the turret and parts
        turret* pTurret = (turret*)g_ObjMgr.GetObjectByGuid( m_Turret.TurretGuid );
        ASSERT( pTurret->IsKindOf( turret::GetRTTI() ) );
        if ( pTurret )
        {
            pTurret->Unhide();
        }
        UnhideObject( g_ObjMgr.GetObjectByGuid( m_Turret.Turret2Guid ) );
        UnhideObject( g_ObjMgr.GetObjectByGuid( m_Turret.Turret3Guid ) );

    }
}

void player::Teleport( const vector3& Position,
                       zone_mgr::zone_id Zone1,
                       zone_mgr::zone_id Zone2,
                       PlayerTeleportVelocityPolicy VelocityPolicy,
                       xbool DoBlend,
                       xbool DoEffect )
{
    Teleport( Position,
              GetPitch(),
              GetYaw(),
              Zone1,
              Zone2,
              VelocityPolicy,
              DoBlend,
              DoEffect );
}

//==============================================================================

void player::Teleport( const vector3& Position,
                       radian Pitch,
                       radian Yaw,
                       zone_mgr::zone_id Zone1,
                       zone_mgr::zone_id Zone2,
                       PlayerTeleportVelocityPolicy VelocityPolicy,
                       xbool DoBlend,
                       xbool DoEffect )
{
    vector3 Velocity = m_Physics.GetMotionVelocity();
    if( VelocityPolicy == PlayerTeleportVelocityPolicy::Keep )
    {
        radian const DeltaYaw = x_MinAngleDiff( Yaw, GetYaw() );
        Velocity.RotateY( DeltaYaw );
    }
    else
    {
        Velocity.Zero();
        m_ForwardVelocity.Zero();
        m_StrafeVelocity.Zero();
        m_Movement.Clear();
    }

    if( !DoBlend )
    {
        extern void ActivateSoundEmitters( const vector3& Position );
        ActivateSoundEmitters( Position );
    }

    // When the player (not actor or ghost) teleports and gets a new pitch/yaw,
    // we need to clear his network targeting information just to be safe.
    m_TargetNetSlot = -1;

    m_bApplyingTeleport = TRUE;
    actor::Teleport( Position, DoBlend, DoEffect );
    SetPitch( Pitch );
    SetYaw( Yaw );
    m_bApplyingTeleport = FALSE;
    m_DeltaPos.Zero();

    m_StepView.Clear();
    m_Physics.InitialGroundCheck( Position );
    g_ZoneMgr.RebaseZoneTracking( *this,
                                  m_ZoneTracker,
                                  Position,
                                  Zone1,
                                  Zone2 );
    g_ZoneMgr.AdvanceZoneTracking( *this, m_ZoneTracker, GetZoneTrackingPosition() );

    ResetFallStateAfterDiscontinuity();
    m_Physics.ClearMotionVelocity();
    if( VelocityPolicy == PlayerTeleportVelocityPolicy::Keep )
    {
        m_Physics.ApplyImpulse( Velocity );
    }
    SetIsAirborn( m_Physics.IsAirborne() );

    if( !DoBlend )
    {
        // Update ears
        matrix4 W2V;
        W2V.Identity();
        W2V.RotateX( Pitch );
        W2V.RotateY( Yaw );
        W2V.Translate( Position + vector3(0,180,0) );
        W2V.InvertRT();
        g_AudioMgr.SetEar( m_AudioEarID, W2V, Position, GetZone1(), 1.0f );

        // Make sure 1st person hands are in sync
        radian AnimPitch = -m_Pitch - m_CurrentVertRigOffset - m_ShakePitch;
        AnimPitch = MIN( R_89, MAX( -R_89, AnimPitch ) );

        m_AnimPlayer.SetYaw( m_Yaw + m_CurrentHorozRigOffset - m_ShakeYaw );
        m_AnimPlayer.SetPitch( AnimPitch );
    }

    // Make sure 1st person weapon is in sync
    AttachWeapon();
}

xbool player::IsAvatar( void )
{
    xbool bRenderAvatar = !m_bActivePlayer
                          || (m_CurrentAnimState == ANIM_STATE_FALLING_TO_DEATH)
                          || (m_LocalSlot == -1);

#if defined(X_EDITOR)
    const view* ActiveView = eng_GetView();
    if ( ActiveView && ((ActiveView->GetPosition() - GetRenderView().GetPosition()).LengthSquared() > 0.5f) )
    {
        bRenderAvatar = TRUE;
    }
#endif // X_EDITOR

    return bRenderAvatar;
}

xbool player::UsingLoco( void )
{
#if defined( X_EDITOR )
    return( FALSE );
#else
    // SB: Use loco so skin geom is setup for bbox collision detection in MP
    return ( GameMgr.IsGameMultiplayer() );
#endif
}

static f32 ComputeSoftLean( f32 LeanAmount )
{
    LeanAmount = MAX( -1.0f, LeanAmount );
    LeanAmount = MIN(  1.0f, LeanAmount );

    const f32 Sign      = (LeanAmount > 0.0f) ? 1.0f : (LeanAmount < 0.0f) ? -1.0f : 0;
    const f32 Period    = x_abs( LeanAmount ) * PI;
    const f32 Phase     = PI / 2.0f;

    return( Sign * (1.0f - ((x_sin( Period + Phase ) + 1.0f) / 2.0f)) );
}

static f32 ComputeLean( f32 SoftLeanAmount )
{
    SoftLeanAmount = MAX( -1.0f, SoftLeanAmount );
    SoftLeanAmount = MIN(  1.0f, SoftLeanAmount );

    const f32 LeanAmount = x_abs( (x_asin( ((x_abs( SoftLeanAmount ) - 1.0f) * -2.0f) - 1.0f ) - (PI / 2.0f)) / PI );
    const f32 Sign = (SoftLeanAmount >= 0.0f) ? 1.0f : -1.0f;

    return Sign * LeanAmount;
}

static f32 ComputeLeanValueForPosition( const vector3& Start, const vector3& HitPoint, xbool bLeaningRight )
{
    vector3 Lean( HitPoint - Start );
    Lean.GetY() = 0.0f;

    f32 LeanAmount = (Lean / GetTweakF32( "LeanX" )).Length();
    if ( bLeaningRight )
    {
        LeanAmount *= -1.0f;
    }

    return LeanAmount;
}

static void HideObject( object* pObj )
{
    if ( pObj )
    {
        vector3 Pos( pObj->GetPosition() );
        Pos.GetY() -= 10000.0f;
        pObj->OnMove( Pos );
    }
}

static void UnhideObject( object* pObj )
{
    if ( pObj )
    {
        vector3 Pos( pObj->GetPosition() );
        Pos.GetY() += 10000.0f;
        pObj->OnMove( Pos );
    }
}

//=========================================================================

void player::UpdateGhostLoco( f32 DeltaTime )
{
    if( m_bDead || !UsingLoco() )
    {
        return;
    }

    m_pLoco->SetGhostIsMoving( m_Physics.GetMotionVelocity().LengthSquared() > x_sqr( 0.01f ) );
    m_pLoco->SetPitch( m_Pitch );
    m_pLoco->SetYaw( m_Yaw );
    OnAdvanceGhostLogic( DeltaTime );
}
