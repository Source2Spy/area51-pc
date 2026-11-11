//==============================================================================
//
//  PhysicsMgr.cpp
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "PhysicsMgr.hpp"
#include "Collider.hpp"
#include "CollisionShape.hpp"
#include "Entropy.hpp"
#include "GameLib/StatsMgr.hpp"
#include "Objects/Actor/Actor.hpp"
#include "Obj_mgr/obj_mgr.hpp"

#ifdef ENABLE_PHYSICS_DEBUG
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#endif


//==============================================================================
// GLOBAL DATA
//==============================================================================
collision_shape     g_WorldColl;                    // World collision shape
rigid_body          g_WorldBody;                    // World rigid body

collision_shape     g_ActorColl;                    // Actor collision shape
rigid_body          g_ActorBody;                    // Actor rigid body
actor*              g_pCollisionActor = NULL;       // Current actor being tested against


physics_mgr         g_PhysicsMgr;                   // Global physics manager


//==============================================================================
// SORT FUNCTIONS
//==============================================================================

// Sorts collisions from lowest to highest
s32 collision_sort_lo_to_hi( const void* pItem1, const void* pItem2 )
{
    // Lookup collision struct pointers
    physics_mgr::collision* pColl0 = (physics_mgr::collision*)pItem1;
    physics_mgr::collision* pColl1 = (physics_mgr::collision*)pItem2;

    // Lookup world collision Y
    f32 Y0 = pColl0->m_Position.GetY();
    f32 Y1 = pColl1->m_Position.GetY();

    // Sort from lowest to highest
    if( Y0 > Y1 )
        return 1;
    else
        if( Y0 < Y1 )
            return -1;
        else
            return 0;
}

//==============================================================================

// Sorts collisions from highest to lowest
s32 collision_sort_hi_to_lo( const void* pItem1, const void* pItem2 )
{
    // Lookup collision struct pointers
    physics_mgr::collision* pColl0 = (physics_mgr::collision*)pItem1;
    physics_mgr::collision* pColl1 = (physics_mgr::collision*)pItem2;

    // Lookup world collision Y
    f32 Y0 = pColl0->m_Position.GetY();
    f32 Y1 = pColl1->m_Position.GetY();

    // Sort from highest to lowest
    if( Y0 > Y1 )
        return -1;
    else
        if( Y0 < Y1 )
            return 1;
        else
            return 0;
}

//==============================================================================
// PHYSICS FUNCTIONS
//==============================================================================

physics_mgr::physics_mgr()
{
    // Settings
    m_Settings.m_MaxSubstepSeconds          = 1.0f / 30.0f;     // Maximum substep duration
    m_Settings.m_Gravity.Set( 0, -9.81f * 100.0f * 2.0f, 0 );   // World gravity
    
    m_Settings.m_nMaxCollisions             = 400;              // Maximum # of collisions
    m_Settings.m_nCollisionIterations       = 4;                // # of iterations to solve collision
    m_Settings.m_bInstInstCollision         = TRUE;             // Collide physics instance with others?
    m_Settings.m_bSelfCollision             = TRUE;             // Collide bodies within physics instance?
    m_Settings.m_CollisionHitBackoffDist    = 1.0f;             // Dist to backoff when a collision occurs
    m_Settings.m_RestitutionMinSpeed        = 100.0f;           // Minimum bounce speed
    m_Settings.m_PositionCorrectionRate     = 7.0f;             // Contact correction rate
    m_Settings.m_MaxContactCorrectionSpeed  = 200.0f;           // Maximum contact correction speed in cm/s
    m_Settings.m_ContactSlop                = 0.5f;             // Allowed penetration in cm
    
    m_Settings.m_bSolveContacts             = TRUE;             // Solve contacts?
    m_Settings.m_nContactIterations         = 5;                // # of iterations to solve contact
    
    m_Settings.m_bShock                     = TRUE;             // Do shock propagation?
    
    m_Settings.m_nMaxActiveConstraints      = 10*56;            // Maximum # of active constraints (56 per ragdoll)
    m_Settings.m_MaxConstraintCorrectionSpeed = 6000.0f;        // Maximum joint correction speed
    
    m_Settings.m_ActiveLinearSpeed          = 30.0f;            // Translation speed at which body is considered active
    m_Settings.m_ActiveAngularSpeed         = 4.0f;             // Rotation speed at which body is considered active
    m_Settings.m_bDeactivate                = TRUE;             // Allow rigid bodies to deactivate
    m_Settings.m_DeactivateStartTime        = 0.5f;             // Time to start blending to deactivate
    m_Settings.m_DeactivateEndTime          = 1.0f;             // Time before being fully deactivate

    m_Settings.m_MaxLinearVel               = 1500.0f;          // Max linear velocity
    m_Settings.m_MaxAngularVel              = 25.0f;            // Max angular velocity
    m_Settings.m_MaxForce                   = 18686.0f*3.0f;    // Max allowed force
    
    m_Settings.m_nRenderIterations          = 10;               // # of render jitter fix iterations
    
    m_Settings.m_bShowStats                 = FALSE;            // Show stats info?
    m_Settings.m_bDebugRender               = FALSE;            // Render debug info?
    m_Settings.m_bUsePolycache              = TRUE;             // Use world collision or ground plane?

    // Collision
    m_NextCollisionGroup = -1;
#ifdef ENABLE_PHYSICS_DEBUG
    
    // Clear profile stats
    m_Profile.m_nActiveInsts             = 0;
    m_Profile.m_nActiveBodies            = 0;
    m_Profile.m_nActiveSpheres           = 0;
    m_Profile.m_nActiveConstraints       = 0;
    m_Profile.m_nCollisions              = 0;
    m_Profile.m_nPrepConstraints         = 0;
    m_Profile.m_nSolveConstraints        = 0;
    m_Profile.m_nBodyBodyCollTests       = 0;
    m_Profile.m_nSphereSphereCollTests   = 0;
    m_Profile.m_nCapsuleCapsuleCollTests = 0;
    m_Profile.m_nSphereCapsuleCollTests  = 0;
    m_Profile.m_nClusters                = 0;
    m_Profile.m_nNGons                   = 0;
    m_Profile.m_nSphereNGonCollTests     = 0;
    m_Profile.m_nSphereNGonIntersecTests = 0;
    m_Profile.m_MaxLinearVel             = 0;
    m_Profile.m_MaxAngularVel            = 0;
    m_Profile.m_MaxForce                 = 0;
    m_Profile.m_StaticMemoryUsed         = 0;
    m_Profile.m_DynamicMemoryUsed        = 0;
    
    PHYSICS_DEBUG_STATIC_MEM_ALLOC( sizeof( physics_mgr ) );
    
#endif  //#ifdef ENABLE_PHYSICS_DEBUG
}

//==============================================================================

physics_mgr::~physics_mgr()
{
    PHYSICS_DEBUG_STATIC_MEM_FREE( sizeof( physics_mgr ) );
}

//==============================================================================

void physics_mgr::Init( void )
{
    PHYSICS_DEBUG_STATIC_MEM_ALLOC( sizeof( collision )         * m_Settings.m_nMaxCollisions );
    PHYSICS_DEBUG_STATIC_MEM_ALLOC( sizeof( active_constraint ) * m_Settings.m_nMaxActiveConstraints );

    // Allocate arrays
    m_Collisions.SetCapacity( m_Settings.m_nMaxCollisions );
    m_ActiveConstraints.SetCapacity( m_Settings.m_nMaxActiveConstraints );
    
    // Setup world collision and rigid body
    g_WorldColl.SetType( collision_shape::TYPE_WORLD );
    g_WorldBody.SetCollisionShape( &g_WorldColl, 0.0f, 100.0f );
    g_WorldBody.SetElasticity( 1.0f );
    g_WorldBody.SetDynamicFriction( 1.0f );
    g_WorldBody.SetStaticFriction( 1.0f );

    // Setup actor collision and rigid body
    g_ActorColl.SetType( collision_shape::TYPE_CAPSULE );
    g_ActorColl.SetSphereCapacity( 2 );
    g_ActorColl.AddSphere( vector3( 0.0f, 0.0f, 0.0f ) );
    g_ActorColl.AddSphere( vector3( 0.0f, 0.0f, 0.0f ) );
    g_ActorBody.SetCollisionShape( &g_ActorColl, 0.0f, 100.0f );
    g_ActorBody.SetElasticity( 1.0f );
    g_ActorBody.SetDynamicFriction( 1.0f );
    g_ActorBody.SetStaticFriction( 1.0f );
    
}

//==============================================================================

void physics_mgr::Kill( void )
{
    PHYSICS_DEBUG_STATIC_MEM_FREE( sizeof( collision )         * m_Settings.m_nMaxCollisions );
    PHYSICS_DEBUG_STATIC_MEM_FREE( sizeof( active_constraint ) * m_Settings.m_nMaxActiveConstraints );

    // Release collision memory
    m_Collisions.Clear();
    m_ActiveConstraints.Clear();
    
    // Make sure all lists are empty
    ASSERT( m_AwakeInstances.GetCount() == 0 );
    ASSERT( m_SleepingInstances.GetCount() == 0 );
    ASSERT( m_ActiveBodies.GetCount() == 0 );
    ASSERT( m_ActiveConstraints.GetCount() == 0 );
    ASSERT( m_SolveConstraints.GetCount() == 0 );
}

//==============================================================================
// Physics instance functions
//==============================================================================

