//=========================================================================
//
//  PlayerView.cpp
//
//=========================================================================

#include "PlayerView.hpp"
#include "Player.hpp"
#include "Objects/Corpse.hpp"
#include "Objects/Camera.hpp"
#include "Objects/LevelSettings.hpp"
#include "StateMgr/StateMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "GameLib/DebugCheats.hpp"

//=========================================================================

static const f32 DeathCamStartBackDist = 800.0f;
static const f32 DeathCamEndDist       = 500.0f;
static const f32 DeathCamSphereRadius  = 20.0f;
static const f32 s_ArmViewPct          = 0.3f;

extern view g_View;

view player::m_Views[MAX_LOCAL_PLAYERS];

//=========================================================================

void player::ResetView( void )
{
    m_ViewInfo.XFOV = m_OriginalViewInfo.XFOV;
}

//=========================================================================

void player::SetFieldOfView( radian FieldOfView )
{
    m_OriginalViewInfo.XFOV = x_clamp( FieldOfView, R_60, DEG_TO_RAD( 120.0f ) );
    m_ViewInfo.XFOV = m_OriginalViewInfo.XFOV;
}

//=========================================================================

void player::InitZoneTracking( void )
{
    g_ZoneMgr.RebaseZoneTracking( *this,
                                  m_ZoneTracker,
                                  GetPosition(),
                                  GetZone1(),
                                  GetZone2() );
    g_ZoneMgr.AdvanceZoneTracking( *this, m_ZoneTracker, GetZoneTrackingPosition() );
}

//=========================================================================

void player::UpdateZoneTrack ( void )
{
    if( m_bApplyingTeleport )
        return;

    g_ZoneMgr.AdvanceZoneTracking( *this, m_ZoneTracker, GetZoneTrackingPosition() );
}

//=========================================================================

vector3 player::GetZoneTrackingPosition( void )
{
    vector3 Offset = m_EyesOffset;
    Offset.RotateY( m_Yaw );
    return vector3( GetPosition().GetX(), GetBBox().Max.GetY(), GetPosition().GetZ() ) + Offset;
}

//=========================================================================

zone_mgr::zone_id player::GetPlayerObjectZone ( void ) const
{
    // Use zone tracker zone
    return m_ZoneTracker.GetMainZone();
}

//=========================================================================

zone_mgr::zone_id player::GetPlayerViewZone( void ) const
{
    return m_RenderViewZoneTracker.GetMainZone();
}

//=========================================================================

void player::ShakeView ( f32 Time, f32 Amount, f32 Speed )
{
    m_ShakeTime             = Time;
    m_ShakeAngle            = 0.0f;      
    m_ShakeAmount           = Amount;
    m_ShakeSpeed            = Speed;
}

//=========================================================================

void player::UpdateCameraShake( f32 DeltaTime )
{
    // Calculate shake using square fall off - the shake is bigger and faster at 
    // the start, and gets slowers as it dampens to zero
    f32    Freq      = SQR(DEG_TO_RAD(( MIN(1.0f,m_ShakeTime) * m_fShakeFreqScalar * 360.0f)*m_ShakeSpeed)) ;
    
    // Apply shake
    m_ShakeAngle += Freq * DeltaTime;

    m_ShakeAngle = x_fmod(m_ShakeAngle, 360.0f );

    // Update shake time
    m_ShakeTime = MAX(0, m_ShakeTime - DeltaTime) ;

    //apply view modification from pain force
    if (m_PitchMod > 0)
        m_PitchMod = MAX(0, MIN(m_PitchMod - (R_40*DeltaTime), R_20));
    else if (m_PitchMod < 0)
        m_PitchMod = MIN(0, MAX(m_PitchMod + (R_40*DeltaTime), -R_20));
        
    if (m_YawMod > 0)
        m_YawMod   = MAX(0, MIN(m_YawMod   - (R_40*DeltaTime), R_20));
    else if (m_YawMod < 0)
        m_YawMod   = MIN(0, MAX(m_YawMod   + (R_40*DeltaTime), -R_20));

    if (m_RollMod > 0)
        m_RollMod  = MAX(0, MIN(m_RollMod  - (R_40*DeltaTime), R_20));
    else if (m_RollMod < 0)
        m_RollMod  = MIN(0, MAX(m_RollMod  + (R_40*DeltaTime), -R_20));
}

