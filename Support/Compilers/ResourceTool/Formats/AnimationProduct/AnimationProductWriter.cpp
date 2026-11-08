//=========================================================================
//
//  AnimationProductWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationProductWriter.hpp"

#include "Animation/AnimData.hpp"
#include "AnimationAsset.hpp"
#include "SkeletonContract.hpp"
#include "x_bitstream.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

extern void CompressAnimationData( bitstream& BitStream, const anim_key* pKeys, const anim_bone* pBones, s32 nBones,
                                   const u32* pStreamFlags, s32 TotalFrames, s32 TotalStreams, s32 StartFrame,
                                   s32 EndFrame, f32* pBoneLengths, s32& DecompressedDataSize, s32& AnimBoneMin,
                                   s32& AnimBoneMax );

namespace resource_tool::animation_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

constexpr s32 kMaxNameLength = 31;
constexpr s32 kMaxFrames = 32767;

struct sampled_key
{
    vector3 Translation;
    quaternion Rotation;
    vector3 Scale;

    sampled_key( void )
    {
        Translation.Zero();
        Rotation.Identity();
        Scale.Set( 1.0f, 1.0f, 1.0f );
    }
};

//=========================================================================

xbool Fail( std::string& Error, const std::string& Message )
{
    Error = Message;
    return FALSE;
}

//=========================================================================

xbool ValidName( const std::string& Name, const char* pWhat, std::string& Error )
{
    if( Name.empty() )
        return Fail( Error, std::string( pWhat ) + " has an empty name" );
    if( Name.size() > (usize)kMaxNameLength )
        return Fail( Error, std::string( pWhat ) + " exceeds the compiled name limit: " + Name );
    return TRUE;
}

//=========================================================================

f32 Clamp01( f32 Value )
{
    return Value < 0.0f ? 0.0f : Value > 1.0f ? 1.0f : Value;
}

//=========================================================================

sampled_key SampleChannel( const resource_tool::animation::bone_channel* pChannel, f32 Time,
                           const sampled_key& Fallback )
{
    if( !pChannel || pChannel->Times.empty() )
        return Fallback;

    const usize Count = pChannel->Times.size();
    usize Right = 0;
    while( Right < Count && pChannel->Times[Right] < Time )
        ++Right;
    if( Right == 0 )
        Right = 1;
    if( Right >= Count )
        Right = Count - 1;

    const usize Left = Right == 0 ? 0 : Right - 1;
    const f32 T0 = pChannel->Times[Left];
    const f32 T1 = pChannel->Times[Right];
    const f32 Alpha = T1 > T0 ? Clamp01( ( Time - T0 ) / ( T1 - T0 ) ) : 0.0f;

    sampled_key Result = Fallback;
    if( pChannel->Translations.size() == Count )
        Result.Translation =
            pChannel->Translations[Left] + Alpha * ( pChannel->Translations[Right] - pChannel->Translations[Left] );
    if( pChannel->Rotations.size() == Count )
        Result.Rotation = Blend( pChannel->Rotations[Left], pChannel->Rotations[Right], Alpha );
    if( pChannel->Scales.size() == Count )
        Result.Scale = pChannel->Scales[Left] + Alpha * ( pChannel->Scales[Right] - pChannel->Scales[Left] );
    return Result;
}

//=========================================================================

const resource_tool::animation::bone_channel* FindChannel( const resource_tool::animation::clip& Clip,
                                                           const std::string& BoneName )
{
    for( const resource_tool::animation::bone_channel& Channel : Clip.Channels )
    {
        if( x_stricmp( Channel.BoneName.c_str(), BoneName.c_str() ) == 0 )
            return &Channel;
    }
    return NULL;
}

//=========================================================================

const resource_tool::animation::property* FindProperty( const resource_tool::animation::clip& Clip, usize Index )
{
    return Index < Clip.Properties.size() ? &Clip.Properties[Index] : NULL;
}

//=========================================================================