void physics_mgr::AddInstance( physics_inst* pInstance )
{
    // Instance should be initialized, but not in ANY lists
    ASSERT( pInstance->m_isInitialized           == FALSE );
    ASSERT( pInstance->m_bInAwakeList           == FALSE );
    ASSERT( pInstance->m_bInSleepingList        == FALSE );
    ASSERT( pInstance->m_bInCollisionWakeupList == FALSE );
    
    // Add to sleeping list
    m_SleepingInstances.Append( pInstance );
    pInstance->m_bInSleepingList = TRUE;

    // Flag as ready    
    pInstance->m_isInitialized = TRUE;
}

//==============================================================================

void physics_mgr::RemoveInstance( physics_inst* pInstance )
{
    // Not in physics mgr?
    if( pInstance->m_isInitialized == FALSE )
    {
        ASSERT( pInstance->m_bInAwakeList           == FALSE );
        ASSERT( pInstance->m_bInSleepingList        == FALSE );
        ASSERT( pInstance->m_bInCollisionWakeupList == FALSE );
        return;
    }
    
    // Instance should be initialized, but never in BOTH lists
    ASSERT( pInstance->m_isInitialized  == TRUE );
    ASSERT( !( ( pInstance->m_bInAwakeList == TRUE ) && ( pInstance->m_bInSleepingList == TRUE ) ) );
    
    // Remove from awake list?
    if( pInstance->m_bInAwakeList )
    {
        m_AwakeInstances.Remove( pInstance );
        pInstance->m_bInAwakeList = FALSE;
    }

    // Remove from sleeping list?
    if( pInstance->m_bInSleepingList )
    {
        m_SleepingInstances.Remove( pInstance );
        pInstance->m_bInSleepingList = FALSE;
    }

    // Should have been removed from both lists!
    ASSERT( pInstance->m_bInAwakeList    == FALSE );
    ASSERT( pInstance->m_bInSleepingList == FALSE );
    
    // Activate any overlapping sleeping instances so 
    // that they aren't left floating in the air
    physics_inst* pSleepingInst = m_SleepingInstances.GetHead();
    while( pSleepingInst )
    {
        // Validate list management
        ASSERT( pSleepingInst->m_isInitialized           == TRUE  );
        ASSERT( pSleepingInst->m_bInAwakeList           == FALSE );
        ASSERT( pSleepingInst->m_bInSleepingList        == TRUE  );
        ASSERT( pSleepingInst->m_bInCollisionWakeupList == FALSE );
    
        // Lookup next instance in-case instance is woken up!
        physics_inst* pNextInst = m_SleepingInstances.GetNext( pSleepingInst );
        
        // Quick zone check - in same zone?
        if( pInstance->GetZone() == pSleepingInst->GetZone() )
        {
            // Overlap?
            if( pInstance->GetWorldBBox().Intersect( pSleepingInst->GetWorldBBox() ) )
            {
                // Activate
                pSleepingInst->Activate();
            }
        }

        // Check next
        pSleepingInst = pNextInst;
    }
    
    // Flag as not ready any more
    pInstance->m_isInitialized = FALSE;
}

//==============================================================================

void physics_mgr::WakeupInstance( physics_inst* pInstance )
{
    // Should be initialized
    ASSERT( pInstance->m_isInitialized );

    // Already awake?
    if( pInstance->m_bInAwakeList )
    {
        // Should not be in sleeping or collision wakeup list also!
        ASSERT( pInstance->m_bInSleepingList        == FALSE );
        ASSERT( pInstance->m_bInCollisionWakeupList == FALSE );
        return;
    }
    
    // Remove from sleeping list?
    if( pInstance->m_bInSleepingList )
    {
        m_SleepingInstances.Remove( pInstance );
        pInstance->m_bInSleepingList = FALSE;
    }

    // Remove from collision wakeup list?
    if( pInstance->m_bInCollisionWakeupList )
    {
        m_CollisionWakeupInstances.Remove( pInstance );
        pInstance->m_bInCollisionWakeupList = FALSE;
    }

    // Add to front of awake list in case we came here from collision detection
    // so that this instances isn't double checked for collision
    ASSERT( pInstance->m_bInAwakeList == FALSE );
    m_AwakeInstances.Prepend( pInstance );
    pInstance->m_bInAwakeList = TRUE;
}

//==============================================================================

void physics_mgr::PutToSleepInstance( physics_inst* pInstance )
{
    // Should be initialized
    ASSERT( pInstance->m_isInitialized );
    ASSERT( pInstance->m_bInCollisionWakeupList == FALSE );

    // Already asleep?
    if( pInstance->m_bInSleepingList )
    {
        // Should not be in awake or collision wakeup list also!
        ASSERT( pInstance->m_bInAwakeList           == FALSE );
        ASSERT( pInstance->m_bInCollisionWakeupList == FALSE );
        return;
    }
    
    // Remove from awake list?
    if( pInstance->m_bInAwakeList )
    {
        m_AwakeInstances.Remove( pInstance );
        pInstance->m_bInAwakeList = FALSE;
    }
    
    // Update bbox of instance (and internal bodies) before it is made inactive
    // (since collision detection only updates bboxes of active instances)
    pInstance->ComputeWorldBBox();
    
    // Add to sleeping list
    ASSERT( pInstance->m_bInSleepingList == FALSE );
    m_SleepingInstances.Append( pInstance );
    pInstance->m_bInSleepingList = TRUE;
}

//==============================================================================

void physics_mgr::CollisionWakeupInstance( physics_inst* pInstance )
{
    // Should be initialized
    ASSERT( pInstance->m_isInitialized );
    ASSERT( pInstance->m_bInAwakeList == FALSE);

    // Already in collision wakeup list?
    if( pInstance->m_bInCollisionWakeupList )
        return;

    // Should be in sleeping list!
    ASSERT( pInstance->m_bInAwakeList    == FALSE );
    ASSERT( pInstance->m_bInSleepingList == TRUE  );
        
    // Remove from sleeping list?
    if( pInstance->m_bInSleepingList )
    {
        m_SleepingInstances.Remove( pInstance );
        pInstance->m_bInSleepingList = FALSE;
    }
    
    // Add to collision wakeup list
    ASSERT( pInstance->m_bInCollisionWakeupList == FALSE );
    m_CollisionWakeupInstances.Append( pInstance );
    pInstance->m_bInCollisionWakeupList = TRUE;

}

//==============================================================================
// Collision functions
//==============================================================================

s32 physics_mgr::AddCollision(       rigid_body* pBody0,
                                     rigid_body* pBody1,
                               const vector3&    Pos,
                               const vector3&    Normal, 
                               const f32&        Dist ) 
{
    ASSERT( pBody0 );
    ASSERT( pBody1 );
    
    // Reached max collision count?
    if( m_Collisions.GetCount() == m_Settings.m_nMaxCollisions )
    {
        LOG_WARNING( "physics_mgr::AddCollision", "Ran out collisions! Need to increase m_Settings.m_nMaxCollisions\n" );
        ASSERTS( 0, "physics_mgr::AddCollision - ran out of collisions!" );
        return -1;
    }
    
    // Create new collision
    s32        Index     = m_Collisions.GetCount();
    collision& Collision = m_Collisions.Append();

    // Setup collision
    Collision.m_pBody0          = pBody0;
    Collision.m_pBody1          = pBody1;
    Collision.m_pActor1         = g_pCollisionActor;        
                                
    Collision.m_Position        = Pos;
    Collision.m_Normal          = Normal;
    Collision.m_Depth           = Dist;

    Collision.m_R0              = Pos - pBody0->GetPosition();
    Collision.m_R1              = Pos - pBody1->GetPosition();
        
    Collision.m_Elasticity      = pBody0->GetElasticity() * pBody1->GetElasticity();
    Collision.m_StaticFriction  = 0.5f * ( pBody0->GetStaticFriction() + pBody1->GetStaticFriction() );
    Collision.m_DynamicFriction = 0.5f * ( pBody0->GetDynamicFriction() + pBody1->GetDynamicFriction() );
    
    Collision.m_PushSpeed   = 0.0f;
    Collision.m_PushImpulse = 0.0f;
    Collision.m_BounceSpeed = 0.0f;
    Collision.m_NormalImpulse = 0.0f;
    Collision.m_FrictionImpulse.Zero();
    Collision.m_Denominator = 0.0f;
    
    return Index;
}

//==============================================================================

void physics_mgr::DetectWorldCollisions( physics_inst* pInst )
{
    // Validate list management
    ASSERT( pInst );
    ASSERT( pInst->m_isInitialized           == TRUE  );
    ASSERT( pInst->m_bInAwakeList           == TRUE  );
    ASSERT( pInst->m_bInSleepingList        == FALSE );
    ASSERT( pInst->m_bInCollisionWakeupList == FALSE );

    // Collect world polys around instance
    g_Collider.CollectWorldPolys( pInst->GetWorldBBox() );
        
    // Check all rigid bodies with world
    for( s32 i = 0; i < pInst->GetNRigidBodies(); i++ )
    {   
        // Lookup body info
        rigid_body* pBody = &pInst->GetRigidBody( i );
        ASSERT( pBody );

        // Skip if body does not collide with world
        if( ( pBody->m_Flags & rigid_body::FLAG_WORLD_COLLISION ) == 0 )
            continue;
            
        // Skip if not active
        if( !pBody->IsActive() )
            continue;

        // Lookup body collision shape
        collision_shape* pColl = pBody->GetCollisionShape();
        if( !pColl )
            continue;

        // Check collision with world
        g_Collider.Check( pColl, &g_WorldColl );
    }
}

//==============================================================================

