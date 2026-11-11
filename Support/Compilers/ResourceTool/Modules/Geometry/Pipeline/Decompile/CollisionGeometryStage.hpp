//=========================================================================
//
//  CollisionGeometryStage.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_COLLISION_GEOMETRY_STAGE_HPP
#define RESOURCE_TOOL_GEOMETRY_COLLISION_GEOMETRY_STAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "GltfWriter.hpp"
#include "Model/GeometryAsset.hpp"

namespace resource_tool
{
namespace geometry
{

//=========================================================================
//  TYPES
//=========================================================================

class collision_geometry_stage final
{
public:
    xbool Execute( const geometry_asset& Model, const char* pOutputFile, const char* pBinaryFile,
                   const char* pBinaryUriFile, std::string& Error ) const;
};

} // namespace geometry
} // namespace resource_tool

#endif // RESOURCE_TOOL_GEOMETRY_COLLISION_GEOMETRY_STAGE_HPP
//=========================================================================
