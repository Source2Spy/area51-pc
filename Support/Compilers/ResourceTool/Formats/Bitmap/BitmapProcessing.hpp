//=========================================================================
//
//  BitmapProcessing.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_PROCESSING_HPP
#define RESOURCE_TOOL_BITMAP_PROCESSING_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "Model/BitmapImage.hpp"

#include <string>

namespace resource_tool::bitmap
{

//=========================================================================
//  PUBLIC FUNCTIONS
//=========================================================================

xbool ApplyDetailMap( image& Source, std::string& Error );
xbool ApplyPunchThrough( image& Diffuse, const image& PunchThrough, std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_PROCESSING_HPP
//=========================================================================
