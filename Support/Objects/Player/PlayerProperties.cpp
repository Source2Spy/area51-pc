//=========================================================================
//
//  PlayerProperties.cpp
//
//=========================================================================

// GS: This entire system is complete nonsense. 
// Player prop's shouldn't be configurable by level, 
// or at least they should be something like .tweak files. 
// This system needs to be reconsidered.

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "Objects/WeaponMutation.hpp"
#include "StateMgr/StateMgr.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void player::OnEnumProp( prop_enum&  rList )
{
    actor::OnEnumProp       ( rList );
    
    //
    // Player info
    //
    rList.PropEnumHeader     ( "Player", "Player/Mutation information", PROP_TYPE_HEADER );

    rList.PropEnumAngle      ( "Player\\CamFOV",       "This is the Field of View in degrees.", PROP_TYPE_EXPOSE );
    rList.PropEnumRotation   ( "Player\\View Rotation","This rotation sets up the player view on startup. Use the cyan cone as your pointer", 0 );

    rList.PropEnumInt        ( "Player\\LoreDiscoveries", "", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SHOW );
    rList.PropEnumBool       ( "Player\\RenderSkeleton",  "Renders the skeleton of the player. This is use for debugging.", PROP_TYPE_DONT_SAVE_GAME );
    rList.PropEnumBool       ( "Player\\RenderBoneNames", "When the Skeleton is render whether you want to render the name of the bones as well", PROP_TYPE_DONT_SAVE_GAME );
    rList.PropEnumBool       ( "Player\\RenderBBox",      "This allows to turn off and on the BBox of the player.", PROP_TYPE_DONT_SAVE_GAME );

    rList.PropEnumFloat      ( "Player\\ArmPitchModifier+1", "This is a scaler value use to multiply the camera pitch (when>0)so that the arms don't fallow exactly the camera.", PROP_TYPE_DONT_SAVE_GAME );
    rList.PropEnumFloat      ( "Player\\ArmPitchModifier-1", "This is a scaler value use to multiply the camera pitch (when<0)so that the arms don't fallow exactly the camera.", PROP_TYPE_DONT_SAVE_GAME );

    rList.PropEnumBool       ( "Player\\Can Die" , "Determines if the player can die.", PROP_TYPE_DONT_SAVE_GAME | PROP_TYPE_EXPOSE );
    rList.PropEnumBool       ( "Player\\Can Jump" , "Determines if the player can jump.", PROP_TYPE_DONT_SAVE_GAME | PROP_TYPE_EXPOSE );
    rList.PropEnumBool       ( "Player\\Hide Player Arms" , "Do we need to hide the player arms for a special event?.", PROP_TYPE_DONT_SAVE_GAME | PROP_TYPE_EXPOSE );
    rList.PropEnumBool       ( "Player\\Play SwitchTo", "If this is true, this will play the SwitchTo animation after arms re-appear from Hide Player Arms(default is TRUE).", PROP_TYPE_DONT_SAVE_GAME | PROP_TYPE_EXPOSE );
    rList.PropEnumBool       ( "Player\\Using Flashlight", "Indicates if the player is using the flashlight", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SHOW | PROP_TYPE_DONT_SAVE | PROP_TYPE_READ_ONLY );
    rList.PropEnumFloat      ( "Player\\Melee Damage", "This is the damage dished out in one direct melee hit -- not mutation melee", 0 );
    rList.PropEnumFloat      ( "Player\\Melee Force", "This is the force dished out in one direct melee hit -- not mutation melee", 0 );
    rList.PropEnumFloat      ( "Player\\Health", "Player's Health (1-100, 100 = Full Health)", PROP_TYPE_EXPOSE );
    rList.PropEnumFloat      ( "Player\\Mutagen", "Player's Mutagen level (0-100, 100 = Full Mutagen)", PROP_TYPE_EXPOSE );    
    rList.PropEnumBool      ( "Player\\In Mutation Tutorial", "TRUE if we are the mutation tutorial is running, changing mutagen behavior", PROP_TYPE_EXPOSE );
    // Third Person Geometry
    m_SkinInst.OnEnumProp       ( rList );
    rList.PropEnumExternal           ( "RenderInst\\Anim", "Resource\0anim\0", "Resource File", PROP_TYPE_MUST_ENUM | PROP_TYPE_DONT_SAVE_GAME );

    // Skins and animations
    // Enumerate the different strains.
    rList.PropEnumHeader( "Player\\Human Strain", "Properties of the human strain", 0 );
    rList.PropEnumHeader( "Player\\Strain One", "Properties of strain one.", 0 );
    rList.PropEnumHeader( "Player\\Strain Two", "Properties of strain two.", 0 );
    rList.PropEnumHeader( "Player\\Strain Three", "Properties of strain three.", 0 );

//    rList.PropEnumEnum (  "Player\\Current Strain", GetStrainEnum(), "Current strain.", PROP_TYPE_EXPOSE  );

    rList.PropEnumBool (  "Player\\Holster Weapon", "When TRUE, player weapon is hidden, and the user can't cycle or fire", PROP_TYPE_EXPOSE );
    
    // Enumerate the human specific properties
    s32 PathIndex = rList.PushPath( "Player\\Human Strain\\" );
    m_Physics.OnEnumProp( rList );
    m_Skin.OnEnumProp( rList );
    rList.PropEnumExternal( "RenderInst\\AnimFile", "Resource\0anim\0", "Resource Animation File", PROP_TYPE_MUST_ENUM | PROP_TYPE_DONT_SAVE_GAME );
    rList.PropEnumExternal( "Audio Package", "Resource\0audiopkg\0","The audio package associated with human strain.", PROP_TYPE_DONT_SAVE_GAME );
    EnumrateStrainControls( rList );
    rList.PropEnumHeader("Friendly Factions", "Reticle doesn't highlight on friends", 0 );
    s32 ID = rList.PushPath( "Friendly Factions\\" );
    factions_manager::OnEnumFriends( rList );
    rList.PopPath( ID );
    rList.PopPath( PathIndex );

    //  Mutation properties
    rList.PropEnumHeader( "Player\\Mutation", "Properties for mutation behavior", 0 );
    rList.PropEnumBool( "Player\\Mutation\\Is Mutated", "READ ONLY: Indicates if the player is mutated or not.", PROP_TYPE_READ_ONLY | PROP_TYPE_EXPOSE );
    rList.PropEnumBool( "Player\\Mutation\\Can Melee", "TRUE if the player can use the mutant melee attack.", PROP_TYPE_EXPOSE );
    rList.PropEnumBool( "Player\\Mutation\\Can Fire Primary Ammo", "TRUE if the player can use primary mutation ammo.", PROP_TYPE_EXPOSE  );
    rList.PropEnumBool( "Player\\Mutation\\Can Fire Secondary Ammo", "TRUE if the player can use secondary mutation ammo.", PROP_TYPE_EXPOSE  );
    rList.PropEnumBool( "Player\\Mutation\\User Can Toggle Mutation", "TRUE if the user can control mutation through the controller.", PROP_TYPE_EXPOSE );
    rList.PropEnumBool( "Player\\Mutation\\Force To Mutant", "Set to TRUE to force the player to mutate, assuming he has the mutation weapon.", PROP_TYPE_EXPOSE );
    rList.PropEnumBool( "Player\\Mutation\\Force To Human", "Set to TRUE to force the player to become human, assuming he has the mutation weapon.", PROP_TYPE_EXPOSE  );
    rList.PropEnumHeader( "Player\\Cinema", "Properties for cinemas", 0 );
    rList.PropEnumBool  ( "Player\\Cinema\\CinemaOn", "Turns cinema mode and off", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SAVE );
    rList.PropEnumGuid  ( "Player\\Cinema\\CinemaCameraGuid", "This points to the camera that should be the player's view", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SAVE );     
    rList.PropEnumBool  ( "Player\\Cinema\\UseViewCorrection", "Do we correct the view at the end of cinema. Don't use this if you are popping player to position", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SAVE );
    rList.PropEnumGuid  ( "Player\\Cinema\\LookAtTarget", "Object to focus camera on", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SAVE );
    rList.PropEnumFloat ( "Player\\Cinema\\BlendInTime", "How long to blend to desired view", PROP_TYPE_EXPOSE | PROP_TYPE_DONT_SAVE );
}

