//=========================================================================
//
//  GeometryMesh.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_MESH_HPP
#define RESOURCE_TOOL_GEOMETRY_MESH_HPP

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

enum
{
    GEOMETRY_VERTEX_MAX_UV = 8,
};

//-------------------------------------------------------------------------

struct geometry_sub_mesh
{
    std::string Name;
    s32 nBones = 0;
};

//-------------------------------------------------------------------------

struct geometry_weight
{
    std::string BoneName;
    s32 iBone = -1;
    f32 Weight = 0.0f;
};

//-------------------------------------------------------------------------

struct geometry_vertex
{
    vector3 Position;
    s32 iFrame = -1;
    std::vector<vector2> UV;
    std::vector<vector3> Normal;
    std::vector<xcolor> Color;
    std::vector<geometry_weight> Weight;
};

//-------------------------------------------------------------------------

struct geometry_facet
{
    std::string MeshName;
    std::string MaterialName;
    s32 iMesh = -1;
    s32 iPrimitive = -1;
    std::string BoneName;
    std::string SubMeshName;
    std::string SectionName;
    s32 iSubMesh = -1;
    s32 iSection = -1;
    f32 WorldPixelSize = 0.001f;
    s32 nVertices = 3;
    s32 iVertex[3] = { -1, -1, -1 };
    s32 iMaterial = -1;
    plane Plane;
};

//-------------------------------------------------------------------------

struct geometry_render_model
{
    std::vector<geometry_vertex> Vertices;
    std::vector<geometry_facet> Facets;
    std::vector<geometry_sub_mesh> SubMeshes;

    void Clear( void )
    {
        Vertices.clear();
        Facets.clear();
        SubMeshes.clear();
    }
};

#endif // RESOURCE_TOOL_GEOMETRY_MESH_HPP
//=========================================================================
