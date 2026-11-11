//=========================================================================
//
//  GeometryProductWriter_Skeleton.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"
#include "SkeletonContract.hpp"

struct bone_to_hit_mapping
{
    const char* pBoneNameFragment;
    geom::bone::hit_location HitLocation;
};

static bone_to_hit_mapping g_BoneHitLocationMapping[] = {
    { "Leg", geom::bone::HIT_LOCATION_LEGS },

    { "Spine", geom::bone::HIT_LOCATION_TORSO },

    { "Neck", geom::bone::HIT_LOCATION_HEAD },
    { "Head", geom::bone::HIT_LOCATION_HEAD },

    { "Arm_L", geom::bone::HIT_LOCATION_SHOULDER_LEFT },
    { "L_Forearm", geom::bone::HIT_LOCATION_SHOULDER_LEFT },
    { "L_Shoulder", geom::bone::HIT_LOCATION_SHOULDER_LEFT },

    { "Arm_R", geom::bone::HIT_LOCATION_SHOULDER_RIGHT },
    { "R_Forearm", geom::bone::HIT_LOCATION_SHOULDER_RIGHT },
    { "R_Shoulder", geom::bone::HIT_LOCATION_SHOULDER_RIGHT },

    // Dust mite mappings.
    { "Body", geom::bone::HIT_LOCATION_TORSO },
    { "Jaw", geom::bone::HIT_LOCATION_HEAD },

};

//=========================================================================

struct object_to_object_mapping
{
    const char* pObjectA;
    const char* pObjectB;
};

// Bone-to-rigid-body aliases.
static object_to_object_mapping g_BoneToRigidBodyMapping[] = {
    // Bone / rigid body aliases.

    // Extra theta bones.
    { "Leg_L_Foot_Squash01", "L_Foot" },
    { "Leg_L_Foot_Squash02", "L_Foot" },
    { "Leg_L_Foot", "L_Foot" },
    { "Leg_L_Calf01", "L_Calf01" },
    { "Leg_L_Calf02", "L_Calf02" },
    { "Leg_L_Thigh", "L_Thigh" },

    { "ChstArm_L_UpperArm", "L_ChstArm_UpperArm" },
    { "ChstArm_L_ForeArm", "L_ChstArm_LowerArm" },
    { "ChstArm_L_Hand", "L_ChstArm_Hand" },
    { "ChstArm_L_Finger", "L_ChstArm_Hand" },
    { "ChstArm_L_Thumb", "L_ChstArm_Hand" },

    { "Leg_R_Foot_Squash01", "R_Foot" },
    { "Leg_R_Foot_Squash02", "R_Foot" },
    { "Leg_R_Foot", "R_Foot" },
    { "Leg_R_Calf01", "R_Calf01" },
    { "Leg_R_Calf02", "R_Calf02" },
    { "Leg_R_Thigh", "R_Thigh" },

    { "ChstArm_R_UpperArm", "R_ChstArm_UpperArm" },
    { "ChstArm_R_ForeArm", "R_ChstArm_LowerArm" },
    { "ChstArm_R_Hand", "R_ChstArm_Hand" },
    { "ChstArm_R_Finger", "R_ChstArm_Hand" },
    { "ChstArm_R_Thumb", "R_ChstArm_Hand" },

    { "Neck", "Neck" },
    { "spore", "Torso" },

    // Normal characters.
    { "Root", "Pelvis" },
    { "Spine01", "Pelvis" },
    { "Spine02", "Torso" },
    { "Head", "Head" },
    { "Neck", "Torso" },
    { "Face", "Head" },
    { "Jaw", "Head" },

    { "Arm_R_UpperArm", "R_UpperArm" },
    { "Arm_R_ForeArm", "R_LowerArm" },
    { "R_Forearm", "R_LowerArm" },
    { "Arm_R_Hand", "R_Hand" },
    { "Hand_R", "R_Hand" },
    { "Attach_R", "R_Hand" },

    { "Arm_L_UpperArm", "L_UpperArm" },
    { "Arm_L_ForeArm", "L_LowerArm" },
    { "L_Forearm", "L_LowerArm" },
    { "Arm_L_Hand", "L_Hand" },
    { "Hand_L", "L_Hand" },
    { "Attach_L", "L_Hand" },

    { "Leg_R_Thigh", "R_Thigh" },
    { "Leg_R_Calf", "R_Calf" },
    { "Leg_R_Foot", "R_Foot" },
    { "Leg_R_Toe", "R_Foot" },

    { "Leg_L_Thigh", "L_Thigh" },
    { "Leg_L_Calf", "L_Calf" },
    { "Leg_L_Foot", "L_Foot" },
    { "Leg_L_Toe", "L_Foot" },

    { "L_Shoulder", "Torso" },
    { "R_Shoulder", "Torso" },
    { "Body", "Torso" },
};

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void geometry_product_writer::BuildBone( geom::bone& Bone, const geometry_bone& RawBone )
{
    // Copy bind data.
    Bone.BindRotation = RawBone.Rotation;
    Bone.BindPosition = RawBone.Position;

    // Copy bounds.
    Bone.BBox = RawBone.BBox;

    // Copy hit location and rigid-body index.
    Bone.HitLocation =
        RawBone.HitLocation == 0 ? geom::bone::HIT_LOCATION_UNKNOWN : (geom::bone::hit_location)RawBone.HitLocation;
    Bone.iRigidBody = RawBone.iRigidBody;
}

