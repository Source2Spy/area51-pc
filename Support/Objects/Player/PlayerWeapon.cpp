//=========================================================================

//
//  PlayerWeapon.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "FX/fx_Mgr.hpp"
#include "Objects/WeaponBBG.hpp"
#include "Objects/WeaponMutation.hpp"
#include "Objects/WeaponShotgun.hpp"
#include "Objects/WeaponSniper.hpp"
#include "Objects/WeaponSMP.hpp"
#include "Objects/Corpse.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "NetworkMgr/Replication/NetObjMgr.hpp"
#include "StateMgr/StateMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "TemplateMgr/TemplateMgr.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

extern s32         g_Difficulty;
extern const char* DifficultyText[];

static const xbool s_LogWeaponCreation = FALSE;

s32        GetStartWeaponsForLevel  ( const char* pLevelName );
inven_item GetEquipedWeaponForLevel ( const char* pLevelName );

mtwt s_MapToWeaponTable[] =
{
    {   "dreamlnd",    1*1024, INVEN_NULL              , WB_DE | WB_SCN                                                                    , 0                                                                                                 , TRUE  },
    {   "undrgrnd",    1*1024, INVEN_NULL              , WB_DE | WB_SCN                                                                    , 0                                                                                                 , TRUE  },
    {   "hotzone",     1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_SCN                                                           , WB_DE | WB_SMP | WB_SCN                                                                           , TRUE  },
    {   "search",      1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_SCN                                           , WB_DE | WB_SMP | WB_FG | WB_SCN                                                                   , FALSE },
    {   "getbig",      1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_SCN                                           , WB_DE | WB_SMP | WB_FG | WB_SCN                                                                   , FALSE },
    {   "laststnd",    1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_SCN                                           , WB_DE | WB_SMP | WB_FG | WB_SG | WB_SCN                                                           , FALSE },
    {   "oneofthm",    1*1024, INVEN_WEAPON_SMP        , WB_MM | WB_SMP | WB_DE | WB_SCN                                                   , WB_MM | WB_SCN                                                                                    , FALSE },
    {   "mutation",    1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_MM | WB_SG | WB_MP | WB_SCN                                   , WB_DE | WB_SMP | WB_MM | WB_SCN                                                                   , FALSE },
    {   "sidetrck",    1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_SG | WB_MM | WB_MP | WB_SCN                                   , WB_DE | WB_SMP | WB_SG | WB_MM | WB_MP | WB_SCN                                                   , FALSE },
    {   "dr_cray",     1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_SCN                   , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_SCN                           , FALSE },
    {   "illumin",     1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_SCN                   , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_SCN                   | WB_MS , FALSE },
    {   "flynobjs",    1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_SCN                   , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_SCN                   | WB_MS , FALSE },
    {   "liespast",    1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_SCN | WB_JBG , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_SCN | WB_JBG          | WB_MS , FALSE },
    {   "caves",       1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_SCN          , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_BBG | WB_SCN          | WB_MS , FALSE },
    {   "boarding",    1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_SCN          , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_BBG | WB_SCN          | WB_MS , FALSE },
    {   "ascend1",     1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_MC | WB_SCN  , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_BBG | WB_SCN          | WB_MS , FALSE },
    {   "ascend2",     1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_MC | WB_SCN  , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_BBG | WB_MC | WB_SCN  | WB_MS , FALSE },
    {   "ascend3",     1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_MC | WB_SCN  , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_BBG | WB_MC | WB_SCN  | WB_MS , FALSE },
    {   "exit",        1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_MC | WB_SCN  , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_BBG | WB_MC | WB_SCN  | WB_MS , FALSE },
    {     NULL,        1*1024, INVEN_WEAPON_SMP        , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_MS | WB_BBG | WB_MC | WB_SCN  , WB_DE | WB_SMP | WB_FG | WB_SG | WB_MM | WB_SR | WB_MP | WB_BBG | WB_MC | WB_SCN  | WB_MS , FALSE },
};

