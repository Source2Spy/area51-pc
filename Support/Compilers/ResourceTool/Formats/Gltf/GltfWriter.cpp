//=========================================================================
//
//  GltfWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GltfWriter.hpp"
#include "GltfDocument.hpp"
#include "GeometryNames.hpp"
#include "Render/Material_Prefs.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

template <typename Item> s32 FindModelName( const std::vector<Item>& Items, const std::string& Name )
{
    for( s32 i = 0; i < (s32)Items.size(); ++i )
    {
        if( geometry_names_match( Items[i].Name.c_str(), Name.c_str() ) )
            return i;
    }
    return -1;
}

//=========================================================================

std::string JsonEscape( const char* pValue )
{
    std::string Result;
    for( const char* p = pValue ? pValue : ""; *p; ++p )
    {
        switch( *p )
        {
        case '"':
            Result += "\\\"";
            break;
        case '\\':
            Result += "\\\\";
            break;
        case '\n':
            Result += "\\n";
            break;
        case '\r':
            Result += "\\r";
            break;
        case '\t':
            Result += "\\t";
            break;
        default:
            Result += *p;
            break;
        }
    }
    return Result;
}

//=========================================================================

xbool IsAlphaBlendMaterial( s32 Type )
{
    switch( Type )
    {
    case Material_Alpha:
    case Material_Alpha_PerPolyEnv:
    case Material_Alpha_PerPixelIllum:
    case Material_Alpha_PerPolyIllum:
    case Material_Distortion:
    case Material_Distortion_PerPolyEnv:
        return TRUE;

    default:
        return FALSE;
    }
}

//=========================================================================

xbool IsPunchThroughMaterial( const geometry_material& Source )
{
    return Source.bPunchthrough || ( ( Source.ProductFlags & MATERIAL_FLAG_IS_PUNCH_THRU ) != 0 );
}

//=========================================================================

xbool UsesPerPixelEnvironmentMask( s32 Type )
{
    return Type == Material_Diff_PerPixelEnv;
}

//=========================================================================

xbool UsesPerPolygonEnvironmentMask( s32 Type )
{
    return Type == Material_Alpha_PerPolyEnv;
}

//=========================================================================

f32 EnvironmentSpecularStrength( const geometry_material& Source )
{
    if( UsesPerPixelEnvironmentMask( Source.Type ) )
    {
        // Convert legacy cubemap strength to glTF specular scale.
        if( ( Source.ProductFlags & MATERIAL_FLAG_ENV_CUBE_MAP ) == 0 )
            return 1.0f;

        // Match Material::Finalize's default intensity.
        const f32 CubeMapIntensity = Source.FixedAlpha > 0.0f ? Source.FixedAlpha : 0.35f;
        const f32 SpecularStrength = CubeMapIntensity * 4.0f;
        return SpecularStrength > 1.0f ? 1.0f : SpecularStrength;
    }

    const f32 Value = Source.FixedAlpha;
    return Value < 0.0f ? 0.0f : ( Value > 1.0f ? 1.0f : Value );
}

struct render_gltf_writer : public geom_gltf_detail::gltf_document_writer
{
    std::vector<s32> SourceTextureIndices;

