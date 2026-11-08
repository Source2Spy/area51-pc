//=========================================================================
//
//  GeometryDescriptor.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_DESCRIPTOR_HPP
#define RESOURCE_TOOL_GEOMETRY_DESCRIPTOR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "Auxiliary/MiscUtils/Property.hpp"
#include "PropertyTree.hpp"

//=========================================================================
//  TYPES
//=========================================================================

class geometry_descriptor : public prop_interface
{
public:
    enum
    {
        MAX_NAME_LENGTH = 256
    };

    enum geometry_type
    {
        GEOMETRY_TYPE_NONE,
        GEOMETRY_TYPE_RIGID,
        GEOMETRY_TYPE_SKIN
    };

    struct uv_animation
    {
        struct key
        {
            key( void ) : U( 0 ), V( 0 ) {}

            u8 U;
            u8 V;
        };

        uv_animation( void ) : AnimType( 1 ), StartFrame( 0 ), FPS( 30 ), nFrames( 0 )
        {
            MatName[0] = '\0';
            Keys.Clear();
        }

        char MatName[MAX_NAME_LENGTH];
        s32 AnimType;
        s32 StartFrame;
        s32 FPS;
        s32 nFrames;
        xarray<key> Keys;
    };

    struct lod_info
    {
        // Mesh names identify authoring data; product masks are compiler state.
        enum
        {
            MAX_LOD_MESHES = 64
        };

        lod_info( void ) : ScreenSize( 10000 ), nMeshes( 0 )
        {
            x_memset( MeshName, 0, sizeof( MeshName ) );
        }

        s32 ScreenSize;
        s32 nMeshes;
        char MeshName[MAX_LOD_MESHES][MAX_NAME_LENGTH];
    };

    struct virtual_mesh
    {
        virtual_mesh( void )
        {
            x_memset( Name, 0, sizeof( Name ) );
            LODs.Clear();
        }

        char Name[MAX_NAME_LENGTH];
        xarray<lod_info> LODs;
    };

    struct texture_info
    {
        texture_info( void )
        {
            FileName[0] = '\0';
            Name[0] = '\0';
        }

        char FileName[MAX_NAME_LENGTH];
        char Name[MAX_NAME_LENGTH];
    };

    struct virtual_texture
    {
        virtual_texture( void )
        {
            Reset();
        }

        void Reset( void )
        {
            Name[0] = '\0';
            OverrideDefault = FALSE;
            OverrideFileName[0] = '\0';
            Textures.Clear();
            Textures.Append();
        }

        char Name[MAX_NAME_LENGTH];
        xarray<texture_info> Textures;
        xbool OverrideDefault;
        char OverrideFileName[MAX_NAME_LENGTH];
    };

    // Engine values not represented by glTF's mesh contract.
    enum texture_slot
    {
        RUNTIME_TEXTURE_DIFFUSE = 0,
        RUNTIME_TEXTURE_ENVIRONMENT,
        RUNTIME_TEXTURE_DETAIL,
        RUNTIME_TEXTURE_PUNCH_THROUGH,
    };

    struct texture
    {
        texture( void ) : Slot( RUNTIME_TEXTURE_DIFFUSE )
        {
            FileName[0] = '\0';
            Name[0] = '\0';
        }

        s32 Slot;
        char FileName[MAX_NAME_LENGTH];
        char Name[MAX_NAME_LENGTH];
    };

    struct material
    {
        material( void ) : Type( 0 ), DetailScale( 1.0f ), FixedAlpha( 1.0f ), Flags( 0 )
        {
            Name[0] = '\0';
            Textures.Clear();
        }

        char Name[MAX_NAME_LENGTH];
        s32 Type;
        f32 DetailScale;
        f32 FixedAlpha;
        s32 Flags;
        xarray<texture> Textures;
    };

    struct submesh
    {
        submesh( void ) : Primitive( -1 ), SubMesh( -1 ), Section( -1 ), WorldPixelSize( 0.001f )
        {
            MeshName[0] = '\0';
            SubMeshName[0] = '\0';
            SectionName[0] = '\0';
            MaterialName[0] = '\0';
        }

        char MeshName[MAX_NAME_LENGTH];
        char SubMeshName[MAX_NAME_LENGTH];
        char SectionName[MAX_NAME_LENGTH];
        s32 Primitive;
        s32 SubMesh;
        s32 Section;
        char MaterialName[MAX_NAME_LENGTH];
        f32 WorldPixelSize;
    };

    struct bone
    {
        bone( void ) : Parent( -1 ), HitLocation( 0 ), iRigidBody( -1 )
        {
            Name[0] = '\0';
            RigidBodyName[0] = '\0';
            BBox.Clear();
        }

