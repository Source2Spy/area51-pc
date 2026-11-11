//=========================================================================
//
//  GeometryProductWriter_Rigid.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"
#include "GeometryProductFormat.hpp"
#include "Render/RigidGeom.hpp"
//=========================================================================

// Rigid model preparation and serialization.

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void geometry_product_writer::BuildRigidGeometry( mesh& Mesh, rigid_geom& RigidGeom )
{
    if( Mesh.SubMesh.GetCount() == 0 )
        ThrowError( "Rigid geometry contains no meshes" );

    const geometry_asset& RawMesh = Context().m_ActiveAsset;
    xarray<rigid_geom::vertex> Vertices;
    xarray<u32> Indices;
    xarray<s32> VertexRemap;

    const s32 MaxFacetsPerSection = 10923;
    s32 nSubMeshes = 0;
    s32 nSections = 0;
    for( s32 i = 0; i < Mesh.SubMesh.GetCount(); i++ )
    {
        const sub_mesh& SourceMesh = Mesh.SubMesh[i];
        nSubMeshes += SourceMesh.Surfaces.GetCount();
        for( s32 j = 0; j < SourceMesh.Surfaces.GetCount(); j++ )
        {
            const surface& SourceSurface = SourceMesh.Surfaces[j];
            if( SourceSurface.Sections.GetCount() > 0 )
            {
                for( s32 k = 0; k < SourceSurface.Sections.GetCount(); k++ )
                {
                    nSections +=
                        ( SourceSurface.Sections[k].Facets.GetCount() + MaxFacetsPerSection - 1 ) / MaxFacetsPerSection;
                }
            }
            else
            {
                nSections += ( SourceSurface.Facets.GetCount() + MaxFacetsPerSection - 1 ) / MaxFacetsPerSection;
            }
        }
    }

    if( nSections == 0 )
        ThrowError( "Rigid geometry contains no drawable surfaces" );

    RigidGeom.m_nMeshes = Mesh.SubMesh.GetCount();
    RigidGeom.m_pMesh = new geom::mesh[RigidGeom.m_nMeshes];
    RigidGeom.m_nSubMeshes = nSubMeshes;
    RigidGeom.m_pSubMesh = new geom::submesh[nSubMeshes];
    RigidGeom.m_nSections = nSections;
    RigidGeom.m_pSection = new rigid_geom::section[nSections];

    collision_data::mat_info* pMaterialInfo = new collision_data::mat_info[nSections];

    VertexRemap.SetCount( (s32)RawMesh.Render.Vertices.size() );
    RigidGeom.m_BBox.Clear();

    s32 iSubMesh = 0;
    s32 iSection = 0;
    for( s32 iMesh = 0; iMesh < Mesh.SubMesh.GetCount(); iMesh++ )
    {
        const sub_mesh& SourceMesh = Mesh.SubMesh[iMesh];
        geom::mesh& TargetMesh = RigidGeom.m_pMesh[iMesh];

        TargetMesh.BBox.Clear();
        TargetMesh.NameOffset = Context().m_Dictionary.Add( SourceMesh.Name );
        TargetMesh.iSubMesh = iSubMesh;
        TargetMesh.nSubMeshes = SourceMesh.Surfaces.GetCount();
        TargetMesh.nBones = RawMesh.Render.SubMeshes[SourceMesh.iAssetMesh].nBones;
        TargetMesh.nFaces = 0;
        TargetMesh.nVertices = 0;

        for( s32 iSurface = 0; iSurface < SourceMesh.Surfaces.GetCount(); iSurface++ )
        {
            const surface& SourceSurface = SourceMesh.Surfaces[iSurface];
            geom::submesh& Submesh = RigidGeom.m_pSubMesh[iSubMesh++];
            Submesh.iSection = iSection;
            Submesh.nSections = 0;
            if( SourceSurface.Sections.GetCount() > 0 )
            {
                for( s32 iSourceSection = 0; iSourceSection < SourceSurface.Sections.GetCount(); iSourceSection++ )
                {
                    Submesh.nSections +=
                        ( SourceSurface.Sections[iSourceSection].Facets.GetCount() + MaxFacetsPerSection - 1 ) /
                        MaxFacetsPerSection;
                }
            }
            else
            {
                Submesh.nSections = ( SourceSurface.Facets.GetCount() + MaxFacetsPerSection - 1 ) / MaxFacetsPerSection;
            }
            Submesh.iMaterial = SourceSurface.iMaterial;
            Submesh.WorldPixelSize = SourceSurface.WorldPixelSize;

            material& Material = Mesh.Material[SourceSurface.iMaterial];
            const geometry_material& RawMaterial = RawMesh.Materials.Items[Material.iAssetMaterial];

            const s32 nSourceSections = SourceSurface.Sections.GetCount() > 0 ? SourceSurface.Sections.GetCount() : 1;
            for( s32 iSourceSection = 0; iSourceSection < nSourceSections; iSourceSection++ )
            {
                const xarray<s32>* pSourceFacets = SourceSurface.Sections.GetCount() > 0
                                                       ? &SourceSurface.Sections[iSourceSection].Facets
                                                       : &SourceSurface.Facets;

                for( s32 iFirstFacet = 0; iFirstFacet < pSourceFacets->GetCount();
                     iFirstFacet += MaxFacetsPerSection, iSection++ )
                {
                    rigid_geom::section& Section = RigidGeom.m_pSection[iSection];

                    for( s32 i = 0; i < VertexRemap.GetCount(); i++ )
                        VertexRemap[i] = -1;

                    Section.FirstVertex = Vertices.GetCount();
                    Section.FirstIndex = Indices.GetCount();
                    Section.iBone = SourceSurface.iBone;
                    Section.iColor = Section.FirstVertex;

                    const s32 iEndFacet = x_min( iFirstFacet + MaxFacetsPerSection, pSourceFacets->GetCount() );
                    for( s32 iFacet = iFirstFacet; iFacet < iEndFacet; iFacet++ )
                    {
                        const geometry_facet& Facet = RawMesh.Render.Facets[( *pSourceFacets )[iFacet]];

                        if( Facet.nVertices != 3 )
                            ThrowError( "Only triangle facets are supported" );

                        for( s32 iCorner = 0; iCorner < 3; iCorner++ )
                        {
                            const s32 iRawVertex = Facet.iVertex[iCorner];
                            if( ( iRawVertex < 0 ) || ( iRawVertex >= (s32)RawMesh.Render.Vertices.size() ) )
                                ThrowError( "Facet references an invalid vertex" );

                            s32 iVertex = VertexRemap[iRawVertex];
                            if( iVertex == -1 )
                            {
                                const geometry_vertex& SourceVertex = RawMesh.Render.Vertices[iRawVertex];
                                if( SourceVertex.Normal.empty() || SourceVertex.UV.empty() )
                                    ThrowError( "Rigid vertex is missing a normal or UV set" );

                                rigid_geom::vertex& Vertex = Vertices.Append();
                                Vertex.Pos = SourceVertex.Position;
                                Vertex.Normal = SourceVertex.Normal[0];
                                Vertex.UV = SourceVertex.UV[0];

                                iVertex = Vertices.GetCount() - 1;
                                VertexRemap[iRawVertex] = iVertex;
                                TargetMesh.BBox += SourceVertex.Position;
                            }

                            Indices.Append( (u32)iVertex );
                        }
                    }

                    Section.nVertices = Vertices.GetCount() - Section.FirstVertex;
                    Section.nIndices = Indices.GetCount() - Section.FirstIndex;
                    TargetMesh.nFaces += Section.nIndices / 3;
                    TargetMesh.nVertices += Section.nVertices;

                    pMaterialInfo[iSection].SoundType = Material.TexInfo.SoundMat;
                    pMaterialInfo[iSection].Flags = 0;
                    if( RawMaterial.bTwoSided )
                    {
                        pMaterialInfo[iSection].Flags |= collision_data::mat_info::FLAG_DOUBLESIDED;
                    }

                    switch( RawMaterial.Type )
                    {
                    case Material_Alpha:
                    case Material_Alpha_PerPolyEnv:
                    case Material_Alpha_PerPixelIllum:
                    case Material_Alpha_PerPolyIllum:
                        pMaterialInfo[iSection].Flags |= collision_data::mat_info::FLAG_TRANSPARENT;
                        break;

                    default:
                        break;
                    }
                }
            }
        }

        RigidGeom.m_BBox += TargetMesh.BBox;
        x_printf( "%30s - %10d Vertices\n", SourceMesh.Name, TargetMesh.nVertices );
    }

    ASSERT( iSubMesh == RigidGeom.m_nSubMeshes );
    ASSERT( iSection == RigidGeom.m_nSections );

    RigidGeom.m_nFaces = 0;
    RigidGeom.m_nVertices = 0;
    for( s32 i = 0; i < RigidGeom.m_nMeshes; i++ )
    {
        RigidGeom.m_nFaces += RigidGeom.m_pMesh[i].nFaces;
        RigidGeom.m_nVertices += RigidGeom.m_pMesh[i].nVertices;
    }

    RigidGeom.m_nVertexData = Vertices.GetCount();
    RigidGeom.m_pVertex = new rigid_geom::vertex[RigidGeom.m_nVertexData];
    if( RigidGeom.m_nVertexData > 0 )
    {
        x_memcpy( RigidGeom.m_pVertex, Vertices.GetPtr(), RigidGeom.m_nVertexData * sizeof( rigid_geom::vertex ) );
    }

    RigidGeom.m_nIndices = Indices.GetCount();
    RigidGeom.m_pIndex = new u32[RigidGeom.m_nIndices];
    if( RigidGeom.m_nIndices > 0 )
    {
        x_memcpy( RigidGeom.m_pIndex, Indices.GetPtr(), RigidGeom.m_nIndices * sizeof( u32 ) );
    }

    ExportMaterial( Mesh, RigidGeom );
    xarray<s32> MaterialIndices;
    MaterialIndices.SetCount( Mesh.Material.GetCount() );
    for( s32 i = 0; i < Mesh.Material.GetCount(); ++i )
        MaterialIndices[i] = Mesh.Material[i].iAssetMaterial;
    ApplyTextureDescriptors( RigidGeom, RawMesh, MaterialIndices.GetPtr(), MaterialIndices.GetCount() );
    ExportVirtualMeshes( Context().m_ActiveAsset, RigidGeom );
    CompileHighCollisionFromGeometry( RigidGeom, pMaterialInfo );

    // Restore rawgeom cluster metadata after collision rebuild.
    ApplyCollisionMetadata( RigidGeom, Context().m_ActiveAsset );

    for( s32 i = 0; i < RigidGeom.m_Collision.nLowClusters; i++ )
        RigidGeom.m_BBox += RigidGeom.m_Collision.pLowCluster[i].BBox;

    delete[] pMaterialInfo;
    CompileDictionary( RigidGeom );
}

//=========================================================================
