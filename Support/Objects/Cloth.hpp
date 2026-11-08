//==============================================================================
//
//  Cloth.hpp
//
//==============================================================================

#ifndef __CLOTH_HPP__
#define __CLOTH_HPP__

//==============================================================================
// INCLUDES
//==============================================================================
#include "x_files.hpp"
#include "CollisionMgr/CollisionMgr.hpp"


//==============================================================================
// FORWARD DECLARATIONS
//==============================================================================
class pain;
class rigid_geom;


//==============================================================================
// CLASSES
//==============================================================================


// Particle
class cloth_particle
{
// Functions
public:
             cloth_particle();

    // Mass functions
    f32     GetMass( void ) const;
    void    SetMass( f32 Mass );

// Data
public:
    vector3 m_BindPos ;     // Bind position
    vector3 m_Pos ;         // Current position
    vector3 m_LastPos ;     // Last frame position
    vector2 m_UV ;          // Texture UV
    f32     m_InvMass ;     // 1.0f / Mass
    f32     m_WindStrength;
    f32     m_WindTarget;
} ;

//==============================================================================

// Connection between 2 particles
class cloth_connection
{
public:
    u16     m_ParticleA ;   // Particle A
    u16     m_ParticleB ;   // Particle B
    f32     m_RestDistSqr ; // Rest distance squared between particles
} ;

//==============================================================================

// Triangle made out of 3 particles
class cloth_triangle
{
public:
    u16     m_Particles[3] ;    // Indices into 3 particles
} ;

//==============================================================================

// Result of walking the rigid geometry's "cloth"/"flag" submeshes during Init().
// RenderMask/MaterialIndex are render-owned outputs of that walk, handed back to
// the caller (cloth_inst) so this class never has to know about rendering.
struct cloth_init_result
{
    u64     RenderMask ;        // Render instance mask (submeshes to skip when rendering the rigid part)
    s32     MaterialIndex ;     // Index of cloth material in rigid geom (-1 if none found)
} ;

//===========================================================================

// Cloth simulation - particles, constraints, wind and collision/pain reactions.
// Rendering (rigid_inst, dynamic vertex/index buffers, draw calls) lives in
// cloth_inst (Objects\Render\ClothInst.hpp).
class cloth
{
// Defines
public:

// Structures
private:

// Data
private:

            // Physics components
            xarray<cloth_particle>      m_Particles;            // List of particles
            xarray<cloth_connection>    m_Connections;          // List of connections
            xarray<cloth_triangle>      m_Triangles;            // List of triangles
            f32                         m_MinDist;              // Min distance squared between particles
            bbox                        m_LocalCollBBox ;       // Local space axis aligned fast collision box
            bbox                        m_WorldCollBBox ;       // World space axis aligned fast collision box
            bbox                        m_GeomLocalBBox ;       // Local bbox of the source geometry, cached at Init() so ComputeBounds() doesn't need the render instance

            // Physics properties
            vector3                     m_Gravity;              // Gravity to add to cloth
            f32                         m_Dampen;               // Dampen amount
            f32                         m_Stretch;               // Stretch amount
            s32                         m_nIterations;          // # of constraint iterations
            f32                         m_ImpactScale;          // Impact scale of bullets
            f32                         m_SimulationTime;       // Time to simulate before level starts

            // Simulation state
            f32                         m_DeltaTime ;           // Accumulated delta time
            u32                         m_RenderRevision;       // Revision of particle positions used by render caches
            guid                        m_ObjectGuid ;          // Owner guid (or NULL if none)
            matrix4                     m_L2W ;                 // Local to world matrix
            matrix4                     m_W2L ;                 // World to local matrix
            bbox                        m_LocalBBox ;           // Local space bbox
            bbox                        m_WorldBBox ;           // World space bbox

