//==============================================================================
//
//  RigidGeomCollision.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "RigidGeomCollision.hpp"
#include "CollisionMgr/CollisionMgr.hpp"
#include "CollisionMgr/PolyCache.hpp"

#define INDEX_MASK  0x7FFF

//==============================================================================
//  FUNCTIONS
//==============================================================================

inline
void RigidGeom_SetPrimKey( s32& PrimKey, xbool bUseHighPoly, s32 iCluster, s32 iTriangle )
{
    PrimKey  = (iCluster << 16) | (iTriangle);
    PrimKey |= (bUseHighPoly) ? (0x80000000) : (0x00000000);
}

//==============================================================================

inline
void RigidGeom_GetPrimKey( s32 PrimKey, xbool& bUseHighPoly, s32& iCluster, s32& iTriangle )
{
    ASSERT( PrimKey != -1 );
    bUseHighPoly    = (PrimKey & 0x80000000) ? (TRUE):(FALSE);
    iTriangle       = (PrimKey & 0xFFFF);
    iCluster        = (PrimKey >> 16) & 0x7FFF;
}

//===========================================================================

xbool RigidGeom_GetTriangle( const rigid_geom*          pRigidGeom,
                             s32                   Key,
                             vector3&              P0,
                             vector3&              P1,
                             vector3&              P2)
{
    if( pRigidGeom==NULL )
        return FALSE;

    ASSERT( pRigidGeom->m_Collision.nHighClusters );
    
    xbool bUseHighPoly;
    s32 iCluster;
    s32 iTriangle;

    RigidGeom_GetPrimKey(Key,bUseHighPoly,iCluster,iTriangle);
        
    if( !IN_RANGE( 0, iCluster, pRigidGeom->m_Collision.nHighClusters-1 ) )
        return( FALSE );

    {
        collision_data::high_cluster& Cluster = pRigidGeom->m_Collision.pHighCluster [iCluster];
        const rigid_geom::section& Section = pRigidGeom->m_pSection[Cluster.iSection];
        if( !IN_RANGE( 0, iTriangle, Cluster.nTris-1 ) )
            return( FALSE );

        s32 Index = pRigidGeom->m_Collision.pHighIndexToVert0[ Cluster.iOffset + iTriangle ];

        // We have arrived.  Fill out the information.

        P0 = pRigidGeom->m_pVertex[pRigidGeom->m_pIndex[Section.FirstIndex + Index+0]].Pos;
        P1 = pRigidGeom->m_pVertex[pRigidGeom->m_pIndex[Section.FirstIndex + Index+1]].Pos;
        P2 = pRigidGeom->m_pVertex[pRigidGeom->m_pIndex[Section.FirstIndex + Index+2]].Pos;

        return( TRUE );
    }
    return( FALSE );

}

//===========================================================================

xbool RigidGeom_GetColDetails(const rigid_geom*     pRigidGeom,
                              const matrix4*        pL2W,
                              const u32*            pColor,
                              s32                   Key,
                              object::detail_tri&   Tri )
{
    ASSERT( pRigidGeom );
    ASSERT( pRigidGeom->m_Collision.nHighClusters );
    const collision_data& Coll = pRigidGeom->m_Collision;

    xbool   bUseHighPoly;
    s32     iCluster;
    s32     iTriangle;
    RigidGeom_GetPrimKey( Key, bUseHighPoly, iCluster, iTriangle );
    ASSERT( bUseHighPoly );
        
    if( !IN_RANGE( 0, iCluster, Coll.nHighClusters-1 ) )
        return( FALSE );

