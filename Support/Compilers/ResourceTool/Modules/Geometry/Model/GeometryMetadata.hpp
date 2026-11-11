//=========================================================================
//
//  GeometryMetadata.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_MODEL_METADATA_HPP
#define RESOURCE_TOOL_GEOMETRY_MODEL_METADATA_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <string>
#include <vector>

//=========================================================================
//  TYPES
//=========================================================================

struct geometry_bone_mask_weight
{
    std::string BoneName;
    s32 iBone = -1;
    f32 Value = 0.0f;
};

//-------------------------------------------------------------------------

struct geometry_bone_mask
{
    std::string Name;
    std::vector<geometry_bone_mask_weight> Weights;
};

//-------------------------------------------------------------------------

struct geometry_engine_property
{
    std::string Section;
    std::string Name;
    std::string Type;
    std::string Value;
};

//-------------------------------------------------------------------------

struct geometry_engine_metadata
{
    std::vector<geometry_bone_mask> BoneMasks;
    std::vector<geometry_engine_property> Properties;

    void Clear( void )
    {
        BoneMasks.clear();
        Properties.clear();
    }
};

#endif // RESOURCE_TOOL_GEOMETRY_MODEL_METADATA_HPP
//=========================================================================
