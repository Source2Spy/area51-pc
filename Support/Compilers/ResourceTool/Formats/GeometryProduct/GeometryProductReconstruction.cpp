//=========================================================================
//
//  GeometryProductReconstruction.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductReconstruction.hpp"
#include "GeometryProductCollisionStage.hpp"
#include "GeometryProductLodStage.hpp"
#include "GeometryProductMaterialStage.hpp"
#include "GeometryProductPhysicsStage.hpp"

#include <cmath>
#include <string>

#include <type_traits>

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

std::string BoneName( s32 iBone )
{
    return iBone == 0 ? "GeomRoot" : (const char*)xfs( "Bone_%d", iBone );
}

//=========================================================================

void PopulateRenderIdentity( const geometry_product_data& Source, geometry_asset& Asset )
{
    Asset.Render.SubMeshes.resize( Source.MeshCount );
    for( s32 iMesh = 0; iMesh < Source.MeshCount; ++iMesh )
    {
        geometry_sub_mesh& Mesh = Asset.Render.SubMeshes[iMesh];
        Mesh.Name = Source.GetMeshName( iMesh );
        if( Mesh.Name.empty() )
            Mesh.Name = (const char*)xfs( "Mesh_%d", iMesh );
        Mesh.nBones = Source.Meshes[iMesh].nBones;
    }

    Asset.Skeleton.Bones.resize( Source.BoneCount );
    for( s32 iBone = 0; iBone < Source.BoneCount; ++iBone )
    {
        const geometry_product_data::bone& SourceBone = Source.Bones[iBone];
        geometry_bone& Bone = Asset.Skeleton.Bones[iBone];
        Bone.Name = BoneName( iBone );
        Bone.iParent = -1;
        Bone.ParentName.clear();
        Bone.Position = SourceBone.BindPosition;
        Bone.Rotation = SourceBone.BindRotation;
        Bone.Scale.Set( 1.0f, 1.0f, 1.0f );
        Bone.InverseBindMatrix.Setup( Bone.Scale, Bone.Rotation, Bone.Position );
        Bone.InverseBindMatrix.InvertSRT();
        Bone.BBox = SourceBone.BBox;
        Bone.HitLocation = SourceBone.HitLocation;
        Bone.iRigidBody = SourceBone.iRigidBody;
        if( SourceBone.iRigidBody >= 0 && SourceBone.iRigidBody < Source.RigidBodyCount )
            Bone.RigidBodyName = Source.GetRigidBodyName( SourceBone.iRigidBody );
    }
    for( s32 iBone = 0; iBone < (s32)Asset.Skeleton.Bones.size(); ++iBone )
    {
        const s32 Parent = Asset.Skeleton.Bones[iBone].iParent;
        if( Parent >= 0 && Parent < (s32)Asset.Skeleton.Bones.size() )
            ++Asset.Skeleton.Bones[Parent].nChildren;
    }
}

//=========================================================================

