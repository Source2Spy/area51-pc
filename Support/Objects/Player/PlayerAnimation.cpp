//=========================================================================

//
//  PlayerAnimation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Objects/Event.hpp"
#include "Objects/GrenadeProjectile.hpp"
#include "Objects/GravChargeProjectile.hpp"
#include "Objects/JumpingBeanProjectile.hpp"
#include "Objects/WeaponMutation.hpp"
#include "GameLib/DebugCheats.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

static tweak_handle JBEAN_PitchThrowOffsetTweak( "JBEAN_PitchThrowOffset" );
static tweak_handle JBEAN_PitchThrowAngleTweak ( "JBEAN_PitchThrowAngle" );
static const f32    s_ArmsVelocityCarryover = 0.5f;

#ifdef DEBUG_GRENADE_THROWING
xbool   g_ShowGrenadeEventCollision = FALSE;
vector3 g_EventPos                  ( 0.0f, 0.0f, 0.0f );
vector3 g_NewEventPos               ( 0.0f, 0.0f, 0.0f );
#endif

//=========================================================================

static
vector3 GetFirstPersonCameraPivotCorrection( char_anim_player& AnimPlayer, s32 CameraBone )
{
    const vector3 BindOffset = (AnimPlayer.GetBindPosition( CameraBone ) -
                                AnimPlayer.GetBindPosition( 0 )) * AnimPlayer.GetScale();

    radian3 Rotation = AnimPlayer.GetRotation();
    Rotation.Yaw -= AnimPlayer.GetAnimInfo().GetHandleAngle();

    return AnimPlayer.GetBasisMatrix().RotateVector(
        PlayerView::GetFirstPersonPivotCorrection( BindOffset, Rotation ) );
}

//=========================================================================

void player::InitializeMeleeAnimStateList(void)
{
    // set index to invalid so GetNextMeleeState() will be setup correctly
    m_MeleeAnimStateIndex = -1;

    // initialize list
    for( s32 i=0; i < MAX_MELEE_STATES; i++ )
    {
        m_MeleeAnimStates[i] = animation_state(ANIM_STATE_MELEE+1+i);
    }

    // randomize it initially
    RandomizeMeleeAnimStateList();
}

//=========================================================================

void player::RandomizeMeleeAnimStateList( void )
{
    s32 j = 0;
    animation_state temp_AnimState;
    s32 maxStates = MAX_MELEE_STATES - 1;

    // save off last anim state in list so we can make sure it's not the top one in the new list
    animation_state LastAnimState = m_MeleeAnimStates[maxStates];
    for( s32 i=0; i < MAX_MELEE_STATES; i++ )
    {
        j = x_irand(0, maxStates);
        temp_AnimState = m_MeleeAnimStates[i];
        m_MeleeAnimStates[i] = m_MeleeAnimStates[j];
        m_MeleeAnimStates[j] = temp_AnimState;
    }

    // oops the last anim in the old list is at the top of the new list, switch.
    if( LastAnimState == m_MeleeAnimStates[0] )
    {
        // put it at the end again (doing this because we at least know the list is as big as 0 -> maxStates).
        temp_AnimState = m_MeleeAnimStates[0];
        m_MeleeAnimStates[0] = m_MeleeAnimStates[maxStates];
        m_MeleeAnimStates[maxStates] = temp_AnimState;
    }
}

//=========================================================================

player::animation_state player::GetNextMeleeState( void )
{
    m_MeleeAnimStateIndex++;
    if( m_MeleeAnimStateIndex >= MAX_MELEE_STATES )
    {
        RandomizeMeleeAnimStateList();
        m_MeleeAnimStateIndex = 0;
    }
    
    ASSERT( m_MeleeAnimStateIndex < MAX_MELEE_STATES && m_MeleeAnimStateIndex >= 0 );

    return m_MeleeAnimStates[m_MeleeAnimStateIndex];
}

//=========================================================================