sampled_key SampleProperty( const resource_tool::animation::property& Property, f32 Time )
{
    sampled_key Result;
    if( Property.Keys.empty() )
        return Result;

    usize Right = 0;
    while( Right < Property.Keys.size() && Property.Keys[Right].Time < Time )
        ++Right;
    if( Right == 0 )
        Right = 1;
    if( Right >= Property.Keys.size() )
        Right = Property.Keys.size() - 1;
    const usize Left = Right == 0 ? 0 : Right - 1;
    const f32 T0 = Property.Keys[Left].Time;
    const f32 T1 = Property.Keys[Right].Time;
    const f32 Alpha = T1 > T0 ? Clamp01( ( Time - T0 ) / ( T1 - T0 ) ) : 0.0f;
    const resource_tool::animation::prop_key& K0 = Property.Keys[Left];
    const resource_tool::animation::prop_key& K1 = Property.Keys[Right];
    Result.Translation = K0.Translation + Alpha * ( K1.Translation - K0.Translation );
    Result.Rotation = Blend( K0.Rotation, K1.Rotation, Alpha );
    Result.Scale = K0.Scale + Alpha * ( K1.Scale - K0.Scale );
    return Result;
}

//=========================================================================

xbool IsMasked( const resource_tool::animation::clip& Clip, const std::string& BoneName )
{
    for( const std::string& MaskedBone : Clip.MaskedBones )
    {
        if( x_stricmp( MaskedBone.c_str(), BoneName.c_str() ) == 0 )
            return TRUE;
    }
    return FALSE;
}

//=========================================================================

u16 EncodeAngle( f32 Angle )
{
    const f32 FullTurn = R_360;
    f32 Normalized = std::fmod( Angle, FullTurn );
    if( Normalized < 0.0f )
        Normalized += FullTurn;
    return (u16)( Normalized * ( 65535.0f / FullTurn ) + 0.5f );
}

//=========================================================================

u16 ClipFlags( const resource_tool::animation::clip& Clip )
{
    u16 Flags = 0;
    if( Clip.Looping )
        Flags |= ANIM_DATA_FLAG_LOOPING;
    if( !Clip.MaskedBones.empty() )
        Flags |= ANIM_DATA_FLAG_HAS_MASKS;
    if( Clip.RootMotion.AccumulateHorizontal )
        Flags |= ANIM_DATA_FLAG_ACCUM_HORIZ_MOTION;
    if( Clip.RootMotion.AccumulateVertical )
        Flags |= ANIM_DATA_FLAG_ACCUM_VERT_MOTION;
    if( Clip.RootMotion.AccumulateYaw )
        Flags |= ANIM_DATA_FLAG_ACCUM_YAW_MOTION;
    if( Clip.Gravity )
        Flags |= ANIM_DATA_FLAG_GRAVITY;
    if( Clip.WorldCollision )
        Flags |= ANIM_DATA_FLAG_WORLD_COLLISION;
    if( Clip.ChainCyclesInteger )
        Flags |= ANIM_DATA_FLAG_CHAIN_CYCLES_INTEGER;
    if( Clip.BlendFrames )
        Flags |= ANIM_DATA_FLAG_BLEND_FRAMES;
    if( Clip.BlendLoop )
        Flags |= ANIM_DATA_FLAG_BLEND_LOOP;
    return Flags;
}

//=========================================================================

