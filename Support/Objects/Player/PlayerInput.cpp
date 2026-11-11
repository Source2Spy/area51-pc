//==============================================================================
//
//
//  PlayerInput.cpp
//
// 
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Player.hpp"
#include "PlayerInput.hpp"
#include "InputMgr/GamePad.hpp"
#include "Objects/ParticleEmiter.hpp"
#include "Objects/Render/PostEffectMgr.hpp"
#include "Objects/SpawnPoint.hpp"
#include "Objects/Event.hpp"
#include "Sound/EventSoundEmitter.hpp"
#include "TemplateMgr/TemplateMgr.hpp"
#include "Characters/Character.hpp"
#include "Characters/Conversation_Packet.hpp"
#include "GameLib/StatsMgr.hpp"
#include "GameLib/RenderContext.hpp"
#include "Dictionary/Global_Dictionary.hpp"
#include "Objects/WeaponSniper.hpp"
#include "Objects/WeaponSMP.hpp"
#include "Objects/Corpse.hpp"
#include "NetworkMgr/Replication/NetObjMgr.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"
#include "Objects/Ladders/Ladder_Field.hpp"
#include "Objects/GrenadeProjectile.hpp"
#include "Objects/GravChargeProjectile.hpp"
#include "Objects/JumpingBeanProjectile.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"
#include "Objects/Door.hpp"
#include "Objects/Projector.hpp"
#include "Objects/WeaponMutation.hpp"
#include "StateMgr/StateMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Objects/HudObject.hpp"
#include "Characters/ActorEffects.hpp"
#include "Configuration/GameConfig.hpp"
#include "Objects/Turret.hpp"
#include "Objects/WeaponShotgun.hpp"
#include "GameLib/DebugCheats.hpp"
#include "Objects/FocusObject.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"
#include "Objects/LoreObject.hpp"
#include "Objects/Camera.hpp"

#ifdef X_EDITOR
#include "../../../Apps/Editor/Project.hpp"
#else
#include "NetworkMgr/Messages/MsgMgr.hpp"
#include "Menu/DebugMenu2.hpp"
#endif

static const f32 s_CrouchUpVelocity = 80.0f;

static const f32 s_MinTimeBetweenMutationChanges = 0.5f;
static const f32 s_JumpBufferDuration             = 0.12f;

//==============================================================================
// FUNCTIONS
//==============================================================================

void player::UpdateUserInput(  f32 DeltaTime )
{
    if( !m_bActivePlayer )
    {
        return;
    }

#ifndef X_EDITOR
    if ( g_StateMgr.IsPaused() )
    {
        ClearMoveLookInput();
        m_Input.Clear();
        g_GameInput.ClearPlayerActions( m_ActivePlayerPad );
        return;
    }
#endif

    if( !m_LockedView.IsActive() &&
        !m_CinemaController.IsActive() &&
             (!m_bDead || !m_bCanDie) )
    {
        if( m_ActivePlayerPad != -1 )
        {
            m_Input.Sample( g_GameInput[m_ActivePlayerPad] );

            //handles button press events
            OnButtonInput( DeltaTime );
            UpdateMoveLookInput();
        }
    }
    else if( m_LockedView.IsActive() || m_CinemaController.IsActive() )
    {
        ClearMoveLookInput();
        m_Input.Clear();
        if( m_ActivePlayerPad != -1 )
        {
            g_GameInput.ClearPlayerActions( m_ActivePlayerPad );
        }
    }
    else if( m_ActivePlayerPad != -1 )
    {
        m_Input.Sample( g_GameInput[m_ActivePlayerPad] );
        ClearMoveLookInput();
    }
}
//==============================================================================

void player::ClearMoveLookInput( void )
{
    m_fMoveValue            = 0.0f;
    m_fStrafeValue          = 0.0f;
    m_JumpBufferTime        = 0.0f;
    m_LookSample            = PlayerLookSample();
    m_MoveInput             = PlayerMoveInput();
    m_Look.Clear();
    m_Movement.Clear();
}

