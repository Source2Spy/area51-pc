//=========================================================================
//
//  ProjectionAtlas.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ProjectionAtlas.hpp"
#include "../../FrameGraph/FrameGraph.hpp"
#include "../../FrameGraph/RenderFailure.hpp"

namespace render
{

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

namespace
{

//=========================================================================

static s32 NextPowerOfTwo( s32 Value )
{
    if( Value <= 0 )
    {
        return 0;
    }

    s32 Result = 1;

    while( Result < Value )
    {
        if( Result > ( S32_MAX / 2 ) )
        {
            return 0;
        }

        Result <<= 1;
    }

    return Result;
}

//=========================================================================

static f32 Log2PowerOfTwo( s32 Value )
{
    f32 Result = 0.0f;

    while( Value > 1 )
    {
        Value >>= 1;
        Result += 1.0f;
    }

    return Result;
}

//=========================================================================

static u8 EncodeMask( const xcolor& Color, ProjectionAtlasEncoding Encoding )
{
    if( Encoding == ProjectionAtlasEncoding::BlueMask )
    {
        return Color.B;
    }

    const f32 Luminance = 0.299f * static_cast<f32>( Color.R ) + 0.587f * static_cast<f32>( Color.G ) +
                          0.114f * static_cast<f32>( Color.B );
    const f32 Mask = Luminance * ( static_cast<f32>( Color.A ) / 255.0f );

    return static_cast<u8>( MINMAX( 0.0f, Mask + 0.5f, 255.0f ) );
}

//=========================================================================

static u32 HashProjectionKey( const char* pResourceName, ProjectionAtlasEncoding Encoding )
{
    u32 Hash = 2166136261u ^ static_cast<u32>( Encoding );

    while( pResourceName && *pResourceName )
    {
        Hash ^= static_cast<u8>( *pResourceName++ );
        Hash *= 16777619u;
    }

    return Hash;
}
} // namespace

//=========================================================================
//  ENTRY
//=========================================================================

ProjectionAtlas::Entry::Entry( void )
    : m_Texture(),
      m_ResourceName(),
      m_Encoding( ProjectionAtlasEncoding::BlueMask ),
      m_Region(),
      m_pSourceBackend( NULL ),
      m_pSourcePixels( NULL ),
      m_TileX( 0 ),
      m_TileY( 0 ),
      m_TileSize( 0 ),
      m_ImageWidth( 0 ),
      m_ImageHeight( 0 ),
      m_LastRequestedFrame( 0 ),
      m_isResident( FALSE )
{
}

//=========================================================================
//  LIFETIME
//=========================================================================

ProjectionAtlas::ProjectionAtlas( void )
    : m_Texture(),
      m_Entries(),
      m_EntryLookup(),
      m_EntryLookupMask( 0 ),
      m_FreeNodes(),
      m_Pending(),
      m_Requested(),
      m_Image(),
      m_Tile(),
      m_iFrameIndex( 0 ),
      m_isInitialized( FALSE ),
      m_hasMipmaps( FALSE ),
      m_isRebuildRequired( FALSE )
{
}

//=========================================================================

ProjectionAtlas::~ProjectionAtlas( void )
{
    Kill();
}

//=========================================================================

xbool ProjectionAtlas::Init( void )
{
    if( m_isInitialized )
    {
        return TRUE;
    }

    vram_texture_desc Desc;
    Desc.Type       = VRAM_TEXTURE_TYPE_2D_ARRAY;
    Desc.Width      = PAGE_SIZE;
    Desc.Height     = PAGE_SIZE;
    Desc.LayerCount = PAGE_COUNT;
    Desc.MipCount   = vram_CalcMipCount( PAGE_SIZE, PAGE_SIZE );
    Desc.Format     = VRAM_TEXTURE_FORMAT_R8;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED | VRAM_TEXTURE_USAGE_COLOR_TARGET;
    Desc.pDebugName = "ProjectionAtlas";

    m_hasMipmaps = TRUE;

    if( !vram_CreateTexture( m_Texture, Desc ) )
    {
        // Some backends cannot render to R8 arrays; CPU uploads only need sampling.
        RENDER_WARNING( "ProjectionAtlas", "R8 render-target atlas unavailable, retrying without mipmaps" );
        Desc.MipCount   = 1;
        Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
        m_hasMipmaps    = FALSE;

        if( !vram_CreateTexture( m_Texture, Desc ) )
        {
            RENDER_THROW( "ProjectionAtlas", "failed to create %dx%dx%d atlas", PAGE_SIZE, PAGE_SIZE, PAGE_COUNT );
            return FALSE;
        }
    }

    m_iFrameIndex       = 0;
    m_isInitialized     = TRUE;
    m_isRebuildRequired = FALSE;
    m_Entries.SetCapacity( PAGE_COUNT * 64 );
    m_EntryLookup.SetCapacity( PAGE_COUNT * 128 );
    m_FreeNodes.SetCapacity( PAGE_COUNT * 64 );
    m_Pending.SetCapacity( PAGE_COUNT * 64 );
    m_Requested.SetCapacity( PAGE_COUNT * 64 );
    m_Image.SetCapacity( PAGE_SIZE * PAGE_SIZE );
    m_Tile.SetCapacity( PAGE_SIZE * PAGE_SIZE );
    ResetAllocator();

    return TRUE;
}

//=========================================================================

void ProjectionAtlas::Kill( void )
{
    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        m_Entries[i].m_Texture.Destroy();
    }