void physics_mgr::DetectSelfCollisions( physics_inst* pInst )
{
    // Validate list management
    ASSERT( pInst );
    ASSERT( pInst->m_isInitialized           == TRUE  );
    ASSERT( pInst->m_bInAwakeList           == TRUE  );
    ASSERT( pInst->m_bInSleepingList        == FALSE );
    ASSERT( pInst->m_bInCollisionWakeupList == FALSE );

    // Check all rigid bodies with other bodies in this instance
    for( s32 i = 0; i < pInst->GetNRigidBodies(); i++ )
    {   
        // Lookup body0 info
        rigid_body* pBody0 = &pInst->GetRigidBody( i );
        ASSERT( pBody0 );

        // Clear collision info ready for tracking the biggest collision
        pBody0->ClearCollision();

        // Lookup body0 collision shape
        collision_shape* pColl0 = pBody0->GetCollisionShape();
        if( !pColl0 )
            continue;

        // Lookup body0 bbox
        const bbox& BBox0 = pBody0->GetWorldBBox();

        // Check with all other bodies in instance
        for( s32 j = i+1; j < pInst->GetNRigidBodies(); j++ )
        {
            // Lookup body1
            rigid_body* pBody1 = &pInst->GetRigidBody( j );
            ASSERT( pBody1 );

            // Lookup body1 collision shape
            collision_shape* pColl1 = pBody1->GetCollisionShape();
            if( !pColl1 )
                continue;

            // Collision enabled between bodies?
            if( pBody0->IsCollisionEnabled( pBody1 ) )
            {
                // At least one of the bodies is active?
                if( pBody0->IsActive() || pBody1->IsActive() )
                {
                    // Do world bounding boxes overlap?
                    const bbox& BBox1  = pBody1->GetWorldBBox();
                    if( BBox0.Intersect( BBox1 ) )
                    {
                        // Detect collision between bodies       
                        PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nBodyBodyCollTests );

                        // Collision occurred?
                        if( g_Collider.Check( pColl0, pColl1 ) )
                        {                        
                            // Wakeup bodies
                            pBody0->CollisionWakeup();
                            pBody1->CollisionWakeup();
                        }                        
                    }
                }
            }
        }
    }
}

//==============================================================================

// Yuck - this is Order(NxM) where N = # of active instances, M = total # of instances
// Because of the fast zone check and the fact that there usually aren't many
// instances in the same zone, this is fast enough for our needs.
void physics_mgr::DetectInstInstCollisions( physics_inst* pInst0 )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "physics_mgr::DetectInstInstCollisions" );

    static xprofile_counter instancePairCandidates =
        x_GetProfiler().RegisterCounter( "PhysicsInstancePairCandidates", "PhysicsCounter" );
    static xprofile_counter instancePairOverlaps =
        x_GetProfiler().RegisterCounter( "PhysicsInstancePairOverlaps", "PhysicsCounter" );
    static xprofile_counter bodyPairCandidates =
        x_GetProfiler().RegisterCounter( "PhysicsBodyPairCandidates", "PhysicsCounter" );
    static xprofile_counter bodyPairBBoxHits =
        x_GetProfiler().RegisterCounter( "PhysicsBodyPairBBoxHits", "PhysicsCounter" );

    s32 InstancePairCandidates = 0;
    s32 InstancePairOverlaps = 0;
    s32 BodyPairCandidates = 0;
    s32 BodyPairBBoxHits = 0;

    // Validate list management
    ASSERT( pInst0 );
    ASSERT( pInst0->m_isInitialized           == TRUE  );
    ASSERT( pInst0->m_bInAwakeList           == TRUE  );
    ASSERT( pInst0->m_bInSleepingList        == FALSE );
    ASSERT( pInst0->m_bInCollisionWakeupList == FALSE );
    ASSERT( pInst0->GetInstCollision() );

    // Lookup bbox
    const bbox& BBox0 = pInst0->GetWorldBBox();

    // Check against the rest of the awake list
    physics_inst* pInst1 = m_AwakeInstances.GetNext( pInst0 );
    while( pInst1 )
    {
        InstancePairCandidates++;

        // Validate list management
        ASSERT( pInst0 != pInst1 );
        ASSERT( pInst1->m_isInitialized           == TRUE  );
        ASSERT( pInst1->m_bInAwakeList           == TRUE  );
        ASSERT( pInst1->m_bInSleepingList        == FALSE );
        ASSERT( pInst1->m_bInCollisionWakeupList == FALSE );

        // Overlap?
        if(    ( pInst1->GetInstCollision() )                   // Inst collision enabled?
            && ( pInst0->GetZone() == pInst1->GetZone() )       // Quick zone check
            && ( BBox0.Intersect( pInst1->GetWorldBBox() ) ) )  // Slow bbox check
        {
            InstancePairOverlaps++;

            // Check all rigid bodies with other bodies in this instance
            for( s32 i = 0; i < pInst0->GetNRigidBodies(); i++ )
            {   
                // Lookup body0 and skip if inactive
                rigid_body* pBody0 = &pInst0->GetRigidBody( i );
                ASSERT( pBody0 );
                if( pBody0->IsActive() == FALSE )
                    continue;

                // Lookup collision info
                const bbox& BBox0  = pBody0->GetWorldBBox();
                collision_shape* pColl0 = pBody0->GetCollisionShape();
                if( !pColl0 )
                    continue;

                // Check with bodies in other instance
                for( s32 j = 0; j < pInst1->GetNRigidBodies(); j++ )
                {
                    // Lookup body1
                    rigid_body* pBody1 = &pInst1->GetRigidBody( j );
                    ASSERT( pBody1 );

                    // Lookup body1 info
                    collision_shape* pColl1 = pBody1->GetCollisionShape();
                    if( !pColl1 )
                        continue;

                    // Do world bounding boxes overlap?
                    const bbox& BBox1  = pBody1->GetWorldBBox();
                    BodyPairCandidates++;
                    if( BBox0.Intersect( BBox1 ) )
                    {
                        BodyPairBBoxHits++;

                        // Detect collision between bodies       
                        PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nBodyBodyCollTests );

                        // Collision occurred?
                        if( g_Collider.Check( pColl0, pColl1 ) )
                        {                        
                            // Wakeup body1
                            pBody1->CollisionWakeup();
                        }                        
                    }
                }
            }
        }

        // Check next awake instance
        pInst1 = m_AwakeInstances.GetNext( pInst1 );                    
    }

    // Check against all of the sleeping list :(
    pInst1 = m_SleepingInstances.GetHead();
    while( pInst1 )
    {
        InstancePairCandidates++;

        // Validate list management
        ASSERT( pInst0 != pInst1 );
        ASSERT( pInst1->m_isInitialized           == TRUE  );
        ASSERT( pInst1->m_bInAwakeList           == FALSE );
        ASSERT( pInst1->m_bInSleepingList        == TRUE  );
        ASSERT( pInst1->m_bInCollisionWakeupList == FALSE );
     
        // Lookup next instance in-case it's moved into the awake list
        physics_inst* pNextInst = m_SleepingInstances.GetNext( pInst1 );                    
        
        // Overlap?
        if(    ( pInst1->GetInstCollision() )                  // Inst collision enabled?
            && ( pInst0->GetZone() == pInst1->GetZone() )      // Quick zone check
            && ( BBox0.Intersect( pInst1->GetWorldBBox() ) ) ) // Slow bbox check
        {
            InstancePairOverlaps++;

            // Check all rigid bodies with other bodies in this instance
            for( s32 i = 0; i < pInst0->GetNRigidBodies(); i++ )
            {   
                // Lookup body0 and skip if inactive
                rigid_body* pBody0 = &pInst0->GetRigidBody( i );
                ASSERT( pBody0 );
                if( pBody0->IsActive() == FALSE )
                    continue;
                
                // Lookup collision info
                const bbox& BBox0  = pBody0->GetWorldBBox();
                collision_shape* pColl0 = pBody0->GetCollisionShape();
                if( !pColl0 )
                    continue;

                // Check with bodies in other instance
                for( s32 j = 0; j < pInst1->GetNRigidBodies(); j++ )
                {
                    // Lookup body1
                    rigid_body* pBody1 = &pInst1->GetRigidBody( j );
                    ASSERT( pBody1 );

                    // Lookup body1 info
                    collision_shape* pColl1 = pBody1->GetCollisionShape();
                    if( !pColl1 )
                        continue;

                    // Do world bounding boxes overlap?
                    const bbox& BBox1  = pBody1->GetWorldBBox();
                    BodyPairCandidates++;
                    if( BBox0.Intersect( BBox1 ) )
                    {
                        BodyPairBBoxHits++;

                        // Detect collision between bodies
                        PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nBodyBodyCollTests );
                        
                        // Collision occurred?
                        if( g_Collider.Check( pColl0, pColl1 ) )
                        {
                            // Add to collision wakeup list
                            CollisionWakeupInstance( pInst1 );

                            // Wakeup body1
                            pBody1->CollisionWakeup();
                        }                            
                    }
                }
            }
        }
                        
        // Check next instance
        pInst1 = pNextInst;
    }

    instancePairCandidates.Add( InstancePairCandidates );
    instancePairOverlaps.Add( InstancePairOverlaps );
    bodyPairCandidates.Add( BodyPairCandidates );
    bodyPairBBoxHits.Add( BodyPairBBoxHits );
}

//==============================================================================

