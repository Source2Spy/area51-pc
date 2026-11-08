//=========================================================================
//
//  PlayerInventory.cpp
//
//=========================================================================

#include "PlayerInventory.hpp"
#include "Player.hpp"
#include "Objects/ParticleEmiter.hpp"
#include "Objects/SpawnPoint.hpp"
#include "Objects/FocusObject.hpp"
#include "Objects/Flashlight.hpp"
#include "Objects/LoreObject.hpp"
#include "Objects/Pickup.hpp"
#include "Characters/ActorEffects.hpp"
#include "StateMgr/StateMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Configuration/GameConfig.hpp"
#include "GameLib/DebugCheats.hpp"
#include "TemplateMgr/TemplateMgr.hpp"

#ifdef X_EDITOR
#include "../../../Apps/Editor/Project.hpp"
#else
#include "NetworkMgr/Messages/MsgMgr.hpp"
#endif

//=========================================================================

extern tweak_handle Lore_Max_Detect_DistanceTweak;

extern s32        GetStartWeaponsForLevel    ( const char* pLevelName );
extern inven_item GetEquipedWeaponForLevel   ( const char* pLevelName );
extern mtwt       s_MapToWeaponTable[];
static s32        GetAvailableWeaponsForLevel( const char* pLevelName );

static tweak_handle Item_Full_Msg_DelayTimeTweak    ( "Item_Full_Msg_DelayTime" );
static tweak_handle Item_Acquired_Msg_DelayTimeTweak( "Item_Acquired_Msg_DelayTime" );
static tweak_handle FlashlightAutoOffSecondsTweak   ( "FlashlightAutoOffSeconds" );
static tweak_handle FlashlightAutoOffBrightnessTweak( "FlashlightAutoOffBrightness" );
static const xbool  s_LogEditorInventory = FALSE;

extern xbool s_bUseTestMap;
extern s32   s_ScannerTestMap;

struct JBGLoreEntry
{
    s32 MapID;
    s32 LoreIndex;
};

static const JBGLoreEntry s_JBGLoreEntries[] =
{
    { 1075, 4 },
    { 1090, 0 }
};

PlayerInventoryInput::PlayerInventoryInput( void ) :
    WeaponRequest( PlayerWeaponRequest::None ),
    UseHeld      ( FALSE ),
    UsePressed   ( FALSE )
{
}


//=========================================================================

