//=========================================================================
//
//  FrameGraph_Private.hpp
//
//  Private implementation details shared by FrameGraph translation units.
//
//=========================================================================

#ifndef FRAME_GRAPH_PRIVATE_HPP
#define FRAME_GRAPH_PRIVATE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameGraph.hpp"
#include "e_VRAM.hpp"

namespace render
{

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static xbool ResourceDescsMatch( const RenderResourceDesc& A, const RenderResourceDesc& B );

//=========================================================================

static void CopyClearColor( f32 pDst[4], const f32 pSrc[4] );

static xbool IsSupportedSampleCount( u32 SampleCount );

// Infer COLOR/DEPTH/SAMPLED from the target; callers must declare copy access.
static xbool GetExternalUsage( const rtarget& Target, u32 RequestedUsage, u32& Usage );

//=========================================================================
//  TYPES
//=========================================================================

struct FrameGraphBuilder::Resource
{
    Resource( void );

    void Reset( void );

    rtarget_backend*      pExternalBackend = NULL;
    const rbuffer*        pExternalBuffer = NULL;
    rbuffer_backend*      pExternalBufferBackend = NULL;
    const vram_texture*   pExternalTexture = NULL;
    vram_texture_backend* pExternalTextureBackend = NULL;
    vram_texture_desc     ExternalTextureDesc;
    RenderResourceHandle Handle;
    RenderResourceHandle Physical;
    RenderResourceDesc   Desc;
    xbool                HasInitialContents;
    RenderPassId         Producer;
    xarray<RenderPassId> Readers;
    RenderPassId         LastReadPass;
    s32                  LastReadAccessIndex;
    u32                  FirstUse;
    u32                  LastUse;
};

//-------------------------------------------------------------------------

struct FrameGraphBuilder::PassAccess
{
    PassAccess( void );
    PassAccess( u32 ResourceIn, u32 FlagsIn );

    u32 Resource;
    u32 Flags;
};

//-------------------------------------------------------------------------

struct FrameGraphBuilder::Pass
{
    Pass( void );

    ~Pass( void );

    void Reset( void );

