//=========================================================================
//
//  GeometryProductReader.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductReader.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_reader::Load( const char* pFileName, geometry_product_kind Kind,
                                     geometry_product_snapshot& Product, std::string& Error ) const
{
    return Product.Load( pFileName, Kind, Error );
}

} // namespace resource_tool::geometry_product_format