PlayerInventoryInput PlayerInventory::BuildInput( const PlayerInputState& Input,
                                                  xbool AllowWeaponSwitch ) const
{
    PlayerInventoryInput InventoryInput;
    InventoryInput.UseHeld    = Input.IsHeld( PlayerAction::Use );
    InventoryInput.UsePressed = Input.WasPressed( PlayerAction::Use );

    if( !AllowWeaponSwitch )
        return InventoryInput;

    if( Input.WasPressed( PlayerAction::CycleWeaponRight ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::CycleRight;
    else if( Input.WasPressed( PlayerAction::CycleWeaponLeft ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::CycleLeft;
    else if( Input.WasPressed( PlayerAction::SwitchToScanner ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::Scanner;
    else if( Input.WasPressed( PlayerAction::SwitchToPistol ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::Pistol;
    else if( Input.WasPressed( PlayerAction::SwitchToSmp ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::Smp;
    else if( Input.WasPressed( PlayerAction::SwitchToShotgun ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::Shotgun;
    else if( Input.WasPressed( PlayerAction::SwitchToSniperRifle ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::SniperRifle;
    else if( Input.WasPressed( PlayerAction::SwitchToBbg ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::Bbg;
    else if( Input.WasPressed( PlayerAction::SwitchToMesonCannon ) )
        InventoryInput.WeaponRequest = PlayerWeaponRequest::MesonCannon;

    return InventoryInput;
}

xbool player::UseFocusObject( void )
{
    // 
    // Look for all focus objects in the world, and try to press every single
    // one of them until we find one that works.  
    //
    const PlayerInventoryInput InventoryInput =
        m_InventoryInteraction.BuildInput( m_Input.GetState(), FALSE );
    const xbool UsePressed = InventoryInput.UsePressed;
    if( UsePressed && (m_CurrentAnimState != ANIM_STATE_THROW ) && !IsChangingMutation() )
    {
        slot_id SlotID = g_ObjMgr.GetFirst( object::TYPE_FOCUS_OBJECT );
        while( SlotID != SLOT_NULL )
        {
            object* pObject = g_ObjMgr.GetObjectBySlot( SlotID );
            SlotID = g_ObjMgr.GetNext(SlotID);

            if (pObject && pObject->IsKindOf( focus_object::GetRTTI()))
            {
                focus_object* pFocusObject = (focus_object*)pObject;
                if( pFocusObject->TestPress() )
                {
                    // If we successfully used the FO, then we don't want to 
                    // do anything else with this button press.
                    m_UseTime = 0.0f;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

xbool player::NearMutagenReservoir( void )
{
    xbool RetVal = FALSE;

    g_ObjMgr.SelectBBox( ATTR_ALL, GetBBox(), TYPE_MUTAGEN_RESERVOIR );
    slot_id ObjectSlot = SLOT_NULL;
    object* pObject = NULL;
    for ( ObjectSlot = g_ObjMgr.StartLoop();
          ObjectSlot != SLOT_NULL;
          ObjectSlot = g_ObjMgr.GetNextResult( ObjectSlot ) )
    {
        pObject = g_ObjMgr.GetObjectBySlot( ObjectSlot );
        if ( pObject
          && GetBBox().Intersect( pObject->GetBBox() ) )
        {
            RetVal = TRUE;
            break;
        }
    }

    g_ObjMgr.EndLoop();

    return RetVal;
}

void player::AcquireAllLoreObjects( void )
{
    // already done
    if( m_bAllLoreObjectsCollected )
    {
        return;
    }

    // make sure flag is set
    m_bAllLoreObjectsCollected = TRUE;

#ifndef X_EDITOR
    // acquire all lore objects
    s32 i = 0;
    for( i = 0; i < MAX_LORE_ITEMS; i++ )
    {
        lore_object* pLoreObject = (lore_object*)g_ObjMgr.GetObjectByGuid(m_LoreObjectGuids[i]);

        // if valid and is still active, get it.
        if( pLoreObject && pLoreObject->IsActivated() )
        {
            pLoreObject->OnAcquire();
        }
    }
#endif
    
}

void player::LoadAllLoreObjects( void )
{
    s32 count = 0;

    // load up all the lore objects for collecting and for updating the Geiger counter
    slot_id SlotID = g_ObjMgr.GetFirst(object::TYPE_LORE_OBJECT);
    while( SlotID != SLOT_NULL && count < MAX_LORE_ITEMS )
    {
        // Lookup lore object
        object* pObj              = g_ObjMgr.GetObjectBySlot(SlotID);
        lore_object*  pLoreObject = &lore_object::GetSafeType( *pObj );

        if( !pLoreObject )
        {
            ASSERT(0);
            continue;
        }

        // true lore object
        if( pLoreObject->IsTrueLoreObject() )
        {
#ifndef X_EDITOR
            s32 VaultIndex = -1;
            m_LoreObjectGuids[count] = pLoreObject->GetGuid();

            if( s_bUseTestMap )
            {
                // For testing
                g_LoreList.GetVaultByMapID( s_ScannerTestMap, VaultIndex );
            }
            else
            {
                s32 MapID = g_ActiveConfig.GetLevelID();
                g_LoreList.GetVaultByMapID( MapID, VaultIndex );
            }

            // only do this if we're in campaign mode
            if( GameMgr.GetGameType() == GAME_CAMPAIGN )
            {
                player_profile& Profile = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex(0) );
                
                if( VaultIndex != -1 )
                {
                    // we have this one
                    if( Profile.GetLoreAcquired( VaultIndex, pLoreObject->GetLoreID() ) )
                    {
                        // silently acquire the lore object.
                        pLoreObject->OnAcquire( TRUE );
                    }
                }
            }
#endif
            count++;
        }
    
        // Get next lore object
        SlotID = g_ObjMgr.GetNext(SlotID);
    }

    m_bAllLoreObjectsCollected = FALSE;
}

xbool player::GetClosestLoreObjectDist( f32 &ClosestDist )
{
    if( m_bAllLoreObjectsCollected )
    {
        return FALSE;
    }

    ClosestDist = Lore_Max_Detect_DistanceTweak.GetF32() + 100.0f; // 31 meters; 1 meter greater than max distance
    s32 i;
    xbool bAllCollected = TRUE;
    xbool bFound        = FALSE;

    for( i = 0; i < MAX_LORE_ITEMS; i++ )
    {
        lore_object* pLoreObject = (lore_object*)g_ObjMgr.GetObjectByGuid(m_LoreObjectGuids[i]);
        if( pLoreObject && pLoreObject->IsActivated() && (pLoreObject->GetLoreID() != -1) )
        {
            // we found an active one
            bAllCollected = FALSE;

            f32 dist = (GetPosition() - pLoreObject->GetPosition()).Length();

            // is this one closer?
            if( dist < ClosestDist )
            {
                bFound = TRUE;
                ClosestDist = dist;
            }
        }
    }

    // all are activated
    if( bAllCollected )
    {
        m_bAllLoreObjectsCollected = TRUE;
    }

    return bFound;
}

void player::OnCollectLight( void )
{
    flashlight_Register( *this );
}

xbool player::IsFlashlightActive( void )
{
    if( !m_bUsingFlashlight )
        return FALSE;

    matrix4 L2W;
    return flashlight_CalcTransform( *this, L2W );
}

void player::SetFlashlightActive( xbool bOn )
{

// Just for MsgMgr testing purposes.


    new_weapon* pWeapon = GetCurrentWeaponPtr();
    const xbool bCanUseFlashlight = ( pWeapon && pWeapon->HasFlashlight() && pWeapon->CheckFlashlightPoint() );
    const xbool bWasUsingFlashlight = m_bUsingFlashlight;
    m_bUsingFlashlight = ( bOn && bCanUseFlashlight );

    if ( bWasUsingFlashlight != m_bUsingFlashlight )
    {
        actor_effects* pActorEffects = GetActorEffects( TRUE );

        if ( m_bUsingFlashlight )
        {
            if ( pActorEffects )
            {
                pActorEffects->InitEffect( actor_effects::FX_FLASHLIGHT, this );
            }
        }
        else
        {
            if( pActorEffects )
            {
                pActorEffects->KillEffect( actor_effects::FX_FLASHLIGHT );
            }
        }
    }

    #ifndef X_EDITOR
    m_NetDirtyBits |= FLASHLIGHT_BIT;
    #endif

    // flashlight is off, reset flashlight timeout
    if( m_bUsingFlashlight == FALSE )
    {
        m_FlashlightTimeout = FlashlightAutoOffSecondsTweak.GetF32();
    }
}

void player::UpdateFlashlightBattery( f32 nDeltaTime )
{
    // KSS -- people hate the flashlight battery, so removing it.
    // battery "conservation" :)
    if( IsFlashlightActive() )
    {
        // if we are playing a cinematic, don't burn battery
        if( IsCinemaRunning() )
        {
            if ( m_bUsingFlashlight )
            {
                m_bUsingFlashlightBeforeCinema = TRUE;
                // if the flashlight is active and a cinema is running, turn it off and get out
                SetFlashlightActive(FALSE);
            }

            return;
        }

        u8 Brightness = (u8)GetFloorIntensity();
        
        // if light levels are high, turn off flashlight after FLASHLIGHT_AUTOOFF_TIME seconds
        if( Brightness > FlashlightAutoOffBrightnessTweak.GetF32() )
        {
            m_FlashlightTimeout -= nDeltaTime;
            
            if( m_FlashlightTimeout <= F32_MIN )
            {
                // this will reset timer as well
                SetFlashlightActive(FALSE);
            }
        }
        else
        {
            m_FlashlightTimeout = FlashlightAutoOffSecondsTweak.GetF32();
        }
    }
}

xbool player::AddBattery( const f32& nDeltaBattery )
{        
    // do not allow Battery to go above max.
    if( m_Battery == m_MaxBattery && nDeltaBattery > 0.0f )
    {
        return FALSE;
    }
    else if( (m_Battery + nDeltaBattery) < 0.0f )  // does what we are using take us below 0?
    {
        // don't have enough
        return false;
    }
    else
    {
        // add/subtract Battery
        m_Battery = fMin( m_Battery + nDeltaBattery , m_MaxBattery );
        m_Battery = fMax( m_Battery , 0.0f );

        return TRUE;
    }

    return FALSE;
}

xbool player::AddItemToInventory2( inven_item Item )
{
    xbool bFirstTimePickedUp = FALSE;

    // if we're dead, don't add this to our inventory.
    if( IsDead() )
    {
        return FALSE;
    }

    // is this the first time they've picked up this weapon on this level
    if( !m_Inventory2.HasItem(Item) )
    {
        bFirstTimePickedUp = TRUE;
    }

    xbool ItemAdded = actor::AddItemToInventory2( Item );

    if( ItemAdded && inventory2::IsAWeapon( Item ) )
    {
        // Adding a weapon -> Determine if we should switch to the new one
        if( ShouldSwitchToWeapon2( Item, bFirstTimePickedUp ) )
        {
            // force the switch if this is the first time we've picked it up
            SetNextWeapon2( Item, bFirstTimePickedUp );
        }
    }
   
    return ItemAdded;
}

xbool player::RemoveItemFromInventory2( inven_item Item, xbool bRemoveAll )
{
    if( Item == m_CurrentWeaponItem )
    {
        // Removing the current weapon -> Switch to having no weapon at all.
        // NOTE:  This logic assumes that when you remove the player's current
        // weapon, they shouldn't have any weapons.
        SwitchWeapon2( INVEN_NULL );
    }

    return actor::RemoveItemFromInventory2( Item, bRemoveAll );
}

void player::ItemAcquiredMessage( inven_item Item )
{
#if !defined(X_EDITOR)    
    f32 currentTime = (f32)g_ObjMgr.GetSimulationTimeSeconds();

    // make sure we don't flood the player with info.
    if( (currentTime - m_fLastItemAcquiredTime) > Item_Acquired_Msg_DelayTimeTweak.GetF32() )
    {
        // tell the player that they picked up something.
        MsgMgr.Message( MSG_ACQUIRED_ITEM, net_GetSlot(), Item );

        // reset time
        m_fLastItemAcquiredTime = currentTime;
    }
#endif
}

void player::ItemFullMessage( inven_item Item )
{
#if !defined(X_EDITOR)
    f32 currentTime = (f32)g_ObjMgr.GetSimulationTimeSeconds();

    // make sure we don't flood the player with info.
    if( (currentTime - m_fLastItemFullTime) > Item_Full_Msg_DelayTimeTweak.GetF32() )
    {
        // tell the player that they can't carry anymore of these.
        MsgMgr.Message( MSG_FULL_ITEM, net_GetSlot(), Item );

        // reset time
        m_fLastItemFullTime = currentTime;
    }
#endif
}

void player::NoWeapon_NoAmmoPickupMessage( inven_item Item )
{
#if !defined(X_EDITOR)
    f32 currentTime = (f32)g_ObjMgr.GetSimulationTimeSeconds();

    // make sure we don't flood the player with info.
    if( (currentTime - m_fLastItemFullTime) > Item_Full_Msg_DelayTimeTweak.GetF32() )
    {
        // tell the player that they can't carry anymore of these.
        MsgMgr.Message( MSG_NO_ITEM_AMMO_FAIL, net_GetSlot(), Item );

        // reset time
        m_fLastItemFullTime = currentTime;
    }
#endif    
}

xbool player::CanTakePickup( pickup& Pickup )
{
//  LOG_MESSAGE( "player::CanTakePickup", "" );

    if( m_bDead )
    {
        LOG_WARNING( "player::CanTakePickup", "Dead player trying to take a pickup!" );
        return FALSE;
    }
    
    if( !Pickup.GetTakeable() )
    {
        return FALSE;
    }   

    inven_item Item = Pickup.GetItem();

    // **HEALTH**
    if( Item == INVEN_HEALTH )
    {
        return( GetHealth() < 100.0f );
    }

    // **MUTAGEN**
    if( Item == INVEN_MUTAGEN )
    {
        return( GetMutagen() < 100.0f );
    }

    // **WEAPONS**
    // The only time you CAN'T take a weapon pickup is when you already have
    // the weapon AND you already have the maximum ammo allowed.
    if( IN_RANGE( INVEN_WEAPON_FIRST, Item, INVEN_WEAPON_LAST ) )
    {
        #ifndef X_EDITOR
        // Special case: In multiplayer, you can't take a weapon pickup if you
        // are currently mutated and (a) you already have the weapon, or (b)
        // you can't un-mutate.
        if( GameMgr.GetGameType() != GAME_CAMPAIGN )
        {
            if( m_bIsMutated && 
                    ((m_Inventory2.GetAmount( Item ) > 0) ||
                     (!m_bCanToggleMutation)) )
            {
                return( FALSE );
            }
        }
        #endif

        if( m_Inventory2.GetAmount( Item ) < m_Inventory2.GetMaxAmount( Item ) )
        {   
            return( TRUE );
        }

        new_weapon* pWeapon = GetWeaponPtr( Item );

        if( ( (Item == INVEN_WEAPON_SMP)          && (m_Inventory2.GetAmount( Item ) == 1.0f) ) ||            
            ( (Item == INVEN_WEAPON_SHOTGUN)      && (m_Inventory2.GetAmount( Item ) == 1.0f) ) ||
            ( (Item == INVEN_WEAPON_DESERT_EAGLE) && (m_Inventory2.GetAmount( Item ) == 1.0f) ) )
        {
            // get the dual weapon
            inven_item DualItem = new_weapon::GetDualWeaponID( Item );
            new_weapon* pDualWeapon = GetWeaponPtr( DualItem );

            // make sure the dual weapon is valid and that we already have the dual in our inventory.
            if( pDualWeapon && m_Inventory2.GetAmount( DualItem ) == 1.0f )
            {
                s32 Current = pDualWeapon->GetTotalPrimaryAmmo();
                s32 Limit   = pDualWeapon->GetMaxPrimaryAmmo();

                // full of ammo, don't get it.
                if( Current >= Limit )
                {
                    // don't pick it up
                    return FALSE;
                }
            }

            // dual weapon and we're missing some ammo or we haven't gotten a dual yet... get it.
            return TRUE;
        }
        
        if( pWeapon )
        {
            s32 Current = pWeapon->GetTotalPrimaryAmmo();
            s32 Limit   = pWeapon->GetMaxPrimaryAmmo();

            // never pickup a BBG for AMMO
            if( Item == INVEN_WEAPON_BBG )
            {
                return FALSE;
            }
            else
            if( (Current < Limit) )
            {
                // pick it up
                return TRUE;
            }
            else
            {
                // ammo full, don't pick it up and notify weapon for message
                pWeapon->NotifyAmmoFull(this);
                return FALSE;
            }
        }
        else
        {
            ASSERT( FALSE );    // We should have a weapon.
            return( FALSE );
        }
    }

    // Special case: In multiplayer, when mutated, you cannot take ammo pickups
    // (which includes grenades).
    #ifndef X_EDITOR
    if( GameMgr.GetGameType() != GAME_CAMPAIGN )
    {
        if( m_bIsMutated )
        {
            return( FALSE );
        }
    }
    #endif

    // **AMMO**
    // The only time you CAN'T take an ammo pickup is when you already have the
    // maximum ammo allowed.
    if( IN_RANGE( INVEN_AMMO_FIRST, Item, INVEN_AMMO_LAST ) )
    {
        // BBG ammo is just for debugging and such, NEVER pick it up
        if( Item == INVEN_AMMO_BBG )
        {
            ASSERTS(0, "You should not be placing BBG ammo, it is for debugging only" );
            return FALSE;
        }

        inven_item  Weapon  = m_Inventory2.AmmoToWeapon( Item );
        new_weapon* pWeapon = GetWeaponPtr( Weapon );
        if( pWeapon )
        {
            if( m_Inventory2.HasItem( Weapon ) )
            {
                s32 Current = pWeapon->GetTotalPrimaryAmmo();
                s32 Limit   = pWeapon->GetMaxPrimaryAmmo();

                if( Current < Limit )
                {
                    // pick it up
                    return TRUE;
                }
                else
                {
                    // ammo full, don't pick it up and notify weapon for message
                    pWeapon->NotifyAmmoFull(this);
                    return FALSE;
                }
            }
            else
            {
                // weapon hasn't been gotten yet, don't pick up ammo.
                NoWeapon_NoAmmoPickupMessage( Item );

                return( FALSE );
            }
        }
        else
        {
            ASSERT( FALSE );    // We should have a weapon.
            return( FALSE );
        }
    }

    // **EVERYTHING ELSE**
    // Otherwise, just check the limit within the inventory.
    if( m_Inventory2.CanHoldMore( Item ) )
    {
        return TRUE;
    }
    else
    {
        /* REMOVED because we couldn't get the German translation to work.
        ItemFullMessage( Item );
        */
        return FALSE;
    }
}    

void player::TakePickup( pickup& Pickup )
{
//  LOG_MESSAGE( "player::TakePickup", "" );

    ItemAcquiredMessage(Pickup.GetItem());

    if( m_bDead )
    {
        LOG_WARNING( "player::TakePickup", "A dead player got a pickup!" );
        return;
    }

    inven_item Item   =      Pickup.GetItem();
    s32        Amount = (s32)Pickup.GetAmount();

#ifndef X_EDITOR
    if( !g_NetworkMgr.IsServer() )
    {
        if( (Item == INVEN_WEAPON_MESON_CANNON) ||
            (Item == INVEN_AMMO_MESON) )
        {
            DirtyAmmo();
        }
    }
#endif

    // **WEAPONS**
    // The only time you CAN'T take a weapon pickup is when you already have
    // the weapon AND you already have the maximum ammo allowed.
    if( IN_RANGE( INVEN_WEAPON_FIRST, Item, INVEN_WEAPON_LAST ) )
    {
        if( (Item == INVEN_WEAPON_SMP) &&
            (m_Inventory2.GetAmount( Item ) == 1.0f) )
        {
            Item = INVEN_WEAPON_DUAL_SMP;
        }
        else
        if( (Item == INVEN_WEAPON_SHOTGUN) &&
                (m_Inventory2.GetAmount( Item ) == 1.0f) )
        {
            Item = INVEN_WEAPON_DUAL_SHT;
        }
        xbool bFirstTimePickedup = FALSE;
        
        // Take the weapon if there is space.
        if( m_Inventory2.GetAmount( Item ) < 1.0f )
        {
            // don't have this item, it's the first time we've grabbed it.
            bFirstTimePickedup = TRUE;

            m_Inventory2.SetAmount( Item, 1.0f );

            // set flag for dual weapon
            xbool bIsDual = (Item == INVEN_WEAPON_DUAL_SMP) || (Item == INVEN_WEAPON_DUAL_SHT );

            // we currently don't have one of these in our inventory
            if( ShouldSwitchToWeapon2( Item, TRUE ) )
            {
                // don't do state change if this is a dual weapon, we'll do it below
                SetNextWeapon2( Item, FALSE, !bIsDual );

                // override animation for dual pickup
                if( bIsDual )
                {
                    SetAnimState( ANIM_STATE_PICKUP );
                }
            }
            else
            {
                // be sure we change the pre-mutation weapon if we pick up a dual
                if( IsMutated() )
                {
                     // set dual as premutation weapon
                     if( bIsDual )
                     {
                         m_PreMutationWeapon2 = Item; 
                     }
                }
            }
        }

        new_weapon* pWeapon = GetWeaponPtr( Item );

        // Take the clip of ammo in the weapon if possible.
        {
            // set up dual weapons specially.
            if( pWeapon )
            {
                inven_item ParentItem = new_weapon::GetParentIDForDualWeapon( Item );

                // make sure the clip is full!
                if( ParentItem != INVEN_NULL )
                {
                    // dual weapon's clip is full... give ammo to parent weapon
                    if( (pWeapon->GetAmmoCount() >= pWeapon->GetAmmoPerClip()) && !bFirstTimePickedup )
                    {
                        new_weapon *pParentWeapon = GetWeaponPtr( ParentItem );

                        // add the ammo to the weapon
                        s32 GunAmmo = pParentWeapon->GetAmmoPerClip( new_weapon::AMMO_PRIMARY );

                        // add ammo to parent item's reserves
                        AddAmmo2( ParentItem, GunAmmo );

                        // make counts match up
                        pWeapon->SetupDualAmmo( ParentItem );
                    }
                    else
                    {
                        // we ran over a dual capable weapon, put the ammo in the dual clip.
                        pWeapon->SetupDualAmmo( ParentItem );
                    }
                }               
                else
                {
                    // add the ammo to the weapon
                    s32 GunAmmo = pWeapon->GetAmmoPerClip( new_weapon::AMMO_PRIMARY );

#ifndef X_EDITOR
                    // KSS -- FIXME -- HACK FOR LAN PARTY
                    if( GameMgr.IsGameMultiplayer() )
                    {
                        GunAmmo = pWeapon->m_WeaponAmmo[new_weapon::AMMO_PRIMARY].m_AmmoMax;
                    }
#endif              
                    // we ran over a dual capable weapon, put the ammo in the dual clip.
                    pWeapon->AddAmmoToWeapon( GunAmmo, 0 );                    

                    // if you've died and picked up the weapon again, reload it
                    if( bFirstTimePickedup )
                    {
                        s32 count = pWeapon->GetAmmoCount();
                        if( count <= 0 )
                        {                            
                            // reload without anim                            
                            pWeapon->RefillClip(new_weapon::AMMO_PRIMARY);

                            return;
                        }
                    }
                }

                // be sure we play reload anim if gun is completely empty
                s32 count = pWeapon->GetAmmoCount();
                if( count <= 0 )
                {
                    // reload with anim
                    ReloadWeapon(new_weapon::AMMO_PRIMARY);
                }
            }
            else
            {
                ASSERTS(FALSE, xfs("Weapon %s missing from blueprint bag", GetInventory2().ItemToName(Item)) );
            }
        }

        return;
    }

    // **EVERYTHING ELSE**
    switch( Item )
    {
    case INVEN_HEALTH:
        m_Inventory2.SetAmount( Item, 0.0f );
        #ifndef X_EDITOR
        if( g_NetworkMgr.IsServer() )
        #endif
        {
            AddHealth( (f32)Amount );
        }
        break;

    case INVEN_MUTAGEN:
    case INVEN_MUTAGEN_CORPSE:
        m_Inventory2.SetAmount( Item, 0.0f );
        AddMutagen( (f32)Amount );
        break;

    case INVEN_AMMO_SMP:
        AddAmmo2( INVEN_WEAPON_SMP, Amount );
        break;
    case INVEN_AMMO_SHOTGUN:
        AddAmmo2( INVEN_WEAPON_SHOTGUN, Amount );
        break;
    case INVEN_AMMO_DESERT_EAGLE:
        AddAmmo2( INVEN_WEAPON_DESERT_EAGLE, Amount );
        break;
    case INVEN_AMMO_SNIPER_RIFLE:
        AddAmmo2( INVEN_WEAPON_SNIPER_RIFLE, Amount );
        break;
    case INVEN_AMMO_MESON:
        AddAmmo2( INVEN_WEAPON_MESON_CANNON, Amount );
        break;

    case INVEN_AMMO_BBG:
        AddAmmo2( INVEN_WEAPON_BBG, Amount );
        break;

    // TODO - Handle ammo boxes (which have multiple ammo types within).

    default:
        // Should this be here?  Should we just ASSERT( FALSE )?
        m_Inventory2.AddAmount( Item, (f32)Amount );
        break;
    }
}

xbool player::OnPickup( pickup& Pickup )
{
//  LOG_MESSAGE( "player::OnPickup", "" );

    // On the server:
    //  - First see if you want the pickup.
    //  - If so, take it.
    //
    // On a client:
    //  - First see if you want the pickup.
    //  - If so, tell the server "I want this pickup".
    //  - Server: If pickup not already taken, "You can have it".
    //  - Client: Take it.
    //  + This is further complicated by health pickups which must be handled
    //    by the ghost on the server side.

    if( !CanTakePickup( Pickup ) )
        return( FALSE );

#ifndef X_EDITOR
    if( g_NetworkMgr.IsServer() )
#endif
    {
        TakePickup( Pickup );
        return( TRUE );
    }
#ifndef X_EDITOR
    else
    {
        ASSERT( g_NetworkMgr.IsClient() );
        net_WantPickup( Pickup );
        return( TRUE );
    }
#endif

    // Should never be able to get here, but added the following line to keep
    // the compiler happy.  (An unhappy compiler is a bad thing!)
    return( FALSE );
}

void player::DebugSetupInventory( const char* pLevelName )
{
    // KSS -- TO ADD NEW WEAPON
    s32 Weapons = GetStartWeaponsForLevel( pLevelName );

    // Give the player some weapons
    if( Weapons & WB_SMP )
        m_Inventory2.SetAmount( INVEN_WEAPON_SMP, 1.0f );
    if( Weapons & WB_SG )
        m_Inventory2.SetAmount( INVEN_WEAPON_SHOTGUN, 1.0f );
    if( Weapons & WB_DE )
        m_Inventory2.SetAmount( INVEN_WEAPON_DESERT_EAGLE, 1.0f );
    if( Weapons & WB_SR )
        m_Inventory2.SetAmount( INVEN_WEAPON_SNIPER_RIFLE, 1.0f );
    if( Weapons & WB_MC )
        m_Inventory2.SetAmount( INVEN_WEAPON_MESON_CANNON, 1.0f );
    if( Weapons & WB_BBG )
        m_Inventory2.SetAmount( INVEN_WEAPON_BBG, 1.0f );
    if( (Weapons & WB_MM) || (Weapons & WB_MP) )
        m_Inventory2.SetAmount( INVEN_WEAPON_MUTATION, 1.0f );
    if( Weapons & WB_FG )
        m_Inventory2.SetAmount( INVEN_GRENADE_FRAG, 5.0f );
    if( Weapons & WB_JBG )
        m_Inventory2.SetAmount( INVEN_GRENADE_JBEAN, 5.0f );
    if( Weapons & WB_SCN )
        m_Inventory2.SetAmount( INVEN_WEAPON_SCANNER, 1.0f );

    m_CurrentWeaponItem = GetEquipedWeaponForLevel( pLevelName );
}

#ifdef X_EDITOR

void player::EditorPreGame( void )
{
    actor::EditorPreGame();

    CLOG_MESSAGE( s_LogEditorInventory, xfs("player::EditorPreGame %08x",(u32)this), "Start creating templates" );

    // Clear weapon guids
    x_memset( &m_WeaponGuids, 0, sizeof(m_WeaponGuids) );

    // Clear inventory
    m_Inventory2.Clear();

    s32 WeaponsForLevel = GetAvailableWeaponsForLevel( (const char*)g_Project.m_DFSName );

    // Here is where we create the inventory items that are specified for each inventory in the editor.
    for( s32 i=0; i<INVEN_NUM_WEAPONS; i++ )
    {
        inven_item WeaponItem = inventory2::WeaponIndexToItem(i);

        xbool DoCreateWeapon = TRUE;

        switch( WeaponItem )
        {
        case INVEN_WEAPON_SMP:
            DoCreateWeapon = WeaponsForLevel & WB_SMP;
            break;
        case INVEN_WEAPON_SHOTGUN:
            DoCreateWeapon = WeaponsForLevel & WB_SG;
            break;
        case INVEN_WEAPON_SNIPER_RIFLE:
            DoCreateWeapon = WeaponsForLevel & WB_SR;
            break;
        case INVEN_WEAPON_DESERT_EAGLE:
            DoCreateWeapon = WeaponsForLevel & WB_DE;
            break;
        case INVEN_WEAPON_MESON_CANNON:
            DoCreateWeapon = WeaponsForLevel & WB_MC;
            break;
        case INVEN_WEAPON_DUAL_SMP:
            DoCreateWeapon = WeaponsForLevel & WB_SMP;
            break;

        case INVEN_WEAPON_DUAL_SHT:
            DoCreateWeapon = WeaponsForLevel & WB_SG;
            break;

        // KSS -- TO ADD NEW WEAPON
        case INVEN_WEAPON_BBG:
            DoCreateWeapon = WeaponsForLevel & WB_BBG;
            break;

        case INVEN_WEAPON_SCANNER:
            DoCreateWeapon = WeaponsForLevel & WB_SCN;
            break;

        //case INVEN_WEAPON_DUAL_EAGLE:
        //    DoCreateWeapon = WeaponsForLevel & WB_DE;
        //    break;
        case INVEN_WEAPON_MUTATION:
            DoCreateWeapon = WeaponsForLevel & WB_MM;
            break;
        }

        if( DoCreateWeapon )
        {
            const char* pBlueprintName = inventory2::ItemToBlueprintName( WeaponItem );
            if( pBlueprintName )
            {
                guid Guid = g_TemplateMgr.EditorCreateSingleTemplateFromPath( pBlueprintName, vector3(0,0,0), radian3(0,0,0), -1, -1 ); 

                CLOG_MESSAGE( s_LogEditorInventory, xfs("player::EditorPreGame %08x",(u32)this), "EditorCreateSingleTemplateFromPath '%s' %08x:%08x", pBlueprintName, Guid.GetHigh(), Guid.GetLow() );

                if( Guid )
                {
                    new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( Guid );
                    ASSERT( pWeapon );
                    pWeapon->InitWeapon( GetPosition(), new_weapon::RENDER_STATE_PLAYER, GetGuid() );
                    OnWeaponAnimInit2( WeaponItem, pWeapon );
                    pWeapon->SetupRenderInformation( );
                    pWeapon->SetAnimation( ANIM_STATE_IDLE, 0.0f );
                    pWeapon->BeginIdle();
                }

                m_WeaponGuids[i] = Guid;
            }
        }
        else
        {
            m_WeaponGuids[i] = 0;
        }
    }

    CLOG_MESSAGE( s_LogEditorInventory, xfs("player::EditorPreGame %08x",(u32)this), "Done creating templates" );

    DebugEnableWeapons( (const char*)g_Project.m_DFSName );

    m_WeaponsCreated = TRUE;
}

#endif // X_EDITOR

void player::InitInventory( void )
{
    if( !m_WeaponsCreated )
    {
        CreateAllWeaponObjects();
        m_WeaponsCreated = TRUE;
#ifdef X_EDITOR
        DebugEnableWeapons( "<null>" );
#else
    #if (!CONFIG_IS_DEMO)
        if( g_StateMgr.UseDefaultLoadOut() )
    #endif
        {
            // Clear inventory
            m_Inventory2.Clear();
            DebugEnableWeapons( g_ActiveConfig.GetLevelPath() );
            g_StateMgr.DisableDefaultLoadOut();
        }
#endif
    }
}

void player::ForceNextWeapon( void )
{
    // Set previous weapon and current weapon, clear next weapon
    m_PrevWeaponItem     = m_CurrentWeaponItem;
    m_CurrentWeaponItem  = m_NextWeaponItem;
    m_NextWeaponItem     = INVEN_NULL;

    // zero out the reticle radius
    m_ReticleRadius                          = 0.0f;
    m_ReticleGrowSpeed                       = 0.0f;
    m_AimAssistData.bReticleOn               = FALSE;
    m_AimAssistData.ReticleEnemyGuid         = 0;
    m_AimAssistData.OnlineFriendlyTargetGuid = 0;

    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon )
    {
        pWeapon->SetupRenderInformation( );
        pWeapon->SetRotation( m_AnimPlayer.GetPitch() , m_AnimPlayer.GetYaw() );
        OnMoveWeapon();

        // Bring the new weapon up.
        SetAnimState( ANIM_STATE_SWITCH_TO );

        #ifndef X_EDITOR
        m_NetDirtyBits |= WEAPON_BIT;  // NETWORK
        #endif // X_EDITOR
    }
    else
    {
        SetAnimState( ANIM_STATE_UNDEFINED );
    }

}

static s32 GetAvailableWeaponsForLevel( const char* pLevelName )
{
    // KSS -- TO ADD NEW WEAPON
    // Determine the weapons a player start with and the start item
    s32 Weapons = WB_SMP | WB_DE | WB_SG | WB_SR | WB_MC | WB_MM | WB_MP | WB_FG | WB_BBG | WB_JBG | WB_SCN;
    for( s32 i=0 ; s_MapToWeaponTable[i].pLevelName ; i++ )
    {
        if( x_stristr( pLevelName, s_MapToWeaponTable[i].pLevelName ) )
        {
            Weapons     = s_MapToWeaponTable[i].AvailableWeapons;
            break;
        }
    }

    return Weapons;
}

//=========================================================================

void player::UpdateJBGLoreUnlock( void )
{
#ifndef X_EDITOR
    if( GetJBGLoreAcquired() )
    {
        return;
    }

    player_profile& Profile =
        g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( 0 ) );

    for( s32 i = 0; i < (s32)(sizeof( s_JBGLoreEntries ) / sizeof( s_JBGLoreEntries[0] )); i++ )
    {
        s32 VaultIndex;
        const s32 MapID = s_bUseTestMap ? s_ScannerTestMap : s_JBGLoreEntries[i].MapID;
        g_LoreList.GetVaultByMapID( MapID, VaultIndex );

        if( Profile.GetLoreAcquired( VaultIndex, s_JBGLoreEntries[i].LoreIndex ) )
        {
            m_bJBGLoreAcquired = TRUE;
            break;
        }
    }
#endif
}

//=========================================================================

void player::UpdateCurrentGrenade( void )
{
    xbool UseJumpingBean = GetJBGLoreAcquired();

#ifndef X_EDITOR
    if( GameMgr.GetGameType() == GAME_CAMPAIGN )
    {
        UseJumpingBean |= (g_ActiveConfig.GetLevelID() == 1030);
    }
    else
    {
        UseJumpingBean = TRUE;
    }
#endif

    const xbool OutOfFrag = (m_Inventory2.GetAmount( INVEN_GRENADE_FRAG ) <= 0);
    const xbool HasJumpingBean = (m_Inventory2.GetAmount( INVEN_GRENADE_JBEAN ) > 0);

#ifndef CONFIG_RETAIL
    UseJumpingBean |= DEBUG_EXPERT_JUMPINGBEAN;
#endif

    m_CurrentGrenadeType2 = (HasJumpingBean && (UseJumpingBean || OutOfFrag))
                          ? INVEN_GRENADE_JBEAN
                          : INVEN_GRENADE_FRAG;
}
