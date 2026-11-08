//=========================================================================

//
//  PlayerMutation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "Objects/Pickup.hpp"
#include "Objects/WeaponMutation.hpp"
#include "PerceptionMgr/PerceptionMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "GameLib/DebugCheats.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

// Mutation drain rates while the player is in mutant form.
static tweak_handle MutagenChangeMutant_AtWill_Tweak  ( "Mutagen_Change_Mutant_At_Will" );
static tweak_handle MutagenChangeMutant_Forced_Tweak  ( "Mutagen_Change_Mutant_Forced" );
static tweak_handle MutagenChangeMutant_Campaign_Tweak( "Mutagen_Change_Mutant_Campaign" );

// Mutation recharge rates while the player is in human form.
static tweak_handle MutagenChangeHuman_AtWill_Tweak  ( "Mutagen_Change_Human_At_Will" );
static tweak_handle MutagenChangeHuman_Forced_Tweak  ( "Mutagen_Change_Human_Forced" );
static tweak_handle MutagenChangeHuman_Campaign_Tweak( "Mutagen_Change_Human_Campaign" );

f32 MPMutagenBurn = 1.0f;

player::strain_control_modifiers::strain_control_modifiers() :
    m_StrainProximityAlertRadius( 300.0f ),
    m_StrainMaxFowardVelocity( 600.0f ),
    m_StrainJumpVelocity( 500.0f ),
    m_StrainMaxHealth( 100.0f ),
    m_StrainMinWalkSpeed( 200.0f ),
    m_StrainMinRunSpeed( 400.0f ),
    m_StrainDecelerationFactor( 2.5f ),
    m_StrainCrouchChangeRate( 10.0f ),
    m_StrainReticleMovementDegrade( 0.5f ),
    m_fStrainForwardAccel( 3000.0f ),
    m_fStrainYawSensitivity( 5.5f ),
    m_fStrainPitchSensitivity( 1.5f )
{
    m_StrainEyesOffset.Set( 0.0f, -25.0f, -20.0f );
}

//=========================================================================

void player::SetCurrentStrain( void )
{
    if( m_bStrainInitialized == FALSE )
    {
        // HACK
#ifndef X_EDITOR
        if( !GameMgr.IsGameMultiplayer() )
#endif
        {
            m_Faction = FACTION_PLAYER_NORMAL; 
        }

        // Set all movement / control variables to match this strain.
        strain_control_modifiers& StrainControl = m_StrainControls;

        // Move all of the data from the strain control structure to the places where it will get used.
        m_fCrouchChangeRate      = StrainControl.m_StrainCrouchChangeRate;
        m_ProximityAlertRadius   = StrainControl.m_StrainProximityAlertRadius;
        m_MaxFowardVelocity      = StrainControl.m_StrainMaxFowardVelocity;
        m_JumpVelocity           = StrainControl.m_StrainJumpVelocity;
        m_MaxHealth              = StrainControl.m_StrainMaxHealth;
        m_EyesOffset             = StrainControl.m_StrainEyesOffset;
        m_Look.SetBaseStickSensitivity( StrainControl.m_fStrainYawSensitivity,
                                        StrainControl.m_fStrainPitchSensitivity );
        m_fMinWalkSpeed          = StrainControl.m_StrainMinWalkSpeed;
        m_fMinRunSpeed           = StrainControl.m_StrainMinRunSpeed;
        m_fDecelerationFactor    = StrainControl.m_StrainDecelerationFactor;
        m_ReticleMovementDegrade = StrainControl.m_StrainReticleMovementDegrade;
        m_fForwardAccel          = StrainControl.m_fStrainForwardAccel;

        m_FriendFlags            = m_StrainFriendFlags;

        // Finish necessary initializations.
        m_fMinWalkSpeed *= m_fMinWalkSpeed;
        m_fMinRunSpeed *= m_fMinRunSpeed;

        //
        // NOTE: Make sure that the CurrentStrain is set before we go set the weapons up  for this strain!!!
        //
        for( s32 i = 0; i < INVEN_NUM_WEAPONS; i++ )
        {
            new_weapon* pWeapon = GetWeaponPtr( inventory2::WeaponIndexToItem(i) );
            if( pWeapon )
            {
                pWeapon->SetupRenderInformation( );
            }
        }
        // Restart the current state so the new animations start.  This will 
        // most likely change when we get some anims.
        if ( m_bStrainInitialized )
        {
            ShakeView( 1.0f );
        }
    }    
}

xbool player::UseAntiMu( collectable_anti_mutagen* pAntiMu )
{
    ( void ) pAntiMu;

    return FALSE;
}