void physics_mgr::DetectInstActorCollisions( physics_inst* pInst )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "physics_mgr::DetectInstActorCollisions" );

    static xprofile_counter actorPairCandidates =
        x_GetProfiler().RegisterCounter( "PhysicsInstanceActorCandidates", "PhysicsCounter" );
    static xprofile_counter actorPairOverlaps =
        x_GetProfiler().RegisterCounter( "PhysicsInstanceActorOverlaps", "PhysicsCounter" );
    s32 ActorPairCandidates = 0;
    s32 ActorPairOverlaps = 0;

    // Validate list management
    ASSERT( pInst->m_isInitialized           == TRUE );
    ASSERT( pInst->m_bInAwakeList           == TRUE );
    ASSERT( pInst->m_bInSleepingList        == FALSE );
    ASSERT( pInst->m_bInCollisionWakeupList == FALSE );

    // Lookup bbox of instance
    const bbox& BBox = pInst->GetWorldBBox();

    // Loop through all actors
    actor* pActor = actor::m_pFirstActive;
    while( pActor )
    {
        ActorPairCandidates++;

        // Overlap?
        if(     ( pInst->GetZone() == pActor->GetZone1() ) // Quick zone check
            &&  ( BBox.Intersect( pActor->GetBBox() ) ) )
        {
            ActorPairOverlaps++;

            // Setup global collision actor
            g_pCollisionActor = pActor;

            // Loop through all rigid bodies in this instance and check for colliding with actor
            for( s32 i = 0; i < pInst->GetNRigidBodies(); i++ )
            {   
                // Lookup body
                rigid_body* pBody = &pInst->GetRigidBody( i );
                ASSERT( pBody );

                // Lookup collision shape and skip if none
                collision_shape* pColl = pBody->GetCollisionShape();
                if( !pColl )
                    continue;

                const f32 CapsuleHeight = pActor->GetCollisionHeight();
                const f32 CapsuleRadius = x_min( pActor->GetCollisionRadius(), CapsuleHeight * 0.5f );
                if( ( CapsuleHeight <= 0.0f ) || ( CapsuleRadius <= 0.0f ) )
                {
                    continue;
                }

                const vector3 CapsuleBase  = pActor->GetPosition();
                const vector3 CapsuleStart = CapsuleBase + vector3( 0.0f, CapsuleRadius, 0.0f );
                const vector3 CapsuleEnd   = CapsuleBase + vector3( 0.0f, CapsuleHeight - CapsuleRadius, 0.0f );
                g_ActorBody.SetPosition( (CapsuleStart + CapsuleEnd) * 0.5f );

                // Setup capsule collision shape (just setup start and end spheres)
                collision_shape::sphere& Start = g_ActorColl.GetSphere( 0 );
                collision_shape::sphere& End   = g_ActorColl.GetSphere( 1 );
                g_ActorColl.SetRadius( CapsuleRadius );
                Start.m_PrevPos     = CapsuleStart;
                Start.m_CurrPos     = CapsuleStart;
                End.m_PrevPos       = CapsuleEnd;
                End.m_CurrPos       = CapsuleEnd;

                // Check for collision
                if( g_Collider.Check( pColl, &g_ActorColl ) )
                {
                    // Wakeup body
                    pBody->CollisionWakeup();
                }
            }                
        }

        // Check against next actor
        pActor = pActor->m_pNextActive;
    }

    // Clear global collision actor
    g_pCollisionActor = NULL;

    actorPairCandidates.Add( ActorPairCandidates );
    actorPairOverlaps.Add( ActorPairOverlaps );
}

//==============================================================================

void physics_mgr::DetectCollisions( void )
{
    PHYSICS_DEBUG_START_TIMER( m_Profile.m_DetectCollision );

    ASSERT( m_CollisionWakeupInstances.GetCount() == 0 );

    // Clear collisions
    m_Collisions.SetCount(0);

    // Update bboxes of active instances (and internal bodies)
    physics_inst* pInst = m_AwakeInstances.GetHead();
    while( pInst )
    {
        // Validate list management
        ASSERT( pInst->m_isInitialized           == TRUE  );
        ASSERT( pInst->m_bInAwakeList           == TRUE  );
        ASSERT( pInst->m_bInSleepingList        == FALSE );
        ASSERT( pInst->m_bInCollisionWakeupList == FALSE );
    
        // Update bbox
        pInst->ComputeWorldBBox();
        
        // Next
        pInst = m_AwakeInstances.GetNext( pInst );
    }        

    // Loop through all active instances
    pInst = m_AwakeInstances.GetHead();
    while( pInst )
    {
        // Validate list management
        ASSERT( pInst->m_isInitialized           == TRUE );
        ASSERT( pInst->m_bInAwakeList           == TRUE );
        ASSERT( pInst->m_bInSleepingList        == FALSE );
        ASSERT( pInst->m_bInCollisionWakeupList == FALSE );
    
        // Detect world and self collisions?
        if( pInst->GetWorldCollision() )
            DetectWorldCollisions( pInst );
        
        // Detect self collisions?
        if( m_Settings.m_bSelfCollision )
            DetectSelfCollisions( pInst );

        // Check collision with ALL other instances in simulation!
        if( ( pInst->GetInstCollision() ) && ( m_Settings.m_bInstInstCollision ) )
            DetectInstInstCollisions( pInst );

        // Collide with actors?
        // (Only active instances are tested for since the punch bag dummy is the only
        // instance that has actor collision and it's always active when visible)
        if( pInst->GetActorCollision( ) )
            DetectInstActorCollisions( pInst );
        
        // Check next active instance
        pInst = m_AwakeInstances.GetNext( pInst );                    
    }

    // Collision may wake up some sleeping instances
    pInst = m_CollisionWakeupInstances.GetHead();
    while( pInst )
    {
        // Validate list management
        ASSERT( pInst->m_isInitialized           == TRUE );
        ASSERT( pInst->m_bInAwakeList           == FALSE );
        ASSERT( pInst->m_bInSleepingList        == FALSE );
        ASSERT( pInst->m_bInCollisionWakeupList == TRUE  );

        // Lookup next since current inst will be removed from list
        physics_inst* pNextInst = m_CollisionWakeupInstances.GetNext( pInst );
    
        // Put into awake list
        WakeupInstance( pInst );

        // Validate list management
        ASSERT( pInst->m_isInitialized           == TRUE  );
        ASSERT( pInst->m_bInAwakeList           == TRUE  );
        ASSERT( pInst->m_bInSleepingList        == FALSE );
        ASSERT( pInst->m_bInCollisionWakeupList == FALSE );
        
        // Check next
        pInst = pNextInst;
    }

    ASSERT( m_CollisionWakeupInstances.GetCount() == 0 );

    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_DetectCollision );
    PHYSICS_DEBUG_SET_COUNT( m_Profile.m_nCollisions, m_Collisions.GetCount() );
}

//==============================================================================

void physics_mgr::PreApplyCollisions( f32 DeltaTime )
{
    // Loop through all collisions
    for( s32 i = 0; i < m_Collisions.GetCount(); i++ )
    {
        // Lookup collision info
        physics_mgr::collision& Collision = m_Collisions[i];
        rigid_body* pBody0 = Collision.m_pBody0;
        rigid_body* pBody1 = Collision.m_pBody1;
        vector3&    R0     = Collision.m_R0;
        vector3&    R1     = Collision.m_R1;
        vector3&    Normal = Collision.m_Normal;
        
        const f32 PositionError = x_max( 0.0f, Collision.m_Depth - m_Settings.m_ContactSlop );
        const f32 CorrectionFraction = 1.0f -
                                       x_exp( -m_Settings.m_PositionCorrectionRate * DeltaTime );
        Collision.m_PushSpeed = x_min( PositionError * CorrectionFraction / DeltaTime,
                                      m_Settings.m_MaxContactCorrectionSpeed );
        Collision.m_PushImpulse = 0.0f;
        Collision.m_NormalImpulse = 0.0f;
        Collision.m_FrictionImpulse.Zero();

        vector3 VRel( 0.0f, 0.0f, 0.0f );
        if( pBody0 )
            VRel += pBody0->GetLinearVelocity() +
                    v3_Cross( pBody0->GetAngularVelocity(), R0 );
        if( pBody1 == &g_ActorBody )
        {
            ASSERT( Collision.m_pActor1 );
            VRel -= Collision.m_pActor1->GetVelocity();
        }
        else if( pBody1 )
        {
            VRel -= pBody1->GetLinearVelocity() +
                    v3_Cross( pBody1->GetAngularVelocity(), R1 );
        }

        const f32 NormalVel = v3_Dot( VRel, Normal );
        if( NormalVel <= -m_Settings.m_RestitutionMinSpeed )
            Collision.m_BounceSpeed = -Collision.m_Elasticity * NormalVel;
        else
            Collision.m_BounceSpeed = 0.0f;
            
        // Compute denominator
        f32 Denominator = 0.0f;
        
        if( ( pBody0 ) && ( pBody0->GetMass() != 0.0f ) )
        {
            Denominator += pBody0->GetInvMass() +
                v3_Dot( Normal, v3_Cross( pBody0->GetWorldInvInertia().RotateVector( v3_Cross( R0, Normal ) ), R0 ) );
        }                       

        if( ( pBody1 ) && ( pBody1->GetMass() != 0.0f ) )
        {
            Denominator += pBody1->GetInvMass() + 
                v3_Dot( Normal, v3_Cross( pBody1->GetWorldInvInertia().RotateVector( v3_Cross( R1, Normal ) ), R1 ) );
        }

        Collision.m_Denominator = Denominator;
    }
}

//==============================================================================

