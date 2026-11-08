//=========================================================================
//
//  SkeletonSource.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SkeletonSource.hpp"

#include "AnimationProductReader.hpp"

#include <vector>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

namespace resource_tool::geometry
{

//=========================================================================

xbool LoadSkeletonSource( const char* pFileName, resource_tool::animation::asset& Asset, std::string& Error )
{
    if( !pFileName || !pFileName[0] )
    {
        Error = "Skin geometry requires a compiled animation skeleton source";
        return FALSE;
    }

    resource_tool::animation_product_format::animation_product_reader Reader;
    if( !Reader.Load( pFileName, Asset, Error ) )
        return FALSE;
    if( Asset.Skeleton.Bones.empty() )
    {
        Error = "Animation skeleton source contains no bones";
        return FALSE;
    }
    if( Asset.Clips.empty() )
    {
        Error = "Animation skeleton source contains no animation clips";
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool ApplySkeletonSource( const resource_tool::animation::asset& Source, geometry_asset& Target, std::string& Error )
{
    if( Source.Skeleton.Bones.size() != Target.Skeleton.Bones.size() )
    {
        Error = "Skin geometry and animation skeleton bone counts differ";
        return FALSE;
    }

    auto ResolveSourceBone = [&]( s32 iTargetBone, const char* pKind ) -> s32
    {
        if( iTargetBone < 0 || iTargetBone >= (s32)Source.Skeleton.Bones.size() )
        {
            Error = xfs( "%s references invalid bone index %d", pKind, iTargetBone );
            return -1;
        }
        return iTargetBone;
    };

    for( geometry_vertex& Vertex : Target.Render.Vertices )
    {
        for( geometry_weight& Weight : Vertex.Weight )
        {
            const s32 iSourceBone = ResolveSourceBone( Weight.iBone, "vertex weight" );
            if( iSourceBone < 0 )
                return FALSE;
            Weight.iBone = iSourceBone;
            Weight.BoneName = Source.Skeleton.Bones[iSourceBone].Name;
        }
    }

    for( geometry_rigid_body& Body : Target.Physics.RigidBodies )
    {
        if( Body.iBone < 0 )
            continue;
        const s32 iSourceBone = ResolveSourceBone( Body.iBone, "rigid body" );
        if( iSourceBone < 0 )
            return FALSE;
        Body.iBone = iSourceBone;
        Body.BoneName = Source.Skeleton.Bones[iSourceBone].Name;
    }

    for( geometry_bone_mask& Mask : Target.EngineMetadata.BoneMasks )
    {
        for( geometry_bone_mask_weight& Weight : Mask.Weights )
        {
            const s32 iSourceBone = ResolveSourceBone( Weight.iBone, "bone mask" );
            if( iSourceBone < 0 )
                return FALSE;
            Weight.iBone = iSourceBone;
            Weight.BoneName = Source.Skeleton.Bones[iSourceBone].Name;
        }
    }

    for( geometry_collision_cluster& Cluster : Target.Collision.Clusters )
    {
        if( Cluster.BoneName.empty() && Cluster.Bone < 0 )
            continue;
        const s32 iSourceBone = ResolveSourceBone( Cluster.Bone, "collision cluster" );
        if( iSourceBone < 0 )
            return FALSE;
        Cluster.Bone = iSourceBone;
        Cluster.BoneName = Source.Skeleton.Bones[iSourceBone].Name;
    }

    std::vector<geometry_bone> CanonicalBones( Source.Skeleton.Bones.size() );
    for( s32 iSourceBone = 0; iSourceBone < (s32)Source.Skeleton.Bones.size(); ++iSourceBone )
    {
        geometry_bone& Destination = CanonicalBones[iSourceBone];
        const resource_tool::animation::skeleton_bone& SourceBone = Source.Skeleton.Bones[iSourceBone];
        const geometry_bone& Metadata = Target.Skeleton.Bones[iSourceBone];

        Destination.Name = SourceBone.Name;
        Destination.ParentName = SourceBone.ParentName;
        Destination.nChildren = SourceBone.Children;
        Destination.iParent = SourceBone.Parent;
        Destination.Scale = SourceBone.Scale;
        Destination.Rotation = SourceBone.Rotation;
        Destination.Position = SourceBone.Translation;
        Destination.InverseBindMatrix = SourceBone.InverseBindMatrix;
        Destination.LODGroup = SourceBone.LodGroup;
        Destination.BBox = Metadata.BBox;
        Destination.iRigidBody = Metadata.iRigidBody;
        Destination.HitLocation = Metadata.HitLocation;
        Destination.RigidBodyName = Metadata.RigidBodyName;
    }

    Target.Skeleton.Bones = std::move( CanonicalBones );

    for( geometry_rigid_body& Body : Target.Physics.RigidBodies )
    {
        if( Body.iBone >= 0 && Body.iBone < (s32)Target.Skeleton.Bones.size() )
            Body.BoneName = Target.Skeleton.Bones[Body.iBone].Name;
    }
    return TRUE;
}

} // namespace resource_tool::geometry
