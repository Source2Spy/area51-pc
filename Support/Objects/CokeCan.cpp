//==============================================================================
//
// CokeCan.cpp
//
// Two-particle cylinder physics
//
// The can is modelled with 2 spheres connected by an equal distance constraint.
// Rolling is faked - the correct roll speed is computed when in contact
// with the ground.
//
// Now because this is a very cheap cpu physics model, there is no stacking as such.
// but because cans are initially inactive, you can place them stacked - then when
// one of them becomes active, it makes any cans close by also become active,
// and the stack will all fall down. Cheeky. 
//
// Collision if performed as follows:
// 1 ) Can collision:   Spheres of each can are kept a set distance from each other.
// 2 ) World collision: Each sphere is just projected out of the colliding plane.
// 3 ) Actor collision: The can capsule is kept outside the actor capsule.
//
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "CokeCan.hpp"
#include "CollisionMgr/CollisionPrimatives.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "Entropy/Entropy.hpp"
#include "Objects/BaseProjectile.hpp"
#include "Objects/NetProjectile.hpp"
#include "Ragdoll/VerletCollision.hpp"
#include "Objects/Actor/Actor.hpp"

#ifdef X_EDITOR
#include "CollisionMgr/PolyCache.hpp"
#include "../../Apps/WorldEditor/WorldEditor.hpp"
#endif

//==============================================================================
// DEFINES
//==============================================================================

struct coke_can_profile
{
    f32         m_InvMass;
    f32         m_ActorPushVelocityScale;
    f32         m_GravityCmPerSecondSquared;
    f32         m_SurfaceFriction;
    f32         m_MajorAxisFriction;
    f32         m_Elasticity;
    f32         m_CollisionBackoffCm;
    f32         m_MinCollisionTravelCm;
    f32         m_CollisionBBoxInflateCm;
    f32         m_AirLinearDecayPerSecond;
    f32         m_AirAngularDecayPerSecond;
    f32         m_GroundLinearDecayPerSecond;
    f32         m_GroundAngularDecayPerSecond;
    f32         m_MaxSpeedCmPerSecond;
    f32         m_SleepSpeedSquaredSum;
    f32         m_PainBulletVelocityScale;
    f32         m_PainExplosionVelocityScale;
    f32         m_ActiveHoldSeconds;
    f32         m_AudioImpactSpeedCmPerSecond;
    f32         m_AudioMaxRollingRate;
    f32         m_AudioMinRollingRate;
    f32         m_AudioCollisionSpeedCmPerSecond;
    f32         m_ImpactAudioCooldownSeconds;
    const char* m_pBulletImpactSound;
    const char* m_pRollingSound;
    const char* m_pWorldImpactSound;
    const char* m_pCanImpactSound;
};

static const f32 k_MaxSubstepSeconds = 1.0f / 30.0f;
static const s32 k_VelocityIterations = 8;
static const s32 k_PositionIterations = 4;

static const coke_can_profile k_CokeCanProfiles[ coke_can::PROFILE_COUNT ] =
{
    // Small can
    { 1.0f,
      1.0f,
      -1960.0f,
      0.10f, 0.10f, 0.20f,
      0.1f, 0.1f, 20.0f,
      0.030015f, 0.030015f,
      0.301510f, 3.160815f,
      3000.0f, 3600.0f,
      625.0f, 1000.0f,
      4.0f / 30.0f,
      120.0f, 60.0f, 1.5f, 120.0f, 0.10f,
      "BulletImpactMetal",
      "Can_Roll_Loop",
      "Can_Impact_World",
      "Can_Impact_Can" },

    // Barrel
    { 1.0f / 20.0f,
      0.35f,
      -1960.0f,
      0.20f, 0.10f, 0.05f,
      0.1f, 0.1f, 20.0f,
      0.030015f, 0.045034f,
      1.224660f, 20.794415f,
      2250.0f, 3600.0f,
      100.0f, 300.0f,
      4.0f / 30.0f,
      150.0f, 60.0f, 1.5f, 150.0f, 0.15f,
      "BulletImpactRubber",
      "Barrel_Roll_Loop",
      "Barrel_Impact_World",
      "Barrel_Impact_Barrel" }
};


#ifndef X_RETAIL
static xbool    DEBUG_COKE_CAN                  = FALSE;
#endif

//=========================================================================
// EXTERNS
//=========================================================================

#ifdef X_EDITOR
extern xbool g_game_running;
extern xbool g_level_loading;
#endif

//=========================================================================
// EDITOR UTILITY FUNCTIONS
//=========================================================================

#ifdef X_EDITOR

static
xbool SphereIntersectsNGon( const vector3& ObjectCenter,
                            const vector3& SpherePos, f32 SphereRadius, f32 CollisionBackOff,
                            const vector3* pVerts, s32 nVerts,
                                  f32&     Depth,
                                  vector3& Normal )
{
    ASSERT( pVerts );
    ASSERT( nVerts >= 3 );

    // Compute plane for NGon
    plane Plane;
    Plane.Setup( pVerts[0], pVerts[1], pVerts[2] );

    // Far enough away from the plane?
    f32 DistFromPlane = Plane.Distance( SpherePos );
    if( x_abs( DistFromPlane ) > ( SphereRadius + CollisionBackOff ) )
        return FALSE;

    // Compute the closest point on the plane to the sphere
    vector3 PointOnPlane = SpherePos + ( DistFromPlane * Plane.Normal );

    // Check to see if point on plane is inside NGon
    for( s32 v = 0; v < nVerts; v++ )
    {
        // Lookup edge end pts
        const vector3& EdgeStart = pVerts[( v == 0 ) ? nVerts-1 : v-1 ];
        const vector3& EdgeEnd   = pVerts[ v ];

        // Exit loop if point is outside of edge
        vector3 EdgeDir        = EdgeEnd - EdgeStart;
        vector3 EdgeNormal     = EdgeDir.Cross( Plane.Normal );
        vector3 EdgePointDelta = EdgeStart - SpherePos;

        // Outside of edge?
        if( EdgeNormal.Dot( EdgePointDelta ) < 0.0f )
            return FALSE;
    }

    // Compute sphere top and bottom points with respect to plane
    vector3 SphereTopPoint     = SpherePos + ( SphereRadius * Plane.Normal );
    vector3 SphereBotPoint     = SpherePos - ( SphereRadius * Plane.Normal );
    f32     TopDist            = -Plane.Distance( SphereTopPoint );
    f32     BotDist            = -Plane.Distance( SphereBotPoint );
    f32     TopDistToCenterSqr = ( ObjectCenter - SphereTopPoint ).LengthSquared();
    f32     BotDistToCenterSqr = ( ObjectCenter - SphereBotPoint ).LengthSquared();
    
    // Choose point furthest away from object center so that coke is not pushed half way through plane
    f32 IntersectionDist = ( TopDistToCenterSqr > BotDistToCenterSqr ) ? TopDist : BotDist;
    
    // Move a tad further away from the plane for float safety
    IntersectionDist += ( CollisionBackOff * 1.5f ) * x_sign( IntersectionDist );
    
    // Smallest intersection so far?
    if( x_abs( IntersectionDist ) < x_abs( Depth ) )
    {
        // Record
        Depth  = IntersectionDist;
        Normal = Plane.Normal; 
    }
        
    // Point is inside all of NGon edges so sphere intersects
    return TRUE;
}

//==============================================================================

static
xbool SphereIntersectsWorld( const vector3& ObjectCenter,
                             const vector3& SpherePos, f32 SphereRadius, f32 CollisionBackOff,
                             f32& Depth, vector3& Normal )
{
    // Compute bbox of sphere with room for float error
    bbox SphereBBox( SpherePos, SphereRadius + ( CollisionBackOff * 2.0f ) );

    // Collect clusters
    g_PolyCache.BuildClusterList( SphereBBox, 
        object::TYPE_ALL_TYPES, 
        object::ATTR_COLLIDABLE, 
        object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );

    // Clear results
    xbool bIntersect = FALSE;
    
    // Loop over all clusters
    for( s32 iCL = 0; iCL < g_PolyCache.m_nClusters; iCL++ )
    {
        // Lookup cluster
        poly_cache::cluster& CL = *g_PolyCache.m_ClusterList[iCL];

        // Skip cluster if it doesn't intersect the sphere bbox
        if ( !SphereBBox.Intersect( CL.BBox ) )
            continue;

        // Loop over all quads in cluster
        for( s32 iQ = 0; iQ < (s32)CL.nQuads; iQ++ )
        {
            // Skip if bbox does not intersect sphere
            bbox* pBBox = (bbox*)(&CL.pBounds[iQ]);
            if( !SphereBBox.Intersect( *pBBox ) ) 
                continue;

            // Lookup quad vertices
            poly_cache::cluster::quad& QD = CL.pQuad[iQ];
            vector3 Vertices[4];
            Vertices[0] = CL.pPoint[ QD.iP[0] ];
            Vertices[1] = CL.pPoint[ QD.iP[1] ];
            Vertices[2] = CL.pPoint[ QD.iP[2] ];
            Vertices[3] = CL.pPoint[ QD.iP[3] ];
            s32 nVertices = ( CL.pBounds[iQ].Flags & BOUNDS_IS_QUAD ) ? 4 : 3;

            // Does sphere intersect NGon?
            bIntersect |= SphereIntersectsNGon( ObjectCenter, 
                                                SpherePos, SphereRadius, CollisionBackOff,
                                                Vertices, nVertices, Depth, Normal );
        }
    }

    // Return result
    return bIntersect;
}

