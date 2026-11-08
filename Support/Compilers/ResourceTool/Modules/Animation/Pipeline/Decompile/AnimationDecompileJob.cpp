//=========================================================================
//
//  AnimationDecompileJob.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationDecompileJob.hpp"

#include "AnimationPaths.hpp"
#include "AnimationAsset.hpp"
#include "AnimationGltfWriter.hpp"
#include "ArtifactManifest.hpp"
#include "Diagnostics.hpp"
#include "OutputTransaction.hpp"
#include "RawAnimWriter.hpp"
#include "AnimationProductReader.hpp"
#include "AnimationValidation.hpp"
#include "ResourceUtilities.hpp"

#include <filesystem>
#include <string>

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool animation_decompile_job::Execute( resource_tool::resource_context& Context ) const
{
    const std::filesystem::path FinalGltf = resource_tool::animation::MakeOutputPath( Context.GetInputPath(), ".gltf" );
    const std::filesystem::path FinalBinary =
        resource_tool::animation::MakeOutputPath( Context.GetInputPath(), ".bin" );
    const std::filesystem::path FinalRawAnim =
        resource_tool::animation::MakeOutputPath( Context.GetInputPath(), ".rawanim" );
    resource_tool::output_transaction& Transaction = Context.Transaction();
    const std::filesystem::path StagedGltf = Transaction.Stage( FinalGltf );
    const std::filesystem::path StagedBinary = Transaction.Stage( FinalBinary );
    const std::filesystem::path StagedRawAnim = Transaction.Stage( FinalRawAnim );

    resource_tool::animation::asset Asset;
    std::string Error;
    resource_tool::animation_product_format::animation_product_reader Reader;
    if( !Reader.Load( Context.GetInputPath().generic_string().c_str(), Asset, Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationProductRead" );
    if( !resource_tool::animation::ValidateAsset( Asset, Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationValidation" );
    Asset.Source.SourceFile = FinalGltf.filename().generic_string();

    resource_tool::animation_product_format::animation_gltf_writer GltfWriter;
    if( !GltfWriter.Write( Asset, StagedGltf.generic_string().c_str(), StagedBinary.generic_string().c_str(),
                           FinalBinary.generic_string().c_str(), Error ) )
    {
        return resource_tool::ReportError( Context, Error, "AnimationGltfExport" );
    }

    resource_tool::animation_product_format::raw_anim_writer RawAnimWriter;
    if( !RawAnimWriter.Write( Asset, StagedRawAnim.generic_string().c_str(), Error ) )
        return resource_tool::ReportError( Context, Error, "AnimationRawAnimExport" );

    Context.Artifacts().Add( resource_tool::artifact_kind::Main, "animation-gltf", FinalGltf );
    Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "animation-gltf-binary", FinalBinary );
    Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "rawanim", FinalRawAnim );
    return TRUE;
}