//=========================================================================
void player::DebugEnableWeapons( const char* pLevelName )
{
    DebugSetupInventory( pLevelName );

    inven_item  StartItem   = GetEquipedWeaponForLevel( pLevelName );
    s32         Weapons     = GetStartWeaponsForLevel ( pLevelName );

    // Setup start item
    m_PreMutationWeapon2 = INVEN_NULL;
    m_CurrentWeaponItem = StartItem;
    if( m_CurrentWeaponItem == INVEN_WEAPON_MUTATION )
    {
        // Force the player to mutate
        m_PreMutationWeapon2 = INVEN_WEAPON_SMP;
        //        m_CurrentWeaponItem = INVEN_WEAPON_SMP;
        SetNextWeapon2( INVEN_WEAPON_MUTATION, TRUE );

        //        SetMutated( TRUE );
        //        m_bIsMutantVisionOn = TRUE;
    }

    // Enable mutation abilities
    if( Weapons & WB_MM )
    {
        m_bMutationMeleeEnabled         = TRUE;
    }

    if( Weapons & WB_MP )
    {
        m_bPrimaryMutationFireEnabled   = TRUE;
    }

    if ( Weapons & WB_MS )
    {
        m_bSecondaryMutationFireEnabled = TRUE;
    }

    // Load tweaks
    LoadAimAssistTweakHandles();
}

//=========================================================================
xbool player::LoadWarnsLowAmmo( void )
{
    // Determine if this level is supposed to warn the player that they are low on clip ammo with a message
    xbool bShouldWarn = FALSE;

    for( s32 i=0; s_MapToWeaponTable[i].pLevelName ; i++ )
    {
        if( x_stristr( g_ActiveConfig.GetLevelPath(), s_MapToWeaponTable[i].pLevelName ) )
        {
            bShouldWarn   = s_MapToWeaponTable[i].bLoadWarnsLowAmmo;
            break;
        }
    }

    return bShouldWarn;
}

//=========================================================================
xbool player::ReloadWeapon( const new_weapon::ammo_priority& Priority, xbool bCheckAmmo )
{
    s32 ammoCount = 0;
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( !pWeapon )
        return FALSE;

    // do NOT reload the mutation weapon
    if( m_CurrentWeaponItem == INVEN_WEAPON_MUTATION )
    {
        return FALSE;
    }

    // if we don't want to check the ammo (i.e. player pushed "reload button") then ignore count
    if( bCheckAmmo )
    {   
        if( Priority == new_weapon::AMMO_PRIMARY )
        {
            ammoCount = pWeapon->GetAmmoCount( pWeapon->GetPrimaryAmmoPriority() ); 
        }
        else
        {
            ammoCount = pWeapon->GetAmmoCount( pWeapon->GetSecondaryAmmoPriority() ); 
        }
    }

    // If our clip is out of ammo, reload
    // KSS -- always use AMMO_PRIMARY when checking reload?
    // if we don't want to check the ammo (i.e. player pushed "reload button") then ignore count
    if ( ( (ammoCount <= 0) || (!bCheckAmmo) ) && pWeapon->CanReload( new_weapon::AMMO_PRIMARY ) )
    {
        // CJ: Reset the zoom state in case of dry fire on sniper rile, etc.
        pWeapon->ClearZoom();

        SetAnimState( ANIM_STATE_RELOAD );

        // succeeded
        return TRUE;
    }

    // didn't reload
    return FALSE;
}

//=========================================================================
void player::CreateAllWeaponObjects( void )
{
    LOG_MESSAGE( "player::CreateAllWeaponObjects", "Creating weapons" );

    // Create the weapons for the player
    for( s32 i=0; i<INVEN_NUM_WEAPONS; i++ )
    {
        inven_item  WeaponItem      = inventory2::WeaponIndexToItem(i);
        const char* pBlueprintName  = inventory2::ItemToBlueprintName( WeaponItem );

        if( pBlueprintName )
        {
            // Get the weapon or create the weapon
            guid WeaponGUID = m_WeaponGuids[i];
            new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( WeaponGUID );
            if( pWeapon == NULL )
            {
                m_WeaponGuids[i] = 0;
                WeaponGUID = g_TemplateMgr.CreateSingleTemplate( pBlueprintName, vector3(0,0,0), radian3( 0,0,0 ), 0, 0 );
            }

            if( WeaponGUID )
            {
                new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( WeaponGUID );
                ASSERT( pWeapon );

                pWeapon->InitWeapon( GetPosition(), new_weapon::RENDER_STATE_PLAYER, GetGuid() );
                OnWeaponAnimInit2( WeaponItem, pWeapon );
                pWeapon->SetupRenderInformation( );
                pWeapon->SetAnimation( ANIM_STATE_IDLE, 0.0f );
                pWeapon->BeginIdle();

                // make sure m_AmmoInCurrentClip and such are set up properly
                pWeapon->Reload(new_weapon::AMMO_PRIMARY);

                m_WeaponGuids[i] = WeaponGUID;

                CLOG_MESSAGE( s_LogWeaponCreation, "player::CreateAllWeaponObjects", "Created: %s", inventory2::ItemToName( WeaponItem ) );
            }
        }
    }

}

