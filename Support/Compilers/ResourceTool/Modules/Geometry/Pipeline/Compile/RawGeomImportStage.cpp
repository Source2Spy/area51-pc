//=========================================================================
//
//  RawGeomImportStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RawGeomImportStage.hpp"

#include "Diagnostics.hpp"
#include "Model/GeometryNames.hpp"
#include "Render/Material_Prefs.hpp"

#include <filesystem>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

void ImportUVAnimations( const geometry_descriptor& Settings, geometry_asset& Asset )
{
    for( s32 iAnimation = 0; iAnimation < Settings.GetUVAnimationCount(); ++iAnimation )
    {
        const geometry_descriptor::uv_animation& Source = Settings.GetUVAnimation( iAnimation );
        if( Source.Keys.GetCount() == 0 )
            continue;

        s32 iMaterial = -1;
        for( s32 i = 0; i < (s32)Asset.Materials.Items.size(); ++i )
        {
            if( !x_stricmp( Asset.Materials.Items[i].Name.c_str(), Source.MatName ) )
            {
                iMaterial = i;
                break;
            }
        }
        if( iMaterial < 0 )
        {
            x_throw( xfs( "UV animation references missing material [%s]", Source.MatName ) );
        }

        geometry_material& Material = Asset.Materials.Items[iMaterial];
        Material.UVAnimation.Type = Source.AnimType;
        Material.UVAnimation.StartFrame = Source.StartFrame;
        Material.UVAnimation.FPS = Source.FPS;
        Material.UVAnimation.Keys.clear();
        Material.UVAnimation.Keys.reserve( Source.Keys.GetCount() );
        for( s32 iKey = 0; iKey < Source.Keys.GetCount(); ++iKey )
        {
            geometry_uv_key& Key = Material.UVAnimation.Keys.emplace_back();
            Key.U = Source.Keys[iKey].U;
            Key.V = Source.Keys[iKey].V;
        }
    }
}

//=========================================================================

std::filesystem::path ResolveTexture( const geometry_asset& Asset, const geometry_descriptor::texture& Source )
{
    std::filesystem::path TexturePath( Source.FileName );
    if( !TexturePath.is_absolute() )
    {
        TexturePath = std::filesystem::path( Asset.Source.SourceFile ).parent_path() / TexturePath;
    }

    std::error_code ErrorCode;
    if( !std::filesystem::exists( TexturePath, ErrorCode ) )
    {
        const std::filesystem::path Directory = TexturePath.parent_path();
        const std::string FileName = TexturePath.filename().string();
        std::filesystem::directory_iterator It( Directory, ErrorCode );
        const std::filesystem::directory_iterator End;
        for( ; !ErrorCode && It != End; It.increment( ErrorCode ) )
        {
            if( !x_stricmp( It->path().filename().string().c_str(), FileName.c_str() ) )
            {
                TexturePath = It->path();
                break;
            }
        }
    }
    return TexturePath;
}

//=========================================================================

s32 AppendTexture( geometry_asset& Asset, const geometry_descriptor::texture& Source,
                   resource_tool::diagnostic_log& Diagnostics, const char* pSourceFile )
{
    const std::filesystem::path TexturePath = ResolveTexture( Asset, Source );
    std::error_code ErrorCode;
    if( !std::filesystem::exists( TexturePath, ErrorCode ) )
    {
        Diagnostics.Warning( xfs( "compiled material texture is unavailable; preserving URI without local product [%s]",
                                  Source.FileName ),
                             "GeometryRawGeomImport", Source.Name, pSourceFile );
        return -1;
    }

    geometry_texture Texture;
    Texture.FileName = TexturePath.generic_string();
    Asset.Materials.Textures.push_back( Texture );
    return (s32)Asset.Materials.Textures.size() - 1;
}

//=========================================================================