    m_Entries.Clear();
    m_EntryLookup.Clear();
    m_EntryLookupMask = 0;
    m_FreeNodes.Clear();
    m_Pending.Clear();
    m_Requested.Clear();
    m_Image.Clear();
    m_Tile.Clear();
    vram_DestroyTexture( m_Texture );

    m_iFrameIndex       = 0;
    m_isInitialized     = FALSE;
    m_hasMipmaps        = FALSE;
    m_isRebuildRequired = FALSE;
}

//=========================================================================
//  FRAME PREPARATION
//=========================================================================

void ProjectionAtlas::BeginFrame( void )
{
    if( !m_isInitialized )
    {
        RENDER_THROW( "ProjectionAtlas", "BeginFrame called before successful Init" );
        return;
    }

    ++m_iFrameIndex;

    if( m_iFrameIndex == 0 )
    {
        m_iFrameIndex = 1;

        for( s32 i = 0; i < m_Entries.GetCount(); ++i )
        {
            m_Entries[i].m_LastRequestedFrame = 0;
        }
    }
}

//=========================================================================

xbool ProjectionAtlas::Request( const texture::handle& TextureHandle, ProjectionAtlasEncoding Encoding )
{
    if( !m_isInitialized || ( Encoding >= ProjectionAtlasEncoding::Count ) )
    {
        return FALSE;
    }

    texture* pTexture = TextureHandle.GetPointer();

    if( !pTexture )
    {
        return FALSE;
    }

    const char* pResourceName = TextureHandle.GetName();

    if( !pResourceName || !pResourceName[0] )
    {
        return FALSE;
    }

    const vram_texture_backend* pSourceBackend = pTexture->m_texture.pBackend;
    const byte*                 pSourcePixels  = pTexture->m_bitmap.GetPixelData();

    if( !pSourcePixels )
    {
        return FALSE;
    }

    Entry*      pEntry     = FindEntry( pResourceName, Encoding );
    const xbool isNewEntry = ( pEntry == NULL );

    if( !pEntry )
    {
        pEntry                 = &m_Entries.Append();
        pEntry->m_ResourceName = pResourceName;
        pEntry->m_Encoding     = Encoding;
        RebuildLookup();
    }

    if( ( pEntry->m_pSourceBackend != pSourceBackend ) || ( pEntry->m_pSourcePixels != pSourcePixels ) )
    {
        if( !isNewEntry && pEntry->m_isResident )
        {
            m_isRebuildRequired = TRUE;
        }

        pEntry->m_isResident     = FALSE;
        pEntry->m_pSourceBackend = pSourceBackend;
        pEntry->m_pSourcePixels  = pSourcePixels;
    }

    pEntry->m_Texture = TextureHandle;

    pEntry->m_LastRequestedFrame = m_iFrameIndex;

    return TRUE;
}

