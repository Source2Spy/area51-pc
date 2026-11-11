//==============================================================================
//  DecalMgr.cpp
//
//  Copyright (c) 2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This class handles pasting decals all around the level (both static and
//  dynamic). This could include blood stains, bullet holes, dirt, mold, etc.
//==============================================================================

//==============================================================================
//  Includes
//==============================================================================

#include "DecalMgr.hpp"
#include "DecalDefinition.hpp"
#include "DecalPackage.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/PlaySurface.hpp"
#include "CollisionMgr/CollisionMgr.hpp"
#include "PlaySurfaceMgr/PlaySurfaceMgr.hpp"
#include "Render/DecalBatch.hpp"
#include "Render/Render.hpp"
#include "Render/SurfaceColor.hpp"
#include "Render/Texture.hpp"

#include "Entropy.hpp"
#include "e_ScratchMem.hpp"

//==============================================================================
// Typedefs
//==============================================================================


//==============================================================================
// Globals and statics
//==============================================================================

decal_mgr   g_DecalMgr;

static const f32    kBulletHoleCoplanarDistCheck = 2.0f;
static const f32    kCoplanarDistanceCheck       = 2.5f;
static const radian kIncomingAngleMinimum        = R_20;
static const f32    kEpsilon                     = 0.005f;
static const f32    kAmbientRayHalfLength        = 5.0f;

#define CLIP_DECAL_VERT_NEG_X   0x01
#define CLIP_DECAL_VERT_POS_X   0x02
#define CLIP_DECAL_VERT_NEG_Y   0x04
#define CLIP_DECAL_VERT_POS_Y   0x08
#define CLIP_DECAL_VERT_NEG_Z   0x10
#define CLIP_DECAL_VERT_POS_Z   0x20
#define CLIP_DECAL_TRI_NEG_X    ((CLIP_DECAL_VERT_NEG_X<<12)+(CLIP_DECAL_VERT_NEG_X<<6)+CLIP_DECAL_VERT_NEG_X)
#define CLIP_DECAL_TRI_POS_X    ((CLIP_DECAL_VERT_POS_X<<12)+(CLIP_DECAL_VERT_POS_X<<6)+CLIP_DECAL_VERT_POS_X)
#define CLIP_DECAL_TRI_NEG_Y    ((CLIP_DECAL_VERT_NEG_Y<<12)+(CLIP_DECAL_VERT_NEG_Y<<6)+CLIP_DECAL_VERT_NEG_Y)
#define CLIP_DECAL_TRI_POS_Y    ((CLIP_DECAL_VERT_POS_Y<<12)+(CLIP_DECAL_VERT_POS_Y<<6)+CLIP_DECAL_VERT_POS_Y)
#define CLIP_DECAL_TRI_NEG_Z    ((CLIP_DECAL_VERT_NEG_Z<<12)+(CLIP_DECAL_VERT_NEG_Z<<6)+CLIP_DECAL_VERT_NEG_Z)
#define CLIP_DECAL_TRI_POS_Z    ((CLIP_DECAL_VERT_POS_Z<<12)+(CLIP_DECAL_VERT_POS_Z<<6)+CLIP_DECAL_VERT_POS_Z)

//==============================================================================
// Helper functions
//==============================================================================

inline
vector3 GetTriNormal( const vector3& P0,
                      const vector3& P1,
                      const vector3& P2,
                      xbool bNormalize )
{
    vector3 Result( v3_Cross( P1-P0, P2-P0 ) );
    if ( bNormalize )
        Result.Normalize();
    return Result;
}

//==============================================================================
// Implementation
//==============================================================================

decal_mgr::decal_span::decal_span( void ) :
    SourceStart     ( 0 ),
    SourceCount     ( 0 ),
    DecalStart      ( 0 ),
    DecalCount      ( 0 ),
    Zone            ( 0xffffffffu ),
    Bounds          ( vector3( 0.0f, 0.0f, 0.0f ) ),
    GeometricNormal ( 0.0f, 1.0f, 0.0f ),
    Ambient         ( 64, 64, 64, 255 ),
    FadeElapsed     ( 0.0f ),
    IsValid         ( FALSE )
{
}

//==============================================================================

decal_mgr::registration_info::registration_info( void ) :
    m_nVertsAllocated   ( 0 ),
    m_Start             ( 0 ),
    m_End               ( 0 ),
    m_Blank             ( 0 ),
    m_pPositions        ( NULL ),
    m_pUVs              ( NULL ),
    m_pColors           ( NULL ),
    m_pDynamicSpans     ( NULL ),
    m_nDynamicSpans     ( 0 ),
    m_nDynamicSpansAllocated( 0 ),
    m_NextDynamicSpan   ( 0 ),
    m_BlendMode         ( decal_definition::DECAL_BLEND_ADD ),
    m_Flags             ( 0 ),
    m_FadeoutTime       ( 1.5f ),
    m_Color             ( 255, 255, 255 ),
#ifdef X_EDITOR
    m_nStaticVertsAlloced   ( 0 ),
    m_nStaticVerts          ( 0 ),
    m_pStaticPositions      ( NULL ),
    m_pStaticUVs            ( NULL ),
    m_pStaticColors         ( NULL ),
#endif
    m_StaticDataOffset  ( -1 ),
    m_StaticSpanStart   ( 0 ),
    m_StaticSpanCount   ( 0 )
{
    ForceDecalLoaderLink();
}

//==============================================================================

decal_mgr::registration_info::~registration_info( void )
{
    Kill();
}

//==============================================================================

void decal_mgr::registration_info::Kill( void )
{
    if ( m_nVertsAllocated )
    {
        x_free( m_pPositions );
    }
    if ( m_pDynamicSpans )
    {
        x_free( m_pDynamicSpans );
    }
    m_nVertsAllocated = 0;
    m_Start           = 0;
    m_End             = 0;
    m_Blank           = 0;
    m_pPositions      = NULL;
    m_pUVs            = NULL;
    m_pColors         = NULL;
    m_pDynamicSpans   = NULL;
    m_nDynamicSpans   = 0;
    m_nDynamicSpansAllocated = 0;
    m_NextDynamicSpan = 0;
#ifdef X_EDITOR
    if ( m_nStaticVertsAlloced )
    {
        x_free( m_pStaticPositions );
    }
    m_nStaticVerts        = 0;
    m_nStaticVertsAlloced = 0;
    m_pStaticPositions    = NULL;
    m_pStaticUVs          = NULL;
    m_pStaticColors       = NULL;
#endif
    m_BlendMode        = decal_definition::DECAL_BLEND_ADD;
    m_Flags            = 0;
    m_FadeoutTime      = 1.5f;
    m_Color            = XCOLOR_WHITE;
    m_StaticDataOffset = -1;
    m_StaticSpanStart  = 0;
    m_StaticSpanCount  = 0;
}

//==============================================================================

s32 decal_mgr::registration_info::GetAllocSize( s32 nVerts )
{
    s32 AllocSize = 0;
    AllocSize += ALIGN_16( nVerts * sizeof(position_data) );
    AllocSize += ALIGN_16( nVerts * sizeof(uv_data) );
    AllocSize += ALIGN_16( nVerts * sizeof(u32) );
    return AllocSize;
}

//==============================================================================

void decal_mgr::registration_info::AllocVertList( s32 nVerts )
{
    //MEMORY_OWNER( "decal_mgr::registration_info::AllocVertList()" );
    MEMORY_OWNER( "DECAL VERTLIST" );

    s32 AllocSize = GetAllocSize( m_nVertsAllocated + nVerts );
    if ( m_nVertsAllocated == 0 )
    {
        // easy case, just alloc the new verts
        m_nVertsAllocated    = nVerts;
        byte* pAllocAddress  = (byte*)x_malloc( AllocSize );
        m_pPositions         = (position_data*)pAllocAddress;
        pAllocAddress       += ALIGN_16( m_nVertsAllocated * sizeof(position_data) );
        m_pUVs               = (uv_data*)pAllocAddress;
        pAllocAddress       += ALIGN_16( m_nVertsAllocated * sizeof(uv_data) );
        m_pColors            = (u32*)pAllocAddress;
        pAllocAddress       += ALIGN_16( m_nVertsAllocated * sizeof(u32) );
        ASSERT( pAllocAddress == ((byte*)m_pPositions)+AllocSize );

        m_nDynamicSpansAllocated = MAX( 1, nVerts / 3 );
        m_pDynamicSpans = static_cast<decal_span*>(
            x_malloc( m_nDynamicSpansAllocated * sizeof( decal_span ) ) );
        ASSERT( m_pDynamicSpans );
        for ( s32 i = 0; i < m_nDynamicSpansAllocated; ++i )
        {
            m_pDynamicSpans[i] = decal_span();
        }

        #ifdef X_EDITOR
        GrowStaticVertListBy( nVerts );
        #endif

        m_Start           = 0;
        m_End             = 0;
        m_Blank           = 0;
    }
    else
    {
        ASSERT( FALSE );
        m_Start           = 0;
        m_End             = 0;
        m_Blank           = 0;
    }
}

//==============================================================================

decal_mgr::decal_mgr( void ) :
    m_DynamicQueueAddPos( 0 ),
    m_DynamicQueueReadPos( 0 ),
    m_pStaticData( NULL )
{
    s32 i;
    for ( i = 0; i < DYNAMIC_QUEUE_SIZE; i++ )
    {
        m_DynamicQueue[i].Valid = FALSE;
    }
    // Setup triangle template for bulletholes etc.
    m_TriangleTemplateUV[0].U = (s16)((+0.500f) * 4096.0f);
    m_TriangleTemplateUV[0].V = (s16)((+1.500f) * 4096.0f);
    m_TriangleTemplateUV[1].U = (s16)((+1.366f) * 4096.0f);
    m_TriangleTemplateUV[1].V = (s16)((+0.000f) * 4096.0f);
    m_TriangleTemplateUV[2].U = (s16)((-0.366f) * 4096.0f);
    m_TriangleTemplateUV[2].V = (s16)((+0.000f) * 4096.0f);

}

//==============================================================================

decal_mgr::~decal_mgr( void )
{
    Kill();
}

//==============================================================================

void decal_mgr::Init( void )
{
    m_RegisteredDefs.Clear();
    m_RegisteredDefs.GrowListBy( decal_mgr::MAX_DECAL_RESOURCES );
}

//==============================================================================

void decal_mgr::Kill( void )
{
    UnloadStaticDecals();
    m_RegisteredDefs.Clear();
    m_StaticSpans.Clear();

    ClearDynamicQueue();
}

//==============================================================================

void decal_mgr::ClearDynamicQueue( void )
{
    s32 i;
    m_DynamicQueueAddPos  = 0;
    m_DynamicQueueReadPos = 0;
    for ( i = 0; i < DYNAMIC_QUEUE_SIZE; i++ )
    {
        m_DynamicQueue[i].Valid = FALSE;
    }
}

//==============================================================================

void decal_mgr::ResetDynamicDecals( void )
{
    s32 i;
    for ( i = 0; i < m_RegisteredDefs.GetCount(); i++ )
    {
        registration_info& RegInfo = m_RegisteredDefs[i];

        RegInfo.m_Start = 0;
        RegInfo.m_End   = 0;
        RegInfo.m_Blank = 0;
        RegInfo.m_nDynamicSpans = 0;
        RegInfo.m_NextDynamicSpan = 0;
        for ( s32 iSpan = 0; iSpan < RegInfo.m_nDynamicSpansAllocated; ++iSpan )
        {
            RegInfo.m_pDynamicSpans[iSpan].IsValid = FALSE;
        }
    }

    ClearDynamicQueue();
}

//==============================================================================

void decal_mgr::CreateDecalFromRayCast( const decal_definition& Def,
                                        const vector3&          Start,
                                        const vector3&          End,
                                        const vector2&          Size,
                                        radian                  Roll )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::CreateDecalFromRayCast" );

    // cast a ray, and see if there were any collisions
    g_CollisionMgr.RaySetup( NULL_GUID, Start, End );
    g_CollisionMgr.CheckCollisions( object::TYPE_PLAY_SURFACE,
                                    object::ATTR_COLLIDABLE,
                                    object::ATTR_COLLISION_PERMEABLE );
    if ( g_CollisionMgr.m_nCollisions == 0 )
        return;

    // get the collision information
    collision_mgr::collision& Collision      = g_CollisionMgr.m_Collisions[0];
    vector3                   NegIncomingRay = (Start-End);
    NegIncomingRay.Normalize();

    // add this guy to the queue so we don't do too many of these
    // horrendous calculations at one time
    if ( !(Def.m_Flags & decal_definition::DECAL_FLAG_NO_CLIP) )
    {
        AddClippedToQueue( Def, Collision.Point, Collision.Plane.Normal, NegIncomingRay, Size, Roll );
        return;
    }

    // generate the decal verts
    matrix4     L2W;
    decal_vert  DecalVerts[MAX_VERTS_PER_DECAL];
    
    // calculate the decal verts
    s32 nVerts = CalcDecalVerts( Def.m_Flags,
                                 Collision.Point,
                                 Collision.Plane.Normal,
                                 NegIncomingRay,
                                 Size,
                                 Roll,
                                 DecalVerts,
                                 L2W );

    if ( nVerts )
    {
        AddDecal( Def.m_Handle, nVerts, DecalVerts, L2W );
    }
}

//==============================================================================

