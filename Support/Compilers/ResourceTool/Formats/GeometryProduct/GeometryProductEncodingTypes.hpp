//=========================================================================
//
//  GeometryProductEncodingTypes.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_TYPES_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_TYPES_HPP

//=========================================================================
//  TYPES
//=========================================================================

namespace resource_tool::geometry_product_format
{

// Product kind selects the semantic encoding layout.
enum class geometry_product_kind
{
    Rigid,
    Skin
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_TYPES_HPP
//=========================================================================