template <typename ProductGeom> void PopulateRenderTopology( const ProductGeom& Source, geometry_asset& Asset )
{
    s32 Primitive = 0;
    for( s32 iMesh = 0; iMesh < Source.MeshCount; ++iMesh )
    {
        const geometry_product_data::mesh& Mesh = Source.Meshes[iMesh];
        if( Mesh.iSubMesh < 0 || Mesh.nSubMeshes < 0 || Mesh.iSubMesh + Mesh.nSubMeshes > Source.SubMeshCount )
        {
            x_throw( xfs( "Product mesh %d has an invalid submesh range", iMesh ) );
        }

        for( s32 iSubMesh = 0; iSubMesh < Mesh.nSubMeshes; ++iSubMesh )
        {
            const geometry_product_data::submesh& SubMesh = Source.SubMeshes[Mesh.iSubMesh + iSubMesh];
            if( SubMesh.iSection < 0 || SubMesh.nSections < 0 ||
                SubMesh.iSection + SubMesh.nSections > Source.SectionCount )
            {
                x_throw( xfs( "Product submesh %d/%d has an invalid section range", iMesh, iSubMesh ) );
            }

            for( s32 iSection = 0; iSection < SubMesh.nSections; ++iSection )
            {
                const s32 SectionIndex = SubMesh.iSection + iSection;
                const typename ProductGeom::section& Section = Source.Sections[SectionIndex];
                if( Section.FirstVertex < 0 || Section.nVertices < 0 ||
                    Section.FirstVertex + Section.nVertices > Source.VertexDataCount || Section.FirstIndex < 0 ||
                    Section.nIndices < 0 || Section.FirstIndex + Section.nIndices > Source.IndexCount ||
                    ( Section.nIndices % 3 ) != 0 )
                {
                    x_throw( xfs( "Product section %d has invalid vertex/index bounds", SectionIndex ) );
                }

                std::vector<s32> VertexMap( Section.nVertices, -1 );
                for( s32 iVertex = 0; iVertex < Section.nVertices; ++iVertex )
                {
                    const s32 ProductVertexIndex = Section.FirstVertex + iVertex;
                    geometry_vertex& Vertex = Asset.Render.Vertices.emplace_back();
                    VertexMap[iVertex] = (s32)Asset.Render.Vertices.size() - 1;

                    if constexpr( std::is_same<ProductGeom, geometry_product_rigid_data>::value )
                    {
                        Vertex.Position = Source.Vertices[ProductVertexIndex].Pos;
                        Vertex.Normal.push_back( Source.Vertices[ProductVertexIndex].Normal );
                        Vertex.UV.push_back( Source.Vertices[ProductVertexIndex].UV );
                        if( Section.iBone >= 0 && Section.iBone < Source.BoneCount )
                        {
                            geometry_weight& Weight = Vertex.Weight.emplace_back();
                            Weight.iBone = Section.iBone;
                            Weight.BoneName = Asset.Skeleton.Bones[Section.iBone].Name;
                            Weight.Weight = 1.0f;
                        }
                    }
                    else
                    {
                        const typename geometry_product_skin_data::vertex& ProductVertexData =
                            Source.Vertices[ProductVertexIndex];
                        Vertex.Position = ProductVertexData.Position;
                        Vertex.Normal.push_back( ProductVertexData.Normal );
                        Vertex.UV.push_back( ProductVertexData.UV );
                        for( s32 iWeight = 0; iWeight < 2; ++iWeight )
                        {
                            const f32 WeightValue =
                                iWeight == 0 ? ProductVertexData.Weights.X : ProductVertexData.Weights.Y;
                            if( WeightValue <= 0.0f )
                                continue;
                            const s32 PaletteIndex = ProductVertexData.Bones[iWeight];
                            if( Section.FirstBone < 0 || PaletteIndex >= Section.nBones ||
                                Section.FirstBone + PaletteIndex >= Source.BonePaletteCount )
                            {
                                x_throw( xfs( "Product skin vertex %d references invalid bone palette data",
                                              ProductVertexIndex ) );
                            }
                            const s32 BoneIndex = Source.BonePalette[Section.FirstBone + PaletteIndex];
                            if( BoneIndex < 0 || BoneIndex >= Source.BoneCount )
                                x_throw( "Product skin vertex references an invalid skeleton bone" );
                            geometry_weight& Weight = Vertex.Weight.emplace_back();
                            Weight.iBone = BoneIndex;
                            Weight.BoneName = Asset.Skeleton.Bones[BoneIndex].Name;
                            Weight.Weight = WeightValue;
                        }
                    }
                }

                const char* pMeshName = Asset.Render.SubMeshes[iMesh].Name.c_str();
                const std::string SubMeshName = (const char*)xfs( "%s/submesh_%d", pMeshName, iSubMesh );
                const std::string SectionName = (const char*)xfs( "%s/section_%d", SubMeshName.c_str(), iSection );
                const char* pMaterialName =
                    SubMesh.iMaterial >= 0 && SubMesh.iMaterial < (s32)Asset.Materials.Items.size()
                        ? Asset.Materials.Items[SubMesh.iMaterial].Name.c_str()
                        : "";
                const char* pBoneName = "";
                if constexpr( std::is_same<ProductGeom, geometry_product_rigid_data>::value )
                {
                    if( Section.iBone >= 0 && Section.iBone < (s32)Asset.Skeleton.Bones.size() )
                    {
                        pBoneName = Asset.Skeleton.Bones[Section.iBone].Name.c_str();
                    }
                }

                for( s32 iIndex = 0; iIndex < Section.nIndices; iIndex += 3 )
                {
                    const u32 A = Source.Indices[Section.FirstIndex + iIndex + 0];
                    const u32 B = Source.Indices[Section.FirstIndex + iIndex + 1];
                    const u32 C = Source.Indices[Section.FirstIndex + iIndex + 2];
                    if( A < (u32)Section.FirstVertex || B < (u32)Section.FirstVertex || C < (u32)Section.FirstVertex ||
                        A >= (u32)( Section.FirstVertex + Section.nVertices ) ||
                        B >= (u32)( Section.FirstVertex + Section.nVertices ) ||
                        C >= (u32)( Section.FirstVertex + Section.nVertices ) )
                    {
                        x_throw( xfs( "Product section %d index is outside its vertex range", SectionIndex ) );
                    }

                    geometry_facet& Facet = Asset.Render.Facets.emplace_back();
                    Facet.MeshName = pMeshName;
                    Facet.MaterialName = pMaterialName;
                    Facet.iMesh = iMesh;
                    Facet.iPrimitive = Primitive;
                    Facet.BoneName = pBoneName;
                    Facet.SubMeshName = SubMeshName;
                    Facet.SectionName = SectionName;
                    Facet.iSubMesh = iSubMesh;
                    Facet.iSection = iSection;
                    Facet.WorldPixelSize =
                        SubMesh.WorldPixelSize >= 0.000001f && std::isfinite( SubMesh.WorldPixelSize )
                            ? SubMesh.WorldPixelSize
                            : 0.001f;
                    Facet.iMaterial = SubMesh.iMaterial;
                    Facet.iVertex[0] = VertexMap[A - Section.FirstVertex];
                    Facet.iVertex[1] = VertexMap[B - Section.FirstVertex];
                    Facet.iVertex[2] = VertexMap[C - Section.FirstVertex];
                    Facet.Plane.Setup( Asset.Render.Vertices[Facet.iVertex[0]].Position,
                                       Asset.Render.Vertices[Facet.iVertex[1]].Position,
                                       Asset.Render.Vertices[Facet.iVertex[2]].Position );
                }
                ++Primitive;
            }
        }
    }
}