//=========================================================================

xbool CokeCanIntersectsWorld( const vector3& ParticlePos0, 
                              const vector3& ParticlePos1, 
                                    f32      ParticleRadius, 
                                    f32      CollisionBackOff,
                                    f32&     Depth, 
                                    vector3& Normal )
{
    // Clear results
    Depth = F32_MAX;
    Normal.Zero();
    
    // Check for intersection and setup smallest depth
    xbool   bIntersect = FALSE;
    vector3 Center( ( ParticlePos0 + ParticlePos1 ) * 0.5f );
    bIntersect  = SphereIntersectsWorld( Center, ParticlePos0, ParticleRadius, CollisionBackOff, Depth, Normal );
    bIntersect |= SphereIntersectsWorld( Center, ParticlePos1, ParticleRadius, CollisionBackOff, Depth, Normal );
    
    // Return result
    return bIntersect;
}    

#endif  //#ifdef X_EDITOR


//=========================================================================
// OBJECT DESC
//=========================================================================

static struct coke_can_desc : public object_desc
{
    coke_can_desc( void ) : object_desc( object::TYPE_COKE_CAN, 
                                        "CokeCan",
                                        "PROPS",

                                        object::ATTR_SPACIAL_ENTRY          |
                                        object::ATTR_NEEDS_LOGIC_TIME        |
                                        object::ATTR_SOUND_SOURCE            |
                                        object::ATTR_COLLIDABLE             | 
                                        object::ATTR_BLOCKS_ALL_PROJECTILES | 
                                        object::ATTR_BLOCKS_ALL_ACTORS      | 
                                        object::ATTR_BLOCKS_RAGDOLL         | 
                                        object::ATTR_DAMAGEABLE             |
                                        object::ATTR_NO_RUNTIME_SAVE        |
                                        object::ATTR_CAST_SHADOWS           |
                                        object::ATTR_RENDERABLE,

                                        FLAGS_GENERIC_EDITOR_CREATE | FLAGS_NO_ICON |
                                        FLAGS_IS_DYNAMIC ) {}

    //-------------------------------------------------------------------------

    virtual object* Create( void ) { return new coke_can; }

    //-------------------------------------------------------------------------

#ifdef X_EDITOR

    virtual s32  OnEditorRender( object& Object ) const
    {
        object_desc::OnEditorRender( Object );
        return -1;
    }

#endif // X_EDITOR

} s_CokeCanDesc;

//=========================================================================

const object_desc& coke_can::GetTypeDesc( void ) const
{
    return s_CokeCanDesc;
}

//=========================================================================

const object_desc& coke_can::GetObjectType( void )
{
    return s_CokeCanDesc;
}

//=========================================================================

//=========================================================================

coke_can::coke_can( void ) :
    m_isInitialized      ( FALSE ),  // TRUE if initialized
    m_bOnGround         ( TRUE ),   // TRUE if lying on the ground
    m_ActiveSeconds     ( 0 ),      // Keeps physics active after an impact
    m_ParticleRadius    ( 0 ),      // Radius of particles
    m_ParticleDist      ( 0 ),      // Constraint distance
    m_Roll              ( 0 ),      // Roll of can
    m_RollRate          ( 0 ),      // Roll rate of can
    m_ImpactAudioCooldownSeconds( 0 ),
    m_iMajorAxis        ( 0 ),      // Longest axis of can
    m_MinInitVel        ( 0,0,0 ),  // Min initial velocity
    m_MaxInitVel        ( 0,0,0 ),  // Max initial velocity
    m_RollAudioID       ( 0 ),      // Can rolling audio id
    m_iProfile          ( PROFILE_CAN )
{
    m_FloorProperties.Init( 1.0f, 0.128f );
}

//=========================================================================

coke_can::~coke_can()
{
}

//=========================================================================

void coke_can::OnMove( const vector3& NewPos )
{
    // Call base class
    object::OnMove( NewPos );

#ifdef X_EDITOR
    // If being moved in the editor, re-initialize
    if( ( !g_game_running ) && ( GetAttrBits() & ( object::ATTR_EDITOR_SELECTED | object::ATTR_EDITOR_PLACEMENT_OBJECT ) ) )
    {
        InitPhysics();
    }
#endif
}

//=========================================================================

void coke_can::OnTransform( const matrix4& L2W )
{
    // Call base class
    object::OnTransform( L2W );

#ifdef X_EDITOR
    // If being moved in the editor, re-initialize
    if( ( !g_game_running ) && ( GetAttrBits() & ( object::ATTR_EDITOR_SELECTED | object::ATTR_EDITOR_PLACEMENT_OBJECT ) ) )
    {
        InitPhysics();
    }
#endif
}

//=========================================================================

bbox coke_can::GetLocalBBox( void ) const
{
    // Grab from geometry?
    geom* pGeom = m_SkinInst.GetGeom();
    if( pGeom )
    {
        return bbox( pGeom->m_BBox.GetCenter(), pGeom->m_BBox.GetRadius() );
    }

    return bbox( vector3( -50, -50, -50 ), vector3( 50,50,50 ) );
}

//===========================================================================

bbox coke_can::GetGeomBBox( void ) const
{
    // Grab from geometry?
    geom* pGeom = m_SkinInst.GetGeom();
    if( pGeom )
        return pGeom->m_BBox;

    return bbox( vector3( -50, -50, -50 ), vector3( 50,50,50 ) );
}

//===========================================================================

void coke_can::OnRender( void )
{
    // Geometry present?
    skin_geom* pSkinGeom = m_SkinInst.GetSkinGeom();
    if( !pSkinGeom )
        return;
        
    // Can only support 1 boned cans!
    ASSERTS( pSkinGeom->m_nBones == 1, "Coke cans can only have 1 bone!!" );        

    // Compute LOD mask
    u64 LODMask = m_SkinInst.GetLODMask( GetL2W() );
    if( LODMask == 0 )
        return;

    // Setup render flags
    u32    Flags   = ( GetFlagBits() & object::FLAG_CHECK_PLANES ) ? render::CLIPPED : 0;
    xcolor Ambient = m_FloorProperties.GetColor();
    
#ifdef X_EDITOR
    // Render transparent if selected in editor so you can see collision
    if ( GetAttrBits() & object::ATTR_EDITOR_SELECTED )
    {
        // Render collision now before the z buffer is primed which will stop the coll render!
        OnColRender( TRUE );

        // Render can transparent        
        Flags |= render::FADING_ALPHA;
        Ambient.A  = 192;
    }
#endif

    m_SkinInst.Render( &GetL2W(),
                       &GetL2W(),
                       1, Flags | GetRenderMode(),
                       LODMask, 
                       Ambient );

#ifndef X_RETAIL
    // Lookup profile
    const coke_can_profile& Profile = GetProfile();

    // Use this to show when cans are active
    if( ( DEBUG_COKE_CAN ) && ( GetSpeedSquaredSum() > Profile.m_SleepSpeedSquaredSum ) )
        OnColRender( FALSE );
#endif
}

//===============================================================================

void coke_can::OnRenderShadowCast( u64 ProjMask )
{
    // Lookup skin geometry
    skin_geom* pSkinGeom = m_SkinInst.GetSkinGeom();
    if( !pSkinGeom )
        return;

    // Coke cans can only support a single bone.
    ASSERTS( pSkinGeom->m_nBones == 1, "Coke cans can only have 1 bone!!" );

    // Setup render geometry
    u64 LODMask = m_SkinInst.GetLODMask( GetL2W() );
    if( LODMask == 0 )
        return;

    // Setup render flags
    u32 Flags = ( GetFlagBits() & object::FLAG_CHECK_PLANES ) ? render::CLIPPED : 0;

    // Render
    m_SkinInst.RenderShadowCast( &GetL2W(),
                                 &GetL2W(),
                                 1,
                                 Flags,
                                 LODMask,
                                 ProjMask );
}

