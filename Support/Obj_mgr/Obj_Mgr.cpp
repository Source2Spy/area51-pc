//==============================================================================
//
//  ObjectMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "x_stdio.hpp"
#include "Entropy.hpp"
#include "Objects/Player/Player.hpp"
#include "Objects/Projector.hpp"
#include "Render/Render.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"
#include "GameLib/RenderContext.hpp"
#include "ManagerRegistration.hpp"
#include "CollisionMgr/CollisionMgr.hpp"
#include "CollisionMgr/PolyCache.hpp"
#include "PlaySurfaceMgr/PlaySurfaceMgr.hpp"
#include "FX/fx_Mgr.hpp"
#include "../Support/GameLib/StatsMgr.hpp"
#include "../Support/Tracers/TracerMgr.hpp"
#include "../Support/Decals/DecalMgr.hpp"
#include "Objects/ProxyPlaySurface.hpp"
#include "Objects/LevelSettings.hpp"
#include "Objects/Render/PostEffectMgr.hpp"
#include "Objects/Camera.hpp"
#include "Objects/NewWeapon.hpp"
#include "Objects/Pip.hpp"
#include "ZoneMgr/ZoneMgr.hpp"
#include "Navigation/Nav_Map.hpp"
#include "PhysicsMgr/PhysicsMgr.hpp"
#include "OccluderMgr/OccluderMgr.hpp"

#ifdef X_EDITOR
extern xbool g_EditorShowNameFlag;
#endif

#if !defined(X_RETAIL) && 0
#define DO_SANITY_CHECK()   SanityCheck();
#else
#define DO_SANITY_CHECK()
#endif

#if defined(cgalley)
#define LOGGING_ENABLED 1
#else
#define LOGGING_ENABLED 0
#endif

//==============================================================================
//  Special structures for sorting transparent objects by distance
//==============================================================================

struct special_render_obj
{
    object* pObject;    // only valid during a single call to render!
    f32     ZDist;      // only valid during a single call to render!
};

//==============================================================================
//  Global declarations
//==============================================================================

obj_mgr                    g_ObjMgr;   
xarray<special_render_obj> g_SpecialRenderObj;

// Initialize static variable
object_desc* object_desc::s_pHead = NULL;

//==============================================================================
//  OBJECT MANAGER FUNCTIONS
//==============================================================================

obj_mgr::obj_mgr( void ) :
    m_InLoop( FALSE )
{
    // To ensure it's singleton nature is not violated
    static xbool AlreadyCreated = FALSE;
    ASSERT( !AlreadyCreated);
    AlreadyCreated      = TRUE;
    m_GameTime          = 0;
    m_SimulationTimeSeconds = 0.0;
    m_pProxyPlaySurface = NULL;

    m_nLogicLoops = 0;

    m_TrapBeforeLogicGuid   = 0;
    m_TrapAfterLogicGuid    = 0;
    m_TrapBeforeRenderGuid  = 0;
    m_bTrapOnCreate         = FALSE;
    m_bTrapOnDestroy        = FALSE;

    m_SafeBBox.Min = vector3(-F32_MAX,-F32_MAX,-F32_MAX);
    m_SafeBBox.Max = vector3(+F32_MAX,+F32_MAX,+F32_MAX);

    m_ReservedGuid = 0;
    m_VisibleSlotCount = 0;
}

//==============================================================================

obj_mgr::~obj_mgr( void )
{
    // Assert that there are no objects left or not needed because it's a singleton
}

//==============================================================================

void obj_mgr::Kill( void )
{
    Clear();
}

//==============================================================================

void obj_mgr::Init( void )
{
    LOG_MESSAGE( "obj_mgr::Init", "" );

    s32 i;

    // Setup guid lookup capacity to number of objects
    //m_GuidLookup.SetCapacity( MAX_OBJECTS, FALSE );
    DO_SANITY_CHECK();

    m_FirstSearchResult = MAX_OBJECTS;
    m_VisibleSlotCount = 0;

    //
    // Clear type lists for all info ptrs
    //

    // Fist make sure that all the resource descriptions are clear
    for( i=0; i<(s32)object::TYPE_END_OF_LIST; i++ )
    {
        m_ObjectType[i].pDesc           = NULL;
    }

    // Now start linking all the resoruce descriptions
    for( i=0; i<(s32)object::TYPE_END_OF_LIST; i++ )
    {
        // Collect all the types for this group
        s32             ObjType = i;
        object_desc*    pNext   = object_desc::s_pHead;
        for(; pNext; pNext = pNext->m_pNext )
        {
            if( pNext->m_Type == (object::type)ObjType )
            {
                pNext->m_pNextGroupType = m_ObjectType[i].pDesc;
                m_ObjectType[i].pDesc   = pNext;
            }
        }
        
        // Set the rest of the info
        m_ObjectType[i].FirstType       = SLOT_NULL;
        m_ObjectType[i].FirstVis        = SLOT_NULL;
        m_ObjectType[i].InstanceCount   = 0;
    }

    // Hook all Slots into free list
    m_FirstFreeSlot = SLOT_NULL;

    // just walk through the whole array and set everything up
    for( i=0; i<MAX_OBJECTS; i++ )
    {
        m_ObjectSlot[i].pObject           = NULL;
        m_ObjectSlot[i].Next              = m_FirstFreeSlot;
        m_ObjectSlot[i].Prev              = SLOT_NULL;
        m_ObjectSlot[i].NextSearch        = SLOT_NULL;
        m_ObjectSlot[i].NextVis           = SLOT_NULL;
        m_ObjectSlot[i].NextRenderable[0] = SLOT_NULL;
        m_ObjectSlot[i].PrevRenderable[0] = SLOT_NULL;
        m_ObjectSlot[i].NextRenderable[1] = SLOT_NULL;
        m_ObjectSlot[i].PrevRenderable[1] = SLOT_NULL;
        m_ObjectSlot[i].NextZone[0]       = SLOT_NULL;
        m_ObjectSlot[i].PrevZone[0]       = SLOT_NULL;
        m_ObjectSlot[i].NextZone[1]       = SLOT_NULL;
        m_ObjectSlot[i].PrevZone[1]       = SLOT_NULL;

        for( s32 j=0; j<8; j++ )
        {
            m_ObjectLink[((link_id)i)*8+j].SpacialCellID = SPATIAL_CELL_NULL;
            m_ObjectLink[((link_id)i)*8+j].Next          = LINK_NULL;
            m_ObjectLink[((link_id)i)*8+j].Prev          = LINK_NULL;
        }

        if( m_FirstFreeSlot != SLOT_NULL )
            m_ObjectSlot[ m_FirstFreeSlot ].Prev = i;

        m_FirstFreeSlot = i;
    }

    // walk through zones and set them up
    for( i = 0; i < 256; i++ )
    {
        m_ObjectZone[i].FirstZone[0]       = SLOT_NULL;
        m_ObjectZone[i].FirstZone[1]       = SLOT_NULL;
        m_ObjectZone[i].FirstRenderable[0] = SLOT_NULL;
        m_ObjectZone[i].FirstRenderable[1] = SLOT_NULL;
    }

    //
    // Register all the objects into the system
    //
    {
        for( object_desc* pNext = object_desc::s_pHead; pNext != NULL; pNext = pNext->m_pNext )
        {
            g_RegGameMgrs.AddManager( xfs("Object Manager\\%s", pNext->m_pTypeName ), pNext );
        }
    }

}

//==============================================================================

void obj_mgr::AddSlotToZone( slot_id SlotID )
{
    ASSERT( (m_ObjectSlot[SlotID].NextZone[0] == SLOT_NULL) &&
            (m_ObjectSlot[SlotID].PrevZone[0] == SLOT_NULL) );
    ASSERT( (m_ObjectSlot[SlotID].NextZone[1] == SLOT_NULL) &&
            (m_ObjectSlot[SlotID].PrevZone[1] == SLOT_NULL) );

    object* pObject = m_ObjectSlot[SlotID].pObject;
    u16     Zone1   = pObject->GetZone1();
    u16     Zone2   = pObject->GetZone2();

    m_ObjectSlot[SlotID].NextZone[0] = m_ObjectZone[Zone1].FirstZone[0];
    m_ObjectSlot[SlotID].PrevZone[0] = SLOT_NULL;
    if ( m_ObjectZone[Zone1].FirstZone[0] != SLOT_NULL )
        m_ObjectSlot[m_ObjectZone[Zone1].FirstZone[0]].PrevZone[0] = SlotID;
    m_ObjectZone[Zone1].FirstZone[0] = SlotID;

    if( Zone2 != 0 )
    {
        m_ObjectSlot[SlotID].NextZone[1] = m_ObjectZone[Zone2].FirstZone[1];
        m_ObjectSlot[SlotID].PrevZone[1] = SLOT_NULL;
        if ( m_ObjectZone[Zone2].FirstZone[1] != SLOT_NULL )
            m_ObjectSlot[m_ObjectZone[Zone2].FirstZone[1]].PrevZone[1] = SlotID;
        m_ObjectZone[Zone2].FirstZone[1] = SlotID;
    }
}

//==============================================================================

void obj_mgr::AddSlotToRenderable( slot_id SlotID )
{
    ASSERT( (m_ObjectSlot[SlotID].NextRenderable[0] == SLOT_NULL) &&
            (m_ObjectSlot[SlotID].PrevRenderable[0] == SLOT_NULL) );
    ASSERT( (m_ObjectSlot[SlotID].NextRenderable[1] == SLOT_NULL) &&
            (m_ObjectSlot[SlotID].PrevRenderable[1] == SLOT_NULL) );

    object* pObject = m_ObjectSlot[SlotID].pObject;
    u16     Zone1   = pObject->GetZone1();
    u16     Zone2   = pObject->GetZone2();

    m_ObjectSlot[SlotID].NextRenderable[0] = m_ObjectZone[Zone1].FirstRenderable[0];
    m_ObjectSlot[SlotID].PrevRenderable[0] = SLOT_NULL;
    if ( m_ObjectZone[Zone1].FirstRenderable[0] != SLOT_NULL )
        m_ObjectSlot[m_ObjectZone[Zone1].FirstRenderable[0]].PrevRenderable[0] = SlotID;
    m_ObjectZone[Zone1].FirstRenderable[0] = SlotID;

    if( Zone2 != 0 )
    {
        m_ObjectSlot[SlotID].NextRenderable[1] = m_ObjectZone[Zone2].FirstRenderable[1];
        m_ObjectSlot[SlotID].PrevRenderable[1] = SLOT_NULL;
        if ( m_ObjectZone[Zone2].FirstRenderable[1] != SLOT_NULL )
            m_ObjectSlot[m_ObjectZone[Zone2].FirstRenderable[1]].PrevRenderable[1] = SlotID;
        m_ObjectZone[Zone2].FirstRenderable[1] = SlotID;
    }
}

//==============================================================================

void obj_mgr::RemoveSlotFromZone( slot_id SlotID )
{
    object* pObject = m_ObjectSlot[SlotID].pObject;
    u16     Zone1   = pObject->GetZone1();
    u16     Zone2   = pObject->GetZone2();

    slot_id Next = m_ObjectSlot[SlotID].NextZone[0];
    slot_id Prev = m_ObjectSlot[SlotID].PrevZone[0];
    if( Next != SLOT_NULL )
        m_ObjectSlot[Next].PrevZone[0] = m_ObjectSlot[SlotID].PrevZone[0];
    if( Prev != SLOT_NULL )
        m_ObjectSlot[Prev].NextZone[0] = m_ObjectSlot[SlotID].NextZone[0];
    if( SlotID == m_ObjectZone[Zone1].FirstZone[0] )
        m_ObjectZone[Zone1].FirstZone[0] = m_ObjectSlot[SlotID].NextZone[0];

    Next = m_ObjectSlot[SlotID].NextZone[1];
    Prev = m_ObjectSlot[SlotID].PrevZone[1];
    if( Next != SLOT_NULL )
        m_ObjectSlot[Next].PrevZone[1] = m_ObjectSlot[SlotID].PrevZone[1];
    if( Prev != SLOT_NULL )
        m_ObjectSlot[Prev].NextZone[1] = m_ObjectSlot[SlotID].NextZone[1];
    if( SlotID == m_ObjectZone[Zone2].FirstZone[1] )
        m_ObjectZone[Zone2].FirstZone[1] = m_ObjectSlot[SlotID].NextZone[1];

    m_ObjectSlot[SlotID].NextZone[0] = SLOT_NULL;
    m_ObjectSlot[SlotID].PrevZone[0] = SLOT_NULL;
    m_ObjectSlot[SlotID].NextZone[1] = SLOT_NULL;
    m_ObjectSlot[SlotID].PrevZone[1] = SLOT_NULL;
}

//==============================================================================