s32 physics_mgr::SolveCollision( collision&  Collision )
{
    // No penetration?
    if( Collision.m_Depth < 0 )
        return FALSE;

    // Too small denominator?
    if( Collision.m_Denominator < 0.0001f )
        return FALSE;

    // Lookup rigid bodies and leave alone if infinite mass
    rigid_body* pBody0 = Collision.m_pBody0;
    rigid_body* pBody1 = Collision.m_pBody1;
    if( ( pBody0 ) && ( pBody0->GetMass() == 0.0f ) )
        pBody0 = NULL;
    if( ( pBody1 ) && ( pBody1 == &g_WorldBody ) )
        pBody1 = NULL;

    // Nothing to do?
    if( ( pBody0 == NULL ) && ( pBody1 == NULL ) )
        return FALSE;

    // Setup body1 velocity from actor?
    if( pBody1 == &g_ActorBody )
    {
        // Grab movement velocity so correct impact happens when the player runs
        // into a rigid body (eg. for the punch bag dummy!)
        ASSERT( Collision.m_pActor1 );
        g_ActorBody.SetLinearVelocity( Collision.m_pActor1->GetVelocity() );
    }
    
    vector3 VRel( 0.0f, 0.0f, 0.0f );
    if( pBody0 )
        VRel += pBody0->GetLinearVelocity() + v3_Cross( pBody0->GetAngularVelocity(), Collision.m_R0 );
    if( pBody1 )
        VRel -= pBody1->GetLinearVelocity() + v3_Cross( pBody1->GetAngularVelocity(), Collision.m_R1 );

    // Compute velocity along penetration normal
    f32 NormalVel = v3_Dot( VRel, Collision.m_Normal );

    if( pBody0 && (NormalVel < 0.0f) )
    {
        pBody0->m_Flags |= rigid_body::FLAG_HAS_COLLIDED;

        const f32 ImpactSpeedSqr = NormalVel * NormalVel;
        if( ImpactSpeedSqr > pBody0->m_ImpactSpeedSqr )
            pBody0->m_ImpactSpeedSqr = ImpactSpeedSqr;
    }

    const f32 OldNormalImpulse = Collision.m_NormalImpulse;
    Collision.m_NormalImpulse = x_max( 0.0f,
                                       OldNormalImpulse +
                                       (Collision.m_BounceSpeed - NormalVel) /
                                       Collision.m_Denominator );
    const f32 NormalImpulseChange = Collision.m_NormalImpulse - OldNormalImpulse;
    xbool bChanged = x_abs( NormalImpulseChange ) > 0.00001f;

    if( bChanged )
    {
        const vector3 Impulse = Collision.m_Normal * NormalImpulseChange;
        if( pBody0 )
            pBody0->ApplyLocalImpulse( Impulse, Collision.m_R0 );
        if( pBody1 )
            pBody1->ApplyLocalImpulse( -Impulse, Collision.m_R1 );
    }

    VRel.Zero();
    if( pBody0 )
        VRel += pBody0->GetLinearVelocity() + v3_Cross( pBody0->GetAngularVelocity(), Collision.m_R0 );
    if( pBody1 )
        VRel -= pBody1->GetLinearVelocity() + v3_Cross( pBody1->GetAngularVelocity(), Collision.m_R1 );

    // Compute tangent vel and speed
    vector3 TangentVel = VRel - v3_Dot( VRel, Collision.m_Normal ) * Collision.m_Normal;
    const f32 TangentSpeedSqr = TangentVel.LengthSquared();

    const vector3 OldFrictionImpulse = Collision.m_FrictionImpulse;
    vector3 FrictionImpulse = OldFrictionImpulse;
    if( TangentSpeedSqr > 0.001f )
    {
        const f32 TangentSpeed = x_sqrt( TangentSpeedSqr );
        const vector3 T = -TangentVel / TangentSpeed;

        f32 Denominator = 0.0f;
        if( pBody0 )
        {
            Denominator += pBody0->GetInvMass() +
                v3_Dot( T, v3_Cross( pBody0->GetWorldInvInertia().RotateVector(
                    v3_Cross( Collision.m_R0, T ) ), Collision.m_R0 ) );
        }

        if( pBody1 )
        {
            Denominator += pBody1->GetInvMass() +
                v3_Dot( T, v3_Cross( pBody1->GetWorldInvInertia().RotateVector(
                    v3_Cross( Collision.m_R1, T ) ), Collision.m_R1 ) );
        }

        if( Denominator > 0.0001f )
            FrictionImpulse += T * (TangentSpeed / Denominator);
    }

    const f32 StaticLimit = Collision.m_StaticFriction * Collision.m_NormalImpulse;
    if( FrictionImpulse.LengthSquared() > x_sqr( StaticLimit ) )
    {
        const f32 DynamicLimit = Collision.m_DynamicFriction * Collision.m_NormalImpulse;
        FrictionImpulse.NormalizeAndScale( DynamicLimit );
    }

    Collision.m_FrictionImpulse = FrictionImpulse;
    const vector3 FrictionImpulseChange = FrictionImpulse - OldFrictionImpulse;
    if( FrictionImpulseChange.LengthSquared() > 0.0000000001f )
    {
        bChanged = TRUE;
        if( pBody0 )
            pBody0->ApplyLocalImpulse( FrictionImpulseChange, Collision.m_R0 );
        if( pBody1 )
            pBody1->ApplyLocalImpulse( -FrictionImpulseChange, Collision.m_R1 );
    }

    return bChanged;
}

//==============================================================================

s32 physics_mgr::SolveCollisionPosition( collision& Collision )
{
    if( (Collision.m_PushSpeed <= 0.0f) || (Collision.m_Denominator < 0.0001f) )
        return FALSE;

    rigid_body* pBody0 = Collision.m_pBody0;
    rigid_body* pBody1 = Collision.m_pBody1;
    if( pBody0 && (pBody0->GetMass() == 0.0f) )
        pBody0 = NULL;
    if( pBody1 && (pBody1->GetMass() == 0.0f) )
        pBody1 = NULL;
    if( !pBody0 && !pBody1 )
        return FALSE;

    vector3 RelativeVelocity( 0.0f, 0.0f, 0.0f );
    if( pBody0 )
        RelativeVelocity += pBody0->m_SplitLinearVelocity +
                            v3_Cross( pBody0->m_SplitAngularVelocity, Collision.m_R0 );
    if( pBody1 )
        RelativeVelocity -= pBody1->m_SplitLinearVelocity +
                            v3_Cross( pBody1->m_SplitAngularVelocity, Collision.m_R1 );

    const f32 NormalSpeed = v3_Dot( RelativeVelocity, Collision.m_Normal );
    const f32 OldImpulse  = Collision.m_PushImpulse;
    Collision.m_PushImpulse = x_max( 0.0f,
                                     OldImpulse +
                                     (Collision.m_PushSpeed - NormalSpeed) /
                                     Collision.m_Denominator );
    const f32 ImpulseChange = Collision.m_PushImpulse - OldImpulse;
    if( x_abs( ImpulseChange ) < 0.00001f )
        return FALSE;

    const vector3 Impulse = Collision.m_Normal * ImpulseChange;
    if( pBody0 )
        pBody0->ApplySplitImpulse( Impulse, Collision.m_R0 );
    if( pBody1 )
        pBody1->ApplySplitImpulse( -Impulse, Collision.m_R1 );

    return TRUE;
}

//==============================================================================

void physics_mgr::PreApplyConstraints( f32 DeltaTime )
{
    // Prepare constraints ready for solving collisions
    PHYSICS_DEBUG_START_TIMER( m_Profile.m_PrepConstraints );
    m_SolveConstraints.Clear();
    s32 nActiveConstraints = m_ActiveConstraints.GetCount();
    for( s32 i = 0; i < nActiveConstraints; i++ )
    {
        // Lookup constraint
        active_constraint& Constraint = m_ActiveConstraints[i];
        
        // Pre apply and add to solve list if not satisfied
        PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nPrepConstraints );
        ASSERT( Constraint.m_pOwner );
        if( Constraint.m_pOwner->PreApply( DeltaTime, Constraint ) )
            m_SolveConstraints.Append( &Constraint );
    }
    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_PrepConstraints );
}

//==============================================================================

void physics_mgr::SolveCollisions( f32 DeltaTime, s32 nIterations )
{
    s32 i;

    // Prepare constraints ready for solving
    PreApplyConstraints( DeltaTime );

    // Get collision and constraint info
    s32 nCollisions  = m_Collisions.GetCount();
    s32 nConstraints = m_SolveConstraints.GetCount();
    if ( ( !nCollisions ) && ( !nConstraints ) )
    {
        return;
    }

    // Prepare collisions
    PreApplyCollisions( DeltaTime );

    // Alternate solve direction to reduce jitter when resting
    s32 Direction = g_ObjMgr.GetNLogicLoops() & 1;
    
    // Process as normal    
    s32 bActive = TRUE;
    s32 Loops = 0;
    while( ( nIterations-- ) && ( bActive ) )
    {
        bActive = FALSE;
        Loops++;

        // Solve in which direction?
        if( Direction )
        {
            // Solve all constraints
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveConstraints );
            active_constraint* pConstraint = m_SolveConstraints.GetHead();
            while( pConstraint )
            {
                PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nSolveConstraints );
                ASSERT( pConstraint->m_pOwner );
                bActive |= pConstraint->m_pOwner->Apply( *pConstraint );
                pConstraint = m_SolveConstraints.GetNext( pConstraint );
            }                
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveConstraints );
            
            // Solve all collisions
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveCollisions );
            for ( i = 0 ; i < nCollisions ; i++ )
                bActive |= SolveCollision( m_Collisions[i] );
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveCollisions );
        }
        else
        {
            // Solve all constraints
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveConstraints );
            active_constraint* pConstraint = m_SolveConstraints.GetHead();
            while( pConstraint )
            {
                PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nSolveConstraints );
                ASSERT( pConstraint->m_pOwner );
                bActive |= pConstraint->m_pOwner->Apply( *pConstraint );
                pConstraint = m_SolveConstraints.GetNext( pConstraint );
            }              
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveConstraints );
            
            // Solve all collisions
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveCollisions );
            for ( i = nCollisions-1 ; i >= 0 ; i-- )
                bActive |= SolveCollision( m_Collisions[i] );
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveCollisions );
        }
        
        // Toggle direction
        Direction ^= 1;
    }
}

