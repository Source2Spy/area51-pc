//=========================================================================
//
//  GeometryProductReconstruction.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_RECONSTRUCTION_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_RECONSTRUCTION_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "Model/GeometryAsset.hpp"
#include "GeometryProductSnapshot.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_reconstruction final
{
public:
    xbool Execute( const geometry_product_snapshot& Product, geometry_asset& Asset, std::string& Error ) const;
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_RECONSTRUCTION_HPP
//=========================================================================