//===============================================================================

#ifndef X_RETAIL
void coke_can::OnColRender( xbool bRenderHigh )
{
    ( void )bRenderHigh;

    // Render world bbox
    render::debug::Box( GetGeomBBox(), GetL2W(), XCOLOR_YELLOW );

    // Render the particles
    render::debug::Sphere( m_Particles[0].m_Pos, m_ParticleRadius, XCOLOR_GREEN );
    render::debug::Sphere( m_Particles[1].m_Pos, m_ParticleRadius, XCOLOR_GREEN );

    // Render constraint
    render::debug::Line( m_Particles[0].m_Pos, m_Particles[1].m_Pos, XCOLOR_RED );

    // Show energy
    //render::debug::Label( GetPosition(), XCOLOR_BLUE, "SpeedSquaredSum:%f", GetSpeedSquaredSum() );

    // Show 1 of the particle speed
    //render::debug::Label( m_Particles[0].m_Pos, XCOLOR_RED, "Speed:%f", m_Particles[0].m_Velocity.Length() );
    render::debug::Label( GetPosition(), XCOLOR_BLUE, "RollRate:%f", x_abs( m_RollRate ) );
}
#endif // X_RETAIL

//===============================================================================

void coke_can::OnAdvanceSimulation( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "coke_can::OnAdvanceSimulation" );

    // This fixes blue-printed cans from not working properly
    // Initialized physics?
    if( !m_isInitialized )
    {
        InitPhysics();
        if( !m_isInitialized )
        {
            return;
        }
    }

    // Lookup profile
    const coke_can_profile& Profile = GetProfile();

    if( x_isvalid( DeltaTime ) && (DeltaTime > 0.0f) )
        m_ImpactAudioCooldownSeconds = MAX( 0.0f, m_ImpactAudioCooldownSeconds - DeltaTime );
    
    // Only update physics if can is active or moving
    if( ( m_ActiveSeconds > 0.0f ) || IsMoving() )
    {
        ASSERTS( x_isvalid( DeltaTime ), "CokeCan received an invalid frame delta" );
        if( x_isvalid( DeltaTime ) && (DeltaTime > 0.0f) )
        {
            const s32 nSubsteps = x_max( 1, (s32)x_ceil( DeltaTime / k_MaxSubstepSeconds ) );
            const f32 SubstepSeconds = DeltaTime / (f32)nSubsteps;

            xarray<guid> Neighbors;
            for( s32 i = 0; i < nSubsteps; i++ )
            {
                const xbool WakeNeighbors = IsMoving();
                CollectCanNeighbors( SubstepSeconds, Neighbors );
                if( WakeNeighbors )
                {
                    WakeCanContacts( Neighbors );
                }
                Integrate( SubstepSeconds );
                ApplyConstraints( Neighbors, WakeNeighbors );
                ApplyDamping( SubstepSeconds );

                m_Roll += m_RollRate * SubstepSeconds;
                UpdateL2W();

                if( IsMoving() )
                {
                    m_ActiveSeconds = Profile.m_ActiveHoldSeconds;
                }
                else
                {
                    m_ActiveSeconds = MAX( 0.0f, m_ActiveSeconds - SubstepSeconds );
                    if( m_ActiveSeconds == 0.0f )
                    {
                        m_Particles[0].m_Velocity.Zero();
                        m_Particles[1].m_Velocity.Zero();
                        m_RollRate = 0.0f;

                        if( m_RollAudioID )
                        {
                            g_AudioMgr.Release( m_RollAudioID, 0.5f );
                            m_RollAudioID = 0;
                        }
                        break;
                    }
                }
            }
        }
    }

    // Update floor tracking
    m_FloorProperties.Update( GetPosition(), DeltaTime );
}

//===============================================================================

void coke_can::OnPain( const pain& Pain )
{
    // Lookup profile
    const coke_can_profile& Profile = GetProfile();

    // Prepare pain
    if( !Pain.ComputeDamageAndForce( GetLogicalName(), GetGuid(), GetBBox().GetCenter() ) )
        return;
    
    // Lookup pain info
    const vector3& Pos   = Pain.GetPosition();
          vector3  Dir   = Pain.GetDirection();
          f32      DeltaSpeed = Pain.GetForce();

    // Compute main axis between 2 spheres
    f32     T    = 0.5f;
    vector3 Axis = m_Particles[1].m_Pos - m_Particles[0].m_Pos;
    if( Axis.SafeNormalizeAndScale( m_ParticleRadius ) )
    {   
        // Compute far ends of can
        vector3 S = m_Particles[0].m_Pos - Axis;     
        vector3 E = m_Particles[1].m_Pos + Axis;     
        
        // Get distance ratio of pain between ends of can
        T = Pos.GetClosestPToLSegRatio( S, E );
    }

    // Direct impacts and explosions have different impulse models.
    if( !Pain.IsExplosion() )
    {
        // Scale force
        DeltaSpeed *= Profile.m_PainBulletVelocityScale;
    
        // Compute impulse based 100% pain direction
        vector3 DeltaVelocity = DeltaSpeed * Dir;
        
        // Apply to particles
        m_Particles[0].m_Velocity += DeltaVelocity * ( 1.0f - T );
        m_Particles[1].m_Velocity += DeltaVelocity * T;
    }
    else    
    {
        // Scale force
        DeltaSpeed *= Profile.m_PainExplosionVelocityScale;
    
        // Compute direction from explosion
        Dir = GetPosition() - Pos;
        if( !Dir.SafeNormalize() )
            Dir.Set( 0.0f, 1.0f, 0.0f );

        // Apply directional + upwards blast to particles
        vector3 DeltaVelocity = DeltaSpeed * ( ( 0.25f * Dir ) + ( vector3( 0.0f, 0.75f, 0.0f ) ) );
        m_Particles[0].m_Velocity += DeltaVelocity;
        m_Particles[1].m_Velocity += DeltaVelocity;

        // Apply spinning blast to particles
        DeltaVelocity = DeltaSpeed * 0.125f * ( Dir + vector3( 0.0f, 1.0f, 0.0f ) );
        m_Particles[0].m_Velocity += DeltaVelocity * ( 1.0f - T );
        m_Particles[1].m_Velocity += DeltaVelocity * T;
        
        // Flag as in air so damping doesn't happen until it lands again
        m_bOnGround = FALSE;
    }
    
    // Activate physics
    m_ActiveSeconds = Profile.m_ActiveHoldSeconds;

    // Audio
    voice_id VoiceID = g_AudioMgr.Play( Profile.m_pBulletImpactSound, Pos, GetZone1(), TRUE );
    g_AudioManager.NewAudioAlert( VoiceID, audio_manager::BULLET_IMPACTS, GetPosition(), GetZone1(), GetGuid() ); 
    
}
    
//===============================================================================

void coke_can::OnColCheck( void )
{
#ifdef X_EDITOR    
    // Editor select?
    if( g_CollisionMgr.IsEditorSelectRay() )
    {
        // Apply object orientated bounding box test
        g_CollisionMgr.StartApply( GetGuid() );
        g_CollisionMgr.ApplyOOBBox( GetGeomBBox(), GetL2W() );
        g_CollisionMgr.EndApply();
        return;
    }
#endif
    
    // Get moving object
    guid    MovingGuid = g_CollisionMgr.GetMovingObjGuid() ;
    object* pObject    = g_ObjMgr.GetObjectByGuid(MovingGuid) ;

    if( pObject && pObject->IsKindOf( actor::GetRTTI() ) &&
        ( g_CollisionMgr.GetDynamicPrimitive() == PRIMITIVE_DYNAMIC_CAPSULE ) )
    {
        if( !m_isInitialized )
        {
            InitPhysics();
            if( !m_isInitialized )
            {
                return;
            }
        }

        g_CollisionMgr.StartApply( GetGuid() );
        g_CollisionMgr.ApplyCapsule( m_Particles[0].m_Pos, m_Particles[1].m_Pos,
                                     m_ParticleRadius, GetMaterial() );
        g_CollisionMgr.EndApply();
        return;
    }

    // Collide with bullets, projectiles, or melee?
    if (        ( pObject ) 
            &&  (       ( pObject->IsKindOf( base_projectile::GetRTTI() ) )     // Normal projectiles
                    ||  ( pObject->IsKindOf( net_proj::GetRTTI() ) )            // Net projectiles
                    ||  ( pObject->IsKindOf( actor::GetRTTI() ) ) ) )           // For melee
    {
        // Apply object orientated bounding box test
        g_CollisionMgr.StartApply( GetGuid() );
        g_CollisionMgr.ApplyOOBBox( GetGeomBBox(), GetL2W() );
        g_CollisionMgr.EndApply();
    }
}

