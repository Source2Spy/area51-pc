//=========================================================================
//
//  BitmapFormat.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_FORMAT_HPP
#define RESOURCE_TOOL_BITMAP_FORMAT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "BitmapBuildProfile.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace resource_tool::bitmap::format_io
{

//=========================================================================
//  TYPES
//=========================================================================

struct xbmp_metadata
{
    u32 Flags = 0;
    u32 Width = 0;
    u32 Height = 0;
    u32 PhysicalWidth = 0;
    s32 Format = 0;
    s32 MipCount = 0;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool LoadSource( const std::filesystem::path& Path, image& Result, std::string& Error );

xbool LoadXbmp( const std::filesystem::path& Path, image& Result, xbmp_metadata& Metadata, std::string& Error );

xbool SaveXbmp( const std::filesystem::path& Path, const image& Source, const build_profile& Profile,
                std::string& Error );

xbool SaveTga( const std::filesystem::path& Path, const image& Source, std::string& Error );

} // namespace resource_tool::bitmap::format_io

#endif // RESOURCE_TOOL_BITMAP_FORMAT_HPP
//=========================================================================
