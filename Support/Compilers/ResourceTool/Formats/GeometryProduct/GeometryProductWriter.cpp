//=========================================================================
//
//  GeometryProductWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"
#include "Diagnostics.hpp"
#include "GeometryProductFormat.hpp"
#include "Render/RigidGeom.hpp"
#include "Render/SkinGeom.hpp"
#include "GeometryProductFileWriter.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

geometry_product_writer::geometry_product_writer( resource_tool::diagnostic_log* pDiagnostics )
    : m_pContext( new encoding_context() )
{
    Context().m_FastCollision[0] = 0;
    Context().m_TexturePath[0] = 0;
    Context().m_OutputFile[0] = 0;
    Context().m_bHasPhysicsAsset = FALSE;
    Context().m_bHasCollisionAsset = FALSE;
    Context().m_bVerbose = FALSE;
    Context().m_bDoCollision = TRUE;
    Context().m_pBitmapSink = NULL;
    m_pDiagnostics = NULL;
    x_strcpy( Context().m_UserName, "Unknown user" );
    x_strcpy( Context().m_ComputerName, "Unknown computer" );
    Context().m_ExportDate[0] = 0;
    Context().m_ExportDate[1] = 0;
    Context().m_ExportDate[2] = 0;

    SetDiagnostics( pDiagnostics );
}

geometry_product_writer::~geometry_product_writer( void ) = default;

//=========================================================================

geometry_product_writer::encoding_context& geometry_product_writer::Context( void )
{
    return *m_pContext;
}

//=========================================================================

const geometry_product_writer::encoding_context& geometry_product_writer::Context( void ) const
{
    return *m_pContext;
}

//=========================================================================

void geometry_product_writer::SetUserInfo( const char* pUserName, const char* pComputerName )
{
    if( pUserName )
        x_strsavecpy( Context().m_UserName, pUserName, sizeof( Context().m_UserName ) );
    else
        x_strcpy( Context().m_UserName, "Unknown user" );

    if( pComputerName )
        x_strsavecpy( Context().m_ComputerName, pComputerName, sizeof( Context().m_ComputerName ) );
    else
        x_strcpy( Context().m_ComputerName, "Unknown computer" );
}

//=========================================================================

void geometry_product_writer::SetExportDate( s32 Month, s32 Day, s32 Year )
{
    Context().m_ExportDate[0] = Month;
    Context().m_ExportDate[1] = Day;
    Context().m_ExportDate[2] = Year;
}

//=========================================================================

void geometry_product_writer::ReportWarning( const char* pWarning )
{
    if( m_pDiagnostics )
    {
        m_pDiagnostics->Warning( pWarning, "Geometry", NULL, Context().m_ActiveAsset.Source.SourceFile.c_str() );
        return;
    }
    x_printf( "WARNING: %s\n", pWarning );
    x_printf( "  Last exported by: %s, on: %d/%d/%d\n", Context().m_UserName, Context().m_ExportDate[0],
              Context().m_ExportDate[1], Context().m_ExportDate[2] );
}

//=========================================================================

void geometry_product_writer::ReportError( const char* pError )
{
    if( m_pDiagnostics )
    {
        m_pDiagnostics->Error( pError, "Geometry", NULL, Context().m_ActiveAsset.Source.SourceFile.c_str() );
        return;
    }
    x_printf( "ERROR: %s\n", pError );
    x_printf( "  Last exported by: %s, on: %d/%d/%d\n", Context().m_UserName, Context().m_ExportDate[0],
              Context().m_ExportDate[1], Context().m_ExportDate[2] );
}

//=========================================================================

void geometry_product_writer::ThrowError( const char* pError )
{
    ReportError( pError );
    x_throw( pError && pError[0] ? pError : "ResourceTool geometry compiler error occurred" );
}

//=========================================================================

void geometry_product_writer::SetDiagnostics( resource_tool::diagnostic_log* pDiagnostics )
{
    m_pDiagnostics = pDiagnostics;
}

//=========================================================================

void geometry_product_writer::Export( const geometry_asset& Asset, const geometry_asset* pPhysicsAsset,
                                      product_kind Kind, const geometry_product_compile_options& Options )
{
    // Reset all product-building state for this request.
    m_pContext.reset( new encoding_context() );
    resource_tool::geometry_product_format::geometry_product_encoding_pipeline::Execute( *this, Asset, pPhysicsAsset,
                                                                                         Kind, Options );
}

//=========================================================================

