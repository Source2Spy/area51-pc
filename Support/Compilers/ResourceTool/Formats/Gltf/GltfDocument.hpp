//=========================================================================
//
//  GltfDocument.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GLTF_DOCUMENT_HPP
#define RESOURCE_TOOL_GLTF_DOCUMENT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "cgltf.h"
#include "Model/AnimationAsset.hpp"

#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace geom_gltf_detail
{

//=========================================================================
//  TYPES
//=========================================================================

class gltf_document_writer
{
protected:
    cgltf_data Data;
    cgltf_buffer Buffer;
    std::vector<u8> Binary;
    std::vector<cgltf_buffer_view> BufferViews;
    std::vector<cgltf_accessor> Accessors;
    std::vector<cgltf_material> Materials;
    std::vector<cgltf_texture> Textures;
    std::vector<cgltf_image> Images;
    std::vector<cgltf_sampler> Samplers;
    std::vector<cgltf_mesh> Meshes;
    std::vector<std::vector<cgltf_primitive>> PrimitiveGroups;
    std::vector<std::vector<cgltf_attribute>> AttributeGroups;
    std::vector<cgltf_node> Nodes;
    std::vector<std::vector<cgltf_node*>> ChildGroups;
    std::vector<cgltf_skin> Skins;
    std::vector<std::vector<cgltf_node*>> SkinJoints;
    std::vector<cgltf_animation> Animations;
    std::vector<std::vector<cgltf_animation_sampler>> AnimationSamplers;
    std::vector<std::vector<cgltf_animation_channel>> AnimationChannels;
    std::vector<cgltf_scene> Scenes;
    std::vector<cgltf_node*> SceneNodes;
    std::list<std::string> Strings;

    const char* Keep( const std::string& Value );
    const char* Keep( const char* pValue, const char* pFallback );

    static usize Align4( usize Value );

    cgltf_buffer_view* AddView( usize Offset, usize Size, cgltf_buffer_view_type Type );

    template <typename T> cgltf_buffer_view* AddView( const std::vector<T>& Values, cgltf_buffer_view_type Type )
    {
        const usize Offset = Align4( Binary.size() );
        Binary.resize( Offset );
        const usize Size = Values.size() * sizeof( T );
        const u8* pBytes = reinterpret_cast<const u8*>( Values.data() );
        Binary.insert( Binary.end(), pBytes, pBytes + Size );
        return AddView( Offset, Size, Type );
    }

    cgltf_accessor* AddAccessor( cgltf_buffer_view* pView, cgltf_component_type ComponentType, cgltf_type Type,
                                 cgltf_size Count );

    cgltf_node* FindNode( const std::string& Name );
    void AddAnimations( const std::vector<resource_tool::animation::clip>& Clips );

    xbool Save( const char* pOutputFile, const char* pBinaryFileName, std::string& Error );
};

} // namespace geom_gltf_detail

#endif // RESOURCE_TOOL_GLTF_DOCUMENT_HPP
//=========================================================================
