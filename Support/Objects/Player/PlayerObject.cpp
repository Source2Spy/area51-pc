//==============================================================================
//
//  PlayerObject.cpp
// 
//==============================================================================
#include "Player.hpp"
#include "InputMgr/GamePad.hpp"
#include "GameLib/StatsMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Objects/HudObject.hpp"
#include "Characters/ActorEffects.hpp"
#include "Configuration/GameConfig.hpp"
#include "GameLib/DebugCheats.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"
#include "e_Audio.hpp"

mp_tweaks g_MPTweaks =
{
    FALSE,   // Active
    1.30f,   // JumpSpeed      
    1.45f,   // Gravity    
    0.10f    // AirControl
};

//=========================================================================
// PLAYER
//=========================================================================

static struct player_desc : public object_desc
{
    player_desc( void ) : object_desc( 
            object::TYPE_PLAYER, 
            "Player", 
            "ACTOR",
            object::ATTR_PLAYER                 |
            object::ATTR_NEEDS_LOGIC_TIME       |
            object::ATTR_COLLIDABLE             | 
            object::ATTR_BLOCKS_ALL_PROJECTILES | 
            object::ATTR_BLOCKS_RAGDOLL         | 
            object::ATTR_BLOCKS_CHARACTER_LOS   | 
            object::ATTR_BLOCKS_PLAYER_LOS      | 
            object::ATTR_BLOCKS_SMALL_DEBRIS    | 
            object::ATTR_RENDERABLE             | 
            object::ATTR_CAST_SHADOWS           |
            object::ATTR_SPACIAL_ENTRY          | 
            object::ATTR_DAMAGEABLE             |
            object::ATTR_TRANSPARENT            |
            object::ATTR_LIVING,

            FLAGS_GENERIC_EDITOR_CREATE | 
            FLAGS_IS_DYNAMIC )
            {}

    virtual object* Create( void )
    {
        return new player;
    }

#ifdef X_EDITOR
    s32 OnEditorRender( object& Object ) const
    {
        player& Player = player::GetSafeType( Object );
        Player.OnEditorRender();

        return -1;
    }
#endif // X_EDITOR

} s_Player;

//=========================================================================
// VARIABLES
//=========================================================================
guid     player::s_ActivePlayerGuid( 0 );
xbool    player::s_bPlayerDied( FALSE );

//=========================================================================

player* player::GetActivePlayer( void )
{
    return (player*)g_ObjMgr.GetObjectByGuid(s_ActivePlayerGuid);
}

//=========================================================================

