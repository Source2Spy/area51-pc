//=========================================================================
//
//  Geom.hpp
//
//=========================================================================

// TODO: Separate materials and textures into their own asset resource.

#ifndef GEOM_HPP
#define GEOM_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "Material_Prefs.hpp"
#include "../Animation/AnimData.hpp"

//=========================================================================
//  GEOM STRUCT
//=========================================================================

//=========================================================================
//
// A single "geom" can be made of multiple "meshs".
// This allows multiple objects to be stored in the same file.
// For example: all LOD levels, a single character object with different heads.
//
// A "mesh" represents a complete object within a geom.
// For example: an LOD or a model of a head.
//
// Each mesh is made from one or more "submeshs".
// There is a submesh for every material used by the mesh.
//
//=========================================================================

class material;

struct geom
{
    // Geometry bone
    struct bone
    {
        // Hit location enums
        enum hit_location
        {
            HIT_LOCATION_START,

            HIT_LOCATION_HEAD = HIT_LOCATION_START,
            HIT_LOCATION_SHOULDER_LEFT,
            HIT_LOCATION_SHOULDER_RIGHT,
            HIT_LOCATION_TORSO,
            HIT_LOCATION_LEGS,

            HIT_LOCATION_COUNT,

            HIT_LOCATION_UNKNOWN,
            HIT_LOCATION_UNKNOWN_WRONG_GUID,
        };

        // Data
        quaternion BindRotation; // Local space bind rotation
        vector3    BindPosition; // Local space bind position
        bbox       BBox;         // Bounding box in local space
        s16        HitLocation;  // Hit location type
        s16        iRigidBody;   // Index of rigid body to attach to (or -1 if none)
    };

    // Bone masks
    struct bone_masks
    {
        s32 NameOffset;              // Offset into string data for name
        s32 nBones;                  // # of bones referenced
        f32 Weights[MAX_ANIM_BONES]; // Weight ( 0 -> 1 ) for each bone
    };

    // Property section
    struct property_section
    {
        // Data
        s32 NameOffset;  // Offset into string data for name
        s32 iProperty;   // Index of first property in section
        s32 nProperties; // # of properties in section
    };

    // Property
    struct property
    {
        // Type defines
        enum type
        {
            TYPE_FLOAT,   // 0 = f32
            TYPE_INTEGER, // 1 = s32
            TYPE_ANGLE,   // 2 = radian
            TYPE_STRING,  // 3 = char*

            TYPE_TOTAL // Total count
        };

        // Data
        s32 NameOffset; // Offset into string data for name
        s32 Type;       // Type of property
        union
        {
            f32    Float;        // Float value
            s32    Integer;      // Integer value
            radian Angle;        // Angle value
            s32    StringOffset; // Offset into string data of string
        } Value;
    };

    // Rigid body
    struct rigid_body
    {
        // IK Degrees of freedom info
        struct dof
        {
            // Axis
            enum axis
            {
                DOF_TX, // X translation
                DOF_TY, // Y translation
                DOF_TZ, // Z translation

                DOF_RX, // X rotation
                DOF_RY, // Y rotation
                DOF_RZ, // Z rotation
            };

            // Flags
            enum flags
            {
                FLAG_ACTIVE = ( 1 << 0 ),  // Axis is active
                FLAG_LIMITED = ( 1 << 1 ), // Axis is limited
            };

            // Data
            u32 Flags; // Flags
            f32 Min;   // Minimum limit
            f32 Max;   // Maximum limit
        };

        // Type
        enum type
        {
            TYPE_SPHERE,
            TYPE_CYLINDER,
            TYPE_BOX
        };

        // Flags
        enum Flags
        {
            FLAG_WORLD_COLLISION = ( 1 << 0 ), // Body has collision with world
        };

        // Data
        quaternion BodyBindRotation;  // World space body bind rotation
        vector3    BodyBindPosition;  // World space body bind position
        quaternion PivotBindRotation; // World space pivot bind rotation
        vector3    PivotBindPosition; // World space pivot bind position
        s32        NameOffset;        // Offset into string data for name
        f32        Mass;              // Mass of rigid body
        f32        Radius;            // Radius of rigid body
        f32        Width;             // Width of rigid body
        f32        Height;            // Height of rigid body
        f32        Length;            // Length of rigid body
        s16        Type;              // Type of rigid body
        u16        Flags;             // Various flags
        s16        iParentBody;       // Index of parent rigid body (or -1)
        s16        iBone;             // Index of best bone to attach to
        u32        CollisionMask;     // Describes collision with other bodies
        dof        DOF[6];            // Degrees of freedom info
    };

