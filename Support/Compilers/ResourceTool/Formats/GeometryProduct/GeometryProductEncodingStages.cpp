//=========================================================================
//
//  GeometryProductEncodingStages.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductEncodingStages.hpp"

#include "GeometryProductWriter.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void resource_tool::geometry_product_format::geometry_product_prepare_stage::Execute(
    geometry_product_writer& Writer, const geometry_asset& Asset, const geometry_asset* pPhysicsAsset,
    geometry_product_kind Kind, const geometry_product_compile_options& Options )
{
    Writer.BeginExport( Asset, pPhysicsAsset, Kind, Options );
}

//=========================================================================

void resource_tool::geometry_product_format::geometry_product_model_stage::Execute( geometry_product_writer& Writer,
                                                                                    geometry_product_kind Kind )
{
    Writer.BuildProductModel( Kind );
}

//=========================================================================

void resource_tool::geometry_product_format::geometry_product_stage::Execute( geometry_product_writer& Writer,
                                                                              geometry_product_kind Kind )
{
    Writer.WriteProduct( Kind );
}
