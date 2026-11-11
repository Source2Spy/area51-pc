//=========================================================================
//
//  GeomFile.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeomFile.hpp"
#include "Auxiliary/MiscUtils/BitseryIO.hpp"
#include "RigidGeom.hpp"
#include "SkinGeom.hpp"

//=========================================================================
//  CONSTANTS
//=========================================================================

enum
{
    MaxSmallArrayCount = ( 1 << 20 ),
    MaxLargeArrayCount = ( 1 << 25 ),
    MaxStringDataSize  = ( 1 << 27 ),
};

//-------------------------------------------------------------------------

static bitsery_io::file_format const RigidFileFormat =
{
    { 'R', 'I', 'G', 'M' },
    geom_file::VERSION,
};

//-------------------------------------------------------------------------

static bitsery_io::file_format const SkinFileFormat =
{
    { 'S', 'K', 'N', 'M' },
    geom_file::VERSION,
};

//=========================================================================
//  TYPES
//=========================================================================

typedef bitsery::Serializer<bitsery_io::output_adapter> output_serializer;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static xbool RangeIsValid( s32 First, s32 Count, s32 ArrayCount )
{
    return ( First >= 0 ) && ( Count >= 0 ) && ( First <= ArrayCount ) && ( Count <= ( ArrayCount - First ) );
}

//=========================================================================

static xbool IsIndexOrMinusOne( s32 Index, s32 ArrayCount )
{
    return ( Index == -1 ) || ( ( Index >= 0 ) && ( Index < ArrayCount ) );
}

//=========================================================================

