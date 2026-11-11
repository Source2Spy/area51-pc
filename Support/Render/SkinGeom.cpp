//=========================================================================
//
//  SkinGeom.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SkinGeom.hpp"
#include "GeomFile.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "FrameGraph/RenderFailure.hpp"

//=========================================================================
//  RESOURCE LOADER
//=========================================================================

static
struct skin_geom_loader : public rsc_loader
{
    skin_geom_loader( void ) : rsc_loader( "SKIN GEOM", ".skingeom" )
    {
    }

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFile )
    {
        MEMORY_OWNER( "SKIN GEOM DATA" );

        skin_geom* pGeom = NULL;
        xstring    error;
        if ( !geom_file::LoadSkin( pFile, pGeom, error ) )
        {
            RENDER_THROW( "SkinGeom", "load failed: %s", (char const*)error );
            x_throw( (char const*)error );
        }

        return ( pGeom );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        return ( pData );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        skin_geom* pSkinGeom = static_cast<skin_geom*>( pData );
        ASSERT( pSkinGeom );

        delete pSkinGeom;
    }

} s_SkinGeomLoader;

//=========================================================================
//  SKIN GEOM
//=========================================================================

skin_geom::skin_geom( void )
    : geom(), m_nSections( 0 ), m_pSection( NULL ), m_nIndices( 0 ), m_pIndex( NULL ), m_nVertexData( 0 ),
      m_pVertex( NULL ), m_nBonePalette( 0 ), m_pBonePalette( NULL )
{
}

//=========================================================================

skin_geom::~skin_geom( void )
{
    delete[] m_pSection;
    delete[] m_pIndex;
    delete[] m_pVertex;
    delete[] m_pBonePalette;
}
