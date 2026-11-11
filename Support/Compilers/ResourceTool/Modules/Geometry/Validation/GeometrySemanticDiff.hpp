//=========================================================================
//
//  GeometrySemanticDiff.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_SEMANTIC_DIFF_HPP
#define RESOURCE_TOOL_GEOMETRY_SEMANTIC_DIFF_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "Model/GeometryAsset.hpp"

namespace resource_tool::geometry
{

//=========================================================================
//  TYPES
//=========================================================================

struct geometry_semantic_diff_options
{
    xbool CompareCollision = TRUE;
    xbool CompareProductMetadata = TRUE;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool GeometrySemanticEqual( const geometry_asset& Expected, const geometry_asset& Actual, std::string& Error,
                             const geometry_semantic_diff_options& Options = geometry_semantic_diff_options() );

} // namespace resource_tool::geometry

#endif // RESOURCE_TOOL_GEOMETRY_SEMANTIC_DIFF_HPP
//=========================================================================