//==============================================================================

void physics_mgr::SolveContacts( f32 DeltaTime, s32 nIterations )
{
    s32 i;

    // Prepare constraints ready for solving
    PreApplyConstraints( DeltaTime );

    // Get collision and constraint info
    s32 nCollisions  = m_Collisions.GetCount();
    s32 nConstraints = m_SolveConstraints.GetCount();
    if ((!nCollisions) && (!nConstraints))
    {
        return;
    }
    
    // Set all collisions to inelastic
    for ( i = 0 ; i < nCollisions ; i++ )
        m_Collisions[i].m_Elasticity = 0.0f;

    // Prepare collisions
    PreApplyCollisions( DeltaTime );

    // Alternate solve direction to reduce jitter when resting
    s32 Direction = g_ObjMgr.GetNLogicLoops() & 1;

    // Process as inelastic collisions
    s32 bActive = TRUE;
    s32 Loops = 0;
    while( ( nIterations-- ) && ( bActive ) )
    {
        bActive = FALSE;
        Loops++;

        // Solve in which direction?
        if( Direction )
        {
            // Solve all constraints
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveConstraints );
            active_constraint* pConstraint = m_SolveConstraints.GetHead();
            while( pConstraint )
            {
                PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nSolveConstraints );
                ASSERT( pConstraint->m_pOwner );
                bActive |= pConstraint->m_pOwner->Apply( *pConstraint );
                pConstraint = m_SolveConstraints.GetNext( pConstraint );
            }                
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveConstraints );
            
            // Solve all collisions
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveContacts );
            for ( i = 0 ; i < nCollisions ; i++ )
                bActive |= SolveCollision( m_Collisions[i] );
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveContacts );
        }
        else
        {
            // Solve all constraints
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveConstraints );
            active_constraint* pConstraint = m_SolveConstraints.GetHead();
            while( pConstraint )
            {
                PHYSICS_DEBUG_INC_COUNT( m_Profile.m_nSolveConstraints );
                ASSERT( pConstraint->m_pOwner );
                bActive |= pConstraint->m_pOwner->Apply( *pConstraint );
                pConstraint = m_SolveConstraints.GetNext( pConstraint );
            }            
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveConstraints );
            
            // Solve all collisions
            PHYSICS_DEBUG_START_TIMER( m_Profile.m_SolveContacts );
            for ( i = nCollisions-1 ; i >= 0 ; i-- )
                bActive |= SolveCollision( m_Collisions[i] );
            PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_SolveContacts );
        }
        
        // Toggle direction
        Direction ^= 1;
    }
}

//==============================================================================

void physics_mgr::ShockPropagation( void )
{
    PHYSICS_DEBUG_START_TIMER( m_Profile.m_ShockPropagation );
    
    // Sort collision bodies from lowest to highest
    if( m_Collisions.GetCount() )
    {
        x_qsort( &m_Collisions[0],          // Address of first item in xarray.
                  m_Collisions.GetCount(),  // Number of items in xarray.
                  sizeof(collision),        // Size of one item.
                  collision_sort_lo_to_hi );   // Compare function.
    }

    // Loop through collisions and make bottom body immovable, then solve
    for( s32 i = 0; i < m_Collisions.GetCount(); i++ )
    {
        // Get collision
        const collision& Collision = m_Collisions[i];
        collision ShockCollision = Collision;

        // Lookup rigid bodies
        rigid_body* pBody0 = Collision.m_pBody0;
        rigid_body* pBody1 = Collision.m_pBody1;
        ASSERT( pBody0 );
        ASSERT( pBody1 );

        // Set lowest body to be immovable
        if( pBody0->GetPosition().GetY() < pBody1->GetPosition().GetY() )
        {
            // Clear lower body
            ShockCollision.m_pBody0 = NULL;
            
            // Re-compute denominator
            ShockCollision.m_Denominator = pBody1->GetInvMass() +
                                      v3_Dot( Collision.m_Normal, v3_Cross( pBody1->GetWorldInvInertia().RotateVector( v3_Cross( Collision.m_R1, Collision.m_Normal ) ), Collision.m_R1 ) );
        }
        else
        {
            // Clear lower body
            ShockCollision.m_pBody1 = NULL;
            
            // Re-compute denominator
            ShockCollision.m_Denominator = pBody0->GetInvMass() +
                                      v3_Dot( Collision.m_Normal, v3_Cross( pBody0->GetWorldInvInertia().RotateVector( v3_Cross( Collision.m_R0, Collision.m_Normal ) ), Collision.m_R0 ) );
        }

        ShockCollision.m_BounceSpeed = 0.0f;
        ShockCollision.m_NormalImpulse = 0.0f;
        ShockCollision.m_FrictionImpulse.Zero();
        
        // Solve the collision
        SolveCollision( ShockCollision );
    }
    
    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_ShockPropagation );
}

//==============================================================================

void physics_mgr::SolvePositionCorrections( f32 DeltaTime, s32 nIterations )
{
    rigid_body* pBody = m_ActiveBodies.GetHead();
    while( pBody )
    {
        pBody->ClearSplitVelocity();
        pBody = m_ActiveBodies.GetNext( pBody );
    }

    PreApplyCollisions( DeltaTime );

    while( nIterations-- > 0 )
    {
        xbool bChanged = FALSE;

        for( s32 i = 0; i < m_Collisions.GetCount(); i++ )
            bChanged |= SolveCollisionPosition( m_Collisions[i] );

        if( !bChanged )
            break;
    }

    pBody = m_ActiveBodies.GetHead();
    while( pBody )
    {
        pBody->IntegrateSplitPosition( DeltaTime );
        pBody->ClearSplitVelocity();
        pBody = m_ActiveBodies.GetNext( pBody );
    }
}

//==============================================================================

void physics_mgr::PutInstancesToSleep( f32 DeltaTime )
{
    PHYSICS_DEBUG_START_TIMER( m_Profile.m_BuildLists );

    // Loop through awake instances checking to see if we can put them to sleep
    physics_inst* pInst = m_AwakeInstances.GetHead();
    while( pInst )
    {
        // Validate list management
        ASSERT( pInst->m_isInitialized );
        ASSERT( pInst->m_bInAwakeList           == TRUE );
        ASSERT( pInst->m_bInSleepingList        == FALSE );
        ASSERT( pInst->m_bInCollisionWakeupList == FALSE );
        
        // Get next instance in-case of deletion from this list
        physics_inst* pNextInst = m_AwakeInstances.GetNext( pInst );            

        xbool bInstActive = FALSE;
        for( s32 i = 0; i < pInst->GetNRigidBodies(); i++ )
        {
            // Lookup body
            rigid_body& Body = pInst->GetRigidBody( i );

            Body.UpdateActiveState( DeltaTime );

            // If body is active, then instance is active
            if( Body.IsActive() )
            {
                // Keep instance in active list
                bInstActive = TRUE;
            }
            else
            {
                // Make sure body vels/forces are cleared for constraints to work properly
                Body.ZeroVelocity();
                Body.ClearForces();
            }
        }
        
        // Put to sleep?
        if( bInstActive == FALSE )
        {
            // Put to sleep
            PutToSleepInstance( pInst );
        }            
        
        // Check next awake instance
        pInst = pNextInst;
    }
    
    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_BuildLists );
}

//==============================================================================
     
void physics_mgr::BuildActiveBodyList( void )
{
    PHYSICS_DEBUG_START_TIMER( m_Profile.m_BuildLists );

    // Clear the list
    m_ActiveBodies.Clear();

    // Loop through all awake instances
    physics_inst* pInst = m_AwakeInstances.GetHead();
    while( pInst )
    {
        // Validate list management
        ASSERT( pInst->m_isInitialized );
        ASSERT( pInst->m_bInAwakeList           == TRUE  );
        ASSERT( pInst->m_bInSleepingList        == FALSE );
        ASSERT( pInst->m_bInCollisionWakeupList == FALSE );

        // Build list of active bodies
        for( s32 i = 0; i < pInst->GetNRigidBodies(); i++ )        
        {
            // Lookup body
            rigid_body& Body = pInst->GetRigidBody( i );

            // Body awake?
            if( Body.IsActive() )
            {
                // Add body to active list
                m_ActiveBodies.Append( &Body );
            }
        }
        
        // Get next awake instance
        pInst = m_AwakeInstances.GetNext( pInst );
    }
    
    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_BuildLists );
}

//==============================================================================

