//=========================================================================
//
//  GeometryProductPhysicsStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductPhysicsStage.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_physics_stage::Execute( const geometry_product_data& Source, geometry_asset& Asset,
                                               std::string& Error ) const
{
    (void)Error;
    Asset.Physics.RigidBodies.resize( Source.RigidBodyCount );
    for( s32 iBody = 0; iBody < Source.RigidBodyCount; ++iBody )
    {
        const geometry_product_data::rigid_body& SourceBody = Source.RigidBodies[iBody];
        geometry_rigid_body& Body = Asset.Physics.RigidBodies[iBody];
        Body.Name = Source.GetRigidBodyName( iBody );
        Body.Type = SourceBody.Type == geometry_product_data::rigid_body::TYPE_BOX        ? "Box"
                    : SourceBody.Type == geometry_product_data::rigid_body::TYPE_CYLINDER ? "Cylinder"
                                                                                          : "Sphere";
        Body.iParent = SourceBody.iParentBody;
        Body.iBone = SourceBody.iBone;
        Body.BoneName =
            ( Body.iBone >= 0 && Body.iBone < Source.BoneCount ) ? Asset.Skeleton.Bones[Body.iBone].Name : "";
        Body.Mass = SourceBody.Mass;
        Body.PivotPosition = SourceBody.PivotBindPosition;
        Body.PivotRotation = SourceBody.PivotBindRotation;
        // Product dimensions already include body scale; use a neutral source scale.
        Body.PivotScale.Set( 1.0f, 1.0f, 1.0f );
        Body.BodyScale.Set( 1.0f, 1.0f, 1.0f );
        Body.BodyPosition = SourceBody.BodyBindPosition;
        Body.BodyRotation = SourceBody.BodyBindRotation;
        Body.Radius = SourceBody.Radius;
        Body.Width = SourceBody.Width;
        Body.Height = SourceBody.Height;
        Body.Length = SourceBody.Length;
        Body.Flags = SourceBody.Flags;
        Body.CollisionMask = (s32)SourceBody.CollisionMask;
        for( s32 iDOF = 0; iDOF < 6; ++iDOF )
        {
            Body.DOF[iDOF].bActive =
                ( SourceBody.DOF[iDOF].Flags & geometry_product_data::rigid_body::dof::FLAG_ACTIVE ) != 0;
            Body.DOF[iDOF].bLimited =
                ( SourceBody.DOF[iDOF].Flags & geometry_product_data::rigid_body::dof::FLAG_LIMITED ) != 0;
            Body.DOF[iDOF].Min = SourceBody.DOF[iDOF].Min;
            Body.DOF[iDOF].Max = SourceBody.DOF[iDOF].Max;
        }
        if( Body.iParent >= 0 && Body.iParent < Source.RigidBodyCount )
            Body.ParentName = Source.GetRigidBodyName( Body.iParent );
    }
    return TRUE;
}

} // namespace resource_tool::geometry_product_format
