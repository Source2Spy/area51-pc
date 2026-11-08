//=========================================================================
//
//  AnimationAsset.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_ASSET_HPP
#define RESOURCE_TOOL_ANIMATION_ASSET_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "x_math.hpp"

#include <array>
#include <string>
#include <vector>

namespace resource_tool::animation
{

//=========================================================================
//  TYPES
//=========================================================================

struct skeleton_bone
{
    std::string Name;
    std::string ParentName;
    s32 Parent = -1;
    s32 Children = 0;
    vector3 Scale;
    quaternion Rotation;
    vector3 Translation;
    matrix4 InverseBindMatrix;
    xbool HasScaleKeys = FALSE;
    xbool HasRotationKeys = FALSE;
    xbool HasTranslationKeys = FALSE;
    xbool IsMasked = FALSE;
    s32 LodGroup = -1;

    skeleton_bone( void )
    {
        Scale.Set( 1.0f, 1.0f, 1.0f );
        Rotation.Identity();
        Translation.Zero();
        InverseBindMatrix.Identity();
    }
};

//-------------------------------------------------------------------------

struct skeleton
{
    std::string Name;
    std::vector<skeleton_bone> Bones;

    void Clear( void )
    {
        Name.clear();
        Bones.clear();
    }

    s32 FindBone( const std::string& BoneName ) const
    {
        for( s32 i = 0; i < (s32)Bones.size(); ++i )
        {
            if( x_stricmp( Bones[i].Name.c_str(), BoneName.c_str() ) == 0 )
                return i;
        }
        return -1;
    }
};

//-------------------------------------------------------------------------

struct bone_channel
{
    std::string BoneName;
    std::vector<f32> Times;
    std::vector<vector3> Translations;
    std::vector<quaternion> Rotations;
    std::vector<vector3> Scales;
};

//-------------------------------------------------------------------------

struct prop_key
{
    f32 Time = 0.0f;
    vector3 Scale;
    quaternion Rotation;
    vector3 Translation;
    xbool Visible = TRUE;

    prop_key( void )
    {
        Scale.Set( 1.0f, 1.0f, 1.0f );
        Rotation.Identity();
        Translation.Zero();
    }
};

//-------------------------------------------------------------------------

struct property
{
    std::string Name;
    std::string Type;
    std::string BoneName;
    s32 Bone = -1;
    std::vector<prop_key> Keys;
};

//-------------------------------------------------------------------------

struct event
{
    std::string Name;
    std::string Type;
    std::string BoneName;
    s32 StartFrame = 0;
    s32 EndFrame = 0;
    s32 Bone = -1;
    f32 Radius = 0.0f;
    u32 DataFlags = 0;
    std::array<s32, 5> Ints = {};
    std::array<f32, 8> Floats = {};
    std::array<vector3, 2> Points;
    std::array<xbool, 8> Bools = {};
    std::array<std::string, 5> Strings;

    event( void )
    {
        for( vector3& Point : Points )
            Point.Zero();
    }
};

//-------------------------------------------------------------------------

struct root_motion
{
    vector3 Translation;
    f32 Yaw = 0.0f;
    xbool AccumulateHorizontal = FALSE;
    xbool AccumulateVertical = FALSE;
    xbool AccumulateYaw = FALSE;

    root_motion( void )
    {
        Translation.Zero();
    }
};

//-------------------------------------------------------------------------

struct clip
{
    std::string Name;
    s32 FPS = 30;
    s32 Frames = 0;
    xbool Looping = FALSE;
    s32 LoopFrame = 0;
    s32 EndFrameOffset = 0;
    f32 Weight = 1.0f;
    f32 BlendTime = -1.0f;
    f32 HandleAngle = 0.0f;
    f32 TotalYaw = 0.0f;
    f32 TotalMoveDirection = 0.0f;
    vector3 TotalTranslation;
    xbool Gravity = FALSE;
    xbool WorldCollision = FALSE;
    std::string ChainAnimation;
    s32 ChainFrame = -1;
    s32 ChainFramesMin = 0;
    s32 ChainFramesMax = 0;
    xbool ChainCyclesInteger = FALSE;
    xbool BlendFrames = FALSE;
    xbool BlendLoop = FALSE;
    xbool Additive = FALSE;
    xbool Subtractive = FALSE;
    std::string AdditiveBaseClip;
    s32 AnimBoneMin = -1;
    s32 AnimBoneMax = -1;
    root_motion RootMotion;
    std::vector<std::string> MaskedBones;
    std::vector<bone_channel> Channels;
    std::vector<property> Properties;
    std::vector<event> Events;

    clip( void )
    {
        TotalTranslation.Zero();
    }
};

//-------------------------------------------------------------------------

struct source_metadata
{
    std::string SourceFile;
    std::string UserName;
    std::string ComputerName;
    std::array<s32, 3> ExportDate = { 0, 0, 0 };
};

//-------------------------------------------------------------------------

struct asset
{
    std::string Name;
    source_metadata Source;
    skeleton Skeleton;
    std::vector<clip> Clips;

    void Clear( void )
    {
        Name.clear();
        Source = source_metadata();
        Skeleton.Clear();
        Clips.clear();
    }
};

} // namespace resource_tool::animation

#endif // RESOURCE_TOOL_ANIMATION_ASSET_HPP
//=========================================================================