//=========================================================================
s32 player::GetAmmoFromWeaponType(inven_item Item)
{
    s32 Amount = 0;

    switch( Item )
    {
    case INVEN_WEAPON_SMP:
    case INVEN_WEAPON_SHOTGUN:
    case INVEN_WEAPON_DESERT_EAGLE:
    case INVEN_WEAPON_SNIPER_RIFLE:
        {
            // Find the weapon from item type and get the amount of max ammo per clip and return it
            new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( m_WeaponGuids[inventory2::ItemToWeaponIndex(Item)] );
            if( pWeapon )
            {
                Amount = pWeapon->GetAmmoPerClip(new_weapon::AMMO_PRIMARY);
            }
        }
        break;
    default:
    case INVEN_WEAPON_DUAL_SMP:
    case INVEN_WEAPON_DUAL_SHT:
        //case INVEN_WEAPON_MESON_CANNON: // don't do anything for meson cannon?
    case INVEN_WEAPON_BBG: // don't do anything for BBG?
        {
            Amount = 0; // make sure
        }
        break;
    }

    return Amount;
}

//=========================================================================
xbool player::CheckForDualWeaponSetup( void )
{
    xbool RetVal = FALSE;

    // set up dual SMPs
    if( m_Inventory2.HasItem(INVEN_WEAPON_SMP) && !m_Inventory2.HasItem(INVEN_WEAPON_DUAL_SMP))
    {
        m_Inventory2.SetAmount( INVEN_WEAPON_DUAL_SMP, 1.0f );
        SetNextWeapon2( INVEN_WEAPON_DUAL_SMP );

        new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( m_WeaponGuids[inventory2::ItemToWeaponIndex(INVEN_WEAPON_DUAL_SMP)] );
        if( pWeapon )
        {
            // make sure this weapon's clip is full
            pWeapon->RefillClip(new_weapon::AMMO_PRIMARY);
        }

        // pick this SMP up, will dual wield it.
        RetVal = TRUE;
    }
    else
    {
        if( m_Inventory2.HasItem(INVEN_WEAPON_DUAL_SMP) )
        {
            // already have one, don't pick it up
            RetVal = FALSE;    
        }
        else
        {
            m_Inventory2.SetAmount( INVEN_WEAPON_SMP, 1.0f );

            // don't have one at all yet, pick it up
            RetVal = TRUE;
        }
    }

    return RetVal;
}

//=========================================================================
xbool player::SetupDualWeaponDiscard( inven_item &WeaponItem )
{
    switch( WeaponItem )
    {
    case INVEN_WEAPON_DUAL_SMP:
        {
            // clear dual
            m_Inventory2.SetAmount( INVEN_WEAPON_DUAL_SMP, 0.0f );

            // Set next weapon to the SMP
            WeaponItem = INVEN_WEAPON_SMP;

            new_weapon* pWeapon = GetWeaponPtr( WeaponItem );

            // clear current weapon's clip ammo so that we'll have to reload once we dump the dual.            
            if( pWeapon )
            {                
                pWeapon->ClearClipAmmo();
            }
        }
        break;
    case INVEN_WEAPON_DUAL_SHT:
        {
            // clear dual
            m_Inventory2.SetAmount( INVEN_WEAPON_DUAL_SHT, 0.0f );

            // Set next weapon to the shotgun
            WeaponItem = INVEN_WEAPON_SHOTGUN;

            new_weapon* pWeapon = GetWeaponPtr( WeaponItem );

            // clear current weapon's clip ammo so that we'll have to reload once we dump the dual.            
            if( pWeapon )
            {                
                pWeapon->ClearClipAmmo();
            }
        }
        break;
    default:
        {
            // no dual discard
            return FALSE;
        }
        break;
    }

    return TRUE;
}

//=========================================================================
s32 player::GetWeaponRenderState( void )
{
    return new_weapon::RENDER_STATE_PLAYER;
}

//=========================================================================
void player::RemoveAllWeaponInventory( void )
{
    // Remove all the weapons from the inventory.
    for( s32 i=0 ; i<INVEN_NUM_WEAPONS ; i++ )
    {
        m_Inventory2.SetAmount( inventory2::WeaponIndexToItem(i), 0.0f );
    }

    // Set the next weapon type and switch from the current weapon.
    m_NextWeaponItem = INVEN_NULL;
    SetAnimState( ANIM_STATE_SWITCH_FROM );
}

//=========================================================================
void player::OnTransformWeapon( const matrix4& L2W )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon )
    {
        // call weapon's OnTransform
        pWeapon->OnTransform( L2W );

        // Update 3rd person weapon also so split screen works
        actor::MoveWeapon( FALSE );

    }
}