player::player( void ) : 
    m_RespawnPosition(0,0,0),
    m_RespawnZone(0),
    m_PitchRate( 0.0f ),
    m_YawRate( 0.0f ),
    m_PitchMax( R_87 ),
    m_PitchMin( -R_87 ),
    m_DesiredPitchMax( R_87 ),
    m_DesiredPitchMin( -R_87 ),
    m_EyesOffset( 0.0f, -25.0f, -5.0f ),
    m_ShakeTime( 0.0f ),
    m_ShakeAngle( 0.0f ),
    m_ShakeAmount(0),
    m_ShakeSpeed(0),
    m_ActivePlayerPad(-1),
    m_LocalSlot( -1 ),
    m_MaxFowardVelocity( 100.0f ),
    m_JumpVelocity( 200.0f ),
    m_ForwardVelocity( 0.0f , 0.0f , 0.0f ),
    m_StrafeVelocity( 0.0f , 0.0f , 0.0f ),
    m_fForwardAccel( 1000.0f ),
    m_fForwardSpeed( 0.0f ),
    m_fCurrentYawOffset( 0.0f ),
    m_fCurrentPitchOffset( 0.0f ),
    m_fPitchChangeSpeed( PI * .45f ),
    m_fStrafeSpeed( 0.0f ),
    m_fDecelerationFactor( 2.5f ),
    m_SoftLeanAmount                    ( 0.0f                  ),
    m_LeanWeaponOffset                  ( 0.0f, 0.0f, 0.0f      ),
    m_PitchArmsScalerPositive           ( 1.0f                  ),
    m_PitchArmsScalerNegative           ( 1.0f                  ),
    m_fCurrentCrouchFactor              ( 0.0f                  ),
    m_fCrouchChangeRate                 ( 10.0f                 ),
    m_bInTurret                         ( FALSE                 ),
    m_pPlayerTitle( NULL ),
    m_fMinWalkSpeed( 0.0f ),
    m_fMinRunSpeed( 0.0f ),
    m_NonExclusiveStateBitFlag( NE_STATE_NULL ),
    m_PreStunPitch(0),
    m_PreStunYaw(0),
    m_PreStunRoll(0),
    m_fStunnedTime(0),
    m_fShakeAmpScalar( 1.f ),
    m_fShakeFreqScalar( 1.f ),
    m_fShakeMaxPitch( 5.f ),
    m_fShakeMaxYaw( 2.f ),
    m_bAllLoreObjectsCollected( FALSE ),
    m_ReticleMovementDegrade( 0.5f ),
    m_InvalidSoundTimer( 0.0f ),
    m_fRigMoveOffsetVelocity( 10.0f ),
    m_fRigStrafeOffsetVelocity( 20.0f ),
    m_fCurrentMoveRigOffset( 0.0f ),
    m_fCurrentStrafeRigOffset( 0.0f ),
    m_RigLookOffset( radian3( 0.0f, 0.0f, 0.0f ) ),
    m_RigLookMaxVertOffset( R_2 ),
    m_RigLookMaxHorozOffset( R_3 ),
    m_RigLookVertVelocity( R_10 ),
    m_RigLookHorozVelocity( R_20 ),
    m_CurrentVertRigOffset( R_0 ),
    m_CurrentHorozRigOffset( R_0 ),
    m_fCurrentPitchAimModifier( 1.f ),
    m_fCurrentYawAimModifier( 1.f ),
    m_YawAimOffset( R_0 ),
    m_TimeSinceLastZonePain( 0.0f ),
    m_fMoveValue( 0.0f ),
    m_fStrafeValue( 0.0f ),
    m_bVoteButtonPressed(FALSE),
    m_bRespawnButtonPressed(FALSE),
    m_LastTimeSeenByEnemy(0.0f),
    m_LastTimeTookDamage(0.0f),
    m_PositionOfLastSafeSpot(0,0,0),
    m_ZoneIDOfLastSafeSpot(0),
    m_NextPositionOfLastSafeSpot(0,0,0),
    m_NextZoneIDOfLastSafeSpot(0),
    m_AimDegradation(0.0f),
    m_AimRecoverSpeed(0.0f),
    m_YawMod(0.0f),
    m_PitchMod(0.0f),
    m_RollMod(0.0f),
    m_ShakePitch(0.0f),
    m_ShakeYaw(0.0f),
    m_NearbyObjectCounter(0),
    m_GameSpeakCounter(0),
    m_SpeakToGuid( 0 ),
    m_GameSpeakEmitterGuid( 0 ),
    m_ProximityAlertRadius( 300.0f ),
    m_bSpeaking                         ( FALSE ),
    m_LastLadderGuid                    ( 0 ),
    m_JumpedOffLadderGuid               ( 0 ),
    m_WeaponState                       ( WEAPON_STATE_NONE ),
    m_ReticleRadius                     ( 0.0f ),
    m_ReticleGrowSpeed                  ( 0.0f ),    
    m_ArmsOffset                        ( 0.0f, 0.0f, 0.0f ),
    m_ArmsVelocity                      ( 0.0f, 0.0f, 0.0f ),
    m_WeaponCollisionOffset             ( 0.0f, 0.0f, 0.0f ),
    m_LastWeaponCollisionOffsetScalar   ( 0.0f ),
    m_WeaponCollisionOffsetScalar       ( 0.0f ),
    m_BatteryChangeTime                 ( 0.0f ),
    m_Battery                           ( 100.0f ),
    m_MaxBattery                        ( 100.0f ),
    m_FlashlightTimeout                 ( 0.0f),
    m_fLastItemFullTime                 ( 0.0f ),
    m_fLastItemAcquiredTime             ( 0.0f ),
    m_bStrainInitialized                ( FALSE ),
    m_CurrentAnimState                  ( ANIM_STATE_UNDEFINED ),
    m_PreviousAnimState                 ( ANIM_STATE_UNDEFINED ),
    m_NextAnimState                     ( ANIM_STATE_UNDEFINED ),
    m_AnimStage                         ( ANIM_STATE_UNDEFINED ),
    m_MeleeAnimStateIndex               ( -1 ),
    m_CurrentAnimIndex                  ( -1 ),
    m_PreviousAnimIndex                 ( -1 ),
    m_CurrentAnimStateIndex             ( -1 ),
    m_PreviousAnimStateIndex            ( -1 ),
    m_fAnimationTime                    ( 0.0f ),
    m_fMaxAnimTime                      ( 0.0f ),
    m_WpnHoldTime                       ( 0.0f ),
    m_LastTimeWeaponFired               ( 0.0f ),
    m_bOnLadder                         ( FALSE ),
    m_LadderOutDir                      ( 0, 0, 0 ),
    m_MaxAnimWeaponHoldTime             ( 20.0f ),
    m_nLoreDiscoveries                  ( 0 ),
    m_bWasMutated                       ( FALSE ),
    m_bIsMutantVisionOn                 ( FALSE ),
    m_PreMutationWeapon2                ( INVEN_NULL ),
    m_bMutationMeleeEnabled             ( FALSE ),
    m_bPrimaryMutationFireEnabled       ( FALSE ),
    m_bSecondaryMutationFireEnabled     ( FALSE ),
    m_bMeleeLunging                     ( FALSE ),
    m_bHolsterWeapon                    ( FALSE ),
    m_MeleeDamage                       ( 120.0f ),
    m_MeleeForce                        ( 30.0f ),
    m_bInMutationTutorial               ( FALSE ),
    m_bHitCombo                         ( FALSE ),
    m_bCanRequestCombo                  ( FALSE ),
    m_bLastMeleeHit                     ( FALSE ),
    m_ComboCount                        ( 0 ),

    m_bTweakHandlesLoaded               ( FALSE ),
    
    // From ghost.cpp
    m_Mutagen                           ( 100.0f ),
    m_MaxMutagen                        ( 100.0f ),
    m_EyesPosition                      ( 0.0f, 0.0f, 0.0f ),
    m_EyesPitch                         ( 0.0f ),
    m_EyesYaw                           ( 0.0f ),
    m_SuckingMutagenLoopID              ( 0 ),