//=========================================================================

void player::PushViewCinematic( lock_view_node* pLockViewBuffer )
{
    ASSERT( pLockViewBuffer );

    PlayerViewNode Nodes[PlayerViewSequence::MaxNodes];
    s32 NodeCount = 0;
    for( s32 i = 0; i < PlayerViewSequence::MaxNodes; i++ )
    {
        if( pLockViewBuffer[i].m_TimeTo <= 0.0f )
        {
            break;
        }

        Nodes[NodeCount].TimeTo = pLockViewBuffer[i].m_TimeTo;
        Nodes[NodeCount].Linger = pLockViewBuffer[i].m_Linger;
        Nodes[NodeCount].LookAt = pLockViewBuffer[i].m_LookAt;
        NodeCount++;
    }

    quaternion const StartRotation( radian3( m_Pitch, m_Yaw, 0.0f ) );
    m_LockedView.Start( Nodes,
                        NodeCount,
                        GetEyesPosition(),
                        StartRotation );

    if( m_LockedView.IsActive() )
    {
        ClearMoveLookInput();
        m_Input.Clear();
        m_ForwardVelocity.Zero();
        m_StrafeVelocity.Zero();
    }
}

//=========================================================================

void player::OnMoveFreeCam( view& View )
{
    vector3 const ViewPosition = View.GetPosition();

    g_ZoneMgr.AdvanceZoneTracking( m_RenderViewZoneTracker, ViewPosition );

    // print out the zone info
    x_printfxy( 1, 1, "FlyModeZone=%d PlayerZone=%d",
                m_RenderViewZoneTracker.GetMainZone(),
                m_ZoneTracker.GetMainZone() );
}

//=========================================================================

void player::OnExitFreeCam( vector3& NewPos )
{
    radian Pitch = m_Pitch;
    radian Yaw = m_Yaw;
#if !defined( X_EDITOR )
    g_View.GetPitchYaw( Pitch, Yaw );
#endif

    Teleport( NewPos,
              Pitch,
              Yaw,
              m_RenderViewZoneTracker.GetMainZone(),
              m_RenderViewZoneTracker.GetZone2(),
              PlayerTeleportVelocityPolicy::Clear,
              FALSE,
              FALSE );

    // Grab the current view orientation, clear any death states
#if !defined( X_EDITOR )
    SetAnimState( ANIM_STATE_IDLE );
    EndDeath();
    GetSimulationView().SetRotation( radian3( m_Pitch, m_Yaw, R_0 ) );
#endif
}

//=========================================================================

vector3 player::GetDefaultViewPos( void )
{
    vector3 FinalPos( 0.0f, 0.0f, 0.0f );
    if( m_iCameraBone > -1 )
    {
        vector3 const AnimBonePos = m_AnimPlayer.GetBonePosition( m_iCameraBone );
        vector3 const Offset = m_vRigOffset - (m_ArmsOffset * s_ArmViewPct);
        FinalPos = AnimBonePos + Offset;
    }
    else
    {
        vector3 Position( GetPosition() );
        vector3 EyesOffset( m_EyesOffset );
        EyesOffset.RotateY( m_Yaw );

        FinalPos.Set( Position.GetX(), GetBBox().Max.GetY(), Position.GetZ() );
        FinalPos += EyesOffset;
        FinalPos.GetY() += m_StepView.GetOffset();
    }

    return FinalPos;
}

//=========================================================================