//=========================================================================
void player::OnMoveWeapon( void )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();

    // update weapon position
    if( pWeapon )
    {
        // call weapon's OnMove
        pWeapon->OnMove( m_AnimPlayer.GetPosition());

        // update zones
        pWeapon->SetZone1( GetZone1() );

    }
}

//=========================================================================
void player::SwitchWeapon2( inven_item WeaponItem )
{
    //==========================================================================================
    // Begin code from ghost.cpp
    //==========================================================================================

    if( WeaponItem != m_CurrentWeaponItem )
    {
        m_NextWeaponItem = WeaponItem;
    }

    //==========================================================================================
    // End code from ghost.cpp
    //==========================================================================================

    if( WeaponItem != m_CurrentWeaponItem )
    {
        //set the state to ANIM_STATE_SWITCH_FROM
        SetAnimState( ANIM_STATE_SWITCH_FROM );

        // turn off the flashlight
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( !pWeapon || (!pWeapon->HasFlashlight()) )
        {
            // weapon is invalid?  Turn off flashlight then
            SetFlashlightActive(FALSE);            
        }
        else if( pWeapon->HasFlashlight() )
        {
            SetFlashlightActive(TRUE);
        }
    }
}

//=========================================================================
guid player::GetCurrentWeaponGuid2( void )
{
    return m_WeaponGuids[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)];
}

//=========================================================================
void player::ResetWeaponFlags( void )
{

}

//=========================================================================
f32 player::GetLastTimeWeaponFired()
{
    return m_LastTimeWeaponFired;
}

//=========================================================================
const char* player::GetCurrentWeaponName( void )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();

    if( pWeapon == NULL )
        return "";

    switch( pWeapon->GetType() )
    {
    case object::TYPE_WEAPON_SMP:           return "SMP";
        // KSS -- TO ADD NEW WEAPON
    case object::TYPE_WEAPON_SHOTGUN:       return "SHT";
    case object::TYPE_WEAPON_SCANNER:       return "SCN";
    case object::TYPE_WEAPON_SNIPER:        return "SNI";
    case object::TYPE_WEAPON_DESERT_EAGLE:  return "EGL";
    case object::TYPE_WEAPON_MSN:           return "MSN";
    case object::TYPE_WEAPON_BBG:           return "BBG";
    case object:: TYPE_WEAPON_TRA:          return "TRA";
    case object::TYPE_WEAPON_MUTATION:      return "MUT";
    default:                                return "";
    }
}

//=========================================================================
void player::ResetWeaponAnimTable2( inven_item WeaponItem )
{
//  LOG_MESSAGE( "player::ResetWeaponAnimTable", "" );

    //clear the weapon animation array only.
    for ( s32 j = 0; j < ANIM_STATE_DEATH; j++ )
    {
        m_Anim[inventory2::ItemToWeaponIndex(WeaponItem)][j].nWeaponAnims = 0;
    }
}

//=========================================================================
s32 player::GetNextFiringAnimIndex( void )
{
    //generate a random number between 0 and 1
    f32 fRand = x_frand( 0.0f, 1.0f );

    for ( s32 i = 0; i < MAX_ANIM_PER_STATE; i++ )
    {
        if ( fRand < m_fAnimPriorityPercentage[i] )
        {
            return i;
        }
    }

    ASSERT( FALSE );
    return 0;
}

//=========================================================================
void player::GenerateFiringAnimPercentages( void )
{
    //get the current state.
    state_anims& State = m_Anim[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)][m_CurrentAnimState];

    //set up the percentages.  if there's 4 animations: .70 / .15 / .10 / .05
    if ( State.nPlayerAnims == MAX_ANIM_PER_STATE )
    {
        m_fAnimPriorityPercentage[0] = 0.70f;
        m_fAnimPriorityPercentage[1] = 0.85f;
        m_fAnimPriorityPercentage[2] = 0.95f;
        m_fAnimPriorityPercentage[3] = 1.00f;
    }

    //if there's 3 animations: .75 / .15 / .10 / 0
    else
        if ( State.nPlayerAnims == 3 )
        {
            m_fAnimPriorityPercentage[0] = 0.7f;
            m_fAnimPriorityPercentage[1] = 0.9f;
            m_fAnimPriorityPercentage[2] = 1.0f;
            m_fAnimPriorityPercentage[3] = 0.0f;
        }

        //if there's 2 animations: .75 / .25 / 0 / 0
        else
            if ( State.nPlayerAnims == 2 )
            {
                m_fAnimPriorityPercentage[0] = 0.75f;
                m_fAnimPriorityPercentage[1] = 1.00f;
                m_fAnimPriorityPercentage[2] = 0.00f;
                m_fAnimPriorityPercentage[3] = 0.00f;
            }

            //if there's 1 animations: 1.0 / 0 / 0 / 0
            else
                if ( State.nPlayerAnims == 1 )
                {
                    m_fAnimPriorityPercentage[0] = 1.0f;
                    m_fAnimPriorityPercentage[1] = 0.0f;
                    m_fAnimPriorityPercentage[2] = 0.0f;
                    m_fAnimPriorityPercentage[3] = 0.0f;
                }
                else
                {
                    ASSERT( FALSE );
                }
}

