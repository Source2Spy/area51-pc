//==============================================================================
//
//  Cloth.cpp
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================
#include "Cloth.hpp"
#include "Entropy.hpp"
#include "GameLib/StatsMgr.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "Render/RigidGeom.hpp"
#include "PainMgr/Pain.hpp"
#include "Objects/Actor/Actor.hpp"

//==============================================================================
// DEFINES
//==============================================================================

#define CLOTH_PRIM_KEY_TRI_FLAG     0x80000000      // Indicates flag tri was hit

//==============================================================================
// DATA
//==============================================================================
static f32       CLOTH_TIME_STEP                     = 1.0f / 60.0f;
static const f32 CLOTH_REFERENCE_TIME_STEP           = 1.0f / 30.0f;
static const f32 CLOTH_WIND_RESPONSE_TIME            = 0.25f;
static f32       CLOTH_PAIN_EXPLOSION_FORCE_SCALE    = 1000.0f;
static f32       CLOTH_PAIN_MELEE_FORCE_SCALE        = 500.0f;

#ifdef X_EDITOR
extern xbool g_game_running;
#endif

//==============================================================================
// CLASSES
//==============================================================================

cloth::cloth()
{
    // Physics components (xarrays only grow on the PC!)
    m_Particles.SetGrowAmount( 128 );
    m_Connections.SetGrowAmount( 256 );
    m_Triangles.SetGrowAmount( 256 );
    m_MinDist = F32_MAX;
    m_LocalCollBBox.Set(vector3(0,0,0), 100*4);
    m_WorldCollBBox = m_LocalCollBBox;
    m_GeomLocalBBox.Set(vector3(0,0,0), 100);

    // Physics properties
    m_Gravity.Set( 0.0f, -9.8f * 100.0f * 2.0f, 0.0f ); // Gravity to add to cloth
    m_Dampen = 0.05f;                                   // Dampen amount
    m_Stretch = 1.0f;                                   // Stretch amount
    m_nIterations = 1;                                  // # of constraint iterations
    m_ImpactScale = 0.001f;                             // Impact scale of bullets
    m_SimulationTime = 5.0f;                            // Time to simulate before level starts

    // Simulation state
    m_DeltaTime  = 0;
    m_RenderRevision = 1;
    m_ObjectGuid = 0;
    m_L2W.Identity();
    m_W2L.Identity();
    m_LocalBBox.Set(vector3(0,0,0), 0);
    m_WorldBBox.Set(vector3(0,0,0), 0);

    // Damage map
    m_DamageMapWidth  = 0;
    m_DamageMapHeight = 0;
    m_DamageStamp     = 0;
    ClearDamageDirty();

    // Wind vars
    m_bWindEnabled = TRUE;
    m_WindSampleTime = 0.0f;        // Time until the next turbulence sample
    m_WindDir.Set( 0, 0, 1 );       // Direction of wind
    m_WindRot.Set( 0, R_20, 0 );    // Rotation of wind
    m_WindMinStrength = -0.8f;      // Min strength of wind
    m_WindMaxStrength = 0.8f;       // Max strength of wind
}

//==============================================================================

cloth::~cloth()
{
}

//===========================================================================

void cloth::OnEnumPropGeometry( prop_enum& List )
{
    // Cloth
    List.PropEnumHeader ( "Cloth", "Properties of cloth", 0 );

    // Geometry
    // NOTE: This MUST be enumerated before the owning object's RenderInst
    //       properties so xarray capacities are setup before geometry loads
    List.PropEnumHeader ( "Cloth\\Geometry", "Render inst properties of cloth", PROP_TYPE_DONT_SHOW );
    List.PropEnumInt    ( "Cloth\\Geometry\\ParticleCount",   "# of particles in cloth",   PROP_TYPE_DONT_SHOW );
    List.PropEnumInt    ( "Cloth\\Geometry\\ConnectionCount", "# of connections in cloth", PROP_TYPE_DONT_SHOW );
    List.PropEnumInt    ( "Cloth\\Geometry\\TriangleCount",   "# of triangles in cloth",   PROP_TYPE_DONT_SHOW );
}

//=============================================================================

// NOTE: Must be enumerated after the owning object's RenderInst properties so
//       particle, connections, and triangles are already initialized
void cloth::OnEnumProp( prop_enum& List )
{
    // Particles
    List.PropEnumHeader( "Cloth\\Particles", "Particle properties of cloth", 0 );
#ifdef X_EDITOR

    // Compute bbox of particles
    bbox BBox;
    BBox.Clear();
    for ( s32 i = 0; i < m_Particles.GetCount(); i++ )
        BBox += m_Particles[i].m_BindPos;
    vector3 Size = BBox.GetSize();

    // Add buttons
    List.PropEnumButton( "Cloth\\Particles\\Reset",        "Sets all masses to 1", PROP_TYPE_MUST_ENUM );
    if( Size.GetX() > 10.0f )
    {
        List.PropEnumButton( "Cloth\\Particles\\PegLeftSide",  "Sets masses of left (-ve X) side to 0", PROP_TYPE_MUST_ENUM );
        List.PropEnumButton( "Cloth\\Particles\\PegRightSide", "Sets masses of right (+ve X) side to 0", PROP_TYPE_MUST_ENUM );
    }
    if( Size.GetY() > 10.0f )
    {
        List.PropEnumButton( "Cloth\\Particles\\PegTopSide",   "Sets masses of top (+ve Y) side to 0", PROP_TYPE_MUST_ENUM );
        List.PropEnumButton( "Cloth\\Particles\\PegBotSide",   "Sets masses of bottom (-ve Y) side to 0", PROP_TYPE_MUST_ENUM );
    }
    if( Size.GetZ() > 10.0f )
    {
        List.PropEnumButton( "Cloth\\Particles\\PegFrontSide", "Sets masses of front (-ve Z) side to 0", PROP_TYPE_MUST_ENUM );
        List.PropEnumButton( "Cloth\\Particles\\PegBackSide",  "Sets masses of back (+ve Z) side to 0", PROP_TYPE_MUST_ENUM );
    }
#endif
    for ( s32 i = 0; i < m_Particles.GetCount(); i++ )
    {
        List.PropEnumVector3( xfs( "Cloth\\Particles\\Position[%d]", i ), "Position",                  PROP_TYPE_DONT_SHOW );
        List.PropEnumFloat  ( xfs( "Cloth\\Particles\\Mass[%d]",     i ), "Mass (set to zero to pin)", PROP_TYPE_MUST_ENUM );
    }

    // Wind vars
    List.PropEnumHeader  ( "Cloth\\Wind", "Wind properties of cloth", 0 );
    List.PropEnumBool    ( "Cloth\\Wind\\Enabled",     "Enable continuous wind", 0 );
    List.PropEnumVector3 ( "Cloth\\Wind\\Dir",         "Direction in local space (relative to object)", 0 );
    List.PropEnumRotation( "Cloth\\Wind\\Rot",         "Local space rotation (per second) of wind direction", 0 );
    List.PropEnumFloat   ( "Cloth\\Wind\\MinStrength", "Minimum strength of wind", 0 );
    List.PropEnumFloat   ( "Cloth\\Wind\\MaxStrength", "Maximum strength of wind", 0 );

    // Physics properties
    List.PropEnumHeader ( "Cloth\\Physics", "Physical properties of cloth", 0 );
    List.PropEnumVector3( "Cloth\\Physics\\Gravity",       "Gravity to apply to cloth", 0 );
    List.PropEnumFloat  ( "Cloth\\Physics\\Dampen",        "Dampen factor of constraints", 0 );
    List.PropEnumFloat  ( "Cloth\\Physics\\Stretch",       "Stretch factor of cloth", 0 );
    //SB - I don't feel comfortable exposing these to artists!
    //List.PropEnumInt    ( "Cloth\\Physics\\Iterations",    "# of constraint iterations to perform", 0 );
    //List.PropEnumFloat  ( "Cloth\\Physics\\ImpactScale",   "Impact scale of bullet force", 0 );
    List.PropEnumBBox   ( "Cloth\\Physics\\CollisionBBox",  "Local collision bounding box to keep cloth inside of", 0 ) ;
    List.PropEnumFloat  ( "Cloth\\Physics\\SimulationTime", "Physics time to simulate before level starts.", PROP_TYPE_MUST_ENUM);
}

