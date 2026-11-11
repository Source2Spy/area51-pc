//==============================================================================
//
//  sdleng_renderbuffer.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    SDLENG_MAX_VERTEX_BUFFER_SLOTS = 16,
    SDLENG_MAX_BUFFER_UPLOAD_BATCH = 64
};

//==============================================================================
//  TYPES
//==============================================================================

struct sdlbuffer_vertex_binding_cache
{
    SDL_GPUBuffer* pBuffer;
    u32            Offset;
};

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static rbuffer_backend* s_pBufferList = NULL;
static u32              s_BufferCount = 0;

static sdlbuffer_vertex_binding_cache s_VertexBindings[SDLENG_MAX_VERTEX_BUFFER_SLOTS];
static SDL_GPUBufferBinding           s_VertexBatchBindings[SDLENG_MAX_VERTEX_BUFFER_SLOTS];
static xbool                          s_VertexBatchUsed[SDLENG_MAX_VERTEX_BUFFER_SLOTS];
static SDL_GPUBuffer*                 s_pIndexBuffer = NULL;
static u32                            s_IndexOffset  = 0;
static rbuffer_index_format           s_IndexFormat  = RBUFFER_INDEX_FORMAT_U16;

//==============================================================================
//  HELPERS
//==============================================================================

static
void sdlbuffer_InvalidateBindings( SDL_GPUBuffer* pBuffer )
{
    for( u32 i = 0; i < SDLENG_MAX_VERTEX_BUFFER_SLOTS; ++i )
    {
        if( s_VertexBindings[i].pBuffer == pBuffer )
        {
            s_VertexBindings[i].pBuffer = NULL;
        }
    }

    if( s_pIndexBuffer == pBuffer )
    {
        s_pIndexBuffer = NULL;
    }
}

//==============================================================================

static
xbool sdlbuffer_HasAnyFlag( u32 Flags, u32 TestFlags )
{
    return ( Flags & TestFlags ) != 0;
}

//==============================================================================

static
u32 sdlbuffer_CountFlags( u32 Flags )
{
    u32 Count = 0;

    while( Flags )
    {
        Count += ( Flags & 1 );
        Flags >>= 1;
    }

    return Count;
}

//==============================================================================

static
xbool sdlbuffer_IsStorageUsage( u32 UsageFlags )
{
    const u32 StorageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ |
                             RBUFFER_USAGE_COMPUTE_STORAGE_READ |
                             RBUFFER_USAGE_COMPUTE_STORAGE_WRITE;

    return sdlbuffer_HasAnyFlag( UsageFlags, StorageFlags );
}

//==============================================================================