void player::OnAnimationInit( void )
{
    s32 i;

    LOG_MESSAGE( "player::OnAnimationInit", "" );

    m_AnimPlayer.SetAnimGroup( m_AnimGroup );
    m_AnimPlayer.SetAnim( 0, TRUE, TRUE );

    if( m_AnimPlayer.GetBoneIndex( "bone_cam") != -1 ) 
        m_iCameraBone = m_AnimPlayer.GetBoneIndex( "bone_cam");

    if( m_AnimPlayer.GetBoneIndex( "bone_cam.Target") != -1 )
        m_iCameraTargetBone = m_AnimPlayer.GetBoneIndex( "bone_cam.Target");

    // Reset our animations for this strain.
    ResetAnimationTable( );

    // Start collecting animations for each state
    animation_state AnimIndex = ANIM_STATE_UNDEFINED;
    const anim_group& AnimGroup = m_AnimPlayer.GetAnimGroup();

    for(  i=0; i < AnimGroup.GetNAnims(); i++ )
    {
        s32 WeaponIndex = -1;
        AnimIndex = ANIM_STATE_UNDEFINED;

        const anim_info& AnimInfo   = AnimGroup.GetAnimInfo( i );
        const char*      pAnimName  = AnimInfo.GetName();

        // This animation is always there...
        if ( x_strcmp( pAnimName, "BIND_POSE" ) == 0 )
            continue;

        WeaponIndex = inventory2::ItemToWeaponIndex( GetWeaponFromAnimName( pAnimName ) );
        AnimIndex   = GetAnimStateFromName( pAnimName );

        if(   !((WeaponIndex == 0) && (AnimIndex == ANIM_STATE_DEATH)) 
            && ((WeaponIndex <= 0) || (AnimIndex < 0)) )
        {
            //x_try;
            //x_throw( xfs( "WARNING!: Don't know what to do with animation %s for player" , pAnimName ));
            //x_catch_display;
            continue;
        }

        //We have valid index to the animation table, now set the values
        state_anims& State =  m_Anim[WeaponIndex][AnimIndex];
        if( State.nPlayerAnims >= MAX_ANIM_PER_STATE )
        {
            x_throw( xfs( "Too many animations of this type %s for player" , pAnimName ));
            return;
        }
        else
        {
            if ( AnimIndex >= ANIM_STATE_DEATH )
            {
                s32 nAnimState = AnimIndex;
                for ( s32 j = 0; j < INVEN_NUM_WEAPONS; j++ )
                {
                    m_Anim[j][nAnimState].PlayerAnim[ANIM_PRIORITY_DEFAULT] = i;
                    m_Anim[j][nAnimState].nPlayerAnims = 1;
                    m_Anim[j][nAnimState].nWeaponAnims++;
                }
            }

            // Set the index of the animation in the table at the appropriate place.
            ASSERT(State.nPlayerAnims < MAX_ANIM_PER_STATE);
            State.PlayerAnim[State.nPlayerAnims] = i;
            State.nPlayerAnims++;
        }
        
    }

}

//=========================================================================

void player::ResetAnimationTable( void )
{
    LOG_MESSAGE( "player::ResetAnimationTable", "" );

    s32 i,j;

    //clear the animation array.
    for( i=0; i < INVEN_NUM_WEAPONS; i++ )
    {
        for ( j = 0; j < ANIM_STATE_MAX; j++ )
        {
            m_Anim[i][j].nPlayerAnims = 0;
            m_Anim[i][j].nWeaponAnims = 0;
        }
    }
}

//=========================================================================

inven_item player::GetWeaponFromAnimName( const char* pAnimName )
{
    //parse for weapon name.  Documentation on naming conventions used for the player and weapon animations can
    //be found in C:\GameData\A51\Source\Art\Characters\Mut 01_02 - Arms\NOTES_MUT01_01.txt
    inven_item retValue = INVEN_NULL;

    if( x_stristr( pAnimName, "SMP_" ) )
    {
        retValue = INVEN_WEAPON_SMP;
    }

// KSS -- TO ADD NEW WEAPON
    else if( x_stristr( pAnimName, "SHT_" ) )
    {
        retValue = INVEN_WEAPON_SHOTGUN;
    }
    else if( x_stristr( pAnimName, "SCN_" ) )
    {
        retValue = INVEN_WEAPON_SCANNER;
    }
    else if( x_stristr( pAnimName, "SNI_" ) )
    {
        retValue = INVEN_WEAPON_SNIPER_RIFLE;
    }
    else if ( x_stristr( pAnimName, "EGL_" ) )
    {
        retValue = INVEN_WEAPON_DESERT_EAGLE;
    }
    else if( x_stristr( pAnimName, "MSN_" ) )
    {
        retValue = INVEN_WEAPON_MESON_CANNON;
    }
    else if( x_stristr( pAnimName, "BBG_" ) )
    {
        retValue = INVEN_WEAPON_BBG;
    }
    else if( x_stristr( pAnimName, "TRA_" ) )
    {
        retValue = INVEN_WEAPON_TRA;
    }
    else if( x_stristr( pAnimName, "2MP_" ) )
    {
        retValue = INVEN_WEAPON_DUAL_SMP;
    }
    else if( x_stristr( pAnimName, "2SH_" ) )
    {
        retValue = INVEN_WEAPON_DUAL_SHT;
    }
    else if( x_stristr( pAnimName, "MUT_" ) )
    {
        retValue = INVEN_WEAPON_MUTATION;
    }

    return retValue;
}

//=========================================================================

