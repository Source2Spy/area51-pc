//=========================================================================
//
//  GeometryDecompileJob.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryDecompileJob.hpp"
#include "CollisionGeometryStage.hpp"
#include "RawGeomWriter.hpp"
#include "RenderGeometryStage.hpp"
#include "GeometryProductReader.hpp"
#include "GeometryProductReconstruction.hpp"
#include "GeometryTextureExport.hpp"
#include "../SkeletonSource.hpp"

#include <filesystem>

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void geometry_decompile_job::Decompile( const geometry_descriptor& Settings, const output_paths* pOutputs )
{
    Begin( Settings, pOutputs );
    BuildSemanticModel();
    ExportTextures();
    ExportRenderGeometry();
    ExportCollisionGeometry();
    WriteRawGeom();
}

//=========================================================================

void geometry_decompile_job::Begin( const geometry_descriptor& Settings, const output_paths* pOutputs )
{
    if( !Settings.GetSourceFileName()[0] )
        x_throw( "rawgeom descriptor does not contain a compiled geometry input" );
    if( !Settings.GetOutputFile()[0] )
        x_throw( "rawgeom descriptor does not contain an output glTF file" );

    m_Settings = Settings;
    m_Type = Settings.GetType();
    m_Asset.Clear();
    m_SkeletonSource.Clear();
    m_TextureMap = resource_tool::geometry::texture_export_map();
    m_bCollisionExported = FALSE;

    const std::filesystem::path GeometryOutput = pOutputs && !pOutputs->Geometry.empty()
                                                     ? pOutputs->Geometry
                                                     : std::filesystem::path( Settings.GetOutputFile() );
    m_Outputs.Geometry = GeometryOutput;
    m_Outputs.GeometryBinary = pOutputs && !pOutputs->GeometryBinary.empty() ? pOutputs->GeometryBinary
                                                                             : [&GeometryOutput]()
    {
        std::filesystem::path Path = GeometryOutput;
        Path.replace_extension( ".bin" );
        return Path;
    }();
    m_Outputs.GeometryBinaryUri =
        pOutputs && !pOutputs->GeometryBinaryUri.empty() ? pOutputs->GeometryBinaryUri : m_Outputs.GeometryBinary;
    m_Outputs.DescriptorGeometry =
        pOutputs && !pOutputs->DescriptorGeometry.empty() ? pOutputs->DescriptorGeometry : GeometryOutput;

    if( m_Type == geometry_descriptor::GEOMETRY_TYPE_RIGID )
    {
        m_Outputs.Collision = pOutputs && !pOutputs->Collision.empty() ? pOutputs->Collision : GeometryOutput;
        if( !( pOutputs && !pOutputs->Collision.empty() ) )
            m_Outputs.Collision.replace_extension( ".collision.gltf" );
        m_Outputs.CollisionBinary =
            pOutputs && !pOutputs->CollisionBinary.empty() ? pOutputs->CollisionBinary : m_Outputs.Collision;
        if( !( pOutputs && !pOutputs->CollisionBinary.empty() ) )
            m_Outputs.CollisionBinary.replace_extension( ".bin" );
        m_Outputs.CollisionBinaryUri = pOutputs && !pOutputs->CollisionBinaryUri.empty() ? pOutputs->CollisionBinaryUri
                                                                                         : m_Outputs.CollisionBinary;
    }

    if( pOutputs && !pOutputs->RawGeom.empty() )
        m_Outputs.RawGeom = pOutputs->RawGeom;
    else if( Settings.GetGeneratedRawGeom()[0] )
        m_Outputs.RawGeom = Settings.GetGeneratedRawGeom();
    else
    {
        m_Outputs.RawGeom = GeometryOutput;
        m_Outputs.RawGeom.replace_extension( ".rawgeom" );
    }

    m_Outputs.DescriptorCollision =
        pOutputs && !pOutputs->DescriptorCollision.empty() ? pOutputs->DescriptorCollision : m_Outputs.Collision;

    if( pOutputs )
    {
        m_Outputs.Transaction = pOutputs->Transaction;
        m_Outputs.Artifacts = pOutputs->Artifacts;
        m_Outputs.Diagnostics = pOutputs->Diagnostics;
    }
}

//=========================================================================