            // Damage map
            xarray<u8>                  m_DamageMap;            // CPU-owned R8 damage mask
            s32                         m_DamageMapWidth;       // Damage mask width
            s32                         m_DamageMapHeight;      // Damage mask height
            s32                         m_DamageDirtyMinX;      // Dirty rectangle, inclusive
            s32                         m_DamageDirtyMinY;
            s32                         m_DamageDirtyMaxX;
            s32                         m_DamageDirtyMaxY;
            u32                         m_DamageStamp;

            // Wind vars
            xbool                       m_bWindEnabled;
            f32                         m_WindSampleTime;       // Time until the next turbulence sample
            vector3                     m_WindDir;              // Direction of wind
            radian3                     m_WindRot;              // Rotation of wind
            f32                         m_WindMinStrength;      // Min strength of wind
            f32                         m_WindMaxStrength;      // Max strength of wind

            // Static vars

// Functions
public:
         cloth() ;
         ~cloth() ;

// Functions

    // Private initialization functions
private:

            s32         FindParticle            ( const vector3& P ) ;
            s32         FindParticle            ( const vector3& P, const vector2& UV ) ;
            s32         AddParticle             ( const vector3& P, const vector2& UV ) ;
            void        PegParticles            ( s32 iAxis, s32 Dir );
            void        ClearAllInvMasses       ( void );
            s32         SampleDamage            ( f32 U, f32 V ) const;
            s32         PunchDamage             ( f32 U, f32 V );
            void        MarkDamageDirty         ( s32 MinX, s32 MinY, s32 MaxX, s32 MaxY );

            s32         FindConnection          ( s32 ParticleA, s32 ParticleB ) ;
            s32         AddConnection           ( s32 ParticleA, s32 ParticleB ) ;

            s32         FindTriangleVert        ( s32 Triangle, s32 Vertex ) ;
            s32         FindTriangle            ( s32 Verts[3] ) ;
            void        AddTriangle             ( const vector3& P0, const vector2& UV0,
                                                  const vector3& P1, const vector2& UV1,
                                                  const vector3& P2, const vector2& UV2 ) ;

    // Public initialization functions
public:
            void        SetObjectGuid           ( guid Guid ) { m_ObjectGuid = Guid; }
            cloth_init_result Init              ( rigid_geom* pGeom );
            void        Reset                   ( void ) ;
            void        Kill                    ( void ) ;
            void        InitDamageMap           ( s32 Width, s32 Height );
            void        ClearDamageDirty        ( void );


            // NOTE: Property enumeration order matters for load/save - callers must
            //       enumerate in this order: OnEnumPropGeometry(), then the owning
            //       object's RenderInst properties (so geometry/particles load),
            //       then OnEnumProp() for the rest of the Cloth\* properties.
            void        OnEnumPropGeometry      ( prop_enum&    rList  );
            void        OnEnumProp              ( prop_enum&    rList  );
            xbool       OnProperty              ( prop_query&   rQuery );
private:

    // Physics functions
public:
            void        ApplyCappedCylinderColl ( const vector3& Bottom, const vector3& Top, f32 Radius ) ;
            void        OnColCheck              ( guid ObjectHitGuid, u32 Flags ) ;

private:
            void        ApplyDistConstraints    ( void ) ;
            void        ApplyCollConstraints    ( void ) ;
            void        ApplyConstraints        ( void ) ;
            void        ApplyWind               ( void ) ;
            void        Integrate               ( f32 DeltaTime ) ;
            void        ComputeBounds           ( void ) ;
            void        ComputeWorldCollBBox    ( void ) ;
            void        MarkRenderDataDirty     ( void ) ;

public:

    // Applies blast force to particles
            void        ApplyBlast              ( const vector3& Pos, f32 Radius, f32 Amount ) ;
            void        ApplyBlast              ( const vector3& Pos, const vector3& Dir, f32 Radius, f32 Amount );

    // Applies pain blast to particles
            void        OnPain                  ( const pain& Pain );

