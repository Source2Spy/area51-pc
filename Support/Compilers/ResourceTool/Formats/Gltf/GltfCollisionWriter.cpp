//=========================================================================
//
//  GltfCollisionWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GltfWriter.hpp"
#include "GltfDocument.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

struct collision_mesh_data
{
    std::string Name;
    std::vector<f32> Positions;
    std::vector<f32> Normals;
    std::vector<u32> Indices;
};

class collision_gltf_writer final : public geom_gltf_detail::gltf_document_writer
{
public:
    xbool Write( const geometry_asset& Model, const char* pOutputFile, std::string& Error, const char* pBinaryFileName,
                 const char* pBinaryUriFileName )
    {
        if( Model.Collision.Meshes.empty() )
        {
            Error = "Canonical geometry contains no collision geometry";
            return FALSE;
        }

        std::vector<collision_mesh_data> CollisionMeshes;
        CollisionMeshes.reserve( Model.Collision.Meshes.size() );
        for( const geometry_collision_mesh& Source : Model.Collision.Meshes )
        {
            if( Source.Vertices.empty() || Source.Indices.empty() || Source.Normals.size() != Source.Vertices.size() ||
                ( Source.Indices.size() % 3 ) != 0 )
            {
                Error = "Canonical collision mesh has invalid topology";
                return FALSE;
            }
            for( u32 Index : Source.Indices )
            {
                if( Index >= Source.Vertices.size() )
                {
                    Error = "Canonical collision mesh contains an invalid vertex index";
                    return FALSE;
                }
            }
            collision_mesh_data& Output = CollisionMeshes.emplace_back();
            Output.Name = Source.Name;
            Output.Indices = Source.Indices;
            Output.Positions.reserve( Source.Vertices.size() * 3 );
            Output.Normals.reserve( Source.Normals.size() * 3 );
            for( usize i = 0; i < Source.Vertices.size(); ++i )
            {
                Output.Positions.push_back( Source.Vertices[i].GetX() );
                Output.Positions.push_back( Source.Vertices[i].GetY() );
                Output.Positions.push_back( Source.Vertices[i].GetZ() );
                Output.Normals.push_back( Source.Normals[i].GetX() );
                Output.Normals.push_back( Source.Normals[i].GetY() );
                Output.Normals.push_back( Source.Normals[i].GetZ() );
            }
        }

        Initialize( CollisionMeshes, pOutputFile, pBinaryFileName,
                    Model.Materials.Textures.empty() ? "collision.xbmp" : Model.Materials.Textures[0].FileName.c_str(),
                    pBinaryUriFileName );
        AttributeGroups.reserve( CollisionMeshes.size() );
        for( usize i = 0; i < CollisionMeshes.size(); ++i )
        {
            AttributeGroups.emplace_back();
            cgltf_primitive Primitive;
            AddPrimitive( CollisionMeshes[i], Primitive, AttributeGroups.back() );
            PrimitiveGroups[i].push_back( Primitive );
            Meshes[i].primitives = PrimitiveGroups[i].data();
            Meshes[i].primitives_count = PrimitiveGroups[i].size();
        }
        return Save( pOutputFile, pBinaryFileName, Error );
    }

private:
    void Initialize( const std::vector<collision_mesh_data>& CollisionMeshes, const char* pOutputFile,
                     const char* pBinaryFileName, const char* pTextureName, const char* pBinaryUriFileName )
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
        BufferViews.reserve( CollisionMeshes.size() * 4 );
        Accessors.reserve( CollisionMeshes.size() * 4 );

        Samplers.resize( 1 );
        x_memset( Samplers.data(), 0, sizeof( cgltf_sampler ) );
        Samplers[0].mag_filter = cgltf_filter_type_linear;
        Samplers[0].min_filter = cgltf_filter_type_linear_mipmap_linear;
        Samplers[0].wrap_s = cgltf_wrap_mode_repeat;
        Samplers[0].wrap_t = cgltf_wrap_mode_repeat;

        Materials.resize( 1 );
        Textures.resize( 1 );
        Images.resize( 1 );
        x_memset( Materials.data(), 0, sizeof( cgltf_material ) );
        x_memset( Textures.data(), 0, sizeof( cgltf_texture ) );
        x_memset( Images.data(), 0, sizeof( cgltf_image ) );

        Images[0].uri = const_cast<char*>( Keep( pTextureName, "collision.xbmp" ) );
        Textures[0].image = &Images[0];
        Textures[0].sampler = &Samplers[0];
        Materials[0].name = const_cast<char*>( Keep( "CollisionMaterial" ) );
        Materials[0].has_pbr_metallic_roughness = 1;
        Materials[0].pbr_metallic_roughness.base_color_factor[0] = 0.25f;
        Materials[0].pbr_metallic_roughness.base_color_factor[1] = 0.75f;
        Materials[0].pbr_metallic_roughness.base_color_factor[2] = 0.25f;
        Materials[0].pbr_metallic_roughness.base_color_factor[3] = 1.0f;
        Materials[0].pbr_metallic_roughness.metallic_factor = 0.0f;
        Materials[0].pbr_metallic_roughness.roughness_factor = 1.0f;
        Materials[0].pbr_metallic_roughness.base_color_texture.texture = &Textures[0];
        Materials[0].pbr_metallic_roughness.base_color_texture.texcoord = 0;

