//=========================================================================
//
//  SkeletonStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SkeletonStage.hpp"

#include "Diagnostics.hpp"
#include "GeometryNames.hpp"

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

s32 FindBone( const geometry_asset& Asset, const std::string& Name )
{
    s32 Found = -1;
    for( s32 i = 0; i < (s32)Asset.Skeleton.Bones.size(); ++i )
    {
        if( !geometry_names_match( Asset.Skeleton.Bones[i].Name.c_str(), Name.c_str() ) )
            continue;
        if( Found >= 0 )
            return -2;
        Found = i;
    }
    return Found;
}

//=========================================================================

s32 FindRigidBody( const geometry_asset& Asset, const std::string& Name )
{
    s32 Found = -1;
    for( s32 i = 0; i < (s32)Asset.Physics.RigidBodies.size(); ++i )
    {
        if( !geometry_names_match( Asset.Physics.RigidBodies[i].Name.c_str(), Name.c_str() ) )
            continue;
        if( Found >= 0 )
            return -2;
        Found = i;
    }
    return Found;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::skeleton_stage::Execute( const geometry_asset& Asset, diagnostic_log& Diagnostics,
                                                        const char* pSourceFile ) const
{
    xbool bValid = TRUE;
    for( s32 iBone = 0; iBone < (s32)Asset.Skeleton.Bones.size(); ++iBone )
    {
        const geometry_bone& Bone = Asset.Skeleton.Bones[iBone];
        if( Bone.iParent < -1 || Bone.iParent >= (s32)Asset.Skeleton.Bones.size() )
        {
            Diagnostics.Error( xfs( "bone [%s] has invalid parent index %d", Bone.Name.c_str(), Bone.iParent ),
                               "GeometrySkeleton", Bone.Name.c_str(), pSourceFile );
            bValid = FALSE;
        }
        if( !Bone.ParentName.empty() )
        {
            const s32 Parent = FindBone( Asset, Bone.ParentName );
            if( Parent < 0 || Parent != Bone.iParent )
            {
                Diagnostics.Error( xfs( "bone [%s] has a parent identity/index mismatch for [%s]", Bone.Name.c_str(),
                                        Bone.ParentName.c_str() ),
                                   "GeometrySkeleton", Bone.Name.c_str(), pSourceFile );
                bValid = FALSE;
            }
        }
        if( !Bone.RigidBodyName.empty() )
        {
            const s32 Body = FindRigidBody( Asset, Bone.RigidBodyName );
            if( Body < 0 || Body != Bone.iRigidBody )
            {
                Diagnostics.Error( xfs( "bone [%s] has a rigid-body identity/index mismatch for [%s]",
                                        Bone.Name.c_str(), Bone.RigidBodyName.c_str() ),
                                   "GeometrySkeleton", Bone.Name.c_str(), pSourceFile );
                bValid = FALSE;
            }
        }
    }

    for( const geometry_facet& Facet : Asset.Render.Facets )
    {
        if( Facet.BoneName.empty() )
            continue;
        const s32 Bone = FindBone( Asset, Facet.BoneName );
        if( Bone < 0 )
        {
            Diagnostics.Error( xfs( "facet references missing bone [%s]", Facet.BoneName.c_str() ), "GeometrySkeleton",
                               Facet.BoneName.c_str(), pSourceFile );
            bValid = FALSE;
        }
    }

    for( const geometry_vertex& Vertex : Asset.Render.Vertices )
    {
        for( const geometry_weight& Weight : Vertex.Weight )
        {
            if( Weight.BoneName.empty() )
                continue;
            const s32 Bone = FindBone( Asset, Weight.BoneName );
            if( Bone < 0 || Bone != Weight.iBone )
            {
                Diagnostics.Error(
                    xfs( "vertex weight has a bone identity/index mismatch for [%s]", Weight.BoneName.c_str() ),
                    "GeometrySkeleton", Weight.BoneName.c_str(), pSourceFile );
                bValid = FALSE;
            }
        }
    }
    return bValid;
}