void ImportMaterials( const geometry_descriptor& Settings, geometry_asset& Asset,
                      resource_tool::diagnostic_log& Diagnostics, const char* pSourceFile )
{
    if( Settings.GetMaterialCount() == 0 )
        return;

    Asset.Materials.PreserveMaterialEntries = TRUE;

    enum
    {
        DETAIL_SCALE = 0,
        ENV_TYPE,
        ENV_BLEND,
        FIXED_ALPHA,
        FORCE_ZFILL,
        USE_DIFFUSE,
        ENVIRONMENT_MAP = 6,
        DETAIL_MAP = 8,
    };

    for( s32 iProduct = 0; iProduct < Settings.GetMaterialCount(); ++iProduct )
    {
        const geometry_descriptor::material& Source = Settings.GetMaterial( iProduct );
        s32 iMaterial = -1;
        for( s32 i = 0; i < (s32)Asset.Materials.Items.size(); ++i )
        {
            if( geometry_names_match( Asset.Materials.Items[i].Name.c_str(), Source.Name ) )
            {
                iMaterial = i;
                break;
            }
        }
        if( iMaterial < 0 )
        {
            x_throw( xfs( "Product material [%s] is missing from the glTF source", Source.Name ) );
        }

        geometry_material& Material = Asset.Materials.Items[iMaterial];
        Material.Type = Source.Type;
        Material.DetailScale = Source.DetailScale;
        Material.FixedAlpha = Source.FixedAlpha;
        Material.ProductFlags = Source.Flags;
        Material.Textures.clear();
        Material.bTwoSided = ( Source.Flags & MATERIAL_FLAG_DOUBLE_SIDED ) != 0;
        Material.bPunchthrough = ( Source.Flags & MATERIAL_FLAG_IS_PUNCH_THRU ) != 0;
        Material.Constants[DETAIL_SCALE].Current[0] = Source.DetailScale;
        Material.Constants[FIXED_ALPHA].Current[0] = Source.FixedAlpha;
        Material.Constants[FORCE_ZFILL].Current[0] = ( Source.Flags & MATERIAL_FLAG_FORCE_ZFILL ) ? 1.0f : 0.0f;
        Material.Constants[USE_DIFFUSE].Current[0] = ( Source.Flags & MATERIAL_FLAG_ILLUM_USES_DIFFUSE ) ? 1.0f : 0.0f;
        Material.Constants[ENV_TYPE].Current[0] = ( Source.Flags & MATERIAL_FLAG_ENV_CUBE_MAP )     ? 0.0f
                                                  : ( Source.Flags & MATERIAL_FLAG_ENV_VIEW_SPACE ) ? 1.0f
                                                                                                    : 2.0f;
        Material.Constants[ENV_BLEND].Current[0] = ( Source.Flags & MATERIAL_FLAG_IS_ADDITIVE )      ? 1.0f
                                                   : ( Source.Flags & MATERIAL_FLAG_IS_SUBTRACTIVE ) ? 2.0f
                                                                                                     : 0.0f;

        for( s32 iMap = 0; iMap < GEOMETRY_MATERIAL_MAX_MAPS; ++iMap )
        {
            Material.Map[iMap].iTexture = -1;
            Material.Map[iMap].nTextures = 0;
        }

        for( s32 iTexture = 0; iTexture < Source.Textures.GetCount(); ++iTexture )
        {
            const geometry_descriptor::texture& Texture = Source.Textures[iTexture];
            geometry_texture& ProductTexture = Material.Textures.emplace_back();
            ProductTexture.Slot = Texture.Slot;
            ProductTexture.FileName = Texture.FileName;
            ProductTexture.Name = Texture.Name;

            if( Texture.Slot == geometry_descriptor::RUNTIME_TEXTURE_PUNCH_THROUGH )
                continue;

            const s32 iRawTexture = AppendTexture( Asset, Texture, Diagnostics, pSourceFile );
            if( iRawTexture < 0 )
                continue;
            if( Texture.Slot == geometry_descriptor::RUNTIME_TEXTURE_DIFFUSE )
            {
                Material.Map[0].iTexture = iRawTexture;
                Material.Map[0].nTextures = 1;
            }
            else if( Texture.Slot == geometry_descriptor::RUNTIME_TEXTURE_ENVIRONMENT )
            {
                Material.Map[ENVIRONMENT_MAP].iTexture = iRawTexture;
                Material.Map[ENVIRONMENT_MAP].nTextures = 1;
            }
            else if( Texture.Slot == geometry_descriptor::RUNTIME_TEXTURE_DETAIL )
            {
                Material.Map[DETAIL_MAP].iTexture = iRawTexture;
                Material.Map[DETAIL_MAP].nTextures = 1;
            }
        }

        Material.nMaps = 10;
    }
}

//=========================================================================

