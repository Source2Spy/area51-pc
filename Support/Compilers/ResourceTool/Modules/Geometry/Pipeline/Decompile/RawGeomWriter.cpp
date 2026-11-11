//=========================================================================
//
//  RawGeomWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RawGeomWriter.hpp"

#include "RawGeomProjection.hpp"

#include <filesystem>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

std::filesystem::path DescriptorPath( const char* pFileName, const std::filesystem::path& DescriptorFile )
{
    if( !pFileName || !pFileName[0] )
        return std::filesystem::path();

    const std::filesystem::path BaseDirectory = std::filesystem::absolute( DescriptorFile ).parent_path();
    std::filesystem::path Path( pFileName );
    if( Path.is_relative() )
        Path = std::filesystem::absolute( Path );

    std::error_code ErrorCode;
    const std::filesystem::path Relative = std::filesystem::relative( Path, BaseDirectory, ErrorCode );
    if( !ErrorCode && !Relative.empty() )
        return Relative.lexically_normal();
    return Path.lexically_normal();
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::raw_geom_writer::Execute( const geometry_descriptor& InputSettings,
                                                         geometry_descriptor::geometry_type Type,
                                                         const char* pGeometryFile, const char* pRawGeomFile,
                                                         const char* pCollisionDescriptor, xbool bCollisionExported,
                                                         const geometry_asset& Model, std::string& Error,
                                                         const texture_export_map* pTextureMap ) const
{
    if( !pGeometryFile || !pGeometryFile[0] || !pRawGeomFile || !pRawGeomFile[0] )
    {
        Error = "Rawgeom writer requires geometry and descriptor output paths";
        return FALSE;
    }

    geometry_descriptor Generated;
    const std::filesystem::path DescriptorFile( pRawGeomFile );
    Generated.SetType( Type );
    Generated.SetCoordinateSystem( InputSettings.GetCoordinateSystem() );
    Generated.SetUnitScale( InputSettings.GetUnitScale() );
    Generated.SetSkeletonSource(
        DescriptorPath( InputSettings.GetSkeletonSource(), DescriptorFile ).generic_string().c_str() );
    Generated.SetPhysicsSource(
        DescriptorPath( InputSettings.GetPhysicsSource(), DescriptorFile ).generic_string().c_str() );
    Generated.SetDoCollision( InputSettings.DoCollision() );
    if( bCollisionExported )
    {
        Generated.SetCollisionSource( DescriptorPath( pCollisionDescriptor, DescriptorFile ).generic_string().c_str() );
    }
    Generated.SetSourceFileName( DescriptorPath( pGeometryFile, DescriptorFile ).generic_string().c_str() );

    std::filesystem::path RecompiledPath( pGeometryFile );
    RecompiledPath.replace_extension( Type == geometry_descriptor::GEOMETRY_TYPE_SKIN ? ".recompiled.skingeom"
                                                                                      : ".recompiled.rigidgeom" );
    Generated.SetOutputFile(
        DescriptorPath( RecompiledPath.generic_string().c_str(), DescriptorFile ).generic_string().c_str() );
    Generated.SetGeneratedRawGeom( "" );
    ProjectProductToDescriptor( Type, Model, Generated, pTextureMap );
    try
    {
        Generated.SaveDescriptor( pRawGeomFile );
    }
    catch( ... )
    {
        Error = xfs( "Unable to write generated rawgeom [%s]", pRawGeomFile );
        return FALSE;
    }
    return TRUE;
}
