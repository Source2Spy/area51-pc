//=========================================================================
//
//  PlayerAudio.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "e_Audio.hpp"
#include "Sound//EventSoundEmitter.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

static const f32 kForwardDelay     = 250.0f;
static const f32 kBackwardDelay    = 175.0f;
static const f32 kFalloff          = 0.85f;
static const f32 kMaxForwardVel    = 6.0f;
static const f32 kFeetsPerInitStep = 3.0f;
static const f32 kFeetsSpeedMod    = 4.0f;
static const f32 kMaxWalkVolume    = 0.5f;
static const f32 kMaxRunVolume     = 1.0f;
static const f32 kMinRunVolume     = 0.65f;
static const f32 kMaxWalkVel       = 2.5f;
static const f32 kMaxStrafeDelay   = 500.0f;
static const f32 kMinStrafeDelay   = 250.0f;
static const f32 kStrafeInit       = 0.5f;
static const f32 kVertStrafeCutOff = 0.3f;
static const f32 kLowestVolume     = 0.1f;
static const f32 s_MinFallVelocity       = 400.0f;
static const f32 s_MinDamageFallVelocity = 1100.0f;
static const f32 s_FallVolumeAttenuation = 900.0f;

//=========================================================================

void player::UpdateBulletSounds( f32 DeltaTime )
{
    // Look for active fly bys...
    for( s32 i=0 ; i<MAX_FLY_BYS ; i++ )
    {
        // Only if its active...
        if( m_BulletFlyBy[i].bIsActive )
        {
            if( g_AudioMgr.IsVoicePending( m_BulletFlyBy[i].VoiceID ) )
                continue;
            if( m_BulletFlyBy[i].Lifetime <= 0.0f )
                m_BulletFlyBy[i].Lifetime = g_AudioMgr.GetLengthSecondsCached( m_BulletFlyBy[i].VoiceID );

            // Update lifetime...
            m_BulletFlyBy[i].Age += DeltaTime;

            // Still alive?
            if( (m_BulletFlyBy[i].Age < m_BulletFlyBy[i].Lifetime) && g_AudioMgr.IsValidVoiceIdCached( m_BulletFlyBy[i].VoiceID ) )
            {
                f32 Scale = m_BulletFlyBy[i].Age / m_BulletFlyBy[i].Lifetime; 
                vector3 Pos = m_BulletFlyBy[i].Start + Scale * (m_BulletFlyBy[i].End - m_BulletFlyBy[i].Start);
                g_AudioMgr.SetPosition( m_BulletFlyBy[i].VoiceID, Pos, GetZone1() );
            }
            else
            {
                // Kill it!
                m_BulletFlyBy[i].bIsActive = FALSE;
            }
        }
    }
}


//=========================================================================

void player::HandleBulletFlyby( bullet_projectile& Bullet )
{
    vector3 ClosestPoint;
    vector3 End         = Bullet.GetCurrentPos();
    vector3 Start       = Bullet.GetInitialPos();
    vector3 Velocity    = Bullet.GetVelocity();
    vector3 EarPosition = GetPosition() + vector3( 0, 100, 0 );

    // Find closest point.
    ClosestPoint = EarPosition.GetClosestPToLSeg( Start, End );
    Velocity.Normalize();

    vector3 Delta = ClosestPoint - End;
    if( Delta.Length() > 1.0f )
    {
        // Should we play the fly by? 5 meter limit for now...
        vector3 EarToBullet    = ClosestPoint - EarPosition;
        f32     BulletDistance = EarToBullet.Length();
        if( BulletDistance < 500.0f )
        {
            // Look for an unused fly by...
            for( s32 i=0 ; i<MAX_FLY_BYS ; i++ )
            {
                // Is it active?
                if( !m_BulletFlyBy[i].bIsActive )
                {
                    // movement over 6 meters at current time...
                    m_BulletFlyBy[i].Start     = ClosestPoint - (Velocity * 400);
                    m_BulletFlyBy[i].End       = ClosestPoint + (Velocity * 1200);
                    m_BulletFlyBy[i].VoiceID   = g_AudioMgr.PlayVolumeClipped( "BulletFlyBy", m_BulletFlyBy[i].Start, GetZone1(), TRUE );
                    m_BulletFlyBy[i].Age       = 0.0f;
                    m_BulletFlyBy[i].Lifetime  = 0.0f;
                    m_BulletFlyBy[i].bIsActive = TRUE;
                    break;
                }
            }
        }
    }
}