player::animation_state player::GetAnimStateFromName( const char* pAnimName )
{
    //parse for animation state.  Documentation on naming conventions used for the player and weapon animations can
    //be found in C:\GameData\A51\Source\Art\Characters\Mut 01_02 - Arms\NOTES_MUT01_01.txt
    animation_state retValue = ANIM_STATE_UNDEFINED;

    if( x_stristr( pAnimName, "_Idle" ) )
    {
        retValue = ANIM_STATE_IDLE;
    }
    else if( x_stristr( pAnimName, "_Switch_To" ) )
    {
        retValue = ANIM_STATE_SWITCH_TO;
    }
    else if( x_stristr( pAnimName, "_Switch_From" ) )
    {
        retValue = ANIM_STATE_SWITCH_FROM;
    }
    else if( x_stristr( pAnimName, "_Pickup" ) )
    {
        retValue = ANIM_STATE_PICKUP;
    }
    else if( x_stristr( pAnimName, "_Discard" ) )
    {
        retValue = ANIM_STATE_DISCARD;
    }
//
// NEEDS TO RESOLVE THIS "_Reload" will return true when looking at ".._Reload_IN"
// for now we will check the Reload_In and Reload_Out stuff first.
//
//
    else if( x_stristr( pAnimName, "_Load_IN" ) )
    {
        retValue = ANIM_STATE_RELOAD_IN;
    }
    else if( x_stristr( pAnimName, "_Load_OUT" ) )
    {
        retValue = ANIM_STATE_RELOAD_OUT;
    }

    else if( x_stristr( pAnimName, "_Reload" ) )
    {
        retValue = ANIM_STATE_RELOAD;
    }
    else if( x_stristr( pAnimName, "_Fire" ) )
    {
        retValue = ANIM_STATE_FIRE;
    }
    else if( x_stristr( pAnimName, "_AltFire" ) )
    {
        retValue = ANIM_STATE_ALT_FIRE;
    }
    else if( x_stristr( pAnimName, "_Grenade" ) )
    {
        retValue = ANIM_STATE_GRENADE;
    }
    else if( x_stristr( pAnimName, "_AltGrenade" ) )
    {
        retValue = ANIM_STATE_ALT_GRENADE;
    }
    // this is the mutation melee "spear"
    else if( x_stristr( pAnimName, "_Spear" ) )
    {
        retValue = ANIM_STATE_MUTATION_SPEAR;
    }    
/////////////////////////////////////////
// START -- Melee Section
    else if( x_stristr( pAnimName, "_Melee" ) )
    {
        retValue = ANIM_STATE_MELEE;
    }
    else if( x_stristr( pAnimName, "_AttackFromCenter" ) )
    {
        retValue = ANIM_STATE_MELEE_FROM_CENTER;
    }
    else if( x_stristr( pAnimName, "_AttackFromDown" ) )
    {
        retValue = ANIM_STATE_MELEE_FROM_DOWN;
    }
    else if( x_stristr( pAnimName, "_AttackFromLeft" ) )
    {
        retValue = ANIM_STATE_MELEE_FROM_LEFT;
    }
    else if( x_stristr( pAnimName, "_AttackFromRight" ) )
    {
        retValue = ANIM_STATE_MELEE_FROM_RIGHT;
    }
    else if( x_stristr( pAnimName, "_AttackFromUp" ) )
    {
        retValue = ANIM_STATE_MELEE_FROM_UP;
    }
// END -- Melee Section
/////////////////////////////////////////

/////////////////////////////////////////
// START -- Combo Section
    else if( x_stristr( pAnimName, "_ComboBegin"))
    {
        retValue = ANIM_STATE_COMBO_BEGIN;
    }
    else if( x_stristr( pAnimName, "_ComboHit"))
    {
        retValue = ANIM_STATE_COMBO_HIT;
    }
    else if( x_stristr( pAnimName, "_ComboEnd"))
    {
        retValue = ANIM_STATE_COMBO_END;
    }
// END -- Combo Section
/////////////////////////////////////////

    else if( x_stristr( pAnimName, "_Ramp_Up" ) )
    {
        retValue = ANIM_STATE_RAMP_UP;
    }
    else if( x_stristr( pAnimName, "_Ramp_Down" ) )
    {
        retValue = ANIM_STATE_RAMP_DOWN;
    }
    else if( x_stristr( pAnimName, "_Hold" ) )
    {
        retValue = ANIM_STATE_HOLD;
    }
    else if ( x_stristr( pAnimName, "_AltHold" ) )
    {
        retValue =  ANIM_STATE_ALT_HOLD;
    }
    else if( x_stristr( pAnimName, "_Run" ) )
    {
        retValue = ANIM_STATE_RUN;
    }
    else if( x_stristr( pAnimName, "_Death01" ) )
    {
        retValue = ANIM_STATE_DEATH;
    }
    else if ( x_stristr( pAnimName, "_Mutation_" ) )
    {
        retValue = ANIM_STATE_CHANGE_MUTATION;
    }
    else if ( x_stristr( pAnimName, "_AltRamp_Up" ) )
    {
        retValue = ANIM_STATE_ALT_RAMP_UP;
    }
    else if ( x_stristr( pAnimName, "_AltRamp_Down" ) )
    {
        retValue = ANIM_STATE_ALT_RAMP_DOWN;
    }
    else if ( x_stristr( pAnimName, "_ZoomIn" ) )
    {
        retValue = ANIM_STATE_ZOOM_IN;
    }
    else if ( x_stristr( pAnimName, "_ZoomOut" ) )
    {
        retValue = ANIM_STATE_ZOOM_OUT;
    }
    else if ( x_stristr( pAnimName, "_ZoomIdle" ) )
    {
        retValue = ANIM_STATE_ZOOM_IDLE;
    }
    else if ( x_stristr( pAnimName, "_ZoomRun" ) )
    {
        retValue = ANIM_STATE_ZOOM_RUN;
    }
    else if ( x_stristr( pAnimName, "_ZoomFire" ) )
    {
        retValue = ANIM_STATE_ZOOM_FIRE;
    }
    else
    {
        retValue = ANIM_STATE_UNDEFINED;
    }

    return retValue;
}