//===============================================================================
// Editor functions
//===============================================================================

void coke_can::OnEnumProp( prop_enum&    List )
{
    // Call base class
    object::OnEnumProp( List );

    // Geometry properties
    m_SkinInst.OnEnumProp( List );

    // Coke can properies
    List.PropEnumHeader ( "CokeCan", "Physically coke can", 0 ); 
    List.PropEnumEnum   ( "CokeCan\\Physics Model",  "CAN\0BARREL\0", "Physics model of the object.", PROP_TYPE_EXPOSE );
    List.PropEnumVector3( "CokeCan\\MinInitVel", "Minimum initial velocity of can in local space", 0 );
    List.PropEnumVector3( "CokeCan\\MaxInitVel", "Maximum initial velocity of can in local space", 0 );

#ifdef X_EDITOR    
    List.PropEnumFloat ( "CokeCan\\ParticleRadius",    "Radius of collision particles inside coke can", PROP_TYPE_MUST_ENUM | PROP_TYPE_READ_ONLY | PROP_TYPE_DONT_SAVE | PROP_TYPE_DONT_EXPORT );
    List.PropEnumButton( "CokeCan\\FixWorldIntersection", "Puts can in collision free position", PROP_TYPE_MUST_ENUM );
#endif    
}

//===============================================================================

xbool coke_can::OnProperty( prop_query&   I )
{
    // Call base class
    if( object::OnProperty( I ) )
        return TRUE;

    // Physics model
    if( I.IsVar( "CokeCan\\Physics Model" ) )
    {
        if( I.IsRead() )
        {
            switch( m_iProfile )
            {
            case PROFILE_CAN:       I.SetVarEnum( "CAN" );        break;
            case PROFILE_BARREL:    I.SetVarEnum( "BARREL" );     break;
            }
        }
        else
        {
            if( !x_stricmp( I.GetVarEnum(), "CAN" ) )
                m_iProfile = PROFILE_CAN;
            else if( !x_stricmp( I.GetVarEnum(), "BARREL" ) )
                m_iProfile = PROFILE_BARREL;            
        }
        return TRUE;
    }


    // Geometry
    if( m_SkinInst.OnProperty( I ) )
    {
        // Was geometry just selected?
        if( ( !I.IsRead() ) && ( I.IsVar( "RenderInst\\File" ) ) )
        {
            // Setup physics again
            InitPhysics();
        }

        return TRUE;
    }

    // Min init velocity?
    if( I.VarVector3( "CokeCan\\MinInitVel", m_MinInitVel ) )
    {
        // Was velocity just updated?
        if( !I.IsRead() )
        {
            // Setup physics again
            InitPhysics();
        }

        return TRUE;
    }

    // Max init velocity?
    if( I.VarVector3( "CokeCan\\MaxInitVel", m_MaxInitVel ) )
    {
        // Was velocity just updated?
        if( !I.IsRead() )
        {
            // Setup physics again
            InitPhysics();
        }

        return TRUE;
    }

#ifdef X_EDITOR    

    // Show collision radius
    if( I.IsVar( "CokeCan\\ParticleRadius" ) )
    {
        // Update UI?
        if( I.IsRead() )
        {
            I.SetVarFloat( m_ParticleRadius );
        }
        
        return TRUE;
    }

    // Move to collision free?
    if( I.IsVar( "CokeCan\\FixWorldIntersection" ) )
    {
        // Update UI?
        if( I.IsRead() )
        {
            I.SetVarButton( "FixWorldIntersection" );
        }            
        else
        {
            // Can only apply if just editing the coke can
            if(        ( !g_game_running  ) 
                    && ( !g_level_loading ) 
                    && ( GetAttrBits() & object::ATTR_EDITOR_SELECTED ) )
            {
                // Clear collision results
                f32     CollisionBackOff = GetProfile().m_CollisionBackoffCm;
                f32     Depth      = F32_MAX;
                vector3 Normal( 0.0f, 0.0f, 0.0f );
                vector3 DeltaPos( 0.0f, 0.0f, 0.0f );
                s32     MaxIters = 100;
                    
                // Does coke can intersect world?
                while( ( MaxIters-- ) && ( CokeCanIntersectsWorld( m_Particles[0].m_Pos + DeltaPos,    // ParticlePos0
                                                                   m_Particles[1].m_Pos + DeltaPos,    // ParticlePos1
                                                                   m_ParticleRadius,                   // ParticleRadius
                                                                   CollisionBackOff,                   // Collision back off dist
                                                                   Depth,                              // Intersection depth
                                                                   Normal ) ) )                        // Intersection normal
                {     
                    // Project out of collision
                    DeltaPos += Depth * Normal;
                }
                
                // Moved?
                if( DeltaPos != vector3( 0.0f, 0.0f, 0.0f ) )
                {
                    // Lookup current position
                    vector3 CurrPos = GetPosition();

                    // If this is a blue-print, use the anchor position!
                    editor_blueprint_ref* pBlueprintRef = NULL;
                    g_WorldEditor.GetBlueprintRefContainingObject2( GetGuid(), &pBlueprintRef );
                    if( pBlueprintRef )
                    {
                        // Use anchor position
                        object* pAnchor = g_ObjMgr.GetObjectByGuid( pBlueprintRef->Anchor );
                        if( pAnchor )
                            CurrPos = pAnchor->GetPosition();
                    }                

                    // Move the position via the world editor so that blue-prints are correctly saved/undo/redo etc
                    prop_query PropQuery;
                    PropQuery.WQueryVector3( "Base\\Position", CurrPos + DeltaPos );
                    g_WorldEditor.OnProperty( PropQuery );
                }                
            }            
        }
                    
        return TRUE;
    }
#endif  //#ifdef X_EDITOR    

    return FALSE;
}

//===============================================================================

// Misc
const coke_can_profile& coke_can::GetProfile( void ) const
{
    // Lookup profile
    ASSERT( m_iProfile >= 0 );
    ASSERT( m_iProfile < PROFILE_COUNT );
    return k_CokeCanProfiles[ m_iProfile ];
}

//===============================================================================

// Simple structure used for computing bbox axis info
struct axis_info
{
    s32 Index;
    f32 Length;
};

// Sorts axis info based on axis length
s32 CompareAxisInfo( const void* pA, const void* pB )
{
    // Lookup anims
    axis_info* pInfoA = ( axis_info* )pA;
    axis_info* pInfoB = ( axis_info* )pB;

    if( pInfoA->Length > pInfoB->Length )
        return 1;
    if( pInfoA->Length < pInfoB->Length )
        return -1;

    return 0;
}

//===============================================================================