//=========================================================================

void player::EnumrateStrainControls( prop_enum& List )
{
    List.PropEnumFloat   ( "MaxHealth",    "Maximum health value", 0 );
    List.PropEnumFloat   ( "Proximity Alert Radius", "Radius of Proximity Broadcast which will alert NPC's to player presence.", 0 );

    List.PropEnumHeader  ( "Controls" , "Variables that effect the player's control" , PROP_TYPE_HEADER );
    List.PropEnumFloat   ( "Controls\\MaxFowardVel", "Maximum player movement speed in every horizontal direction", 0 );
    List.PropEnumFloat   ( "Controls\\FowardAccel", "Gamepad acceleration in every horizontal direction", 0 );
    List.PropEnumFloat   ( "Controls\\JumpVelocity", "How fast does the player jump. This is the initial velocity for the jump.", 0 );
    List.PropEnumVector3 ( "Controls\\EyesOffSet",   "This is where the eyes are relative to the top of his head", 0 );
    List.PropEnumFloat   ( "Controls\\Pitch Stick Sensitivity" , "Maximum gamepad pitch rate. Expects a number above zero.", 0 );
    List.PropEnumFloat   ( "Controls\\Yaw Stick Sensitivity" , "Maximum gamepad yaw rate. Expects a number above zero.", 0 );
    List.PropEnumFloat   ( "Controls\\MinWalkSpeed" , "Minimum speed at which the player walks.", 0 );
    List.PropEnumFloat   ( "Controls\\MinRunSpeed" , "Minimum speed at which the player runs.", 0 );
    List.PropEnumFloat   ( "Controls\\Deceleration Multiplier" , "This is how many times faster the player slows down than speeds up.", 0 );
    List.PropEnumFloat   ( "Controls\\Crouch Change Rate" , "This is how fast the player crouches.  10 is a good place to start tweaking.  Lower is slower, higher is faster", 0 );
    List.PropEnumFloat   ( "Controls\\Movement Aim Degradation", "How much aim you are going to lose by movement 0 -> 1", 0 );

    List.PropEnumHeader  ( "Controls\\Stun Properties", "Effects the way this guy is stunned", 0 );
    List.PropEnumAngle   ( "Controls\\Stun Properties\\MaxStunPitchOffset",   "How far does the pitch go?", 0 );
    List.PropEnumAngle   ( "Controls\\Stun Properties\\MaxStunYawOffset",     "How far does the yaw go?", 0 );
    List.PropEnumAngle   ( "Controls\\Stun Properties\\MaxStunRollOffset",    "How far does the roll go?", 0 );
    List.PropEnumFloat   ( "Controls\\Stun Properties\\StunYawChangeSpeed",   "How fast does the yaw change", 0 );
    List.PropEnumFloat   ( "Controls\\Stun Properties\\StunPitchChangeSpeed", "How fast does the yaw change", 0 );
    List.PropEnumFloat   ( "Controls\\Stun Properties\\StunRollChangeSpeed",  "How fast does the yaw change", 0 );
    List.PropEnumFloat   ( "Controls\\Stun Properties\\Stun Time",  "How long does he stay stunned?", 0 );
}

