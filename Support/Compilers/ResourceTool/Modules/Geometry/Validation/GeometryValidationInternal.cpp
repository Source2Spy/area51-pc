//=========================================================================
//
//  GeometryValidationInternal.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryValidationInternal.hpp"

#include "Diagnostics.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::validation_detail::Report( diagnostic_log& Diagnostics, const char* pMessage,
                                                          const char* pAsset, const char* pSourceFile )
{
    Diagnostics.Error( pMessage, "GeometryValidation", pAsset, pSourceFile );
    return FALSE;
}
