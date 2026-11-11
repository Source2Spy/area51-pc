//=========================================================================
//
//  BitmapDependencyDiscovery.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapDependencyDiscovery.hpp"

#include "DependencyGraph.hpp"
#include "Diagnostics.hpp"
#include "RawBitmapReader.hpp"

#include <filesystem>

namespace resource_tool::bitmap
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool DiscoverDependencies( resource_tool::resource_context& Context, bitmap_format::raw_bitmap_descriptor& Descriptor,
                            std::filesystem::path& SourcePath, std::string& Error )
{
    const std::filesystem::path InputPath = Context.GetInputPath().lexically_normal();
    std::error_code ErrorCode;
    if( !std::filesystem::is_regular_file( InputPath, ErrorCode ) || ErrorCode )
    {
        Error = "Bitmap input is not a regular file: " + InputPath.generic_string();
        return FALSE;
    }

    bitmap_format::raw_bitmap_reader Reader;
    if( !Reader.Load( InputPath, Descriptor, Error ) )
        return FALSE;

    SourcePath = Descriptor.SourceFile;
    if( SourcePath.is_relative() )
        SourcePath = InputPath.parent_path() / SourcePath;
    SourcePath = SourcePath.lexically_normal();
    if( !std::filesystem::is_regular_file( SourcePath, ErrorCode ) || ErrorCode )
    {
        Error = "Raw bitmap source is not a regular file: " + SourcePath.generic_string();
        return FALSE;
    }
    Context.Dependencies().Add( resource_tool::dependency_kind::Source, InputPath );
    Context.Dependencies().Add( resource_tool::dependency_kind::Source, SourcePath );
    return TRUE;
}

} // namespace resource_tool::bitmap
