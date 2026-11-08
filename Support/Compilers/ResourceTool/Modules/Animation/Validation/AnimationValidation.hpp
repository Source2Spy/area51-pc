//=========================================================================
//
//  AnimationValidation.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_VALIDATION_HPP
#define RESOURCE_TOOL_ANIMATION_VALIDATION_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "AnimationAsset.hpp"

#include <string>

namespace resource_tool::animation
{

//=========================================================================
//  PUBLIC FUNCTIONS
//=========================================================================

xbool ValidateAsset( const asset& Asset, std::string& Error );

} // namespace resource_tool::animation

#endif // RESOURCE_TOOL_ANIMATION_VALIDATION_HPP
//=========================================================================
