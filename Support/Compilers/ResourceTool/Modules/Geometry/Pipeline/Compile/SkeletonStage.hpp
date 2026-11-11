//=========================================================================
//
//  SkeletonStage.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_SKELETON_STAGE_HPP
#define RESOURCE_TOOL_GEOMETRY_SKELETON_STAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "Model/GeometryAsset.hpp"

namespace resource_tool
{
class diagnostic_log;

namespace geometry
{

//=========================================================================
//  TYPES
//=========================================================================

class skeleton_stage final
{
public:
    xbool Execute( const geometry_asset& Asset, diagnostic_log& Diagnostics, const char* pSourceFile ) const;
};

} // namespace geometry
} // namespace resource_tool

#endif // RESOURCE_TOOL_GEOMETRY_SKELETON_STAGE_HPP
//=========================================================================