void  decal_mgr::CreateBulletHole( const decal_definition& Def,
                                   const vector3&          CenterPoint,
                                   const plane&            Plane,
                                   const vector3*          pTriPos )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::CreateBulletHole" );

    // Assumptions:
    // - Decal will not be clipped.
    // - Decal uses uniform size. Width == Height
    // - Decal is rendered with a triangle rather than a quad
    // - If all three points are not coplanar then decal will not be rendered
    ASSERTS( Def.m_Flags & decal_definition::DECAL_FLAG_USE_TRI, "Bulletholes must have DECAL_FLAG_USE_TRI" );

    //
    // Get the plane-parallel axiis and build the three world-space positions.
    // Because of floating-point error, the ortho vectors can get turned 90 degrees
    // when hitting a wall. Handle that as a special-case by forcing the "up"
    // vector to be the Y axis.
    //
    vector3 AxisA, AxisB;
    if ( Plane.Normal.GetY() < 0.001f )
    {
        vector3 Dir( 0.0f, 1.0f, 0.0f );
        AxisA = Plane.Normal.Cross( Dir );
        AxisB = Plane.Normal.Cross( AxisA );
        AxisA.Normalize();
        AxisB.Normalize();
    }
    else
    {
        Plane.GetOrthoVectors( AxisA, AxisB );
    }

    // Scale the axis to a random size
    f32 Radius  = x_frand( Def.m_MinSize.X, Def.m_MaxSize.X );
    AxisA *= Radius;
    AxisB *= Radius;

    // Build the points from the axis
    vector3 Pos[3];
    Pos[0] = CenterPoint + AxisB;
    AxisA *=  0.866f;
    AxisB *= -0.500f;
    AxisB += CenterPoint;
    Pos[1] = AxisB - AxisA;
    Pos[2] = AxisB + AxisA;

    // Get triangle edge normals 
    vector3 EdgeNormal[3];
    EdgeNormal[0] = v3_Cross(Plane.Normal,pTriPos[1]-pTriPos[0]);
    EdgeNormal[1] = v3_Cross(Plane.Normal,pTriPos[2]-pTriPos[1]);
    EdgeNormal[2] = v3_Cross(Plane.Normal,pTriPos[0]-pTriPos[2]);

    //
    // Loop through the worldspace points and test if the point is inside the triangle
    // or if the point is sitting on a coplanar neighbor.
    //
    s32 i,j;
    for( i=0; i<3; i++ )
    {
        xbool bCollOK = FALSE;
        for( j=0; j<3; j++ )
        {
            // If point is outside edge then check for coplanar neighbor
            if( EdgeNormal[j].Dot( Pos[i]-pTriPos[j] ) < 0.0f )
            {
                vector3 CollRay = Plane.Normal * kBulletHoleCoplanarDistCheck;

                g_CollisionMgr.RaySetup( NULL_GUID, Pos[i] + CollRay, Pos[i] - CollRay );
                g_CollisionMgr.StopOnFirstCollisionFound();
                g_CollisionMgr.CheckCollisions( object::TYPE_PLAY_SURFACE,
                                                object::ATTR_COLLIDABLE,
                                                object::ATTR_COLLISION_PERMEABLE );

                // If no collision then we are hanging off an edge.
                if( g_CollisionMgr.m_nCollisions==0 )
                    break;

                // Check if the collision location is close enough to the plane
                //if( Plane.Distance(g_CollisionMgr.m_Collisions[0].Point) > -kCoplanarDistanceCheck )
                //    break;

                // The coll check verifies the point is good so we don't need to bother
                // testing the other edges.
                bCollOK = TRUE;
                break;
            }
        }

        // Here are the cases
        // (j==3) The point was inside all the edges (OK)
        // (j<3) && (bCollOK==TRUE) We were forced to do a ColCheck but it came out positive so we quit early (OK)
        // (j<3) && (bCollOK==FALSE) We were forced to do a ColCheck but it came out negative so we quit early (BAD)

        // If we quit the (j) loop early and we never testthen a point must have failed
        if( (j<3) && (bCollOK==FALSE) )
            break;
    }

    // If we quit the (i) loop early then a point must have failed and we can't keep 
    // this decal.
    if( i<3 )
        return;
    
    //
    // Add the three vertices to the bank of decals
    //
    {
        s32 iDecalStart = GetDecalStart( Def.m_Handle, 3 );
        if( iDecalStart != -1 )
        {
            registration_info& RegInfo = m_RegisteredDefs(Def.m_Handle);
            
            position_data*  pPos   = &RegInfo.m_pPositions[iDecalStart];
            uv_data*        pUV    = &RegInfo.m_pUVs[iDecalStart];
            u32*            pColor = &RegInfo.m_pColors[iDecalStart];

            #ifdef X_EDITOR
            u32 Color = RegInfo.m_Color;
            #else
            u32 Color = (RegInfo.m_Color.R<< 0) | (RegInfo.m_Color.G<< 8) |
                        (RegInfo.m_Color.B<<16) | (RegInfo.m_Color.A<<24);
            #endif

            pPos[0].Pos     = Pos[0];
            pPos[1].Pos     = Pos[1];
            pPos[2].Pos     = Pos[2];
            pPos[0].Flags   = decal_vert::FLAG_SKIP_TRIANGLE | decal_vert::FLAG_DECAL_START;
            pPos[1].Flags   = decal_vert::FLAG_SKIP_TRIANGLE;
            pPos[2].Flags   = 0;
            pColor[0]       = Color;
            pColor[1]       = Color;
            pColor[2]       = Color;
            pUV[0]          = m_TriangleTemplateUV[0];
            pUV[1]          = m_TriangleTemplateUV[1];
            pUV[2]          = m_TriangleTemplateUV[2];

            BuildDynamicSpans( RegInfo, iDecalStart, 3 );
        }
    }
}

//==============================================================================

void decal_mgr::CreateDecalAtPoint( const decal_definition& Def,
                                    const vector3&          Point,
                                    const vector3&          Normal,
                                    const vector2&          Size,
                                    radian                  Roll )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::CreateDecalAtPoint" );

    matrix4     L2W;
    decal_vert  DecalVerts[MAX_VERTS_PER_DECAL];

    s32 nVerts = CalcDecalVerts( Def.m_Flags,
                                 Point,
                                 Normal,
                                 Normal,
                                 Size,
                                 Roll,
                                 DecalVerts,
                                 L2W );
    if ( nVerts )
    {
        AddDecal( Def.m_Handle, nVerts, DecalVerts, L2W );
    }
}

//==============================================================================

xhandle decal_mgr::RegisterDefinition( decal_definition& Def )
{
    // make sure this guy is not already registered
    if ( Def.m_Handle.IsNonNull() )
    {
        ASSERTS( FALSE, "Attempt to register a decal multiple times!?!?" );
        registration_info& RegInfo = m_RegisteredDefs( Def.m_Handle );
        RegInfo.Kill();
        m_RegisteredDefs.DeleteByHandle( Def.m_Handle );
        Def.m_Handle = HNULL;
    }

    // create a new registration for this decal, and fill in the data
    registration_info& RegInfo = m_RegisteredDefs.Add( Def.m_Handle );
    RegInfo.m_BlendMode = Def.m_BlendMode;
    RegInfo.m_Flags     = Def.m_Flags;
    RegInfo.m_Color     = Def.m_Color;
    RegInfo.m_FadeoutTime = Def.m_FadeTime;

    // No paths allowed!
    char* p = &Def.m_BitmapName[x_strlen(Def.m_BitmapName)];
    while( (p > Def.m_BitmapName) && (*(p-1) != '\\') && (*(p-1) != '/') )
        p--;
    RegInfo.m_bitmap.SetName( p );

    // how much vert space should we allocate? make sure to align it to the
    // hardware buffer size
    s32 HWBufferSize  = render::GetHardwareBufferSize();
    s32 nVertsToAlloc = 0;
    if ( Def.m_Flags & decal_definition::DECAL_FLAG_USE_TRI )
        nVertsToAlloc = Def.m_MaxVisible * 3;
    else
        nVertsToAlloc = Def.m_MaxVisible * 4;
    if ( nVertsToAlloc % HWBufferSize )
    {
        nVertsToAlloc  = (nVertsToAlloc/HWBufferSize)*HWBufferSize;
        nVertsToAlloc += HWBufferSize;
    }

    // allocate vert space
    RegInfo.AllocVertList( nVertsToAlloc );
    ReserveRenderCapacity();

    return Def.m_Handle;
}

//==============================================================================

void decal_mgr::UnregisterDefinition( decal_definition& Def )
{
    registration_info& RegInfo = m_RegisteredDefs( Def.m_Handle );
    RegInfo.Kill();
    
    m_RegisteredDefs.DeleteByHandle( Def.m_Handle );
    ReserveRenderCapacity();
}

//==============================================================================

s32 decal_mgr::CalcNoClipDecal( s32            Flags,
                                const vector3& Point,
                                const vector3& Normal,
                                const vector2& Size,
                                radian         Roll,
                                decal_vert     Verts[MAX_VERTS_PER_DECAL],
                                matrix4&       L2W )
{
    s32 nVerts;

    // set up the verts
    if ( Flags & decal_definition::DECAL_FLAG_USE_TRI )
    {
        // Imagine a circle with radius size. Then fit an equilateral triangle
        // around that circle. That is our triangle. Note that some texture
        // will have to be clamped on the bottom and top.
        f32 VerticalRadius = 0.5f * Size.Y;
        
        Verts[0].Pos.Set(  0.0f,            Size.Y,          0.0f );
        Verts[1].Pos.Set(  0.866f * Size.X, -VerticalRadius, 0.0f );
        Verts[2].Pos.Set( -0.866f * Size.X, -VerticalRadius, 0.0f );

        Verts[0].UV.Set( 0.5f,        -0.5f );
        Verts[1].UV.Set( 0.5f-0.866f,  1.0f );
        Verts[2].UV.Set( 0.5f+0.866f,  1.0f );

        Verts[0].Flags   = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[1].Flags   = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[2].Flags   = 0;

        nVerts = 3;
    }
    else
    {
        // set up a quad strip that matches the size exactly
        Verts[0].Pos.Set( -Size.X*0.5f,  Size.Y*0.5f, 0.0f );
        Verts[1].Pos.Set(  Size.X*0.5f,  Size.Y*0.5f, 0.0f );
        Verts[2].Pos.Set( -Size.X*0.5f, -Size.Y*0.5f, 0.0f );
        Verts[3].Pos.Set(  Size.X*0.5f, -Size.Y*0.5f, 0.0f );

        Verts[0].UV.Set( 1.0f, 0.0f );
        Verts[1].UV.Set( 0.0f, 0.0f );
        Verts[2].UV.Set( 1.0f, 1.0f );
        Verts[3].UV.Set( 0.0f, 1.0f );

        Verts[0].Flags   = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[1].Flags   = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[2].Flags   = 0;
        Verts[3].Flags   = 0;

        nVerts = 4;
    }

    vector3 Collisions[MAX_VERTS_PER_DECAL];
    vector3 CollisionRay = Normal * kCoplanarDistanceCheck;

    // set up the transform matrix
    radian3 Orient( Normal.GetPitch(), Normal.GetYaw(), Roll );
    L2W.Setup( vector3(1.0f,1.0f,1.0f), Orient, Point );

    // grab the collision points for all of the verts
    s32 i;
    ASSERT( nVerts >= 3 );
    for ( i = 0; i < nVerts; i++ )
    {
        vector3 TransPt = L2W * Verts[i].Pos;
        
        g_CollisionMgr.RaySetup( NULL_GUID, vector3(TransPt-CollisionRay), vector3(TransPt+CollisionRay) );
        g_CollisionMgr.CheckCollisions( object::TYPE_PLAY_SURFACE,
                                        object::ATTR_COLLIDABLE,
                                        object::ATTR_COLLISION_PERMEABLE );

        // if there wasn't a collision, then it's no longer a simple problem
        if ( g_CollisionMgr.m_nCollisions == 0 )
            return 0;

        Collisions[i] = g_CollisionMgr.m_Collisions[0].Point;
    }

    // figure out what plane these guys should be in
    plane DecalPlane;
    DecalPlane.Setup( Point, Normal );

    // make sure all of the points are within the plane
    for ( i = 0; i < nVerts; i++ )
    {
        if ( x_abs(DecalPlane.Distance(Collisions[i])) > kCoplanarDistanceCheck )
        {
            return 0;
        }
    }

    // success
    return nVerts;
}

//==============================================================================