void CopyEvent( const resource_tool::animation::event& Source, s32 BoneIndex, anim_event& Output )
{
    resource_tool::animation::event Effective = Source;
    Effective.Ints[anim_event::INT_IDX_START_FRAME] = Source.StartFrame;
    Effective.Ints[anim_event::INT_IDX_END_FRAME] = Source.EndFrame;
    if( BoneIndex >= 0 )
        Effective.Ints[anim_event::INT_IDX_BONE] = BoneIndex;
    Effective.Floats[anim_event::FLOAT_IDX_RADIUS] = Source.Radius;
    Effective.DataFlags |= ( 1u << anim_event::INT_IDX_START_FRAME ) | ( 1u << anim_event::INT_IDX_END_FRAME );
    if( BoneIndex >= 0 )
        Effective.DataFlags |= ( 1u << anim_event::INT_IDX_BONE );
    if( Source.Radius != 0.0f )
        Effective.DataFlags |= ( 1u << ( EVENT_MAX_INTS + anim_event::FLOAT_IDX_RADIUS ) );

    event_data Data;
    Data.SetType( Effective.Type.c_str() );
    Data.SetName( Effective.Name.c_str() );

    const u32 Flags = Effective.DataFlags;
    s32 Count = 0;
    for( s32 i = 0; i < EVENT_MAX_INTS; ++i )
        if( Flags & ( 1u << i ) )
            Data.StoreInt( Count++, Effective.Ints[i] );
    Count = 0;
    for( s32 i = 0; i < EVENT_MAX_FLOATS; ++i )
        if( Flags & ( 1u << ( EVENT_MAX_INTS + i ) ) )
            Data.StoreFloat( Count++, Effective.Floats[i] );
    Count = 0;
    for( s32 i = 0; i < EVENT_MAX_POINTS; ++i )
        if( Flags & ( 1u << ( EVENT_MAX_INTS + EVENT_MAX_FLOATS + i ) ) )
            Data.StorePoint( Count++, Effective.Points[i] );
    Count = 0;
    for( s32 i = 0; i < EVENT_MAX_BOOLS; ++i )
        if( Flags & ( 1u << ( EVENT_MAX_INTS + EVENT_MAX_FLOATS + EVENT_MAX_POINTS + i ) ) )
            Data.StoreBool( Count++, Effective.Bools[i] ? TRUE : FALSE );
    Count = 0;
    for( s32 i = 0; i < EVENT_MAX_STRINGS; ++i )
        if( Flags & ( 1u << ( EVENT_MAX_INTS + EVENT_MAX_FLOATS + EVENT_MAX_POINTS + EVENT_MAX_BOOLS + i ) ) )
            Data.StoreString( Count++, Effective.Strings[i].c_str() );
    Output.SetData( Data );
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool animation_product_writer::BuildGroup( const resource_tool::animation::asset& Asset, anim_group& Group,
                                            std::string& Error ) const
{
    const s32 nBones = (s32)Asset.Skeleton.Bones.size();
    if( nBones <= 0 || nBones > resource_tool::skeleton_contract::MaxBones )
        return Fail( Error, "Animation skeleton must contain 1..80 bones" );
    if( Asset.Clips.empty() )
        return Fail( Error, "Animation asset contains no clips" );

    for( const resource_tool::animation::skeleton_bone& Bone : Asset.Skeleton.Bones )
        if( !ValidName( Bone.Name, "Bone", Error ) )
            return FALSE;
    for( const resource_tool::animation::clip& Clip : Asset.Clips )
    {
        if( !ValidName( Clip.Name, "Clip", Error ) )
            return FALSE;
        if( Clip.FPS <= 0 || Clip.FPS > 65535 || Clip.Frames < 2 || Clip.Frames > kMaxFrames )
            return Fail( Error, "Clip has unsupported FPS or frame count: " + Clip.Name );
        if( Clip.Additive || Clip.Subtractive || !Clip.AdditiveBaseClip.empty() )
            return Fail( Error, "Product .anim cannot encode additive/subtractive clip metadata: " + Clip.Name );
    }

    Group.m_Version = ANIM_DATA_VERSION;
    Group.m_nBones = nBones;
    Group.m_pBone = (anim_bone*)x_malloc( sizeof( anim_bone ) * nBones );
    x_memset( Group.m_pBone, 0, sizeof( anim_bone ) * nBones );
    for( s32 iBone = 0; iBone < nBones; ++iBone )
    {
        const resource_tool::animation::skeleton_bone& Source = Asset.Skeleton.Bones[iBone];
        anim_bone& Bone = Group.m_pBone[iBone];
        x_strncpy( Bone.Name, Source.Name.c_str(), kMaxNameLength );
        Bone.iBone = (s16)iBone;
        Bone.iParent = (s16)Source.Parent;
        Bone.nChildren = (s16)Source.Children;
        Bone.LocalTranslation = Source.Translation;
        matrix4 WorldBind = Source.InverseBindMatrix;
        WorldBind.InvertSRT();
        Bone.BindTranslation = WorldBind.GetTranslation();
        Bone.BindMatrixInv = Source.InverseBindMatrix;
    }

    Group.m_nAnims = (s32)Asset.Clips.size();
    Group.m_pAnimInfo = (anim_info*)x_malloc( sizeof( anim_info ) * Group.m_nAnims );
    x_memset( Group.m_pAnimInfo, 0, sizeof( anim_info ) * Group.m_nAnims );
    Group.m_nProps = 0;
    Group.m_nEvents = 0;
    for( const resource_tool::animation::clip& Clip : Asset.Clips )
    {
        Group.m_nProps += (s32)Clip.Properties.size();
        Group.m_nEvents += (s32)Clip.Events.size();
    }
    Group.m_pProp = Group.m_nProps ? (anim_prop*)x_malloc( sizeof( anim_prop ) * Group.m_nProps ) : NULL;
    Group.m_pEvent = Group.m_nEvents ? new anim_event[Group.m_nEvents] : NULL;
    if( Group.m_nProps )
        x_memset( Group.m_pProp, 0, sizeof( anim_prop ) * Group.m_nProps );
    anim_event::Init();

    s32 PropCursor = 0;
    s32 EventCursor = 0;
    Group.m_nKeyBlocks = 0;
    Group.m_TotalNFrames = 0;
    Group.m_TotalNKeys = 0;
    for( s32 iClip = 0; iClip < Group.m_nAnims; ++iClip )
    {
        const resource_tool::animation::clip& Source = Asset.Clips[iClip];
        anim_info& Output = Group.m_pAnimInfo[iClip];
        Output.m_nAnims = 1;
        Output.m_AnimsWeight = Source.Weight;
        x_strncpy( Output.m_Name, Source.Name.c_str(), kMaxNameLength );
        Output.m_Weight = Source.Weight;
        Output.m_BlendTime = Source.BlendTime;
        Output.m_nFrames = (s16)Source.Frames;
        Output.m_iLoopFrame = (s16)Source.LoopFrame;
        Output.m_EndFrameOffset = (s16)Source.EndFrameOffset;
        Output.m_nEvents = (s16)Source.Events.size();
        Output.m_iEvent = (s16)EventCursor;
        Output.m_nProps = (s16)Source.Properties.size();
        Output.m_iProp = (s16)PropCursor;
        Output.m_nChainFramesMin = (s16)Source.ChainFramesMin;
        Output.m_nChainFramesMax = (s16)Source.ChainFramesMax;
        Output.m_iChainFrame = (s16)Source.ChainFrame;
        Output.m_iChainAnim = -1;
        for( s32 iChain = 0; iChain < Group.m_nAnims; ++iChain )
            if( !Source.ChainAnimation.empty() &&
                x_stricmp( Source.ChainAnimation.c_str(), Asset.Clips[iChain].Name.c_str() ) == 0 )
            {
                Output.m_iChainAnim = (s16)iChain;
                break;
            }
        Output.m_iAnimBoneMin = (s16)Source.AnimBoneMin;
        Output.m_iAnimBoneMax = (s16)Source.AnimBoneMax;
        Output.m_FPS = (u16)Source.FPS;
        Output.m_HandleAngle = EncodeAngle( Source.HandleAngle );
        Output.m_TotalYaw = EncodeAngle( Source.TotalYaw );
        Output.m_TotalMoveDir = EncodeAngle( Source.TotalMoveDirection );
        Output.m_TotalTranslation = Source.TotalTranslation;
        Output.m_Flags = ClipFlags( Source );

        for( const resource_tool::animation::property& Property : Source.Properties )
        {
            if( !ValidName( Property.Type.empty() ? Property.Name : Property.Type, "Property", Error ) )
                return FALSE;
            anim_prop& Destination = Group.m_pProp[PropCursor++];
            x_strncpy( Destination.m_Type, ( Property.Type.empty() ? Property.Name : Property.Type ).c_str(),
                       kMaxNameLength );
            Destination.m_iBone = Asset.Skeleton.FindBone( Property.BoneName );
        }
        for( const resource_tool::animation::event& Event : Source.Events )
        {
            const s32 EventBone = Event.Bone >= 0 ? Event.Bone : Asset.Skeleton.FindBone( Event.BoneName );
            CopyEvent( Event, EventBone, Group.m_pEvent[EventCursor++] );
        }
        EventCursor = Output.m_iEvent + Output.m_nEvents;
        Group.m_TotalNFrames += Source.Frames;
        Group.m_TotalNKeys += ( nBones + (s32)Source.Properties.size() ) * Source.Frames;
        Output.m_AnimKeys.m_nFrames = (s16)Source.Frames;
        Output.m_AnimKeys.m_nBones = (s16)nBones;
        Output.m_AnimKeys.m_nProps = (s16)Source.Properties.size();
        Output.m_AnimKeys.m_iKeyBlock = Group.m_nKeyBlocks;
        Output.m_AnimKeys.m_nKeyBlocks = ( Source.Frames - 2 ) / MAX_KEYS_PER_BLOCK + 1;
        Group.m_nKeyBlocks += Output.m_AnimKeys.m_nKeyBlocks;
    }

    Group.m_pKeyBlock = (anim_key_block*)x_malloc( sizeof( anim_key_block ) * Group.m_nKeyBlocks );
    x_memset( Group.m_pKeyBlock, 0, sizeof( anim_key_block ) * Group.m_nKeyBlocks );

    bitstream BitStream;
    BitStream.Init( 500 * 1024 );
    BitStream.SetMaxGrowSize( 1024 * 1024 * 1024 );
    std::vector<f32> BoneLengths( nBones, 0.0f );
    for( s32 iClip = 0; iClip < Group.m_nAnims; ++iClip )
    {
        const resource_tool::animation::clip& Source = Asset.Clips[iClip];
        anim_info& Output = Group.m_pAnimInfo[iClip];
        const s32 nStreams = nBones + (s32)Source.Properties.size();
        std::vector<anim_key> Keys( (usize)nStreams * Source.Frames );
        std::vector<u32> StreamFlags( nStreams, 0 );

        for( s32 iBone = 0; iBone < nBones; ++iBone )
        {
            const resource_tool::animation::skeleton_bone& Bone = Asset.Skeleton.Bones[iBone];
            const resource_tool::animation::bone_channel* pChannel = FindChannel( Source, Bone.Name );
            if( IsMasked( Source, Bone.Name ) )
                StreamFlags[iBone] |= STREAM_FLAG_MASKED;
            for( s32 iFrame = 0; iFrame < Source.Frames; ++iFrame )
            {
                sampled_key Fallback;
                Fallback.Translation = Bone.Translation;
                Fallback.Rotation = Bone.Rotation;
                Fallback.Scale = Bone.Scale;
                const sampled_key Key = SampleChannel( pChannel, (f32)iFrame / (f32)Source.FPS, Fallback );
                anim_key& Destination = Keys[(usize)iBone * Source.Frames + iFrame];
                Destination.Scale = Key.Scale;
                Destination.Rotation = Key.Rotation;
                Destination.Translation = Key.Translation;
                if( StreamFlags[iBone] & STREAM_FLAG_MASKED )
                {
                    Destination.Scale.Set( 1.0f, 1.0f, 1.0f );
                    Destination.Rotation.Identity();
                    Destination.Translation.Zero();
                }
            }
        }
        for( usize iProperty = 0; iProperty < Source.Properties.size(); ++iProperty )
        {
            const resource_tool::animation::property* pProperty = FindProperty( Source, iProperty );
            for( s32 iFrame = 0; iFrame < Source.Frames; ++iFrame )
            {
                const sampled_key Key = SampleProperty( *pProperty, (f32)iFrame / (f32)Source.FPS );
                anim_key& Destination = Keys[(usize)( nBones + iProperty ) * Source.Frames + iFrame];
                Destination.Scale = Key.Scale;
                Destination.Rotation = Key.Rotation;
                Destination.Translation = Key.Translation;
            }
        }

        s32 AnimBoneMin = -1;
        s32 AnimBoneMax = -1;
        for( s32 iBlock = 0; iBlock < Output.m_AnimKeys.m_nKeyBlocks; ++iBlock )
        {
            anim_key_block& Block = Group.m_pKeyBlock[Output.m_AnimKeys.m_iKeyBlock + iBlock];
            const s32 StartFrame = iBlock * MAX_KEYS_PER_BLOCK;
            const s32 EndFrame = x_min( StartFrame + MAX_KEYS_PER_BLOCK, Source.Frames - 1 );
            Block.nFrames = (s32)( EndFrame - StartFrame + 1 );
            Block.CompressedDataOffset = BitStream.GetCursor() >> 3;
            const s32 DataStart = Block.CompressedDataOffset;
            s32 DecompressedSize = 0;
            CompressAnimationData( BitStream, Keys.data(), Group.m_pBone, nBones, StreamFlags.data(), Source.Frames,
                                   nStreams, StartFrame, EndFrame, BoneLengths.data(), DecompressedSize, AnimBoneMin,
                                   AnimBoneMax );
            Block.DecompressedDataSize = DecompressedSize;
            const s32 DataEnd = BitStream.GetCursor() >> 3;
            Block.Checksum = x_chksum( BitStream.GetDataPtr() + DataStart, DataEnd - DataStart );
        }
        Output.m_iAnimBoneMin = (s16)AnimBoneMin;
        Output.m_iAnimBoneMax = (s16)AnimBoneMax;
    }

    Group.m_CompressedDataSize = BitStream.GetNBytesUsed();
    Group.m_pCompressedData = (byte*)x_malloc( Group.m_CompressedDataSize );
    x_memcpy( Group.m_pCompressedData, BitStream.GetDataPtr(), Group.m_CompressedDataSize );
    const s32 BoneSize = (s32)sizeof( anim_bone ) * Group.m_nBones;
    const s32 InfoSize = (s32)sizeof( anim_info ) * Group.m_nAnims;
    const s32 PropSize = (s32)sizeof( anim_prop ) * Group.m_nProps;
    const s32 KeySize = (s32)sizeof( anim_key_block ) * Group.m_nKeyBlocks;
    const s32 BoneOffset = 0;
    const s32 InfoOffset = ( BoneOffset + BoneSize + 3 ) & ~3;
    const s32 PropOffset = ( InfoOffset + InfoSize + 3 ) & ~3;
    const s32 KeyOffset = ( PropOffset + PropSize + 3 ) & ~3;
    Group.m_UncompressedDataSize = ( KeyOffset + KeySize + 3 ) & ~3;
    Group.m_pUncompressedData = (byte*)x_malloc( Group.m_UncompressedDataSize );
    x_memset( Group.m_pUncompressedData, 0, Group.m_UncompressedDataSize );
    x_memcpy( Group.m_pUncompressedData + BoneOffset, Group.m_pBone, BoneSize );
    x_memcpy( Group.m_pUncompressedData + InfoOffset, Group.m_pAnimInfo, InfoSize );
    x_memcpy( Group.m_pUncompressedData + PropOffset, Group.m_pProp, PropSize );
    x_memcpy( Group.m_pUncompressedData + KeyOffset, Group.m_pKeyBlock, KeySize );
    x_free( Group.m_pBone );
    x_free( Group.m_pAnimInfo );
    x_free( Group.m_pProp );
    x_free( Group.m_pKeyBlock );
    Group.m_pBone = (anim_bone*)( Group.m_pUncompressedData + BoneOffset );
    Group.m_pAnimInfo = (anim_info*)( Group.m_pUncompressedData + InfoOffset );
    Group.m_pProp = (anim_prop*)( Group.m_pUncompressedData + PropOffset );
    Group.m_pKeyBlock = (anim_key_block*)( Group.m_pUncompressedData + KeyOffset );
    for( s32 iClip = 0; iClip < Group.m_nAnims; ++iClip )
        Group.m_pAnimInfo[iClip].m_pAnimGroup = &Group;
    return TRUE;
}

//=========================================================================

xbool animation_product_writer::Write( const resource_tool::animation::asset& Asset, const char* pOutputFile,
                                       std::string& Error ) const
{
    if( !pOutputFile || !pOutputFile[0] )
        return Fail( Error, "Product animation output path is empty" );

    anim_group Group;
    if( !BuildGroup( Asset, Group, Error ) )
        return FALSE;
    return WriteDiskFile( Group, pOutputFile, Error );
}

//=========================================================================

xbool animation_product_writer::WriteDiskFile( const anim_group& Group, const char* pOutputFile,
                                               std::string& Error ) const
{
    constexpr s32 HeaderSize = 176;
    constexpr s32 DiskBoneSize = 144;
    constexpr s32 DiskInfoSize = 160;
    constexpr s32 DiskPropSize = 36;
    constexpr s32 DiskKeySize = 28;
    const auto Align16 = []( s32 Value ) { return ( Value + 15 ) & ~15; };

    if( (s32)sizeof( anim_bone ) != DiskBoneSize || (s32)sizeof( anim_prop ) != DiskPropSize )
    {
        return Fail( Error, "Product animation disk layout does not match the Area 51 format" );
    }

    const s32 BoneOffset = 0;
    const s32 AnimOffset = Align16( BoneOffset + Group.m_nBones * DiskBoneSize );
    const s32 PropOffset = Align16( AnimOffset + Group.m_nAnims * DiskInfoSize );
    const s32 KeyOffset = Align16( PropOffset + Group.m_nProps * DiskPropSize );
    const s32 DiskDataSize = Align16( KeyOffset + Group.m_nKeyBlocks * DiskKeySize );
    std::vector<byte> DiskData( DiskDataSize, 0 );

    x_memcpy( DiskData.data() + BoneOffset, Group.m_pBone, Group.m_nBones * DiskBoneSize );
    for( s32 iAnim = 0; iAnim < Group.m_nAnims; ++iAnim )
    {
        const byte* pSource = (const byte*)&Group.m_pAnimInfo[iAnim];
        byte* pDestination = DiskData.data() + AnimOffset + iAnim * DiskInfoSize;
        x_memcpy( pDestination, pSource, 48 );
        x_memcpy( pDestination + 52, pSource + 56, 94 );
    }
    x_memcpy( DiskData.data() + PropOffset, Group.m_pProp, Group.m_nProps * DiskPropSize );
    for( s32 iKey = 0; iKey < Group.m_nKeyBlocks; ++iKey )
    {
        const anim_key_block& Source = Group.m_pKeyBlock[iKey];
        byte* pDestination = DiskData.data() + KeyOffset + iKey * DiskKeySize;
        x_memcpy( pDestination + 12, &Source.Checksum, sizeof( Source.Checksum ) );
        x_memcpy( pDestination + 20, &Source.CompressedDataOffset, sizeof( Source.CompressedDataOffset ) );
        const u32 BitField =
            ( (u32)Source.nFrames & 0xffu ) | ( ( (u32)Source.DecompressedDataSize & 0x00ffffffu ) << 8 );
        x_memcpy( pDestination + 24, &BitField, sizeof( BitField ) );
    }

    std::vector<byte> Header( HeaderSize, 0 );
    x_memcpy( Header.data() + 0, &Group.m_BBox, 32 );
    const std::string FileName = std::filesystem::path( pOutputFile ).filename().string();
    x_strncpy( (char*)Header.data() + 32, FileName.c_str(), 59 );
    const auto StoreU32 = [&Header]( s32 Offset, u32 Value )
    { x_memcpy( Header.data() + Offset, &Value, sizeof( Value ) ); };
    StoreU32( 96, ANIM_DATA_VERSION );
    StoreU32( 100, Group.m_TotalNFrames );
    StoreU32( 104, Group.m_TotalNKeys );
    StoreU32( 108, Group.m_nBones );
    StoreU32( 112, BoneOffset );
    StoreU32( 116, Group.m_nAnims );
    StoreU32( 120, AnimOffset );
    StoreU32( 124, Group.m_nProps );
    StoreU32( 128, PropOffset );
    StoreU32( 132, Group.m_nEvents );
    StoreU32( 144, Group.m_nKeyBlocks );
    StoreU32( 148, KeyOffset );
    StoreU32( 152, DiskDataSize );
    StoreU32( 160, Group.m_CompressedDataSize );

    std::ofstream Output( pOutputFile ? pOutputFile : "", std::ios::binary );
    if( !Output )
        return Fail( Error,
                     "Unable to open compiled animation output: " + std::string( pOutputFile ? pOutputFile : "" ) );
    Output.write( (const char*)Header.data(), Header.size() );
    Output.write( (const char*)DiskData.data(), DiskData.size() );
    Output.write( (const char*)Group.m_pCompressedData, Group.m_CompressedDataSize );
    for( s32 iEvent = 0; iEvent < Group.m_nEvents; ++iEvent )
    {
        const event_data Data = Group.m_pEvent[iEvent].GetData();
        Output.write( (const char*)&Data, sizeof( Data ) );
    }
    if( !Output )
        return Fail( Error,
                     "Unable to finish compiled animation output: " + std::string( pOutputFile ? pOutputFile : "" ) );
    return TRUE;
}

} // namespace resource_tool::animation_product_format