//=========================================================================

player::animation_state player::GetMotionTransitionAnimState( void )
{
    animation_state retState = ANIM_STATE_UNDEFINED;

    f32     VelocitySquared = m_Physics.GetMotionVelocity().LengthSquared();
    xbool   bIdle           = VelocitySquared < m_fMinRunSpeed;

    if ( bIdle )
    {
        retState = ANIM_STATE_IDLE;
    }
    else
    {
        //if we're transitioning from 
        retState = ANIM_STATE_RUN;
    }

    return retState;    
}

//=========================================================================

void player::SetAnimation( const animation_state& AnimState , const s32& nAnimIndex , const f32& fBlendTime )
{
    // increment our pain event ID whenever we change animations;
    m_CurrentPainEventID = pain_event::CurrentEventID++;
    if( pain_event::CurrentEventID >= S32_MAX )
    {
        pain_event::CurrentEventID = 0;
    }

    //Get a reference to the state that we are considering
    s32 WeaponIndex = inventory2::ItemToWeaponIndex( m_CurrentWeaponItem );
    state_anims& State          = m_Anim[WeaponIndex][AnimState];
    state_anims& WeaponState    = m_Anim[WeaponIndex][AnimState];
    

    xbool bResetFrame       = (m_CurrentAnimStateIndex == nAnimIndex) ? TRUE : FALSE;

    m_PreviousAnimIndex     = m_CurrentAnimIndex;
    m_CurrentAnimIndex      = WeaponState.WeaponAnim[nAnimIndex];

    m_PreviousAnimStateIndex= m_CurrentAnimStateIndex;
    m_CurrentAnimStateIndex = nAnimIndex;

    if( State.nPlayerAnims > nAnimIndex &&  WeaponState.nWeaponAnims > nAnimIndex )
    {
        xbool bWeaponAnimationSet = FALSE;

        //set the animation in the player.
        m_AnimPlayer.SetAnim( State.PlayerAnim[nAnimIndex], TRUE, TRUE , fBlendTime , bResetFrame );
        
        switch ( m_CurrentAnimState )
        {
        case ANIM_STATE_DEATH:              // Intentional fallthrough
        case ANIM_STATE_MISSION_FAILED:
            // do nothing
            break; 
        default:
            {
                //set the animation for the weapon
                new_weapon* pWeapon = GetCurrentWeaponPtr();
                if ( pWeapon )
                {
                    pWeapon->SetAnimation( WeaponState.WeaponAnim[nAnimIndex] , fBlendTime , bResetFrame );
                    bWeaponAnimationSet = TRUE;
                }
            }
        }
    }
}

//=========================================================================

void player::CameraFall( f32 fPercentHeight )
{
    f32 fHeight =  GetBBox().Min.GetY() + ((GetBBox().GetSize().GetY() + m_EyesOffset.GetY() ) * fPercentHeight) + 10.0f; //10 cm buffer
    m_PosOverrideCamera = vector3( GetPosition().GetX(), fHeight, GetPosition().GetZ() );
    MoveAnimPlayer( m_PosOverrideCamera );
}

//=========================================================================

void player::OnAnimEvents( void )
{
    g_EventMgr.HandleSuperEvents( m_AnimPlayer, this );
}

//=========================================================================

