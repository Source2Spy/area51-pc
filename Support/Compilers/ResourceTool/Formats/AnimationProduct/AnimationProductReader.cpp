//=========================================================================
//
//  AnimationProductReader.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationProductReader.hpp"

#include "Animation/AnimData.hpp"
#include "SkeletonContract.hpp"

#include <cstring>
#include <fstream>
#include <vector>

namespace resource_tool::animation_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool LoadSkeleton( const char* pFileName, resource_tool::animation::skeleton& Skeleton, std::string& Error )
{
    std::ifstream File( pFileName ? pFileName : "", std::ios::binary );
    if( !File )
    {
        Error = "Unable to open animation source: " + std::string( pFileName ? pFileName : "" );
        return FALSE;
    }

    u8 Header[176];
    File.read( reinterpret_cast<char*>( Header ), sizeof( Header ) );
    if( !File )
    {
        Error = "Animation source has a truncated header";
        return FALSE;
    }

    auto ReadU32 = [&Header]( usize Offset ) -> u32
    {
        u32 Value = 0;
        x_memcpy( &Value, Header + Offset, sizeof( Value ) );
        return Value;
    };

    const u32 Version = ReadU32( 96 );
    const u32 nBones = ReadU32( 108 );
    const u32 BoneOffset = ReadU32( 112 );
    const u32 DataSize = ReadU32( 152 );
    if( Version != 2014 )
    {
        Error = xfs( "Unsupported animation version %d", Version );
        return FALSE;
    }
    if( nBones == 0 || nBones > resource_tool::skeleton_contract::MaxBones )
    {
        Error = "Invalid animation skeleton bone count";
        return FALSE;
    }
    if( BoneOffset > DataSize || nBones > ( DataSize - BoneOffset ) / sizeof( anim_bone ) )
    {
        Error = "Animation skeleton table is outside the payload";
        return FALSE;
    }

    std::vector<u8> Data( DataSize );
    File.read( reinterpret_cast<char*>( Data.data() ), Data.size() );
    if( !File )
    {
        Error = "Animation source has a truncated payload";
        return FALSE;
    }

    std::vector<matrix4> WorldBind( nBones );
    Skeleton.Clear();
    Skeleton.Bones.resize( nBones );
    for( u32 i = 0; i < nBones; ++i )
    {
        anim_bone Bone;
        x_memcpy( &Bone, Data.data() + BoneOffset + i * sizeof( anim_bone ), sizeof( Bone ) );
        if( Bone.iParent < -1 || Bone.iParent >= (s16)nBones || Bone.iParent >= (s16)i )
        {
            Error = "Animation skeleton contains an invalid parent";
            Skeleton.Clear();
            return FALSE;
        }

        resource_tool::animation::skeleton_bone& Output = Skeleton.Bones[i];
        Output.Name = Bone.Name[0] ? Bone.Name : (const char*)xfs( "Bone_%d", i );
        Output.Parent = Bone.iParent;
        Output.InverseBindMatrix = Bone.BindMatrixInv;
        WorldBind[i] = Bone.BindMatrixInv;
        WorldBind[i].InvertSRT();

        matrix4 Local = WorldBind[i];
        if( Bone.iParent >= 0 )
        {
            matrix4 ParentInverse = WorldBind[Bone.iParent];
            ParentInverse.InvertSRT();
            Local = ParentInverse * WorldBind[i];
        }
        Local.DecomposeSRT( Output.Scale, Output.Rotation, Output.Translation );
    }

    for( u32 i = 0; i < nBones; ++i )
    {
        resource_tool::animation::skeleton_bone& Bone = Skeleton.Bones[i];
        if( Bone.Parent >= 0 )
        {
            Bone.ParentName = Skeleton.Bones[Bone.Parent].Name;
            ++Skeleton.Bones[Bone.Parent].Children;
        }
    }
    return TRUE;
}

//=========================================================================

