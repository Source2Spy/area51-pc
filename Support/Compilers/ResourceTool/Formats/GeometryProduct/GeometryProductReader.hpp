//=========================================================================
//
//  GeometryProductReader.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_READER_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_READER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "GeometryProductSnapshot.hpp"

#include <string>

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_reader final
{
public:
    xbool Load( const char* pFileName, geometry_product_kind Kind, geometry_product_snapshot& Product,
                std::string& Error ) const;
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_READER_HPP
//=========================================================================