void player::GetFirstPersonViewClearance( f32& NearClip, f32& Clearance )
{
    // Use the widest current gameplay FOV so zoom changes do not move the rig.
    radian XFOV = MAX( m_ViewInfo.XFOV, m_OriginalViewInfo.XFOV );
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon && pWeapon->IsZoomEnabled() )
    {
        XFOV = MAX( XFOV, pWeapon->GetXFOV() );
    }
    view Projection = GetSimulationView();
    Projection.SetXFOV( XFOV );
    PlayerView::ComputeFirstPersonClearance( m_Physics.GetColRadius(), XFOV, Projection.GetYFOV(),
                                              NearClip, Clearance );
}

//=========================================================================

vector3 player::ConstrainFirstPersonViewPosition( const vector3& Position )
{
    f32 NearClip, Clearance;
    GetFirstPersonViewClearance( NearClip, Clearance );

    // The upper-body hull follows exactly the offset validated by ConstrainLean,
    // including the downward motion. The movement capsule stays at the feet.
    const vector3 Lean = GetLeanOffset();
    return PlayerView::ConstrainFirstPersonPosition( Position, GetPosition() + Lean,
                                                     m_Physics.GetColHeight(),
                                                     m_Physics.GetColRadius(), Clearance );
}

//=========================================================================

void player::ComputeStunnedPitchYawOffset( radian& PitchOffset, radian& YawOffset )
{
    f32 YawRotFactor = m_fStunnedTime * m_fStunYawChangeSpeed;
    f32 PitchRotFactor = m_fStunnedTime * m_fStunPitchChangeSpeed;

    YawOffset = x_sin( YawRotFactor );
    PitchOffset = x_sin( PitchRotFactor );

    YawOffset *= m_MaxStunPitchOffset;
    PitchOffset *= m_MaxStunYawOffset;
}

//=========================================================================

PlayerViewContext player::BuildPlayerViewContext( void ) const
{
    PlayerViewContext Context;
    Context.HasInputSlot        = m_ActivePlayerPad >= 0;
    Context.IsLockedViewActive  = m_LockedView.IsActive();
    Context.IsCinemaActive      = m_CinemaController.IsActive();
    Context.IsDeathCameraActive = m_DeathCamera.IsActive();
    Context.IsInTurret          = m_bInTurret;
    Context.IsDead              = m_bDead;
    Context.CanDie              = m_bCanDie;
#ifndef X_EDITOR
    Context.IsPaused            = g_StateMgr.IsPaused();
#else
    Context.IsPaused            = FALSE;
#endif
    return Context;
}

//=========================================================================

