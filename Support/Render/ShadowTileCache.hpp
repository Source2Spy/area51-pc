//=========================================================================
//
//  ShadowTileCache.hpp
//
//  Exact depth-content snapshots for independently updated atlas tiles.
//
//=========================================================================

#ifndef SHADOW_TILE_CACHE_HPP
#define SHADOW_TILE_CACHE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_array.hpp"
#include "x_memory.hpp"

namespace render
{

//=========================================================================
//  CLASSES
//=========================================================================

class ShadowTileCache
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum
    {
        MAX_TILES = 64
    };

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    ShadowTileCache( void );
    void Invalidate( void );
    void Clear( void );

    //-------------------------------------------------------------------------
    //  TILE UPDATES
    //-------------------------------------------------------------------------

    void Begin( u64 Identity, u64 ResourceEpoch, s32 DepthSize, s32 MomentSize, s32 Count );
    void Append( s32 Tile, const void* pData, s32 Size );

    template <class T>
    void Append( s32 Tile, const T& Value );
    void Resolve( s32 Tile, xbool MutableCoverage );
    void Commit( void );

    //-------------------------------------------------------------------------
    //  QUERIES
    //-------------------------------------------------------------------------

    xbool IsDirty( s32 Tile ) const;
    xbool HasUpdates( void ) const;
    xbool IsFullRefresh( void ) const;

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    u64        m_Identity;
    u64        m_ResourceEpoch;
    s32        m_DepthSize;
    s32        m_MomentSize;
    u64        m_ValidMask;
    u64        m_DirtyMask;
    u64        m_MutableMask;
    s32        m_nTiles;
    xbool      m_isFullRefresh;
    xarray<u8> m_Pending[MAX_TILES];
    xarray<u8> m_Committed[MAX_TILES];
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
ShadowTileCache::ShadowTileCache( void )
    : m_Identity( 0 ),
      m_ResourceEpoch( 0 ),
      m_DepthSize( 0 ),
      m_MomentSize( 0 ),
      m_ValidMask( 0 ),
      m_DirtyMask( 0 ),
      m_MutableMask( 0 ),
      m_nTiles( 0 ),
      m_isFullRefresh( TRUE )
{
}

//=========================================================================

inline
void ShadowTileCache::Invalidate( void )
{
    m_ValidMask     = 0;
    m_isFullRefresh = TRUE;
}

//=========================================================================

inline
void ShadowTileCache::Clear( void )
{
    Invalidate();

    for( s32 i = 0; i < MAX_TILES; ++i )
    {
        m_Pending[i].Clear();
        m_Committed[i].Clear();
    }
}

//=========================================================================

inline
void ShadowTileCache::Begin( u64 Identity, u64 ResourceEpoch, s32 DepthSize, s32 MomentSize, s32 Count )
{
    ASSERT( Count >= 0 && Count <= MAX_TILES );

    if( m_Identity != Identity || m_ResourceEpoch != ResourceEpoch || m_DepthSize != DepthSize ||
        m_MomentSize != MomentSize )
    {
        Invalidate();
    }

    m_Identity      = Identity;
    m_ResourceEpoch = ResourceEpoch;
    m_DepthSize     = DepthSize;
    m_MomentSize    = MomentSize;
    m_nTiles        = Count;

    m_isFullRefresh = m_ValidMask == 0;
    m_DirtyMask     = 0;
    m_MutableMask   = 0;

    for( s32 i = 0; i < Count; ++i )
    {
        m_Pending[i].SetCount( 0 );
    }
}

//=========================================================================

inline
void ShadowTileCache::Append( s32 Tile, const void* pData, s32 Size )
{
    ASSERT( Tile >= 0 && Tile < m_nTiles && Size >= 0 );

    xarray<u8>& Bytes  = m_Pending[Tile];
    const s32   Offset = Bytes.GetCount();

    if( Offset + Size > Bytes.GetCapacity() )
    {
        Bytes.SetCapacity( MAX( Offset + Size, MAX( Bytes.GetCapacity() * 2, 512 ) ) );
    }

    Bytes.SetCount( Offset + Size );

    if( Size )
    {
        x_memcpy( Bytes.GetPtr() + Offset, pData, Size );
    }
}

//=========================================================================

template <class T>
inline
void ShadowTileCache::Append( s32 Tile, const T& Value )
{
    Append( Tile, &Value, sizeof( Value ) );
}

//=========================================================================

inline
void ShadowTileCache::Resolve( s32 Tile, xbool MutableCoverage )
{
    const u64 Bit = u64( 1 ) << Tile;

    if( MutableCoverage )
    {
        m_MutableMask |= Bit;
    }

    const xarray<u8>& Current  = m_Pending[Tile];
    const xarray<u8>& Previous = m_Committed[Tile];

    // Coverage textures without content revisions cannot establish reuse.
    if( MutableCoverage || !( m_ValidMask & Bit ) || Current.GetCount() != Previous.GetCount() ||
        ( Current.GetCount() && x_memcmp( Current.GetPtr(), Previous.GetPtr(), Current.GetCount() ) ) )
    {
        m_DirtyMask |= Bit;
    }

    const u64 AllTiles = m_nTiles == MAX_TILES ? ~u64( 0 ) : ( u64( 1 ) << m_nTiles ) - 1;

    if( m_DirtyMask == AllTiles )
    {
        m_isFullRefresh = TRUE;
    }
}

//=========================================================================

inline
xbool ShadowTileCache::IsDirty( s32 Tile ) const
{
    return ( m_DirtyMask & ( u64( 1 ) << Tile ) ) != 0;
}

//=========================================================================

inline
xbool ShadowTileCache::HasUpdates( void ) const
{
    return m_DirtyMask != 0;
}

//=========================================================================

inline
xbool ShadowTileCache::IsFullRefresh( void ) const
{
    return m_isFullRefresh;
}

//=========================================================================

inline
void ShadowTileCache::Commit( void )
{
    for( s32 i = 0; i < m_nTiles; ++i )
    {
        if( !IsDirty( i ) || ( m_MutableMask & ( u64( 1 ) << i ) ) )
        {
            continue;
        }

        xarray<u8>&       Destination = m_Committed[i];
        const xarray<u8>& Source      = m_Pending[i];

        Destination.SetCount( Source.GetCount() );

        if( Source.GetCount() )
        {
            x_memcpy( Destination.GetPtr(), Source.GetPtr(), Source.GetCount() );
        }
    }

    m_ValidMask = ( m_nTiles == MAX_TILES ? ~u64( 0 ) : ( u64( 1 ) << m_nTiles ) - 1 ) & ~m_MutableMask;
}

} // namespace render

#endif // SHADOW_TILE_CACHE_HPP