#if !defined( CONFIG_RETAIL )
    m_bRenderSkeleton                   ( FALSE ),
    m_bRenderSkeletonNames              ( TRUE ),
    m_bRenderBBox                       ( TRUE ),
#endif // !defined( CONFIG_RETAIL )
    m_PrevWeaponItem                    ( INVEN_NULL ),
    m_NextWeaponItem                    ( INVEN_NULL ),
    m_bJustLanded                       ( FALSE ),
    m_DeltaPos                          ( 0.0f, 0.0f, 0.0f ),
    m_bCanJump                          ( TRUE ),
    m_JumpBufferTime                    ( 0.0f ),
    m_DeltaTime                         ( 0.0f ),
    m_TimeInState                       ( 0.0f ),
    m_MissionFailedTableName            ( -1 ),
    m_MissionFailedReasonName           ( -1 ),
    m_VoteMode                          ( FALSE ),
    m_DelayTillNextStep                 ( 0.0f ),
    m_DistanceTraveled                  ( 0.0f ),
    m_DelayCountDown                    ( 0.0f ),
    m_HeelID                            ( 0 ),
    m_SlideID                           ( 0 ),
    m_ToeID                             ( 0 ),
    m_TrailStep                         ( 0 ),
    m_MutationChangeTime                ( 0.0f ),
    m_UseTime                           ( 0.0f )
{
    s32 i;

    SetIsActive( TRUE );

    InitializeMeleeAnimStateList();

    m_vRigOffset.Set( 0.0f, 0.0f, 0.0f );

    // The title for this player
    m_pPlayerTitle = "Unknown Mutation";
    
    m_bJumpStarted          = FALSE;

    // Get the players ear id.
    m_AudioEarID = g_AudioMgr.CreateEar();

    m_RespawnZone = 0;
    
    m_SpeakToGuid = 0 ;
    m_CurrentTargetingModifation.Zero();
    m_OffsetToTarget.Zero();

    m_bActivePlayer = TRUE;

    m_fRigMaxMoveOffset = 2.0f;
    m_fRigMaxStrafeOffset = 3.0f;

    m_bHidePlayerArms = FALSE;
    m_bArmsWereHidden = FALSE;
    m_bPlaySwitchTo   = TRUE;

    m_CinemaPhysicsCaptured        = FALSE;
    m_CinemaSolveActorCollisions   = TRUE;
    m_CinemaLocoCollision          = TRUE;
    m_CinemaLocoGravity            = TRUE;
    m_SimulationViewInitialized    = FALSE;
    m_RenderViewZoneSourceGuid      = NULL_GUID;
    m_RenderViewZoneInitialized     = FALSE;
    m_RenderViewZoneMode            = PlayerViewMode::FirstPerson;
    m_RenderViewZoneSeedRevision    = 0;
    m_bApplyingTeleport             = FALSE;
    m_iCameraBone = -1;
    m_iCameraTargetBone = -1;

    // Initialize our arrays.
    x_memset( m_fAnimPriorityPercentage, 0, sizeof( f32 ) * MAX_ANIM_PER_STATE );

    m_StrainFriendFlags = 0;
    
    m_CurrentGrenadeType2           = INVEN_GRENADE_FRAG;
    m_PlayMeleeSound                = TRUE;
    m_IsRunning                     = FALSE;
    m_MutationAudioLoopSfx          = 0;
    m_NeedRelaodIn                  = TRUE;
    m_LastFireAnimStateIndex        = 0;
    m_bUsingFlashlight              = FALSE;
    m_bUsingFlashlightBeforeCinema  = FALSE;

    // Reset all weapon anim tables
    for( i=0; i<INVEN_NUM_WEAPONS; i++ )
    {
        inven_item WeaponItem = inventory2::WeaponIndexToItem(i);
        ResetWeaponAnimTable2( WeaponItem );
    }

    // Clear the last pain event.
    m_LastPainEvent.Clear();
    m_LastPainEvent.SetGrowAmount( 4 );

    m_CurrentAnimState  = ANIM_STATE_UNDEFINED;

    // on initialize, the JBG is not in expert mode
    m_bJBGLoreAcquired = FALSE;

    // initialize tap fire time
    m_TapRefireTime = 0.0f;

    // the first time, don't let it "double fire"
    m_bCanTapFire = FALSE;

    // Initalize the FlyBys!
    for( i=0 ; i<MAX_FLY_BYS ; i++ )
    {
        x_memset( (void*)&m_BulletFlyBy[i], 0, sizeof(m_BulletFlyBy[i]) );
    }

    LOG_MESSAGE( "player::player", "Addr:%08X", this );

    //#ifndef X_EDITOR
    //s32 nPlayers = g_StateMgr.GetPlayerCount();
    //LOG_MESSAGE( "player::player", 
    //             "Addr:%08X - LocalSlot:%d - TotalLocalPlayers:%d - NetSlot:%d",
    //             this, m_LocalSlot, nPlayers, m_NetSlot );
    //#endif
    

    //==========================================================================
    // Begin code from ghost.cpp
    //==========================================================================

    m_Faction     = FACTION_PLAYER_NORMAL;
    m_FriendFlags = FACTION_WORKERS;   

    // Start out DEAD.  Then spawn.
    m_Health.Dead();
    m_bDead        = TRUE;
    m_bWantToSpawn = TRUE;

    #ifndef X_EDITOR
    actor::m_Net.LifeSeq = -1;  // Odd number means 'dead'.
    #endif

    // Setup pointer to loco for base class to use.
    m_pLoco = &m_Loco;
    m_pLoco->SetGhostMode( TRUE );  // Player controls movement, not animations
   
    //==========================================================================
    // End code from ghost.cpp
    //==========================================================================

#ifndef X_EDITOR
    if( g_StateMgr.UseDefaultLoadOut() )
    {
        DebugSetupInventory( g_ActiveConfig.GetLevelPath() );
    }
#else
    DebugSetupInventory( "<null>" );
#endif

    m_Turret.TurretGuid     = 0;
    m_Turret.Turret2Guid     = 0;
    m_Turret.Turret3Guid     = 0;
    m_Turret.PreviousWeapon = INVEN_WEAPON_SMP;
    m_Turret.PreviousZone1  = 0;
    m_Turret.PreviousZone2  = 0;
    m_Turret.AnchorL2W.Identity();

    for( i = 0; i < MAX_LORE_ITEMS; i++ )
    {
        m_LoreObjectGuids[i] = NULL_GUID;
    }

    //-- Mission Failer code
    {
       m_MissionFailedBmp.SetName( PRELOAD_FILE("SCREENS/UI_MISSION_FAILED.XBMP") );
    }  
}

