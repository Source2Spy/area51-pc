//=========================================================================
//
//  RawAnimReader.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RawAnimReader.hpp"

#include "AnimationAsset.hpp"
#include "PropertyTree.hpp"

#include <cerrno>
#include <cstdlib>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace resource_tool::animation_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

struct path_component
{
    std::string Name;
    s32 Index = -1;
};

xbool SplitPath( const std::string& Value, std::vector<path_component>& Result )
{
    Result.clear();
    usize Begin = 0;
    while( Begin <= Value.size() )
    {
        const usize End = Value.find( '\\', Begin );
        const std::string Part = Value.substr( Begin, End == std::string::npos ? std::string::npos : End - Begin );
        if( Part.empty() )
            return FALSE;

        path_component Component;
        const usize Open = Part.find( '[' );
        if( Open == std::string::npos )
            Component.Name = Part;
        else
        {
            if( Part.back() != ']' || Open == 0 || Open + 2 > Part.size() )
                return FALSE;
            Component.Name = Part.substr( 0, Open );
            const std::string IndexText = Part.substr( Open + 1, Part.size() - Open - 2 );
            char* pEnd = NULL;
            const long Index = std::strtol( IndexText.c_str(), &pEnd, 10 );
            if( pEnd == IndexText.c_str() || *pEnd != '\0' || Index < 0 )
                return FALSE;
            Component.Index = (s32)Index;
        }
        Result.push_back( Component );

        if( End == std::string::npos )
            break;
        Begin = End + 1;
    }
    return TRUE;
}

//=========================================================================

xbool IsPath( const std::vector<path_component>& Path, std::initializer_list<const char*> Names )
{
    if( Path.size() != Names.size() )
        return FALSE;
    usize i = 0;
    for( const char* pName : Names )
    {
        if( Path[i].Name != pName || Path[i].Index >= 0 )
            return FALSE;
        ++i;
    }
    return TRUE;
}

//=========================================================================

xbool HasPrefix( const std::vector<path_component>& Path, std::initializer_list<const char*> Names )
{
    if( Path.size() < Names.size() )
        return FALSE;
    usize i = 0;
    for( const char* pName : Names )
    {
        if( Path[i].Name != pName )
            return FALSE;
        ++i;
    }
    return TRUE;
}

//=========================================================================

xbool Integer( const std::string& Value, s32& Result )
{
    char* pEnd = NULL;
    errno = 0;
    const long Parsed = std::strtol( Value.c_str(), &pEnd, 10 );
    if( pEnd == Value.c_str() || *pEnd != '\0' || errno == ERANGE )
        return FALSE;
    Result = (s32)Parsed;
    return TRUE;
}

//=========================================================================

xbool Real( const std::string& Value, f32& Result )
{
    char* pEnd = NULL;
    errno = 0;
    const f32 Parsed = std::strtof( Value.c_str(), &pEnd );
    if( pEnd == Value.c_str() || *pEnd != '\0' || errno == ERANGE )
        return FALSE;
    Result = Parsed;
    return TRUE;
}

//=========================================================================

xbool Boolean( const std::string& Value, xbool& Result )
{
    s32 Parsed = 0;
    if( !Integer( Value, Parsed ) )
        return FALSE;
    Result = Parsed != 0;
    return TRUE;
}

//=========================================================================

xbool StringValue( const std::string& Type, const std::string& Value, std::string& Result )
{
    if( Type != "STRING" && Type != "FILENAME" && Type != "ENUM" )
        return FALSE;
    Result = Value;
    return TRUE;
}

//=========================================================================

xbool Vector3Value( const std::string& Type, const std::string& Value, vector3& Result )
{
    if( Type != "VECTOR3" )
        return FALSE;
    std::istringstream Stream( Value );
    f32 X = 0.0f, Y = 0.0f, Z = 0.0f;
    if( !( Stream >> X >> Y >> Z ) )
        return FALSE;
    Result.Set( X, Y, Z );
    return TRUE;
}

//=========================================================================

xbool QuaternionValue( const std::string& Type, const std::string& Value, quaternion& Result )
{
    if( Type != "STRING" )
        return FALSE;
    std::istringstream Stream( Value );
    f32 X = 0.0f, Y = 0.0f, Z = 0.0f, W = 1.0f;
    if( !( Stream >> X >> Y >> Z >> W ) )
        return FALSE;
    Result = quaternion( X, Y, Z, W );
    return TRUE;
}

//=========================================================================