void ImportSubMeshes( const geometry_descriptor& Settings, geometry_asset& Asset )
{
    if( Settings.GetSubMeshCount() == 0 )
        return;

    for( geometry_facet& Facet : Asset.Render.Facets )
    {
        for( s32 i = 0; i < Settings.GetSubMeshCount(); ++i )
        {
            const geometry_descriptor::submesh& Source = Settings.GetSubMesh( i );
            const xbool bSameMeshName =
                Source.MeshName[0] && Facet.iMesh >= 0 && Facet.iMesh < (s32)Asset.Render.SubMeshes.size() &&
                geometry_names_match( Source.MeshName, Asset.Render.SubMeshes[Facet.iMesh].Name.c_str() );
            const xbool bSamePrimitive = Source.Primitive < 0 || Source.Primitive == Facet.iPrimitive;
            const xbool bSameSubMesh = Source.SubMeshName[0] && Facet.SubMeshName[0]
                                           ? geometry_names_match( Source.SubMeshName, Facet.SubMeshName.c_str() )
                                           : ( Source.SubMesh < 0 || Source.SubMesh == Facet.iSubMesh );
            const xbool bSameSection = Source.SectionName[0] && Facet.SectionName[0]
                                           ? geometry_names_match( Source.SectionName, Facet.SectionName.c_str() )
                                           : ( Source.Section < 0 || Source.Section == Facet.iSection );
            if( !bSameMeshName || !bSamePrimitive || !bSameSubMesh || !bSameSection )
                continue;

            Facet.iSubMesh = Source.SubMesh;
            Facet.iSection = Source.Section;
            Facet.WorldPixelSize = Source.WorldPixelSize >= 0.000001f ? Source.WorldPixelSize : 0.001f;
            s32 iMaterial = -1;
            for( s32 iMaterialCandidate = 0; iMaterialCandidate < (s32)Asset.Materials.Items.size();
                 ++iMaterialCandidate )
            {
                if( geometry_names_match( Asset.Materials.Items[iMaterialCandidate].Name.c_str(),
                                          Source.MaterialName ) )
                {
                    iMaterial = iMaterialCandidate;
                    break;
                }
            }
            if( iMaterial < 0 )
            {
                x_throw( xfs( "Product submesh refers to missing material [%s]", Source.MaterialName ) );
            }
            Facet.iMaterial = iMaterial;
            break;
        }
    }
}

//=========================================================================

void ImportDescriptorSettings( const geometry_descriptor& Settings, geometry_asset& Asset )
{
    Asset.EngineMetadata.Clear();
    Asset.Collision.Clusters.clear();
    Asset.Collision.HasCollisionBBox = Settings.HasCollisionBBox();
    if( Asset.Collision.HasCollisionBBox )
        Asset.Collision.CollisionBBox = Settings.GetCollisionBBox();

    for( s32 iMask = 0; iMask < Settings.GetBoneMaskCount(); ++iMask )
    {
        const geometry_descriptor::bone_mask& Source = Settings.GetBoneMask( iMask );
        geometry_bone_mask& Destination = Asset.EngineMetadata.BoneMasks.emplace_back();
        Destination.Name = Source.Name;
        for( s32 iWeight = 0; iWeight < Source.Weights.GetCount(); ++iWeight )
        {
            const geometry_descriptor::bone_mask::weight& SourceWeight = Source.Weights[iWeight];
            geometry_bone_mask_weight& DestinationWeight = Destination.Weights.emplace_back();
            DestinationWeight.iBone = SourceWeight.Bone;
            DestinationWeight.Value = SourceWeight.Value;
            if( SourceWeight.Bone >= 0 && SourceWeight.Bone < (s32)Asset.Skeleton.Bones.size() )
            {
                DestinationWeight.BoneName = Asset.Skeleton.Bones[SourceWeight.Bone].Name;
            }
        }
    }

    for( s32 iProperty = 0; iProperty < Settings.GetPropertyCount(); ++iProperty )
    {
        const geometry_descriptor::property& Source = Settings.GetProperty( iProperty );
        geometry_engine_property& Destination = Asset.EngineMetadata.Properties.emplace_back();
        Destination.Section = Source.Section;
        Destination.Name = Source.Name;
        Destination.Type = Source.Type;
        Destination.Value = Source.Value;
    }

    for( s32 iCluster = 0; iCluster < Settings.GetCollisionClusterCount(); ++iCluster )
    {
        const geometry_descriptor::collision_cluster& Source = Settings.GetCollisionCluster( iCluster );
        geometry_collision_cluster& Destination = Asset.Collision.Clusters.emplace_back();
        Destination.High = Source.High;
        Destination.BBox = Source.BBox;
        Destination.MeshName = Source.MeshName;
        Destination.BoneName = Source.BoneName;
        Destination.SectionName = Source.SectionName;
        Destination.Mesh = Source.Mesh;
        Destination.Bone = Source.Bone;
        Destination.Section = Source.Section;
        Destination.Offset = Source.Offset;
        Destination.Count = Source.Count;
        Destination.VectorOffset = Source.VectorOffset;
        Destination.Points = Source.Points;
        Destination.Normals = Source.Normals;
        Destination.QuadOffset = Source.QuadOffset;
        Destination.SoundType = Source.SoundType;
        Destination.Flags = Source.Flags;
        for( s32 i = 0; i < Source.TriangleOffsets.GetCount(); ++i )
            Destination.TriangleOffsets.push_back( Source.TriangleOffsets[i] );
        for( s32 i = 0; i < Source.QuadFlags.GetCount(); ++i )
            Destination.QuadFlags.push_back( Source.QuadFlags[i] );
    }
}