//=========================================================================

player::~player( void )
{
    SetIsActive( FALSE );

    // Remove the player's ear from the audio manager.
    g_AudioMgr.DestroyEar( m_AudioEarID );
}

//==============================================================================

const object_desc& player::GetTypeDesc( void ) const
{
    return s_Player;
}

//==============================================================================

const object_desc& player::GetObjectType( void )
{
    return s_Player;
}

//=========================================================================

void player::OnInit( void )
{
//  LOG_MESSAGE( "player::OnInit", "" );

    //#ifndef X_EDITOR
    //s32 nPlayers = g_StateMgr.GetPlayerCount();
    //LOG_MESSAGE( "player::OnInit",
    //             "Addr:%08X - LocalSlot:%d - TotalLocalPlayers:%d - NetSlot:%d",
    //             this, m_LocalSlot, nPlayers, m_NetSlot );
    //#endif

    #ifdef X_EDITOR

    ////////////////////
    // NON-NETWORKING //
    ////////////////////

    // HACK for the editor.  Since the networking stuff does not work in 
    // the editor, we need to take care of a little business manually.
    //
    // And, since OnInit seems to get called twice, make sure we don't do this
    // stuff a second time.

    {
        SetLocalPlayer( 0 );
    }

    #endif // X_EDITOR


    if ( UsingLoco() )
    {
        InitLoco();
    }

    m_Physics.Init( GetGuid() );
    m_StepView.Clear();
    m_Physics.SetSolveActorCollisions( TRUE );

    // Call base
    actor::OnInit();

    //
    // Initialize EVERYTHING
    //

    // character physics init
    m_Physics.SetColHeight               ( 180.0f    );
    m_Physics.SetColRadius               ( 30.0f     );                                                                         
    m_Physics.SetColCrouchOffset         ( 70.0f     );
    m_Physics.SetHandlePermeable         ( TRUE      );

    if ( UsingLoco() )
    {
        // loco MOVE_STYLE_WALK
        loco::move_style_info_default Defaults;
        Defaults.m_IdleBlendTime         = 0.2f;
        Defaults.m_MoveBlendTime         = 0.2f;
        Defaults.m_FromPlayAnimBlendTime = 0.2f;
        Defaults.m_MoveTurnRate          = R_180;
        m_Loco.SetMoveStyleDefaults( loco::MOVE_STYLE_WALK,      Defaults );

        // loco MOVE_STYLE_RUN, MOVE_STYLE_RUN_AIM, MOVE_STYLE_CHARGE
        Defaults.m_IdleBlendTime         = 0.125f;
        Defaults.m_MoveBlendTime         = 0.125f;
        Defaults.m_FromPlayAnimBlendTime = 0.125f;
        Defaults.m_MoveTurnRate          = R_360;
        m_Loco.SetMoveStyleDefaults( loco::MOVE_STYLE_RUN,       Defaults );
        m_Loco.SetMoveStyleDefaults( loco::MOVE_STYLE_RUNAIM,    Defaults );
        m_Loco.SetMoveStyleDefaults( loco::MOVE_STYLE_CHARGE,    Defaults );

        // loco MOVE_STYLE_PROWL, MOVE_STYLE_CROUCH, MOVE_STYLE_CROUCHAIM
        Defaults.m_IdleBlendTime         = 0.2f;
        Defaults.m_MoveBlendTime         = 0.2f;
        Defaults.m_FromPlayAnimBlendTime = 0.2f;
        Defaults.m_MoveTurnRate          = R_90;
        m_Loco.SetMoveStyleDefaults( loco::MOVE_STYLE_PROWL,     Defaults );
        m_Loco.SetMoveStyleDefaults( loco::MOVE_STYLE_CROUCH,    Defaults );
        m_Loco.SetMoveStyleDefaults( loco::MOVE_STYLE_CROUCHAIM, Defaults );
    }

    // Fall damage
    tweak_handle SafeFallTweak (xfs("%s_MinFallDistToTakeDamage",GetLogicalName()));
    tweak_handle DeathFallTweak(xfs("%s_MaxFallDistToTakeDamage",GetLogicalName()));
    m_SafeFallAltitude = SafeFallTweak.GetF32();
    m_DeathFallAltitude = DeathFallTweak.GetF32();

    // Blood Decals
    m_hBloodDecalPackage.SetName( PRELOAD_FILE( "Blood.decalpkg" ) );
    m_BloodDecalGroup = 0;

    // Mutant vision
    m_bAllowedToGlow = TRUE;
    m_FriendlyGlowColor.Set(  50, 255, 0, 255 );
    m_EnemyGlowColor.Set   ( 255,  50, 0, 255 );

    // player stuff
    SetFieldOfView( DEG_TO_RAD( (f32)g_StateMgr.GetActiveSettings().GetFieldOfView() ) );
    m_nLoreDiscoveries          = 0;
#if !defined( CONFIG_RETAIL )
    m_bRenderSkeleton           = FALSE;
    m_bRenderSkeletonNames      = TRUE;
    m_bRenderBBox               = TRUE;
#endif // !defined( CONFIG_RETAIL )
    m_PitchArmsScalerPositive   = 1.0f;
    m_PitchArmsScalerNegative   = 1.0f;
    m_bCanDie                   = TRUE;

    // Avatar (only if we're not in split screen)
    if( UsingLoco() )
    {
        PrepPlayerAvatar();
    }

    // Arms
    {
        m_Skin.SetUpSkinGeom( PRELOAD_FILE("FP_PLR_Human_BIND.skingeom") );
        m_Skin.SetVMeshMask( 0 );
        UpdateFirstPersonArmsVisibility();

        // WARNING:
        // It may be some problem here. The resource handles can't start counting references 
        // untill a name has been assign to them. Not only that but when a new name is set it
        // must make sure that the old name is decremented reference wise.
        m_AnimGroup.SetName( PRELOAD_FILE("FP_PlayerArms.anim") );

        // Make sure that this are clear not matter what
        m_iCameraBone        = -1;
        m_iCameraTargetBone  = -1;

        // If we can load this anim group then we need to extract some info
        if( m_AnimGroup.GetPointer() )
        {
            OnAnimationInit();
        }
    }

    m_MaxStunPitchOffset     = R_1;
    m_MaxStunYawOffset       = R_8;
    m_MaxStunRollOffset      = R_3;
    m_fStunYawChangeSpeed    = 0.5f;
    m_fStunPitchChangeSpeed  = 0.1f;
    m_fStunRollChangeSpeed   = 0.2f;
    m_fMaxStunTime           = 5.0f;
    
    m_StrainFriendFlags          = 0;
    m_StrainFriendFlags          |= FACTION_PLAYER_NORMAL;
    m_StrainFriendFlags          |= FACTION_NEUTRAL;
    m_StrainFriendFlags          |= FACTION_MILITARY;
    m_StrainFriendFlags          |= FACTION_WORKERS;

#ifndef X_EDITOR
    if( GameMgr.GetGameType() != GAME_CAMPAIGN )
    {
        m_bMutationMeleeEnabled         = TRUE;
        m_bPrimaryMutationFireEnabled   = TRUE;
        m_bSecondaryMutationFireEnabled = TRUE;
    }
    else
#endif
    {
        m_bMutationMeleeEnabled         = FALSE;
        m_bPrimaryMutationFireEnabled   = FALSE;
        m_bSecondaryMutationFireEnabled = FALSE;
    }

    m_bInTurret     = FALSE;

    // load all the Lore Object guids for us
    LoadAllLoreObjects();
}