//=========================================================================

xbool ProjectionAtlas::Prepare( FrameGraphBuilder& Graph )
{
    if( !m_isInitialized )
    {
        RENDER_THROW( "ProjectionAtlas", "Prepare called before successful Init" );
        return FALSE;
    }

    if( m_isRebuildRequired )
    {
        return RebuildRequested( Graph );
    }

    m_Pending.SetCount( 0 );

    if( m_Pending.GetCapacity() < m_Entries.GetCount() )
    {
        m_Pending.SetCapacity( m_Entries.GetCount() );
    }

    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        if( IsRequested( m_Entries[i] ) && !m_Entries[i].m_isResident )
        {
            m_Pending.Append() = i;
        }
    }

    for( s32 i = 1; i < m_Pending.GetCount(); ++i )
    {
        const s32      EntryIndex = m_Pending[i];
        const texture* pTexture   = m_Entries[EntryIndex].m_Texture.GetPointer();
        const s32      EntrySize =
            pTexture ? NextPowerOfTwo( MAX( pTexture->m_bitmap.GetWidth(), pTexture->m_bitmap.GetHeight() ) ) : 0;

        s32 Insert = i;

        while( Insert > 0 )
        {
            const texture* pPrevious = m_Entries[m_Pending[Insert - 1]].m_Texture.GetPointer();
            const s32      PreviousSize =
                pPrevious ? NextPowerOfTwo( MAX( pPrevious->m_bitmap.GetWidth(), pPrevious->m_bitmap.GetHeight() ) )
                               : 0;

            if( PreviousSize >= EntrySize )
            {
                break;
            }

            m_Pending[Insert] = m_Pending[Insert - 1];
            --Insert;
        }

        m_Pending[Insert] = EntryIndex;
    }

    xbool isUploaded = FALSE;

    for( s32 i = 0; i < m_Pending.GetCount(); ++i )
    {
        if( !Allocate( m_Entries[m_Pending[i]] ) )
        {
            return RebuildRequested( Graph );
        }
    }

    for( s32 i = 0; i < m_Pending.GetCount(); ++i )
    {
        Entry& EntryData = m_Entries[m_Pending[i]];

        if( !Upload( EntryData, Graph ) )
        {
            EntryData.m_isResident = FALSE;
            m_isRebuildRequired    = TRUE;
            return FALSE;
        }

        EntryData.m_isResident = TRUE;
        isUploaded             = TRUE;
    }

    // Some backends reject mip generation for a one-level texture.
    if( isUploaded && m_hasMipmaps && !Graph.QueueGenerateMipmaps( m_Texture ) )
    {
        RENDER_THROW( "ProjectionAtlas", "mip generation failed after uploading %d entries", m_Pending.GetCount() );
        m_isRebuildRequired = TRUE;

        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool ProjectionAtlas::RebuildRequested( FrameGraphBuilder& Graph )
{
    m_isRebuildRequired = TRUE;
    ResetAllocator();

    for( s32 i = m_Entries.GetCount() - 1; i >= 0; --i )
    {
        if( !IsRequested( m_Entries[i] ) )
        {
            m_Entries[i].m_Texture.Destroy();
            m_Entries.Delete( i );
        }
    }

    RebuildLookup();

    m_Requested.SetCount( 0 );

    if( m_Requested.GetCapacity() < m_Entries.GetCount() )
    {
        m_Requested.SetCapacity( m_Entries.GetCount() );
    }

    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        m_Entries[i].m_isResident = FALSE;

        if( IsRequested( m_Entries[i] ) )
        {
            m_Requested.Append() = i;
        }
    }

    for( s32 i = 1; i < m_Requested.GetCount(); ++i )
    {
        const s32      EntryIndex = m_Requested[i];
        const texture* pTexture   = m_Entries[EntryIndex].m_Texture.GetPointer();
        const s32      EntrySize =
            pTexture ? NextPowerOfTwo( MAX( pTexture->m_bitmap.GetWidth(), pTexture->m_bitmap.GetHeight() ) ) : 0;

        s32 Insert = i;

        while( Insert > 0 )
        {
            const texture* pPrevious = m_Entries[m_Requested[Insert - 1]].m_Texture.GetPointer();
            const s32      PreviousSize =
                pPrevious ? NextPowerOfTwo( MAX( pPrevious->m_bitmap.GetWidth(), pPrevious->m_bitmap.GetHeight() ) )
                               : 0;

            if( PreviousSize >= EntrySize )
            {
                break;
            }

            m_Requested[Insert] = m_Requested[Insert - 1];
            --Insert;
        }

        m_Requested[Insert] = EntryIndex;
    }

    for( s32 i = 0; i < m_Requested.GetCount(); ++i )
    {
        Entry& EntryData = m_Entries[m_Requested[i]];

        if( !Allocate( EntryData ) || !Upload( EntryData, Graph ) )
        {
            RENDER_THROW( "ProjectionAtlas", "requested projection images exceed atlas capacity" );
            return FALSE;
        }

        EntryData.m_isResident = TRUE;
    }

    const xbool isPrepared = ( m_Requested.GetCount() == 0 ) || !m_hasMipmaps || Graph.QueueGenerateMipmaps( m_Texture );

    if( isPrepared )
    {
        m_isRebuildRequired = FALSE;
    }

    return isPrepared;
}

