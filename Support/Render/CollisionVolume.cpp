//=========================================================================
//
//  CollisionVolume.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "CollisionVolume.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

collision_data::collision_data( void )
{
    BBox.Clear();

    nHighClusters       = 0;
    pHighCluster        = NULL;
    nHighIndices        = 0;
    pHighIndexToVert0   = NULL;

    nLowClusters    = 0;
    pLowCluster     = NULL;
    nLowVectors     = 0;
    pLowVector      = NULL;
    nLowQuads       = 0;
    pLowQuad        = NULL;
}