    {
        collision_data::high_cluster& Cluster = Coll.pHighCluster[iCluster];
        const rigid_geom::section& Section = pRigidGeom->m_pSection[Cluster.iSection];

        if( !IN_RANGE( 0, iTriangle, Cluster.nTris-1 ) )
            return( FALSE );

        s32 Index  = Coll.pHighIndexToVert0[ Cluster.iOffset + iTriangle ] & INDEX_MASK;

        // We have arrived.  Fill out the information.
        const matrix4& L2W = *pL2W;

        const u32 I0 = pRigidGeom->m_pIndex[Section.FirstIndex + Index+0];
        const u32 I1 = pRigidGeom->m_pIndex[Section.FirstIndex + Index+1];
        const u32 I2 = pRigidGeom->m_pIndex[Section.FirstIndex + Index+2];
        Tri.Vertex[0] = pRigidGeom->m_pVertex[I0].Pos;
        Tri.Vertex[1] = pRigidGeom->m_pVertex[I1].Pos;
        Tri.Vertex[2] = pRigidGeom->m_pVertex[I2].Pos;
        L2W.Transform( Tri.Vertex, Tri.Vertex, 3 );

        if( !pColor )
        {
            Tri.Color [0] = 0xFFFFFFFF;
            Tri.Color [1] = 0xFFFFFFFF;
            Tri.Color [2] = 0xFFFFFFFF;
        }
        else
        {
            Tri.Color [0] = pColor[ Section.iColor + I0 - Section.FirstVertex ];
            Tri.Color [1] = pColor[ Section.iColor + I1 - Section.FirstVertex ];
            Tri.Color [2] = pColor[ Section.iColor + I2 - Section.FirstVertex ];
        }

        Tri.Normal[0] = pRigidGeom->m_pVertex[I0].Normal;
        Tri.Normal[1] = pRigidGeom->m_pVertex[I1].Normal;
        Tri.Normal[2] = pRigidGeom->m_pVertex[I2].Normal;
        L2W.Transform( Tri.Normal, Tri.Normal, 3 );
        Tri.Normal[0] -= L2W.GetTranslation();
        Tri.Normal[1] -= L2W.GetTranslation();
        Tri.Normal[2] -= L2W.GetTranslation();

        Tri.UV    [0] = pRigidGeom->m_pVertex[I0].UV;
        Tri.UV    [1] = pRigidGeom->m_pVertex[I1].UV;
        Tri.UV    [2] = pRigidGeom->m_pVertex[I2].UV;

        Tri.MaterialInfo = Cluster.MaterialInfo;

        return( TRUE );
    }
    (void)Tri;
    return( FALSE );
}

//==============================================================================

xbool ClipLineToBBoxWithSafeSpace( const bbox& BBox, const vector3& P0, const vector3& P1, f32& T0, f32& T1 )
{
    vector3 Dir = P1 - P0;
    f32 tx_min, tx_max;
    f32 ty_min, ty_max;
    f32 tz_min, tz_max;
    f32 t_min, t_max;

    vector3 MinMinusP0 = BBox.Min - P0;
    vector3 MaxMinusP0 = BBox.Max - P0;

    if( Dir.GetX() >= 0.0f )
    {
        t_min = tx_min = MinMinusP0.GetX() / Dir.GetX();
        t_max = tx_max = MaxMinusP0.GetX() / Dir.GetX();
    }
    else
    {
        t_min = tx_min = MaxMinusP0.GetX() / Dir.GetX();
        t_max = tx_max = MinMinusP0.GetX() / Dir.GetX();
    }

    if( Dir.GetY() >= 0.0f )
    {
        ty_min = MinMinusP0.GetY() / Dir.GetY();
        ty_max = MaxMinusP0.GetY() / Dir.GetY();
    }
    else
    {
        ty_min = MaxMinusP0.GetY() / Dir.GetY();
        ty_max = MinMinusP0.GetY() / Dir.GetY();
    }

    if( t_min > ty_max || ty_min > t_max )
        return FALSE;

    if( t_min < ty_min )
        t_min = ty_min;
    if( t_max > ty_max )
        t_max = ty_max;

    if( Dir.GetZ() >= 0.0f )
    {
        tz_min = MinMinusP0.GetZ() / Dir.GetZ();
        tz_max = MaxMinusP0.GetZ() / Dir.GetZ();
    }
    else
    {
        tz_min = MaxMinusP0.GetZ() / Dir.GetZ();
        tz_max = MinMinusP0.GetZ() / Dir.GetZ();
    }

    if( t_min > tz_max || tz_min > t_max )
        return FALSE;

    if( t_min < tz_min )
        t_min = tz_min;
    if( t_max > tz_max )
        t_max = tz_max;

    if( !((t_min <= 1.0f) && (t_max >= 0.0f)) ) 
        return FALSE;

   // Set the initial entry and exit points of the ray
   T0 = t_min;
   T1 = t_max;

   // Backup and push forward by 1cm.
   f32 RayLen = Dir.Length();
   f32 SafeT = 1.0f / RayLen;
   T0 -= SafeT;
   T1 += SafeT;
   if( T0 < 0 ) T0 = 0;
   if( T1 > 1 ) T1 = 1;

   return TRUE;
}