    void InitializeModel( const geometry_asset& Model, xbool bSkin, const char* pOutputFile,
                          const char* pBinaryFileName, const char* pBinaryUriFileName,
                          const resource_tool::geometry::texture_export_map* pTextureMap,
                          const resource_tool::animation::asset* pAnimation )
    {
        x_memset( &Data, 0, sizeof( Data ) );
        x_memset( &Buffer, 0, sizeof( Buffer ) );
        Data.file_type = cgltf_file_type_gltf;
        Data.asset.version = const_cast<char*>( Keep( "2.0" ) );
        Data.asset.generator = const_cast<char*>( Keep( "Area51 ResourceTool" ) );

        std::filesystem::path BinaryUriPath(
            pBinaryUriFileName && pBinaryUriFileName[0]
                ? pBinaryUriFileName
                : ( pBinaryFileName && pBinaryFileName[0] ? pBinaryFileName : pOutputFile ) );
        if( BinaryUriPath.extension() != ".bin" )
            BinaryUriPath.replace_extension( ".bin" );
        Buffer.uri = const_cast<char*>( Keep( BinaryUriPath.filename().generic_string() ) );

        const usize nFacets = Model.Render.Facets.size();
        const usize AnimationCapacity =
            pAnimation ? pAnimation->Clips.size() * pAnimation->Skeleton.Bones.size() * 6 : 0;
        BufferViews.reserve( nFacets * 8 + Model.Skeleton.Bones.size() + AnimationCapacity + 16 );
        Accessors.reserve( nFacets * 8 + Model.Skeleton.Bones.size() + AnimationCapacity + 16 );

        Samplers.resize( 1 );
        x_memset( Samplers.data(), 0, sizeof( cgltf_sampler ) );
        Samplers[0].mag_filter = cgltf_filter_type_linear;
        Samplers[0].min_filter = cgltf_filter_type_linear_mipmap_linear;
        Samplers[0].wrap_s = cgltf_wrap_mode_repeat;
        Samplers[0].wrap_t = cgltf_wrap_mode_repeat;

        SourceTextureIndices.assign( Model.Materials.Textures.size(), -1 );
        usize ExportTextureCount = 0;
        for( usize i = 0; i < Model.Materials.Textures.size(); ++i )
        {
            const xbool bAvailable = !pTextureMap || pTextureMap->Find( Model.Materials.Textures[i].FileName ) != NULL;
            if( bAvailable )
                SourceTextureIndices[i] = (s32)ExportTextureCount++;
        }
        Textures.resize( ExportTextureCount );
        Images.resize( ExportTextureCount );
        Materials.resize( Model.Materials.Items.size() );
        if( !Textures.empty() )
            x_memset( Textures.data(), 0, sizeof( cgltf_texture ) * Textures.size() );
        if( !Images.empty() )
            x_memset( Images.data(), 0, sizeof( cgltf_image ) * Images.size() );
        x_memset( Materials.data(), 0, sizeof( cgltf_material ) * Materials.size() );

        for( usize i = 0; i < Model.Materials.Textures.size(); ++i )
        {
            const s32 iExportTexture = SourceTextureIndices[i];
            if( iExportTexture < 0 )
                continue;
            const std::string Fallback = (const char*)xfs( "texture_%d.png", (s32)i );
            std::string ImageUri =
                Model.Materials.Textures[i].FileName.empty() ? Fallback : Model.Materials.Textures[i].FileName;
            if( pTextureMap )
            {
                const resource_tool::geometry::texture_export_entry* pEntry =
                    pTextureMap->Find( Model.Materials.Textures[i].FileName );
                if( pEntry )
                    ImageUri = pEntry->GltfUri;
            }
            Images[iExportTexture].uri = const_cast<char*>( Keep( ImageUri ) );
            Textures[iExportTexture].image = &Images[iExportTexture];
            Textures[iExportTexture].sampler = &Samplers[0];
        }
        for( usize i = 0; i < Model.Materials.Items.size(); ++i )
        {
            const geometry_material& Source = Model.Materials.Items[i];
            cgltf_material& Material = Materials[i];
            // FixedAlpha controls the environment path, not material opacity.
            const std::string Fallback = (const char*)xfs( "Material_%d", (s32)i );
            Material.name = const_cast<char*>( Keep( Source.Name.empty() ? Fallback : Source.Name ) );
            Material.has_pbr_metallic_roughness = 1;
            Material.pbr_metallic_roughness.base_color_factor[0] = 1.0f;
            Material.pbr_metallic_roughness.base_color_factor[1] = 1.0f;
            Material.pbr_metallic_roughness.base_color_factor[2] = 1.0f;
            Material.pbr_metallic_roughness.base_color_factor[3] = 1.0f;
            Material.pbr_metallic_roughness.metallic_factor = 0.0f;
            // Match the legacy environment specular lobe.
            Material.pbr_metallic_roughness.roughness_factor =
                ( UsesPerPixelEnvironmentMask( Source.Type ) || UsesPerPolygonEnvironmentMask( Source.Type ) ) ? 0.33f
                                                                                                               : 1.0f;
            const s32 iTexture = Source.nMaps > 0 ? Source.Map[0].iTexture : -1;
            if( iTexture >= 0 && iTexture < (s32)SourceTextureIndices.size() && SourceTextureIndices[iTexture] >= 0 )
            {
                Material.pbr_metallic_roughness.base_color_texture.texture = &Textures[SourceTextureIndices[iTexture]];
                Material.pbr_metallic_roughness.base_color_texture.texcoord = Source.Map[0].iUVChannel;
            }

            // Reuse diffuse alpha as the environment mask; disable unused specular paths.
            Material.has_specular = 1;
            Material.specular.specular_factor = 0.0f;
            if( UsesPerPixelEnvironmentMask( Source.Type ) && iTexture >= 0 &&
                iTexture < (s32)SourceTextureIndices.size() && SourceTextureIndices[iTexture] >= 0 )
            {
                Material.specular.specular_texture.texture = &Textures[SourceTextureIndices[iTexture]];
                Material.specular.specular_texture.texcoord = Source.Map[0].iUVChannel;
                Material.specular.specular_factor = EnvironmentSpecularStrength( Source );
            }
            else if( UsesPerPolygonEnvironmentMask( Source.Type ) )
            {
                Material.specular.specular_factor = EnvironmentSpecularStrength( Source );
            }
            Material.specular.specular_color_factor[0] = 1.0f;
            Material.specular.specular_color_factor[1] = 1.0f;
            Material.specular.specular_color_factor[2] = 1.0f;

            if( IsPunchThroughMaterial( Source ) )
            {
                // Punch-through uses a 0.5 alpha test.
                Material.alpha_mode = cgltf_alpha_mode_mask;
                Material.alpha_cutoff = 0.5f;
            }
            else if( IsAlphaBlendMaterial( Source.Type ) )
            {
                Material.alpha_mode = cgltf_alpha_mode_blend;
            }
            else
            {
                // These shader paths use alpha as a mask, not transparency.
                Material.alpha_mode = cgltf_alpha_mode_opaque;
            }
            Material.double_sided = Source.bTwoSided;
        }

        Meshes.resize( Model.Render.SubMeshes.size() );
        PrimitiveGroups.resize( Model.Render.SubMeshes.size() );
        Nodes.reserve( Model.Render.SubMeshes.size() + ( bSkin ? Model.Skeleton.Bones.size() : 0 ) );
        for( usize i = 0; i < Model.Render.SubMeshes.size(); ++i )
        {
            x_memset( &Meshes[i], 0, sizeof( cgltf_mesh ) );
            const geometry_sub_mesh& Source = Model.Render.SubMeshes[i];
            const std::string Fallback = (const char*)xfs( "Mesh_%d", (s32)i );
            Meshes[i].name = const_cast<char*>( Keep( Source.Name.empty() ? Fallback : Source.Name ) );
            cgltf_node Node;
            x_memset( &Node, 0, sizeof( Node ) );
            Node.name = Meshes[i].name;
            Node.mesh = &Meshes[i];
            Nodes.push_back( Node );
            SceneNodes.push_back( &Nodes.back() );
        }

        if( bSkin )
        {
            if( Model.Skeleton.Bones.empty() )
                x_throw( "Skinned geometry has no canonical skeleton" );
            ChildGroups.resize( Model.Skeleton.Bones.size() );
            SkinJoints.resize( 1 );
            SkinJoints[0].reserve( Model.Skeleton.Bones.size() );
            for( usize i = 0; i < Model.Skeleton.Bones.size(); ++i )
            {
                const geometry_bone& Source = Model.Skeleton.Bones[i];
                cgltf_node Node;
                x_memset( &Node, 0, sizeof( Node ) );
                const std::string Fallback = (const char*)xfs( "Bone_%d", (s32)i );
                Node.name = const_cast<char*>( Keep( Source.Name.empty() ? Fallback : Source.Name ) );
                Node.has_translation = 1;
                Node.translation[0] = Source.Position.GetX();
                Node.translation[1] = Source.Position.GetY();
                Node.translation[2] = Source.Position.GetZ();
                Node.has_rotation = 1;
                Node.rotation[0] = Source.Rotation.X;
                Node.rotation[1] = Source.Rotation.Y;
                Node.rotation[2] = Source.Rotation.Z;
                Node.rotation[3] = Source.Rotation.W;
                Node.has_scale = 1;
                Node.scale[0] = Source.Scale.GetX();
                Node.scale[1] = Source.Scale.GetY();
                Node.scale[2] = Source.Scale.GetZ();
                Nodes.push_back( Node );
            }
            for( usize i = 0; i < Model.Skeleton.Bones.size(); ++i )
            {
                SkinJoints[0].push_back( &Nodes[Model.Render.SubMeshes.size() + i] );
                const s32 Parent = Model.Skeleton.Bones[i].iParent;
                if( Parent >= 0 && Parent < (s32)Model.Skeleton.Bones.size() )
                {
                    Nodes[Model.Render.SubMeshes.size() + i].parent = &Nodes[Model.Render.SubMeshes.size() + Parent];
                    ChildGroups[Parent].push_back( &Nodes[Model.Render.SubMeshes.size() + i] );
                }
                else
                {
                    SceneNodes.push_back( &Nodes[Model.Render.SubMeshes.size() + i] );
                }
            }
            for( usize i = 0; i < Model.Skeleton.Bones.size(); ++i )
            {
                Nodes[Model.Render.SubMeshes.size() + i].children = ChildGroups[i].data();
                Nodes[Model.Render.SubMeshes.size() + i].children_count = ChildGroups[i].size();
            }

            std::vector<f32> InverseBindMatrices;
            InverseBindMatrices.reserve( Model.Skeleton.Bones.size() * 16 );
            for( const geometry_bone& Bone : Model.Skeleton.Bones )
            {
                for( s32 Column = 0; Column < 4; ++Column )
                    for( s32 Row = 0; Row < 4; ++Row )
                        InverseBindMatrices.push_back( Bone.InverseBindMatrix( Column, Row ) );
            }
            cgltf_buffer_view* pInverseBindView = AddView( InverseBindMatrices, cgltf_buffer_view_type_invalid );
            cgltf_accessor* pInverseBind = AddAccessor( pInverseBindView, cgltf_component_type_r_32f, cgltf_type_mat4,
                                                        Model.Skeleton.Bones.size() );

            cgltf_skin Skin;
            x_memset( &Skin, 0, sizeof( Skin ) );
            Skin.name = const_cast<char*>( Keep( "Skeleton" ) );
            Skin.joints = SkinJoints[0].data();
            Skin.joints_count = SkinJoints[0].size();
            Skin.inverse_bind_matrices = pInverseBind;
            Skins.push_back( Skin );
            for( usize i = 0; i < Model.Render.SubMeshes.size(); ++i )
                Nodes[i].skin = &Skins[0];
            for( usize i = 0; i < Model.Skeleton.Bones.size(); ++i )
            {
                if( Model.Skeleton.Bones[i].iParent < 0 && !Skins[0].skeleton )
                    Skins[0].skeleton = &Nodes[Model.Render.SubMeshes.size() + i];
            }
        }

        cgltf_scene Scene;
        x_memset( &Scene, 0, sizeof( Scene ) );
        Scene.name = const_cast<char*>( Keep( "Scene" ) );
        Scene.nodes = SceneNodes.data();
        Scene.nodes_count = SceneNodes.size();
        Scenes.push_back( Scene );
        Data.buffers = &Buffer;
        Data.buffers_count = 1;
        Data.materials = Materials.data();
        Data.materials_count = Materials.size();
        Data.textures = Textures.data();
        Data.textures_count = Textures.size();
        Data.images = Images.data();
        Data.images_count = Images.size();
        Data.samplers = Samplers.data();
        Data.samplers_count = Samplers.size();
        Data.meshes = Meshes.data();
        Data.meshes_count = Meshes.size();
        Data.nodes = Nodes.data();
        Data.nodes_count = Nodes.size();
        Data.scenes = Scenes.data();
        Data.scenes_count = Scenes.size();
        Data.scene = &Scenes[0];
        Data.skins = Skins.data();
        Data.skins_count = Skins.size();
    }

