//=========================================================================
//
//  GeometryLod.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_MODEL_LOD_HPP
#define RESOURCE_TOOL_GEOMETRY_MODEL_LOD_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <string>
#include <vector>

//=========================================================================
//  TYPES
//=========================================================================

struct geometry_lod_level
{
    s32 ScreenSize = 10000;
    std::vector<std::string> MeshNames;
};

//-------------------------------------------------------------------------

struct geometry_lod_group
{
    std::string Name;
    std::vector<geometry_lod_level> Levels;
};

//-------------------------------------------------------------------------

struct geometry_lod_model
{
    std::vector<geometry_lod_group> Groups;

    void Clear( void )
    {
        Groups.clear();
    }
};

#endif // RESOURCE_TOOL_GEOMETRY_MODEL_LOD_HPP
//=========================================================================
