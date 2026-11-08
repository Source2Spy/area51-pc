//=========================================================================
//
//  RawAnimWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RawAnimWriter.hpp"

#include "PropertyTree.hpp"

#include <cstdio>
#include <iomanip>
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

class property_builder final
{
public:
    void Add( const std::string& Name, const char* pType, const std::string& Value )
    {
        m_Document.Add( Name, pType, Value );
    }

    void AddString( const std::string& Name, const std::string& Value )
    {
        Add( Name, "STRING", Value );
    }

    void AddFile( const std::string& Name, const std::string& Value )
    {
        Add( Name, "FILENAME", Value );
    }

    void AddEnum( const std::string& Name, const std::string& Value )
    {
        Add( Name, "ENUM", Value );
    }

    void AddInt( const std::string& Name, s32 Value )
    {
        char Buffer[32];
        x_sprintf( Buffer, "%d", Value );
        Add( Name, "INT", Buffer );
    }

    void AddBool( const std::string& Name, xbool Value )
    {
        Add( Name, "BOOL", Value ? "1" : "0" );
    }

    void AddFloat( const std::string& Name, f32 Value )
    {
        std::ostringstream Stream;
        Stream << std::setprecision( 9 ) << Value;
        Add( Name, "FLOAT", Stream.str() );
    }

    void AddVector3( const std::string& Name, const vector3& Value )
    {
        std::ostringstream Stream;
        Stream << std::setprecision( 9 ) << Value.GetX() << ' ' << Value.GetY() << ' ' << Value.GetZ();
        Add( Name, "VECTOR3", Stream.str() );
    }

    void AddQuaternion( const std::string& Name, const quaternion& Value )
    {
        std::ostringstream Stream;
        Stream << std::setprecision( 9 ) << Value.X << ' ' << Value.Y << ' ' << Value.Z << ' ' << Value.W;
        // Store quaternions in the canonical STRING property slot.
        Add( Name, "STRING", Stream.str() );
    }

    void AddMatrix4( const std::string& Name, const matrix4& Value )
    {
        std::ostringstream Stream;
        Stream << std::setprecision( 9 );
        for( s32 Row = 0; Row < 4; ++Row )
        {
            for( s32 Column = 0; Column < 4; ++Column )
            {
                if( Row != 0 || Column != 0 )
                    Stream << ' ';
                Stream << Value( Row, Column );
            }
        }
        // MATRIX4 is likewise a rawanim domain value, not a new file syntax.
        Add( Name, "STRING", Stream.str() );
    }

    const resource_tool::property_tree::document& Get( void ) const
    {
        return m_Document;
    }

private:
    resource_tool::property_tree::document m_Document;
};

//=========================================================================

std::string Path( const char* pFormat, s32 A = -1, s32 B = -1, s32 C = -1 )
{
    char Buffer[512];
    if( B >= 0 && C >= 0 )
        x_sprintf( Buffer, pFormat, A, B, C );
    else if( B >= 0 )
        x_sprintf( Buffer, pFormat, A, B );
    else if( A >= 0 )
        x_sprintf( Buffer, pFormat, A );
    else
        x_sprintf( Buffer, "%s", pFormat );
    return Buffer;
}

//=========================================================================

void AddBone( property_builder& Properties, const resource_tool::animation::skeleton_bone& Bone, s32 Index )
{
    const std::string Base = Path( "ResDesc\\Skeleton\\Bone[%d]", Index );
    Properties.AddString( Base + "\\Name", Bone.Name );
    Properties.AddString( Base + "\\ParentName", Bone.ParentName );
    Properties.AddInt( Base + "\\Parent", Bone.Parent );
    Properties.AddInt( Base + "\\Children", Bone.Children );
    Properties.AddInt( Base + "\\LodGroup", Bone.LodGroup );
    Properties.AddBool( Base + "\\Masked", Bone.IsMasked );
    Properties.AddVector3( Base + "\\Scale", Bone.Scale );
    Properties.AddQuaternion( Base + "\\Rotation", Bone.Rotation );
    Properties.AddVector3( Base + "\\Translation", Bone.Translation );
    Properties.AddMatrix4( Base + "\\InverseBindMatrix", Bone.InverseBindMatrix );
    Properties.AddBool( Base + "\\HasScaleKeys", Bone.HasScaleKeys );
    Properties.AddBool( Base + "\\HasRotationKeys", Bone.HasRotationKeys );
    Properties.AddBool( Base + "\\HasTranslationKeys", Bone.HasTranslationKeys );
}

