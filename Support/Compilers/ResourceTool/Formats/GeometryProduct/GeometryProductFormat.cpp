//=========================================================================
//
//  GeometryProductFormat.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductFormat.hpp"

#include "Render/GeomFile.hpp"
#include "Render/RigidGeom.hpp"
#include "Render/SkinGeom.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool LoadRigid( X_FILE* pFile, rigid_geom*& pGeometry, std::string& Error )
{
    xstring XError;
    const xbool bResult = geom_file::LoadRigid( pFile, pGeometry, XError );
    Error = (const char*)XError;
    return bResult;
}

//=========================================================================

xbool LoadSkin( X_FILE* pFile, skin_geom*& pGeometry, std::string& Error )
{
    xstring XError;
    const xbool bResult = geom_file::LoadSkin( pFile, pGeometry, XError );
    Error = (const char*)XError;
    return bResult;
}

//=========================================================================

xbool LoadRigid( const char* pFileName, rigid_geom*& pGeometry, std::string& Error )
{
    pGeometry = NULL;
    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        Error = xfs( "Unable to open compiled rigid geometry [%s]", pFileName ? pFileName : "" );
        return FALSE;
    }
    const xbool bResult = LoadRigid( pFile, pGeometry, Error );
    x_fclose( pFile );
    return bResult;
}

//=========================================================================

xbool LoadSkin( const char* pFileName, skin_geom*& pGeometry, std::string& Error )
{
    pGeometry = NULL;
    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        Error = xfs( "Unable to open compiled skin geometry [%s]", pFileName ? pFileName : "" );
        return FALSE;
    }
    const xbool bResult = LoadSkin( pFile, pGeometry, Error );
    x_fclose( pFile );
    return bResult;
}

//=========================================================================

xbool SaveRigid( const char* pFileName, const rigid_geom& Geometry, std::string& Error )
{
    xstring XError;
    const xbool bResult = geom_file::SaveRigid( pFileName, Geometry, XError );
    Error = (const char*)XError;
    return bResult;
}

//=========================================================================

xbool SaveSkin( const char* pFileName, const skin_geom& Geometry, std::string& Error )
{
    xstring XError;
    const xbool bResult = geom_file::SaveSkin( pFileName, Geometry, XError );
    Error = (const char*)XError;
    return bResult;
}

} // namespace resource_tool::geometry_product_format