        char Name[MAX_NAME_LENGTH];
        s32 Parent;
        bbox BBox;
        s32 HitLocation;
        s32 iRigidBody;
        char RigidBodyName[MAX_NAME_LENGTH];
    };

    struct rigid_body
    {
        struct dof
        {
            dof( void ) : Active( 0 ), Limited( 0 ), Min( 0.0f ), Max( 0.0f ) {}

            s32 Active;
            s32 Limited;
            f32 Min;
            f32 Max;
        };

        rigid_body( void )
            : Mass( 0.0f ), Parent( -1 ), Bone( -1 ), Radius( 0.0f ), Width( 0.0f ), Height( 0.0f ), Length( 0.0f ),
              Flags( 1 ), CollisionMask( -1 )
        {
            Name[0] = '\0';
            Type[0] = '\0';
            ParentName[0] = '\0';
            BoneName[0] = '\0';
            PivotScale.Set( 1.0f, 1.0f, 1.0f );
            BodyScale.Set( 1.0f, 1.0f, 1.0f );
            PivotRotation.Identity();
            BodyRotation.Identity();
            PivotPosition.Zero();
            BodyPosition.Zero();
        }

        char Name[MAX_NAME_LENGTH];
        char Type[MAX_NAME_LENGTH];
        f32 Mass;
        s32 Parent;
        char ParentName[MAX_NAME_LENGTH];
        s32 Bone;
        char BoneName[MAX_NAME_LENGTH];
        vector3 PivotPosition;
        vector3 PivotScale;
        quaternion PivotRotation;
        vector3 BodyPosition;
        vector3 BodyScale;
        quaternion BodyRotation;
        f32 Radius;
        f32 Width;
        f32 Height;
        f32 Length;
        s32 Flags;
        s32 CollisionMask;
        dof DOF[6];
    };

    struct bone_mask
    {
        struct weight
        {
            weight( void ) : Bone( -1 ), Value( 0.0f ) {}
            s32 Bone;
            f32 Value;
        };

        bone_mask( void )
        {
            Name[0] = '\0';
            Weights.Clear();
        }

        char Name[MAX_NAME_LENGTH];
        xarray<weight> Weights;
    };

    struct property
    {
        property( void )
        {
            Section[0] = '\0';
            Name[0] = '\0';
            Type[0] = '\0';
            Value[0] = '\0';
        }

        char Section[MAX_NAME_LENGTH];
        char Name[MAX_NAME_LENGTH];
        char Type[32];
        char Value[256];
    };

    struct collision_cluster
    {
        collision_cluster( void )
            : High( FALSE ), Mesh( -1 ), Bone( -1 ), Section( -1 ), Offset( 0 ), Count( 0 ), VectorOffset( 0 ),
              Points( 0 ), Normals( 0 ), QuadOffset( 0 ), SoundType( 0 ), Flags( 0 )
        {
            MeshName[0] = '\0';
            BoneName[0] = '\0';
            SectionName[0] = '\0';
            BBox.Clear();
        }

        xbool High;
        bbox BBox;
        char MeshName[MAX_NAME_LENGTH];
        char BoneName[MAX_NAME_LENGTH];
        char SectionName[MAX_NAME_LENGTH];
        s32 Mesh;
        s32 Bone;
        s32 Section;
        s32 Offset;
        s32 Count;
        s32 VectorOffset;
        s32 Points;
        s32 Normals;
        s32 QuadOffset;
        s32 SoundType;
        s32 Flags;
        xarray<s32> TriangleOffsets;
        xarray<s32> QuadFlags;
    };

    geometry_descriptor( void );

    void Reset( void );
    void LoadDescriptor( const char* pFileName );
    void SaveDescriptor( const char* pFileName ) const;
    void SetSourceFileName( const char* pFileName );
    void SetOutputFile( const char* pFileName );
    void SetGeneratedRawGeom( const char* pFileName );
    void SetType( geometry_type Type );
    void SetCoordinateSystem( const char* pName );
    void SetUnitScale( f32 Scale );
    void SetSkeletonSource( const char* pFileName );
    void SetPhysicsSource( const char* pFileName );
    void SetCollisionSource( const char* pFileName );
    void SetDoCollision( xbool bEnable );
    const char* GetSourceFileName( void ) const;
    const char* GetOutputFile( void ) const;
    const char* GetGeneratedRawGeom( void ) const;
    geometry_type GetType( void ) const;
    const char* GetCoordinateSystem( void ) const;
    f32 GetUnitScale( void ) const;
    const char* GetSkeletonSource( void ) const;
    const char* GetPhysicsSource( void ) const;
    const char* GetCollisionSource( void ) const;
    xbool DoCollision( void ) const;