//=========================================================================

void geometry_product_writer::BuildBones( geom& Geom, const geometry_asset& GeomRawMesh,
                                          const geometry_asset& PhysicsRawMesh )
{
    s32 i, j, nMaps;

    // Get bone count.
    s32 nBones = (s32)GeomRawMesh.Skeleton.Bones.size();
    if( !nBones )
        return;

    // Create bones.
    Geom.m_nBones = nBones;
    Geom.m_pBone = new geom::bone[nBones];
    if( Geom.m_pBone == NULL )
        x_throw( "Out of memory" );

    // Build bones.
    for( i = 0; i < nBones; i++ )
        BuildBone( Geom.m_pBone[i], GeomRawMesh.Skeleton.Bones[i] );

    // Resolve hit locations.
    for( i = 0; i < nBones; i++ )
    {
        // Check hit-location aliases.
        nMaps = sizeof( g_BoneHitLocationMapping ) / sizeof( bone_to_hit_mapping );
        for( j = 0; j < nMaps; j++ )
        {
            // Match the bone name.
            if( x_stristr( GeomRawMesh.Skeleton.Bones[i].Name.c_str(), g_BoneHitLocationMapping[j].pBoneNameFragment ) )
            {
                // Keep an explicit mapping when present.
                geom::bone& Bone = Geom.m_pBone[i];
                if( Bone.HitLocation == geom::bone::HIT_LOCATION_UNKNOWN )
                    Bone.HitLocation = g_BoneHitLocationMapping[j].HitLocation;
            }
        }
    }

    // Resolve bone-to-rigid-body mappings.
    if( !PhysicsRawMesh.Physics.RigidBodies.empty() )
    {
        // Check each bone.
        for( i = 0; i < nBones; i++ )
        {
            // Check mapping aliases.
            nMaps = sizeof( g_BoneToRigidBodyMapping ) / sizeof( object_to_object_mapping );
            for( j = 0; j < nMaps; j++ )
            {
                // Match the bone name.
                if( x_stristr( GeomRawMesh.Skeleton.Bones[i].Name.c_str(), g_BoneToRigidBodyMapping[j].pObjectA ) )
                {
                    // Keep an explicit mapping when present.
                    geom::bone& Bone = Geom.m_pBone[i];
                    if( Bone.iRigidBody == -1 )
                        Bone.iRigidBody =
                            PhysicsRawMesh.GetRigidBodyIDFromName( g_BoneToRigidBodyMapping[j].pObjectB, TRUE );
                }
            }
        }

        // Fill unresolved mappings.
        for( i = 0; i < nBones; i++ )
        {
            // Get the bone.
            geom::bone& Bone = Geom.m_pBone[i];

            // Try name lookup when needed.
            if( Bone.iRigidBody == -1 )
            {
                // Try matching rigid-body names.
                const char* pBoneName = GeomRawMesh.Skeleton.Bones[i].Name.c_str();
                while( ( Bone.iRigidBody == -1 ) && ( pBoneName[0] ) )
                {
                    // Try the current name suffix.
                    Bone.iRigidBody = PhysicsRawMesh.GetRigidBodyIDFromName( pBoneName, TRUE );

                    // Advance through the name.
                    pBoneName++;
                }

                // Fall back to the root body.
                if( Bone.iRigidBody == -1 )
                {
                    Bone.iRigidBody = 0;

                    ReportWarning( xfs( "Bone [%s] -> RigidBody mapping not found, defaulting to root rigid body.",
                                        GeomRawMesh.Skeleton.Bones[i].Name.c_str() ) );
                }
            }
        }
    }

    // Dump mappings in verbose mode.
    if( ( Context().m_bVerbose ) && ( !PhysicsRawMesh.Physics.RigidBodies.empty() ) )
    {
        x_printf( "\nBone->RigidBody mappings:\n" );
        for( i = 0; i < nBones; i++ )
        {
            geom::bone& Bone = Geom.m_pBone[i];
            if( Bone.iRigidBody != -1 )
            {
                x_printf( "\"%s\" mapped to \"%s\"\n", GeomRawMesh.Skeleton.Bones[i].Name.c_str(),
                          PhysicsRawMesh.Physics.RigidBodies[Bone.iRigidBody].Name.c_str() );
            }
            else
            {
                x_printf( "\"%s\" mapped to NULL!\n", GeomRawMesh.Skeleton.Bones[i].Name.c_str() );
            }
        }
        x_printf( "\n\n" );
    }
}

