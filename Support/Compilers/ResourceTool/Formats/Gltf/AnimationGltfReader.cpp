//=========================================================================
//
//  AnimationGltfReader.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationGltfReader.hpp"

#include "AnimationAsset.hpp"
#include "cgltf.h"
#include "x_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace resource_tool::animation_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

struct node_transform
{
    vector3 Scale;
    quaternion Rotation;
    vector3 Translation;

    node_transform( void )
    {
        Scale.Set( 1.0f, 1.0f, 1.0f );
        Rotation.Identity();
        Translation.Zero();
    }
};

//-------------------------------------------------------------------------

enum class track_kind
{
    Translation,
    Rotation,
    Scale
};

//-------------------------------------------------------------------------

struct track
{
    track_kind Kind;
    xbool Step = FALSE;
    std::vector<f32> Times;
    std::vector<std::vector<f32>> Values;
};

//-------------------------------------------------------------------------

struct node_tracks
{
    const cgltf_node* pNode = NULL;
    std::vector<track> Tracks;
};

//=========================================================================

xbool IsFinite( f32 Value )
{
    return std::isfinite( Value ) != 0;
}

//=========================================================================

node_transform ReadTransform( const cgltf_node* pNode )
{
    node_transform Result;
    if( !pNode )
        return Result;

    if( pNode->has_matrix )
    {
        cgltf_float Values[16] = {};
        cgltf_node_transform_local( pNode, Values );
        matrix4 Local;
        for( s32 Row = 0; Row < 4; ++Row )
            for( s32 Column = 0; Column < 4; ++Column )
                Local( Column, Row ) = Values[Column * 4 + Row];
        Local.DecomposeSRT( Result.Scale, Result.Rotation, Result.Translation );
        Result.Rotation.Normalize();
        return Result;
    }

    if( pNode->has_scale )
        Result.Scale.Set( pNode->scale[0], pNode->scale[1], pNode->scale[2] );
    if( pNode->has_translation )
        Result.Translation.Set( pNode->translation[0], pNode->translation[1], pNode->translation[2] );
    if( pNode->has_rotation )
    {
        Result.Rotation.X = pNode->rotation[0];
        Result.Rotation.Y = pNode->rotation[1];
        Result.Rotation.Z = pNode->rotation[2];
        Result.Rotation.W = pNode->rotation[3];
        Result.Rotation.Normalize();
    }
    return Result;
}

//=========================================================================

xbool ReadAccessor( const cgltf_accessor* pAccessor, cgltf_size Index, cgltf_size Components, std::vector<f32>& Values,
                    std::string& Error )
{
    Values.resize( Components );
    if( !pAccessor || pAccessor->count <= Index ||
        !cgltf_accessor_read_float( pAccessor, Index, Values.data(), Components ) )
    {
        Error = "Animation glTF accessor is unreadable";
        return FALSE;
    }
    for( const f32 Value : Values )
    {
        if( !IsFinite( Value ) )
        {
            Error = "Animation glTF accessor contains a non-finite value";
            return FALSE;
        }
    }
    return TRUE;
}

//=========================================================================