void obj_mgr::RemoveSlotFromRenderable( slot_id SlotID )
{
    object* pObject = m_ObjectSlot[SlotID].pObject;
    u16     Zone1   = pObject->GetZone1();
    u16     Zone2   = pObject->GetZone2();

    slot_id Next = m_ObjectSlot[SlotID].NextRenderable[0];
    slot_id Prev = m_ObjectSlot[SlotID].PrevRenderable[0];
    if( Next != SLOT_NULL )
        m_ObjectSlot[Next].PrevRenderable[0] = m_ObjectSlot[SlotID].PrevRenderable[0];
    if( Prev != SLOT_NULL )
        m_ObjectSlot[Prev].NextRenderable[0] = m_ObjectSlot[SlotID].NextRenderable[0];
    if( SlotID == m_ObjectZone[Zone1].FirstRenderable[0] )
        m_ObjectZone[Zone1].FirstRenderable[0] = m_ObjectSlot[SlotID].NextRenderable[0];

    Next = m_ObjectSlot[SlotID].NextRenderable[1];
    Prev = m_ObjectSlot[SlotID].PrevRenderable[1];
    if( Next != SLOT_NULL )
        m_ObjectSlot[Next].PrevRenderable[1] = m_ObjectSlot[SlotID].PrevRenderable[1];
    if( Prev != SLOT_NULL )
        m_ObjectSlot[Prev].NextRenderable[1] = m_ObjectSlot[SlotID].NextRenderable[1];
    if( SlotID == m_ObjectZone[Zone2].FirstRenderable[1] )
        m_ObjectZone[Zone2].FirstRenderable[1] = m_ObjectSlot[SlotID].NextRenderable[1];

    m_ObjectSlot[SlotID].NextRenderable[0] = SLOT_NULL;
    m_ObjectSlot[SlotID].PrevRenderable[0] = SLOT_NULL;
    m_ObjectSlot[SlotID].NextRenderable[1] = SLOT_NULL;
    m_ObjectSlot[SlotID].PrevRenderable[1] = SLOT_NULL;
}

//==============================================================================

slot_id obj_mgr::AllocSlot( void )
{
#ifdef X_DEBUG
    if( m_FirstFreeSlot == SLOT_NULL )
        DisplayStats();
#endif

    // Find slot for object
    ASSERT( m_FirstFreeSlot != SLOT_NULL );

    slot_id Slot = m_FirstFreeSlot;

    ASSERT(m_FirstFreeSlot != m_ObjectSlot[Slot].Next);

    m_FirstFreeSlot = m_ObjectSlot[Slot].Next;
    m_ObjectSlot[Slot].Next              = SLOT_NULL;
    m_ObjectSlot[Slot].Prev              = SLOT_NULL;
    m_ObjectSlot[Slot].NextRenderable[0] = SLOT_NULL;
    m_ObjectSlot[Slot].PrevRenderable[0] = SLOT_NULL;
    m_ObjectSlot[Slot].NextRenderable[1] = SLOT_NULL;
    m_ObjectSlot[Slot].PrevRenderable[1] = SLOT_NULL;
    m_ObjectSlot[Slot].NextZone[0]       = SLOT_NULL;
    m_ObjectSlot[Slot].PrevZone[0]       = SLOT_NULL;
    m_ObjectSlot[Slot].NextZone[1]       = SLOT_NULL;
    m_ObjectSlot[Slot].PrevZone[1]       = SLOT_NULL;
    if( m_FirstFreeSlot != SLOT_NULL )
        m_ObjectSlot[m_FirstFreeSlot].Prev = SLOT_NULL;

    return Slot;
}

//==============================================================================

object* obj_mgr::CreateObject( const object_desc& Desc, slot_id Slot )
{
    object* pObject;
    s32     Type;
    
    //MEMORY_OWNER( xfs( "obj_mgr::CreateObject( %s )", Desc.GetTypeName() ) );
    MEMORY_OWNER( xfs( "OBJECT DATA '%s'", Desc.GetTypeName() ) );

    ASSERT( Slot != SLOT_NULL && Slot < MAX_OBJECTS );
    DO_SANITY_CHECK();

    Type = Desc.m_Type;
    ASSERT( IN_RANGE( 0, (s32)Type, (s32)object::TYPE_END_OF_LIST-1 ) );

//  ASSERT( m_ObjectType[ Type ].InstanceCount < 1000 );  // Sanity check.

    //
    // Add slot to type list
    //
    if( m_ObjectType[ Type ].FirstType != SLOT_NULL )
        m_ObjectSlot[ m_ObjectType[Type].FirstType ].Prev = Slot;

    m_ObjectSlot[ Slot ].Next              = m_ObjectType[Type].FirstType;
    m_ObjectSlot[ Slot ].Prev              = SLOT_NULL;
    m_ObjectSlot[ Slot ].NextRenderable[0] = SLOT_NULL;
    m_ObjectSlot[ Slot ].PrevRenderable[0] = SLOT_NULL;
    m_ObjectSlot[ Slot ].NextRenderable[1] = SLOT_NULL;
    m_ObjectSlot[ Slot ].PrevRenderable[1] = SLOT_NULL;
    m_ObjectSlot[ Slot ].NextZone[0]       = SLOT_NULL;
    m_ObjectSlot[ Slot ].PrevZone[0]       = SLOT_NULL;
    m_ObjectSlot[ Slot ].NextZone[1]       = SLOT_NULL;
    m_ObjectSlot[ Slot ].PrevZone[1]       = SLOT_NULL;
    m_ObjectType[ Type ].FirstType = Slot;
    m_ObjectType[ Type ].InstanceCount++;

    //
    // Find the right object description to create the object
    //        
    {
        ASSERT( m_ObjectType[ Type ].pDesc );
        object_desc& SaveDesc = const_cast<object_desc&>( Desc );
        pObject = SaveDesc.Create();
        SaveDesc.AddObjectCount( 1 );
        ASSERT( pObject );

        // Inser the object in its slot
        m_ObjectSlot[Slot].pObject = pObject;
    }

    //
    // Clear links
    //
    for( s32 i=0; i<8; i++ )
    {
        m_ObjectLink[((link_id)Slot)*8+i].SpacialCellID = SPATIAL_CELL_NULL;
        m_ObjectLink[((link_id)Slot)*8+i].Next          = LINK_NULL;
        m_ObjectLink[((link_id)Slot)*8+i].Prev          = LINK_NULL;
    }

    // Init base values in object
    // TODO: This has to go
    pObject->m_SlotID       = Slot;

    //
    // Add the slot to the zone list
    //
    AddSlotToZone( Slot );
    if( pObject->GetAttrBits() & object::ATTR_RENDERABLE )
    {
        AddSlotToRenderable( Slot );
    }

    //
    // Fill out debug info for object
    //
#if defined( USE_OBJECT_DEBUGINFO )
    pObject->m_DebugInfo.m_pDesc = (object_desc*)&Desc;
#endif

    DO_SANITY_CHECK();

    return pObject;
}

//==============================================================================

void obj_mgr::CreateObject( const object_desc& Desc, guid Guid )
{
    ASSERT( Guid != 0 );

    slot_id Slot    = AllocSlot();
    object* pObject = CreateObject( Desc, Slot );

    // Add the slot in the hash table
    if( m_GuidLookup.GetCapacity() == 0 )
    {
        m_GuidLookup.SetCapacity( MAX_OBJECTS, FALSE );
    }

    m_GuidLookup.Add( Guid, (s32)Slot );

    // Initialize a few things for the object
    pObject->m_Guid         = Guid;
    pObject->SetAttrBits( Desc.m_CommonAttrBits );

    // Trap if we are watching for creations of this object type
    if( Desc.m_bTrapOnCreate || m_bTrapOnCreate )
        BREAK;

    // Notify the object that he need to initialize
    pObject->OnInit();

    // Insert the object into the data base if we have to
    if( pObject->GetAttrBits() & object::ATTR_SPACIAL_ENTRY )
    {
        AddToSpatialDBase( Slot );
    }
}
        
//==============================================================================

void obj_mgr::ReserveGuid( guid Guid )
{
    ASSERT( m_ReservedGuid == 0 );
    m_ReservedGuid = Guid;
}

//==============================================================================

guid obj_mgr::CreateObject( const object_desc& Desc )
{
    guid Guid;
    if( m_ReservedGuid )
    {
        Guid = m_ReservedGuid;
        m_ReservedGuid = 0;
    }
    else
    {
        Guid = guid_New();
    }
    CreateObject( Desc, Guid );
    return Guid;
}

//==============================================================================

guid obj_mgr::CreateObject( const char* pObjectTypeName )
{
    if( pObjectTypeName == NULL )
        x_throw( "Unkown Type" );

    for( object_desc* pNext = object_desc::s_pHead; pNext; pNext = pNext->m_pNext )
    {
        if( x_stricmp( pNext->m_pTypeName, pObjectTypeName ) == 0 )
            return CreateObject( *pNext );
    }

//    ASSERT(FALSE);
    return guid(0);      // FIXME:  Should never get here but need a better way to handle the error
}

//==============================================================================

const object_desc* obj_mgr::GetDescFromName( const char* pObjectTypeName )
{
    if( pObjectTypeName == NULL )
        x_throw( "Unkown Type" );

    for( object_desc* pNext = object_desc::s_pHead; pNext; pNext = pNext->m_pNext )
    {
        if( x_stricmp( pNext->m_pTypeName, pObjectTypeName ) == 0 )
            return( pNext );
    }

    ASSERT( FALSE );
    return( NULL );
}

//==============================================================================

void obj_mgr::CreateObject( const char* pObjectTypeName, guid Guid )
{
    if( pObjectTypeName == NULL )
        x_throw( "Unkown Type" );

#ifndef X_EDITOR
    CLOG_WARNING( LOGGING_ENABLED, "obj_mgr::CreateObject", "Creating object from string typename '%s'", pObjectTypeName );
#endif

    char pModifiedObjectTypeName[256];
    if ( !x_strcmp( pObjectTypeName, "Player All Strains" ) )
    {
        x_strcpy( pModifiedObjectTypeName, "Player" );
    }
    else
    {
        x_strcpy( pModifiedObjectTypeName, pObjectTypeName );
    }

    for( object_desc* pNext = object_desc::s_pHead; pNext; pNext = pNext->m_pNext )
    {
        if( x_strcmp( pNext->m_pTypeName, pModifiedObjectTypeName ) == 0 )
        {
            CreateObject( *pNext, Guid );
            return;
        }
    }

    x_throw( xfs("Unable to find the specified object type [%s]", pModifiedObjectTypeName ) );
}

//==============================================================================

void obj_mgr::ChangeObjectGuid( guid CurrentGuid, guid NewGuid )
{
    // Get object info
    s32     Slot    = GetSlotFromGuid( CurrentGuid ) ;
    object* pObject = GetObjectBySlot( Slot ) ;

    // Update?
    if (pObject)
    {
        // Update guid lookup
        m_GuidLookup.Del( CurrentGuid ) ;
        m_GuidLookup.Add( NewGuid, Slot ) ;

        // Update object
        pObject->m_Guid = NewGuid ;

        // Make sure we worked!
        ASSERT( GetObjectByGuid( NewGuid ) == pObject ) ;
    }
}

//==============================================================================

slot_id obj_mgr::GetSlotFromGuid( guid Guid )
{
    s32 Slot;

    if( m_GuidLookup.Find( Guid, Slot ) )
        return (slot_id)Slot;

    return SLOT_NULL;
}

//==============================================================================

void obj_mgr::UnlinkAndFreeObject( slot_id Slot )
{
    // make sure this guy is not in the spatial dbase anymore
    //RemoveFromSpatialDBase( Slot );

    // Remove from zone lists
    RemoveSlotFromZone( Slot );
    RemoveSlotFromRenderable( Slot );

    // Remove the obejct from memory
    m_ObjectSlot[Slot].pObject->GetTypeDesc().AddObjectCount( -1 );
    delete m_ObjectSlot[Slot].pObject;

    // Put slot in free list
    m_ObjectSlot[ Slot ].Next = m_FirstFreeSlot;
    m_ObjectSlot[ Slot ].Prev = SLOT_NULL;

    if( m_FirstFreeSlot != SLOT_NULL )
        m_ObjectSlot[ m_FirstFreeSlot ].Prev = Slot;

    m_FirstFreeSlot = Slot;

    // Clear node
    m_ObjectSlot[Slot].pObject = NULL;
}

//==============================================================================

void obj_mgr::EmptyDeleteObjectList( void )
{
    for( s32 i=0; i<m_DeleteObject.GetCount(); i++ )
    {
        UnlinkAndFreeObject( m_DeleteObject[i] );
    }

    m_DeleteObject.Clear();
}


//==============================================================================

void obj_mgr::DestroyObjectEx( guid Guid, xbool bRemoveNow )
{
    slot_id Slot = GetSlotFromGuid( Guid );

    // If the object does not exist, exit gracefully.
    if ( SLOT_NULL == Slot )
        return;    

    // Trap if we are watching for destroys of this object type
    const object_desc& Desc = *m_ObjectType[m_ObjectSlot[Slot].pObject->GetType()].pDesc;
    if( Desc.m_bTrapOnDestroy || m_bTrapOnDestroy )
        BREAK;

    m_ObjectType[m_ObjectSlot[Slot].pObject->GetType()].InstanceCount--;

    // Check type loop cursors
    RemoveFromSpatialDBase( Slot );

    // Tell object to remove
    m_ObjectSlot[Slot].pObject->OnKill();

    // Remove from type list
    if( m_ObjectType[ m_ObjectSlot[Slot].pObject->GetType() ].FirstType == Slot )
        m_ObjectType[ m_ObjectSlot[Slot].pObject->GetType() ].FirstType = m_ObjectSlot[Slot].Next;

    if( m_ObjectSlot[Slot].Prev != SLOT_NULL )
        m_ObjectSlot[ m_ObjectSlot[Slot].Prev ].Next = m_ObjectSlot[Slot].Next;

    if( m_ObjectSlot[Slot].Next != SLOT_NULL )
        m_ObjectSlot[ m_ObjectSlot[Slot].Next ].Prev = m_ObjectSlot[Slot].Prev;

    // Mark this object as it dead
    m_ObjectSlot[Slot].pObject->SetAttrBits( object::ATTR_DESTROY );

    // Remove GUID from lookup
    m_GuidLookup.Del( m_ObjectSlot[Slot].pObject->GetGuid() );

    if( bRemoveNow )
    {
        UnlinkAndFreeObject( Slot );
    }
    else
    {
        // Added in the list of objects that need to be deleted
        m_DeleteObject.Append( Slot );
    }
}