//=========================================================================
//  ALLOCATION AND UPLOAD
//=========================================================================

void ProjectionAtlas::ResetAllocator( void )
{
    m_FreeNodes.SetCount( 0 );

    for( s32 Page = 0; Page < PAGE_COUNT; ++Page )
    {
        FreeNode& Node = m_FreeNodes.Append();
        Node.m_Page    = Page;
        Node.m_X       = 0;
        Node.m_Y       = 0;
        Node.m_Size    = PAGE_SIZE;
    }
}

//=========================================================================

xbool ProjectionAtlas::Allocate( Entry& EntryData )
{
    const texture* pTexture = EntryData.m_Texture.GetPointer();

    if( !pTexture )
    {
        return FALSE;
    }

    const s32 SourceWidth  = pTexture->m_bitmap.GetWidth();
    const s32 SourceHeight = pTexture->m_bitmap.GetHeight();

    if( ( SourceWidth <= 0 ) || ( SourceHeight <= 0 ) )
    {
        return FALSE;
    }

    s32       ImageWidth   = SourceWidth;
    s32       ImageHeight  = SourceHeight;
    const s32 SourceExtent = MAX( SourceWidth, SourceHeight );

    if( SourceExtent > PAGE_SIZE )
    {
        const f32 Scale = static_cast<f32>( PAGE_SIZE ) / static_cast<f32>( SourceExtent );
        ImageWidth      = MAX( 1, static_cast<s32>( static_cast<f32>( SourceWidth ) * Scale + 0.5f ) );
        ImageHeight     = MAX( 1, static_cast<s32>( static_cast<f32>( SourceHeight ) * Scale + 0.5f ) );
    }

    const s32 TileSize = NextPowerOfTwo( MAX( ImageWidth, ImageHeight ) );

    if( TileSize <= 0 )
    {
        return FALSE;
    }

    s32 BestNode = -1;

    for( s32 i = 0; i < m_FreeNodes.GetCount(); ++i )
    {
        const FreeNode& Node = m_FreeNodes[i];

        if( Node.m_Size < TileSize )
        {
            continue;
        }

        if( ( BestNode < 0 ) || ( Node.m_Size < m_FreeNodes[BestNode].m_Size ) ||
            ( ( Node.m_Size == m_FreeNodes[BestNode].m_Size ) && ( Node.m_Page < m_FreeNodes[BestNode].m_Page ) ) ||
            ( ( Node.m_Size == m_FreeNodes[BestNode].m_Size ) && ( Node.m_Page == m_FreeNodes[BestNode].m_Page ) &&
              ( Node.m_Y < m_FreeNodes[BestNode].m_Y ) ) ||
            ( ( Node.m_Size == m_FreeNodes[BestNode].m_Size ) && ( Node.m_Page == m_FreeNodes[BestNode].m_Page ) &&
              ( Node.m_Y == m_FreeNodes[BestNode].m_Y ) && ( Node.m_X < m_FreeNodes[BestNode].m_X ) ) )
        {
            BestNode = i;
        }
    }

    if( BestNode < 0 )
    {
        return FALSE;
    }

    FreeNode Node = m_FreeNodes[BestNode];
    m_FreeNodes.Delete( BestNode );

    while( Node.m_Size > TileSize )
    {
        const s32 ChildSize = Node.m_Size / 2;

        FreeNode& TopRight = m_FreeNodes.Append();
        TopRight.m_Page    = Node.m_Page;
        TopRight.m_X       = Node.m_X + ChildSize;
        TopRight.m_Y       = Node.m_Y;
        TopRight.m_Size    = ChildSize;

        FreeNode& BottomLeft = m_FreeNodes.Append();
        BottomLeft.m_Page    = Node.m_Page;
        BottomLeft.m_X       = Node.m_X;
        BottomLeft.m_Y       = Node.m_Y + ChildSize;
        BottomLeft.m_Size    = ChildSize;

        FreeNode& BottomRight = m_FreeNodes.Append();
        BottomRight.m_Page    = Node.m_Page;
        BottomRight.m_X       = Node.m_X + ChildSize;
        BottomRight.m_Y       = Node.m_Y + ChildSize;
        BottomRight.m_Size    = ChildSize;

        Node.m_Size = ChildSize;
    }

    const s32 ImageX = Node.m_X + ( TileSize - ImageWidth ) / 2;
    const s32 ImageY = Node.m_Y + ( TileSize - ImageHeight ) / 2;

    EntryData.m_TileX         = Node.m_X;
    EntryData.m_TileY         = Node.m_Y;
    EntryData.m_TileSize      = TileSize;
    EntryData.m_ImageWidth    = ImageWidth;
    EntryData.m_ImageHeight   = ImageHeight;
    EntryData.m_Region.Layer  = static_cast<u32>( Node.m_Page );
    EntryData.m_Region.MaxMip = m_hasMipmaps ? Log2PowerOfTwo( TileSize ) : 0.0f;

    const f32 SamplingTileSize = m_hasMipmaps ? static_cast<f32>( TileSize ) : 1.0f;
    const f32 TileMinX         = ImageX - x_floor( ( SamplingTileSize - ImageWidth ) * 0.5f );
    const f32 TileMinY         = ImageY - x_floor( ( SamplingTileSize - ImageHeight ) * 0.5f );
    const f32 InvPageSize      = 1.0f / static_cast<f32>( PAGE_SIZE );
    EntryData.m_Region.TileBounds.Set( TileMinX * InvPageSize, TileMinY * InvPageSize,
                                       ( TileMinX + SamplingTileSize ) * InvPageSize,
                                       ( TileMinY + SamplingTileSize ) * InvPageSize );
    EntryData.m_Region.Sampling.Set( static_cast<f32>( Node.m_Page ), EntryData.m_Region.MaxMip,
                                     static_cast<f32>( MAX( ImageWidth - 1, 0 ) ),
                                     static_cast<f32>( MAX( ImageHeight - 1, 0 ) ) );
    EntryData.m_Region.UVScaleBias.Set(
        ( ImageWidth > 1 ) ? static_cast<f32>( ImageWidth - 1 ) / static_cast<f32>( PAGE_SIZE ) : 0.0f,
        ( ImageHeight > 1 ) ? static_cast<f32>( ImageHeight - 1 ) / static_cast<f32>( PAGE_SIZE ) : 0.0f,
        ( static_cast<f32>( ImageX ) + 0.5f ) / static_cast<f32>( PAGE_SIZE ),
        ( static_cast<f32>( ImageY ) + 0.5f ) / static_cast<f32>( PAGE_SIZE ) );

    return TRUE;
}

