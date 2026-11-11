//=========================================================================
//
//  CollisionImportStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "CollisionImportStage.hpp"

#include "Diagnostics.hpp"
#include "GeometrySource.hpp"
#include "IdentityStage.hpp"
#include "ValidationStage.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::collision_import_stage::Execute( const char* pSourceFile, f32 UnitScale,
                                                                geometry_asset& Asset,
                                                                diagnostic_log& Diagnostics ) const
{
    std::string Error;
    if( !geometry_source::LoadMesh( pSourceFile, Asset, geometry_source::ASSET_ROLE_COLLISION, UnitScale, Error ) )
    {
        Diagnostics.Error( Error.c_str(), "GeometryCollisionImport", "collision", pSourceFile );
        return FALSE;
    }

    identity_stage Identity;
    if( !Identity.Execute( Asset, Diagnostics, pSourceFile ) )
        return FALSE;

    validation_options Options;
    Options.Collision = TRUE;
    validation_stage Validation;
    return Validation.Execute( Asset, Options, Diagnostics, pSourceFile );
}
