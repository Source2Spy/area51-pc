//=========================================================================
//
//  PlayerStatus.cpp
//
//=========================================================================

#include "PlayerStatus.hpp"
#include "Player.hpp"
#include "Objects/ParticleEmiter.hpp"
#include "Objects/Corpse.hpp"
#include "Characters/ActorEffects.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Objects/Render/PostEffectMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "InputMgr/GamePad.hpp"
#include "x_math.hpp"

//=========================================================================

static const f32 k_PainParticleDisplace = 20.0f;
static const f32 PLAYER_FORCE_RUMBLE_DURATION  = 0.25f;
static const f32 PLAYER_FORCE_RUMBLE_INTENSITY = 1.0f;
static const f32 PLAYER_FORCE_SHAKE             = 0.5f;
static const f32 PLAYER_FORCE_BLUR              = 1.0f;
static const f32 PLAYER_FORCE_ROTATE            = 0.05f;

PlayerStatus::PlayerStatus( void ) :
    m_LifePhase      ( PlayerLifePhase::Dead ),
    m_SafeSpotElapsed( 0.0f )
{
}


//=========================================================================

void PlayerStatus::SetLifePhase( PlayerLifePhase Phase )
{
    m_LifePhase = Phase;
    m_SafeSpotElapsed = 0.0f;
}

//=========================================================================

PlayerLifePhase PlayerStatus::GetLifePhase( void ) const
{
    return m_LifePhase;
}

//=========================================================================

xbool PlayerStatus::ShouldRefreshSafeSpot( f32 DeltaTime, f32 RefreshInterval )
{
    if( m_LifePhase != PlayerLifePhase::Alive )
    {
        return FALSE;
    }

    const f32 Interval = MAX( 0.0f, RefreshInterval );
    if( Interval == 0.0f )
    {
        return TRUE;
    }

    m_SafeSpotElapsed += MAX( 0.0f, DeltaTime );
    if( m_SafeSpotElapsed < Interval )
    {
        return FALSE;
    }

    // Preserve overshoot so the result does not drift across different frame
    // delta partitions.
    m_SafeSpotElapsed -= Interval;
    return TRUE;
}

void player::UpdateSafeSpot( f32 DeltaTime )
{
    static const f32 TimeBetweenSafeSpotChecks = 10.0f;
    if( m_Status.ShouldRefreshSafeSpot( DeltaTime, TimeBetweenSafeSpotChecks ) )
    {
        if( GetIsSafeSpot() )
        {
            SetCurrentSpotAsSafeSpot();
        }
    }
}

void player::OnReset( void )
{
    m_Status.SetLifePhase( PlayerLifePhase::Alive );
    m_Health.Reset();

    Teleport( m_RespawnPosition,
              static_cast<zone_mgr::zone_id>( m_RespawnZone ),
              0,
              PlayerTeleportVelocityPolicy::Clear,
              FALSE,
              FALSE );
}

void player::ClearPainEvent( void )
{
    m_LastPainEvent.Clear();
}

void player::DoBasicPainFeedback( f32 Force )
{                                                                                                                                                                                           
    // Shake the camera
    ShakeView( PLAYER_FORCE_SHAKE * Force );

    //force feedback
    DoFeedback(PLAYER_FORCE_RUMBLE_DURATION  * Force, PLAYER_FORCE_RUMBLE_INTENSITY * Force);

    // Start up the shaky-blur pain post-effect
    f32 EffectForce = PLAYER_FORCE_BLUR * Force;
    g_PostEffectMgr.StartPainBlur( GetLocalSlot(),
        MIN(EffectForce * 20.0f, 20.0f),
        xcolor( (u8)MIN(255, 200 + (EffectForce * 55.0f) ), 128, 128, (u8)MIN(180, 100 + (EffectForce * 80.0f) ) ) );
}

