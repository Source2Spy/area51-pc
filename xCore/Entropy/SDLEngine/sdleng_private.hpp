//==============================================================================
//
//  sdleng_private.hpp
//
//==============================================================================

#ifndef SDLENG_PRIVATE_HPP
#define SDLENG_PRIVATE_HPP

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "sdleng_window.hpp"
#include "e_Shader.hpp"
#include "Shader/ecs_package_store.hpp"
#include "e_RenderBuffer.hpp"
#include "e_RenderDraw.hpp"
#include "e_RenderTarget.hpp"
#include "e_RenderState.hpp"
#include "e_VRAM.hpp"

#include "SDL3/SDL.h"

//==============================================================================
//  CONFIGURATION
//==============================================================================

#ifndef SDLENG_GPU_DRIVER_NAME
    #define SDLENG_GPU_DRIVER_NAME "vulkan" // DO NOT USE DX12!
#endif

#ifndef SDLENG_GPU_SHADER_FORMATS
    #define SDLENG_GPU_SHADER_FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL)
#endif

#ifndef SDLENG_GPU_SWAPCHAIN_COMPOSITION
    #define SDLENG_GPU_SWAPCHAIN_COMPOSITION SDL_GPU_SWAPCHAINCOMPOSITION_SDR
#endif

//==============================================================================
//  LIMITS
//==============================================================================

enum
{
    SDLENG_MAX_2D_TEXTURE_DIMENSION    = 16384,
    SDLENG_MAX_3D_TEXTURE_DIMENSION    = 2048,
    SDLENG_MAX_SAMPLER_SLOTS           = 16,
    SDLENG_MAX_STORAGE_TEXTURE_SLOTS   = 8,
    SDLENG_MAX_STORAGE_BUFFER_SLOTS    = 8,
    SDLENG_MAX_UNIFORM_BUFFER_SLOTS    = 4,
    SDLENG_MAX_SHADER_BINDINGS         = SDLENG_MAX_SAMPLER_SLOTS +
                                         SDLENG_MAX_STORAGE_TEXTURE_SLOTS * 2 +
                                         SDLENG_MAX_STORAGE_BUFFER_SLOTS * 2 +
                                         SDLENG_MAX_UNIFORM_BUFFER_SLOTS,
    SDLENG_SHADER_BINDING_LOOKUP_SLOTS = 128
};

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

struct ecs_loaded_package;
struct render_pipeline_cache_entry;
struct rstate_sampler_native;

//==============================================================================
//  SHARED STATE TYPES
//==============================================================================

struct sdleng_transfer_stats
{
    u64 ReservedBytes;
    u64 UsedBytes;
    u32 BufferCount;
    u64 Creates;
    u64 Trims;
};

//------------------------------------------------------------------------------

struct sdleng_dynamic_state
{
    SDL_GPUViewport Viewport;
    SDL_Rect        Scissor;
    SDL_FColor      Blend;
    u8              Stencil;
    xbool           ViewportValid;
    xbool           ScissorValid;
    xbool           BlendValid;
    xbool           StencilValid;
};

//------------------------------------------------------------------------------

enum sdleng_present_policy
{
    SDLENG_PRESENT_VSYNC = 0,
    SDLENG_PRESENT_MAILBOX,
    SDLENG_PRESENT_IMMEDIATE
};

//==============================================================================
//  BACKEND RESOURCE HANDLES
//==============================================================================

enum sdleng_shader_resource_kind
{
    SDLENG_SHADER_RESOURCE_NONE = 0,
    SDLENG_SHADER_RESOURCE_TEXTURE,
    SDLENG_SHADER_RESOURCE_BUFFER
};

//------------------------------------------------------------------------------

struct shader_resource_backend
{
    sdleng_shader_resource_kind Kind;
    SDL_GPUTexture*             pTexture;
    SDL_GPUBuffer*              pBuffer;
    u32                         UsageFlags;
    u32                         MipCount;
    u32                         LayerCount;
    const rbuffer*              pBufferOwner;
    const rtarget*              pTargetOwner;
    const vram_texture*         pTextureOwner;