//=========================================================================

void AddPropertyKey( property_builder& Properties, const resource_tool::animation::prop_key& Key, s32 ClipIndex,
                     s32 PropertyIndex, s32 KeyIndex )
{
    const std::string Base =
        Path( "ResDesc\\Animations\\Animation[%d]\\Property[%d]\\Key[%d]", ClipIndex, PropertyIndex, KeyIndex );
    Properties.AddFloat( Base + "\\Time", Key.Time );
    Properties.AddBool( Base + "\\Visible", Key.Visible );
    Properties.AddVector3( Base + "\\Scale", Key.Scale );
    Properties.AddQuaternion( Base + "\\Rotation", Key.Rotation );
    Properties.AddVector3( Base + "\\Translation", Key.Translation );
}

//=========================================================================

void AddChannelKey( property_builder& Properties, const resource_tool::animation::bone_channel& Channel, s32 ClipIndex,
                    s32 ChannelIndex, s32 KeyIndex )
{
    const std::string Base =
        Path( "ResDesc\\Animations\\Animation[%d]\\Channel[%d]\\Key[%d]", ClipIndex, ChannelIndex, KeyIndex );
    Properties.AddFloat( Base + "\\Time", Channel.Times[KeyIndex] );
    if( KeyIndex < (s32)Channel.Translations.size() )
        Properties.AddVector3( Base + "\\Translation", Channel.Translations[KeyIndex] );
    if( KeyIndex < (s32)Channel.Rotations.size() )
        Properties.AddQuaternion( Base + "\\Rotation", Channel.Rotations[KeyIndex] );
    if( KeyIndex < (s32)Channel.Scales.size() )
        Properties.AddVector3( Base + "\\Scale", Channel.Scales[KeyIndex] );
}

//=========================================================================