void player::RespondToPain( const pain& Pain )
{

    // Get force and damage from pain
    f32 Force = Pain.GetForce();

    //
    // Do additional effects
    //
#ifndef X_EDITOR
    // take no damage from friendly sources.
    if( !g_NetworkMgr.IsOnline() )
    {
#endif
        DoBasicPainFeedback( Force );
#ifndef X_EDITOR
    }
#endif

    //
    // Rotate player based on pain direction
    //
    if( 1 )        
    {
        //mess with pitch and yaw
        radian rPainAngleYaw   = v3_AngleBetween(vector3(0, m_EyesYaw), Pain.GetDirection());
        radian rPainAnglePitch = v3_AngleBetween(vector3(m_EyesPitch, 0), Pain.GetDirection());

        vector3 PlayerFaceDir(0,0,1);
        PlayerFaceDir.RotateY( m_Yaw );
        plane Plane( vector3(0,0,0), PlayerFaceDir, vector3(0,1,0) );
        
        if( Plane.InFront( Pain.GetDirection() ) )
        {
            rPainAngleYaw = rPainAngleYaw;
        }
        else
        {
            rPainAngleYaw = -rPainAngleYaw;
        }

        f32 RotateForce = Force * PLAYER_FORCE_ROTATE;
        
        m_YawMod   -= rPainAngleYaw   * RotateForce;
        m_PitchMod -= rPainAnglePitch * RotateForce;
        m_RollMod  -= rPainAngleYaw   * RotateForce;
    }
    
/* CJ: Removed force on player from pain to fix several bugs.
    //
    // Push the player using the force
    //
    if( !m_bInTurret )
    {
        vector3 ForceVel = Pain.GetForceVelocity() * PLAYER_FORCE_VEL;
        m_Physics.ApplyImpulse( ForceVel );
    }
*/
}

actor::eHitType player::OverrideFlinchType( actor::eHitType hitType )
{
    // Players 3rd person avatar can only play light (additive) hits
    switch( hitType )
    {
        case HITTYPE_HARD:
        case HITTYPE_LIGHT:
        case HITTYPE_IDLE:
        case HITTYPE_PLAYER_MELEE_1:
            return HITTYPE_LIGHT;
        ASSERTS( 0, "Need to add new hit type here..." );
        default:
            return hitType;
    }
}

void player::OnPain( const pain& Pain )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "player::OnPain");

    // Player cannot take damage while viewing a cinematic..
    if( m_LockedView.IsActive() )
        return;

    // I have no idea what the line above does.  Is that code even used?
    // Let's check the real cinema system...
    if( IsCinemaRunning() )
        return;

    // If you are already dead, no pain!
    if( m_bDead )
        return;

    // If the same pain event as the last one, ignore it.
    if( (Pain.GetAnimEventID()!=-1) && 
        (Pain.GetAnimEventID() == m_LastAnimPainID) )
        return;

    // if we are neutral, we ignore pain
    if ( m_SpawnNeutralTime > 0.0f )
    {
        return;
    }
      
#ifndef X_EDITOR
    // take no damage from friendly sources.
    if( !GameMgr.IsGameMultiplayer() )
    {
#endif
        // If we didn't create the pain, then early-out if it's friendly
        if( (Pain.GetOriginGuid() != GetGuid()) && IsFriendlyFaction(GetFactionForGuid(Pain.GetOriginGuid())) )
        {
            return;
        }
#ifndef X_EDITOR
    }
#endif

    xstring StringPrefix = (const char*)xfs( "%s", GetLogicalName() );

    // Modify damage on mutated players.
    if( m_bIsMutated )
    {
        StringPrefix += "_MUTANT";
    }

    // Decide which health id to use
#ifndef X_EDITOR
    if( GameMgr.IsGameMultiplayer() )
    {
        switch( GetHitLocation( Pain ) )
        {   
        case geom::bone::HIT_LOCATION_HEAD:
            StringPrefix += "_H";
            break;
        case geom::bone::HIT_LOCATION_LEGS:
            StringPrefix += "_L";
            break;
        default: // includes TORSO, ARMS
            StringPrefix += "_B";
            break;
        };
    }
    else
