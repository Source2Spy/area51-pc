//=========================================================================
//
//  GeometryProductTypes.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Render/geom.hpp"
#include "Render/RigidGeom.hpp"
#include "Render/SkinGeom.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

collision_data::collision_data( void )
{
    BBox.Clear();

    nHighClusters = 0;
    pHighCluster = NULL;
    nHighIndices = 0;
    pHighIndexToVert0 = NULL;

    nLowClusters = 0;
    pLowCluster = NULL;
    nLowVectors = 0;
    pLowVector = NULL;
    nLowQuads = 0;
    pLowQuad = NULL;
}

//=========================================================================

geom::geom( void )
{
    x_memset( this, 0, sizeof( geom ) );
}

//=========================================================================

geom::~geom( void )
{
    delete[] m_pBone;
    delete[] m_pBoneMasks;
    delete[] m_pPropertySections;
    delete[] m_pProperties;
    delete[] m_pRigidBodies;
    delete[] m_pMesh;
    delete[] m_pSubMesh;
    delete[] m_pMaterial;
    delete[] m_pTexture;
    delete[] m_pUVKey;
    delete[] m_pLODSizes;
    delete[] m_pLODMasks;
    delete[] m_pVirtualMeshes;
    delete[] m_pVirtualTextures;
    delete[] m_pStringData;
}

//=========================================================================

xbool geom::PrepareMaterials( void )
{
    return TRUE;
}

//=========================================================================

rigid_geom::rigid_geom( void )
    : geom(), m_Collision(), m_nSections( 0 ), m_pSection( NULL ), m_nIndices( 0 ), m_pIndex( NULL ),
      m_nVertexData( 0 ), m_pVertex( NULL )
{
}

//=========================================================================

rigid_geom::~rigid_geom( void )
{
    delete[] m_Collision.pHighCluster;
    delete[] m_Collision.pHighIndexToVert0;
    delete[] m_Collision.pLowCluster;
    delete[] m_Collision.pLowVector;
    delete[] m_Collision.pLowQuad;
    delete[] m_pSection;
    delete[] m_pIndex;
    delete[] m_pVertex;
}

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