void coke_can::InitPhysics( void )
{
    s32 i;
    m_isInitialized = FALSE;

    // Lookup geometry
    geom* pGeom = m_SkinInst.GetGeom();
    if( !pGeom )
        return;

    // Lookup profile
    // Lookup geometry info
    bbox& BBox = pGeom->m_BBox;

    // Compute axis info
    axis_info AxisInfo[3];
    for ( i = 0; i < 3; i++ )
    {
        AxisInfo[i].Index  = i;
        AxisInfo[i].Length = BBox.Max[i] - BBox.Min[i];
    }

    // Sort from smallest to largest
    x_qsort( AxisInfo, 3, sizeof( axis_info ), CompareAxisInfo );
    
    // Lookup axis info
    s32 iAxis0 = AxisInfo[0].Index;
    s32 iAxis1 = AxisInfo[1].Index;
    s32 iAxis2 = AxisInfo[2].Index;

    // Compute radius to use
    m_ParticleRadius = 0.5f * x_max( AxisInfo[0].Length, AxisInfo[1].Length );

    // Compute height
    //f32 Height = BBox.Max[iAxis2] - BBox.Min[iAxis2];
    
    // Setup positions in local space
    vector3 LocalTop;
    LocalTop[iAxis0] = ( BBox.Min[iAxis0] + BBox.Max[iAxis0] ) * 0.5f;
    LocalTop[iAxis1] = ( BBox.Min[iAxis1] + BBox.Max[iAxis1] ) * 0.5f;
    LocalTop[iAxis2] = BBox.Max[iAxis2] - m_ParticleRadius;

    vector3 LocalBot;
    LocalBot[iAxis0] = ( BBox.Min[iAxis0] + BBox.Max[iAxis0] ) * 0.5f;
    LocalBot[iAxis1] = ( BBox.Min[iAxis1] + BBox.Max[iAxis1] ) * 0.5f;
    LocalBot[iAxis2] = BBox.Min[iAxis2] + m_ParticleRadius;

    // Compute distance constraint
    m_ParticleDist = x_abs( LocalTop[iAxis2] - LocalBot[iAxis2] );

    // Put into world space
    const matrix4& L2W = GetL2W();
    vector3 WorldTop = L2W * LocalTop;
    vector3 WorldBot = L2W * LocalBot;

    // Init particles
    m_Particles[0].m_BindPos = LocalTop;
    m_Particles[0].m_Pos =
    m_Particles[0].m_LastCollPos = WorldTop;
    m_Particles[0].m_Velocity.Zero();

    m_Particles[1].m_BindPos = LocalBot;
    m_Particles[1].m_Pos =
    m_Particles[1].m_LastCollPos = WorldBot;
    m_Particles[1].m_Velocity.Zero();

    // Compute local space random init vel
    vector3 InitLocalVel( x_frand( m_MinInitVel.GetX(), m_MaxInitVel.GetX() ),
                          x_frand( m_MinInitVel.GetY(), m_MaxInitVel.GetY() ),
                          x_frand( m_MinInitVel.GetZ(), m_MaxInitVel.GetZ() ) );

    // Compute world velocity
    vector3 InitWorldVel = L2W.RotateVector( InitLocalVel );

    // Setup init velocity
    m_Particles[0].m_Velocity = InitWorldVel;
    m_Particles[1].m_Velocity = InitWorldVel;

    // Keep major axis
    m_iMajorAxis = iAxis2;

    // Clear roll
    m_Roll      = 0.0f;
    m_RollRate = 0.0f;

    // Force local bbox to recompute
    SetFlagBits( GetFlagBits() | object::FLAG_DIRTY_TRANSFORM );
    m_isInitialized = TRUE;
}

//===============================================================================

f32 coke_can::GetSpeedSquaredSum( void ) const
{
    // Accumulate velocities squared
    f32 E;
    E  = m_Particles[0].m_Velocity.LengthSquared();
    E += m_Particles[1].m_Velocity.LengthSquared();
    
    return E;
}

//===============================================================================

xbool coke_can::IsMoving( void ) const
{
    return GetSpeedSquaredSum() > GetProfile().m_SleepSpeedSquaredSum;
}

//==============================================================================

void coke_can::UpdateL2W( void )
{
    // Clear L2W
    matrix4 L2W;
    L2W.Identity();

    // Compute initial direction
    vector3 InitDir = m_Particles[1].m_BindPos - m_Particles[0].m_BindPos;
    vector3 CurrDir = m_Particles[1].m_Pos     - m_Particles[0].m_Pos;

    // Compute rotation
    quaternion Rot;
    Rot.Setup( InitDir, CurrDir );
    L2W.SetRotation( Rot );

    // Compute translation
    L2W.SetTranslation( 0.5f * ( m_Particles[0].m_Pos + m_Particles[1].m_Pos ) );

    // Apply roll
    switch( m_iMajorAxis )
    {
        case 0: L2W.PreRotateX( m_Roll );  break;
        case 1: L2W.PreRotateY( m_Roll );  break;
        case 2: L2W.PreRotateZ( m_Roll );  break;
    }

    // Update
    OnTransform( L2W );
}

//===============================================================================

void coke_can::Integrate( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "coke_can::Integrate" );

    // Nothing to do?
    if( DeltaTime == 0 )
        return;

    // Lookup profile
    const coke_can_profile& Profile = GetProfile();

    // Setup constants
    vector3 Gravity( 0, Profile.m_GravityCmPerSecondSquared, 0 );

    // Integrate particles
    for ( s32 i = 0; i < 2; i++ )
    {
        // Lookup particle
        particle& Particle = m_Particles[i];

        Particle.m_Velocity += Gravity * DeltaTime;
        Particle.m_Pos      += Particle.m_Velocity * DeltaTime;
    }
}

//===============================================================================

void coke_can::ApplyEqualDistConstraint( particle& ParticleA, particle& ParticleB, f32 EqualDist )
{
    // Get distance between particles
    vector3 Delta   = ParticleB.m_Pos - ParticleA.m_Pos;
    f32     DistSqr = Delta.LengthSquared();
    if( DistSqr < 0.001f )
        return;

    // Move to target dist
    f32 Dist = x_sqrt( DistSqr );
    f32 Diff = ( Dist - EqualDist ) / Dist;

    // Scale
    Delta *= Diff * 0.5f;

    // Apply deltas
    ParticleA.m_Pos += Delta;
    ParticleB.m_Pos -= Delta;

    // Remove velocity along the constrained axis
    vector3 Axis = ParticleB.m_Pos - ParticleA.m_Pos;
    if( Axis.SafeNormalize() )
    {
        const f32 RelativeAxisSpeed = ( ParticleB.m_Velocity - ParticleA.m_Velocity ).Dot( Axis );
        const vector3 Correction = 0.5f * RelativeAxisSpeed * Axis;
        ParticleA.m_Velocity += Correction;
        ParticleB.m_Velocity -= Correction;
    }
}

//===============================================================================

f32 coke_can::ApplyMinDistConstraint( particle& ParticleA, particle& ParticleB, f32 MinDist,
                                      f32 InvMassA, f32 InvMassB, f32 Elasticity, f32 Friction )
{
    const f32 TotalInvMass = InvMassA + InvMassB;
    ASSERT( TotalInvMass > 0.0f );
    const vector3 OtherVelocity = InvMassB > 0.0f ? ParticleB.m_Velocity : vector3( 0.0f, 0.0f, 0.0f );

    vector3   Normal   = ParticleA.m_Pos - ParticleB.m_Pos;
    const f32 Distance = Normal.Length();
    if( Distance > MinDist + CAPSULE_CAST_EPSILON )
    {
        return 0.0f;
    }
    if( !Normal.SafeNormalize() )
    {
        Normal = ParticleA.m_Velocity - OtherVelocity;
        if( !Normal.SafeNormalize() )
        {
            Normal.Set( 1.0f, 0.0f, 0.0f );
        }
    }

    const vector3 Correction = Normal * ( x_max( 0.0f, MinDist - Distance ) / TotalInvMass );
    ParticleA.m_Pos += InvMassA * Correction;
    ParticleB.m_Pos -= InvMassB * Correction;

    velocity_constraint* pConstraint = NULL;
    for( s32 i = 0; i < m_VelocityConstraints.GetCount(); i++ )
    {
        velocity_constraint& Constraint = m_VelocityConstraints[i];
        if( ( Constraint.pParticleA == &ParticleA ) && ( Constraint.pParticleB == &ParticleB ) )
        {
            pConstraint = &Constraint;
            break;
        }
    }
    const xbool NewContact = pConstraint == NULL;
    if( NewContact )
    {
        pConstraint = &m_VelocityConstraints.Append();
    }
    pConstraint->pParticleA = &ParticleA;
    pConstraint->pParticleB = &ParticleB;
    pConstraint->Plane.Setup( ParticleB.m_Pos, Normal );
    pConstraint->Distance = MinDist;
    pConstraint->InvMassA = InvMassA;
    pConstraint->InvMassB = InvMassB;

    vector3 RelativeVelocity = ParticleA.m_Velocity - OtherVelocity;
    const f32 NormalSpeed    = RelativeVelocity.Dot( Normal );
    const f32 ImpactSpeedSqr = x_sqr( x_min( NormalSpeed, 0.0f ) );
    if( NormalSpeed < 0.0f )
    {
        const f32 Restitution = NewContact ? Elasticity : 0.0f;
        const f32 NormalImpulse = -( 1.0f + Restitution ) * NormalSpeed / TotalInvMass;
        const vector3 NormalDeltaVelocity = NormalImpulse * Normal;
        ParticleA.m_Velocity += InvMassA * NormalDeltaVelocity;
        ParticleB.m_Velocity -= InvMassB * NormalDeltaVelocity;

        RelativeVelocity = ParticleA.m_Velocity;
        if( InvMassB > 0.0f )
        {
            RelativeVelocity -= ParticleB.m_Velocity;
        }
        vector3 TangentVelocity = RelativeVelocity - Normal * RelativeVelocity.Dot( Normal );
        const f32 TangentSpeed = TangentVelocity.Length();
        if( TangentSpeed > 0.0001f )
        {
            const f32 TangentImpulse = x_min( TangentSpeed / TotalInvMass, Friction * NormalImpulse );
            TangentVelocity *= TangentImpulse / TangentSpeed;
            ParticleA.m_Velocity -= InvMassA * TangentVelocity;
            ParticleB.m_Velocity += InvMassB * TangentVelocity;
        }
    }
    return ImpactSpeedSqr;
}