void physics_mgr::BuildActiveBodyAndConstraintList( void )
{
    PHYSICS_DEBUG_START_TIMER( m_Profile.m_BuildLists );

    s32 i;
    
    // Clear all lists
    m_ActiveBodies.Clear();
    m_ActiveConstraints.SetCount( 0 );
    m_SolveConstraints.Clear();
    
    // Loop through all awake instances
    physics_inst* pInst = m_AwakeInstances.GetHead();
    while( pInst )
    {
        // Validate list management
        ASSERT( pInst->m_isInitialized );
        ASSERT( pInst->m_bInAwakeList           == TRUE );
        ASSERT( pInst->m_bInSleepingList        == FALSE );
        ASSERT( pInst->m_bInCollisionWakeupList == FALSE );

        // Are there enough active constraints to make this instance active?
        s32   NConstraintsNeeded    = pInst->GetNBodyBodyConstraints() + pInst->GetNBodyWorldConstraints();
        xbool bConstraintsAvailable = ( ( m_ActiveConstraints.GetCount() + NConstraintsNeeded ) <= m_Settings.m_nMaxActiveConstraints );
        ASSERT( bConstraintsAvailable );
        if( bConstraintsAvailable )
        {        
            // Force render matrices to be rebuilt
            pInst->DirtyMatrices();
        
            // Build list of active bodies
            for( i = 0; i < pInst->GetNRigidBodies(); i++ )        
            {
                // Lookup body
                rigid_body& Body = pInst->GetRigidBody( i );

                // Body awake?
                if( Body.IsActive() )
                {
                    // Add body to active list
                    m_ActiveBodies.Append( &Body );

#ifdef ENABLE_PHYSICS_DEBUG

                    // Get collision
                    collision_shape* pColl = Body.GetCollisionShape();
                    if( pColl )
                    {
                        // Update active sphere count
                        m_Profile.m_nActiveSpheres += pColl->GetNSpheres();
                    }   
#endif            
                }
            }

            // Lookup blend in constraint weight
            f32 ConstraintWeight = pInst->GetConstraintWeight();

            // Build list of active body -> body constraints
            for( i = 0; i < pInst->GetNBodyBodyConstraints(); i++ )        
            {
                // Add if active
                constraint& Constraint = pInst->GetBodyBodyConstraint( i );
                if( Constraint.IsActive() )
                {
                    // Allocate and add active constraint?
                    if( m_ActiveConstraints.GetCount() < m_Settings.m_nMaxActiveConstraints )
                    {
                        active_constraint& ActiveConstraint = m_ActiveConstraints.Append();
                        ActiveConstraint.m_pOwner = &Constraint;

                        // Setup weight
                        if( Constraint.GetFlags() & constraint::FLAG_BLEND_IN )
                            ActiveConstraint.m_Weight = ConstraintWeight;
                        else                            
                            ActiveConstraint.m_Weight = 1.0f;
                    }
                    else
                    {
                        LOG_WARNING( "physics_mgr::Step", "Ran out active constraints! Need to increase m_Settings.m_nMaxActiveConstraints\n" );
                        ASSERTS( 0, "physics_mgr::Step - ran out of active constraints!" );
                    }
                }                                        
            }

            // Build list of active body -> world constraints
            for( i = 0; i < pInst->GetNBodyWorldConstraints(); i++ )        
            {
                // Add if active
                constraint& Constraint = pInst->GetBodyWorldConstraint( i );
                if( Constraint.IsActive() )
                {
                    // Allocate and add active constraint?
                    if( m_ActiveConstraints.GetCount() < m_Settings.m_nMaxActiveConstraints )
                    {
                        active_constraint& ActiveConstraint = m_ActiveConstraints.Append();
                        ActiveConstraint.m_pOwner = &Constraint;
                        ActiveConstraint.m_Weight = 1.0f;   // Body-World constraints always fully on
                    }
                    else
                    {
                        LOG_WARNING( "physics_mgr::Step", "Ran out active constraints! Need to increase m_Settings.m_nMaxActiveConstraints\n" );
                        ASSERTS( 0, "physics_mgr::Step - ran out of active constraints!" );
                    }
                }                                        
            }
        }
        
        // Get next awake instance
        pInst = m_AwakeInstances.GetNext( pInst );
    }

    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_BuildLists );
}

//==============================================================================

void physics_mgr::Step( f32 DeltaTime )
{
/*
    // Physics simulation is as follows:
    0) Try put active instances to sleep
    1) Build active body list
    2) compute external forces
    3) store original position/vel/force
    4) predict new velocity
    5) predict new position
    6) detect collisions (may add more active bodies)
    7) Build new list of active bodies and active constraints
    8) restore velocity to original
    9) process collisions/constraints
    10) reset position to original
    11) integrate vels
    12) clear forces
    13) process contacts/constraints
    14) shock propagation
    15) resolve position error using split impulses
    16) integrate positions
*/

    // Try put awake bodies to sleep
    PutInstancesToSleep( DeltaTime );

    // Build list of active rigid bodies
    BuildActiveBodyList();

    // Store body states and predict new velocities/positions using external forces
    if(1)
    {
        PHYSICS_DEBUG_START_TIMER( m_Profile.m_PredictNew );
        
        rigid_body* pBody = m_ActiveBodies.GetHead();
        while( pBody )
        {
            pBody->GetState         ( pBody->m_BackupState );
            pBody->ComputeForces();
            pBody->IntegrateVelocity( DeltaTime );
            pBody->IntegratePosition( DeltaTime );
            
            pBody = m_ActiveBodies.GetNext( pBody );
        }
        
        PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_PredictNew );
    }

    // Detect collisions with predicted velocity and position
    if(1)
    {
        DetectCollisions();
    }

    // Restore velocities to original zero forces 
    if(1)
    {
        PHYSICS_DEBUG_START_TIMER( m_Profile.m_Restore );
    
        rigid_body* pBody = m_ActiveBodies.GetHead();
        while( pBody )
        {
            // Restore state
            pBody->SetVelocity( pBody->m_BackupState );
            
            // Next
            pBody = m_ActiveBodies.GetNext( pBody );
        }
        
        PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_Restore );
    }
    
    // Build list of active bodies/constraints ready for solving
    BuildActiveBodyAndConstraintList();

    // Solve collisions using old velocity
    if(1)
    {
        SolveCollisions( DeltaTime, m_Settings.m_nCollisionIterations );
    }

    // Restore original positions, integrate to new velocities, and clear forces
    if(1)
    {
        PHYSICS_DEBUG_START_TIMER( m_Profile.m_IntegrateVel );
    
        rigid_body* pBody = m_ActiveBodies.GetHead();
        while( pBody )
        {
            pBody->SetPosition      ( pBody->m_BackupState );
            pBody->IntegrateVelocity( DeltaTime );
            pBody->ClearForces();
            pBody = m_ActiveBodies.GetNext( pBody );
        }
        
        PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_IntegrateVel );
    }

    // Solve contacts using new velocity
    if( m_Settings.m_bSolveContacts )
    {
        SolveContacts( DeltaTime, m_Settings.m_nContactIterations );
    }

    // Perform shock propagation for stacking etc.
    if( m_Settings.m_bShock )
    {
        ShockPropagation();
    }

    // Resolve penetration
    SolvePositionCorrections( DeltaTime, m_Settings.m_nContactIterations );

    // Finally, integrate position
    if(1)
    {
        PHYSICS_DEBUG_START_TIMER( m_Profile.m_IntegratePos );
    
        rigid_body* pBody = m_ActiveBodies.GetHead();
        while( pBody )
        {
            // Integrate
            pBody->IntegratePosition( DeltaTime );
            
            // Next body
            pBody = m_ActiveBodies.GetNext( pBody );
        }
        
        PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_IntegratePos );
    }

    // Grab profile info
    PHYSICS_DEBUG_SET_COUNT( m_Profile.m_nActiveInsts,        m_AwakeInstances.GetCount() );
    PHYSICS_DEBUG_SET_COUNT( m_Profile.m_nActiveBodies,       m_ActiveBodies.GetCount() );
    PHYSICS_DEBUG_SET_COUNT( m_Profile.m_nActiveConstraints,  m_ActiveConstraints.GetCount() );

    // Clear lists in-case active instances are removed during game logic
    m_ActiveConstraints.SetCount(0);
    m_SolveConstraints.Clear();
    m_ActiveBodies.Clear();
}

//==============================================================================

void physics_mgr::Advance( f32 DeltaTime )
{
    LOG_STAT( k_stats_Physics );

    X_PROFILE_SCOPE_CATEGORY( "Context", "physics_mgr::Advance" );
    static xprofile_counter physicsSubsteps =
        x_GetProfiler().RegisterCounter( "PhysicsSubsteps", "PhysicsCounter" );

    ASSERTS( x_isvalid( DeltaTime ), "Physics received an invalid frame delta" );
    if( !x_isvalid( DeltaTime ) || (DeltaTime <= 0.0f) )
        return;

    // Clear profile stats
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nActiveInsts );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nActiveBodies );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nActiveSpheres );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nActiveConstraints );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nCollisions );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nPrepConstraints );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nSolveConstraints );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nBodyBodyCollTests );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nSphereSphereCollTests );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nCapsuleCapsuleCollTests );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nSphereCapsuleCollTests );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nClusters );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nNGons );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nSphereNGonCollTests );
    PHYSICS_DEBUG_ZERO_COUNT( m_Profile.m_nSphereNGonIntersecTests );

    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_Advance );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_BuildLists );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_PredictNew );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_DetectCollision );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_Restore );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_SolveCollisions );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_IntegrateVel );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_SolveContacts );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_PrepConstraints );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_SolveConstraints );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_ShockPropagation );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_IntegratePos );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_Render );

    ASSERT( m_Settings.m_MaxSubstepSeconds > 0.0f );
    if( m_Settings.m_MaxSubstepSeconds <= 0.0f )
        return;

    const s32 nSubsteps = x_max( 1, (s32)x_ceil( DeltaTime / m_Settings.m_MaxSubstepSeconds ) );
    const f32 SubstepSeconds = DeltaTime / (f32)nSubsteps;

    PHYSICS_DEBUG_START_TIMER( m_Profile.m_Advance );
    for( s32 i = 0; i < nSubsteps; i++ )
    {
        Step( SubstepSeconds );
    }
    physicsSubsteps.Add( nSubsteps );
    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_Advance );
}

