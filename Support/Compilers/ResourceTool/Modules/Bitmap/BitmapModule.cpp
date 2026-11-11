//=========================================================================
//
//  BitmapModule.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapModule.hpp"

#include "ArtifactManifest.hpp"
#include "BitmapDependencyDiscovery.hpp"
#include "BitmapCompileJob.hpp"
#include "BitmapDecompileJob.hpp"
#include "DependencyGraph.hpp"
#include "Diagnostics.hpp"
#include "OutputTransaction.hpp"
#include "RawBitmapReader.hpp"
#include "ResourceUtilities.hpp"

#include <filesystem>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{
xbool IsDecompileProductExtension( const xstring& Extension )
{
    return Extension == ".xbmp";
}

//=========================================================================

std::filesystem::path ProductPath( const resource_tool::resource_request& Request )
{
    std::filesystem::path Result = Request.InputPath;
    if( Request.Operation == resource_tool::operation::Compile )
        Result.replace_extension( ".XBMP" );
    else
        Result.replace_extension( ".tga" );
    return Result.lexically_normal();
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

namespace resource_tool::bitmap
{

//=========================================================================

xbool module::CanHandle( const resource_tool::resource_request& Request ) const
{
    const xstring Extension = resource_tool::LowerExtension( Request.InputPath );
    if( Request.Operation == resource_tool::operation::Compile )
        return Extension == ".rawbitmap";
    return IsDecompileProductExtension( Extension );
}

//=========================================================================

xbool module::DiscoverDependencies( resource_tool::resource_context& Context )
{
    resource_tool::bitmap_format::raw_bitmap_descriptor Descriptor;
    std::filesystem::path SourcePath;
    std::string Error;
    std::error_code ErrorCode;
    if( !std::filesystem::is_regular_file( Context.GetInputPath(), ErrorCode ) || ErrorCode )
        return resource_tool::ReportError(
            Context, "Bitmap input is not a regular file: " + Context.GetInputPath().generic_string(),
            "BitmapDiscovery" );

    if( Context.GetRequest().Operation == resource_tool::operation::Compile )
    {
        if( !resource_tool::bitmap::DiscoverDependencies( Context, Descriptor, SourcePath, Error ) )
        {
            return resource_tool::ReportError(
                Context, Error.empty() ? "Unable to discover bitmap dependencies" : Error, "BitmapDiscovery" );
        }
        return TRUE;
    }

    Context.Dependencies().Add( resource_tool::dependency_kind::Source, Context.GetInputPath() );
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
    const std::filesystem::path FinalProduct = ProductPath( Context.GetRequest() );
    resource_tool::bitmap_format::raw_bitmap_descriptor Descriptor;
    std::string Error;
    resource_tool::bitmap_format::raw_bitmap_reader Reader;
    if( !Reader.Load( Context.GetInputPath(), Descriptor, Error ) )
        return resource_tool::ReportError( Context, Error, "BitmapRawBitmapImport" );

    std::filesystem::path SourcePath = Descriptor.SourceFile;
    if( SourcePath.is_relative() )
        SourcePath = Context.GetInputPath().parent_path() / SourcePath;
    SourcePath = SourcePath.lexically_normal();

    resource_tool::output_transaction& Transaction = Context.Transaction();
    const std::filesystem::path StagedProduct = Transaction.Stage( FinalProduct );
    resource_tool::bitmap::bitmap_compile_job Job;
    if( !Job.Execute( SourcePath, StagedProduct, Descriptor.Profile, Error ) )
        return resource_tool::ReportError( Context, Error, "BitmapCompile" );

    Context.Artifacts().Add( resource_tool::artifact_kind::Main, "bitmap", FinalProduct );
    if( !Transaction.Commit( Error ) )
        return resource_tool::ReportError( Context, Error, "OutputTransaction" );
    return TRUE;
}

//=========================================================================

xbool module::Decompile( resource_tool::resource_context& Context )
{
    const std::filesystem::path FinalProduct = ProductPath( Context.GetRequest() );
    std::filesystem::path FinalRawBitmap = Context.GetRequest().InputPath;
    FinalRawBitmap.replace_extension( ".rawbitmap" );
    std::string Error;

    resource_tool::output_transaction& Transaction = Context.Transaction();
    const std::filesystem::path StagedProduct = Transaction.Stage( FinalProduct );
    const std::filesystem::path StagedRawBitmap = Transaction.Stage( FinalRawBitmap );
    resource_tool::bitmap::bitmap_decompile_job Job;
    if( !Job.Execute( Context.GetInputPath(), StagedProduct, StagedRawBitmap, Error ) )
    {
        return resource_tool::ReportError( Context, Error, "BitmapDecompile" );
    }

    Context.Artifacts().Add( resource_tool::artifact_kind::Main, "bitmap", FinalProduct );
    Context.Artifacts().Add( resource_tool::artifact_kind::Auxiliary, "rawbitmap", FinalRawBitmap );
    if( !Transaction.Commit( Error ) )
        return resource_tool::ReportError( Context, Error, "OutputTransaction" );
    return TRUE;
}

} // namespace resource_tool::bitmap
