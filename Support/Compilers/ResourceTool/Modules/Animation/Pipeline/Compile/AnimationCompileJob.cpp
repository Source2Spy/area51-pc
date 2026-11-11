//=========================================================================
//
//  AnimationCompileJob.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationCompileJob.hpp"

#include "AnimationAsset.hpp"
#include "AnimationPaths.hpp"
#include "AnimationGltfReader.hpp"
#include "ArtifactManifest.hpp"
#include "Diagnostics.hpp"
#include "OutputTransaction.hpp"
#include "RawAnimReader.hpp"
#include "AnimationProductWriter.hpp"
#include "AnimationValidation.hpp"
#include "ResourceUtilities.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool MergeRawAnimMetadata( resource_tool::animation::asset& Scene, const resource_tool::animation::asset& Metadata,
                            std::string& Error )
{
    if( Scene.Skeleton.Bones.size() != Metadata.Skeleton.Bones.size() )
    {
        Error = "Animation glTF/rawanim skeleton bone counts differ";
        return FALSE;
    }
    for( usize iBone = 0; iBone < Metadata.Skeleton.Bones.size(); ++iBone )
    {
        const resource_tool::animation::skeleton_bone& MetadataBone = Metadata.Skeleton.Bones[iBone];
        const resource_tool::animation::skeleton_bone& SceneBone = Scene.Skeleton.Bones[iBone];
        if( x_stricmp( SceneBone.Name.c_str(), MetadataBone.Name.c_str() ) != 0 )
        {
            Error = xfs( "Animation glTF/rawanim bone order differs at index %d: glTF [%s], rawanim [%s]",
                         (s32)iBone, SceneBone.Name.c_str(), MetadataBone.Name.c_str() );
            return FALSE;
        }
        if( SceneBone.Parent != MetadataBone.Parent || SceneBone.Children != MetadataBone.Children )
        {
            Error = xfs( "Animation glTF/rawanim hierarchy differs at bone index %d [%s]", (s32)iBone,
                         MetadataBone.Name.c_str() );
            return FALSE;
        }
        Scene.Skeleton.Bones[iBone].LodGroup = MetadataBone.LodGroup;
        Scene.Skeleton.Bones[iBone].IsMasked = MetadataBone.IsMasked;
    }

    if( Scene.Clips.size() != Metadata.Clips.size() )
    {
        Error = "Animation glTF/rawanim clip counts differ";
        return FALSE;
    }
    for( usize iClip = 0; iClip < Metadata.Clips.size(); ++iClip )
    {
        const resource_tool::animation::clip& MetadataClip = Metadata.Clips[iClip];
        resource_tool::animation::clip& SceneClip = Scene.Clips[iClip];
        if( x_stricmp( SceneClip.Name.c_str(), MetadataClip.Name.c_str() ) != 0 )
        {
            Error = xfs( "Animation glTF/rawanim clip order differs at index %d: glTF [%s], rawanim [%s]",
                         (s32)iClip, SceneClip.Name.c_str(), MetadataClip.Name.c_str() );
            return FALSE;
        }
        std::vector<resource_tool::animation::bone_channel> Channels = std::move( SceneClip.Channels );
        SceneClip = MetadataClip;
        SceneClip.Channels = std::move( Channels );
        if( SceneClip.Frames < 2 || SceneClip.FPS <= 0 )
        {
            Error = "Animation rawanim clip has invalid timing: " + MetadataClip.Name;
            return FALSE;
        }
    }
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool animation_compile_job::Execute( resource_tool::resource_context& Context ) const
{
    const std::filesystem::path GltfPath = resource_tool::animation::MakeOutputPath( Context.GetInputPath(), ".gltf" );
    const std::filesystem::path FinalAnim = resource_tool::animation::MakeOutputPath( Context.GetInputPath(), ".anim" );
    resource_tool::animation::asset Scene;
    resource_tool::animation::asset Metadata;
    std::string Error;
    resource_tool::animation_product_format::animation_gltf_reader GltfReader;
    resource_tool::animation_product_format::raw_anim_reader RawAnimReader;
    if( !GltfReader.Load( GltfPath.generic_string().c_str(), Scene, Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationGltfImport" );
    if( !RawAnimReader.Load( Context.GetInputPath().generic_string().c_str(), Metadata, Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationRawAnimImport" );
    if( !MergeRawAnimMetadata( Scene, Metadata, Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationMetadataMerge" );
    if( !resource_tool::animation::ValidateAsset( Scene, Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationValidation" );

    const std::filesystem::path StagedAnim = Context.Transaction().Stage( FinalAnim );
    resource_tool::animation_product_format::animation_product_writer Writer;
    if( !Writer.Write( Scene, StagedAnim.generic_string().c_str(), Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationProductWrite" );
    Context.Artifacts().Add( resource_tool::artifact_kind::Main, "animation-product", FinalAnim );
    return TRUE;
}
