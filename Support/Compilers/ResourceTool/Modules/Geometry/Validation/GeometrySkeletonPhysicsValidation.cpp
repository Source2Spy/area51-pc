//=========================================================================
//
//  GeometrySkeletonPhysicsValidation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryValidationInternal.hpp"

#include "Diagnostics.hpp"

namespace resource_tool::geometry::validation_detail
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool ValidateBoneParents( const geometry_asset& Asset, diagnostic_log& Diagnostics, const char* pSourceFile )
{
    xbool bValid = TRUE;
    for( s32 iBone = 0; iBone < (s32)Asset.Skeleton.Bones.size(); ++iBone )
    {
        const geometry_bone& Bone = Asset.Skeleton.Bones[iBone];
        if( Bone.iParent < -1 || Bone.iParent >= (s32)Asset.Skeleton.Bones.size() )
        {
            bValid =
                Report( Diagnostics, xfs( "bone [%s] has invalid parent index %d", Bone.Name.c_str(), Bone.iParent ),
                        Bone.Name.c_str(), pSourceFile ) &&
                bValid;
            continue;
        }
        if( Bone.iParent == iBone )
        {
            bValid = Report( Diagnostics, xfs( "bone [%s] cannot parent itself", Bone.Name.c_str() ), Bone.Name.c_str(),
                             pSourceFile ) &&
                     bValid;
            continue;
        }

        s32 Parent = Bone.iParent;
        for( s32 Depth = 0; Parent >= 0 && Parent < (s32)Asset.Skeleton.Bones.size(); ++Depth )
        {
            if( Depth > (s32)Asset.Skeleton.Bones.size() )
            {
                bValid = Report( Diagnostics, xfs( "bone [%s] has a cyclic parent hierarchy", Bone.Name.c_str() ),
                                 Bone.Name.c_str(), pSourceFile ) &&
                         bValid;
                break;
            }
            Parent = Asset.Skeleton.Bones[Parent].iParent;
        }
    }
    return bValid;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool ValidateSkeletonAndPhysics( const geometry_asset& Asset, const validation_options& Options,
                                  diagnostic_log& Diagnostics, const char* pSourceFile )
{
    xbool bValid = ValidateBoneParents( Asset, Diagnostics, pSourceFile );
    for( s32 iBody = 0; iBody < (s32)Asset.Physics.RigidBodies.size(); ++iBody )
    {
        const geometry_rigid_body& Body = Asset.Physics.RigidBodies[iBody];
        if( !Body.BoneName.empty() && FindNamed( Asset.Skeleton.Bones, Body.BoneName ) != Body.iBone )
        {
            bValid = Report( Diagnostics,
                             xfs( "rigid body [%s] has a bone identity/index mismatch for [%s]", Body.Name.c_str(),
                                  Body.BoneName.c_str() ),
                             Body.Name.c_str(), pSourceFile ) &&
                     bValid;
        }
        if( !Body.ParentName.empty() && FindNamed( Asset.Physics.RigidBodies, Body.ParentName ) != Body.iParent )
        {
            bValid = Report( Diagnostics,
                             xfs( "rigid body [%s] has a parent identity/index mismatch for [%s]", Body.Name.c_str(),
                                  Body.ParentName.c_str() ),
                             Body.Name.c_str(), pSourceFile ) &&
                     bValid;
        }
    }
    (void)Options;
    return bValid;
}

} // namespace resource_tool::geometry::validation_detail
