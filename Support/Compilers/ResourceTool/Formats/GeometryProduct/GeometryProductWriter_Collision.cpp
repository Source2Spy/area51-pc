//=========================================================================
//
//  GeometryProductWriter_Collision.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"
#include "Render/RigidGeom.hpp"
#include "Model/GeometryNames.hpp"

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

s32 ResolveCollisionMesh( const geometry_collision_cluster& Source, const rigid_geom& Geom,
                          const dictionary& Dictionary )
{
    if( !Source.MeshName.empty() )
    {
        for( s32 iMesh = 0; iMesh < Geom.m_nMeshes; ++iMesh )
        {
            if( geometry_names_match( Source.MeshName.c_str(),
                                      Dictionary.GetString( Geom.m_pMesh[iMesh].NameOffset ) ) )
                return iMesh;
        }
        return -1;
    }
    return Source.Mesh;
}

//=========================================================================

s32 ResolveCollisionBone( const geometry_collision_cluster& Source, const geometry_asset& Asset )
{
    if( !Source.BoneName.empty() )
    {
        for( s32 iBone = 0; iBone < (s32)Asset.Skeleton.Bones.size(); ++iBone )
        {
            if( geometry_names_match( Source.BoneName.c_str(), Asset.Skeleton.Bones[iBone].Name.c_str() ) )
                return iBone;
        }
        return -1;
    }
    return Source.Bone;
}

//=========================================================================

