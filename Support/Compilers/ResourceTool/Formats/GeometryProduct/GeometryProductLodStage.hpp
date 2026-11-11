//=========================================================================
//
//  GeometryProductLodStage.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_LOD_STAGE_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_LOD_STAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "Model/GeometryAsset.hpp"
#include "GeometryProductData.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_lod_stage final
{
public:
    xbool Execute( const geometry_product_data& Source, geometry_asset& Asset, std::string& Error ) const;
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_LOD_STAGE_HPP
//=========================================================================
