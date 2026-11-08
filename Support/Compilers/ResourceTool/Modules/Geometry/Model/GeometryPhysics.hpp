//=========================================================================
//
//  GeometryPhysics.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PHYSICS_HPP
#define RESOURCE_TOOL_GEOMETRY_PHYSICS_HPP

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

struct geometry_rigid_body
{
    struct dof
    {
        s32 bActive = 0;
        s32 bLimited = 0;
        f32 Min = 0.0f;
        f32 Max = 0.0f;
    };

    std::string Name;
    std::string Type;
    std::string ParentName;
    std::string BoneName;
    f32 Mass = 0.0f;
    s32 iParent = -1;
    vector3 PivotPosition;
    vector3 PivotScale;
    quaternion PivotRotation;
    vector3 BodyScale;
    quaternion BodyRotation;
    vector3 BodyPosition;
    f32 Radius = 0.0f;
    f32 Width = 0.0f;
    f32 Height = 0.0f;
    f32 Length = 0.0f;
    s32 iBone = -1;
    s32 Flags = 1;
    s32 CollisionMask = -1;
    dof DOF[6];
};

//-------------------------------------------------------------------------

struct geometry_physics_model
{
    std::vector<geometry_rigid_body> RigidBodies;

    void Clear( void )
    {
        RigidBodies.clear();
    }
};

#endif // RESOURCE_TOOL_GEOMETRY_PHYSICS_HPP
//=========================================================================
