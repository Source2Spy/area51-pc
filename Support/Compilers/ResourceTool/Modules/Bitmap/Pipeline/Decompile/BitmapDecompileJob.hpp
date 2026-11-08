//=========================================================================
//
//  BitmapDecompileJob.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_DECOMPILE_JOB_HPP
#define RESOURCE_TOOL_BITMAP_DECOMPILE_JOB_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include <filesystem>
#include <string>

namespace resource_tool::bitmap
{

//=========================================================================
//  TYPES
//=========================================================================

class bitmap_decompile_job final
{
public:
    xbool Execute( const std::filesystem::path& SourcePath, const std::filesystem::path& ImageOutputPath,
                   std::string& Error ) const;

    xbool Execute( const std::filesystem::path& SourcePath, const std::filesystem::path& ImageOutputPath,
                   const std::filesystem::path& RawBitmapOutputPath, std::string& Error ) const;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool DecompileProduct( const std::filesystem::path& SourcePath, const std::filesystem::path& ImageOutputPath,
                        std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_DECOMPILE_JOB_HPP
//=========================================================================