//=========================================================================

xbool ProjectionAtlas::Upload( const Entry& EntryData, FrameGraphBuilder& Graph )
{
    const texture* pTexture = EntryData.m_Texture.GetPointer();

    if( !pTexture || ( EntryData.m_TileSize <= 0 ) )
    {
        return FALSE;
    }

    const xbitmap& Bitmap = pTexture->m_bitmap;
    m_Image.SetCount( EntryData.m_ImageWidth * EntryData.m_ImageHeight );

    const xbool isResizeRequired =
        ( EntryData.m_ImageWidth != Bitmap.GetWidth() ) || ( EntryData.m_ImageHeight != Bitmap.GetHeight() );

    for( s32 y = 0; y < EntryData.m_ImageHeight; ++y )
    {
        for( s32 x = 0; x < EntryData.m_ImageWidth; ++x )
        {
            xcolor Color;

            if( isResizeRequired )
            {
                const f32 U = ( static_cast<f32>( x ) + 0.5f ) / static_cast<f32>( EntryData.m_ImageWidth );
                const f32 V = ( static_cast<f32>( y ) + 0.5f ) / static_cast<f32>( EntryData.m_ImageHeight );
                Color       = Bitmap.GetBilinearColor( U, V, TRUE );
            }
            else
            {
                Color = Bitmap.GetPixelColor( x, y );
            }

            m_Image[y * EntryData.m_ImageWidth + x] = EncodeMask( Color, EntryData.m_Encoding );
        }
    }

    m_Tile.SetCount( EntryData.m_TileSize * EntryData.m_TileSize );

    const s32 ImageOffsetX = ( EntryData.m_TileSize - EntryData.m_ImageWidth ) / 2;
    const s32 ImageOffsetY = ( EntryData.m_TileSize - EntryData.m_ImageHeight ) / 2;

    for( s32 y = 0; y < EntryData.m_TileSize; ++y )
    {
        const s32 SourceY = MINMAX( 0, y - ImageOffsetY, EntryData.m_ImageHeight - 1 );

        for( s32 x = 0; x < EntryData.m_TileSize; ++x )
        {
            const s32 SourceX                    = MINMAX( 0, x - ImageOffsetX, EntryData.m_ImageWidth - 1 );
            m_Tile[y * EntryData.m_TileSize + x] = m_Image[SourceY * EntryData.m_ImageWidth + SourceX];
        }
    }

    vram_texture_upload_desc Upload;
    Upload.Region.Layer  = EntryData.m_Region.Layer;
    Upload.Region.X      = EntryData.m_TileX;
    Upload.Region.Y      = EntryData.m_TileY;
    Upload.Region.Width  = EntryData.m_TileSize;
    Upload.Region.Height = EntryData.m_TileSize;
    Upload.pData         = m_Tile.GetPtr();
    Upload.Size          = m_Tile.GetCount();
    Upload.RowPitch      = EntryData.m_TileSize;
    Upload.SlicePitch    = m_Tile.GetCount();

    return Graph.QueueUpload( m_Texture, Upload );
}