//=============================================================================

xbool cloth::OnProperty( prop_query& I )
{
    // Quick exit?
    if( I.IsSimilarPath( "Cloth" ) == FALSE )
        return FALSE;

    // Geometry? (used to pre-allocate xarrays)
    if( I.IsSimilarPath( "Cloth\\Geometry" ) )
    {
        if( I.IsVar( "Cloth\\Geometry\\ParticleCount" ) )
        {
            if( I.IsRead() )
                I.SetVarInt( m_Particles.GetCount() );
            else
                m_Particles.SetCapacity( I.GetVarInt() );
            return TRUE;
        }
        if( I.IsVar( "Cloth\\Geometry\\ConnectionCount" ) )
        {
            if( I.IsRead() )
                I.SetVarInt( m_Connections.GetCount() );
            else
                m_Connections.SetCapacity( I.GetVarInt() );
            return TRUE;
        }
        if( I.IsVar( "Cloth\\Geometry\\TriangleCount" ) )
        {
            if( I.IsRead() )
                I.SetVarInt( m_Triangles.GetCount() );
            else
                m_Triangles.SetCapacity( I.GetVarInt() );
            return TRUE;
        }
    }

    // Wind properties?
    if( I.IsSimilarPath( "Cloth\\Wind" ) )
    {
        if( I.VarBool( "Cloth\\Wind\\Enabled", m_bWindEnabled ) )
        {
            return TRUE;
        }

        if( I.VarVector3( "Cloth\\Wind\\Dir", m_WindDir ) )
            return TRUE;
        if( I.VarRotation( "Cloth\\Wind\\Rot", m_WindRot ) )
            return TRUE;
        if( I.VarFloat( "Cloth\\Wind\\MinStrength", m_WindMinStrength ) )
            return TRUE;
        if( I.VarFloat( "Cloth\\Wind\\MaxStrength", m_WindMaxStrength ) )
            return TRUE;
    }

    // Particle properties?
    if( I.IsSimilarPath( "Cloth\\Particles" ) )
    {
    #ifdef X_EDITOR

        // Reset mass?
        if( I.IsVar( "Cloth\\Particles\\Reset" ) )
        {
            // Setup UI?
            if( I.IsRead() )
            {
                I.SetVarButton( "Reset" );
            }
            else
            {
                // Clear all masses
                ClearAllInvMasses();
            }

            return TRUE;
        }

        // Peg left side?
        if( I.IsVar( "Cloth\\Particles\\PegLeftSide" ) )
        {
            if( I.IsRead() )
                I.SetVarButton( "Peg Left Side" );
            else
                PegParticles( 0, -1 );   // X, -ve dir
            return TRUE;
        }

        // Peg right side?
        if( I.IsVar( "Cloth\\Particles\\PegRightSide" ) )
        {
            if( I.IsRead() )
                I.SetVarButton( "Peg Right Side" );
            else
                PegParticles( 0, +1 );   // X, +ve dir
            return TRUE;
        }

        // Peg top side?
        if( I.IsVar( "Cloth\\Particles\\PegTopSide" ) )
        {
            if( I.IsRead() )
                I.SetVarButton( "Peg Top Side" );
            else
                PegParticles( 1, 1 );   // Y, +ve dir
            return TRUE;
        }

        // Peg bottom side?
        if( I.IsVar( "Cloth\\Particles\\PegBotSide" ) )
        {
            if( I.IsRead() )
                I.SetVarButton( "Peg Bottom Side" );
            else
                PegParticles( 1, -1 );   // Y, -ve dir
            return TRUE;
        }

        // Peg front side?
        if( I.IsVar( "Cloth\\Particles\\PegFrontSide" ) )
        {
            if( I.IsRead() )
                I.SetVarButton( "Peg Front Side" );
            else
                PegParticles( 2, -1 );   // Z, -ve dir
            return TRUE;
        }

        // Peg right side?
        if( I.IsVar( "Cloth\\Particles\\PegBackSide" ) )
        {
            if( I.IsRead() )
                I.SetVarButton( "Peg Back Side" );
            else
                PegParticles( 2, +1 );   // Z, +ve dir
            return TRUE;
        }
    #endif  //#ifdef X_EDITOR

        // NOTE: This particle index is static so that when loading the mass,
        // subsequent Position[]/Mass[] property pairs for the same particle
        // resolve without doing a linear position search each time.
        static s32 iParticle = -1;

        // Particle position?
        if( I.IsVar( "Cloth\\Particles\\Position[]" ) )
        {
            // Setup UI?
            if( I.IsRead() )
            {
                // Read position from particle
                iParticle = I.GetIndex(0);
                ASSERT( iParticle >= 0 );
                ASSERT( iParticle < m_Particles.GetCount() );
                I.SetVarVector3( m_Particles[iParticle].m_BindPos );
            }
            else
            {
                // Prefer the serialized index so particles split at UV seams can share a position.
                s32 const PropertyIndex = I.GetIndex( 0 );
                if( ( PropertyIndex >= 0 ) && ( PropertyIndex < m_Particles.GetCount() ) &&
                    ( ( m_Particles[PropertyIndex].m_BindPos - I.GetVarVector3() ).LengthSquared() < x_sqr( 0.01f ) ) )
                {
                    iParticle = PropertyIndex;
                }
                else
                {
                    iParticle = FindParticle( I.GetVarVector3() );
                }
            }

            return TRUE;
        }

        // Particle mass?
        if( I.IsVar( "Cloth\\Particles\\Mass[]" ) )
        {
            // Setup UI?
            if( I.IsRead() )
            {
                // Read mass from particle
                iParticle = I.GetIndex(0);
                ASSERT( iParticle >= 0 );
                ASSERT( iParticle < m_Particles.GetCount() );
                I.SetVarFloat( m_Particles[iParticle].GetMass() );
            }
            else
            {
    #ifdef X_EDITOR
                // Use UI index when editing in the editor
                iParticle = I.GetIndex(0);
    #endif
                // Set mass particle
                if( ( iParticle >= 0 ) && ( iParticle < m_Particles.GetCount() ) )
                    m_Particles[ iParticle ].SetMass( I.GetVarFloat() );
            }
            return TRUE;
        }
    }


    // Physics properties?
    if( I.IsSimilarPath( "Cloth\\Physics" ) )
    {
        if( I.VarVector3( "Cloth\\Physics\\Gravity", m_Gravity ) )
            return TRUE;
        if( I.VarFloat( "Cloth\\Physics\\Dampen", m_Dampen, 0.0f, 2.0f ) )
            return TRUE;
        if( I.VarFloat( "Cloth\\Physics\\Stretch", m_Stretch, 0.0f, 1.0f ) )
            return TRUE;
        //if( I.VarInt( "Cloth\\Physics\\Iterations", m_nIterations, 1, 10 ) )
        //return TRUE;
        //if( I.VarFloat( "Cloth\\Physics\\ImpactScale", m_ImpactScale, 0.0f, 100.0f ) )
        //return TRUE;
        if( I.IsVar( "Cloth\\Physics\\CollisionBBox" ) )
        {
            if( I.IsRead() )
            {
                I.SetVarBBox( m_LocalCollBBox ) ;
            }
            else
            {
                SetLocalCollBBox( I.GetVarBBox() ) ;
            }

            return TRUE ;
        }

        if( I.VarFloat( "Cloth\\Physics\\SimulationTime", m_SimulationTime, 0.0f, 10.0f ) )
        {
#ifdef X_EDITOR
            // If in the editor and not running the game, leave in bind pose
            if( !g_game_running )
                return TRUE;
#endif
            // Simulate physics for set time so flags don't visibly fall down when level starts
            f32 Step = 1.0f / 30.0f;
            for( f32 Time = 0; Time < m_SimulationTime; Time += Step )
                Advance( Step );

            return TRUE;
        }
    }

    return FALSE;
}