//==============================================================================
hud_object* player::GetHud( void )
{
    slot_id SlotID  = g_ObjMgr.GetFirst( object::TYPE_HUD_OBJECT );

    if( SlotID != SLOT_NULL )
    {
        object* pObj = g_ObjMgr.GetObjectBySlot( SlotID );

        // for some reason the object isn't valid
        if( pObj )
        {
            // get the HUD from the object
            return &hud_object::GetSafeType( *pObj );
        }
    }

    // failed
    return NULL;
}

//=============================================================================
//=============================================================================
//  Checks for a LOS to an object
//=============================================================================

xbool player::CanSeeObject(object* pObject)
{
    ASSERT( pObject );
    g_CollisionMgr.LineOfSightSetup( GetGuid(),                             // MovingObjGuid,
                                     GetEyesPosition(),                     // WorldStart,
                                     pObject->GetBBox().GetCenter() );      // WorldEnd,
    g_CollisionMgr.AddToIgnoreList ( pObject->GetGuid() );
    g_CollisionMgr.IgnoreGlass     ();

    g_CollisionMgr.CheckCollisions ( object::TYPE_ALL_TYPES, object::ATTR_BLOCKS_PLAYER_LOS, (object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING)) ;
    if( g_CollisionMgr.m_nCollisions )
    {
        return FALSE;
    }
    
    return TRUE;
}