void player::UpdateMoveLookInput(void)
{
    ASSERT( m_ActivePlayerPad != -1 );

    PlayerInputState const& InputState = m_Input.GetState();
    BuildLookInputSample( InputState.Look, m_LookSample );

#if defined(X_EDITOR)
    // Mirror weapon?
    extern xbool g_MirrorWeapon;
    if( g_MirrorWeapon )
    {
        // Turn on mirror for hands
        m_AnimPlayer.SetMirrorBone( 0 );

        // Turn on mirror for weapon
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( pWeapon )
            pWeapon->GetCurrentAnimPlayer().SetMirrorBone( 0 );
    }
    else
    {
        // Turn off mirror for hands
        m_AnimPlayer.SetMirrorBone( -1 );

        // Turn off mirror for weapon
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( pWeapon )
            pWeapon->GetCurrentAnimPlayer().SetMirrorBone( -1 );
    }
#endif

    if ( !m_bInTurret )
    {
        m_Movement.BuildInput( InputState.Move, m_MoveInput );
        m_fMoveValue   = x_clamp( m_MoveInput.KeyboardForward + m_MoveInput.GamepadForward,
                                  -1.0f,
                                   1.0f );
        m_fStrafeValue = x_clamp( m_MoveInput.KeyboardStrafe + m_MoveInput.GamepadStrafe,
                                  -1.0f,
                                   1.0f );
    }
    else
    {
        m_MoveInput = PlayerMoveInput();
        m_Movement.Clear();
        m_fMoveValue   = 0.0f;
        m_fStrafeValue = 0.0f;
    }

}

//===========================================================================