//==============================================================================
// Private initialization functions
//==============================================================================

// Returns index of particle if found
s32 cloth::FindParticle( const vector3& P )
{
    // See if the particle already exists
    for (s32 i = 0; i < m_Particles.GetCount(); i++)
    {
        // Compute distance between particle and position
        vector3 Delta   = m_Particles[i].m_BindPos - P;
        f32     DistSqr = Delta.LengthSquared();

        // If below threshold distance, particle has been found
        if( DistSqr < x_sqr( 0.01f ) )
            return i;
    }

    // Not found
    return -1;
}

//==============================================================================

s32 cloth::FindParticle( const vector3& P, const vector2& UV )
{
    for( s32 i = 0; i < m_Particles.GetCount(); i++ )
    {
        vector3 const PositionDelta = m_Particles[i].m_BindPos - P;
        vector2 const UVDelta = m_Particles[i].m_UV - UV;
        if( ( PositionDelta.LengthSquared() < x_sqr( 0.01f ) ) &&
            ( UVDelta.LengthSquared() < x_sqr( 0.0001f ) ) )
        {
            return i;
        }
    }

    return -1;
}

//==============================================================================

// Adds particle and returns index
s32 cloth::AddParticle( const vector3& P, const vector2& UV )
{
    // Already exist?
    s32 Index = FindParticle( P, UV );
    if( Index != -1 )
    {
        return Index;
    }

    if( m_Particles.GetCount() >= 65536 )
    {
        x_throw( "Cloth surface exceeds the u16 vertex index limit" );
        return -1;
    }

    // Create new particle
    Index = m_Particles.GetCount();
    cloth_particle& Particle = m_Particles.Append();

    // Setup particle
    Particle.m_BindPos = P;
    Particle.m_Pos     = Particle.m_LastPos = m_L2W * P;
    Particle.m_InvMass = 1.0f;
    Particle.m_UV      = UV;

    return Index;
}

//==============================================================================

// Returns index of connection, or -1 if not found
s32 cloth::FindConnection( s32 ParticleA, s32 ParticleB )
{
    // Does connection already exist?
    for (s32 i = 0; i < m_Connections.GetCount(); i++)
    {
        cloth_connection& Connection = m_Connections[i];

        // Found?
        if (        (Connection.m_ParticleA == ParticleA)
                &&  (Connection.m_ParticleB == ParticleB) )
            return i;

        // Found?
        if (        (Connection.m_ParticleB == ParticleA)
                &&  (Connection.m_ParticleA == ParticleB) )
            return i;
    }

    // Not found
    return -1;
}

//==============================================================================

// Adds connection between 2 particles and returns index
s32 cloth::AddConnection( s32 ParticleA, s32 ParticleB )
{
    // Does connection already exist?
    s32 Index = FindConnection(ParticleA, ParticleB);
    if (Index != -1)
        return Index;

    // Create new connection
    Index = m_Connections.GetCount();
    cloth_connection& Connection = m_Connections.Append();

    // Setup connection
    Connection.m_ParticleA   = ParticleA;
    Connection.m_ParticleB   = ParticleB;
    Connection.m_RestDistSqr = (m_Particles[ParticleA].m_BindPos - m_Particles[ParticleB].m_BindPos).LengthSquared();

    // Update the min distance squared (it's used for self intersection)
    if( Connection.m_RestDistSqr > 0.001f )
        m_MinDist = x_min( m_MinDist, x_sqrt( Connection.m_RestDistSqr ) );

    return Index;
}

//==============================================================================

s32 cloth::FindTriangleVert( s32 Triangle, s32 Vertex )
{
    // Check all verts
    for (s32 i = 0; i < 3; i++)
    {
        // Found?
        if (m_Triangles[Triangle].m_Particles[i] == Vertex)
            return i;
    }

    // Not found
    return -1;
}

//==============================================================================

// Returns index of triangle or -1 if not found
s32 cloth::FindTriangle( s32 Verts[3] )
{
    // Check all triangles
    for (s32 i = 0; i < m_Triangles.GetCount(); i++)
    {
        // Found all three verts?
        if (     (FindTriangleVert(i, Verts[0]) != -1)
              && (FindTriangleVert(i, Verts[1]) != -1)
              && (FindTriangleVert(i, Verts[2]) != -1) )
        {
            return i;
        }
    }

    // Not found
    return -1;
}

//==============================================================================

// Adds triangle particles and connections
void cloth::AddTriangle( const vector3& P0, const vector2& UV0,
                         const vector3& P1, const vector2& UV1,
                         const vector3& P2, const vector2& UV2 )
{
    s32 i;

    // Add particles
    s32 Verts[3];
    Verts[0] = AddParticle(P0, UV0);
    Verts[1] = AddParticle(P1, UV1);
    Verts[2] = AddParticle(P2, UV2);

    // Ran out of particles?
    if ( ( Verts[0] == -1 ) || ( Verts[1] == -1 ) || ( Verts[2] == -1 ) )
        return ;

    // Create new triangle?
    if (FindTriangle(Verts) == -1)
    {
        cloth_triangle& Triangle = m_Triangles.Append();
        Triangle.m_Particles[0] = Verts[0];
        Triangle.m_Particles[1] = Verts[1];
        Triangle.m_Particles[2] = Verts[2];

        // Find the diagonal edge of the triangle
        f32 DiagDist = 0;
        s32 DiagEdge = 0;
        for (i = 0; i < 3; i++)
        {
            // Longest edge so far?
            f32 Dist = (m_Particles[Verts[i]].m_BindPos - m_Particles[Verts[(i+1)%3]].m_BindPos).LengthSquared();
            if (Dist > DiagDist)
            {
                // Record
                DiagDist = Dist;
                DiagEdge = i;
            }
        }

        // Loop through all edges and create connections
        for (i = 0; i < 3; i++)
        {
            // Skip the diagonal edge - makes the cloth much more flexible
            if (i != DiagEdge)
                AddConnection(Verts[i], Verts[(i+1)%3]);
        }
    }
}

//==============================================================================
// Internal connection sort data/functions
//==============================================================================

static cloth_particle* s_pClothParticles = NULL;
static s32             s_nClothParticles = 0;

static
s32 ConnectionSortFn( const void* pItem0, const void* pItem1 )
{
    // Lookup connections
    cloth_connection* pCon0 = (cloth_connection*)pItem0;
    cloth_connection* pCon1 = (cloth_connection*)pItem1;

    // Make sure connections are valid
    ASSERT( s_nClothParticles );
    ASSERT( s_pClothParticles );
    ASSERT( pCon0 );
    ASSERT( pCon1 );
    ASSERT( pCon0->m_ParticleA >= 0 );
    ASSERT( pCon0->m_ParticleA < s_nClothParticles );
    ASSERT( pCon0->m_ParticleB >= 0 );
    ASSERT( pCon0->m_ParticleB < s_nClothParticles );
    ASSERT( pCon1->m_ParticleA >= 0 );
    ASSERT( pCon1->m_ParticleA < s_nClothParticles );
    ASSERT( pCon1->m_ParticleB >= 0 );
    ASSERT( pCon1->m_ParticleB < s_nClothParticles );

    // Lookup particles
    cloth_particle& Con0PartA = s_pClothParticles[ pCon0->m_ParticleA ];
    cloth_particle& Con0PartB = s_pClothParticles[ pCon0->m_ParticleB ];
    cloth_particle& Con1PartA = s_pClothParticles[ pCon1->m_ParticleA ];
    cloth_particle& Con1PartB = s_pClothParticles[ pCon1->m_ParticleB ];

    // Compute mid position of connections
    f32 Con0MidY = ( Con0PartA.m_BindPos.GetY() + Con0PartB.m_BindPos.GetY() ) * 0.5f;
    f32 Con1MidY = ( Con1PartA.m_BindPos.GetY() + Con1PartB.m_BindPos.GetY() ) * 0.5f;

    // Sort by Y component from top(bigger) -> lower(smaller)
    if( Con0MidY < Con1MidY )
        return 1;
    else if( Con0MidY > Con1MidY )
        return -1;
    else
        return 0;
}