//==============================================================================

void obj_mgr::Clear( void )
{
    LOG_MESSAGE( "obj_mgr::Clear", "" );
    DO_SANITY_CHECK();

    s32 i;

    // Make sure to empty the list of deleted objects
    EmptyDeleteObjectList();

    // Remove the rest
    for( i = 0; i < MAX_OBJECTS; i++ )
    {
        object* pObject = m_ObjectSlot[i].pObject;
        if (pObject)
        {
            // Destroy
            DestroyObjectEx( m_ObjectSlot[i].pObject->GetGuid(), TRUE );
        }
    }

    // It is possible that calling DestroyObjectEx caused other objects
    // to be destroyed without doing it immediately. For example, the
    // muzzle flash that is attached to the gun could do this. So,
    // make sure our objects really are destroyed!
    EmptyDeleteObjectList();

    // the guid lookup should be completely clear, but just to be safe,
    // we'll clear it again
    m_GuidLookup.Clear();

    // Clear out the object types (shouldn't be necessary, and we'll assert on that!)
    for( i=0; i<(s32)object::TYPE_END_OF_LIST; i++ )
    {
#ifndef X_EDITOR
        ASSERT( m_ObjectType[i].FirstType == SLOT_NULL );
        ASSERT( m_ObjectType[i].InstanceCount == 0 );
#endif
        m_ObjectType[i].FirstType     = SLOT_NULL;
        m_ObjectType[i].FirstVis      = SLOT_NULL;
        m_ObjectType[i].InstanceCount = 0;
    }

    // Clear out the zone and renderable list (this shouldn't be necessary
    // if we've maintained our linked lists properly)
    for( i = 0; i < 256; i++ )
    {
        ASSERT( m_ObjectZone[i].FirstRenderable[0] == SLOT_NULL );
        ASSERT( m_ObjectZone[i].FirstRenderable[1] == SLOT_NULL );
        ASSERT( m_ObjectZone[i].FirstZone[0] == SLOT_NULL );
        ASSERT( m_ObjectZone[i].FirstZone[0] == SLOT_NULL );
        m_ObjectZone[i].FirstRenderable[0] = SLOT_NULL;
        m_ObjectZone[i].FirstRenderable[1] = SLOT_NULL;
        m_ObjectZone[i].FirstZone[0] = SLOT_NULL;
        m_ObjectZone[i].FirstZone[1] = SLOT_NULL;
    }

    // Hook all Slots into free list
    m_FirstFreeSlot = SLOT_NULL;

    // just walk through the whole array and set everything up again,
    // just to be on the safe side
    for( i=0; i<MAX_OBJECTS; i++ )
    {
        ASSERT( m_ObjectSlot[i].pObject == NULL );
        ASSERT( m_ObjectSlot[i].NextRenderable[0] == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].NextRenderable[0] == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].PrevRenderable[0] == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].NextRenderable[1] == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].PrevRenderable[1] == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].NextZone[0]       == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].PrevZone[0]       == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].NextZone[1]       == SLOT_NULL );
        ASSERT( m_ObjectSlot[i].PrevZone[1]       == SLOT_NULL );

        m_ObjectSlot[i].pObject    = NULL;
        m_ObjectSlot[i].Next       = m_FirstFreeSlot;
        m_ObjectSlot[i].Prev       = SLOT_NULL;
        m_ObjectSlot[i].NextSearch = SLOT_NULL;
        m_ObjectSlot[i].NextVis    = SLOT_NULL;
        m_ObjectSlot[i].NextRenderable[0] = SLOT_NULL;
        m_ObjectSlot[i].PrevRenderable[0] = SLOT_NULL;
        m_ObjectSlot[i].NextRenderable[1] = SLOT_NULL;
        m_ObjectSlot[i].PrevRenderable[1] = SLOT_NULL;
        m_ObjectSlot[i].NextZone[0]       = SLOT_NULL;
        m_ObjectSlot[i].PrevZone[0]       = SLOT_NULL;
        m_ObjectSlot[i].NextZone[1]       = SLOT_NULL;
        m_ObjectSlot[i].PrevZone[1]       = SLOT_NULL;

        for( s32 j=0; j<8; j++ )
        {
            m_ObjectLink[((link_id)i)*8+j].SpacialCellID = SPATIAL_CELL_NULL;
            m_ObjectLink[((link_id)i)*8+j].Next          = LINK_NULL;
            m_ObjectLink[((link_id)i)*8+j].Prev          = LINK_NULL;
        }

        if( m_FirstFreeSlot != SLOT_NULL )
            m_ObjectSlot[ m_FirstFreeSlot ].Prev = i;

        m_FirstFreeSlot = i;
    }

    g_SpecialRenderObj.SetCount( 0 );
    g_SpecialRenderObj.FreeExtra();

    m_DeleteObject.SetCount( 0 );
    m_DeleteObject.FreeExtra();
}

//==============================================================================

void obj_mgr::AdvanceSimulation( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::AdvanceSimulation" );

    static xprofile_counter logicObjectsVisited =
        x_GetProfiler().RegisterCounter( "ObjMgrLogicObjectsVisited", "RenderCounter" );
    static xprofile_counter logicObjectsAdvanced =
        x_GetProfiler().RegisterCounter( "ObjMgrLogicObjectsAdvanced", "RenderCounter" );

    if( !x_isvalid( DeltaTime ) || (DeltaTime <= 0.0f) )
        return;

    DeltaTime = MIN( DeltaTime, 0.1f );

    m_nLogicLoops++;

//    DeltaTime *= m_TimeDilation;
#ifdef ENABLE_COLLISION_STATS
    g_CollisionMgr.m_Stats.Reset();
#endif // ENABLE_COLLISION_STATS

    s32     i;
    s32     LogicObjectsVisited = 0;
    s32     LogicObjectsAdvanced = 0;

    //ASSERT( DeltaTime > 0.0f );
    //ASSERT( DeltaTime <= 0.1f );

    m_SimulationTimeSeconds += static_cast<f64>( DeltaTime );
    m_GameTime = (xtick)( m_SimulationTimeSeconds * 1000.0 * (f64)x_GetTicksPerMs() );

    for( i = 0; i < (s32)object::TYPE_END_OF_LIST; i++ )
    {
        if( ( m_ObjectType[i].InstanceCount > 0 ) &&
            m_ObjectType[i].pDesc &&
            m_ObjectType[i].pDesc->OnBeginLogic() )
        {
            slot_id SlotID =  GetFirst( ( object::type)i );

            while( SlotID != SLOT_NULL )
            {
                // The object had better exist if we have a slot for it.
                object* pObject = GetObjectBySlot( SlotID );
                ASSERT( pObject );
                SlotID = GetNext(SlotID);
                LogicObjectsVisited++;

                if( pObject->GetAttrBits() & object::ATTR_NEEDS_LOGIC_TIME )
                {
                    //if( m_TrapBeforeLogicGuid && (pObject->GetGuid() == m_TrapBeforeLogicGuid) )
                    //    BREAK;

                    m_pAdvanceLogicActiveObject = pObject;
                    m_pAdvanceLogicActiveObject->OnAdvanceSimulation( DeltaTime );
                    m_pAdvanceLogicActiveObject = NULL;
                    LogicObjectsAdvanced++;

                    // Trapping after logic
                    //if( m_TrapAfterLogicGuid && (pObject->GetGuid() == m_TrapAfterLogicGuid) )
                    //    BREAK;
                }
            }
        }
    }

    // Refresh external cinema views only after every simulation object has
    // updated. Cameras are ordered after players for legacy object type
    // stability, so their final pose is not available during player logic.
    object_desc* pPlayerDesc = m_ObjectType[object::TYPE_PLAYER].pDesc;
    if( ( m_ObjectType[object::TYPE_PLAYER].InstanceCount > 0 ) &&
        pPlayerDesc &&
        pPlayerDesc->OnBeginLogic() )
    {
        slot_id PlayerSlotID = GetFirst( object::TYPE_PLAYER );
        while( PlayerSlotID != SLOT_NULL )
        {
            player* pPlayer = static_cast<player*>( GetObjectBySlot( PlayerSlotID ) );
            PlayerSlotID = GetNext( PlayerSlotID );

            if( pPlayer &&
                (pPlayer->GetAttrBits() & object::ATTR_NEEDS_LOGIC_TIME) )
            {
                pPlayer->FinalizeCinemaView();
            }
        }
    }

    //
    // Empty the delete list
    //
    logicObjectsVisited.Add( LogicObjectsVisited );
    logicObjectsAdvanced.Add( LogicObjectsAdvanced );
    EmptyDeleteObjectList();
}

//==============================================================================

#if !defined(X_RETAIL) 
void obj_mgr::SanityCheck( void )
{
    SlotSanityCheck();
}
#endif

//==============================================================================
void obj_mgr::ResetSearchResult( void )
{
    ASSERT( !m_InLoop );
    m_FirstSearchResult = SLOT_NULL;

    //
    // Update the Sequence
    //
    m_Sequence ++;
    if( m_Sequence == 0 )
    {
        for( s32 i=0; i<MAX_OBJECTS; i++ )
        {
            m_ObjectSlot[i].Sequence = 0;
        }
        m_Sequence ++;
    }
}

//==============================================================================

//check LOS from point to point
xbool obj_mgr::HasLOS( guid Object0, const vector3& P0, guid Object1, const vector3& P1 )
{
    // Cast a ray
    g_CollisionMgr.LineOfSightSetup( Object0, P0,  P1);

    // This must be after setup.
    g_CollisionMgr.SetMaxCollisions(2);

    g_CollisionMgr.AddToIgnoreList( Object0 );
    g_CollisionMgr.AddToIgnoreList( Object1 );


    // Perform collision
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_COLLIDABLE, (object::object_attr)(object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING));

    s32 nCollision = g_CollisionMgr.m_nCollisions;
    
    for( s32 i=0; i < g_CollisionMgr.m_nCollisions; i++)
    {
        if (g_CollisionMgr.m_Collisions[i].ObjectHitGuid == Object1)
        {
            //don't count that collision since it was the destination object
            nCollision--;
            break;
        }
    }

    // If no collisions, then we can see the object
    return (nCollision == 0);    
}

//==============================================================================

struct select_func_info
{
    u32     AttrFlags;
    bbox    BBox;
};

//==============================================================================

xbool SelectFunc( spatial_cell* pCell, void* pPrivateData )
{
    select_func_info* pInfo = (select_func_info*)pPrivateData;


    // Loop through them
    u32 Channel = 0;
    for (Channel = 0; Channel < NUM_SPATIAL_CHANNELS; Channel++)
    {
        link_id LI = pCell->FirstObjectLink[ Channel ];
        s32     C2 = 0;
        (void)C2;
        while( LI != LINK_NULL )
        {
            ASSERT( (C2++)<1000 );

            // Get object ptr
//            object* pObject = (object*)g_ObjMgr.LockObjectFromSlot( LI/8 );
            object* pObject = (object*)g_ObjMgr.GetObjectBySlot( LI/8 );
            ASSERT(pObject);
            
            if( pObject->GetAttrBits() & pInfo->AttrFlags )
            {
                if( pInfo->BBox.Intersect( pObject->GetBBox() ) )
                {
//                    selset_Add( pInfo->Set, pObject->GetGuid() );
                      g_ObjMgr.SetNextSearchResult( LI/8 );
                }
            }

//            g_ObjMgr.UnlockObject( pObject->GetGuid() );

            // Move to next link for this cell
            LI = g_ObjMgr.GetCellLinks()[LI].Next;
        }
    }

    return TRUE;
}

//==============================================================================

