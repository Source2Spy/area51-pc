//=========================================================================
//
//  RigidGeom.hpp
//
//=========================================================================

#ifndef RIGID_GEOM_HPP
#define RIGID_GEOM_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "geom.hpp"
#include "CollisionVolume.hpp"

//=========================================================================
//  RIGID GEOM CLASS
//=========================================================================

struct rigid_geom : public geom
{
    struct vertex
    {
        vector3p Pos;
        vector3p Normal;
        vector2  UV;
    };

    struct section
    {
        s32 FirstVertex;
        s32 nVertices;
        s32 FirstIndex;
        s32 nIndices;
        s32 iBone;
        s32 iColor;
    };

    rigid_geom  ( void );
    ~rigid_geom ( void );

    collision_data m_Collision;
    s32            m_nSections;
    section*       m_pSection;
    s32            m_nIndices;
    u32*           m_pIndex;
    s32            m_nVertexData;
    vertex*        m_pVertex;

    //-------------------------------------------------------------------------

    xbool GetGeoTri ( s32 TriKey, vector3& v0, vector3& v1, vector3& v2 ) const;
};

//=========================================================================

#endif // RIGID_GEOM_HPP
//=========================================================================
