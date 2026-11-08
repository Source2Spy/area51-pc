//=========================================================================
//
//  GeometrySkeleton.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_SKELETON_HPP
#define RESOURCE_TOOL_GEOMETRY_SKELETON_HPP

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

struct geometry_bone
{
    std::string Name;
    std::string ParentName;
    s32 nChildren = 0;
    s32 iParent = -1;
    vector3 Scale;
    quaternion Rotation;
    vector3 Position;
    matrix4 InverseBindMatrix;
    bbox BBox;
    s32 iRigidBody = -1;
    s32 LODGroup = -1;
    s32 HitLocation = 0;
    std::string RigidBodyName;
};

//-------------------------------------------------------------------------

struct geometry_skeleton_model
{
    std::vector<geometry_bone> Bones;

    void Clear( void )
    {
        Bones.clear();
    }
};

#endif // RESOURCE_TOOL_GEOMETRY_SKELETON_HPP
//=========================================================================