    shader_resource_backend( void ) :
        Kind         ( SDLENG_SHADER_RESOURCE_NONE ),
        pTexture     ( NULL ),
        pBuffer      ( NULL ),
        UsageFlags   ( 0 ),
        MipCount     ( 0 ),
        LayerCount   ( 0 ),
        pBufferOwner ( NULL ),
        pTargetOwner ( NULL ),
        pTextureOwner( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_binding_record
{
    shader_binding_kind Kind;
    xstring             Name;
    u32                 Slot;
    u32                 Count;

    shader_binding_record( void ) :
        Kind ( SHADER_BINDING_SAMPLED_TEXTURE ),
        Name (),
        Slot ( 0 ),
        Count( 0 )
    {
    }
};

//------------------------------------------------------------------------------

// Native stages outlive handles while cached or pending PSOs reference them.
struct shader_native : public xref_counted
{
    SDL_GPUDevice*          pDevice;
    SDL_GPUShader*          pShader;
    SDL_GPUComputePipeline* pComputePipeline;
    u64                     Serial;
    u32                     RegistryBucket;

    shader_native( void ) :
        pDevice         ( NULL ),
        pShader         ( NULL ),
        pComputePipeline( NULL ),
        Serial          ( 0 ),
        RegistryBucket  ( 0 )
    {
    }

    ~shader_native( void );
    void ReleaseReference( void ) const;
};

//------------------------------------------------------------------------------

struct shader_backend
{
    ecs_package_ptr         Package;
    xstring                 PackageFile;
    xref<shader_native>     Native;
    shader*                 pOwner;
    SDL_GPUShader*          pShader;
    SDL_GPUComputePipeline* pComputePipeline;
    u64                     Serial;
    shader_stage            Stage;
    shader_format           Format;
    SDL_GPUShaderFormat     SDLFormat;
    shader_resource_counts  Resources;
    shader_compute_desc     Compute;
    shader_binding_record   Bindings[SDLENG_MAX_SHADER_BINDINGS];
    u32                     BindingCount;
    s32                     BindingLookup[SDLENG_SHADER_BINDING_LOOKUP_SLOTS];
    u32                     BindingLookupCount;
    u32                     BindingLookupMask;
    shader_backend*         pPrev;
    shader_backend*         pNext;

    shader_backend( void ) :
        pOwner            ( NULL ),
        pShader           ( NULL ),
        pComputePipeline  ( NULL ),
        Serial            ( 0 ),
        Stage             ( SHADER_STAGE_VERTEX ),
        Format            ( SHADER_FORMAT_UNKNOWN ),
        SDLFormat         ( SDL_GPU_SHADERFORMAT_INVALID ),
        Resources         (),
        BindingCount      ( 0 ),
        BindingLookupCount( 0 ),
        BindingLookupMask ( 0 ),
        pPrev             ( NULL ),
        pNext             ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rstate_sampler_backend
{
    rstate_sampler_native*  pNative;
    rstate_sampler*         pOwner;
    SDL_GPUSampler*         pSampler;
    rstate_sampler_desc     Desc;
    xstring                 DebugName;
    rstate_sampler_backend* pPrev;
    rstate_sampler_backend* pNext;

    rstate_sampler_backend( void ) :
        pNative ( NULL ),
        pOwner  ( NULL ),
        pSampler( NULL ),
        pPrev   ( NULL ),
        pNext   ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct render_pipeline_backend
{
    render_pipeline*             pOwner;
    render_pipeline_cache_entry* pEntry;
    shader_resource_counts       VertexResources;
    shader_resource_counts       PixelResources;
    char*                        pDebugName;
    render_pipeline_backend*     pPrev;
    render_pipeline_backend*     pNext;

    render_pipeline_backend( void ) :
        pOwner         ( NULL ),
        pEntry         ( NULL ),
        VertexResources(),
        PixelResources (),
        pDebugName     ( NULL ),
        pPrev          ( NULL ),
        pNext          ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rtarget_backend
{
    rtarget*                pOwner;
    SDL_GPUTexture*         pTexture;
    shader_resource         Resource;
    shader_resource_backend ResourceBackend;
    xbool                   bOwnTexture;
    xbool                   bBackBuffer;
    xbool                   bShaderResource;
    rtarget_backend*        pPrev;
    rtarget_backend*        pNext;

    rtarget_backend( void ) :
        pOwner         ( NULL ),
        pTexture       ( NULL ),
        Resource       (),
        ResourceBackend(),
        bOwnTexture    ( FALSE ),
        bBackBuffer    ( FALSE ),
        bShaderResource( FALSE ),
        pPrev          ( NULL ),
        pNext          ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rbuffer_backend
{
    rbuffer*                pOwner;
    SDL_GPUBuffer*          pBuffer;
    shader_resource         Resource;
    shader_resource_backend ResourceBackend;
    rbuffer_backend*        pPrev;
    rbuffer_backend*        pNext;

    rbuffer_backend( void ) :
        pOwner         ( NULL ),
        pBuffer        ( NULL ),
        Resource       (),
        ResourceBackend(),
        pPrev          ( NULL ),
        pNext          ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct vram_texture_backend
{
    vram_texture*           pOwner;
    SDL_GPUTexture*         pTexture;
    shader_resource         Resource;
    shader_resource_backend ResourceBackend;
    vram_texture_backend*   pPrev;
    vram_texture_backend*   pNext;

    vram_texture_backend( void ) :
        pOwner         ( NULL ),
        pTexture       ( NULL ),
        Resource       (),
        ResourceBackend(),
        pPrev          ( NULL ),
        pNext          ( NULL )
    {
    }
};

//==============================================================================
//  GLOBAL BACKEND HANDLES
//==============================================================================

extern SDL_GPUDevice* g_pSDLGPUDevice;
extern SDL_Window*    g_pSDLWindow;

extern sdleng_dynamic_state g_SDLDynamicState;

//==============================================================================
//  SHARED SDL GPU POLICY
//==============================================================================

void                          sdleng_LogError                         ( const char* pSubsystem,
                                                                        const char* pContext );
xbool                         sdleng_InitializeFormatPolicy           ( void );
void                          sdleng_ResetFormatPolicy                ( void );
xbool                         sdleng_ToSDLSampleCount                 ( u32                 SampleCount,
                                                                        SDL_GPUSampleCount& SDLSampleCount );
SDL_GPUTextureFormat          sdleng_ToSDLTextureFormat               ( rtarget_format Format );
SDL_GPUTextureFormat          sdleng_ToSDLTextureFormat               ( vram_texture_format Format );
xbool                         sdleng_IsDepthFormat                    ( rtarget_format Format );
xbool                         sdleng_IsDepthFormat                    ( vram_texture_format Format );
xbool                         sdleng_HasStencil                       ( rtarget_format Format );
xbool                         sdleng_HasAlpha                         ( rtarget_format Format );
//==============================================================================
//  TRANSIENT COMMAND BUFFERS
//==============================================================================

xbool                         sdleng_AcquireTransientCommandBuffer    ( SDL_GPUCommandBuffer*& pCommandBuffer,
                                                                        xbool&                 bOwned,
                                                                        const char*            pSubsystem );
xbool                         sdleng_SubmitTransientCommandBuffer     ( SDL_GPUCommandBuffer* pCommandBuffer,
                                                                        xbool                 bOwned,
                                                                        const char*           pSubsystem );
void                          sdleng_CancelTransientCommandBuffer     ( SDL_GPUCommandBuffer* pCommandBuffer,
                                                                        xbool                 bOwned );
//==============================================================================
//  TRANSFER COMMANDS
//==============================================================================

xbool                         sdleng_BeginTransferCommands            ( const char* pSubsystem );
xbool                         sdleng_BeginTransfer                    ( u32         RequiredSize,
                                                                        u32         Alignment,
                                                                        const char* pSubsystem );
xbool                         sdleng_BeginCopyPass                    ( const char* pSubsystem );
xbool                         sdleng_ReserveTransfer                  ( u32  Size,
                                                                        u32  Alignment,
                                                                        u32& Offset );
SDL_GPUTransferBuffer*        sdleng_GetTransferBuffer                ( void );
xbool                         sdleng_CommitTransfer                   ( void );
SDL_GPUCopyPass*              sdleng_GetTransferCopyPass              ( void );
SDL_GPUCommandBuffer*         sdleng_GetTransferCommandBuffer         ( void );
xbool                         sdleng_MapTransferBuffer                ( void*&      pMapped,
                                                                        const char* pSubsystem );
void                          sdleng_UnmapTransferBuffer              ( void );
void                          sdleng_EndTransferPass                  ( void );
xbool                         sdleng_FlushTransferCommands            ( void );
void                          sdleng_CancelTransfer                   ( void );
void                          sdleng_ReleaseTransferState             ( void );

//==============================================================================
//  TRANSFER STATISTICS
//==============================================================================

sdleng_transfer_stats         sdleng_GetTransferStats                 ( void );
void                          sdleng_TrimTransferState                ( xtick Now );

//==============================================================================
//  BACKEND LIFETIME
//==============================================================================

xbool                         sdleng_CreateDeviceForWindow            ( sdleng_native_window_handle hWindow,
                                                                        s32                         Width,
                                                                        s32                         Height );
void                          sdleng_DestroyDevice                    ( void );
void*                         sdleng_LoadPipelineCache                ( usize& Size );
xbool                         sdleng_WaitForIdle                      ( void );
u32                           sdleng_GetCurrentFrameSlot              ( void );
u64                           sdleng_GetCurrentFrameSerial            ( void );
u64                           sdleng_GetFrameLifetimeSerial           ( void );
u32                           sdleng_GetFramesInFlight                ( void );
xbool                         sdleng_IsFrameComplete                  ( u64 FrameSerial );

//==============================================================================
//  FRAME AND PASS LIFETIME
//==============================================================================

xbool                         sdleng_AcquireCommandBuffer             ( void );
xbool                         sdleng_AcquireSwapchainTexture          ( void );
xbool                         sdleng_IsPresentPolicySupported         ( sdleng_present_policy Policy );
xbool                         sdleng_SetPresentPolicy                 ( sdleng_present_policy Policy );
sdleng_present_policy         sdleng_GetPresentPolicy                 ( void );
xbool                         sdleng_BeginRenderPass                  ( const SDL_GPUColorTargetInfo*        pColorTargets,
                                                                        u32                                  ColorTargetCount,
                                                                        const SDL_GPUDepthStencilTargetInfo* pDepthStencilTarget );
void                          sdleng_EndRenderPass                    ( void );
xbool                         sdleng_BeginComputePass                 ( const SDL_GPUStorageTextureReadWriteBinding* pTextures,
                                                                        u32                                          TextureCount,
                                                                        const SDL_GPUStorageBufferReadWriteBinding*  pBuffers,
                                                                        u32                                          BufferCount );
void                          sdleng_EndComputePass                   ( void );
xbool                         sdleng_InComputePass                    ( void );
SDL_GPUComputePass*           sdleng_GetComputePass                   ( void );
xbool                         sdleng_EndFrame                         ( void );
void                          sdleng_CancelFrame                      ( void );
xbool                         sdleng_WasBackBufferRendered            ( void );

xbool                         sdleng_InFrame                          ( void );
xbool                         sdleng_InRenderPass                     ( void );

//==============================================================================
//  ACCESSORS
//==============================================================================

SDL_GPUDevice*                sdleng_GetDevice                        ( void );
SDL_Window*                   sdleng_GetWindow                        ( void );
SDL_GPUCommandBuffer*         sdleng_GetCommandBuffer                 ( void );
SDL_GPURenderPass*            sdleng_GetRenderPass                    ( void );
SDL_GPUTexture*               sdleng_GetSwapchainTexture              ( void );
SDL_GPUTextureFormat          sdleng_GetSwapchainFormat               ( void );
f32                           sdleng_GetFramePacingWaitMs             ( void );
f32                           sdleng_GetRenderSubmitMs                ( void );
void                          sdleng_GetBackBufferSize                ( s32& Width,
                                                                        s32& Height );

SDL_GPUTexture*               sdleng_GetGPUTexture                    ( const shader_resource* pResource );
SDL_GPUBuffer*                sdleng_GetGPUBuffer                     ( const shader_resource* pResource );
SDL_GPUSampler*               sdleng_GetGPUSampler                    ( const rstate_sampler* pSampler );
SDL_GPUShader*                sdleng_GetGPUShader                     ( const shader* pShader );
xbool                         sdleng_IsCurrentRenderAttachment        ( SDL_GPUTexture* pTexture );
const shader_resource_counts* sdleng_GetShaderResourceCounts          ( const shader* pShader );

u64                           sdleng_GetPipelineCreateCalls           ( void );
void                          sdleng_CollectPipelineWork              ( void );
void                          sdleng_ResetDynamicState                ( void );
void                          sdleng_ResetPipelineBinding             ( void );
void                          sdleng_ResetBufferBindings              ( void );
void                          sdleng_ResetShaderBindings              ( void );
void                          sdleng_ResetShaderUniforms              ( void );
void                          composite_InvalidateShader              ( const shader_backend* pShaderBackend );
void                          sdlstate_InvalidateShaderPipelines      ( u64 ShaderSerial );
u32                           sdleng_GetPipelineCount                 ( void );
u32                           sdleng_GetNativeShaderCount             ( void );
ecs_block_cache_stats         sdleng_GetShaderPackageCacheStats       ( void );
u32                           sdleng_GetPipelineHandleCount           ( void );
u32                           sdleng_GetBufferCount                   ( void );
u32                           sdleng_GetShaderCount                   ( void );
u32                           sdleng_GetRenderTargetCount             ( void );

//==============================================================================
//  GRAPHICS BINDING VALIDATION
//==============================================================================

#if defined(X_RETAIL)
    inline void sdleng_ResetGraphicsBindingDebug( void )
{
}

inline void sdleng_SetGraphicsPipelineDebug( const render_pipeline_backend* )
{
}

inline void sdleng_ClearGraphicsPipelineDebug( const render_pipeline_backend* )
{
}

inline void sdleng_RecordSamplerBindingDebug( shader_stage, u32 )
{
}

inline void sdleng_RecordStorageTextureBindingDebug( shader_stage, u32 )
{
}

inline void sdleng_RecordStorageBufferBindingDebug( shader_stage, u32 )
{
}

inline xbool sdleng_ValidateGraphicsBindings( void )
{
    return TRUE;
}
#else
    void sdleng_ResetGraphicsBindingDebug( void );
void                          sdleng_SetGraphicsPipelineDebug         ( const render_pipeline_backend* pPipeline );
void                          sdleng_ClearGraphicsPipelineDebug       ( const render_pipeline_backend* pPipeline );
void                          sdleng_RecordSamplerBindingDebug        ( shader_stage Stage,
                                                                        u32          Slot );
void                          sdleng_RecordStorageTextureBindingDebug ( shader_stage Stage,
                                                                        u32          Slot );
void                          sdleng_RecordStorageBufferBindingDebug  ( shader_stage Stage,
                                                                        u32          Slot );
xbool                         sdleng_ValidateGraphicsBindings         ( void );
#endif

//==============================================================================
//  BACKEND LIST HELPERS
//==============================================================================

template <typename T>
inline void sdleng_LinkBackend( T*& pHead, T* pBackend, u32& Count )
{
    pBackend->pPrev = NULL;
    pBackend->pNext = pHead;
    if( pHead )
    {
        pHead->pPrev = pBackend;
    }

    pHead = pBackend;
    ++Count;
}

//==============================================================================

template <typename T>
inline void sdleng_UnlinkBackend( T*& pHead, T* pBackend, u32& Count )
{
    if( pBackend->pPrev )
    {
        pBackend->pPrev->pNext = pBackend->pNext;
    }
    else if( pHead == pBackend )
    {
        pHead = pBackend->pNext;
    }

    if( pBackend->pNext )
    {
        pBackend->pNext->pPrev = pBackend->pPrev;
    }

    pBackend->pPrev = NULL;
    pBackend->pNext = NULL;

    if( Count )
    {
        --Count;
    }
}

//==============================================================================

#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
#endif // SDLENG_PRIVATE_HPP
//==============================================================================