//=========================================================================
void player::OnWeaponAnimInit2( inven_item WeaponItem, new_weapon* pWeapon )
{
    LOG_MESSAGE( "player::OnWeaponAnimInit", "" );

    // If the weapons has already been initialized with the player don't do it again.
    if( m_WeaponGuids[inventory2::ItemToWeaponIndex(WeaponItem)] != 0 )
        return;

    //reset this weapon's animation table
    ResetWeaponAnimTable2( WeaponItem );

    if ( pWeapon->IsInited() )
    {
        pWeapon->SetupRenderInformation( );

        if( pWeapon->HasAnimGroup() == FALSE )
            return;

        // Get the weapon's anim group that we're initializing.
        const anim_group& WeaponAnimGroup = pWeapon->GetCurrentAnimGroup();
        animation_state AnimIndex = ANIM_STATE_UNDEFINED;

        for ( s32 i = 0; i < WeaponAnimGroup.GetNAnims(); i++ )
        {
            AnimIndex = ANIM_STATE_UNDEFINED;

            const anim_info& AnimInfo   = WeaponAnimGroup.GetAnimInfo( i );
            const char*      pAnimName  = AnimInfo.GetName();

            // This animation is always there...
            if ( x_strcmp( pAnimName, "BIND_POSE" ) == 0 )
                continue;

            AnimIndex = GetAnimStateFromName( pAnimName );

            if ( AnimIndex == ANIM_STATE_UNDEFINED )
            {
                continue;
            }

            //We have valid index to the animation table, now set the values
            state_anims& State =  m_Anim[inventory2::ItemToWeaponIndex(WeaponItem)][AnimIndex];

            if( State.nWeaponAnims >= MAX_ANIM_PER_STATE )
            {
                x_try;
                x_throw( xfs( "WARNING: Too many animations of this type %s for weapon" , pAnimName ));
                x_catch_display;
                continue;
            }
            else
            {
                // TODO:  Eventually, weapons will need to contain animation sets for all
                //        player strains.  This sets all weapon animations to the human set for now.
                // Set the index of the animation
                ASSERT(State.nWeaponAnims < MAX_ANIM_PER_STATE);
                State.WeaponAnim[State.nWeaponAnims] = i;
                State.nWeaponAnims++;
            }
        }

        pWeapon->SetupRenderInformation( );
    }   
}

//=========================================================================
xbool player::ShouldSwitchToWeapon2( inven_item WeaponItem, xbool bFirstPickup )
{
    if( m_bDead )
    {
        return FALSE;
    }

    if( m_CurrentWeaponItem == INVEN_NULL )
    {
        // Player has no weapon -> Probably a good idea to switch
        return TRUE;
    }

    // make sure that if we are mutated, we don't switch weapons, AT ALL!
    if( IsMutated() )
    {
        return FALSE;
    }

    // TODO: CJ: WEAPONS: Revisit this to put the rules in place
    // mreed: temporary hack until we get all the rules defined and in place
    xbool RetVal = FALSE;

    // this is a defaulted value for the editor, in the game it will check a profile setting.
    xbool bShouldCheckRating = TRUE;
    
#ifndef X_EDITOR
    // get player profile
    player_profile& p = g_StateMgr.GetActiveProfile(g_StateMgr.GetProfileListIndex(m_LocalSlot));

    bShouldCheckRating = p.GetWeaponAutoSwitch();
#endif

    inven_item ParentItem = new_weapon::GetParentIDForDualWeapon( WeaponItem );

    // see if this is a dual weapon
    xbool bSwitchDualWeapon = FALSE;

    if( ParentItem != INVEN_NULL )
    {
        // is our current weapon the parent of this new weapon?
        // if so, this means we need to switch to the dual version
        bSwitchDualWeapon = ( m_CurrentWeaponItem == ParentItem );
    }

    // if weapon auto-switch is set, check weapon auto-switch ratings if this is the first time we picked it up.
    if( (bShouldCheckRating && bFirstPickup) || bSwitchDualWeapon )
    {
        new_weapon *pCurrentWeapon  = GetCurrentWeaponPtr();
        new_weapon *pNewWeapon      = GetWeaponPtr(WeaponItem);

        // if our new weapon is rated higher than current, switch to it.
        if(    pNewWeapon 
            && pCurrentWeapon 
            && (pNewWeapon->GetAutoSwitchRating() > pCurrentWeapon->GetAutoSwitchRating()) )
        {
            RetVal = TRUE;
        }
    }

    return RetVal;
}