//=========================================================================

xbool player::OnStrainControlProperty( prop_query& I )
{
    if( I.VarFloat( "Proximity Alert Radius", m_StrainControls.m_StrainProximityAlertRadius ) )
    {
        return TRUE;
    }

    if( I.VarFloat( "Controls\\MaxFowardVel", m_StrainControls.m_StrainMaxFowardVelocity ) )
    {
        return TRUE;
    }
    
    // Consume the retired property so existing strain data remains loadable.
    f32 LegacyMaxStrafeVelocity = m_StrainControls.m_StrainMaxFowardVelocity;
    if( I.VarFloat( "Controls\\MaxStrafeVel", LegacyMaxStrafeVelocity ) )
    {
        return TRUE;
    }
    
    if( I.VarFloat( "Controls\\JumpVelocity", m_StrainControls.m_StrainJumpVelocity ) )
    {
        return TRUE;
    }
    
    if( I.VarVector3( "Controls\\EyesOffSet", m_StrainControls.m_StrainEyesOffset ) )
    {
        return TRUE;
    }
    

    if( I.VarFloat( "MaxHealth", m_StrainControls.m_StrainMaxHealth ) )
    {
        return TRUE;
    }

    if( I.VarFloat( "Controls\\FowardAccel" , m_StrainControls.m_fStrainForwardAccel) )
    {
        return TRUE;
    }

    // Consume the retired property so existing strain data remains loadable.
    f32 LegacyStrafeAcceleration = m_StrainControls.m_fStrainForwardAccel;
    if( I.VarFloat( "Controls\\StrafeAccel", LegacyStrafeAcceleration ) )
    {
        return TRUE;
    }


    if( I.VarFloat( "Controls\\Pitch Stick Sensitivity" , m_StrainControls.m_fStrainPitchSensitivity ) )
    {
        return TRUE;
    }

    if( I.VarFloat( "Controls\\Yaw Stick Sensitivity" , m_StrainControls.m_fStrainYawSensitivity ) )
    {
        return TRUE;
    }

    if( I.VarFloat( "Controls\\MinWalkSpeed" , m_StrainControls.m_StrainMinWalkSpeed ) )
    {

        return TRUE;
    }

    if( I.VarFloat  ( "Controls\\MinRunSpeed" , m_StrainControls.m_StrainMinRunSpeed ) )
    {

        return TRUE;
    }

    if( I.VarFloat( "Controls\\Deceleration Multiplier" , m_StrainControls.m_StrainDecelerationFactor ) )
    {
        return TRUE;
    }

    if( I.VarFloat( "Controls\\Crouch Change Rate" , m_StrainControls.m_StrainCrouchChangeRate ) )
    {
        return TRUE;
    }

    if( I.VarFloat( "Controls\\Movement Aim Degradation" , m_StrainControls.m_StrainReticleMovementDegrade, 0.0f, 1.0f ) )
    {
        return TRUE;
    }

    if ( I.VarAngle( "Controls\\Stun Properties\\MaxStunPitchOffset", m_MaxStunPitchOffset ) )
    {
        return TRUE;
    }
    
    if ( I.VarAngle( "Controls\\Stun Properties\\MaxStunYawOffset", m_MaxStunYawOffset ) )
    {
        return TRUE;
    }

    if ( I.VarAngle( "Controls\\Stun Properties\\MaxStunRollOffset", m_MaxStunRollOffset ) )
    {
        return TRUE;
    }
   
    if ( I.VarFloat( "Controls\\Stun Properties\\StunYawChangeSpeed", m_fStunYawChangeSpeed ) )
    {
        return TRUE;
    }

    if ( I.VarFloat( "Controls\\Stun Properties\\StunPitchChangeSpeed", m_fStunPitchChangeSpeed ) )
    {
        return TRUE;
    }

    if ( I.VarFloat( "Controls\\Stun Properties\\StunRollChangeSpeed", m_fStunRollChangeSpeed ) )
    {
        return TRUE;
    }
    
    if ( I.VarFloat( "Controls\\Stun Properties\\Stun Time", m_fMaxStunTime ) )
    {
        return TRUE;
    }
    
    return FALSE;    
}

