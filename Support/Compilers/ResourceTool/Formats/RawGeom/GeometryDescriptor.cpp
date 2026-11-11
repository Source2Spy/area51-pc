//=========================================================================
//
//  GeometryDescriptor.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryDescriptor.hpp"
#include "SkeletonContract.hpp"

#include <filesystem>
#include <vector>

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

geometry_descriptor::geometry_descriptor( void )
{
    Reset();
}

//=========================================================================

void geometry_descriptor::Reset( void )
{
    m_SourceFile[0] = '\0';
    m_OutputFile[0] = '\0';
    m_GeneratedRawGeom[0] = '\0';
    m_Type[0] = '\0';
    x_strcpy( m_CoordinateSystem, "GltfNative" );
    m_UnitScale = 1.0f;
    m_SkeletonSource[0] = '\0';
    m_PhysicsSource[0] = '\0';
    m_CollisionSource[0] = '\0';
    m_DoCollision = TRUE;
    m_UVAnimations.Clear();
    m_VirtualMeshes.Clear();
    m_VirtualTextures.Clear();
    m_Materials.Clear();
    m_SubMeshes.Clear();
    m_Bones.Clear();
    m_RigidBodies.Clear();
    m_BoneMasks.Clear();
    m_Properties.Clear();
    m_CollisionClusters.Clear();
    m_CollisionBBox.Clear();
    m_HasCollisionBBox = FALSE;
}

//=========================================================================

void geometry_descriptor::LoadDescriptor( const char* pFileName )
{
    Reset();

    resource_tool::property_tree::document Document;
    resource_tool::property_tree::reader Reader;
    std::string Error;
    if( !Reader.Load( pFileName, Document, Error ) )
        x_throw( xfs( "Unable to open geometry descriptor [%s]", pFileName ) );

    for( const resource_tool::property_tree::property_record& Record : Document.Records() )
        ApplyPropertyRecord( Record );

    if( m_SourceFile[0] == '\0' )
    {
        x_throw( xfs( "Geometry descriptor [%s] does not contain ResDesc\\FileName", pFileName ) );
    }
}

//=========================================================================

void geometry_descriptor::SaveDescriptor( const char* pFileName ) const
{
    if( !pFileName || !pFileName[0] )
        x_throw( "Unable to save geometry descriptor without a filename" );

    resource_tool::property_tree::document Properties;

    s32 Count = 0;
    Count += m_SourceFile[0] != '\0';
    Count += m_OutputFile[0] != '\0';
    Count += m_GeneratedRawGeom[0] != '\0';
    Count += m_Type[0] != '\0';
    Count += m_CoordinateSystem[0] != '\0';
    Count += 1; // UnitScale
    Count += m_SkeletonSource[0] != '\0';
    Count += m_PhysicsSource[0] != '\0';
    Count += m_CollisionSource[0] != '\0';
    Count += 1; // DoCollision
    Count += 1; // NumUVAnims
    Count += m_UVAnimations.GetCount() * 6;
    for( s32 i = 0; i < m_UVAnimations.GetCount(); i++ )
        Count += m_UVAnimations[i].Keys.GetCount() * 2;
    Count += 1; // NumVirtualMeshes
    for( s32 i = 0; i < m_VirtualMeshes.GetCount(); i++ )
    {
        Count += 2; // Name + NumLODs
        for( s32 j = 0; j < m_VirtualMeshes[i].LODs.GetCount(); j++ )
        {
            Count += 2 + m_VirtualMeshes[i].LODs[j].nMeshes;
        }
    }
    Count += 1; // NumVirtualTextures
    for( s32 i = 0; i < m_VirtualTextures.GetCount(); i++ )
    {
        Count += 4; // Name, NumDiffTextures, OverrideDefault, OverrideFileName
        Count += m_VirtualTextures[i].Textures.GetCount() * 2;
    }
    Count += 1; // NumMaterials
    for( s32 i = 0; i < m_Materials.GetCount(); i++ )
        Count += 6 + m_Materials[i].Textures.GetCount() * 3;
    Count += 1; // NumSubMeshes
    Count += m_SubMeshes.GetCount() * 8;
    Count += 1; // NumBones
    Count += m_Bones.GetCount() * 5;
    for( s32 i = 0; i < m_Bones.GetCount(); ++i )
        Count += m_Bones[i].RigidBodyName[0] != '\0';
    Count += 1; // NumRigidBodies
    Count += m_RigidBodies.GetCount() * 49;
    Count += 1; // NumBoneMasks
    for( s32 i = 0; i < m_BoneMasks.GetCount(); i++ )
        Count += 2 + m_BoneMasks[i].Weights.GetCount() * 2;
    Count += 1; // NumProperties
    Count += m_Properties.GetCount() * 4;
    Count += 1; // NumCollisionClusters
    Count += m_CollisionClusters.GetCount() * 16;
    Count += m_HasCollisionBBox ? 1 : 0;
    for( s32 i = 0; i < m_CollisionClusters.GetCount(); ++i )
    {
        if( m_CollisionClusters[i].High )
            Count += 1 + m_CollisionClusters[i].TriangleOffsets.GetCount();
        else
            Count += 1 + m_CollisionClusters[i].QuadFlags.GetCount();
    }
    (void)Count;

#define WRITE_FILE_PROPERTY( Name, Value )                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "FILENAME", Value );                                                                     \
    } while( 0 )
#define WRITE_STRING_PROPERTY( Name, Value )                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "STRING", Value );                                                                       \
    } while( 0 )
#define WRITE_ENUM_PROPERTY( Name, Value )                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "ENUM", Value );                                                                         \
    } while( 0 )
#define WRITE_INT_PROPERTY( Name, Value )                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "INT", (const char*)xfs( "%d", Value ) );                                                \
    } while( 0 )
#define WRITE_BOOL_PROPERTY( Name, Value )                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "BOOL", (const char*)xfs( "%d", Value ? 1 : 0 ) );                                       \
    } while( 0 )
#define WRITE_FLOAT_PROPERTY( Name, Value )                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "FLOAT", (const char*)xfs( "%g", Value ) );                                              \
    } while( 0 )
#define WRITE_VECTOR3_PROPERTY( Name, Value )                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "VECTOR3", (const char*)xfs( "%g %g %g", Value.GetX(), Value.GetY(), Value.GetZ() ) );   \
    } while( 0 )