xbool Matrix4Value( const std::string& Type, const std::string& Value, matrix4& Result )
{
    if( Type != "STRING" )
        return FALSE;
    std::istringstream Stream( Value );
    for( s32 Row = 0; Row < 4; ++Row )
    {
        for( s32 Column = 0; Column < 4; ++Column )
        {
            f32 Element = 0.0f;
            if( !( Stream >> Element ) )
                return FALSE;
            Result( Row, Column ) = Element;
        }
    }
    return TRUE;
}

//=========================================================================

template <typename T> T& Ensure( std::vector<T>& Values, s32 Index )
{
    if( Index < 0 )
        throw "negative rawanim property index";
    if( (s32)Values.size() <= Index )
        Values.resize( Index + 1 );
    return Values[Index];
}

//=========================================================================

xbool Invalid( std::string& Error, s32 Record, const char* pMessage )
{
    Error = xfs( "rawanim property record %d: %s", Record, pMessage ? pMessage : "" );
    return FALSE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool raw_anim_reader::Load( const char* pFileName, resource_tool::animation::asset& Asset, std::string& Error ) const
{
    Asset.Clear();
    if( !pFileName || !pFileName[0] )
    {
        Error = "Unable to open rawanim without a filename";
        return FALSE;
    }

    resource_tool::property_tree::document Document;
    resource_tool::property_tree::reader Reader;
    if( !Reader.Load( pFileName, Document, Error ) )
    {
        Error = "Unable to read rawanim: " + Error;
        return FALSE;
    }

    const std::vector<resource_tool::property_tree::property_record>& Records = Document.Records();
    for( s32 Record = 0; Record < (s32)Records.size(); ++Record )
    {
        const resource_tool::property_tree::property_record& Property = Records[Record];
        const std::string& Name = Property.Name;
        const std::string& Type = Property.Type;
        const std::string& Value = Property.Value;

        std::vector<path_component> Path;
        if( !SplitPath( Name, Path ) )
        {
            return Invalid( Error, Record, "invalid property path" );
        }

        const std::string TypeString = Type;
        const std::string ValueString = Value;
        xbool Valid = TRUE;
        xbool Known = TRUE;

        if( IsPath( Path, { "ResDesc", "Version" } ) )
        {
            s32 Version = 0;
            Valid = TypeString == "INT" && Integer( ValueString, Version ) && Version == 1;
        }
        else if( IsPath( Path, { "ResDesc", "Type" } ) )
        {
            std::string ResourceType;
            Valid = StringValue( TypeString, ValueString, ResourceType ) && ResourceType == "Animation";
        }
        else if( IsPath( Path, { "ResDesc", "Name" } ) )
            Valid = StringValue( TypeString, ValueString, Asset.Name );
        else if( IsPath( Path, { "ResDesc", "FileName" } ) )
            Valid = StringValue( TypeString, ValueString, Asset.Source.SourceFile );
        else if( IsPath( Path, { "ResDesc", "Source", "UserName" } ) )
            Valid = StringValue( TypeString, ValueString, Asset.Source.UserName );
        else if( IsPath( Path, { "ResDesc", "Source", "ComputerName" } ) )
            Valid = StringValue( TypeString, ValueString, Asset.Source.ComputerName );
        else if( Path.size() == 3 && Path[0].Name == "ResDesc" && Path[1].Name == "Source" &&
                 Path[2].Name == "ExportDate" && Path[2].Index >= 0 )
        {
            Valid = TypeString == "INT" && Path[2].Index < (s32)Asset.Source.ExportDate.size() &&
                    Integer( ValueString, Asset.Source.ExportDate[Path[2].Index] );
        }
        else if( IsPath( Path, { "ResDesc", "Skeleton", "Name" } ) )
            Valid = StringValue( TypeString, ValueString, Asset.Skeleton.Name );
        else if( IsPath( Path, { "ResDesc", "Skeleton", "NumBones" } ) )
        {
            s32 Count = 0;
            Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
            if( Valid )
                Asset.Skeleton.Bones.resize( Count );
        }
        else if( Path.size() == 4 && Path[0].Name == "ResDesc" && Path[1].Name == "Skeleton" &&
                 Path[2].Name == "Bone" && Path[2].Index >= 0 && Path[3].Index < 0 )
        {
            resource_tool::animation::skeleton_bone& Bone = Ensure( Asset.Skeleton.Bones, Path[2].Index );
            const std::string& Field = Path[3].Name;
            if( Field == "Name" )
                Valid = StringValue( TypeString, ValueString, Bone.Name );
            else if( Field == "ParentName" )
                Valid = StringValue( TypeString, ValueString, Bone.ParentName );
            else if( Field == "Parent" )
                Valid = TypeString == "INT" && Integer( ValueString, Bone.Parent );
            else if( Field == "Children" )
                Valid = TypeString == "INT" && Integer( ValueString, Bone.Children );
            else if( Field == "LodGroup" )
                Valid = TypeString == "INT" && Integer( ValueString, Bone.LodGroup );
            else if( Field == "Masked" )
                Valid = TypeString == "BOOL" && Boolean( ValueString, Bone.IsMasked );
            else if( Field == "Scale" )
                Valid = Vector3Value( TypeString, ValueString, Bone.Scale );
            else if( Field == "Rotation" )
                Valid = QuaternionValue( TypeString, ValueString, Bone.Rotation );
            else if( Field == "Translation" )
                Valid = Vector3Value( TypeString, ValueString, Bone.Translation );
            else if( Field == "InverseBindMatrix" )
                Valid = Matrix4Value( TypeString, ValueString, Bone.InverseBindMatrix );
            else if( Field == "HasScaleKeys" )
                Valid = TypeString == "BOOL" && Boolean( ValueString, Bone.HasScaleKeys );
            else if( Field == "HasRotationKeys" )
                Valid = TypeString == "BOOL" && Boolean( ValueString, Bone.HasRotationKeys );
            else if( Field == "HasTranslationKeys" )
                Valid = TypeString == "BOOL" && Boolean( ValueString, Bone.HasTranslationKeys );
            else
                Known = FALSE;
        }
        else if( IsPath( Path, { "ResDesc", "Animations", "NumAnimations" } ) )
        {
            s32 Count = 0;
            Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
            if( Valid )
                Asset.Clips.resize( Count );
        }
        else if( Path.size() >= 4 && Path[0].Name == "ResDesc" && Path[1].Name == "Animations" &&
                 Path[2].Name == "Animation" && Path[2].Index >= 0 )
        {
            const s32 ClipIndex = Path[2].Index;
            resource_tool::animation::clip& Clip = Ensure( Asset.Clips, ClipIndex );

            if( Path.size() == 4 && Path[3].Index < 0 )
            {
                const std::string& Field = Path[3].Name;
                if( Field == "Name" )
                    Valid = StringValue( TypeString, ValueString, Clip.Name );
                else if( Field == "FPS" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.FPS );
                else if( Field == "Frames" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.Frames );
                else if( Field == "Looping" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.Looping );
                else if( Field == "LoopFrame" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.LoopFrame );
                else if( Field == "EndFrameOffset" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.EndFrameOffset );
                else if( Field == "Weight" )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Clip.Weight );
                else if( Field == "BlendTime" )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Clip.BlendTime );
                else if( Field == "HandleAngle" )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Clip.HandleAngle );
                else if( Field == "TotalYaw" )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Clip.TotalYaw );
                else if( Field == "TotalMoveDirection" )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Clip.TotalMoveDirection );
                else if( Field == "TotalTranslation" )
                    Valid = Vector3Value( TypeString, ValueString, Clip.TotalTranslation );
                else if( Field == "Gravity" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.Gravity );
                else if( Field == "WorldCollision" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.WorldCollision );
                else if( Field == "ChainAnimation" )
                    Valid = StringValue( TypeString, ValueString, Clip.ChainAnimation );
                else if( Field == "ChainFrame" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.ChainFrame );
                else if( Field == "ChainFramesMin" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.ChainFramesMin );
                else if( Field == "ChainFramesMax" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.ChainFramesMax );
                else if( Field == "ChainCyclesInteger" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.ChainCyclesInteger );
                else if( Field == "BlendFrames" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.BlendFrames );
                else if( Field == "BlendLoop" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.BlendLoop );
                else if( Field == "Additive" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.Additive );
                else if( Field == "Subtractive" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.Subtractive );
                else if( Field == "AdditiveBaseClip" )
                    Valid = StringValue( TypeString, ValueString, Clip.AdditiveBaseClip );
                else if( Field == "AnimBoneMin" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.AnimBoneMin );
                else if( Field == "AnimBoneMax" )
                    Valid = TypeString == "INT" && Integer( ValueString, Clip.AnimBoneMax );
                else
                    Known = FALSE;
            }
            else if( Path.size() == 5 && Path[3].Name == "RootMotion" )
            {
                const std::string& Field = Path[4].Name;
                if( Field == "AccumulateHorizontal" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.RootMotion.AccumulateHorizontal );
                else if( Field == "AccumulateVertical" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.RootMotion.AccumulateVertical );
                else if( Field == "AccumulateYaw" )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Clip.RootMotion.AccumulateYaw );
                else if( Field == "Yaw" )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Clip.RootMotion.Yaw );
                else if( Field == "Translation" )
                    Valid = Vector3Value( TypeString, ValueString, Clip.RootMotion.Translation );
                else
                    Known = FALSE;
            }
            else if( Path.size() == 5 && Path[3].Name == "MaskedBones" && Path[4].Name == "NumBones" )
            {
                s32 Count = 0;
                Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
                if( Valid )
                    Clip.MaskedBones.resize( Count );
            }
            else if( Path.size() == 5 && Path[3].Name == "MaskedBones" && Path[4].Name == "Bone" && Path[4].Index >= 0 )
            {
                Valid = StringValue( TypeString, ValueString, Ensure( Clip.MaskedBones, Path[4].Index ) );
            }
            else if( Path.size() == 5 && Path[3].Name == "Channels" && Path[4].Name == "NumChannels" )
            {
                s32 Count = 0;
                Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
                if( Valid )
                    Clip.Channels.resize( Count );
            }
            else if( Path.size() == 5 && Path[3].Name == "Properties" && Path[4].Name == "NumProperties" )
            {
                s32 Count = 0;
                Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
                if( Valid )
                    Clip.Properties.resize( Count );
            }
            else if( Path.size() == 5 && Path[3].Name == "Events" && Path[4].Name == "NumEvents" )
            {
                s32 Count = 0;
                Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
                if( Valid )
                    Clip.Events.resize( Count );
            }
            else if( Path.size() >= 5 && Path[3].Name == "Channel" && Path[3].Index >= 0 )
            {
                resource_tool::animation::bone_channel& Channel = Ensure( Clip.Channels, Path[3].Index );
                if( Path.size() == 5 && Path[4].Name == "BoneName" )
                    Valid = StringValue( TypeString, ValueString, Channel.BoneName );
                else if( Path.size() == 5 && Path[4].Name == "NumKeys" )
                {
                    s32 Count = 0;
                    Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
                    if( Valid )
                        Channel.Times.resize( Count );
                }
                else if( Path.size() == 6 && Path[4].Name == "Key" && Path[4].Index >= 0 )
                {
                    const s32 KeyIndex = Path[4].Index;
                    if( Path[5].Name == "Time" )
                        Valid = TypeString == "FLOAT" && Real( ValueString, Ensure( Channel.Times, KeyIndex ) );
                    else if( Path[5].Name == "Translation" )
                        Valid = Vector3Value( TypeString, ValueString, Ensure( Channel.Translations, KeyIndex ) );
                    else if( Path[5].Name == "Rotation" )
                        Valid = QuaternionValue( TypeString, ValueString, Ensure( Channel.Rotations, KeyIndex ) );
                    else if( Path[5].Name == "Scale" )
                        Valid = Vector3Value( TypeString, ValueString, Ensure( Channel.Scales, KeyIndex ) );
                    else
                        Known = FALSE;
                }
                else
                    Known = FALSE;
            }
            else if( Path.size() >= 5 && Path[3].Name == "Property" && Path[3].Index >= 0 )
            {
                resource_tool::animation::property& Property = Ensure( Clip.Properties, Path[3].Index );
                if( Path.size() == 5 && Path[4].Name == "Name" )
                    Valid = StringValue( TypeString, ValueString, Property.Name );
                else if( Path.size() == 5 && Path[4].Name == "Type" )
                    Valid = StringValue( TypeString, ValueString, Property.Type );
                else if( Path.size() == 5 && Path[4].Name == "BoneName" )
                    Valid = StringValue( TypeString, ValueString, Property.BoneName );
                else if( Path.size() == 5 && Path[4].Name == "Bone" )
                    Valid = TypeString == "INT" && Integer( ValueString, Property.Bone );
                else if( Path.size() == 5 && Path[4].Name == "NumKeys" )
                {
                    s32 Count = 0;
                    Valid = TypeString == "INT" && Integer( ValueString, Count ) && Count >= 0;
                    if( Valid )
                        Property.Keys.resize( Count );
                }
                else if( Path.size() == 6 && Path[4].Name == "Key" && Path[4].Index >= 0 )
                {
                    resource_tool::animation::prop_key& Key = Ensure( Property.Keys, Path[4].Index );
                    if( Path[5].Name == "Time" )
                        Valid = TypeString == "FLOAT" && Real( ValueString, Key.Time );
                    else if( Path[5].Name == "Visible" )
                        Valid = TypeString == "BOOL" && Boolean( ValueString, Key.Visible );
                    else if( Path[5].Name == "Scale" )
                        Valid = Vector3Value( TypeString, ValueString, Key.Scale );
                    else if( Path[5].Name == "Rotation" )
                        Valid = QuaternionValue( TypeString, ValueString, Key.Rotation );
                    else if( Path[5].Name == "Translation" )
                        Valid = Vector3Value( TypeString, ValueString, Key.Translation );
                    else
                        Known = FALSE;
                }
                else
                    Known = FALSE;
            }
            else if( Path.size() >= 5 && Path[3].Name == "Event" && Path[3].Index >= 0 )
            {
                resource_tool::animation::event& Event = Ensure( Clip.Events, Path[3].Index );
                if( Path.size() == 5 && Path[4].Name == "Name" )
                    Valid = StringValue( TypeString, ValueString, Event.Name );
                else if( Path.size() == 5 && Path[4].Name == "Type" )
                    Valid = StringValue( TypeString, ValueString, Event.Type );
                else if( Path.size() == 5 && Path[4].Name == "BoneName" )
                    Valid = StringValue( TypeString, ValueString, Event.BoneName );
                else if( Path.size() == 5 && Path[4].Name == "StartFrame" )
                    Valid = TypeString == "INT" && Integer( ValueString, Event.StartFrame );
                else if( Path.size() == 5 && Path[4].Name == "EndFrame" )
                    Valid = TypeString == "INT" && Integer( ValueString, Event.EndFrame );
                else if( Path.size() == 5 && Path[4].Name == "Bone" )
                    Valid = TypeString == "INT" && Integer( ValueString, Event.Bone );
                else if( Path.size() == 5 && Path[4].Name == "Radius" )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Event.Radius );
                else if( Path.size() == 5 && Path[4].Name == "DataFlags" )
                {
                    s32 Flags = 0;
                    Valid = TypeString == "INT" && Integer( ValueString, Flags );
                    Event.DataFlags = (u32)Flags;
                }
                else if( Path.size() == 5 && Path[4].Name == "Int" && Path[4].Index >= 0 &&
                         Path[4].Index < (s32)Event.Ints.size() )
                    Valid = TypeString == "INT" && Integer( ValueString, Event.Ints[Path[4].Index] );
                else if( Path.size() == 5 && Path[4].Name == "Float" && Path[4].Index >= 0 &&
                         Path[4].Index < (s32)Event.Floats.size() )
                    Valid = TypeString == "FLOAT" && Real( ValueString, Event.Floats[Path[4].Index] );
                else if( Path.size() == 5 && Path[4].Name == "Point" && Path[4].Index >= 0 &&
                         Path[4].Index < (s32)Event.Points.size() )
                    Valid = Vector3Value( TypeString, ValueString, Event.Points[Path[4].Index] );
                else if( Path.size() == 5 && Path[4].Name == "Bool" && Path[4].Index >= 0 &&
                         Path[4].Index < (s32)Event.Bools.size() )
                    Valid = TypeString == "BOOL" && Boolean( ValueString, Event.Bools[Path[4].Index] );
                else if( Path.size() == 5 && Path[4].Name == "String" && Path[4].Index >= 0 &&
                         Path[4].Index < (s32)Event.Strings.size() )
                    Valid = StringValue( TypeString, ValueString, Event.Strings[Path[4].Index] );
                else
                    Known = FALSE;
            }
            else
                Known = FALSE;
        }
        else
            Known = FALSE;

        if( Known && !Valid )
        {
            return Invalid( Error, Record, "invalid property value or path" );
        }
    }

    for( resource_tool::animation::skeleton_bone& Bone : Asset.Skeleton.Bones )
    {
        if( Bone.Parent < 0 && !Bone.ParentName.empty() )
            Bone.Parent = Asset.Skeleton.FindBone( Bone.ParentName );
        Bone.Children = 0;
    }
    for( const resource_tool::animation::skeleton_bone& Bone : Asset.Skeleton.Bones )
    {
        if( Bone.Parent >= 0 && Bone.Parent < (s32)Asset.Skeleton.Bones.size() )
            ++Asset.Skeleton.Bones[Bone.Parent].Children;
    }

    if( Asset.Skeleton.Bones.empty() || Asset.Clips.empty() )
    {
        Error = "rawanim has no skeleton or animations";
        return FALSE;
    }
    return TRUE;
}

} // namespace resource_tool::animation_product_format
