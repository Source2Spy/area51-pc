//=========================================================================
//
//  GeometryProductFileWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductFileWriter.hpp"

#include "GeometryProductFormat.hpp"
#include "Render/RigidGeom.hpp"
#include "Render/SkinGeom.hpp"

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool VerifyRigid( const char* pOutputFile, std::string& Error )
{
    X_FILE* pFile = x_fopen( pOutputFile, "rb" );
    rigid_geom* pVerified = NULL;
    const xbool bValid = pFile && resource_tool::geometry_product_format::LoadRigid( pFile, pVerified, Error );
    if( pFile )
        x_fclose( pFile );
    delete pVerified;
    if( !bValid && Error[0] == '\0' )
        Error = xfs( "Unable to read back compiled rigid geometry [%s]", pOutputFile ? pOutputFile : "" );
    return bValid;
}

//=========================================================================

xbool VerifySkin( const char* pOutputFile, std::string& Error )
{
    X_FILE* pFile = x_fopen( pOutputFile, "rb" );
    skin_geom* pVerified = NULL;
    const xbool bValid = pFile && resource_tool::geometry_product_format::LoadSkin( pFile, pVerified, Error );
    if( pFile )
        x_fclose( pFile );
    delete pVerified;
    if( !bValid && Error[0] == '\0' )
        Error = xfs( "Unable to read back compiled skin geometry [%s]", pOutputFile ? pOutputFile : "" );
    return bValid;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::geometry_product_file_writer::WriteRigid( const char* pOutputFile,
                                                                         const rigid_geom& Geometry,
                                                                         std::string& Error ) const
{
    if( !resource_tool::geometry_product_format::SaveRigid( pOutputFile, Geometry, Error ) )
        return FALSE;
    return VerifyRigid( pOutputFile, Error );
}

//=========================================================================

xbool resource_tool::geometry::geometry_product_file_writer::WriteSkin( const char* pOutputFile,
                                                                        const skin_geom& Geometry,
                                                                        std::string& Error ) const
{
    if( !resource_tool::geometry_product_format::SaveSkin( pOutputFile, Geometry, Error ) )
        return FALSE;
    return VerifySkin( pOutputFile, Error );
}
