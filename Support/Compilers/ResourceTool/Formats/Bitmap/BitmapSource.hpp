//=========================================================================
//
//  BitmapSource.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_SOURCE_HPP
#define RESOURCE_TOOL_BITMAP_SOURCE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "Model/BitmapImage.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace resource_tool::bitmap
{

//=========================================================================
//  PUBLIC FUNCTIONS
//=========================================================================

xbool ResolveSourcePath( const std::filesystem::path& RequestedPath, std::filesystem::path& ResolvedPath,
                         std::string& Error );

xbool LoadSourceImage( const std::filesystem::path& RequestedPath, image& Result, std::filesystem::path& ResolvedPath,
                       std::string& Error );

xbool ValidateTextureDimensions( const image& Source, u32 MaximumWidth, u32 MaximumHeight, xbool RequirePowerOfTwo,
                                 const std::filesystem::path& SourcePath, std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_SOURCE_HPP
//=========================================================================