#define WRITE_BBOX_PROPERTY( Name, Value )                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        Properties.Add( Name, "BBOX",                                                                                  \
                        (const char*)xfs( "%g %g %g %g %g %g", Value.Min.GetX(), Value.Min.GetY(), Value.Min.GetZ(),   \
                                          Value.Max.GetX(), Value.Max.GetY(), Value.Max.GetZ() ) );                    \
    } while( 0 )

    if( m_SourceFile[0] )
        WRITE_FILE_PROPERTY( "ResDesc\\FileName", m_SourceFile );
    if( m_OutputFile[0] )
        WRITE_FILE_PROPERTY( "ResDesc\\OutputFile", m_OutputFile );
    if( m_GeneratedRawGeom[0] )
        WRITE_FILE_PROPERTY( "ResDesc\\GeneratedRawGeom", m_GeneratedRawGeom );
    if( m_Type[0] )
        WRITE_ENUM_PROPERTY( "ResDesc\\Type", m_Type );
    if( m_CoordinateSystem[0] )
        WRITE_STRING_PROPERTY( "ResDesc\\CoordinateSystem", m_CoordinateSystem );
    WRITE_FLOAT_PROPERTY( "ResDesc\\UnitScale", m_UnitScale );
    if( m_SkeletonSource[0] )
        WRITE_FILE_PROPERTY( "ResDesc\\SkeletonSource", m_SkeletonSource );
    if( m_PhysicsSource[0] )
        WRITE_FILE_PROPERTY( "ResDesc\\PhysicsSource", m_PhysicsSource );
    if( m_CollisionSource[0] )
        WRITE_FILE_PROPERTY( "ResDesc\\Collision", m_CollisionSource );
    WRITE_BOOL_PROPERTY( "ResDesc\\DoCollision", m_DoCollision );

    WRITE_INT_PROPERTY( "ResDesc\\UVAnims\\NumUVAnims", m_UVAnimations.GetCount() );
    for( s32 i = 0; i < m_UVAnimations.GetCount(); i++ )
    {
        const uv_animation& A = m_UVAnimations[i];
        Properties.Add( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\MaterialName", i ), "STRING", A.MatName );
        WRITE_ENUM_PROPERTY( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\AnimType", i ),
                             A.AnimType == 0   ? "Fixed"
                             : A.AnimType == 2 ? "PingPong"
                             : A.AnimType == 3 ? "OneShot"
                                               : "Looped" );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\Start", i ), A.StartFrame );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\FPS", i ), A.FPS );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\NumFrames", i ), A.nFrames );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\NumKeys", i ), A.Keys.GetCount() );
        for( s32 j = 0; j < A.Keys.GetCount(); j++ )
        {
            WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\Key[%d]\\U", i, j ), A.Keys[j].U );
            WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\UVAnims\\UVAnim[%d]\\Key[%d]\\V", i, j ), A.Keys[j].V );
        }
    }

    WRITE_INT_PROPERTY( "ResDesc\\VirtualMeshes\\NumVirtualMeshes", m_VirtualMeshes.GetCount() );
    for( s32 i = 0; i < m_VirtualMeshes.GetCount(); i++ )
    {
        const virtual_mesh& V = m_VirtualMeshes[i];
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\VirtualMeshes\\VirtualMesh[%d]\\Name", i ), V.Name );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\VirtualMeshes\\VirtualMesh[%d]\\NumLODs", i ),
                            V.LODs.GetCount() );
        for( s32 j = 0; j < V.LODs.GetCount(); j++ )
        {
            const lod_info& L = V.LODs[j];
            WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\VirtualMeshes\\VirtualMesh[%d]\\LOD[%d]\\NumMeshs", i, j ),
                                L.nMeshes );
            WRITE_INT_PROPERTY(
                (const char*)xfs( "ResDesc\\VirtualMeshes\\VirtualMesh[%d]\\LOD[%d]\\ScreenSize", i, j ),
                L.ScreenSize );
            for( s32 k = 0; k < L.nMeshes; k++ )
                WRITE_STRING_PROPERTY(
                    (const char*)xfs( "ResDesc\\VirtualMeshes\\VirtualMesh[%d]\\LOD[%d]\\Mesh[%d]\\Name", i, j, k ),
                    L.MeshName[k] );
        }
    }

    WRITE_INT_PROPERTY( "ResDesc\\VirtualTextures\\NumVirtualTextures", m_VirtualTextures.GetCount() );
    for( s32 i = 0; i < m_VirtualTextures.GetCount(); i++ )
    {
        const virtual_texture& V = m_VirtualTextures[i];
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\VirtualTextures\\VirtualTexture[%d]\\Name", i ), V.Name );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\VirtualTextures\\VirtualTexture[%d]\\NumDiffTextures", i ),
                            V.Textures.GetCount() );
        WRITE_BOOL_PROPERTY( (const char*)xfs( "ResDesc\\VirtualTextures\\VirtualTexture[%d]\\OverrideDefault", i ),
                             V.OverrideDefault );
        WRITE_FILE_PROPERTY( (const char*)xfs( "ResDesc\\VirtualTextures\\VirtualTexture[%d]\\OverrideFileName", i ),
                             V.OverrideFileName );
        for( s32 j = 0; j < V.Textures.GetCount(); j++ )
        {
            const texture_info& T = V.Textures[j];
            WRITE_FILE_PROPERTY(
                (const char*)xfs( "ResDesc\\VirtualTextures\\VirtualTexture[%d]\\DiffTexture[%d]\\FileName", i, j ),
                T.FileName );
            WRITE_STRING_PROPERTY(
                (const char*)xfs( "ResDesc\\VirtualTextures\\VirtualTexture[%d]\\DiffTexture[%d]\\Name", i, j ),
                T.Name );
        }
    }

    WRITE_INT_PROPERTY( "ResDesc\\Materials\\NumMaterials", m_Materials.GetCount() );
    for( s32 i = 0; i < m_Materials.GetCount(); i++ )
    {
        const material& M = m_Materials[i];
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\Name", i ), M.Name );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\Type", i ), M.Type );
        WRITE_FLOAT_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\DetailScale", i ), M.DetailScale );
        WRITE_FLOAT_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\FixedAlpha", i ), M.FixedAlpha );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\Flags", i ), M.Flags );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\NumTextures", i ),
                            M.Textures.GetCount() );
        for( s32 j = 0; j < M.Textures.GetCount(); j++ )
        {
            const texture& T = M.Textures[j];
            WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\Texture[%d]\\Slot", i, j ),
                                T.Slot );
            WRITE_FILE_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\Texture[%d]\\FileName", i, j ),
                                 T.FileName );
            WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Materials\\Material[%d]\\Texture[%d]\\Name", i, j ),
                                   T.Name );
        }
    }

    WRITE_INT_PROPERTY( "ResDesc\\SubMeshes\\NumSubMeshes", m_SubMeshes.GetCount() );
    for( s32 i = 0; i < m_SubMeshes.GetCount(); ++i )
    {
        const submesh& S = m_SubMeshes[i];
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\MeshName", i ), S.MeshName );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\SubMeshName", i ), S.SubMeshName );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\SectionName", i ), S.SectionName );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\Primitive", i ), S.Primitive );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\SubMesh", i ), S.SubMesh );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\Section", i ), S.Section );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\MaterialName", i ), S.MaterialName );
        WRITE_FLOAT_PROPERTY( (const char*)xfs( "ResDesc\\SubMeshes\\SubMesh[%d]\\WorldPixelSize", i ),
                              S.WorldPixelSize );
    }

    WRITE_INT_PROPERTY( "ResDesc\\Bones\\NumBones", m_Bones.GetCount() );
    for( s32 i = 0; i < m_Bones.GetCount(); i++ )
    {
        const bone& B = m_Bones[i];
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Bones\\Bone[%d]\\Name", i ), B.Name );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\Bones\\Bone[%d]\\Parent", i ), B.Parent );
        WRITE_BBOX_PROPERTY( (const char*)xfs( "ResDesc\\Bones\\Bone[%d]\\BBox", i ), B.BBox );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\Bones\\Bone[%d]\\HitLocation", i ), B.HitLocation );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\Bones\\Bone[%d]\\RigidBody", i ), B.iRigidBody );
        if( B.RigidBodyName[0] )
            WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Bones\\Bone[%d]\\RigidBodyName", i ), B.RigidBodyName );
    }

    WRITE_INT_PROPERTY( "ResDesc\\RigidBodies\\NumBodies", m_RigidBodies.GetCount() );
    for( s32 i = 0; i < m_RigidBodies.GetCount(); ++i )
    {
        const rigid_body& B = m_RigidBodies[i];
        xstring Prefix;
        Prefix.Format( "ResDesc\\RigidBodies\\Body[%d]", i );
#define WRITE_BODY_PATH( Name ) (const char*)xfs( "%s\\%s", (const char*)Prefix, Name )
        WRITE_STRING_PROPERTY( WRITE_BODY_PATH( "Name" ), B.Name );
        WRITE_STRING_PROPERTY( WRITE_BODY_PATH( "Type" ), B.Type );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "Mass" ), B.Mass );
        WRITE_INT_PROPERTY( WRITE_BODY_PATH( "Parent" ), B.Parent );
        WRITE_STRING_PROPERTY( WRITE_BODY_PATH( "ParentName" ), B.ParentName );
        WRITE_INT_PROPERTY( WRITE_BODY_PATH( "Bone" ), B.Bone );
        WRITE_STRING_PROPERTY( WRITE_BODY_PATH( "BoneName" ), B.BoneName );
        WRITE_VECTOR3_PROPERTY( WRITE_BODY_PATH( "PivotPosition" ), B.PivotPosition );
        WRITE_VECTOR3_PROPERTY( WRITE_BODY_PATH( "PivotScale" ), B.PivotScale );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "PivotRotationX" ), B.PivotRotation.X );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "PivotRotationY" ), B.PivotRotation.Y );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "PivotRotationZ" ), B.PivotRotation.Z );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "PivotRotationW" ), B.PivotRotation.W );
        WRITE_VECTOR3_PROPERTY( WRITE_BODY_PATH( "BodyPosition" ), B.BodyPosition );
        WRITE_VECTOR3_PROPERTY( WRITE_BODY_PATH( "BodyScale" ), B.BodyScale );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "BodyRotationX" ), B.BodyRotation.X );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "BodyRotationY" ), B.BodyRotation.Y );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "BodyRotationZ" ), B.BodyRotation.Z );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "BodyRotationW" ), B.BodyRotation.W );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "Radius" ), B.Radius );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "Width" ), B.Width );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "Height" ), B.Height );
        WRITE_FLOAT_PROPERTY( WRITE_BODY_PATH( "Length" ), B.Length );
        WRITE_INT_PROPERTY( WRITE_BODY_PATH( "Flags" ), B.Flags );
        WRITE_INT_PROPERTY( WRITE_BODY_PATH( "CollisionMask" ), B.CollisionMask );
        for( s32 j = 0; j < 6; ++j )
        {
            WRITE_INT_PROPERTY( (const char*)xfs( "%s\\DOF[%d]\\Active", (const char*)Prefix, j ), B.DOF[j].Active );
            WRITE_INT_PROPERTY( (const char*)xfs( "%s\\DOF[%d]\\Limited", (const char*)Prefix, j ), B.DOF[j].Limited );
            WRITE_FLOAT_PROPERTY( (const char*)xfs( "%s\\DOF[%d]\\Min", (const char*)Prefix, j ), B.DOF[j].Min );
            WRITE_FLOAT_PROPERTY( (const char*)xfs( "%s\\DOF[%d]\\Max", (const char*)Prefix, j ), B.DOF[j].Max );
        }