//==============================================================================
// Public initialization functions
//==============================================================================

cloth_init_result cloth::Init( rigid_geom* pGeom )
{
    s32 i,j;

    cloth_init_result Result;
    Result.RenderMask    = (u64)-1;
    Result.MaterialIndex = -1;

    // Lookup geometry
    if (!pGeom)
        return Result;

    // Cache the geometry's local bbox so ComputeBounds() doesn't need the render instance
    m_GeomLocalBBox = pGeom->m_BBox;

    // Setup default collision bbox
    vector3 C = pGeom->m_BBox.GetCenter();
    f32     R = pGeom->m_BBox.GetRadius();
    m_LocalCollBBox.Set( C, R * 1.25f );

    // Clear lists
    m_Particles.SetCount( 0 );
    m_Connections.SetCount( 0 );
    m_Triangles.SetCount( 0 );
    m_MinDist = F32_MAX;
    InitDamageMap( 0, 0 );

    s32 ClothSurfaceCount = 0;

    // Loop through all submeshes
    for (i = 0; i < pGeom->m_nMeshes; i++)
    {
        // Skip if not "cloth" mesh
        if ( x_stristr(pGeom->GetMeshName( i ), "cloth") == NULL )
            continue;

        ClothSurfaceCount++;

        // Do not draw this part of the rigid instance
        Result.RenderMask &= ~( u64{ 1 } << i );

        if( pGeom->m_pMesh[i].nSubMeshes != 1 )
            x_throw( "Cloth meshes should only use one texture" );

        // Lookup submesh
        geom::submesh&  SubMesh = pGeom->m_pSubMesh[pGeom->m_pMesh[i].iSubMesh];

        // Lookup material
        geom::material& Material = pGeom->m_pMaterial[SubMesh.iMaterial];

        // Lookup diffuse texture
        const char* pTexName = pGeom->GetTextureName( Material.iTexture );

        // Skip if not "flag" or "cloth" texture
        if (        (x_stristr(pTexName, "flag") == NULL)
                &&  (x_stristr(pTexName, "cloth") == NULL) )
            continue;

        // Save out the material index so we can do texture lookups later on.
        Result.MaterialIndex = SubMesh.iMaterial;

        ASSERT( SubMesh.nSections == 1 );
        const rigid_geom::section& Section = pGeom->m_pSection[SubMesh.iSection];

        // Loop through all tri indices
        ASSERT((Section.nIndices % 3) == 0);
        for (j = 0; j < Section.nIndices; j += 3)
        {
            // Lookup tri indices
            u32 V0 = pGeom->m_pIndex[Section.FirstIndex + j+0];
            u32 V1 = pGeom->m_pIndex[Section.FirstIndex + j+1];
            u32 V2 = pGeom->m_pIndex[Section.FirstIndex + j+2];

            // Add tri
            AddTriangle(pGeom->m_pVertex[V0].Pos, pGeom->m_pVertex[V0].UV,
                pGeom->m_pVertex[V1].Pos, pGeom->m_pVertex[V1].UV,
                pGeom->m_pVertex[V2].Pos, pGeom->m_pVertex[V2].UV);
        }



    }

    if( ClothSurfaceCount != 1 )
    {
        x_throw( "Cloth geometry must contain exactly one cloth surface" );
    }

    if( m_Triangles.GetCount() == 0 )
    {
        x_throw( "Cloth surface does not contain any triangles" );
    }

    if( m_Particles.GetCount() > 65535 )
    {
        x_throw( "Cloth surface exceeds the u16 vertex index limit" );
    }

    // Sort connections from top -> bottom so cloth isn't as stretchy
    if( m_Connections.GetCount() )
    {
        // Sort
        s_pClothParticles = &m_Particles[0];
        s_nClothParticles = m_Particles.GetCount();
        x_qsort( &m_Connections[0], m_Connections.GetCount(), sizeof(cloth_connection), ConnectionSortFn );
        s_pClothParticles = NULL;
        s_nClothParticles = 0;
    }

    // Default to pegging top particles
    PegParticles( 1, 1 );   // Y, +ve dir

    // Reset motion.
    Reset();

    return Result;
}

//===========================================================================

void cloth::PegParticles( s32 iAxis, s32 Dir )
{
    s32 i;

    ASSERT( iAxis >= 0 );
    ASSERT( iAxis <= 2 );
    ASSERT( Dir != 0 );

    // Compute bbox of particles
    bbox BBox;
    BBox.Clear();
    for ( i = 0; i < m_Particles.GetCount(); i++)
        BBox += m_Particles[i].m_BindPos;

    // Peg particles
    for( i = 0; i < m_Particles.GetCount(); i++ )
    {
        // Lookup particle
        cloth_particle& Particle = m_Particles[i];

        // Peg?
        xbool bPeg = FALSE;
        if( Dir > 0 )
            bPeg = ( Particle.m_BindPos[iAxis] >= ( BBox.Max[iAxis] - 10.0f ) );
        else
            bPeg = ( Particle.m_BindPos[iAxis] <= ( BBox.Min[iAxis] + 10.0f ) );

        // Set inverse mass to zero on particles that should be pegged
        if( bPeg )
            Particle.m_InvMass = 0.0f;
    }
}

//===========================================================================

void cloth::ClearAllInvMasses( void )
{
    // Everyone moves.
    for ( s32 i = 0; i < m_Particles.GetCount(); i++)
    {
        m_Particles[i].m_InvMass = 1.0f;
    }
}

//==============================================================================

void cloth::Reset( void )
{
    MarkRenderDataDirty();
    m_DeltaTime = 0.0f;
    m_WindSampleTime = 0.0f;

    // Resets all particles to bind position
    for( s32 i = 0; i < m_Particles.GetCount(); ++i )
    {
        cloth_particle& Particle = m_Particles[i];
        Particle.m_Pos = Particle.m_LastPos = m_L2W * Particle.m_BindPos;
        Particle.m_WindStrength = 0.0f;
        Particle.m_WindTarget   = 0.0f;
    }

    // Update bounds
    ComputeBounds();
    ComputeWorldCollBBox();

    // Apply constraints incase collision bbox is smaller than geometry
    ApplyConstraints();

}

//==============================================================================

void cloth::Kill( void )
{
    InitDamageMap( 0, 0 );
}

//==============================================================================

void cloth::InitDamageMap( s32 Width, s32 Height )
{
    m_DamageMap.SetCount( 0 );
    m_DamageMapWidth  = MINMAX( 0, Width,  512 );
    m_DamageMapHeight = MINMAX( 0, Height, 512 );
    m_DamageStamp     = 0;
    ClearDamageDirty();

    if( ( m_DamageMapWidth == 0 ) || ( m_DamageMapHeight == 0 ) )
    {
        return;
    }

    m_DamageMap.SetCount( m_DamageMapWidth * m_DamageMapHeight );
    x_memset( m_DamageMap.GetPtr(), 255, m_DamageMap.GetCount() );
    MarkDamageDirty( 0, 0, m_DamageMapWidth - 1, m_DamageMapHeight - 1 );
}

//==============================================================================

void cloth::ClearDamageDirty( void )
{
    m_DamageDirtyMinX = S32_MAX;
    m_DamageDirtyMinY = S32_MAX;
    m_DamageDirtyMaxX = -1;
    m_DamageDirtyMaxY = -1;
}

//==============================================================================

