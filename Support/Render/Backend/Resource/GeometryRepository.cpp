//=========================================================================
//
//  GeometryRepository.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryRepository.hpp"
#include "../Storage/SkinnedGeometryStorage.hpp"
#include "../Storage/StaticGeometryStorage.hpp"
#include "x_workers.hpp"

namespace render
{

//=========================================================================
//  PREPARE JOBS
//=========================================================================

namespace
{
struct RigidPrepareJob
{
    const rigid_geom*                   pGeom;
    s32                                 iSurface;
    StaticGeometryStorage::PreparedMesh Prepared;
    xbool                               Success;
};

//-------------------------------------------------------------------------

struct SkinPrepareJob
{
    const skin_geom*                     pGeom;
    s32                                  iSurface;
    SkinnedGeometryStorage::PreparedMesh Prepared;
    xbool                                Success;
};

//=========================================================================

void PrepareRigidSurface( void* pData, s32 iJob )
{
    RigidPrepareJob* pJob = static_cast<RigidPrepareJob*>( pData );

    static_cast<void>( iJob );

    ASSERT( pJob );
    if( !pJob )
    {
        return;
    }

    pJob->Success = FALSE;
    pJob->Prepared.Clear();

    ASSERT( pJob->pGeom );
    if( !pJob->pGeom )
    {
        return;
    }

    const rigid_geom& Geom = *pJob->pGeom;

    if( !Geom.m_pSubMesh || !Geom.m_pSection || pJob->iSurface < 0 || pJob->iSurface >= Geom.m_nSubMeshes )
    {
        return;
    }

    const geom::submesh& Surface = Geom.m_pSubMesh[pJob->iSurface];

    if( Surface.nSections != 1 || Surface.iSection < 0 || Surface.iSection >= Geom.m_nSections )
    {
        return;
    }

    const rigid_geom::section& SectionData = Geom.m_pSection[Surface.iSection];
    ASSERT( Geom.m_pVertex );
    ASSERT( Geom.m_pIndex );
    ASSERT( SectionData.nVertices > 0 );
    ASSERT( SectionData.nIndices > 0 );

    if( !Geom.m_pVertex || !Geom.m_pIndex || ( SectionData.nVertices <= 0 ) || ( SectionData.nVertices > 65536 ) ||
        ( SectionData.FirstVertex < 0 ) || ( SectionData.nVertices > Geom.m_nVertexData - SectionData.FirstVertex ) ||
        ( SectionData.FirstIndex < 0 ) || ( SectionData.nIndices <= 0 ) ||
        ( SectionData.nIndices > Geom.m_nIndices - SectionData.FirstIndex ) )
    {
        return;
    }

    if( !pJob->Prepared.Allocate( SectionData.nVertices, SectionData.nIndices, sizeof( rigid_geom::vertex ) ) )
    {
        return;
    }

    x_memcpy( pJob->Prepared.GetVertexData(), Geom.m_pVertex + SectionData.FirstVertex,
              sizeof( rigid_geom::vertex ) * SectionData.nVertices );

    bbox LocalBounds;
    LocalBounds.Clear();

    u16* pIndices = pJob->Prepared.GetIndexData();

    for( s32 i = 0; i < SectionData.nIndices; ++i )
    {
        const u32 SourceIndex = Geom.m_pIndex[SectionData.FirstIndex + i];

        if( SourceIndex < static_cast<u32>( SectionData.FirstVertex ) ||
            SourceIndex >= static_cast<u32>( SectionData.FirstVertex + SectionData.nVertices ) )
        {
            pJob->Prepared.Clear();
            return;
        }

        pIndices[i] = static_cast<u16>( SourceIndex - SectionData.FirstVertex );
        LocalBounds += vector3( Geom.m_pVertex[SourceIndex].Pos );
    }

    pJob->Prepared.SetLocalBounds( LocalBounds );
    pJob->Success = pJob->Prepared.IsValid();

    if( !pJob->Success )
    {
        pJob->Prepared.Clear();
    }
}

//=========================================================================

void PrepareSkinSurface( void* pData, s32 iJob )
{
    SkinPrepareJob* pJob = static_cast<SkinPrepareJob*>( pData );

    static_cast<void>( iJob );

    ASSERT( pJob );
    if( !pJob )
    {
        return;
    }

    pJob->Success = FALSE;
    pJob->Prepared.Clear();

    ASSERT( pJob->pGeom );
    if( !pJob->pGeom )
    {
        return;
    }

    const skin_geom& Geom = *pJob->pGeom;

    if( !Geom.m_pSubMesh || !Geom.m_pSection || !Geom.m_pVertex || !Geom.m_pIndex || pJob->iSurface < 0 ||
        pJob->iSurface >= Geom.m_nSubMeshes )
    {
        return;
    }

    const geom::submesh& Surface = Geom.m_pSubMesh[pJob->iSurface];

    if( Surface.nSections <= 0 || Surface.iSection < 0 || Surface.nSections > Geom.m_nSections - Surface.iSection )
    {
        return;
    }

    const skin_geom::section& FirstSection = Geom.m_pSection[Surface.iSection];

    if( FirstSection.nVertices <= 0 || FirstSection.nVertices > 65536 || FirstSection.FirstVertex < 0 ||
        FirstSection.nVertices > Geom.m_nVertexData - FirstSection.FirstVertex )
    {
        return;
    }

    s32 nIndices = 0;

    for( s32 i = 0; i < Surface.nSections; ++i )
    {
        const skin_geom::section& SectionData = Geom.m_pSection[Surface.iSection + i];

        if( SectionData.FirstVertex != FirstSection.FirstVertex || SectionData.nVertices != FirstSection.nVertices ||
            SectionData.FirstIndex < 0 || SectionData.nIndices <= 0 ||
            SectionData.nIndices > Geom.m_nIndices - SectionData.FirstIndex || SectionData.FirstBone < 0 ||
            SectionData.nBones < 0 || SectionData.nBones > SkinnedGeometryStorage::MAX_BONE_PALETTE ||
            SectionData.nBones > Geom.m_nBonePalette - SectionData.FirstBone ||
            ( SectionData.nBones > 0 && !Geom.m_pBonePalette ) || SectionData.nIndices > 0x7fffffff - nIndices )
        {
            return;
        }

        nIndices += SectionData.nIndices;
    }

    xarray<SkinGpuVertex>                        Vertices;
    xarray<u16>                                  Indices;
    xarray<SkinnedGeometryStorage::SectionInput> Sections;
    Vertices.SetCount( FirstSection.nVertices );
    Indices.SetCount( nIndices );
    Sections.SetCount( Surface.nSections );

    for( s32 i = 0; i < FirstSection.nVertices; ++i )
    {
        const skin_geom::vertex& Source      = Geom.m_pVertex[FirstSection.FirstVertex + i];
        SkinGpuVertex&           Destination = Vertices[i];
        Destination.Position.Set( Source.Position.X, Source.Position.Y, Source.Position.Z,
                                  static_cast<f32>( Source.Bones[0] ) );
        Destination.Normal.Set( Source.Normal.X, Source.Normal.Y, Source.Normal.Z,
                                static_cast<f32>( Source.Bones[1] ) );
        Destination.UVWeights.Set( Source.UV.X, Source.UV.Y, Source.Weights.X, Source.Weights.Y );
    }

    s32 FirstIndex = 0;

    for( s32 i = 0; i < Surface.nSections; ++i )
    {
        const skin_geom::section&             SourceSection      = Geom.m_pSection[Surface.iSection + i];
        SkinnedGeometryStorage::SectionInput& DestinationSection = Sections[i];
        DestinationSection.FirstIndex                            = FirstIndex;
        DestinationSection.IndexCount                            = SourceSection.nIndices;
        DestinationSection.pBoneRemap = SourceSection.nBones ? Geom.m_pBonePalette + SourceSection.FirstBone : NULL;
        DestinationSection.BoneCount  = SourceSection.nBones;

        for( s32 j = 0; j < SourceSection.nIndices; ++j )
        {
            const u32 SourceIndex = Geom.m_pIndex[SourceSection.FirstIndex + j];

            if( SourceIndex < static_cast<u32>( FirstSection.FirstVertex ) ||
                SourceIndex >= static_cast<u32>( FirstSection.FirstVertex + FirstSection.nVertices ) )
            {
                return;
            }

            Indices[FirstIndex + j] = static_cast<u16>( SourceIndex - FirstSection.FirstVertex );
        }

        FirstIndex += SourceSection.nIndices;
    }

    pJob->Success = SkinnedGeometryStorage::PrepareMesh( pJob->Prepared, Vertices.GetPtr(), Vertices.GetCount(),
                                                         Indices.GetPtr(), Indices.GetCount(), Geom.m_nBones,
                                                         Sections.GetPtr(), Sections.GetCount() );
}
} // namespace

//=========================================================================
//  LIFETIME
//=========================================================================

GeometryRepository::GeometryRepository( void )
    : m_Rigid(),
      m_Skin(),
      m_RigidStorage(),
      m_SkinStorage(),
      m_isInitialized( FALSE )
{
}

//=========================================================================

GeometryRepository::~GeometryRepository( void )
{
    Kill();
}

//=========================================================================

xbool GeometryRepository::Init( void )
{
    if( m_isInitialized )
    {
        return TRUE;
    }

    m_RigidStorage.Init( sizeof( rigid_geom::vertex ) );

    if( !m_RigidStorage.IsInitialized() )
    {
        return FALSE;
    }

    m_SkinStorage.Init();

    if( !m_SkinStorage.IsInitialized() )
    {
        m_RigidStorage.Kill();
        return FALSE;
    }

    m_isInitialized = TRUE;

    return TRUE;
}

//=========================================================================

void GeometryRepository::Kill( void )
{
    for( s32 i = 0; i < m_Rigid.Entries.GetCount(); ++i )
    {
        Entry& EntryData = m_Rigid.Entries[i];

        if( EntryData.Active )
        {
            for( s32 j = 0; j < EntryData.Surfaces.GetCount(); ++j )
            {
                m_RigidStorage.RemoveMesh( EntryData.Surfaces[j] );
            }

            Release( m_Rigid, xhandle( i | static_cast<s32>( EntryData.Generation << SLOT_BITS ) ) );
        }
    }

    for( s32 i = 0; i < m_Skin.Entries.GetCount(); ++i )
    {
        Entry& EntryData = m_Skin.Entries[i];

        if( EntryData.Active )
        {
            for( s32 j = 0; j < EntryData.Surfaces.GetCount(); ++j )
            {
                m_SkinStorage.RemoveMesh( EntryData.Surfaces[j] );
            }

            Release( m_Skin, xhandle( i | static_cast<s32>( EntryData.Generation << SLOT_BITS ) ) );
        }
    }

    m_SkinStorage.Kill();
    m_RigidStorage.Kill();
    m_isInitialized = FALSE;
}

//=========================================================================

xbool GeometryRepository::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

const GeometryRepository::Entry* GeometryRepository::Resolve( const Registry& RegistryData, xhandle Handle )
{
    if( Handle.Handle < 0 )
    {
        return NULL;
    }

    const u32 Value      = static_cast<u32>( Handle.Handle );
    const u32 SlotIndex  = Value & ( SLOT_LIMIT - 1 );
    const u32 Generation = Value >> SLOT_BITS;

    if( SlotIndex >= static_cast<u32>( RegistryData.Entries.GetCount() ) )
    {
        return NULL;
    }

    const Entry& EntryData = RegistryData.Entries[SlotIndex];

    return ( EntryData.Active && EntryData.Generation == Generation ) ? &EntryData : NULL;
}

//=========================================================================

GeometryRepository::Entry* GeometryRepository::Resolve( Registry& RegistryData, xhandle Handle )
{
    return const_cast<Entry*>( Resolve( static_cast<const Registry&>( RegistryData ), Handle ) );
}

//=========================================================================

xhandle GeometryRepository::Allocate( Registry& RegistryData, const xarray<xhandle>& Surfaces )
{
    u32 SlotIndex;

    if( RegistryData.FreeSlots.GetCount() )
    {
        const s32 Last = RegistryData.FreeSlots.GetCount() - 1;
        SlotIndex      = RegistryData.FreeSlots[Last];
        RegistryData.FreeSlots.Delete( Last );
    }
    else
    {
        if( RegistryData.Entries.GetCount() >= SLOT_LIMIT )
        {
            return xhandle( HNULL );
        }

        SlotIndex = static_cast<u32>( RegistryData.Entries.GetCount() );
        RegistryData.Entries.Append();
    }

    Entry& EntryData = RegistryData.Entries[SlotIndex];
    ASSERT( !EntryData.Active && !EntryData.Retired && EntryData.Generation < GENERATION_LIMIT );
    EntryData.Surfaces = Surfaces;
    EntryData.Active   = TRUE;

    return xhandle( static_cast<s32>( SlotIndex | ( EntryData.Generation << SLOT_BITS ) ) );
}

//=========================================================================

void GeometryRepository::Release( Registry& RegistryData, xhandle Handle )
{
    Entry* pEntry = Resolve( RegistryData, Handle );
    ASSERT( pEntry );
    if( !pEntry )
    {
        return;
    }

    pEntry->Surfaces.Clear();
    pEntry->Active = FALSE;

    if( ++pEntry->Generation == GENERATION_LIMIT )
    {
        pEntry->Retired = TRUE;
    }
    else
    {
        RegistryData.FreeSlots.Append() = static_cast<u32>( Handle.Handle ) & ( SLOT_LIMIT - 1 );
    }
}

//=========================================================================
//  REGISTRATION
//=========================================================================

xhandle GeometryRepository::RegisterRigid( const rigid_geom& Geom )
{
    if( !m_isInitialized || Geom.m_nSubMeshes < 0 )
    {
        return xhandle( HNULL );
    }

    xarray<RigidPrepareJob> Jobs;
    xarray<xhandle>         Surfaces;
    xhandle                 hJob;
    xhandle                 hGeom;
    hJob.Handle = HNULL;

    hGeom.Handle = HNULL;

    // Allocate all surfaces before publishing the registration.

    x_try;

        const s32 nSurfaces = Geom.m_nSubMeshes;

        if( nSurfaces > 0 )
        {
            Jobs.SetCount( nSurfaces );
            Surfaces.SetCapacity( nSurfaces );

            for( s32 i = 0; i < nSurfaces; ++i )
            {
                Jobs[i].pGeom    = &Geom;
                Jobs[i].iSurface = i;
                Jobs[i].Success  = FALSE;
            }

            if( x_WorkersIsInit() &&
                !x_WorkerJobSubmitBatch( PrepareRigidSurface, Jobs.GetPtr(), sizeof( RigidPrepareJob ),
                                         nSurfaces, "RigidVertexPrep", hJob ) )
            {
                x_throw( "Failed to submit rigid vertex prepare job" );
            }

            if( x_WorkersIsInit() )
            {
                x_WorkerJobWait( hJob );
                x_WorkerJobRelease( hJob );
                hJob.Handle = HNULL;
            }
            else
            {
                for( s32 i = 0; i < nSurfaces; ++i )
                {
                    PrepareRigidSurface( &Jobs[i], i );
                }
            }

            for( s32 i = 0; i < nSurfaces; ++i )
            {
                if( !Jobs[i].Success )
                {
                    x_throw( "Rigid vertex prepare job failed" );
                }

                const xhandle hSurface = m_RigidStorage.AddMesh( Jobs[i].Prepared );

                if( hSurface.IsNull() )
                {
                    x_throw( "Rigid static geometry allocation failed" );
                }

                Surfaces.Append() = hSurface;
                Jobs[i].Prepared.Clear();
            }
        }

        hGeom = Allocate( m_Rigid, Surfaces );

        if( hGeom.IsNull() )
        {
            x_throw( "Rigid geometry handle capacity exhausted" );
        }

    x_catch_begin;

        if( hJob.IsNonNull() )
        {
            x_WorkerJobWait( hJob );
            x_WorkerJobRelease( hJob );
        }

        for( s32 i = 0; i < Surfaces.GetCount(); ++i )
        {
            m_RigidStorage.RemoveMesh( Surfaces[i] );
        }

        if( hGeom.IsNonNull() )
        {
            Release( m_Rigid, hGeom );
        }

    x_catch_end_ret;

    return hGeom;
}

//=========================================================================

xhandle GeometryRepository::RegisterSkin( const skin_geom& Geom )
{
    if( !m_isInitialized || Geom.m_nSubMeshes < 0 )
    {
        return xhandle( HNULL );
    }

    xarray<SkinPrepareJob> Jobs;
    xarray<xhandle>        Surfaces;
    xhandle                hJob;
    xhandle                hGeom;
    hJob.Handle = HNULL;

    hGeom.Handle = HNULL;

    x_try;

        const s32 nSurfaces = Geom.m_nSubMeshes;

        if( nSurfaces > 0 )
        {
            Jobs.SetCount( nSurfaces );
            Surfaces.SetCapacity( nSurfaces );

            for( s32 i = 0; i < nSurfaces; ++i )
            {
                Jobs[i].pGeom    = &Geom;
                Jobs[i].iSurface = i;
                Jobs[i].Success  = FALSE;
            }

            if( x_WorkersIsInit() &&
                !x_WorkerJobSubmitBatch( PrepareSkinSurface, Jobs.GetPtr(), sizeof( SkinPrepareJob ),
                                         nSurfaces, "SkinVertexPrep", hJob ) )
            {
                x_throw( "Failed to submit skin vertex prepare job" );
            }

            if( x_WorkersIsInit() )
            {
                x_WorkerJobWait( hJob );
                x_WorkerJobRelease( hJob );
                hJob.Handle = HNULL;
            }
            else
            {
                for( s32 i = 0; i < nSurfaces; ++i )
                {
                    PrepareSkinSurface( &Jobs[i], i );
                }
            }

            for( s32 i = 0; i < nSurfaces; ++i )
            {
                if( !Jobs[i].Success )
                {
                    x_throw( "Skin vertex prepare job failed" );
                }

                const xhandle hSurface = m_SkinStorage.AddMesh( Jobs[i].Prepared );

                if( hSurface.IsNull() )
                {
                    x_throw( "Skin static geometry allocation failed" );
                }

                Surfaces.Append() = hSurface;
                Jobs[i].Prepared.Clear();
            }
        }

        hGeom = Allocate( m_Skin, Surfaces );

        if( hGeom.IsNull() )
        {
            x_throw( "Skin geometry handle capacity exhausted" );
        }

    x_catch_begin;

        if( hJob.IsNonNull() )
        {
            x_WorkerJobWait( hJob );
            x_WorkerJobRelease( hJob );
        }

        for( s32 i = 0; i < Surfaces.GetCount(); ++i )
        {
            m_SkinStorage.RemoveMesh( Surfaces[i] );
        }

        if( hGeom.IsNonNull() )
        {
            Release( m_Skin, hGeom );
        }

    x_catch_end_ret;

    return hGeom;
}

//=========================================================================

xbool GeometryRepository::UnregisterRigid( xhandle hGeom )
{
    Entry* pResource = Resolve( m_Rigid, hGeom );

    if( !pResource )
    {
        return FALSE;
    }

    for( s32 i = 0; i < pResource->Surfaces.GetCount(); ++i )
    {
        m_RigidStorage.RemoveMesh( pResource->Surfaces[i] );
    }

    Release( m_Rigid, hGeom );

    return TRUE;
}

//=========================================================================

xbool GeometryRepository::UnregisterSkin( xhandle hGeom )
{
    Entry* pResource = Resolve( m_Skin, hGeom );

    if( !pResource )
    {
        return FALSE;
    }

    for( s32 i = 0; i < pResource->Surfaces.GetCount(); ++i )
    {
        m_SkinStorage.RemoveMesh( pResource->Surfaces[i] );
    }

    Release( m_Skin, hGeom );

    return TRUE;
}

//=========================================================================
//  ACCESS
//=========================================================================

xhandle GeometryRepository::GetRigidSurface( xhandle hGeom, s32 iSurface ) const
{
    const Entry* pResource = Resolve( m_Rigid, hGeom );

    if( !pResource || iSurface < 0 || iSurface >= pResource->Surfaces.GetCount() )
    {
        return xhandle( HNULL );
    }

    return pResource->Surfaces[iSurface];
}

//=========================================================================

xhandle GeometryRepository::GetSkinSurface( xhandle hGeom, s32 iSurface ) const
{
    const Entry* pResource = Resolve( m_Skin, hGeom );

    if( !pResource || iSurface < 0 || iSurface >= pResource->Surfaces.GetCount() )
    {
        return xhandle( HNULL );
    }

    return pResource->Surfaces[iSurface];
}

//=========================================================================

xbool GeometryRepository::GetRigidMeshView( xhandle hMesh, RigidMeshView& View ) const
{
    return hMesh.IsNonNull() && m_RigidStorage.GetMeshDrawRange( hMesh, View );
}

//=========================================================================

xbool GeometryRepository::GetSkinMeshView( xhandle hMesh, SkinMeshView& View ) const
{
    return hMesh.IsNonNull() && m_SkinStorage.GetMeshView( hMesh, View );
}

//=========================================================================

void GeometryRepository::DeclareRigidResources( FrameGraphBuilder& Graph, u32 Pass, const RigidMeshView& View ) const
{
    m_RigidStorage.DeclareResources( Graph, Pass, View );
}

//=========================================================================

void GeometryRepository::DeclareSkinResources( FrameGraphBuilder& Graph, u32 Pass,
                                               const StaticGeometryStorage::mesh_range& Range ) const
{
    m_SkinStorage.DeclareResources( Graph, Pass, Range );
}

//=========================================================================

xbool GeometryRepository::BindRigidMesh( const RigidMeshView& View ) const
{
    return m_RigidStorage.BindPools( View );
}

//=========================================================================

xbool GeometryRepository::BindRigidVertexIndices( const RigidMeshView& View, u32 SlotIndex ) const
{
    return m_RigidStorage.BindVertexIndices( View, SlotIndex );
}

//=========================================================================

xbool GeometryRepository::BindSkinMesh( const SkinMeshView& View ) const
{
    return m_SkinStorage.BindPools( View.Geom );
}

//=========================================================================

const shader_resource* GeometryRepository::GetSkinBoneRemapResource( void ) const
{
    return m_SkinStorage.GetBoneRemapResource();
}

} // namespace render