xbool LoadClips( const char* pFileName, resource_tool::animation::asset& Asset, std::string& Error )
{
    anim_event::Init();
    anim_group Group;
    if( !Group.Load( pFileName ) )
    {
        Error = "Unable to load compiled animation group";
        return FALSE;
    }

    Asset.Clips.clear();
    Asset.Clips.reserve( Group.GetNAnims() );
    for( s32 iAnimation = 0; iAnimation < Group.GetNAnims(); ++iAnimation )
    {
        const anim_info& Source = Group.GetAnimInfo( iAnimation );
        resource_tool::animation::clip Clip;
        Clip.Name = (const char*)Source.GetName();
        Clip.FPS = Source.GetFPS();
        Clip.Frames = Source.GetNFrames();
        Clip.Looping = Source.DoesLoop();
        Clip.LoopFrame = Source.GetLoopFrame();
        Clip.EndFrameOffset = Source.GetEndFrameOffset();
        Clip.Weight = Source.GetWeight();
        Clip.BlendTime = Source.GetBlendTime();
        Clip.HandleAngle = Source.GetHandleAngle();
        Clip.TotalYaw = Source.GetTotalYaw();
        Clip.TotalMoveDirection = Source.GetTotalMoveDir();
        Clip.TotalTranslation = Source.GetTotalTranslation();
        Clip.RootMotion.Translation = Source.GetTotalTranslation();
        Clip.RootMotion.Yaw = Source.GetTotalYaw();
        Clip.RootMotion.AccumulateHorizontal = Source.AccumHorizMotion();
        Clip.RootMotion.AccumulateVertical = Source.AccumVertMotion();
        Clip.RootMotion.AccumulateYaw = Source.AccumYawMotion();
        Clip.Gravity = Source.Gravity();
        Clip.WorldCollision = Source.WorldCollision();
        Clip.ChainFrame = Source.GetChainFrame();
        Clip.ChainFramesMin = Source.GetChainFramesMin();
        Clip.ChainFramesMax = Source.GetChainFramesMax();
        Clip.ChainCyclesInteger = Source.ChainCyclesInteger();
        Clip.BlendFrames = Source.BlendFrames();
        Clip.BlendLoop = Source.BlendLoop();
        Clip.AnimBoneMin = Source.GetAnimBoneMinIndex();
        Clip.AnimBoneMax = Source.GetAnimBoneMaxIndex();

        const s32 ChainAnimation = Source.GetChainAnim();
        if( ChainAnimation >= 0 && ChainAnimation < Group.GetNAnims() )
            Clip.ChainAnimation = (const char*)Group.GetAnimInfo( ChainAnimation ).GetName();

        if( Clip.FPS <= 0 || Clip.Frames <= 0 )
        {
            Error = "Animation clip has invalid FPS or frame count: " + Clip.Name;
            return FALSE;
        }

        for( s32 iBone = 0; iBone < Group.GetNBones(); ++iBone )
        {
            if( Source.HasMasks() && Source.IsBoneMasked( iBone ) )
            {
                Clip.MaskedBones.push_back( (const char*)Group.GetBone( iBone ).Name );
                continue;
            }

            resource_tool::animation::bone_channel Channel;
            Channel.BoneName = (const char*)Group.GetBone( iBone ).Name;
            Channel.Times.resize( Clip.Frames );
            Channel.Translations.resize( Clip.Frames );
            Channel.Rotations.resize( Clip.Frames );
            for( s32 iFrame = 0; iFrame < Clip.Frames; ++iFrame )
            {
                anim_key Key;
                Source.GetRawKey( iFrame, iBone, Key );
                Channel.Times[iFrame] = (f32)iFrame / (f32)Clip.FPS;
                Channel.Translations[iFrame] = Key.Translation;
                Channel.Rotations[iFrame] = Key.Rotation;
            }
            Clip.Channels.push_back( std::move( Channel ) );
        }

        for( s32 iProperty = 0; iProperty < Source.GetNProps(); ++iProperty )
        {
            resource_tool::animation::property Property;
            Property.Name = Source.GetPropName( iProperty );
            Property.Type = Property.Name;
            Property.Bone = Source.GetPropParentBoneIndex( iProperty );
            if( Property.Bone >= 0 && Property.Bone < Group.GetNBones() )
                Property.BoneName = (const char*)Group.GetBone( Property.Bone ).Name;
            Property.Keys.resize( Clip.Frames );
            for( s32 iFrame = 0; iFrame < Clip.Frames; ++iFrame )
            {
                anim_key Key;
                Source.GetPropRawKey( iProperty, iFrame, Key );
                resource_tool::animation::prop_key& Output = Property.Keys[iFrame];
                Output.Time = (f32)iFrame / (f32)Clip.FPS;
                Output.Translation = Key.Translation;
                Output.Rotation = Key.Rotation;
#if USE_SCALE_KEYS
                Output.Scale = Key.Scale;
#endif
            }
            Clip.Properties.push_back( std::move( Property ) );
        }

        for( s32 iEvent = 0; iEvent < Source.GetNEvents(); ++iEvent )
        {
            const anim_event& SourceEvent = Source.GetEvent( iEvent );
            resource_tool::animation::event Event;
            Event.Name = SourceEvent.GetName();
            Event.Type = SourceEvent.GetType() ? SourceEvent.GetType() : "";
            Event.DataFlags = SourceEvent.GetDataFlags();
            const s32 nInts = x_min( SourceEvent.GetNInts(), 5 );
            const s32 nFloats = x_min( SourceEvent.GetNFloats(), 8 );
            const s32 nPoints = x_min( SourceEvent.GetNPoints(), 2 );
            const s32 nBools = x_min( SourceEvent.GetNBools(), 8 );
            const s32 nStrings = x_min( SourceEvent.GetNStrings(), 5 );
            for( s32 i = 0; i < nInts; ++i )
                Event.Ints[i] = SourceEvent.GetInt( i );
            for( s32 i = 0; i < nFloats; ++i )
                Event.Floats[i] = SourceEvent.GetFloat( i );
            for( s32 i = 0; i < nPoints; ++i )
                Event.Points[i] = SourceEvent.GetPoint( i );
            for( s32 i = 0; i < nBools; ++i )
                Event.Bools[i] = SourceEvent.GetBool( i ) != FALSE;
            for( s32 i = 0; i < nStrings; ++i )
                Event.Strings[i] = SourceEvent.GetString( i );
            Event.StartFrame =
                nInts > anim_event::INT_IDX_START_FRAME ? Event.Ints[anim_event::INT_IDX_START_FRAME] : 0;
            Event.EndFrame =
                nInts > anim_event::INT_IDX_END_FRAME ? Event.Ints[anim_event::INT_IDX_END_FRAME] : Event.StartFrame;
            Event.Bone = nInts > anim_event::INT_IDX_BONE ? Event.Ints[anim_event::INT_IDX_BONE] : -1;
            Event.Radius = nFloats > anim_event::FLOAT_IDX_RADIUS ? Event.Floats[anim_event::FLOAT_IDX_RADIUS] : 0.0f;
            if( Event.Bone >= 0 && Event.Bone < Group.GetNBones() )
                Event.BoneName = (const char*)Group.GetBone( Event.Bone ).Name;
            Clip.Events.push_back( std::move( Event ) );
        }
        Asset.Clips.push_back( std::move( Clip ) );
    }
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool animation_product_reader::Load( const char* pFileName, resource_tool::animation::asset& Asset,
                                      std::string& Error ) const
{
    Asset.Clear();
    Asset.Source.SourceFile = pFileName ? pFileName : "";
    if( !LoadSkeleton( pFileName, Asset.Skeleton, Error ) )
        return FALSE;
    return LoadClips( pFileName, Asset, Error );
}

} // namespace resource_tool::animation_product_format