    void AddModelPrimitive( const geometry_asset& Model, const std::vector<const geometry_facet*>& Facets, xbool bSkin,
                            cgltf_primitive& Primitive, std::vector<cgltf_attribute>& Attributes )
    {
        if( Facets.empty() )
            x_throw( "Canonical geometry primitive has no facets" );
        std::vector<s32> VertexIds;
        std::vector<u32> Indices;
        for( const geometry_facet* pFacet : Facets )
        {
            if( !pFacet || pFacet->nVertices != 3 )
                x_throw( "Canonical geometry contains a non-triangle facet" );
            for( s32 iCorner = 0; iCorner < 3; ++iCorner )
            {
                const s32 iVertex = pFacet->iVertex[iCorner];
                if( iVertex < 0 || iVertex >= (s32)Model.Render.Vertices.size() )
                    x_throw( "Canonical geometry facet references an invalid vertex" );
                s32 Local = -1;
                for( s32 i = 0; i < (s32)VertexIds.size(); ++i )
                {
                    if( VertexIds[i] == iVertex )
                    {
                        Local = i;
                        break;
                    }
                }
                if( Local < 0 )
                {
                    VertexIds.push_back( iVertex );
                    Local = (s32)VertexIds.size() - 1;
                }
                Indices.push_back( (u32)Local );
            }
        }

        std::vector<f32> Positions;
        std::vector<f32> Normals;
        std::vector<f32> UVs;
        std::vector<u16> Joints;
        std::vector<f32> Weights;
        Positions.reserve( VertexIds.size() * 3 );
        Normals.reserve( VertexIds.size() * 3 );
        UVs.reserve( VertexIds.size() * 2 );
        if( bSkin )
        {
            Joints.reserve( VertexIds.size() * 4 );
            Weights.reserve( VertexIds.size() * 4 );
        }
        for( s32 iVertex : VertexIds )
        {
            const geometry_vertex& Source = Model.Render.Vertices[iVertex];
            Positions.push_back( Source.Position.GetX() );
            Positions.push_back( Source.Position.GetY() );
            Positions.push_back( Source.Position.GetZ() );
            const vector3 Normal = Source.Normal.empty() ? vector3( 0.0f, 0.0f, 1.0f ) : Source.Normal[0];
            Normals.push_back( Normal.GetX() );
            Normals.push_back( Normal.GetY() );
            Normals.push_back( Normal.GetZ() );
            const vector2 UV = Source.UV.empty() ? vector2( 0.0f, 0.0f ) : Source.UV[0];
            UVs.push_back( UV.X );
            UVs.push_back( UV.Y );
            if( bSkin )
            {
                f32 WeightSum = 0.0f;
                for( s32 iWeight = 0; iWeight < 4; ++iWeight )
                {
                    u16 Joint = 0;
                    f32 WeightValue = 0.0f;
                    if( iWeight < (s32)Source.Weight.size() )
                    {
                        const geometry_weight& SourceWeight = Source.Weight[iWeight];
                        s32 iBone = SourceWeight.iBone;
                        if( iBone < 0 && !SourceWeight.BoneName.empty() )
                            iBone = Model.GetBoneIDFromName( SourceWeight.BoneName.c_str(), TRUE );
                        if( iBone < 0 || iBone >= (s32)Model.Skeleton.Bones.size() )
                            x_throw( "Canonical skin vertex references an invalid bone" );
                        Joint = (u16)iBone;
                        WeightValue = SourceWeight.Weight;
                    }
                    Joints.push_back( Joint );
                    Weights.push_back( WeightValue );
                    WeightSum += WeightValue;
                }
                if( WeightSum <= 0.0f )
                    x_throw( "Canonical skin vertex has no positive weights" );
            }
        }

        cgltf_buffer_view* pPositionView = AddView( Positions, cgltf_buffer_view_type_vertices );
        cgltf_buffer_view* pNormalView = AddView( Normals, cgltf_buffer_view_type_vertices );
        cgltf_buffer_view* pUVView = AddView( UVs, cgltf_buffer_view_type_vertices );
        cgltf_buffer_view* pIndexView = AddView( Indices, cgltf_buffer_view_type_indices );
        cgltf_accessor* pPosition =
            AddAccessor( pPositionView, cgltf_component_type_r_32f, cgltf_type_vec3, VertexIds.size() );
        cgltf_accessor* pNormal =
            AddAccessor( pNormalView, cgltf_component_type_r_32f, cgltf_type_vec3, VertexIds.size() );
        cgltf_accessor* pUV = AddAccessor( pUVView, cgltf_component_type_r_32f, cgltf_type_vec2, VertexIds.size() );
        cgltf_accessor* pIndex =
            AddAccessor( pIndexView, cgltf_component_type_r_32u, cgltf_type_scalar, Indices.size() );
        Attributes.resize( bSkin ? 5 : 3 );
        x_memset( Attributes.data(), 0, sizeof( cgltf_attribute ) * Attributes.size() );
        Attributes[0].name = const_cast<char*>( "POSITION" );
        Attributes[0].type = cgltf_attribute_type_position;
        Attributes[0].data = pPosition;
        Attributes[1].name = const_cast<char*>( "NORMAL" );
        Attributes[1].type = cgltf_attribute_type_normal;
        Attributes[1].data = pNormal;
        Attributes[2].name = const_cast<char*>( "TEXCOORD_0" );
        Attributes[2].type = cgltf_attribute_type_texcoord;
        Attributes[2].data = pUV;
        if( bSkin )
        {
            cgltf_buffer_view* pJointView = AddView( Joints, cgltf_buffer_view_type_vertices );
            cgltf_buffer_view* pWeightView = AddView( Weights, cgltf_buffer_view_type_vertices );
            cgltf_accessor* pJoint =
                AddAccessor( pJointView, cgltf_component_type_r_16u, cgltf_type_vec4, VertexIds.size() );
            cgltf_accessor* pWeight =
                AddAccessor( pWeightView, cgltf_component_type_r_32f, cgltf_type_vec4, VertexIds.size() );
            Attributes[3].name = const_cast<char*>( "JOINTS_0" );
            Attributes[3].type = cgltf_attribute_type_joints;
            Attributes[3].data = pJoint;
            Attributes[4].name = const_cast<char*>( "WEIGHTS_0" );
            Attributes[4].type = cgltf_attribute_type_weights;
            Attributes[4].data = pWeight;
        }

        x_memset( &Primitive, 0, sizeof( Primitive ) );
        Primitive.type = cgltf_primitive_type_triangles;
        Primitive.indices = pIndex;
        Primitive.attributes = Attributes.data();
        Primitive.attributes_count = Attributes.size();
        s32 iMaterial = Facets[0]->iMaterial;
        if( !Facets[0]->MaterialName.empty() )
            iMaterial = FindModelName( Model.Materials.Items, Facets[0]->MaterialName );
        if( iMaterial >= 0 && iMaterial < (s32)Materials.size() )
            Primitive.material = &Materials[iMaterial];
        const geometry_facet& Identity = *Facets[0];
        const std::string MeshName = JsonEscape( Identity.MeshName.c_str() );
        const std::string SubMeshName = JsonEscape( Identity.SubMeshName.c_str() );
        const std::string SectionName = JsonEscape( Identity.SectionName.c_str() );
        const std::string MaterialName =
            JsonEscape( Identity.MaterialName.empty() ? "" : Identity.MaterialName.c_str() );
        const std::string BoneName = JsonEscape( Identity.BoneName.c_str() );
        const f32 WorldPixelSize = std::isfinite( Identity.WorldPixelSize ) && Identity.WorldPixelSize >= 0.000001f
                                       ? Identity.WorldPixelSize
                                       : 0.001f;
        Primitive.extras.data = const_cast<char*>(
            Keep( (const char*)xfs( "{\"area51_submesh\":%d,\"area51_section\":%d,"
                                    "\"area51_mesh_name\":\"%s\",\"area51_submesh_name\":\"%s\","
                                    "\"area51_section_name\":\"%s\",\"area51_material_name\":\"%s\","
                                    "\"area51_bone_name\":\"%s\",\"area51_world_pixel_size\":%g}",
                                    Identity.iSubMesh, Identity.iSection, MeshName.c_str(), SubMeshName.c_str(),
                                    SectionName.c_str(), MaterialName.c_str(), BoneName.c_str(), WorldPixelSize ) ) );
    }