//===============================================================================

void coke_can::ApplyDistConstraints( void )
{
    // Keep particles at set distance
    ApplyEqualDistConstraint( m_Particles[0], m_Particles[1], m_ParticleDist );
}

//==============================================================================

xbool coke_can::ApplyCapsuleConstraint( const vector3& Bottom, const vector3& Top, f32 Radius,
                                        f32 ContactOffset, const vector3& Velocity )
{
    vector3 CoincidentNormal = Velocity;
    if( !CoincidentNormal.SafeNormalize() )
    {
        CoincidentNormal.Set( 1.0f, 0.0f, 0.0f );
    }

    vector3 Normal;
    const f32 Separation = ComputeCapsuleCapsuleSeparation( m_Particles[0].m_Pos, m_Particles[1].m_Pos,
                                                            m_ParticleRadius, Bottom, Top, Radius,
                                                            CoincidentNormal, Normal );
    if( Separation > ContactOffset + CAPSULE_CAST_EPSILON )
    {
        return FALSE;
    }

    const f32 CorrectionDepth = x_max( 0.0f, ContactOffset - Separation );
    const f32 PushSpeed = x_max( 0.0f, Velocity.Dot( Normal ) ) * GetProfile().m_ActorPushVelocityScale;
    xbool Changed = FALSE;
    for( s32 i = 0; i < 2; i++ )
    {
        particle& Particle = m_Particles[i];
        if( CorrectionDepth > CAPSULE_CAST_EPSILON )
        {
            Particle.m_Pos += Normal * CorrectionDepth;
            Changed = TRUE;
        }
        const f32 Speed = Particle.m_Velocity.Dot( Normal );
        if( PushSpeed > Speed )
        {
            Particle.m_Velocity += ( PushSpeed - Speed ) * Normal;
            Changed = TRUE;
        }
    }
    return Changed;
}

//==============================================================================

void coke_can::ApplyCollConstraints( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "coke_can::ApplyCollConstraints" );

    s32   i;

    // Lookup profile
    const coke_can_profile& Profile = GetProfile();

    // Compute world bbox taking particle velocities into account
    bbox WorldBBox;
    WorldBBox.Clear();
    for ( i = 0; i < 2; i++ )
    {
        // Get particle
        particle& Particle = m_Particles[i];

        // Compute movement distance of particle (min of 1.0f)
        vector3 Delta   = Particle.m_Pos - Particle.m_LastCollPos;
        f32     DistSqr = Delta.LengthSquared();
        f32     Dist    = 1.0f; 
        if( DistSqr > 1.0f )
            Dist = x_sqrt( DistSqr );
            
        // Compute particle movement bbox, taking projection into account
        bbox ParticleBBox( Particle.m_Pos, Dist );
        
        // Accumulate to world bounds
        WorldBBox += ParticleBBox;
    }
    
    const xbool bWasOnGround = m_bOnGround;

    // Clear on ground flag
    m_bOnGround = FALSE;

    // Inflate to take particle radius into account and for a bit of safety
    f32 InflateAmt = m_ParticleRadius + Profile.m_CollisionBBoxInflateCm + Profile.m_MinCollisionTravelCm;
    WorldBBox.Inflate( InflateAmt,InflateAmt,InflateAmt ); 

    // Collect possible world collision objects.
    VerletCollision_CollectObjects( WorldBBox );

    // Compute world space major axis and roll axis
    vector3 LocalMajorAxis( 0,0,0 );
    LocalMajorAxis[m_iMajorAxis] = 1.0f;
    vector3 MajorAxis = GetL2W().RotateVector( LocalMajorAxis );
    plane   MajorAxisPlane( MajorAxis, 0 );
    vector3 RollAxis   = v3_Cross( vector3( 0, -1, 0 ), MajorAxis );
    
    // Clear max values
    f32     MaxRollRate       = 0.0f;
    f32     MaxImpactSpeedSqr = 0.0f;

    // Apply collision constraints
    for ( i = 0; i < 2; i++ )
    {
        // Get particle
        particle& Particle = m_Particles[i];

        // Compute start and end pts
        vector3 S = Particle.m_LastCollPos;
        vector3 E = Particle.m_Pos;
        vector3 Delta = E-S;
        f32     DistSq = Delta.LengthSquared();
        if( DistSq < x_sqr( Profile.m_MinCollisionTravelCm ) )
            continue;

        sphere_cast Cast;

        // Collision?
        if( VerletCollision_SphereCast( S, E, m_ParticleRadius, Cast ) )
        {
            const xbool NewContact = AddWorldConstraint( Particle, Cast.m_CollPlane );
            const f32 Restitution = NewContact ? Profile.m_Elasticity : 0.0f;

            // Pull back from collision a tad
            f32     T     = Cast.m_CollT;
            f32     Dist  = x_sqrt( DistSq );
            T -= Profile.m_CollisionBackoffCm / Dist;
            if( T < 0 )
                T = 0;

            // Put new start pos at collision pos
            S = S + ( T * Delta );

            // On the ground?
            if( Cast.m_CollPlane.Normal.GetY() > 0.5f )
                m_bOnGround = TRUE;

            // Get penetration depth of the end point we wanted to reach
            Dist = Cast.m_CollPlane.Distance( E ) - m_ParticleRadius;

            // Split vel into components
            vector3 Vel = Particle.m_Velocity;
            vector3 Perp, Para;
            Cast.m_CollPlane.GetComponents( Vel, Para, Perp );

            const f32 NormalSpeed = Perp.Dot( Cast.m_CollPlane.Normal );
            const f32 IncomingSpeed = x_max( 0.0f, -NormalSpeed );

            // Compute impact speed squared into plane and update max
            f32 ImpactSpeedSqr = x_sqr( IncomingSpeed );
            MaxImpactSpeedSqr = x_max( MaxImpactSpeedSqr, ImpactSpeedSqr );

            if( IncomingSpeed > 0.0f )
            {
                const f32 NormalDeltaSpeed = ( 1.0f + Restitution ) * IncomingSpeed;

                // Apply surface friction
                const f32 TangentSpeed = Para.Length();
                if( TangentSpeed > 0.0001f )
                {
                    const f32 FrictionDeltaSpeed = x_min( TangentSpeed,
                                                          Profile.m_SurfaceFriction * NormalDeltaSpeed );
                    Para *= ( TangentSpeed - FrictionDeltaSpeed ) / TangentSpeed;
                }

                // Apply axial friction
                vector3 MajorParallel, MajorAxisVelocity;
                MajorAxisPlane.GetComponents( Para, MajorParallel, MajorAxisVelocity );
                const f32 MajorAxisSpeed = MajorAxisVelocity.Length();
                if( MajorAxisSpeed > 0.0001f )
                {
                    const f32 AxisFrictionDeltaSpeed = x_min( MajorAxisSpeed,
                                                              Profile.m_MajorAxisFriction * NormalDeltaSpeed );
                    MajorAxisVelocity *= ( MajorAxisSpeed - AxisFrictionDeltaSpeed ) / MajorAxisSpeed;
                    Para = MajorParallel + MajorAxisVelocity;
                }

                Perp = Restitution * IncomingSpeed * Cast.m_CollPlane.Normal;
            }

            // Compute new vel
            Vel = Perp + Para;

            // Compute roll speed
            f32 RollRate = v3_Dot( Para, RollAxis ) / m_ParticleRadius;
            if( x_abs( RollRate ) > x_abs( MaxRollRate ) )
                MaxRollRate = RollRate;

            // Project end point out of plane that was collided with
            E += Cast.m_CollPlane.Normal * ( -Dist + Profile.m_CollisionBackoffCm );

            // Now see how close we can get to the final projected pos
            if( VerletCollision_SphereCast( S, E, m_ParticleRadius, Cast ) )
            {
                AddWorldConstraint( Particle, Cast.m_CollPlane );

                // Pull back from collision a tad
                T     = Cast.m_CollT;
                Delta = E - S;
                Dist  = Delta.Length();
                if( Dist > 0.0f )
                    T -= Profile.m_CollisionBackoffCm / Dist;
                if( T < 0 )
                    T = 0;

                // Setup new end pt
                E = S + ( T * ( E - S ) );
            }

            // Set new velocity
            Particle.m_Velocity = Vel;

            // Set new position
            Particle.m_Pos = E;
        }

        // Update last collision free pos
        Particle.m_LastCollPos = Particle.m_Pos;
    }

    // Update roll speed?
    if( MaxRollRate != 0.0f )
    {
        m_RollRate = MaxRollRate;
    }
    
    // Audio for roll
    f32 RollMagnitude = x_abs( MaxRollRate );
    if( RollMagnitude > Profile.m_AudioMinRollingRate )
    {
        // Is sound not playing?
        if( m_RollAudioID == 0 )
        {
            // Play the roll!
            m_RollAudioID = g_AudioMgr.PlayVolumeClipped( Profile.m_pRollingSound, GetPosition(), GetZone1(), TRUE );
        }

        // Now adjust volume and pitch based on the velocity.
        f32 Velocity = x_min( RollMagnitude, Profile.m_AudioMaxRollingRate ) - Profile.m_AudioMinRollingRate;
        f32 Range    = Profile.m_AudioMaxRollingRate - Profile.m_AudioMinRollingRate;
        f32 Scale    = Velocity / Range;
        f32 Volume   = 0.1f  + Scale * 0.9f;  // volume range is [0.10..1.0]
        f32 Pitch    = 0.94f + Scale * 0.06f; // pitch range is  [0.94..1.0]
        g_AudioMgr.SetVolume( m_RollAudioID, Volume );
        g_AudioMgr.SetPitch( m_RollAudioID, Pitch );
    }
    else
    {
        if( m_RollAudioID )
        {
            g_AudioMgr.Release( m_RollAudioID, 0.5f );
            m_RollAudioID = 0;
        }
    }

    // Play audio impact?
    const f32 HardGroundImpactSpeed = 2.0f * Profile.m_AudioImpactSpeedCmPerSecond;
    if( (m_ImpactAudioCooldownSeconds == 0.0f) &&
        (MaxImpactSpeedSqr > x_sqr( Profile.m_AudioImpactSpeedCmPerSecond )) &&
        (!bWasOnGround || (MaxImpactSpeedSqr > x_sqr( HardGroundImpactSpeed ))) )
    {
        // Rob - hookup volume control here if you want to...
        //f32 ImpactSpeed = x_sqrt( ImpactSpeedSqr );
        //f32 Volume = x_min( 1.0f, ImpactSpeed * ?? );

        // Play audio
        g_AudioMgr.PlayVolumeClipped( Profile.m_pWorldImpactSound, GetPosition(), GetZone1(), TRUE );
        m_ImpactAudioCooldownSeconds = Profile.m_ImpactAudioCooldownSeconds;
    }
}