void player::OnEvent( const event& Event )
{
    (void)Event;

    if( m_ActivePlayerPad == -1 )
        return;

    if( Event.Type == event::EVENT_INTENSITY )
    {
         const intensity_event& IntensityEvent = intensity_event::GetSafeType( Event );
         
         DoFeedback(IntensityEvent.ControllerDuration, IntensityEvent.ControllerIntensity);
         ShakeView(IntensityEvent.CameraShakeTime, IntensityEvent.CameraShakeAmount, IntensityEvent.CameraShakeSpeed );
    }

    if( Event.Type == event::EVENT_GENERIC )
    {
        const generic_event& GenericEvent = generic_event::GetSafeType( Event );
        if( x_strcmp( GenericEvent.GenericType, "FP_Mutation_Switch" ) == 0 )
        {
            s32 WeaponIndex = inventory2::ItemToWeaponIndex( m_CurrentWeaponItem );
            s32 nAnimIndex = m_Anim[WeaponIndex][ANIM_STATE_CHANGE_MUTATION].PlayerAnim[0];
            f32 nFrame = m_AnimPlayer.GetFrame();
            
            m_AnimPlayer.SetAnim( nAnimIndex, TRUE, TRUE, 0.f );
            m_AnimPlayer.SetFrame(nFrame);

            SetAnimState( ANIM_STATE_CHANGE_MUTATION );
//          g_PostEffectMgr.AddToHowlBlur( 0.4f, 0.5f, 1.0f, .3f );
        }
        else if( x_strcmp( GenericEvent.GenericType, "Player_Death" ) == 0 )
        {
            if ((m_CurrentAnimState == ANIM_STATE_DEATH) && (m_AnimStage > 1))
            {
                const anim_event& Event = m_AnimPlayer.GetEvent( GenericEvent.EventIndex );
                // the timerange here is for falling
                f32 nTotalFramesForEvent = (f32)Event.GetInt(anim_event::INT_IDX_END_FRAME) - Event.GetInt(anim_event::INT_IDX_START_FRAME);
                f32 nCurrentEventFrame = m_AnimPlayer.GetFrame() - Event.GetInt(anim_event::INT_IDX_START_FRAME);
                f32 fPercentHeight =  ((nTotalFramesForEvent-nCurrentEventFrame)/nTotalFramesForEvent);
                CameraFall(fPercentHeight);
            }
        }
        else if( x_strcmp( GenericEvent.GenericType, "Spear_Out_Left" ) == 0 )
        {
            GetMutationMeleeWeapon()->FireTendril( GetEyesPosition() , 
                                                    m_ForwardVelocity + m_StrafeVelocity, 
                                                    GetProjectileTrajectory() , 
                                                    GetGuid(), TRUE );
        }
        else if( x_strcmp( GenericEvent.GenericType, "Spear_Out_Right" ) == 0 )
        {
            GetMutationMeleeWeapon()->FireTendril( GetEyesPosition(), 
                                                   m_ForwardVelocity + m_StrafeVelocity, 
                                                   GetProjectileTrajectory(), 
                                                   GetGuid(), FALSE );
        }
        else if( x_strcmp( GenericEvent.GenericType, "Spear_In_Left" ) == 0 )
        {
            GetMutationMeleeWeapon()->RetractTendril(TRUE);
        }
        else if( x_strcmp( GenericEvent.GenericType, "Spear_In_Right" ) == 0 )
        {
            GetMutationMeleeWeapon()->RetractTendril(FALSE);
        }
        else if( x_strcmp( GenericEvent.GenericType, "Mutant Vision" ) == 0 )
        {
            if ( IsMutated() )
                m_bIsMutantVisionOn = TRUE;
            else
                m_bIsMutantVisionOn = FALSE;


        }
        // check for mutant melee stuff here
        else if( x_stristr( GenericEvent.GenericType, "MeleeFrom" ) )
        {
            GetMutationMeleeWeapon()->DoExtremeMelee();
        }
        else if( x_stristr(GenericEvent.GenericType, "Combo_Start") )
        {
            // threshold start
            m_bCanRequestCombo = TRUE;
        }
        else if( x_stristr(GenericEvent.GenericType, "Combo_End") )
        {
            // threshold timed out
            m_bCanRequestCombo = FALSE;
        }
        else if( x_stristr(GenericEvent.GenericType, "CompletedReload") )
        {

            // reload sequence has gone far enough to count, the rest is fluff
            new_weapon *pWeapon = GetCurrentWeaponPtr();
            if( pWeapon )
            {
                pWeapon->SetReloadCompleted(TRUE);
            }
        }
    }
    else if( Event.Type == event::EVENT_WEAPON )
    {
        const weapon_event& WeaponEvent = weapon_event::GetSafeType( Event );
        
        switch( WeaponEvent.WeaponState )
        {
            case new_weapon::EVENT_FIRE:
            case new_weapon::EVENT_FIRE_LEFT:
            case new_weapon::EVENT_FIRE_RIGHT:
            {
                // don't allow player to switch weapons, zoom in, attack, etc.
                if( m_bHidePlayerArms )
                {
                    break;
                }

                s32 iFirePoint = -1;
                
                switch( WeaponEvent.WeaponState )
                {
                    case new_weapon::EVENT_FIRE:        iFirePoint = new_weapon::FIRE_POINT_DEFAULT; break;
                    case new_weapon::EVENT_FIRE_LEFT:   iFirePoint = new_weapon::FIRE_POINT_LEFT;    break;
                    case new_weapon::EVENT_FIRE_RIGHT:  iFirePoint = new_weapon::FIRE_POINT_RIGHT;   break;
                }

                new_weapon* pWeapon = GetCurrentWeaponPtr();
                
                if( pWeapon )
                {
                    new_weapon::reticle_radius_parameters ReticleParams = GetReticleParams();
                    m_ReticleShotPenalty += ReticleParams.m_PenaltyForShot;
                    const f32 MaxPenalty = (ReticleParams.m_MaxRadius- ReticleParams.m_MaxMovementPenalty) - ReticleParams.m_MinRadius;
                    m_ReticleShotPenalty = MIN( MaxPenalty, m_ReticleShotPenalty );
                    pWeapon->SetTarget( GetEnemyOnReticle() );
                    pWeapon->FireWeapon( GetEyesPosition() , m_ForwardVelocity + m_StrafeVelocity, m_WpnHoldTime, GetProjectileTrajectory() , GetGuid(), iFirePoint );
                }

                //ShakeView( s_FireShakeTime, s_FireShakeAmount, s_FireShakeSpeed );
                //DoFeedback(s_FireFeedbackDuration, s_FireFeedbackIntensity );
            }
            break;
            case new_weapon::EVENT_ALT_FIRE:
            case new_weapon::EVENT_ALT_FIRE_LEFT: 
            case new_weapon::EVENT_ALT_FIRE_RIGHT: 
            {
                // don't allow player to switch weapons, zoom in, attack, etc.
                if( m_bHidePlayerArms )
                {
                    break;
                }

                s32 iFirePoint = -1;
                
                switch( WeaponEvent.WeaponState )
                {
                    case new_weapon::EVENT_ALT_FIRE:        iFirePoint = new_weapon::FIRE_POINT_DEFAULT; break;
                    case new_weapon::EVENT_ALT_FIRE_LEFT:   iFirePoint = new_weapon::FIRE_POINT_LEFT;    break;
                    case new_weapon::EVENT_ALT_FIRE_RIGHT:  iFirePoint = new_weapon::FIRE_POINT_RIGHT;   break;
                }

                new_weapon* pWeapon = GetCurrentWeaponPtr();
                if( pWeapon )
                {
                    new_weapon::reticle_radius_parameters ReticleParams = GetReticleParams();
                    m_ReticleShotPenalty += ReticleParams.m_PenaltyForShot;
                    const f32 MaxPenalty = (ReticleParams.m_MaxRadius- ReticleParams.m_MaxMovementPenalty) - ReticleParams.m_MinRadius;
                    m_ReticleShotPenalty = MIN( MaxPenalty, m_ReticleShotPenalty );
                    pWeapon->SetTarget( GetEnemyOnReticle() );
                    pWeapon->FireSecondary( GetEyesPosition() , m_ForwardVelocity + m_StrafeVelocity, m_WpnHoldTime, GetProjectileTrajectory() , GetGuid(), iFirePoint );
                }

                //ShakeView( s_AltFireShakeTime, s_AltFireShakeAmount, s_AltFireShakeSpeed );
                //DoFeedback( s_AltFireFeedbackDuration, s_AltFireFeedbackIntensity );
            }
            break;
            case new_weapon::EVENT_GRENADE:
            {
                // don't allow player to switch weapons, zoom in, attack, etc.
                if( m_bHidePlayerArms )
                {
                    break;
                }

                tweak_handle SpeedTweak("PLAYER_GrenadeThrowSpeed");
                // Compute velocity
                vector3 Dir = GetSimulationView().GetViewZ();
                // TODO: Tweak throw speed based on pitch of vector, less power when looking down, etc.
                vector3 Velocity = Dir * SpeedTweak.GetF32();
                Velocity += m_ForwardVelocity + m_StrafeVelocity;

                pain_handle PainHandle(xfs("%s_GRENADE",GetLogicalName()));

                // which grenade do we throw?
                if( m_CurrentGrenadeType2 == INVEN_GRENADE_FRAG )
                {
                    // Create the Grenade projectile.
                    guid GrenadeID = CREATE_NET_OBJECT( grenade_projectile::GetObjectType(), TYPE_GRENADE );
                    grenade_projectile* pFragGrenade = ( grenade_projectile* ) g_ObjMgr.GetObjectByGuid( GrenadeID );
                    
                    // make sure the grenade was created.
                    ASSERT( pFragGrenade );

                    // New Position
                    vector3 NewEventPos = SetupGrenadeThrow( WeaponEvent.Pos );

                    pFragGrenade->Setup( GetGuid(),
                        net_GetSlot(),
                        NewEventPos,
                        radian3(0.0f,0.0f,0.0f),
                        Velocity,
                        GetZone1(),
                        GetZone2(),
                        PainHandle );
                
                    if( !DEBUG_INFINITE_AMMO )
                    {
                        m_Inventory2.RemoveAmount( m_CurrentGrenadeType2, 1.0f );
                    }

                    #ifndef X_EDITOR
                    m_NetDirtyBits |= TOSS_BIT; // NETWORK
                    #endif // X_EDITOR
                }
            }    
            break;
            case new_weapon::EVENT_ALT_GRENADE:
            {
                // don't allow player to switch weapons, zoom in, attack, etc.
                if( m_bHidePlayerArms )
                {
                    break;
                }

                pain_handle PainHandle(xfs("%s_JBEAN",GetLogicalName()));

                // which grenade do we throw?
                if( m_CurrentGrenadeType2 == INVEN_GRENADE_JBEAN )
                { 
                    // Create the Jumping Bean Grenade projectile.
                    guid GrenadeID = CREATE_NET_OBJECT( jumping_bean_projectile::GetObjectType(), TYPE_JBEAN_GRENADE );
                    jumping_bean_projectile* pJBeanGrenade = ( jumping_bean_projectile* ) g_ObjMgr.GetObjectByGuid( GrenadeID );

                    // make sure the grenade was created.
                    ASSERT( pJBeanGrenade );

                    if( !pJBeanGrenade )
                    {
                        return;
                    }

                    f32 Speed = 0.0f;
                    xbool bExpert = (pJBeanGrenade->GetGrenadeMode() == GM_EXPERT);
                    #ifndef X_EDITOR
                    if( GameMgr.GetGameType() != GAME_CAMPAIGN )
                    {
                        bExpert = FALSE;
                    }
                    #endif
                    if( bExpert )
                    {
                        tweak_handle SpeedTweak("JBEAN_ThrowSpeed");
                        Speed = SpeedTweak.GetF32();
                    }
                    else
                    {
                        tweak_handle SpeedTweak("JBEAN_ThrowSpeed_Normal");
                        Speed = SpeedTweak.GetF32();
                    }

                    // Compute velocity
                    vector3 Dir = GetSimulationView().GetViewZ();
                    // TODO: Tweak throw speed based on pitch of vector, less power when looking down, etc.
                    vector3 Velocity = Dir * Speed;
                    Velocity += m_ForwardVelocity + m_StrafeVelocity;

                    if( bExpert )
                    {
                        radian Pitch, Yaw;
                        Velocity.GetPitchYaw(Pitch, Yaw);

                        radian JBG_PitchAngle = JBEAN_PitchThrowAngleTweak.GetRadian();

                        // up to a point, rotate the way for a faked "lob" of the grenade
                        if( Pitch > JBG_PitchAngle )
                        {
                            f32 T = 1.0f;

                            // if we're looking upwards, scale pitch offset
                            if( Pitch < R_0 )
                            {
                                T = Pitch/JBG_PitchAngle;

                                // since the values have to be backwards to work, must flip T
                                T = 1.0f - T;
                            }

                            radian JBG_PitchOffset = JBEAN_PitchThrowOffsetTweak.GetRadian();
                            Pitch -= JBG_PitchOffset*T;
                            f32 Scalar = Velocity.Length();
                            Velocity.Set(Pitch, Yaw);
                            Velocity.Scale(Scalar);
                        }
                    }

                    vector3 NewEventPos = SetupGrenadeThrow( WeaponEvent.Pos );

                    pJBeanGrenade->Setup( GetGuid(),
                        net_GetSlot(),
                        NewEventPos,
                        radian3(0.0f,0.0f,0.0f),
                        Velocity,
                        GetZone1(),
                        GetZone2(),
                        PainHandle );
                
                    if( !DEBUG_INFINITE_AMMO )
                    {
                        m_Inventory2.RemoveAmount( m_CurrentGrenadeType2, 1.0f );
                    }

                    #ifndef X_EDITOR
                    m_NetDirtyBits |= TOSS_BIT; // NETWORK
                    #endif // X_EDITOR
                }
            }   
            break;

            default:
            break;
        }
    }
    else if( Event.Type == event::EVENT_PAIN )
    {
        const pain_event& PainSuperEvent = pain_event::GetSafeType( Event );

        // check if this is a melee pain and kick it off
        if( ( PainSuperEvent.PainType == pain_event::EVENT_PAIN_MELEE ) )
        {
            EmitMeleePain();
        }
    }
}