s32 decal_mgr::CalcProjectedDecal( const vector3&    Point,
                                   const vector3&    SurfaceNormal,
                                   const vector3&    NegIncomingRay,
                                   const vector2&    Size,
                                   radian            Roll,
                                   decal_vert        Verts[MAX_VERTS_PER_DECAL],
                                   matrix4&          L2W )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::CalcProjectedDecal" );

    ////////////////////////////////////////////////////////////////////////////
    // Time for some ascii art fun...
    //
    //  ---------------
    //         /|
    //        /t|
    //       /  |  
    //      /   |   
    //     L    |
    //          V
    //
    //  Let's imagine that the arrow pointing down is the surface normal, and
    //  the arrow pointing away is the negative incoming ray. Both vectors are
    //  normalized.
    //
    //  What we want to do is build an orthogonal projection matrix that takes
    //  the points on the surface into "decal" space. The width and height for
    //  that matrix will just come from the decal definition, and then we will
    //  calculate the intersection points with the surface to figure out the
    //  near and far planes of the projection.
    //
    //  Once we have the orthogonal projection, we'll collect all of the
    //  triangles nearby, project and clip them, and generate uv's. If we have
    //  complex geometry (particular a spike sticking up from the surface), it
    //  will be possible to see where the decal clipped. This should be a rare
    //  case, and if it becomes a problem, we'll have to solve it separately.
    //
    ////////////////////////////////////////////////////////////////////////////

    plane   SurfacePlane;
    radian  theta         = x_acos( NegIncomingRay.Dot( SurfaceNormal ) );
    vector3 ProjectionRay = NegIncomingRay;
    SurfacePlane.Setup( Point, SurfaceNormal );
    
    ////////////////////////////////////////////////////////////////////////////
    // Don't let the angle we hit the surface be too shallow, cause that will
    // just cause too much stretchiness.
    ////////////////////////////////////////////////////////////////////////////

    // Incoming vector is parallel with the surface or behind the surface.
    // This should be an extremely rare case, and if it ever happens, just
    // throw away the decal
    if ( theta > R_89 )
    {
        return 0;
    }

    if ( theta >= (R_90-kIncomingAngleMinimum) )
    {
        vector3 Parallel;
        vector3 Perpendicular;

        // Break down the incoming ray into it's parallel and perpendicular
        // components. Then scale the parallel and perpendicular components
        // based on what we want the incoming angle to be. Add the two
        // components together to get the new projection that is not quite
        // as shallow.
        Parallel      = NegIncomingRay - (NegIncomingRay.Dot(SurfacePlane.Normal)*SurfacePlane.Normal);
        Parallel.Normalize();
        Perpendicular = SurfacePlane.Normal;
        ProjectionRay = Perpendicular * x_sin(kIncomingAngleMinimum) +
                        Parallel      * x_cos(kIncomingAngleMinimum);

        // If we've done the math correctly, we should've ended up with a
        // normalized vector
        ASSERT( ProjectionRay.Length() >= (1.0f-kEpsilon) &&
                ProjectionRay.Length() <= (1.0f+kEpsilon) );
        theta = kIncomingAngleMinimum;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Figure out what the "near" and "far points of the matrix should be. We'll
    // cast 4 rays along the projection and see where the min and max T's are.
    ////////////////////////////////////////////////////////////////////////////

    // grab the initial for corners of the decal
    ProjectionRay.Negate(); // (now we're casting into the surface rather than away from it)
    vector3 DecalCorners[4];
    vector2 HalfDimensions = Size*0.5f;
    DecalCorners[0].Set(  HalfDimensions.X,  HalfDimensions.Y, 0.0f );
    DecalCorners[1].Set( -HalfDimensions.X,  HalfDimensions.Y, 0.0f );
    DecalCorners[2].Set( -HalfDimensions.X, -HalfDimensions.Y, 0.0f );
    DecalCorners[3].Set(  HalfDimensions.X, -HalfDimensions.Y, 0.0f );

    // rotate them into "decal" space, and see where the decal corners intersect
    // with the plane when being cast down the projection ray
    s32 i;
    f32 Dist;
    f32 MinDist = 0.0f;
    f32 MaxDist = 0.0f;
    L2W.Setup( vector3(1.0f,1.0f,1.0f),
               radian3(ProjectionRay.GetPitch(), ProjectionRay.GetYaw(), Roll),
               Point );
    f32 RayDotNormal = ProjectionRay.Dot( SurfacePlane.Normal );
    f32 AbsDot = x_abs(RayDotNormal);
    for ( i = 0; i < 4; i++ )
    {
        DecalCorners[i] = L2W * DecalCorners[i];

        if ( AbsDot >= kEpsilon )
        {
            Dist = -SurfacePlane.Distance(DecalCorners[i]) / RayDotNormal;

            if ( Dist < MinDist )
                MinDist = Dist;
            if ( Dist > MaxDist )
                MaxDist = Dist;
        }
    }

    // fudge the min and max a few centimeters to avoid any floating-point issues
    MinDist -= 5.0f;
    MaxDist += 5.0f;


    if ( 0 )
    {
        // To debug the incoming projection, just enable this code and it will
        // build a non-clipped decal that is oriented like you'd expect. Just
        // imagine an orthogonal viewing projection down the z-axis, and the
        // mapping should work out the same.
        Verts[0].Pos.Set( -Size.X*0.5f,  Size.Y*0.5f, 0.0f );
        Verts[1].Pos.Set(  Size.X*0.5f,  Size.Y*0.5f, 0.0f );
        Verts[2].Pos.Set( -Size.X*0.5f, -Size.Y*0.5f, 0.0f );
        Verts[3].Pos.Set(  Size.X*0.5f, -Size.Y*0.5f, 0.0f );
        
        Verts[0].UV.Set( 1.0f, 0.0f );
        Verts[1].UV.Set( 0.0f, 0.0f );
        Verts[2].UV.Set( 1.0f, 1.0f );
        Verts[3].UV.Set( 0.0f, 1.0f );
        
        Verts[0].Flags   = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[1].Flags   = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[2].Flags   = 0;
        Verts[3].Flags   = 0;
        
        return 4;
    }

    // Now we have some "near" and "far" planes...because we're not doing
    // perspective-correct or any craziness like that these values are allowed
    // to span across zero so we won't have to adjust our L2W or anything.
    // This will be nice if someone manually slides a decal along a wall after
    // it's been placed (as long as they are only sliding across a coplanar
    // surface, that is).
    matrix4 WorldToDecal = L2W;
    WorldToDecal.InvertSRT();
    matrix4 WorldToClip;
    WorldToClip.Identity();
    WorldToClip(0,0) = 2.0f / Size.X;
    WorldToClip(1,1) = 2.0f / Size.Y;
    WorldToClip(2,2) = 2.0f / (MaxDist-MinDist);
    WorldToClip(3,2) = ((-2.0f*MinDist) / (MaxDist-MinDist)) - 1.0f;
    WorldToClip = WorldToClip * WorldToDecal;

    if ( 0 )
    {
        // enable this code to see what the clipping box looks like
        Verts[ 0].Pos.Set( -Size.X/2.0f,  Size.Y/2.0f, MinDist );
        Verts[ 1].Pos.Set(  Size.X/2.0f,  Size.Y/2.0f, MinDist );
        Verts[ 2].Pos.Set( -Size.X/2.0f, -Size.Y/2.0f, MinDist );
        Verts[ 3].Pos.Set(  Size.X/2.0f, -Size.Y/2.0f, MinDist );
        Verts[ 4].Pos.Set( -Size.X/2.0f, -Size.Y/2.0f, MaxDist );
        Verts[ 5].Pos.Set(  Size.X/2.0f, -Size.Y/2.0f, MaxDist );
        Verts[ 6].Pos.Set( -Size.X/2.0f,  Size.Y/2.0f, MaxDist );
        Verts[ 7].Pos.Set(  Size.X/2.0f,  Size.Y/2.0f, MaxDist );
        Verts[ 8].Pos.Set( -Size.X/2.0f,  Size.Y/2.0f, MinDist );
        Verts[ 9].Pos.Set(  Size.X/2.0f,  Size.Y/2.0f, MinDist );
        Verts[10].Pos.Set( -Size.X/2.0f,  Size.Y/2.0f, MaxDist );
        Verts[11].Pos.Set( -Size.X/2.0f,  Size.Y/2.0f, MinDist );
        Verts[12].Pos.Set( -Size.X/2.0f, -Size.Y/2.0f, MaxDist );
        Verts[13].Pos.Set( -Size.X/2.0f, -Size.Y/2.0f, MinDist );
        Verts[14].Pos.Set(  Size.X/2.0f,  Size.Y/2.0f, MinDist );
        Verts[15].Pos.Set(  Size.X/2.0f,  Size.Y/2.0f, MaxDist );
        Verts[16].Pos.Set(  Size.X/2.0f, -Size.Y/2.0f, MinDist );
        Verts[17].Pos.Set(  Size.X/2.0f, -Size.Y/2.0f, MaxDist );
        for ( s32 iVert = 0; iVert < 18; iVert++ )
        {
            Verts[iVert].UV.Set(0.0f, 0.0f);
            Verts[iVert].Flags = 0;
        }
        Verts[ 0].Flags = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[ 1].Flags = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[10].Flags = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[11].Flags = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[14].Flags = decal_vert::FLAG_SKIP_TRIANGLE;
        Verts[15].Flags = decal_vert::FLAG_SKIP_TRIANGLE;

        return 18;
    }
    else
    {
        ////////////////////////////////////////////////////////////////////////////
        // Clip all nearby triangles to the clip volume we've just defined, and
        // then move them back into world space, simultaneously generating UV's.
        ////////////////////////////////////////////////////////////////////////////

        // build a bbox around our orthogonal projection, which we'll use to collect
        // triangles to clip
        bbox WorldBBox;
        WorldBBox.Clear();
        for ( i = 0; i < 4; i++ )
        {
            vector3 BBoxVerts[2];
            BBoxVerts[0] = DecalCorners[i] + MinDist * ProjectionRay;
            BBoxVerts[1] = DecalCorners[i] + MaxDist * ProjectionRay;
            WorldBBox.AddVerts( BBoxVerts, 2 );
        }

        // inflate the bbox slightly if any one dimension is too small
        vector3 Inflate(0.0f,0.0f,0.0f);
        vector3 BBoxSize = WorldBBox.GetSize();
        if ( BBoxSize.GetX() < kEpsilon )
            Inflate.GetX() = kEpsilon;
        if ( BBoxSize.GetY() < kEpsilon )
            Inflate.GetY() = kEpsilon;
        if ( BBoxSize.GetZ() < kEpsilon )
            Inflate.GetZ() = kEpsilon;
        WorldBBox.Min -= Inflate;
        WorldBBox.Max += Inflate;

        // allocate enough workspace data to handle an extremely clipped decal
        smem_StackPushMarker();
        working_data* pWorkingData = (working_data*)smem_StackAlloc(sizeof(working_data));

        // build the vertices in our decal clip space
        CalcDecalVertsFromVolume( WorldBBox, WorldToClip, ProjectionRay, *pWorkingData );

        // transform the decal verts back to world space
        if ( 0 )
        {
            // DEBUGGING CODE! DON'T CHECK IN!!!!
            matrix4 ClipToWorld = WorldToClip;
            ClipToWorld.Invert();
            s32 blah;
            for ( blah = 0; blah < pWorkingData->nVerts; blah++ )
            {
                pWorkingData->ClippedVerts[blah] = ClipToWorld*pWorkingData->ClippedVerts[blah];
            }
        }

        // reduce the number of clipped polygons to a reasonable amount by
        // recombining the coplanar polygons and re-triangulating
        CombineCoplanarPolys( *pWorkingData );

        // transform the decal verts back to world space, and store them out in the proper
        // format (simultaneously generating UV's)
        s32 nVerts = pWorkingData->nFinalVerts;
        if ( nVerts )
        {
            // transform the decal verts back to local space, and store them out in the
            // proper format
            matrix4 W2L = L2W;
            W2L.InvertSRT();
            matrix4 ClipToWorld = WorldToClip;
            ClipToWorld.Invert();
            ClipToWorld = W2L*ClipToWorld;
            for ( i = 0; i < nVerts; i++ )
            {
                Verts[i].UV.X  = -0.5f * pWorkingData->ClippedVerts[i].GetX() + 0.5f;
                Verts[i].UV.Y  = -0.5f * pWorkingData->ClippedVerts[i].GetY() + 0.5f;
                Verts[i].Pos   = ClipToWorld * pWorkingData->ClippedVerts[i];
                Verts[i].Flags = pWorkingData->FinalVertFlags[i];
            }
        }

        // clean up
        smem_StackPopToMarker();

        return nVerts;
    }
}

//==============================================================================

void decal_mgr::CalcDecalVertsFromVolume( const bbox&    WorldBBox,
                                          const matrix4& OrthoProjection,
                                          const vector3& ProjectionRay,
                                          working_data&  WD )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::CalcDecalVertsFromVolume" );

    WD.nVerts = 0;

    // collect all of the rigid surface triangles we could possibly clip against
    #ifdef X_EDITOR

    // for the editor, we need to walk the play surface objects
    g_ObjMgr.SelectBBox( object::ATTR_COLLIDABLE, WorldBBox, object::TYPE_PLAY_SURFACE );
    slot_id ID;
    for ( ID = g_ObjMgr.StartLoop(); ID != SLOT_NULL; ID = g_ObjMgr.GetNextResult(ID) )
    {
        ASSERT( g_ObjMgr.GetObjectBySlot(ID) );

        // grab the information needed by the volume clipper
        play_surface&       Surface    = play_surface::GetSafeType( *g_ObjMgr.GetObjectBySlot(ID) );
        rigid_inst&         RigidInst  = Surface.GetRigidInst();
        render::GeometryInstanceHandle  hGeomInst  = RigidInst.GetInst();
        const rigid_geom*   pRigidGeom = (const rigid_geom*)render::GetGeom( hGeomInst );

        if ( pRigidGeom && (pRigidGeom->m_Collision.nHighClusters!=0) )
        {
            AddGeometryToDecal( pRigidGeom, Surface.GetL2W(), OrthoProjection, ProjectionRay, WD );
        }
    }
    g_ObjMgr.EndLoop();
    
    #else

    // for an ingame level we need to query the playsurface_mgr
    g_PlaySurfaceMgr.CollectSurfaces( WorldBBox, object::ATTR_COLLIDABLE, 0 );
    playsurface_mgr::surface* pSurface = g_PlaySurfaceMgr.GetNextSurface();
    while ( pSurface != NULL )
    {
        render::GeometryInstanceHandle hGeomInst  = pSurface->RenderInst;
        const rigid_geom*  pRigidGeom = (const rigid_geom*)render::GetGeom( hGeomInst );

        if ( pRigidGeom && (pRigidGeom->m_Collision.nHighClusters!=0) )
        {
            AddGeometryToDecal( pRigidGeom, pSurface->L2W, OrthoProjection, ProjectionRay, WD );
        }

        pSurface = g_PlaySurfaceMgr.GetNextSurface();
    }
    
    #endif
}

//==============================================================================

static
s32 ClipNegXVerts( vector3* pDst, const vector3* pSrc, s32 NSrcVerts )
{
    s32   CurrVert;
    xbool bCurrIn;
    s32   NextVert = NSrcVerts-1;
    xbool bNextIn  = pSrc[NextVert].GetX() >= (-1.0f-kEpsilon);
    s32   NDst     = 0;

    for ( s32 i = 0; i < NSrcVerts; i++ )
    {
        CurrVert  = NextVert;
        bCurrIn   = bNextIn;
        NextVert  = i;
        bNextIn   = (pSrc[NextVert].GetX() >= (-1.0f-kEpsilon));
        
        if ( bCurrIn )
        {
            pDst[NDst++] = pSrc[CurrVert];
        }

        if ( bCurrIn != bNextIn )
        {
            vector3 V( pSrc[NextVert] - pSrc[CurrVert] );
            f32 Denom = V.GetX();
            if ( x_abs(Denom) < 0.0001f )
                Denom = 0.0001f;
            f32 T = (-1.0f - pSrc[CurrVert].GetX()) / Denom;
            pDst[NDst++] = pSrc[CurrVert] + T*V;
        }
    }

    return NDst;
}

//==============================================================================

