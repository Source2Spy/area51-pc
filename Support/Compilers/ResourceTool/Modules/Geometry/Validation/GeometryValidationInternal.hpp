//=========================================================================
//
//  GeometryValidationInternal.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_VALIDATION_INTERNAL_HPP
#define RESOURCE_TOOL_GEOMETRY_VALIDATION_INTERNAL_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "Model/GeometryAsset.hpp"
#include "ValidationStage.hpp"
#include "Model/GeometryNames.hpp"

#include <cmath>

namespace resource_tool::geometry::validation_detail
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

inline xbool IsFinite( f32 Value )
{
    return std::isfinite( Value ) != 0;
}

xbool Report( diagnostic_log& Diagnostics, const char* pMessage, const char* pAsset, const char* pSourceFile );

template <typename Item> s32 FindNamed( const std::vector<Item>& Items, const std::string& Name )
{
    s32 Found = -1;
    for( s32 i = 0; i < (s32)Items.size(); ++i )
    {
        if( !geometry_names_match( Items[i].Name.c_str(), Name.c_str() ) )
            continue;
        if( Found >= 0 )
            return -2;
        Found = i;
    }
    return Found;
}

xbool ValidateRender( const geometry_asset& Asset, const validation_options& Options, diagnostic_log& Diagnostics,
                      const char* pSourceFile );

xbool ValidateMaterials( const geometry_asset& Asset, const validation_options& Options, diagnostic_log& Diagnostics,
                         const char* pSourceFile );

xbool ValidateSkeletonAndPhysics( const geometry_asset& Asset, const validation_options& Options,
                                  diagnostic_log& Diagnostics, const char* pSourceFile );

xbool ValidateLodsAndCollision( const geometry_asset& Asset, const validation_options& Options,
                                diagnostic_log& Diagnostics, const char* pSourceFile );

} // namespace resource_tool::geometry::validation_detail

#endif // RESOURCE_TOOL_GEOMETRY_VALIDATION_INTERNAL_HPP
//=========================================================================