xbool cloth::GetDamageDirty( s32& MinX, s32& MinY, s32& MaxX, s32& MaxY ) const
{
    if( ( m_DamageDirtyMinX > m_DamageDirtyMaxX ) || ( m_DamageDirtyMinY > m_DamageDirtyMaxY ) )
    {
        return FALSE;
    }

    MinX = m_DamageDirtyMinX;
    MinY = m_DamageDirtyMinY;
    MaxX = m_DamageDirtyMaxX;
    MaxY = m_DamageDirtyMaxY;
    return TRUE;
}

//==============================================================================

void cloth::MarkDamageDirty( s32 MinX, s32 MinY, s32 MaxX, s32 MaxY )
{
    m_DamageDirtyMinX = MIN( m_DamageDirtyMinX, MinX );
    m_DamageDirtyMinY = MIN( m_DamageDirtyMinY, MinY );
    m_DamageDirtyMaxX = MAX( m_DamageDirtyMaxX, MaxX );
    m_DamageDirtyMaxY = MAX( m_DamageDirtyMaxY, MaxY );
}


//==============================================================================
// Physics functions
//==============================================================================

void cloth::ApplyCappedCylinderColl ( const vector3& Bottom, const vector3& Top, f32 Radius )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::ApplyCappedCylinderColl");

    f32 RadiusSqr = Radius * Radius;

    // Loop through all particles
    for (s32 i = 0; i < m_Particles.GetCount(); i++)
    {
        // Lookup particle
        cloth_particle& Particle = m_Particles[i];

        // Skip if it cannot be moved
        if (Particle.m_InvMass == 0)
            continue;

        // Get vector and distance to line down middle of cylinder
        vector3 Delta   = Particle.m_Pos.GetClosestVToLSeg(Bottom, Top);
        f32     DistSqr = Delta.LengthSquared();

        // Project out of capped cylinder?
        if ((DistSqr > 0.00001f) && (DistSqr < RadiusSqr))
        {
            // Keep away from cylinder center line
            f32 Dist = x_sqrt(DistSqr);
            f32 Diff = (Dist - Radius) / Dist;

            // Scale and dampen
            Delta *= Diff;

            // Project particle out of cylinder
            Particle.m_Pos += Delta;
        }
    }
}

//==============================================================================

void cloth::OnColCheck( guid ObjectHitGuid, u32 Flags )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::OnColCheck" );

    g_CollisionMgr.StartApply( ObjectHitGuid );

    // Loop through all triangles
    for( s32 i = 0; i < m_Triangles.GetCount(); i++ )
    {
        // Lookup triangle
        cloth_triangle& Triangle = m_Triangles[i];

        // Lookup particle positions
        const vector3& P0 = m_Particles[(s32)Triangle.m_Particles[0]].m_Pos;
        const vector3& P1 = m_Particles[(s32)Triangle.m_Particles[1]].m_Pos;
        const vector3& P2 = m_Particles[(s32)Triangle.m_Particles[2]].m_Pos;

        // Apply both sides to collision manager - pass in triangle number | CLOTH_PRIM_KEY_TRI_FLAG
        // as the primitive key so that if a projectile hits the flag, we can identify the exact UV hit
        g_CollisionMgr.ApplyTriangle( P0, P1, P2, Flags, i | CLOTH_PRIM_KEY_TRI_FLAG );
        g_CollisionMgr.ApplyTriangle( P2, P1, P0, Flags, i | CLOTH_PRIM_KEY_TRI_FLAG );
    }

    g_CollisionMgr.EndApply();
}

//==============================================================================

void cloth::ApplyDistConstraints( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::ApplyDistConstraints");

    // Authored stiffness is the correction per iteration at the reference rate.
    f32 const StepScale = CLOTH_TIME_STEP / CLOTH_REFERENCE_TIME_STEP;
    f32 const Stretch = 1.0f - x_pow( 1.0f - x_clamp( m_Stretch, 0.0f, 1.0f ), StepScale );

    // Loop through all connections
    for (s32 i = 0; i < m_Connections.GetCount(); i++)
    {
        // Lookup connection
        cloth_connection& Connection = m_Connections[i];

        // Lookup particles and distance
        cloth_particle& ParticleA   = m_Particles[(s32)Connection.m_ParticleA];
        cloth_particle& ParticleB   = m_Particles[(s32)Connection.m_ParticleB];
        f32             RestDistSqr = Connection.m_RestDistSqr;

        // Lookup mass info and skip connection if both particles are rigid
        f32 InvMass0 = ParticleA.m_InvMass;
        f32 InvMass1 = ParticleB.m_InvMass;
        f32 TotalInvMass = InvMass0 + InvMass1;
        if (TotalInvMass < 0.000001f)
            continue;

        // Get distance between particles
        vector3 Delta   = ParticleB.m_Pos - ParticleA.m_Pos;
        f32     DistSqr = Delta.LengthSquared();

        // Move to target dist
        //f32 Dist = x_sqrt(DistSqr);
        //f32 Diff = (Dist - RestDist) / (Dist * TotalInvMass);

        // Using sqrt approx
        f32 Diff = -2.0f * ((RestDistSqr / (DistSqr + RestDistSqr)) - 0.5f) / TotalInvMass;

        // Scale and dampen
        Delta *= Diff * Stretch;

        // Apply deltas
        ParticleA.m_Pos += InvMass0 * Delta;
        ParticleB.m_Pos -= InvMass1 * Delta;
    }
}

//==============================================================================

void cloth::ApplyCollConstraints( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::ApplyCollConstraints");

    s32 i, j;

    // Loop through all particles and keep them inside the collision bbox
    for (i = 0; i < m_Particles.GetCount(); i++)
    {
        // Lookup particle
        cloth_particle& Particle = m_Particles[i];

        // Skip if fixed
        if (Particle.m_InvMass == 0)
            continue;

        // Keep particle inside world collision bbox by projection
        Particle.m_Pos.Max( m_WorldCollBBox.Min );
        Particle.m_Pos.Min( m_WorldCollBBox.Max );
    }

    // Keep particles a set distance away from each other
    f32 RestDistSqr = x_sqr( m_MinDist );
    for (i = 0; i < m_Particles.GetCount(); i++)
    {
        cloth_particle& ParticleA = m_Particles[i];

        for (j = i+1; j < m_Particles.GetCount(); j++)
        {
            cloth_particle& ParticleB = m_Particles[j];

            // Lookup mass info and skip connection if both particles are rigid
            f32 InvMass0 = ParticleA.m_InvMass;
            f32 InvMass1 = ParticleB.m_InvMass;
            f32 TotalInvMass = InvMass0 + InvMass1;
            if (TotalInvMass < 0.000001f)
                continue;

            // Get distance between particles
            vector3 Delta   = ParticleB.m_Pos - ParticleA.m_Pos;
            f32     DistSqr = Delta.LengthSquared();
            if (DistSqr < RestDistSqr)
            {
                // Move to target dist
                //f32 Dist = x_sqrt(DistSqr);
                //f32 Diff = (Dist - RestDist) / (Dist * TotalInvMass);

                // Using sqrt approx
                f32 Diff = -2.0f * ((RestDistSqr / (DistSqr + RestDistSqr)) - 0.5f) / TotalInvMass;

                // Scale and dampen
                Delta *= Diff;//* m_Damp;

                // Apply deltas
                ParticleA.m_Pos += InvMass0 * Delta;
                ParticleB.m_Pos -= InvMass1 * Delta;
            }
        }
    }
}

//==============================================================================

void cloth::ApplyConstraints( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::ApplyConstraints");

    s32 i;

    // Apply collision constraints
    ApplyCollConstraints();

    // Apply distance constraints - keeps the cloth held together
    for (i = 0; i < m_nIterations; i++)
        ApplyDistConstraints();
}

//==============================================================================