PlayerViewSample player::BuildFirstPersonViewSample( void )
{
    xbool const ApplyLockedRotation = m_LockedView.IsActive() &&
                                      !m_CinemaController.IsActive();

    PlayerViewSample Sample;
    Sample.Mode = ApplyLockedRotation
                    ? PlayerViewMode::LockedFirstPerson
                    : PlayerViewMode::FirstPerson;
    Sample.XFOV = m_ViewInfo.XFOV;
    Sample.HasZoneSnapshot = TRUE;
    Sample.ZoneSnapshot = m_ZoneTracker;

    radian3 Rot( 0.0f, 0.0f, 0.0f );
    vector3 Pos = GetDefaultViewPos();
    if( !IsAvatar() && IsAlive() && !IsCinemaRunning() )
    {
        // Also cover startup and strains with no camera bone/arms animation.
        if( m_iCameraBone < 0 )
        {
            Pos += GetLeanOffset();
        }
        Pos = ConstrainFirstPersonViewPosition( Pos );
        GetFirstPersonViewClearance( Sample.NearClip, Sample.FirstPersonClearance );
    }

    if( (m_CurrentWeaponItem != INVEN_NULL) && (m_iCameraBone != -1) )
    {
        Rot = m_AnimPlayer.GetBoneL2W( m_iCameraBone ).GetRotation();
        Rot.Yaw   += R_180;
        Rot.Pitch = -Rot.Pitch;
        Rot.Roll  = -Rot.Roll;
        Rot.Yaw   -= m_CurrentHorozRigOffset;
        Rot.Pitch -= m_CurrentVertRigOffset;
    }
    else
    {
        Rot.Set( m_Pitch,
                 m_Yaw,
                 -DEG_TO_RAD( GetTweakF32( "LeanMaxDegrees" ) * m_SoftLeanAmount ) );
    }

    if( ApplyLockedRotation )
    {
        Rot = m_LockedView.GetRotation().GetRotation();
    }
    else if( (m_CurrentAnimState != ANIM_STATE_FALLING_TO_DEATH) && IsAlive() )
    {
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( pWeapon && pWeapon->IsZoomEnabled() )
        {
            m_ViewInfo.XFOV = pWeapon->GetXFOV();
        }
        else
        {
            m_ViewInfo.XFOV = m_OriginalViewInfo.XFOV;
        }
        Sample.XFOV = m_ViewInfo.XFOV;
    Sample.HasZoneSnapshot = TRUE;
    Sample.ZoneSnapshot = m_ZoneTracker;
    }

    if( (m_CurrentAnimState != ANIM_STATE_DEATH) &&
        (m_CurrentAnimState != ANIM_STATE_FALLING_TO_DEATH) &&
        !ApplyLockedRotation )
    {
        if( m_NonExclusiveStateBitFlag & NE_STATE_STUNNED )
        {
            radian PitchOffset = 0.0f;
            radian YawOffset   = 0.0f;
            ComputeStunnedPitchYawOffset( PitchOffset, YawOffset );
            Rot.Set( m_PreStunPitch + PitchOffset,
                     m_PreStunYaw + YawOffset,
                     0.0f );
        }

        f32 const Amp = SQR( MIN( 1.0f, m_ShakeTime * m_fShakeAmpScalar ) );
        m_ShakePitch = DEG_TO_RAD( m_fShakeMaxPitch ) * Amp *
                       x_sin( m_ShakeAngle * 0.981f ) * m_ShakeAmount;
        m_ShakeYaw   = DEG_TO_RAD( m_fShakeMaxYaw ) * Amp *
                       x_cos( m_ShakeAngle * 1.375f ) * m_ShakeAmount;
        Rot.Pitch += m_PitchMod - m_ShakePitch;
        Rot.Yaw   += m_YawMod   - m_ShakeYaw;
    }

    Sample.ViewToWorld.Setup( vector3( 1.0f, 1.0f, 1.0f ), Rot, Pos );

    slot_id const SlotID = g_ObjMgr.GetFirst( object::TYPE_LEVEL_SETTINGS );
    if( SlotID != SLOT_NULL )
    {
        object* pObject = g_ObjMgr.GetObjectBySlot( SlotID );
        Sample.FarClip = level_settings::GetSafeType( *pObject ).GetFarPlane();
    }

    return Sample;
}

//=========================================================================

PlayerViewSample player::BuildCinemaViewSample( void )
{
    if( m_CinemaSettings.CameraGuid != 0 )
    {
        return m_CinemaViewSample;
    }

    PlayerViewSample Sample = BuildFirstPersonViewSample();
    Sample.Mode = PlayerViewMode::Cinema;
    return Sample;
}

//=========================================================================

PlayerViewSample player::BuildDeathViewSample( void ) const
{
    PlayerViewSample Sample;
    Sample.Mode = PlayerViewMode::Death;
    Sample.XFOV = m_ViewInfo.XFOV;
    Sample.HasZoneSnapshot = TRUE;
    Sample.ZoneSnapshot = m_ZoneTracker;

    PlayerDeathCameraPose const Pose = m_DeathCamera.GetPose();
    view DeathView;
    DeathView.LookAtPoint( Pose.Position, Pose.Target );
    Sample.ViewToWorld = DeathView.GetV2W();

    slot_id const SlotID = g_ObjMgr.GetFirst( object::TYPE_LEVEL_SETTINGS );
    if( SlotID != SLOT_NULL )
    {
        object* pObject = g_ObjMgr.GetObjectBySlot( SlotID );
        Sample.FarClip = level_settings::GetSafeType( *pObject ).GetFarPlane();
    }

    return Sample;
}