    xbool WriteModel( const geometry_asset& Model, xbool bSkin, const char* pOutputFile, std::string& Error,
                      const char* pBinaryFileName, const char* pBinaryUriFileName,
                      const resource_tool::geometry::texture_export_map* pTextureMap,
                      const resource_tool::animation::asset* pAnimation )
    {
        if( Model.Render.SubMeshes.empty() || Model.Render.Facets.empty() )
        {
            Error = "Canonical geometry contains no drawable meshes";
            return FALSE;
        }
        if( bSkin )
        {
            if( !pAnimation )
            {
                Error = "Skinned geometry glTF export requires an animation asset";
                return FALSE;
            }
            if( pAnimation->Skeleton.Bones.size() != Model.Skeleton.Bones.size() )
            {
                Error = "Skinned geometry and animation skeleton bone counts differ";
                return FALSE;
            }
            if( pAnimation->Clips.empty() )
            {
                Error = "Skinned geometry glTF export requires animation clips";
                return FALSE;
            }
        }
        InitializeModel( Model, bSkin, pOutputFile, pBinaryFileName, pBinaryUriFileName, pTextureMap, pAnimation );
        AttributeGroups.reserve( Model.Render.Facets.size() );
        for( s32 iMesh = 0; iMesh < (s32)Model.Render.SubMeshes.size(); ++iMesh )
        {
            struct group
            {
                s32 iSubMesh;
                s32 iSection;
                std::vector<const geometry_facet*> Facets;
            };
            std::vector<group> Groups;
            for( const geometry_facet& Facet : Model.Render.Facets )
            {
                if( !geometry_names_match( Facet.MeshName.c_str(), Model.Render.SubMeshes[iMesh].Name.c_str() ) )
                    continue;
                group* pGroup = NULL;
                for( group& Candidate : Groups )
                {
                    if( geometry_names_match( Candidate.Facets[0]->SubMeshName.c_str(), Facet.SubMeshName.c_str() ) &&
                        geometry_names_match( Candidate.Facets[0]->SectionName.c_str(), Facet.SectionName.c_str() ) )
                    {
                        pGroup = &Candidate;
                        break;
                    }
                }
                if( !pGroup )
                {
                    Groups.push_back( group() );
                    pGroup = &Groups.back();
                    pGroup->iSubMesh = Facet.iSubMesh;
                    pGroup->iSection = Facet.iSection;
                }
                pGroup->Facets.push_back( &Facet );
            }
            for( const group& Group : Groups )
            {
                AttributeGroups.emplace_back();
                cgltf_primitive Primitive;
                AddModelPrimitive( Model, Group.Facets, bSkin, Primitive, AttributeGroups.back() );
                PrimitiveGroups[iMesh].push_back( Primitive );
            }
            Meshes[iMesh].primitives = PrimitiveGroups[iMesh].data();
            Meshes[iMesh].primitives_count = PrimitiveGroups[iMesh].size();
        }
        if( bSkin )
            AddAnimations( pAnimation->Clips );
        return Save( pOutputFile, pBinaryFileName, Error );
    }
};

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geom_gltf_writer::WriteRigid( const geometry_asset& Model, const char* pOutputFile, std::string& Error,
                                    const char* pBinaryFileName, const char* pBinaryUriFileName,
                                    const resource_tool::geometry::texture_export_map* pTextureMap )
{
    render_gltf_writer Exporter;
    return Exporter.WriteModel( Model, FALSE, pOutputFile, Error, pBinaryFileName, pBinaryUriFileName, pTextureMap,
                                NULL );
}

//=========================================================================

xbool geom_gltf_writer::WriteSkin( const geometry_asset& Model, const char* pOutputFile, std::string& Error,
                                   const char* pBinaryFileName, const char* pBinaryUriFileName,
                                   const resource_tool::geometry::texture_export_map* pTextureMap,
                                   const resource_tool::animation::asset* pAnimation )
{
    render_gltf_writer Exporter;
    return Exporter.WriteModel( Model, TRUE, pOutputFile, Error, pBinaryFileName, pBinaryUriFileName, pTextureMap,
                                pAnimation );
}