xbool ReadTrack( const cgltf_animation_sampler& Sampler, track_kind Kind, track& Result, std::string& Error )
{
    if( !Sampler.input || !Sampler.output || Sampler.input->type != cgltf_type_scalar )
    {
        Error = "Animation sampler input must be a scalar accessor";
        return FALSE;
    }
    if( Sampler.interpolation == cgltf_interpolation_type_cubic_spline )
    {
        Error = "Animation glTF CUBICSPLINE interpolation is not supported yet; "
                "use LINEAR or STEP";
        return FALSE;
    }

    const cgltf_size Components = Kind == track_kind::Rotation ? 4 : 3;
    if( Sampler.output->type != ( Components == 4 ? cgltf_type_vec4 : cgltf_type_vec3 ) )
    {
        Error = "Animation sampler output has an invalid component type";
        return FALSE;
    }
    if( Sampler.input->count != Sampler.output->count )
    {
        Error = "Animation sampler input/output key counts differ";
        return FALSE;
    }

    Result.Kind = Kind;
    Result.Step = Sampler.interpolation == cgltf_interpolation_type_step;
    Result.Times.resize( Sampler.input->count );
    Result.Values.resize( Sampler.input->count );
    for( cgltf_size iKey = 0; iKey < Sampler.input->count; ++iKey )
    {
        std::vector<f32> Time;
        if( !ReadAccessor( Sampler.input, iKey, 1, Time, Error ) )
            return FALSE;
        Result.Times[iKey] = Time[0];
        if( iKey > 0 && Result.Times[iKey] < Result.Times[iKey - 1] )
        {
            Error = "Animation sampler input times are not sorted";
            return FALSE;
        }
        if( !ReadAccessor( Sampler.output, iKey, Components, Result.Values[iKey], Error ) )
            return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool SameTime( f32 A, f32 B )
{
    return std::fabs( A - B ) <= 0.000001f;
}

//=========================================================================

void AddTime( std::vector<f32>& Times, f32 Value )
{
    for( const f32 Existing : Times )
        if( SameTime( Existing, Value ) )
            return;
    Times.push_back( Value );
}

//=========================================================================

std::vector<f32> BuildTimes( const node_tracks& Tracks )
{
    std::vector<f32> Result;
    for( const track& Track : Tracks.Tracks )
        for( const f32 Time : Track.Times )
            AddTime( Result, Time );
    std::sort( Result.begin(), Result.end() );
    return Result;
}

//=========================================================================

std::vector<f32> SampleTrack( const track* pTrack, f32 Time, const std::vector<f32>& Default )
{
    if( !pTrack || pTrack->Times.empty() )
        return Default;
    if( Time <= pTrack->Times.front() || pTrack->Times.size() == 1 )
        return pTrack->Values.front();
    if( Time >= pTrack->Times.back() )
        return pTrack->Values.back();

    usize Upper = 1;
    while( Upper < pTrack->Times.size() && pTrack->Times[Upper] < Time )
        ++Upper;
    const usize Lower = Upper - 1;
    if( SameTime( pTrack->Times[Lower], Time ) )
        return pTrack->Values[Lower];
    if( SameTime( pTrack->Times[Upper], Time ) )
        return pTrack->Values[Upper];

    if( pTrack->Step )
        return pTrack->Values[Lower];
    if( pTrack->Kind == track_kind::Scale || pTrack->Kind == track_kind::Translation )
    {
        const f32 Denominator = pTrack->Times[Upper] - pTrack->Times[Lower];
        const f32 Alpha = Denominator > 0.0f ? ( Time - pTrack->Times[Lower] ) / Denominator : 0.0f;
        std::vector<f32> Result( pTrack->Values[Lower].size() );
        for( usize i = 0; i < Result.size(); ++i )
            Result[i] = pTrack->Values[Lower][i] + Alpha * ( pTrack->Values[Upper][i] - pTrack->Values[Lower][i] );
        return Result;
    }

    // STEP uses the sampler interpolation; this path handles LINEAR tracks.
    const f32 Denominator = pTrack->Times[Upper] - pTrack->Times[Lower];
    const f32 Alpha = Denominator > 0.0f ? ( Time - pTrack->Times[Lower] ) / Denominator : 0.0f;
    std::vector<f32> Result( pTrack->Values[Lower].size() );
    for( usize i = 0; i < Result.size(); ++i )
        Result[i] = pTrack->Values[Lower][i] + Alpha * ( pTrack->Values[Upper][i] - pTrack->Values[Lower][i] );
    return Result;
}

//=========================================================================

const track* FindTrack( const node_tracks& Tracks, track_kind Kind )
{
    for( const track& Track : Tracks.Tracks )
        if( Track.Kind == Kind )
            return &Track;
    return NULL;
}

//=========================================================================

xbool BuildSkeleton( cgltf_data* pData, resource_tool::animation::asset& Asset,
                     std::unordered_map<const cgltf_node*, s32>& NodeToBone, std::string& Error )
{
    std::vector<const cgltf_node*> Nodes;
    if( pData->skins_count > 0 && pData->skins[0].joints_count > 0 )
    {
        for( cgltf_size i = 0; i < pData->skins[0].joints_count; ++i )
            Nodes.push_back( pData->skins[0].joints[i] );
        Asset.Skeleton.Name = pData->skins[0].name ? pData->skins[0].name : "Skeleton";
    }
    else
    {
        std::set<const cgltf_node*> Used;
        for( cgltf_size iAnimation = 0; iAnimation < pData->animations_count; ++iAnimation )
        {
            const cgltf_animation& Animation = pData->animations[iAnimation];
            for( cgltf_size iChannel = 0; iChannel < Animation.channels_count; ++iChannel )
            {
                const cgltf_node* pNode = Animation.channels[iChannel].target_node;
                while( pNode )
                {
                    Used.insert( pNode );
                    pNode = pNode->parent;
                }
            }
        }
        for( cgltf_size iNode = 0; iNode < pData->nodes_count; ++iNode )
            if( Used.find( &pData->nodes[iNode] ) != Used.end() )
                Nodes.push_back( &pData->nodes[iNode] );
        Asset.Skeleton.Name = "Skeleton";
    }

    if( Nodes.empty() )
    {
        Error = "Animation glTF contains no skeleton nodes";
        return FALSE;
    }

    Asset.Skeleton.Bones.resize( Nodes.size() );
    const cgltf_skin* pSkin = pData->skins_count > 0 ? &pData->skins[0] : NULL;
    for( usize iNode = 0; iNode < Nodes.size(); ++iNode )
    {
        NodeToBone[Nodes[iNode]] = (s32)iNode;
        resource_tool::animation::skeleton_bone& Bone = Asset.Skeleton.Bones[iNode];
        Bone.Name =
            Nodes[iNode]->name && Nodes[iNode]->name[0] ? Nodes[iNode]->name : (const char*)xfs( "Bone_%d", iNode );
        const node_transform Transform = ReadTransform( Nodes[iNode] );
        Bone.Scale = Transform.Scale;
        Bone.Rotation = Transform.Rotation;
        Bone.Translation = Transform.Translation;
        Bone.InverseBindMatrix.Identity();
        if( pSkin && pSkin->inverse_bind_matrices && pSkin->inverse_bind_matrices->count > iNode )
        {
            cgltf_float Values[16] = {};
            if( !cgltf_accessor_read_float( pSkin->inverse_bind_matrices, iNode, Values, 16 ) )
            {
                Error = "Animation glTF inverse bind matrix accessor is unreadable";
                return FALSE;
            }
            for( s32 Row = 0; Row < 4; ++Row )
                for( s32 Column = 0; Column < 4; ++Column )
                    Bone.InverseBindMatrix( Column, Row ) = Values[Column * 4 + Row];
        }
    }
    for( usize iNode = 0; iNode < Nodes.size(); ++iNode )
    {
        const cgltf_node* pParent = Nodes[iNode]->parent;
        const auto It = NodeToBone.find( pParent );
        if( It != NodeToBone.end() )
        {
            Asset.Skeleton.Bones[iNode].Parent = It->second;
            Asset.Skeleton.Bones[iNode].ParentName = Asset.Skeleton.Bones[It->second].Name;
            ++Asset.Skeleton.Bones[It->second].Children;
        }
    }
    return TRUE;
}

//=========================================================================

xbool BuildClip( const cgltf_animation& Source, const std::unordered_map<const cgltf_node*, s32>& NodeToBone,
                 resource_tool::animation::asset& Asset, std::string& Error )
{
    resource_tool::animation::clip Clip;
    Clip.Name =
        Source.name && Source.name[0] ? Source.name : (const char*)xfs( "Animation_%u", (u32)Asset.Clips.size() );
    std::vector<node_tracks> PerBone( Asset.Skeleton.Bones.size() );
    for( usize iBone = 0; iBone < PerBone.size(); ++iBone )
        PerBone[iBone].pNode = NULL;

    f32 Duration = 0.0f;
    for( cgltf_size iChannel = 0; iChannel < Source.channels_count; ++iChannel )
    {
        const cgltf_animation_channel& Channel = Source.channels[iChannel];
        const auto It = NodeToBone.find( Channel.target_node );
        if( It == NodeToBone.end() || !Channel.sampler )
        {
            Error = "Animation channel targets a node outside the skeleton";
            return FALSE;
        }
        const track_kind Kind = Channel.target_path == cgltf_animation_path_type_translation ? track_kind::Translation
                                : Channel.target_path == cgltf_animation_path_type_rotation  ? track_kind::Rotation
                                                                                             : track_kind::Scale;
        track Track;
        if( !ReadTrack( *Channel.sampler, Kind, Track, Error ) )
            return FALSE;
        if( Track.Times.empty() )
        {
            Error = "Animation channel contains no keys";
            return FALSE;
        }
        Duration = x_max( Duration, Track.Times.back() );
        node_tracks& BoneTracks = PerBone[It->second];
        BoneTracks.pNode = Channel.target_node;
        if( FindTrack( BoneTracks, Kind ) )
        {
            Error = "Animation contains duplicate channels for one node/path";
            return FALSE;
        }
        BoneTracks.Tracks.push_back( std::move( Track ) );
    }

    Clip.FPS = 30;
    Clip.Frames = x_max( 1, (s32)std::ceil( Duration * Clip.FPS ) + 1 );
    for( usize iBone = 0; iBone < PerBone.size(); ++iBone )
    {
        const node_tracks& BoneTracks = PerBone[iBone];
        if( BoneTracks.Tracks.empty() )
            continue;
        const std::vector<f32> Times = BuildTimes( BoneTracks );
        resource_tool::animation::bone_channel Channel;
        Channel.BoneName = Asset.Skeleton.Bones[iBone].Name;
        Channel.Times = Times;
        const node_transform Base = ReadTransform( BoneTracks.pNode );
        const track* pTranslation = FindTrack( BoneTracks, track_kind::Translation );
        const track* pRotation = FindTrack( BoneTracks, track_kind::Rotation );
        const track* pScale = FindTrack( BoneTracks, track_kind::Scale );
        for( const f32 Time : Times )
        {
            const std::vector<f32> T = SampleTrack(
                pTranslation, Time, { Base.Translation.GetX(), Base.Translation.GetY(), Base.Translation.GetZ() } );
            const std::vector<f32> R =
                SampleTrack( pRotation, Time, { Base.Rotation.X, Base.Rotation.Y, Base.Rotation.Z, Base.Rotation.W } );
            Channel.Translations.emplace_back( T[0], T[1], T[2] );
            quaternion Rotation( R[0], R[1], R[2], R[3] );
            Rotation.Normalize();
            Channel.Rotations.push_back( Rotation );
            if( pScale )
            {
                const std::vector<f32> S =
                    SampleTrack( pScale, Time, { Base.Scale.GetX(), Base.Scale.GetY(), Base.Scale.GetZ() } );
                Channel.Scales.emplace_back( S[0], S[1], S[2] );
            }
        }
        Clip.Channels.push_back( std::move( Channel ) );
    }
    Asset.Clips.push_back( std::move( Clip ) );
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool animation_gltf_reader::Load( const char* pFileName, resource_tool::animation::asset& Asset,
                                   std::string& Error ) const
{
    Asset.Clear();
    cgltf_options Options;
    x_memset( &Options, 0, sizeof( Options ) );
    cgltf_data* pData = NULL;
    if( cgltf_parse_file( &Options, pFileName, &pData ) != cgltf_result_success || !pData )
    {
        Error = "Unable to parse animation glTF: " + std::string( pFileName ? pFileName : "" );
        return FALSE;
    }
    if( cgltf_load_buffers( &Options, pData, pFileName ) != cgltf_result_success ||
        cgltf_validate( pData ) != cgltf_result_success )
    {
        Error = "Animation glTF failed validation or buffer loading";
        cgltf_free( pData );
        return FALSE;
    }

    Asset.Name = pFileName ? pFileName : "";
    std::unordered_map<const cgltf_node*, s32> NodeToBone;
    xbool bOK = BuildSkeleton( pData, Asset, NodeToBone, Error );
    if( bOK )
    {
        for( cgltf_size iAnimation = 0; iAnimation < pData->animations_count; ++iAnimation )
        {
            if( !BuildClip( pData->animations[iAnimation], NodeToBone, Asset, Error ) )
            {
                bOK = FALSE;
                break;
            }
        }
    }
    cgltf_free( pData );
    if( !bOK )
        Asset.Clear();
    return bOK;
}

} // namespace resource_tool::animation_product_format
