//=========================================================================
//
//  GeometryCollision.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_COLLISION_HPP
#define RESOURCE_TOOL_GEOMETRY_COLLISION_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "x_math.hpp"

#include <string>
#include <vector>

//=========================================================================
//  TYPES
//=========================================================================

struct geometry_collision_cluster
{
    xbool High = FALSE;
    bbox BBox;
    std::string MeshName;
    std::string BoneName;
    std::string SectionName;
    s32 Mesh = -1;
    s32 Bone = -1;
    s32 Section = -1;
    s32 Offset = 0;
    s32 Count = 0;
    s32 VectorOffset = 0;
    s32 Points = 0;
    s32 Normals = 0;
    s32 QuadOffset = 0;
    s32 SoundType = 0;
    s32 Flags = 0;
    std::vector<s32> TriangleOffsets;
    std::vector<s32> QuadFlags;
};

//-------------------------------------------------------------------------

struct geometry_collision_mesh
{
    std::string Name;
    std::vector<vector3> Vertices;
    std::vector<vector3> Normals;
    std::vector<u32> Indices;
};

//-------------------------------------------------------------------------

struct geometry_collision_model
{
    std::vector<geometry_collision_cluster> Clusters;
    std::vector<geometry_collision_mesh> Meshes;
    bbox CollisionBBox;
    xbool HasCollisionBBox = FALSE;

    void Clear( void )
    {
        Clusters.clear();
        Meshes.clear();
        CollisionBBox.Clear();
        HasCollisionBBox = FALSE;
    }
};

#endif // RESOURCE_TOOL_GEOMETRY_COLLISION_HPP
//=========================================================================
