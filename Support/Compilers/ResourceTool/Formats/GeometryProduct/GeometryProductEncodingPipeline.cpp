//=========================================================================
//
//  GeometryProductEncodingPipeline.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductEncodingPipeline.hpp"

#include "GeometryProductWriter.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void resource_tool::geometry_product_format::geometry_product_encoding_pipeline::Execute(
    geometry_product_writer& Writer, const geometry_asset& Asset, const geometry_asset* pPhysicsAsset,
    geometry_product_kind Kind, const geometry_product_compile_options& Options )
{
    geometry_product_prepare_stage::Execute( Writer, Asset, pPhysicsAsset, Kind, Options );
    geometry_product_model_stage::Execute( Writer, Kind );
    geometry_product_stage::Execute( Writer, Kind );
}