        Meshes.resize( CollisionMeshes.size() );
        PrimitiveGroups.resize( CollisionMeshes.size() );
        Nodes.reserve( CollisionMeshes.size() );
        for( usize i = 0; i < CollisionMeshes.size(); ++i )
        {
            x_memset( &Meshes[i], 0, sizeof( cgltf_mesh ) );
            Meshes[i].name = const_cast<char*>( Keep( CollisionMeshes[i].Name ) );
            PrimitiveGroups[i].reserve( 1 );

            cgltf_node Node;
            x_memset( &Node, 0, sizeof( Node ) );
            Node.name = const_cast<char*>( Keep( CollisionMeshes[i].Name ) );
            Node.mesh = &Meshes[i];
            Nodes.push_back( Node );
            SceneNodes.push_back( &Nodes.back() );
        }

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

        cgltf_scene Scene;
        x_memset( &Scene, 0, sizeof( Scene ) );
        Scene.name = const_cast<char*>( Keep( "CollisionScene" ) );
        Scene.nodes = SceneNodes.data();
        Scene.nodes_count = SceneNodes.size();
        Scenes.push_back( Scene );
        Data.scenes = Scenes.data();
        Data.scenes_count = Scenes.size();
        Data.scene = &Scenes[0];
    }

    void AddPrimitive( const collision_mesh_data& CollisionMesh, cgltf_primitive& Primitive,
                       std::vector<cgltf_attribute>& Attributes )
    {
        std::vector<f32> UVs( CollisionMesh.Positions.size() / 3 * 2, 0.0f );
        for( u32 Index : CollisionMesh.Indices )
        {
            if( Index >= CollisionMesh.Positions.size() / 3 )
                x_throw( "Collision mesh contains an invalid vertex index" );
        }

        cgltf_buffer_view* pPositionView = AddView( CollisionMesh.Positions, cgltf_buffer_view_type_vertices );
        cgltf_buffer_view* pNormalView = AddView( CollisionMesh.Normals, cgltf_buffer_view_type_vertices );
        cgltf_buffer_view* pUVView = AddView( UVs, cgltf_buffer_view_type_vertices );
        cgltf_buffer_view* pIndexView = AddView( CollisionMesh.Indices, cgltf_buffer_view_type_indices );
        cgltf_accessor* pPosition = AddAccessor( pPositionView, cgltf_component_type_r_32f, cgltf_type_vec3,
                                                 CollisionMesh.Positions.size() / 3 );
        cgltf_accessor* pNormal =
            AddAccessor( pNormalView, cgltf_component_type_r_32f, cgltf_type_vec3, CollisionMesh.Normals.size() / 3 );
        cgltf_accessor* pUV = AddAccessor( pUVView, cgltf_component_type_r_32f, cgltf_type_vec2, UVs.size() / 2 );
        cgltf_accessor* pIndex =
            AddAccessor( pIndexView, cgltf_component_type_r_32u, cgltf_type_scalar, CollisionMesh.Indices.size() );

        Attributes.resize( 3 );
        x_memset( Attributes.data(), 0, sizeof( cgltf_attribute ) * Attributes.size() );
        Attributes[0].name = const_cast<char*>( "POSITION" );
        Attributes[0].type = cgltf_attribute_type_position;
        Attributes[0].data = pPosition;
        Attributes[1].name = const_cast<char*>( "NORMAL" );
        Attributes[1].type = cgltf_attribute_type_normal;
        Attributes[1].data = pNormal;
        Attributes[2].name = const_cast<char*>( "TEXCOORD_0" );
        Attributes[2].type = cgltf_attribute_type_texcoord;
        Attributes[2].index = 0;
        Attributes[2].data = pUV;

        x_memset( &Primitive, 0, sizeof( Primitive ) );
        Primitive.type = cgltf_primitive_type_triangles;
        Primitive.indices = pIndex;
        Primitive.attributes = Attributes.data();
        Primitive.attributes_count = Attributes.size();
        Primitive.material = &Materials[0];
    }
};

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geom_gltf_writer::WriteCollision( const geometry_asset& Model, const char* pOutputFile, std::string& Error,
                                        const char* pBinaryFileName, const char* pBinaryUriFileName )
{
    collision_gltf_writer Writer;
    return Writer.Write( Model, pOutputFile, Error, pBinaryFileName, pBinaryUriFileName );
}