xbool player::SetMutated( xbool bMutate )
{
    if( actor::SetMutated( bMutate ) )
    {
        if( bMutate )
        {
            if( (GetMutagen() <= F32_MIN) && !m_bDead )
            {
                m_bIsMutated = FALSE;

                // this failed
                return FALSE;
            }
        }
        
        m_bIsMutated = bMutate;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void player::SetupMutationChange( xbool bMutate )
{
    //SB: This breaks forced mutations (eg. in mp infection game)
    //if( (m_CurrentAnimState == ANIM_STATE_GRENADE) ||
        //(m_CurrentAnimState == ANIM_STATE_ALT_GRENADE) ||
        //(m_CurrentAnimState == ANIM_STATE_DISCARD) )
        //return;

    xbool bIsMutated = m_bIsMutated;
    if( bIsMutated == bMutate )
        return;

//  LOG_MESSAGE( "player::SetupMutationChange", "Mutate:%d", bMutate );

    if( bMutate )
    {
        // can we mutate?
        if( SetMutated(TRUE) )
        {
            m_PreMutationWeapon2 = m_CurrentWeaponItem;
            SetNextWeapon2( INVEN_WEAPON_MUTATION );
        //  LOG_MESSAGE( "player::SetupMutationChange", "SetNextWeapon2( %s )", inventory2::ItemToName( INVEN_WEAPON_MUTATION ) );
            m_MutationAudioLoopSfx = g_AudioMgr.Play( "Mutation_Vision_Loop" );
            g_PerceptionMgr.BeginMutate();
        }

        // turn off flashlight when mutating
        SetFlashlightActive( FALSE );
    }
    else
    {
        g_PerceptionMgr.EndMutate();
        static const f32 FADE_TIME = 0.25f;
        g_AudioMgr.Release( m_MutationAudioLoopSfx, FADE_TIME );
        // see if our previous weapon is in inventory
        if( m_Inventory2.HasItem( m_PreMutationWeapon2 ) && 
            (m_PreMutationWeapon2 != INVEN_WEAPON_MUTATION) )
        {
            SetMutated(FALSE);
            SetNextWeapon2( m_PreMutationWeapon2 );
        //  LOG_MESSAGE( "player::SetupMutationChange", "SetNextWeapon2( %s )", inventory2::ItemToName( m_PreMutationWeapon2 ) );
        }
        else
        {
            // see if we have another weapon in inventory
            s32 i;
            for( i = 0; i < INVEN_NUM_WEAPONS; ++i )
            {
                if ( inventory2::WeaponIndexToItem(i) == INVEN_WEAPON_MUTATION )
                {
                    continue;
                }
                else if ( m_Inventory2.HasItem( inventory2::WeaponIndexToItem(i) ) )
                {
                    SetMutated(FALSE);
                    SetNextWeapon2( inventory2::WeaponIndexToItem(i) );
                }
            }
        }
    }
}

void player::UpdateMutagen( f32 DeltaTime )
{
    // if we are playing a cinematic, don't burn mutagen
    if( IsCinemaRunning() )
    {
        return;
    }

    f32 AmountToChange = 0.0f;  // If this is negative, for instance, when in mutant form in campaign mode, then it acts as a burn.
    f32 pct = 1.0f;

    switch( GetMutagenBurnMode() )
    {
    case MBM_AT_WILL:
        {
            pct = IsMutated() ? MutagenChangeMutant_AtWill_Tweak.GetF32() : MutagenChangeHuman_AtWill_Tweak.GetF32();
            pct = pct/100.0f; // make it a percentage, tweaks are whole numbers.
            AmountToChange = (pct * GetMaxMutagen() * DeltaTime);
            AmountToChange *= MPMutagenBurn;
        }
        break;

    case MBM_FORCED:
        {
            pct = IsMutated() ? MutagenChangeMutant_Forced_Tweak.GetF32() : MutagenChangeHuman_Forced_Tweak.GetF32();
            pct = pct/100.0f; // make it a percentage, tweaks are whole numbers.
            AmountToChange = (pct * GetMaxMutagen() * DeltaTime);
        }
        break;

    default:
    case MBM_NORMAL_CAMPAIGN:
        {
            pct = IsMutated() ? MutagenChangeMutant_Campaign_Tweak.GetF32() : MutagenChangeHuman_Campaign_Tweak.GetF32();
            pct = pct/100.0f; // make it a percentage, tweaks are whole numbers.
            AmountToChange = (pct * GetMaxMutagen() * DeltaTime);
        }
        break;
    }

    // this is removing mutagen
    if( AmountToChange < 0.0f )
    {
        // see if the change takes us below 0 (remember AmountToChange is negative here so we have to add)
        if( (GetMutagen() + AmountToChange) < 0.0f )
        {
            // only burn as much as we have, otherwise the sanity check in AddMutagen won't let this happen
            AmountToChange = -GetMutagen();
        }
    }

    // Don't burn mutagen if we have unlimited ammo
    if( DEBUG_INFINITE_AMMO )
    {
        // fill it up if we have infinite ammo
        AmountToChange = GetMaxMutagen();
    }

    // this will only remove mutagen if AmountToChange is negative.
    AddMutagen(AmountToChange);

    if( IsMutated() )
    {
        // We don't burn mutagen if we have unlimited ammo, so, we should never run out.
        if( !DEBUG_INFINITE_AMMO )
        {
            // check if we need to de-mutate
            if( GetMutagen() < F32_MIN )
            {
                if ( m_bInMutationTutorial )
                {
                    pain_handle PainHandle("GENERIC_LETHAL");
                    pain Pain;
                    Pain.Setup( "GENERIC_LETHAL", 0, GetPosition() );
                    Pain.SetDirectHitGuid( GetGuid() );
                    Pain.ApplyToObject( this );
                }
                else
                {
                    SetupMutationChange(FALSE);
                }
            }
        }
    }    
}

xbool player::AddMutagen( const f32& nDeltaMutagen )
{        
    if( m_bDead ) 
    {
        return FALSE;
    }
    // do not allow Mutagen to go above max.
    else if( m_Mutagen == m_MaxMutagen && nDeltaMutagen > 0.0f )
    {
        return FALSE;
    }
    else if( (m_Mutagen + nDeltaMutagen) < 0.0f )  // does what we are using take us below 0?
    {
        // don't have enough
        return FALSE;
    }
    else if ( m_bInMutationTutorial && ((m_Mutagen + nDeltaMutagen) < 1.0f) )
    {
        return FALSE;
    }
    else if( IsCinemaRunning() )
    {
        return FALSE;
    }
    else
    {
        // add/subtract mutagen
        m_Mutagen = fMin( m_Mutagen + nDeltaMutagen , m_MaxMutagen );
        m_Mutagen = fMax( m_Mutagen , 0.0f );

        return TRUE;
    }

    return FALSE;
}

void player::ForceMutationChange( xbool bMutate )
{
//  LOG_MESSAGE( "player::ForceMutationChange", "%s", bMutate ? "TRUE" : "FALSE" );
    
    SetupMutationChange( bMutate );

    if( m_bIsMutated && (m_CurrentWeaponItem != INVEN_WEAPON_MUTATION) )
    {
        ForceNextWeapon();
    }
    m_bIsMutantVisionOn = m_bIsMutated;
}

void player::ContagionDOT( void )
{

#ifndef X_EDITOR

    // Now, if we are in a multiplayer game, and I am an actual physical player,
    // I should go out and smack anyone standing too close to my very ill self 
    // with whom I have a clear LOS.  (The LOS testing is done in actor.)

    ASSERT( GameMgr.IsGameMultiplayer() );
    ASSERT( m_bContagious );
    ASSERT( m_pMPContagion );

    /*
    LOG_MESSAGE( "player::ContagionDOT", 
                 "Attacking:%08X", 
                 m_pMPContagion->PlayerMask );
    */

    actor* pActor = NULL;
    guid   Source = NULL_GUID;

    if( m_pMPContagion->Origin != -1 )
    {
        pActor = (actor*)NetObjMgr.GetObjFromSlot( m_pMPContagion->Origin );
        if( pActor )
            Source = pActor->GetGuid();
        else
            m_pMPContagion->Origin = -1;
    }

    for( s32 i = 0; i < 32; i++ )
    {
        if( !(m_pMPContagion->PlayerMask & (1<<i)) )
            continue;

        pActor = (actor*)NetObjMgr.GetObjFromSlot( i );
        if( !pActor )
            continue;

        pain Pain;
        Pain.Setup( "CONTAGION_TICK_TO_OTHERS", 
                    Source, GetPosition() );
        Pain.SetDirectHitGuid( pActor->GetGuid() );
        Pain.ApplyToObject( pActor->GetGuid() );
    }

    // Don't forget to take a little pain for your self!
    {   
        pain Pain;
        Pain.Setup( "CONTAGION_TICK", Source, GetPosition() );
        Pain.SetDirectHitGuid( GetGuid() );
        Pain.ApplyToObject( GetGuid() );
    }

#endif
}
