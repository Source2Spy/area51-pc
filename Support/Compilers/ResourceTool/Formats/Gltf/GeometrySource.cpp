//=========================================================================
//
//  GeometrySource.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometrySource.hpp"
#include "GltfReader.hpp"

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool IsGltfFile( const char* pFileName )
{
    if( !pFileName || !pFileName[0] )
        return FALSE;

    char Extension[X_MAX_EXT];
    x_splitpath( pFileName, NULL, NULL, NULL, Extension );
    return x_stricmp( Extension, ".gltf" ) == 0 || x_stricmp( Extension, ".glb" ) == 0;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_source::IsSourceFile( const char* pFileName )
{
    return IsGltfFile( pFileName ) ? TRUE : FALSE;
}

//=========================================================================

xbool geometry_source::LoadMesh( const char* pFileName, geometry_asset& Asset, asset_role Role, f32 UnitScale,
                                 std::string& Error )
{
    if( !IsGltfFile( pFileName ) )
    {
        Error = (const char*)xfs( "No geometry source reader handles [%s]", pFileName ? pFileName : "" );
        return FALSE;
    }
    return gltf_reader::LoadMesh( pFileName, Asset, Role, UnitScale, Error );
}