//==============================================================================
// Render functions
//==============================================================================

#ifdef ENABLE_PHYSICS_DEBUG

void physics_mgr::DebugRender( void )
{
    // Show stats?
    if( m_Settings.m_bShowStats )
        DebugShowStats();
    
    // Render instances and collision?
    if( m_Settings.m_bDebugRender )
    {
        DebugRenderInstances();
        DebugRenderCollisions();
    }
}

//==============================================================================

void physics_mgr::DebugFindClosestInstanceInView( const view*         pView,
                                                  physics_inst_list&  InstList,
                                                  f32&                BestDist,
                                                  physics_inst*&      pBestInst )
{
    // Loop through list
    physics_inst* pInst = InstList.GetHead(); 
    while( pInst )
    {
        // Validate list management
        ASSERT( pInst->m_isInitialized );
        ASSERT( pInst->m_bInAwakeList || pInst->m_bInSleepingList );
        ASSERT(!( pInst->m_bInAwakeList && pInst->m_bInSleepingList ) );
    
        // Only consider if in view
        if( pView->BBoxInView( pInst->GetWorldBBox() ) )
        {
            // Draw inst bbox  
            render::debug::Box( pInst->GetWorldBBox(), XCOLOR_WHITE );

            // Render rigid bodies
            for( s32 i = 0; i < pInst->GetNRigidBodies(); i++ )
            {
                rigid_body& Body = pInst->GetRigidBody( i );
                render::debug::Box( Body.GetWorldBBox(), XCOLOR_RED );

                Body.DebugRender();
            }

            // Closest so far?
            f32 Dist = ( pInst->GetPosition() - pView->GetPosition() ).LengthSquared();
            if( Dist < BestDist )
            {
                // Record
                BestDist  = Dist;
                pBestInst = pInst;
            }
        }

        // Check next instance in list
        pInst = InstList.GetNext( pInst );
    }
}

//==============================================================================

void physics_mgr::DebugRenderInstances( xbool bRenderConstraints /*= FALSE*/ )
{
    s32 i;

    // Search for the closest instance to the camera (otherwise we run out of dlist)
    const view*     pView     = eng_GetView();
    f32             BestDist  = F32_MAX;
    physics_inst*   pBestInst = NULL;
    
    // Check awake and sleeping lists
    DebugFindClosestInstanceInView( pView, m_AwakeInstances,    BestDist, pBestInst );
    DebugFindClosestInstanceInView( pView, m_SleepingInstances, BestDist, pBestInst );

    // Draw instance?
    physics_inst* pInst = pBestInst;
    if( pInst )
    {
        // Render rigid bodies
        //for( i = 0; i < pInst->GetNRigidBodies(); i++ )
        //{
            //rigid_body& Body = pInst->GetRigidBody( i );
            //Body.DebugRender();
        //}

        // Render constraints?
        if( bRenderConstraints )
        {
            // Body -> body constraints
            for( i = 0; i < pInst->GetNBodyBodyConstraints(); i++ )
            {
                constraint& Con = pInst->GetBodyBodyConstraint( i );
                Con.DebugRender();
            }
            
            // Body -> world constraints
            for( i = 0; i < pInst->GetNBodyWorldConstraints(); i++ )
            {
                constraint& Con = pInst->GetBodyWorldConstraint( i );
                Con.DebugRender();
            }
        }
    }        
}

//==============================================================================

void physics_mgr::DebugRenderCollisions( void )
{
    for ( s32 i = 0 ; i < m_Collisions.GetCount() ; i++)
    {
        collision& Collision = m_Collisions[i];

        // Collided with world?
        if(     ( Collision.m_pBody0 == &g_WorldBody )
            ||  ( Collision.m_pBody1 == &g_WorldBody ) )
        {
            // Draw world collision normal
            render::debug::Line( Collision.m_Position,
                                 Collision.m_Position + ( 25.0f * Collision.m_Normal ),
                                 XCOLOR_BLUE, render::PRIMITIVE_DEPTH_DISABLED );
        }
        else
        {
            // Draw body v body collision normal
            render::debug::Line( Collision.m_Position,
                                 Collision.m_Position + ( 25.0f * Collision.m_Normal ),
                                 XCOLOR_RED, render::PRIMITIVE_DEPTH_DISABLED );
        }
    }
}


//==============================================================================

void physics_mgr::DebugText( s32 X, s32 Y, xcolor Color, const char* pFormatStr, ... )
{
    // Create final string    
    x_va_list   Args;
    x_va_start( Args, pFormatStr );
    xvfs XVFS( pFormatStr, Args );
    x_va_end( Args );

#ifndef X_EDITOR
    // UI manager present?
    if( g_UiMgr )
    {    
        // Compute text screen position
        irect TextPos;
        const irect Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();
        TextPos.l = Bounds.l + 8 + ( X * 18 );
        TextPos.t = Bounds.t + 18 + ( Y * 17 );
        TextPos.r = TextPos.l + 400;
        TextPos.b = TextPos.t + 20;
    
        // Use ui manager so test appears in X_RETAIL builds
        g_UiMgr->RenderText( 0, TextPos, ui_font::h_left|ui_font::v_top, Color, XVFS, TRUE, TRUE );
        return;
    }
#endif    
    
    // Use x files (will not appear in X_RETAIL builds!)
    x_printfxy( X,Y, "%s", (const char*)XVFS );
}

//==============================================================================

void physics_mgr::DebugShowStats( void )
{
    s32 X = 0, Y = 0;
    xcolor Color = XCOLOR_WHITE;
    
    DebugText( X, Y++, Color, "#AwakeInsts:%d #SleepInsts:%d Collisions:%d",
                m_AwakeInstances.GetCount(), 
                m_SleepingInstances.GetCount(), 
                m_Profile.m_nCollisions );

    DebugText( X, Y++, Color, "ActInsts:%d ActBods:%d ActSpheres:%d ActCons:%d", 
        m_Profile.m_nActiveInsts,
        m_Profile.m_nActiveBodies,
        m_Profile.m_nActiveSpheres,
        m_Profile.m_nActiveConstraints );

    DebugText( X, Y++, Color, "StatMem:%dK DynMem:%dK",
        ( m_Profile.m_StaticMemoryUsed + 1023 ) / 1024,
        ( m_Profile.m_DynamicMemoryUsed + 1023 ) / 1024 );

    Y++;
    DebugText( X, Y++, Color, "*Total*:%.2f", m_Profile.m_Advance.ReadMs() );
    DebugText( X, Y++, Color, "BldList:%.2f", m_Profile.m_BuildLists.ReadMs() );
    DebugText( X, Y++, Color, "PredNew:%.2f", m_Profile.m_PredictNew.ReadMs() );
    DebugText( X, Y++, Color, "DetColl:%.2f", m_Profile.m_DetectCollision.ReadMs() );
    DebugText( X, Y++, Color, "Restore:%.2f", m_Profile.m_Restore.ReadMs() );
    DebugText( X, Y++, Color, "SolColl:%.2f", m_Profile.m_SolveCollisions.ReadMs() );
    DebugText( X, Y++, Color, "IntVels:%.2f", m_Profile.m_IntegrateVel.ReadMs() );
    DebugText( X, Y++, Color, "SolRest:%.2f", m_Profile.m_SolveContacts.ReadMs() );
    DebugText( X, Y++, Color, "PrpCsts:%.2f", m_Profile.m_PrepConstraints.ReadMs() );
    DebugText( X, Y++, Color, "SolCsts:%.2f", m_Profile.m_SolveConstraints.ReadMs() );
    DebugText( X, Y++, Color, "ShockPr:%.2f", m_Profile.m_ShockPropagation.ReadMs() );
    DebugText( X, Y++, Color, "IntPos :%.2f", m_Profile.m_IntegratePos.ReadMs() );
    DebugText( X, Y++, Color, "Render :%.2f", m_Profile.m_Render.ReadMs() );
    
    Y += 1;
    DebugText( X, Y++, Color, "nPrepCsts:%d nSolvCsts:%d", 
               m_Profile.m_nPrepConstraints,
               m_Profile.m_nSolveConstraints );
    
    DebugText( X, Y++, Color, "nBodVBod:%d nSphVSph:%d",
        m_Profile.m_nBodyBodyCollTests,
        m_Profile.m_nSphereSphereCollTests );

    DebugText( X, Y++, Color, "nCapVCap:%d nSphVCap:%d", 
        m_Profile.m_nCapsuleCapsuleCollTests,
        m_Profile.m_nSphereCapsuleCollTests );

    DebugText( X, Y++, Color, "nClusters:%d nNGons:%d", 
               m_Profile.m_nClusters,
               m_Profile.m_nNGons );
    
    DebugText( X, Y++, Color, "nSphVNGonC:%d nSphVNGonI:%d", 
               m_Profile.m_nSphereNGonCollTests,
               m_Profile.m_nSphereNGonIntersecTests );
               
    // Make sure render stats are reset    
    PHYSICS_DEBUG_STOP_TIMER( m_Profile.m_Render );
    PHYSICS_DEBUG_RESET_TIMER( m_Profile.m_Render );
}

//==============================================================================

#endif  //#ifdef ENABLE_PHYSICS_DEBUG