//=========================================================================

f32 player::GetMovementNoiseLevel()
{
    f32 MaxVelocity = GetMaxVelocity();
    
    if( MaxVelocity == 0.0f )
    {
        return MaxVelocity;
    }
    else
    {
        return x_clamp( GetCurrentVelocity() / MaxVelocity, 0.0f, 1.0f );
    }
}

//=========================================================================

xbool player::InvalidSound( void )
{
    if( m_InvalidSoundTimer <= 0.0f )
    {
        g_AudioMgr.Play( "Klaxon_01_Shot", TRUE );
        m_InvalidSoundTimer = 3.0f;
        return TRUE;
    }
    
    return FALSE;
}

//=========================================================================

void player::UpdateAudio( f32 DeltaTime )
{
    if( GetLocalSlot() == -1 )
    {
        m_bJumpStarted = FALSE;
        return;
    }

    // Update the ear.
    view& View = GetSimulationView();
    ComputeView( View );
    g_AudioMgr.SetEar( m_AudioEarID, View.GetW2V(), GetPosition(), GetZone1(), 1.0f );

    if( DoFootfallCollisions() )
        PlayFootfall( DeltaTime );
    ProcessSfxEvents();    
}

//=========================================================================

void player::ProcessSfxEvents ( void )
{
    // Did we jump?
    if( m_bJumpStarted )
    {
        m_bJumpStarted = FALSE;
        g_AudioMgr.PlayVolumeClipped( "HumanMale_JumpGrunt", GetPosition(), GetZone1(), TRUE );
    }
            
    if( m_Physics.HasLanded() )
    {
        const f32 ImpactSpeed = m_Physics.GetLandingSpeed();
        if( ImpactSpeed > s_MinFallVelocity )
        {
            if( ImpactSpeed > s_MinDamageFallVelocity )
            {
                g_AudioMgr.PlayVolumeClipped( "HumanMale_LandGrunt", GetPosition(), GetZone1(), TRUE );
            }

            const f32 ImpactVolume = x_min( 1.0f, ImpactSpeed / s_FallVolumeAttenuation );
            const s32 VoiceId = g_AudioMgr.PlayVolumeClipped( GetFootfallLandSweetner( GetFloorMaterial() ),
                                                              GetPosition(), GetZone1(), TRUE );
            g_AudioMgr.SetVolume( VoiceId, ImpactVolume );
            g_AudioMgr.PlayVolumeClipped( GetFootfallHeel( GetFloorMaterial() ), GetPosition(), GetZone1(), TRUE );
        }
    }
}

//=========================================================================