#endif
    {
        StringPrefix += "_B";
    }


    // turn into string pointer
    const char* pString = (const char*)StringPrefix;

    // Decide which health id to use
    health_handle HealthHandle( pString );

    // Resolve Pain
    if( !Pain.ComputeDamageAndForce( HealthHandle, GetGuid(), GetBBox().GetCenter() ) )
        return;

    /*
#ifndef X_EDITOR
#ifdef DATA_VAULT_KEEP_NAMES
    LOG_MESSAGE( "player::OnPain",
        "Player %d taking %f damage in %s from weapon %s.",
        m_NetSlot, Pain.GetDamage(), pString, Pain.GetPainHealthHandle().GetName() );
#else
    LOG_MESSAGE( "player::OnPain",
        "Player %d taking %f damage in %s from weapon %d.",
        m_NetSlot, Pain.GetDamage(), pString, Pain.GetHitType() );
#endif
#endif
    */

    //
    // Apply the damage
    //
    TakeDamage( Pain );

    // If this is not the active player, then it needs to become it
    // Is this needed anymore !?!
    if( !m_bActivePlayer )
    {
        ASSERT(FALSE);
        player* pPlayer = SMP_UTIL_GetActivePlayer();
        if( pPlayer ) g_ObjMgr.DestroyObject( pPlayer->GetGuid() );
        SetAsActivePlayer(TRUE);
    }

    // Record the last pain event and time.
    m_LastPainEvent.Append( Pain );

    // Do shakes and pushes and rumbles
    RespondToPain( Pain );
    
#ifndef X_EDITOR
    // For multi-player flinch 3rd person avatar and create blood
    if( GameMgr.IsGameMultiplayer() )
    {
        // Do flinches, blood, impact sounds etc
        DoMultiplayerPainEffects( Pain );
    }
#endif
}

void player::BackUpCurrentState  ( void )
{
    OnCopy( m_SaveSpotProperties );
}       

void player::RestoreState        ( void )
{
    if(m_SaveSpotProperties.GetCount() > 0 )
    {    
        OnPaste(m_SaveSpotProperties);
    }
}

void player::ParseOnPainForEffects ( const pain& Pain )
{
    // Skip?
    if( !IsBloodEnabled() )
        return;

    // Create blood impact if blood decals are assigned
    const decal_package* pBloodDecalPackage = m_hBloodDecalPackage.GetPointer();
    if( pBloodDecalPackage )
    {
        // Create blood based on pain type and use color of assigned blood decal group
        particle_emitter::CreateOnPainEffect( Pain, 
                                              k_PainParticleDisplace, 
                                              particle_emitter::UNINITIALIZED_PARTICLE, 
                                              pBloodDecalPackage->GetGroupColor( m_BloodDecalGroup ) );
    }                                                  
}

