//=========================================================================
//
//  BitmapDependencyDiscovery.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_DEPENDENCY_DISCOVERY_HPP
#define RESOURCE_TOOL_BITMAP_DEPENDENCY_DISCOVERY_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "ResourceHandler.hpp"
#include "RawBitmapDescriptor.hpp"

#include <filesystem>
#include <string>

namespace resource_tool::bitmap
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool DiscoverDependencies( resource_tool::resource_context& Context, bitmap_format::raw_bitmap_descriptor& Descriptor,
                            std::filesystem::path& SourcePath, std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_DEPENDENCY_DISCOVERY_HPP
//=========================================================================