//=========================================================================

void ImportVirtualMeshes( const geometry_descriptor& Settings, geometry_asset& Asset )
{
    if( Settings.GetVirtualMeshCount() == 0 )
        return;

    Asset.LODs.Clear();
    for( s32 iVMesh = 0; iVMesh < Settings.GetVirtualMeshCount(); ++iVMesh )
    {
        const geometry_descriptor::virtual_mesh& Source = Settings.GetVirtualMesh( iVMesh );
        geometry_lod_group& Group = Asset.LODs.Groups.emplace_back();
        Group.Name = Source.Name;
        for( s32 iLOD = 0; iLOD < Source.LODs.GetCount(); ++iLOD )
        {
            const geometry_descriptor::lod_info& SourceLOD = Source.LODs[iLOD];
            geometry_lod_level& Level = Group.Levels.emplace_back();
            Level.ScreenSize = SourceLOD.ScreenSize;
            for( s32 iMesh = 0; iMesh < SourceLOD.nMeshes; ++iMesh )
                Level.MeshNames.emplace_back( SourceLOD.MeshName[iMesh] );
        }
    }
}

//=========================================================================

void ImportVirtualTextures( const geometry_descriptor& Settings, geometry_asset& Asset )
{
    if( Settings.GetVirtualTextureCount() == 0 )
        return;

    Asset.Materials.VirtualTextures.clear();
    for( s32 iTexture = 0; iTexture < Settings.GetVirtualTextureCount(); ++iTexture )
    {
        const geometry_descriptor::virtual_texture& Source = Settings.GetVirtualTexture( iTexture );
        geometry_virtual_texture& Destination = Asset.Materials.VirtualTextures.emplace_back();
        Destination.Name = Source.Name;
        Destination.OverrideDefault = Source.OverrideDefault;
        Destination.OverrideFileName = Source.OverrideFileName;
        for( s32 iMap = 0; iMap < Source.Textures.GetCount(); ++iMap )
        {
            geometry_texture& Texture = Destination.Textures.emplace_back();
            Texture.FileName = Source.Textures[iMap].FileName;
            Texture.Name = Source.Textures[iMap].Name;
        }
    }
}

//=========================================================================

xbool ImportBones( const geometry_descriptor& Settings, geometry_asset& Asset,
                   resource_tool::diagnostic_log& Diagnostics, const char* pSourceFile )
{
    if( Settings.GetBoneCount() == 0 )
        return TRUE;

    if( Asset.Skeleton.Bones.empty() )
    {
        Asset.Skeleton.Bones.resize( Settings.GetBoneCount() );
        for( s32 iBone = 0; iBone < Settings.GetBoneCount(); ++iBone )
        {
            const geometry_descriptor::bone& Source = Settings.GetBone( iBone );
            geometry_bone& Destination = Asset.Skeleton.Bones[iBone];
            Destination.Name = Source.Name;
            Destination.iParent = Source.Parent;
            Destination.ParentName = Source.Parent >= 0 && Source.Parent < Settings.GetBoneCount()
                                         ? Settings.GetBone( Source.Parent ).Name
                                         : "";
            Destination.Scale.Set( 1.0f, 1.0f, 1.0f );
            Destination.Rotation.Identity();
            Destination.Position.Zero();
            Destination.InverseBindMatrix.Identity();
        }
    }

    xbool bValid = TRUE;
    for( s32 iBoneIndex = 0; iBoneIndex < Settings.GetBoneCount(); ++iBoneIndex )
    {
        const geometry_descriptor::bone& Source = Settings.GetBone( iBoneIndex );
        const s32 iBone = Asset.GetBoneIDFromName( Source.Name );
        if( iBone < 0 )
        {
            Diagnostics.Error( xfs( "compiled bone [%s] is missing from source skeleton", Source.Name ),
                               "GeometryRawGeomImport", Source.Name, pSourceFile );
            bValid = FALSE;
            continue;
        }

        geometry_bone& Destination = Asset.Skeleton.Bones[iBone];
        Destination.BBox = Source.BBox;
        Destination.HitLocation = Source.HitLocation;
        Destination.iRigidBody = Source.iRigidBody;
        Destination.RigidBodyName = Source.RigidBodyName;
    }
    return bValid;
}

