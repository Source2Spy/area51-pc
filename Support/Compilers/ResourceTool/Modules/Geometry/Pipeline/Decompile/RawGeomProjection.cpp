//=========================================================================
//
//  RawGeomProjection.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RawGeomProjection.hpp"

#include <filesystem>
#include <set>
#include <tuple>

namespace resource_tool::geometry
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

const char* MaterialName( const geometry_asset& Model, s32 iMaterial )
{
    if( iMaterial >= 0 && iMaterial < (s32)Model.Materials.Items.size() &&
        !Model.Materials.Items[iMaterial].Name.empty() )
    {
        return Model.Materials.Items[iMaterial].Name.c_str();
    }
    return (const char*)xfs( "Material_%d", iMaterial );
}

//=========================================================================

std::string DescriptorTexturePath( const std::string& FileName, const texture_export_map* pTextureMap )
{
    if( pTextureMap )
    {
        const texture_export_entry* pEntry = pTextureMap->Find( FileName );
        if( pEntry )
            return pEntry->RawGeomPath;
    }
    const std::filesystem::path Reference( FileName );
    if( Reference.is_absolute() )
        return Reference.generic_string();
    return FileName;
}

//=========================================================================

void CopyUVAnimations( const geometry_asset& Model, geometry_descriptor& Settings )
{
    for( const geometry_material& Material : Model.Materials.Items )
    {
        if( Material.UVAnimation.Keys.empty() )
            continue;
        geometry_descriptor::uv_animation& Output = Settings.AppendUVAnimation();
        x_strsavecpy( Output.MatName, Material.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.AnimType = Material.UVAnimation.Type;
        Output.StartFrame = Material.UVAnimation.StartFrame;
        Output.FPS = Material.UVAnimation.FPS;
        Output.nFrames = (s32)Material.UVAnimation.Keys.size();
        Output.Keys.SetCount( Output.nFrames );
        for( s32 iKey = 0; iKey < Output.nFrames; ++iKey )
        {
            Output.Keys[iKey].U = Material.UVAnimation.Keys[iKey].U;
            Output.Keys[iKey].V = Material.UVAnimation.Keys[iKey].V;
        }
    }
}

//=========================================================================

void CopyVirtualMeshes( const geometry_asset& Model, geometry_descriptor& Settings )
{
    for( const geometry_lod_group& Source : Model.LODs.Groups )
    {
        geometry_descriptor::virtual_mesh& Output = Settings.AppendVirtualMesh();
        x_strsavecpy( Output.Name, Source.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        for( const geometry_lod_level& SourceLOD : Source.Levels )
        {
            geometry_descriptor::lod_info& LOD = Output.LODs.Append();
            LOD.ScreenSize = SourceLOD.ScreenSize;
            for( const std::string& MeshName : SourceLOD.MeshNames )
            {
                if( LOD.nMeshes >= geometry_descriptor::lod_info::MAX_LOD_MESHES )
                    break;
                x_strsavecpy( LOD.MeshName[LOD.nMeshes++], MeshName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
            }
        }
    }
}

//=========================================================================

void CopyVirtualTextures( const geometry_asset& Model, geometry_descriptor& Settings,
                          const texture_export_map* pTextureMap )
{
    for( const geometry_virtual_texture& Source : Model.Materials.VirtualTextures )
    {
        geometry_descriptor::virtual_texture& Output = Settings.AppendVirtualTexture();
        x_strsavecpy( Output.Name, Source.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.OverrideDefault = Source.OverrideDefault;
        x_strsavecpy( Output.OverrideFileName, Source.OverrideFileName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.Textures.Clear();
        for( const geometry_texture& SourceTexture : Source.Textures )
        {
            geometry_descriptor::texture_info& Texture = Output.Textures.Append();
            x_strsavecpy( Texture.FileName, DescriptorTexturePath( SourceTexture.FileName, pTextureMap ).c_str(),
                          geometry_descriptor::MAX_NAME_LENGTH );
            x_strsavecpy( Texture.Name, SourceTexture.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        }
        if( Output.Textures.GetCount() == 0 )
            Output.Textures.Append();
    }
}

//=========================================================================

void CopyMaterials( const geometry_asset& Model, geometry_descriptor& Settings, const texture_export_map* pTextureMap )
{
    for( const geometry_material& Source : Model.Materials.Items )
    {
        geometry_descriptor::material& Output = Settings.AppendMaterial();
        x_strsavecpy( Output.Name, Source.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.Type = Source.Type;
        Output.DetailScale = Source.DetailScale;
        Output.FixedAlpha = Source.FixedAlpha;
        Output.Flags = Source.ProductFlags;
        for( const geometry_texture& SourceTexture : Source.Textures )
        {
            geometry_descriptor::texture& Texture = Output.Textures.Append();
            Texture.Slot = SourceTexture.Slot;
            x_strsavecpy( Texture.FileName, DescriptorTexturePath( SourceTexture.FileName, pTextureMap ).c_str(),
                          geometry_descriptor::MAX_NAME_LENGTH );
            x_strsavecpy( Texture.Name, SourceTexture.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        }
    }
}

//=========================================================================

void CopySubMeshes( const geometry_asset& Model, geometry_descriptor& Settings )
{
    std::set<std::tuple<s32, s32, s32>> Seen;
    for( const geometry_facet& Source : Model.Render.Facets )
    {
        const std::tuple<s32, s32, s32> Key( Source.iMesh, Source.iSubMesh, Source.iSection );
        if( !Seen.insert( Key ).second )
            continue;

        geometry_descriptor::submesh& Output = Settings.AppendSubMesh();
        x_strsavecpy( Output.MeshName, Source.MeshName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        x_strsavecpy( Output.SubMeshName, Source.SubMeshName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        x_strsavecpy( Output.SectionName, Source.SectionName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.Primitive = Source.iPrimitive;
        Output.SubMesh = Source.iSubMesh;
        Output.Section = Source.iSection;
        x_strsavecpy( Output.MaterialName,
                      Source.MaterialName.empty() ? MaterialName( Model, Source.iMaterial )
                                                  : Source.MaterialName.c_str(),
                      geometry_descriptor::MAX_NAME_LENGTH );
        Output.WorldPixelSize = Source.WorldPixelSize;
    }
}

//=========================================================================

void CopyBones( const geometry_asset& Model, geometry_descriptor& Settings )
{
    for( const geometry_bone& Source : Model.Skeleton.Bones )
    {
        geometry_descriptor::bone& Output = Settings.AppendBone();
        x_strsavecpy( Output.Name, Source.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.Parent = Source.iParent;
        Output.BBox = Source.BBox;
        Output.HitLocation = Source.HitLocation;
        Output.iRigidBody = Source.iRigidBody;
        x_strsavecpy( Output.RigidBodyName, Source.RigidBodyName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
    }
}

//=========================================================================

const char* BodyTypeName( const geometry_rigid_body& Body )
{
    return Body.Type.empty() ? "Sphere" : Body.Type.c_str();
}

//=========================================================================

void CopyRigidBodies( const geometry_asset& Model, geometry_descriptor& Settings )
{
    for( const geometry_rigid_body& Source : Model.Physics.RigidBodies )
    {
        geometry_descriptor::rigid_body& Output = Settings.AppendRigidBody();
        x_strsavecpy( Output.Name, Source.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        x_strsavecpy( Output.Type, BodyTypeName( Source ), geometry_descriptor::MAX_NAME_LENGTH );
        Output.Mass = Source.Mass;
        Output.Parent = Source.iParent;
        x_strsavecpy( Output.ParentName, Source.ParentName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.Bone = Source.iBone;
        x_strsavecpy( Output.BoneName, Source.BoneName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.PivotPosition = Source.PivotPosition;
        Output.PivotScale = Source.PivotScale;
        Output.PivotRotation = Source.PivotRotation;
        Output.BodyPosition = Source.BodyPosition;
        Output.BodyScale = Source.BodyScale;
        Output.BodyRotation = Source.BodyRotation;
        Output.Radius = Source.Radius;
        Output.Width = Source.Width;
        Output.Height = Source.Height;
        Output.Length = Source.Length;
        Output.Flags = Source.Flags;
        Output.CollisionMask = Source.CollisionMask;
        for( s32 iDOF = 0; iDOF < 6; ++iDOF )
        {
            Output.DOF[iDOF].Active = Source.DOF[iDOF].bActive;
            Output.DOF[iDOF].Limited = Source.DOF[iDOF].bLimited;
            Output.DOF[iDOF].Min = Source.DOF[iDOF].Min;
            Output.DOF[iDOF].Max = Source.DOF[iDOF].Max;
        }
    }
}

//=========================================================================

void CopyProductSettings( const geometry_asset& Model, geometry_descriptor& Settings )
{
    for( const geometry_bone_mask& Source : Model.EngineMetadata.BoneMasks )
    {
        geometry_descriptor::bone_mask& Output = Settings.AppendBoneMask();
        x_strsavecpy( Output.Name, Source.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        for( const geometry_bone_mask_weight& SourceWeight : Source.Weights )
        {
            geometry_descriptor::bone_mask::weight& Weight = Output.Weights.Append();
            Weight.Bone = SourceWeight.iBone;
            Weight.Value = SourceWeight.Value;
        }
    }
    for( const geometry_engine_property& Source : Model.EngineMetadata.Properties )
    {
        geometry_descriptor::property& Output = Settings.AppendProperty();
        x_strsavecpy( Output.Section, Source.Section.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        x_strsavecpy( Output.Name, Source.Name.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        x_strsavecpy( Output.Type, Source.Type.c_str(), sizeof( Output.Type ) );
        x_strsavecpy( Output.Value, Source.Value.c_str(), sizeof( Output.Value ) );
    }
}

//=========================================================================

void CopyCollisionMetadata( const geometry_asset& Model, geometry_descriptor& Settings )
{
    if( Model.Collision.HasCollisionBBox )
        Settings.SetCollisionBBox( Model.Collision.CollisionBBox );
    for( const geometry_collision_cluster& Source : Model.Collision.Clusters )
    {
        geometry_descriptor::collision_cluster& Output = Settings.AppendCollisionCluster();
        Output.High = Source.High;
        Output.BBox = Source.BBox;
        x_strsavecpy( Output.MeshName, Source.MeshName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        x_strsavecpy( Output.BoneName, Source.BoneName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        x_strsavecpy( Output.SectionName, Source.SectionName.c_str(), geometry_descriptor::MAX_NAME_LENGTH );
        Output.Mesh = Source.Mesh;
        Output.Bone = Source.Bone;
        Output.Section = Source.Section;
        Output.Offset = Source.Offset;
        Output.Count = Source.Count;
        Output.VectorOffset = Source.VectorOffset;
        Output.Points = Source.Points;
        Output.Normals = Source.Normals;
        Output.QuadOffset = Source.QuadOffset;
        Output.SoundType = Source.SoundType;
        Output.Flags = Source.Flags;
        for( s32 Value : Source.TriangleOffsets )
            Output.TriangleOffsets.Append( Value );
        for( s32 Value : Source.QuadFlags )
            Output.QuadFlags.Append( Value );
    }
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void ProjectProductToDescriptor( geometry_descriptor::geometry_type Type, const geometry_asset& Model,
                                 geometry_descriptor& Settings, const texture_export_map* pTextureMap )
{
    if( Type != geometry_descriptor::GEOMETRY_TYPE_RIGID && Type != geometry_descriptor::GEOMETRY_TYPE_SKIN )
    {
        x_throw( "Geometry type must be Rigid or Skin for descriptor generation" );
    }

    CopyVirtualMeshes( Model, Settings );
    CopyUVAnimations( Model, Settings );
    CopyVirtualTextures( Model, Settings, pTextureMap );
    CopyMaterials( Model, Settings, pTextureMap );
    CopySubMeshes( Model, Settings );
    CopyBones( Model, Settings );
    CopyRigidBodies( Model, Settings );
    CopyProductSettings( Model, Settings );
    if( Type == geometry_descriptor::GEOMETRY_TYPE_RIGID )
        CopyCollisionMetadata( Model, Settings );
}

} // namespace resource_tool::geometry
