//=========================================================================
//
//  GeometryProductSnapshot.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductSnapshot.hpp"

#include "GeometryProductFormat.hpp"
#include "SkeletonContract.hpp"
#include "Render/geom.hpp"
#include "Render/RigidGeom.hpp"
#include "Render/SkinGeom.hpp"

#include <memory>

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

std::string ReadProductString( const geom& Source, s32 Offset )
{
    if( !Source.m_pStringData || Offset < 0 || Offset >= Source.m_StringDataSize )
        return std::string();

    const char* pString = Source.m_pStringData + Offset;
    const s32 Remaining = Source.m_StringDataSize - Offset;
    s32 Length = 0;
    while( Length < Remaining && pString[Length] != '\0' )
        ++Length;
    return std::string( pString, Length );
}

//=========================================================================

void CopyBase( const geom& Source, geometry_product_data& Target )
{
    Target.BBox = Source.m_BBox;
    Target.FaceCount = Source.m_nFaces;
    Target.VertexCount = Source.m_nVertices;

    Target.Bones.resize( Source.m_nBones > 0 ? Source.m_nBones : 0 );
    for( s32 i = 0; i < Source.m_nBones; ++i )
    {
        Target.Bones[i].BindRotation = Source.m_pBone[i].BindRotation;
        Target.Bones[i].BindPosition = Source.m_pBone[i].BindPosition;
        Target.Bones[i].BBox = Source.m_pBone[i].BBox;
        Target.Bones[i].HitLocation = Source.m_pBone[i].HitLocation;
        Target.Bones[i].iRigidBody = Source.m_pBone[i].iRigidBody;
    }

    Target.BoneMasks.resize( Source.m_nBoneMasks > 0 ? Source.m_nBoneMasks : 0 );
    for( s32 i = 0; i < Source.m_nBoneMasks; ++i )
    {
        const geom::bone_masks& SourceMask = Source.m_pBoneMasks[i];
        geometry_product_data::bone_masks& Mask = Target.BoneMasks[i];
        Mask.Name = ReadProductString( Source, SourceMask.NameOffset );
        Mask.nBones = SourceMask.nBones;
        const s32 nWeights = SourceMask.nBones > 0 ? SourceMask.nBones : 0;
        Mask.Weights.resize( nWeights );
        for( s32 iBone = 0; iBone < nWeights && iBone < resource_tool::skeleton_contract::MaxBones; ++iBone )
            Mask.Weights[iBone] = SourceMask.Weights[iBone];
    }

    Target.PropertySections.resize( Source.m_nPropertySections > 0 ? Source.m_nPropertySections : 0 );
    for( s32 i = 0; i < Source.m_nPropertySections; ++i )
    {
        const geom::property_section& SourceSection = Source.m_pPropertySections[i];
        geometry_product_data::property_section& Section = Target.PropertySections[i];
        Section.Name = ReadProductString( Source, SourceSection.NameOffset );
        Section.iProperty = SourceSection.iProperty;
        Section.nProperties = SourceSection.nProperties;
    }

    Target.Properties.resize( Source.m_nProperties > 0 ? Source.m_nProperties : 0 );
    for( s32 i = 0; i < Source.m_nProperties; ++i )
    {
        const geom::property& SourceProperty = Source.m_pProperties[i];
        geometry_product_data::property& Property = Target.Properties[i];
        Property.Name = ReadProductString( Source, SourceProperty.NameOffset );
        Property.Type = SourceProperty.Type;
        switch( SourceProperty.Type )
        {
        case geom::property::TYPE_FLOAT:
            Property.Float = SourceProperty.Value.Float;
            break;
        case geom::property::TYPE_INTEGER:
            Property.Integer = SourceProperty.Value.Integer;
            break;
        case geom::property::TYPE_ANGLE:
            Property.Angle = SourceProperty.Value.Angle;
            break;
        case geom::property::TYPE_STRING:
            Property.String = ReadProductString( Source, SourceProperty.Value.StringOffset );
            break;
        default:
            break;
        }
    }

    Target.RigidBodies.resize( Source.m_nRigidBodies > 0 ? Source.m_nRigidBodies : 0 );
    for( s32 i = 0; i < Source.m_nRigidBodies; ++i )
    {
        const geom::rigid_body& SourceBody = Source.m_pRigidBodies[i];
        geometry_product_data::rigid_body& Body = Target.RigidBodies[i];
        Body.BodyBindRotation = SourceBody.BodyBindRotation;
        Body.BodyBindPosition = SourceBody.BodyBindPosition;
        Body.PivotBindRotation = SourceBody.PivotBindRotation;
        Body.PivotBindPosition = SourceBody.PivotBindPosition;
        Body.Name = ReadProductString( Source, SourceBody.NameOffset );
        Body.Mass = SourceBody.Mass;
        Body.Radius = SourceBody.Radius;
        Body.Width = SourceBody.Width;
        Body.Height = SourceBody.Height;
        Body.Length = SourceBody.Length;
        Body.Type = SourceBody.Type;
        Body.Flags = SourceBody.Flags;
        Body.iParentBody = SourceBody.iParentBody;
        Body.iBone = SourceBody.iBone;
        Body.CollisionMask = SourceBody.CollisionMask;
        for( s32 iDOF = 0; iDOF < 6; ++iDOF )
        {
            Body.DOF[iDOF].Flags = SourceBody.DOF[iDOF].Flags;
            Body.DOF[iDOF].Min = SourceBody.DOF[iDOF].Min;
            Body.DOF[iDOF].Max = SourceBody.DOF[iDOF].Max;
        }
    }

    Target.Meshes.resize( Source.m_nMeshes > 0 ? Source.m_nMeshes : 0 );
    for( s32 i = 0; i < Source.m_nMeshes; ++i )
    {
        const geom::mesh& SourceMesh = Source.m_pMesh[i];
        geometry_product_data::mesh& Mesh = Target.Meshes[i];
        Mesh.BBox = SourceMesh.BBox;
        Mesh.Name = ReadProductString( Source, SourceMesh.NameOffset );
        Mesh.nSubMeshes = SourceMesh.nSubMeshes;
        Mesh.iSubMesh = SourceMesh.iSubMesh;
        Mesh.nBones = SourceMesh.nBones;
        Mesh.nFaces = SourceMesh.nFaces;
        Mesh.nVertices = SourceMesh.nVertices;
    }

    Target.SubMeshes.resize( Source.m_nSubMeshes > 0 ? Source.m_nSubMeshes : 0 );
    for( s32 i = 0; i < Source.m_nSubMeshes; ++i )
    {
        const geom::submesh& SourceSubMesh = Source.m_pSubMesh[i];
        geometry_product_data::submesh& SubMesh = Target.SubMeshes[i];
        SubMesh.iSection = SourceSubMesh.iSection;
        SubMesh.nSections = SourceSubMesh.nSections;
        SubMesh.iMaterial = SourceSubMesh.iMaterial;
        SubMesh.WorldPixelSize = SourceSubMesh.WorldPixelSize;
    }

    Target.Materials.resize( Source.m_nMaterials > 0 ? Source.m_nMaterials : 0 );
    for( s32 i = 0; i < Source.m_nMaterials; ++i )
    {
        const geom::material& SourceMaterial = Source.m_pMaterial[i];
        geometry_product_data::material& Material = Target.Materials[i];
        Material.UVAnim.Type = SourceMaterial.UVAnim.Type;
        Material.UVAnim.StartFrame = SourceMaterial.UVAnim.StartFrame;
        Material.UVAnim.FPS = SourceMaterial.UVAnim.FPS;
        Material.UVAnim.nKeys = SourceMaterial.UVAnim.nKeys;
        Material.UVAnim.iKey = SourceMaterial.UVAnim.iKey;
        Material.DetailScale = SourceMaterial.DetailScale;
        Material.FixedAlpha = SourceMaterial.FixedAlpha;
        Material.Flags = SourceMaterial.Flags;
        Material.Type = SourceMaterial.Type;
        Material.nTextures = SourceMaterial.nTextures;
        Material.iTexture = SourceMaterial.iTexture;
        Material.nVirtualMats = SourceMaterial.nVirtualMats;
        Material.iVirtualMat = SourceMaterial.iVirtualMat;
    }

    Target.Textures.resize( Source.m_nTextures > 0 ? Source.m_nTextures : 0 );
    for( s32 i = 0; i < Source.m_nTextures; ++i )
    {
        Target.Textures[i].Desc = ReadProductString( Source, Source.m_pTexture[i].DescOffset );
        Target.Textures[i].FileName = ReadProductString( Source, Source.m_pTexture[i].FileNameOffset );
    }

    Target.UVKeys.resize( Source.m_nUVKeys > 0 ? Source.m_nUVKeys : 0 );
    for( s32 i = 0; i < Source.m_nUVKeys; ++i )
    {
        Target.UVKeys[i].OffsetU = Source.m_pUVKey[i].OffsetU;
        Target.UVKeys[i].OffsetV = Source.m_pUVKey[i].OffsetV;
    }

    Target.LODSizes.resize( Source.m_nLODs > 0 ? Source.m_nLODs : 0 );
    Target.LODMasks.resize( Source.m_nLODs > 0 ? Source.m_nLODs : 0 );
    for( s32 i = 0; i < Source.m_nLODs; ++i )
    {
        Target.LODSizes[i] = Source.m_pLODSizes[i];
        Target.LODMasks[i] = Source.m_pLODMasks[i];
    }

    Target.VirtualMeshes.resize( Source.m_nVirtualMeshes > 0 ? Source.m_nVirtualMeshes : 0 );
    for( s32 i = 0; i < Source.m_nVirtualMeshes; ++i )
    {
        Target.VirtualMeshes[i].Name = ReadProductString( Source, Source.m_pVirtualMeshes[i].NameOffset );
        Target.VirtualMeshes[i].nLODs = Source.m_pVirtualMeshes[i].nLODs;
        Target.VirtualMeshes[i].iLOD = Source.m_pVirtualMeshes[i].iLOD;
    }

    Target.VirtualTextures.resize( Source.m_nVirtualTextures > 0 ? Source.m_nVirtualTextures : 0 );
    for( s32 i = 0; i < Source.m_nVirtualTextures; ++i )
    {
        Target.VirtualTextures[i].Name = ReadProductString( Source, Source.m_pVirtualTextures[i].NameOffset );
        Target.VirtualTextures[i].MaterialMask = Source.m_pVirtualTextures[i].MaterialMask;
    }

    Target.BoneCount = (s32)Target.Bones.size();
    Target.BoneMaskCount = (s32)Target.BoneMasks.size();
    Target.PropertySectionCount = (s32)Target.PropertySections.size();
    Target.PropertyCount = (s32)Target.Properties.size();
    Target.RigidBodyCount = (s32)Target.RigidBodies.size();
    Target.MeshCount = (s32)Target.Meshes.size();
    Target.SubMeshCount = (s32)Target.SubMeshes.size();
    Target.MaterialCount = (s32)Target.Materials.size();
    Target.TextureCount = (s32)Target.Textures.size();
    Target.UVKeyCount = (s32)Target.UVKeys.size();
    Target.LODCount = (s32)Target.LODSizes.size();
    Target.VirtualMeshCount = (s32)Target.VirtualMeshes.size();
    Target.VirtualMaterialCount = Source.m_nVirtualMaterials;
    Target.VirtualTextureCount = (s32)Target.VirtualTextures.size();
}