#undef WRITE_BODY_PATH
    }

    WRITE_INT_PROPERTY( "ResDesc\\BoneMasks\\NumMasks", m_BoneMasks.GetCount() );
    for( s32 i = 0; i < m_BoneMasks.GetCount(); i++ )
    {
        const bone_mask& M = m_BoneMasks[i];
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\BoneMasks\\Mask[%d]\\Name", i ), M.Name );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\BoneMasks\\Mask[%d]\\NumWeights", i ), M.Weights.GetCount() );
        for( s32 j = 0; j < M.Weights.GetCount(); j++ )
        {
            WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\BoneMasks\\Mask[%d]\\Weight[%d]\\Bone", i, j ),
                                M.Weights[j].Bone );
            WRITE_FLOAT_PROPERTY( (const char*)xfs( "ResDesc\\BoneMasks\\Mask[%d]\\Weight[%d]\\Value", i, j ),
                                  M.Weights[j].Value );
        }
    }

    WRITE_INT_PROPERTY( "ResDesc\\Properties\\NumProperties", m_Properties.GetCount() );
    for( s32 i = 0; i < m_Properties.GetCount(); i++ )
    {
        const property& P = m_Properties[i];
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Properties\\Property[%d]\\Section", i ), P.Section );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Properties\\Property[%d]\\Name", i ), P.Name );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Properties\\Property[%d]\\Type", i ), P.Type );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\Properties\\Property[%d]\\Value", i ), P.Value );
    }

    WRITE_INT_PROPERTY( "ResDesc\\CollisionMetadata\\NumClusters", m_CollisionClusters.GetCount() );
    if( m_HasCollisionBBox )
        WRITE_BBOX_PROPERTY( "ResDesc\\CollisionMetadata\\BBox", m_CollisionBBox );
    for( s32 i = 0; i < m_CollisionClusters.GetCount(); i++ )
    {
        const collision_cluster& C = m_CollisionClusters[i];
        WRITE_BOOL_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\High", i ), C.High );
        WRITE_BBOX_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\BBox", i ), C.BBox );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\MeshName", i ), C.MeshName );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\BoneName", i ), C.BoneName );
        WRITE_STRING_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\SectionName", i ),
                               C.SectionName );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Mesh", i ), C.Mesh );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Bone", i ), C.Bone );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Section", i ), C.Section );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Offset", i ), C.Offset );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Count", i ), C.Count );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\VectorOffset", i ),
                            C.VectorOffset );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Points", i ), C.Points );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Normals", i ), C.Normals );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\QuadOffset", i ),
                            C.QuadOffset );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\SoundType", i ), C.SoundType );
        WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\Flags", i ), C.Flags );
        if( C.High )
        {
            WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\NumTriangleOffsets", i ),
                                C.TriangleOffsets.GetCount() );
            for( s32 j = 0; j < C.TriangleOffsets.GetCount(); ++j )
            {
                WRITE_INT_PROPERTY(
                    (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\TriangleOffset[%d]\\Value", i, j ),
                    C.TriangleOffsets[j] );
            }
        }
        else
        {
            WRITE_INT_PROPERTY( (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\NumQuadFlags", i ),
                                C.QuadFlags.GetCount() );
            for( s32 j = 0; j < C.QuadFlags.GetCount(); ++j )
            {
                WRITE_INT_PROPERTY(
                    (const char*)xfs( "ResDesc\\CollisionMetadata\\Cluster[%d]\\QuadFlag[%d]\\Value", i, j ),
                    C.QuadFlags[j] );
            }
        }
    }

