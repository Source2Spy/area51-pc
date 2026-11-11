//=========================================================================
//
//  SkinnedGeometryStorage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "SkinnedGeometryStorage.hpp"
#include "../../FrameGraph/FrameGraph.hpp"

namespace render
{

//=========================================================================
//  PREPARED DATA
//=========================================================================

SkinnedGeometryStorage::SoftSection::SoftSection( void )
    : FirstIndex( 0 ),
      IndexCount( 0 )
{
    for( s32 i = 0; i < MAX_BONE_PALETTE; ++i )
    {
        BoneRemap[i] = INVALID_BONE_REMAP;
    }
}

//=========================================================================

void SkinnedGeometryStorage::PreparedMesh::Clear( void )
{
    VertexData.Clear();
    Sections.Clear();
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

SkinnedGeometryStorage::SkinnedGeometryStorage( void )
    : m_BoneRemapCapacity( 0 )
{
}

//=========================================================================

void SkinnedGeometryStorage::Init( void )
{
    m_StaticGeometryStorage.Init( sizeof( SkinGpuVertex ) );
}

//=========================================================================

void SkinnedGeometryStorage::Kill( void )
{
    m_Meshes.Clear();
    m_BoneRemaps.Clear();
    m_FreeBoneRemaps.Clear();
    rbuffer_Destroy( m_BoneRemapBuffer );
    m_BoneRemapCapacity = 0;
    m_StaticGeometryStorage.Kill();
}

//=========================================================================

xbool SkinnedGeometryStorage::PrepareMesh( PreparedMesh&        Prepared,
                                           const SkinGpuVertex* pVertex,
                                           s32                  nVertices,
                                           const u16*           pIndex,
                                           s32                  nIndices,
                                           s32                  nBones,
                                           const SectionInput*  pSections,
                                           s32                  nSections )
{
    Prepared.Clear();

    if( ( nBones <= 0 ) || ( nSections <= 0 ) || !pSections )
    {
        return FALSE;
    }

    if( !StaticGeometryStorage::PrepareMesh( Prepared.VertexData, pVertex, nVertices, pIndex, nIndices,
                                             sizeof( SkinGpuVertex ) ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < nSections; ++i )
    {
        const SectionInput& Input = pSections[i];

        if( ( Input.FirstIndex < 0 ) || ( Input.IndexCount <= 0 ) || ( Input.FirstIndex > nIndices ) ||
            ( Input.IndexCount > nIndices - Input.FirstIndex ) || ( Input.BoneCount < 0 ) ||
            ( Input.BoneCount > MAX_BONE_PALETTE ) || ( ( Input.BoneCount > 0 ) && !Input.pBoneRemap ) )
        {
            Prepared.Clear();
            return FALSE;
        }

        SoftSection& Section = Prepared.Sections.Append();
        Section.FirstIndex   = Input.FirstIndex;
        Section.IndexCount   = Input.IndexCount;

        for( s32 j = 0; j < Input.BoneCount; ++j )
        {
            if( Input.pBoneRemap[j] != INVALID_BONE_REMAP && Input.pBoneRemap[j] >= nBones )
            {
                Prepared.Clear();
                return FALSE;
            }

            Section.BoneRemap[j] = Input.pBoneRemap[j];
        }
    }

    if( Prepared.Sections.GetCount() <= 0 )
    {
        Prepared.Clear();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xhandle SkinnedGeometryStorage::AddMesh( const PreparedMesh& Prepared )
{
    ASSERT( Prepared.VertexData.IsValid() );
    ASSERT( Prepared.Sections.GetCount() > 0 );

    if( !Prepared.VertexData.IsValid() || ( Prepared.Sections.GetCount() <= 0 ) )
    {
        x_throw( "Invalid prepared skinned geometry" );
        return xhandle();
    }

    const s32 IndexCount = Prepared.VertexData.GetIndexCount();

    for( s32 i = 0; i < Prepared.Sections.GetCount(); ++i )
    {
        const SoftSection& Section = Prepared.Sections[i];

        if( Section.FirstIndex < 0 || Section.IndexCount <= 0 || Section.FirstIndex > IndexCount ||
            Section.IndexCount > IndexCount - Section.FirstIndex )
        {
            x_throw( "Invalid prepared skin section" );
            return xhandle();
        }
    }

    xhandle   hMesh;
    SkinMesh& MeshData = m_Meshes.Add( hMesh );

    MeshData.hGeom.Handle   = HNULL;
    MeshData.FirstBoneRemap = 0xffffffffu;
    MeshData.Sections.Clear();

    const char* pFailure = NULL;

    x_try;

        do
        {

            MeshData.hGeom = m_StaticGeometryStorage.AddMesh( Prepared.VertexData );

            if( MeshData.hGeom.IsNull() )
            {
                pFailure = "Failed to register skinned geometry";
                break;
            }

            MeshData.Sections = Prepared.Sections;

            const u64 Required = static_cast<u64>( MeshData.Sections.GetCount() ) * MAX_BONE_PALETTE;

            if( Required > ( 256u * 1024u * 1024u / sizeof( u32 ) ) )
            {
                pFailure = "Skin remaps exceed buffer size limit";
                break;
            }

            const u32 Count = static_cast<u32>( Required );

            for( s32 i = 0; i < m_FreeBoneRemaps.GetCount(); ++i )
            {
                RemapRange& Range = m_FreeBoneRemaps[i];

                if( Range.Count < Count )
                {
                    continue;
                }

                MeshData.FirstBoneRemap = Range.First;

                Range.First += Count;
                Range.Count -= Count;

                if( Range.Count == 0 )
                {
                    m_FreeBoneRemaps.Delete( i );
                }

                break;
            }

            if( MeshData.FirstBoneRemap == 0xffffffffu )
            {
                const u32 First = static_cast<u32>( m_BoneRemaps.GetCount() );

                if( First > ( 256u * 1024u * 1024u / sizeof( u32 ) ) - Count )
                {
                    pFailure = "Skin remap storage exhausted";
                    break;
                }

                m_BoneRemaps.SetCount( static_cast<s32>( First + Count ) );
                MeshData.FirstBoneRemap = First;
            }

            for( s32 Section = 0; Section < MeshData.Sections.GetCount(); ++Section )
            {
                for( s32 Bone = 0; Bone < MAX_BONE_PALETTE; ++Bone )
                {
                    const u16 Index = MeshData.Sections[Section].BoneRemap[Bone];
                    m_BoneRemaps[MeshData.FirstBoneRemap + Section * MAX_BONE_PALETTE + Bone] =
                        Index == INVALID_BONE_REMAP ? 0u : static_cast<u32>( Index );
                }
            }

            if( !UploadBoneRemaps() )
            {
                pFailure = "Failed to upload static skin remaps";
            }

        } while( FALSE );

    x_catch_begin;

        if( MeshData.hGeom.IsNonNull() )
        {
            m_StaticGeometryStorage.RemoveMesh( MeshData.hGeom );
        }

        if( MeshData.FirstBoneRemap != 0xffffffffu )
        {
            FreeBoneRemaps( MeshData.FirstBoneRemap,
                            static_cast<u32>( MeshData.Sections.GetCount() ) * MAX_BONE_PALETTE );
        }

        MeshData.FirstBoneRemap = 0xffffffffu;
        MeshData.Sections.Clear();
        m_Meshes.DeleteByHandle( hMesh );

    x_catch_end_ret;

    if( pFailure )
    {
        RemoveMesh( hMesh );
        x_throw( pFailure );
        return xhandle();
    }

    return hMesh;
}

//=========================================================================

void SkinnedGeometryStorage::RemoveMesh( xhandle hMesh )
{
    SkinMesh& MeshData = m_Meshes( hMesh );

    if( MeshData.hGeom.IsNonNull() )
    {
        m_StaticGeometryStorage.RemoveMesh( MeshData.hGeom );
    }

    MeshData.hGeom.Handle = HNULL;

    if( MeshData.FirstBoneRemap != 0xffffffffu )
    {
        FreeBoneRemaps( MeshData.FirstBoneRemap, static_cast<u32>( MeshData.Sections.GetCount() ) * MAX_BONE_PALETTE );
    }

    MeshData.FirstBoneRemap = 0xffffffffu;
    MeshData.Sections.Clear();
    m_Meshes.DeleteByHandle( hMesh );
}

//=========================================================================

void SkinnedGeometryStorage::DeclareResources( FrameGraphBuilder& Graph, u32 Pass,
                                               const StaticGeometryStorage::mesh_range& Range ) const
{
    m_StaticGeometryStorage.DeclareResources( Graph, Pass, Range );
    Graph.Read( Pass, GetBoneRemapResource(), RENDER_RESOURCE_ACCESS_STORAGE_READ );
}

//=========================================================================

xbool SkinnedGeometryStorage::BindPools( const StaticGeometryStorage::mesh_range& Range ) const
{
    return m_StaticGeometryStorage.BindPools( Range );
}

//=========================================================================

xbool SkinnedGeometryStorage::GetMeshView( xhandle hMesh, MeshView& View ) const
{
    View = MeshView();

    const SkinMesh& MeshData = m_Meshes( hMesh );

    if( MeshData.hGeom.IsNull() || ( MeshData.Sections.GetCount() <= 0 ) ||
        !m_StaticGeometryStorage.GetMeshDrawRange( MeshData.hGeom, View.Geom ) )
    {
        return FALSE;
    }

    View.pSections      = MeshData.Sections.GetPtr();
    View.SectionCount   = MeshData.Sections.GetCount();
    View.FirstBoneRemap = MeshData.FirstBoneRemap;

    return TRUE;
}

//=========================================================================

xbool SkinnedGeometryStorage::UploadBoneRemaps( void )
{
    const u32           Count = static_cast<u32>( m_BoneRemaps.GetCount() );
    rbuffer_upload_desc Upload;
    Upload.pData  = m_BoneRemaps.GetPtr();
    Upload.Size   = Count * sizeof( u32 );
    Upload.bCycle = TRUE;

    if( m_BoneRemapBuffer && Count <= m_BoneRemapCapacity )
    {
        return rbuffer_Upload( m_BoneRemapBuffer, Upload );
    }

    u32 Capacity = MAX( m_BoneRemapCapacity, 256u );

    while( Capacity < Count )
    {
        Capacity *= 2u;
    }

    rbuffer_desc Desc;
    Desc.Size       = Capacity * sizeof( u32 );
    Desc.Stride     = sizeof( u32 );
    Desc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    Desc.pDebugName = "SkinStaticBoneRemaps";

    rbuffer Buffer;

    if( !rbuffer_Create( Buffer, Desc ) )
    {
        return FALSE;
    }

    if( !rbuffer_Upload( Buffer, Upload ) )
    {
        rbuffer_Destroy( Buffer );
        return FALSE;
    }

    rbuffer_Swap( m_BoneRemapBuffer, Buffer );
    rbuffer_Destroy( Buffer );
    m_BoneRemapCapacity = Capacity;

    return TRUE;
}

//=========================================================================

void SkinnedGeometryStorage::FreeBoneRemaps( u32 First, u32 Count )
{
    for( s32 i = 0; i < m_FreeBoneRemaps.GetCount(); )
    {
        const RemapRange Range = m_FreeBoneRemaps[i];

        if( Range.First + Range.Count == First )
        {
            First = Range.First;
            Count += Range.Count;
            m_FreeBoneRemaps.Delete( i );
            i = 0;
        }
        else if( First + Count == Range.First )
        {
            Count += Range.Count;
            m_FreeBoneRemaps.Delete( i );
            i = 0;
        }
        else
        {
            ++i;
        }
    }

    if( First + Count == static_cast<u32>( m_BoneRemaps.GetCount() ) )
    {
        m_BoneRemaps.SetCount( static_cast<s32>( First ) );
    }
    else
    {
        RemapRange& Range = m_FreeBoneRemaps.Append();
        Range.First       = First;
        Range.Count       = Count;
    }
}

//=========================================================================

const shader_resource* SkinnedGeometryStorage::GetBoneRemapResource( void ) const
{
    return rbuffer_GetResource( m_BoneRemapBuffer );
}

} // namespace render