//=========================================================================

void CopyCollision( const collision_data& Source, geometry_product_collision_data& Target )
{
    Target.BBox = Source.BBox;
    Target.HighClusters.resize( Source.nHighClusters > 0 ? Source.nHighClusters : 0 );
    for( s32 i = 0; i < Source.nHighClusters; ++i )
    {
        const collision_data::high_cluster& SourceCluster = Source.pHighCluster[i];
        geometry_product_collision_data::high_cluster& Cluster = Target.HighClusters[i];
        Cluster.BBox = SourceCluster.BBox;
        Cluster.nTris = SourceCluster.nTris;
        Cluster.iMesh = SourceCluster.iMesh;
        Cluster.iBone = SourceCluster.iBone;
        Cluster.iSection = SourceCluster.iSection;
        Cluster.iOffset = SourceCluster.iOffset;
        Cluster.MaterialInfo.SoundType = SourceCluster.MaterialInfo.SoundType;
        Cluster.MaterialInfo.Flags = SourceCluster.MaterialInfo.Flags;
    }

    Target.HighIndexToVert0.resize( Source.nHighIndices > 0 ? Source.nHighIndices : 0 );
    for( s32 i = 0; i < Source.nHighIndices; ++i )
        Target.HighIndexToVert0[i] = Source.pHighIndexToVert0[i];

    Target.LowClusters.resize( Source.nLowClusters > 0 ? Source.nLowClusters : 0 );
    for( s32 i = 0; i < Source.nLowClusters; ++i )
    {
        const collision_data::low_cluster& SourceCluster = Source.pLowCluster[i];
        geometry_product_collision_data::low_cluster& Cluster = Target.LowClusters[i];
        Cluster.BBox = SourceCluster.BBox;
        Cluster.iVectorOffset = SourceCluster.iVectorOffset;
        Cluster.nPoints = SourceCluster.nPoints;
        Cluster.nNormals = SourceCluster.nNormals;
        Cluster.iQuadOffset = SourceCluster.iQuadOffset;
        Cluster.nQuads = SourceCluster.nQuads;
        Cluster.iMesh = SourceCluster.iMesh;
        Cluster.iBone = SourceCluster.iBone;
    }

    Target.LowVectors.resize( Source.nLowVectors > 0 ? Source.nLowVectors : 0 );
    for( s32 i = 0; i < Source.nLowVectors; ++i )
        Target.LowVectors[i] = Source.pLowVector[i];

    Target.LowQuads.resize( Source.nLowQuads > 0 ? Source.nLowQuads : 0 );
    for( s32 i = 0; i < Source.nLowQuads; ++i )
    {
        Target.LowQuads[i].iP[0] = Source.pLowQuad[i].iP[0];
        Target.LowQuads[i].iP[1] = Source.pLowQuad[i].iP[1];
        Target.LowQuads[i].iP[2] = Source.pLowQuad[i].iP[2];
        Target.LowQuads[i].iP[3] = Source.pLowQuad[i].iP[3];
        Target.LowQuads[i].iN = Source.pLowQuad[i].iN;
        Target.LowQuads[i].Flags = Source.pLowQuad[i].Flags;
    }
}

