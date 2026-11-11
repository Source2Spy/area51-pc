//==============================================================================
//
//  ecs_block_cache.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ecs_block_cache.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

ecs_block_cache::ecs_block_cache( u64 Budget )
    : m_Budget( Budget )
{
}

//==============================================================================

ecs_block_cache::~ecs_block_cache( void )
{
    for( s32 i = 0; i < m_Transient.GetCount(); ++i )
    {
        m_Transient[i]->pCache = NULL;
    }
}

//==============================================================================
//  BLOCK ACQUISITION
//==============================================================================

xbool ecs_block_cache::Acquire( ecs_package_read         Read,
                                void*                    pContext,
                                const ecs_package_index& Index,
                                u32                      Block,
                                ecs_block_view&          View,
                                xstring&                 Error )
{
    View.Reset();
    Error.Clear();

    if( Block >= ( u32 )Index.Blocks.GetCount() || !Index.CatalogId || !Index.PackageId )
    {
        Error = "invalid package cache request";
        return FALSE;
    }

    if( Find( Index, Block, View ) )
    {
        return TRUE;
    }

    xref<ecs_block_data> Data( new ecs_block_data );

    if( !ecs_ReadPackageBlock( Read, pContext, Index, Block, Data->Bytes, Error ) )
    {
        return FALSE;
    }

    Insert( Index, Block, Data, View );

    return TRUE;
}

//==============================================================================
//  HASH AND LRU INDEX
//==============================================================================

u32 ecs_block_cache::Hash( u64 CatalogId, u64 PackageId, u32 Block ) const
{
    u64 Value = CatalogId ^ ( PackageId + 0x9e3779b97f4a7c15ull ) ^ Block;

    Value ^= Value >> 30;
    Value *= 0xbf58476d1ce4e5b9ull;
    Value ^= Value >> 27;
    Value *= 0x94d049bb133111ebull;

    return static_cast<u32>( Value ^ ( Value >> 31 ) );
}

//==============================================================================

void ecs_block_cache::RebuildHash( u32 Capacity )
{
    m_HashSlots.SetCount( Capacity );

    for( u32 i = 0; i < Capacity; ++i )
    {
        m_HashSlots[i] = -1;
    }

    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        CacheEntry& Entry = m_Entries[i];

        if( !Entry.Data )
        {
            continue;
        }

        const u32 Slot = Hash( Entry.CatalogId, Entry.PackageId, Entry.Block ) & ( Capacity - 1 );

        Entry.HashNext    = m_HashSlots[Slot];
        m_HashSlots[Slot] = i;
    }
}

//==============================================================================

void ecs_block_cache::TouchEntry( s32 Index )
{
    CacheEntry& Entry = m_Entries[Index];

    if( Index == m_iNewest )
    {
        return;
    }

    if( Entry.Previous >= 0 )
    {
        m_Entries[Entry.Previous].Next = Entry.Next;
    }
    else if( m_iOldest == Index )
    {
        m_iOldest = Entry.Next;
    }

    if( Entry.Next >= 0 )
    {
        m_Entries[Entry.Next].Previous = Entry.Previous;
    }

    Entry.Previous = m_iNewest;
    Entry.Next     = -1;

    if( m_iNewest >= 0 )
    {
        m_Entries[m_iNewest].Next = Index;
    }

    m_iNewest = Index;

    if( m_iOldest < 0 )
    {
        m_iOldest = Index;
    }
}

//==============================================================================
//  CACHE LOOKUP
//==============================================================================

xbool ecs_block_cache::Find( const ecs_package_index& Index, u32 Block, ecs_block_view& View )
{
    View.Reset();

    if( !m_HashSlots.GetCount() )
    {
        return FALSE;
    }

    const u32 Slot = Hash( Index.CatalogId, Index.PackageId, Block ) & ( m_HashSlots.GetCount() - 1 );

    for( s32 i = m_HashSlots[Slot]; i >= 0; i = m_Entries[i].HashNext )
    {
        CacheEntry& Entry = m_Entries[i];

        if( Entry.CatalogId != Index.CatalogId || Entry.PackageId != Index.PackageId || Entry.Block != Block )
        {
            continue;
        }

        TouchEntry( i );
        View.Data = Entry.Data;
        ++m_Stats.Hits;

        return TRUE;
    }

    return FALSE;
}

//==============================================================================
//  BLOCK INSERTION
//==============================================================================