void player::PlayFootfall( f32 DeltaTime )
{
    //not the active player, don't play footfalls
    if (!IsActivePlayer())
        return;
    
    // Use the player velocity.
    f32 XVel = m_fStrafeValue;
    f32 YVel = m_fMoveValue;

    f32 AbsYVel         = x_abs( YVel );
    f32 AbsXVel         = x_abs( XVel );
    f32 ComboVel        = x_sqrt( x_sqr( YVel ) + x_sqr( XVel ) );

    f32 CurrentVel    = kMaxForwardVel*ComboVel;
    f32 FeetsPerStep  = (kFeetsPerInitStep + (ComboVel*kFeetsSpeedMod));
    f32 MeterPerStep  = FeetsPerStep*0.3048f;

    m_DelayTillNextStep -= (DeltaTime*1000.0f);
    if( m_DelayTillNextStep < 0.0f )
        m_DistanceTraveled = m_DistanceTraveled + (CurrentVel*DeltaTime);
    m_IsRunning = FALSE;

    // Going forward.
    if( YVel >= 0.0f )
    {
        if( m_DistanceTraveled >= MeterPerStep )
        {
            // Get the delay till toe hit.
            m_DelayCountDown = kForwardDelay * ( ( (kFalloff-ComboVel) < 0.0f ) ? 0.0f : (kFalloff-ComboVel)/kFalloff );
            
            // Play the heel sound and set the volume level depending on the speed.
            m_HeelID = g_AudioMgr.Play( GetFootfallHeel( GetFloorMaterial() ), GetPosition(), GetZone1(), TRUE );
            g_AudioManager.NewAudioAlert( m_HeelID, audio_manager::FOOT_STEP, GetPosition(), GetZone1(), GetGuid() );

            f32 Volume  = 0.0f;
            if( (ComboVel*kMaxForwardVel) < kMaxWalkVel || m_bIsCrouching )
            {
                Volume = kLowestVolume + (kMaxWalkVolume-kLowestVolume)*MIN(ComboVel, kMaxWalkVolume);
            }
            else
            {            
                m_IsRunning = TRUE;
                Volume = kMinRunVolume + (kMaxRunVolume-kMinRunVolume)*MIN(ComboVel, kMaxRunVolume);
            }

            g_AudioMgr.SetVolume( m_HeelID, Volume );

            if( AbsXVel > kStrafeInit )
            {
                m_SlideID = g_AudioMgr.Play( GetFootfallSlide( GetFloorMaterial()) );

                f32 StrafeVolume = MAX(AbsXVel-kStrafeInit, 0.0f)*(1.0f/kStrafeInit);
                StrafeVolume     *= (AbsYVel > kVertStrafeCutOff) ? 0.0f : ( (kVertStrafeCutOff-AbsYVel)/kVertStrafeCutOff );
                g_AudioMgr.SetVolume( m_SlideID, StrafeVolume );
            }
            
            m_DistanceTraveled -= MeterPerStep;
        }

        if( m_HeelID && ((AbsYVel < kFalloff) || (AbsXVel > kStrafeInit) ) )
        {
            m_DelayCountDown -= (DeltaTime*1000.0f);

            if( m_DelayCountDown < 0.0f )
            {   
                f32 Volume  = kLowestVolume + (1.0f-kLowestVolume)*(MIN(ComboVel, 1.0f)*(1.0f/kFalloff));

                m_ToeID     = g_AudioMgr.PlayVolumeClipped( GetFootfallToe( GetFloorMaterial() ), GetPosition(), GetZone1(), TRUE );
                g_AudioMgr.SetVolume( m_ToeID, Volume );
                m_HeelID = 0;

                m_TrailStep ^= (1<<0);
            }
        }

        if( (AbsXVel >= AbsYVel) && m_TrailStep && (m_DelayTillNextStep <= 0.0f) )
            m_DelayTillNextStep = ((kMaxStrafeDelay - kMinStrafeDelay) * (1.0f-AbsXVel)) + kMinStrafeDelay;
        else
            m_DelayTillNextStep = 0.0f;

    }
    // Going backwards.
    else
    {
        if( m_DistanceTraveled >= MeterPerStep )
        {
            if( (AbsYVel < kFalloff) || (AbsXVel > kStrafeInit) )
            {
                f32 Volume  = kLowestVolume + (1.0f-kLowestVolume)*(AbsYVel*(1.0f/kFalloff));
                m_ToeID     = g_AudioMgr.PlayVolumeClipped( GetFootfallToe( GetFloorMaterial()), GetPosition(), GetZone1(), TRUE );
                g_AudioMgr.SetVolume( m_ToeID, Volume );
            }
        
            m_TrailStep ^= (1<<0);
            
            m_HeelID = 0;
            m_DelayCountDown    = kBackwardDelay * ( ( (kFalloff-ComboVel) < 0.0f ) ? 0.0f : (kFalloff-ComboVel)/kFalloff );
            m_DistanceTraveled -= MeterPerStep;
        }

        if( !m_HeelID )
        {
            m_DelayCountDown -= (DeltaTime*1000.0f);

            if( m_DelayCountDown < 0.0f )
            {   
                //f32 Volume = g_LowestVolume + (1.0f-g_LowestVolume)*MIN( ComboVel, 1.0f );
                f32 Volume  = 0.0f;
                if( (ComboVel*kMaxForwardVel) < kMaxWalkVel )
                    Volume = kLowestVolume + (kMaxWalkVolume-kLowestVolume)*MIN(ComboVel, kMaxWalkVolume);
                else
                    Volume = kMinRunVolume + (kMaxRunVolume-kMinRunVolume)*MIN(ComboVel, kMaxRunVolume);

                m_HeelID = g_AudioMgr.Play( GetFootfallHeel( GetFloorMaterial()), GetPosition(), GetZone1(), TRUE );
                g_AudioManager.NewAudioAlert( m_HeelID, audio_manager::FOOT_STEP, GetPosition(), GetZone1(), GetGuid() );
                g_AudioMgr.SetVolume( m_HeelID, Volume );

                if( AbsXVel > kStrafeInit )
                {
                    m_SlideID = g_AudioMgr.Play( GetFootfallSlide( GetFloorMaterial()) );

                    f32 StrafeVolume = MAX(AbsXVel-kStrafeInit, 0.0f)*(1.0f/kStrafeInit);
                    StrafeVolume     *= (AbsYVel > kVertStrafeCutOff) ? 0.0f : ( (kVertStrafeCutOff-AbsYVel)/kVertStrafeCutOff );
                    g_AudioMgr.SetVolume( m_SlideID, StrafeVolume );
                }

            }
        }

        if( (AbsXVel >= AbsYVel) && m_TrailStep && (m_DelayTillNextStep <= 0.0f) )
            m_DelayTillNextStep = ((kMaxStrafeDelay - kMinStrafeDelay) * (1.0f-AbsXVel)) + kMinStrafeDelay;
        else
            m_DelayTillNextStep = 0.0f;
    }
}