//=========================================================================

void CopyRigid( const rigid_geom& Source, geometry_product_rigid_data& Target )
{
    CopyBase( Source, Target );
    CopyCollision( Source.m_Collision, Target.Collision );
    Target.Sections.resize( Source.m_nSections > 0 ? Source.m_nSections : 0 );
    for( s32 i = 0; i < Source.m_nSections; ++i )
    {
        const rigid_geom::section& SourceSection = Source.m_pSection[i];
        geometry_product_rigid_data::section& Section = Target.Sections[i];
        Section.FirstVertex = SourceSection.FirstVertex;
        Section.nVertices = SourceSection.nVertices;
        Section.FirstIndex = SourceSection.FirstIndex;
        Section.nIndices = SourceSection.nIndices;
        Section.iBone = SourceSection.iBone;
        Section.iColor = SourceSection.iColor;
    }
    Target.Indices.resize( Source.m_nIndices > 0 ? Source.m_nIndices : 0 );
    for( s32 i = 0; i < Source.m_nIndices; ++i )
        Target.Indices[i] = Source.m_pIndex[i];
    Target.Vertices.resize( Source.m_nVertexData > 0 ? Source.m_nVertexData : 0 );
    for( s32 i = 0; i < Source.m_nVertexData; ++i )
    {
        Target.Vertices[i].Pos = Source.m_pVertex[i].Pos;
        Target.Vertices[i].Normal = Source.m_pVertex[i].Normal;
        Target.Vertices[i].UV = Source.m_pVertex[i].UV;
    }
    Target.SectionCount = (s32)Target.Sections.size();
    Target.IndexCount = (s32)Target.Indices.size();
    Target.VertexDataCount = (s32)Target.Vertices.size();
}

