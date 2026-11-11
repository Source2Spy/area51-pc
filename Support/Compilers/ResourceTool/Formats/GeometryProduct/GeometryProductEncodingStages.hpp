//=========================================================================
//
//  GeometryProductEncodingStages.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_STAGES_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_STAGES_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "GeometryProductEncodingTypes.hpp"

struct geometry_asset;
struct geometry_product_compile_options;
class geometry_product_writer;

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_prepare_stage final
{
public:
    static void Execute( geometry_product_writer& Writer, const geometry_asset& Asset,
                         const geometry_asset* pPhysicsAsset, geometry_product_kind Kind,
                         const geometry_product_compile_options& Options );
};

//-------------------------------------------------------------------------

class geometry_product_model_stage final
{
public:
    static void Execute( geometry_product_writer& Writer, geometry_product_kind Kind );
};

//-------------------------------------------------------------------------

class geometry_product_stage final
{
public:
    static void Execute( geometry_product_writer& Writer, geometry_product_kind Kind );
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_ENCODING_STAGES_HPP
//=========================================================================