s32 ResolveCollisionSection( const geometry_collision_cluster& Source, const rigid_geom& Geom,
                             const dictionary& Dictionary )
{
    if( Source.SectionName.empty() )
        return Source.Section;

    for( s32 iMesh = 0; iMesh < Geom.m_nMeshes; ++iMesh )
    {
        const geom::mesh& Mesh = Geom.m_pMesh[iMesh];
        for( s32 iSubMesh = 0; iSubMesh < Mesh.nSubMeshes; ++iSubMesh )
        {
            const geom::submesh& SubMesh = Geom.m_pSubMesh[Mesh.iSubMesh + iSubMesh];
            for( s32 iSection = 0; iSection < SubMesh.nSections; ++iSection )
            {
                const s32 iSectionIndex = SubMesh.iSection + iSection;
                const char* pName =
                    (const char*)xfs( "%s/submesh_%d/section_%d",
                                      Dictionary.GetString( Geom.m_pMesh[iMesh].NameOffset ), iSubMesh, iSection );
                if( geometry_names_match( Source.SectionName.c_str(), pName ) )
                    return iSectionIndex;
            }
        }
    }
    // Prefer the semantic section name; validate the stored index as a fallback.
    return Source.Section;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_writer::ApplyCollisionMetadata( rigid_geom& Geom, const geometry_asset& Asset ) const
{
    if( Asset.Collision.Clusters.empty() )
        return TRUE;

    s32 nLowMetadata = 0;
    s32 nHighMetadata = 0;
    for( s32 i = 0; i < (s32)Asset.Collision.Clusters.size(); ++i )
    {
        if( Asset.Collision.Clusters[i].High )
            ++nHighMetadata;
        else
            ++nLowMetadata;
    }

    xbool bLowExact = nLowMetadata == Geom.m_Collision.nLowClusters;
    xbool bHighExact = TRUE;

    xbool bHasHighTriangleData = TRUE;
    for( s32 i = 0; i < (s32)Asset.Collision.Clusters.size(); ++i )
    {
        const geometry_collision_cluster& Source = Asset.Collision.Clusters[i];
        if( Source.High && (s32)Source.TriangleOffsets.size() != Source.Count )
        {
            bHasHighTriangleData = FALSE;
            break;
        }
    }

    // Preserve high-cluster triangle offsets when available.
    if( bHasHighTriangleData && nHighMetadata > 0 )
    {
        s32 nHighIndices = 0;
        for( s32 i = 0; i < (s32)Asset.Collision.Clusters.size(); ++i )
        {
            const geometry_collision_cluster& Source = Asset.Collision.Clusters[i];
            if( Source.High )
                nHighIndices = x_max( nHighIndices, Source.Offset + Source.Count );
        }

        std::vector<u16> RestoredIndices( nHighIndices, 0 );
        std::vector<collision_data::high_cluster> RestoredClusters;
        RestoredClusters.reserve( nHighMetadata );
        for( s32 i = 0; i < (s32)Asset.Collision.Clusters.size(); ++i )
        {
            const geometry_collision_cluster& Source = Asset.Collision.Clusters[i];
            if( !Source.High )
                continue;

            const s32 SourceMesh = ResolveCollisionMesh( Source, Geom, Context().m_Dictionary );
            const s32 SourceBone = ResolveCollisionBone( Source, Asset );
            const s32 SourceSection = ResolveCollisionSection( Source, Geom, Context().m_Dictionary );

            if( SourceMesh < 0 || SourceMesh >= Geom.m_nMeshes || SourceSection < 0 ||
                SourceSection >= Geom.m_nSections || Source.Offset < 0 || Source.Count < 0 ||
                Source.Offset + Source.Count > nHighIndices )
            {
                bHighExact = FALSE;
                break;
            }

            const rigid_geom::section& Section = Geom.m_pSection[SourceSection];
            collision_data::high_cluster Target;
            Target.BBox = Source.BBox;
            Target.nTris = Source.Count;
            Target.iMesh = SourceMesh;
            Target.iBone = SourceBone;
            Target.iSection = SourceSection;
            Target.iOffset = Source.Offset;
            Target.MaterialInfo.SoundType = (u16)Source.SoundType;
            Target.MaterialInfo.Flags = (u16)Source.Flags;

            for( s32 j = 0; j < (s32)Source.TriangleOffsets.size(); ++j )
            {
                const s32 EncodedIndex = Source.TriangleOffsets[j];
                const s32 FirstIndex = EncodedIndex & 0x7fff;
                if( EncodedIndex < 0 || EncodedIndex > 0xffff || FirstIndex + 2 >= Section.nIndices )
                {
                    bHighExact = FALSE;
                    break;
                }
                RestoredIndices[Source.Offset + j] = (u16)EncodedIndex;
            }
            if( !bHighExact )
                break;
            RestoredClusters.push_back( Target );
        }

        if( bHighExact )
        {
            delete[] Geom.m_Collision.pHighCluster;
            delete[] Geom.m_Collision.pHighIndexToVert0;
            Geom.m_Collision.nHighClusters = nHighMetadata;
            Geom.m_Collision.pHighCluster = nHighMetadata > 0 ? new collision_data::high_cluster[nHighMetadata] : NULL;
            Geom.m_Collision.nHighIndices = nHighIndices;
            Geom.m_Collision.pHighIndexToVert0 = nHighIndices > 0 ? new u16[nHighIndices] : NULL;
            if( nHighMetadata > 0 )
            {
                x_memcpy( Geom.m_Collision.pHighCluster, RestoredClusters.data(),
                          sizeof( collision_data::high_cluster ) * nHighMetadata );
            }
            if( nHighIndices > 0 )
            {
                x_memcpy( Geom.m_Collision.pHighIndexToVert0, RestoredIndices.data(), sizeof( u16 ) * nHighIndices );
            }
        }
    }

    // Rebuild high-cluster ranges from the generated section layout.
    std::vector<s32> GeneratedSectionCluster( Geom.m_nSections, -1 );
    for( s32 i = 0; !bHasHighTriangleData && i < Geom.m_Collision.nHighClusters; ++i )
    {
        const collision_data::high_cluster& Cluster = Geom.m_Collision.pHighCluster[i];
        if( Cluster.iSection >= 0 && Cluster.iSection < Geom.m_nSections &&
            GeneratedSectionCluster[Cluster.iSection] < 0 )
        {
            GeneratedSectionCluster[Cluster.iSection] = i;
        }
    }

    for( s32 i = 0; !bHasHighTriangleData && i < (s32)Asset.Collision.Clusters.size(); ++i )
    {
        const geometry_collision_cluster& Source = Asset.Collision.Clusters[i];
        if( !Source.High )
            continue;

        const s32 SourceMesh = ResolveCollisionMesh( Source, Geom, Context().m_Dictionary );
        const s32 SourceBone = ResolveCollisionBone( Source, Asset );
        const s32 SourceSection = ResolveCollisionSection( Source, Geom, Context().m_Dictionary );

        if( SourceMesh < 0 || SourceMesh >= Geom.m_nMeshes || SourceSection < 0 || SourceSection >= Geom.m_nSections ||
            Source.Offset < 0 || Source.Count < 0 || Source.Offset + Source.Count > Geom.m_Collision.nHighIndices )
        {
            bHighExact = FALSE;
            continue;
        }

        const s32 iGenerated = GeneratedSectionCluster[SourceSection];
        if( iGenerated < 0 )
        {
            bHighExact = FALSE;
            continue;
        }

        const collision_data::high_cluster& Generated = Geom.m_Collision.pHighCluster[iGenerated];
        if( Generated.iMesh != SourceMesh || Generated.iBone != SourceBone || Source.Offset < Generated.iOffset ||
            Source.Offset + Source.Count > Generated.iOffset + Generated.nTris )
        {
            bHighExact = FALSE;
        }
    }

    if( bHighExact && !bHasHighTriangleData )
    {
        std::vector<collision_data::high_cluster> Restored( nHighMetadata );
        s32 iHigh = 0;
        for( s32 i = 0; i < (s32)Asset.Collision.Clusters.size(); ++i )
        {
            const geometry_collision_cluster& Source = Asset.Collision.Clusters[i];
            if( !Source.High )
                continue;

            const s32 SourceMesh = ResolveCollisionMesh( Source, Geom, Context().m_Dictionary );
            const s32 SourceBone = ResolveCollisionBone( Source, Asset );
            const s32 SourceSection = ResolveCollisionSection( Source, Geom, Context().m_Dictionary );

            collision_data::high_cluster& Target = Restored[iHigh++];
            Target.BBox = Source.BBox;
            Target.nTris = Source.Count;
            Target.iMesh = SourceMesh;
            Target.iBone = SourceBone;
            Target.iSection = SourceSection;
            Target.iOffset = Source.Offset;
            Target.MaterialInfo.SoundType = (u16)Source.SoundType;
            Target.MaterialInfo.Flags = (u16)Source.Flags;
        }

        delete[] Geom.m_Collision.pHighCluster;
        Geom.m_Collision.nHighClusters = nHighMetadata;
        Geom.m_Collision.pHighCluster = nHighMetadata > 0 ? new collision_data::high_cluster[nHighMetadata] : NULL;
        if( nHighMetadata > 0 )
        {
            x_memcpy( Geom.m_Collision.pHighCluster, Restored.data(),
                      sizeof( collision_data::high_cluster ) * nHighMetadata );
        }
    }

    // Copy low offsets only when the generated layout is compatible.
    if( bLowExact )
    {
        s32 iLow = 0;
        for( s32 i = 0; i < (s32)Asset.Collision.Clusters.size(); ++i )
        {
            const geometry_collision_cluster& Source = Asset.Collision.Clusters[i];
            if( Source.High )
                continue;

            const s32 SourceMesh = ResolveCollisionMesh( Source, Geom, Context().m_Dictionary );
            const s32 SourceBone = ResolveCollisionBone( Source, Asset );

            collision_data::low_cluster& Target = Geom.m_Collision.pLowCluster[iLow++];
            if( Target.iMesh != SourceMesh || Target.iBone != SourceBone ||
                Target.iVectorOffset != Source.VectorOffset || Target.nPoints != Source.Points ||
                Target.nNormals != Source.Normals || Target.iQuadOffset != Source.QuadOffset ||
                Target.nQuads != Source.Count )
            {
                bLowExact = FALSE;
                break;
            }
        }
    }

    if( nLowMetadata != Geom.m_Collision.nLowClusters )
    {
        x_printf( "WARNING: Collision low-cluster count changed from %d to %d; "
                  "kept regenerated low collision layout.\n",
                  nLowMetadata, Geom.m_Collision.nLowClusters );
    }
    else if( !bLowExact )
    {
        x_printf( "WARNING: Collision low topology changed; kept regenerated "
                  "low collision offsets.\n" );
    }

    if( !bHighExact )
    {
        x_printf( "WARNING: Collision high topology changed; kept regenerated "
                  "high collision layout.\n" );
    }

    // Preserve bounds and material metadata independent of topology.
    s32 iLow = 0;
    s32 iHigh = 0;
    for( s32 i = 0; i < (s32)Asset.Collision.Clusters.size(); ++i )
    {
        const geometry_collision_cluster& Source = Asset.Collision.Clusters[i];
        if( Source.High )
        {
            if( !bHighExact )
                continue;
            Geom.m_Collision.pHighCluster[iHigh].BBox = Source.BBox;
            Geom.m_Collision.pHighCluster[iHigh].MaterialInfo.SoundType = (u16)Source.SoundType;
            Geom.m_Collision.pHighCluster[iHigh].MaterialInfo.Flags = (u16)Source.Flags;
            ++iHigh;
        }
        else if( iLow < Geom.m_Collision.nLowClusters )
        {
            collision_data::low_cluster& Target = Geom.m_Collision.pLowCluster[iLow++];
            Target.BBox = Source.BBox;
            if( bLowExact && (s32)Source.QuadFlags.size() == Target.nQuads )
            {
                for( s32 j = 0; j < Target.nQuads; ++j )
                    Geom.m_Collision.pLowQuad[Target.iQuadOffset + j].Flags = (byte)Source.QuadFlags[j];
            }
        }
    }

    Geom.m_Collision.BBox.Clear();
    for( s32 i = 0; i < Geom.m_Collision.nLowClusters; ++i )
        Geom.m_Collision.BBox += Geom.m_Collision.pLowCluster[i].BBox;
    for( s32 i = 0; i < Geom.m_Collision.nHighClusters; ++i )
        Geom.m_Collision.BBox += Geom.m_Collision.pHighCluster[i].BBox;

    if( Asset.Collision.HasCollisionBBox )
        Geom.m_Collision.BBox = Asset.Collision.CollisionBBox;

    return bLowExact && bHighExact;
}

//=========================================================================

s32 DetermineMeshBone( const geometry_asset& Mesh, s32 iMesh )
{
    for( s32 i = 0; i < (s32)Mesh.Render.Facets.size(); i++ )
    {
        const geometry_facet& Facet = Mesh.Render.Facets[i];
        if( Facet.iMesh != iMesh )
            continue;

        const s32 iVertex = Facet.iVertex[0];
        if( ( iVertex >= 0 ) && ( iVertex < (s32)Mesh.Render.Vertices.size() ) &&
            !Mesh.Render.Vertices[iVertex].Weight.empty() )
        {
            return Mesh.Render.Vertices[iVertex].Weight[0].iBone;
        }
    }

    return ( Mesh.Skeleton.Bones.size() == 1 ) ? 0 : -1;
}

//=========================================================================

void geometry_product_writer::CompileLowCollision( rigid_geom& RigidGeom, geometry_asset& LowMesh,
                                                   geometry_asset& HighMesh )
{
    collision_data& Collision = RigidGeom.m_Collision;
    Collision.nLowClusters = 0;
    Collision.nLowVectors = 0;
    Collision.nLowQuads = 0;
    Collision.pLowCluster = NULL;
    Collision.pLowVector = NULL;
    Collision.pLowQuad = NULL;

    if( !Context().m_bDoCollision )
        return;

    if( Context().m_MeshToCompiled.GetCount() != (s32)HighMesh.Render.SubMeshes.size() )
        ThrowError( "Internal mesh mapping does not match collision source" );

    xarray<s32> LowToHigh;
    xarray<s32> HighBone;
    LowToHigh.SetCount( (s32)LowMesh.Render.SubMeshes.size() );
    HighBone.SetCount( (s32)HighMesh.Render.SubMeshes.size() );

    for( s32 i = 0; i < (s32)HighMesh.Render.SubMeshes.size(); i++ )
    {
        HighBone[i] = DetermineMeshBone( HighMesh, i );
    }

    for( s32 iLowMesh = 0; iLowMesh < (s32)LowMesh.Render.SubMeshes.size(); iLowMesh++ )
    {
        char LowName[256];
        x_strsavecpy( LowName, LowMesh.Render.SubMeshes[iLowMesh].Name.c_str(), sizeof( LowName ) );

        const s32 Length = x_strlen( LowName );
        if( ( Length < 3 ) || ( LowName[Length - 2] != '_' ) ||
            ( ( LowName[Length - 1] != 'c' ) && ( LowName[Length - 1] != 'C' ) ) )
        {
            ThrowError( xfs( "Collision mesh needs '_c' suffix [%s], File [%s]", LowName, Context().m_FastCollision ) );
        }
        LowName[Length - 2] = 0;

        s32 iHighMesh = 0;
        for( ; iHighMesh < (s32)HighMesh.Render.SubMeshes.size(); iHighMesh++ )
        {
            if( x_stricmp( HighMesh.Render.SubMeshes[iHighMesh].Name.c_str(), LowName ) == 0 )
            {
                break;
            }
        }

        if( iHighMesh == (s32)HighMesh.Render.SubMeshes.size() )
        {
            ThrowError( xfs( "Unreferenced collision mesh [%s], File [%s]",
                             LowMesh.Render.SubMeshes[iLowMesh].Name.c_str(), Context().m_FastCollision ) );
        }
        if( Context().m_MeshToCompiled[iHighMesh] < 0 )
        {
            ThrowError( xfs( "Collision mesh references empty render mesh [%s]",
                             HighMesh.Render.SubMeshes[iHighMesh].Name.c_str() ) );
        }
        if( HighBone[iHighMesh] < 0 )
        {
            ThrowError(
                xfs( "Mesh [%s] is not attached to a bone", HighMesh.Render.SubMeshes[iHighMesh].Name.c_str() ) );
        }

        LowToHigh[iLowMesh] = iHighMesh;
    }

    struct low_triangle
    {
        vector3 Point[4];
        s32 nPoints;
        s32 iLowMesh;
        vector3 Normal;
        s32 iMesh;
        s32 iBone;
    };

    xarray<low_triangle> Triangles;
    s32 nDegenerateFacets = 0;

    auto SamePoint = []( const vector3& A, const vector3& B ) { return ( A - B ).Length() < 0.00001f; };

    for( s32 iLowMesh = 0; iLowMesh < (s32)LowMesh.Render.SubMeshes.size(); ++iLowMesh )
    {
        xarray<s32> MeshFacets;
        for( s32 iFacet = 0; iFacet < (s32)LowMesh.Render.Facets.size(); ++iFacet )
        {
            if( LowMesh.Render.Facets[iFacet].iMesh == iLowMesh )
                MeshFacets.Append( iFacet );
        }

        const s32 iHighMesh = LowToHigh[iLowMesh];
        for( s32 iMeshFacet = 0; iMeshFacet < MeshFacets.GetCount(); )
        {
            const geometry_facet& Facet = LowMesh.Render.Facets[MeshFacets[iMeshFacet]];
            if( Facet.nVertices != 3 )
                ThrowError( "Low collision supports only triangle facets" );

            low_triangle& Triangle = Triangles.Append();
            Triangle.nPoints = 3;
            Triangle.iLowMesh = iLowMesh;
            Triangle.iMesh = Context().m_MeshToCompiled[iHighMesh];
            Triangle.iBone = HighBone[iHighMesh];
            for( s32 i = 0; i < 3; ++i )
            {
                const s32 iVertex = Facet.iVertex[i];
                if( ( iVertex < 0 ) || ( iVertex >= (s32)LowMesh.Render.Vertices.size() ) )
                    ThrowError( "Low collision facet references an invalid vertex" );
                Triangle.Point[i] = LowMesh.Render.Vertices[iVertex].Position;
            }

            // Recover the exporter quad split before building collision_data.
            if( iMeshFacet + 1 < MeshFacets.GetCount() )
            {
                const geometry_facet& Next = LowMesh.Render.Facets[MeshFacets[iMeshFacet + 1]];
                if( Next.nVertices == 3 )
                {
                    const vector3& N0 = LowMesh.Render.Vertices[Next.iVertex[0]].Position;
                    const vector3& N1 = LowMesh.Render.Vertices[Next.iVertex[1]].Position;
                    if( ( Facet.iPrimitive < 0 || Next.iPrimitive == Facet.iPrimitive ) &&
                        SamePoint( N0, Triangle.Point[0] ) && SamePoint( N1, Triangle.Point[2] ) )
                    {
                        const s32 iVertex = Next.iVertex[2];
                        if( ( iVertex < 0 ) || ( iVertex >= (s32)LowMesh.Render.Vertices.size() ) )
                            ThrowError( "Low collision facet references an invalid vertex" );
                        Triangle.Point[3] = LowMesh.Render.Vertices[iVertex].Position;
                        Triangle.nPoints = 4;
                        ++iMeshFacet;
                    }
                }
            }

            Triangle.Normal = v3_Cross( Triangle.Point[1] - Triangle.Point[0], Triangle.Point[2] - Triangle.Point[0] );
            if( Triangle.Normal.Length() < 0.00001f )
            {
                Triangles.Delete( Triangles.GetCount() - 1 );
                nDegenerateFacets++;
                ++iMeshFacet;
                continue;
            }
            Triangle.Normal.Normalize();
            ++iMeshFacet;
        }
    }

    if( nDegenerateFacets > 0 )
    {
        ReportWarning( xfs( "Ignored %d degenerate low-collision triangle(s)", nDegenerateFacets ) );
    }

    const s32 MaxTrianglesPerCluster = 10;
    s32 nClusters = 0;
    for( s32 iTriangle = 0; iTriangle < Triangles.GetCount(); )
    {
        const s32 iMesh = Triangles[iTriangle].iMesh;
        const s32 iBone = Triangles[iTriangle].iBone;
        const s32 iLowMesh = Triangles[iTriangle].iLowMesh;
        s32 nTriangles = 0;
        while( ( iTriangle + nTriangles < Triangles.GetCount() ) && ( nTriangles < MaxTrianglesPerCluster ) &&
               ( Triangles[iTriangle + nTriangles].iMesh == iMesh ) &&
               ( Triangles[iTriangle + nTriangles].iBone == iBone ) &&
               ( Triangles[iTriangle + nTriangles].iLowMesh == iLowMesh ) )
        {
            nTriangles++;
        }
        iTriangle += nTriangles;
        nClusters++;
    }

    Collision.nLowClusters = nClusters;
    Collision.nLowQuads = Triangles.GetCount();
    Collision.nLowVectors = 0;
    for( s32 i = 0; i < Triangles.GetCount(); ++i )
        Collision.nLowVectors += Triangles[i].nPoints + 1;
    const s32 MaxLowVectors = Collision.nLowVectors;
    Collision.pLowCluster = nClusters > 0 ? new collision_data::low_cluster[nClusters] : NULL;
    Collision.pLowQuad = Collision.nLowQuads > 0 ? new collision_data::low_quad[Collision.nLowQuads] : NULL;
    Collision.pLowVector = Collision.nLowVectors > 0 ? new vector3[Collision.nLowVectors] : NULL;

    s32 iCluster = 0;
    s32 iTriangle = 0;
    s32 iVector = 0;
    while( iTriangle < Triangles.GetCount() )
    {
        const s32 iFirstTriangle = iTriangle;
        const s32 iMesh = Triangles[iTriangle].iMesh;
        const s32 iBone = Triangles[iTriangle].iBone;
        const s32 iLowMesh = Triangles[iTriangle].iLowMesh;
        while( ( iTriangle < Triangles.GetCount() ) && ( ( iTriangle - iFirstTriangle ) < MaxTrianglesPerCluster ) &&
               ( Triangles[iTriangle].iMesh == iMesh ) && ( Triangles[iTriangle].iBone == iBone ) &&
               ( Triangles[iTriangle].iLowMesh == iLowMesh ) )
        {
            iTriangle++;
        }

        collision_data::low_cluster& Cluster = Collision.pLowCluster[iCluster++];
        Cluster.BBox.Clear();
        Cluster.iVectorOffset = iVector;
        Cluster.nPoints = 0;
        Cluster.nNormals = iTriangle - iFirstTriangle;
        Cluster.iQuadOffset = iFirstTriangle;
        Cluster.nQuads = iTriangle - iFirstTriangle;
        Cluster.iMesh = iMesh;
        Cluster.iBone = iBone;

        std::vector<std::array<byte, 4>> PointIndices( Cluster.nQuads );
        for( s32 i = 0; i < Cluster.nQuads; i++ )
        {
            const low_triangle& Triangle = Triangles[iFirstTriangle + i];
            for( s32 j = 0; j < Triangle.nPoints; ++j )
            {
                s32 iPoint = -1;
                for( s32 k = 0; k < Cluster.nPoints; ++k )
                {
                    const vector3& Existing = Collision.pLowVector[Cluster.iVectorOffset + k];
                    if( ( Existing - Triangle.Point[j] ).Length() < 0.00001f )
                    {
                        iPoint = k;
                        break;
                    }
                }
                if( iPoint < 0 )
                {
                    iPoint = Cluster.nPoints++;
                    Collision.pLowVector[Cluster.iVectorOffset + iPoint] = Triangle.Point[j];
                }
                PointIndices[i][j] = (byte)iPoint;
            }

            collision_data::low_quad& Quad = Collision.pLowQuad[Cluster.iQuadOffset + i];
            Quad.iP[0] = PointIndices[i][0];
            Quad.iP[1] = PointIndices[i][1];
            Quad.iP[2] = PointIndices[i][2];
            Quad.iP[3] = ( Triangle.nPoints == 4 ) ? PointIndices[i][3] : PointIndices[i][0];
            Quad.iN = (byte)i;
            Quad.Flags = 0;

            for( s32 j = 0; j < Triangle.nPoints; ++j )
                Cluster.BBox += Triangle.Point[j];
        }

        for( s32 i = 0; i < Cluster.nNormals; ++i )
        {
            Collision.pLowVector[Cluster.iVectorOffset + Cluster.nPoints + i] = Triangles[iFirstTriangle + i].Normal;
        }

        iVector += Cluster.nPoints + Cluster.nNormals;
    }

    ASSERT( iVector <= MaxLowVectors );
    Collision.nLowVectors = iVector;

    ASSERT( iCluster == Collision.nLowClusters );
    ASSERT( iTriangle == Collision.nLowQuads );
    ASSERT( iVector == Collision.nLowVectors );
}

//=========================================================================

void geometry_product_writer::CompileLowCollisionFromBBox( rigid_geom& RigidGeom, geometry_asset& HighMesh )
{
    s32 iTargetMesh = 0;
    while( ( iTargetMesh < Context().m_MeshToCompiled.GetCount() ) && ( Context().m_MeshToCompiled[iTargetMesh] < 0 ) )
    {
        iTargetMesh++;
    }
    if( iTargetMesh == Context().m_MeshToCompiled.GetCount() )
        ThrowError( "Cannot build collision for an empty mesh" );

    geometry_asset BBoxMesh;
    BBoxMesh.Skeleton.Bones.push_back( HighMesh.Skeleton.Bones.empty() ? geometry_bone() : HighMesh.Skeleton.Bones[0] );
    BBoxMesh.Materials.Items.push_back( HighMesh.Materials.Items.empty() ? geometry_material()
                                                                         : HighMesh.Materials.Items[0] );
    BBoxMesh.Render.SubMeshes.push_back( HighMesh.Render.SubMeshes[iTargetMesh] );
    BBoxMesh.Render.SubMeshes[0].Name += "_c";
    BBoxMesh.Render.Vertices.resize( 8 );

    bbox HighBBox = HighMesh.GetBBox();

    // Inflate thin bounds.
    {
        f32 MinSize = 1.0f;
        for( s32 i = 0; i < 3; i++ )
        {
            if( ( HighBBox.Max[i] - HighBBox.Min[i] ) < MinSize )
            {
                f32 Center = ( HighBBox.Max[i] + HighBBox.Min[i] ) * 0.5f;
                HighBBox.Min[i] = Center - MinSize * 0.5f;
                HighBBox.Max[i] = Center + MinSize * 0.5f;
            }
        }
    }

    // Convert bounds to eight corners.
    static byte CornerIndices[8 * 3] = { 0, 1, 2,   // 0: MinX, MinY, MinZ
                                         4, 1, 2,   // 1: MaxX, MinY, MinZ
                                         0, 5, 2,   // 2: MinX, MaxY, MinZ
                                         4, 5, 2,   // 3: MaxX, MaxY, MinZ
                                         0, 1, 6,   // 4: MinX, MinY, MaxZ
                                         4, 1, 6,   // 5: MaxX, MinY, MaxZ
                                         0, 5, 6,   // 6: MinX, MaxY, MaxZ
                                         4, 5, 6 }; // 7: MaxX, MaxY, MaxZ;

    // Convert corners to four-sided faces.
    static byte SideIndices[6 * 4] = { 4, 6, 2, 0,   // -X
                                       1, 3, 7, 5,   // +X
                                       4, 0, 1, 5,   // -Y
                                       2, 6, 7, 3,   // +Y
                                       0, 2, 3, 1,   // -Z
                                       5, 7, 6, 4 }; // +Z

    // Build corners.
    vector3 Corner[8];
    {
        const byte* pI = CornerIndices;
        const f32* pBBoxF = &HighBBox.Min.GetX();
        for( s32 i = 0; i < 8; i++ )
        {
            BBoxMesh.Render.Vertices[i].Position.Set( pBBoxF[pI[0]], pBBoxF[pI[1]], pBBoxF[pI[2]] );
            BBoxMesh.Render.Vertices[i].Weight.resize( 1 );
            BBoxMesh.Render.Vertices[i].Weight[0].iBone = 0;
            BBoxMesh.Render.Vertices[i].Weight[0].Weight = 1.0f;
            pI += 3;
        }
    }

    // Build two triangles per side.
    {
        const byte* pI = SideIndices;
        s32 iFacet = 0;
        for( s32 i = 0; i < 6; i++, pI += 4 )
        {
            // Build the side normal.
            plane BBoxPlane( BBoxMesh.Render.Vertices[pI[0]].Position, BBoxMesh.Render.Vertices[pI[1]].Position,
                             BBoxMesh.Render.Vertices[pI[2]].Position );

            // Check high-mesh faces with this normal.
            s32 f;
            for( f = 0; f < (s32)HighMesh.Render.Facets.size(); f++ )
            {
                // Get the facet.
                geometry_facet& Facet = HighMesh.Render.Facets[f];

                // Build the facet normal.
                plane FacetPlane( HighMesh.Render.Vertices[Facet.iVertex[0]].Position,
                                  HighMesh.Render.Vertices[Facet.iVertex[1]].Position,
                                  HighMesh.Render.Vertices[Facet.iVertex[2]].Position );

                // Keep faces pointing away from the volume.
                if( v3_Dot( BBoxPlane.Normal, FacetPlane.Normal ) > 0.0f )
                    break;

                // Two-sided and alpha materials are visible from both sides.
                geometry_material& Mat = HighMesh.Materials.Items[Facet.iMaterial];
                if( ( Mat.bTwoSided )                               // Two-sided.
                    || ( Mat.Map[Max_PunchThrough].iTexture != -1 ) // Punch-through.
                    || ( Mat.Type == Material_Alpha )               // Alpha blend.
                    || ( Mat.Type == Material_Alpha_PerPolyEnv ) || ( Mat.Type == Material_Alpha_PerPixelIllum ) ||
                    ( Mat.Type == Material_Alpha_PerPolyIllum ) )
                {
                    // Keep the opposite-facing side.
                    if( v3_Dot( BBoxPlane.Normal, FacetPlane.Normal ) < 0.0f )
                        break;
                }
            }

            // Skip sides without matching faces.
            if( f == (s32)HighMesh.Render.Facets.size() )
                continue;

            // Add facets.
            geometry_facet& Facet0 = BBoxMesh.Render.Facets.emplace_back();
            Facet0.iMaterial = 0;
            Facet0.iMesh = 0;
            Facet0.nVertices = 3;
            Facet0.iVertex[0] = pI[0];
            Facet0.iVertex[1] = pI[1];
            Facet0.iVertex[2] = pI[2];
            Facet0.Plane.Setup( 1, 0, 0, 0 );
            iFacet++;

            geometry_facet& Facet1 = BBoxMesh.Render.Facets.emplace_back();
            Facet1.iMaterial = 0;
            Facet1.iMesh = 0;
            Facet1.nVertices = 3;
            Facet1.iVertex[0] = pI[0];
            Facet1.iVertex[1] = pI[2];
            Facet1.iVertex[2] = pI[3];
            Facet1.Plane.Setup( 1, 0, 0, 0 );
            iFacet++;
        }
    }

    CompileLowCollision( RigidGeom, BBoxMesh, HighMesh );
}

//=========================================================================
//  BUILDING HIGH POLY COLLISION GEOMETRY
//=========================================================================

void geometry_product_writer::CompileHighCollisionFromGeometry( rigid_geom& RigidGeom,
                                                                collision_data::mat_info* pMaterialInfo )
{
    collision_data& Collision = RigidGeom.m_Collision;
    Collision.nHighClusters = 0;
    Collision.nHighIndices = 0;
    Collision.pHighCluster = NULL;
    Collision.pHighIndexToVert0 = NULL;
    Collision.BBox.Clear();

    if( !Context().m_bDoCollision )
    {
        Collision.BBox = RigidGeom.m_BBox;
        return;
    }

    s32 nTriangles = 0;
    for( s32 i = 0; i < RigidGeom.m_nSections; i++ )
    {
        const rigid_geom::section& Section = RigidGeom.m_pSection[i];
        if( ( Section.nIndices % 3 ) != 0 )
            ThrowError( "Rigid section contains a partial triangle" );
        if( Section.nIndices > 32769 )
            ThrowError( "Rigid collision section exceeds the encoded triangle offset limit" );

        if( Section.nIndices > 0 )
        {
            Collision.nHighClusters++;
            nTriangles += Section.nIndices / 3;
        }
    }

    Collision.nHighIndices = nTriangles;
    Collision.pHighCluster =
        Collision.nHighClusters > 0 ? new collision_data::high_cluster[Collision.nHighClusters] : NULL;
    Collision.pHighIndexToVert0 = nTriangles > 0 ? new u16[nTriangles] : NULL;

    s32 iCluster = 0;
    s32 iTriangle = 0;
    for( s32 iMesh = 0; iMesh < RigidGeom.m_nMeshes; iMesh++ )
    {
        const geom::mesh& MeshInfo = RigidGeom.m_pMesh[iMesh];
        for( s32 i = 0; i < MeshInfo.nSubMeshes; i++ )
        {
            const geom::submesh& Submesh = RigidGeom.m_pSubMesh[MeshInfo.iSubMesh + i];

            for( s32 iSubSection = 0; iSubSection < Submesh.nSections; iSubSection++ )
            {
                const s32 iSection = Submesh.iSection + iSubSection;
                const rigid_geom::section& Section = RigidGeom.m_pSection[iSection];
                if( Section.nIndices == 0 )
                    continue;

                collision_data::high_cluster& Cluster = Collision.pHighCluster[iCluster++];
                Cluster.BBox.Clear();
                Cluster.nTris = Section.nIndices / 3;
                Cluster.iMesh = iMesh;
                Cluster.iBone = Section.iBone;
                Cluster.iSection = iSection;
                Cluster.iOffset = iTriangle;
                Cluster.MaterialInfo = pMaterialInfo[iSection];

                for( s32 iIndex = 0; iIndex < Section.nIndices; iIndex += 3 )
                {
                    Collision.pHighIndexToVert0[iTriangle++] = (u16)iIndex;

                    for( s32 iCorner = 0; iCorner < 3; iCorner++ )
                    {
                        const u32 iVertex = RigidGeom.m_pIndex[Section.FirstIndex + iIndex + iCorner];
                        Cluster.BBox += RigidGeom.m_pVertex[iVertex].Pos;
                    }
                }

                Cluster.BBox.Inflate( 1.0f, 1.0f, 1.0f );
                Collision.BBox += Cluster.BBox;
            }
        }
    }

    ASSERT( iCluster == Collision.nHighClusters );
    ASSERT( iTriangle == Collision.nHighIndices );
}

//=========================================================================

xbool RigidGeom_GetTriangle( const rigid_geom* pRigidGeom, s32 Key, vector3& P0, vector3& P1, vector3& P2 )
{
    if( !pRigidGeom || ( Key == -1 ) )
        return ( FALSE );

    const s32 iCluster = ( Key >> 16 ) & 0x7fff;
    const s32 iTriangle = Key & 0xffff;
    if( ( iCluster < 0 ) || ( iCluster >= pRigidGeom->m_Collision.nHighClusters ) )
    {
        return ( FALSE );
    }

    const collision_data::high_cluster& Cluster = pRigidGeom->m_Collision.pHighCluster[iCluster];
    if( ( iTriangle < 0 ) || ( iTriangle >= Cluster.nTris ) )
        return ( FALSE );

    const rigid_geom::section& Section = pRigidGeom->m_pSection[Cluster.iSection];
    const u16 EncodedIndex = pRigidGeom->m_Collision.pHighIndexToVert0[Cluster.iOffset + iTriangle];
    const s32 FirstIndex = EncodedIndex & 0x7fff;

    P0 = pRigidGeom->m_pVertex[pRigidGeom->m_pIndex[Section.FirstIndex + FirstIndex + 0]].Pos;
    P1 = pRigidGeom->m_pVertex[pRigidGeom->m_pIndex[Section.FirstIndex + FirstIndex + 1]].Pos;
    P2 = pRigidGeom->m_pVertex[pRigidGeom->m_pIndex[Section.FirstIndex + FirstIndex + 2]].Pos;
    return ( TRUE );
}

//=========================================================================
