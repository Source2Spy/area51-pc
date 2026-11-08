//=========================================================================
//
//  GeometryProductFormat.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_FORMAT_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_FORMAT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <string>

struct rigid_geom;
struct skin_geom;

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool LoadRigid( X_FILE* pFile, rigid_geom*& pGeometry, std::string& Error );
xbool LoadSkin( X_FILE* pFile, skin_geom*& pGeometry, std::string& Error );
xbool LoadRigid( const char* pFileName, rigid_geom*& pGeometry, std::string& Error );
xbool LoadSkin( const char* pFileName, skin_geom*& pGeometry, std::string& Error );

xbool SaveRigid( const char* pFileName, const rigid_geom& Geometry, std::string& Error );
xbool SaveSkin( const char* pFileName, const skin_geom& Geometry, std::string& Error );

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_FORMAT_HPP
//=========================================================================