void cloth::Integrate( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::Integrate");

    // Nothing to do?
    if (m_DeltaTime == 0)
        return;

    // Setup constants
    f32     DeltaTimeSquared = DeltaTime * DeltaTime;
    vector3 Accel            = m_Gravity * DeltaTimeSquared;

    // Apply verlet integration to all particles
    // Preserve the loss of velocity over time when the solver rate changes.
    f32 const Dampen = x_pow( x_clamp( 1.0f - m_Dampen, 0.0f, 1.0f ),
                              DeltaTime / CLOTH_REFERENCE_TIME_STEP );
    for (s32 i = 0; i < m_Particles.GetCount(); i++)
    {
        // Lookup particle
        cloth_particle& Particle = m_Particles[i];

        // Compute movement
        vector3 Pos   = Particle.m_Pos;
        if (Particle.m_InvMass != 0)
        {
            // Compute velocity
            vector3 Vel = Dampen * ( Particle.m_Pos - Particle.m_LastPos );

            // Move!
            Particle.m_Pos += Vel + Accel;
        }

        // Update last position
        Particle.m_LastPos = Pos;
    }
}

//==============================================================================

void cloth::ApplyWind( void )
{
    const xbool SampleWind = m_WindSampleTime <= 0.0f;
    if( SampleWind )
    {
        m_WindSampleTime += CLOTH_REFERENCE_TIME_STEP;
    }
    m_WindSampleTime -= CLOTH_TIME_STEP;

    const f32     StepScale    = CLOTH_TIME_STEP / CLOTH_REFERENCE_TIME_STEP;
    const f32     Blend        = 1.0f - x_exp( -CLOTH_TIME_STEP / CLOTH_WIND_RESPONSE_TIME );
    const vector3 WorldWindDir = m_L2W.RotateVector( m_WindDir );

    for( s32 i = 0; i < m_Particles.GetCount(); ++i )
    {
        cloth_particle& Particle = m_Particles[i];
        if( Particle.m_InvMass == 0.0f )
        {
            continue;
        }

        if( !m_bWindEnabled )
        {
            Particle.m_WindTarget = 0.0f;
        }
        else if( SampleWind )
        {
            Particle.m_WindTarget = x_frand( m_WindMinStrength, m_WindMaxStrength );
        }

        Particle.m_WindStrength += ( Particle.m_WindTarget - Particle.m_WindStrength ) * Blend;
        Particle.m_LastPos -= WorldWindDir * ( Particle.m_WindStrength * x_sqr( StepScale ) );
    }
}

//==============================================================================

void cloth::ComputeBounds( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::ComputeBounds");

    // Compute world bbox from the cached geometry bbox
    m_WorldBBox = m_GeomLocalBBox;
    m_WorldBBox.Transform(m_L2W);
    for (s32 i = 0; i < m_Particles.GetCount(); i++)
        m_WorldBBox += m_Particles[i].m_Pos;

    // Always have thickness
    m_WorldBBox.Inflate(10,10,10);

    // Compute local bbox
    m_LocalBBox = m_WorldBBox;
    m_LocalBBox.Transform(m_W2L);
}

//==============================================================================

// Computes world space collision bbox
void cloth::ComputeWorldCollBBox( void )
{
    m_WorldCollBBox = m_LocalCollBBox;
    m_WorldCollBBox.Transform(m_L2W);
}

//==============================================================================

// Applies blast force to particles
void cloth::ApplyBlast( const vector3& Pos, f32 Radius, f32 Amount )
{
    MarkRenderDataDirty();
    // Compute radius squared just once ready for loop
    f32 RadiusSqr = Radius * Radius;

    // Bake in frame rate multiplier
    Amount *= CLOTH_TIME_STEP;

    // Loop through all particles
    for( s32 i = 0; i < m_Particles.GetCount(); i++ )
    {
        // Lookup particle
        cloth_particle& P = m_Particles[i];

        // Skip if particle is pinned
        if( P.m_InvMass == 0.0f )
            continue;

        // Compute distance squared from particles
        vector3 Delta   = P.m_Pos - Pos;
        f32     DistSqr = Delta.LengthSquared();

        // Within force radius and not right on top of particle?
        if( ( DistSqr < RadiusSqr ) && ( DistSqr > 0.001f ) )
        {
            // Compute distance from particle
            f32 Dist = x_sqrt( DistSqr );

            // Compute normalized direction to particle
            vector3 Dir = Delta * ( 1.0f / Dist );

            // Compute force ratio
            f32 Force = ( Radius - Dist ) / Radius;

            // Apply impulse to update particle velocity
            P.m_LastPos -= P.m_InvMass * Dir * Force * Amount;
        }
    }
}

//==============================================================================

// Applies directional blast force to particles
void cloth::ApplyBlast( const vector3& Pos, const vector3& Dir, f32 Radius, f32 Amount )
{
    MarkRenderDataDirty();
    // Compute radius squared just once ready for loop
    f32 RadiusSqr = Radius * Radius;

    // Bake in frame rate multiplier
    Amount *= CLOTH_TIME_STEP;

    // Loop through all particles
    for( s32 i = 0; i < m_Particles.GetCount(); i++ )
    {
        // Lookup particle
        cloth_particle& P = m_Particles[i];

        // Skip if particle is pinned
        if( P.m_InvMass == 0.0f )
            continue;

        // Compute distance squared from particles
        vector3 Delta   = P.m_Pos - Pos;
        f32     DistSqr = Delta.LengthSquared();

        // Within force radius?
        if( DistSqr < RadiusSqr )
        {
            // Compute distance from particle
            f32 Dist = 0.0f;
            if( DistSqr > 0.001f )
                Dist = x_sqrt( DistSqr );

            // Compute force ratio
            f32 Force = ( Radius - Dist ) / Radius;

            // Apply impulse to update particle velocity
            P.m_LastPos -= P.m_InvMass * Dir * Force * Amount;
        }
    }
}

//==============================================================================

// Applies pain blast to particles
void cloth::OnPain( const pain& Pain )
{
    // Direct impacts from actors can apply melee force. A direct-hit target
    // on an explosion must not prevent the radial blast response.
    if( !Pain.IsExplosion() )
    {
        // Come from an actor's melee?
        object* pSource = g_ObjMgr.GetObjectByGuid( Pain.GetOriginGuid() );
        if( pSource && pSource->IsKindOf( actor::GetRTTI() ) )
        {
            // Apply melee force
            ApplyBlast( Pain.GetPosition(),
                        Pain.GetDirection(),
                        m_MinDist * 2.0f,
                        Pain.GetForce() * CLOTH_PAIN_MELEE_FORCE_SCALE );
        }
        return;
    }

    // Pain came from an explosion...

    // Lookup max force
    f32                        MaxForce           = 10.0f;
    pain_health_handle         hPainHealthHandle  = Pain.GetPainHealthHandle();
    const pain_health_profile* pPainHealthProfile = hPainHealthHandle.GetPainHealthProfile();
    if( pPainHealthProfile )
    {
        MaxForce = pPainHealthProfile->m_Force;
    }

    // Lookup force max radius
    f32                 ForceFarDist = 100.0f * 5.0f;
    pain_handle         hPain        = Pain.GetPainHandle();
    const pain_profile* pPainProfile = hPain.GetPainProfile();
    if( pPainProfile )
    {
        ForceFarDist = pPainProfile->m_ForceFarDist;
    }

    // Apply blast to particles
    ApplyBlast( Pain.GetPosition(), ForceFarDist, MaxForce * CLOTH_PAIN_EXPLOSION_FORCE_SCALE );
}

//==============================================================================

s32 cloth::SampleDamage( f32 U, f32 V ) const
{
    if( ( m_DamageMapWidth == 0 ) || ( m_DamageMapHeight == 0 ) || ( m_DamageMap.GetCount() == 0 ) )
    {
        return 15;
    }

    s32 const X = (s32)( MINMAX( 0.0f, U, 1.0f ) * ( m_DamageMapWidth  - 1 ) );
    s32 const Y = (s32)( MINMAX( 0.0f, V, 1.0f ) * ( m_DamageMapHeight - 1 ) );
    u8 const Alpha = m_DamageMap[( Y * m_DamageMapWidth ) + X];
    return MINMAX( 0, ( (s32)Alpha - 128 ) * 15 / 127, 15 );
}

