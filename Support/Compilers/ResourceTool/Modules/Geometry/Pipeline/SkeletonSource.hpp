//=========================================================================
//
//  SkeletonSource.hpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#ifndef RESOURCE_TOOL_GEOMETRY_SKELETON_SOURCE_HPP
#define RESOURCE_TOOL_GEOMETRY_SKELETON_SOURCE_HPP

#include "AnimationAsset.hpp"
#include "Model/GeometryAsset.hpp"

#include <string>

namespace resource_tool::geometry
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool LoadSkeletonSource( const char* pFileName, resource_tool::animation::asset& Asset, std::string& Error );

xbool ApplySkeletonSource( const resource_tool::animation::asset& Source, geometry_asset& Target, std::string& Error );

} // namespace resource_tool::geometry

#endif // RESOURCE_TOOL_GEOMETRY_SKELETON_SOURCE_HPP
//=========================================================================