//=========================================================================
void player::AddNewWeapon2( inven_item WeaponItem )
{
    // Already have the weapon?
    if( m_Inventory2.HasItem( WeaponItem ) )
    {
        return;
    }

    // Add the weapon
    m_Inventory2.AddAmount( WeaponItem, 1.0f );

    if( (m_CurrentWeaponItem == INVEN_NULL) && (WeaponItem != INVEN_WEAPON_MUTATION) )
    {
        m_CurrentWeaponItem = WeaponItem;

        // Set the render index.
        new_weapon* pWeapon = GetWeaponPtr( WeaponItem );
        if( pWeapon )
        {
            pWeapon->SetupRenderInformation( );
        }
    }
}

//=========================================================================
void player::AttachWeapon( void )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if ( pWeapon )
    {
        radian3 Rot( m_AnimPlayer.GetPitch() , m_AnimPlayer.GetYaw(), m_AnimPlayer.GetRoll() );
        vector3 Pos( m_AnimPlayer.GetPosition() );

        matrix4 L2W;
        L2W.Identity();
        L2W.SetRotation( Rot );
        L2W.SetTranslation( Pos );

        //( (new_weapon*)pObject )->SetRotation
        //( (new_weapon*)pObject )->OnMove
        OnTransformWeapon( L2W );
        pWeapon->SetZone1( GetZone1() );
    }
}

//=========================================================================
inven_item player::GetNextAvailableWeapon2( const cycle_direction& CycleDirection )
{
    inven_item CurrentWeaponItem = m_CurrentWeaponItem;

    //determine if we need to add or subtract from m_CurrentWeapon
    s32 Direction = 1;
    if ( CycleDirection == CYCLE_LEFT )
    {
        Direction = -1;
    }
    //select the next weapon in the list to test.
    u32 NextWeapon = (inven_item)(( (s32)CurrentWeaponItem + Direction ) % INVEN_NUM_WEAPONS);
    NextWeapon = ( NextWeapon>INVEN_NUM_WEAPONS ) ? INVEN_NUM_WEAPONS-1: NextWeapon;

    // TODO: CJ: WEAPONS: Check for infinite loop?
    while( (inven_item)NextWeapon != CurrentWeaponItem )
    {
        // should we skip this weapon (i.e. dual SMPs switching to/from SMP)
        if( ! ShouldSkipWeaponCycle(CurrentWeaponItem, (inven_item)NextWeapon) )
        {
            //if the weapon is in our inventory, that's is for the while loop. can't cycle to mutation weapon
            if( (NextWeapon != INVEN_WEAPON_MUTATION) && (m_Inventory2.HasItem( (inven_item)NextWeapon )) )
                break;
        }

        NextWeapon = (((s32)NextWeapon + Direction ) % INVEN_NUM_WEAPONS);
        NextWeapon = ( NextWeapon>INVEN_NUM_WEAPONS ) ? INVEN_NUM_WEAPONS-1: NextWeapon;
    }

    // Check for Dual SMP
    if( ((inven_item)NextWeapon == INVEN_WEAPON_SMP) && m_Inventory2.HasItem(INVEN_WEAPON_DUAL_SMP) )
    {
        NextWeapon = INVEN_WEAPON_DUAL_SMP;
    }
    else
    // Check for Dual Shotguns
    if( ((inven_item)NextWeapon == INVEN_WEAPON_SHOTGUN) && m_Inventory2.HasItem(INVEN_WEAPON_DUAL_SHT) )
    {
        NextWeapon = INVEN_WEAPON_DUAL_SHT;
    }

    return (inven_item)NextWeapon;
}