static
s32 ClipPosXVerts( vector3* pDst, const vector3* pSrc, s32 NSrcVerts )
{
    s32   CurrVert;
    xbool bCurrIn;
    s32   NextVert = NSrcVerts-1;
    xbool bNextIn  = pSrc[NextVert].GetX() <= (1.0f+kEpsilon);
    s32   NDst     = 0;

    for ( s32 i = 0; i < NSrcVerts; i++ )
    {
        CurrVert  = NextVert;
        bCurrIn   = bNextIn;
        NextVert  = i;
        bNextIn   = (pSrc[NextVert].GetX() <= (1.0f+kEpsilon));
        
        if ( bCurrIn )
        {
            pDst[NDst++] = pSrc[CurrVert];
        }

        if ( bCurrIn != bNextIn )
        {
            vector3 V( pSrc[NextVert] - pSrc[CurrVert] );
            f32 Denom = V.GetX();
            if ( x_abs(Denom) < 0.0001f )
                Denom = 0.0001f;
            f32 T = (1.0f - pSrc[CurrVert].GetX()) / Denom;
            pDst[NDst++] = pSrc[CurrVert] + T*V;
        }
    }

    return NDst;
}

//==============================================================================

static
s32 ClipNegYVerts( vector3* pDst, const vector3* pSrc, s32 NSrcVerts )
{
    s32   CurrVert;
    xbool bCurrIn;
    s32   NextVert = NSrcVerts-1;
    xbool bNextIn  = pSrc[NextVert].GetY() >= (-1.0f-kEpsilon);
    s32   NDst     = 0;

    for ( s32 i = 0; i < NSrcVerts; i++ )
    {
        CurrVert  = NextVert;
        bCurrIn   = bNextIn;
        NextVert  = i;
        bNextIn   = (pSrc[NextVert].GetY() >= (-1.0f-kEpsilon));
        
        if ( bCurrIn )
        {
            pDst[NDst++] = pSrc[CurrVert];
        }

        if ( bCurrIn != bNextIn )
        {
            vector3 V( pSrc[NextVert] - pSrc[CurrVert] );
            f32 Denom = V.GetY();
            if ( x_abs(Denom) < 0.0001f )
                Denom = 0.0001f;
            f32 T = (-1.0f - pSrc[CurrVert].GetY()) / Denom;
            pDst[NDst++] = pSrc[CurrVert] + T*V;
        }
    }

    return NDst;
}

//==============================================================================

static
s32 ClipPosYVerts( vector3* pDst, const vector3* pSrc, s32 NSrcVerts )
{
    s32   CurrVert;
    xbool bCurrIn;
    s32   NextVert = NSrcVerts-1;
    xbool bNextIn  = pSrc[NextVert].GetY() <= (1.0f+kEpsilon);
    s32   NDst     = 0;

    for ( s32 i = 0; i < NSrcVerts; i++ )
    {
        CurrVert  = NextVert;
        bCurrIn   = bNextIn;
        NextVert  = i;
        bNextIn   = (pSrc[NextVert].GetY() <= (1.0f+kEpsilon));
        
        if ( bCurrIn )
        {
            pDst[NDst++] = pSrc[CurrVert];
        }

        if ( bCurrIn != bNextIn )
        {
            vector3 V( pSrc[NextVert] - pSrc[CurrVert] );
            f32 Denom = V.GetY();
            if ( x_abs(Denom) < 0.0001f )
                Denom = 0.0001f;
            f32 T = (1.0f - pSrc[CurrVert].GetY()) / Denom;
            pDst[NDst++] = pSrc[CurrVert] + T*V;
        }
    }

    return NDst;
}

//==============================================================================

static
s32 ClipNegZVerts( vector3* pDst, const vector3* pSrc, s32 NSrcVerts )
{
    s32   CurrVert;
    xbool bCurrIn;
    s32   NextVert = NSrcVerts-1;
    xbool bNextIn  = pSrc[NextVert].GetZ() >= (-1.0f-kEpsilon);
    s32   NDst     = 0;

    for ( s32 i = 0; i < NSrcVerts; i++ )
    {
        CurrVert  = NextVert;
        bCurrIn   = bNextIn;
        NextVert  = i;
        bNextIn   = (pSrc[NextVert].GetZ() >= (-1.0f-kEpsilon));
        
        if ( bCurrIn )
        {
            pDst[NDst++] = pSrc[CurrVert];
        }

        if ( bCurrIn != bNextIn )
        {
            vector3 V( pSrc[NextVert] - pSrc[CurrVert] );
            f32 Denom = V.GetZ();
            if ( x_abs(Denom) < 0.0001f )
                Denom = 0.0001f;
            f32 T = (-1.0f - pSrc[CurrVert].GetZ()) / Denom;
            pDst[NDst++] = pSrc[CurrVert] + T*V;
        }
    }

    return NDst;
}

//==============================================================================

static
s32 ClipPosZVerts( vector3* pDst, const vector3* pSrc, s32 NSrcVerts )
{
    s32   CurrVert;
    xbool bCurrIn;
    s32   NextVert = NSrcVerts-1;
    xbool bNextIn  = pSrc[NextVert].GetZ() <= (1.0f+kEpsilon);
    s32   NDst     = 0;

    for ( s32 i = 0; i < NSrcVerts; i++ )
    {
        CurrVert  = NextVert;
        bCurrIn   = bNextIn;
        NextVert  = i;
        bNextIn   = (pSrc[NextVert].GetZ() <= (1.0f+kEpsilon));
        
        if ( bCurrIn )
        {
            pDst[NDst++] = pSrc[CurrVert];
        }

        if ( bCurrIn != bNextIn )
        {
            vector3 V( pSrc[NextVert] - pSrc[CurrVert] );
            f32 Denom = V.GetZ();
            if ( x_abs(Denom) < 0.0001f )
                Denom = 0.0001f;
            f32 T = (1.0f - pSrc[CurrVert].GetZ()) / Denom;
            pDst[NDst++] = pSrc[CurrVert] + T*V;
        }
    }

    return NDst;
}

//==============================================================================


void decal_mgr::AddGeometryToDecal( const rigid_geom* pRigidGeom,
                                    const matrix4&    GeomL2W,
                                    const matrix4&    OrthoProjection,
                                    const vector3&    ProjectionRay,
                                    working_data&     WD )
{
    ASSERT( pRigidGeom );

    vector3 ClipVerts[2][10];   // a clipped tri has at most 9 verts, plus duplicate the first for simplicity
    s32     Buff = 0;
    
    for ( s32 iCluster = 0; iCluster < pRigidGeom->m_Collision.nHighClusters; iCluster++ )
    {
        for ( s32 iTri = 0; iTri < pRigidGeom->m_Collision.pHighCluster[iCluster].nTris; iTri++ )
        {
            s32 Key = (iCluster<<16) | iTri;
            
            // get the triangle and transform it to world space
            pRigidGeom->GetGeoTri( Key, ClipVerts[Buff][0], ClipVerts[Buff][1], ClipVerts[Buff][2] );
            ClipVerts[Buff][0] = GeomL2W * ClipVerts[Buff][0];
            ClipVerts[Buff][1] = GeomL2W * ClipVerts[Buff][1];
            ClipVerts[Buff][2] = GeomL2W * ClipVerts[Buff][2];

            // do backface culling
            vector3 TriN = GetTriNormal( ClipVerts[Buff][0], ClipVerts[Buff][1], ClipVerts[Buff][2], FALSE );
            if ( TriN.Dot( ProjectionRay ) > -kEpsilon )
                continue;

            // move the triangle into decal clip space
            ClipVerts[Buff][0] = OrthoProjection * ClipVerts[Buff][0];
            ClipVerts[Buff][1] = OrthoProjection * ClipVerts[Buff][1];
            ClipVerts[Buff][2] = OrthoProjection * ClipVerts[Buff][2];

            // accumulate clipping bits
            u32 ClipBits = 0;
            if ( ClipVerts[Buff][0].GetX() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_X <<  0);
            if ( ClipVerts[Buff][0].GetX() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_X <<  0);
            if ( ClipVerts[Buff][0].GetY() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_Y <<  0);
            if ( ClipVerts[Buff][0].GetY() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_Y <<  0);
            if ( ClipVerts[Buff][0].GetZ() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_Z <<  0);
            if ( ClipVerts[Buff][0].GetZ() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_Z <<  0);
            if ( ClipVerts[Buff][1].GetX() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_X <<  6);
            if ( ClipVerts[Buff][1].GetX() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_X <<  6);
            if ( ClipVerts[Buff][1].GetY() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_Y <<  6);
            if ( ClipVerts[Buff][1].GetY() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_Y <<  6);
            if ( ClipVerts[Buff][1].GetZ() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_Z <<  6);
            if ( ClipVerts[Buff][1].GetZ() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_Z <<  6);
            if ( ClipVerts[Buff][2].GetX() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_X << 12);
            if ( ClipVerts[Buff][2].GetX() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_X << 12);
            if ( ClipVerts[Buff][2].GetY() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_Y << 12);
            if ( ClipVerts[Buff][2].GetY() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_Y << 12);
            if ( ClipVerts[Buff][2].GetZ() < -1.0f ) ClipBits |= (CLIP_DECAL_VERT_NEG_Z << 12);
            if ( ClipVerts[Buff][2].GetZ() >  1.0f ) ClipBits |= (CLIP_DECAL_VERT_POS_Z << 12);

            // trivial rejection
            if ( (ClipBits & CLIP_DECAL_TRI_NEG_X) == CLIP_DECAL_TRI_NEG_X )    continue;
            if ( (ClipBits & CLIP_DECAL_TRI_POS_X) == CLIP_DECAL_TRI_POS_X )    continue;
            if ( (ClipBits & CLIP_DECAL_TRI_NEG_Y) == CLIP_DECAL_TRI_NEG_Y )    continue;
            if ( (ClipBits & CLIP_DECAL_TRI_POS_Y) == CLIP_DECAL_TRI_POS_Y )    continue;
            if ( (ClipBits & CLIP_DECAL_TRI_NEG_Z) == CLIP_DECAL_TRI_NEG_Z )    continue;
            if ( (ClipBits & CLIP_DECAL_TRI_POS_Z) == CLIP_DECAL_TRI_POS_Z )    continue;

            // clip it to each plane if necessary
            s32 nClippedVerts = 3;
            if ( ClipBits )
            {
                if ( ClipBits & CLIP_DECAL_TRI_NEG_X )
                {
                    s32 NewBuff = Buff ^ 1;
                    nClippedVerts = ClipNegXVerts( ClipVerts[NewBuff], ClipVerts[Buff], nClippedVerts );
                    Buff = NewBuff;
                    if ( nClippedVerts == 0 )
                        continue;
                }

                if ( ClipBits & CLIP_DECAL_TRI_POS_X )
                {
                    s32 NewBuff = Buff ^ 1;
                    nClippedVerts = ClipPosXVerts( ClipVerts[NewBuff], ClipVerts[Buff], nClippedVerts );
                    Buff = NewBuff;
                    if ( nClippedVerts == 0 )
                        continue;
                }

                if ( ClipBits & CLIP_DECAL_TRI_NEG_Y )
                {
                    s32 NewBuff = Buff ^ 1;
                    nClippedVerts = ClipNegYVerts( ClipVerts[NewBuff], ClipVerts[Buff], nClippedVerts );
                    Buff = NewBuff;
                    if ( nClippedVerts == 0 )
                        continue;
                }

                if ( ClipBits & CLIP_DECAL_TRI_POS_Y )
                {
                    s32 NewBuff = Buff ^ 1;
                    nClippedVerts = ClipPosYVerts( ClipVerts[NewBuff], ClipVerts[Buff], nClippedVerts );
                    Buff = NewBuff;
                    if ( nClippedVerts == 0 )
                        continue;
                }

                if ( ClipBits & CLIP_DECAL_TRI_NEG_Z )
                {
                    s32 NewBuff = Buff ^ 1;
                    nClippedVerts = ClipNegZVerts( ClipVerts[NewBuff], ClipVerts[Buff], nClippedVerts );
                    Buff = NewBuff;
                    if ( nClippedVerts == 0 )
                        continue;
                }

                if ( ClipBits & CLIP_DECAL_TRI_POS_Z )
                {
                    s32 NewBuff = Buff ^ 1;
                    nClippedVerts = ClipPosZVerts( ClipVerts[NewBuff], ClipVerts[Buff], nClippedVerts );
                    Buff = NewBuff;
                    if ( nClippedVerts == 0 )
                        continue;
                }
            }

            // will this strip fit into the array?
            if ( (nClippedVerts+WD.nVerts) > working_data::MAX_WORKING_VERTS )
                continue;

            // copy the clipped verts into our working array
            ASSERT( nClippedVerts >= 3 );
            s32 iClipVert;
            for ( iClipVert = 0; iClipVert < nClippedVerts; iClipVert++ )
            {
                WD.ClippedVerts[WD.nVerts+iClipVert] = ClipVerts[Buff][iClipVert];
                WD.VertFlags[WD.nVerts+iClipVert]    = 0;
            }

            // set the new poly flag on the first two verts
            WD.VertFlags[WD.nVerts+0] = working_data::FLAG_POLY_START;
            WD.VertFlags[WD.nVerts+1] = working_data::FLAG_POLY_START;

            // bring on the next tri
            WD.nVerts += nClippedVerts;
        }
    }
}

//==============================================================================

void decal_mgr::CreateIndexedVertPool( working_data& WD )
{
    s32 i, j;
    
    // grab a couple of pointers to make life a little easier
    vector3* pSrcVerts = WD.ClippedVerts;
    vector3* pDstVerts = WD.IndexedVertPool;

    WD.nIndexedVerts = 0;
    for ( i = 0; i < WD.nVerts; i++ )
    {
        // look for the current vert in our pool (giving a one-centimeter leeway)
        for ( j = 0; j < WD.nIndexedVerts; j++ )
        {
            if ( (x_abs(pSrcVerts[i].GetX() - pDstVerts[j].GetX()) < kEpsilon) &&
                 (x_abs(pSrcVerts[i].GetY() - pDstVerts[j].GetY()) < kEpsilon) &&
                 (x_abs(pSrcVerts[i].GetZ() - pDstVerts[j].GetZ()) < kEpsilon) )
            {
                break;
            }
        }

        if ( j == WD.nIndexedVerts )
        {
            // add this vert to our pool
            pDstVerts[WD.nIndexedVerts++] = pSrcVerts[i];
        }

        WD.RemapIndices[i] = j;
    }
}

