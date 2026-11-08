//=========================================================================
//
//  GltfDocument.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GltfDocument.hpp"
#include "cgltf_write.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace geom_gltf_detail
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

const char* gltf_document_writer::Keep( const std::string& Value )
{
    Strings.push_back( Value );
    return Strings.back().c_str();
}

//=========================================================================

const char* gltf_document_writer::Keep( const char* pValue, const char* pFallback )
{
    return Keep( ( pValue && pValue[0] ) ? pValue : pFallback );
}

//=========================================================================

usize gltf_document_writer::Align4( usize Value )
{
    return ( Value + 3u ) & ~usize( 3u );
}

//=========================================================================

cgltf_buffer_view* gltf_document_writer::AddView( usize Offset, usize Size, cgltf_buffer_view_type Type )
{
    cgltf_buffer_view View;
    x_memset( &View, 0, sizeof( View ) );
    View.buffer = &Buffer;
    View.offset = Offset;
    View.size = Size;
    View.type = Type;
    BufferViews.push_back( View );
    return &BufferViews.back();
}

//=========================================================================

cgltf_accessor* gltf_document_writer::AddAccessor( cgltf_buffer_view* pView, cgltf_component_type ComponentType,
                                                   cgltf_type Type, cgltf_size Count )
{
    cgltf_accessor Accessor;
    x_memset( &Accessor, 0, sizeof( Accessor ) );
    Accessor.component_type = ComponentType;
    Accessor.type = Type;
    Accessor.count = Count;
    Accessor.buffer_view = pView;
    Accessors.push_back( Accessor );
    return &Accessors.back();
}

//=========================================================================

cgltf_node* gltf_document_writer::FindNode( const std::string& Name )
{
    for( cgltf_node& Node : Nodes )
    {
        if( Node.name && x_stricmp( Node.name, Name.c_str() ) == 0 )
            return &Node;
    }
    return NULL;
}

//=========================================================================

