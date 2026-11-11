//=========================================================================
//
//  GeometryProductWriter_Skin.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"
#include "GeometryProductFormat.hpp"
#include "Render/SkinGeom.hpp"

// Skin model preparation and serialization.

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void geometry_product_writer::BuildSkinGeometry( mesh& Mesh, skin_geom& SkinGeom )
{
    if( Mesh.SubMesh.GetCount() == 0 )
        ThrowError( "Skin geometry contains no meshes" );

    const geometry_asset& RawMesh = Context().m_ActiveAsset;
    if( RawMesh.Skeleton.Bones.size() > 96 )
        ThrowError( "Skin geometry exceeds the 96-bone compiled palette limit" );

    xarray<skin_geom::vertex> Vertices;
    xarray<u32> Indices;
    xarray<s32> VertexRemap;

    const s32 MaxFacetsPerSection = 21845;
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
        ThrowError( "Skin geometry contains no drawable surfaces" );

    SkinGeom.m_nMeshes = Mesh.SubMesh.GetCount();
    SkinGeom.m_pMesh = new geom::mesh[SkinGeom.m_nMeshes];
    SkinGeom.m_nSubMeshes = nSubMeshes;
    SkinGeom.m_pSubMesh = new geom::submesh[nSubMeshes];
    SkinGeom.m_nSections = nSections;
    SkinGeom.m_pSection = new skin_geom::section[nSections];

    SkinGeom.m_nBonePalette = (s32)RawMesh.Skeleton.Bones.size();
    SkinGeom.m_pBonePalette = RawMesh.Skeleton.Bones.size() > 0 ? new u16[RawMesh.Skeleton.Bones.size()] : NULL;
    for( s32 i = 0; i < (s32)RawMesh.Skeleton.Bones.size(); i++ )
        SkinGeom.m_pBonePalette[i] = (u16)i;

    VertexRemap.SetCount( (s32)RawMesh.Render.Vertices.size() );
    SkinGeom.m_BBox.Clear();

    s32 iSubMesh = 0;
    s32 iSection = 0;
    for( s32 iMesh = 0; iMesh < Mesh.SubMesh.GetCount(); iMesh++ )
    {
        const sub_mesh& SourceMesh = Mesh.SubMesh[iMesh];
        geom::mesh& TargetMesh = SkinGeom.m_pMesh[iMesh];

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
            geom::submesh& Submesh = SkinGeom.m_pSubMesh[iSubMesh++];
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

            const s32 nSourceSections = SourceSurface.Sections.GetCount() > 0 ? SourceSurface.Sections.GetCount() : 1;
            for( s32 iSourceSection = 0; iSourceSection < nSourceSections; iSourceSection++ )
            {
                const xarray<s32>* pSourceFacets = SourceSurface.Sections.GetCount() > 0
                                                       ? &SourceSurface.Sections[iSourceSection].Facets
                                                       : &SourceSurface.Facets;

                for( s32 iFirstFacet = 0; iFirstFacet < pSourceFacets->GetCount();
                     iFirstFacet += MaxFacetsPerSection, iSection++ )
                {
                    skin_geom::section& Section = SkinGeom.m_pSection[iSection];

                    for( s32 i = 0; i < VertexRemap.GetCount(); i++ )
                        VertexRemap[i] = -1;

                    Section.FirstVertex = Vertices.GetCount();
                    Section.FirstIndex = Indices.GetCount();
                    Section.FirstBone = 0;
                    Section.nBones = (s32)RawMesh.Skeleton.Bones.size();

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
                                if( SourceVertex.Normal.empty() || SourceVertex.UV.empty() ||
                                    SourceVertex.Weight.empty() || SourceVertex.Weight.size() > 2 )
                                {
                                    ThrowError( "Skin vertex has invalid render attributes" );
                                }

                                skin_geom::vertex& Vertex = Vertices.Append();
                                Vertex.Position = SourceVertex.Position;
                                Vertex.Normal = SourceVertex.Normal[0];
                                Vertex.UV = SourceVertex.UV[0];
                                Vertex.Weights.X = SourceVertex.Weight[0].Weight;
                                Vertex.Weights.Y =
                                    SourceVertex.Weight.size() > 1 ? SourceVertex.Weight[1].Weight : 0.0f;

                                const s32 Bone0 =
                                    !SourceVertex.Weight[0].BoneName.empty()
                                        ? RawMesh.GetBoneIDFromName( SourceVertex.Weight[0].BoneName.c_str() )
                                        : SourceVertex.Weight[0].iBone;
                                const s32 Bone1 =
                                    SourceVertex.Weight.size() > 1
                                        ? ( !SourceVertex.Weight[1].BoneName.empty()
                                                ? RawMesh.GetBoneIDFromName( SourceVertex.Weight[1].BoneName.c_str() )
                                                : SourceVertex.Weight[1].iBone )
                                        : 0;
                                if( ( Bone0 < 0 ) || ( Bone0 >= (s32)RawMesh.Skeleton.Bones.size() ) || ( Bone1 < 0 ) ||
                                    ( Bone1 >= (s32)RawMesh.Skeleton.Bones.size() ) )
                                {
                                    ThrowError( "Skin vertex references an invalid bone" );
                                }

                                Vertex.Bones[0] = (u16)Bone0;
                                Vertex.Bones[1] = (u16)Bone1;

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
                }
            }
        }

        SkinGeom.m_BBox += TargetMesh.BBox;
    }

    ASSERT( iSubMesh == SkinGeom.m_nSubMeshes );
    ASSERT( iSection == SkinGeom.m_nSections );

    SkinGeom.m_nFaces = 0;
    SkinGeom.m_nVertices = 0;
    for( s32 i = 0; i < SkinGeom.m_nMeshes; i++ )
    {
        SkinGeom.m_nFaces += SkinGeom.m_pMesh[i].nFaces;
        SkinGeom.m_nVertices += SkinGeom.m_pMesh[i].nVertices;
    }

    SkinGeom.m_nVertexData = Vertices.GetCount();
    SkinGeom.m_pVertex = new skin_geom::vertex[SkinGeom.m_nVertexData];
    if( SkinGeom.m_nVertexData > 0 )
    {
        x_memcpy( SkinGeom.m_pVertex, Vertices.GetPtr(), SkinGeom.m_nVertexData * sizeof( skin_geom::vertex ) );
    }

    SkinGeom.m_nIndices = Indices.GetCount();
    SkinGeom.m_pIndex = new u32[SkinGeom.m_nIndices];
    if( SkinGeom.m_nIndices > 0 )
    {
        x_memcpy( SkinGeom.m_pIndex, Indices.GetPtr(), SkinGeom.m_nIndices * sizeof( u32 ) );
    }

    ExportMaterial( Mesh, SkinGeom );
    xarray<s32> MaterialIndices;
    MaterialIndices.SetCount( Mesh.Material.GetCount() );
    for( s32 i = 0; i < Mesh.Material.GetCount(); ++i )
        MaterialIndices[i] = Mesh.Material[i].iAssetMaterial;
    ApplyTextureDescriptors( SkinGeom, RawMesh, MaterialIndices.GetPtr(), MaterialIndices.GetCount() );
    ExportVirtualMeshes( Context().m_ActiveAsset, SkinGeom );
    CompileDictionary( SkinGeom );
}

//=========================================================================
