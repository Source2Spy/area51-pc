//=========================================================================
//
//  AnimationValidation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationValidation.hpp"

#include "SkeletonContract.hpp"

#include <cmath>
#include <set>

namespace resource_tool::animation
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool IsFinite( f32 Value )
{
    return std::isfinite( Value ) != 0;
}

//=========================================================================

xbool IsFinite( const vector3& Value )
{
    return IsFinite( Value.GetX() ) && IsFinite( Value.GetY() ) && IsFinite( Value.GetZ() );
}

//=========================================================================

xbool IsFinite( const quaternion& Value )
{
    return IsFinite( Value.X ) && IsFinite( Value.Y ) && IsFinite( Value.Z ) && IsFinite( Value.W );
}

//=========================================================================

xbool IsUsableQuaternion( const quaternion& Value )
{
    const f32 LengthSquared = Value.X * Value.X + Value.Y * Value.Y + Value.Z * Value.Z + Value.W * Value.W;
    return IsFinite( LengthSquared ) && LengthSquared > 0.0000001f;
}

//=========================================================================

xbool Fail( std::string& Error, const std::string& Message )
{
    Error = Message;
    return FALSE;
}

//=========================================================================

xbool IsSameName( const std::string& A, const std::string& B )
{
    return x_stricmp( A.c_str(), B.c_str() ) == 0;
}

//=========================================================================

xbool ContainsBoneName( const std::vector<skeleton_bone>& Bones, usize Count, const std::string& Name )
{
    for( usize Index = 0; Index < Count; ++Index )
        if( IsSameName( Bones[Index].Name, Name ) )
            return TRUE;
    return FALSE;
}

//=========================================================================

xbool ContainsClipName( const std::vector<clip>& Clips, const std::string& Name )
{
    for( const clip& Clip : Clips )
        if( IsSameName( Clip.Name, Name ) )
            return TRUE;
    return FALSE;
}

//=========================================================================

xbool ValidateSkeleton( const skeleton& Skeleton, std::string& Error )
{
    if( Skeleton.Bones.empty() )
        return Fail( Error, "Animation skeleton contains no bones" );
    if( Skeleton.Bones.size() > resource_tool::skeleton_contract::MaxBones )
        return Fail( Error, "Animation skeleton exceeds the shared bone limit" );

    for( usize Index = 0; Index < Skeleton.Bones.size(); ++Index )
    {
        const skeleton_bone& Bone = Skeleton.Bones[Index];
        if( Bone.Name.empty() )
            return Fail( Error, "Animation skeleton contains an unnamed bone" );
        if( ContainsBoneName( Skeleton.Bones, Index, Bone.Name ) )
            return Fail( Error, "Animation skeleton contains duplicate bone: " + Bone.Name );
        if( !IsFinite( Bone.Scale ) || !IsFinite( Bone.Translation ) || !IsFinite( Bone.Rotation ) ||
            !IsUsableQuaternion( Bone.Rotation ) )
        {
            return Fail( Error, "Animation skeleton contains a non-finite transform: " + Bone.Name );
        }

        if( Bone.Parent < -1 || Bone.Parent >= static_cast<s32>( Skeleton.Bones.size() ) )
            return Fail( Error, "Animation bone parent index is invalid: " + Bone.Name );
        if( Bone.Parent < 0 )
        {
            if( !Bone.ParentName.empty() )
                return Fail( Error, "Animation root bone has a parent name: " + Bone.Name );
        }
        else if( Bone.ParentName != Skeleton.Bones[Bone.Parent].Name )
        {
            return Fail( Error, "Animation bone parent name does not match hierarchy: " + Bone.Name );
        }
    }

    for( usize Index = 0; Index < Skeleton.Bones.size(); ++Index )
    {
        std::set<usize> Visited;
        s32 Current = static_cast<s32>( Index );
        while( Current >= 0 )
        {
            if( !Visited.insert( static_cast<usize>( Current ) ).second )
                return Fail( Error, "Animation skeleton contains a parent cycle" );
            Current = Skeleton.Bones[Current].Parent;
        }
    }
    return TRUE;
}

//=========================================================================

