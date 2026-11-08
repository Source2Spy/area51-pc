//=========================================================================
//
//  GeometryProductFileWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_FILE_WRITER_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_FILE_WRITER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <string>

struct rigid_geom;
struct skin_geom;

namespace resource_tool::geometry
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_file_writer final
{
public:
    xbool WriteRigid( const char* pOutputFile, const rigid_geom& Geometry, std::string& Error ) const;
    xbool WriteSkin( const char* pOutputFile, const skin_geom& Geometry, std::string& Error ) const;
};

} // namespace resource_tool::geometry

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_FILE_WRITER_HPP
//=========================================================================