            void        OnProjectileImpact      ( const object&          ProjectileObject,
                                                  const vector3&         ProjectileVel,
                                                        u32              CollPrimKey,
                                                  const vector3&         CollPoint,
                                                  xbool                  PunchDamageHole,       // TRUE to punch out a hole
                                                  f32                    ManualImpactForce );    // <= 0 : normal behaviour for bullets
                                                                                                // >  0 : scale force to this length


    // Logic functions
            void        Advance                 ( f32 DeltaTime) ;

    // Set functions
            void        SetL2W                  ( const matrix4& L2W, xbool bReset = TRUE, f32 AirResistance = 0.0f ) ;
            void        SetLocalCollBBox        ( const bbox& BBox ) ;
            void        SetWindEnabled          ( xbool Enabled );

    // Tuning functions (let owning objects like flag specialize physics behaviour)
            void        SetGravity              ( const vector3& Gravity ) { m_Gravity = Gravity; }
            void        SetDampen               ( f32 Dampen )             { m_Dampen = Dampen; }
            void        SetIterations           ( s32 Iterations )         { m_nIterations = Iterations; }

    // Query functions
    const matrix4&                     GetL2W           ( void ) const ;
    const bbox&                        GetLocalCollBBox ( void ) ;
    const bbox&                        GetLocalBBox     ( void ) const ;
    const bbox&                        GetWorldBBox     ( void ) const ;
    const bbox&                        GetWorldCollBBox ( void ) const ;
    const xarray<cloth_particle>&      GetParticles     ( void ) const ;
    const xarray<cloth_connection>&    GetConnections   ( void ) const ;
    const xarray<cloth_triangle>&      GetTriangles     ( void ) const ;
    const xarray<u8>&                  GetDamageMap     ( void ) const ;
          s32                          GetDamageWidth   ( void ) const ;
          s32                          GetDamageHeight  ( void ) const ;
          xbool                        GetDamageDirty   ( s32& MinX, s32& MinY, s32& MaxX, s32& MaxY ) const ;
    const vector3&                     GetWindDir       ( void ) const ;
          xbool                        IsWindActive     ( void ) const ;
          u32                          GetRenderRevision ( void ) const ;
} ;

//==============================================================================
// Query functions
//==============================================================================

inline
void cloth::SetWindEnabled( xbool Enabled )
{
    m_bWindEnabled = Enabled;
}

//==============================================================================

inline
const matrix4& cloth::GetL2W( void ) const
{
    return m_L2W;
}

//==============================================================================

inline
const bbox& cloth::GetLocalCollBBox( void )
{
    return m_LocalCollBBox;
}

//==============================================================================

inline
const bbox& cloth::GetLocalBBox( void ) const
{
    return m_LocalBBox;
}

//==============================================================================

inline
const bbox& cloth::GetWorldBBox( void ) const
{
    return m_WorldBBox;
}

//==============================================================================

inline
const bbox& cloth::GetWorldCollBBox( void ) const
{
    return m_WorldCollBBox;
}

//==============================================================================

inline
const xarray<cloth_particle>& cloth::GetParticles( void ) const
{
    return m_Particles;
}

//==============================================================================

inline
const xarray<cloth_connection>& cloth::GetConnections( void ) const
{
    return m_Connections;
}

//==============================================================================

inline
const xarray<cloth_triangle>& cloth::GetTriangles( void ) const
{
    return m_Triangles;
}

//==============================================================================

inline
const xarray<u8>& cloth::GetDamageMap( void ) const
{
    return m_DamageMap;
}

//==============================================================================

inline
s32 cloth::GetDamageWidth( void ) const
{
    return m_DamageMapWidth;
}

//==============================================================================

inline
s32 cloth::GetDamageHeight( void ) const
{
    return m_DamageMapHeight;
}

//==============================================================================

inline
const vector3& cloth::GetWindDir( void ) const
{
    return m_WindDir;
}

//==============================================================================

inline
xbool cloth::IsWindActive( void ) const
{
    return m_bWindEnabled;
}

//==============================================================================


#endif  // #ifndef __CLOTH_HPP__