//==============================================================================

s32 cloth::PunchDamage( f32 U, f32 V )
{
    s32 const Status = SampleDamage( U, V );
    if( ( m_DamageMapWidth == 0 ) || ( m_DamageMapHeight == 0 ) || ( m_DamageMap.GetCount() == 0 ) )
    {
        return Status;
    }

    // Keep the hit between texels so nearby shots do not share the same centre.
    f32 const X = MINMAX( 0.0f, U, 1.0f ) * ( m_DamageMapWidth  - 1 );
    f32 const Y = MINMAX( 0.0f, V, 1.0f ) * ( m_DamageMapHeight - 1 );
    f32 const Radius = MAX( 2.0f, (f32)MIN( m_DamageMapWidth, m_DamageMapHeight ) / 64.0f );
    u32 Seed = (u32)X * 73856093u ^ (u32)Y * 19349663u ^ ++m_DamageStamp * 83492791u;
    Seed ^= Seed << 13;
    Seed ^= Seed >> 17;
    Seed ^= Seed << 5;

    f32 const Phase = (f32)( Seed & 255 ) * ( R_360 / 255.0f );
    f32 const Stretch = 1.15f + (f32)( ( Seed >> 8 ) & 255 ) / 255.0f * 0.6f;
    xbool const AlongU = ( Seed & 0x10000 ) != 0;
    f32 const EdgeWidth = MAX( 0.65f, Radius * 0.22f );

    // Tears follow the warp and weft. Each direction occurs only once, with
    // unequal lengths and a small bend instead of several overlapping capsules.
    enum { MAX_TEAR_BRANCHES = 4 };
    f32 BranchLength[MAX_TEAR_BRANCHES];
    f32 BranchBend[MAX_TEAR_BRANCHES];
    f32 MaxRadius = Radius * Stretch;
    for( s32 i = 0; i < MAX_TEAR_BRANCHES; ++i )
    {
        Seed ^= Seed << 13;
        Seed ^= Seed >> 17;
        Seed ^= Seed << 5;
        BranchLength[i] = Radius * ( 1.4f + (f32)( Seed & 255 ) / 255.0f * 1.9f );
        BranchBend[i] = ( (f32)( ( Seed >> 8 ) & 255 ) / 255.0f - 0.5f ) * 0.32f;
        MaxRadius = MAX( MaxRadius, BranchLength[i] );
    }

    // A short missing arm keeps the silhouette from becoming a regular cross.
    BranchLength[( Seed >> 16 ) & 3] *= 0.45f;
    MaxRadius += Radius * 0.4f + EdgeWidth;

    s32 const MinX = MAX( 0, (s32)x_floor( X - MaxRadius ) );
    s32 const MaxX = MIN( m_DamageMapWidth - 1, (s32)x_ceil( X + MaxRadius ) );
    s32 const MinY = MAX( 0, (s32)x_floor( Y - MaxRadius ) );
    s32 const MaxY = MIN( m_DamageMapHeight - 1, (s32)x_ceil( Y + MaxRadius ) );
    xbool Changed = FALSE;

    for( s32 iy = MinY; iy <= MaxY; ++iy )
    {
        for( s32 ix = MinX; ix <= MaxX; ++ix )
        {
            f32 const DX = (f32)ix - X;
            f32 const DY = (f32)iy - Y;
            f32 const PX = DX / Radius;
            f32 const PY = DY / Radius;

            // Low frequency distortion makes torn flaps; independent texel noise
            // would make the edge look chipped rather than pulled apart.
            f32 const Roughness = ( x_sin( PX * 2.3f + PY * 1.7f + Phase ) +
                                    x_sin( PX * 4.1f - PY * 3.2f - Phase ) * 0.35f ) * Radius * 0.12f;
            f32 const CoreX = AlongU ? DX / Stretch : DX;
            f32 const CoreY = AlongU ? DY : DY / Stretch;
            f32 Distance = x_sqrt( CoreX * CoreX + CoreY * CoreY ) - Radius - Roughness;

            for( s32 i = 0; i < MAX_TEAR_BRANCHES; ++i )
            {
                f32 const Along = ( i < 2 ? DX : DY ) * ( ( i & 1 ) ? -1.0f : 1.0f );
                if( ( Along < 0.0f ) || ( Along > BranchLength[i] ) )
                    continue;

                f32 const T = Along / BranchLength[i];
                f32 const Across = ( i < 2 ? DY : DX ) - BranchBend[i] * Along * T;
                f32 const Width = Radius * 0.6f * ( 1.0f - T );
                f32 const Tear = MAX( x_abs( Across ) - Width - Roughness * 0.3f,
                                      Along - BranchLength[i] );
                Distance = MIN( Distance, Tear );
            }

            if( Distance >= EdgeWidth )
                continue;

            f32 KeepAlpha = MINMAX( 0.0f, 0.5f + Distance / ( 2.0f * EdgeWidth ), 1.0f );

            // Leave a few short yarns at the lips, with an open centre. Their
            // positions stay fixed in cloth UVs when the simulation moves.
            if( ( x_abs( DX ) + x_abs( DY ) > Radius * 1.25f ) && ( Distance < 0.0f ) )
            {
                f32 const YarnX = DX + x_sin( PY * 1.6f + Phase ) * 0.25f;
                f32 const YarnY = DY + x_sin( PX * 1.8f - Phase ) * 0.25f;
                f32 const Spacing = MAX( 2.5f, Radius * 1.1f );
                f32 const ThreadX = x_abs( YarnX - x_floor( YarnX / Spacing + 0.5f ) * Spacing );
                f32 const ThreadY = x_abs( YarnY - x_floor( YarnY / Spacing + 0.5f ) * Spacing );
                f32 const Thread = 1.0f - MINMAX( 0.0f, MIN( ThreadX, ThreadY ) / 0.65f, 1.0f );
                f32 const Lip = MINMAX( 0.0f, 1.0f + Distance / ( Radius * 0.55f ), 1.0f );
                KeepAlpha = MAX( KeepAlpha, Thread * Lip );
            }

            u8 const Alpha = (u8)( KeepAlpha * 255.0f );
            u8& Current = m_DamageMap[( iy * m_DamageMapWidth ) + ix];
            if( Alpha < Current )
            {
                Current = Alpha;
                Changed = TRUE;
            }
        }
    }

    if( Changed )
    {
        MarkDamageDirty( MinX, MinY, MaxX, MaxY );
    }

    return Status;
}

//==============================================================================

//
//          P0
//         /|\
//        / | \
//       /  |  \
//      / 2 | 1 \
//     /  _-TP-_ \
//    / _-      -_\
//   /_-    0     -\
//  P1--------------P2
//
static
xbool ComputeBarys( const vector3& P0,
                   const vector3& P1,
                   const vector3& P2,
                   const vector3& TP,
                         vector3& Bary )
{
    // Compute scaled normal
    vector3 Normal = v3_Cross(P1-P0, P2-P0);
    f32 const NormalLengthSquared = Normal.LengthSquared();
    if( NormalLengthSquared < 0.000001f )
    {
        Bary.Zero();
        return FALSE;
    }
    Normal *= 1.0f / NormalLengthSquared;

    // Compute barycentric co-ords
    Bary.Set( v3_Cross(P2-P1, TP-P1).Dot(Normal),
              v3_Cross(P0-P2, TP-P2).Dot(Normal),
              v3_Cross(P1-P0, TP-P0).Dot(Normal) );
    return TRUE;
}

//==============================================================================