//=========================================================================

char* player::GetFootfallHeel( s32 Material )
{
    switch( Material )
    {        
        case MAT_TYPE_NULL:                 return "FF_Boot_Null_Heel";                  break;
        case MAT_TYPE_EARTH:                return "FF_Boot_Earth_Heel";                 break;
        case MAT_TYPE_ROCK:                 return "FF_Boot_Rock_Heel";                  break;
        case MAT_TYPE_CONCRETE:             return "FF_Boot_Concrete_Heel";              break;
        case MAT_TYPE_SOLID_METAL:          return "FF_Boot_Metal_Heel";                 break;
        case MAT_TYPE_HOLLOW_METAL:         return "FF_Boot_HollowMetal_Heel";           break;
        case MAT_TYPE_METAL_GRATE:          return "FF_Boot_MetalGrate_Heel";            break;
        case MAT_TYPE_PLASTIC:              return "FF_Boot_Plastic_Heel";               break;
        case MAT_TYPE_WATER:                return "FF_Boot_Water_Heel";                 break;
        case MAT_TYPE_WOOD:                 return "FF_Boot_Wood_Heel";                  break;
        case MAT_TYPE_ENERGY_FIELD:         return "FF_Boot_EnergyField_Heel";           break;
        case MAT_TYPE_BULLET_PROOF_GLASS:   return "FF_Boot_BulletProofGlass_Heel";      break;
        case MAT_TYPE_ICE:                  return "FF_Boot_Ice_Heel";                   break;

        case MAT_TYPE_LEATHER:              return "FF_Boot_Leather_Heel";               break;
        case MAT_TYPE_EXOSKELETON:          return "FF_Boot_Exoskeleton_Heel";           break;
        case MAT_TYPE_FLESH:                return "FF_Boot_Flesh_Heel";                 break;
        case MAT_TYPE_BLOB:                 return "FF_Boot_Blob_Heel";                  break;
        
        case MAT_TYPE_FIRE:                 return "FF_Boot_Fire_Heel";                  break;
        case MAT_TYPE_GHOST:                return "FF_Boot_Ghost_Heel";                 break;
        case MAT_TYPE_FABRIC:               return "FF_Boot_Fabric_Heel";                break;
        case MAT_TYPE_CERAMIC:              return "FF_Boot_Ceramic_Heel";               break;
        case MAT_TYPE_WIRE_FENCE:           return "FF_Boot_WireFence_Heel";             break;

        case MAT_TYPE_GLASS:                return "FF_Boot_Glass_Heel";                 break;
        default:
                                            return "Null";
        break;
    }
}