//===========================================================================

void player::SetLocalPlayer( s32 LocalIndex )
{
    ASSERT( IN_RANGE( 0, LocalIndex, MAX_LOCAL_PLAYERS-1 ) );
    ASSERT( m_LocalSlot <= 0 ); 

    m_LocalSlot = LocalIndex;

    m_ActivePlayerPad = LocalIndex;

#if !defined( X_EDITOR )
    s32 iPad = LocalIndex;

    s32 nIndex = LocalIndex;
    for( iPad = 0; iPad < MAX_LOCAL_PLAYERS; iPad++ )
    {
        if( g_StateMgr.GetControllerRequested( iPad ) )
        {
            if( nIndex == 0 )
            {
                break;
            }

            nIndex--;
        }
    }

    if( iPad >= MAX_LOCAL_PLAYERS )
    {
        iPad = LocalIndex;
        g_StateMgr.SetControllerRequested( iPad, TRUE );
    }

    g_GameInput.SetPlayerDevice( LocalIndex, iPad );

    // enable vibration based on profile settings.
    player_profile& Profile = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( LocalIndex ) );
    g_Input.EnableFeedback( Profile.GetVibration(), iPad );

#else
    // always controller 0.
    g_GameInput.SetPlayerDevice( LocalIndex, 0 );