void cloth::OnProjectileImpact( const object&   ProjectileObject,
                                const vector3&  ProjectileVel,
                                      u32       CollPrimKey,
                                const vector3&  CollPoint,
                                      xbool     PunchDamageHole,
                                      f32       ManualImpactForce )
{
    // Skip if not collision is not with cloth triangle
    if( ( CollPrimKey & CLOTH_PRIM_KEY_TRI_FLAG ) == 0 )
        return;

    // Lookup the tri that was hit - skip if not valid
    s32 HitTri = (s32)( CollPrimKey & ~CLOTH_PRIM_KEY_TRI_FLAG );
    if( ( HitTri < 0 ) || ( HitTri >= m_Triangles.GetCount() ) )
        return;

    // Lookup tri and tri particles
    cloth_triangle& Tri       = m_Triangles[HitTri];
    cloth_particle& ParticleA = m_Particles[(s32)Tri.m_Particles[0]];
    cloth_particle& ParticleB = m_Particles[(s32)Tri.m_Particles[1]];
    cloth_particle& ParticleC = m_Particles[(s32)Tri.m_Particles[2]];

    vector3 Bary;
    if( !ComputeBarys( ParticleA.m_Pos, ParticleB.m_Pos, ParticleC.m_Pos, CollPoint, Bary ) )
    {
        return;
    }

    MarkRenderDataDirty();

    vector2 const UV = ( Bary.GetX() * ParticleA.m_UV ) +
                       ( Bary.GetY() * ParticleB.m_UV ) +
                       ( Bary.GetZ() * ParticleC.m_UV );

    f32 Scale = ( 1.0f / 15.0f ) * 0.8f;
    Scale *= PunchDamageHole ? PunchDamage( UV.X, UV.Y ) : SampleDamage( UV.X, UV.Y );
    Scale += 0.2f;

    // Compute impact vel
    vector3 ImpactVel = ProjectileVel;
    if( ManualImpactForce > 0 )
    {
        ImpactVel.NormalizeAndScale( ManualImpactForce );
    }
    else
    {
        ImpactVel *= m_ImpactScale;
    }
    // Impact settings describe a velocity change at the reference step. The
    // previous-position offset must shrink with dt to keep that velocity.
    ImpactVel *= Scale * ( CLOTH_TIME_STEP / CLOTH_REFERENCE_TIME_STEP );

    // Apply to particles
    ParticleA.m_LastPos -= Bary.GetX() * ImpactVel;
    ParticleB.m_LastPos -= Bary.GetY() * ImpactVel;
    ParticleC.m_LastPos -= Bary.GetZ() * ImpactVel;

    voice_id VoiceID = g_AudioMgr.Play( "BulletImpactFabric",
                                        CollPoint, ProjectileObject.GetZone1(), TRUE );

    g_AudioManager.NewAudioAlert( VoiceID,
                                  audio_manager::BULLET_IMPACTS,
                                  CollPoint,
                                  ProjectileObject.GetZone1(),
                                  NULL_GUID );

}

//==============================================================================
// Logic functions
//==============================================================================

void cloth::Advance( f32 DeltaTime)
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth::Advance");

    LOG_STAT(k_stats_Physics);

    // Add to total time
    m_DeltaTime += DeltaTime;

    // Apply delta time pool
    while(m_DeltaTime >= CLOTH_TIME_STEP)
    {
        m_WindDir.Rotate( m_WindRot * CLOTH_TIME_STEP );
        m_WindDir.Normalize();
        ApplyWind();

        // Integrate
        Integrate(CLOTH_TIME_STEP);

        // Iterate to solve constraints
        ApplyConstraints();

        // Next
        m_DeltaTime -= CLOTH_TIME_STEP;
    }

    // Update bounds
    ComputeBounds();
    MarkRenderDataDirty();

}

//==============================================================================
// Set functions
//==============================================================================

void cloth::SetL2W( const matrix4& L2W, xbool bReset, f32 AirResistance )
{
    ASSERT( AirResistance >= 0.0f );
    ASSERT( AirResistance <= 1.0f );

    // Reset positions and bbox?
    if( bReset )
    {
        // Store new matrices
        m_L2W = L2W;
        m_W2L = m4_InvertRT( L2W );

        // Reset all particles
        Reset();
    }
    else
    {
        // Compute old and new info
        vector3    OldPos = m_L2W.GetTranslation();
        quaternion OldRot = m_L2W.GetQuaternion();
        vector3    NewPos = L2W.GetTranslation();
        quaternion NewRot = L2W.GetQuaternion();
        quaternion InvOldRot = OldRot;
        InvOldRot.Invert();

        // Compute delta position/yaw for non-pinned particles
        vector3    DeltaPos = NewPos - OldPos;
        quaternion DeltaRot = NewRot * InvOldRot; // NOTE: Quats read left->right so this is equiv to NewRot - OldRot

        // Take air resistance into account
        DeltaPos *= ( 1.0f - AirResistance );                   // if AR=0 then DP=DeltaPos, if AR=1 then DP=(0,0,0)
        DeltaRot = BlendToIdentity( DeltaRot, AirResistance );  // if AR=0 then DR=DeltaRot, if AR=1 then DR=Identity

        // Compute delta transform matrix that will apply delta to non-pinned particles
        matrix4 DeltaTM;
        DeltaTM.Identity();
        DeltaTM.SetTranslation( -OldPos );          // Move to pivot
        DeltaTM.Rotate( DeltaRot );                 // Rotate around pivot
        DeltaTM.Translate( OldPos + DeltaPos );     // Put back into world

        // Store new matrices
        m_L2W = L2W;
        m_W2L = m4_InvertRT( L2W );

        // Update particle positions
        for (s32 i = 0; i < m_Particles.GetCount(); i++)
        {
            cloth_particle& Particle = m_Particles[i];

            // Pinned?
            if( Particle.m_InvMass == 0.0f )
            {
                // Compute position as if rigidly attached to the pole
                Particle.m_Pos = Particle.m_LastPos = m_L2W * Particle.m_BindPos;
            }
            else
            {
                // Update position and previous position so velocity isn't changed
                Particle.m_Pos     = DeltaTM * Particle.m_Pos;
                Particle.m_LastPos = DeltaTM * Particle.m_LastPos;
            }
        }

        // Update bounds
        ComputeBounds();
        ComputeWorldCollBBox();
        MarkRenderDataDirty();
    }
}

//==============================================================================

void cloth::MarkRenderDataDirty( void )
{
    ++m_RenderRevision;
    if( m_RenderRevision == 0 )
        m_RenderRevision = 1;
}

//==============================================================================

u32 cloth::GetRenderRevision( void ) const
{
    return m_RenderRevision;
}

//==============================================================================

void cloth::SetLocalCollBBox( const bbox& BBox )
{
    // Setup local bbox with validation incase min/max are backwards
    m_LocalCollBBox = BBox;
    m_LocalCollBBox.Min.Min( BBox.Max );
    m_LocalCollBBox.Max.Max( BBox.Min );

    // Update world bbox
    ComputeWorldCollBBox();

    // Reset positions and bbox
    Reset();
}

//==============================================================================

cloth_particle::cloth_particle() :
    m_BindPos       ( 0.0f, 0.0f, 0.0f ),
    m_Pos           ( 0.0f, 0.0f, 0.0f ),
    m_LastPos       ( 0.0f, 0.0f, 0.0f ),
    m_UV            ( 0.0f, 0.0f ),
    m_InvMass       ( 1.0f ),
    m_WindStrength  ( 0.0f ),
    m_WindTarget    ( 0.0f )
{
}

//===========================================================================

f32 cloth_particle::GetMass( void ) const
{
    // Infinite mass?
    if( m_InvMass == 0.0f )
        return 0.0f;
    else
        return 1.0f / m_InvMass;
}

//===========================================================================

void cloth_particle::SetMass( f32 Mass )
{
    // Infinite mass?
    if( Mass == 0.0f )
        m_InvMass = 0.0f;
    else
        m_InvMass = 1.0f / Mass;
}

//===========================================================================