void obj_mgr::SelectBBox( u32 Attribute, const bbox& BBox, object::type Type, u32 NotTheseAttributes )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::SelectBBox" );

    static xprofile_counter bboxQueries =
        x_GetProfiler().RegisterCounter( "ObjMgrBBoxQueries", "PhysicsCounter" );
    static xprofile_counter bboxLinkCandidates =
        x_GetProfiler().RegisterCounter( "ObjMgrBBoxLinkCandidates", "PhysicsCounter" );
    static xprofile_counter bboxObjectsTested =
        x_GetProfiler().RegisterCounter( "ObjMgrBBoxObjectsTested", "PhysicsCounter" );
    static xprofile_counter bboxResults =
        x_GetProfiler().RegisterCounter( "ObjMgrBBoxResults", "PhysicsCounter" );

    s32 BBoxLinkCandidates = 0;
    s32 BBoxObjectsTested  = 0;
    s32 BBoxResults        = 0;

    //
    // update the sequence
    //
    ResetSearchResult();

    //
    // Ask the spacial data base to collect cells that have object in there
    //
    g_SpatialDBase.TraverseCells( BBox );

    //
    // Loop through all visible nodes that contain objects
    //
    #ifdef X_ASSERT
    s32 InfCount1 = 0;
    #endif
    u16 CI = g_SpatialDBase.GetFirstInSearch();

    while( CI != SPATIAL_CELL_NULL )
    {
        spatial_cell& Cell = g_SpatialDBase.GetCell( CI );

        // Check for inf loops
        #ifdef X_ASSERT
        ASSERT( (InfCount1++)<obj_mgr::MAX_OBJECTS );
        #endif
        
        // Check if there are any objects in cell
        if( Cell.OccFlags )
        {
            s32 Channel;
            for( Channel=0; Channel<NUM_SPATIAL_CHANNELS; Channel++ )
            {
               link_id LI = Cell.FirstObjectLink[ Channel ];

               #ifdef X_ASSERT
               s32 InfCount2 = 0;
                #endif

                for(; LI != LINK_NULL; LI = m_ObjectLink[LI].Next )
                {
                    BBoxLinkCandidates++;

                    #ifdef X_ASSERT
                    ASSERT( (InfCount2++) < obj_mgr::MAX_OBJECTS );
                    #endif

                    // Get object index
                    slot_id I       = LI / 8;

                    if( m_ObjectSlot[I].Sequence == m_Sequence )
                        continue;
                    m_ObjectSlot[I].Sequence = m_Sequence;

                    object* pObject = m_ObjectSlot[I].pObject;
                    BBoxObjectsTested++;
                    ASSERT( pObject->GetType() > (s32)object::TYPE_NULL );

                    u32 const         AttrBits  = pObject->GetAttrBits();
                    object::type const ObjectType = pObject->GetType();

                    // This is object match the attributes?
                    if( (AttrBits & Attribute) == 0 )
                        continue;

                    if( (AttrBits & NotTheseAttributes) != 0 )
                        continue;

                    // Make sure that it matches the type
                    if( Type != object::TYPE_ALL_TYPES)
                        if( ObjectType != Type )
                            continue;

                    bbox const& ObjectBBox = pObject->GetBBox();
                    if( !BBox.Intersect( ObjectBBox ) )
                        continue;

                    m_ObjectSlot[I].Sequence         = m_Sequence;
                    m_ObjectSlot[I].NextSearch       = m_FirstSearchResult;
                    m_FirstSearchResult              = I;
                    BBoxResults++;
                    // Move to next link for this cell
                    ASSERT( LI != m_ObjectLink[LI].Next );                    
                }
            }
        }
    
        // Get the next cell
        CI = g_SpatialDBase.GetNextInSearch( CI );
    }

    bboxQueries.Add();
    bboxLinkCandidates.Add( BBoxLinkCandidates );
    bboxObjectsTested.Add( BBoxObjectsTested );
    bboxResults.Add( BBoxResults );

}

//==============================================================================

void obj_mgr::SelectRay( u32 Attribute, const vector3& RayStart, const vector3& RayEnd, object::type Type, u32 NotTheseAttributes )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::SelectRay" );

    static xprofile_counter rayQueries =
        x_GetProfiler().RegisterCounter( "ObjMgrRayQueries", "PhysicsCounter" );
    static xprofile_counter rayLinkCandidates =
        x_GetProfiler().RegisterCounter( "ObjMgrRayLinkCandidates", "PhysicsCounter" );
    static xprofile_counter rayObjectsTested =
        x_GetProfiler().RegisterCounter( "ObjMgrRayObjectsTested", "PhysicsCounter" );
    static xprofile_counter rayResults =
        x_GetProfiler().RegisterCounter( "ObjMgrRayResults", "PhysicsCounter" );

    s32 RayLinkCandidates = 0;
    s32 RayObjectsTested  = 0;
    s32 RayResults        = 0;

    //
    // update the sequence
    //
    ResetSearchResult();

    //
    // Ask the spacial data base to collect cells that have object in there
    //
    g_SpatialDBase.TraverseCells( RayStart, RayEnd );

    //
    // Loop through all nodes that contain objects
    //
    #ifdef X_ASSERT
    s32 InfCount1 = 0;
    #endif
    u16 CI = g_SpatialDBase.GetFirstInSearch();

    while( CI != SPATIAL_CELL_NULL )
    {
        spatial_cell& Cell = g_SpatialDBase.GetCell( CI );

        // Check for inf loops
        #ifdef X_ASSERT
        ASSERT( (InfCount1++)<obj_mgr::MAX_OBJECTS );
        #endif
        
        // Check if there are any objects in cell
        if( Cell.OccFlags )
        {
            s32 Channel;
            for( Channel=0; Channel<NUM_SPATIAL_CHANNELS; Channel++ )
            {
               link_id LI = Cell.FirstObjectLink[ Channel ];

               #ifdef X_ASSERT
               s32 InfCount2 = 0;
                #endif

                for(; LI != LINK_NULL; LI = m_ObjectLink[LI].Next )
                {
                    RayLinkCandidates++;

                    #ifdef X_ASSERT
                    ASSERT( (InfCount2++) < obj_mgr::MAX_OBJECTS );
                    #endif

                    // Get object index
                    slot_id I       = LI / 8;

                    if( m_ObjectSlot[I].Sequence == m_Sequence )
                        continue;
                    m_ObjectSlot[I].Sequence = m_Sequence;

                    object* pObject = m_ObjectSlot[I].pObject;
                    RayObjectsTested++;
                    ASSERT( pObject->GetType() > (s32)object::TYPE_NULL );

                    u32 const         AttrBits  = pObject->GetAttrBits();
                    object::type const ObjectType = pObject->GetType();

                    // This is object match the attributes?
                    if( (AttrBits & Attribute) == 0 )
                        continue;

                    if( (AttrBits & NotTheseAttributes) != 0 )
                        continue;

                    // Make sure that it matches the type
                    if( Type != object::TYPE_ALL_TYPES)
                        if( ObjectType != Type )
                            continue;

                    f32 T;
                    bbox const& ObjectBBox = pObject->GetBBox();
                    if( !ObjectBBox.Intersect( T, RayStart, RayEnd ) )
                        continue;

                    // Add it to the list
                    m_ObjectSlot[I].Sequence         = m_Sequence;
                    m_ObjectSlot[I].NextSearch       = m_FirstSearchResult;
                    m_FirstSearchResult              = I;
                    RayResults++;
                    // Move to next link for this cell
                    ASSERT( LI != m_ObjectLink[LI].Next );                    
                }
            }
        }
    
        // Get the next cell
        CI = g_SpatialDBase.GetNextInSearch( CI );
    }

    rayQueries.Add();
    rayLinkCandidates.Add( RayLinkCandidates );
    rayObjectsTested.Add( RayObjectsTested );
    rayResults.Add( RayResults );

}

//==============================================================================

