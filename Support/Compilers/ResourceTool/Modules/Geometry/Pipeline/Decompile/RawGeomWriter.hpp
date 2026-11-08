//=========================================================================
//
//  RawGeomWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_RAW_GEOM_WRITER_HPP
#define RESOURCE_TOOL_GEOMETRY_RAW_GEOM_WRITER_HPP

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
//  TYPES
//=========================================================================

class raw_geom_writer final
{
public:
    xbool Execute( const geometry_descriptor& InputSettings, geometry_descriptor::geometry_type Type,
                   const char* pGeometryFile, const char* pRawGeomFile, const char* pCollisionDescriptor,
                   xbool bCollisionExported, const geometry_asset& Model, std::string& Error,
                   const texture_export_map* pTextureMap = NULL ) const;
};

} // namespace geometry
} // namespace resource_tool

#endif // RESOURCE_TOOL_GEOMETRY_RAW_GEOM_WRITER_HPP
//=========================================================================