static
xbool sdlbuffer_ValidateUsageFlags( u32 UsageFlags )
{
    const u32 KnownFlags = RBUFFER_USAGE_VERTEX |
                           RBUFFER_USAGE_INDEX |
                           RBUFFER_USAGE_INDIRECT |
                           RBUFFER_USAGE_GRAPHICS_STORAGE_READ |
                           RBUFFER_USAGE_COMPUTE_STORAGE_READ |
                           RBUFFER_USAGE_COMPUTE_STORAGE_WRITE;

    if( ( UsageFlags == 0 ) || ( UsageFlags & ~KnownFlags ) )
    {
        return FALSE;
    }

    const u32 DrawInputFlags = UsageFlags & ( RBUFFER_USAGE_VERTEX |
                                              RBUFFER_USAGE_INDEX |
                                              RBUFFER_USAGE_INDIRECT );

    if( sdlbuffer_CountFlags( DrawInputFlags ) > 1 )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlbuffer_ToSDLUsageFlags( u32 UsageFlags, SDL_GPUBufferUsageFlags& SDLFlags )
{
    if( !sdlbuffer_ValidateUsageFlags( UsageFlags ) )
    {
        return FALSE;
    }

    SDLFlags = 0;

    if( UsageFlags & RBUFFER_USAGE_VERTEX )
    {
        SDLFlags |= SDL_GPU_BUFFERUSAGE_VERTEX;
    }

    if( UsageFlags & RBUFFER_USAGE_INDEX )
    {
        SDLFlags |= SDL_GPU_BUFFERUSAGE_INDEX;
    }

    if( UsageFlags & RBUFFER_USAGE_INDIRECT )
    {
        SDLFlags |= SDL_GPU_BUFFERUSAGE_INDIRECT;
    }

    if( UsageFlags & RBUFFER_USAGE_GRAPHICS_STORAGE_READ )
    {
        SDLFlags |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    }

    if( UsageFlags & RBUFFER_USAGE_COMPUTE_STORAGE_READ )
    {
        SDLFlags |= SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    }

    if( UsageFlags & RBUFFER_USAGE_COMPUTE_STORAGE_WRITE )
    {
        SDLFlags |= SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    }

    return SDLFlags != 0;
}

//==============================================================================

static
xbool sdlbuffer_ToSDLIndexFormat( rbuffer_index_format     Format,
                                  SDL_GPUIndexElementSize& SDLFormat )
{
    switch( Format )
    {
        case RBUFFER_INDEX_FORMAT_U16:
        {
            SDLFormat = SDL_GPU_INDEXELEMENTSIZE_16BIT;
            return TRUE;
        }

        case RBUFFER_INDEX_FORMAT_U32:
        {
            SDLFormat = SDL_GPU_INDEXELEMENTSIZE_32BIT;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//==============================================================================

static
void sdlbuffer_ReleaseBackend( rbuffer_backend* pBackend )
{
    if( !pBackend )
    {
        return;
    }

    if( pBackend->pBuffer && g_pSDLGPUDevice )
    {
        SDL_ReleaseGPUBuffer( g_pSDLGPUDevice, pBackend->pBuffer );
    }

    pBackend->pBuffer                  = NULL;
    pBackend->Resource.pBackend        = NULL;
    pBackend->ResourceBackend.Kind     = SDLENG_SHADER_RESOURCE_NONE;
    pBackend->ResourceBackend.pTexture = NULL;
    pBackend->ResourceBackend.pBuffer  = NULL;
}

//==============================================================================

static
u32 sdlbuffer_AlignUploadOffset( u32 Offset )
{
    const u32 Alignment = 4;
    const u32 Remainder = Offset % Alignment;

    if( !Remainder )
    {
        return Offset;
    }

    const u32 Padding = Alignment - Remainder;

    if( Offset > ( 0xffffffffu - Padding ) )
    {
        return 0xffffffffu;
    }

    return Offset + Padding;
}

//==============================================================================

static
xbool sdlbuffer_SubmitUploads( const rbuffer_upload_batch_item* pItems, u32 Count )
{
    static xprofile_counter UploadRequests =
        x_GetProfiler().RegisterCounter( "BufferUploadRequests", "RenderCounter" );

    if( Count == 0 )
    {
        return TRUE;
    }

    if( !pItems || ( Count > SDLENG_MAX_BUFFER_UPLOAD_BATCH ) ||
        !g_pSDLGPUDevice || sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        return FALSE;
    }

    u32 UploadOffsets[SDLENG_MAX_BUFFER_UPLOAD_BATCH];

    u64 TotalSize64 = 0;

    for( u32 i = 0; i < Count; ++i )
    {
        rbuffer*                   pBuffer = pItems[i].pBuffer;
        rbuffer_upload_desc const& Upload  = pItems[i].Upload;

        if( !pBuffer || !pBuffer->pBackend || !pBuffer->pBackend->pBuffer ||
            !shader_ValidateResourceAccess( pBuffer->pBackend->Resource, SHADER_RESOURCE_ACCESS_UPLOAD ) ||
            !Upload.pData || ( Upload.Size == 0 ) || ( Upload.Offset > pBuffer->Desc.Size ) ||
            ( Upload.Size > ( pBuffer->Desc.Size - Upload.Offset ) ) )
        {
            return FALSE;
        }

        for( u32 j = 0; j < i; ++j )
        {
            if( pItems[j].pBuffer == pBuffer )
            {
                sdleng_LogError( "SDLBuffer", "duplicate buffer in upload batch" );
                return FALSE;
            }
        }

        if( TotalSize64 > 0xffffffffULL )
        {
            return FALSE;
        }

        const u32 Offset = sdlbuffer_AlignUploadOffset( (u32)TotalSize64 );

        if( ( Offset == 0xffffffffu ) || ( Upload.Size > ( 0xffffffffu - Offset ) ) )
        {
            return FALSE;
        }

        TotalSize64 = (u64)Offset + Upload.Size;
    }

    if( ( TotalSize64 == 0 ) || ( TotalSize64 > 0xffffffffULL ) )
    {
        return FALSE;
    }

    if( !sdleng_BeginTransfer( (u32)TotalSize64, 4, "SDLBuffer" ) )
    {
        return FALSE;
    }

    for( u32 i = 0; i < Count; ++i )
    {
        if( !sdleng_ReserveTransfer( pItems[i].Upload.Size, 4, UploadOffsets[i] ) )
        {
            sdleng_CancelTransfer();
            return FALSE;
        }
    }

    void* pMapped = NULL;

    if( !sdleng_MapTransferBuffer( pMapped, "SDLBuffer" ) )
    {
        sdleng_CancelTransfer();
        return FALSE;
    }

    SDL_GPUTransferBuffer* pTransferBuffer = sdleng_GetTransferBuffer();

    for( u32 i = 0; i < Count; ++i )
    {
        rbuffer_upload_desc const& Upload = pItems[i].Upload;
        x_memcpy( (u8*)pMapped + UploadOffsets[i], Upload.pData, Upload.Size );
    }

    sdleng_UnmapTransferBuffer();

    SDL_GPUCopyPass* pCopyPass = sdleng_GetTransferCopyPass();

    if( !pTransferBuffer || !pCopyPass )
    {
        sdleng_CancelTransfer();
        return FALSE;
    }

    for( u32 i = 0; i < Count; ++i )
    {
        rbuffer*                      pBuffer = pItems[i].pBuffer;
        rbuffer_upload_desc const&    Upload  = pItems[i].Upload;
        SDL_GPUTransferBufferLocation Source;
        x_memset( &Source, 0, sizeof( Source ) );
        Source.transfer_buffer = pTransferBuffer;
        Source.offset          = UploadOffsets[i];

        SDL_GPUBufferRegion Destination;
        x_memset( &Destination, 0, sizeof( Destination ) );
        Destination.buffer = pBuffer->pBackend->pBuffer;
        Destination.offset = Upload.Offset;
        Destination.size   = Upload.Size;

        SDL_UploadToGPUBuffer( pCopyPass,
                               &Source,
                               &Destination,
                               Upload.bCycle ? true : false );
    }

    UploadRequests.Add( Count );

    return sdleng_CommitTransfer();
}

//==============================================================================
//  RENDER BUFFER FUNCTIONS
//==============================================================================
//  BUFFER LIFETIME
//==============================================================================

xbool rbuffer_Create( rbuffer& Buffer, const rbuffer_desc& Desc, const void* pInitialData )
{
    if( Buffer.pBackend && ( sdleng_InRenderPass() || sdleng_InComputePass() ) )
    {
        return FALSE;
    }

    if( !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    if( ( Desc.Size < 4 ) || !sdlbuffer_ValidateUsageFlags( Desc.UsageFlags ) )
    {
        return FALSE;
    }

    if( ( Desc.Stride != 0 ) && ( Desc.Size % Desc.Stride ) )
    {
        return FALSE;
    }

    SDL_GPUBufferUsageFlags UsageFlags;

    if( !sdlbuffer_ToSDLUsageFlags( Desc.UsageFlags, UsageFlags ) )
    {
        return FALSE;
    }

    SDL_PropertiesID Props = 0;

    if( Desc.pDebugName && Desc.pDebugName[0] )
    {
        Props = SDL_CreateProperties();

        if( !Props )
        {
            sdleng_LogError( "SDLBuffer", "SDL_CreateProperties" );
            return FALSE;
        }

        if( !SDL_SetStringProperty( Props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, Desc.pDebugName ) )
        {
            sdleng_LogError( "SDLBuffer", "SDL_SetStringProperty" );
            SDL_DestroyProperties( Props );
            return FALSE;
        }
    }

    SDL_GPUBufferCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof( CreateInfo ) );
    CreateInfo.usage = UsageFlags;
    CreateInfo.size  = Desc.Size;
    CreateInfo.props = Props;

    SDL_GPUBuffer* pBuffer = SDL_CreateGPUBuffer( g_pSDLGPUDevice, &CreateInfo );

    if( Props )
    {
        SDL_DestroyProperties( Props );
    }

    if( !pBuffer )
    {
        sdleng_LogError( "SDLBuffer", "SDL_CreateGPUBuffer" );
        return FALSE;
    }

    rbuffer_backend* pBackend = new rbuffer_backend;

    if( !pBackend )
    {
        SDL_ReleaseGPUBuffer( g_pSDLGPUDevice, pBuffer );
        return FALSE;
    }

    rbuffer_backend* pOldBackend = Buffer.pBackend;
    rbuffer_desc     OldDesc     = Buffer.Desc;

    pBackend->pOwner  = &Buffer;
    pBackend->pBuffer = pBuffer;

    pBackend->Resource.pBackend            = &pBackend->ResourceBackend;
    pBackend->ResourceBackend.Kind         = SDLENG_SHADER_RESOURCE_BUFFER;
    pBackend->ResourceBackend.pBuffer      = pBuffer;
    pBackend->ResourceBackend.UsageFlags   = UsageFlags;
    pBackend->ResourceBackend.pBufferOwner = &Buffer;

    Buffer.Desc     = Desc;
    Buffer.pBackend = pBackend;

    if( pInitialData )
    {
        if( !rbuffer_Upload( Buffer, pInitialData, Desc.Size ) )
        {
            Buffer.Desc      = OldDesc;
            Buffer.pBackend  = pOldBackend;
            pBackend->pOwner = NULL;
            sdlbuffer_ReleaseBackend( pBackend );
            delete pBackend;
            return FALSE;
        }
    }

    if( pOldBackend )
    {
        sdlbuffer_InvalidateBindings( pOldBackend->pBuffer );
        sdleng_UnlinkBackend( s_pBufferList, pOldBackend, s_BufferCount );
        sdlbuffer_ReleaseBackend( pOldBackend );
        delete pOldBackend;
    }

    sdleng_LinkBackend( s_pBufferList, pBackend, s_BufferCount );

    return TRUE;
}

//==============================================================================

void rbuffer_Swap( rbuffer& A, rbuffer& B )
{
    if( &A == &B )
    {
        return;
    }

    rbuffer_desc     Desc     = A.Desc;
    rbuffer_backend* pBackend = A.pBackend;
    A.Desc                    = B.Desc;
    A.pBackend                = B.pBackend;
    B.Desc                    = Desc;
    B.pBackend                = pBackend;

    if( A.pBackend )
    {
        A.pBackend->pOwner                       = &A;
        A.pBackend->ResourceBackend.pBufferOwner = &A;
    }

    if( B.pBackend )
    {
        B.pBackend->pOwner                       = &B;
        B.pBackend->ResourceBackend.pBufferOwner = &B;
    }
}

//==============================================================================

void rbuffer_Destroy( rbuffer& Buffer )
{
    rbuffer_backend* pBackend = Buffer.pBackend;

    if( !pBackend )
    {
        return;
    }

    if( sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        ASSERTS( FALSE, "Cannot destroy a render buffer during an active GPU pass" );
        return;
    }

    sdleng_ResetShaderBindings();
    sdlbuffer_InvalidateBindings( pBackend->pBuffer );
    sdleng_UnlinkBackend( s_pBufferList, pBackend, s_BufferCount );
    sdlbuffer_ReleaseBackend( pBackend );
    delete pBackend;

    Buffer.Desc     = rbuffer_desc();
    Buffer.pBackend = NULL;
}

//==============================================================================

xbool rbuffer_UploadBatch( const rbuffer_upload_batch_item* pItems, u32 Count )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "BufferUploads" );

    if( Count && !pItems )
    {
        return FALSE;
    }

    return sdlbuffer_SubmitUploads( pItems, Count );
}

//==============================================================================
//  BUFFER UPLOADS
//==============================================================================

xbool rbuffer_Upload( rbuffer& Buffer, const rbuffer_upload_desc& Upload )
{
    rbuffer_upload_batch_item Item;
    Item.pBuffer = &Buffer;
    Item.Upload  = Upload;

    return rbuffer_UploadBatch( &Item, 1 );
}

//==============================================================================

xbool rbuffer_Upload( rbuffer& Buffer, const void* pData, u32 Size, u32 Offset, xbool bCycle )
{
    rbuffer_upload_desc Upload;
    Upload.pData  = pData;
    Upload.Size   = Size;
    Upload.Offset = Offset;
    Upload.bCycle = bCycle;

    return rbuffer_Upload( Buffer, Upload );
}

//==============================================================================
//  BUFFER BINDINGS
//==============================================================================

xbool rbuffer_BindVertex( const rbuffer& Buffer, u32 Slot, u32 Offset )
{
    if( !Buffer.pBackend || !Buffer.pBackend->pBuffer )
    {
        return FALSE;
    }

    if( !shader_ValidateResourceAccess( *rbuffer_GetResource( Buffer ), SHADER_RESOURCE_ACCESS_VERTEX ) )
    {
        return FALSE;
    }

    if( !( Buffer.Desc.UsageFlags & RBUFFER_USAGE_VERTEX ) )
    {
        return FALSE;
    }

    if( Offset >= Buffer.Desc.Size )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        return FALSE;
    }

    if( Slot >= SDLENG_MAX_VERTEX_BUFFER_SLOTS )
    {
        return FALSE;
    }

    sdlbuffer_vertex_binding_cache& Cached  = s_VertexBindings[Slot];
    const xbool                     bIssued = ( Cached.pBuffer != Buffer.pBackend->pBuffer ) ||
                          ( Cached.Offset != Offset );

    static xprofile_counter BufferBindRequestMetric =
        x_GetProfiler().RegisterCounter( "BufferBindRequests", "Renderer" );

    static xprofile_counter BufferBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "BufferBindIssued", "Renderer" );

    BufferBindRequestMetric.Add();

    if( bIssued )
    {
        BufferBindIssuedMetric.Add();
    }

    if( !bIssued )
    {
        return TRUE;
    }

    SDL_GPUBufferBinding Binding;
    Binding.buffer = Buffer.pBackend->pBuffer;
    Binding.offset = Offset;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_BindGPUVertexBuffers( pRenderPass, Slot, &Binding, 1 );
    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone BufferBindMetric =
            x_GetProfiler().RegisterZone( "BufferBindAPI", "RendererAPI" );

        BufferBindMetric.Record( End - Start );
    }

    Cached.pBuffer = Binding.buffer;
    Cached.Offset  = Binding.offset;

    return TRUE;
}