    uv_animation& AppendUVAnimation( void );
    s32 GetVirtualMeshCount( void ) const;
    const virtual_mesh& GetVirtualMesh( s32 Index ) const;
    virtual_mesh& AppendVirtualMesh( void );
    s32 GetVirtualTextureCount( void ) const;
    const virtual_texture& GetVirtualTexture( s32 Index ) const;
    virtual_texture& AppendVirtualTexture( void );

    s32 GetMaterialCount( void ) const;
    const material& GetMaterial( s32 Index ) const;
    material& AppendMaterial( void );
    s32 GetSubMeshCount( void ) const;
    const submesh& GetSubMesh( s32 Index ) const;
    submesh& AppendSubMesh( void );
    xbool IsCompiledTexture( const char* pFileName ) const;
    s32 GetBoneCount( void ) const;
    const bone& GetBone( s32 Index ) const;
    bone& AppendBone( void );
    s32 GetRigidBodyCount( void ) const;
    const rigid_body& GetRigidBody( s32 Index ) const;
    rigid_body& AppendRigidBody( void );
    s32 GetBoneMaskCount( void ) const;
    const bone_mask& GetBoneMask( s32 Index ) const;
    bone_mask& AppendBoneMask( void );
    s32 GetPropertyCount( void ) const;
    const property& GetProperty( s32 Index ) const;
    property& AppendProperty( void );
    s32 GetCollisionClusterCount( void ) const;
    const collision_cluster& GetCollisionCluster( s32 Index ) const;
    collision_cluster& AppendCollisionCluster( void );
    void SetCollisionBBox( const bbox& BBox );
    xbool HasCollisionBBox( void ) const;
    const bbox& GetCollisionBBox( void ) const;

    s32 GetUVAnimationCount( void ) const;
    const uv_animation& GetUVAnimation( s32 Index ) const;

protected:
    void ApplyPropertyRecord( const resource_tool::property_tree::property_record& Record );
    virtual void OnEnumProp( prop_enum& List );
    virtual xbool OnProperty( prop_query& Query );

    xbool OnPropertyUVAnimation( prop_query& Query );
    xbool OnPropertyVirtualMesh( prop_query& Query );
    xbool OnPropertyLOD( prop_query& Query, s32 VirtualMeshIndex );
    xbool OnPropertyMesh( prop_query& Query, s32 VirtualMeshIndex, s32 LODIndex );
    xbool OnPropertyVirtualTexture( prop_query& Query );
    xbool OnPropertyDiffuse( prop_query& Query, s32 VirtualTextureIndex );
    xbool OnPropertyMaterial( prop_query& Query );
    xbool OnPropertyTexture( prop_query& Query, s32 MaterialIndex );
    xbool OnPropertySubMesh( prop_query& Query );
    xbool OnPropertyBone( prop_query& Query );
    xbool OnPropertyRigidBody( prop_query& Query );
    xbool OnPropertyBoneMask( prop_query& Query );
    xbool OnPropertyWeight( prop_query& Query, s32 MaskIndex );
    xbool OnPropertyProperty( prop_query& Query );
    xbool OnPropertyCollisionCluster( prop_query& Query );

    char m_SourceFile[X_MAX_PATH];
    char m_OutputFile[X_MAX_PATH];
    char m_GeneratedRawGeom[X_MAX_PATH];
    char m_Type[MAX_NAME_LENGTH];
    char m_CoordinateSystem[MAX_NAME_LENGTH];
    f32 m_UnitScale;
    char m_SkeletonSource[X_MAX_PATH];
    char m_PhysicsSource[X_MAX_PATH];
    char m_CollisionSource[X_MAX_PATH];
    xbool m_DoCollision;
    xarray<uv_animation> m_UVAnimations;
    xarray<virtual_mesh> m_VirtualMeshes;
    xarray<virtual_texture> m_VirtualTextures;
    xarray<material> m_Materials;
    xarray<submesh> m_SubMeshes;
    xarray<bone> m_Bones;
    xarray<rigid_body> m_RigidBodies;
    xarray<bone_mask> m_BoneMasks;
    xarray<property> m_Properties;
    xarray<collision_cluster> m_CollisionClusters;
    bbox m_CollisionBBox;
    xbool m_HasCollisionBBox;
};

#endif // RESOURCE_TOOL_GEOMETRY_DESCRIPTOR_HPP
//=========================================================================
