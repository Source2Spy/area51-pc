//=========================================================================
//
//  ValidationStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ValidationStage.hpp"

#include "Validation/GeometryValidationInternal.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::validation_stage::Execute( const geometry_asset& Asset,
                                                          const validation_options& Options,
                                                          diagnostic_log& Diagnostics, const char* pSourceFile ) const
{
    using namespace validation_detail;
    xbool bValid = TRUE;
    bValid = ValidateRender( Asset, Options, Diagnostics, pSourceFile ) && bValid;
    bValid = ValidateMaterials( Asset, Options, Diagnostics, pSourceFile ) && bValid;
    bValid = ValidateSkeletonAndPhysics( Asset, Options, Diagnostics, pSourceFile ) && bValid;
    bValid = ValidateLodsAndCollision( Asset, Options, Diagnostics, pSourceFile ) && bValid;
    return bValid;
}
