//=========================================================================
//
//  AnimationPaths.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_PATHS_HPP
#define RESOURCE_TOOL_ANIMATION_PATHS_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <filesystem>

namespace resource_tool::animation
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

std::filesystem::path MakeOutputPath( const std::filesystem::path& Input, const char* pExtension );

} // namespace resource_tool::animation

#endif // RESOURCE_TOOL_ANIMATION_PATHS_HPP
//=========================================================================
