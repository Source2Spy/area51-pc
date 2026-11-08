//=========================================================================
//
//  AnimationGltfWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationGltfWriter.hpp"

#include "GltfDocument.hpp"

#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace resource_tool::animation_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

class animation_document_writer final : public geom_gltf_detail::gltf_document_writer
{
public:
    xbool Write( const resource_tool::animation::asset& Asset, const char* pOutputFile, const char* pBinaryFileName,
                 const char* pBinaryUriFileName, std::string& Error )
    {
        if( Asset.Skeleton.Bones.empty() )
        {
            Error = "Animation asset has no skeleton";
            return FALSE;
        }

        x_memset( &Data, 0, sizeof( Data ) );
        x_memset( &Buffer, 0, sizeof( Buffer ) );
        Data.file_type = cgltf_file_type_gltf;
        Data.asset.version = const_cast<char*>( Keep( "2.0" ) );
        Data.asset.generator = const_cast<char*>( Keep( "Area51 ResourceTool Animation" ) );

        const std::filesystem::path BinaryUri(
            pBinaryUriFileName && pBinaryUriFileName[0]
                ? pBinaryUriFileName
                : ( pBinaryFileName && pBinaryFileName[0] ? pBinaryFileName : "animation.bin" ) );
        Buffer.uri = const_cast<char*>( Keep( BinaryUri.filename().generic_string() ) );

        Nodes.reserve( Asset.Skeleton.Bones.size() );
        ChildGroups.resize( Asset.Skeleton.Bones.size() );
        BufferViews.reserve( Asset.Clips.size() * Asset.Skeleton.Bones.size() * 6 + 4 );
        Accessors.reserve( Asset.Clips.size() * Asset.Skeleton.Bones.size() * 6 + 4 );

        for( const resource_tool::animation::skeleton_bone& Source : Asset.Skeleton.Bones )
        {
            cgltf_node Node;
            x_memset( &Node, 0, sizeof( Node ) );
            Node.name = const_cast<char*>( Keep( Source.Name ) );
            Node.has_translation = 1;
            Node.translation[0] = Source.Translation.GetX();
            Node.translation[1] = Source.Translation.GetY();
            Node.translation[2] = Source.Translation.GetZ();
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

        for( usize iBone = 0; iBone < Asset.Skeleton.Bones.size(); ++iBone )
        {
            const s32 Parent = Asset.Skeleton.Bones[iBone].Parent;
            if( Parent >= 0 && Parent < (s32)Asset.Skeleton.Bones.size() )
            {
                Nodes[iBone].parent = &Nodes[Parent];
                ChildGroups[Parent].push_back( &Nodes[iBone] );
            }
            else
            {
                SceneNodes.push_back( &Nodes[iBone] );
            }
        }
        for( usize iBone = 0; iBone < Nodes.size(); ++iBone )
        {
            Nodes[iBone].children = ChildGroups[iBone].data();
            Nodes[iBone].children_count = ChildGroups[iBone].size();
        }

        SkinJoints.resize( 1 );
        SkinJoints[0].reserve( Nodes.size() );
        for( cgltf_node& Node : Nodes )
            SkinJoints[0].push_back( &Node );

        std::vector<f32> InverseBindMatrices;
        InverseBindMatrices.reserve( Nodes.size() * 16 );
        for( const resource_tool::animation::skeleton_bone& Bone : Asset.Skeleton.Bones )
            for( s32 Column = 0; Column < 4; ++Column )
                for( s32 Row = 0; Row < 4; ++Row )
                    InverseBindMatrices.push_back( Bone.InverseBindMatrix( Column, Row ) );
        cgltf_skin Skin;
        x_memset( &Skin, 0, sizeof( Skin ) );
        Skin.name = const_cast<char*>( Keep( Asset.Skeleton.Name.empty() ? "Skeleton" : Asset.Skeleton.Name ) );
        Skin.joints = SkinJoints[0].data();
        Skin.joints_count = SkinJoints[0].size();
        Skin.inverse_bind_matrices = AddAccessor( AddView( InverseBindMatrices, cgltf_buffer_view_type_invalid ),
                                                  cgltf_component_type_r_32f, cgltf_type_mat4, Nodes.size() );
        for( cgltf_node& Node : Nodes )
            if( !Skin.skeleton && !Node.parent )
                Skin.skeleton = &Node;
        Skins.push_back( Skin );

        cgltf_scene Scene;
        x_memset( &Scene, 0, sizeof( Scene ) );
        Scene.name = const_cast<char*>( Keep( "AnimationScene" ) );
        Scene.nodes = SceneNodes.data();
        Scene.nodes_count = SceneNodes.size();
        Scenes.push_back( Scene );

        Data.buffers = &Buffer;
        Data.buffers_count = 1;
        Data.nodes = Nodes.data();
        Data.nodes_count = Nodes.size();
        Data.scenes = Scenes.data();
        Data.scenes_count = Scenes.size();
        Data.scene = &Scenes[0];
        Data.skins = Skins.data();
        Data.skins_count = Skins.size();

        AddAnimations( Asset.Clips );
        std::string SaveError;
        const xbool bSaved = Save( pOutputFile, pBinaryFileName, SaveError ) == TRUE;
        if( !bSaved )
            Error = SaveError;
        return bSaved;
    }
};

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool animation_gltf_writer::Write( const resource_tool::animation::asset& Asset, const char* pOutputFile,
                                    const char* pBinaryFileName, const char* pBinaryUriFileName,
                                    std::string& Error ) const
{
    try
    {
        animation_document_writer Writer;
        return Writer.Write( Asset, pOutputFile, pBinaryFileName, pBinaryUriFileName, Error );
    }
    catch( const std::exception& Exception )
    {
        Error = Exception.what();
        return FALSE;
    }
}

} // namespace resource_tool::animation_product_format