//==============================================================================

#ifndef X_RETAIL
s32 TRI_STATS[20]={0};
xbool bDISPLAY_TIME = 0;
#endif // X_RETAIL

//==============================================================================

xbool IsPointInsideTri( const vector3& P, const vector3& TriP0, const vector3& TriP1, const vector3& TriP2, const vector3& TriNormal )
{
    vector3 EdgeNormal;

    EdgeNormal = TriNormal.Cross( TriP1 - TriP0 );
    f32 PDist = EdgeNormal.Dot( P - TriP0 );
    if( (PDist < 0.0f) )
        return FALSE;

    EdgeNormal = TriNormal.Cross( TriP2 - TriP1 );
    if( EdgeNormal.Dot( P - TriP1 ) < 0.0f )
        return FALSE;

    EdgeNormal = TriNormal.Cross( TriP0 - TriP2 );
    if( EdgeNormal.Dot( P - TriP2 ) < 0.0f )
        return FALSE;

    return TRUE;
}

//==============================================================================

#ifdef TARGET_WINDOWS
s32 RPC_COUNT=0;
void RigidGeom_RayVsPCHiPoly(      guid        Guid, 
                               const bbox&      WorldBBox,
                               const u64         MeshMask,
                               const matrix4*    pL2W, 
                               const rigid_geom& RigidGeom )
{
    s32 k;

#ifndef X_RETAIL
    TRI_STATS[0]++;
#endif
    extern xbool COLL_DISPLAY_OBJECTS;
    if( COLL_DISPLAY_OBJECTS)
    {
        RPC_COUNT++;
        if( RPC_COUNT == 800*5*2 )
            BREAK;
    }

    //
    // Dig out Ray and clip to world bbox
    //
    vector3 WorldRayStart;
    vector3 WorldRayEnd;
    f32     LocalRayT0;
    f32     LocalRayT1;
    {

        const collision_mgr::dynamic_ray& Ray = g_CollisionMgr.GetDynamicRay();
        if( !ClipLineToBBoxWithSafeSpace( WorldBBox, Ray.Start, Ray.End, LocalRayT0, LocalRayT1 ) )
            return;

        vector3 Direction = Ray.End - Ray.Start;
        WorldRayStart = Ray.Start + LocalRayT0*(Direction);
        WorldRayEnd   = Ray.Start + LocalRayT1*(Direction);
    }

    // Which collision resolution to use?
    ASSERT( g_CollisionMgr.IsUsingHighPoly() );
    ASSERT( RigidGeom.m_Collision.nHighClusters );

    const collision_data& Coll = RigidGeom.m_Collision;

    // For every bone...
    s32 iCurrentBone = -1;
    const matrix4* pCurrentL2W = NULL;
    vector3 LocalRayStart;
    vector3 LocalRayEnd;
    vector3 LocalRayDelta;
    bbox    LocalRayBBox;
    vector3 ClusterRayStart;
    vector3 ClusterRayEnd;
    vector3 ClusterRayDelta;
    bbox    ClusterRayBBox;
    f32     ClusterRayT0;
    f32     ClusterRayT1;

    // Loop thru the clusters and apply them if:
    //  - mesh is active
    //  - cluster uses the current bone
    for( s32 iC = 0; iC < Coll.nHighClusters; iC++ )
    {
        const collision_data::high_cluster& Cluster = Coll.pHighCluster[iC];
#ifndef X_RETAIL
        TRI_STATS[1]++;
#endif
        //
        // Do quick checks to see if we should be skipping this mesh or material
        //
        {
            u64 Bit = 1 << Cluster.iMesh;
            if( !(MeshMask & Bit) )
                continue;

            if( (Cluster.MaterialInfo.SoundType == object::MAT_TYPE_GLASS) && 
                (g_CollisionMgr.IsIgnoringGlass()) )
                continue;
        }
#ifndef X_RETAIL
        TRI_STATS[2]++;
#endif
        //
        // Update local ray info
        //
        if( Cluster.iBone != iCurrentBone )
        {
#ifndef X_RETAIL
            TRI_STATS[3]++;
#endif
            iCurrentBone = Cluster.iBone;

            // Get ptr to the current matrix
            pCurrentL2W = &pL2W[iCurrentBone];

            // Transform ray into space of bone.  Rather than InvertRT the matrix
            // we're just going to rework the transform.
            vector3 Translation = pCurrentL2W->GetTranslation();
            vector3 T,C1,C2,C3;
            pCurrentL2W->GetColumns(C1,C2,C3);

            T  = WorldRayStart - Translation;
            LocalRayStart.GetX() = T.Dot(C1);
            LocalRayStart.GetY() = T.Dot(C2);
            LocalRayStart.GetZ() = T.Dot(C3);

            T  = WorldRayEnd - Translation;
            LocalRayEnd.GetX() = T.Dot(C1);
            LocalRayEnd.GetY() = T.Dot(C2);
            LocalRayEnd.GetZ() = T.Dot(C3);

            LocalRayDelta = LocalRayEnd - LocalRayStart;
            LocalRayBBox.Set(LocalRayStart,LocalRayEnd);
            LocalRayBBox.Inflate(1,1,1);
        }

        //
        // Check if local ray bbox intersects cluster bbox and if ray intersects
        //
        {
            if( !LocalRayBBox.Intersect( Cluster.BBox ) )
            {
#ifndef X_RETAIL
                TRI_STATS[4]++;
#endif
                continue;
            }
        
            if( !ClipLineToBBoxWithSafeSpace( Cluster.BBox, LocalRayStart, LocalRayEnd, ClusterRayT0, ClusterRayT1 ) )
            {
#ifndef X_RETAIL
                TRI_STATS[5]++;
#endif
                continue;
            }

            ClusterRayStart = LocalRayStart + ClusterRayT0*LocalRayDelta;
            ClusterRayEnd   = LocalRayStart + ClusterRayT1*LocalRayDelta;
            ClusterRayDelta = ClusterRayEnd - ClusterRayStart;
            ClusterRayBBox.Set(ClusterRayStart,ClusterRayEnd);
        }


#ifndef X_RETAIL
        TRI_STATS[6]++;
#endif

        // Apply the cluster to the collision manager.
        const rigid_geom::section& Section = RigidGeom.m_pSection[Cluster.iSection];
//        extern xbool COLL_DISPLAY_OBJECTS;
//        if( COLL_DISPLAY_OBJECTS )
//            x_DebugMsg("CLUSTER %4d\n",Cluster.nTris);

        ASSERT( RigidGeom.m_pIndex );
        ASSERT( RigidGeom.m_pVertex );
        if( (RigidGeom.m_pIndex == NULL) || (RigidGeom.m_pVertex == NULL) ||
            (Section.nIndices < 3) || (Section.nVertices <= 0) )
            continue;

        // 0.133
        for( k = 0; k < Cluster.nTris; k++ )
        {
#ifndef X_RETAIL
            TRI_STATS[7]++;
#endif

            const u32 Offset  = Coll.pHighIndexToVert0[ Cluster.iOffset + k ] & INDEX_MASK;

            ASSERT( (Offset + 2) < (u32)Section.nIndices );
            if( (Offset + 2) >= (u32)Section.nIndices )
                continue;

            const u32 I0 = RigidGeom.m_pIndex[Section.FirstIndex + Offset+0];
            const u32 I1 = RigidGeom.m_pIndex[Section.FirstIndex + Offset+1];
            const u32 I2 = RigidGeom.m_pIndex[Section.FirstIndex + Offset+2];

            ASSERT( I0 >= (u32)Section.FirstVertex && I0 < (u32)(Section.FirstVertex + Section.nVertices) );
            ASSERT( I1 >= (u32)Section.FirstVertex && I1 < (u32)(Section.FirstVertex + Section.nVertices) );
            ASSERT( I2 >= (u32)Section.FirstVertex && I2 < (u32)(Section.FirstVertex + Section.nVertices) );
            if( (I0 < (u32)Section.FirstVertex) || (I0 >= (u32)(Section.FirstVertex + Section.nVertices)) ||
                (I1 < (u32)Section.FirstVertex) || (I1 >= (u32)(Section.FirstVertex + Section.nVertices)) ||
                (I2 < (u32)Section.FirstVertex) || (I2 >= (u32)(Section.FirstVertex + Section.nVertices)) )
                continue;

            //
            // Setup positions
            //
            const vector3 P0 = RigidGeom.m_pVertex[I0].Pos;
            const vector3 P1 = RigidGeom.m_pVertex[I1].Pos;
            const vector3 P2 = RigidGeom.m_pVertex[I2].Pos;

            if( ClusterRayBBox.IntersectTriBBox( P0, P1, P2 ) )
            {
#ifndef X_RETAIL
                TRI_STATS[8]++;
#endif

                vector3 PlaneNormal;
                f32     PlaneD;

                vector3 P1MinusP0 = P1-P0;
                vector3 P2MinusP0 = P2-P0;

                // Calculate non-normalized normal vector
                PlaneNormal = v3_Cross( P1MinusP0, P2MinusP0 );

                f32 PlaneNormalDotClusterRayStartPlusPlaneD;
                f32 PlaneNormalDotClusterRayDelta;
                f32 PlaneIntersectT;

                xbool bDoubleSided = !!(Cluster.MaterialInfo.Flags & collision_data::mat_info::FLAG_DOUBLESIDED);
                
#ifdef X_EDITOR
                // SB: Allow user to select transparent triangles from either side in editor select mode!
                if( g_CollisionMgr.IsEditorSelectRay() )
                    bDoubleSided |= !!(Cluster.MaterialInfo.Flags & collision_data::mat_info::FLAG_TRANSPARENT);
#endif                

                if (!bDoubleSided)
                {
                    // Check if we are moving away from the triangle
                    PlaneNormalDotClusterRayDelta = PlaneNormal.Dot( ClusterRayDelta );
                    if( PlaneNormalDotClusterRayDelta >= 0 )
                        continue;

                    // Compute PlaneD
                    PlaneD = -PlaneNormal.Dot( P0 );

                    // Check if ray end is in front of triangle
                    if( PlaneNormal.Dot(ClusterRayEnd)+PlaneD > 0 )
                        continue;

                    // Check if ray start is behind triangle
                    PlaneNormalDotClusterRayStartPlusPlaneD = PlaneNormal.Dot(ClusterRayStart) + PlaneD;
                    if( PlaneNormalDotClusterRayStartPlusPlaneD < 0 )
                        continue;

                    PlaneIntersectT = -PlaneNormalDotClusterRayStartPlusPlaneD / PlaneNormalDotClusterRayDelta;
                }
                else
                {
                    PlaneNormalDotClusterRayDelta = PlaneNormal.Dot( ClusterRayDelta );
                    PlaneD = -PlaneNormal.Dot( P0 );
                    PlaneNormalDotClusterRayStartPlusPlaneD = PlaneNormal.Dot(ClusterRayStart) + PlaneD;
                    PlaneIntersectT = -PlaneNormalDotClusterRayStartPlusPlaneD / PlaneNormalDotClusterRayDelta;

                    // Check if intersection is not in the interval of the test
                    if ((PlaneIntersectT < 0) || (PlaneIntersectT > 1))
                        continue;
                }

                vector3 HP = ClusterRayStart + PlaneIntersectT*ClusterRayDelta;

                // Confirm that intersection point is inside triangle bounds.  The order 
                // of testing is a bit twisted to make use of data already available.
                // For the first two edgenormals we are also testing the intersection point
                // against the opposite point boundary.
                f32 HPDist;
                vector3 EdgeNormal;
                vector3 HPMinusP0 = HP - P0;

                // Test P0->P1 edge
                EdgeNormal = PlaneNormal.Cross( P1MinusP0 );
                HPDist = EdgeNormal.Dot( HPMinusP0 );
                if( (HPDist < 0.0f) || (HPDist > EdgeNormal.Dot(P2MinusP0)) )
                {
#ifndef X_RETAIL
                    TRI_STATS[13]++;
#endif
                    continue;
                }

                // Test P0->P2 edge
                EdgeNormal = PlaneNormal.Cross( P2MinusP0 );
                HPDist = EdgeNormal.Dot( HPMinusP0 );
                if( (HPDist > 0.0f) || (HPDist < EdgeNormal.Dot(P1MinusP0)) )
                {
#ifndef X_RETAIL
                    TRI_STATS[14]++;
#endif
                    continue;
                }

                // Test P1->P2 edge
                EdgeNormal = PlaneNormal.Cross( P2 - P1 );
                if( EdgeNormal.Dot( HP - P1 ) < 0.0f )
                {
#ifndef X_RETAIL
                    TRI_STATS[15]++;    
#endif
                    continue;
                }

                {
#ifndef X_RETAIL
                    TRI_STATS[16]++;
#endif
                    // Compute the Primitive Key
                    s32 PrimKey;
                    RigidGeom_SetPrimKey( PrimKey, TRUE, iC, k );

                    // Transform back into worldspace
                    plane TrianglePlane( P0, P1, P2 );
                    TrianglePlane.Transform( *pCurrentL2W );
                    HP = *pCurrentL2W * HP;

                    // Work back and solve what the T value should be for the original ray
                    f32 T = PlaneIntersectT;
                        T = ClusterRayT0 + T*(ClusterRayT1-ClusterRayT0);
                        T = LocalRayT0 + T*(LocalRayT1-LocalRayT0);

                    // Build collision entry
                    collision_mgr::collision TempCollision(
                        T,
                        HP,
                        TrianglePlane,
                        TrianglePlane,
                        Guid,
                        PrimKey,
                        PRIMITIVE_STATIC_TRIANGLE,
                        FALSE,
                        MAX( P0.GetY(), MAX( P1.GetY(), P2.GetY() ) ),
                        Cluster.MaterialInfo.SoundType );

                    // Handle it off to the collision mgr
                    g_CollisionMgr.RecordCollision( TempCollision );

                    // If we are doing LOS then we are done!
                    if( g_CollisionMgr.IsStopOnFirstCollision() )
                        return;
                }
            }
        }
    }
}
#endif
//==============================================================================