//=========================================================================

xbool player::OnProperty( prop_query& rPropQuery )
{
    if( rPropQuery.VarBool( "Player\\Can Die", m_bCanDie ) )
    {
        return TRUE;
    }

    if( rPropQuery.VarBool( "Player\\Can Jump", m_bCanJump ) )
    {
        return TRUE;
    }

    if( rPropQuery.VarBool( "Player\\Hide Player Arms", m_bHidePlayerArms ) )
    {
        return TRUE;
    }

    if( rPropQuery.VarBool( "Player\\Play SwitchTo", m_bPlaySwitchTo ) )
    {
        return TRUE;
    }    

    if ( rPropQuery.VarBool( "Player\\Using Flashlight", m_bUsingFlashlight ) )
    {
        return TRUE;
    }

    if ( rPropQuery.IsVar(  "Player\\Cinema\\CinemaOn" ) )
    {
        if ( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarBool( IsCinemaRunning() );
        }
        else
        {
            xbool const CinemaOn = rPropQuery.GetVarBool();
            SetCinemaActive( CinemaOn );

            if ( m_bIsMutated )
            {
                // make sure our mutant vision is on, since the cinema will interrupt any
                // animations, and this will prevent the mutant vision event from firing
                m_bIsMutantVisionOn = TRUE;

                if( CinemaOn )
                {
                    // we need to turn off the mutant perception stuff.
                    g_PerceptionMgr.EndMutate();
                }
                else
                {
                    // we need to turn mutant perception stuff back on.
                    g_PerceptionMgr.BeginMutate();

                    // Force switch to mutation weapon?
                    new_weapon* pWeapon = GetCurrentWeaponPtr();
                    if( ( !pWeapon ) || ( !pWeapon->IsKindOf( weapon_mutation::GetRTTI() ) ) )
                    {
                        m_NextWeaponItem = INVEN_WEAPON_MUTATION;
                        ForceNextWeapon();
                    }

                    // Make sure the weapon mesh is set properly
                    pWeapon = GetCurrentWeaponPtr();
                    ASSERT( pWeapon && pWeapon->IsKindOf( weapon_mutation::GetRTTI() ) );
                    render_inst* pInst = pWeapon->GetRenderInstPtr();
                    ASSERT( pInst );
                    pInst->SetVMeshMask( 0xffffffff );
                }
            }
            else
            {
                // make sure our mutant vision is OFF, since the cinema will interrupt any
                // animations, and this will prevent the mutant vision event from firing
                m_bIsMutantVisionOn = FALSE;

                if( !CinemaOn )
                {
                    // Make sure the weapon mesh is set properly
                    new_weapon* pWeapon = GetCurrentWeaponPtr();
                    if( pWeapon && pWeapon->IsKindOf( weapon_mutation::GetRTTI() ) )
                    {
                        // Turn off mutant hands
                        render_inst* pInst = pWeapon->GetRenderInstPtr();
                        ASSERT( pInst );
                        pInst->SetVMeshMask( 0 );
                        
                        // Force switch to previous weapon?
                        m_NextWeaponItem = m_PreMutationWeapon2;
                        ForceNextWeapon();
                    }
                }
            }

            if ( m_bIsCrouching )
            {
                SetIsCrouching( FALSE );
            }
        }

        if( IsCinemaRunning() )
        {
            // Stop leaning
            m_LeanAmount     = 0.0f;
            m_SoftLeanAmount = 0.0f;
        }
        return TRUE;
    }

    if( rPropQuery.VarGUID( "Player\\Cinema\\CinemaCameraGuid", m_CinemaSettings.CameraGuid ) )
    {
        m_CinemaController.UpdateSettings( m_CinemaSettings );
        return TRUE;
    }

    if( rPropQuery.VarGUID( "Player\\Cinema\\LookAtTarget", m_CinemaSettings.LookAtTargetGuid ) )
    {
        m_CinemaController.UpdateSettings( m_CinemaSettings );
        return TRUE;
    }

    if( rPropQuery.VarBool( "Player\\Cinema\\UseViewCorrection", m_CinemaSettings.UseViewCorrection ) )
    {
        m_CinemaController.UpdateSettings( m_CinemaSettings );
        return TRUE;
    }

    if( rPropQuery.VarFloat( "Player\\Cinema\\BlendInTime", m_CinemaSettings.LookAtBlendTime ) )
    {
        m_CinemaController.UpdateSettings( m_CinemaSettings );
        return TRUE;
    }

    if( rPropQuery.IsVar( "Player\\Health" ) )
    {
        if( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarFloat( m_Health.GetHealth() );
        }
        else
        {
            // Get new health value
            f32 NewHealth = rPropQuery.GetVarFloat();
            if( NewHealth < 1.0f )
            {
                NewHealth = 1.0f;
            }

            // Now add the difference between the new health and current health
            f32 DeltaHealth = NewHealth - m_Health.GetHealth();
            if( DeltaHealth != 0.0f )
            {
                m_Health.Add( DeltaHealth, FALSE );
            }
        }

        return TRUE;
    }

    if ( rPropQuery.VarFloat( "Player\\Mutagen", m_Mutagen, 0, 100.0f ) )
    {
        return TRUE;
    }
    
    if ( rPropQuery.VarBool( "Player\\In Mutation Tutorial", m_bInMutationTutorial ) )
    {
        return TRUE;
    }

    if( object::OnProperty( rPropQuery ) )
    {
        if( !rPropQuery.IsRead() &&
            (rPropQuery.IsVar( "Base\\Position" ) || rPropQuery.IsVar( "Base\\ZoneInfo" )) )
        {
            InitZoneTracking();
        }

        if( rPropQuery.IsVar( "Base\\Position" ) )
        {
            if( rPropQuery.IsRead() )
            {
                m_PositionOfLastSafeSpot = GetPosition();
                m_NextPositionOfLastSafeSpot = m_PositionOfLastSafeSpot ;
                m_RespawnPosition =  GetPosition();
            }
            else
            {
                m_PositionOfLastSafeSpot = rPropQuery.GetVarVector3();
                m_NextPositionOfLastSafeSpot = m_PositionOfLastSafeSpot ;
                m_RespawnPosition = rPropQuery.GetVarVector3();
            }
        }
        else if ( rPropQuery.IsVar( "Base\\ZoneInfo" )  && !rPropQuery.IsRead())
        {
            m_NextZoneIDOfLastSafeSpot = (u8)GetZone1();
            m_ZoneIDOfLastSafeSpot = (u8)GetZone1();
            m_RespawnZone = (u8)GetZone1();
        }
        return TRUE;
    }


    if( rPropQuery.VarString( "Player", m_pPlayerTitle, 256 ) )
    {
        // You can only read this guy
        ASSERT( rPropQuery.IsRead() == TRUE );
        return TRUE;
    }

    if( rPropQuery.VarInt(   "Player\\LoreDiscoveries", m_nLoreDiscoveries) )
    {
        return TRUE;
    }
    
#if !defined( CONFIG_RETAIL )
    if( rPropQuery.VarBool( "Player\\RenderSkeleton", m_bRenderSkeleton ) )
    {
        return TRUE;
    }

    if( rPropQuery.VarBool( "Player\\RenderBoneNames", m_bRenderSkeletonNames ) )
    {
        return TRUE;
    }

    if( rPropQuery.VarBool( "Player\\RenderBBox", m_bRenderBBox ) )
    {
        return TRUE;
    } 
#endif // !defined( CONFIG_RETAIL )

    if( rPropQuery.VarFloat( "Player\\ArmPitchModifier+1", m_PitchArmsScalerPositive ) )
    {
        return TRUE;
    } 

    if( rPropQuery.VarFloat( "Player\\ArmPitchModifier-1", m_PitchArmsScalerNegative ) )
    {
        return TRUE;
    }

    if ( rPropQuery.VarFloat( "Player\\Melee Damage", m_MeleeDamage ) )
    {
        return TRUE;
    }

    if ( rPropQuery.VarFloat( "Player\\Melee Force", m_MeleeForce) )
    {
        return TRUE;
    }

    // GS: This is so stupid I'm speechless. 
    // Why the hell is FOV set in the level editor? What the fuck? 
    // FOV should be controlled by the user, period! I won't use this code.

    // Also TODO: Make FOV settingable and auto scalable in hud_MutantVision, hud_ContagiousVision and weapons stuff.

    //if( rPropQuery.IsVar( "Player\\CamFOV" ) )
    //{
    //    if( rPropQuery.IsRead() )
    //    {
    //        rPropQuery.SetVarAngle( m_ViewInfo.XFOV );
    //    }
    //    else
    //    {
    //        m_ViewInfo.XFOV = rPropQuery.GetVarAngle();
    //        m_OriginalViewInfo.XFOV = m_ViewInfo.XFOV;
    //    }
    //    return TRUE;
    //}

    radian3 Rot( m_Pitch, m_Yaw, 0.0f );
    if ( rPropQuery.VarRotation( "Player\\View Rotation", Rot ) )
    {
        m_Pitch     = Rot.Pitch;
        m_Yaw       = Rot.Yaw;
        return TRUE;
    }

    xbool bIsMutated = m_bIsMutated;
    if ( rPropQuery.VarBool( "Player\\Mutation\\Is Mutated", bIsMutated ) )
    {
        m_bIsMutated = bIsMutated;
        return TRUE;
    }

    if ( rPropQuery.VarBool( "Player\\Mutation\\Can Melee", m_bMutationMeleeEnabled ) )
    {
        return TRUE;
    }

    if ( rPropQuery.VarBool( "Player\\Mutation\\Can Fire Primary Ammo", m_bPrimaryMutationFireEnabled ) )
    {
        return TRUE;
    }

    if ( rPropQuery.VarBool( "Player\\Mutation\\Can Fire Secondary Ammo", m_bSecondaryMutationFireEnabled ) )
    {
        return TRUE;
    }

    // HACK: old editor assets may still serialize these retired convulsion
    // properties. Accept them without enumerating or restoring dead behavior.
    f32 IgnoredConvulsionValue = 0.0f;
    if ( rPropQuery.VarFloat( "Player\\Mutation\\Convulsion Feedback Duration", IgnoredConvulsionValue ) )
    {
        return TRUE;
    }

    if ( rPropQuery.VarFloat( "Player\\Mutation\\Convulsion Feedback Intensity", IgnoredConvulsionValue ) )
    {
        return TRUE;
    }

    xbool Temp = m_bCanToggleMutation;
    if ( rPropQuery.VarBool( "Player\\Mutation\\User Can Toggle Mutation", Temp ) )
    {
        m_bCanToggleMutation = Temp;
        return TRUE;
    }

    xbool TempForce = FALSE;
    if ( rPropQuery.VarBool( "Player\\Mutation\\Force To Mutant", TempForce ) )
    {
        // if we're not mutated, get there
        if ( TempForce && m_Inventory2.HasItem( INVEN_WEAPON_MUTATION ) && !IsMutated() )
        {
            ForceMutationChange( TRUE );
        }

        return TRUE;
    }

    if ( rPropQuery.VarBool( "Player\\Mutation\\Force To Human", TempForce ) )
    {
        // if we're mutant, go human
        if ( TempForce && IsMutated() )
        {
            SetupMutationChange( FALSE );
        }

        return TRUE;
    }

    if( actor::OnProperty( rPropQuery ) )
    {
        return TRUE;
    }

    // Human
    s32 PathIndex = rPropQuery.PushPath( "Player\\Human Strain\\" );
    {
        if ( OnStrainProperty( rPropQuery ) )
        {
            return TRUE;
        }
        if ( OnStrainControlProperty( rPropQuery ) )
        {
            return TRUE;
        }
    }
    rPropQuery.PopPath( PathIndex );

    return FALSE;
}

