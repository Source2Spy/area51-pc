//==============================================================================
//
//  ecs_block_cache.hpp
//
//==============================================================================

#ifndef ECS_BLOCK_CACHE_HPP
#define ECS_BLOCK_CACHE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ecs_package.hpp"
#include "x_ref.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ecs_block_cache;

//==============================================================================
//  STRUCTS
//==============================================================================

struct ecs_block_data : public xref_counted
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    ecs_block_data( void );

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    xarray<byte>     Bytes;
    ecs_block_cache* pCache;

protected:

    void OnReferenceReleased( s32 Count ) const override;
};

//------------------------------------------------------------------------------

struct ecs_block_view
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    void Reset( void );

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    xref<ecs_block_data> Data;
    xbool                Transient = FALSE;
};

//------------------------------------------------------------------------------

struct ecs_block_cache_stats
{
    u64 ResidentBytes     = 0;
    u64 PinnedBytes       = 0;
    u64 TransientBytes    = 0;
    u64 Hits              = 0;
    u64 Misses            = 0;
    u64 Decodes           = 0;
    u64 ReadBytes         = 0;
    u64 InFlightBytes     = 0;
    u64 PeakInFlightBytes = 0;
    f64 ReadMs            = 0;
    f64 DecodeMs          = 0;
};

//==============================================================================
//  CLASSES
//==============================================================================

class ecs_block_cache
{
public:

    //------------------------------------------------------------------------------
    //  LIFETIME
    //------------------------------------------------------------------------------

    explicit ecs_block_cache( u64 Budget );
    ~ecs_block_cache( void );
    ecs_block_cache( const ecs_block_cache& )            = delete;
    ecs_block_cache& operator=( const ecs_block_cache& ) = delete;

    //------------------------------------------------------------------------------
    //  CACHE OPERATIONS
    //------------------------------------------------------------------------------

    xbool Acquire( ecs_package_read         Read,
                   void*                    pContext,
                   const ecs_package_index& Index,
                   u32                      Block,
                   ecs_block_view&          View,
                   xstring&                 Error );
    xbool                 Find( const ecs_package_index& Index, u32 Block, ecs_block_view& View );
    void Insert( const ecs_package_index&    Index,
                 u32                         Block,
                 const xref<ecs_block_data>& Data,
                 ecs_block_view&             View,
                 f64                         ReadMs   = 0,
                 f64                         DecodeMs = 0 );
    void                  Clear( void );
    void                  RemovePackage( u64 CatalogId, u64 PackageId );
    ecs_block_cache_stats GetStats( void ) const;

private:

    friend struct ecs_block_data;

    //------------------------------------------------------------------------------
    //  TYPES
    //------------------------------------------------------------------------------

    struct CacheEntry
    {
        u64                  CatalogId = 0;
        u64                  PackageId = 0;
        u32                  Block     = 0;
        xref<ecs_block_data> Data;
        s32                  HashNext = -1;
        s32                  Previous = -1;
        s32                  Next     = -1;
    };

    //------------------------------------------------------------------------------
    //  HELPER FUNCTIONS
    //------------------------------------------------------------------------------

    void RemoveTransient( const ecs_block_data* pData );
    void TrackTransient( const xref<ecs_block_data>& Data );
    void RemoveEntry( s32 Index );
    void TouchEntry( s32 Index );
    void RebuildHash( u32 Capacity );
    u32  Hash( u64 CatalogId, u64 PackageId, u32 Block ) const;

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    u64                          m_Budget;
    u64                          m_ResidentBytes = 0;
    s32                          m_iOldest       = -1;
    s32                          m_iNewest       = -1;
    s32                          m_iFree         = -1;
    u32                          m_nEntries      = 0;
    ecs_block_cache_stats        m_Stats;
    xarray<CacheEntry>           m_Entries;
    xarray<s32>                  m_HashSlots;
    xarray<xref<ecs_block_data>> m_Transient;
};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline
ecs_block_data::ecs_block_data( void )
    : xref_counted( TRUE ),
      pCache( NULL )
{
}

//==============================================================================

inline
void ecs_block_view::Reset( void )
{
    Data.Clear();
    Transient = FALSE;
}

//==============================================================================
#endif // ECS_BLOCK_CACHE_HPP
//==============================================================================