    struct mesh
    {
        bbox BBox;
        s32  NameOffset; // into string data
        s32  nSubMeshes;
        s32  iSubMesh;
        s32  nBones; // Number of bones used
        s32  nFaces;
        s32  nVertices;
    };

    struct submesh
    {
        s32 iSection;       // First draw section
        s32 nSections;      // Number of draw sections
        s32 iMaterial;      // Index of the Material that this SubMesh uses
        f32 WorldPixelSize; // Average World Pixel size for this SubMesh
    };

    struct material
    {
        struct uvanim
        {
            enum type
            {
                FIXED = 0,
                LOOPED,
                PINGPONG,
                ONESHOT,
            };

            s8  Type;
            s8  StartFrame;
            s8  FPS;
            s32 nKeys;
            s32 iKey;
        };

        enum
        {
            MAX_PARAMS = 12,
        };

        enum
        {
            FLAG_DOUBLE_SIDED = MATERIAL_FLAG_DOUBLE_SIDED,
            FLAG_HAS_ENV_MAP = MATERIAL_FLAG_HAS_ENV_MAP,
            FLAG_HAS_DETAIL_MAP = MATERIAL_FLAG_HAS_DETAIL_MAP,
            FLAG_ENV_WORLD_SPACE = MATERIAL_FLAG_ENV_WORLD_SPACE,
            FLAG_ENV_VIEW_SPACE = MATERIAL_FLAG_ENV_VIEW_SPACE,
            FLAG_ENV_CUBE_MAP = MATERIAL_FLAG_ENV_CUBE_MAP,
            FLAG_FORCE_ZFILL = MATERIAL_FLAG_FORCE_ZFILL,
            FLAG_ILLUM_USES_DIFFUSE = MATERIAL_FLAG_ILLUM_USES_DIFFUSE,
            FLAG_IS_PUNCH_THRU = MATERIAL_FLAG_IS_PUNCH_THRU,
            FLAG_IS_ADDITIVE = MATERIAL_FLAG_IS_ADDITIVE,
            FLAG_IS_SUBTRACTIVE = MATERIAL_FLAG_IS_SUBTRACTIVE
        };

        uvanim UVAnim; // UV Animation data
        f32    DetailScale;
        f32    FixedAlpha;
        u16    Flags; // flags
        s8     Type;
        s32    nTextures;    // Total number of textures used in the material
        s32    iTexture;     // Index into global texture list for the Geom
        s32    nVirtualMats; // Number of registered mats based on this material (1 unless there is a virtual texture
                             // present)
        s32 iVirtualMat;     // Offset to the bitmaps
    };

    struct texture
    {
        s32 DescOffset;
        s32 FileNameOffset;
    };

    struct uvkey
    {
        u8 OffsetU;
        u8 OffsetV;
    };

    struct virtual_mesh
    {
        s32 NameOffset;
        s32 nLODs;
        s32 iLOD;
    };

    struct virtual_texture
    {
        s32 NameOffset;   // name of the virtual texture
        u32 MaterialMask; // mask of which materials it will effect
    };

    //-------------------------------------------------------------------------

    geom( void );
    ~geom( void );
    xbool PrepareMaterials( void );
    ::material const& GetPreparedMaterial( s32 VirtualMaterial ) const;
    s32   GetNFaces( void ) const;
    s32   GetNVerts( void ) const;
    xbool HasUVAnim( s32 iMaterial ) const;

    s32         GetVMeshIndex( char const* pName ) const;
    s32         GetMeshIndex( char const* pName ) const;
    s32         GetVTextureIndex( char const* pName ) const;
    s32         GetSubMeshIndex( s32 iMesh, s32 iMaterial ) const;
    xbool       HasUVAnim( s32 iMesh, s32 iMaterial ) const;
    char const* GetVMeshName( s32 iVMesh ) const;
    char const* GetMeshName( s32 iMesh ) const;
    char const* GetVTextureName( s32 iVTexture ) const;
    char const* GetTextureDesc( s32 iTexture ) const;
    char const* GetTextureName( s32 iTexture ) const;
    u64         GetLODMask( u32 vMeshMask, u16 ScreenSize ) const;