//=========================================================================
xbool player::ShouldSkipWeaponCycle( const inven_item& CurrentWeaponItem, const inven_item& NextWeapon )
{
    switch( CurrentWeaponItem )
    {
    case INVEN_WEAPON_SMP:
        {
            if( NextWeapon == INVEN_WEAPON_DUAL_SMP )
            {
                return TRUE;
            }            
        }
        break;
    case INVEN_WEAPON_DUAL_SMP:
        {
            if( NextWeapon == INVEN_WEAPON_SMP )
            {
                return TRUE;
            }
        }
        break;

    case INVEN_WEAPON_SHOTGUN:
        {
            if( NextWeapon == INVEN_WEAPON_DUAL_SHT )
            {
                return TRUE;
            }            
        }
        break;
    case INVEN_WEAPON_DUAL_SHT:
        {
            if( NextWeapon == INVEN_WEAPON_SHOTGUN )
            {
                return TRUE;
            }
        }
        break;
    default:
        {
            return FALSE;
        }
        break;
    }

    return FALSE;
}

//=========================================================================
void player::SetNextWeapon2( inven_item WeaponItem, xbool ForceSwitch, xbool StateChange )
{
    if( (WeaponItem == m_CurrentWeaponItem) && !ForceSwitch )
    {
        // No need to switch to the current weapon
        return;
    }

    m_NextWeaponItem = WeaponItem;

    // turn off the flashlight
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon )
    {
        pWeapon->ClearZoom();

        // make sure we clear anything the weapon is doing
        pWeapon->BeginSwitchFrom();
    }
    else
    {
        // weapon is invalid?  Turn off flashlight then
        SetFlashlightActive( FALSE );
    }

    // Check weapon that's about to be the current weapon.
    pWeapon = GetWeaponPtr( WeaponItem );
    if( pWeapon )
    {
        // this new weapon doesn't have a flashlight, turn it off
        if( !pWeapon->HasFlashlight() )
        {
            SetFlashlightActive( FALSE );
        }
    }

    if( StateChange && !m_bDead )
    {
        // Switch to the next weapon.
        SetAnimState( ANIM_STATE_SWITCH_FROM );
    }

#ifndef X_EDITOR
    m_NetDirtyBits |= WEAPON_BIT;  // NETWORK
#endif // X_EDITOR 
}

//=========================================================================
void player::OnWeaponSwitch2( const cycle_direction& CycleDirection )
{
    if( (m_CurrentAnimState == ANIM_STATE_GRENADE) ||
        (m_CurrentAnimState == ANIM_STATE_ALT_GRENADE) ||
        (m_CurrentAnimState == ANIM_STATE_DISCARD) )
        return;

    if( IsMutated() )
    {
        return;
    }

    //player is attempting to switch weapons, we'll cycle through the weapons that are in the inventory
    SetNextWeapon2( GetNextAvailableWeapon2( CycleDirection ) );
}

//=========================================================================
xbool player::AddAmmo2( inven_item WeaponItem, s32 Amount )
{
    // not sending in anything
    if( Amount == 0 )
        return FALSE;

    if( m_Inventory2.HasItem( WeaponItem ) )
    {
        new_weapon* pWeapon = GetWeaponPtr( WeaponItem );

        if( pWeapon )
        {
            s32 nCurrentAmmoCount = pWeapon->GetTotalPrimaryAmmo();
            s32 nMaxAmmoCount     = pWeapon->GetMaxPrimaryAmmo();

            if( nCurrentAmmoCount < nMaxAmmoCount )
            {
                f32 NewAmount = (f32)Amount;
#ifndef X_EDITOR
                if( GameMgr.GetGameType() == GAME_CAMPAIGN )
                {
                    tweak_handle AmmoScalarTweak( xfs( "AmmoAmount_%s", DifficultyText[g_Difficulty] ) );

                    f32 AmmoScalar = 0.0f;

                    if( AmmoScalarTweak.Exists() )
                    {
                        AmmoScalar = AmmoScalarTweak.GetF32();
                    }

                    // Scale ammo based on difficulty level
                    // the scalar could be +/- and is a whole percentage i.e. -20
                    NewAmount = Amount + ( Amount * (AmmoScalar/100.0f) );

                    // make sure we don't give less than 1, that's bad
                    if( NewAmount < 1.0f )
                    {
                        NewAmount = 1.0f;
                    }
                }
#endif 
                // actually put ammo into weapon
                pWeapon->AddAmmoToWeapon( (s32)NewAmount, 0 );

                // if this weapon was empty, reload it for us
                if( nCurrentAmmoCount == 0 )
                {
                    // if this is our current weapon, do all the animations and such
                    if( WeaponItem == m_CurrentWeaponItem )
                    {
                        ReloadWeapon(new_weapon::AMMO_PRIMARY);
                    }
                    else
                    {
                        // otherwise, just auto-reload it
                        pWeapon->Reload(new_weapon::AMMO_PRIMARY);
                    }
                }

                return TRUE;
            }
        }
    }
    return FALSE;
}