#undef WRITE_FILE_PROPERTY
#undef WRITE_STRING_PROPERTY
#undef WRITE_ENUM_PROPERTY
#undef WRITE_INT_PROPERTY
#undef WRITE_BOOL_PROPERTY
#undef WRITE_FLOAT_PROPERTY
#undef WRITE_VECTOR3_PROPERTY
#undef WRITE_BBOX_PROPERTY

    if( (s32)Properties.Records().size() != Count )
        x_throw( "Geometry descriptor property count mismatch" );

    resource_tool::property_tree::writer Writer;
    std::string Error;
    if( !Writer.Write( pFileName, Properties, Error ) )
        x_throw( xfs( "%s", Error.c_str() ) );
}

//=========================================================================

void geometry_descriptor::ApplyPropertyRecord( const resource_tool::property_tree::property_record& Record )
{
    const char* pName = Record.Name.c_str();
    const char* pValue = Record.Value.c_str();

    if( Record.Type == "FLOAT" )
    {
        const f32 Value = x_atof( pValue );
        OnProperty( g_PropQuery.WQueryFloat( pName, Value ) );
    }
    else if( Record.Type == "INT" )
    {
        const s32 Value = x_atoi( pValue );
        OnProperty( g_PropQuery.WQueryInt( pName, Value ) );
    }
    else if( Record.Type == "BOOL" )
    {
        const xbool Value = x_atoi( pValue ) != 0;
        OnProperty( g_PropQuery.WQueryBool( pName, Value ) );
    }
    else if( Record.Type == "VECTOR3" )
    {
        vector3 Value;
        std::sscanf( pValue, "%f %f %f", &Value.GetX(), &Value.GetY(), &Value.GetZ() );
        OnProperty( g_PropQuery.WQueryVector3( pName, Value ) );
    }
    else if( Record.Type == "ROTATION" )
    {
        radian3 Value;
        std::sscanf( pValue, "%f %f %f", &Value.Roll, &Value.Pitch, &Value.Yaw );
        Value.Roll = DEG_TO_RAD( Value.Roll );
        Value.Pitch = DEG_TO_RAD( Value.Pitch );
        Value.Yaw = DEG_TO_RAD( Value.Yaw );
        OnProperty( g_PropQuery.WQueryRotation( pName, Value ) );
    }
    else if( Record.Type == "ANGLE" )
    {
        radian Value = DEG_TO_RAD( x_atof( pValue ) );
        OnProperty( g_PropQuery.WQueryAngle( pName, Value ) );
    }
    else if( Record.Type == "BBOX" )
    {
        bbox Value;
        std::sscanf( pValue, "%f %f %f %f %f %f", &Value.Min.GetX(), &Value.Min.GetY(), &Value.Min.GetZ(),
                     &Value.Max.GetX(), &Value.Max.GetY(), &Value.Max.GetZ() );
        OnProperty( g_PropQuery.WQueryBBox( pName, Value ) );
    }
    else if( Record.Type == "GUID" )
    {
        s32 Low = 0;
        s32 High = 0;
        guid Value;
        std::sscanf( pValue, "%d %d", &Low, &High );
        Value.Guid = (u32)Low | ( (u64)High << 32 );
        OnProperty( g_PropQuery.WQueryGUID( pName, Value ) );
    }
    else if( Record.Type == "COLOR" )
    {
        f32 R = 0.0f;
        f32 G = 0.0f;
        f32 B = 0.0f;
        f32 A = 0.0f;
        xcolor Value;
        std::sscanf( pValue, "%f %f %f %f", &R, &G, &B, &A );
        Value.SetfRGBA( R, G, B, A );
        OnProperty( g_PropQuery.WQueryColor( pName, Value ) );
    }
    else if( Record.Type == "STRING" )
        OnProperty( g_PropQuery.WQueryString( pName, pValue ) );
    else if( Record.Type == "ENUM" )
        OnProperty( g_PropQuery.WQueryEnum( pName, pValue ) );
    else if( Record.Type == "EXTERNAL" )
        OnProperty( g_PropQuery.WQueryExternal( pName, pValue ) );
    else if( Record.Type == "FILENAME" )
        OnProperty( g_PropQuery.WQueryFileName( pName, pValue ) );
}

//=========================================================================

void geometry_descriptor::SetSourceFileName( const char* pFileName )
{
    x_strsavecpy( m_SourceFile, pFileName, sizeof( m_SourceFile ) );
}

//=========================================================================

void geometry_descriptor::SetOutputFile( const char* pFileName )
{
    x_strsavecpy( m_OutputFile, pFileName, sizeof( m_OutputFile ) );
}

//=========================================================================

void geometry_descriptor::SetGeneratedRawGeom( const char* pFileName )
{
    x_strsavecpy( m_GeneratedRawGeom, pFileName, sizeof( m_GeneratedRawGeom ) );
}

//=========================================================================

void geometry_descriptor::SetType( geometry_type Type )
{
    const char* pType = Type == GEOMETRY_TYPE_RIGID ? "Rigid" : Type == GEOMETRY_TYPE_SKIN ? "Skin" : "";
    x_strsavecpy( m_Type, pType, sizeof( m_Type ) );
}

//=========================================================================

void geometry_descriptor::SetCoordinateSystem( const char* pName )
{
    x_strsavecpy( m_CoordinateSystem, pName, sizeof( m_CoordinateSystem ) );
}

//=========================================================================

void geometry_descriptor::SetUnitScale( f32 Scale )
{
    m_UnitScale = Scale;
}

//=========================================================================

void geometry_descriptor::SetSkeletonSource( const char* pFileName )
{
    x_strsavecpy( m_SkeletonSource, pFileName, sizeof( m_SkeletonSource ) );
}

//=========================================================================

void geometry_descriptor::SetPhysicsSource( const char* pFileName )
{
    x_strsavecpy( m_PhysicsSource, pFileName, sizeof( m_PhysicsSource ) );
}

//=========================================================================

void geometry_descriptor::SetCollisionSource( const char* pFileName )
{
    x_strsavecpy( m_CollisionSource, pFileName, sizeof( m_CollisionSource ) );
}

//=========================================================================

void geometry_descriptor::SetDoCollision( xbool bEnable )
{
    m_DoCollision = bEnable;
}

//=========================================================================

const char* geometry_descriptor::GetSourceFileName( void ) const
{
    return m_SourceFile;
}

//=========================================================================

const char* geometry_descriptor::GetOutputFile( void ) const
{
    return m_OutputFile;
}

//=========================================================================

const char* geometry_descriptor::GetGeneratedRawGeom( void ) const
{
    return m_GeneratedRawGeom;
}

//=========================================================================

geometry_descriptor::geometry_type geometry_descriptor::GetType( void ) const
{
    if( !x_stricmp( m_Type, "Rigid" ) )
        return GEOMETRY_TYPE_RIGID;
    if( !x_stricmp( m_Type, "Skin" ) )
        return GEOMETRY_TYPE_SKIN;
    return GEOMETRY_TYPE_NONE;
}

//=========================================================================

const char* geometry_descriptor::GetCoordinateSystem( void ) const
{
    return m_CoordinateSystem;
}

//=========================================================================

f32 geometry_descriptor::GetUnitScale( void ) const
{
    return m_UnitScale;
}

//=========================================================================

const char* geometry_descriptor::GetSkeletonSource( void ) const
{
    return m_SkeletonSource;
}

//=========================================================================

const char* geometry_descriptor::GetPhysicsSource( void ) const
{
    return m_PhysicsSource;
}

//=========================================================================

//=========================================================================

