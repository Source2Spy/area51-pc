//=========================================================================
//
//  GeomFile.hpp
//
//=========================================================================

#ifndef GEOM_FILE_HPP
#define GEOM_FILE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

struct rigid_geom;
struct skin_geom;

//=========================================================================
//  TYPES
//=========================================================================

class geom_file
{
public:

    enum
    {
        VERSION = 42,
    };

    static xbool Validate  ( rigid_geom const& Geom, xstring& Error );
    static xbool Validate  ( skin_geom const& Geom, xstring& Error );

    static xbool LoadRigid ( X_FILE* pFile, rigid_geom*& pGeom, xstring& Error );
    static xbool LoadSkin  ( X_FILE* pFile, skin_geom*& pGeom, xstring& Error );

    static xbool SaveRigid ( X_FILE* pFile, rigid_geom const& Geom, xstring& Error );
    static xbool SaveSkin  ( X_FILE* pFile, skin_geom const& Geom, xstring& Error );
    static xbool SaveRigid ( char const* pFileName, rigid_geom const& Geom, xstring& Error );
    static xbool SaveSkin  ( char const* pFileName, skin_geom const& Geom, xstring& Error );
    static xbool EncodeSkin( skin_geom const& Geom, xarray<u8>& Bytes, xstring& Error );
};

//=========================================================================

#endif // GEOM_FILE_HPP
//=========================================================================