xbool ValidateChannel( const bone_channel& Channel, const skeleton& Skeleton, std::string& Error )
{
    if( Channel.BoneName.empty() || Skeleton.FindBone( Channel.BoneName ) < 0 )
        return Fail( Error, "Animation channel targets an unknown bone: " + Channel.BoneName );

    const usize KeyCount = Channel.Times.size();
    if( ( !Channel.Translations.empty() && Channel.Translations.size() != KeyCount ) ||
        ( !Channel.Rotations.empty() && Channel.Rotations.size() != KeyCount ) ||
        ( !Channel.Scales.empty() && Channel.Scales.size() != KeyCount ) )
    {
        return Fail( Error, "Animation channel key arrays have different sizes: " + Channel.BoneName );
    }

    for( usize Index = 0; Index < KeyCount; ++Index )
    {
        if( !IsFinite( Channel.Times[Index] ) || Channel.Times[Index] < 0.0f )
            return Fail( Error, "Animation channel contains an invalid timestamp: " + Channel.BoneName );
        if( Index > 0 && Channel.Times[Index] < Channel.Times[Index - 1] )
            return Fail( Error, "Animation channel timestamps are not monotonic: " + Channel.BoneName );
        if( !Channel.Translations.empty() && !IsFinite( Channel.Translations[Index] ) )
            return Fail( Error, "Animation translation contains a non-finite value: " + Channel.BoneName );
        if( !Channel.Scales.empty() && !IsFinite( Channel.Scales[Index] ) )
            return Fail( Error, "Animation scale contains a non-finite value: " + Channel.BoneName );
        if( !Channel.Rotations.empty() &&
            ( !IsFinite( Channel.Rotations[Index] ) || !IsUsableQuaternion( Channel.Rotations[Index] ) ) )
        {
            return Fail( Error, "Animation rotation is invalid: " + Channel.BoneName );
        }
    }
    return TRUE;
}

//=========================================================================

xbool ValidateClip( const clip& Clip, const skeleton& Skeleton, const std::vector<clip>& Clips, std::string& Error )
{
    if( Clip.Name.empty() )
        return Fail( Error, "Animation clip has no name" );
    if( Clip.FPS <= 0 || Clip.Frames < 2 )
        return Fail( Error, "Animation clip has invalid timing: " + Clip.Name );
    if( Clip.LoopFrame < 0 || Clip.LoopFrame >= Clip.Frames )
        return Fail( Error, "Animation clip loop frame is outside the clip: " + Clip.Name );
    if( !IsFinite( Clip.Weight ) || !IsFinite( Clip.BlendTime ) || !IsFinite( Clip.HandleAngle ) ||
        !IsFinite( Clip.TotalYaw ) || !IsFinite( Clip.TotalMoveDirection ) || !IsFinite( Clip.TotalTranslation ) ||
        !IsFinite( Clip.RootMotion.Translation ) || !IsFinite( Clip.RootMotion.Yaw ) )
    {
        return Fail( Error, "Animation clip contains a non-finite value: " + Clip.Name );
    }
    if( Clip.Additive && Clip.Subtractive )
        return Fail( Error, "Animation clip cannot be both additive and subtractive: " + Clip.Name );
    if( ( Clip.Additive || Clip.Subtractive ) && Clip.AdditiveBaseClip.empty() )
        return Fail( Error, "Animation additive/subtractive clip has no base clip: " + Clip.Name );
    if( !Clip.AdditiveBaseClip.empty() && !ContainsClipName( Clips, Clip.AdditiveBaseClip ) )
    {
        return Fail( Error, "Animation base clip does not exist: " + Clip.AdditiveBaseClip );
    }

    for( const bone_channel& Channel : Clip.Channels )
        if( !ValidateChannel( Channel, Skeleton, Error ) )
            return FALSE;
    for( const std::string& BoneName : Clip.MaskedBones )
        if( Skeleton.FindBone( BoneName ) < 0 )
            return Fail( Error, "Animation mask references an unknown bone: " + BoneName );
    for( const property& Property : Clip.Properties )
    {
        if( Property.Name.empty() )
            return Fail( Error, "Animation property has no name in clip: " + Clip.Name );
        if( !Property.BoneName.empty() && Skeleton.FindBone( Property.BoneName ) < 0 )
            return Fail( Error, "Animation property references an unknown bone: " + Property.BoneName );
        for( const prop_key& Key : Property.Keys )
        {
            if( !IsFinite( Key.Time ) || Key.Time < 0.0f || !IsFinite( Key.Scale ) || !IsFinite( Key.Translation ) ||
                !IsFinite( Key.Rotation ) || !IsUsableQuaternion( Key.Rotation ) )
            {
                return Fail( Error, "Animation property contains an invalid key: " + Property.Name );
            }
        }
    }
    for( const event& Event : Clip.Events )
    {
        if( Event.Name.empty() || Event.StartFrame < 0 || Event.EndFrame < Event.StartFrame ||
            Event.EndFrame >= Clip.Frames )
        {
            return Fail( Error, "Animation event has invalid timing: " + Clip.Name );
        }
        if( !Event.BoneName.empty() && Skeleton.FindBone( Event.BoneName ) < 0 )
            return Fail( Error, "Animation event references an unknown bone: " + Event.BoneName );
        if( !IsFinite( Event.Radius ) )
            return Fail( Error, "Animation event radius is not finite: " + Event.Name );
    }
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool ValidateAsset( const asset& Asset, std::string& Error )
{
    if( !ValidateSkeleton( Asset.Skeleton, Error ) )
        return FALSE;

    for( const clip& Clip : Asset.Clips )
        if( !ValidateClip( Clip, Asset.Skeleton, Asset.Clips, Error ) )
            return FALSE;
    return TRUE;
}

} // namespace resource_tool::animation
