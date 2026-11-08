//=========================================================================
//
//  RawBitmapDescriptor.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RAW_BITMAP_DESCRIPTOR_HPP
#define RESOURCE_TOOL_RAW_BITMAP_DESCRIPTOR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "BitmapBuildProfile.hpp"

#include <filesystem>
#include <string>

namespace resource_tool::bitmap_format
{

//=========================================================================
//  TYPES
//=========================================================================

struct raw_bitmap_descriptor
{
    std::filesystem::path SourceFile;
    resource_tool::bitmap::build_profile Profile;
};

} // namespace resource_tool::bitmap_format

#endif // RESOURCE_TOOL_RAW_BITMAP_DESCRIPTOR_HPP
//=========================================================================