//=========================================================================

void geometry_product_writer::BuildRigidBody( geom::rigid_body& RigidBody, const geometry_rigid_body& RawRigidBody )
{
    // Copy rigid-body properties.
    RigidBody.BodyBindRotation = RawRigidBody.BodyRotation;
    RigidBody.BodyBindPosition = RawRigidBody.BodyPosition;
    RigidBody.PivotBindRotation = RawRigidBody.PivotRotation;
    RigidBody.PivotBindPosition = RawRigidBody.PivotPosition;
    RigidBody.NameOffset = Context().m_Dictionary.Add( RawRigidBody.Name.c_str() );
    RigidBody.Mass = RawRigidBody.Mass;
    RigidBody.Radius = RawRigidBody.Radius;
    RigidBody.Width = RawRigidBody.Width;
    RigidBody.Height = RawRigidBody.Height;
    RigidBody.Length = RawRigidBody.Length;
    RigidBody.iParentBody = RawRigidBody.iParent;
    RigidBody.iBone = RawRigidBody.iBone;

    // Bake bind scale into the dimensions.
    RigidBody.Radius *= RawRigidBody.BodyScale.GetX();
    RigidBody.Width *= RawRigidBody.BodyScale.GetX();
    RigidBody.Height *= RawRigidBody.BodyScale.GetY();
    RigidBody.Length *= RawRigidBody.BodyScale.GetZ();

    RigidBody.Flags = (u16)RawRigidBody.Flags;

    // Preserve the legacy rope collision rule.
    if( x_stristr( RawRigidBody.Name.c_str(), "RB_Rope" ) == RawRigidBody.Name.c_str() )
        RigidBody.Flags &= ~geom::rigid_body::FLAG_WORLD_COLLISION;

    // Resolve body type.
    if( x_stricmp( RawRigidBody.Type.c_str(), "Sphere" ) == 0 )
    {
        RigidBody.Type = geom::rigid_body::TYPE_SPHERE;
    }
    else if( x_stricmp( RawRigidBody.Type.c_str(), "Cylinder" ) == 0 )
    {
        RigidBody.Type = geom::rigid_body::TYPE_CYLINDER;
    }
    else if( x_stricmp( RawRigidBody.Type.c_str(), "Box" ) == 0 )
    {
        RigidBody.Type = geom::rigid_body::TYPE_BOX;
    }
    else
    {
        RigidBody.Type = geom::rigid_body::TYPE_SPHERE;
    }

    RigidBody.CollisionMask = (u32)RawRigidBody.CollisionMask;

    // Copy degrees of freedom.
    for( s32 i = 0; i < 6; i++ )
    {
        geom::rigid_body::dof& DOF = RigidBody.DOF[i];
        const geometry_rigid_body::dof& RawDOF = RawRigidBody.DOF[i];

        DOF.Flags = 0;

        if( RawDOF.bActive )
            DOF.Flags |= geom::rigid_body::dof::FLAG_ACTIVE;

        if( RawDOF.bLimited )
            DOF.Flags |= geom::rigid_body::dof::FLAG_LIMITED;

        DOF.Min = RawDOF.Min;
        DOF.Max = RawDOF.Max;
    }
}