//=========================================================================

void PopulateProductSettings( const geometry_product_data& Source, geometry_asset& Asset )
{
    Asset.EngineMetadata.Clear();
    for( s32 iMask = 0; iMask < Source.BoneMaskCount; ++iMask )
    {
        const geometry_product_data::bone_masks& SourceMask = Source.BoneMasks[iMask];
        geometry_bone_mask& Mask = Asset.EngineMetadata.BoneMasks.emplace_back();
        Mask.Name = SourceMask.Name;
        for( s32 iBone = 0; iBone < SourceMask.nBones; ++iBone )
        {
            geometry_bone_mask_weight& Weight = Mask.Weights.emplace_back();
            Weight.iBone = iBone;
            Weight.BoneName = iBone < (s32)Asset.Skeleton.Bones.size() ? Asset.Skeleton.Bones[iBone].Name : "";
            Weight.Value = SourceMask.Weights[iBone];
        }
    }

    for( s32 iSection = 0; iSection < Source.PropertySectionCount; ++iSection )
    {
        const geometry_product_data::property_section& Section = Source.PropertySections[iSection];
        if( Section.iProperty < 0 || Section.nProperties < 0 ||
            Section.iProperty + Section.nProperties > Source.PropertyCount )
            x_throw( "Product property section is outside the property array" );
        for( s32 iProperty = 0; iProperty < Section.nProperties; ++iProperty )
        {
            const geometry_product_data::property& SourceProperty = Source.Properties[Section.iProperty + iProperty];
            geometry_engine_property& Property = Asset.EngineMetadata.Properties.emplace_back();
            Property.Section = Section.Name;
            Property.Name = SourceProperty.Name;
            switch( SourceProperty.Type )
            {
            case geometry_product_data::property::TYPE_FLOAT:
                Property.Type = "FLOAT";
                Property.Value = (const char*)xfs( "%g", SourceProperty.Float );
                break;
            case geometry_product_data::property::TYPE_INTEGER:
                Property.Type = "INTEGER";
                Property.Value = (const char*)xfs( "%d", SourceProperty.Integer );
                break;
            case geometry_product_data::property::TYPE_ANGLE:
                Property.Type = "ANGLE";
                Property.Value = (const char*)xfs( "%g", RAD_TO_DEG( SourceProperty.Angle ) );
                break;
            case geometry_product_data::property::TYPE_STRING:
                Property.Type = "STRING";
                Property.Value = SourceProperty.String;
                break;
            default:
                x_throw( xfs( "Product property %d has an unsupported type %d", iProperty, SourceProperty.Type ) );
            }
        }
    }
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_reconstruction::Execute( const geometry_product_snapshot& Product, geometry_asset& Asset,
                                                std::string& Error ) const
{
    (void)Error;

    Asset.Clear();
    PopulateRenderIdentity( Product.Geometry(), Asset );

    geometry_product_material_stage MaterialStage;
    if( !MaterialStage.Execute( Product.Geometry(), Asset, Error ) )
        return FALSE;

    if( Product.Kind() == geometry_product_kind::Rigid )
    {
        PopulateRenderTopology( Product.Rigid(), Asset );
    }
    else if( Product.Kind() == geometry_product_kind::Skin )
    {
        PopulateRenderTopology( Product.Skin(), Asset );
    }

    geometry_product_lod_stage LodStage;
    if( !LodStage.Execute( Product.Geometry(), Asset, Error ) )
        return FALSE;

    geometry_product_physics_stage PhysicsStage;
    if( !PhysicsStage.Execute( Product.Geometry(), Asset, Error ) )
        return FALSE;

    if( Product.Kind() == geometry_product_kind::Rigid )
    {
        geometry_product_collision_stage CollisionStage;
        if( !CollisionStage.Execute( Product.Rigid(), Asset, Error ) )
            return FALSE;
    }

    PopulateProductSettings( Product.Geometry(), Asset );
    return TRUE;
}

} // namespace resource_tool::geometry_product_format
