//=========================================================================
//
//  GeometrySource.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_SOURCE_HPP
#define RESOURCE_TOOL_GEOMETRY_SOURCE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "Model/GeometryAsset.hpp"

#include <string>

namespace geometry_source
{

//=========================================================================
//  TYPES
//=========================================================================

enum asset_role
{
    ASSET_ROLE_RENDER,
    ASSET_ROLE_COLLISION,
    ASSET_ROLE_PHYSICS,
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool IsSourceFile( const char* pFileName );
xbool LoadMesh( const char* pFileName, geometry_asset& Asset, asset_role Role, f32 UnitScale, std::string& Error );

} // namespace geometry_source

#endif // RESOURCE_TOOL_GEOMETRY_SOURCE_HPP
//=========================================================================