//=========================================================================
//  QUERIES
//=========================================================================

ProjectionAtlas::Entry* ProjectionAtlas::FindEntry( const char* pResourceName, ProjectionAtlasEncoding Encoding )
{
    const s32 EntryIndex = FindEntryIndex( pResourceName, Encoding );
    return ( EntryIndex >= 0 ) ? &m_Entries[EntryIndex] : NULL;
}

//=========================================================================

const ProjectionAtlas::Entry* ProjectionAtlas::FindEntry( const char*             pResourceName,
                                                          ProjectionAtlasEncoding Encoding ) const
{
    const s32 EntryIndex = FindEntryIndex( pResourceName, Encoding );
    return ( EntryIndex >= 0 ) ? &m_Entries[EntryIndex] : NULL;
}

//=========================================================================

s32 ProjectionAtlas::FindEntryIndex( const char* pResourceName, ProjectionAtlasEncoding Encoding ) const
{
    if( !pResourceName || !pResourceName[0] || ( m_EntryLookup.GetCount() == 0 ) )
    {
        return -1;
    }

    u32 SlotIndex = HashProjectionKey( pResourceName, Encoding ) & m_EntryLookupMask;

    for( u32 Probe = 0; Probe <= m_EntryLookupMask; ++Probe )
    {
        const s32 EntryIndex = m_EntryLookup[SlotIndex];

        if( EntryIndex < 0 )
        {
            return -1;
        }

        const Entry& EntryData = m_Entries[EntryIndex];

        if( ( EntryData.m_Encoding == Encoding ) && ( EntryData.m_ResourceName == pResourceName ) )
        {
            return EntryIndex;
        }

        SlotIndex = ( SlotIndex + 1 ) & m_EntryLookupMask;
    }

    return -1;
}