void ecs_block_cache::Insert( const ecs_package_index&    Index,
                              u32                         Block,
                              const xref<ecs_block_data>& Data,
                              ecs_block_view&             View,
                              f64                         ReadMs,
                              f64                         DecodeMs )
{
    View.Reset();
    ASSERT( Data && Block < static_cast<u32>( Index.Blocks.GetCount() ) );
    ++m_Stats.Misses;
    ++m_Stats.Decodes;
    m_Stats.ReadMs += ReadMs;
    m_Stats.DecodeMs += DecodeMs;
    m_Stats.ReadBytes += Index.Blocks[Block].StoredSize;

    const u64 Size = Data->Bytes.GetCount();

    // Evict unpinned entries until the block fits.
    if( Size <= m_Budget )
    {
        s32 Oldest = m_iOldest;

        while( m_ResidentBytes > m_Budget - Size && Oldest >= 0 )
        {
            const s32 Next = m_Entries[Oldest].Next;

            if( m_Entries[Oldest].Data->GetReferenceCount() == 1 )
            {
                m_ResidentBytes -= m_Entries[Oldest].Data->Bytes.GetCount();
                RemoveEntry( Oldest );
            }

            Oldest = Next;
        }
    }

    // Install resident blocks in the hash and LRU lists.
    if( Size <= m_Budget && m_ResidentBytes <= m_Budget - Size )
    {
        if( !m_HashSlots.GetCount() || ( m_nEntries + 1 ) * 2 > static_cast<u32>( m_HashSlots.GetCount() ) )
        {
            RebuildHash( MAX( 64, m_HashSlots.GetCount() * 2 ) );
        }

        const s32 SlotIndex = m_iFree >= 0 ? m_iFree : m_Entries.GetCount();

        if( m_iFree >= 0 )
        {
            m_iFree = m_Entries[m_iFree].HashNext;
        }
        else
        {
            if( m_Entries.GetCount() == m_Entries.GetCapacity() )
            {
                m_Entries.SetCapacity( m_Entries.GetCapacity() + MAX( 64, m_Entries.GetCapacity() / 2 ) );
            }

            m_Entries.Append();
        }

        CacheEntry& Entry = m_Entries[SlotIndex];

        Entry.CatalogId = Index.CatalogId;
        Entry.PackageId = Index.PackageId;
        Entry.Block     = Block;
        Entry.Data      = Data;
        Entry.Previous = Entry.Next = -1;

        const u32 Slot = Hash( Index.CatalogId, Index.PackageId, Block ) & ( m_HashSlots.GetCount() - 1 );

        Entry.HashNext    = m_HashSlots[Slot];
        m_HashSlots[Slot] = SlotIndex;
        TouchEntry( SlotIndex );
        ++m_nEntries;
        m_ResidentBytes += Size;
    }
    else
    {
        View.Transient = TRUE;
        TrackTransient( Data );
    }

    View.Data = Data;
}

//==============================================================================
//  CACHE MAINTENANCE
//==============================================================================

void ecs_block_cache::Clear( void )
{
    // Outstanding views remain valid after clearing an old catalog.
    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        if( m_Entries[i].Data && m_Entries[i].Data->GetReferenceCount() > 1 )
        {
            TrackTransient( m_Entries[i].Data );
        }
    }

    m_Entries.Clear();
    m_ResidentBytes = 0;
    m_HashSlots.Clear();
    m_iOldest = m_iNewest = m_iFree = -1;
    m_nEntries                      = 0;
}

//==============================================================================

void ecs_block_cache::RemovePackage( u64 CatalogId, u64 PackageId )
{
    for( s32 i = m_Entries.GetCount() - 1; i >= 0; --i )
    {
        const CacheEntry& Entry = m_Entries[i];

        if( !Entry.Data || Entry.CatalogId != CatalogId || Entry.PackageId != PackageId )
        {
            continue;
        }

        if( Entry.Data->GetReferenceCount() > 1 )
        {
            TrackTransient( Entry.Data );
        }

        m_ResidentBytes -= Entry.Data->Bytes.GetCount();
        RemoveEntry( i );
    }
}

//==============================================================================
//  STATISTICS
//==============================================================================

ecs_block_cache_stats ecs_block_cache::GetStats( void ) const
{
    ecs_block_cache_stats Stats = m_Stats;

    Stats.ResidentBytes = m_ResidentBytes;

    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        if( m_Entries[i].Data && m_Entries[i].Data->GetReferenceCount() > 1 )
        {
            Stats.PinnedBytes += m_Entries[i].Data->Bytes.GetCount();
        }
    }

    for( s32 i = 0; i < m_Transient.GetCount(); ++i )
    {
        const xref<ecs_block_data>& Data = m_Transient[i];

        if( Data )
        {
            Stats.TransientBytes += Data->Bytes.GetCount();
        }
    }

    return Stats;
}

//==============================================================================
//  ENTRY REMOVAL
//==============================================================================

void ecs_block_cache::RemoveEntry( s32 Index )
{
    CacheEntry& Entry = m_Entries[Index];
    const u32   Slot  = Hash( Entry.CatalogId, Entry.PackageId, Entry.Block ) & ( m_HashSlots.GetCount() - 1 );
    s32*        pLink = &m_HashSlots[Slot];

    while( *pLink != Index )
    {
        ASSERT( *pLink >= 0 );
        pLink = &m_Entries[*pLink].HashNext;
    }

    *pLink = Entry.HashNext;

    if( Entry.Previous >= 0 )
    {
        m_Entries[Entry.Previous].Next = Entry.Next;
    }
    else
    {
        m_iOldest = Entry.Next;
    }

    if( Entry.Next >= 0 )
    {
        m_Entries[Entry.Next].Previous = Entry.Previous;
    }
    else
    {
        m_iNewest = Entry.Previous;
    }

    // Recycle the slot after releasing the block.
    Entry.Data.Clear();
    Entry.HashNext = m_iFree;
    Entry.Previous = Entry.Next = -1;
    m_iFree                     = Index;
    --m_nEntries;
}

//==============================================================================
//  TRANSIENT BLOCK LIFETIME
//==============================================================================

void ecs_block_data::OnReferenceReleased( s32 ) const
{
    if( pCache )
    {
        pCache->RemoveTransient( this );
    }
}

//==============================================================================

void ecs_block_cache::TrackTransient( const xref<ecs_block_data>& Data )
{
    ASSERT( !Data->pCache );
    Data->pCache = this;
    m_Transient.Append( Data );
}

//==============================================================================

void ecs_block_cache::RemoveTransient( const ecs_block_data* pData )
{
    for( s32 i = 0; i < m_Transient.GetCount(); ++i )
    {
        if( m_Transient[i].Get() != pData )
        {
            continue;
        }

        m_Transient[i]->pCache = NULL;

        const s32 Last = m_Transient.GetCount() - 1;

        if( i != Last )
        {
            m_Transient[i] = m_Transient[Last];
        }

        m_Transient[Last].Clear();
        m_Transient.SetCount( Last );

        return;
    }
    ASSERT( FALSE );
}