//=========================================================================

void player::ApplyViewSample( view& View, const PlayerViewSample& Sample )
{
    View.SetV2W( Sample.ViewToWorld );
    View.SetXFOV( Sample.XFOV );
    View.SetZLimits( Sample.NearClip, Sample.FarClip );

    m_EyesPosition = View.GetPosition();
    View.GetPitchYaw( m_EyesPitch, m_EyesYaw );

    xbool const SourceChanged = Sample.HasZoneSnapshot &&
        ((Sample.ZoneSourceGuid != m_RenderViewZoneSourceGuid) ||
         ((Sample.Mode == PlayerViewMode::Death) !=
          (m_RenderViewZoneMode == PlayerViewMode::Death)));
    xbool const SeedChanged = Sample.HasZoneSnapshot &&
        (Sample.ZoneSnapshot.GetSeedRevision() != m_RenderViewZoneSeedRevision);

    if( !m_RenderViewZoneInitialized || SourceChanged || SeedChanged )
    {
        g_ZoneMgr.RebaseZoneTracking( m_RenderViewZoneTracker, Sample.ZoneSnapshot );
        g_ZoneMgr.AdvanceZoneTracking( m_RenderViewZoneTracker, View.GetPosition() );
        m_RenderViewZoneInitialized = TRUE;
        if( Sample.HasZoneSnapshot )
        {
            m_RenderViewZoneSourceGuid = Sample.ZoneSourceGuid;
            m_RenderViewZoneMode = Sample.Mode;
            m_RenderViewZoneSeedRevision = Sample.ZoneSnapshot.GetSeedRevision();
        }
    }
    else
    {
        g_ZoneMgr.AdvanceZoneTracking( m_RenderViewZoneTracker,
                                       View.GetPosition() );
    }

}

//=========================================================================

void player::UpdatePlayerView( f32 DeltaTime )
{
    m_LockedView.Advance( DeltaTime, GetEyesPosition() );

    if( m_LockedView.IsActive() )
    {
        radian3 const Rotation = m_LockedView.GetRotation().GetRotation();
        m_Pitch = Rotation.Pitch;
        m_Yaw   = Rotation.Yaw;
    }

    UpdateCinema( DeltaTime );
    UpdateDeathCamera( DeltaTime );

    PlayerViewContext const Context = BuildPlayerViewContext();
    PlayerViewSample Target;
    switch( PlayerView::SelectMode( Context ) )
    {
    case PlayerViewMode::Cinema:
        Target = BuildCinemaViewSample();
        break;
    case PlayerViewMode::Death:
        Target = BuildDeathViewSample();
        break;
    case PlayerViewMode::LockedFirstPerson:
    case PlayerViewMode::FirstPerson:
    default:
        Target = BuildFirstPersonViewSample();
        break;
    }

    m_SimulationViewSample = m_ViewController.Evaluate( Target, DeltaTime );
    m_SimulationViewInitialized = TRUE;
    ComputeView( GetSimulationView(), player::VIEW_NULL );
}

//=========================================================================

void player::ComputeView( view& View, view_flags Flags )
{
    (void)Flags;

    if( !m_SimulationViewInitialized )
    {
        PlayerViewSample Target = BuildFirstPersonViewSample();
        m_ViewController.Reset( Target );
        m_SimulationViewSample = Target;
        m_SimulationViewInitialized = TRUE;
    }

    ApplyViewSample( View, m_SimulationViewSample );
}

//=========================================================================

