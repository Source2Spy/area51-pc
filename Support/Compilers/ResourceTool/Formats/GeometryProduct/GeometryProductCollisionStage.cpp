//=========================================================================
//
//  GeometryProductCollisionStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductCollisionStage.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

void PopulateCollision( const geometry_product_rigid_data& Source, geometry_asset& Asset )
{
    const geometry_product_collision_data& Collision = Source.Collision;
    Asset.Collision.Clear();
    Asset.Collision.CollisionBBox = Collision.BBox;
    Asset.Collision.HasCollisionBBox = TRUE;
    auto AppendTriangle = []( geometry_collision_mesh& Mesh, const vector3& P0, const vector3& P1, const vector3& P2,
                              const vector3& Normal )
    {
        const u32 Base = (u32)Mesh.Vertices.size();
        Mesh.Vertices.push_back( P0 );
        Mesh.Vertices.push_back( P1 );
        Mesh.Vertices.push_back( P2 );
        Mesh.Normals.push_back( Normal );
        Mesh.Normals.push_back( Normal );
        Mesh.Normals.push_back( Normal );
        Mesh.Indices.push_back( Base + 0 );
        Mesh.Indices.push_back( Base + 1 );
        Mesh.Indices.push_back( Base + 2 );
    };
    for( s32 iCluster = 0; iCluster < (s32)Collision.LowClusters.size(); ++iCluster )
    {
        const geometry_product_collision_data::low_cluster& SourceCluster = Collision.LowClusters[iCluster];
        geometry_collision_cluster& Cluster = Asset.Collision.Clusters.emplace_back();
        Cluster.High = FALSE;
        Cluster.BBox = SourceCluster.BBox;
        Cluster.Mesh = SourceCluster.iMesh;
        Cluster.Bone = SourceCluster.iBone;
        Cluster.VectorOffset = SourceCluster.iVectorOffset;
        Cluster.Points = SourceCluster.nPoints;
        Cluster.Normals = SourceCluster.nNormals;
        Cluster.QuadOffset = SourceCluster.iQuadOffset;
        Cluster.Count = SourceCluster.nQuads;
        if( Cluster.Mesh >= 0 && Cluster.Mesh < (s32)Asset.Render.SubMeshes.size() )
            Cluster.MeshName = Asset.Render.SubMeshes[Cluster.Mesh].Name;
        if( Cluster.Bone >= 0 && Cluster.Bone < (s32)Asset.Skeleton.Bones.size() )
            Cluster.BoneName = Asset.Skeleton.Bones[Cluster.Bone].Name;
        for( s32 iQuad = 0; iQuad < SourceCluster.nQuads; ++iQuad )
        {
            Cluster.QuadFlags.push_back( Collision.LowQuads[SourceCluster.iQuadOffset + iQuad].Flags );
        }
    }
    if( (s32)Collision.LowClusters.size() > 0 )
    {
        for( s32 iCluster = 0; iCluster < (s32)Collision.LowClusters.size(); ++iCluster )
        {
            const geometry_product_collision_data::low_cluster& SourceCluster = Collision.LowClusters[iCluster];
            if( SourceCluster.iVectorOffset < 0 || SourceCluster.nPoints < 0 || SourceCluster.nNormals < 0 ||
                SourceCluster.iQuadOffset < 0 || SourceCluster.nQuads < 0 ||
                SourceCluster.iVectorOffset + SourceCluster.nPoints + SourceCluster.nNormals >
                    (s32)Collision.LowVectors.size() ||
                SourceCluster.iQuadOffset + SourceCluster.nQuads > (s32)Collision.LowQuads.size() )
            {
                x_throw( "Product low collision cluster is outside its data arrays" );
            }
            geometry_collision_mesh& Mesh = Asset.Collision.Meshes.emplace_back();
            Mesh.Name = ( SourceCluster.iMesh >= 0 && SourceCluster.iMesh < (s32)Asset.Render.SubMeshes.size()
                              ? Asset.Render.SubMeshes[SourceCluster.iMesh].Name
                              : "Collision" ) +
                        "_c";
            for( s32 iQuad = 0; iQuad < SourceCluster.nQuads; ++iQuad )
            {
                const geometry_product_collision_data::low_quad& Quad =
                    Collision.LowQuads[SourceCluster.iQuadOffset + iQuad];
                if( iQuad >= SourceCluster.nNormals || Quad.iP[0] >= SourceCluster.nPoints ||
                    Quad.iP[1] >= SourceCluster.nPoints || Quad.iP[2] >= SourceCluster.nPoints ||
                    Quad.iP[3] >= SourceCluster.nPoints )
                {
                    x_throw( "Product low collision quad is outside its cluster" );
                }
                const vector3& Normal =
                    Collision.LowVectors[SourceCluster.iVectorOffset + SourceCluster.nPoints + iQuad];
                const vector3& P0 = Collision.LowVectors[SourceCluster.iVectorOffset + Quad.iP[0]];
                const vector3& P1 = Collision.LowVectors[SourceCluster.iVectorOffset + Quad.iP[1]];
                const vector3& P2 = Collision.LowVectors[SourceCluster.iVectorOffset + Quad.iP[2]];
                const vector3& P3 = Collision.LowVectors[SourceCluster.iVectorOffset + Quad.iP[3]];
                AppendTriangle( Mesh, P0, P1, P2, Normal );
                if( Quad.iP[3] != Quad.iP[0] )
                    AppendTriangle( Mesh, P0, P2, P3, Normal );
            }
        }
    }
    for( s32 iCluster = 0; iCluster < (s32)Collision.HighClusters.size(); ++iCluster )
    {
        const geometry_product_collision_data::high_cluster& SourceCluster = Collision.HighClusters[iCluster];
        geometry_collision_cluster& Cluster = Asset.Collision.Clusters.emplace_back();
        Cluster.High = TRUE;
        Cluster.BBox = SourceCluster.BBox;
        Cluster.Mesh = SourceCluster.iMesh;
        Cluster.Bone = SourceCluster.iBone;
        Cluster.Section = SourceCluster.iSection;
        Cluster.Offset = SourceCluster.iOffset;
        Cluster.Count = SourceCluster.nTris;
        Cluster.SoundType = SourceCluster.MaterialInfo.SoundType;
        Cluster.Flags = SourceCluster.MaterialInfo.Flags;
        if( Cluster.Mesh >= 0 && Cluster.Mesh < (s32)Asset.Render.SubMeshes.size() )
            Cluster.MeshName = Asset.Render.SubMeshes[Cluster.Mesh].Name;
        if( Cluster.Bone >= 0 && Cluster.Bone < (s32)Asset.Skeleton.Bones.size() )
            Cluster.BoneName = Asset.Skeleton.Bones[Cluster.Bone].Name;
        for( const geometry_facet& Facet : Asset.Render.Facets )
        {
            if( Facet.iMesh == Cluster.Mesh && Facet.iSection == Cluster.Section )
            {
                Cluster.SectionName = Facet.SectionName;
                break;
            }
        }
        for( s32 iTriangle = 0; iTriangle < SourceCluster.nTris; ++iTriangle )
            Cluster.TriangleOffsets.push_back( Collision.HighIndexToVert0[SourceCluster.iOffset + iTriangle] );
    }
    if( (s32)Collision.LowClusters.size() == 0 )
    {
        std::vector<s32> MeshMap( Source.MeshCount, -1 );
        for( s32 iCluster = 0; iCluster < (s32)Collision.HighClusters.size(); ++iCluster )
        {
            const geometry_product_collision_data::high_cluster& SourceCluster = Collision.HighClusters[iCluster];
            if( SourceCluster.iMesh < 0 || SourceCluster.iMesh >= Source.MeshCount || SourceCluster.iSection < 0 ||
                SourceCluster.iSection >= Source.SectionCount || SourceCluster.iOffset < 0 || SourceCluster.nTris < 0 ||
                SourceCluster.iOffset + SourceCluster.nTris > (s32)Collision.HighIndexToVert0.size() )
            {
                x_throw( "Product high collision cluster is outside its data arrays" );
            }
            s32& iMesh = MeshMap[SourceCluster.iMesh];
            if( iMesh < 0 )
            {
                iMesh = (s32)Asset.Collision.Meshes.size();
                geometry_collision_mesh& Mesh = Asset.Collision.Meshes.emplace_back();
                Mesh.Name = std::string( Source.GetMeshName( SourceCluster.iMesh ) ) + "_c";
            }
            geometry_collision_mesh& Mesh = Asset.Collision.Meshes[iMesh];
            const geometry_product_rigid_data::section& Section = Source.Sections[SourceCluster.iSection];
            for( s32 iTriangle = 0; iTriangle < SourceCluster.nTris; ++iTriangle )
            {
                const u16 Encoded = Collision.HighIndexToVert0[SourceCluster.iOffset + iTriangle];
                const s32 FirstIndex = Encoded & 0x7fff;
                if( FirstIndex < 0 || FirstIndex + 2 >= Section.nIndices )
                    x_throw( "Product high collision triangle is outside its section" );
                const u32 Vertex0 = Source.Indices[Section.FirstIndex + FirstIndex + 0];
                const u32 Vertex1 = Source.Indices[Section.FirstIndex + FirstIndex + 1];
                const u32 Vertex2 = Source.Indices[Section.FirstIndex + FirstIndex + 2];
                if( Vertex0 >= (u32)Source.VertexDataCount || Vertex1 >= (u32)Source.VertexDataCount ||
                    Vertex2 >= (u32)Source.VertexDataCount )
                    x_throw( "Product high collision triangle references an invalid vertex" );
                const vector3 P0 = Source.Vertices[Vertex0].Pos;
                const vector3 P1 = Source.Vertices[Vertex1].Pos;
                const vector3 P2 = Source.Vertices[Vertex2].Pos;
                vector3 Normal = v3_Cross( P1 - P0, P2 - P0 );
                if( Normal.SafeNormalize() )
                    AppendTriangle( Mesh, P0, P1, P2, Normal );
            }
        }
    }
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_collision_stage::Execute( const geometry_product_rigid_data& Source, geometry_asset& Asset,
                                                 std::string& Error ) const
{
    (void)Error;
    PopulateCollision( Source, Asset );
    return TRUE;
}

} // namespace resource_tool::geometry_product_format