//=========================================================================

xbool player::OnStrainProperty( prop_query& rPropQuery )
{
    if( m_Skin.OnProperty( rPropQuery ) )
    {
        return TRUE;
    }

    if ( m_Physics.OnProperty( rPropQuery ) )
    {
        return TRUE;
    }

    if( rPropQuery.IsVar( "RenderInst\\AnimFile" ) )
    {
        if( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarExternal( m_AnimGroup.GetName(), RESOURCE_NAME_SIZE );
        }
        else            
        {
            // WARNING:
            // It may be some problem here. The resurce handles can't start counting references 
            // untill a name has been assign to them. Not only that but when a new name is set it
            // must make sure that the old name is decremented reference wise.
            m_AnimGroup.SetName( rPropQuery.GetVarExternal() );

            // Make sure that this are clear not matter what
            m_iCameraBone        = -1;
            m_iCameraTargetBone  = -1;

            // If we can load this animgoup then we need to extract some info
            if( m_AnimGroup.GetPointer() )
                OnAnimationInit( );

            // Notify the user if we don't have certain key bones
            if( m_iCameraBone == -1 )
                x_DebugMsg( "WARNING: There is not Camera bone (bone_cam) in the skeleton of the player(%d)\n", m_pPlayerTitle );

            if( m_iCameraTargetBone == -1 )
                x_DebugMsg( "WARNING: There is not Camera TargetBone bone(bone_cam.Target) in the skeleton of the player(%d)\n", m_pPlayerTitle );
        }
        return TRUE;
    }

    // External
    if( rPropQuery.IsVar( "Audio Package" ) )
    {
        if( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarExternal( m_hAudioPackage.GetName(), RESOURCE_NAME_SIZE );
        }
        else
        {
            // Get the FileName
            const char* pString = rPropQuery.GetVarExternal();

            if( pString[0] )
            {
                if( xstring(pString) == "<null>" )
                {
                    m_hAudioPackage.SetName( "" );
                }
                else
                {
                    m_hAudioPackage.SetName( pString );                

                    // Load the audio package.
                    if( m_hAudioPackage.IsLoaded() == FALSE )
                        m_hAudioPackage.GetPointer();
                }
            }
        }
        return TRUE;
    } 
    
     s32 ID = rPropQuery.PushPath( "Friendly Factions\\" );
     if ( factions_manager::OnPropertyFriends( rPropQuery, m_StrainFriendFlags ) )
     {
         return TRUE;
     }
     rPropQuery.PopPath( ID );



    return FALSE;
}