f32 player::GetDeathCameraClearDistance( const vector3& Direction,
                                         f32 MaxDistance ) const
{
    vector3 Dir = Direction;
    if( (MaxDistance <= 0.0f) || (Dir.LengthSquared() <= F32_MIN) )
    {
        return 0.0f;
    }
    Dir.NormalizeAndScale( MaxDistance );

    f32 ClearDistance = MaxDistance;
    g_CollisionMgr.RaySetup( GetGuid(),
                             m_DeathCamera.GetPose().Target,
                             m_DeathCamera.GetPose().Target + Dir );
    g_CollisionMgr.CheckCollisions(
        object::TYPE_ALL_TYPES,
        object::ATTR_BLOCKS_CHARACTER | object::ATTR_BLOCKS_CHARACTER_LOS,
        object::ATTR_COLLISION_PERMEABLE | object::ATTR_PLAYER |
        object::ATTR_CHARACTER_OBJECT );
    if( g_CollisionMgr.m_nCollisions > 0 )
    {
        ClearDistance = MaxDistance * g_CollisionMgr.m_Collisions[0].T;
    }

    g_CollisionMgr.SphereSetup( GetGuid(),
                                m_DeathCamera.GetPose().Target,
                                m_DeathCamera.GetPose().Target + Dir,
                                DeathCamSphereRadius );
    g_CollisionMgr.CheckCollisions(
        object::TYPE_ALL_TYPES,
        object::ATTR_BLOCKS_CHARACTER | object::ATTR_BLOCKS_CHARACTER_LOS,
        object::ATTR_COLLISION_PERMEABLE | object::ATTR_PLAYER |
        object::ATTR_CHARACTER_OBJECT );
    if( g_CollisionMgr.m_nCollisions > 0 )
    {
        ClearDistance = MIN( ClearDistance,
                             MaxDistance * g_CollisionMgr.m_Collisions[0].T );
    }

    return MAX( 0.0f, ClearDistance );
}

//=========================================================================

void player::StartDeathCamera( void )
{
    if( m_LocalSlot == -1 )
    {
        return;
    }

    vector3 OrbitPoint = m_SimulationViewInitialized
                           ? m_SimulationViewSample.ViewToWorld.GetTranslation()
                           : GetEyesPosition();

    vector3 PreferredDirection( 0.0f, 0.0f, 1.0f );
    corpse_pain const& DeathPain = GetCorpseDeathPain();
    if( DeathPain.GetOriginGuid() )
    {
        object* pSource = g_ObjMgr.GetObjectByGuid( DeathPain.GetOriginGuid() );
        if( pSource )
        {
            PreferredDirection = pSource->GetPosition() - GetPosition();
        }
    }
    else if( DeathPain.IsDirectHit() )
    {
        PreferredDirection = -DeathPain.GetDirection();
    }

    if( PreferredDirection.LengthSquared() <= F32_MIN )
    {
        PreferredDirection.Set( 0.0f, 0.0f, 1.0f );
    }

    radian PreferredPitch;
    radian PreferredYaw;
    PreferredDirection.GetPitchYaw( PreferredPitch, PreferredYaw );
    (void)PreferredPitch;

    f32 ClearDistances[PlayerDeathCamera::DirectionCount];
    for( s32 i = 0; i < PlayerDeathCamera::DirectionCount; i++ )
    {
        radian const CandidateYaw = PlayerDeathCamera::GetCandidateYaw( PreferredYaw, i );
        vector3 CandidateDirection( 0.0f, 0.0f, 1.0f );
        CandidateDirection.RotateX( -R_20 );
        CandidateDirection.RotateY( CandidateYaw );

        // Seed the owned camera so the collision helper has the right origin.
        m_DeathCamera.Start( OrbitPoint,
                             -R_20,
                             CandidateYaw,
                             0.0f,
                             DeathCamEndDist,
                             0.0f );
        ClearDistances[i] = GetDeathCameraClearDistance( CandidateDirection,
                                                         DeathCamStartBackDist );
    }

    s32 const BestDirection = PlayerDeathCamera::SelectDirection(
        ClearDistances,
        PlayerDeathCamera::DirectionCount );
    radian const BestYaw = PlayerDeathCamera::GetCandidateYaw( PreferredYaw,
                                                                BestDirection );
    f32 const StartDistance = MIN( DeathCamStartBackDist,
                                   ClearDistances[BestDirection] );
    m_DeathCamera.Start( OrbitPoint,
                         -R_20,
                         BestYaw,
                         StartDistance,
                         DeathCamEndDist,
                         ClearDistances[BestDirection] );
}