//=========================================================================

xbool ImportRigidBodies( const geometry_descriptor& Settings, const geometry_asset& BoneSource, geometry_asset& Asset )
{
    if( Settings.GetRigidBodyCount() == 0 )
        return TRUE;

    Asset.Physics.RigidBodies.clear();
    Asset.Physics.RigidBodies.resize( Settings.GetRigidBodyCount() );
    for( s32 iBody = 0; iBody < Settings.GetRigidBodyCount(); ++iBody )
    {
        const geometry_descriptor::rigid_body& Source = Settings.GetRigidBody( iBody );
        geometry_rigid_body& Destination = Asset.Physics.RigidBodies[iBody];
        Destination.Name = Source.Name;
        Destination.Type = Source.Type;
        Destination.Mass = Source.Mass;
        Destination.iParent = Source.Parent;
        Destination.iBone = Source.Bone;
        Destination.PivotPosition = Source.PivotPosition;
        Destination.PivotScale = Source.PivotScale;
        Destination.PivotRotation = Source.PivotRotation;
        Destination.BodyScale = Source.BodyScale;
        Destination.BodyRotation = Source.BodyRotation;
        Destination.BodyPosition = Source.BodyPosition;
        Destination.Radius = Source.Radius;
        Destination.Width = Source.Width;
        Destination.Height = Source.Height;
        Destination.Length = Source.Length;
        Destination.Flags = Source.Flags;
        Destination.CollisionMask = Source.CollisionMask;
        for( s32 iDOF = 0; iDOF < 6; ++iDOF )
        {
            Destination.DOF[iDOF].bActive = Source.DOF[iDOF].Active;
            Destination.DOF[iDOF].bLimited = Source.DOF[iDOF].Limited;
            Destination.DOF[iDOF].Min = Source.DOF[iDOF].Min;
            Destination.DOF[iDOF].Max = Source.DOF[iDOF].Max;
        }

        if( Source.ParentName[0] )
        {
            Destination.iParent = -1;
            for( s32 iParent = 0; iParent < Settings.GetRigidBodyCount(); ++iParent )
            {
                if( geometry_names_match( Source.ParentName, Settings.GetRigidBody( iParent ).Name ) )
                {
                    Destination.iParent = iParent;
                    break;
                }
            }
            if( Destination.iParent < 0 )
            {
                x_throw( xfs( "Rigid body [%s] references missing parent body [%s]", Source.Name, Source.ParentName ) );
            }
        }
        else if( Destination.iParent < -1 || Destination.iParent >= Settings.GetRigidBodyCount() )
        {
            x_throw( xfs( "Rigid body [%s] has invalid parent index %d", Source.Name, Destination.iParent ) );
        }

        if( Destination.iParent >= iBody )
        {
            x_throw( xfs( "Rigid body [%s] parent must appear before the child", Source.Name ) );
        }

        if( Source.BoneName[0] )
        {
            Destination.iBone = -1;
            for( s32 iBone = 0; iBone < (s32)BoneSource.Skeleton.Bones.size(); ++iBone )
            {
                if( geometry_names_match( Source.BoneName, BoneSource.Skeleton.Bones[iBone].Name.c_str() ) )
                {
                    Destination.iBone = iBone;
                    break;
                }
            }
            if( Destination.iBone < 0 )
            {
                x_throw( xfs( "Rigid body [%s] references missing bone [%s]", Source.Name, Source.BoneName ) );
            }
        }
    }
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::rawgeom_import_stage::Execute( const geometry_descriptor& Settings,
                                                              geometry_asset& Asset, diagnostic_log& Diagnostics,
                                                              const char* pSourceFile ) const
{
    ImportMaterials( Settings, Asset, Diagnostics, pSourceFile );
    ImportUVAnimations( Settings, Asset );
    ImportSubMeshes( Settings, Asset );
    ImportDescriptorSettings( Settings, Asset );
    if( !ImportRigidBodies( Settings, Asset, Asset ) )
        return FALSE;

    ImportVirtualMeshes( Settings, Asset );
    ImportVirtualTextures( Settings, Asset );
    return ImportBones( Settings, Asset, Diagnostics, pSourceFile );
}