//==============================================================================

void coke_can::ApplyCanConstraints( coke_can& CokeCan, xbool WakeNeighbors )
{
    // Lookup profile
    const coke_can_profile& Profile = GetProfile();
    const coke_can_profile& OtherProfile = CokeCan.GetProfile();
    if( !HasCanContact( CokeCan ) )
    {
        return;
    }
    if( WakeNeighbors )
    {
        CokeCan.m_ActiveSeconds = OtherProfile.m_ActiveHoldSeconds;
    }
    const xbool OtherActive = ( CokeCan.m_ActiveSeconds > 0.0f ) || CokeCan.IsMoving();

    // Compute dist to keep particles away
    f32 MinDist = m_ParticleRadius + CokeCan.m_ParticleRadius +
                  x_max( Profile.m_CollisionBackoffCm, OtherProfile.m_CollisionBackoffCm );

    // Make sure masses are valid
    ASSERT( Profile.m_InvMass > 0.0f );
    ASSERT( OtherProfile.m_InvMass > 0.0f );
    
    // Compute mass info
    f32 InvMassA = Profile.m_InvMass;
    f32 InvMassB = OtherActive ? OtherProfile.m_InvMass : 0.0f;
    f32 Elasticity = x_min( Profile.m_Elasticity, OtherProfile.m_Elasticity );
    f32 Friction = x_sqrt( Profile.m_SurfaceFriction * OtherProfile.m_SurfaceFriction );
    
    // Keep particles a set distance from each other
    f32 MaxImpactSpeedSqr = 0.0f;
    
    MaxImpactSpeedSqr = x_max( MaxImpactSpeedSqr, ApplyMinDistConstraint( m_Particles[0], CokeCan.m_Particles[0], MinDist,
                                                                          InvMassA, InvMassB, Elasticity, Friction ) );
    
    MaxImpactSpeedSqr = x_max( MaxImpactSpeedSqr, ApplyMinDistConstraint( m_Particles[0], CokeCan.m_Particles[1], MinDist,
                                                                          InvMassA, InvMassB, Elasticity, Friction ) );
    
    MaxImpactSpeedSqr = x_max( MaxImpactSpeedSqr, ApplyMinDistConstraint( m_Particles[1], CokeCan.m_Particles[0], MinDist,
                                                                          InvMassA, InvMassB, Elasticity, Friction ) );
    
    MaxImpactSpeedSqr = x_max( MaxImpactSpeedSqr, ApplyMinDistConstraint( m_Particles[1], CokeCan.m_Particles[1], MinDist,
                                                                          InvMassA, InvMassB, Elasticity, Friction ) );

    // Play audio?
    if( (m_ImpactAudioCooldownSeconds == 0.0f) &&
        (MaxImpactSpeedSqr > x_sqr( Profile.m_AudioCollisionSpeedCmPerSecond )) )
    {
        // Play some coke can on coke can collision audio!
        g_AudioMgr.PlayVolumeClipped( Profile.m_pCanImpactSound, GetPosition(), GetZone1(), TRUE );
        m_ImpactAudioCooldownSeconds = Profile.m_ImpactAudioCooldownSeconds;
    }
}

//==============================================================================

void coke_can::CollectCanNeighbors( f32 DeltaTime, xarray<guid>& Neighbors )
{
    const coke_can_profile& Profile = GetProfile();
    const vector3 Gravity( 0.0f, Profile.m_GravityCmPerSecondSquared, 0.0f );
    bbox Bounds;
    Bounds.Clear();
    for( s32 i = 0; i < 2; i++ )
    {
        Bounds += m_Particles[i].m_Pos;
        Bounds += m_Particles[i].m_Pos + ( m_Particles[i].m_Velocity + Gravity * DeltaTime ) * DeltaTime;
    }
    const f32 Radius = m_ParticleRadius + Profile.m_CollisionBackoffCm;
    Bounds.Inflate( Radius, Radius, Radius );

    Neighbors.Clear();
    g_ObjMgr.SelectBBox( object::ATTR_COLLIDABLE, Bounds, object::TYPE_COKE_CAN );
    for( slot_id Slot = g_ObjMgr.StartLoop(); Slot != SLOT_NULL; Slot = g_ObjMgr.GetNextResult( Slot ) )
    {
        object* pObject = g_ObjMgr.GetObjectBySlot( Slot );
        if( pObject != this )
        {
            Neighbors.Append( pObject->GetGuid() );
        }
    }
    g_ObjMgr.EndLoop();

    for( s32 i = 0; i < Neighbors.GetCount(); i++ )
    {
        object* pObject = g_ObjMgr.GetObjectByGuid( Neighbors[i] );
        if( pObject )
        {
            coke_can& Can = coke_can::GetSafeType( *pObject );
            if( !Can.m_isInitialized )
            {
                Can.InitPhysics();
            }
        }
    }
}

//==============================================================================