//=========================================================================

void player::UpdateDeathCamera( f32 DeltaTime )
{
    if( !m_DeathCamera.IsActive() )
    {
        return;
    }

    object_ptr<corpse> pCorpse( m_CorpseGuid );
    if( pCorpse )
    {
        physics_inst& PhysicsInst = pCorpse->GetPhysicsInst();
        if( PhysicsInst.GetNRigidBodies() > 0 )
        {
            rigid_body const& RigidBody = PhysicsInst.GetRigidBody( 0 );
            m_DeathCamera.SetOrbitPoint( RigidBody.GetWorldBBox().GetCenter() );
        }
        else
        {
            m_DeathCamera.SetOrbitPoint( pCorpse->GetBBox().GetCenter() );
        }
    }

    PlayerDeathCameraPose const Pose = m_DeathCamera.GetPose();
    vector3 Direction = Pose.Position - Pose.Target;
    f32 const ClearDistance = GetDeathCameraClearDistance(
        Direction,
        MAX( DeathCamStartBackDist, DeathCamEndDist ) );
    m_DeathCamera.Advance( DeltaTime, ClearDistance );
}

//=========================================================================

void player::StopDeathCamera( void )
{
    if( m_DeathCamera.IsActive() )
    {
        m_DeathCamera.Stop();
    }
}

//=========================================================================

view& player::GetLiveView( s32 Player ) 
{ 
#ifndef X_EDITOR
    ASSERT( (Player < MAX_LOCAL_PLAYERS) && (Player >= 0) ); 
#endif
    return m_Views[Player]; 
}

//=========================================================================

view& player::GetSimulationView( void )
{
#ifdef X_EDITOR
    return GetLiveView( 0 );
#else
    return GetLiveView( GetLocalSlot() );
#endif
}

//=========================================================================

void player::SetViewViewport( s32 X0, s32 Y0, s32 X1, s32 Y1 )
{
    view& View = GetSimulationView();
    View.SetViewport( X0, Y0, X1, Y1 );

    if( m_SimulationViewInitialized && (m_SimulationViewSample.FirstPersonClearance > 0.0f) )
    {
        // Rendering can resize/split the viewport after simulation. Preserve the
        // already-resolved eye/rig pose and fit the new projection into its margin.
        const f32 UnitRadius = PlayerView::GetNearPlaneRadius( 1.0f, View.GetXFOV(), View.GetYFOV() );
        const f32 MaxNearClip = (m_SimulationViewSample.FirstPersonClearance - PlayerView::ClearanceMargin) / UnitRadius;
        View.SetZLimits( MIN( m_SimulationViewSample.NearClip, MaxNearClip ), m_SimulationViewSample.FarClip );
    }
}

//=========================================================================

const view& player::GetRenderView( void ) const
{
#ifdef X_EDITOR
    return GetLiveView( 0 );
#else
    return GetLiveView( GetLocalSlot() );
#endif
}

//=========================================================================

void player::SetCinemaActive( xbool Active )
{
    if( Active )
    {
        BeginCinema();
        return;
    }

    EndCinema( PlayerCinemaFinishReason::Deactivated );
}

//=========================================================================

void player::BindCinemaObject( guid CinemaGuid )
{
    m_CinemaController.BindCinemaObject( CinemaGuid );
}

//=========================================================================

void player::UnbindCinemaObject( guid CinemaGuid )
{
    m_CinemaController.UnbindCinemaObject( CinemaGuid );
}