void AddEvent( property_builder& Properties, const resource_tool::animation::event& Event, s32 ClipIndex,
               s32 EventIndex )
{
    const std::string Base = Path( "ResDesc\\Animations\\Animation[%d]\\Event[%d]", ClipIndex, EventIndex );
    Properties.AddString( Base + "\\Name", Event.Name );
    Properties.AddString( Base + "\\Type", Event.Type );
    Properties.AddString( Base + "\\BoneName", Event.BoneName );
    Properties.AddInt( Base + "\\StartFrame", Event.StartFrame );
    Properties.AddInt( Base + "\\EndFrame", Event.EndFrame );
    Properties.AddInt( Base + "\\Bone", Event.Bone );
    Properties.AddFloat( Base + "\\Radius", Event.Radius );
    Properties.AddInt( Base + "\\DataFlags", (s32)Event.DataFlags );
    for( s32 i = 0; i < (s32)Event.Ints.size(); ++i )
        Properties.AddInt( Base + Path( "\\Int[%d]", i ), Event.Ints[i] );
    for( s32 i = 0; i < (s32)Event.Floats.size(); ++i )
        Properties.AddFloat( Base + Path( "\\Float[%d]", i ), Event.Floats[i] );
    for( s32 i = 0; i < (s32)Event.Points.size(); ++i )
        Properties.AddVector3( Base + Path( "\\Point[%d]", i ), Event.Points[i] );
    for( s32 i = 0; i < (s32)Event.Bools.size(); ++i )
        Properties.AddBool( Base + Path( "\\Bool[%d]", i ), Event.Bools[i] );
    for( s32 i = 0; i < (s32)Event.Strings.size(); ++i )
        Properties.AddString( Base + Path( "\\String[%d]", i ), Event.Strings[i] );
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool raw_anim_writer::Write( const resource_tool::animation::asset& Asset, const char* pOutputFile,
                              std::string& Error ) const
{
    if( !pOutputFile || !pOutputFile[0] )
    {
        Error = "Unable to write rawanim without an output filename";
        return FALSE;
    }

    property_builder Properties;
    Properties.AddInt( "ResDesc\\Version", 1 );
    Properties.AddEnum( "ResDesc\\Type", "Animation" );
    Properties.AddString( "ResDesc\\Name", Asset.Name );
    if( !Asset.Source.SourceFile.empty() )
        Properties.AddFile( "ResDesc\\FileName", Asset.Source.SourceFile );

    Properties.AddString( "ResDesc\\Source\\UserName", Asset.Source.UserName );
    Properties.AddString( "ResDesc\\Source\\ComputerName", Asset.Source.ComputerName );
    for( s32 i = 0; i < (s32)Asset.Source.ExportDate.size(); ++i )
        Properties.AddInt( Path( "ResDesc\\Source\\ExportDate[%d]", i ), Asset.Source.ExportDate[i] );

    Properties.AddString( "ResDesc\\Skeleton\\Name", Asset.Skeleton.Name );
    Properties.AddInt( "ResDesc\\Skeleton\\NumBones", (s32)Asset.Skeleton.Bones.size() );
    for( s32 i = 0; i < (s32)Asset.Skeleton.Bones.size(); ++i )
        AddBone( Properties, Asset.Skeleton.Bones[i], i );

    Properties.AddInt( "ResDesc\\Animations\\NumAnimations", (s32)Asset.Clips.size() );
    for( s32 iClip = 0; iClip < (s32)Asset.Clips.size(); ++iClip )
    {
        const resource_tool::animation::clip& Clip = Asset.Clips[iClip];
        const std::string Base = Path( "ResDesc\\Animations\\Animation[%d]", iClip );
        Properties.AddString( Base + "\\Name", Clip.Name );
        Properties.AddInt( Base + "\\FPS", Clip.FPS );
        Properties.AddInt( Base + "\\Frames", Clip.Frames );
        Properties.AddBool( Base + "\\Looping", Clip.Looping );
        Properties.AddInt( Base + "\\LoopFrame", Clip.LoopFrame );
        Properties.AddInt( Base + "\\EndFrameOffset", Clip.EndFrameOffset );
        Properties.AddFloat( Base + "\\Weight", Clip.Weight );
        Properties.AddFloat( Base + "\\BlendTime", Clip.BlendTime );
        Properties.AddFloat( Base + "\\HandleAngle", Clip.HandleAngle );
        Properties.AddFloat( Base + "\\TotalYaw", Clip.TotalYaw );
        Properties.AddFloat( Base + "\\TotalMoveDirection", Clip.TotalMoveDirection );
        Properties.AddVector3( Base + "\\TotalTranslation", Clip.TotalTranslation );
        Properties.AddBool( Base + "\\Gravity", Clip.Gravity );
        Properties.AddBool( Base + "\\WorldCollision", Clip.WorldCollision );
        Properties.AddString( Base + "\\ChainAnimation", Clip.ChainAnimation );
        Properties.AddInt( Base + "\\ChainFrame", Clip.ChainFrame );
        Properties.AddInt( Base + "\\ChainFramesMin", Clip.ChainFramesMin );
        Properties.AddInt( Base + "\\ChainFramesMax", Clip.ChainFramesMax );
        Properties.AddBool( Base + "\\ChainCyclesInteger", Clip.ChainCyclesInteger );
        Properties.AddBool( Base + "\\BlendFrames", Clip.BlendFrames );
        Properties.AddBool( Base + "\\BlendLoop", Clip.BlendLoop );
        Properties.AddBool( Base + "\\Additive", Clip.Additive );
        Properties.AddBool( Base + "\\Subtractive", Clip.Subtractive );
        Properties.AddString( Base + "\\AdditiveBaseClip", Clip.AdditiveBaseClip );
        Properties.AddInt( Base + "\\AnimBoneMin", Clip.AnimBoneMin );
        Properties.AddInt( Base + "\\AnimBoneMax", Clip.AnimBoneMax );
        Properties.AddBool( Base + "\\RootMotion\\AccumulateHorizontal", Clip.RootMotion.AccumulateHorizontal );
        Properties.AddBool( Base + "\\RootMotion\\AccumulateVertical", Clip.RootMotion.AccumulateVertical );
        Properties.AddBool( Base + "\\RootMotion\\AccumulateYaw", Clip.RootMotion.AccumulateYaw );
        Properties.AddFloat( Base + "\\RootMotion\\Yaw", Clip.RootMotion.Yaw );
        Properties.AddVector3( Base + "\\RootMotion\\Translation", Clip.RootMotion.Translation );

        Properties.AddInt( Base + "\\MaskedBones\\NumBones", (s32)Clip.MaskedBones.size() );
        for( s32 j = 0; j < (s32)Clip.MaskedBones.size(); ++j )
            Properties.AddString( Base + Path( "\\MaskedBones\\Bone[%d]", j ), Clip.MaskedBones[j] );

        Properties.AddInt( Base + "\\Channels\\NumChannels", (s32)Clip.Channels.size() );
        for( s32 iChannel = 0; iChannel < (s32)Clip.Channels.size(); ++iChannel )
        {
            const resource_tool::animation::bone_channel& Channel = Clip.Channels[iChannel];
            const std::string ChannelBase = Path( "ResDesc\\Animations\\Animation[%d]\\Channel[%d]", iClip, iChannel );
            Properties.AddString( ChannelBase + "\\BoneName", Channel.BoneName );
            Properties.AddInt( ChannelBase + "\\NumKeys", (s32)Channel.Times.size() );
            for( s32 iKey = 0; iKey < (s32)Channel.Times.size(); ++iKey )
                AddChannelKey( Properties, Channel, iClip, iChannel, iKey );
        }

        Properties.AddInt( Base + "\\Properties\\NumProperties", (s32)Clip.Properties.size() );
        for( s32 iProperty = 0; iProperty < (s32)Clip.Properties.size(); ++iProperty )
        {
            const resource_tool::animation::property& Property = Clip.Properties[iProperty];
            const std::string PropertyBase =
                Path( "ResDesc\\Animations\\Animation[%d]\\Property[%d]", iClip, iProperty );
            Properties.AddString( PropertyBase + "\\Name", Property.Name );
            Properties.AddString( PropertyBase + "\\Type", Property.Type );
            Properties.AddString( PropertyBase + "\\BoneName", Property.BoneName );
            Properties.AddInt( PropertyBase + "\\Bone", Property.Bone );
            Properties.AddInt( PropertyBase + "\\NumKeys", (s32)Property.Keys.size() );
            for( s32 iKey = 0; iKey < (s32)Property.Keys.size(); ++iKey )
                AddPropertyKey( Properties, Property.Keys[iKey], iClip, iProperty, iKey );
        }

        Properties.AddInt( Base + "\\Events\\NumEvents", (s32)Clip.Events.size() );
        for( s32 iEvent = 0; iEvent < (s32)Clip.Events.size(); ++iEvent )
            AddEvent( Properties, Clip.Events[iEvent], iClip, iEvent );
    }

    try
    {
        resource_tool::property_tree::writer Writer;
        if( !Writer.Write( pOutputFile, Properties.Get(), Error ) )
            return FALSE;
    }
    catch( ... )
    {
        Error = "Unable to write rawanim: " + std::string( pOutputFile );
        return FALSE;
    }

    return TRUE;
}

} // namespace resource_tool::animation_product_format