//==============================================================================

void decal_mgr::RemoveDegenerateTris( working_data& WD )
{
    s16 VertIndices[working_data::MAX_WORKING_VERTS];
    s32 i, j, k;

    for ( i = 0; i < WD.nVerts-2; i++ )
    {
        // find the next polygon to deal with
        if ( (WD.VertFlags[i]   & working_data::FLAG_POLY_START) &&
             (WD.VertFlags[i+1] & working_data::FLAG_POLY_START) )
        {
            s32 PolyStart, PolyEnd;
            PolyStart = i;
            PolyEnd   = PolyStart + 2;
            while ( (PolyEnd < WD.nVerts) &&
                    !(WD.VertFlags[PolyEnd] & working_data::FLAG_POLY_START) )
            {
                PolyEnd++;
            }

            // create a new version of the polygon without duplicate verts
            s32 nNewPolyVerts = 0;
            for ( j = PolyStart; j < PolyEnd; j++ )
            {
                for ( k = 0; k < nNewPolyVerts; k++ )
                {
                    if ( WD.RemapIndices[j] == VertIndices[k] )
                        break;
                }

                if ( k == nNewPolyVerts )
                    VertIndices[nNewPolyVerts++] = WD.RemapIndices[j];
            }

            if ( nNewPolyVerts != (PolyEnd-PolyStart) )
            {
                // We've had to remove some duplicate verts which would've caused
                // degenerate tris. Fix up the working data.
                if ( nNewPolyVerts < 3 )
                    nNewPolyVerts = 0;

                s32 NewPolyEnd = PolyStart + nNewPolyVerts;

                x_memmove( &WD.RemapIndices[NewPolyEnd],
                           &WD.RemapIndices[PolyEnd],
                           sizeof(s16)*(WD.nVerts-PolyEnd) );
                x_memmove( &WD.VertFlags[NewPolyEnd],
                           &WD.VertFlags[PolyEnd],
                           sizeof(u16)*(WD.nVerts-PolyEnd) );
                WD.nVerts -= (PolyEnd-NewPolyEnd);

                // now copy the new verts in
                if ( nNewPolyVerts )
                {
                    for ( j = PolyStart; j < PolyStart+nNewPolyVerts; j++ )
                    {
                        WD.RemapIndices[j] = VertIndices[j-PolyStart];
                        WD.VertFlags[j]    = 0;
                    }

                    WD.VertFlags[PolyStart]   = working_data::FLAG_POLY_START;
                    WD.VertFlags[PolyStart+1] = working_data::FLAG_POLY_START;
                }

                // since we've shifted all of the other polys down, make sure
                // our loop counter will reflect that change
                i = NewPolyEnd-1;
            }
        }
    }
}

//==============================================================================

xbool decal_mgr::CollectCoplanarEdges( working_data&     WD,
                                       plane&            PolyPlane       )
                                       