void RigidGeom_ApplyCollision(       guid        Guid, 
                               const bbox&       BBox,
                               const u64         MeshMask,
                               const matrix4*    pL2W, 
                               const rigid_geom* pRigidGeom )
{
    s32 i, j, k;

    //
    // If we made it to here we should only be checking high poly!
    //
    ASSERT( g_CollisionMgr.IsUsingHighPoly() );

    //
    // Is the geometry available?  If not, apply bbox
    //
    if( pRigidGeom == NULL )
    {
        g_CollisionMgr.StartApply( Guid );
        g_CollisionMgr.ApplyAABBox( BBox );    
        g_CollisionMgr.EndApply();
        return;    
    }
   
    const rigid_geom& RigidGeom = *pRigidGeom;

    //
    // Did we compile high poly collision?
    //
    if( RigidGeom.m_Collision.nHighClusters == 0 )
    {
        return;
    }

    #ifdef TARGET_WINDOWS
    //
    // Check for Ray vs. PC High poly
    //
    if( g_CollisionMgr.IsUsingHighPoly() &&
        ((g_CollisionMgr.GetDynamicPrimitive()==PRIMITIVE_DYNAMIC_RAY) ||
         (g_CollisionMgr.GetDynamicPrimitive()==PRIMITIVE_DYNAMIC_LOS))
      )
    {
        RigidGeom_RayVsPCHiPoly( Guid, BBox, MeshMask, pL2W, *pRigidGeom );
        return;
    }
    #endif

    // Which collision resolution to use?
    if( g_CollisionMgr.IsUsingHighPoly() )
    {
        ASSERT( RigidGeom.m_Collision.nHighClusters );
        const collision_data& Coll = RigidGeom.m_Collision;

        // For every bone...
        for( i = 0; i < RigidGeom.m_nBones; i++ )
        {
            // Compute the inverse matrix.
            const matrix4& L2W = pL2W[i];
            const matrix4  W2L = m4_InvertRT( L2W );

            g_CollisionMgr.StartApply( Guid, L2W, W2L );

            // Loop thru the clusters and apply them if:
            //  - mesh is active
            //  - cluster uses the current bone

            for( j = 0; j < Coll.nHighClusters; j++ )
            {
                const collision_data::high_cluster& Cluster = Coll.pHighCluster[j];

                if( (Cluster.MaterialInfo.SoundType == object::MAT_TYPE_GLASS) && 
                    (g_CollisionMgr.IsIgnoringGlass()) )
                    continue;

                xbool bDoubleSided = !!(Cluster.MaterialInfo.Flags & collision_data::mat_info::FLAG_DOUBLESIDED);

                if( Cluster.iBone != i )
                    continue;

                u64 Bit = 1 << Cluster.iMesh;
                if( !(MeshMask & Bit) )
                    continue;

                {
                    // Apply the cluster to the collision manager.
                    const rigid_geom::section& Section = RigidGeom.m_pSection[Cluster.iSection];

                    for( k = 0; k < Cluster.nTris; k++ )
                    {
                        const u32 Offset  = Coll.pHighIndexToVert0[ Cluster.iOffset + k ] & INDEX_MASK;
                        const u32 I0 = RigidGeom.m_pIndex[Section.FirstIndex + Offset+0];
                        const u32 I1 = RigidGeom.m_pIndex[Section.FirstIndex + Offset+1];
                        const u32 I2 = RigidGeom.m_pIndex[Section.FirstIndex + Offset+2];

                        s32 PrimKey;
                        RigidGeom_SetPrimKey( PrimKey, TRUE, j, k );

                        g_CollisionMgr.ApplyTriangle(
                            RigidGeom.m_pVertex[I0].Pos,
                            RigidGeom.m_pVertex[I1].Pos,
                            RigidGeom.m_pVertex[I2].Pos,
                            Cluster.MaterialInfo.SoundType,
                            PrimKey );

                        // Double Sided single plane so apply collision for both sides!
                        if ( bDoubleSided )
                        {
                            g_CollisionMgr.ApplyTriangle(
                                RigidGeom.m_pVertex[I2].Pos,
                                RigidGeom.m_pVertex[I1].Pos,
                                RigidGeom.m_pVertex[I0].Pos,
                                Cluster.MaterialInfo.SoundType,
                                PrimKey );
                        }
                    }
                }
            }

            g_CollisionMgr.EndApply();    
        }
    }
}