//==============================================================================

xbool rbuffer_BindVertex( const rbuffer_vertex_binding* pBindings, u32 Count )
{
    if( Count == 0 )
    {
        return TRUE;
    }

    if( !pBindings )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        return FALSE;
    }

    u32 MaxSlot = 0;

    for( u32 i = 0; i < Count; i++ )
    {
        const rbuffer_vertex_binding& Binding = pBindings[i];

        if( !Binding.pBuffer || !Binding.pBuffer->pBackend ||
            !Binding.pBuffer->pBackend->pBuffer ||
            !( Binding.pBuffer->Desc.UsageFlags & RBUFFER_USAGE_VERTEX ) ||
            !shader_ValidateResourceAccess( *rbuffer_GetResource( *Binding.pBuffer ), SHADER_RESOURCE_ACCESS_VERTEX ) ||
            ( Binding.Offset >= Binding.pBuffer->Desc.Size ) ||
            ( Binding.Slot >= SDLENG_MAX_VERTEX_BUFFER_SLOTS ) )
        {
            return FALSE;
        }

        if( Binding.Slot > MaxSlot )
        {
            MaxSlot = Binding.Slot;
        }
    }

    for( u32 Slot = 0; Slot <= MaxSlot; ++Slot )
    {
        s_VertexBatchUsed[Slot] = FALSE;
    }

    static xprofile_counter BufferBindRequestMetric =
        x_GetProfiler().RegisterCounter( "BufferBindRequests", "Renderer" );

    static xprofile_counter BufferBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "BufferBindIssued", "Renderer" );

    for( u32 i = 0; i < Count; i++ )
    {
        const rbuffer_vertex_binding&   Binding = pBindings[i];
        sdlbuffer_vertex_binding_cache& Cached  = s_VertexBindings[Binding.Slot];
        SDL_GPUBuffer*                  pBuffer = Binding.pBuffer->pBackend->pBuffer;
        const xbool                     bIssued = ( Cached.pBuffer != pBuffer ) ||
                              ( Cached.Offset != Binding.Offset );

        BufferBindRequestMetric.Add();

        if( bIssued )
        {
            BufferBindIssuedMetric.Add();
        }

        if( !bIssued )
        {
            continue;
        }

        s_VertexBatchBindings[Binding.Slot].buffer = pBuffer;
        s_VertexBatchBindings[Binding.Slot].offset = Binding.Offset;
        s_VertexBatchUsed[Binding.Slot]            = TRUE;
        Cached.pBuffer                             = pBuffer;
        Cached.Offset                              = Binding.Offset;
    }

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    u32         Slot        = 0;

    while( Slot <= MaxSlot )
    {
        while( ( Slot <= MaxSlot ) && !s_VertexBatchUsed[Slot] )
        {
            ++Slot;
        }

        if( Slot > MaxSlot )
        {
            break;
        }

        const u32 FirstSlot = Slot;

        while( ( Slot <= MaxSlot ) && s_VertexBatchUsed[Slot] )
        {
            ++Slot;
        }

        SDL_BindGPUVertexBuffers( pRenderPass,
                                  FirstSlot,
                                  &s_VertexBatchBindings[FirstSlot],
                                  Slot - FirstSlot );
    }

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone BufferBindMetric =
            x_GetProfiler().RegisterZone( "BufferBindAPI", "RendererAPI" );

        BufferBindMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool rbuffer_BindIndex( const rbuffer& Buffer, rbuffer_index_format Format, u32 Offset )
{
    if( !Buffer.pBackend || !Buffer.pBackend->pBuffer )
    {
        return FALSE;
    }

    if( !shader_ValidateResourceAccess( *rbuffer_GetResource( Buffer ), SHADER_RESOURCE_ACCESS_INDEX ) )
    {
        return FALSE;
    }

    if( !( Buffer.Desc.UsageFlags & RBUFFER_USAGE_INDEX ) )
    {
        return FALSE;
    }

    if( Offset >= Buffer.Desc.Size )
    {
        return FALSE;
    }

    SDL_GPUIndexElementSize SDLFormat;

    if( !sdlbuffer_ToSDLIndexFormat( Format, SDLFormat ) )
    {
        return FALSE;
    }

    const u32 IndexAlignment = ( Format == RBUFFER_INDEX_FORMAT_U32 ) ? sizeof( u32 ) : sizeof( u16 );

    if( Offset & ( IndexAlignment - 1 ) )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        return FALSE;
    }

    const xbool bIssued = ( s_pIndexBuffer != Buffer.pBackend->pBuffer ) ||
                          ( s_IndexOffset != Offset ) ||
                          ( s_IndexFormat != Format );

    static xprofile_counter BufferBindRequestMetric =
        x_GetProfiler().RegisterCounter( "BufferBindRequests", "Renderer" );

    static xprofile_counter BufferBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "BufferBindIssued", "Renderer" );

    BufferBindRequestMetric.Add();

    if( bIssued )
    {
        BufferBindIssuedMetric.Add();
    }

    if( !bIssued )
    {
        return TRUE;
    }

    SDL_GPUBufferBinding Binding;
    Binding.buffer = Buffer.pBackend->pBuffer;
    Binding.offset = Offset;

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_BindGPUIndexBuffer( pRenderPass, &Binding, SDLFormat );
    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone BufferBindMetric =
            x_GetProfiler().RegisterZone( "BufferBindAPI", "RendererAPI" );

        BufferBindMetric.Record( End - Start );
    }

    s_pIndexBuffer = Binding.buffer;
    s_IndexOffset  = Binding.offset;
    s_IndexFormat  = Format;

    return TRUE;
}