    // Rigid body functions
    char const* GetRigidBodyName( s32 iRigidBody ) const;
    s32         GetRigidBodyIndex( char const* pName ) const;

    // Bone mask functions
    bone_masks const* FindBoneMasks( char const* pName ) const;

    // Property search: Returns address of section/property if present
    property_section const* FindPropertySection( char const* pSection ) const;
    property const*         FindProperty( property_section const* pSection, char const* pName,
                                          geom::property::type type ) const;

    // Property query: Returns TRUE and sets up the value if present, else just returns FALSE and leaves value
    xbool GetPropertyFloat( geom::property_section const* pSection, char const* pName, f32* pValue ) const;
    xbool GetPropertyInteger( geom::property_section const* pSection, char const* pName, s32* pValue ) const;
    xbool GetPropertyAngle( geom::property_section const* pSection, char const* pName, radian* pValue ) const;
    xbool GetPropertyString( geom::property_section const* pSection, char const* pName, char* pValue,
                             s32 nChars ) const;

    //-------------------------------------------------------------------------

    bbox m_BBox;
    s32  m_nFaces;    // including all meshes/lods
    s32  m_nVertices; // including all meshes/lods
    s32  m_nBones;
    s32  m_nBoneMasks;
    s32  m_nPropertySections;
    s32  m_nProperties;
    s32  m_nRigidBodies;
    s32  m_nMeshes;
    s32  m_nSubMeshes;
    s32  m_nMaterials;
    s32  m_nTextures;
    s32  m_nUVKeys;
    s32  m_nLODs;
    s32  m_nVirtualMeshes;
    s32  m_nVirtualMaterials;
    s32  m_nVirtualTextures;
    s32  m_StringDataSize;