{
    s32 i, j;
    for ( i = 0; i < WD.nVerts-2; i++ )
    {
        // see if we have a poly that hasn't been added yet. If so, this
        // will be the start of a new reduced polygon
        if ( (WD.VertFlags[i] & working_data::FLAG_POLY_START) &&
             (WD.VertFlags[i+1] & working_data::FLAG_POLY_START) &&
             !(WD.VertFlags[i] & working_data::FLAG_POLY_ADDED) )
        {
            // start a new coplanar set of edges
            PolyPlane.Setup( WD.IndexedVertPool[WD.RemapIndices[i]],
                             WD.IndexedVertPool[WD.RemapIndices[i+1]],
                             WD.IndexedVertPool[WD.RemapIndices[i+2]] );
            
            // find the beginning and end of the current strip
            s32 PolyStart, PolyEnd, iEdge;
            PolyStart = i;
            PolyEnd   = PolyStart+2;
            while ( (PolyEnd < WD.nVerts) &&
                    !(WD.VertFlags[PolyEnd] & working_data::FLAG_POLY_START) )
            {
                PolyEnd++;
            }

            // add all of the edges of this poly
            WD.nEdges = 0;
            WD.EdgeList[WD.nEdges].P0    = WD.RemapIndices[PolyEnd-1];
            WD.EdgeList[WD.nEdges].P1    = WD.RemapIndices[PolyStart];
            WD.EdgeList[WD.nEdges].Added = FALSE;
            WD.nEdges++;

            for ( iEdge = PolyStart+1; iEdge < PolyEnd; iEdge++ )
            {
                WD.EdgeList[WD.nEdges].P0    = WD.RemapIndices[iEdge-1];
                WD.EdgeList[WD.nEdges].P1    = WD.RemapIndices[iEdge];
                WD.EdgeList[WD.nEdges].Added = FALSE;
                WD.nEdges++;
            }

            WD.VertFlags[i] |= working_data::FLAG_POLY_ADDED;

            // now loop through all other polys and see if they can be added as well
            for ( j = PolyEnd; j < WD.nVerts-2; j++ )
            {
                if ( (WD.VertFlags[j] & working_data::FLAG_POLY_START) &&
                     (WD.VertFlags[j+1] & working_data::FLAG_POLY_START) &&
                     !(WD.VertFlags[j] & working_data::FLAG_POLY_ADDED) )
                {
                    // make sure this poly is coplanar with the current one
                    plane TestPlane;
                    TestPlane.Setup( WD.IndexedVertPool[WD.RemapIndices[j]],
                                     WD.IndexedVertPool[WD.RemapIndices[j+1]],
                                     WD.IndexedVertPool[WD.RemapIndices[j+2]] );
                    if ( x_abs( TestPlane.D - PolyPlane.D ) > kEpsilon )
                        continue;

                    f32 Dot = TestPlane.Normal.Dot( PolyPlane.Normal );
                    if ( (Dot < (1.0f-kEpsilon)) ||
                         (Dot > (1.0f+kEpsilon)) )
                    {
                        continue;
                    }

                    // find the beginning and end of the current poly
                    PolyStart = j;
                    PolyEnd   = PolyStart+2;
                    while ( (PolyEnd < WD.nVerts) &&
                            !(WD.VertFlags[PolyEnd] & working_data::FLAG_POLY_START) )
                    {
                        PolyEnd++;
                    }

                    // add all of the edges of this poly
                    WD.EdgeList[WD.nEdges].P0    = WD.RemapIndices[PolyEnd-1];
                    WD.EdgeList[WD.nEdges].P1    = WD.RemapIndices[PolyStart];
                    WD.EdgeList[WD.nEdges].Added = FALSE;
                    WD.nEdges++;

                    for ( iEdge = PolyStart+1; iEdge < PolyEnd; iEdge++ )
                    {
                        WD.EdgeList[WD.nEdges].P0    = WD.RemapIndices[iEdge-1];
                        WD.EdgeList[WD.nEdges].P1    = WD.RemapIndices[iEdge];
                        WD.EdgeList[WD.nEdges].Added = FALSE;
                        WD.nEdges++;
                    }

                    WD.VertFlags[j] |= working_data::FLAG_POLY_ADDED;
                }
            }

            // re-order the edges so that the lower index comes first
            for ( j = 0; j < WD.nEdges; j++ )
            {
                if ( WD.EdgeList[j].P0 > WD.EdgeList[j].P1 )
                {
                    s32 Temp          = WD.EdgeList[j].P0;
                    WD.EdgeList[j].P0 = WD.EdgeList[j].P1;
                    WD.EdgeList[j].P1 = Temp;
                }
            }

            // success
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

s32 DecalEdgeSortFn( const void* pA, const void* pB )
{
    decal_mgr::decal_edge* pEdgeA = (decal_mgr::decal_edge*)pA;
    decal_mgr::decal_edge* pEdgeB = (decal_mgr::decal_edge*)pB;
    if ( pEdgeA->P0 < pEdgeB->P0 )      return -1;
    else if ( pEdgeA->P0 > pEdgeB->P0 ) return 1;
    else if ( pEdgeA->P1 < pEdgeB->P1 ) return -1;
    else if ( pEdgeA->P1 > pEdgeB->P1 ) return 1;
    
    return 0;
}

//==============================================================================

void decal_mgr::RemoveDuplicateEdges( working_data& WD )
{
    s32        i;
    s32        nNewEdges = 0;
    decal_edge NewEdgeList[working_data::MAX_WORKING_VERTS];

    for ( i = 0; i < WD.nEdges; i++ )
    {
        // if the edge before or after the current one is already in the list,
        // this is a duplicate edge and should not be added.
        if ( (i<WD.nEdges-1) &&
             (WD.EdgeList[i].P0 == WD.EdgeList[i+1].P0) &&
             (WD.EdgeList[i].P1 == WD.EdgeList[i+1].P1) )
        {
            continue;
        }

        if ( (i>0) &&
             (WD.EdgeList[i].P0 == WD.EdgeList[i-1].P0) &&
             (WD.EdgeList[i].P1 == WD.EdgeList[i-1].P1) )
        {
            continue;
        }

        // we have a unique edge
        NewEdgeList[nNewEdges++] = WD.EdgeList[i];
    }

    // copy our new edge list back over the original
    WD.nEdges = nNewEdges;
    x_memcpy( WD.EdgeList, NewEdgeList, sizeof(decal_edge)*nNewEdges );
}

//==============================================================================

xbool decal_mgr::GetCoplanarPolyVerts( working_data& WD )
{
    s32 i, j;

    WD.nCoplanarPolyVerts = 0;
    
    // find an edge that hasn't been added yet
    for ( i = 0; i < WD.nEdges; i++ )
    {
        if ( WD.EdgeList[i].Added )
            continue;

        // this edge will become the start of our new poly
        WD.EdgeList[i].Added = TRUE;
        WD.CoplanarPolyVerts[WD.nCoplanarPolyVerts++] = WD.EdgeList[i].P0;
        WD.CoplanarPolyVerts[WD.nCoplanarPolyVerts++] = WD.EdgeList[i].P1;

        // keep adding edges with a common endpoint until we get back to the
        // start, forming a complete loop
        xbool CompleteLoop = FALSE;
        s32 SafetyCount = 0;
        while ( !CompleteLoop )
        {
            if ( ++SafetyCount > 500 )
            {
                ASSERTS( FALSE, "Internal decal error." );
                return FALSE;
            }

            for ( j = 0; j < WD.nEdges; j++ )
            {
                if ( WD.EdgeList[j].Added )
                    continue;

                if ( WD.EdgeList[j].P0 ==
                     WD.CoplanarPolyVerts[WD.nCoplanarPolyVerts-1] )
                {
                    if ( WD.EdgeList[j].P1 == WD.CoplanarPolyVerts[0] )
                        CompleteLoop = TRUE;
                    else
                        WD.CoplanarPolyVerts[WD.nCoplanarPolyVerts++] = WD.EdgeList[j].P1;

                    WD.EdgeList[j].Added = TRUE;
                    break;
                }
                else
                if ( WD.EdgeList[j].P1 ==
                     WD.CoplanarPolyVerts[WD.nCoplanarPolyVerts-1] )
                {
                    if ( WD.EdgeList[j].P0 == WD.CoplanarPolyVerts[0] )
                        CompleteLoop = TRUE;
                    else
                        WD.CoplanarPolyVerts[WD.nCoplanarPolyVerts++] = WD.EdgeList[j].P0;

                    WD.EdgeList[j].Added = TRUE;
                    break;
                }
            }
            
            if ( j == WD.nEdges )
            {
                // It's really not a complete loop, but give up. Most likely we had
                // a rounding error which caused a bow-tie, which totally screwed things up.
                CompleteLoop = TRUE;
            }
        }

        if ( WD.nCoplanarPolyVerts < 3 )
        {            
            return FALSE;
        }

        return TRUE;
    }

    return FALSE;
}

//==============================================================================

xbool decal_mgr::IsCollinear( const vector3& StartPoint,
                              const vector3& MidPoint,
                              const vector3& EndPoint )
{
    vector3 V1( MidPoint - StartPoint );
    vector3 V2( EndPoint - MidPoint );
    V1.Normalize();
    V2.Normalize();
    f32 Dot = V1.Dot( V2 );
    if ( Dot < (1.0f-kEpsilon) )
        return FALSE;
    if ( Dot > (1.0f+kEpsilon) )
        return FALSE;

    return TRUE;
}

//==============================================================================

void decal_mgr::RedoWinding( working_data&  WD,
                             const plane&   PolyPlane )
{
    vector3 Normal(0.0f,0.0f,0.0f);

    s32 i;
    s32 nVerts = WD.nCoplanarPolyVerts;
    for ( i = 0; i < nVerts; i++ )
    {
        s32 P0 = WD.CoplanarPolyVerts[i];
        s32 P1 = WD.CoplanarPolyVerts[(i+1)%nVerts];
        s32 P2 = WD.CoplanarPolyVerts[(i+2)%nVerts];
        Normal += GetTriNormal( WD.IndexedVertPool[P0],
                                WD.IndexedVertPool[P1],
                                WD.IndexedVertPool[P2],
                                FALSE );
    }

    // do we need to reverse the vert order?
    if ( PolyPlane.Normal.Dot( Normal ) < 0.0f )
    {
        for ( i = 0; i < nVerts/2; i++ )
        {
            s16 Temp                         = WD.CoplanarPolyVerts[i];
            WD.CoplanarPolyVerts[i]          = WD.CoplanarPolyVerts[nVerts-1-i];
            WD.CoplanarPolyVerts[nVerts-1-i] = Temp;
        }
    }
}

//==============================================================================

void decal_mgr::BuildConvexStrip( working_data& WD )
{
    // room left to add a strip?
    if ( (WD.nFinalVerts + 3) > MAX_VERTS_PER_DECAL )
        return;

    // useful ptrs
    const s16*     pVertIndices = WD.CoplanarPolyVerts;
    const vector3* pVerts       = WD.IndexedVertPool;

    // the first two verts get added as they are
    WD.ClippedVerts[WD.nFinalVerts]   = pVerts[pVertIndices[0]];
    WD.FinalVertFlags[WD.nFinalVerts] = decal_vert::FLAG_SKIP_TRIANGLE;
    WD.nFinalVerts++;
    WD.ClippedVerts[WD.nFinalVerts]   = pVerts[pVertIndices[1]];
    WD.FinalVertFlags[WD.nFinalVerts] = decal_vert::FLAG_SKIP_TRIANGLE;
    WD.nFinalVerts++;

    // the next verts will get alternated back and forth from the start vert, so
    // if we have 6 verts (0,1,2,3,4,5) the strip order would become
    // (0,1,5,2,4,3). See the pattern?
    s32 i;
    s32 Index      = 1;
    s32 nPolyVerts = WD.nCoplanarPolyVerts;
    for ( i = 2; i < nPolyVerts; i++ )
    {
        if ( i & 1 )
        {
            WD.ClippedVerts[WD.nFinalVerts]   = pVerts[pVertIndices[Index]];
            WD.FinalVertFlags[WD.nFinalVerts] = 0;
            WD.nFinalVerts++;
        }
        else
        {
            WD.ClippedVerts[WD.nFinalVerts]   = pVerts[pVertIndices[nPolyVerts-Index]];
            WD.FinalVertFlags[WD.nFinalVerts] = 0;
            WD.nFinalVerts++;
            Index++;
        }

        // have we reached the max? We may have to end the strip pre-maturely.
        if ( WD.nFinalVerts == MAX_VERTS_PER_DECAL )
            return;
    }
}

//==============================================================================

xbool decal_mgr::TriContainsVert( working_data&     WD,
                                  s32               P0,
                                  s32               P1,
                                  s32               P2,
                                  s32               PointToTest )
{
    if ( (PointToTest == P0) || (PointToTest == P1) || (PointToTest == P2) )
        return FALSE;

    const vector3& V0 = WD.IndexedVertPool[WD.CoplanarPolyVerts[P0]];
    const vector3& V1 = WD.IndexedVertPool[WD.CoplanarPolyVerts[P1]];
    const vector3& V2 = WD.IndexedVertPool[WD.CoplanarPolyVerts[P2]];
    const vector3& T0 = WD.IndexedVertPool[WD.CoplanarPolyVerts[PointToTest]];

    vector3 Edge0 = V1-V0;
    vector3 CP1   = v3_Cross( Edge0, T0-V0 );
    vector3 CP2   = v3_Cross( Edge0, V2-V0 );
    if ( CP1.Dot( CP2 ) < 0.0f )
        return FALSE;
    
    vector3 Edge1 = V2-V1;
    CP1           = v3_Cross( Edge1, T0-V1 );
    CP2           = v3_Cross( Edge1, V0-V1 );
    if ( CP1.Dot( CP2 ) < 0.0f )
        return FALSE;

    vector3 Edge2 = V0-V2;
    CP1           = v3_Cross( Edge2, T0-V2 );
    CP2           = v3_Cross( Edge2, V1-V2 );
    if ( CP1.Dot( CP2 ) < 0.0f )
        return FALSE;

    return TRUE;        
}

//==============================================================================

xbool decal_mgr::IsAnEar( working_data&     WorkingData,
                          triangulate_link  VertPool[MAX_VERTS_PER_DECAL],
                          s32               FirstConcave,
                          s32               VertToTest )
{
    if ( FirstConcave == -1 )
    {
        return TRUE;
    }
    else
    if ( !VertPool[VertToTest].bConcave )
    {
        s32 i    = FirstConcave;
        s32 Prev = VertPool[VertToTest].iPrev;
        s32 Next = VertPool[VertToTest].iNext;

        s32 SafetyCount = 0;
        do
        {
            if ( TriContainsVert( WorkingData, Prev, VertToTest, Next, i ) )
                return FALSE;
            
            if ( ++SafetyCount >= 200 )
            {
                break;
            }

            i = VertPool[i].iNextConcave;
        } while ( i != FirstConcave );

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

//==============================================================================

void decal_mgr::RemoveFromPool( triangulate_link  VertPool[MAX_VERTS_PER_DECAL],
                                s32&              FirstVert,
                                s32               VertToRemove    )
{
    if ( VertPool[VertToRemove].iNext == VertToRemove )
    {
        // handle the case where we've emptied out the pool
        ASSERT( VertPool[VertToRemove].iPrev == VertToRemove );
        FirstVert = -1;
    }
    else
    {
        // fix up the prev and next pointers
        ASSERT( VertPool[VertToRemove].iPrev != VertToRemove );
        VertPool[VertPool[VertToRemove].iPrev].iNext = VertPool[VertToRemove].iNext;
        VertPool[VertPool[VertToRemove].iNext].iPrev = VertPool[VertToRemove].iPrev;
    }

    // fix up the first link ptr
    if ( VertToRemove == FirstVert )
    {
        FirstVert = VertPool[VertToRemove].iNext;
    }

    // clear out the links
    VertPool[VertToRemove].iPrev = -1;
    VertPool[VertToRemove].iNext = -1;
}

//==============================================================================

void decal_mgr::RemoveFromConcave( triangulate_link  VertPool[MAX_VERTS_PER_DECAL],
                                   s32&              FirstConcave,
                                   s32               VertToRemove    )
{
    if ( VertPool[VertToRemove].iNextConcave == VertToRemove )
    {
        // handle the case where we've emptied out the pool
        ASSERT( VertPool[VertToRemove].iPrevConcave == VertToRemove );
        FirstConcave = -1;
    }
    else
    {
        // fix up the prev and next pointers
        ASSERT( VertPool[VertToRemove].iPrevConcave != VertToRemove );
        VertPool[VertPool[VertToRemove].iPrevConcave].iNextConcave = VertPool[VertToRemove].iNextConcave;
        VertPool[VertPool[VertToRemove].iNextConcave].iPrevConcave = VertPool[VertToRemove].iPrevConcave;
    }

    // fix up the first link ptr
    if ( VertToRemove == FirstConcave )
    {
        FirstConcave = VertPool[VertToRemove].iNextConcave;
    }

    // clear out the links
    VertPool[VertToRemove].bConcave     = FALSE;
    VertPool[VertToRemove].iPrevConcave = -1;
    VertPool[VertToRemove].iNextConcave = -1;
}

//==============================================================================

xbool decal_mgr::IsConvex( working_data&     WD,
                           triangulate_link  VertPool[MAX_VERTS_PER_DECAL],
                           s32               VertToTest,
                           const plane&      PolyPlane )
{
    s32     P0   = WD.CoplanarPolyVerts[VertPool[VertToTest].iPrev];
    s32     P1   = WD.CoplanarPolyVerts[VertToTest];
    s32     P2   = WD.CoplanarPolyVerts[VertPool[VertToTest].iNext];
    vector3 TriN = GetTriNormal( WD.IndexedVertPool[P0],
                                 WD.IndexedVertPool[P1],
                                 WD.IndexedVertPool[P2],
                                 FALSE );
    f32     Dot  = TriN.Dot( PolyPlane.Normal );

    return (Dot > 0.0f);
}

//==============================================================================

void decal_mgr::Triangulate( working_data&  WD,
                             const plane&   PolyPlane )
{
    // build the initial set of verts to be triangulated
    triangulate_link   VertPool[MAX_VERTS_PER_DECAL];
    s32                PoolStart    = 0;
    s32                ConcaveStart = -1;
    xbool              bConvex      = TRUE;
    s32                nPolyVerts   = WD.nCoplanarPolyVerts;
    s32                i;
    for ( i = 0; i < nPolyVerts; i++ )
    {
        VertPool[i].iPrev        = (i-1+nPolyVerts)%nPolyVerts;
        VertPool[i].iNext        = (i+1)%nPolyVerts;
        VertPool[i].iPrevConcave = -1;
        VertPool[i].iNextConcave = -1;

        if ( IsConvex( WD, VertPool, i, PolyPlane ) )
        {
            VertPool[i].bConcave = FALSE;
        }
        else
        {
            if ( ConcaveStart == -1 )
            {
                ConcaveStart = i;
                VertPool[i].iPrevConcave = i;
                VertPool[i].iNextConcave = i;
            }
            else
            {
                VertPool[i].iPrevConcave = VertPool[ConcaveStart].iPrevConcave;
                VertPool[i].iNextConcave = ConcaveStart;
                VertPool[VertPool[i].iPrevConcave].iNextConcave = i;
                VertPool[VertPool[i].iNextConcave].iPrevConcave = i;
                ConcaveStart = i;
            }
            VertPool[i].bConcave = TRUE;
            bConvex              = FALSE;
        }
    }

    // Early out if we ended up with a convex polygon. In this case we can make a
    // really nice triangle strip.
    if ( bConvex )
    {
        BuildConvexStrip( WD );
        return;
    }

    // handle concave polygons
    s32   SafetyCount  = 0;
    s32   PrevStrip0   = -1;
    s32   PrevStrip1   = -1;
    i                  = 2;
    while ( i != 0 )
    {
        if ( ++SafetyCount > 500 )
        {
            ASSERTS( FALSE, "Internal decal error" );
            break;
        }
        else
        if ( IsAnEar( WD, VertPool, ConcaveStart, VertPool[i].iPrev ) &&
             (nPolyVerts >= 3) )
        {
            // see if there are two shared verts that we can continue a strip with
            xbool bNewStrip = TRUE;
            s32   P0        = WD.CoplanarPolyVerts[VertPool[i].iPrev];
            s32   P1        = WD.CoplanarPolyVerts[i];
            s32   P2        = WD.CoplanarPolyVerts[VertPool[VertPool[i].iPrev].iPrev];
            s32   P3        = -1;
            if ( ((P0 == PrevStrip0) || (P1 == PrevStrip0)) &&
                 ((P0 == PrevStrip1) || (P1 == PrevStrip1)) )
            {
                bNewStrip = FALSE;
                P3 = P2;
            }
            else
            if ( ((P0 == PrevStrip0) || (P2 == PrevStrip0)) &&
                 ((P0 == PrevStrip1) || (P2 == PrevStrip1)) )
            {
                bNewStrip = FALSE;
                P3 = P1;
            }
            else
            if ( ((P1 == PrevStrip0) || (P2 == PrevStrip0)) &&
                 ((P1 == PrevStrip1) || (P2 == PrevStrip1)) )
            {
                bNewStrip = FALSE;
                P3 = P0;
            }
            
            // store the strip verts
            if ( bNewStrip )
            {
                // start a new strip
                if ( (WD.nFinalVerts + 3) > MAX_VERTS_PER_DECAL )
                    break;

                WD.ClippedVerts[WD.nFinalVerts]   = WD.IndexedVertPool[P0];
                WD.FinalVertFlags[WD.nFinalVerts] = decal_vert::FLAG_SKIP_TRIANGLE;
                WD.nFinalVerts++;

                WD.ClippedVerts[WD.nFinalVerts]   = WD.IndexedVertPool[P1];
                WD.FinalVertFlags[WD.nFinalVerts] = decal_vert::FLAG_SKIP_TRIANGLE;
                WD.nFinalVerts++;

                WD.ClippedVerts[WD.nFinalVerts]   = WD.IndexedVertPool[P2];
                WD.FinalVertFlags[WD.nFinalVerts] = 0;
                WD.nFinalVerts++;

                PrevStrip0 = P1;
                PrevStrip1 = P2;
            }
            else
            {
                // extend our strip by one vert
                if ( (WD.nFinalVerts + 1) > MAX_VERTS_PER_DECAL )
                    break;

                WD.ClippedVerts[WD.nFinalVerts]   = WD.IndexedVertPool[P3];
                WD.FinalVertFlags[WD.nFinalVerts] = 0;
                WD.nFinalVerts++;

                PrevStrip0 = PrevStrip1;
                PrevStrip1 = P3;
            }

            // chop the ear off
            RemoveFromPool( VertPool, PoolStart, VertPool[i].iPrev );
            nPolyVerts--;
            if ( nPolyVerts < 3 )
                break;

            if ( VertPool[i].bConcave &&
                 IsConvex( WD, VertPool, i, PolyPlane ) )
            {
                // chopping the ear off made i convex
                RemoveFromConcave( VertPool, ConcaveStart, i );
            }

            if ( VertPool[VertPool[i].iPrev].bConcave &&
                 IsConvex( WD, VertPool, VertPool[i].iPrev, PolyPlane ) )
            {
                // chopping the ear off made PRED(i) convex
                RemoveFromConcave( VertPool, ConcaveStart, VertPool[i].iPrev );
            }

            // if SUCC(0) was cut advance the scan
            if ( VertPool[i].iPrev == 0 )
                i = VertPool[i].iNext;
        }
        else
        {
            // advance the scan
            i = VertPool[i].iNext;
        }
    }
}

//==============================================================================

void decal_mgr::CombineCoplanarPolys( working_data& WD )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::CombineCoplanarPolys" );

    // build a list of unique vertices, and a way to remap them
    CreateIndexedVertPool( WD );

    if ( 0 )
    {
        s32     i;
        X_FILE* fh = x_fopen( "C:\\DebugDecals.txt", "w" );
        for ( i = 0; i < WD.nIndexedVerts; i++ )
        {
            x_fprintf( fh,
                       "%8d %10.3f %10.3f %10.3f\n",
                       i,
                       WD.IndexedVertPool[i].GetX(),
                       WD.IndexedVertPool[i].GetY(),
                       WD.IndexedVertPool[i].GetZ() );
        }

        for ( i = 0; i < WD.nVerts; i++ )
        {
            x_fprintf( fh,
                       "%8d %10d %10d\n",
                       i,
                       WD.RemapIndices[i],
                       WD.VertFlags[i] );
        }

        x_fclose( fh );
    }

    // degenerate tris can really cause havok, so let's get rid of them.
    // (degenerate tris can occur when rounding errors cause the clipper
    // to create small slivers, and then when we convert to an indexed vert
    // pool, the slivers become degenerate)
    RemoveDegenerateTris( WD );

    // deal with each coplanar set separately...for each one add the edges
    // and do our edge reduction, etc...
    WD.nFinalVerts = 0;

    // keep adding coplanar edges until there are no more to add
    plane PolyPlane;
    while ( CollectCoplanarEdges( WD, PolyPlane ) )
    {
        // sort the edges
        x_qsort( WD.EdgeList, WD.nEdges, sizeof(decal_edge), DecalEdgeSortFn );

        // remove duplicate edges
        RemoveDuplicateEdges( WD );

        // pull out the edges into multiple polys
        while ( GetCoplanarPolyVerts( WD ) )
        {
            s16 NewPolyVertIndices[working_data::MAX_WORKING_VERTS];
            s32 nNewPolyVerts = 0;
            s32 i;

            // remove collinear verts from this poly
            for ( i = 0; i < WD.nCoplanarPolyVerts; i++ )
            {
                s32 StartPoint = WD.CoplanarPolyVerts[i];
                s32 MidPoint   = WD.CoplanarPolyVerts[(i+1)%WD.nCoplanarPolyVerts];
                s32 EndPoint   = WD.CoplanarPolyVerts[(i+2)%WD.nCoplanarPolyVerts];
                if ( !IsCollinear( WD.IndexedVertPool[StartPoint],
                                   WD.IndexedVertPool[MidPoint],
                                   WD.IndexedVertPool[EndPoint] ) )
                {
                    NewPolyVertIndices[nNewPolyVerts++] = MidPoint;
                }
            }

            // don't let the decal get too crazy...if there's too many verts, just
            // don't add this poly
            if( nNewPolyVerts >= MAX_VERTS_PER_DECAL )
            {
                continue;
            }


            if ( nNewPolyVerts >= 3 )
            {
                WD.nCoplanarPolyVerts = nNewPolyVerts;
                x_memcpy( WD.CoplanarPolyVerts, NewPolyVertIndices, nNewPolyVerts*sizeof(s16) );

                // determine if this normal is facing the right direction and reverse
                // the vertex order if it is not
                RedoWinding( WD, PolyPlane );

                // now triangulate the possibly concave polygon
                Triangulate( WD, PolyPlane );
            }
        }
    }
}

//==============================================================================

s32 decal_mgr::CalcDecalVerts( s32                     Flags,
                               const vector3&          Point,
                               const vector3&          SurfaceNormal,
                               const vector3&          NegIncomingRay,
                               const vector2&          Size,
                               radian                  Roll,
                               decal_vert              Verts[MAX_VERTS_PER_DECAL],
                               matrix4&                L2W )
{
    if ( Flags & decal_definition::DECAL_FLAG_NO_CLIP )
    {
        // The most efficient way to do a decal is a coplanar poly that is not clipped.
        // Because of that, we'll go through a separate pipeline to get the points.
        // This means we will not be doing any kind of projection, but that should
        // be fine since this is meant for small stuff like bullet holes.
        return CalcNoClipDecal( Flags, Point, -SurfaceNormal, Size, Roll, Verts, L2W );
    }
    else
    {
        // Create a proper 3d decal. We'll set up an orthogonal projection matrix,
        // clip to that volume, calculate alpha based on the angle of approach,
        // and modify alpha based on distance from the initial strike point.
        if ( Flags & decal_definition::DECAL_FLAG_USE_PROJECTION )
        {
            return CalcProjectedDecal( Point, SurfaceNormal, NegIncomingRay, Size, Roll, Verts, L2W );
        }
        else
        {
            return CalcProjectedDecal( Point, SurfaceNormal, SurfaceNormal, Size, Roll, Verts, L2W );
        }
    }
}

//==============================================================================

s32 decal_mgr::GetDecalStart( xhandle RegInfoHandle, s32 nVerts )
{
    if ( RegInfoHandle.IsNull() )
        return -1;

    s32 HWBufferSize = render::GetHardwareBufferSize();
    registration_info& RegInfo = m_RegisteredDefs(RegInfoHandle);
    if ( nVerts > HWBufferSize )
        return -1;

    // find some space to store this decal
    xbool bAlloced = FALSE;
    s32   Start = -1;
    while ( !bAlloced )
    {
        if ( RegInfo.m_End >= RegInfo.m_Start )
        {
            ASSERT( RegInfo.m_End == RegInfo.m_Blank );

            s32 BufferEnd = RegInfo.m_Blank +
                            HWBufferSize -
                            (RegInfo.m_Blank%HWBufferSize);

            s32 nFreeInHWBuff     = BufferEnd - RegInfo.m_Blank;
            s32 nFreeInNextHWBuff = (BufferEnd==RegInfo.m_nVertsAllocated) ? 0 : HWBufferSize;
            if ( nFreeInHWBuff > nVerts )
            {
                // we had space at the end, and we can fit it into
                // the current hardware buffer
                bAlloced         = TRUE;
                Start            = RegInfo.m_End;
                RegInfo.m_End   += nVerts;
                RegInfo.m_Blank += nVerts;
            }
            else
            if ( nFreeInNextHWBuff > nVerts )
            {
                // we didn't have space available in this vert batch,
                // but we do have space available in the next.

                // mark the rest of these verts not to render so we don't render garbage
                s32 i;
                for ( i = RegInfo.m_Blank; i < BufferEnd; i++ )
                    RegInfo.m_pPositions[i].Flags = decal_vert::FLAG_SKIP_TRIANGLE;

                bAlloced        = TRUE;
                Start           = BufferEnd;
                RegInfo.m_End   = BufferEnd+nVerts;
                RegInfo.m_Blank = BufferEnd+nVerts;
            }
            else
            {
                // we can't wrap around the buffer with permanent decals
                if ( RegInfo.m_Flags & decal_definition::DECAL_FLAG_PERMANENT )
                    return -1;

                // no space at the end of the buffer, we'll attempt to wrap around
                s32 nVertsAvailable = MIN(RegInfo.m_Start, HWBufferSize);
                if ( nVertsAvailable > nVerts )
                {
                    // we have enough room at the beginning of the buffer
                    bAlloced      = TRUE;
                    Start         = 0;
                    RegInfo.m_End = nVerts;
                }
                else
                {
                    // try freeing up some space
                    s32 i;
                    for ( i = RegInfo.m_Start+1; i < RegInfo.m_Blank; i++ )
                    {
                        if ( RegInfo.m_pPositions[i].Flags & decal_vert::FLAG_DECAL_START )
                            break;
                        RegInfo.m_pPositions[i].Flags = decal_vert::FLAG_SKIP_TRIANGLE;
                    }

                    RegInfo.m_Start = i;
                    ASSERT( RegInfo.m_End >= RegInfo.m_Start );
                    if ( i == RegInfo.m_Blank )
                    {
                        // we had to free up the entire buffer to get enough room.
                        // this must be one big-ass decal
                        bAlloced        = TRUE;
                        Start           = 0;
                        RegInfo.m_Start = 0;
                        RegInfo.m_End   = nVerts;
                        RegInfo.m_Blank = nVerts;
                    }
                }
            }
        }
        else
        {
            // the list of verts wraps around the buffer
            ASSERT( RegInfo.m_Blank > RegInfo.m_Start );

            s32 BufferEnd = RegInfo.m_End +
                            HWBufferSize -
                            (RegInfo.m_End%HWBufferSize);
            
            s32 nFreeInHWBuff     = (BufferEnd>RegInfo.m_Start)     ?
                                    RegInfo.m_Start - RegInfo.m_End :
                                    BufferEnd - RegInfo.m_End;
            s32 nFreeInNextHWBuff = (BufferEnd+HWBufferSize>RegInfo.m_Start) ?
                                    RegInfo.m_Start-BufferEnd                 :
                                    HWBufferSize;

            if ( nFreeInHWBuff > nVerts )
            {
                // we have room in this hardware buffer
                bAlloced       = TRUE;
                Start          = RegInfo.m_End;
                RegInfo.m_End += nVerts;
            }
            else
            if ( nFreeInNextHWBuff > nVerts )
            {
                // we don't have room in this hardware buffer, but we do
                // have room in the next one

                // disable the rest of the triangles for this hardware buffer
                // so that we don't render garbage
                s32 i;
                for ( i = RegInfo.m_End; i < BufferEnd; i++ )
                    RegInfo.m_pPositions[i].Flags = decal_vert::FLAG_SKIP_TRIANGLE;

                bAlloced      = TRUE;
                Start         = BufferEnd;
                RegInfo.m_End = BufferEnd+nVerts;
            }
            else
            {
                // try freeing up some space
                s32 i;
                for ( i = RegInfo.m_Start+1; i < RegInfo.m_Blank; i++ )
                {
                    if ( RegInfo.m_pPositions[i].Flags & decal_vert::FLAG_DECAL_START )
                        break;
                    RegInfo.m_pPositions[i].Flags = decal_vert::FLAG_SKIP_TRIANGLE;
                }

                RegInfo.m_Start = i;
                if ( i >= RegInfo.m_Blank )
                {
                    // we had to free up the rest of the buffer
                    RegInfo.m_Blank = RegInfo.m_End;
                    RegInfo.m_Start = 0;
                }
            }
        }
    }

    // make sure the space we've allocated is set not to draw...
    // this is because we're not using a double-buffer like we should be
    // on the ps2 (too much memory). If the dma hasn't finished reading
    // the buffer yet, this will make sure it will not read corrupt data.
    s32 i;
    for ( i = Start; i < Start+nVerts; i++ )
    {
        RegInfo.m_pPositions[i].Flags |= decal_vert::FLAG_SKIP_TRIANGLE;
    }

    ASSERT( (Start/HWBufferSize) == ((Start+nVerts)/HWBufferSize) );
    ASSERT( Start != -1 );
    return Start;
}

//==============================================================================

void decal_mgr::AddDecal( xhandle                 RegInfoHandle,
                          s32                     nVerts,
                          decal_vert              DecalVerts[MAX_VERTS_PER_DECAL],
                          const matrix4&          L2W )
{
    s32 DecalStart = GetDecalStart( RegInfoHandle, nVerts );
    if ( DecalStart != -1 )
    {
        registration_info& RegInfo = m_RegisteredDefs(RegInfoHandle);
        
        s32 i;
        for ( i = 0; i < nVerts; i++ )
        {
            RegInfo.m_pPositions[DecalStart+i].Flags = DecalVerts[i].Flags;
            RegInfo.m_pPositions[DecalStart+i].Pos   = L2W * DecalVerts[i].Pos;
            RegInfo.m_pUVs[DecalStart+i].U           = (s16)(DecalVerts[i].UV.X*4096.0f);
            RegInfo.m_pUVs[DecalStart+i].V           = (s16)(DecalVerts[i].UV.Y*4096.0f);
            #ifdef X_EDITOR
            RegInfo.m_pColors[DecalStart+i]          = RegInfo.m_Color;
            #else
            RegInfo.m_pColors[DecalStart+i]          = (RegInfo.m_Color.R<<0)  +
                                                       (RegInfo.m_Color.G<<8)  +
                                                       (RegInfo.m_Color.B<<16) +
                                                       (RegInfo.m_Color.A<<24);
            #endif
        }

        RegInfo.m_pPositions[DecalStart].Flags |= decal_vert::FLAG_DECAL_START;
        BuildDynamicSpans( RegInfo, DecalStart, nVerts );
    }
}

//==============================================================================

void decal_mgr::AddClippedToQueue( const decal_definition& DecalDef,
                                   const vector3&          Point,
                                   const vector3&          Normal,
                                   const vector3&          NegIncomingRay,
                                   const vector2&          Size,
                                   radian                  Roll )
{
    if ( (m_DynamicQueueAddPos == m_DynamicQueueReadPos) &&
         m_DynamicQueue[m_DynamicQueueReadPos].Valid )
    {
        // the queue is full, just throw away this decal
        return;
    }

    queue_element& QElement = m_DynamicQueue[m_DynamicQueueAddPos];
    QElement.Color  = DecalDef.m_Color;
    QElement.Handle = DecalDef.m_Handle;
    QElement.Flags  = DecalDef.m_Flags;
    QElement.Point  = Point;
    QElement.Normal = Normal;
    QElement.NegRay = NegIncomingRay;
    QElement.Size   = Size;
    QElement.Roll   = Roll;
    QElement.Valid  = TRUE;
    m_DynamicQueueAddPos++;
    if ( m_DynamicQueueAddPos >= DYNAMIC_QUEUE_SIZE )
        m_DynamicQueueAddPos = 0;
}

//==============================================================================

namespace
{
    static render::decal_blend_mode GetDecalBlendMode( u16 BlendMode )
    {
        switch( BlendMode )
        {
            case decal_definition::DECAL_BLEND_ADD:       return render::DECAL_BLEND_ADDITIVE;
            case decal_definition::DECAL_BLEND_SUBTRACT:  return render::DECAL_BLEND_SUBTRACTIVE;
            case decal_definition::DECAL_BLEND_INTENSITY: return render::DECAL_BLEND_INTENSITY;
            case decal_definition::DECAL_BLEND_NORMAL:
            default:                                      return render::DECAL_BLEND_ALPHA;
        }
    }

    static xcolor GetDecalColor( u32 Color )
    {
        return xcolor( (u8)( Color        & 0xff ),
                       (u8)((Color >>  8) & 0xff ),
                       (u8)((Color >> 16) & 0xff ),
                       (u8)((Color >> 24) & 0xff ) );
    }
}

//==============================================================================

xbool decal_mgr::BuildSpan( decal_span& Span,
                            s32         SourceStart,
                            s32         SourceCount,
                            s32         DecalStart,
                            s32         DecalCount,
                            u32         Zone,
                            const position_data* pPos ) const
{
    if ( !pPos || ( SourceStart < 0 ) || ( SourceCount < 3 ) ||
         ( SourceCount > MAX_VERTS_PER_DECAL ) )
    {
        return FALSE;
    }

    Span = decal_span();
    Span.SourceStart = SourceStart;
    Span.SourceCount = SourceCount;
    Span.DecalStart = DecalStart;
    Span.DecalCount = DecalCount;
    Span.Zone = Zone;

    vector3 firstPosition( pPos[SourceStart].Pos.X, pPos[SourceStart].Pos.Y, pPos[SourceStart].Pos.Z );
    Span.Bounds.Set( firstPosition, 0.0f );
    for ( s32 i = 1; i < SourceCount; ++i )
    {
        position_data const& source = pPos[SourceStart + i];
        Span.Bounds += vector3( source.Pos.X, source.Pos.Y, source.Pos.Z );
    }
    Span.Bounds.Inflate( 1.0f, 1.0f, 1.0f );

    vector3 normal( 0.0f, 0.0f, 0.0f );
    xbool windingCW = TRUE;
    for ( s32 i = SourceStart + 2; i < SourceStart + SourceCount; ++i )
    {
        if ( pPos[i].Flags & decal_vert::FLAG_SKIP_TRIANGLE )
        {
            windingCW = TRUE;
            continue;
        }

        vector3 const p0( pPos[i - 2].Pos.X, pPos[i - 2].Pos.Y, pPos[i - 2].Pos.Z );
        vector3 const p1( pPos[i - 1].Pos.X, pPos[i - 1].Pos.Y, pPos[i - 1].Pos.Z );
        vector3 const p2( pPos[i].Pos.X, pPos[i].Pos.Y, pPos[i].Pos.Z );
        normal += windingCW ? v3_Cross( p1 - p0, p2 - p0 ) : v3_Cross( p1 - p2, p0 - p2 );
        windingCW = !windingCW;
    }

    if ( !normal.SafeNormalize() )
    {
        return FALSE;
    }

    Span.GeometricNormal = normal;
    Span.Ambient = SampleSpanAmbient( Span );
    Span.IsValid = TRUE;
    return TRUE;
}

//==============================================================================

void decal_mgr::BuildDynamicSpans( registration_info& RegInfo, s32 DecalStart, s32 nVerts )
{
    s32 const decalEnd = DecalStart + nVerts;
    for ( s32 i = 0; i < RegInfo.m_nDynamicSpans; ++i )
    {
        decal_span& span = RegInfo.m_pDynamicSpans[i];
        s32 const spanEnd = span.DecalStart + span.DecalCount;
        if ( span.IsValid && ( DecalStart < spanEnd ) && ( span.DecalStart < decalEnd ) )
        {
            span.IsValid = FALSE;
        }
    }

    s32 spanStart = -1;
    for ( s32 i = DecalStart + 2; i <= decalEnd; ++i )
    {
        xbool const isEnd = i == decalEnd;
        xbool const skipsTriangle = !isEnd && ( RegInfo.m_pPositions[i].Flags & decal_vert::FLAG_SKIP_TRIANGLE );
        if ( !isEnd && !skipsTriangle && ( spanStart < 0 ) )
        {
            spanStart = i - 2;
        }

        if ( ( spanStart < 0 ) || ( !isEnd && !skipsTriangle ) )
        {
            continue;
        }

        s32 destination = -1;
        for ( s32 j = 0; j < RegInfo.m_nDynamicSpans; ++j )
        {
            if ( !RegInfo.m_pDynamicSpans[j].IsValid ||
                 !IsDynamicSpanActive( RegInfo, RegInfo.m_pDynamicSpans[j] ) )
            {
                destination = j;
                break;
            }
        }
        if ( ( destination < 0 ) && ( RegInfo.m_nDynamicSpans < RegInfo.m_nDynamicSpansAllocated ) )
        {
            destination = RegInfo.m_nDynamicSpans++;
        }
        if ( destination < 0 )
        {
            destination = RegInfo.m_NextDynamicSpan;
            RegInfo.m_NextDynamicSpan =
                ( RegInfo.m_NextDynamicSpan + 1 ) % RegInfo.m_nDynamicSpansAllocated;
        }

        BuildSpan( RegInfo.m_pDynamicSpans[destination], spanStart, i - spanStart,
                   DecalStart, nVerts, 0xffffffffu, RegInfo.m_pPositions );
        spanStart = -1;
    }
}

//==============================================================================

xcolor decal_mgr::SampleSpanAmbient( const decal_span& Span ) const
{
    xcolor const fallback( 64, 64, 64, 255 );
    vector3 const center = Span.Bounds.GetCenter();
    vector3 const ray = Span.GeometricNormal * kAmbientRayHalfLength;

    g_CollisionMgr.RaySetup( NULL_GUID, center + ray, center - ray );
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES,
                                    object::ATTR_COLLIDABLE,
                                    object::ATTR_LIVING | object::ATTR_COLLISION_PERMEABLE );

    for ( s32 i = 0; i < g_CollisionMgr.m_nCollisions; ++i )
    {
        collision_mgr::collision const& collision = g_CollisionMgr.m_Collisions[i];
        if ( collision.PrimitiveKey < 0 )
        {
            continue;
        }

        object* pObject = g_ObjMgr.GetObjectByGuid( collision.ObjectHitGuid );
        object::detail_tri triangle;
        xcolor ambient;
        if ( pObject && pObject->GetColDetails( collision.PrimitiveKey, triangle ) &&
             render::SampleSurfaceColor( triangle, collision.Point, ambient ) )
        {
            return ambient;
        }
    }

    return fallback;
}

//==============================================================================

xbool decal_mgr::IsDynamicSpanActive( const registration_info& RegInfo, const decal_span& Span ) const
{
    if ( !Span.IsValid || ( RegInfo.m_Blank == 0 ) )
    {
        return FALSE;
    }

    s32 const position = Span.DecalStart;
    if ( RegInfo.m_End > RegInfo.m_Start )
    {
        return ( position >= RegInfo.m_Start ) && ( position < RegInfo.m_End );
    }
    if ( RegInfo.m_End < RegInfo.m_Start )
    {
        return ( position < RegInfo.m_End ) ||
               ( ( position >= RegInfo.m_Start ) && ( position < RegInfo.m_Blank ) );
    }
    return FALSE;
}

//==============================================================================

xbool decal_mgr::SubmitSpan( const registration_info& RegInfo,
                             const decal_span&         Span,
                             const position_data*      pPos,
                             const uv_data*            pUV,
                             const u32*                pColor,
                             const texture&            Texture ) const
{
    static f32 const UVScale = 1.0f / 4096.0f;
    render::decal_vertex vertices[MAX_VERTS_PER_DECAL];
    u16 indices[( MAX_VERTS_PER_DECAL - 2 ) * 3];
    s32 indexCount = 0;

    if ( !Span.IsValid || !pPos || !pUV || !pColor ||
         ( Span.SourceCount < 3 ) || ( Span.SourceCount > MAX_VERTS_PER_DECAL ) )
    {
        return FALSE;
    }

    for ( s32 i = 0; i < Span.SourceCount; ++i )
    {
        s32 const sourceIndex = Span.SourceStart + i;
        vector3 const position( pPos[sourceIndex].Pos.X, pPos[sourceIndex].Pos.Y, pPos[sourceIndex].Pos.Z );
        vector2 const uv( static_cast<f32>( pUV[sourceIndex].U ) * UVScale,
                          static_cast<f32>( pUV[sourceIndex].V ) * UVScale );
        xcolor Color = GetDecalColor( pColor[sourceIndex] );
        if ( RegInfo.m_Flags & decal_definition::DECAL_FLAG_FADE_OUT )
        {
            f32 const FadeTime = MAX( RegInfo.m_FadeoutTime, 0.0001f );
            f32 const Fade = x_clamp( 1.0f - ( Span.FadeElapsed / FadeTime ), 0.0f, 1.0f );
            Color.A = static_cast<u8>( static_cast<f32>( Color.A ) * Fade );
        }
        vertices[i] = render::decal_vertex( position, uv, Color );
    }

    xbool windingCW = TRUE;
    for ( s32 i = 2; i < Span.SourceCount; ++i )
    {
        s32 const sourceIndex = Span.SourceStart + i;
        if ( pPos[sourceIndex].Flags & decal_vert::FLAG_SKIP_TRIANGLE )
        {
            windingCW = TRUE;
            continue;
        }

        u16 const index0 = static_cast<u16>( i - 2 );
        u16 const index1 = static_cast<u16>( i - 1 );
        u16 const index2 = static_cast<u16>( i );
        indices[indexCount++] = windingCW ? index0 : index2;
        indices[indexCount++] = index1;
        indices[indexCount++] = windingCW ? index2 : index0;
        windingCW = !windingCW;
    }

    if ( indexCount == 0 )
    {
        return TRUE;
    }

    render::decal_draw_desc desc;
    desc.pTexture = &Texture;
    desc.Blend = GetDecalBlendMode( RegInfo.m_BlendMode );
    desc.Flags = render::DECAL_DRAW_FLAG_NONE;
    if ( RegInfo.m_Flags & decal_definition::DECAL_FLAG_ADD_GLOW )
    {
        desc.Flags |= render::DECAL_DRAW_FLAG_GLOW;
    }
    if ( RegInfo.m_Flags & decal_definition::DECAL_FLAG_ENV_MAPPED )
    {
        desc.Flags |= render::DECAL_DRAW_FLAG_ENV_MAPPED;
    }
    desc.Bounds = Span.Bounds;
    desc.GeometricNormal = Span.GeometricNormal;
    desc.Ambient = Span.Ambient;
    return render::SubmitDecalBatch( desc, vertices, Span.SourceCount, indices, indexCount );
}

//==============================================================================

xbool decal_mgr::RenderDynamicDecals( registration_info& RegInfo, const texture& Texture )
{
    for ( s32 i = 0; i < RegInfo.m_nDynamicSpans; ++i )
    {
        decal_span const& span = RegInfo.m_pDynamicSpans[i];
        if ( IsDynamicSpanActive( RegInfo, span ) &&
             !SubmitSpan( RegInfo, span, RegInfo.m_pPositions, RegInfo.m_pUVs, RegInfo.m_pColors, Texture ) )
        {
            return FALSE;
        }
    }
    return TRUE;
}

//==============================================================================

void decal_mgr::ReserveRenderCapacity( void ) const
{
    s32 vertexCount = 0;
    s32 indexCount = 0;
    s32 drawCount = 0;
    for ( s32 i = 0; i < m_RegisteredDefs.GetCount(); ++i )
    {
        registration_info const& regInfo = m_RegisteredDefs[i];
        vertexCount += regInfo.m_nVertsAllocated;
        indexCount += regInfo.m_nVertsAllocated * 3;
        drawCount += regInfo.m_nDynamicSpansAllocated;
#ifdef X_EDITOR
        vertexCount += regInfo.m_nStaticVertsAlloced;
        indexCount += regInfo.m_nStaticVertsAlloced * 3;
        drawCount += regInfo.m_nStaticVertsAlloced / 3;
#endif
    }
    for ( s32 i = 0; i < m_StaticSpans.GetCount(); ++i )
    {
        vertexCount += m_StaticSpans[i].SourceCount;
        indexCount += ( m_StaticSpans[i].SourceCount - 2 ) * 3;
        ++drawCount;
    }

    render::ReserveDecalSubmissionCapacity( MAX( vertexCount, 1 ), MAX( indexCount, 3 ), MAX( drawCount, 1 ) );
}

//==============================================================================

void decal_mgr::OnRender( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::OnRender" );

    for ( s32 i = 0; i < m_RegisteredDefs.GetCount(); ++i )
    {
        registration_info& regInfo = m_RegisteredDefs[i];
        if ( ( regInfo.m_Blank == 0 ) &&
#ifdef X_EDITOR
             ( regInfo.m_nStaticVerts == 0 ) &&
#endif
             ( regInfo.m_StaticDataOffset == -1 ) )
        {
            continue;
        }

        texture* pTexture = regInfo.m_bitmap.GetPointer();
        if ( !pTexture )
        {
            continue;
        }

        if ( !RenderDynamicDecals( regInfo, *pTexture ) ||
             !RenderStaticDecals( regInfo, *pTexture ) )
        {
            break;
        }
    }
}

//==============================================================================

#ifdef X_EDITOR
void decal_mgr::OnRenderEditorWireframes( void )
{
    RenderEditorStaticWireframes();
}
#endif

//==============================================================================

void decal_mgr::OnUpdate( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "decal_mgr::OnUpdate" );

    // Track fade time per span instead of updating every vertex.
    for( s32 i = 0; i < m_RegisteredDefs.GetCount(); ++i )
    {
        registration_info& RegInfo = m_RegisteredDefs[i];
        if( RegInfo.m_Flags & decal_definition::DECAL_FLAG_FADE_OUT )
        {
            for( s32 SpanIndex = 0; SpanIndex < RegInfo.m_nDynamicSpans; ++SpanIndex )
            {
                decal_span& Span = RegInfo.m_pDynamicSpans[SpanIndex];
                if ( IsDynamicSpanActive( RegInfo, Span ) )
                    Span.FadeElapsed += DeltaTime;
            }
        }
    }

    // add one of the non-clipped decals if we can
    if ( m_DynamicQueue[m_DynamicQueueReadPos].Valid )
    {
        // generate the decal verts
        queue_element& QElement = m_DynamicQueue[m_DynamicQueueReadPos];
        matrix4        L2W;
        decal_vert     DecalVerts[MAX_VERTS_PER_DECAL];
    
        // calculate the decal verts
        s32 nVerts = CalcDecalVerts( QElement.Flags,
                                     QElement.Point,
                                     QElement.Normal,
                                     QElement.NegRay,
                                     QElement.Size,
                                     QElement.Roll,
                                     DecalVerts,
                                     L2W );

        if ( nVerts )
        {
            AddDecal( QElement.Handle, nVerts, DecalVerts, L2W );
        }

        // we have added this decal, so mark this queue entry as invalid
        QElement.Valid = FALSE;
        m_DynamicQueueReadPos++;
        if ( m_DynamicQueueReadPos >= DYNAMIC_QUEUE_SIZE )
            m_DynamicQueueReadPos = 0; 
    }
}