//=========================================================================

static object_to_object_mapping g_RigidBodyDisableCollisionMapping[] = {
    { "Head", "Pelvis" },    { "Head", "L_Thigh" },   { "Head", "R_Thigh" },  { "Head", "L_Foot" },
    { "Head", "R_Foot" },    { "Head", "L_Calf" },    { "Head", "R_Calf" },

    { "Torso", "L_Thigh" },  { "Torso", "R_Thigh" },  { "Torso", "L_Calf" },  { "Torso", "R_Calf" },
    { "Torso", "L_Foot" },   { "Torso", "R_Foot" },

    { "Pelvis", "L_Calf" },  { "Pelvis", "R_Calf" },  { "Pelvis", "L_Foot" }, { "Pelvis", "R_Foot" },

    { "L_Thigh", "L_Foot" }, { "R_Thigh", "R_Foot" },
};

//=========================================================================

void geometry_product_writer::BuildRigidBodies( geom& Geom, const geometry_asset& GeomRawMesh,
                                                const geometry_asset& PhysicsRawMesh )
{
    s32 i, j, nMaps;

    // Get the rigid-body count.
    s32 nRigidBodies = (s32)PhysicsRawMesh.Physics.RigidBodies.size();
    if( !nRigidBodies )
        return;

    // Create rigid bodies.
    Geom.m_nRigidBodies = nRigidBodies;
    Geom.m_pRigidBodies = new geom::rigid_body[nRigidBodies];
    if( Geom.m_pRigidBodies == NULL )
        x_throw( "Out of memory" );

    // Build rigid bodies.
    for( i = 0; i < nRigidBodies; i++ )
    {
        BuildRigidBody( Geom.m_pRigidBodies[i], PhysicsRawMesh.Physics.RigidBodies[i] );
    }

    // Resolve rigid-body-to-bone mappings.
    for( i = 0; i < nRigidBodies; i++ )
    {
        // Check mapping aliases.
        nMaps = sizeof( g_BoneToRigidBodyMapping ) / sizeof( object_to_object_mapping );
        for( j = 0; j < nMaps; j++ )
        {
            // Match the rigid-body name.
            if( x_stristr( PhysicsRawMesh.Physics.RigidBodies[i].Name.c_str(), g_BoneToRigidBodyMapping[j].pObjectB ) )
            {
                // Keep an explicit mapping when present.
                geom::rigid_body& Body = Geom.m_pRigidBodies[i];
                if( Body.iBone == -1 )
                    Body.iBone = GeomRawMesh.GetBoneIDFromName( g_BoneToRigidBodyMapping[j].pObjectA, TRUE );
            }
        }
    }

    // Fill unresolved mappings.
    for( i = 0; i < nRigidBodies; i++ )
    {
        geom::rigid_body& Body = Geom.m_pRigidBodies[i];
        if( Body.iBone == -1 )
        {
            // Try matching bone names.
            const char* pBodyName = PhysicsRawMesh.Physics.RigidBodies[i].Name.c_str();
            while( ( Body.iBone == -1 ) && ( pBodyName[0] ) )
            {
                // Try the current name suffix.
                Body.iBone = GeomRawMesh.GetBoneIDFromName( pBodyName, TRUE );

                // Advance through the name.
                pBodyName++;
            }

            // Fall back to the root bone.
            if( Body.iBone == -1 )
            {
                Body.iBone = 0;

                ReportWarning( xfs( "RigidBody [%s] -> Bone mapping not found, defaulting to root bone.",
                                    PhysicsRawMesh.Physics.RigidBodies[i].Name.c_str() ) );
            }
        }
    }

    // Disable parent-child collisions.
    for( i = 0; i < nRigidBodies; i++ )
    {
        s32 iParentBody = Geom.m_pRigidBodies[i].iParentBody;
        if( iParentBody != -1 )
        {
            // Remove both collision pairs.
            Geom.m_pRigidBodies[i].CollisionMask &= ~( 1 << iParentBody );
            Geom.m_pRigidBodies[iParentBody].CollisionMask &= ~( 1 << i );
        }
    }

    // Disable configured collision pairs.
    nMaps = sizeof( g_RigidBodyDisableCollisionMapping ) / sizeof( object_to_object_mapping );
    for( i = 0; i < nMaps; i++ )
    {
        s32 iBodyA = PhysicsRawMesh.GetRigidBodyIDFromName( g_RigidBodyDisableCollisionMapping[i].pObjectA, TRUE );
        s32 iBodyB = PhysicsRawMesh.GetRigidBodyIDFromName( g_RigidBodyDisableCollisionMapping[i].pObjectB, TRUE );

        if( ( iBodyA != -1 ) && ( iBodyB != -1 ) )
        {
            Geom.m_pRigidBodies[iBodyA].CollisionMask &= ~( 1 << iBodyB );
            Geom.m_pRigidBodies[iBodyB].CollisionMask &= ~( 1 << iBodyA );
        }
    }
}