void player::OnButtonInput( f32 DeltaTime )
{
    PlayerInputState const& Input = m_Input.GetState();

    // don't allow player to switch weapons, zoom in, attack, etc.
    if( m_bHidePlayerArms )
    {
        m_JumpBufferTime = 0.0f;
        return;
    }
    // This is to make sure you don't lean or fire on the same button press as 
    // voting or respawning, respectively.
    {   
        xbool PrimaryDown = Input.IsHeld( PlayerAction::PrimaryFire );
        if( !PrimaryDown )
        {
            m_bRespawnButtonPressed = FALSE;
        }

        if( !Input.IsHeld( PlayerAction::VoteYes ) &&
            !Input.IsHeld( PlayerAction::VoteNo  ) )
        {
            m_bVoteButtonPressed = FALSE;
        }
    }

    //
    // Handle voting menu input
    //
    if( m_VoteCanCast )
    {
        if( !m_VoteMode )
        {
            // Activate vote mode / menu.
            if( Input.WasPressed( PlayerAction::VoteMenuOn ) )
            {
                m_VoteMode = TRUE;
                // Activate the vote menu.
                LOG_MESSAGE( "player::OnButtonInput", "Vote menu activated." );
                return;
            }
        }
        else
        {
            // Deactivate vote mode / menu.
            if( Input.WasPressed( PlayerAction::VoteMenuOff ) )
            {
                m_VoteMode      = FALSE;
                // Deactivate the vote menu.
                LOG_MESSAGE( "player::OnButtonInput", "Vote menu deactivated." );
                return;
            }

            // Vote YES.
            if( Input.WasPressed( PlayerAction::VoteYes ) )
            {
                m_VoteMode      = FALSE;
                m_VoteCanCast   = FALSE;
                m_bVoteButtonPressed = TRUE;
                // Deactivate the vote menu.
                VoteCast( +1 );
                LOG_MESSAGE( "player::OnButtonInput", "Vote YES." );
                return;
            }

            // Vote NO.
            if( Input.WasPressed( PlayerAction::VoteNo ) )
            {
                m_VoteMode      = FALSE;
                m_VoteCanCast   = FALSE;
                m_bVoteButtonPressed = TRUE;
                // Deactivate the vote menu.
                VoteCast( -1 );
                LOG_MESSAGE( "player::OnButtonInput", "Vote NO." );
                return;
            }

            // Vote ABSTAIN.
            if( Input.WasPressed( PlayerAction::VoteAbstain ) )
            {
                m_VoteMode      = FALSE;
                m_VoteCanCast   = FALSE;
                // Deactivate the vote menu.
                VoteCast( 0 );
                LOG_MESSAGE( "player::OnButtonInput", "Vote ABSTAIN." );
                return;
            }
        }
    }
    else 
    {
        // So that the vote key doesn't show up if the vote expires while it's showing.
        m_VoteMode = FALSE;

        if( Input.IsHeld( PlayerAction::DropFlag ) )
        {
            #ifndef X_EDITOR
            m_NetDirtyBits |= DROP_ITEM_BIT;
            if( g_NetworkMgr.IsServer() )
                pGameLogic->DropFlag( m_NetSlot );
            #endif
        }
    }

    //
    // Do nothing if stunned.
    //
    if ( m_NonExclusiveStateBitFlag & NE_STATE_STUNNED )
    {
        return;
    }

    //update base class button input
    ASSERT( m_ActivePlayerPad != -1 );

    xbool bStopCrouching = FALSE;


#ifndef X_EDITOR
    if( g_NetworkMgr.IsOnline() )
    {
        if( Input.WasPressed( PlayerAction::TalkModeToggle ) )
        {
            g_VoiceMgr.ToggleTalkMode();
        }

        // Voice chat
        {
            g_VoiceMgr.SetTalking( FALSE );

            if( g_VoiceMgr.IsHeadsetPresent() == TRUE )
            {
                // check for voice chat
                if( Input.IsHeld( PlayerAction::Chat ) )
                {
                    // Test to make sure the player is allowed to chat
                    if( (g_VoiceMgr.IsVoiceBanned()  == FALSE) &&
                        (g_VoiceMgr.IsVoiceEnabled() ==  TRUE) )
                         g_VoiceMgr.SetTalking( TRUE );
                }
            }
        }
    }

    player_profile& p = g_StateMgr.GetActiveProfile(g_StateMgr.GetProfileListIndex(m_LocalSlot));
    if( p.GetCrouchOn() )
    {
        xbool CrouchKeyPressed = Input.WasPressed( PlayerAction::Crouch );
        if( CrouchKeyPressed )
        {
            // crouch is a toggle and we're crouching so turn it off
            if( IsCrouching() )
            {
                bStopCrouching = TRUE;
            }
            else if ( !m_bInTurret )
            {
                // move the arms a little
                m_ArmsVelocity += vector3( 0.0f, -s_CrouchUpVelocity, 0.0f );

                // Start crouching
                SetIsCrouching( TRUE );
            }
        }
    }
    else
#endif
    {
        xbool CrouchKeyIsPressed = Input.IsHeld( PlayerAction::Crouch );
        if( CrouchKeyIsPressed )
        {
            // only do this if we weren't crouching previously
            if( !IsCrouching() && !m_bInTurret )
            {
                // move the arms a little
                m_ArmsVelocity += vector3( 0.0f, -s_CrouchUpVelocity, 0.0f );

                // Start crouching
                SetIsCrouching( TRUE );
            }
        }
        else
        {
            // Only do this if we are already crouching
            if( IsCrouching() )
            {
                bStopCrouching = TRUE;
            }
        }
    }

    // for whatever reason, we need to quit crouching
    if( bStopCrouching )
    {
        // Stop crouching
        SetIsCrouching( FALSE );

        // move the arms a little
        m_ArmsVelocity += vector3( 0.0f, s_CrouchUpVelocity, 0.0f );
    }

    // Jump input is retained briefly so a press just before landing is not lost.
    if( m_bInTurret || !m_bCanJump )
    {
        m_JumpBufferTime = 0.0f;
    }
    else
    {
        m_JumpBufferTime = MAX( 0.0f, m_JumpBufferTime - DeltaTime );

        if( Input.WasPressed( PlayerAction::Jump ) )
            m_JumpBufferTime = s_JumpBufferDuration;

        if( (m_JumpBufferTime > 0.0f) && (m_bOnLadder || m_Physics.IsGrounded()) )
        {
            if( Jump() )
            {
                m_JumpBufferTime = 0.0f;

                // Stop crouching when the jump actually starts.
                SetIsCrouching( FALSE );
            }
        }
    }

    // Look for 'game speak' buttons.
    OnGameSpeak() ;

    //
    // Toggle mutation
    //
    xbool Multiplayer = FALSE;
#ifndef X_EDITOR
    Multiplayer = GameMgr.IsGameMultiplayer();
#endif

    //
    // mreed:
    // This is a little strange, but we need more pressure to toggle mutation when we're 
    // leaning. This means it's less likely to succeed with a "WasValue" since you only get
    // one shot at it. So, when leaning, we will use "IsValue" so that we have more than
    // one chance to get the pressure needed. The side-effect of this is that we are no longer
    // debounced for mutation when leaning.
    // Luckily (?), there is a mutation frequency timer that prevents rapid mutation/demutation
    // so we'll always get through the mutation cycle before we come back. Plus, this only matters
    // when intentionally trying to mutate while leaning.
    //
    // So, this is where we get the button press values
    //
    PlayerActionState const& MutationInput   = Input.GetAction( PlayerAction::Mutation );
    
    const xbool IsLeaning = x_abs( m_SoftLeanAmount ) > 0.1f;
    const f32 ActionMutation   = IsLeaning ? MutationInput.Value   : MutationInput.PressedValue;

    xbool MutationPressed = ActionMutation > 0.25f;

    static const f32 MinTimeSinceUseToThrowGrenade = 1.0f;
    static const f32 MinTimeSinceUseToMutate = 1.0f;

    if (   !m_bInTurret 
        &&  m_bCanToggleMutation 
        && !IsChangingMutation()
        &&  MutationPressed
        && (m_MutationChangeTime > s_MinTimeBetweenMutationChanges)
        && (m_UseTime > MinTimeSinceUseToMutate) )
    {

        //
        // If we're leaning, we need to press harder to toggle mutation
        //
        xbool HaveButtonPressureToToggleMutation = TRUE;

        f32 MutationValue = 1.0f;

        if ( IsLeaning )
        {
            MutationValue = MutationInput.Value;
        }

        ASSERTS( MutationValue > 0.0f, "Mutation input is zero when toggling mutation; check the input mapping" );

        static const f32 MinValueToToggleMutationWhileLeaning = 0.3f;
        HaveButtonPressureToToggleMutation = MutationValue > MinValueToToggleMutationWhileLeaning;

        if ( HaveButtonPressureToToggleMutation )
        {
            // OK, our input is in order, see what we need to do
            if( m_Inventory2.HasItem( INVEN_WEAPON_MUTATION ) && !IsMutated() )
            {
                SetupMutationChange(TRUE);
            }
            else if( IsMutated() )
            {
                SetupMutationChange(FALSE);
            }        
        }
    }

    //
    // So, switch to desired weapon ( i we have it, ofk )
    //
    PlayerInventoryInput const InventoryInput =
        m_InventoryInteraction.BuildInput( Input, !m_bInTurret );

    switch( InventoryInput.WeaponRequest )
    {
    case PlayerWeaponRequest::CycleRight:  OnWeaponSwitch2( CYCLE_RIGHT );                break;
    case PlayerWeaponRequest::CycleLeft:   OnWeaponSwitch2( CYCLE_LEFT );                 break;
    case PlayerWeaponRequest::Scanner:     OnWeaponSwitch2( INVEN_WEAPON_SCANNER );       break;
    case PlayerWeaponRequest::Pistol:      OnWeaponSwitch2( INVEN_WEAPON_DESERT_EAGLE );  break;
    case PlayerWeaponRequest::Smp:         OnWeaponSwitch2( INVEN_WEAPON_SMP );           break;
    case PlayerWeaponRequest::Shotgun:     OnWeaponSwitch2( INVEN_WEAPON_SHOTGUN );       break;
    case PlayerWeaponRequest::SniperRifle: OnWeaponSwitch2( INVEN_WEAPON_SNIPER_RIFLE );  break;
    case PlayerWeaponRequest::Bbg:         OnWeaponSwitch2( INVEN_WEAPON_BBG );           break;
    case PlayerWeaponRequest::MesonCannon: OnWeaponSwitch2( INVEN_WEAPON_MESON_CANNON );  break;
    case PlayerWeaponRequest::None:                                                        break;
    }

    //
    // NOTE: TWEEK THIS, WE MIGHT WANT TO MAKE THE RAMP DOWN FIRST BEFORE DOING MELEE.
    //
    xbool MeleePressed = Input.IsHeld( PlayerAction::MeleeAttack );
    if( !m_bInTurret && MeleePressed )
    {
        animation_state MeleeState = ANIM_STATE_MELEE;

        // make sure we are completely mutated
        if( IsMutated() )
        {
            switch( m_CurrentAnimState )
            {
                // we don't want melee kicking off when we are switching to while we are mutated.
                // This will cause the meshes to get hosed
            case ANIM_STATE_SWITCH_TO:
            case ANIM_STATE_SWITCH_FROM:
                {}
                break;

                //////////////////////////////////////////////////////////////////////////
                // put any other mutation special cases here
                //////////////////////////////////////////////////////////////////////////

            default:
                {                    

                    // if we are mutated, do extreme melee attack.                
                    if( m_bMutationMeleeEnabled )
                    {
                        // if we are already attacking, return.
                        // NOTE: we can't expect SetAnimState to check if the anims are the same because we have 5 different ones here.
                        if( !m_bMeleeLunging )
                        {
                            MeleeState = SetupMutationMeleeWeapon();
                            SetMeleeState(MeleeState);
                        }
                    }                    
                }
                break;
            }
        }
        else // we aren't mutated, do normal melee stuff
        {   
            if( m_ComboCount >= MAX_COMBO_HITS )
            {
                m_ComboCount = MAX_COMBO_HITS-1;
                ASSERT(0);
                return;
            }  

            if( m_ComboCount == 0 )
            {
                // if you don't do this, when you demutate you will swing your wittle human arms and it looks dumb :)
                if( m_CurrentAnimState != ANIM_STATE_SWITCH_FROM && m_CurrentAnimState != ANIM_STATE_DISCARD )
                {
                    if( m_bCanRequestCombo && m_bLastMeleeHit )
                    {
                        // Still stage 0 and we're requesting to start a combo
                        SetMeleeState(ANIM_STATE_COMBO_BEGIN);
                    }
                    else
                    {
                        // we aren't requesting a combo yet, do initial melee
                        SetMeleeState(MeleeState);
                    }
                }
            }            

            // if you can request a combo, set the flag
            if( m_bCanRequestCombo )
            {
                m_bHitCombo = TRUE;
            }
        }
    }

    // don't throw a grenade if we're just exiting fly mode
#if defined( ENABLE_DEBUG_MENU )
    xbool GrenadePressed = Input.IsHeld( PlayerAction::ThrowGrenade ) &&
                           !Input.IsHeld( PlayerAction::TalkModeToggle );
#else
    xbool GrenadePressed = Input.IsHeld( PlayerAction::ThrowGrenade );
#endif


    if( GrenadePressed )
    {
        if (   (m_Inventory2.GetAmount( m_CurrentGrenadeType2 ) > 0)
            && ( !IsMutated() )
            && AllowedToFire() 
            && !m_bInTurret
            && (m_UseTime > MinTimeSinceUseToThrowGrenade) )
        {
            // Get a reference to the state that we are considering
            s32 GrenadeState = (m_CurrentGrenadeType2==INVEN_GRENADE_FRAG) ? ANIM_STATE_GRENADE : ANIM_STATE_ALT_GRENADE;

            // if we are already throwing a grenade of any type, don't any other grenade be thrown
            if( m_CurrentAnimState != ANIM_STATE_GRENADE && m_CurrentAnimState != ANIM_STATE_ALT_GRENADE )
            {
                state_anims& State = m_Anim[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)][GrenadeState];

                // Can we fire the secondary weapon?
                if( State.nPlayerAnims > 0 )
                {
                    new_weapon* pWeapon = GetCurrentWeaponPtr();
                    if( pWeapon )
                        pWeapon->ClearZoom();

                    SetAnimState( (animation_state)GrenadeState );
                }
            }
        }
    }

    // flashlight button
    xbool FlashlightPressed = Input.WasPressed( PlayerAction::Flashlight );

    if ( FlashlightPressed && !IsMutated() )
    {
        new_weapon* pWeapon = GetCurrentWeaponPtr();

        if( pWeapon && pWeapon->HasFlashlight() )
        {
            SetFlashlightActive( !IsFlashlightActive() );
        }
        else
        {
            // weapon is invalid?  Turn off flashlight then
            SetFlashlightActive( FALSE );
        }
    }

    // Use on a mutagen reservoir
    xbool UseKeyPressed = InventoryInput.UseHeld;
    
    if ( UseKeyPressed && NearMutagenReservoir() )
    {
        static const f32 ChangeRate = 10.0f;
        AddMutagen( ChangeRate * DeltaTime );

        // play the sucking sound when refilling mutagen from a super-contagious dead body.
        if( m_SuckingMutagenLoopID == 0 )
        {
            m_SuckingMutagenLoopID = g_AudioMgr.Play( "SCDB_Suck_Mutagen_Loop", GetPosition(), GetZone1(), TRUE );
        }
    }
    else
    {
        if( m_SuckingMutagenLoopID != 0 )
        {
            // not refilling anymore, release ID
            g_AudioMgr.Release( m_SuckingMutagenLoopID, 1.0f );
            m_SuckingMutagenLoopID = 0;
        }
    }

    xbool LeanLeftPressed  = Input.IsHeld( PlayerAction::LeanLeft  ) && !m_bVoteButtonPressed;
    xbool LeanRightPressed = Input.IsHeld( PlayerAction::LeanRight ) && !m_bVoteButtonPressed;

    if ( !m_bInTurret && LeanLeftPressed )
    {
        UpdateLean( 1.0f, DeltaTime );
    }
    else if ( !m_bInTurret && LeanRightPressed )
    {
        UpdateLean( -1.0f, DeltaTime );
    }
    else
    {
        UpdateLean( 0.0f, DeltaTime );
    }
}
