//=========================================================================
//
//  CollisionImportStage.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_COLLISION_IMPORT_STAGE_HPP
#define RESOURCE_TOOL_GEOMETRY_COLLISION_IMPORT_STAGE_HPP

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

class collision_import_stage final
{
public:
    xbool Execute( const char* pSourceFile, f32 UnitScale, geometry_asset& Asset, diagnostic_log& Diagnostics ) const;
};

} // namespace geometry
} // namespace resource_tool

#endif // RESOURCE_TOOL_GEOMETRY_COLLISION_IMPORT_STAGE_HPP
//=========================================================================