xbool player::GetIsSafeSpot ( void )
{
    const f32 k_MIN_TIME_NOT_SPOTTED_TO_CONSIDER_SAFE = 10.0f;
    const f32 k_MIN_TIME_NOT_INJURED_TO_CONSIDER_SAFE = 10.0f;
    f32 currentTime = (f32)g_ObjMgr.GetSimulationTimeSeconds();

    if( ( currentTime - m_LastTimeSeenByEnemy ) > k_MIN_TIME_NOT_SPOTTED_TO_CONSIDER_SAFE && 
        ( currentTime - m_LastTimeTookDamage  ) > k_MIN_TIME_NOT_INJURED_TO_CONSIDER_SAFE )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void player::SetCurrentSpotAsSafeSpot ( void )
{
    m_PositionOfLastSafeSpot     = m_NextPositionOfLastSafeSpot;
    m_ZoneIDOfLastSafeSpot       = m_NextZoneIDOfLastSafeSpot;
    m_NextPositionOfLastSafeSpot = GetPosition(); 
    m_NextZoneIDOfLastSafeSpot   = GetPlayerObjectZone() ;    
}

void player::ResetToLastSafeSpot ( void )
{
    if( m_PositionOfLastSafeSpot.LengthSquared() != 0.0f )
    {
        Teleport( m_PositionOfLastSafeSpot,
                  static_cast<zone_mgr::zone_id>( m_ZoneIDOfLastSafeSpot ),
                  0,
                  PlayerTeleportVelocityPolicy::Clear,
                  FALSE,
                  FALSE );
    }
    else
    {
        Teleport( m_RespawnPosition,
                  static_cast<zone_mgr::zone_id>( m_RespawnZone ),
                  0,
                  PlayerTeleportVelocityPolicy::Clear,
                  FALSE,
                  FALSE );
    }

}

void player::ResetPlayer ( const vector3& rPos, const radian3& rViewRot )
{
    Teleport( rPos,
              rViewRot.Pitch,
              rViewRot.Yaw,
              static_cast<zone_mgr::zone_id>( GetZone1() ),
              static_cast<zone_mgr::zone_id>( GetZone2() ),
              PlayerTeleportVelocityPolicy::Clear,
              FALSE,
              FALSE );
}

void player::DoFeedback( f32 Duration, f32 Intensity )
{
    // don't do feedback if we're dead
    if( m_ActivePlayerPad != -1 && !IsDead() )
    {
        g_Input.Feedback( Duration, Intensity, g_GameInput.GetPlayerDevice( m_ActivePlayerPad ) );
    }
}

void player::OnKill( void )
{
    m_Status.SetLifePhase( PlayerLifePhase::Removed );
    actor::OnKill();

    // free our slot
    if( m_LocalSlot != -1 )
    {
        m_LocalSlot = -1;
    }    
}

void player::OnDeath( void )
{
    m_Status.SetLifePhase( PlayerLifePhase::Dead );

    // Prevent the fire press that caused this tick from becoming an immediate
    // respawn request. Dead-player input is sampled again on the next tick.
    ClearMoveLookInput();
    m_Input.Clear();

    new_weapon *pWeapon = GetCurrentWeaponPtr();

    // get rid of our weapon (mostly for multiplayer)
    if( pWeapon )
    {
        // Make sure to stop any looping weapon sfx
        pWeapon->ReleaseAudio();
    
        // end zoom
        pWeapon->ClearZoom();

        // shut off flashlight
        SetFlashlightActive(FALSE);        

        // clear weapon
        SetAnimState(ANIM_STATE_IDLE);
        m_NextAnimState = ANIM_STATE_UNDEFINED;
    }

    actor::OnDeath();

#ifndef X_EDITOR
    if ( GameMgr.IsGameMultiplayer() )
    {
        StartDeathCamera();
        m_DeathCamera.SetDesiredPitch( -R_30 );
        ForceMutationChange( FALSE );
    }
    else
    {
        // Set this so the state mgr knows what is going on.
        s_bPlayerDied = TRUE;
    }

#endif

    SetAnimState( ANIM_STATE_DEATH );

    // tell perception manager we died
    if (IsMutated())
        g_PerceptionMgr.EndMutate();    

    ClearAllNonExclusiveStates();

    if ( m_bInTurret )
    {
        // make sure we don't go flying
        GetLocoPointer()->m_Physics.SetMotionVelocity( vector3(0.0f,0.0f,0.0f) );
    }
    
    m_bInTurret     = FALSE;

    // reset lore flag.  It will get set to true again if all are actually collected (self-fixing).
    m_bAllLoreObjectsCollected = FALSE;

    m_LeanState      = LEAN_NONE;
    m_SoftLeanAmount = 0.0f;
    m_LeanWeaponOffset.Zero();
}

void player::OnMissionFailed( s32 TableName, s32 ReasonName )
{
#ifndef X_EDITOR
    LOG_MESSAGE( "player::OnMissionFailed", "Slot:%d", m_NetSlot );
#endif

    m_MissionFailedTableName    = TableName;
    m_MissionFailedReasonName   = ReasonName;

    // We need to die, then go into ANIM_STATE_MISSION_FAILED
    OnDeath();
    SetAnimState( ANIM_STATE_MISSION_FAILED );
}

void player::OnSpawn( void )
{
    actor::OnSpawn();
    m_Status.SetLifePhase( PlayerLifePhase::Alive );

#ifndef X_EDITOR
    m_NetDirtyBits |= WEAPON_BIT;  // NETWORK
#endif // X_EDITOR

    InitZoneTracking();

    // Reset the state.
    SetAnimState( ANIM_STATE_IDLE );

    // refill mutagen
    AddMutagen(GetMaxMutagen());
}

void player::OnAliveLogic( f32 DeltaTime )
{
    // Recover aim (double the recovery just to make it faster).
    m_AimDegradation = MAX( 0.0f, m_AimDegradation - (m_AimRecoverSpeed*DeltaTime*2.0f) );

    f32 AbsForwardSpeed = x_abs( m_fForwardSpeed );
    f32 AbsStrafeSpeed  = x_abs( m_fStrafeSpeed );

    f32 ScalerForward   = (AbsForwardSpeed/m_MaxFowardVelocity) * m_ReticleMovementDegrade;
    f32 ScalerStrafe    = (AbsStrafeSpeed /m_MaxFowardVelocity) * m_ReticleMovementDegrade;

    f32 ShootDegrade    = 1.0f - m_ReticleMovementDegrade;
    f32 AimDegrade      = MIN( 1.0f, (m_AimDegradation*ShootDegrade) + MAX( ScalerStrafe, ScalerForward ) );

    if( AimDegrade < 0.0f )
        AimDegrade = 1.0f;

    f32 AlteredDeltaTime = DeltaTime;

    if( ( m_NonExclusiveStateBitFlag & NE_STATE_STUNNED ) != 0 )
    {
        AlteredDeltaTime *= 0.3f;
    }

    // A first-person cinema is still the player's own camera and rig. Keep
    // normal movement decay, arms offsets and character rotation updating
    // while input is blocked. Only an external cinema camera freezes them.
    if( IsCinemaRunning() && (GetCinemaCameraGuid() != 0) )
    {
        actor::OnAliveLogic( DeltaTime );
        return;
    }

#ifndef X_EDITOR
    if( GameMgr.IsZoneLocked( GetZone1() ) )
    {
        m_TimeSinceLastZonePain += DeltaTime;

        if( m_TimeSinceLastZonePain > 0.5f )
        {
            //Do Damage
            pain Pain;

            pain_handle PainHandle( "ZONE_PAIN" );
            Pain.Setup( PainHandle, 0, GetBBox().GetCenter() );

            Pain.SetCustomScalar( m_TimeSinceLastZonePain );

            Pain.ApplyToObject( this );

            m_TimeSinceLastZonePain = 0.0f;
        }
    }
#endif

    //==========================================================================================
    // Begin code from ghost.cpp
    //==========================================================================================
    
    // Let physics keep track of riding on platform
    m_Physics.CatchUpWithRidingPlatform( DeltaTime );
    m_Physics.WatchForRidingPlatform();

    UpdateCrouchHeight( AlteredDeltaTime );
    UpdateMovement( DeltaTime );

    // Call base class
    actor::OnAliveLogic( DeltaTime );
    
    //==========================================================================================
    // End code from ghost.cpp
    //==========================================================================================

    // Handles rotation.
    UpdateRotation          ( AlteredDeltaTime );
    UpdateCharacterRotation ( AlteredDeltaTime );

}

void player::OnDeathLogic( f32 DeltaTime )
{
    //==========================================================================================
    // Begin code from ghost.cpp
    //==========================================================================================

    // Keep physics going for death while falling.
    m_Physics.Advance( m_Physics.GetPosition(), DeltaTime, TRUE );

    OnMove( m_Physics.GetPosition() );

    if ( UsingLoco() )
    {
        if( m_pLoco && m_pLoco->IsPlayAnimComplete() )
        {
            CreateCorpse();
        }
    }

    // Call base class
    actor::OnDeathLogic( DeltaTime );
    
    //==========================================================================================
    // End code from ghost.cpp
    //==========================================================================================
}

void player::SetNonExclusiveState( non_exclusive_states nStateBit )
{
    // don't set the state if we're already in it
    if ( m_NonExclusiveStateBitFlag & nStateBit )
        return;

    m_NonExclusiveStateBitFlag |= nStateBit;
    BeginNonExclusiveState( nStateBit );
}

void player::ClearNonExclusiveState( non_exclusive_states nStateBit )
{
    if ( nStateBit & m_NonExclusiveStateBitFlag )
    {
        EndNonExclusiveState( nStateBit );
        m_NonExclusiveStateBitFlag &= ~nStateBit;
    }

}

void player::ClearAllNonExclusiveStates( void )
{
    ClearNonExclusiveState( NE_STATE_STUNNED );
}

void player::BeginNonExclusiveState( non_exclusive_states nStateBit )
{
    switch( nStateBit )
    {
        case NE_STATE_STUNNED:
            BeginStunnedNE();
            break;
        default:
            break;
    }
}

void player::TakeFallPain( void )
{
    const f32 CurrentAltitude = GetPosition().GetY();
    const f32 FallDist = m_FellFromAltitude - CurrentAltitude;

    if( FallDist > m_SafeFallAltitude )
    {
        // Get parametric fall distance where 0=safe, 1=dead
        f32 T = x_parametric( FallDist, m_SafeFallAltitude, m_DeathFallAltitude, TRUE );

        // Build a pain event to describe damage and apply to player
        pain Pain;
        Pain.Setup(xfs("%s_FALL_DAMAGE",GetLogicalName()),0,GetPosition());
        Pain.SetCustomScalar( T );
        Pain.SetDirectHitGuid( GetGuid() );
        Pain.ApplyToObject( GetGuid() );

        // reset fall altitude
        m_FellFromAltitude = GetPosition().GetY();
    }
}

xbool player::AddHealth( f32 DeltaHealth )
{
    // Check to see if the player took damage
    if( DeltaHealth < 0.0f )
    {
        m_LastTimeTookDamage = (f32)g_ObjMgr.GetSimulationTimeSeconds();
    }

    return actor::AddHealth( DeltaHealth );
}

void player::StunPlayer( void )
{
    SetNonExclusiveState( NE_STATE_STUNNED );
}

void player::EndStunnedNE( void )
{
}

//=========================================================================

void player::UpdateMultiplayerDeath( void )
{
#ifndef X_EDITOR
    if( !GameMgr.IsGameMultiplayer() || (!IsDead() && (GetHealth() > 0.0f)) )
    {
        return;
    }

    m_Status.SetLifePhase( PlayerLifePhase::Dead );

    // Recover from legacy paths that can leave a zero-health player outside
    // the death state.
    if( m_CurrentAnimState != ANIM_STATE_DEATH )
    {
        StartDeathCamera();
        m_DeathCamera.SetDesiredPitch( -R_30 );

        ForceMutationChange( FALSE );
        SetAnimState( ANIM_STATE_DEATH );
    }

    if( m_Input.GetState().WasPressed( PlayerAction::PrimaryFire ) )
    {
        m_bWantToSpawn = TRUE;
        m_NetDirtyBits |= WANT_SPAWN_BIT;
        m_bRespawnButtonPressed = TRUE;
    }
#endif
}

//=========================================================================

void player::UpdateFallState( xbool HadLaunchResponse )
{
    SetIsAirborn( m_Physics.IsAirborne() );
    m_bFalling = m_Physics.IsAirborne();

    if( m_Physics.HasLanded() )
    {
        m_bJustLanded = TRUE;
        if( !HadLaunchResponse )
        {
            TakeFallPain();
        }
    }

    if( !m_bFalling )
    {
        m_JumpedOffLadderGuid = 0;
    }
}

//=========================================================================

void player::ResetFallStateAfterDiscontinuity( void )
{
    m_FellFromAltitude = GetPosition().GetY();
    m_bFalling = FALSE;
    m_bJustLanded = FALSE;
    m_JumpBufferTime = 0.0f;
    m_bJumpStarted = FALSE;
    m_JumpedOffLadderGuid = NULL_GUID;
    SetIsAirborn( m_Physics.IsAirborne() );
}

void player::UpdateStunnedNE( f32 DeltaTime )
{
    m_fStunnedTime += DeltaTime;

    f32 YawRotFactor = m_fStunnedTime * m_fStunYawChangeSpeed;
    f32 PitchRotFactor = m_fStunnedTime * m_fStunPitchChangeSpeed;


    f32 YawOffset = x_sin( YawRotFactor );
    f32 PitchOffset = x_sin( PitchRotFactor );
    YawOffset *= m_MaxStunPitchOffset;
    PitchOffset *= m_MaxStunYawOffset;

    m_AnimPlayer.SetPitch( m_PreStunPitch + PitchOffset );
    m_AnimPlayer.SetYaw( m_PreStunYaw + YawOffset );

    if ( !m_bInTurret )
    {
        m_Physics.Advance( m_Physics.GetPosition() , DeltaTime );
        OnMove( m_Physics.GetPosition() );
    }

    if ( m_fStunnedTime >= m_fMaxStunTime )
    {
        ClearNonExclusiveState( NE_STATE_STUNNED );
    }
}

void player::BeginStunnedNE( void )
{
    // Get the current rotation from the view
    m_fStunnedTime = 0.f;

    GetEyesPitchYaw( m_PreStunPitch, m_PreStunYaw );
}

void player::ProcessStunnedPain( const pain& Pain )
{
    (void)Pain;
    SetNonExclusiveState( NE_STATE_STUNNED );
}

void player::UpdateActiveNonExclusiveStates( f32 DeltaTime )
{
    if ( m_NonExclusiveStateBitFlag & NE_STATE_STUNNED )
    {
        UpdateStunnedNE( DeltaTime );
    }
}

void player::EndNonExclusiveState( non_exclusive_states nStateBit )
{
    switch( nStateBit )
    {
        case NE_STATE_STUNNED:
            EndStunnedNE();
            break;
        default:
            break;
    }
}
