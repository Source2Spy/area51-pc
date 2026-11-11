//=========================================================================
//
//  CollisionVolume.hpp
//
//=========================================================================

#ifndef COLLISION_VOLUME_HPP
#define COLLISION_VOLUME_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"

//=========================================================================
//  COLLISION DATA CLASS
//=========================================================================

struct collision_data
{
    collision_data( void );

    struct mat_info
    {
        enum
        {
            FLAG_DOUBLESIDED = ( 1 << 0 ), // Material is double sided
            FLAG_TRANSPARENT = ( 1 << 1 ), // Material is transparent
        };

        u16 SoundType;
        u16 Flags;
    };

    struct high_cluster
    {
        bbox     BBox;
        s32      nTris;
        s32      iMesh;
        s32      iBone;
        s32      iSection;
        s32      iOffset;
        mat_info MaterialInfo;
    };

    struct low_quad
    {
        byte iP[4];
        byte iN;
        byte Flags;
    };

    struct low_cluster
    {
        bbox BBox;
        s32  iVectorOffset;
        s32  nPoints;
        s32  nNormals;
        s32  iQuadOffset;
        s32  nQuads;
        s32  iMesh;
        s32  iBone;
    };

    bbox          BBox; // Only valid for "zero pose".
    s32           nHighClusters;
    high_cluster* pHighCluster;
    s32           nHighIndices;
    u16*          pHighIndexToVert0;
    s32           nLowClusters;
    s32           nLowVectors;
    s32           nLowQuads;
    low_cluster*  pLowCluster;
    vector3*      pLowVector;
    low_quad*     pLowQuad;
};

//=========================================================================

#endif // COLLISION_VOLUME_HPP
//=========================================================================