static xbool HasString( geom const& Geom, s32 Offset )
{
    if( ( Offset < 0 ) || ( Offset >= Geom.m_StringDataSize ) )
    {
        return FALSE;
    }

    for( s32 Index = Offset; Index < Geom.m_StringDataSize; Index++ )
    {
        if( Geom.m_pStringData[Index] == 0 )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================
//  VALUE SERIALIZATION
//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, vector2& Value )
{
    Serializer.value4b( Value.X );
    Serializer.value4b( Value.Y );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, vector3p& Value )
{
    Serializer.value4b( Value.X );
    Serializer.value4b( Value.Y );
    Serializer.value4b( Value.Z );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, vector3& Value )
{
    Serializer.value4b( Value.GetX() );
    Serializer.value4b( Value.GetY() );
    Serializer.value4b( Value.GetZ() );
    Value.GetIW() = 0;
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, quaternion& Value )
{
    Serializer.value4b( Value.X );
    Serializer.value4b( Value.Y );
    Serializer.value4b( Value.Z );
    Serializer.value4b( Value.W );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, bbox& Value )
{
    Serializer.object( Value.Min );
    Serializer.object( Value.Max );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::bone& Value )
{
    Serializer.object( Value.BindRotation );
    Serializer.object( Value.BindPosition );
    Serializer.object( Value.BBox );
    Serializer.value2b( Value.HitLocation );
    Serializer.value2b( Value.iRigidBody );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::property_section& Value )
{
    bitsery_io::ReadS32( Serializer, Value.NameOffset );
    bitsery_io::ReadS32( Serializer, Value.iProperty );
    bitsery_io::ReadS32( Serializer, Value.nProperties );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::property& Value )
{
    u8 type = 0;

    bitsery_io::ReadS32( Serializer, Value.NameOffset );
    Serializer.value1b( type );
    Serializer.value4b( Value.Value.Integer );

    Value.Type = type;
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::rigid_body::dof& Value )
{
    Serializer.value4b( Value.Flags );
    Serializer.value4b( Value.Min );
    Serializer.value4b( Value.Max );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::rigid_body& Value )
{
    Serializer.object( Value.BodyBindRotation );
    Serializer.object( Value.BodyBindPosition );
    Serializer.object( Value.PivotBindRotation );
    Serializer.object( Value.PivotBindPosition );
    bitsery_io::ReadS32( Serializer, Value.NameOffset );
    Serializer.value4b( Value.Mass );
    Serializer.value4b( Value.Radius );
    Serializer.value4b( Value.Width );
    Serializer.value4b( Value.Height );
    Serializer.value4b( Value.Length );
    Serializer.value2b( Value.Type );
    Serializer.value2b( Value.Flags );
    Serializer.value2b( Value.iParentBody );
    Serializer.value2b( Value.iBone );
    Serializer.value4b( Value.CollisionMask );

    for( s32 i = 0; i < 6; i++ )
    {
        Serializer.object( Value.DOF[i] );
    }
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::mesh& Value )
{
    Serializer.object( Value.BBox );
    bitsery_io::ReadS32( Serializer, Value.NameOffset );
    bitsery_io::ReadS32( Serializer, Value.iSubMesh );
    bitsery_io::ReadS32( Serializer, Value.nSubMeshes );
    bitsery_io::ReadS32( Serializer, Value.nBones );
    bitsery_io::ReadS32( Serializer, Value.nFaces );
    bitsery_io::ReadS32( Serializer, Value.nVertices );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::submesh& Value )
{
    bitsery_io::ReadS32( Serializer, Value.iSection );
    bitsery_io::ReadS32( Serializer, Value.nSections );
    bitsery_io::ReadS32( Serializer, Value.iMaterial );
    Serializer.value4b( Value.WorldPixelSize );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::material::uvanim& Value )
{
    Serializer.value1b( Value.Type );
    Serializer.value1b( Value.StartFrame );
    Serializer.value1b( Value.FPS );
    bitsery_io::ReadS32( Serializer, Value.iKey );
    bitsery_io::ReadS32( Serializer, Value.nKeys );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::material& Value )
{
    Serializer.object( Value.UVAnim );
    Serializer.value4b( Value.DetailScale );
    Serializer.value4b( Value.FixedAlpha );
    Serializer.value2b( Value.Flags );
    Serializer.value1b( Value.Type );
    bitsery_io::ReadS32( Serializer, Value.iTexture );
    bitsery_io::ReadS32( Serializer, Value.nTextures );
    bitsery_io::ReadS32( Serializer, Value.iVirtualMat );
    bitsery_io::ReadS32( Serializer, Value.nVirtualMats );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::texture& Value )
{
    bitsery_io::ReadS32( Serializer, Value.DescOffset );
    bitsery_io::ReadS32( Serializer, Value.FileNameOffset );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::uvkey& Value )
{
    Serializer.value1b( Value.OffsetU );
    Serializer.value1b( Value.OffsetV );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::virtual_mesh& Value )
{
    bitsery_io::ReadS32( Serializer, Value.NameOffset );
    bitsery_io::ReadS32( Serializer, Value.iLOD );
    bitsery_io::ReadS32( Serializer, Value.nLODs );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, geom::virtual_texture& Value )
{
    bitsery_io::ReadS32( Serializer, Value.NameOffset );
    Serializer.value4b( Value.MaterialMask );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, collision_data::mat_info& Value )
{
    Serializer.value2b( Value.SoundType );
    Serializer.value2b( Value.Flags );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, collision_data::high_cluster& Value )
{
    Serializer.object( Value.BBox );
    bitsery_io::ReadS32( Serializer, Value.nTris );
    Serializer.value4b( Value.iMesh );
    Serializer.value4b( Value.iBone );
    Serializer.value4b( Value.iSection );
    bitsery_io::ReadS32( Serializer, Value.iOffset );
    Serializer.object( Value.MaterialInfo );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, collision_data::low_cluster& Value )
{
    Serializer.object( Value.BBox );
    bitsery_io::ReadS32( Serializer, Value.iVectorOffset );
    bitsery_io::ReadS32( Serializer, Value.nPoints );
    bitsery_io::ReadS32( Serializer, Value.nNormals );
    bitsery_io::ReadS32( Serializer, Value.iQuadOffset );
    bitsery_io::ReadS32( Serializer, Value.nQuads );
    Serializer.value4b( Value.iMesh );
    Serializer.value4b( Value.iBone );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, collision_data::low_quad& Value )
{
    for( s32 i = 0; i < 4; i++ )
    {
        Serializer.value1b( Value.iP[i] );
    }

    Serializer.value1b( Value.iN );
    Serializer.value1b( Value.Flags );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, rigid_geom::vertex& Value )
{
    Serializer.object( Value.Pos );
    Serializer.object( Value.Normal );
    Serializer.object( Value.UV );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, skin_geom::vertex& Value )
{
    Serializer.object( Value.Position );
    Serializer.object( Value.Normal );
    Serializer.object( Value.UV );
    Serializer.value4b( Value.Weights.X );
    Serializer.value4b( Value.Weights.Y );
    Serializer.value2b( Value.Bones[0] );
    Serializer.value2b( Value.Bones[1] );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, rigid_geom::section& Value )
{
    bitsery_io::ReadS32( Serializer, Value.FirstVertex );
    bitsery_io::ReadS32( Serializer, Value.nVertices );
    bitsery_io::ReadS32( Serializer, Value.FirstIndex );
    bitsery_io::ReadS32( Serializer, Value.nIndices );
    Serializer.value4b( Value.iBone );
    Serializer.value4b( Value.iColor );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, skin_geom::section& Value )
{
    bitsery_io::ReadS32( Serializer, Value.FirstVertex );
    bitsery_io::ReadS32( Serializer, Value.nVertices );
    bitsery_io::ReadS32( Serializer, Value.FirstIndex );
    bitsery_io::ReadS32( Serializer, Value.nIndices );
    bitsery_io::ReadS32( Serializer, Value.FirstBone );
    bitsery_io::ReadS32( Serializer, Value.nBones );
}

//=========================================================================
//  GEOMETRY DESERIALIZATION
//=========================================================================

template <class SERIALIZER>
static void ReadBoneMasks( SERIALIZER& Serializer, geom& Geom )
{
    u32 MaskCount = 0;
    Serializer.value4b( MaskCount );

    if( MaskCount > MaxSmallArrayCount )
    {
        bitsery_io::SetInvalidData( Serializer );
        return;
    }

    Geom.m_nBoneMasks = static_cast<s32>( MaskCount );
    Geom.m_pBoneMasks = Geom.m_nBoneMasks > 0 ? new geom::bone_masks[Geom.m_nBoneMasks] : nullptr;

    xarray<u32> FirstWeights;
    xarray<u32> WeightCounts;
    FirstWeights.SetCount( Geom.m_nBoneMasks );
    WeightCounts.SetCount( Geom.m_nBoneMasks );

    for( s32 i = 0; i < Geom.m_nBoneMasks; i++ )
    {
        u32 NameOffset = 0;
        Serializer.value4b( NameOffset );
        Serializer.value4b( FirstWeights[i] );
        Serializer.value4b( WeightCounts[i] );

        if( NameOffset > 0x7fffffffu )
        {
            bitsery_io::SetInvalidData( Serializer );
            return;
        }

        Geom.m_pBoneMasks[i].NameOffset = static_cast<s32>( NameOffset );
        Geom.m_pBoneMasks[i].nBones = 0;
        x_memset( Geom.m_pBoneMasks[i].Weights, 0, sizeof( Geom.m_pBoneMasks[i].Weights ) );
    }

    f32* pWeights = nullptr;
    s32  nWeights = 0;
    bitsery_io::ReadValueArray<4>( Serializer, pWeights, nWeights, MaxLargeArrayCount );

    for( s32 i = 0; i < Geom.m_nBoneMasks; i++ )
    {
        if( ( WeightCounts[i] > MAX_ANIM_BONES ) || ( FirstWeights[i] > static_cast<u32>( nWeights ) ) ||
             ( WeightCounts[i] > ( static_cast<u32>( nWeights ) - FirstWeights[i] ) ) )
        {
            delete[] pWeights;
            bitsery_io::SetInvalidData( Serializer );
            return;
        }

        Geom.m_pBoneMasks[i].nBones = static_cast<s32>( WeightCounts[i] );
        for( s32 j = 0; j < Geom.m_pBoneMasks[i].nBones; j++ )
        {
            Geom.m_pBoneMasks[i].Weights[j] = pWeights[FirstWeights[i] + j];
        }
    }

    delete[] pWeights;
}

//=========================================================================

template <class SERIALIZER>
static void ReadCommonGeom( SERIALIZER& Serializer, geom& Geom )
{
    Serializer.object( Geom.m_BBox );
    bitsery_io::ReadS32( Serializer, Geom.m_nFaces );
    bitsery_io::ReadS32( Serializer, Geom.m_nVertices );
    bitsery_io::ReadS32( Serializer, Geom.m_nVirtualMaterials );
    if( Geom.m_nVirtualMaterials > MaxSmallArrayCount )
    {
        bitsery_io::SetInvalidData( Serializer );
        return;
    }

    bitsery_io::ReadObjectArray( Serializer, Geom.m_pBone, Geom.m_nBones, MaxSmallArrayCount );
    ReadBoneMasks( Serializer, Geom );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pPropertySections, Geom.m_nPropertySections, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pProperties, Geom.m_nProperties, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pRigidBodies, Geom.m_nRigidBodies, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pMesh, Geom.m_nMeshes, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pSubMesh, Geom.m_nSubMeshes, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pMaterial, Geom.m_nMaterials, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pTexture, Geom.m_nTextures, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pUVKey, Geom.m_nUVKeys, MaxLargeArrayCount );

    s32 LodMaskCount = 0;
    bitsery_io::ReadValueArray<2>( Serializer, Geom.m_pLODSizes, Geom.m_nLODs, MaxSmallArrayCount );
    bitsery_io::ReadValueArray<8>( Serializer, Geom.m_pLODMasks, LodMaskCount, MaxSmallArrayCount );
    if( LodMaskCount != Geom.m_nLODs )
    {
        bitsery_io::SetInvalidData( Serializer );
        return;
    }

    bitsery_io::ReadObjectArray( Serializer, Geom.m_pVirtualMeshes, Geom.m_nVirtualMeshes, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pVirtualTextures, Geom.m_nVirtualTextures, MaxSmallArrayCount );
    bitsery_io::ReadValueArray<1>( Serializer, Geom.m_pStringData, Geom.m_StringDataSize, MaxStringDataSize );
}

//=========================================================================

template <class SERIALIZER>
static void ReadCollision( SERIALIZER& Serializer, collision_data& Collision )
{
    Serializer.object( Collision.BBox );
    bitsery_io::ReadObjectArray( Serializer, Collision.pHighCluster, Collision.nHighClusters, MaxSmallArrayCount );
    bitsery_io::ReadValueArray<2>( Serializer, Collision.pHighIndexToVert0, Collision.nHighIndices, MaxLargeArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Collision.pLowCluster, Collision.nLowClusters, MaxSmallArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Collision.pLowVector, Collision.nLowVectors, MaxLargeArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Collision.pLowQuad, Collision.nLowQuads, MaxLargeArrayCount );
}

//=========================================================================

template <class SERIALIZER>
static void ReadEmptyCollision( SERIALIZER& Serializer )
{
    bbox ignored;
    Serializer.object( ignored );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadEmptyArray( Serializer );
}

//=========================================================================
//  GEOMETRY VALIDATION
//=========================================================================

static xbool ValidateCommon( geom const& Geom )
{
    if( ( Geom.m_nFaces < 0 ) || ( Geom.m_nVertices < 0 ) || ( Geom.m_nVirtualMaterials < 0 ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < Geom.m_nBones; i++ )
    {
        if( !IsIndexOrMinusOne( Geom.m_pBone[i].iRigidBody, Geom.m_nRigidBodies ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nBoneMasks; i++ )
    {
        if( !HasString( Geom, Geom.m_pBoneMasks[i].NameOffset ) || ( Geom.m_pBoneMasks[i].nBones < 0 ) ||
             ( Geom.m_pBoneMasks[i].nBones > MAX_ANIM_BONES ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nPropertySections; i++ )
    {
        geom::property_section const& section = Geom.m_pPropertySections[i];
        if( !HasString( Geom, section.NameOffset ) ||
             !RangeIsValid( section.iProperty, section.nProperties, Geom.m_nProperties ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nProperties; i++ )
    {
        geom::property const& property = Geom.m_pProperties[i];
        if( !HasString( Geom, property.NameOffset ) || ( property.Type < 0 ) ||
             ( property.Type >= geom::property::TYPE_TOTAL ) )
        {
            return FALSE;
        }

        if( ( property.Type == geom::property::TYPE_STRING ) && !HasString( Geom, property.Value.StringOffset ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nRigidBodies; i++ )
    {
        geom::rigid_body const& body = Geom.m_pRigidBodies[i];
        if( !HasString( Geom, body.NameOffset ) || !IsIndexOrMinusOne( body.iParentBody, Geom.m_nRigidBodies ) ||
             !IsIndexOrMinusOne( body.iBone, Geom.m_nBones ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nMeshes; i++ )
    {
        geom::mesh const& mesh = Geom.m_pMesh[i];
        if( !HasString( Geom, mesh.NameOffset ) || !RangeIsValid( mesh.iSubMesh, mesh.nSubMeshes, Geom.m_nSubMeshes ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nMaterials; i++ )
    {
        geom::material const& material = Geom.m_pMaterial[i];
        if( !RangeIsValid( material.iTexture, material.nTextures, Geom.m_nTextures ) ||
             !RangeIsValid( material.iVirtualMat, material.nVirtualMats, Geom.m_nVirtualMaterials ) ||
             !RangeIsValid( material.UVAnim.iKey, material.UVAnim.nKeys, Geom.m_nUVKeys ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nTextures; i++ )
    {
        if( !HasString( Geom, Geom.m_pTexture[i].DescOffset ) ||
             !HasString( Geom, Geom.m_pTexture[i].FileNameOffset ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nVirtualMeshes; i++ )
    {
        geom::virtual_mesh const& mesh = Geom.m_pVirtualMeshes[i];
        if( !HasString( Geom, mesh.NameOffset ) || !RangeIsValid( mesh.iLOD, mesh.nLODs, Geom.m_nLODs ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nVirtualTextures; i++ )
    {
        if( !HasString( Geom, Geom.m_pVirtualTextures[i].NameOffset ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

static xbool ValidateRigid( rigid_geom const& Geom )
{
    if( !ValidateCommon( Geom ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < Geom.m_nSubMeshes; i++ )
    {
        geom::submesh const& submesh = Geom.m_pSubMesh[i];
        if( !RangeIsValid( submesh.iSection, submesh.nSections, Geom.m_nSections ) || ( submesh.iMaterial < 0 ) ||
             ( submesh.iMaterial >= Geom.m_nMaterials ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nSections; i++ )
    {
        rigid_geom::section const& section = Geom.m_pSection[i];
        if( !RangeIsValid( section.FirstVertex, section.nVertices, Geom.m_nVertexData ) ||
             !RangeIsValid( section.FirstIndex, section.nIndices, Geom.m_nIndices ) ||
             !IsIndexOrMinusOne( section.iBone, Geom.m_nBones ) )
        {
            return FALSE;
        }

        for( s32 j = 0; j < section.nIndices; j++ )
        {
            u32 const VertexIndex = Geom.m_pIndex[section.FirstIndex + j];
            if( ( VertexIndex < static_cast<u32>( section.FirstVertex ) ) ||
                 ( VertexIndex >= static_cast<u32>( section.FirstVertex + section.nVertices ) ) )
            {
                return FALSE;
            }
        }
    }

    for( s32 i = 0; i < Geom.m_Collision.nHighClusters; i++ )
    {
        collision_data::high_cluster const& cluster = Geom.m_Collision.pHighCluster[i];

        if( !RangeIsValid( cluster.iOffset, cluster.nTris, Geom.m_Collision.nHighIndices ) ||
             !IsIndexOrMinusOne( cluster.iMesh, Geom.m_nMeshes ) ||
             !IsIndexOrMinusOne( cluster.iBone, Geom.m_nBones ) ||
             !IsIndexOrMinusOne( cluster.iSection, Geom.m_nSections ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_Collision.nLowClusters; i++ )
    {
        collision_data::low_cluster const& cluster = Geom.m_Collision.pLowCluster[i];
        u64 const VectorCount = static_cast<u64>( cluster.nPoints ) + static_cast<u64>( cluster.nNormals );

        if( ( VectorCount > 0x7fffffffu ) ||
             !RangeIsValid( cluster.iVectorOffset, static_cast<s32>( VectorCount ), Geom.m_Collision.nLowVectors ) ||
             !RangeIsValid( cluster.iQuadOffset, cluster.nQuads, Geom.m_Collision.nLowQuads ) ||
             !IsIndexOrMinusOne( cluster.iMesh, Geom.m_nMeshes ) || !IsIndexOrMinusOne( cluster.iBone, Geom.m_nBones ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

static xbool ValidateSkin( skin_geom const& Geom )
{
    if( !ValidateCommon( Geom ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < Geom.m_nSubMeshes; i++ )
    {
        geom::submesh const& submesh = Geom.m_pSubMesh[i];
        if( !RangeIsValid( submesh.iSection, submesh.nSections, Geom.m_nSections ) || ( submesh.iMaterial < 0 ) ||
             ( submesh.iMaterial >= Geom.m_nMaterials ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < Geom.m_nSections; i++ )
    {
        skin_geom::section const& section = Geom.m_pSection[i];
        if( !RangeIsValid( section.FirstVertex, section.nVertices, Geom.m_nVertexData ) ||
             !RangeIsValid( section.FirstIndex, section.nIndices, Geom.m_nIndices ) ||
             !RangeIsValid( section.FirstBone, section.nBones, Geom.m_nBonePalette ) )
        {
            return FALSE;
        }

        for( s32 j = 0; j < section.nIndices; j++ )
        {
            u32 const VertexIndex = Geom.m_pIndex[section.FirstIndex + j];
            if( ( VertexIndex < static_cast<u32>( section.FirstVertex ) ) ||
                 ( VertexIndex >= static_cast<u32>( section.FirstVertex + section.nVertices ) ) )
            {
                return FALSE;
            }

            skin_geom::vertex const& vertex = Geom.m_pVertex[VertexIndex];
            for( s32 WeightIndex = 0; WeightIndex < 2; WeightIndex++ )
            {
                if( vertex.Weights[WeightIndex] == 0.0f )
                {
                    continue;
                }

                u16 const PaletteIndex = vertex.Bones[WeightIndex];
                if( ( PaletteIndex >= section.nBones ) ||
                     ( Geom.m_pBonePalette[section.FirstBone + PaletteIndex] == 0xffffu ) )
                {
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

//=========================================================================
//  ARCHIVE SERIALIZATION
//=========================================================================

static void WriteS32( output_serializer& Serializer, s32 Value )
{
    u32 WireValue = static_cast<u32>( Value );
    Serializer.value4b( WireValue );
}

//=========================================================================

template <usize SIZE, class TYPE>
static void WriteValueArray( output_serializer& Serializer, TYPE* pArray, s32 count )
{
    u32 WireCount = static_cast<u32>( count );
    Serializer.value4b( WireCount );

    for( s32 i = 0; i < count; i++ )
    {
        Serializer.template value<SIZE>( pArray[i] );
    }
}

//=========================================================================

template <class TYPE>
static void WriteObjectArray( output_serializer& Serializer, TYPE* pArray, s32 count )
{
    u32 WireCount = static_cast<u32>( count );
    Serializer.value4b( WireCount );

    for( s32 i = 0; i < count; i++ )
    {
        Serializer.object( pArray[i] );
    }
}

//=========================================================================

static void WriteEmptyArray( output_serializer& Serializer )
{
    u32 count = 0;
    Serializer.value4b( count );
}

//=========================================================================

static void WriteBoneMasks( output_serializer& Serializer, geom& Geom )
{
    u32 MaskCount = static_cast<u32>( Geom.m_nBoneMasks );
    Serializer.value4b( MaskCount );

    u32 FirstWeight = 0;
    for( s32 i = 0; i < Geom.m_nBoneMasks; i++ )
    {
        geom::bone_masks& mask = Geom.m_pBoneMasks[i];
        u32               NameOffset = static_cast<u32>( mask.NameOffset );
        u32               WeightCount = static_cast<u32>( mask.nBones );

        Serializer.value4b( NameOffset );
        Serializer.value4b( FirstWeight );
        Serializer.value4b( WeightCount );
        FirstWeight += WeightCount;
    }

    Serializer.value4b( FirstWeight );
    for( s32 i = 0; i < Geom.m_nBoneMasks; i++ )
    {
        geom::bone_masks& mask = Geom.m_pBoneMasks[i];
        for( s32 j = 0; j < mask.nBones; j++ )
        {
            Serializer.value4b( mask.Weights[j] );
        }
    }
}

//=========================================================================

static void WriteCommonGeom( output_serializer& Serializer, geom& Geom )
{
    Serializer.object( Geom.m_BBox );
    WriteS32( Serializer, Geom.m_nFaces );
    WriteS32( Serializer, Geom.m_nVertices );
    WriteS32( Serializer, Geom.m_nVirtualMaterials );

    WriteObjectArray( Serializer, Geom.m_pBone, Geom.m_nBones );
    WriteBoneMasks( Serializer, Geom );
    WriteObjectArray( Serializer, Geom.m_pPropertySections, Geom.m_nPropertySections );
    WriteObjectArray( Serializer, Geom.m_pProperties, Geom.m_nProperties );
    WriteObjectArray( Serializer, Geom.m_pRigidBodies, Geom.m_nRigidBodies );
    WriteObjectArray( Serializer, Geom.m_pMesh, Geom.m_nMeshes );
    WriteObjectArray( Serializer, Geom.m_pSubMesh, Geom.m_nSubMeshes );
    WriteObjectArray( Serializer, Geom.m_pMaterial, Geom.m_nMaterials );
    WriteObjectArray( Serializer, Geom.m_pTexture, Geom.m_nTextures );
    WriteObjectArray( Serializer, Geom.m_pUVKey, Geom.m_nUVKeys );
    WriteValueArray<2>( Serializer, Geom.m_pLODSizes, Geom.m_nLODs );
    WriteValueArray<8>( Serializer, Geom.m_pLODMasks, Geom.m_nLODs );
    WriteObjectArray( Serializer, Geom.m_pVirtualMeshes, Geom.m_nVirtualMeshes );
    WriteObjectArray( Serializer, Geom.m_pVirtualTextures, Geom.m_nVirtualTextures );
    WriteValueArray<1>( Serializer, Geom.m_pStringData, Geom.m_StringDataSize );
}

//=========================================================================

static void WriteCollision( output_serializer& Serializer, collision_data& Collision )
{
    Serializer.object( Collision.BBox );
    WriteObjectArray( Serializer, Collision.pHighCluster, Collision.nHighClusters );
    WriteValueArray<2>( Serializer, Collision.pHighIndexToVert0, Collision.nHighIndices );
    WriteObjectArray( Serializer, Collision.pLowCluster, Collision.nLowClusters );
    WriteObjectArray( Serializer, Collision.pLowVector, Collision.nLowVectors );
    WriteObjectArray( Serializer, Collision.pLowQuad, Collision.nLowQuads );
}

//=========================================================================

static void WriteEmptyCollision( output_serializer& Serializer )
{
    bbox EmptyBBox;
    EmptyBBox.Min.Set( 0.0f, 0.0f, 0.0f );
    EmptyBBox.Max.Set( 0.0f, 0.0f, 0.0f );
    Serializer.object( EmptyBBox );
    WriteEmptyArray( Serializer );
    WriteEmptyArray( Serializer );
    WriteEmptyArray( Serializer );
    WriteEmptyArray( Serializer );
    WriteEmptyArray( Serializer );
}

//=========================================================================
//  OUTPUT OBJECT SERIALIZATION
//=========================================================================

static void serialize( output_serializer& Serializer, geom::property_section& Value )
{
    WriteS32( Serializer, Value.NameOffset );
    WriteS32( Serializer, Value.iProperty );
    WriteS32( Serializer, Value.nProperties );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::property& Value )
{
    WriteS32( Serializer, Value.NameOffset );
    u8 type = static_cast<u8>( Value.Type );
    Serializer.value1b( type );
    Serializer.value4b( Value.Value.Integer );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::rigid_body& Value )
{
    Serializer.object( Value.BodyBindRotation );
    Serializer.object( Value.BodyBindPosition );
    Serializer.object( Value.PivotBindRotation );
    Serializer.object( Value.PivotBindPosition );
    WriteS32( Serializer, Value.NameOffset );
    Serializer.value4b( Value.Mass );
    Serializer.value4b( Value.Radius );
    Serializer.value4b( Value.Width );
    Serializer.value4b( Value.Height );
    Serializer.value4b( Value.Length );
    Serializer.value2b( Value.Type );
    Serializer.value2b( Value.Flags );
    Serializer.value2b( Value.iParentBody );
    Serializer.value2b( Value.iBone );
    Serializer.value4b( Value.CollisionMask );

    for( s32 i = 0; i < 6; i++ )
    {
        Serializer.object( Value.DOF[i] );
    }
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::mesh& Value )
{
    Serializer.object( Value.BBox );
    WriteS32( Serializer, Value.NameOffset );
    WriteS32( Serializer, Value.iSubMesh );
    WriteS32( Serializer, Value.nSubMeshes );
    WriteS32( Serializer, Value.nBones );
    WriteS32( Serializer, Value.nFaces );
    WriteS32( Serializer, Value.nVertices );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::submesh& Value )
{
    WriteS32( Serializer, Value.iSection );
    WriteS32( Serializer, Value.nSections );
    WriteS32( Serializer, Value.iMaterial );
    Serializer.value4b( Value.WorldPixelSize );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::material::uvanim& Value )
{
    Serializer.value1b( Value.Type );
    Serializer.value1b( Value.StartFrame );
    Serializer.value1b( Value.FPS );
    WriteS32( Serializer, Value.iKey );
    WriteS32( Serializer, Value.nKeys );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::material& Value )
{
    Serializer.object( Value.UVAnim );
    Serializer.value4b( Value.DetailScale );
    Serializer.value4b( Value.FixedAlpha );
    Serializer.value2b( Value.Flags );
    Serializer.value1b( Value.Type );
    WriteS32( Serializer, Value.iTexture );
    WriteS32( Serializer, Value.nTextures );
    WriteS32( Serializer, Value.iVirtualMat );
    WriteS32( Serializer, Value.nVirtualMats );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::texture& Value )
{
    WriteS32( Serializer, Value.DescOffset );
    WriteS32( Serializer, Value.FileNameOffset );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::virtual_mesh& Value )
{
    WriteS32( Serializer, Value.NameOffset );
    WriteS32( Serializer, Value.iLOD );
    WriteS32( Serializer, Value.nLODs );
}

//=========================================================================

static void serialize( output_serializer& Serializer, geom::virtual_texture& Value )
{
    WriteS32( Serializer, Value.NameOffset );
    Serializer.value4b( Value.MaterialMask );
}

//=========================================================================

static void serialize( output_serializer& Serializer, collision_data::high_cluster& Value )
{
    Serializer.object( Value.BBox );
    WriteS32( Serializer, Value.nTris );
    Serializer.value4b( Value.iMesh );
    Serializer.value4b( Value.iBone );
    Serializer.value4b( Value.iSection );
    WriteS32( Serializer, Value.iOffset );
    Serializer.object( Value.MaterialInfo );
}

//=========================================================================

static void serialize( output_serializer& Serializer, collision_data::low_cluster& Value )
{
    Serializer.object( Value.BBox );
    WriteS32( Serializer, Value.iVectorOffset );
    WriteS32( Serializer, Value.nPoints );
    WriteS32( Serializer, Value.nNormals );
    WriteS32( Serializer, Value.iQuadOffset );
    WriteS32( Serializer, Value.nQuads );
    Serializer.value4b( Value.iMesh );
    Serializer.value4b( Value.iBone );
}

//=========================================================================

static void serialize( output_serializer& Serializer, rigid_geom::section& Value )
{
    WriteS32( Serializer, Value.FirstVertex );
    WriteS32( Serializer, Value.nVertices );
    WriteS32( Serializer, Value.FirstIndex );
    WriteS32( Serializer, Value.nIndices );
    Serializer.value4b( Value.iBone );
    Serializer.value4b( Value.iColor );
}

//=========================================================================

static void serialize( output_serializer& Serializer, skin_geom::section& Value )
{
    WriteS32( Serializer, Value.FirstVertex );
    WriteS32( Serializer, Value.nVertices );
    WriteS32( Serializer, Value.FirstIndex );
    WriteS32( Serializer, Value.nIndices );
    WriteS32( Serializer, Value.FirstBone );
    WriteS32( Serializer, Value.nBones );
}

//=========================================================================

static void serialize( output_serializer& Serializer, rigid_geom& Geom )
{
    WriteCommonGeom( Serializer, Geom );
    WriteCollision( Serializer, Geom.m_Collision );
    WriteValueArray<4>( Serializer, Geom.m_pIndex, Geom.m_nIndices );
    WriteObjectArray( Serializer, Geom.m_pVertex, Geom.m_nVertexData );
    WriteEmptyArray( Serializer );
    WriteObjectArray( Serializer, Geom.m_pSection, Geom.m_nSections );
    WriteEmptyArray( Serializer );
    WriteEmptyArray( Serializer );
}

//=========================================================================

static void serialize( output_serializer& Serializer, skin_geom& Geom )
{
    WriteCommonGeom( Serializer, Geom );
    WriteEmptyCollision( Serializer );
    WriteValueArray<4>( Serializer, Geom.m_pIndex, Geom.m_nIndices );
    WriteEmptyArray( Serializer );
    WriteObjectArray( Serializer, Geom.m_pVertex, Geom.m_nVertexData );
    WriteEmptyArray( Serializer );
    WriteObjectArray( Serializer, Geom.m_pSection, Geom.m_nSections );
    WriteValueArray<2>( Serializer, Geom.m_pBonePalette, Geom.m_nBonePalette );
}

//=========================================================================
//  ARCHIVE DESERIALIZATION
//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, rigid_geom& Geom )
{
    ReadCommonGeom( Serializer, Geom );
    ReadCollision( Serializer, Geom.m_Collision );
    bitsery_io::ReadValueArray<4>( Serializer, Geom.m_pIndex, Geom.m_nIndices, MaxLargeArrayCount );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pVertex, Geom.m_nVertexData, MaxLargeArrayCount );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pSection, Geom.m_nSections, MaxSmallArrayCount );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadEmptyArray( Serializer );
}

//=========================================================================

template <class SERIALIZER>
static void serialize( SERIALIZER& Serializer, skin_geom& Geom )
{
    ReadCommonGeom( Serializer, Geom );
    ReadEmptyCollision( Serializer );
    bitsery_io::ReadValueArray<4>( Serializer, Geom.m_pIndex, Geom.m_nIndices, MaxLargeArrayCount );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pVertex, Geom.m_nVertexData, MaxLargeArrayCount );
    bitsery_io::ReadEmptyArray( Serializer );
    bitsery_io::ReadObjectArray( Serializer, Geom.m_pSection, Geom.m_nSections, MaxSmallArrayCount );
    bitsery_io::ReadValueArray<2>( Serializer, Geom.m_pBonePalette, Geom.m_nBonePalette, MaxLargeArrayCount );
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool geom_file::Validate( rigid_geom const& Geom, xstring& Error )
{
    Error.Clear();
    if( ValidateRigid( Geom ) )
    {
        return TRUE;
    }

    return bitsery_io::Fail( Error, "Rigid geometry payload failed validation." );
}

//=========================================================================

xbool geom_file::Validate( skin_geom const& Geom, xstring& Error )
{
    Error.Clear();
    if( ValidateSkin( Geom ) )
    {
        return TRUE;
    }

    return bitsery_io::Fail( Error, "Skin geometry payload failed validation." );
}

//=========================================================================

xbool geom_file::LoadRigid( X_FILE* pFile, rigid_geom*& pGeom, xstring& Error )
{
    Error.Clear();
    pGeom = nullptr;

    pGeom = new rigid_geom;
    if( !bitsery_io::Read( pFile, RigidFileFormat, *pGeom, Error ) )
    {
        delete pGeom;
        pGeom = nullptr;
        return FALSE;
    }

    if( !ValidateRigid( *pGeom ) || !pGeom->PrepareMaterials() )
    {
        delete pGeom;
        pGeom = nullptr;
        return bitsery_io::Fail( Error, "Rigid geometry payload failed validation." );
    }

    return TRUE;
}

//=========================================================================

xbool geom_file::LoadSkin( X_FILE* pFile, skin_geom*& pGeom, xstring& Error )
{
    Error.Clear();
    pGeom = nullptr;

    pGeom = new skin_geom;
    if( !bitsery_io::Read( pFile, SkinFileFormat, *pGeom, Error ) )
    {
        delete pGeom;
        pGeom = nullptr;
        return FALSE;
    }

    if( !ValidateSkin( *pGeom ) || !pGeom->PrepareMaterials() )
    {
        delete pGeom;
        pGeom = nullptr;
        return bitsery_io::Fail( Error, "Skin geometry payload failed validation." );
    }

    return TRUE;
}

//=========================================================================

xbool geom_file::SaveRigid( X_FILE* pFile, rigid_geom const& Geom, xstring& Error )
{
    if( !Validate( Geom, Error ) )
    {
        return FALSE;
    }

    return bitsery_io::Write( pFile, RigidFileFormat, Geom, Error );
}

//=========================================================================

xbool geom_file::SaveSkin( X_FILE* pFile, skin_geom const& Geom, xstring& Error )
{
    if( !Validate( Geom, Error ) )
    {
        return FALSE;
    }

    return bitsery_io::Write( pFile, SkinFileFormat, Geom, Error );
}

//=========================================================================

xbool geom_file::EncodeSkin( skin_geom const& Geom, xarray<u8>& Bytes, xstring& Error )
{
    return Validate( Geom, Error ) && bitsery_io::EncodeFile( SkinFileFormat, Geom, Bytes, Error );
}

//=========================================================================

xbool geom_file::SaveRigid( char const* pFileName, rigid_geom const& Geom, xstring& Error )
{
    if( !pFileName || !pFileName[0] )
    {
        return bitsery_io::Fail( Error, "Rigid geometry output filename is empty." );
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if( !pFile )
    {
        return bitsery_io::Fail( Error, "Failed to open the rigid geometry output file." );
    }

    xbool const Result = SaveRigid( pFile, Geom, Error );
    x_fclose( pFile );
    return Result;
}

//=========================================================================

xbool geom_file::SaveSkin( char const* pFileName, skin_geom const& Geom, xstring& Error )
{
    if( !pFileName || !pFileName[0] )
    {
        return bitsery_io::Fail( Error, "Skin geometry output filename is empty." );
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if( !pFile )
    {
        return bitsery_io::Fail( Error, "Failed to open the skin geometry output file." );
    }

    xbool const Result = SaveSkin( pFile, Geom, Error );
    x_fclose( pFile );
    return Result;
}