#endif

#ifndef X_EDITOR
    const player_profile& InputProfile = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( LocalIndex ) );
    g_GameInput.ApplyKeyboardBindings( LocalIndex, InputProfile.GetKeyboardBindings() );
#endif

    ASSERT( g_GameInput.GetPlayerDevice( LocalIndex ) != -1 );
}

//===========================================================================

void player::OnAdvanceSimulation( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "player::OnAdvanceSimulation" );
    LOG_STAT(k_stats_Player);

    // set vibration
#ifndef X_EDITOR
    if( m_LocalSlot != -1 )
    {
        const s32 iPad = g_GameInput.GetPlayerDevice( m_LocalSlot );
        player_profile& Profile =
            g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( m_LocalSlot ) );
        g_Input.EnableFeedback( Profile.GetVibration(), iPad );
    }
#endif

    g_ZoneMgr.UpdateEar( m_AudioEarID );

#ifndef CONFIG_RETAIL
    m_bCanDie = !DEBUG_INVULNERABLE;
#endif

    const xbool HadLaunchResponse = m_Physics.HasLaunchResponse();

    if( !x_isvalid( DeltaTime ) || (DeltaTime < 0.0f) )
    {
        ASSERT( FALSE );
        DeltaTime = 0.0f;
    }

    f32 PlayerTimeDilation = g_PerceptionMgr.GetPlayerTimeDialation();
    if( !x_isvalid( PlayerTimeDilation ) || (PlayerTimeDilation < 0.0f) )
    {
        ASSERT( FALSE );
        PlayerTimeDilation = 0.0f;
    }

    DeltaTime *= PlayerTimeDilation;
    m_DeltaTime = DeltaTime;

    if ( IsChangingMutation() )
    {
        m_MutationChangeTime = 0.0f;
    }
    else
    {
        m_MutationChangeTime += DeltaTime;
    }

    m_UseTime += DeltaTime;

    // Store a pointer to the current weapon.  This pointer is only valid 
    // this frame and can only be used in my methods, not engine overloads.  
    // Must be very careful with this.
    new_weapon* pWeapon = GetCurrentWeaponPtr();

    // Check to see if our current weapon is still in our inventory
    if( pWeapon && !m_Inventory2.HasItem( m_CurrentWeaponItem ) )
    {
        // We no longer have the weapon, drop it
        pWeapon = NULL;
    }

    UpdateUserInput( DeltaTime );

    actor::OnAdvanceSimulation( DeltaTime );

    UpdateMultiplayerDeath();
    UpdateWeaponVisibility();
    UpdateJBGLoreUnlock();
    UpdateFallState( HadLaunchResponse );

    // Handled by player
    WakeUpDoors();
    m_InvalidSoundTimer = x_max( 0.0f, m_InvalidSoundTimer - DeltaTime );

    UpdateCurrentGrenade();

    UpdateSafeSpot( DeltaTime );
    UpdateBulletSounds( DeltaTime );

    // recharge/burn our mutagen if we have the mutation ability
    if( m_Inventory2.HasItem( INVEN_WEAPON_MUTATION ) )
    {
        UpdateMutagen( DeltaTime );
    }
    
    // drain/charge flashlight battery
    UpdateFlashlightBattery( DeltaTime );

    pWeapon = GetCurrentWeaponPtr();
    if( m_bIsMutated && !m_bWasMutated )
    {
        SetMutated( TRUE );
    }

    m_bWasMutated = m_bIsMutated;

    UpdateState( DeltaTime );

    // First advance initializes the strain.
    if( !m_bStrainInitialized )
    {
        SetCurrentStrain();
        m_bStrainInitialized = TRUE;
    }   

    UpdateAudio( DeltaTime );    
    UpdateReticleTarget( DeltaTime );
    GatherGameSpeakGuid();
    
    UpdateActiveNonExclusiveStates( DeltaTime );
    
    // Keep this ordering: ghost loco must be current before the weapon is
    // positioned and the first-person animation is advanced.
    if( !IsAvatar() && IsAlive() && !IsCinemaRunning() )
    {
        // Validate lean at the final physics position before both poses and
        // replicated lean consume it, including platform motion/returning lean.
        ConstrainLean();
    }
    UpdateGhostLoco( DeltaTime );
    UpdateArmsAnimation( pWeapon, DeltaTime );

    // Update any effects
    if( m_pEffects )
    {
        m_pEffects->Update( this, DeltaTime );
    }

    UpdatePlayerView( DeltaTime );
}

//===========================================================================

void player::FinalizeCinemaView( void )
{
    // The ordinary view is needed by simulation consumers during this pass.
    // Refresh only an external cinema view here, after its camera has advanced.
    if( IsCinemaRunning() && (GetCinemaCameraGuid() != 0) )
    {
        // A zero delta samples the final camera pose without advancing any
        // cinematic or view-controller timers for a second time.
        UpdatePlayerView( 0.0f );
    }
}