const char* geometry_descriptor::GetCollisionSource( void ) const
{
    return m_CollisionSource;
}

//=========================================================================

xbool geometry_descriptor::DoCollision( void ) const
{
    return m_DoCollision;
}

//=========================================================================

geometry_descriptor::uv_animation& geometry_descriptor::AppendUVAnimation( void )
{
    return m_UVAnimations.Append();
}

//=========================================================================

s32 geometry_descriptor::GetVirtualMeshCount( void ) const
{
    return m_VirtualMeshes.GetCount();
}

//=========================================================================

const geometry_descriptor::virtual_mesh& geometry_descriptor::GetVirtualMesh( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetVirtualMeshCount() ) );
    return m_VirtualMeshes[Index];
}

//=========================================================================

geometry_descriptor::virtual_mesh& geometry_descriptor::AppendVirtualMesh( void )
{
    return m_VirtualMeshes.Append();
}

//=========================================================================

s32 geometry_descriptor::GetVirtualTextureCount( void ) const
{
    return m_VirtualTextures.GetCount();
}

//=========================================================================

const geometry_descriptor::virtual_texture& geometry_descriptor::GetVirtualTexture( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetVirtualTextureCount() ) );
    return m_VirtualTextures[Index];
}

//=========================================================================

geometry_descriptor::virtual_texture& geometry_descriptor::AppendVirtualTexture( void )
{
    return m_VirtualTextures.Append();
}

//=========================================================================

s32 geometry_descriptor::GetMaterialCount( void ) const
{
    return m_Materials.GetCount();
}

//=========================================================================

s32 geometry_descriptor::GetSubMeshCount( void ) const
{
    return m_SubMeshes.GetCount();
}

//=========================================================================

const geometry_descriptor::submesh& geometry_descriptor::GetSubMesh( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetSubMeshCount() ) );
    return m_SubMeshes[Index];
}

//=========================================================================

geometry_descriptor::submesh& geometry_descriptor::AppendSubMesh( void )
{
    return m_SubMeshes.Append();
}

//=========================================================================

const geometry_descriptor::material& geometry_descriptor::GetMaterial( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetMaterialCount() ) );
    return m_Materials[Index];
}

//=========================================================================

geometry_descriptor::material& geometry_descriptor::AppendMaterial( void )
{
    return m_Materials.Append();
}

//=========================================================================

xbool geometry_descriptor::IsCompiledTexture( const char* pFileName ) const
{
    for( s32 iMaterial = 0; iMaterial < m_Materials.GetCount(); ++iMaterial )
    {
        const material& Material = m_Materials[iMaterial];
        for( s32 iTexture = 0; iTexture < Material.Textures.GetCount(); ++iTexture )
        {
            if( !x_stricmp( Material.Textures[iTexture].FileName, pFileName ? pFileName : "" ) )
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

//=========================================================================

s32 geometry_descriptor::GetBoneCount( void ) const
{
    return m_Bones.GetCount();
}

//=========================================================================

const geometry_descriptor::bone& geometry_descriptor::GetBone( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetBoneCount() ) );
    return m_Bones[Index];
}

//=========================================================================

geometry_descriptor::bone& geometry_descriptor::AppendBone( void )
{
    return m_Bones.Append();
}

//=========================================================================

s32 geometry_descriptor::GetRigidBodyCount( void ) const
{
    return m_RigidBodies.GetCount();
}

//=========================================================================

const geometry_descriptor::rigid_body& geometry_descriptor::GetRigidBody( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetRigidBodyCount() ) );
    return m_RigidBodies[Index];
}

//=========================================================================

geometry_descriptor::rigid_body& geometry_descriptor::AppendRigidBody( void )
{
    return m_RigidBodies.Append();
}

//=========================================================================

s32 geometry_descriptor::GetBoneMaskCount( void ) const
{
    return m_BoneMasks.GetCount();
}

//=========================================================================

const geometry_descriptor::bone_mask& geometry_descriptor::GetBoneMask( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetBoneMaskCount() ) );
    return m_BoneMasks[Index];
}

//=========================================================================

geometry_descriptor::bone_mask& geometry_descriptor::AppendBoneMask( void )
{
    return m_BoneMasks.Append();
}

//=========================================================================

s32 geometry_descriptor::GetPropertyCount( void ) const
{
    return m_Properties.GetCount();
}

//=========================================================================

const geometry_descriptor::property& geometry_descriptor::GetProperty( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetPropertyCount() ) );
    return m_Properties[Index];
}

//=========================================================================

geometry_descriptor::property& geometry_descriptor::AppendProperty( void )
{
    return m_Properties.Append();
}

//=========================================================================

s32 geometry_descriptor::GetCollisionClusterCount( void ) const
{
    return m_CollisionClusters.GetCount();
}

//=========================================================================

const geometry_descriptor::collision_cluster& geometry_descriptor::GetCollisionCluster( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetCollisionClusterCount() ) );
    return m_CollisionClusters[Index];
}

//=========================================================================

geometry_descriptor::collision_cluster& geometry_descriptor::AppendCollisionCluster( void )
{
    return m_CollisionClusters.Append();
}

//=========================================================================

void geometry_descriptor::SetCollisionBBox( const bbox& BBox )
{
    m_CollisionBBox = BBox;
    m_HasCollisionBBox = TRUE;
}

//=========================================================================

xbool geometry_descriptor::HasCollisionBBox( void ) const
{
    return m_HasCollisionBBox;
}

//=========================================================================

const bbox& geometry_descriptor::GetCollisionBBox( void ) const
{
    return m_CollisionBBox;
}

//=========================================================================

s32 geometry_descriptor::GetUVAnimationCount( void ) const
{
    return m_UVAnimations.GetCount();
}

//=========================================================================

const geometry_descriptor::uv_animation& geometry_descriptor::GetUVAnimation( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < GetUVAnimationCount() ) );
    return m_UVAnimations[Index];
}

//=========================================================================

//=========================================================================

void geometry_descriptor::OnEnumProp( prop_enum& List )
{
    (void)List;
}

//=========================================================================

