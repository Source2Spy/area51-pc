//=========================================================================
//
//  ProjectionAtlas.hpp
//
//  Shared GPU atlas for projected textures and local-light cookies.
//
//=========================================================================

#ifndef PROJECTION_ATLAS_HPP
#define PROJECTION_ATLAS_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../Texture.hpp"

namespace render
{

class FrameGraphBuilder;

//=========================================================================
//  TYPES
//=========================================================================

enum class ProjectionAtlasEncoding
{
    BlueMask = 0,
    LuminanceAlpha,
    Count
};

//-------------------------------------------------------------------------

struct ProjectionAtlasRegion
{
    ProjectionAtlasRegion( void );

    //-------------------------------------------------------------------------

    vector4 Sampling;
    vector4 UVScaleBias;
    vector4 TileBounds; // xy = minimum, zw = maximum in normalized atlas coordinates
    u32     Layer;
    f32     MaxMip;
};

//=========================================================================
//  PROJECTION ATLAS
//=========================================================================

class ProjectionAtlas
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    ProjectionAtlas( void );
    ~ProjectionAtlas( void );

    xbool Init( void );
    void  Kill( void );
    xbool IsInitialized( void ) const;
    xbool HasMipmaps( void ) const;
    s32   GetPageSize( void ) const;
    s32   GetPageCount( void ) const;

    void  BeginFrame( void );
    xbool Request( const texture::handle& Texture, ProjectionAtlasEncoding Encoding );
    xbool Prepare( FrameGraphBuilder& Graph );
    void  Invalidate( void )
    {
        m_isRebuildRequired = TRUE;
    }

    xbool GetRegion( const texture::handle&  Texture,
                     ProjectionAtlasEncoding Encoding,
                     ProjectionAtlasRegion&  Region ) const;
    const shader_resource* GetShaderResource( void ) const;
    vector4                GetSamplingParams( void ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum
    {
        PAGE_SIZE  = 2048,
        PAGE_COUNT = 4
    };

    //-------------------------------------------------------------------------

    struct FreeNode
    {
        s32 m_Page;
        s32 m_X;
        s32 m_Y;
        s32 m_Size;
    };

    //-------------------------------------------------------------------------

    struct Entry
    {
        Entry( void );

        //-------------------------------------------------------------------------

        texture::handle             m_Texture;
        xstring                     m_ResourceName;
        ProjectionAtlasEncoding     m_Encoding;
        ProjectionAtlasRegion       m_Region;
        const vram_texture_backend* m_pSourceBackend;
        const byte*                 m_pSourcePixels;
        s32                         m_TileX;
        s32                         m_TileY;
        s32                         m_TileSize;
        s32                         m_ImageWidth;
        s32                         m_ImageHeight;
        u32                         m_LastRequestedFrame;
        xbool                       m_isResident;
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    Entry*       FindEntry( const char* pResourceName, ProjectionAtlasEncoding Encoding );
    const Entry* FindEntry( const char* pResourceName, ProjectionAtlasEncoding Encoding ) const;
    s32          FindEntryIndex( const char* pResourceName, ProjectionAtlasEncoding Encoding ) const;
    void         RebuildLookup( void );
    void         ResetAllocator( void );
    xbool        Allocate( Entry& EntryData );
    xbool        Upload( const Entry& EntryData, FrameGraphBuilder& Graph );
    xbool        RebuildRequested( FrameGraphBuilder& Graph );
    xbool        IsRequested( const Entry& EntryData ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    vram_texture     m_Texture;
    xarray<Entry>    m_Entries;
    xarray<s32>      m_EntryLookup;
    u32              m_EntryLookupMask;
    xarray<FreeNode> m_FreeNodes;
    xarray<s32>      m_Pending;
    xarray<s32>      m_Requested;
    xarray<u8>       m_Image;
    xarray<u8>       m_Tile;
    u32              m_iFrameIndex;
    xbool            m_isInitialized;
    xbool            m_hasMipmaps;
    xbool            m_isRebuildRequired;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
ProjectionAtlasRegion::ProjectionAtlasRegion( void )
    : Sampling( 0.0f, 0.0f, 0.0f, 0.0f ),
      UVScaleBias( 0.0f, 0.0f, 0.0f, 0.0f ),
      TileBounds( 0.0f, 0.0f, 0.0f, 0.0f ),
      Layer( 0 ),
      MaxMip( 0.0f )
{
}

//=========================================================================

inline
xbool ProjectionAtlas::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
xbool ProjectionAtlas::HasMipmaps( void ) const
{
    return m_hasMipmaps;
}

//=========================================================================

inline
s32 ProjectionAtlas::GetPageSize( void ) const
{
    return PAGE_SIZE;
}

//=========================================================================

inline
s32 ProjectionAtlas::GetPageCount( void ) const
{
    return PAGE_COUNT;
}

} // namespace render

#endif // PROJECTION_ATLAS_HPP
//=========================================================================
