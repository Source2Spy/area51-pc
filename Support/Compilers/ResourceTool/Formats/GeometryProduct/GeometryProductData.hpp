//=========================================================================
//
//  GeometryProductData.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_DATA_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_DATA_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "x_math.hpp"

#include <string>
#include <vector>

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

struct geometry_product_data
{
    struct bone
    {
        quaternion BindRotation;
        vector3 BindPosition;
        bbox BBox;
        s16 HitLocation = 0;
        s16 iRigidBody = -1;
    };

    struct bone_masks
    {
        std::string Name;
        s32 nBones = 0;
        std::vector<f32> Weights;
    };

    struct property_section
    {
        std::string Name;
        s32 iProperty = 0;
        s32 nProperties = 0;
    };

    struct property
    {
        enum
        {
            TYPE_FLOAT = 0,
            TYPE_INTEGER,
            TYPE_ANGLE,
            TYPE_STRING,
        };

        std::string Name;
        s32 Type = TYPE_FLOAT;
        f32 Float = 0.0f;
        s32 Integer = 0;
        radian Angle = 0.0f;
        std::string String;
    };

    struct rigid_body
    {
        struct dof
        {
            enum
            {
                FLAG_ACTIVE = ( 1 << 0 ),
                FLAG_LIMITED = ( 1 << 1 ),
            };

            u32 Flags = 0;
            f32 Min = 0.0f;
            f32 Max = 0.0f;
        };

        enum
        {
            TYPE_SPHERE,
            TYPE_CYLINDER,
            TYPE_BOX,
        };

        quaternion BodyBindRotation;
        vector3 BodyBindPosition;
        quaternion PivotBindRotation;
        vector3 PivotBindPosition;
        std::string Name;
        f32 Mass = 0.0f;
        f32 Radius = 0.0f;
        f32 Width = 0.0f;
        f32 Height = 0.0f;
        f32 Length = 0.0f;
        s16 Type = TYPE_SPHERE;
        u16 Flags = 0;
        s16 iParentBody = -1;
        s16 iBone = -1;
        u32 CollisionMask = 0;
        dof DOF[6];
    };

    struct mesh
    {
        bbox BBox;
        std::string Name;
        s32 nSubMeshes = 0;
        s32 iSubMesh = 0;
        s32 nBones = 0;
        s32 nFaces = 0;
        s32 nVertices = 0;
    };

    struct submesh
    {
        s32 iSection = 0;
        s32 nSections = 0;
        s32 iMaterial = -1;
        f32 WorldPixelSize = 0.0f;
    };

    struct material
    {
        struct uvanim
        {
            enum
            {
                FIXED = 0,
                LOOPED,
                PINGPONG,
                ONESHOT,
            };

            s8 Type = FIXED;
            s8 StartFrame = 0;
            s8 FPS = 0;
            s32 nKeys = 0;
            s32 iKey = 0;
        };

        enum
        {
            FLAG_DOUBLE_SIDED = ( 1 << 0 ),
            FLAG_HAS_ENV_MAP = ( 1 << 1 ),
            FLAG_HAS_DETAIL_MAP = ( 1 << 2 ),
            FLAG_ENV_WORLD_SPACE = ( 1 << 3 ),
            FLAG_ENV_VIEW_SPACE = ( 1 << 4 ),
            FLAG_ENV_CUBE_MAP = ( 1 << 5 ),
            FLAG_FORCE_ZFILL = ( 1 << 6 ),
            FLAG_ILLUM_USES_DIFFUSE = ( 1 << 7 ),
            FLAG_IS_PUNCH_THRU = ( 1 << 8 ),
            FLAG_IS_ADDITIVE = ( 1 << 9 ),
            FLAG_IS_SUBTRACTIVE = ( 1 << 10 ),
        };

        uvanim UVAnim;
        f32 DetailScale = 1.0f;
        f32 FixedAlpha = 1.0f;
        u16 Flags = 0;
        s8 Type = 0;
        s32 nTextures = 0;
        s32 iTexture = -1;
        s32 nVirtualMats = 0;
        s32 iVirtualMat = 0;
    };

    struct texture
    {
        std::string Desc;
        std::string FileName;
    };

    struct uvkey
    {
        u8 OffsetU = 0;
        u8 OffsetV = 0;
    };

    struct virtual_mesh
    {
        std::string Name;
        s32 nLODs = 0;
        s32 iLOD = 0;
    };

    struct virtual_texture
    {
        std::string Name;
        u32 MaterialMask = 0;
    };

