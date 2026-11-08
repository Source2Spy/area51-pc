//=========================================================================
//
//  BitmapValidation.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_VALIDATION_HPP
#define RESOURCE_TOOL_BITMAP_VALIDATION_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "BitmapBuildProfile.hpp"

#include <string>

namespace resource_tool::bitmap
{

//=========================================================================
//  PUBLIC FUNCTIONS
//=========================================================================

xbool ValidateImage( const image& Image, std::string& Error );
xbool ValidateBuildProfile( const build_profile& Profile, std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_VALIDATION_HPP
//=========================================================================
