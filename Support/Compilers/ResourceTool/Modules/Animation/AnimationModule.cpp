//=========================================================================
//
//  AnimationModule.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationModule.hpp"

#include "AnimationCompileJob.hpp"
#include "AnimationDecompileJob.hpp"
#include "Diagnostics.hpp"
#include "DependencyGraph.hpp"
#include "ResourceUtilities.hpp"

#include <filesystem>

namespace resource_tool::animation
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool module::CanHandle( const resource_tool::resource_request& Request ) const
{
    const xstring Extension = resource_tool::LowerExtension( Request.InputPath );
    return ( Request.Operation == resource_tool::operation::Decompile && Extension == ".anim" ) ||
           ( Request.Operation == resource_tool::operation::Compile && Extension == ".rawanim" );
}

//=========================================================================

xbool module::DiscoverDependencies( resource_tool::resource_context& Context )
{
    std::error_code ErrorCode;
    if( !std::filesystem::is_regular_file( Context.GetInputPath(), ErrorCode ) || ErrorCode )
    {
        return resource_tool::ReportError(
            Context, "Animation input is not a regular file: " + Context.GetInputPath().generic_string(),
            "AnimationDiscovery" );
    }

    Context.Dependencies().Add( resource_tool::dependency_kind::Source, Context.GetInputPath() );
    if( Context.GetRequest().Operation == resource_tool::operation::Compile )
    {
        std::filesystem::path GltfPath = Context.GetInputPath();
        GltfPath.replace_extension( ".gltf" );
        if( !std::filesystem::is_regular_file( GltfPath, ErrorCode ) || ErrorCode )
        {
            return resource_tool::ReportError( Context,
                                               "Animation compile requires sibling glTF: " + GltfPath.generic_string(),
                                               "AnimationDiscovery" );
        }
        Context.Dependencies().Add( resource_tool::dependency_kind::Source, GltfPath );
    }
    return TRUE;
}

//=========================================================================

xbool module::Execute( resource_tool::resource_context& Context )
{
    if( Context.GetRequest().Operation == resource_tool::operation::Compile )
        return Compile( Context );
    return Decompile( Context );
}

//=========================================================================

xbool module::Compile( resource_tool::resource_context& Context )
{
    animation_compile_job Job;
    if( !Job.Execute( Context ) )
        return FALSE;

    std::string Error;
    if( !Context.Transaction().Commit( Error ) )
        return resource_tool::ReportError( Context, Error, "OutputTransaction" );
    return TRUE;
}

//=========================================================================

xbool module::Decompile( resource_tool::resource_context& Context )
{
    animation_decompile_job Job;
    if( !Job.Execute( Context ) )
        return FALSE;

    std::string Error;
    if( !Context.Transaction().Commit( Error ) )
        return resource_tool::ReportError( Context, Error, "OutputTransaction" );
    return TRUE;
}

} // namespace resource_tool::animation
