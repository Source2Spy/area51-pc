//=========================================================================
//
//  BitmapCompileJob.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_COMPILE_JOB_HPP
#define RESOURCE_TOOL_BITMAP_COMPILE_JOB_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "BitmapBuildProfile.hpp"

#include <filesystem>
#include <string>

namespace resource_tool::bitmap
{

//=========================================================================
//  TYPES
//=========================================================================

class bitmap_compile_job final
{
public:
    xbool Execute( const std::filesystem::path& SourcePath, const std::filesystem::path& OutputPath,
                   const build_profile& Profile, std::string& Error ) const;
};

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_COMPILE_JOB_HPP
//=========================================================================