    bbox BBox;
    s32 FaceCount = 0;
    s32 VertexCount = 0;
    std::vector<bone> Bones;
    std::vector<bone_masks> BoneMasks;
    std::vector<property_section> PropertySections;
    std::vector<property> Properties;
    std::vector<rigid_body> RigidBodies;
    std::vector<mesh> Meshes;
    std::vector<submesh> SubMeshes;
    std::vector<material> Materials;
    std::vector<texture> Textures;
    std::vector<uvkey> UVKeys;
    std::vector<u16> LODSizes;
    std::vector<u64> LODMasks;
    std::vector<virtual_mesh> VirtualMeshes;
    std::vector<virtual_texture> VirtualTextures;

    // Keep decoded counts explicit for range validation.
    s32 BoneCount = 0;
    s32 BoneMaskCount = 0;
    s32 PropertySectionCount = 0;
    s32 PropertyCount = 0;
    s32 RigidBodyCount = 0;
    s32 MeshCount = 0;
    s32 SubMeshCount = 0;
    s32 MaterialCount = 0;
    s32 TextureCount = 0;
    s32 UVKeyCount = 0;
    s32 LODCount = 0;
    s32 VirtualMaterialCount = 0;
    s32 VirtualMeshCount = 0;
    s32 VirtualTextureCount = 0;

    const char* GetMeshName( s32 iMesh ) const;
    const char* GetVMeshName( s32 iVMesh ) const;
    const char* GetVTextureName( s32 iVTexture ) const;
    const char* GetTextureDesc( s32 iTexture ) const;
    const char* GetTextureName( s32 iTexture ) const;
    const char* GetRigidBodyName( s32 iRigidBody ) const;
};

//-------------------------------------------------------------------------

struct geometry_product_collision_mat_info
{
    u16 SoundType = 0;
    u16 Flags = 0;
};

//-------------------------------------------------------------------------

struct geometry_product_collision_data
{
    struct high_cluster
    {
        bbox BBox;
        s32 nTris = 0;
        s32 iMesh = -1;
        s32 iBone = -1;
        s32 iSection = -1;
        s32 iOffset = 0;
        geometry_product_collision_mat_info MaterialInfo;
    };

    struct low_quad
    {
        u8 iP[4] = { 0, 0, 0, 0 };
        u8 iN = 0;
        u8 Flags = 0;
    };

    struct low_cluster
    {
        bbox BBox;
        s32 iVectorOffset = 0;
        s32 nPoints = 0;
        s32 nNormals = 0;
        s32 iQuadOffset = 0;
        s32 nQuads = 0;
        s32 iMesh = -1;
        s32 iBone = -1;
    };

    bbox BBox;
    std::vector<high_cluster> HighClusters;
    std::vector<u16> HighIndexToVert0;
    std::vector<low_cluster> LowClusters;
    std::vector<vector3> LowVectors;
    std::vector<low_quad> LowQuads;
};

//-------------------------------------------------------------------------

struct geometry_product_rigid_data : geometry_product_data
{
    struct vertex
    {
        vector3 Pos;
        vector3 Normal;
        vector2 UV;
    };

    struct section
    {
        s32 FirstVertex = 0;
        s32 nVertices = 0;
        s32 FirstIndex = 0;
        s32 nIndices = 0;
        s32 iBone = -1;
        s32 iColor = -1;
    };

    geometry_product_collision_data Collision;
    std::vector<section> Sections;
    std::vector<u32> Indices;
    std::vector<vertex> Vertices;
    s32 SectionCount = 0;
    s32 IndexCount = 0;
    s32 VertexDataCount = 0;
};

//-------------------------------------------------------------------------

struct geometry_product_skin_data : geometry_product_data
{
    struct vertex
    {
        vector3 Position;
        vector3 Normal;
        vector2 UV;
        vector2 Weights;
        u16 Bones[2] = { 0, 0 };
    };

    struct section
    {
        s32 FirstVertex = 0;
        s32 nVertices = 0;
        s32 FirstIndex = 0;
        s32 nIndices = 0;
        s32 FirstBone = -1;
        s32 nBones = 0;
    };

    std::vector<section> Sections;
    std::vector<u32> Indices;
    std::vector<vertex> Vertices;
    std::vector<u16> BonePalette;
    s32 SectionCount = 0;
    s32 IndexCount = 0;
    s32 VertexDataCount = 0;
    s32 BonePaletteCount = 0;
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_DATA_HPP
//=========================================================================