//==============================================================================

void sdleng_ResetBufferBindings( void )
{
    for( u32 i = 0; i < SDLENG_MAX_VERTEX_BUFFER_SLOTS; ++i )
    {
        s_VertexBindings[i].pBuffer = NULL;
        s_VertexBindings[i].Offset  = 0;
    }

    s_pIndexBuffer = NULL;
    s_IndexOffset  = 0;
    s_IndexFormat  = RBUFFER_INDEX_FORMAT_U16;
}

//==============================================================================
//  BUFFER ACCESSORS
//==============================================================================

xbool rbuffer_IsValid( const rbuffer& Buffer )
{
    return ( Buffer.pBackend != NULL ) && ( Buffer.pBackend->pBuffer != NULL );
}

//==============================================================================

const rbuffer_desc* rbuffer_GetDesc( const rbuffer& Buffer )
{
    return rbuffer_IsValid( Buffer ) ? &Buffer.Desc : NULL;
}

//==============================================================================

const shader_resource* rbuffer_GetResource( const rbuffer& Buffer )
{
    if( !rbuffer_IsValid( Buffer ) ||
        !Buffer.pBackend->Resource.pBackend )
    {
        return NULL;
    }

    return &Buffer.pBackend->Resource;
}

//==============================================================================

u32 sdleng_GetBufferCount( void )
{
    return s_BufferCount;
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