//=========================================================================

void CopySkin( const skin_geom& Source, geometry_product_skin_data& Target )
{
    CopyBase( Source, Target );
    Target.Sections.resize( Source.m_nSections > 0 ? Source.m_nSections : 0 );
    for( s32 i = 0; i < Source.m_nSections; ++i )
    {
        const skin_geom::section& SourceSection = Source.m_pSection[i];
        geometry_product_skin_data::section& Section = Target.Sections[i];
        Section.FirstVertex = SourceSection.FirstVertex;
        Section.nVertices = SourceSection.nVertices;
        Section.FirstIndex = SourceSection.FirstIndex;
        Section.nIndices = SourceSection.nIndices;
        Section.FirstBone = SourceSection.FirstBone;
        Section.nBones = SourceSection.nBones;
    }
    Target.Indices.resize( Source.m_nIndices > 0 ? Source.m_nIndices : 0 );
    for( s32 i = 0; i < Source.m_nIndices; ++i )
        Target.Indices[i] = Source.m_pIndex[i];
    Target.Vertices.resize( Source.m_nVertexData > 0 ? Source.m_nVertexData : 0 );
    for( s32 i = 0; i < Source.m_nVertexData; ++i )
    {
        Target.Vertices[i].Position = Source.m_pVertex[i].Position;
        Target.Vertices[i].Normal = Source.m_pVertex[i].Normal;
        Target.Vertices[i].UV = Source.m_pVertex[i].UV;
        Target.Vertices[i].Weights = Source.m_pVertex[i].Weights;
        Target.Vertices[i].Bones[0] = Source.m_pVertex[i].Bones[0];
        Target.Vertices[i].Bones[1] = Source.m_pVertex[i].Bones[1];
    }
    Target.BonePalette.resize( Source.m_nBonePalette > 0 ? Source.m_nBonePalette : 0 );
    for( s32 i = 0; i < Source.m_nBonePalette; ++i )
        Target.BonePalette[i] = Source.m_pBonePalette[i];
    Target.SectionCount = (s32)Target.Sections.size();
    Target.IndexCount = (s32)Target.Indices.size();
    Target.VertexDataCount = (s32)Target.Vertices.size();
    Target.BonePaletteCount = (s32)Target.BonePalette.size();
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

geometry_product_snapshot::geometry_product_snapshot( void )
    : m_Kind( geometry_product_kind::Rigid ), m_pRigid(), m_pSkin()
{
}

//=========================================================================

geometry_product_snapshot::~geometry_product_snapshot( void )
{
    Reset();
}

//=========================================================================

void geometry_product_snapshot::Reset( void )
{
    m_pRigid.reset();
    m_pSkin.reset();
    m_Kind = geometry_product_kind::Rigid;
}

//=========================================================================

s32 geometry_product_snapshot::TextureCount( void ) const
{
    return m_Kind == geometry_product_kind::Rigid ? ( m_pRigid ? m_pRigid->TextureCount : 0 )
                                                  : ( m_pSkin ? m_pSkin->TextureCount : 0 );
}

//=========================================================================

const char* geometry_product_snapshot::TextureName( s32 iTexture ) const
{
    if( iTexture < 0 || iTexture >= TextureCount() )
        return "";

    const geometry_product_data* pGeometry = m_Kind == geometry_product_kind::Rigid
                                                 ? static_cast<const geometry_product_data*>( m_pRigid.get() )
                                                 : static_cast<const geometry_product_data*>( m_pSkin.get() );
    return pGeometry->GetTextureName( iTexture );
}

//=========================================================================

xbool geometry_product_snapshot::Load( const char* pFileName, geometry_product_kind KindValue, std::string& Error )
{
    Reset();
    if( !pFileName || !pFileName[0] )
    {
        Error = "Product geometry path is empty";
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        Error = xfs( "Unable to open compiled geometry [%s]", pFileName );
        return FALSE;
    }

    xbool bLoaded = FALSE;
    if( KindValue == geometry_product_kind::Rigid )
    {
        rigid_geom* pLoaded = NULL;
        bLoaded = geometry_product_format::LoadRigid( pFile, pLoaded, Error );
        std::unique_ptr<rigid_geom> Loaded( pLoaded );
        if( bLoaded && Loaded )
        {
            m_pRigid.reset( new geometry_product_rigid_data );
            CopyRigid( *Loaded, *m_pRigid );
        }
    }
    else
    {
        skin_geom* pLoaded = NULL;
        bLoaded = geometry_product_format::LoadSkin( pFile, pLoaded, Error );
        std::unique_ptr<skin_geom> Loaded( pLoaded );
        if( bLoaded && Loaded )
        {
            m_pSkin.reset( new geometry_product_skin_data );
            CopySkin( *Loaded, *m_pSkin );
        }
    }
    x_fclose( pFile );

    if( !bLoaded )
    {
        Reset();
        return FALSE;
    }

    m_Kind = KindValue;
    return TRUE;
}

//=========================================================================

geometry_product_kind geometry_product_snapshot::Kind( void ) const
{
    return m_Kind;
}

//=========================================================================

const geometry_product_data& geometry_product_snapshot::Geometry( void ) const
{
    return m_Kind == geometry_product_kind::Rigid ? static_cast<const geometry_product_data&>( Rigid() )
                                                  : static_cast<const geometry_product_data&>( Skin() );
}

//=========================================================================

const geometry_product_rigid_data& geometry_product_snapshot::Rigid( void ) const
{
    if( !m_pRigid )
        x_throw( "Product geometry snapshot does not contain rigid geometry" );
    return *m_pRigid;
}

//=========================================================================

const geometry_product_skin_data& geometry_product_snapshot::Skin( void ) const
{
    if( !m_pSkin )
        x_throw( "Product geometry snapshot does not contain skin geometry" );
    return *m_pSkin;
}

} // namespace resource_tool::geometry_product_format