//=========================================================================

vector3 player::SetupGrenadeThrow( const vector3 &EventPos )
{
    vector3 NewEventPos = EventPos;

#ifdef DEBUG_GRENADE_THROWING
    // for drawing spheres and such
    g_EventPos = NewEventPos;
#endif
    // do some LOS checks here so we don't throw the grenade through the wall.
    {
        /*
        3
        /|
        / |
        /  |
        /   |
        /    |
        /     |
        1------2
        (arm)
        1 = eyes position
        2 = point perpendicular to eyes position
        3 = event pos

        To make sure we can throw the grenade at the event pos, we need to:
        ~ make sure that we can see point 2 from point 1
        AND, If you can see it then,
        ~ make sure that we can see point 3 from point 2
        if not, it failed right off

        This will ensure all bases are covered in events such as:

        =====================================================================
        (FAIL)
                    3           - Actual Event Position (3)
        --------------------    - Wall
                    N           - New event position (N)
             1------|2          - Player Eye Position (1) and point perpendicular to eyes position (2)
        (arm)

        * You can't see point 2 from put 1 and you can't see point 3 from point 2

        =====================================================================
        (FAIL) 
                      (wall)
                         /
            3      _2  /N           - Actual Event Position (3) then New Event Position (N)
                    |/
                   /|               - Point perpendicular to eyes position (2) 
                 /  | (arm)         
               /    1               - Player Eye Position (1)

        * You can't see point 2 from point 1 even though you can see point 3 from point 2

        =====================================================================  
        (PASS)

                3            - Actual Event Position (3)
        -------              - Wall                                        
         1------|2           - Player Eye Position (1) and point perpendicular to eyes position (2)
        (arm)

        * You can see point 2 from point 1 and you can see point 3 from point 2
        =====================================================================  
        */

        // get player position
        vector3 Point1 = vector3(0.0f, 0.0f, 0.0f);
        // not set yet
        vector3 Point2 = vector3(0.0f, 0.0f, 0.0f);
        // get event position
        vector3 Point3 = NewEventPos;

        // load up our points for calculations
        GetThrowPoints( Point1, Point2, Point3 );

        // ---------------------------------------------------------------------------------
        // Check point 1 to point 2 (player 'eye' position to 'shoulder')
        g_CollisionMgr.RaySetup( GetGuid(),     // MovingObjGuid,
                                 Point1,        // WorldStart,
                                 Point2);       // WorldEnd,

        g_CollisionMgr.IgnoreGlass();

        g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, 
                                        object::ATTR_BLOCKS_PLAYER_LOS, 
                                        (object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING)) ;

        // we have collisions, this failed
        if( g_CollisionMgr.m_nCollisions )
        {
            // back off the wall a bit
            vector3 theNormal = g_CollisionMgr.m_Collisions[0].Plane.Normal;
            NewEventPos = g_CollisionMgr.m_Collisions[0].Point + (theNormal * 3.5f);
        }
        else  // clear from eyes to shoulder
        {                        
            // ---------------------------------------------------------------------------------
            // Check point 2 to point 3 ('shoulder' position to event position)
            g_CollisionMgr.RaySetup( GetGuid(),     // MovingObjGuid,
                                     Point2,        // WorldStart,
                                     Point3);       // WorldEnd,                        

            g_CollisionMgr.IgnoreGlass();

            g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, 
                                            object::ATTR_BLOCKS_LARGE_PROJECTILES, 
                                            (object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING)) ;

            // if we have collisions, pick new point
            if( g_CollisionMgr.m_nCollisions )
            {
                // back off the wall a bit
                vector3 theNormal = g_CollisionMgr.m_Collisions[0].Plane.Normal;
                NewEventPos = g_CollisionMgr.m_Collisions[0].Point + (theNormal * 3.5f);
            }
        }                        
    }