void player::OnWeaponSwitch2( inven_item WeaponItem )
{
    if( (m_CurrentAnimState == ANIM_STATE_GRENADE) ||
        (m_CurrentAnimState == ANIM_STATE_ALT_GRENADE) ||
        (m_CurrentAnimState == ANIM_STATE_DISCARD) )
        return;

    if( IsMutated() )
    {
        return;
    }

    if( (WeaponItem == INVEN_WEAPON_SMP) && m_Inventory2.HasItem(INVEN_WEAPON_DUAL_SMP) )
    {
        WeaponItem = INVEN_WEAPON_DUAL_SMP;
    }
    else
    if( (WeaponItem == INVEN_WEAPON_SHOTGUN) && m_Inventory2.HasItem(INVEN_WEAPON_DUAL_SHT) )
    {
        WeaponItem = INVEN_WEAPON_DUAL_SHT;
    }

    if( !m_Inventory2.HasItem( WeaponItem ) )
    {
        return;
    }

    //player is attempting to switch weapons, we'll switch directly to the requested weapon that is in the inventory
    SetNextWeapon2( WeaponItem );
}

void player:: ReInitInventory( void )
{
    ASSERT( m_WeaponsCreated );
    if( m_WeaponsCreated )
    {
        for( s32 i=0; i<INVEN_NUM_WEAPONS; i++ )
        {
            // Nuke the object, if it exists!
            new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( m_WeaponGuids[i] );
            if( pWeapon )
                g_ObjMgr.DestroyObjectEx( m_WeaponGuids[i], TRUE );
            m_WeaponGuids[i] = 0;
        }
        m_WeaponsCreated = FALSE;

        FXMgr.EndOfFrame(); // Call FXMgr EndOfFrame to flush any deferred deletes - must be after ObjMgr.Clear
        FXMgr.EndOfFrame();
        FXMgr.EndOfFrame();
        FXMgr.EndOfFrame();
    }

    // Now init the inventory.
    InitInventory();
}

s32 GetStartWeaponsForLevel( const char* pLevelName )
{
    // KSS -- TO ADD NEW WEAPON
    // Determine the weapons a player start with and the start item
    s32         Weapons     = WB_SMP | WB_DE | WB_SG | WB_SR | WB_MC | WB_MM | WB_MP | WB_FG | WB_BBG | WB_JBG | WB_SCN | WB_MS;
    for( s32 i=0; s_MapToWeaponTable[i].pLevelName ; i++ )
    {
        if( x_stristr( pLevelName, s_MapToWeaponTable[i].pLevelName ) )
        {
            Weapons     = s_MapToWeaponTable[i].StartWeapons;
            break;
        }
    }

    return Weapons;
}

s32 GetMemoryBallastForLevel( const char* pLevelName )
{
    // KSS -- TO ADD NEW WEAPON
    // Determine the amount of memory to withold.
    s32 Ballast = 0;
    for( s32 i=0; s_MapToWeaponTable[i].pLevelName ; i++ )
    {
        if( x_stristr( pLevelName, s_MapToWeaponTable[i].pLevelName ) )
        {
            Ballast = s_MapToWeaponTable[i].MemoryBallast;
            break;
        }
    }

    return Ballast;
}

inven_item GetEquipedWeaponForLevel( const char* pLevelName )
{
    // Determine the weapons a player start with and the start item
    inven_item  StartItem   = INVEN_WEAPON_SMP;
    for( s32 i=0; s_MapToWeaponTable[i].pLevelName ; i++ )
    {
        if( x_stristr( pLevelName, s_MapToWeaponTable[i].pLevelName ) )
        {
            StartItem   = s_MapToWeaponTable[i].StartItem;
            break;
        }
    }

    return StartItem;
}

//=========================================================================

void player::UpdateWeaponVisibility( void )
{
#ifndef X_EDITOR
    if( GameMgr.GetGameType() != GAME_CAMPAIGN )
    {
        m_bHidePlayerArms = (m_SpawnNeutralTime > 0.0f);
    }
#endif

    if( m_bHidePlayerArms )
    {
        if( !m_bArmsWereHidden )
        {
            new_weapon* pWeapon = GetCurrentWeaponPtr();
            if( pWeapon )
            {
                pWeapon->ClearZoom();
                SetAnimState( ANIM_STATE_IDLE );
                m_NextAnimState = ANIM_STATE_UNDEFINED;
            }
        }

        m_bArmsWereHidden = TRUE;
    }
    else if( m_bArmsWereHidden && m_bPlaySwitchTo )
    {
        SetAnimState( ANIM_STATE_SWITCH_TO );
        m_bArmsWereHidden = FALSE;
    }
}