//=========================================================================

void ProjectionAtlas::RebuildLookup( void )
{
    u32 TableSize = 8;

    while( TableSize < static_cast<u32>( MAX( 1, m_Entries.GetCount() * 2 ) ) )
    {
        TableSize <<= 1;
    }

    m_EntryLookup.SetCount( TableSize );
    m_EntryLookupMask = TableSize - 1;

    for( u32 i = 0; i < TableSize; ++i )
    {
        m_EntryLookup[i] = -1;
    }

    for( s32 i = 0; i < m_Entries.GetCount(); ++i )
    {
        u32 SlotIndex = HashProjectionKey( m_Entries[i].m_ResourceName, m_Entries[i].m_Encoding ) & m_EntryLookupMask;

        while( m_EntryLookup[SlotIndex] >= 0 )
        {
            SlotIndex = ( SlotIndex + 1 ) & m_EntryLookupMask;
        }

        m_EntryLookup[SlotIndex] = i;
    }
}

//=========================================================================

xbool ProjectionAtlas::IsRequested( const Entry& EntryData ) const
{
    return EntryData.m_LastRequestedFrame == m_iFrameIndex;
}

//=========================================================================

xbool ProjectionAtlas::GetRegion( const texture::handle&  TextureHandle,
                                  ProjectionAtlasEncoding Encoding,
                                  ProjectionAtlasRegion&  Region ) const
{
    if( !TextureHandle.GetPointer() )
    {
        return FALSE;
    }

    const Entry* pEntry = FindEntry( TextureHandle.GetName(), Encoding );

    if( !pEntry || !pEntry->m_isResident )
    {
        return FALSE;
    }

    Region = pEntry->m_Region;

    return TRUE;
}

//=========================================================================

const shader_resource* ProjectionAtlas::GetShaderResource( void ) const
{
    return vram_GetShaderResource( m_Texture );
}

//=========================================================================

vector4 ProjectionAtlas::GetSamplingParams( void ) const
{
    // x/y are inverse dimensions, z is the highest available mip and w is
    // the number of array layers. Keep these values in the projection
    // constant buffer so pixel shaders do not query the resource per sample.
    const f32 MaxMip = m_hasMipmaps ? ( Log2PowerOfTwo( PAGE_SIZE ) ) : 0.0f;
    return vector4( 1.0f / static_cast<f32>( PAGE_SIZE ), 1.0f / static_cast<f32>( PAGE_SIZE ), MaxMip,
                    static_cast<f32>( PAGE_COUNT ) );
}

//=========================================================================

} // namespace render