#ifdef DEBUG_GRENADE_THROWING
    // for drawing spheres and such
    g_NewEventPos = NewEventPos;
#endif

    return NewEventPos;
}

//=========================================================================

void player::GetThrowPoints( vector3 &Point1, vector3 &Point2, vector3 &Point3 )
{    
    // get the player position and raise the "eyes" up to the event pos.
    Point1 = GetPosition();
    Point1.GetY() = Point3.GetY();

    // get our view vector (normalized)
    vector3 ViewZ = GetSimulationView().GetViewZ();

    // get the vector closest to the line segment from player position to event position
    vector3 Closest = ViewZ.Cross(Point3 - Point1);

    // get distance between player position and event position
    f32 d = Closest.Length();

    // get the lean
    vector3 lean = (GetSimulationView().GetViewX());

    if( m_CurrentGrenadeType2 == INVEN_GRENADE_FRAG )
    {
        d = -d;
    }

    // this is our "shoulder" position
    Point2 = Point1 + (lean * d);  // perpendicular position
}

//=========================================================================

xbool player::IsAnimStateAvailable2( inven_item WeaponItem, animation_state AnimState )
{
    // Get a reference to the state that we are considering
    s32 WeaponIndex = inventory2::ItemToWeaponIndex( WeaponItem );
    state_anims& State = m_Anim[WeaponIndex][AnimState];

    if( (State.nWeaponAnims > 0) && (State.nPlayerAnims > 0) )
        return TRUE;

    return FALSE;
}

