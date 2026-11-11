//=========================================================================
//
//  RawGeomProjection.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_RAW_GEOM_PROJECTION_HPP
#define RESOURCE_TOOL_GEOMETRY_RAW_GEOM_PROJECTION_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "GeometryDescriptor.hpp"
#include "Model/GeometryAsset.hpp"
#include "Model/GeometryTextureMap.hpp"

namespace resource_tool
{
namespace geometry
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

void ProjectProductToDescriptor( geometry_descriptor::geometry_type Type, const geometry_asset& Model,
                                 geometry_descriptor& Settings, const texture_export_map* pTextureMap = NULL );

} // namespace geometry
} // namespace resource_tool

#endif // RESOURCE_TOOL_GEOMETRY_RAW_GEOM_PROJECTION_HPP
//=========================================================================