    RenderPassDesc       Desc;
    RenderPassPayload*   pPayload;
    xarray<RenderPassId> Dependencies;
    xarray<RenderPassId> Dependents;
    xarray<u32>          Reads;
    xarray<u32>          Writes;
    xarray<PassAccess>   Accesses;
    u32                  GroupEnd;
    rtarget_store_op     ColorStore[3];
    rtarget_store_op     DepthStore;
    xbool                KeepAlive;
    xbool                Live;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
static xbool ResourceDescsMatch( const RenderResourceDesc& A, const RenderResourceDesc& B )
{
    return ( A.Type == B.Type ) && ( A.Buffer.Size == B.Buffer.Size ) && ( A.Buffer.Stride == B.Buffer.Stride ) &&
           ( A.Buffer.UsageFlags == B.Buffer.UsageFlags ) &&
           ( A.Extent.Width == B.Extent.Width ) && ( A.Extent.Height == B.Extent.Height ) && ( A.Format == B.Format ) &&
           ( A.MipCount == B.MipCount ) &&
           ( A.SampleCount == B.SampleCount ) && ( A.ArrayLayers == B.ArrayLayers ) && ( A.ViewMask == B.ViewMask ) &&
           ( A.Usage == B.Usage ) && ( A.Lifetime == B.Lifetime ) && ( A.PreserveContents == B.PreserveContents ) &&
           ( A.Lifetime != RENDER_RESOURCE_PERSISTENT || A.PersistentKey == B.PersistentKey );
}

//=========================================================================

inline
static u32 GetTextureStorageUsage( u32 Usage )
{
    return ( Usage & RENDER_RESOURCE_USAGE_STORAGE_READ ? VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ : 0 ) |
           ( Usage & RENDER_RESOURCE_USAGE_STORAGE_WRITE ? VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE : 0 ) |
           ( Usage & RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE ? VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE : 0 );
}

//=========================================================================

inline
static xbool IsResourceDescValid( const RenderResourceDesc& Desc )
{
    if( !Desc.pDebugName || Desc.Type < RENDER_RESOURCE_TEXTURE || Desc.Type > RENDER_RESOURCE_BUFFER ||
        Desc.Lifetime < RENDER_RESOURCE_TRANSIENT || Desc.Lifetime >= RENDER_RESOURCE_EXTERNAL ||
        ( Desc.Lifetime == RENDER_RESOURCE_PERSISTENT ) != ( Desc.PersistentKey != 0 ) ||
        ( Desc.PreserveContents && Desc.Lifetime != RENDER_RESOURCE_PERSISTENT ) )
    {
        return FALSE;
    }

    if( Desc.IsBuffer() )
    {
        const u32 KnownUsage = RBUFFER_USAGE_VERTEX | RBUFFER_USAGE_INDEX | RBUFFER_USAGE_INDIRECT |
                               RBUFFER_USAGE_GRAPHICS_STORAGE_READ | RBUFFER_USAGE_COMPUTE_STORAGE_READ |
                               RBUFFER_USAGE_COMPUTE_STORAGE_WRITE;
        return Desc.Buffer.Size && Desc.Buffer.UsageFlags && !( Desc.Buffer.UsageFlags & ~KnownUsage ) &&
               Desc.Buffer.Stride && !( Desc.Buffer.Size % Desc.Buffer.Stride );
    }

    const u32 StorageUsage = RENDER_RESOURCE_USAGE_STORAGE_READ | RENDER_RESOURCE_USAGE_STORAGE_WRITE |
                             RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE;
    const u32 KnownUsage = RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_DEPTH |
                           RENDER_RESOURCE_USAGE_COPY_SRC | RENDER_RESOURCE_USAGE_COPY_DST | StorageUsage;
    const xbool Depth = rtarget_IsDepthFormat( Desc.Format );

    return Desc.Extent.IsValid() && Desc.Format >= 0 && Desc.Format < RTARGET_FORMAT_COUNT &&
           IsSupportedSampleCount( Desc.SampleCount ) && Desc.ArrayLayers == 1 && Desc.ViewMask == 1 &&
           Desc.MipCount && Desc.MipCount <= vram_CalcMipCount( Desc.Extent.Width, Desc.Extent.Height ) &&
           !( Desc.SampleCount > 1 && Desc.MipCount != 1 ) &&
           Desc.Usage && !( Desc.Usage & ~KnownUsage ) &&
           !( ( Desc.Usage & RENDER_RESOURCE_USAGE_SAMPLED ) &&
              ( Desc.Format == RTARGET_FORMAT_R8_UINT || Desc.Format == RTARGET_FORMAT_R32_UINT ) ) &&
           ( Depth ? ( Desc.Usage & RENDER_RESOURCE_USAGE_DEPTH ) &&
                       !( Desc.Usage & ( RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_STORAGE_WRITE |
                                         RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE ) )
                   : !( Desc.Usage & RENDER_RESOURCE_USAGE_DEPTH ) &&
                       ( Desc.Usage & ( RENDER_RESOURCE_USAGE_COLOR | StorageUsage ) ) ) &&
           !( ( Desc.Usage & RENDER_RESOURCE_USAGE_SAMPLED ) && ( Desc.Usage & RENDER_RESOURCE_USAGE_STORAGE_READ ) ) &&
           !( ( Desc.Usage & RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE ) &&
              ( Desc.Usage & ( RENDER_RESOURCE_USAGE_STORAGE_READ | RENDER_RESOURCE_USAGE_STORAGE_WRITE ) ) ) &&
           !( Desc.SampleCount > 1 &&
              ( Desc.Usage & ( RENDER_RESOURCE_USAGE_STORAGE_WRITE | RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE |
                                RENDER_RESOURCE_USAGE_COPY_SRC | RENDER_RESOURCE_USAGE_COPY_DST |
                                RENDER_RESOURCE_USAGE_SAMPLED ) ) );
}

//=========================================================================

inline
static void CopyClearColor( f32 pDst[4], const f32 pSrc[4] )
{
    for( s32 i = 0; i < 4; ++i )
    {
        pDst[i] = pSrc[i];
    }
}

//=========================================================================

inline
static xbool IsSupportedSampleCount( u32 SampleCount )
{
    return ( SampleCount == 1 ) || ( SampleCount == 2 ) || ( SampleCount == 4 ) || ( SampleCount == 8 );
}

//=========================================================================

inline
static xbool GetExternalUsage( const rtarget& Target, u32 RequestedUsage, u32& Usage )
{
    const u32 StorageUsage =
        ( Target.Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ
              ? RENDER_RESOURCE_USAGE_STORAGE_READ : 0 ) |
        ( Target.Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE
              ? RENDER_RESOURCE_USAGE_STORAGE_WRITE : 0 ) |
        ( Target.Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE
              ? RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE : 0 );
    const u32 KnownUsage = RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_DEPTH |
                           RENDER_RESOURCE_USAGE_COPY_SRC | RENDER_RESOURCE_USAGE_COPY_DST |
                           RENDER_RESOURCE_USAGE_STORAGE_READ | RENDER_RESOURCE_USAGE_STORAGE_WRITE |
                           RENDER_RESOURCE_USAGE_STORAGE_READ_WRITE;
    const u32 ActualUsage = ( Target.Desc.bBindAsTexture && rtarget_HasShaderResource( Target )
                                 ? RENDER_RESOURCE_USAGE_SAMPLED : 0 ) |
                            ( rtarget_HasRenderTarget( Target ) ? RENDER_RESOURCE_USAGE_COLOR : 0 ) |
                            ( rtarget_HasDepthStencil( Target ) ? RENDER_RESOURCE_USAGE_DEPTH : 0 ) | StorageUsage;

    if( ( RequestedUsage & ~KnownUsage ) ||
        ( ( RequestedUsage & RENDER_RESOURCE_USAGE_SAMPLED ) && !( ActualUsage & RENDER_RESOURCE_USAGE_SAMPLED ) ) ||
        ( ( RequestedUsage & RENDER_RESOURCE_USAGE_COLOR ) && !( ActualUsage & RENDER_RESOURCE_USAGE_COLOR ) ) ||
        ( ( RequestedUsage & RENDER_RESOURCE_USAGE_DEPTH ) && !( ActualUsage & RENDER_RESOURCE_USAGE_DEPTH ) ) ||
        ( ( RequestedUsage & ~( RENDER_RESOURCE_USAGE_COPY_SRC | RENDER_RESOURCE_USAGE_COPY_DST ) ) & ~ActualUsage ) ||
        !rtarget_IsValid( Target ) )
    {
        return FALSE;
    }

    Usage = ActualUsage | RequestedUsage;

    return TRUE;
}

//=========================================================================

inline
FrameGraphBuilder::Resource::Resource( void )
    : Handle(),
      Physical(),
      Desc(),
      HasInitialContents( FALSE ),
      Producer( INVALID_RENDER_PASS ),
      LastReadPass( INVALID_RENDER_PASS ),
      LastReadAccessIndex( -1 ),
      FirstUse( 0xffffffffu ),
      LastUse( 0 )
{
}

//=========================================================================

inline
void FrameGraphBuilder::Resource::Reset( void )
{
    Handle             = RenderResourceHandle();
    Physical           = RenderResourceHandle();
    Desc               = RenderResourceDesc();
    HasInitialContents = FALSE;
    pExternalBackend   = NULL;
    pExternalBuffer    = NULL;
    pExternalBufferBackend = NULL;
    pExternalTexture = NULL;
    pExternalTextureBackend = NULL;
    ExternalTextureDesc = vram_texture_desc();
    Producer           = INVALID_RENDER_PASS;
    Readers.SetCount( 0 );
    LastReadPass        = INVALID_RENDER_PASS;
    LastReadAccessIndex = -1;
    FirstUse = 0xffffffffu;
    LastUse  = 0;
}

//=========================================================================

inline
FrameGraphBuilder::PassAccess::PassAccess( void )
    : Resource( 0xffffffffu ),
      Flags( 0 )
{
}

//=========================================================================

inline
FrameGraphBuilder::PassAccess::PassAccess( u32 ResourceIn, u32 FlagsIn )
    : Resource( ResourceIn ),
      Flags( FlagsIn )
{
}

//=========================================================================

inline
FrameGraphBuilder::Pass::Pass( void )
    : Desc(),
      pPayload( NULL ),
      Dependencies(),
      Dependents(),
      Reads(),
      Writes(),
      Accesses(),
      KeepAlive( FALSE ),
      Live( FALSE )
{
}

//=========================================================================

inline
FrameGraphBuilder::Pass::~Pass( void )
{
}

//=========================================================================

inline
void FrameGraphBuilder::Pass::Reset( void )
{
    Desc     = RenderPassDesc();
    pPayload = NULL;
    Dependencies.SetCount( 0 );
    Dependents.SetCount( 0 );
    Reads.SetCount( 0 );
    Writes.SetCount( 0 );
    Accesses.SetCount( 0 );
    KeepAlive = FALSE;
    Live      = FALSE;
}

} // namespace render

#endif // FRAME_GRAPH_PRIVATE_HPP