//=========================================================================

vector3 player::GetBonePos( s32 BoneIndex )
{
    // for NPCs or 3rd person view of avatar
    if( IsAvatar() )
    {
        return actor::GetBonePos(BoneIndex);
    }

    // First-person player arms
    return m_AnimPlayer.GetBonePosition(BoneIndex); 
}

//=========================================================================

void player::UpdateArmsAnimation( new_weapon* pWeapon, f32 DeltaTime )
{
    const xbool ConstrainFirstPerson = !IsAvatar() && IsAlive() && !IsCinemaRunning();
    if( ConstrainFirstPerson )
    {
        // Rebuild from the physical eye anchor, never from last frame's correction.
        OnMoveViewPosition( GetPosition() );
        m_AnimPlayer.SetRoll( m_SoftLeanAmount * DEG_TO_RAD( GetTweakF32( "LeanMaxDegrees" ) ) );
    }

    if( m_AnimGroup.GetPointer() )
    {
        vector3 Position;
        m_AnimPlayer.Advance( DeltaTime, Position );

        if( ConstrainFirstPerson && (m_iCameraBone >= 0) )
        {
            const vector3 ViewPosition = GetDefaultViewPos();
            const vector3 PreferredPosition = ViewPosition +
                GetFirstPersonCameraPivotCorrection( m_AnimPlayer, m_iCameraBone );
            const vector3 Correction = ConstrainFirstPersonViewPosition( PreferredPosition ) - ViewPosition;

            // Apply the shared rig correction before weapon attachment and events.
            m_AnimPlayer.SetPosition( m_AnimPlayer.GetPosition() + Correction );
        }

        if( pWeapon )
        {
            AttachWeapon();
            // First-person weapon animation is advanced here. NPC/third-person
            // weapon animation is handled by actor::UpdateWeapon.
            if( pWeapon->GetRenderState() == new_weapon::RENDER_STATE_PLAYER )
            {
                pWeapon->OnAdvanceSimulation( DeltaTime );
            }
        }
    }

    OnAnimEvents();

    if( !m_Physics.IsAirborne() && m_bJustLanded )
    {
        const f32 DistanceFell = m_FellFromAltitude - GetPosition().GetY();
        if( DistanceFell > 0.001f )
        {
            static const f32 Gravity = 980.0f;
            const f32 TimeFalling = x_sqrt( DistanceFell / Gravity );
            const f32 FallSpeed = Gravity * TimeFalling * s_ArmsVelocityCarryover;
            m_ArmsVelocity += vector3( 0.0f, -FallSpeed, 0.0f );
        }

        m_bJustLanded = FALSE;
    }
}