void obj_mgr::SelectVolume( u32 Attribute, const plane* pPlane, s32 NPlanes, xbool Fast, object::type Type  )
{
    (void)Type;
    s32 i,j,k;

    ASSERT( NPlanes <= 16 );



    ResetSearchResult();
    // Loop through all objects
    for( i=0; i<MAX_OBJECTS; i++ )
    {
    
        if( GetObjectBySlot(i) )
        {
            object* pObj = GetObjectBySlot(i);
            ASSERT( pObj );

            if( pObj->GetAttrBits() & Attribute )
            {
                const bbox& BBox = pObj->GetBBox();
                u32 Spanning = 0;

                // Determine for each plane whether it is in or out
                for( j=0; j<NPlanes; j++ )
                {
                    vector3 BestPt;
                    vector3 WorstPt;

                    if( pPlane[j].Normal.GetX() > 0 ) { BestPt.GetX() = BBox.Max.GetX(); WorstPt.GetX() = BBox.Min.GetX(); }
                    else                              { BestPt.GetX() = BBox.Min.GetX(); WorstPt.GetX() = BBox.Max.GetX(); }
                    if( pPlane[j].Normal.GetY() > 0 ) { BestPt.GetY() = BBox.Max.GetY(); WorstPt.GetY() = BBox.Min.GetY(); }
                    else                              { BestPt.GetY() = BBox.Min.GetY(); WorstPt.GetY() = BBox.Max.GetY(); }
                    if( pPlane[j].Normal.GetZ() > 0 ) { BestPt.GetZ() = BBox.Max.GetZ(); WorstPt.GetZ() = BBox.Min.GetZ(); }
                    else                              { BestPt.GetZ() = BBox.Min.GetZ(); WorstPt.GetZ() = BBox.Max.GetZ(); }

                    f32 BestD = pPlane[j].Distance( BestPt );
                    f32 WorstD = pPlane[j].Distance( WorstPt );

                    if( BestD < 0.0f ) 
                    {
                        // Trivial rejection so terminate early
                        break;
                    }

                    if( WorstD < 0.0f )
                    {
                        // Spanning plane so store for clipping
                        Spanning |= (1<<j);
                    }
                }

                // Did pass trivial rejection?
                if( j==NPlanes )
                {
                    // If fast or bbox is completely inside all planes just add
                    if( Fast || (Spanning==0) )
                    {
//                        selset_Add( Set, m_ObjectSlot[i].Guid );
                          SetNextSearchResult(i);
                    }
                    else
                    {
                        s16 Index[] = {3,1,5,7, 7,5,4,6, 3,2,0,1, 6,4,0,2, 2,3,7,6, 1,0,4,5};
                        vector3 P[8];

                        P[0].GetX() = BBox.Min.GetX();    P[0].GetY() = BBox.Min.GetY();    P[0].GetZ() = BBox.Min.GetZ();
                        P[1].GetX() = BBox.Min.GetX();    P[1].GetY() = BBox.Min.GetY();    P[1].GetZ() = BBox.Max.GetZ();
                        P[2].GetX() = BBox.Min.GetX();    P[2].GetY() = BBox.Max.GetY();    P[2].GetZ() = BBox.Min.GetZ();
                        P[3].GetX() = BBox.Min.GetX();    P[3].GetY() = BBox.Max.GetY();    P[3].GetZ() = BBox.Max.GetZ();
                        P[4].GetX() = BBox.Max.GetX();    P[4].GetY() = BBox.Min.GetY();    P[4].GetZ() = BBox.Min.GetZ();
                        P[5].GetX() = BBox.Max.GetX();    P[5].GetY() = BBox.Min.GetY();    P[5].GetZ() = BBox.Max.GetZ();
                        P[6].GetX() = BBox.Max.GetX();    P[6].GetY() = BBox.Max.GetY();    P[6].GetZ() = BBox.Min.GetZ();
                        P[7].GetX() = BBox.Max.GetX();    P[7].GetY() = BBox.Max.GetY();    P[7].GetZ() = BBox.Max.GetZ();

                        // Loop through the six quads of the bbox
                        for( k=0; k<6; k++ )
                        {
                            vector3 Vert[64];
                            s32     SrcOffset=0;
                            s32     DstOffset=32;
                            s32     NSrcVerts=4;
                            s32     NDstVerts;
                            Vert[0] = P[Index[k*4+0]];
                            Vert[1] = P[Index[k*4+1]];
                            Vert[2] = P[Index[k*4+2]];
                            Vert[3] = P[Index[k*4+3]];

                            // Loop through the planes and clip ngon to each plane
                            for( j=0; j<NPlanes; j++ )
                            if( Spanning & (1<<j) )
                            {
                                if( !pPlane[j].ClipNGon( Vert+DstOffset, NDstVerts, Vert+SrcOffset, NSrcVerts ) )
                                    break;

                                // Flip buffers
                                SrcOffset = DstOffset;
                                DstOffset = (DstOffset + 32)%64;
                                NSrcVerts = NDstVerts;
                            }
                    
                            // Did we survive all planes?
                            if( j==NPlanes )
                            {
                                SetNextSearchResult(i);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

//==============================================================================

void obj_mgr::SelectByAttribute( u32 Attribute, object::type Type  )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::SelectByAttribute" );

    static xprofile_counter attributeQueries =
        x_GetProfiler().RegisterCounter( "ObjMgrAttributeQueries", "PhysicsCounter" );
    static xprofile_counter attributeObjectsVisited =
        x_GetProfiler().RegisterCounter( "ObjMgrAttributeObjectsVisited", "PhysicsCounter" );
    static xprofile_counter attributeResults =
        x_GetProfiler().RegisterCounter( "ObjMgrAttributeResults", "PhysicsCounter" );

    s32     i;
    slot_id ID;
    s32     AttributeObjectsVisited = 0;
    s32     AttributeResults        = 0;

    ResetSearchResult();

    if( Type == object::TYPE_NULL )
    {
        for( i = 0; i < (s32)object::TYPE_END_OF_LIST; i++ )
        {
            ID = GetFirst((object::type)i);
            while ( ID != SLOT_NULL )
            {
                ASSERT( m_ObjectSlot[ID].pObject );
                AttributeObjectsVisited++;
                if(Attribute & m_ObjectSlot[ID].pObject->GetAttrBits() )
                {
                    SetNextSearchResult( ID );
                    AttributeResults++;
                }

                ID = m_ObjectSlot[ID].Next;
            }
        }
    }
    else
    {
            ID = GetFirst( Type );
            while ( ID != SLOT_NULL )
            {
                ASSERT( m_ObjectSlot[ID].pObject );
                AttributeObjectsVisited++;
                if(Attribute & m_ObjectSlot[ID].pObject->GetAttrBits() )
                {
                    SetNextSearchResult( ID );
                    AttributeResults++;
                }

                ID = m_ObjectSlot[ID].Next;

            }
    }
    attributeQueries.Add();
    attributeObjectsVisited.Add( AttributeObjectsVisited );
    attributeResults.Add( AttributeResults );
}

//==============================================================================

object::type obj_mgr::GetTypeFromName( const char* pName )
{
    ASSERT( pName );

    for( s32 i=0; i<(s32)object::TYPE_END_OF_LIST; i++ )
    {
        for( object_desc* pNext = m_ObjectType[i].pDesc; pNext; pNext = pNext->m_pNextGroupType )
        {
            if( x_stricmp( pNext->m_pTypeName, pName )==0 )
                return (object::type)i;
        }
    }

    return object::TYPE_NULL;
}

//==============================================================================

const char* obj_mgr::GetNameFromType( object::type Type )
{
    ASSERT( (Type>object::TYPE_NULL) && (Type<object::TYPE_END_OF_LIST) );

    return m_ObjectType[Type].pDesc->m_pTypeName;
}

//==============================================================================
void obj_mgr::RemoveFromSpatialDBase(guid Guid)
{
    RemoveFromSpatialDBase(GetSlotFromGuid(Guid) );

}

//==============================================================================

void obj_mgr::AddToSpatialDBase(guid Guid)
{
    AddToSpatialDBase(GetSlotFromGuid(Guid) );

}

//==============================================================================

void obj_mgr::RemoveFromSpatialDBase( slot_id SlotID )
{
    obj_cell_link* pLink = GetCellLinks();

    // Remove ourselves
    for( s32 i=0; i<8; i++ )
    {
        // Get index to this link
        link_id LI = ((link_id)SlotID)*8+i;

        // Check if unused
        if( pLink[LI].SpacialCellID == SPATIAL_CELL_NULL )
            break;

        // Get access to cell
        spatial_cell& Cell = g_SpatialDBase.GetCell( pLink[LI].SpacialCellID );

        // Remove from linked list
        if( Cell.FirstObjectLink[0] == LI )
            Cell.FirstObjectLink[0] = pLink[LI].Next;

        if( pLink[LI].Next != LINK_NULL )
            pLink[ pLink[LI].Next ].Prev = pLink[LI].Prev;

        if( pLink[LI].Prev != LINK_NULL )
            pLink[ pLink[LI].Prev ].Next = pLink[LI].Next;

        // Check if list is empty
        if( Cell.FirstObjectLink[0] == LINK_NULL )
        {
            Cell.OccFlags &= (~(1<<0));
//            x_DebugMsg("Removed  SlotID  -- %d  --- CellID  ---  %d\n",SlotID, pLink[LI].SpacialCellID );
            g_SpatialDBase.UpdateCell( Cell );
        }

        pLink[LI].SpacialCellID = SPATIAL_CELL_NULL;
        pLink[LI].Next          = LINK_NULL;
        pLink[LI].Prev          = LINK_NULL;
    }
}

//==============================================================================

void obj_mgr::AddToSpatialDBase( slot_id SlotID )
{
    // Update spatial database
    s32 i;
    s32 X,Y,Z;
    s32 MinX,MinY,MinZ;
    s32 MaxX,MaxY,MaxZ;

    // Get cell regions affected
    object* pObject = GetObjectBySlot( SlotID );
    ASSERT( pObject );
    bbox ObjBBox( pObject->GetBBox() );

    if( !ObjBBox.Min.IsValid() || !ObjBBox.Max.IsValid() )
    {
        x_DebugMsg( "CRITICAL ERROR!!! Object of Type %s has an invalid spacial dbase bbox: "
                    "Min(%.3f, %.3f, %.3f) Max(%.3f, %.3f, %.3f)\n",
                    pObject->GetTypeDesc().GetTypeName(),
                    ObjBBox.Min.GetX(), ObjBBox.Min.GetY(), ObjBBox.Min.GetZ(),
                    ObjBBox.Max.GetX(), ObjBBox.Max.GetY(), ObjBBox.Max.GetZ() );
        return;
    }
    
    s32  Level = 0;

    if( !g_SpatialDBase.TryGetBBoxLevel( ObjBBox, Level ) )
    {
#ifdef X_EDITOR
        x_DebugMsg("CRITICAL ERROR!!! Object of Type %s, [%s] has bad spacial dbase entry!\n",
            GetObjectBySlot( SlotID )->GetTypeDesc().GetTypeName(), guid_ToString(GetObjectBySlot( SlotID )->GetGuid()));
#else // X_EDITOR
        x_DebugMsg("CRITICAL ERROR!!! Object of Type %s has bad spacial dbase entry!\n",
            GetObjectBySlot( SlotID )->GetTypeDesc().GetTypeName());
#endif // X_EDITOR
        return;
    }

    g_SpatialDBase.GetCellRegion( ObjBBox, Level, MinX, MinY, MinZ, MaxX, MaxY, MaxZ );
    ASSERT( IN_RANGE( 0, (MaxX-MinX), 1 ) );
    ASSERT( IN_RANGE( 0, (MaxY-MinY), 1 ) );
    ASSERT( IN_RANGE( 0, (MaxZ-MinZ), 1 ) );

    obj_cell_link* pLink = g_ObjMgr.GetCellLinks();

#ifdef X_DEBUG
    for( i=0; i<8; i++ )
    {
        link_id LI = ((link_id)SlotID)*8+i;
        (void)LI;
        ASSERT( pLink[LI].SpacialCellID == SPATIAL_CELL_NULL );
        ASSERT( pLink[LI].Next          == LINK_NULL );
        ASSERT( pLink[LI].Prev          == LINK_NULL );
    }
#endif

    // Add ourselves
    i=0;
    for( X=MinX; X<=MaxX; X++ )
    for( Y=MinY; Y<=MaxY; Y++ )
    for( Z=MinZ; Z<=MaxZ; Z++ )
    {
        // Get index to link
        link_id LI = ((link_id)SlotID)*8+i;
        ASSERT( i<8 );

        // Get access to cell
        pLink[LI].SpacialCellID = g_SpatialDBase.GetCellIndex( X, Y, Z, Level, TRUE );

        spatial_cell& Cell = g_SpatialDBase.GetCell( pLink[LI].SpacialCellID );

        // Add link to list
        pLink[LI].Prev = LINK_NULL;
        ASSERT(LI != Cell.FirstObjectLink[0]);

        pLink[LI].Next = Cell.FirstObjectLink[0];        

        if( Cell.FirstObjectLink[0] != LINK_NULL )
            pLink[ Cell.FirstObjectLink[0] ].Prev = LI;

        Cell.FirstObjectLink[0] = LI;

        // Turn on occupied flag
        Cell.OccFlags |= (1<<0);
//        x_DebugMsg("Added SlotID  -- %d  --- CellID  ---  %d\n",SlotID, pLink[LI].SpacialCellID );
        // Move to next link
        i++;
    }
}

//==============================================================================

s32 UPDATE_SPATIAL_DBASE_HITS=0;
s32 UPDATE_SPATIAL_DBASE_MISSES=0;

void obj_mgr::UpdateSpatialDBase( slot_id SlotID, const bbox& OldBBox )
{
    s32 OldMinX,OldMinY,OldMinZ;
    s32 OldMaxX,OldMaxY,OldMaxZ;
    s32 NewMinX,NewMinY,NewMinZ;
    s32 NewMaxX,NewMaxY,NewMaxZ;

    // Get cell regions affected
    object* pObject = GetObjectBySlot( SlotID );
    ASSERT( pObject );

    bbox NewBBox( pObject->GetBBox() );

    if( !OldBBox.Min.IsValid() || !OldBBox.Max.IsValid() ||
        !NewBBox.Min.IsValid() || !NewBBox.Max.IsValid() )
    {
        x_DebugMsg( "CRITICAL ERROR!!! Object of Type %s has an invalid spacial dbase bbox during update.\n",
                    pObject->GetTypeDesc().GetTypeName() );
        return;
    }
    
    s32  OldLevel = 0;
    s32  NewLevel = 0;

    if( !g_SpatialDBase.TryGetBBoxLevel( OldBBox, OldLevel ) ||
        !g_SpatialDBase.TryGetBBoxLevel( NewBBox, NewLevel ) )
    {
#ifdef X_EDITOR
        x_DebugMsg("CRITICAL ERROR!!! Object of Type %s, [%s] has bad spacial dbase entry!\n",
            GetObjectBySlot( SlotID )->GetTypeDesc().GetTypeName(), guid_ToString(GetObjectBySlot( SlotID )->GetGuid()));
#else // X_EDITOR
        x_DebugMsg("CRITICAL ERROR!!! Object of Type %s has bad spacial dbase entry!\n",
            GetObjectBySlot( SlotID )->GetTypeDesc().GetTypeName());
#endif // X_EDITOR
        return;
    }

    g_SpatialDBase.GetCellRegion( OldBBox, OldLevel, OldMinX, OldMinY, OldMinZ, OldMaxX, OldMaxY, OldMaxZ );
    g_SpatialDBase.GetCellRegion( NewBBox, NewLevel, NewMinX, NewMinY, NewMinZ, NewMaxX, NewMaxY, NewMaxZ );
    ASSERT( IN_RANGE( 0, (OldMaxX-OldMinX), 1 ) );
    ASSERT( IN_RANGE( 0, (OldMaxY-OldMinY), 1 ) );
    ASSERT( IN_RANGE( 0, (OldMaxZ-OldMinZ), 1 ) );
    ASSERT( IN_RANGE( 0, (OldMaxX-OldMinX), 1 ) );
    ASSERT( IN_RANGE( 0, (OldMaxY-OldMinY), 1 ) );
    ASSERT( IN_RANGE( 0, (OldMaxZ-OldMinZ), 1 ) );

    // Check if we would be updating exactly the same cells!
    if( (OldLevel==NewLevel) && 
        (OldMinX==NewMinX) && (OldMinY==NewMinY) && (OldMinZ==NewMinZ) &&
        (OldMaxX==NewMaxX) && (OldMaxY==NewMaxY) && (OldMaxZ==NewMaxZ) ) 
    {
        UPDATE_SPATIAL_DBASE_HITS++;
        return;
    }
    else
    {
        UPDATE_SPATIAL_DBASE_MISSES++;
    }

    //
    // Remove from old cells
    //
    RemoveFromSpatialDBase( SlotID );

    //
    // Add to new cells
    //
    obj_cell_link* pLink = g_ObjMgr.GetCellLinks();

    // Add ourselves
    s32 i=0;
    for( s32 X=NewMinX; X<=NewMaxX; X++ )
    for( s32 Y=NewMinY; Y<=NewMaxY; Y++ )
    for( s32 Z=NewMinZ; Z<=NewMaxZ; Z++ )
    {
        // Get index to link
        link_id LI = ((link_id)SlotID)*8+i;
        ASSERT( i<8 );

        // Get access to cell
        pLink[LI].SpacialCellID = g_SpatialDBase.GetCellIndex( X, Y, Z, NewLevel, TRUE );

        spatial_cell& Cell = g_SpatialDBase.GetCell( pLink[LI].SpacialCellID );

        // Add link to list
        pLink[LI].Prev = LINK_NULL;
        ASSERT(LI != Cell.FirstObjectLink[0]);

        pLink[LI].Next = Cell.FirstObjectLink[0];        

        if( Cell.FirstObjectLink[0] != LINK_NULL )
            pLink[ Cell.FirstObjectLink[0] ].Prev = LI;

        Cell.FirstObjectLink[0] = LI;

        // Turn on occupied flag
        Cell.OccFlags |= (1<<0);
        i++;
    }

}

//==============================================================================

s32 obj_mgr::IsBoxInView( 
    const bbox&         BBox,
    u32                 CheckPlaneMask  ) const
{
    s32             i;
    s32             PlanesHit       = 0;
    const f32*      pF              = (const f32*)&BBox;
    const s32*      pMinI           = m_PlaneMinIndex;
    const s32*      pMaxI           = m_PlaneMaxIndex;
    const plane*    pPlane          = m_Plane;

    for( i=0; i<6; i++, pMinI += 3, pMaxI += 3, pPlane++ )
    {
        if( !(CheckPlaneMask & (1<<i)) )
            continue;

        // Compute max dist along normal
        f32 MaxDist = pPlane->Normal.GetX() * pF[pMaxI[0]] +
                      pPlane->Normal.GetY() * pF[pMaxI[1]] +
                      pPlane->Normal.GetZ() * pF[pMaxI[2]] +
                      pPlane->D;

        // If outside plane, we are culled.
        if( MaxDist < 0 )
            return -1;

        // Compute min dist along normal
        f32 MinDist  = pPlane->Normal.GetX() * pF[pMinI[0]] +
                       pPlane->Normal.GetY() * pF[pMinI[1]] +
                       pPlane->Normal.GetZ() * pF[pMinI[2]] +
                       pPlane->D;

        // Partially visible boxes need clipping, including at the far plane.
        if(MinDist<0)
            PlanesHit++;
    }

    // Zero means fully inside; positive values count intersected planes.
    return PlanesHit;

}

//==============================================================================

#ifdef X_EDITOR
void obj_mgr::RenderIcons( void )
{
    if( eng_Begin( "Render Icons" ) )
    {
        vector3 Position = eng_GetView()->GetPosition();
        xcolor Tint( 255,255,255,255);

        for( s32 type=object::TYPE_NULL+1; type<(s32)object::TYPE_END_OF_LIST; type++ )
        {
            // If a type is renderable then render
            if( m_ObjectType[type].pDesc && m_ObjectType[ type ].pDesc->CanRender() ) 
            {
                // Render any objects of this type that are selected
                {
                    slot_id ID = GetFirst( (object::type)type ) ;
                    while ( ID != SLOT_NULL )
                    {
                        // Lookup object
                        object* pObject = GetObjectBySlot( ID ) ;
                        if (pObject)
                        {
                            // If selected then render.
                            if( (pObject->GetAttrBits() & object::ATTR_EDITOR_SELECTED) &&
                                (pObject->IsHidden()==FALSE) )
                            {
                                EditorIcon const iconToDraw =
                                    static_cast<EditorIcon>( pObject->GetTypeDesc().OnEditorRender( *pObject ) );

                                if(g_EditorShowNameFlag)
                                {
                                    vector3 LabelPos = pObject->GetBBox().GetCenter();
                                    
                                    //Only show named Objects labels
                                    if(x_strcmp(pObject->GetName(), pObject->GetTypeDesc().GetTypeName())!=0 &&
                                        x_strcmp(pObject->GetName(), "<Trigger>")!=0)
                                    {
                                        render::debug::Label(LabelPos, xcolor(255,255,255,255), pObject->GetName());
                                    }
                                }

                                if( iconToDraw != EditorIcon::Invalid )
                                {
                                    const matrix4&      M       = pObject->GetL2W();
                                    DrawEditorIcon(iconToDraw, M, TRUE, Tint );
                                }
                            }
                        }

                        // Check next
                        ID = GetNext( ID ) ;
                    }
                }

                // Go through all the objects in the given type and render visible ones that are not selected
                slot_id ID = m_ObjectType[type].FirstVis;
                for(; ID != SLOT_NULL; ID = GetNextVis( ID ) )
                {
                    // Get the object
                    object* pObject = GetObjectBySlot( ID );
                    if( pObject == NULL ) continue;

                    // Skip rendering the object if it is selected because we have already rendered it from
                    // above.
                    if( pObject->GetAttrBits() & object::ATTR_EDITOR_SELECTED )
                        continue;

                    // Don't render if hidden
                    if( pObject->IsHidden() )
                        continue;

                    object&             Object  = *pObject;
                    const matrix4&      M       = Object.GetL2W();

                    // Don't render any thing if it is too far
                    vector3             V( Position - M.GetTranslation());
                    f32 F = V.Dot( V );
                    if( F > (10000.0f*10000.0f) || F < (100*100) )
                        continue;

                    if(g_EditorShowNameFlag)
                    {
                        vector3 LabelPos = Object.GetBBox().GetCenter();
                     
                        //Only show named Objects labels
                        if(x_strcmp(Object.GetName(), Object.GetTypeDesc().GetTypeName())!=0 &&
                            x_strcmp(Object.GetName(), "<Trigger>")!=0)
                        {
                            if( F < (1000.0f*10000.0f))
                                render::debug::Label(LabelPos, xcolor(255,255,255,255), Object.GetName());
                        }
                    }

                    // Handle the rendering of it all.
                    const object_desc&  ObjDesc = Object.GetTypeDesc();
                    EditorIcon const iconToDraw = static_cast<EditorIcon>( ObjDesc.OnEditorRender( Object ) );

                    if( iconToDraw != EditorIcon::Invalid )
                    {
                        DrawEditorIcon(iconToDraw, M, FALSE, Tint );
                    }
                }
            }
        }
        eng_End();
    }
}
#endif

//==============================================================================

void obj_mgr::DoVisibilityTests( const view& View )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::DoVisibilityTests" );

    static xprofile_counter visibilityCandidates =
        x_GetProfiler().RegisterCounter( "ObjMgrVisibilityCandidates", "RenderCounter" );
    static xprofile_counter visibleObjects =
        x_GetProfiler().RegisterCounter( "ObjMgrVisibleObjects", "RenderCounter" );

    s32 VisibilityCandidates = 0;

    // Clear vis lists
    s32 i;
    for( i = 0; i < (s32)object::TYPE_END_OF_LIST; i++ )
    {
        m_ObjectType[i].FirstVis = SLOT_NULL;
        m_ObjectType[i].nVis     = 0;
    }
    ResetSearchResult();
    m_VisibleSlotCount = 0;

    // Build the cull planes
    View.GetViewPlanes( m_Plane[3],
                        m_Plane[2],
                        m_Plane[0],
                        m_Plane[1],
                        m_Plane[4],
                        m_Plane[5],
                        view::WORLD );
    
    for( i=0; i<6;i++)
        m_Plane[i].GetBBoxIndices( &m_PlaneMinIndex[i*3], &m_PlaneMaxIndex[i*3] );

    // loop through renderable objects of each visible zone
    s32 StartingZone = g_ZoneMgr.GetStartingZone();
#ifdef X_EDITOR
    s32 ZoneCount = 256;
#else
    s32 ZoneCount = g_ZoneMgr.GetZoneCount();
#endif

    for( i = 0; i < ZoneCount; i++ )
    {
        if( (i == 0) || g_ZoneMgr.IsZoneVisible( i ) )
        {
            for( s32 WhichZone = 0; WhichZone < 2; WhichZone++ )
            {
                #if defined(X_EDITOR)
                slot_id SlotID = m_ObjectZone[i].FirstZone[WhichZone];
                #else
                slot_id SlotID = m_ObjectZone[i].FirstRenderable[WhichZone];
                #endif
                while( SlotID != SLOT_NULL )
                {
                    VisibilityCandidates++;

                    // get a pointer to the object to be tested
                    obj_slot& ObjSlot = m_ObjectSlot[SlotID];
                    object*   pObject = ObjSlot.pObject;

                    // advance the slot id
                    slot_id NextSlotID;
                    #if defined(X_EDITOR)
                    NextSlotID = ObjSlot.NextZone[WhichZone];
                    #else
                    NextSlotID = ObjSlot.NextRenderable[WhichZone];
                    #endif

                    // have we already checked this object?
                    if( ObjSlot.Sequence == m_Sequence )
                    {
                        SlotID = NextSlotID;
                        continue;
                    }

                    ASSERT( pObject );
                    #if !defined(X_RETAIL)
                    if( pObject && m_TrapBeforeRenderGuid && (pObject->GetGuid() == m_TrapBeforeRenderGuid))
                        BREAK;
                    #endif

                    // sanity check
                    #if !defined(X_EDITOR)
                    ASSERT( pObject && pObject->GetAttrBits() & object::ATTR_RENDERABLE );
                    #endif
                    
                    // perform zone manager visibility tests
                    const bbox& BBox = pObject->GetBBox();
                    if( (i != StartingZone) && !g_ZoneMgr.IsBBoxVisible( BBox, (zone_mgr::zone_id)pObject->GetZone1(), (zone_mgr::zone_id)pObject->GetZone2() ) )
                    {
                        SlotID = NextSlotID;
                        continue;
                    }

                    // perform clipping tests
                    s32 InView = IsBoxInView( BBox, XBIN(111111) );

                    // Outside the view?
                    if( InView == -1 )
                    {
                        SlotID = NextSlotID;
                        continue;
                    }

                    // completely in or needs clipping?
                    if( InView == 0 )
                    {
                        pObject->SetFlagBits( pObject->GetFlagBits() & ~object::FLAG_CHECK_PLANES );
                    }
                    else
                    {
                        pObject->SetFlagBits( (pObject->GetFlagBits() & ~object::FLAG_CHECK_PLANES) | (1<<object::FLAG_CHECK_PLANES_SHIFT) );
                    }

                    // mark this object as needing to be rendered
                    s32 ObjType = pObject->GetType();
                    ASSERT( ObjType > (s32)object::TYPE_NULL );
                    ObjSlot.Sequence = m_Sequence;
                    ObjSlot.NextVis  = m_ObjectType[ObjType].FirstVis;
                    m_ObjectType[ObjType].FirstVis = SlotID;
                    m_ObjectType[ObjType].nVis++;
                    ASSERT( m_VisibleSlotCount < MAX_OBJECTS );
                    m_VisibleSlots[m_VisibleSlotCount++] = SlotID;

                    // move to the next object
                    SlotID = NextSlotID;
                }
            }
        }
    }

    // Lights are a special case. It is too easy for lights to overlap zones,
    // and it would be a pain in the butt to move all the lights into portals,
    // so we'll always allow them to be rendered.
    //for( i = (s32)object::TYPE_CHARACTER_LIGHT; i <= (s32)object::TYPE_DYNAMIC_LIGHT; i++ )

    // Only allow this for character lights. Dynamic lights should still be zone
    // based to avoid bleeding through walls.
    i = (s32)object::TYPE_CHARACTER_LIGHT;
    {
        slot_id SlotID = GetFirst( (object::type)i );
        while( SlotID != SLOT_NULL )
        {
            VisibilityCandidates++;

            // get a pointer to the object to be tested
            obj_slot& ObjSlot = m_ObjectSlot[SlotID];
            object*   pObject = ObjSlot.pObject;

            // advance the slot id
            slot_id NextSlotID;
            NextSlotID = GetNext( SlotID );

            // have we already checked this object?
            if( ObjSlot.Sequence == m_Sequence )
            {
                SlotID = NextSlotID;
                continue;
            }

            ASSERT( pObject );
#if !defined(X_RETAIL)
            if( pObject && m_TrapBeforeRenderGuid && (pObject->GetGuid() == m_TrapBeforeRenderGuid))
                BREAK;
#endif

            // sanity check
#if !defined(X_EDITOR)
            ASSERT( pObject && pObject->GetAttrBits() & object::ATTR_RENDERABLE );
#endif

            // perform clipping tests
                    const bbox& BBox = pObject->GetBBox();
            s32 InView = IsBoxInView( BBox, XBIN(111111) );

            // Outside the view?
            if( InView == -1 )
            {
                SlotID = NextSlotID;
                continue;
            }

            // completely in or needs clipping?
            if( InView == 0 )
            {
                pObject->SetFlagBits( pObject->GetFlagBits() & ~object::FLAG_CHECK_PLANES );
            }
            else
            {
                pObject->SetFlagBits( (pObject->GetFlagBits() & ~object::FLAG_CHECK_PLANES) | (1<<object::FLAG_CHECK_PLANES_SHIFT) );
            }

            // mark this object as needing to be rendered
            s32 ObjType = i;
            ASSERT( ObjType > (s32)object::TYPE_NULL );
            ObjSlot.Sequence = m_Sequence;
            ObjSlot.NextVis  = m_ObjectType[ObjType].FirstVis;
            m_ObjectType[ObjType].FirstVis = SlotID;
            m_ObjectType[ObjType].nVis++;
            ASSERT( m_VisibleSlotCount < MAX_OBJECTS );
            m_VisibleSlots[m_VisibleSlotCount++] = SlotID;

            // move to the next object
            SlotID = NextSlotID;
        }
    }

    visibilityCandidates.Add( VisibilityCandidates );
    visibleObjects.Add( m_VisibleSlotCount );
}

//==============================================================================

void obj_mgr::RenderNormalObjects( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::RenderNormalObjects" );

    for( s32 type=0; type<(s32)object::TYPE_END_OF_LIST; type++ )
    {
        // Editor special case: Solve path visiblity by hand since they fail the zone checks if they are huge
        // (see Render3dObjects for where they get rendered)
#ifdef X_EDITOR
        if ( type == object::TYPE_PATH )
            continue ;
#endif // X_EDITOR

        obj_type_node const& TypeInfo = m_ObjectType[type];

        // Skip descriptor hooks and callbacks when no instances are visible.
        if( ( TypeInfo.nVis > 0 ) &&
            TypeInfo.pDesc &&
            TypeInfo.pDesc->OnBeginRender() )
        {
#ifndef X_RETAIL
            // turn on clipping if the screen shot is active
            if ( eng_ScreenShotActive() )
            {
                for( s32 visible = 0; visible < TypeInfo.nVis; ++visible )
                {
                    slot_id const ID = m_PackedVisibleSlots[m_VisibleTypeStart[type] + visible];
                    object* pObject  = m_ObjectSlot[ID].pObject;
                    pObject->m_FlagBits = pObject->GetFlagBits() & ~object::FLAG_CHECK_PLANES;
                    pObject->SetFlagBits( pObject->GetFlagBits() | (1<<object::FLAG_CHECK_PLANES_SHIFT) );
                }
            }
#endif

            // Go through all the objects in the given type
            for( s32 visible = 0; visible < TypeInfo.nVis; ++visible )
            {
                slot_id const ID = m_PackedVisibleSlots[m_VisibleTypeStart[type] + visible];
                object* pObject = m_ObjectSlot[ID].pObject;

#ifdef X_EDITOR
                if( pObject->IsHidden() )
                    continue;
#endif

                // These guys rendered last.
                u32 const AttrBits = pObject->GetAttrBits();
                if( AttrBits & object::ATTR_DRAW_2D )
                    continue;

                // Render the object finally
                if( AttrBits & object::ATTR_TRANSPARENT )
                {
                    special_render_obj& SpecialObj = g_SpecialRenderObj.Append();
                    SpecialObj.pObject = pObject;
                }

#if !defined(X_RETAIL)
                if( m_TrapBeforeRenderGuid && (pObject->GetGuid() == m_TrapBeforeRenderGuid))
                    BREAK;
#endif

                pObject->OnRender();
            }
        
            // Done rendering this type
            TypeInfo.pDesc->OnEndRender();
        }
    }
}

//==============================================================================

#if !defined( X_RETAIL )
void obj_mgr::DisplayLocations( void )
{
    for( s32 type=object::TYPE_TRIGGER; type<(s32)object::TYPE_END_OF_LIST; type++ )
    {
        // If a type is renderable then render
        if( m_ObjectType[type].pDesc && m_ObjectType[type].pDesc->m_bDisplayLocation ) 
        {
            // Go through all the objects in the given type
            slot_id ID = m_ObjectType[type].FirstVis;
            for(; ID != SLOT_NULL; ID = GetNextVis( ID ) )
            {
                // Get the object
                object* pObject = GetObjectBySlot( ID );

                // Get the center of the object's bbox
                vector3 Center = pObject->GetBBox().GetCenter();

                // Render a label
                render::debug::Label( Center, XCOLOR_WHITE,"%04d", pObject->m_SlotID );
            }
        }
    }
}

//==============================================================================

void obj_mgr::RenderCollision( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::RenderCollision" );
    for( s32 type=object::TYPE_TRIGGER; type<(s32)object::TYPE_END_OF_LIST; type++ )
    {
        // If a type is renderable then render
        if( m_ObjectType[type].pDesc && 
            (m_ObjectType[type].pDesc->m_bRenderHiCollision ||
             m_ObjectType[type].pDesc->m_bRenderLoCollision)
          ) 
        {
            // Go through all the objects in the given type
            slot_id ID = m_ObjectType[type].FirstVis;
            for(; ID != SLOT_NULL; ID = GetNextVis( ID ) )
            {
                // Get the object
                object* pObject = GetObjectBySlot( ID );

                if( m_ObjectType[type].pDesc->m_bRenderHiCollision )
                    pObject->OnColRender( TRUE );

                if( m_ObjectType[type].pDesc->m_bRenderLoCollision )
                    pObject->OnColRender( FALSE );
            }
        }
    }

    if( m_ObjectType[object::TYPE_PLAY_SURFACE].pDesc->m_bRenderHiCollision )
        g_PlaySurfaceMgr.RenderPlaySurfacesCollision(TRUE);

    if( m_ObjectType[object::TYPE_PLAY_SURFACE].pDesc->m_bRenderLoCollision )
        g_PlaySurfaceMgr.RenderPlaySurfacesCollision(FALSE);
}
#endif

//==============================================================================

void obj_mgr::RenderPlaySurfaces( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::RenderPlaySurfaces" );
    g_PlaySurfaceMgr.RenderPlaySurfaces();
}

//==============================================================================

s32 TransparentSortFn( const void* pItem1, const void* pItem2 )
{
    special_render_obj* pObj1 = (special_render_obj*)pItem1;
    special_render_obj* pObj2 = (special_render_obj*)pItem2;

    if ( pObj1->ZDist > pObj2->ZDist ) return -1;
    if ( pObj1->ZDist < pObj2->ZDist ) return 1;
    
    return 0;
}

//==============================================================================

void obj_mgr::RenderSpecialObjects( void )
{
    xbool const bSceneCapture = render::GetActiveRenderScopeDesc().Purpose == render::RENDER_VIEW_SCENE_CAPTURE;

    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::RenderSpecialObjects" );
    LOG_STAT( k_stats_OtherRender );

    if( eng_Begin( "Special Objects" ) )
    {
#ifndef X_RETAIL
        if( !bSceneCapture )
            g_NavMap.RenderNavNearView();
#endif

        // Enter primitive submission mode for tracers, particles, and the
        // remaining transparent world primitives.

#ifndef X_RETAIL
        // Collect debug overlays before committing the render graph.
        if( !bSceneCapture )
            g_PolyCache.Render();
#endif

#ifdef ENABLE_PHYSICS_DEBUG
        if( !bSceneCapture )
            g_PhysicsMgr.DebugRender();
#endif

        g_TracerMgr.Render();

        if( g_SpecialRenderObj.GetCount() )
        {
            // get the object's position in view space and store off their z-depth in preparation for sort
            s32 i;
            const view* pActiveView = eng_GetView();
            f32 previousZ = 0.0f;
            xbool alreadySorted = TRUE;
            for( i=0; i<g_SpecialRenderObj.GetCount(); i++ )
            {
                special_render_obj& SpecialObj = g_SpecialRenderObj[i];
                
                vector3 Pos = SpecialObj.pObject->GetPosition();
                Pos         = pActiveView->ConvertW2V( Pos );
                SpecialObj.ZDist = Pos.GetZ();
                if( ( i > 0 ) && ( SpecialObj.ZDist > previousZ ) )
                    alreadySorted = FALSE;
                previousZ = SpecialObj.ZDist;
            }

            // sort the transparent objects from back to front
            // qsort complains when there are 0 items to sort since the pointer acquired from g_SpecialRenderObj.GetPtr() is NULL
            // as no data has yet been allocated.
            if( !alreadySorted && ( g_SpecialRenderObj.GetCount() > 1 ) )
            {
                x_qsort( g_SpecialRenderObj.GetPtr(), g_SpecialRenderObj.GetCount(), sizeof(special_render_obj), TransparentSortFn );
            }

            // now render them
            for( i=0; i<g_SpecialRenderObj.GetCount(); i++ )
            {
#if !defined(X_RETAIL)
                if( m_TrapBeforeRenderGuid && (g_SpecialRenderObj[i].pObject->GetGuid() == m_TrapBeforeRenderGuid))
                    BREAK;
#endif

    #ifdef X_EDITOR
                if( g_SpecialRenderObj[i].pObject->IsHidden() == FALSE )
    #endif
                {
                    g_SpecialRenderObj[i].pObject->OnRenderTransparent();
                }
            }
        }

        #ifndef X_EDITOR
        // Render the MP contagion effects.
        if( GameMgr.GetGameType() != GAME_CAMPAIGN )
        {
            actor* pActor;
            for( s32 i = 0; i < 32; i++ )
            {
                pActor = (actor*)NetObjMgr.GetObjFromSlot( i );
                if( pActor )
                    pActor->RenderContagion();
            }
        }
        #endif

        // Close collection, draw opaque/decals, composite scene fog, then draw
        // forward geometry with its own per-pixel fog.
        if( !bSceneCapture )
        {
#ifdef X_EDITOR
            g_DecalMgr.OnRenderEditorWireframes();
#endif
        }
        g_PostEffectMgr.RenderFog();
        if( !bSceneCapture )
        {
            new_weapon::CreateScopeTexture();
            g_PostEffectMgr.Render();
        }
        eng_End();
    }

    //x_printfxy( 0, 11, "SpecialRender=%3.3fms", Temp.StopMs() );

    if( !bSceneCapture )
        FXMgr.EndOfFrame();
}

//==============================================================================

void obj_mgr::Render2dObjects( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::Render - 2d Objects" );
    LOG_STAT( k_stats_OtherRender );

    static xprofile_counter objects2dVisited =
        x_GetProfiler().RegisterCounter( "ObjMgr2DObjectsVisited", "RenderCounter" );
    static xprofile_counter objects2dRendered =
        x_GetProfiler().RegisterCounter( "ObjMgr2DObjectsRendered", "RenderCounter" );
    s32 Objects2dVisited = 0;
    s32 Objects2dRendered = 0;

#ifndef X_RETAIL
    if( eng_ScreenShotActive() )
        return;
#endif

    if( eng_Begin( "2d Objects" ) )
    {
        // Go through all the object types that have the Draw 2D flags set on them.
        for( s32 i = object::TYPE_NULL; (u32)i < object::TYPE_END_OF_LIST; i++ )
        {
            if( m_ObjectType[i].pDesc && ( m_ObjectType[i].InstanceCount > 0 ) )
            {
                if( m_ObjectType[i].pDesc->m_CommonAttrBits & object::ATTR_DRAW_2D )
                {
                    // Go through all the object of this type from the list.
                    slot_id SlotID = m_ObjectType[i].FirstType;
                    while( SlotID != SLOT_NULL )
                    {
                        object* pObj = g_ObjMgr.GetObjectBySlot( SlotID );
                        Objects2dVisited++;

                        // Does this specific object have the Draw 2D flag set?
                        if( pObj->GetAttrBits() & object::ATTR_DRAW_2D )
                        {
#if !defined(X_RETAIL)
                            if( m_TrapBeforeRenderGuid && (pObj->GetGuid() == m_TrapBeforeRenderGuid))
                                BREAK;
#endif

#ifdef X_EDITOR
                            if( pObj->IsHidden() == FALSE )
#endif // X_EDITOR
                            {
                                pObj->OnRender();
                                Objects2dRendered++;
                            }
                        }

                        SlotID = g_ObjMgr.GetNext( SlotID );
                    }
                }
            }
        }
        objects2dVisited.Add( Objects2dVisited );
        objects2dRendered.Add( Objects2dRendered );
        eng_End();
    }
}

//==============================================================================

void obj_mgr::CollectVisibleLights( void )
{
    for( s32 Type = (s32)object::TYPE_LIGHT; Type <= (s32)object::TYPE_DYNAMIC_LIGHT; Type++ )
    {
        if( !m_ObjectType[Type].pDesc )
            continue;

        for( slot_id ID = m_ObjectType[Type].FirstType; ID != SLOT_NULL; ID = GetNext( ID ) )
        {
            object* pObject = m_ObjectSlot[ID].pObject;
            ASSERT( pObject );

#ifdef X_EDITOR
            if( pObject->IsHidden() )
                continue;
#endif

            if( IsBoxInView( pObject->GetBBox(), XBIN(111111) ) == -1 )
                continue;

            pObject->OnCollectLight();
        }
    }

    // The player owns the runtime flashlight light directly.
    for( slot_id ID = m_ObjectType[object::TYPE_PLAYER].FirstType; ID != SLOT_NULL; ID = GetNext( ID ) )
    {
        object* pObject = GetObjectBySlot( ID );
        ASSERT( pObject );
        pObject->OnCollectLight();
    }
}

//==============================================================================

void obj_mgr::CompleteVisibilityTests( void )
{
    // Keep the active player visible when the bbox briefly falls outside the
    // frustum while looking up or crouching.
    player* pActivePlayer = SMP_UTIL_GetActivePlayer();
    if( !pActivePlayer )
        return;

    pActivePlayer->SetFlagBits( ( pActivePlayer->GetFlagBits() & ~object::FLAG_CHECK_PLANES ) |
                                ( 1 << object::FLAG_CHECK_PLANES_SHIFT ) );

    slot_id   ActivePlayerSlot    = pActivePlayer->GetSlot();
    obj_slot& ActivePlayerObjSlot = m_ObjectSlot[ActivePlayerSlot];
    if( ActivePlayerObjSlot.Sequence == m_Sequence )
        return;

    ActivePlayerObjSlot.Sequence = m_Sequence;
    ActivePlayerObjSlot.NextVis  = m_ObjectType[object::TYPE_PLAYER].FirstVis;
    m_ObjectType[object::TYPE_PLAYER].FirstVis = ActivePlayerSlot;
    m_ObjectType[object::TYPE_PLAYER].nVis++;
    ASSERT( m_VisibleSlotCount < MAX_OBJECTS );
    m_VisibleSlots[m_VisibleSlotCount++] = ActivePlayerSlot;
}

//==============================================================================

void obj_mgr::BuildPackedVisibleSlots( void )
{
    s32 writeOffsets[object::TYPE_END_OF_LIST];
    s32 packedCount = 0;

    for( s32 type = 0; type < (s32)object::TYPE_END_OF_LIST; ++type )
    {
        m_VisibleTypeStart[type] = packedCount;
        writeOffsets[type] = packedCount;
        packedCount += m_ObjectType[type].nVis;
    }

    ASSERT( packedCount == m_VisibleSlotCount );

    // Reverse the discovery array to preserve FirstVis/NextVis order.
    for( s32 i = m_VisibleSlotCount - 1; i >= 0; --i )
    {
        slot_id const SlotID = m_VisibleSlots[i];
        object* const pObject = m_ObjectSlot[SlotID].pObject;
        ASSERT( pObject );
        s32 const type = pObject->GetType();
        ASSERT( ( type >= 0 ) && ( type < (s32)object::TYPE_END_OF_LIST ) );
        m_PackedVisibleSlots[writeOffsets[type]++] = SlotID;
    }
}

void obj_mgr::Render3dPrep( xbool DoPortalWalk, const view& PortalView, const zone_mgr::tracker& ViewZones )
{
    u8 const StartZone = ViewZones.GetMainZone();

    // Prepare visibility and other per-frame state for 3D rendering.
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::Render3dPrep" );

    // Update the OccluderMgr with the latest view
    #ifdef X_EDITOR
    g_OccluderMgr.GatherOccluders();
    #endif
    g_OccluderMgr.SetView( PortalView );

    // create the list of visible objects and they're clipping flags
    if ( DoPortalWalk )
        g_ZoneMgr.PortalWalk( PortalView, ViewZones );
    else
        g_ZoneMgr.TurnOff();
    DoVisibilityTests( PortalView );

    CompleteVisibilityTests();
    BuildPackedVisibleSlots();

    // clear the list of dynamic and character lights so visible light objects can add them back in
    render::GetRenderPipeline().GetLightSystem().ClearLights( PortalView.GetPosition() );
    CollectVisibleLights();

    xbool EnableShadows = render::GetActiveRenderScopeDesc().EnableShadows;
#ifndef X_RETAIL
    EnableShadows = EnableShadows && !render::GetRenderPipeline().GetDebugOptions().DisableShadowMaps;
#endif
    if ( !EnableShadows )
        render::GetRenderPipeline().GetShadowMapAtlas().ClearSources();
    else
        render::GetRenderPipeline().GetShadowMapAtlas().CreateShadowMap();

    // Clear the list of special render objects;
    g_SpecialRenderObj.Delete(0, g_SpecialRenderObj.GetCount() );

    // set up the environment map
    cubemap::handle Handle;
    Handle.SetName( PRELOAD_FILE("DefaultEnvMap.envmap") );

    if( DoPortalWalk && (StartZone < g_ZoneMgr.GetZoneCount()) )
    {
        const char* pEnvMap = g_ZoneMgr.GetZoneEnvMap( StartZone );
        if( pEnvMap && pEnvMap[0] )
            Handle.SetName( pEnvMap );
    }

//#define cubemapverb
#ifdef cubemapverb
    x_DebugMsg( "EnvMap: Zone=%d Name=[%s]\n", StartZone, Handle.GetName() );
#endif    

    render::SetAreaCubeMap( Handle );
}

//==============================================================================

xbool g_bRenderPlaysurfaces = TRUE;

void obj_mgr::Render3dObjects( xbool bDoPortalWalk, const view& PortalView, const zone_mgr::tracker& ViewZones )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::Render3dObjects" );
    LOG_STAT( k_stats_HighLevelRender );

    if( g_ZoneMgr.GetPortalCount() == 0 ) bDoPortalWalk = FALSE;
    Render3dPrep( bDoPortalWalk, PortalView, ViewZones );
    {
        // Handle the normal rendering
        if( eng_Begin( "3d Objects" ) )
        {

            {
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/LightBegin" );
                render::GetRenderPipeline().GetLightSystem().BeginLightCollection();
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/BeginRenderScope" );
                if ( !render::BeginRenderScope() )
                {
                    render::GetRenderPipeline().GetLightSystem().EndLightCollection();
                    eng_End();
                    return;
                }
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/CollectObjects" );
                RenderNormalObjects();
            }

            // Editor special case: Solve path visiblity by hand since they fail the zone checks if they are huge
#ifdef X_EDITOR
            {
                // Check all paths
                if (m_ObjectType[object::TYPE_PATH].pDesc && m_ObjectType[object::TYPE_PATH].pDesc->CanRender())
                {
                    slot_id ID = GetFirst( object::TYPE_PATH ) ;
                    while ( ID != SLOT_NULL )
                    {
                        // Lookup path
                        object* pPath = GetObjectBySlot( ID ) ;
                        if( pPath && (pPath->IsHidden()==FALSE))
                        {
                            // Render if in view
                            if (PortalView.BBoxInView( pPath->GetBBox() ))
                                pPath->GetTypeDesc().OnEditorRender( *pPath );
                        }

                        // Check next
                        ID = GetNext( ID ) ;
                    }
                }
            }
#endif // X_EDITOR

            if( g_bRenderPlaysurfaces )
            {
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/PlaySurfaces" );
                RenderPlaySurfaces();
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/Decals" );
                g_DecalMgr.OnRender();
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/LightEnd" );
                render::GetRenderPipeline().GetLightSystem().EndLightCollection();
            }

            // Check if any types need their collision rendered
#ifndef X_RETAIL
            {
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/Collision" );
                RenderCollision();
            }
#endif
            {
                LOG_STAT( k_stats_ObjectRender );
                X_PROFILE_SCOPE_CATEGORY( "RenderSection", "3D/SubmitRenderScope" );
                if ( !render::SubmitRenderScope() )
                {
                    eng_End();
                    return;
                }
            }

#ifdef X_EDITOR
            g_NavMap.RenderConnectionsBright();
#endif
            eng_End();
        }
    }
    {
        LOG_STAT( k_stats_OtherRender );
        RenderSpecialObjects();
    }
}

//==============================================================================

void obj_mgr::Render( xbool bDoPortalWalk, const view& PortalView, const zone_mgr::tracker& ViewZones )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "obj_mgr::Render" );
    LOG_STAT( k_stats_HighLevelRender );

    slot_id SlotID ;
    (void)SlotID;

    #if ENABLE_RENDER_STATS
    render::GetStats().Begin();
    #endif

    {
        SlotID = m_ObjectType[object::TYPE_PIP].FirstType;
        while( SlotID != SLOT_NULL )
        {
            pip* pPip = (pip*)g_ObjMgr.GetObjectBySlot( SlotID );
            ASSERT( pPip );
            if( pPip->GetState() == pip::STATE_ACTIVE )
            {
                pPip->RenderView();
            }

            SlotID = m_ObjectSlot[SlotID].Next;
        }
    }

    // Render 3d scene
    Render3dObjects( bDoPortalWalk, PortalView, ViewZones );

#ifdef X_EDITOR
    // Render selected cameras (editor only)
    xbool bCameraRendered = FALSE;
    SlotID = m_ObjectType[object::TYPE_CAMERA].FirstType;
    while( SlotID != SLOT_NULL )
    {
        // Get camera and render
        camera* pCamera = (camera*)g_ObjMgr.GetObjectBySlot( SlotID );
        ASSERT(pCamera);
        if( pCamera->IsEditorSelected() ) 
        {
            // Render camera and mark
            pCamera->RenderEditorView();
            bCameraRendered = TRUE;
            break;
        }

        // Check next
        SlotID = m_ObjectSlot[SlotID].Next;
    }

    // Render active pips if no selected camera was drawn
    if (!bCameraRendered)
    {
        SlotID = m_ObjectType[object::TYPE_PIP].FirstType;
        while( SlotID != SLOT_NULL )
        {
            // Get pip and render
            pip* pPip = (pip*)g_ObjMgr.GetObjectBySlot( SlotID );
            ASSERT(pPip);
            if (pPip->GetState() == pip::STATE_ACTIVE)
            {
                pPip->RenderView();
                bCameraRendered = TRUE;
                break;
            }                

            // Check next
            SlotID = m_ObjectSlot[SlotID].Next;
        }
    }
    
    // So that icons are drawn in the editor, this functions needs to be called
    // again, otherwise only the icons that are visible in the camera will be drawn.
    if (bCameraRendered)
        Render3dPrep( bDoPortalWalk, PortalView, ViewZones );
#endif // X_EDITOR

    // Render HUD
    Render2dObjects();

    // Render pip hud objects
    SlotID = m_ObjectType[object::TYPE_PIP].FirstType;
    while( SlotID != SLOT_NULL )
    {
        // Get pip and render
        pip* pPip = (pip*)g_ObjMgr.GetObjectBySlot( SlotID );
        ASSERT(pPip);
        if ( (pPip->IsActive()) && (pPip->GetType() == pip::TYPE_HUD) )
        {
            if( eng_Begin("Pip") )
            {
                pPip->OnRender();
                eng_End();
            }
        }

        // Check next
        SlotID = m_ObjectSlot[SlotID].Next;
    }

    // Render screen fades after the 2d stuff so that the hud will also fade
    eng_Begin( "Screen Fade" );
    g_PostEffectMgr.RenderScreenFade();
    eng_End();

    #if ENABLE_RENDER_STATS
    render::GetStats().End();
    #endif

    //
    // Render icons and debug info
    //
#if !defined( X_RETAIL )
    DisplayLocations();
#endif // X_RETAIL

#ifdef ENABLE_COLLISION_STATS
    // Render collisions times
    if( 1 )
    {
        if( eng_Begin("CollisionMgr") )
        {
            g_CollisionMgr.Render(); 
            eng_End();
        }
    }
#endif // ENABLE_COLLISION_STATS
}

//==============================================================================

void object_desc::OnEnumProp( prop_enum& List )
{
    List.PropEnumHeader( "ObjectDesc", "This is the object description information of an object", 0 );
    List.PropEnumString( "ObjectDesc\\TypeName", "This is the name of the type been descrive", PROP_TYPE_READ_ONLY );
    List.PropEnumInt   ( "ObjectDesc\\TypeID",   "This is the ID of the type been descrive", PROP_TYPE_READ_ONLY );
    List.PropEnumInt   ( "ObjectDesc\\Count",    "The total count of this type of object", PROP_TYPE_READ_ONLY );


    if( m_CommonAttrBits&object::ATTR_RENDERABLE )
        List.PropEnumBool   ( "ObjectDesc\\Render",   "This tells whether all the object of this type should render or not.", 0 );

    if( m_CommonAttrBits&(object::ATTR_NEEDS_LOGIC_TIME) )
        List.PropEnumBool   ( "ObjectDesc\\Logic",   "This tells whether all the object of this type should run logic or not.", 0 );    
}

//==============================================================================

xbool object_desc::OnProperty( prop_query&  I )
{
    s32 Type = (s32)m_Type;
    if( I.VarString( "ObjectDesc\\TypeName", (char*)m_pTypeName, 256 ) )
    {
        // You can only read this guy
        ASSERT( I.IsRead() == TRUE );
    }
    else if( I.VarInt( "ObjectDesc\\TypeID", Type ) )
    {
    }
    else if( I.VarBool( "ObjectDesc\\Render", m_bRender ) )
    {
    }
    else if( I.VarBool( "ObjectDesc\\Logic", m_bLogic ) )
    {
    }
    else if( I.VarInt( "ObjectDesc\\Count", m_ObjectCount ) )
    {
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void obj_mgr::DisplayStats( void )
{
    s32 i;
    s32 C=0;

    for( i=0; i<(s32)object::TYPE_END_OF_LIST; i++ )
    if( m_ObjectType[i].pDesc )
    {
        x_DebugMsg("%3d] %4d %s\n",i,m_ObjectType[i].InstanceCount,m_ObjectType[i].pDesc->GetTypeName());
        C += m_ObjectType[i].InstanceCount;
    }
    x_DebugMsg("TOTAL: %d\n",C);
}

//==============================================================================

#if !defined(X_RETAIL)
void obj_mgr::SlotSanityCheck( void )
{
    // THIS ROUTINE WILL BE SLOW!!! Please only use it when debugging a specific
    // crash or infinite loop.

    s32 i, j;
    
    // check the zone links
    for( i = 0; i < g_ZoneMgr.GetZoneCount(); i++ )
    {
        for( j = 0; j < 2; j++ )
        {
            #ifdef X_ASSERT
            s32 SafetyCount = 0;
            #endif

            slot_id Slot = m_ObjectZone[i].FirstZone[j];
            ASSERT( (Slot == SLOT_NULL) || (m_ObjectSlot[Slot].PrevZone[j] == SLOT_NULL) );
            while( Slot != SLOT_NULL )
            {
                slot_id Next = m_ObjectSlot[Slot].NextZone[j];
                slot_id Prev = m_ObjectSlot[Slot].PrevZone[j];
                (void)Prev;

                // verify the previous and next links are valid
                ASSERT( (Next == SLOT_NULL) ||
                        (Slot == m_ObjectSlot[Next].PrevZone[j]) );
                ASSERT( (Prev == SLOT_NULL) ||
                        (Slot == m_ObjectSlot[Prev].NextZone[j]) );
                ASSERT( m_ObjectSlot[Slot].pObject );

                // verify the object is in the zone we think he is
                if( j == 0 )
                {
                    ASSERT( m_ObjectSlot[Slot].pObject->GetZone1() == i );
                }
                else
                {
                    ASSERT( m_ObjectSlot[Slot].pObject->GetZone2() == i );
                }

                // if we hit the safety count, we've gotten caught in
                // an infinite loop
                ASSERT( ++SafetyCount <= MAX_OBJECTS );

                Slot = Next;
            }

            #ifdef X_ASSERT
            SafetyCount = 0;
            #endif

            Slot = m_ObjectZone[i].FirstRenderable[j];
            ASSERT( (Slot == SLOT_NULL) || (m_ObjectSlot[Slot].PrevRenderable[j] == SLOT_NULL) );
            while( Slot != SLOT_NULL )
            {
                slot_id Next = m_ObjectSlot[Slot].NextRenderable[j];
                slot_id Prev = m_ObjectSlot[Slot].PrevRenderable[j];
                (void)Prev;

                // verify the previous and next links are valid
                ASSERT( (Next == SLOT_NULL) ||
                        (Slot == m_ObjectSlot[Next].PrevRenderable[j]) );
                ASSERT( (Prev == SLOT_NULL) ||
                        (Slot == m_ObjectSlot[Prev].NextRenderable[j]) );
                ASSERT( m_ObjectSlot[Slot].pObject );

                // verify the object is in the zone we think he is
                if( j == 0 )
                {
                    ASSERT( m_ObjectSlot[Slot].pObject->GetZone1() == i );
                }
                else
                {
                    ASSERT( m_ObjectSlot[Slot].pObject->GetZone2() == i );
                }

                // if we hit the safety count, we've gotten caught in
                // an infinite loop
                ASSERT( ++SafetyCount <= MAX_OBJECTS );

                Slot = Next;
            }
        }
    }
}

//==============================================================================

void obj_mgr::DumpStatsToFile( const char* pFileName )
{
    X_FILE* pFile = x_fopen( pFileName, "w" );
    if( pFile )
    {
        s32             i;
        s32             CountTotal = 0;
        xarray<s32>     CountType;

        // Init array to count with
        CountType.SetCapacity( object::TYPE_END_OF_LIST );
        CountType.SetCount( object::TYPE_END_OF_LIST );
        for( i=0 ; i<(s32)object::TYPE_END_OF_LIST ; i++ )
            CountType[i] = 0;

        // Loop over the objects counting them
        for( i=0 ; i<MAX_OBJECTS ; i++ )
        {
            object* pObject = m_ObjectSlot[i].pObject;
            if( pObject != NULL )
            {
                CountTotal++;
                CountType[pObject->GetType()]++;
            }
        }

        // Output the summary
        for( i=0 ; i<(s32)object::TYPE_END_OF_LIST ; i++ )
        {
            const object_desc* pDesc = g_ObjMgr.GetTypeDesc((object::type)i);
            if( pDesc )
                x_fprintf( pFile, "%04d,%s\n", CountType[i], pDesc->GetTypeName() );
        }
        x_fprintf( pFile, "\n%04d,Total\n", CountTotal );

        // Close the file
        x_fclose( pFile );
    }
}
#endif // !defined( X_RETAIL )