void gltf_document_writer::AddAnimations( const std::vector<resource_tool::animation::clip>& Clips )
{
    Animations.resize( Clips.size() );
    AnimationSamplers.resize( Clips.size() );
    AnimationChannels.resize( Clips.size() );

    for( usize iClip = 0; iClip < Clips.size(); ++iClip )
    {
        const resource_tool::animation::clip& Clip = Clips[iClip];
        std::vector<cgltf_animation_sampler>& Samplers = AnimationSamplers[iClip];
        std::vector<cgltf_animation_channel>& Channels = AnimationChannels[iClip];
        usize nChannels = 0;
        for( const resource_tool::animation::bone_channel& Channel : Clip.Channels )
        {
            if( !Channel.Times.empty() )
                nChannels += 2 + ( Channel.Scales.size() == Channel.Times.size() ? 1 : 0 );
        }
        Samplers.resize( nChannels );
        Channels.resize( nChannels );

        usize iOutput = 0;
        for( const resource_tool::animation::bone_channel& Source : Clip.Channels )
        {
            const usize nKeys = Source.Times.size();
            if( nKeys == 0 )
                continue;
            if( Source.Translations.size() != nKeys || Source.Rotations.size() != nKeys )
            {
                throw std::runtime_error( "Animation channel has inconsistent key counts" );
            }

            std::vector<f32> Times;
            std::vector<f32> Translations;
            std::vector<f32> Rotations;
            Times.reserve( nKeys );
            Translations.reserve( nKeys * 3 );
            Rotations.reserve( nKeys * 4 );
            for( usize iKey = 0; iKey < nKeys; ++iKey )
            {
                Times.push_back( Source.Times[iKey] );
                const vector3& T = Source.Translations[iKey];
                Translations.push_back( T.GetX() );
                Translations.push_back( T.GetY() );
                Translations.push_back( T.GetZ() );
                const quaternion& R = Source.Rotations[iKey];
                Rotations.push_back( R.X );
                Rotations.push_back( R.Y );
                Rotations.push_back( R.Z );
                Rotations.push_back( R.W );
            }

            cgltf_accessor* pTime = AddAccessor( AddView( Times, cgltf_buffer_view_type_invalid ),
                                                 cgltf_component_type_r_32f, cgltf_type_scalar, nKeys );
            cgltf_accessor* pTranslation = AddAccessor( AddView( Translations, cgltf_buffer_view_type_invalid ),
                                                        cgltf_component_type_r_32f, cgltf_type_vec3, nKeys );
            cgltf_accessor* pRotation = AddAccessor( AddView( Rotations, cgltf_buffer_view_type_invalid ),
                                                     cgltf_component_type_r_32f, cgltf_type_vec4, nKeys );

            cgltf_node* pNode = FindNode( Source.BoneName );
            if( !pNode )
                throw std::runtime_error( "Animation references an unknown bone: " + Source.BoneName );

            cgltf_animation_sampler& TranslationSampler = Samplers[iOutput];
            x_memset( &TranslationSampler, 0, sizeof( TranslationSampler ) );
            TranslationSampler.input = pTime;
            TranslationSampler.output = pTranslation;
            TranslationSampler.interpolation = cgltf_interpolation_type_linear;
            cgltf_animation_channel& TranslationChannel = Channels[iOutput++];
            x_memset( &TranslationChannel, 0, sizeof( TranslationChannel ) );
            TranslationChannel.sampler = &TranslationSampler;
            TranslationChannel.target_node = pNode;
            TranslationChannel.target_path = cgltf_animation_path_type_translation;

            cgltf_animation_sampler& RotationSampler = Samplers[iOutput];
            x_memset( &RotationSampler, 0, sizeof( RotationSampler ) );
            RotationSampler.input = pTime;
            RotationSampler.output = pRotation;
            RotationSampler.interpolation = cgltf_interpolation_type_linear;
            cgltf_animation_channel& RotationChannel = Channels[iOutput++];
            x_memset( &RotationChannel, 0, sizeof( RotationChannel ) );
            RotationChannel.sampler = &RotationSampler;
            RotationChannel.target_node = pNode;
            RotationChannel.target_path = cgltf_animation_path_type_rotation;

            if( Source.Scales.size() == nKeys )
            {
                std::vector<f32> Scales;
                Scales.reserve( nKeys * 3 );
                for( const vector3& Scale : Source.Scales )
                {
                    Scales.push_back( Scale.GetX() );
                    Scales.push_back( Scale.GetY() );
                    Scales.push_back( Scale.GetZ() );
                }
                cgltf_accessor* pScale = AddAccessor( AddView( Scales, cgltf_buffer_view_type_invalid ),
                                                      cgltf_component_type_r_32f, cgltf_type_vec3, nKeys );
                cgltf_animation_sampler& ScaleSampler = Samplers[iOutput];
                x_memset( &ScaleSampler, 0, sizeof( ScaleSampler ) );
                ScaleSampler.input = pTime;
                ScaleSampler.output = pScale;
                ScaleSampler.interpolation = cgltf_interpolation_type_linear;
                cgltf_animation_channel& ScaleChannel = Channels[iOutput++];
                x_memset( &ScaleChannel, 0, sizeof( ScaleChannel ) );
                ScaleChannel.sampler = &ScaleSampler;
                ScaleChannel.target_node = pNode;
                ScaleChannel.target_path = cgltf_animation_path_type_scale;
            }
        }

        cgltf_animation& Animation = Animations[iClip];
        x_memset( &Animation, 0, sizeof( Animation ) );
        Animation.name = const_cast<char*>( Keep( Clip.Name ) );
        Animation.samplers = Samplers.data();
        Animation.samplers_count = iOutput;
        Animation.channels = Channels.data();
        Animation.channels_count = iOutput;
    }
    Data.animations = Animations.data();
    Data.animations_count = Animations.size();
}

//=========================================================================

xbool gltf_document_writer::Save( const char* pOutputFile, const char* pBinaryFileName, std::string& Error )
{
    Buffer.size = Align4( Binary.size() );
    Binary.resize( Buffer.size );
    Buffer.data = Binary.data();

    std::filesystem::path BinaryPath( pBinaryFileName && pBinaryFileName[0] ? pBinaryFileName : pOutputFile );
    if( BinaryPath.extension() != ".bin" )
        BinaryPath.replace_extension( ".bin" );
    std::ofstream BinaryFile( BinaryPath, std::ios::binary );
    if( !BinaryFile )
    {
        Error = xfs( "Unable to write glTF buffer [%s]", BinaryPath.string().c_str() );
        return FALSE;
    }
    BinaryFile.write( reinterpret_cast<const char*>( Binary.data() ), Binary.size() );
    if( !BinaryFile )
    {
        Error = xfs( "Unable to write glTF buffer [%s]", BinaryPath.string().c_str() );
        return FALSE;
    }

    Data.buffer_views = BufferViews.data();
    Data.buffer_views_count = BufferViews.size();
    Data.accessors = Accessors.data();
    Data.accessors_count = Accessors.size();

    cgltf_options Options;
    x_memset( &Options, 0, sizeof( Options ) );
    Options.type = cgltf_file_type_gltf;
    if( cgltf_write_file( &Options, pOutputFile, &Data ) != cgltf_result_success )
    {
        Error = xfs( "Unable to write glTF JSON [%s]", pOutputFile );
        return FALSE;
    }
    return TRUE;
}

} // namespace geom_gltf_detail