xbool geometry_descriptor::OnProperty( prop_query& Query )
{
    if( Query.VarFileName( "ResDesc\\FileName", m_SourceFile, sizeof( m_SourceFile ) ) )
        return TRUE;

    if( Query.VarFileName( "ResDesc\\OutputFile", m_OutputFile, sizeof( m_OutputFile ) ) )
        return TRUE;

    if( Query.VarFileName( "ResDesc\\GeneratedRawGeom", m_GeneratedRawGeom, sizeof( m_GeneratedRawGeom ) ) )
        return TRUE;

    if( Query.VarEnum( "ResDesc\\Type", m_Type ) )
        return TRUE;

    if( Query.VarString( "ResDesc\\CoordinateSystem", m_CoordinateSystem, sizeof( m_CoordinateSystem ) ) )
        return TRUE;

    if( Query.VarFloat( "ResDesc\\UnitScale", m_UnitScale, 0.000001f, F32_MAX ) )
        return TRUE;

    if( Query.VarFileName( "ResDesc\\SkeletonSource", m_SkeletonSource, sizeof( m_SkeletonSource ) ) )
        return TRUE;

    if( Query.VarFileName( "ResDesc\\PhysicsSource", m_PhysicsSource, sizeof( m_PhysicsSource ) ) )
        return TRUE;

    if( Query.VarFileName( "ResDesc\\Collision", m_CollisionSource, sizeof( m_CollisionSource ) ) )
        return TRUE;

    if( Query.VarBool( "ResDesc\\DoCollision", m_DoCollision ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\UVAnims\\NumUVAnims" ) )
    {
        if( !Query.IsRead() )
            m_UVAnimations.SetCount( Query.GetVarInt( 0, 32 ) );
        return TRUE;
    }

    if( OnPropertyUVAnimation( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\VirtualMeshes\\NumVirtualMeshes" ) )
    {
        if( !Query.IsRead() )
            m_VirtualMeshes.SetCount( Query.GetVarInt( 0, 64 ) );
        return TRUE;
    }

    if( OnPropertyVirtualMesh( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\VirtualTextures\\NumVirtualTextures" ) )
    {
        if( !Query.IsRead() )
            m_VirtualTextures.SetCount( Query.GetVarInt( 0, 32 ) );
        return TRUE;
    }

    if( OnPropertyVirtualTexture( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\Materials\\NumMaterials" ) )
    {
        if( !Query.IsRead() )
            m_Materials.SetCount( Query.GetVarInt( 0, 4096 ) );
        return TRUE;
    }

    if( OnPropertyMaterial( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\SubMeshes\\NumSubMeshes" ) )
    {
        if( !Query.IsRead() )
            m_SubMeshes.SetCount( Query.GetVarInt( 0, 1000000 ) );
        return TRUE;
    }

    if( OnPropertySubMesh( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\Bones\\NumBones" ) )
    {
        if( !Query.IsRead() )
            m_Bones.SetCount( Query.GetVarInt( 0, resource_tool::skeleton_contract::MaxBones ) );
        return TRUE;
    }

    if( OnPropertyBone( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\RigidBodies\\NumBodies" ) )
    {
        if( !Query.IsRead() )
            m_RigidBodies.SetCount( Query.GetVarInt( 0, 4096 ) );
        return TRUE;
    }

    if( OnPropertyRigidBody( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\BoneMasks\\NumMasks" ) )
    {
        if( !Query.IsRead() )
            m_BoneMasks.SetCount( Query.GetVarInt( 0, 4096 ) );
        return TRUE;
    }

    if( OnPropertyBoneMask( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\Properties\\NumProperties" ) )
    {
        if( !Query.IsRead() )
            m_Properties.SetCount( Query.GetVarInt( 0, 100000 ) );
        return TRUE;
    }

    if( OnPropertyProperty( Query ) )
        return TRUE;

    if( Query.IsVar( "ResDesc\\CollisionMetadata\\NumClusters" ) )
    {
        if( !Query.IsRead() )
            m_CollisionClusters.SetCount( Query.GetVarInt( 0, 100000 ) );
        return TRUE;
    }

    if( Query.VarBBox( "ResDesc\\CollisionMetadata\\BBox", m_CollisionBBox ) )
    {
        m_HasCollisionBBox = TRUE;
        return TRUE;
    }

    if( OnPropertyCollisionCluster( Query ) )
        return TRUE;

    return FALSE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyUVAnimation( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\UVAnims\\UVAnim[]" ) )
        return FALSE;

    s32 const Index = Query.GetIndex( 0 );
    if( ( Index < 0 ) || ( Index >= m_UVAnimations.GetCount() ) )
        return FALSE;

    uv_animation& Animation = m_UVAnimations[Index];
    s32 const Path = Query.PushPath( "ResDesc\\UVAnims\\UVAnim[]\\" );

    if( Query.VarString( "MaterialName", Animation.MatName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.IsVar( "AnimType" ) )
    {
        if( !Query.IsRead() )
        {
            const char* Type = Query.GetVarEnum();
            if( !x_strcmp( Type, "Fixed" ) )
                Animation.AnimType = 0;
            else if( !x_strcmp( Type, "Looped" ) )
                Animation.AnimType = 1;
            else if( !x_strcmp( Type, "PingPong" ) )
                Animation.AnimType = 2;
            else if( !x_strcmp( Type, "OneShot" ) )
                Animation.AnimType = 3;
        }
    }
    else if( Query.VarInt( "Start", Animation.StartFrame, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "FPS", Animation.FPS, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "NumFrames", Animation.nFrames, 0, S32_MAX ) )
    {
    }
    else if( Query.IsVar( "NumKeys" ) )
    {
        if( !Query.IsRead() )
            Animation.Keys.SetCount( Query.GetVarInt( 0, 1000000 ) );
    }
    else if( Query.IsBasePath( "Key[]" ) )
    {
        const s32 KeyIndex = Query.GetIndex( 1 );
        if( ( KeyIndex < 0 ) || ( KeyIndex >= Animation.Keys.GetCount() ) )
        {
            Query.PopPath( Path );
            return FALSE;
        }

        const s32 KeyPath = Query.PushPath( "Key[]\\" );
        s32 Value = 0;
        if( Query.VarInt( "U", Value, 0, 255 ) )
        {
            if( !Query.IsRead() )
                Animation.Keys[KeyIndex].U = (u8)Value;
        }
        else if( Query.VarInt( "V", Value, 0, 255 ) )
        {
            if( !Query.IsRead() )
                Animation.Keys[KeyIndex].V = (u8)Value;
        }
        else
        {
            Query.PopPath( KeyPath );
            Query.PopPath( Path );
            return FALSE;
        }
        Query.PopPath( KeyPath );
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyVirtualMesh( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\VirtualMeshes\\VirtualMesh[]" ) )
        return FALSE;

    s32 const Index = Query.GetIndex( 0 );
    if( ( Index < 0 ) || ( Index >= m_VirtualMeshes.GetCount() ) )
        return FALSE;

    virtual_mesh& Mesh = m_VirtualMeshes[Index];
    s32 const Path = Query.PushPath( "ResDesc\\VirtualMeshes\\VirtualMesh[]\\" );
    if( Query.VarString( "Name", Mesh.Name, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.IsVar( "NumLODs" ) )
    {
        if( !Query.IsRead() )
            Mesh.LODs.SetCount( Query.GetVarInt( 0, 64 ) );
    }
    else if( OnPropertyLOD( Query, Index ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyLOD( prop_query& Query, s32 VirtualMeshIndex )
{
    virtual_mesh& Mesh = m_VirtualMeshes[VirtualMeshIndex];
    s32 const LODIndex = Query.GetIndex( 1 );
    if( ( LODIndex < 0 ) || ( LODIndex >= Mesh.LODs.GetCount() ) )
        return FALSE;

    s32 const Path = Query.PushPath( "LOD[]\\" );
    lod_info& LOD = Mesh.LODs[LODIndex];
    if( Query.VarInt( "NumMeshs", LOD.nMeshes, 0, lod_info::MAX_LOD_MESHES ) )
    {
    }
    else if( Query.VarInt( "ScreenSize", LOD.ScreenSize, 0, S16_MAX ) )
    {
    }
    else if( OnPropertyMesh( Query, VirtualMeshIndex, LODIndex ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyMesh( prop_query& Query, s32 VirtualMeshIndex, s32 LODIndex )
{
    virtual_mesh& Mesh = m_VirtualMeshes[VirtualMeshIndex];
    lod_info& LOD = Mesh.LODs[LODIndex];
    s32 const MeshIndex = Query.GetIndex( 2 );
    if( ( MeshIndex < 0 ) || ( MeshIndex >= LOD.nMeshes ) )
        return FALSE;

    return Query.VarString( "Mesh[]\\Name", LOD.MeshName[MeshIndex], MAX_NAME_LENGTH );
}

//=========================================================================

xbool geometry_descriptor::OnPropertyVirtualTexture( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\VirtualTextures\\VirtualTexture[]" ) )
        return FALSE;

    s32 const Index = Query.GetIndex( 0 );
    if( ( Index < 0 ) || ( Index >= m_VirtualTextures.GetCount() ) )
        return FALSE;

    virtual_texture& Texture = m_VirtualTextures[Index];
    s32 const Path = Query.PushPath( "ResDesc\\VirtualTextures\\VirtualTexture[]\\" );

    if( Query.VarString( "Name", Texture.Name, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.IsVar( "NumDiffTextures" ) )
    {
        if( !Query.IsRead() )
            Texture.Textures.SetCount( Query.GetVarInt( 0, 32 ) );
    }
    else if( Query.VarBool( "OverrideDefault", Texture.OverrideDefault ) )
    {
    }
    else if( Query.VarFileName( "OverrideFileName", Texture.OverrideFileName, MAX_NAME_LENGTH ) )
    {
    }
    else if( OnPropertyDiffuse( Query, Index ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyDiffuse( prop_query& Query, s32 VirtualTextureIndex )
{
    virtual_texture& Texture = m_VirtualTextures[VirtualTextureIndex];
    s32 const DiffuseIndex = Query.GetIndex( 1 );
    if( ( DiffuseIndex < 0 ) || ( DiffuseIndex >= Texture.Textures.GetCount() ) )
        return FALSE;

    s32 const Path = Query.PushPath( "DiffTexture[]\\" );
    texture_info& Diffuse = Texture.Textures[DiffuseIndex];
    if( Query.VarFileName( "FileName", Diffuse.FileName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "Name", Diffuse.Name, MAX_NAME_LENGTH ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyMaterial( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\Materials\\Material[]" ) )
        return FALSE;

    const s32 Index = Query.GetIndex( 0 );
    if( Index < 0 || Index >= m_Materials.GetCount() )
        return FALSE;

    material& Material = m_Materials[Index];
    const s32 Path = Query.PushPath( "ResDesc\\Materials\\Material[]\\" );
    if( Query.VarString( "Name", Material.Name, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarInt( "Type", Material.Type, S32_MIN, S32_MAX ) )
    {
    }
    else if( Query.VarFloat( "DetailScale", Material.DetailScale, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "FixedAlpha", Material.FixedAlpha, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarInt( "Flags", Material.Flags, S32_MIN, S32_MAX ) )
    {
    }
    else if( Query.IsVar( "NumTextures" ) )
    {
        if( !Query.IsRead() )
            Material.Textures.SetCount( Query.GetVarInt( 0, 4096 ) );
    }
    else if( OnPropertyTexture( Query, Index ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertySubMesh( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\SubMeshes\\SubMesh[]" ) )
        return FALSE;

    const s32 Index = Query.GetIndex( 0 );
    if( Index < 0 || Index >= m_SubMeshes.GetCount() )
        return FALSE;

    submesh& SubMesh = m_SubMeshes[Index];
    const s32 Path = Query.PushPath( "ResDesc\\SubMeshes\\SubMesh[]\\" );
    if( Query.VarString( "MeshName", SubMesh.MeshName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "SubMeshName", SubMesh.SubMeshName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "SectionName", SubMesh.SectionName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarInt( "Primitive", SubMesh.Primitive, -1, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "SubMesh", SubMesh.SubMesh, -1, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Section", SubMesh.Section, -1, S32_MAX ) )
    {
    }
    else if( Query.VarString( "MaterialName", SubMesh.MaterialName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarFloat( "WorldPixelSize", SubMesh.WorldPixelSize, 0.0f, F32_MAX ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyTexture( prop_query& Query, s32 MaterialIndex )
{
    material& Material = m_Materials[MaterialIndex];
    const s32 TextureIndex = Query.GetIndex( 1 );
    if( !Query.IsBasePath( "Texture[]" ) || TextureIndex < 0 || TextureIndex >= Material.Textures.GetCount() )
    {
        return FALSE;
    }

    const s32 Path = Query.PushPath( "Texture[]\\" );
    texture& Texture = Material.Textures[TextureIndex];
    if( Query.VarInt( "Slot", Texture.Slot, RUNTIME_TEXTURE_DIFFUSE, RUNTIME_TEXTURE_PUNCH_THROUGH ) )
    {
    }
    else if( Query.VarFileName( "FileName", Texture.FileName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "Name", Texture.Name, MAX_NAME_LENGTH ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }
    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyBone( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\Bones\\Bone[]" ) )
        return FALSE;

    const s32 Index = Query.GetIndex( 0 );
    if( Index < 0 || Index >= m_Bones.GetCount() )
        return FALSE;

    bone& Bone = m_Bones[Index];
    const s32 Path = Query.PushPath( "ResDesc\\Bones\\Bone[]\\" );
    if( Query.VarString( "Name", Bone.Name, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarInt( "Parent", Bone.Parent, -1, resource_tool::skeleton_contract::MaxBones ) )
    {
    }
    else if( Query.VarBBox( "BBox", Bone.BBox ) )
    {
    }
    else if( Query.VarInt( "HitLocation", Bone.HitLocation, S32_MIN, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "RigidBody", Bone.iRigidBody, -1, S32_MAX ) )
    {
    }
    else if( Query.VarString( "RigidBodyName", Bone.RigidBodyName, MAX_NAME_LENGTH ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }
    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyRigidBody( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\RigidBodies\\Body[]" ) )
        return FALSE;

    const s32 Index = Query.GetIndex( 0 );
    if( Index < 0 || Index >= m_RigidBodies.GetCount() )
        return FALSE;

    rigid_body& Body = m_RigidBodies[Index];
    const s32 Path = Query.PushPath( "ResDesc\\RigidBodies\\Body[]\\" );
    if( Query.VarString( "Name", Body.Name, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "Type", Body.Type, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarFloat( "Mass", Body.Mass, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarInt( "Parent", Body.Parent, -1, S32_MAX ) )
    {
    }
    else if( Query.VarString( "ParentName", Body.ParentName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarInt( "Bone", Body.Bone, -1, S32_MAX ) )
    {
    }
    else if( Query.VarString( "BoneName", Body.BoneName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarVector3( "PivotPosition", Body.PivotPosition ) )
    {
    }
    else if( Query.VarVector3( "PivotScale", Body.PivotScale ) )
    {
    }
    else if( Query.VarFloat( "PivotRotationX", Body.PivotRotation.X, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "PivotRotationY", Body.PivotRotation.Y, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "PivotRotationZ", Body.PivotRotation.Z, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "PivotRotationW", Body.PivotRotation.W, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarVector3( "BodyPosition", Body.BodyPosition ) )
    {
    }
    else if( Query.VarVector3( "BodyScale", Body.BodyScale ) )
    {
    }
    else if( Query.VarFloat( "BodyRotationX", Body.BodyRotation.X, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "BodyRotationY", Body.BodyRotation.Y, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "BodyRotationZ", Body.BodyRotation.Z, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "BodyRotationW", Body.BodyRotation.W, -F32_MAX, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "Radius", Body.Radius, 0.0f, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "Width", Body.Width, 0.0f, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "Height", Body.Height, 0.0f, F32_MAX ) )
    {
    }
    else if( Query.VarFloat( "Length", Body.Length, 0.0f, F32_MAX ) )
    {
    }
    else if( Query.VarInt( "Flags", Body.Flags, S32_MIN, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "CollisionMask", Body.CollisionMask, S32_MIN, S32_MAX ) )
    {
    }
    else if( Query.IsBasePath( "DOF[]" ) )
    {
        const s32 DOFIndex = Query.GetIndex( 1 );
        if( DOFIndex < 0 || DOFIndex >= 6 )
        {
            Query.PopPath( Path );
            return FALSE;
        }

        const s32 DOFPath = Query.PushPath( "DOF[]\\" );
        rigid_body::dof& DOF = Body.DOF[DOFIndex];
        if( Query.VarInt( "Active", DOF.Active, 0, 1 ) )
        {
        }
        else if( Query.VarInt( "Limited", DOF.Limited, 0, 1 ) )
        {
        }
        else if( Query.VarFloat( "Min", DOF.Min, -F32_MAX, F32_MAX ) )
        {
        }
        else if( Query.VarFloat( "Max", DOF.Max, -F32_MAX, F32_MAX ) )
        {
        }
        else
        {
            Query.PopPath( DOFPath );
            Query.PopPath( Path );
            return FALSE;
        }
        Query.PopPath( DOFPath );
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }

    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyBoneMask( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\BoneMasks\\Mask[]" ) )
        return FALSE;

    const s32 Index = Query.GetIndex( 0 );
    if( Index < 0 || Index >= m_BoneMasks.GetCount() )
        return FALSE;

    bone_mask& Mask = m_BoneMasks[Index];
    const s32 Path = Query.PushPath( "ResDesc\\BoneMasks\\Mask[]\\" );
    if( Query.VarString( "Name", Mask.Name, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.IsVar( "NumWeights" ) )
    {
        if( !Query.IsRead() )
            Mask.Weights.SetCount( Query.GetVarInt( 0, resource_tool::skeleton_contract::MaxBones ) );
    }
    else if( OnPropertyWeight( Query, Index ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }
    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyWeight( prop_query& Query, s32 MaskIndex )
{
    bone_mask& Mask = m_BoneMasks[MaskIndex];
    const s32 WeightIndex = Query.GetIndex( 1 );
    if( !Query.IsBasePath( "Weight[]" ) || WeightIndex < 0 || WeightIndex >= Mask.Weights.GetCount() )
    {
        return FALSE;
    }

    const s32 Path = Query.PushPath( "Weight[]\\" );
    bone_mask::weight& Weight = Mask.Weights[WeightIndex];
    if( Query.VarInt( "Bone", Weight.Bone, -1, resource_tool::skeleton_contract::MaxBones ) )
    {
    }
    else if( Query.VarFloat( "Value", Weight.Value, -F32_MAX, F32_MAX ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }
    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyProperty( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\Properties\\Property[]" ) )
        return FALSE;

    const s32 Index = Query.GetIndex( 0 );
    if( Index < 0 || Index >= m_Properties.GetCount() )
        return FALSE;

    property& Property = m_Properties[Index];
    const s32 Path = Query.PushPath( "ResDesc\\Properties\\Property[]\\" );
    if( Query.VarString( "Section", Property.Section, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "Name", Property.Name, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "Type", Property.Type, sizeof( Property.Type ) ) )
    {
    }
    else if( Query.VarString( "Value", Property.Value, sizeof( Property.Value ) ) )
    {
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }
    Query.PopPath( Path );
    return TRUE;
}

//=========================================================================

xbool geometry_descriptor::OnPropertyCollisionCluster( prop_query& Query )
{
    if( !Query.IsBasePath( "ResDesc\\CollisionMetadata\\Cluster[]" ) )
        return FALSE;

    const s32 Index = Query.GetIndex( 0 );
    if( Index < 0 || Index >= m_CollisionClusters.GetCount() )
        return FALSE;

    collision_cluster& Cluster = m_CollisionClusters[Index];
    const s32 Path = Query.PushPath( "ResDesc\\CollisionMetadata\\Cluster[]\\" );
    if( Query.VarBool( "High", Cluster.High ) )
    {
    }
    else if( Query.VarBBox( "BBox", Cluster.BBox ) )
    {
    }
    else if( Query.VarString( "MeshName", Cluster.MeshName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "BoneName", Cluster.BoneName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarString( "SectionName", Cluster.SectionName, MAX_NAME_LENGTH ) )
    {
    }
    else if( Query.VarInt( "Mesh", Cluster.Mesh, -1, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Bone", Cluster.Bone, -1, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Section", Cluster.Section, -1, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Offset", Cluster.Offset, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Count", Cluster.Count, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "VectorOffset", Cluster.VectorOffset, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Points", Cluster.Points, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Normals", Cluster.Normals, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "QuadOffset", Cluster.QuadOffset, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "SoundType", Cluster.SoundType, 0, S32_MAX ) )
    {
    }
    else if( Query.VarInt( "Flags", Cluster.Flags, 0, S32_MAX ) )
    {
    }
    else if( Query.IsVar( "NumTriangleOffsets" ) )
    {
        if( !Query.IsRead() )
            Cluster.TriangleOffsets.SetCount( Query.GetVarInt( 0, 1000000 ) );
    }
    else if( Query.IsBasePath( "TriangleOffset[]" ) )
    {
        const s32 OffsetIndex = Query.GetIndex( 1 );
        if( OffsetIndex < 0 || OffsetIndex >= Cluster.TriangleOffsets.GetCount() )
        {
            Query.PopPath( Path );
            return FALSE;
        }

        const s32 OffsetPath = Query.PushPath( "TriangleOffset[]\\" );
        if( Query.VarInt( "Value", Cluster.TriangleOffsets[OffsetIndex], 0, 0xffff ) )
        {
        }
        else
        {
            Query.PopPath( OffsetPath );
            Query.PopPath( Path );
            return FALSE;
        }
        Query.PopPath( OffsetPath );
    }
    else if( Query.IsVar( "NumQuadFlags" ) )
    {
        if( !Query.IsRead() )
            Cluster.QuadFlags.SetCount( Query.GetVarInt( 0, 1000000 ) );
    }
    else if( Query.IsBasePath( "QuadFlag[]" ) )
    {
        const s32 FlagIndex = Query.GetIndex( 1 );
        if( FlagIndex < 0 || FlagIndex >= Cluster.QuadFlags.GetCount() )
        {
            Query.PopPath( Path );
            return FALSE;
        }

        const s32 FlagPath = Query.PushPath( "QuadFlag[]\\" );
        if( Query.VarInt( "Value", Cluster.QuadFlags[FlagIndex], 0, 255 ) )
        {
        }
        else
        {
            Query.PopPath( FlagPath );
            Query.PopPath( Path );
            return FALSE;
        }
        Query.PopPath( FlagPath );
    }
    else
    {
        Query.PopPath( Path );
        return FALSE;
    }
    Query.PopPath( Path );
    return TRUE;
}