    bone*             m_pBone;
    bone_masks*       m_pBoneMasks;
    property_section* m_pPropertySections;
    property*         m_pProperties;
    rigid_body*       m_pRigidBodies;
    mesh*             m_pMesh;
    submesh*          m_pSubMesh;
    material*         m_pMaterial;
    ::material*       m_pPreparedMaterials;
    texture*          m_pTexture;
    uvkey*            m_pUVKey;
    u16*              m_pLODSizes;
    u64*              m_pLODMasks;
    virtual_mesh*     m_pVirtualMeshes;
    virtual_texture*  m_pVirtualTextures;
    char*             m_pStringData;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline s32 geom::GetNFaces( void ) const
{
    s32 Total = 0;
    for ( s32 i = 0; i < m_nMeshes; i++ )
    {
        Total += m_pMesh[i].nFaces;
    }

    return Total;
}

//=========================================================================

inline s32 geom::GetNVerts( void ) const
{
    s32 Total = 0;
    for ( s32 i = 0; i < m_nMeshes; i++ )
    {
        Total += m_pMesh[i].nVertices;
    }

    return Total;
}

//=========================================================================

inline xbool geom::HasUVAnim( s32 iMaterial ) const
{
    ASSERT( ( iMaterial >= 0 ) && ( iMaterial < m_nMaterials ) );
    return ( m_pMaterial[iMaterial].UVAnim.nKeys > 0 );
}

//=========================================================================

inline char const* geom::GetRigidBodyName( s32 iRigidBody ) const
{
    ASSERT( ( iRigidBody >= 0 ) && ( iRigidBody < m_nRigidBodies ) );
    return &m_pStringData[m_pRigidBodies[iRigidBody].NameOffset];
}

//=========================================================================

inline s32 geom::GetRigidBodyIndex( char const* pName ) const
{
    // Check all rigid bodies
    for ( s32 i = 0; i < m_nRigidBodies; i++ )
    {
        // Found?
        if ( x_strcmp( GetRigidBodyName( i ), pName ) == 0 )
        {
            return i;
        }
    }

    // Not found
    return -1;
}

//=========================================================================

inline s32 geom::GetVMeshIndex( char const* pName ) const
{
    for ( s32 i = 0; i < m_nVirtualMeshes; i++ )
    {
        if ( !x_strcmp( &m_pStringData[m_pVirtualMeshes[i].NameOffset], pName ) )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

inline s32 geom::GetMeshIndex( char const* pName ) const
{
    for ( s32 i = 0; i < m_nMeshes; i++ )
    {
        if ( !x_strcmp( &m_pStringData[m_pMesh[i].NameOffset], pName ) )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

inline s32 geom::GetVTextureIndex( char const* pName ) const
{
    for ( s32 i = 0; i < m_nVirtualTextures; i++ )
    {
        if ( !x_strcmp( &m_pStringData[m_pVirtualTextures[i].NameOffset], pName ) )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

inline s32 geom::GetSubMeshIndex( s32 iMesh, s32 iMaterial ) const
{
    ASSERT( ( iMesh >= 0 ) && ( iMesh < m_nMeshes ) );
    for ( s32 i = m_pMesh[iMesh].iSubMesh; i < m_pMesh[iMesh].iSubMesh + m_pMesh[iMesh].nSubMeshes; i++ )
    {
        if ( m_pSubMesh[i].iMaterial == iMaterial )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

inline xbool geom::HasUVAnim( s32 iMesh, s32 iMaterial ) const
{
    s32 iSubMesh = GetSubMeshIndex( iMesh, iMaterial );
    if ( iSubMesh < 0 )
    {
        return FALSE;
    }
    else
    {
        return HasUVAnim( m_pSubMesh[iSubMesh].iMaterial );
    }
}

//=========================================================================

inline char const* geom::GetVMeshName( s32 iVMesh ) const
{
    ASSERT( ( iVMesh >= 0 ) && ( iVMesh < m_nVirtualMeshes ) );
    s32 StringDataOffset = m_pVirtualMeshes[iVMesh].NameOffset;
    return &m_pStringData[StringDataOffset];
}

//=========================================================================

inline char const* geom::GetMeshName( s32 iMesh ) const
{
    ASSERT( ( iMesh >= 0 ) && ( iMesh < m_nMeshes ) );
    s32 StringDataOffset = m_pMesh[iMesh].NameOffset;
    return &m_pStringData[StringDataOffset];
}

//=========================================================================

inline char const* geom::GetVTextureName( s32 iVTexture ) const
{
    ASSERT( ( iVTexture >= 0 ) && ( iVTexture < m_nVirtualTextures ) );
    s32 StringDataOffset = m_pVirtualTextures[iVTexture].NameOffset;
    return &m_pStringData[StringDataOffset];
}

//=========================================================================

inline char const* geom::GetTextureDesc( s32 iTexture ) const
{
    ASSERT( ( iTexture >= 0 ) && ( iTexture < m_nTextures ) );
    s32 StringDataOffset = m_pTexture[iTexture].DescOffset;
    return &m_pStringData[StringDataOffset];
}

//=========================================================================

inline char const* geom::GetTextureName( s32 iTexture ) const
{
    ASSERT( ( iTexture >= 0 ) && ( iTexture < m_nTextures ) );
    s32 StringDataOffset = m_pTexture[iTexture].FileNameOffset;
    return &m_pStringData[StringDataOffset];
}

//=========================================================================

inline u64 geom::GetLODMask( u32 vMeshMask, u16 ScreenSize ) const
{
    if ( m_nVirtualMeshes == 0 )
    {
        return static_cast<u64>( -1 );
    }

    u64 LODMask = 0;
    for ( s32 i = 0; i < m_nVirtualMeshes; i++ )
    {
        // check the vmesh mask
        if ( ( vMeshMask & ( 1 << i ) ) && ( m_pVirtualMeshes[i].nLODs ) )
        {
            // okay, this vmesh is on, which LOD?
            s32 Choice = 0;
            for ( s32 j = 1; j < m_pVirtualMeshes[i].nLODs; j++ )
            {
                if ( ScreenSize < m_pLODSizes[j + m_pVirtualMeshes[i].iLOD] )
                {
                    Choice = j;
                }
            }

            // or in this LOD into the total mask
            LODMask |= m_pLODMasks[Choice + m_pVirtualMeshes[i].iLOD];
        }
    }

    return LODMask;
}

//=========================================================================

#endif // GEOM_HPP
//=========================================================================