//==============================================================================

#ifndef X_RETAIL
void RigidGeom_RenderCollision( const matrix4*  pBone, 
                                const rigid_geom*     pRigidGeom, 
                                xbool           bRenderHigh,
                                u64             LODMask)
{
    static const s32 LO_COLOR = 128;
    static const s32 HI_COLOR = 255;

    if( pRigidGeom == NULL )
        return;

    s32         i, j;
    xrandom     R;
    R.srand( ((u32)(uaddr)(pRigidGeom)) & 0x0000FFFF );

    if( bRenderHigh )
    {
        if( pRigidGeom->m_Collision.nHighClusters )
        {
            const collision_data& Coll = pRigidGeom->m_Collision;

            for( i = 0; i < Coll.nHighClusters; i++ )
            {      
                collision_data::high_cluster& Cluster = Coll.pHighCluster[i];

                u64 Bit = 1 << Cluster.iMesh;
                if( !(LODMask & Bit) )
                    continue;

                xcolor C = xcolor( R.irand( LO_COLOR, HI_COLOR ),
                                   R.irand( LO_COLOR, HI_COLOR ),
                                   R.irand( LO_COLOR, HI_COLOR ) );
                render::debug::Box( Cluster.BBox, pBone[Cluster.iBone], C,
                                    render::PRIMITIVE_DEPTH_READ_ONLY,
                                    render::PRIMITIVE_RASTER_COLLISION_BIASED );
            }  
        }
    }
    else
    {
        if( pRigidGeom->m_Collision.nLowClusters )
        {
            const collision_data& Coll = pRigidGeom->m_Collision;

            xrandom R;
            R.srand( ((u32)(uaddr)(pRigidGeom)) & 0x0000FFFF );
            
            for( i=0; i<Coll.nLowClusters; i++ )
            {
                collision_data::low_cluster& CL = Coll.pLowCluster[i];

                const render::primitive_draw_desc Material( NULL,
                                                            render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                            render::PRIMITIVE_BLEND_OPAQUE,
                                                            render::PRIMITIVE_DEPTH_READ_ONLY,
                                                            render::PRIMITIVE_RASTER_COLLISION_BIASED,
                                                            render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);
                render::PrimitiveBatch Batch( Material );

                xcolor CC = xcolor( R.irand( LO_COLOR, HI_COLOR ),
                                    R.irand( LO_COLOR, HI_COLOR ),
                                    R.irand( LO_COLOR, HI_COLOR ),
                                    255 );

                for( j=0; j<CL.nQuads; j++ )
                {
                    collision_data::low_quad& QD = Coll.pLowQuad[ CL.iQuadOffset + j ];

                    vector3* P0 = &Coll.pLowVector[ CL.iVectorOffset + QD.iP[0] ];
                    vector3* P1 = &Coll.pLowVector[ CL.iVectorOffset + QD.iP[1] ];
                    vector3* P2 = &Coll.pLowVector[ CL.iVectorOffset + QD.iP[2] ];
                    vector3* P3 = &Coll.pLowVector[ CL.iVectorOffset + QD.iP[3] ];

                    f32 I = R.frand(0.5f,1.0f);
                    xcolor C = CC;
                    C.R = (u8)( CC.R * I );
                    C.G = (u8)( CC.G * I );
                    C.B = (u8)( CC.B * I );
                    C.A = 255;

                    const render::primitive_vertex V0( *P0, vector2( 0.0f, 0.0f ), C );
                    const render::primitive_vertex V1( *P1, vector2( 0.0f, 0.0f ), C );
                    const render::primitive_vertex V2( *P2, vector2( 0.0f, 0.0f ), C );
                    Batch.AddTriangle( V0, V1, V2 );

                    if( QD.Flags )
                    {
                        const render::primitive_vertex V3( *P3, vector2( 0.0f, 0.0f ), C );
                        Batch.AddTriangle( V0, V2, V3 );
                    }
                }

                Batch.Submit( pBone[CL.iBone] );
            }
        }
        else
        {
            bbox BBox = pRigidGeom->m_Collision.BBox;
            BBox.Transform( *pBone );
            render::debug::Box( BBox, XCOLOR_YELLOW );
        }
    }
}
#endif // X_RETAIL

//==============================================================================

void RigidGeom_GatherToPolyCache(       guid        Guid, 
                               const bbox&       BBox,
                                     u64         MeshMask,
                               const matrix4*    pL2W, 
                               const rigid_geom* pRigidGeom )
{
    (void)BBox;
    if( (pRigidGeom==NULL) || (pRigidGeom->m_Collision.nLowClusters==0) )
        return;

    g_PolyCache.GatherCluster( pRigidGeom->m_Collision, pL2W, MeshMask, Guid );
}

//==============================================================================
