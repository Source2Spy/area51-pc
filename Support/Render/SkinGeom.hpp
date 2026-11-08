//=========================================================================
//
//  SkinGeom.hpp
//
//=========================================================================

#ifndef SKIN_GEOM_HPP
#define SKIN_GEOM_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "geom.hpp"
#include "CollisionVolume.hpp"

//=========================================================================
//  SKIN GEOM CLASS
//=========================================================================

struct skin_geom : public geom
{
    struct vertex
    {
        vector3p Position;
        vector3p Normal;
        vector2  UV;
        vector2  Weights;
        u16      Bones[2];
    };

    struct section
    {
        s32 FirstVertex;
        s32 nVertices;
        s32 FirstIndex;
        s32 nIndices;
        s32 FirstBone;
        s32 nBones;
    };

    skin_geom  ( void );
    ~skin_geom ( void );

    s32      m_nSections;
    section* m_pSection;
    s32      m_nIndices;
    u32*     m_pIndex;
    s32      m_nVertexData;
    vertex*  m_pVertex;
    s32      m_nBonePalette;
    u16*     m_pBonePalette;
};

//=========================================================================

#endif // SKIN_GEOM_HPP
//=========================================================================