xbool coke_can::HasCanContact( const coke_can& CokeCan ) const
{
    if( !CokeCan.m_isInitialized )
    {
        return FALSE;
    }
    const f32 Distance = m_ParticleRadius + CokeCan.m_ParticleRadius +
                         x_max( GetProfile().m_CollisionBackoffCm, CokeCan.GetProfile().m_CollisionBackoffCm ) +
                         CAPSULE_CAST_EPSILON;
    for( s32 i = 0; i < 2; i++ )
    {
        for( s32 j = 0; j < 2; j++ )
        {
            if( ( m_Particles[i].m_Pos - CokeCan.m_Particles[j].m_Pos ).LengthSquared() <= x_sqr( Distance ) )
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

//==============================================================================

void coke_can::WakeCanContacts( const xarray<guid>& Neighbors )
{
    for( s32 i = 0; i < Neighbors.GetCount(); i++ )
    {
        object* pObject = g_ObjMgr.GetObjectByGuid( Neighbors[i] );
        if( pObject )
        {
            coke_can& Can = coke_can::GetSafeType( *pObject );
            if( HasCanContact( Can ) )
            {
                Can.m_ActiveSeconds = Can.GetProfile().m_ActiveHoldSeconds;
            }
        }
    }
}

//==============================================================================

xbool coke_can::ApplyActorConstraints( const vector3& Position, f32 Radius, f32 Height,
                                       f32 ContactOffset, const vector3& Velocity )
{
    if( !m_isInitialized )
    {
        InitPhysics();
        if( !m_isInitialized )
        {
            return FALSE;
        }
    }

    Radius = x_min( Radius, Height * 0.5f );
    if( ( Radius <= 0.0f ) || ( Height <= 0.0f ) )
    {
        return FALSE;
    }

    const vector3 Bottom = Position + vector3( 0.0f, Radius, 0.0f );
    const vector3 Top    = Position + vector3( 0.0f, Height - Radius, 0.0f );
    if( ApplyCapsuleConstraint( Bottom, Top, Radius, ContactOffset, Velocity ) )
    {
        m_ActiveSeconds = GetProfile().m_ActiveHoldSeconds;
        m_VelocityConstraints.Clear();
        ApplyCollConstraints();
        SolveVelocityConstraints();
        UpdateL2W();
        return TRUE;
    }
    return FALSE;
}

//==============================================================================

void coke_can::ApplyConstraints( const xarray<guid>& Neighbors, xbool WakeNeighbors )
{
    m_VelocityConstraints.Clear();
    for( s32 Iteration = 0; Iteration < k_PositionIterations; Iteration++ )
    {
        const vector3 OldA = m_Particles[0].m_Pos;
        const vector3 OldB = m_Particles[1].m_Pos;
        ApplyDistConstraints();

        for( s32 i = 0; i < Neighbors.GetCount(); i++ )
        {
            object* pObject = g_ObjMgr.GetObjectByGuid( Neighbors[i] );
            if( pObject )
            {
                ApplyCanConstraints( coke_can::GetSafeType( *pObject ), WakeNeighbors );
            }
        }

        ApplyDistConstraints();
        ApplyCollConstraints();
        if( ( m_Particles[0].m_Pos - OldA ).LengthSquared() +
            ( m_Particles[1].m_Pos - OldB ).LengthSquared() <= x_sqr( CAPSULE_CAST_EPSILON ) )
        {
            break;
        }
    }
    SolveVelocityConstraints();
}

//==============================================================================

xbool coke_can::AddWorldConstraint( particle& Particle, const plane& Plane )
{
    for( s32 i = 0; i < m_VelocityConstraints.GetCount(); i++ )
    {
        velocity_constraint& Constraint = m_VelocityConstraints[i];
        if( ( Constraint.pParticleA == &Particle ) && !Constraint.pParticleB &&
            ( Constraint.Plane.Normal.Dot( Plane.Normal ) > 0.999f ) )
        {
            Constraint.Plane = Plane;
            return FALSE;
        }
    }
    velocity_constraint& Constraint = m_VelocityConstraints.Append();
    Constraint.pParticleA = &Particle;
    Constraint.pParticleB = NULL;
    Constraint.Plane      = Plane;
    Constraint.Distance   = m_ParticleRadius + GetProfile().m_CollisionBackoffCm;
    Constraint.InvMassA   = GetProfile().m_InvMass;
    Constraint.InvMassB   = 0.0f;
    return TRUE;
}

//==============================================================================

void coke_can::SolveVelocityConstraints( void )
{
    vector3     Axis    = m_Particles[1].m_Pos - m_Particles[0].m_Pos;
    const xbool HasAxis = Axis.SafeNormalize();
    m_bOnGround         = FALSE;
    for( s32 Iteration = 0; Iteration < k_VelocityIterations; Iteration++ )
    {
        if( HasAxis )
        {
            const f32 Speed = ( m_Particles[1].m_Velocity - m_Particles[0].m_Velocity ).Dot( Axis );
            const vector3 Correction = Axis * ( Speed * 0.5f );
            m_Particles[0].m_Velocity += Correction;
            m_Particles[1].m_Velocity -= Correction;
        }

        for( s32 i = 0; i < m_VelocityConstraints.GetCount(); i++ )
        {
            velocity_constraint& Constraint = m_VelocityConstraints[i];
            particle& A        = *Constraint.pParticleA;
            particle* pB       = Constraint.pParticleB;
            vector3   Normal   = Constraint.Plane.Normal;
            vector3   Velocity = A.m_Velocity;
            if( pB )
            {
                const vector3 Delta    = A.m_Pos - pB->m_Pos;
                const f32     Distance = Delta.Length();
                if( Distance > Constraint.Distance + CAPSULE_CAST_EPSILON )
                {
                    continue;
                }
                if( Distance > CAPSULE_CAST_EPSILON )
                {
                    Normal = Delta / Distance;
                }
                if( Constraint.InvMassB > 0.0f )
                {
                    Velocity -= pB->m_Velocity;
                }
            }
            else if( Constraint.Plane.Distance( A.m_Pos ) > Constraint.Distance + CAPSULE_CAST_EPSILON )
            {
                continue;
            }

            if( Normal.GetY() > 0.5f )
            {
                m_bOnGround = TRUE;
            }
            const f32 Speed = Velocity.Dot( Normal );
            if( Speed < 0.0f )
            {
                const vector3 Impulse = Normal * ( -Speed / ( Constraint.InvMassA + Constraint.InvMassB ) );
                A.m_Velocity += Constraint.InvMassA * Impulse;
                if( pB && ( Constraint.InvMassB > 0.0f ) )
                {
                    pB->m_Velocity -= Constraint.InvMassB * Impulse;
                }
            }
        }
    }
    m_VelocityConstraints.Clear();
}

//==============================================================================

void coke_can::ApplyDamping( f32 DeltaTime )
{
    s32 i;

    // Lookup profile
    const coke_can_profile& Profile = GetProfile();

    // Clamp speed
    for ( i = 0; i < 2; i++ )
    {
        // Get particle
        particle& Particle = m_Particles[i];

        // Compute vel and speed
        vector3 Vel      = Particle.m_Velocity;
        f32     SpeedSqr = Vel.LengthSquared();
        
        // Clamp speed?
        if( SpeedSqr > x_sqr( Profile.m_MaxSpeedCmPerSecond ) )
        {
            // Scale down speed
            f32 Speed = x_sqrt( SpeedSqr );
            f32 Scale = Profile.m_MaxSpeedCmPerSecond / Speed;
            Vel *= Scale;

            // Set new vel
            Particle.m_Velocity = Vel;
        }
    }

    // Compute center of mass velocity
    vector3 CenterVel( 0,0,0 );
    for ( i = 0; i < 2; i++ )
        CenterVel += m_Particles[i].m_Velocity;
    CenterVel *= 0.5f;

    // Apply damping
    for ( i = 0; i < 2; i++ )
    {
        // Get particle
        particle& Particle = m_Particles[i];

        // Compute angular and linear velocity
        vector3 Vel        = Particle.m_Velocity;
        vector3 AngularVel = Vel - CenterVel;
        vector3 LinearVel  = Vel - AngularVel;
            
        // Dampen
        if( m_bOnGround )
        {
            LinearVel  *= x_exp( -Profile.m_GroundLinearDecayPerSecond  * DeltaTime );
            AngularVel *= x_exp( -Profile.m_GroundAngularDecayPerSecond * DeltaTime );
        }
        else
        {
            LinearVel  *= x_exp( -Profile.m_AirLinearDecayPerSecond  * DeltaTime );
            AngularVel *= x_exp( -Profile.m_AirAngularDecayPerSecond * DeltaTime );
        }
        
        // Compute new vel
        Vel = LinearVel + AngularVel;
        
        // Set new vel
        Particle.m_Velocity = Vel;
    }

    const f32 RollDecay = m_bOnGround ? Profile.m_GroundAngularDecayPerSecond
                                      : Profile.m_AirAngularDecayPerSecond;
    m_RollRate *= x_exp( -RollDecay * DeltaTime );
}

//==============================================================================

#ifdef X_EDITOR

s32 coke_can::OnValidateProperties( xstring& ErrorMsg )
{
    // Does coke can intersect world?
    f32     Depth;
    vector3 Normal;
    f32     CollisionBackOff = GetProfile().m_CollisionBackoffCm;
    if( CokeCanIntersectsWorld( m_Particles[0].m_Pos,   // ParticlePos0
                                m_Particles[1].m_Pos,   // ParticlePos1
                                m_ParticleRadius,       // ParticleRadius
                                CollisionBackOff,       // Collision back off dist
                                Depth,                  // Intersection depth
                                Normal ) )              // Intersection normal
    {    
        // Report error
        ErrorMsg += "COKE CAN IS INTERSECTING THE WORLD!\n\nThis will cause the coke can to fall half way though the floor etc.\n\nUse the \"FixWorldIntersection\" property button or re-position manually to resolve this issue.\n";
        return 1;
    }
    
    // No error
    return 0;
}
#endif // X_EDITOR

//==============================================================================
