//=========================================================================
//
//  GeometryProductEncodingPipeline.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_PIPELINE_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_PIPELINE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "GeometryProductEncodingTypes.hpp"
#include "GeometryProductEncodingStages.hpp"

struct geometry_asset;
struct geometry_product_compile_options;
class geometry_product_writer;

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_encoding_pipeline final
{
public:
    static void Execute( geometry_product_writer& Writer, const geometry_asset& Asset,
                         const geometry_asset* pPhysicsAsset, geometry_product_kind Kind,
                         const geometry_product_compile_options& Options );
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_PIPELINE_HPP
//=========================================================================