void geometry_decompile_job::BuildSemanticModel( void )
{
    std::string Error;
    resource_tool::geometry_product_format::geometry_product_snapshot Product;
    resource_tool::geometry_product_format::geometry_product_reader ProductReader;
    const resource_tool::geometry_product_format::geometry_product_kind Kind =
        m_Type == geometry_descriptor::GEOMETRY_TYPE_RIGID
            ? resource_tool::geometry_product_format::geometry_product_kind::Rigid
            : resource_tool::geometry_product_format::geometry_product_kind::Skin;
    if( !ProductReader.Load( m_Settings.GetSourceFileName(), Kind, Product, Error ) )
        x_throw( Error.c_str() );

    resource_tool::geometry_product_format::geometry_product_reconstruction Reconstruction;
    if( !Reconstruction.Execute( Product, m_Asset, Error ) )
        x_throw( Error.c_str() );

    if( m_Type == geometry_descriptor::GEOMETRY_TYPE_SKIN )
    {
        std::string SkeletonError;
        if( !resource_tool::geometry::LoadSkeletonSource( m_Settings.GetSkeletonSource(), m_SkeletonSource,
                                                          SkeletonError ) ||
            !resource_tool::geometry::ApplySkeletonSource( m_SkeletonSource, m_Asset, SkeletonError ) )
        {
            x_throw( SkeletonError.c_str() );
        }
    }
}

//=========================================================================

void geometry_decompile_job::ExportTextures( void )
{
    if( !m_Outputs.Transaction || !m_Outputs.Artifacts || !m_Outputs.Diagnostics )
    {
        x_throw( "Geometry texture export requires ResourceTool output context" );
    }

    std::string Error;
    resource_tool::geometry::geometry_texture_export_stage Stage;
    if( !Stage.Execute( m_Settings, m_Asset, m_Outputs.DescriptorGeometry, m_Outputs.RawGeom, *m_Outputs.Transaction,
                        *m_Outputs.Artifacts, *m_Outputs.Diagnostics, m_TextureMap, Error ) )
    {
        x_throw( Error.c_str() );
    }
}

//=========================================================================

void geometry_decompile_job::ExportRenderGeometry( void )
{
    std::string Error;
    resource_tool::geometry::render_geometry_stage Stage;
    if( !Stage.Execute( m_Type, m_Outputs.Geometry.generic_string().c_str(),
                        m_Outputs.GeometryBinary.generic_string().c_str(),
                        m_Outputs.GeometryBinaryUri.empty() ? m_Outputs.GeometryBinary.generic_string().c_str()
                                                            : m_Outputs.GeometryBinaryUri.generic_string().c_str(),
                        m_Asset, Error, &m_TextureMap,
                        m_Type == geometry_descriptor::GEOMETRY_TYPE_SKIN ? &m_SkeletonSource : NULL ) )
    {
        x_throw( xfs( "Unable to write glTF: %s", Error.c_str() ) );
    }
}

//=========================================================================

void geometry_decompile_job::ExportCollisionGeometry( void )
{
    if( m_Type != geometry_descriptor::GEOMETRY_TYPE_RIGID )
        return;

    std::string Error;
    resource_tool::geometry::collision_geometry_stage Stage;
    if( !Stage.Execute( m_Asset, m_Outputs.Collision.generic_string().c_str(),
                        m_Outputs.CollisionBinary.generic_string().c_str(),
                        m_Outputs.CollisionBinaryUri.empty() ? m_Outputs.CollisionBinary.generic_string().c_str()
                                                             : m_Outputs.CollisionBinaryUri.generic_string().c_str(),
                        Error ) )
    {
        if( Error == "Product geometry contains no collision geometry" )
        {
            m_bCollisionExported = FALSE;
            return;
        }
        x_throw( xfs( "Unable to write collision glTF: %s", Error.c_str() ) );
    }
    else
    {
        m_bCollisionExported = TRUE;
    }
}

//=========================================================================

void geometry_decompile_job::WriteRawGeom( void )
{
    std::string Error;
    resource_tool::geometry::raw_geom_writer Writer;
    if( !Writer.Execute( m_Settings, m_Type, m_Outputs.DescriptorGeometry.generic_string().c_str(),
                         m_Outputs.RawGeom.generic_string().c_str(),
                         m_Outputs.DescriptorCollision.generic_string().c_str(), m_bCollisionExported, m_Asset, Error,
                         &m_TextureMap ) )
    {
        x_throw( Error.c_str() );
    }
}

//=========================================================================

xbool geometry_decompile_job::HasCollisionOutput( void ) const
{
    return m_bCollisionExported;
}
