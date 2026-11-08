//==============================================================================
//
//  sdleng_rtarget.cpp
//
//  SDL3 GPU render targets and render pass ownership for explicit PSO rendering.
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"
#include "e_Engine.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    SDLRTARGET_REGISTRY_MAX = 64
};

//==============================================================================
//  TYPES
//==============================================================================

struct sdlrtarget_registry_entry
{
    rtarget*             pTarget;
    rtarget_registration Reg;
    xbool                bInUse;

    sdlrtarget_registry_entry( void ) :
        pTarget( NULL ),
        Reg    (),
        bInUse ( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct sdlrtarget_cache
{
    rtarget        BackBufferTarget;
    const rtarget* pCurrentTargets[RTARGET_MAX_TARGETS];
    const rtarget* pCurrentDepth;
    u32            CurrentCount;
    xbool          bInitialized;
    xbool          bBackBufferValid;
    const char*    pGraphPassOwner;

    sdlrtarget_cache( void ) :
        BackBufferTarget(),
        pCurrentDepth   ( NULL ),
        CurrentCount    ( 0 ),
        bInitialized    ( FALSE ),
        bBackBufferValid( FALSE ),
        pGraphPassOwner ( NULL )
    {
        x_memset( pCurrentTargets, 0, sizeof( pCurrentTargets ) );
    }
};

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static rtarget_backend*          s_pTargetList = NULL;
static u32                       s_TargetCount = 0;
static sdlrtarget_registry_entry s_TargetRegistry[SDLRTARGET_REGISTRY_MAX];
static sdlrtarget_cache          s_TargetCache;

//==============================================================================
//  HELPERS
//==============================================================================

static
SDL_FColor sdlrtarget_ToSDLColor( const f32 Color[4] )
{
    SDL_FColor SDLColor;
    SDLColor.r = Color ? Color[0] : 0.0f;
    SDLColor.g = Color ? Color[1] : 0.0f;
    SDLColor.b = Color ? Color[2] : 0.0f;
    SDLColor.a = Color ? Color[3] : 1.0f;

    return SDLColor;
}

//==============================================================================

static
rtarget_format sdlrtarget_FromSDLFormat( SDL_GPUTextureFormat Format )
{
    switch( Format )
    {
        case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:     return RTARGET_FORMAT_RGBA8;   break;
        case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:     return RTARGET_FORMAT_BGRA8;   break;
        case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:  return RTARGET_FORMAT_RGB10A2; break;
        case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT: return RTARGET_FORMAT_RGBA16F; break;
        case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT: return RTARGET_FORMAT_RGBA32F; break;
        default:                                       return RTARGET_FORMAT_COUNT;   break;
    }
}

//==============================================================================

static
SDL_GPULoadOp sdlrtarget_ToSDLLoadOp( rtarget_load_op Op )
{
    switch( Op )
    {
        case RTARGET_LOAD_LOAD:      return SDL_GPU_LOADOP_LOAD;      break;
        case RTARGET_LOAD_CLEAR:     return SDL_GPU_LOADOP_CLEAR;     break;
        case RTARGET_LOAD_DONT_CARE: return SDL_GPU_LOADOP_DONT_CARE; break;
        default:                     return SDL_GPU_LOADOP_LOAD;      break;
    }
}

//==============================================================================

static
xbool sdlrtarget_ToSDLStoreOp( rtarget_store_op Op, SDL_GPUStoreOp& SDLStoreOp )
{
    switch( Op )
    {
        case RTARGET_STORE_STORE:
        {
            SDLStoreOp = SDL_GPU_STOREOP_STORE;
            return TRUE;
        }

        break;

        case RTARGET_STORE_DONT_CARE:
        {
            SDLStoreOp = SDL_GPU_STOREOP_DONT_CARE;
            return TRUE;
        }

        break;

        case RTARGET_STORE_RESOLVE:
        {
            SDLStoreOp = SDL_GPU_STOREOP_RESOLVE;
            return TRUE;
        }

        break;

        case RTARGET_STORE_RESOLVE_AND_STORE:
        {
            SDLStoreOp = SDL_GPU_STOREOP_RESOLVE_AND_STORE;
            return TRUE;
        }

        break;

        default:
        {
            return FALSE;
        }

        break;
    }
}

//==============================================================================

static
xbool sdlrtarget_ToSDLDepthStoreOp( rtarget_store_op Op, SDL_GPUStoreOp& SDLStoreOp )
{
    switch( Op )
    {
        case RTARGET_STORE_STORE:
        {
            SDLStoreOp = SDL_GPU_STOREOP_STORE;
            return TRUE;
        }

        break;

        case RTARGET_STORE_DONT_CARE:
        {
            SDLStoreOp = SDL_GPU_STOREOP_DONT_CARE;
            return TRUE;
        }

        break;

        default:
        {
            return FALSE;
        }

        break;
    }
}

//==============================================================================

static
void sdlrtarget_ReleaseBackend( rtarget_backend* pBackend )
{
    if( !pBackend )
    {
        return;
    }

    if( pBackend->bOwnTexture && pBackend->pTexture && g_pSDLGPUDevice )
    {
        SDL_ReleaseGPUTexture( g_pSDLGPUDevice, pBackend->pTexture );
    }

    pBackend->pTexture                 = NULL;
    pBackend->Resource.pBackend        = NULL;
    pBackend->ResourceBackend.Kind     = SDLENG_SHADER_RESOURCE_NONE;
    pBackend->ResourceBackend.pTexture = NULL;
    pBackend->ResourceBackend.pBuffer  = NULL;
    pBackend->bShaderResource          = FALSE;
}

//==============================================================================

static
void sdlrtarget_ClearCurrentPass( void )
{
    x_memset( s_TargetCache.pCurrentTargets, 0, sizeof( s_TargetCache.pCurrentTargets ) );
    s_TargetCache.pCurrentDepth = NULL;
    s_TargetCache.CurrentCount  = 0;
}

//==============================================================================

static
xbool sdlrtarget_IsCurrentTarget( const rtarget& Target )
{
    for( u32 i = 0; i < s_TargetCache.CurrentCount; i++ )
    {
        if( s_TargetCache.pCurrentTargets[i] == &Target )
        {
            return TRUE;
        }
    }

    return s_TargetCache.pCurrentDepth == &Target;
}

//==============================================================================

static
void sdlrtarget_ClearRegistry( void )
{
    for( u32 i = 0; i < SDLRTARGET_REGISTRY_MAX; i++ )
    {
        s_TargetRegistry[i] = sdlrtarget_registry_entry();
    }
}

//==============================================================================

static
s32 sdlrtarget_FindRegistryEntry( const rtarget* pTarget )
{
    if( !pTarget )
    {
        return -1;
    }

    for( s32 i = 0; i < SDLRTARGET_REGISTRY_MAX; i++ )
    {
        if( s_TargetRegistry[i].bInUse && ( s_TargetRegistry[i].pTarget == pTarget ) )
        {
            return i;
        }
    }

    return -1;
}

//==============================================================================

static
xbool sdlrtarget_GetBackBufferSize( u32& Width, u32& Height )
{
    s32 XRes = 0;
    s32 YRes = 0;
    sdleng_GetBackBufferSize( XRes, YRes );

    if( ( XRes <= 0 ) || ( YRes <= 0 ) )
    {
        return FALSE;
    }

    Width  = (u32)XRes;
    Height = (u32)YRes;

    return TRUE;
}

//==============================================================================

static
xbool sdlrtarget_ResolveRegisteredSize( const rtarget_registration& Reg, u32& Width, u32& Height )
{
    Width  = 0;
    Height = 0;

    switch( Reg.Policy )
    {
        case RTARGET_SIZE_ABSOLUTE:
        {
            Width  = Reg.BaseWidth;
            Height = Reg.BaseHeight;
        }

        break;

        case RTARGET_SIZE_BACKBUFFER:
        {
            if( !sdlrtarget_GetBackBufferSize( Width, Height ) )
            {
                return FALSE;
            }
        }

        break;

        case RTARGET_SIZE_RELATIVE_TO_BACKBUFFER:
        {
            if( !sdlrtarget_GetBackBufferSize( Width, Height ) )
            {
                return FALSE;
            }

            if( !x_isvalid( Reg.ScaleX ) || !x_isvalid( Reg.ScaleY ) ||
                ( Reg.ScaleX <= 0.0f ) || ( Reg.ScaleY <= 0.0f ) )
            {
                return FALSE;
            }

            {
                const f64 ScaledWidth  = (f64)Width * (f64)Reg.ScaleX;
                const f64 ScaledHeight = (f64)Height * (f64)Reg.ScaleY;

                if( ( ScaledWidth > 4294967295.0 ) || ( ScaledHeight > 4294967295.0 ) )
                {
                    return FALSE;
                }

                Width  = (u32)ScaledWidth;
                Height = (u32)ScaledHeight;
            }
        }

        break;

        case RTARGET_SIZE_RELATIVE_TO_VIEW:
        {
            x_DebugMsg( "SDLRTarget: RTARGET_SIZE_RELATIVE_TO_VIEW needs a view-size owner in the SDL backend\n" );
            return FALSE;
        }

        break;
    }

    if( Width == 0 )
    {
        Width = 1;
    }

    if( Height == 0 )
    {
        Height = 1;
    }

    return TRUE;
}

//==============================================================================

static
s32 sdlrtarget_RegisterInternal( rtarget& Target, const rtarget_registration& Reg )
{
    s32 Index = sdlrtarget_FindRegistryEntry( &Target );

    if( Index >= 0 )
    {
        s_TargetRegistry[Index].Reg = Reg;
        return Index;
    }

    for( s32 i = 0; i < SDLRTARGET_REGISTRY_MAX; i++ )
    {
        if( !s_TargetRegistry[i].bInUse )
        {
            s_TargetRegistry[i].pTarget = &Target;
            s_TargetRegistry[i].Reg     = Reg;
            s_TargetRegistry[i].bInUse  = TRUE;
            return i;
        }
    }

    x_DebugMsg( "SDLRTarget: registry full\n" );

    return -1;
}

//==============================================================================

static
xbool sdlrtarget_RecreateFromRegistration( sdlrtarget_registry_entry& Entry )
{
    if( !Entry.bInUse || !Entry.pTarget )
    {
        return FALSE;
    }

    u32 Width  = 0;
    u32 Height = 0;

    if( !sdlrtarget_ResolveRegisteredSize( Entry.Reg, Width, Height ) )
    {
        return FALSE;
    }

    if( Entry.pTarget->pBackend &&
        ( Entry.pTarget->Desc.Width == Width ) &&
        ( Entry.pTarget->Desc.Height == Height ) &&
        ( Entry.pTarget->Desc.Format == Entry.Reg.Format ) &&
        ( Entry.pTarget->Desc.SampleCount == Entry.Reg.SampleCount ) &&
        ( Entry.pTarget->Desc.SampleQuality == Entry.Reg.SampleQuality ) &&
        ( Entry.pTarget->Desc.bBindAsTexture == Entry.Reg.bBindAsTexture ) )
    {
        return TRUE;
    }

    rtarget_desc Desc;
    Desc.Width          = Width;
    Desc.Height         = Height;
    Desc.Format         = Entry.Reg.Format;
    Desc.SampleCount    = Entry.Reg.SampleCount;
    Desc.SampleQuality  = Entry.Reg.SampleQuality;
    Desc.bBindAsTexture = Entry.Reg.bBindAsTexture;
    Desc.pDebugName     = Entry.Reg.pDebugName;

    return rtarget_Create( *Entry.pTarget, Desc );
}

//==============================================================================

static
xbool sdlrtarget_EnsureBackBufferBackend( void )
{
    if( s_TargetCache.BackBufferTarget.pBackend )
    {
        return TRUE;
    }

    rtarget_backend* pBackend = new rtarget_backend;

    if( !pBackend )
    {
        return FALSE;
    }

    pBackend->pOwner      = &s_TargetCache.BackBufferTarget;
    pBackend->bOwnTexture = FALSE;
    pBackend->bBackBuffer = TRUE;

    s_TargetCache.BackBufferTarget.pBackend       = pBackend;
    s_TargetCache.BackBufferTarget.bIsDepthTarget = FALSE;

    return TRUE;
}

//==============================================================================

static
xbool sdlrtarget_RefreshBackBufferTarget( void )
{
    if( !sdlrtarget_EnsureBackBufferBackend() )
    {
        return FALSE;
    }

    SDL_GPUTextureFormat SDLFormat = sdleng_GetSwapchainFormat();
    rtarget_format       Format    = sdlrtarget_FromSDLFormat( SDLFormat );

    if( Format == RTARGET_FORMAT_COUNT )
    {
        return FALSE;
    }

    u32 Width  = 0;
    u32 Height = 0;

    if( !sdlrtarget_GetBackBufferSize( Width, Height ) )
    {
        return FALSE;
    }

    s_TargetCache.BackBufferTarget.Desc.Width          = Width;
    s_TargetCache.BackBufferTarget.Desc.Height         = Height;
    s_TargetCache.BackBufferTarget.Desc.Format         = Format;
    s_TargetCache.BackBufferTarget.Desc.SampleCount    = 1;
    s_TargetCache.BackBufferTarget.Desc.SampleQuality  = 0;
    s_TargetCache.BackBufferTarget.Desc.bBindAsTexture = FALSE;
    s_TargetCache.BackBufferTarget.Desc.pDebugName     = "BackBuffer";
    s_TargetCache.BackBufferTarget.pBackend->pTexture  = sdleng_GetSwapchainTexture();
    s_TargetCache.bBackBufferValid                     = TRUE;

    return TRUE;
}

//==============================================================================

static
xbool sdlrtarget_ValidateResolveTarget( const rtarget& Target, const rtarget& ResolveTarget )
{
    if( Target.bIsDepthTarget || ResolveTarget.bIsDepthTarget )
    {
        return FALSE;
    }

    if( Target.Desc.Format != ResolveTarget.Desc.Format )
    {
        return FALSE;
    }

    if( ( Target.Desc.Width != ResolveTarget.Desc.Width ) ||
        ( Target.Desc.Height != ResolveTarget.Desc.Height ) ||
        ( Target.pBackend->pTexture == ResolveTarget.pBackend->pTexture ) )
    {
        return FALSE;
    }

    if( Target.Desc.SampleCount <= 1 )
    {
        return FALSE;
    }

    return ResolveTarget.Desc.SampleCount == 1;
}

//==============================================================================

static
xbool sdlrtarget_FillColorTargetInfo( const rtarget_color_attachment_desc& Src,
                                      SDL_GPUColorTargetInfo&              Dst )
{
    x_memset( &Dst, 0, sizeof( Dst ) );

    if( !Src.pTarget || !Src.pTarget->pBackend || !Src.pTarget->pBackend->pTexture )
    {
        return FALSE;
    }

    if( Src.pTarget->bIsDepthTarget )
    {
        return FALSE;
    }

    if( ( Src.LoadOp < RTARGET_LOAD_LOAD ) || ( Src.LoadOp > RTARGET_LOAD_DONT_CARE ) )
    {
        return FALSE;
    }

    if( Src.LoadOp == RTARGET_LOAD_CLEAR )
    {
        for( u32 i = 0; i < 4; ++i )
        {
            if( !x_isvalid( Src.ClearColor[i] ) )
            {
                return FALSE;
            }
        }
    }

    if( ( Src.MipLevel != 0 ) || ( Src.Layer != 0 ) )
    {
        return FALSE;
    }

    SDL_GPUStoreOp StoreOp;
    if( !sdlrtarget_ToSDLStoreOp( Src.StoreOp, StoreOp ) )
    {
        return FALSE;
    }

    if( Src.bCycle && ( Src.LoadOp == RTARGET_LOAD_LOAD ) )
    {
        return FALSE;
    }

    Dst.texture              = Src.pTarget->pBackend->pTexture;
    Dst.mip_level            = Src.MipLevel;
    Dst.layer_or_depth_plane = Src.Layer;
    Dst.clear_color          = sdlrtarget_ToSDLColor( Src.ClearColor );
    Dst.load_op              = sdlrtarget_ToSDLLoadOp( Src.LoadOp );
    Dst.store_op             = StoreOp;
    Dst.cycle                = Src.bCycle ? true : false;

    if( ( Src.StoreOp == RTARGET_STORE_RESOLVE ) ||
        ( Src.StoreOp == RTARGET_STORE_RESOLVE_AND_STORE ) )
    {
        if( !Src.pResolveTarget ||
            !Src.pResolveTarget->pBackend ||
            !Src.pResolveTarget->pBackend->pTexture ||
            !sdlrtarget_ValidateResolveTarget( *Src.pTarget, *Src.pResolveTarget ) ||
            ( Src.ResolveMipLevel != 0 ) ||
            ( Src.ResolveLayer != 0 ) )
        {
            return FALSE;
        }

        Dst.resolve_texture       = Src.pResolveTarget->pBackend->pTexture;
        Dst.resolve_mip_level     = Src.ResolveMipLevel;
        Dst.resolve_layer         = Src.ResolveLayer;
        Dst.cycle_resolve_texture = Src.bCycleResolve ? true : false;
    }
    else if( Src.pResolveTarget )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlrtarget_FillDepthTargetInfo( const rtarget_depth_attachment_desc& Src,
                                      SDL_GPUDepthStencilTargetInfo&       Dst )
{
    x_memset( &Dst, 0, sizeof( Dst ) );

    if( !Src.pTarget || !Src.pTarget->pBackend || !Src.pTarget->pBackend->pTexture )
    {
        return FALSE;
    }

    if( !Src.pTarget->bIsDepthTarget )
    {
        return FALSE;
    }

    if( ( Src.DepthLoadOp < RTARGET_LOAD_LOAD ) || ( Src.DepthLoadOp > RTARGET_LOAD_DONT_CARE ) ||
        ( Src.StencilLoadOp < RTARGET_LOAD_LOAD ) || ( Src.StencilLoadOp > RTARGET_LOAD_DONT_CARE ) ||
        ( ( Src.DepthLoadOp == RTARGET_LOAD_CLEAR ) &&
          ( !x_isvalid( Src.ClearDepth ) || ( Src.ClearDepth < 0.0f ) || ( Src.ClearDepth > 1.0f ) ) ) )
    {
        return FALSE;
    }

    // rtarget textures are currently always single-level, single-layer 2D textures.
    if( ( Src.MipLevel != 0 ) || ( Src.Layer != 0 ) )
    {
        return FALSE;
    }

    SDL_GPUStoreOp DepthStoreOp;
    SDL_GPUStoreOp StencilStoreOp;

    if( !sdlrtarget_ToSDLDepthStoreOp( Src.DepthStoreOp, DepthStoreOp ) ||
        !sdlrtarget_ToSDLDepthStoreOp( Src.StencilStoreOp, StencilStoreOp ) )
    {
        return FALSE;
    }

    if( Src.bCycle &&
        ( ( Src.DepthLoadOp == RTARGET_LOAD_LOAD ) ||
          ( sdleng_HasStencil( Src.pTarget->Desc.Format ) &&
            ( Src.StencilLoadOp == RTARGET_LOAD_LOAD ) ) ) )
    {
        return FALSE;
    }

    Dst.texture          = Src.pTarget->pBackend->pTexture;
    Dst.clear_depth      = Src.ClearDepth;
    Dst.load_op          = sdlrtarget_ToSDLLoadOp( Src.DepthLoadOp );
    Dst.store_op         = DepthStoreOp;
    Dst.stencil_load_op  = sdleng_HasStencil( Src.pTarget->Desc.Format ) ? sdlrtarget_ToSDLLoadOp( Src.StencilLoadOp ) : SDL_GPU_LOADOP_DONT_CARE;
    Dst.stencil_store_op = sdleng_HasStencil( Src.pTarget->Desc.Format ) ? StencilStoreOp : SDL_GPU_STOREOP_DONT_CARE;
    Dst.cycle            = Src.bCycle ? true : false;
    Dst.clear_stencil    = Src.ClearStencil;
    Dst.mip_level        = (Uint8)Src.MipLevel;
    Dst.layer            = (Uint8)Src.Layer;

    return TRUE;
}

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void rtarget_Init( void )
{
    s_TargetCache = sdlrtarget_cache();
    sdlrtarget_ClearRegistry();
    s_pTargetList              = NULL;
    s_TargetCount              = 0;
    s_TargetCache.bInitialized = TRUE;
}

//==============================================================================

void rtarget_Kill( void )
{
    rtarget_EndPass();
    rtarget_ReleaseBackBufferTargets();

    while( s_pTargetList )
    {
        rtarget_backend* pBackend = s_pTargetList;
        sdleng_UnlinkBackend( s_pTargetList, pBackend, s_TargetCount );
        sdlrtarget_ReleaseBackend( pBackend );

        if( pBackend->pOwner )
        {
            pBackend->pOwner->Desc           = rtarget_desc();
            pBackend->pOwner->bIsDepthTarget = FALSE;
            pBackend->pOwner->pBackend       = NULL;
            pBackend->pOwner                 = NULL;
        }

        delete pBackend;
    }

    sdlrtarget_ClearRegistry();
    s_TargetCache = sdlrtarget_cache();
}

//==============================================================================
//  RENDER TARGET CREATION
//==============================================================================

xbool rtarget_Create( rtarget& Target, const rtarget_desc& Desc )
{
    if( !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    rtarget_backend* pOldBackend = Target.pBackend;
    if( pOldBackend && ( pOldBackend->bBackBuffer || sdlrtarget_IsCurrentTarget( Target ) ) )
    {
        x_DebugMsg( "SDLRTarget: cannot recreate a current render target\n" );
        ASSERT( FALSE );
        return FALSE;
    }

    if( ( Desc.Width == 0 ) || ( Desc.Height == 0 ) ||
        ( Desc.Width > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
        ( Desc.Height > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
        ( Desc.SampleCount == 0 ) || ( Desc.MipCount == 0 ) ||
        ( Desc.MipCount > vram_CalcMipCount( Desc.Width, Desc.Height ) ) ||
        ( ( Desc.SampleCount > 1 ) && ( Desc.MipCount != 1 ) ) )
    {
        return FALSE;
    }

    if( Desc.SampleQuality != 0 )
    {
        x_DebugMsg( "SDLRTarget: SampleQuality is not part of the SDL GPU PSO model\n" );
        return FALSE;
    }

    if( ( Desc.SampleCount > 1 ) && Desc.bBindAsTexture )
    {
        x_DebugMsg( "SDLRTarget: multisample targets cannot be sampled; use a color resolve target\n" );
        return FALSE;
    }

    if( Desc.bBindAsTexture &&
        ( ( Desc.Format == RTARGET_FORMAT_R8_UINT ) || ( Desc.Format == RTARGET_FORMAT_R32_UINT ) ) )
    {
        return FALSE;
    }

    SDL_GPUTextureFormat Format = sdleng_ToSDLTextureFormat( Desc.Format );
    if( Format == SDL_GPU_TEXTUREFORMAT_INVALID )
    {
        return FALSE;
    }

    SDL_GPUSampleCount SampleCount;
    if( !sdleng_ToSDLSampleCount( Desc.SampleCount, SampleCount ) )
    {
        return FALSE;
    }

    if( !SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, Format, SampleCount ) )
    {
        return FALSE;
    }

    const xbool bDepth = rtarget_IsDepthFormat( Desc.Format );

    const u32 StorageFlags = VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ |
                             VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE |
                             VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE;

    if( ( Desc.StorageUsage & ~StorageFlags ) ||
        ( bDepth && ( Desc.StorageUsage & ( VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE |
                                            VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE ) ) ) ||
        ( ( Desc.SampleCount > 1 ) &&
          ( Desc.StorageUsage & ( VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE |
                                  VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE ) ) ) ||
        ( ( Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE ) &&
          ( Desc.StorageUsage & ( VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ |
                                  VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE ) ) ) ||
        ( Desc.bBindAsTexture && ( Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ ) ) )
    {
        return FALSE;
    }

    SDL_GPUTextureUsageFlags Usage = Desc.bRenderTarget
                                         ? ( bDepth ? SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET : SDL_GPU_TEXTUREUSAGE_COLOR_TARGET )
                                         : 0;

    if( Desc.bBindAsTexture )
    {
        Usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    }

    if( Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ )
    {
        Usage |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    }

    if( Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE )
    {
        Usage |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
    }

    if( Desc.StorageUsage & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE )
    {
        Usage |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;
    }

    if( !SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice, Format, SDL_GPU_TEXTURETYPE_2D, Usage ) )
    {
        return FALSE;
    }

    SDL_PropertiesID Props = SDL_CreateProperties();
    if( !Props )
    {
        sdleng_LogError( "SDLRTarget", "SDL_CreateProperties" );
        return FALSE;
    }

    xbool bPropertiesValid = FALSE;

    if( bDepth )
    {
        bPropertiesValid = SDL_SetFloatProperty( Props,
                                                 SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_DEPTH_FLOAT,
                                                 Desc.ClearDepth ) &&
                           SDL_SetNumberProperty( Props,
                                                  SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_STENCIL_NUMBER,
                                                  Desc.ClearStencil );
    }
    else
    {
        bPropertiesValid = SDL_SetFloatProperty( Props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_R_FLOAT, Desc.ClearColor[0] ) &&
                           SDL_SetFloatProperty( Props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_G_FLOAT, Desc.ClearColor[1] ) &&
                           SDL_SetFloatProperty( Props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_B_FLOAT, Desc.ClearColor[2] ) &&
                           SDL_SetFloatProperty( Props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT, Desc.ClearColor[3] );
    }

    if( !bPropertiesValid )
    {
        sdleng_LogError( "SDLRTarget", "SDL_SetProperty" );
        SDL_DestroyProperties( Props );
        return FALSE;
    }

    if( Desc.pDebugName && Desc.pDebugName[0] )
    {
        if( !SDL_SetStringProperty( Props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, Desc.pDebugName ) )
        {
            sdleng_LogError( "SDLRTarget", "SDL_SetStringProperty" );
            SDL_DestroyProperties( Props );
            return FALSE;
        }
    }

    SDL_GPUTextureCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof( CreateInfo ) );
    CreateInfo.type                 = SDL_GPU_TEXTURETYPE_2D;
    CreateInfo.format               = Format;
    CreateInfo.usage                = Usage;
    CreateInfo.width                = Desc.Width;
    CreateInfo.height               = Desc.Height;
    CreateInfo.layer_count_or_depth = 1;
    CreateInfo.num_levels           = Desc.MipCount;
    CreateInfo.sample_count         = SampleCount;
    CreateInfo.props                = Props;

    SDL_GPUTexture* pTexture = SDL_CreateGPUTexture( g_pSDLGPUDevice, &CreateInfo );

    SDL_DestroyProperties( Props );

    if( !pTexture )
    {
        sdleng_LogError( "SDLRTarget", "SDL_CreateGPUTexture" );
        return FALSE;
    }

    rtarget_backend* pBackend = new rtarget_backend;

    if( !pBackend )
    {
        SDL_ReleaseGPUTexture( g_pSDLGPUDevice, pTexture );
        return FALSE;
    }

    pBackend->pOwner      = &Target;
    pBackend->pTexture    = pTexture;
    pBackend->bOwnTexture = TRUE;
    pBackend->bBackBuffer = FALSE;

    if( Desc.bBindAsTexture || Desc.StorageUsage )
    {
        pBackend->Resource.pBackend            = &pBackend->ResourceBackend;
        pBackend->ResourceBackend.Kind         = SDLENG_SHADER_RESOURCE_TEXTURE;
        pBackend->ResourceBackend.pTexture     = pTexture;
        pBackend->ResourceBackend.UsageFlags   = Usage;
        pBackend->ResourceBackend.MipCount     = Desc.MipCount;
        pBackend->ResourceBackend.LayerCount   = 1;
        pBackend->ResourceBackend.pTargetOwner = &Target;
        pBackend->ResourceBackend.pBuffer      = NULL;
        pBackend->bShaderResource              = TRUE;
    }

    if( pOldBackend )
    {
        sdleng_ResetShaderBindings();
        sdleng_UnlinkBackend( s_pTargetList, pOldBackend, s_TargetCount );
        sdlrtarget_ReleaseBackend( pOldBackend );
        delete pOldBackend;
    }

    Target.Desc           = Desc;
    Target.bIsDepthTarget = bDepth;
    Target.pBackend       = pBackend;

    sdleng_LinkBackend( s_pTargetList, pBackend, s_TargetCount );

    return TRUE;
}

//==============================================================================

void rtarget_Destroy( rtarget& Target )
{
    rtarget_backend* pBackend = Target.pBackend;

    if( !pBackend )
    {
        return;
    }

    if( pBackend->bBackBuffer || sdlrtarget_IsCurrentTarget( Target ) )
    {
        ASSERT( FALSE );
        return;
    }

    sdleng_ResetShaderBindings();
    sdleng_UnlinkBackend( s_pTargetList, pBackend, s_TargetCount );
    sdlrtarget_ReleaseBackend( pBackend );
    delete pBackend;

    Target.Desc           = rtarget_desc();
    Target.bIsDepthTarget = FALSE;
    Target.pBackend       = NULL;
}

//==============================================================================
//  TARGET REGISTRATION
//==============================================================================

xbool rtarget_Register( rtarget& Target, const rtarget_registration& Reg )
{
    return sdlrtarget_RegisterInternal( Target, Reg ) >= 0;
}

//==============================================================================

xbool rtarget_GetOrCreate( rtarget& Target, const rtarget_registration& Reg )
{
    const s32 Index = sdlrtarget_RegisterInternal( Target, Reg );

    if( Index < 0 )
    {
        return FALSE;
    }

    return sdlrtarget_RecreateFromRegistration( s_TargetRegistry[Index] );
}

//==============================================================================

void rtarget_Unregister( rtarget& Target )
{
    const s32 Index = sdlrtarget_FindRegistryEntry( &Target );

    if( Index < 0 )
    {
        return;
    }

    s_TargetRegistry[Index] = sdlrtarget_registry_entry();
}

//==============================================================================

xbool rtarget_NotifyResolutionChanged( void )
{
    if( sdleng_InFrame() || sdleng_InRenderPass() )
    {
        x_DebugMsg( "SDLRTarget: cannot recreate targets during an active frame\n" );
        ASSERT( FALSE );
        return FALSE;
    }

    if( !sdleng_WaitForIdle() )
    {
        return FALSE;
    }

    rtarget_ReleaseBackBufferTargets();

    xbool Success = TRUE;

    for( u32 i = 0; i < SDLRTARGET_REGISTRY_MAX; i++ )
    {
        if( s_TargetRegistry[i].bInUse && !sdlrtarget_RecreateFromRegistration( s_TargetRegistry[i] ) )
        {
            Success = FALSE;
        }
    }

    return Success;
}

//==============================================================================

void rtarget_ReleaseBackBufferTargets( void )
{
    rtarget_EndPass();

    if( s_TargetCache.BackBufferTarget.pBackend )
    {
        rtarget_backend* pBackend = s_TargetCache.BackBufferTarget.pBackend;
        sdleng_ResetShaderBindings();
        sdlrtarget_ReleaseBackend( pBackend );
        delete pBackend;
        s_TargetCache.BackBufferTarget = rtarget();
    }

    s_TargetCache.bBackBufferValid = FALSE;
}

//==============================================================================
//  RENDER PASS MANAGEMENT
//==============================================================================
//  RENDER PASSES
//==============================================================================

xbool rtarget_BeginPass( const rtarget_pass_desc& Desc )
{
    if( s_TargetCache.pGraphPassOwner )
    {
        x_DebugMsg( "SDLRTarget: graph pass '%s' owns the active pass lifecycle\n",
                    s_TargetCache.pGraphPassOwner );

        ASSERTS( FALSE, s_TargetCache.pGraphPassOwner );
        return FALSE;
    }

    if( !s_TargetCache.bInitialized )
    {
        return FALSE;
    }

    if( sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        x_DebugMsg( "SDLRTarget: GPU pass already active\n" );
        return FALSE;
    }

    if( ( Desc.ColorCount == 0 ) && !Desc.pDepthStencil )
    {
        return FALSE;
    }

    if( Desc.ColorCount > RTARGET_MAX_TARGETS )
    {
        return FALSE;
    }

    if( Desc.ColorCount && !Desc.pColors )
    {
        return FALSE;
    }

    if( !sdleng_InFrame() )
    {
        x_DebugMsg( "SDLRTarget: render pass requires an active frame lifecycle\n" );
        return FALSE;
    }

    SDL_GPUColorTargetInfo ColorTargets[RTARGET_MAX_TARGETS];
    x_memset( ColorTargets, 0, sizeof( ColorTargets ) );

    for( u32 i = 0; i < Desc.ColorCount; i++ )
    {
        if( !sdlrtarget_FillColorTargetInfo( Desc.pColors[i], ColorTargets[i] ) )
        {
            return FALSE;
        }
    }

    SDL_GPUDepthStencilTargetInfo  DepthTarget;
    SDL_GPUDepthStencilTargetInfo* pDepthTarget = NULL;

    if( Desc.pDepthStencil )
    {
        if( !sdlrtarget_FillDepthTargetInfo( *Desc.pDepthStencil, DepthTarget ) )
        {
            return FALSE;
        }

        pDepthTarget = &DepthTarget;
    }

    const rtarget* pReference = Desc.ColorCount ? Desc.pColors[0].pTarget : Desc.pDepthStencil->pTarget;

    for( u32 i = 0; i < Desc.ColorCount; ++i )
    {
        const rtarget& Target = *Desc.pColors[i].pTarget;

        if( ( Target.Desc.Width != pReference->Desc.Width ) ||
            ( Target.Desc.Height != pReference->Desc.Height ) ||
            ( Target.Desc.SampleCount != pReference->Desc.SampleCount ) )
        {
            return FALSE;
        }

        for( u32 j = 0; j < Desc.ColorCount; ++j )
        {
            if( ( i != j ) && ( ColorTargets[i].texture == ColorTargets[j].texture ) )
            {
                return FALSE;
            }

            if( ColorTargets[i].resolve_texture &&
                ( ( ColorTargets[i].resolve_texture == ColorTargets[j].texture ) ||
                  ( ( i != j ) && ( ColorTargets[i].resolve_texture == ColorTargets[j].resolve_texture ) ) ) )
            {
                return FALSE;
            }
        }
    }

    if( Desc.pDepthStencil )
    {
        const rtarget& Depth = *Desc.pDepthStencil->pTarget;

        if( ( Depth.Desc.Width != pReference->Desc.Width ) ||
            ( Depth.Desc.Height != pReference->Desc.Height ) ||
            ( Depth.Desc.SampleCount != pReference->Desc.SampleCount ) )
        {
            return FALSE;
        }
    }

    if( !sdleng_BeginRenderPass( ColorTargets, Desc.ColorCount, pDepthTarget ) )
    {
        return FALSE;
    }

    sdlrtarget_ClearCurrentPass();

    for( u32 i = 0; i < Desc.ColorCount; i++ )
    {
        s_TargetCache.pCurrentTargets[i] = Desc.pColors[i].pTarget;
    }

    s_TargetCache.CurrentCount  = Desc.ColorCount;
    s_TargetCache.pCurrentDepth = Desc.pDepthStencil ? Desc.pDepthStencil->pTarget : NULL;

    return TRUE;
}

//==============================================================================

xbool rtarget_BeginPass( const rtarget_color_attachment_desc* pColors,
                         u32                                  ColorCount,
                         const rtarget_depth_attachment_desc* pDepthStencil )
{
    rtarget_pass_desc Desc;
    Desc.pColors       = pColors;
    Desc.ColorCount    = ColorCount;
    Desc.pDepthStencil = pDepthStencil;

    return rtarget_BeginPass( Desc );
}

//==============================================================================

void rtarget_EndPass( void )
{
    if( s_TargetCache.pGraphPassOwner )
    {
        x_DebugMsg( "SDLRTarget: graph pass '%s' rejected an external pass end\n",
                    s_TargetCache.pGraphPassOwner );

        ASSERTS( FALSE, s_TargetCache.pGraphPassOwner );
        return;
    }

    if( !sdleng_InRenderPass() )
    {
        return;
    }

    sdleng_EndRenderPass();
    sdlrtarget_ClearCurrentPass();
}

//==============================================================================

void rtarget_SetGraphPassOwner( const char* pPassName )
{
    s_TargetCache.pGraphPassOwner = pPassName;
}

//==============================================================================

rtarget_format rtarget_GetBackBufferFormat( void )
{
    if( !s_TargetCache.bInitialized )
    {
        return RTARGET_FORMAT_COUNT;
    }

    // Pipeline preparation needs swapchain metadata between frames, without acquisition.
    return sdlrtarget_FromSDLFormat( sdleng_GetSwapchainFormat() );
}

//==============================================================================

const rtarget* rtarget_GetBackBuffer( void )
{
    if( !s_TargetCache.bInitialized )
    {
        return NULL;
    }

    // Acquire the swapchain on demand so offscreen targets can render first.
    if( !eng_EnsurePresentTarget() )
    {
        return NULL;
    }

    if( !sdlrtarget_RefreshBackBufferTarget() )
    {
        return NULL;
    }

    if( !rtarget_HasTexture( s_TargetCache.BackBufferTarget ) )
    {
        return NULL;
    }

    return &s_TargetCache.BackBufferTarget;
}

//==============================================================================

const rtarget* rtarget_GetCurrentTarget( u32 Index )
{
    if( Index >= s_TargetCache.CurrentCount )
    {
        return NULL;
    }

    return s_TargetCache.pCurrentTargets[Index];
}

//==============================================================================

u32 rtarget_GetCurrentCount( void )
{
    return s_TargetCache.CurrentCount;
}

//==============================================================================

const rtarget* rtarget_GetCurrentDepth( void )
{
    return s_TargetCache.pCurrentDepth;
}

//==============================================================================
//  COPY AND RESOURCE ACCESS
//==============================================================================

xbool rtarget_Copy( const rtarget_copy_desc& Desc )
{
    if( sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        return FALSE;
    }

    if( !Desc.pDestination || !Desc.pSource )
    {
        return FALSE;
    }

    if( !rtarget_IsValid( *Desc.pDestination ) || !rtarget_IsValid( *Desc.pSource ) )
    {
        return FALSE;
    }

    if( !Desc.pDestination->pBackend->pTexture || !Desc.pSource->pBackend->pTexture )
    {
        return FALSE;
    }

    if( Desc.pDestination->pBackend->pTexture == Desc.pSource->pBackend->pTexture )
    {
        return FALSE;
    }

    if( Desc.pDestination->Desc.Format != Desc.pSource->Desc.Format )
    {
        return FALSE;
    }

    if( Desc.pDestination->Desc.SampleCount != Desc.pSource->Desc.SampleCount )
    {
        return FALSE;
    }

    if( ( Desc.SrcMipLevel != 0 ) || ( Desc.DstMipLevel != 0 ) ||
        ( Desc.SrcLayer != 0 ) || ( Desc.DstLayer != 0 ) )
    {
        return FALSE;
    }

    if( ( Desc.SrcX >= Desc.pSource->Desc.Width ) ||
        ( Desc.SrcY >= Desc.pSource->Desc.Height ) ||
        ( Desc.DstX >= Desc.pDestination->Desc.Width ) ||
        ( Desc.DstY >= Desc.pDestination->Desc.Height ) )
    {
        return FALSE;
    }

    const u32 Width  = Desc.Width ? Desc.Width : ( Desc.pSource->Desc.Width - Desc.SrcX );
    const u32 Height = Desc.Height ? Desc.Height : ( Desc.pSource->Desc.Height - Desc.SrcY );
    const u32 Depth  = Desc.Depth ? Desc.Depth : 1;

    if( ( Width == 0 ) || ( Height == 0 ) || ( Depth == 0 ) )
    {
        return FALSE;
    }

    if( ( Desc.SrcZ != 0 ) || ( Desc.DstZ != 0 ) ||
        ( Desc.Depth > 1 ) ||
        ( Width > ( Desc.pSource->Desc.Width - Desc.SrcX ) ) ||
        ( Height > ( Desc.pSource->Desc.Height - Desc.SrcY ) ) ||
        ( Width > ( Desc.pDestination->Desc.Width - Desc.DstX ) ) ||
        ( Height > ( Desc.pDestination->Desc.Height - Desc.DstY ) ) )
    {
        return FALSE;
    }

    if( !sdleng_BeginCopyPass( "SDLRTarget" ) )
    {
        return FALSE;
    }

    SDL_GPUCopyPass* pCopyPass = sdleng_GetTransferCopyPass();
    if( !pCopyPass )
    {
        sdleng_CancelTransfer();
        return FALSE;
    }

    SDL_GPUTextureLocation Source;
    x_memset( &Source, 0, sizeof( Source ) );
    Source.texture   = Desc.pSource->pBackend->pTexture;
    Source.mip_level = Desc.SrcMipLevel;
    Source.layer     = Desc.SrcLayer;
    Source.x         = Desc.SrcX;
    Source.y         = Desc.SrcY;
    Source.z         = Desc.SrcZ;

    SDL_GPUTextureLocation Destination;
    x_memset( &Destination, 0, sizeof( Destination ) );
    Destination.texture   = Desc.pDestination->pBackend->pTexture;
    Destination.mip_level = Desc.DstMipLevel;
    Destination.layer     = Desc.DstLayer;
    Destination.x         = Desc.DstX;
    Destination.y         = Desc.DstY;
    Destination.z         = Desc.DstZ;

    SDL_CopyGPUTextureToTexture( pCopyPass,
                                 &Source,
                                 &Destination,
                                 Width,
                                 Height,
                                 Depth,
                                 Desc.bCycle ? true : false );

    return TRUE;
}

//==============================================================================

xbool rtarget_Copy( rtarget& Destination, const rtarget& Source )
{
    return rtarget_Copy( Destination, Source, FALSE );
}

//==============================================================================

xbool rtarget_Copy( rtarget& Destination, const rtarget& Source, xbool bCycle )
{
    rtarget_copy_desc Desc;
    Desc.pDestination = &Destination;
    Desc.pSource      = &Source;
    Desc.Width        = Source.Desc.Width;
    Desc.Height       = Source.Desc.Height;
    Desc.Depth        = 1;
    Desc.bCycle       = bCycle;

    return rtarget_Copy( Desc );
}

//==============================================================================

xbool rtarget_IsValid( const rtarget& Target )
{
    if( !Target.pBackend )
    {
        return FALSE;
    }

    if( Target.pBackend->bBackBuffer )
    {
        return ( Target.Desc.Width > 0 ) && ( Target.Desc.Height > 0 );
    }

    return Target.pBackend->pTexture != NULL;
}

//==============================================================================

xbool rtarget_HasTexture( const rtarget& Target )
{
    return Target.pBackend && ( Target.pBackend->pTexture != NULL );
}

//==============================================================================

xbool rtarget_HasRenderTarget( const rtarget& Target )
{
    return rtarget_IsValid( Target ) && Target.Desc.bRenderTarget && !Target.bIsDepthTarget;
}

//==============================================================================

xbool rtarget_HasDepthStencil( const rtarget& Target )
{
    return rtarget_IsValid( Target ) && Target.bIsDepthTarget;
}

//==============================================================================

xbool rtarget_HasShaderResource( const rtarget& Target )
{
    return Target.pBackend &&
           Target.pBackend->bShaderResource &&
           ( Target.pBackend->Resource.pBackend != NULL );
}

//==============================================================================

const shader_resource* rtarget_GetShaderResource( const rtarget& Target )
{
    if( !rtarget_HasShaderResource( Target ) )
    {
        return NULL;
    }

    return &Target.pBackend->Resource;
}

//==============================================================================

u32 sdleng_GetRenderTargetCount( void )
{
    return s_TargetCount;
}

//==============================================================================
//  FORMAT HELPERS
//==============================================================================

xbool rtarget_IsDepthFormat( rtarget_format Format )
{
    return sdleng_IsDepthFormat( Format );
}

//==============================================================================

xbool rtarget_IsSampleCountSupported( rtarget_format Format, u32 SampleCount )
{
    if( !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    SDL_GPUSampleCount SDLCount;
    if( !sdleng_ToSDLSampleCount( SampleCount, SDLCount ) )
    {
        return FALSE;
    }

    SDL_GPUTextureFormat SDLFormat = sdleng_ToSDLTextureFormat( Format );
    if( SDLFormat == SDL_GPU_TEXTUREFORMAT_INVALID )
    {
        return FALSE;
    }

    const SDL_GPUTextureUsageFlags Usage = rtarget_IsDepthFormat( Format )
                                               ? SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
                                               : SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    return SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice,
                                         SDLFormat,
                                         SDL_GPU_TEXTURETYPE_2D,
                                         Usage ) &&
           SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, SDLFormat, SDLCount );
}

//==============================================================================

const char* rtarget_GetFormatName( rtarget_format Format )
{
    switch( Format )
    {
        case RTARGET_FORMAT_RGBA8:         return "RGBA8";         break;
        case RTARGET_FORMAT_BGRA8:         return "BGRA8";         break;
        case RTARGET_FORMAT_RGBA16F:       return "RGBA16F";       break;
        case RTARGET_FORMAT_RGBA32F:       return "RGBA32F";       break;
        case RTARGET_FORMAT_RGB10A2:       return "RGB10A2";       break;
        case RTARGET_FORMAT_R8:            return "R8";            break;
        case RTARGET_FORMAT_RG16F:         return "RG16F";         break;
        case RTARGET_FORMAT_R32F:          return "R32F";          break;
        case RTARGET_FORMAT_R8_UINT:       return "R8_UINT";       break;
        case RTARGET_FORMAT_R32_UINT:      return "R32_UINT";      break;
        case RTARGET_FORMAT_DEPTH_STENCIL: return "DEPTH_STENCIL"; break;
        case RTARGET_FORMAT_DEPTH32F:      return "DEPTH32F";      break;
        default:                           return "UNKNOWN";       break;
    }
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