void geometry_product_writer::BeginExport( const geometry_asset& Asset, const geometry_asset* pPhysicsAsset,
                                           product_kind Kind, const geometry_product_compile_options& Options )
{
    if( !Options.OutputFile || !Options.OutputFile[0] )
        x_throw( "No geometry output file was specified" );
    Context().m_Dictionary.Reset();
    Context().m_ActiveAsset = Asset;
    Context().m_CompiledMesh.Material.Clear();
    Context().m_CompiledMesh.SubMesh.Clear();
    Context().m_pRigidGeom.reset();
    Context().m_pSkinGeom.reset();
    Context().m_MeshToCompiled.Clear();
    Context().m_PhysicsAsset.Clear();
    Context().m_bHasPhysicsAsset = pPhysicsAsset != NULL;
    if( pPhysicsAsset )
        Context().m_PhysicsAsset = *pPhysicsAsset;
    Context().m_CollisionAsset.Clear();
    Context().m_bHasCollisionAsset = Options.FastCollisionAsset != NULL;
    if( Options.FastCollisionAsset )
        Context().m_CollisionAsset = *Options.FastCollisionAsset;
    x_strsavecpy( Context().m_OutputFile, Options.OutputFile, sizeof( Context().m_OutputFile ) );
    x_strsavecpy( Context().m_TexturePath, Options.TexturePath ? Options.TexturePath : "",
                  sizeof( Context().m_TexturePath ) );
    Context().m_bVerbose = Options.Verbose;
    Context().m_bDoCollision = Options.DoCollision;
    Context().m_pBitmapSink = Options.BitmapSink;
    x_strsavecpy( Context().m_FastCollision, Options.FastCollision ? Options.FastCollision : "",
                  sizeof( Context().m_FastCollision ) );
    SetUserInfo(
        Context().m_ActiveAsset.Source.UserName.empty() ? NULL : Context().m_ActiveAsset.Source.UserName.c_str(),
        Context().m_ActiveAsset.Source.ComputerName.empty() ? NULL
                                                            : Context().m_ActiveAsset.Source.ComputerName.c_str() );
    SetExportDate( Context().m_ActiveAsset.Source.ExportDate[0], Context().m_ActiveAsset.Source.ExportDate[1],
                   Context().m_ActiveAsset.Source.ExportDate[2] );
}

//=========================================================================

void geometry_product_writer::BuildProductModel( product_kind Kind )
{
    if( Kind == product_kind::Rigid )
    {
        Context().m_pRigidGeom.reset( new rigid_geom );
        BuildCompileModel( *Context().m_pRigidGeom, Context().m_ActiveAsset, Context().m_CompiledMesh, TRUE );
        if( !Context().m_bDoCollision )
        {
            Context().m_pRigidGeom->m_Collision.BBox = Context().m_ActiveAsset.GetBBox();
        }
        else if( Context().m_bHasCollisionAsset )
        {
            CompileLowCollision( *Context().m_pRigidGeom, Context().m_CollisionAsset, Context().m_ActiveAsset );
        }
        else
        {
            CompileLowCollisionFromBBox( *Context().m_pRigidGeom, Context().m_ActiveAsset );
        }
        return;
    }

    if( Kind == product_kind::Skin )
    {
        Context().m_pSkinGeom.reset( new skin_geom );
        BuildCompileModel( *Context().m_pSkinGeom, Context().m_ActiveAsset, Context().m_CompiledMesh, FALSE );
        return;
    }

    x_throw( "Unknown compiler type" );
}

//=========================================================================

void geometry_product_writer::WriteProduct( product_kind Kind )
{
    std::string Error;
    resource_tool::geometry::geometry_product_file_writer ProductWriter;
    if( Kind == product_kind::Rigid )
    {
        if( !Context().m_pRigidGeom )
            x_throw( "Rigid compiled model was not built" );
        BuildRigidGeometry( Context().m_CompiledMesh, *Context().m_pRigidGeom );
        if( !ProductWriter.WriteRigid( Context().m_OutputFile, *Context().m_pRigidGeom, Error ) )
            ThrowError( xfs( "Unable to write v42 rigid geometry: %s", Error.c_str() ) );
        if( Context().m_bVerbose )
            PrintSummary( *Context().m_pRigidGeom );
        return;
    }

    if( Kind == product_kind::Skin )
    {
        if( !Context().m_pSkinGeom )
            x_throw( "Skin compiled model was not built" );
        BuildSkinGeometry( Context().m_CompiledMesh, *Context().m_pSkinGeom );
        if( !ProductWriter.WriteSkin( Context().m_OutputFile, *Context().m_pSkinGeom, Error ) )
            ThrowError( xfs( "Unable to write v42 skin geometry: %s", Error.c_str() ) );
        if( Context().m_bVerbose )
            PrintSummary( *Context().m_pSkinGeom );
        return;
    }

    x_throw( "Unknown compiler type" );
}

//=========================================================================