//=========================================================================

void geometry_product_writer::BuildSettings( geom& Geom, const geometry_asset& GeomRawMesh )
{
    Geom.m_nBoneMasks = 0;
    Geom.m_pBoneMasks = NULL;
    Geom.m_nPropertySections = 0;
    Geom.m_pPropertySections = NULL;
    Geom.m_nProperties = 0;
    Geom.m_pProperties = NULL;

    const s32 nMasks = (s32)GeomRawMesh.EngineMetadata.BoneMasks.size();
    if( nMasks > 0 )
    {
        Geom.m_nBoneMasks = nMasks;
        Geom.m_pBoneMasks = new geom::bone_masks[nMasks];
        for( s32 iMask = 0; iMask < nMasks; ++iMask )
        {
            const geometry_bone_mask& Source = GeomRawMesh.EngineMetadata.BoneMasks[iMask];
            geom::bone_masks& Dest = Geom.m_pBoneMasks[iMask];
            Dest.NameOffset = Context().m_Dictionary.Add( Source.Name.c_str() );
            Dest.nBones = 0;
            for( s32 iBone = 0; iBone < resource_tool::skeleton_contract::MaxBones; ++iBone )
                Dest.Weights[iBone] = 0.0f;
            for( s32 iWeight = 0; iWeight < (s32)Source.Weights.size(); ++iWeight )
            {
                const geometry_bone_mask_weight& Weight = Source.Weights[iWeight];
                const s32 iBone =
                    Weight.iBone >= 0 ? Weight.iBone : GeomRawMesh.GetBoneIDFromName( Weight.BoneName.c_str() );
                if( iBone < 0 || iBone >= (s32)GeomRawMesh.Skeleton.Bones.size() ||
                    iBone >= resource_tool::skeleton_contract::MaxBones )
                {
                    ReportWarning( xfs( "Bone mask [%s] references invalid bone %d", Source.Name.c_str(), iBone ) );
                    continue;
                }
                Dest.Weights[iBone] = Weight.Value;
                Dest.nBones = x_max( Dest.nBones, iBone + 1 );
            }
        }
    }

    const s32 nProperties = (s32)GeomRawMesh.EngineMetadata.Properties.size();
    if( nProperties <= 0 )
        return;

    struct section_info
    {
        xstring Name;
        s32 iProperty;
        s32 nProperties;
    };
    xarray<section_info> Sections;
    for( s32 iProperty = 0; iProperty < nProperties; ++iProperty )
    {
        const geometry_engine_property& Source = GeomRawMesh.EngineMetadata.Properties[iProperty];
        s32 iSection = -1;
        for( s32 i = 0; i < Sections.GetCount(); ++i )
        {
            if( !x_stricmp( Sections[i].Name, Source.Section.c_str() ) )
            {
                iSection = i;
                break;
            }
        }
        if( iSection == -1 )
        {
            section_info& Section = Sections.Append();
            Section.Name = Source.Section.c_str();
            Section.iProperty = iProperty;
            Section.nProperties = 0;
            iSection = Sections.GetCount() - 1;
        }
        Sections[iSection].nProperties++;
    }

    Geom.m_nPropertySections = Sections.GetCount();
    Geom.m_pPropertySections = new geom::property_section[Geom.m_nPropertySections];
    Geom.m_nProperties = nProperties;
    Geom.m_pProperties = new geom::property[nProperties];

    for( s32 iSection = 0; iSection < Sections.GetCount(); ++iSection )
    {
        Geom.m_pPropertySections[iSection].NameOffset = Context().m_Dictionary.Add( Sections[iSection].Name );
        Geom.m_pPropertySections[iSection].iProperty = Sections[iSection].iProperty;
        Geom.m_pPropertySections[iSection].nProperties = Sections[iSection].nProperties;
    }

    for( s32 iProperty = 0; iProperty < nProperties; ++iProperty )
    {
        const geometry_engine_property& Source = GeomRawMesh.EngineMetadata.Properties[iProperty];
        geom::property& Dest = Geom.m_pProperties[iProperty];
        Dest.NameOffset = Context().m_Dictionary.Add( Source.Name.c_str() );
        if( !x_stricmp( Source.Type.c_str(), "FLOAT" ) )
        {
            Dest.Type = geom::property::TYPE_FLOAT;
            Dest.Value.Float = x_atof( Source.Value.c_str() );
        }
        else if( !x_stricmp( Source.Type.c_str(), "INTEGER" ) )
        {
            Dest.Type = geom::property::TYPE_INTEGER;
            Dest.Value.Integer = x_atoi( Source.Value.c_str() );
        }
        else if( !x_stricmp( Source.Type.c_str(), "ANGLE" ) )
        {
            Dest.Type = geom::property::TYPE_ANGLE;
            Dest.Value.Float = DEG_TO_RAD( x_atof( Source.Value.c_str() ) );
        }
        else if( !x_stricmp( Source.Type.c_str(), "STRING" ) )
        {
            Dest.Type = geom::property::TYPE_STRING;
            Dest.Value.StringOffset = Context().m_Dictionary.Add( Source.Value.c_str() );
        }
        else
        {
            ThrowError( xfs( "Unknown property type %s", Source.Type.c_str() ) );
        }
    }
}

//=========================================================================
