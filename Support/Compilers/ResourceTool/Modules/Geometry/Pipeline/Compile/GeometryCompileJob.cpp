//=========================================================================
//
//  GeometryCompileJob.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryCompileJob.hpp"

#include "Diagnostics.hpp"
#include "CollisionImportStage.hpp"
#include "GeometrySource.hpp"
#include "IdentityStage.hpp"
#include "LodStage.hpp"
#include "MaterialStage.hpp"
#include "NormalizeStage.hpp"
#include "PhysicsStage.hpp"
#include "RawGeomImportStage.hpp"
#include "GeometryProductWriter.hpp"
#include "SkeletonStage.hpp"
#include "../SkeletonSource.hpp"
#include "ValidationStage.hpp"
#include "BitmapProduct.hpp"

#include <memory>
#include <string>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

geometry_product_writer::product_kind GetCompilerType( const geometry_descriptor& Settings )
{
    switch( Settings.GetType() )
    {
    case geometry_descriptor::GEOMETRY_TYPE_RIGID:
        return geometry_product_writer::product_kind::Rigid;
    case geometry_descriptor::GEOMETRY_TYPE_SKIN:
        return geometry_product_writer::product_kind::Skin;
    default:
        x_throw( "rawgeom descriptor must specify Type=Rigid or Type=Skin" );
        return geometry_product_writer::product_kind::Rigid;
    }
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::geometry_compile_job::Execute( const geometry_descriptor& Settings,
                                                              const std::filesystem::path& FinalOutput, xbool bVerbose,
                                                              diagnostic_log& Diagnostics,
                                                              bitmap::product_sink* pBitmapSink )
{
    if( !Settings.GetSourceFileName()[0] )
        x_throw( "rawgeom descriptor does not contain a geometry source" );
    if( !Settings.GetOutputFile()[0] )
        x_throw( "rawgeom descriptor does not contain OutputFile" );

    const geometry_product_writer::product_kind Type = GetCompilerType( Settings );
    geometry_descriptor CompileSettings = Settings;

    geometry_asset Asset;
    std::string Error;
    if( !geometry_source::LoadMesh( CompileSettings.GetSourceFileName(), Asset, geometry_source::ASSET_ROLE_RENDER,
                                    CompileSettings.GetUnitScale(), Error ) )
    {
        x_throw( Error.c_str() );
    }

    if( Type == geometry_product_writer::product_kind::Skin )
    {
        resource_tool::animation::asset Skeleton;
        std::string SkeletonError;
        if( !resource_tool::geometry::LoadSkeletonSource( CompileSettings.GetSkeletonSource(), Skeleton,
                                                          SkeletonError ) ||
            !resource_tool::geometry::ApplySkeletonSource( Skeleton, Asset, SkeletonError ) )
        {
            x_throw( SkeletonError.c_str() );
        }
    }

    const char* pSourceFile = CompileSettings.GetSourceFileName();
    identity_stage Identity;
    if( !Identity.Execute( Asset, Diagnostics, pSourceFile ) )
        x_throw( "Geometry identity stage failed" );

    rawgeom_import_stage RawGeomImport;
    if( !RawGeomImport.Execute( CompileSettings, Asset, Diagnostics, pSourceFile ) )
    {
        x_throw( "Geometry rawgeom import stage failed" );
    }

    material_stage Materials;
    if( !Materials.Execute( Asset, Diagnostics, pSourceFile ) )
        x_throw( "Geometry material stage failed" );

    lod_stage LODs;
    if( !LODs.Execute( Asset, Diagnostics, pSourceFile ) )
        x_throw( "Geometry LOD stage failed" );

    skeleton_stage Skeleton;
    if( !Skeleton.Execute( Asset, Diagnostics, pSourceFile ) )
        x_throw( "Geometry skeleton stage failed" );

    physics_stage Physics;
    if( !Physics.Execute( Asset, Diagnostics, pSourceFile ) )
        x_throw( "Geometry physics stage failed" );

    normalize_options NormalizeOptions;
    NormalizeOptions.Rigid = Type == geometry_product_writer::product_kind::Rigid;
    NormalizeOptions.Verbose = bVerbose;
    normalize_stage Normalize;
    if( !Normalize.Execute( Asset, NormalizeOptions, Diagnostics, pSourceFile ) )
    {
        x_throw( "Geometry normalization stage failed" );
    }

    validation_options ValidationOptions;
    ValidationOptions.Collision = FALSE;
    ValidationOptions.Skin = Type == geometry_product_writer::product_kind::Skin;
    validation_stage Validation;
    if( !Validation.Execute( Asset, ValidationOptions, Diagnostics, pSourceFile ) )
    {
        x_throw( "Geometry validation stage failed" );
    }

    geometry_asset PhysicsAsset;
    const geometry_asset* pPhysicsAsset = NULL;
    if( CompileSettings.GetPhysicsSource()[0] )
    {
        if( !geometry_source::LoadMesh( CompileSettings.GetPhysicsSource(), PhysicsAsset,
                                        geometry_source::ASSET_ROLE_PHYSICS, CompileSettings.GetUnitScale(), Error ) )
        {
            x_throw( Error.c_str() );
        }
        pPhysicsAsset = &PhysicsAsset;
    }

    geometry_asset CollisionAsset;
    const geometry_asset* pCollisionAsset = NULL;
    if( CompileSettings.DoCollision() && CompileSettings.GetCollisionSource()[0] )
    {
        collision_import_stage CollisionImport;
        if( !CollisionImport.Execute( CompileSettings.GetCollisionSource(), CompileSettings.GetUnitScale(),
                                      CollisionAsset, Diagnostics ) )
        {
            x_throw( "Collision geometry validation stage failed" );
        }
        pCollisionAsset = &CollisionAsset;
    }

    const std::string TexturePath =
        std::filesystem::path( CompileSettings.GetSourceFileName() ).parent_path().generic_string();
    geometry_product_compile_options Options;
    Options.OutputFile = CompileSettings.GetOutputFile();
    Options.TexturePath = TexturePath.c_str();
    Options.FastCollision = CompileSettings.GetCollisionSource();
    Options.FastCollisionAsset = pCollisionAsset;
    Options.DoCollision = CompileSettings.DoCollision();
    Options.Verbose = bVerbose ? TRUE : FALSE;
    std::unique_ptr<bitmap::product_sink> DirectBitmapSink;
    if( pBitmapSink )
        Options.BitmapSink = pBitmapSink;
    else
    {
        DirectBitmapSink = bitmap::CreateFilesystemProductSink();
        Options.BitmapSink = DirectBitmapSink.get();
    }

    geometry_product_writer Compiler( &Diagnostics );
    Compiler.Export( Asset, pPhysicsAsset, Type, Options );

    if( CompileSettings.GetGeneratedRawGeom()[0] )
    {
        // Keep the descriptor on the committed product path.
        CompileSettings.SetOutputFile( FinalOutput.generic_string().c_str() );
        CompileSettings.SaveDescriptor( CompileSettings.GetGeneratedRawGeom() );
    }
    return TRUE;
}