//=========================================================================

xbool player::DoFootfallCollisions( void )
{
    if( m_Physics.IsAirborne() )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

char* player::GetFootfallSlide( s32 Material )
{
    switch( Material )
    {        
        case MAT_TYPE_NULL:                 return "FF_Boot_Null_Slide";                  break;
        case MAT_TYPE_EARTH:                return "FF_Boot_Earth_Slide";                 break;
        case MAT_TYPE_ROCK:                 return "FF_Boot_Rock_Slide";                  break;
        case MAT_TYPE_CONCRETE:             return "FF_Boot_Concrete_Slide";              break;
        case MAT_TYPE_SOLID_METAL:          return "FF_Boot_Metal_Slide";                 break;
        case MAT_TYPE_HOLLOW_METAL:         return "FF_Boot_HollowMetal_Slide";           break;
        case MAT_TYPE_METAL_GRATE:          return "FF_Boot_MetalGrate_Slide";            break;
        case MAT_TYPE_PLASTIC:              return "FF_Boot_Plastic_Slide";               break;
        case MAT_TYPE_WATER:                return "FF_Boot_Water_Slide";                 break;
        case MAT_TYPE_WOOD:                 return "FF_Boot_Wood_Slide";                  break;
        case MAT_TYPE_ENERGY_FIELD:         return "FF_Boot_EnergyField_Slide";           break;
        case MAT_TYPE_BULLET_PROOF_GLASS:   return "FF_Boot_BulletProofGlass_Slide";      break;
        case MAT_TYPE_ICE:                  return "FF_Boot_Ice_Slide";                   break;

        case MAT_TYPE_LEATHER:              return "FF_Boot_Leather_Slide";               break;
        case MAT_TYPE_EXOSKELETON:          return "FF_Boot_Exoskeleton_Slide";           break;
        case MAT_TYPE_FLESH:                return "FF_Boot_Flesh_Slide";                 break;
        case MAT_TYPE_BLOB:                 return "FF_Boot_Blob_Slide";                  break;
        
        case MAT_TYPE_FIRE:                 return "FF_Boot_Fire_Slide";                  break;
        case MAT_TYPE_GHOST:                return "FF_Boot_Ghost_Slide";                 break;
        case MAT_TYPE_FABRIC:               return "FF_Boot_Fabric_Slide";                break;
        case MAT_TYPE_CERAMIC:              return "FF_Boot_Ceramic_Slide";               break;
        case MAT_TYPE_WIRE_FENCE:           return "FF_Boot_WireFence_Slide";             break;

        case MAT_TYPE_GLASS:                return "FF_Boot_Glass_Slide";                 break;
        default:
                                            return "Null";
        break;
    }
}

//=========================================================================

char* player::GetFootfallLandSweetner( s32 Material )
{
    switch( Material )
    {        
        case MAT_TYPE_NULL:                 return "FF_Boot_Land_Sweetner_Null";                  break;
        case MAT_TYPE_EARTH:                return "FF_Boot_Land_Sweetner_Earth";                 break;
        case MAT_TYPE_ROCK:                 return "FF_Boot_Land_Sweetner_Rock";                  break;
        case MAT_TYPE_CONCRETE:             return "FF_Boot_Land_Sweetner_Concrete";              break;
        case MAT_TYPE_SOLID_METAL:          return "FF_Boot_Land_Sweetner_Metal";                 break;
        case MAT_TYPE_HOLLOW_METAL:         return "FF_Boot_Land_Sweetner_HollowMetal";           break;
        case MAT_TYPE_METAL_GRATE:          return "FF_Boot_Land_Sweetner_MetalGrate";            break;
        case MAT_TYPE_PLASTIC:              return "FF_Boot_Land_Sweetner_Plastic";               break;
        case MAT_TYPE_WATER:                return "FF_Boot_Land_Sweetner_Water";                 break;
        case MAT_TYPE_WOOD:                 return "FF_Boot_Land_Sweetner_Wood";                  break;
        case MAT_TYPE_ENERGY_FIELD:         return "FF_Boot_Land_Sweetner_EnergyField";           break;
        case MAT_TYPE_BULLET_PROOF_GLASS:   return "FF_Boot_Land_Sweetner_BulletProofGlass";      break;
        case MAT_TYPE_ICE:                  return "FF_Boot_Land_Sweetner_Ice";                   break;

        case MAT_TYPE_LEATHER:              return "FF_Boot_Land_Sweetner_Leather";               break;
        case MAT_TYPE_EXOSKELETON:          return "FF_Boot_Land_Sweetner_Exoskeleton";           break;
        case MAT_TYPE_FLESH:                return "FF_Boot_Land_Sweetner_Flesh";                 break;
        case MAT_TYPE_BLOB:                 return "FF_Boot_Land_Sweetner_Blob";                  break;
        
        case MAT_TYPE_FIRE:                 return "FF_Boot_Land_Sweetner_Fire";                  break;
        case MAT_TYPE_GHOST:                return "FF_Boot_Land_Sweetner_Ghost";                 break;
        case MAT_TYPE_FABRIC:               return "FF_Boot_Land_Sweetner_Fabric";                break;
        case MAT_TYPE_CERAMIC:              return "FF_Boot_Land_Sweetner_Ceramic";               break;
        case MAT_TYPE_WIRE_FENCE:           return "FF_Boot_Land_Sweetner_WireFence";             break;

        case MAT_TYPE_GLASS:                return "FF_Boot_Land_Sweetner_Glass";                 break;
        default:
                                            return "FF_Boot_Land_Sweetner_Null";
        break;
    }
}

//=========================================================================

char* player::GetFootfallToe( s32 Material )
{
    switch( Material )
    {        
        case MAT_TYPE_NULL:                 return "FF_Boot_Null_Toe";                  break;
        case MAT_TYPE_EARTH:                return "FF_Boot_Earth_Toe";                 break;
        case MAT_TYPE_ROCK:                 return "FF_Boot_Rock_Toe";                  break;
        case MAT_TYPE_CONCRETE:             return "FF_Boot_Concrete_Toe";              break;
        case MAT_TYPE_SOLID_METAL:          return "FF_Boot_Metal_Toe";                 break;
        case MAT_TYPE_HOLLOW_METAL:         return "FF_Boot_HollowMetal_Toe";           break;
        case MAT_TYPE_METAL_GRATE:          return "FF_Boot_MetalGrate_Toe";            break;
        case MAT_TYPE_PLASTIC:              return "FF_Boot_Plastic_Toe";               break;
        case MAT_TYPE_WATER:                return "FF_Boot_Water_Toe";                 break;
        case MAT_TYPE_WOOD:                 return "FF_Boot_Wood_Toe";                  break;
        case MAT_TYPE_ENERGY_FIELD:         return "FF_Boot_EnergyField_Toe";           break;
        case MAT_TYPE_BULLET_PROOF_GLASS:   return "FF_Boot_BulletProofGlass_Toe";      break;
        case MAT_TYPE_ICE:                  return "FF_Boot_Ice_Toe";                   break;

        case MAT_TYPE_LEATHER:              return "FF_Boot_Leather_Toe";               break;
        case MAT_TYPE_EXOSKELETON:          return "FF_Boot_Exoskeleton_Toe";           break;
        case MAT_TYPE_FLESH:                return "FF_Boot_Flesh_Toe";                 break;
        case MAT_TYPE_BLOB:                 return "FF_Boot_Blob_Toe";                  break;
        
        case MAT_TYPE_FIRE:                 return "FF_Boot_Fire_Toe";                  break;
        case MAT_TYPE_GHOST:                return "FF_Boot_Ghost_Toe";                 break;
        case MAT_TYPE_FABRIC:               return "FF_Boot_Fabric_Toe";                break;
        case MAT_TYPE_CERAMIC:              return "FF_Boot_Ceramic_Toe";               break;
        case MAT_TYPE_WIRE_FENCE:           return "FF_Boot_WireFence_Toe";             break;

        case MAT_TYPE_GLASS:                return "FF_Boot_Glass_Toe";                 break;
        default:
                                            return "Null";
        break;
    }
}
