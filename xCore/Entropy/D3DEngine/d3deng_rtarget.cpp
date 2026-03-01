//==============================================================================
//
//  d3deng_rtarget.cpp
//
//  Render target manager for D3DENG
//
//==============================================================================

//#define RTARGET_VERBOSE_MODE

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "d3deng_rtarget.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  STRUCTURES
//==============================================================================

static
struct rtarget_cache
{
    ID3D11RenderTargetView*   pCurrentRTVs[RTARGET_MAX_TARGETS];
    ID3D11DepthStencilView*   pCurrentDSV;
    u32                       CurrentRTVCount;

    // Store original targets for texture access
    const rtarget*            pCurrentTargets[RTARGET_MAX_TARGETS];
    const rtarget*            pCurrentDepthTarget;

    // Internal back buffer target
    rtarget                   BackBufferTarget;
    xbool                     bBackBufferValid;

    // Back buffer depth target
    rtarget                   BackBufferDepth;
    xbool                     bBackBufferDepthValid;

    xbool                     bInitialized;

    rtarget_cache( void ) : pCurrentDSV(NULL), CurrentRTVCount(0),
                           pCurrentDepthTarget(NULL),
                           bBackBufferValid(FALSE), bBackBufferDepthValid(FALSE),
                           bInitialized(FALSE)
    {
        for( u32 i = 0; i < RTARGET_MAX_TARGETS; i++ )
        {
            pCurrentRTVs[i] = NULL;
            pCurrentTargets[i] = NULL;
        }
    }
} s_TargetCache;

//------------------------------------------------------------------------------

#define RTARGET_REGISTRY_MAX 128

static
struct rtarget_registry_entry
{
    rtarget*              pTarget;
    rtarget_registration  Reg;
    xbool                 bInUse;

    rtarget_registry_entry( void ) : pTarget(NULL), bInUse(FALSE) {}
} s_TargetRegistry[RTARGET_REGISTRY_MAX];

//------------------------------------------------------------------------------

static
struct rtarget_stack_entry
{
    ID3D11RenderTargetView*   pRTVs[RTARGET_MAX_TARGETS];
    ID3D11DepthStencilView*   pDSV;
    u32                       RTVCount;

    // Store original targets for restoration
    const rtarget*            pTargets[RTARGET_MAX_TARGETS];
    const rtarget*            pDepthTarget;

    rtarget_stack_entry( void ) : pDSV(NULL), RTVCount(0), pDepthTarget(NULL)
    {
        for( u32 i = 0; i < RTARGET_MAX_TARGETS; i++ )
        {
            pRTVs[i] = NULL;
            pTargets[i] = NULL;
        }
    }
} s_TargetStack[RTARGET_STACK_DEPTH];

static s32 s_StackDepth = -1;

//------------------------------------------------------------------------------

extern void eng_GetRes( s32& XRes, s32& YRes );

//==============================================================================
//  FORMAT HELPERS
//==============================================================================

static
DXGI_FORMAT rtarget_GetDXGIFormat( rtarget_format Format, xbool bForTexture )
{
    switch( Format )
    {
        case RTARGET_FORMAT_RGBA8:           return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RTARGET_FORMAT_RGBA16F:         return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case RTARGET_FORMAT_RGBA32F:         return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RTARGET_FORMAT_RGB10A2:         return DXGI_FORMAT_R10G10B10A2_UNORM;
        case RTARGET_FORMAT_R8:              return DXGI_FORMAT_R8_UNORM;
        case RTARGET_FORMAT_RG16F:           return DXGI_FORMAT_R16G16_FLOAT;
        case RTARGET_FORMAT_R32F:            return DXGI_FORMAT_R32_FLOAT;
        case RTARGET_FORMAT_DEPTH24_STENCIL8:
            return bForTexture ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
        case RTARGET_FORMAT_DEPTH32F:
            return bForTexture ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
        default:
            x_DebugMsg( "RTargetMgr: Unknown format %d, defaulting to RGBA8\n", Format );
            return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

//==============================================================================

static
xbool rtarget_IsDepthFormat( rtarget_format Format )
{
    return (Format == RTARGET_FORMAT_DEPTH24_STENCIL8 || Format == RTARGET_FORMAT_DEPTH32F);
}

//==============================================================================

static
DXGI_FORMAT rtarget_GetSRVFormat( rtarget_format Format )
{
    switch( Format )
    {
        case RTARGET_FORMAT_DEPTH24_STENCIL8: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case RTARGET_FORMAT_DEPTH32F:         return DXGI_FORMAT_R32_FLOAT;
        default:                              return rtarget_GetDXGIFormat( Format, FALSE );
    }
}

//==============================================================================

static
const char* rtarget_GetFormatName( rtarget_format Format )
{
    static const char* s_FormatNames[RTARGET_FORMAT_COUNT] =
    {
        "RGBA8", "RGBA16F", "RGBA32F", "RGB10A2", "R8", "RG16F", "R32F",
        "DEPTH24_STENCIL8", "DEPTH32F"
    };

    return (Format < RTARGET_FORMAT_COUNT) ? s_FormatNames[Format] : "UNKNOWN";
}

//==============================================================================
//  INTERNAL HELPERS
//==============================================================================

static
s32 rtarget_FindRegistryEntry( const rtarget* pTarget )
{
    if( !pTarget )
        return -1;

    for( s32 i = 0; i < RTARGET_REGISTRY_MAX; i++ )
    {
        if( s_TargetRegistry[i].bInUse && s_TargetRegistry[i].pTarget == pTarget )
            return i;
    }

    return -1;
}

//==============================================================================

static
void rtarget_ClearRegistry( void )
{
    for( s32 i = 0; i < RTARGET_REGISTRY_MAX; i++ )
    {
        s_TargetRegistry[i].pTarget = NULL;
        s_TargetRegistry[i].bInUse = FALSE;
        s_TargetRegistry[i].Reg = rtarget_registration();
    }
}

//==============================================================================

static
xbool rtarget_GetBackBufferSize( u32& Width, u32& Height )
{
    Width = 0;
    Height = 0;

    if( s_TargetCache.bBackBufferValid )
    {
        Width = s_TargetCache.BackBufferTarget.Desc.Width;
        Height = s_TargetCache.BackBufferTarget.Desc.Height;
        return (Width > 0 && Height > 0);
    }

    if( g_pSwapChain )
    {
        DXGI_SWAP_CHAIN_DESC desc;
        g_pSwapChain->GetDesc( &desc );
        Width = desc.BufferDesc.Width;
        Height = desc.BufferDesc.Height;
        return (Width > 0 && Height > 0);
    }

    return FALSE;
}

//==============================================================================

static
xbool rtarget_ResolveRegisteredSize( const rtarget_registration& Reg, u32& Width, u32& Height )
{
    Width = 0;
    Height = 0;

    switch( Reg.Policy )
    {
        case RTARGET_SIZE_ABSOLUTE:
            Width = Reg.BaseWidth;
            Height = Reg.BaseHeight;
            break;

        case RTARGET_SIZE_BACKBUFFER:
            if( !rtarget_GetBackBufferSize( Width, Height ) )
                return FALSE;
            break;

        case RTARGET_SIZE_RELATIVE_TO_BACKBUFFER:
            if( !rtarget_GetBackBufferSize( Width, Height ) )
                return FALSE;
            Width = (u32)((f32)Width * Reg.ScaleX);
            Height = (u32)((f32)Height * Reg.ScaleY);
            break;

        case RTARGET_SIZE_RELATIVE_TO_VIEW:
        {
            s32 xRes, yRes;
            eng_GetRes( xRes, yRes );
            if( xRes <= 0 || yRes <= 0 )
                return FALSE;
            Width = (u32)((f32)xRes * Reg.ScaleX);
            Height = (u32)((f32)yRes * Reg.ScaleY);
            break;
        }
    }

    if( Width == 0 )
        Width = 1;
    if( Height == 0 )
        Height = 1;

    return TRUE;
}

//==============================================================================

static
void rtarget_ReleaseCachedViews( void )
{
    for( u32 i = 0; i < RTARGET_MAX_TARGETS; i++ )
    {
        if( s_TargetCache.pCurrentRTVs[i] )
        {
            s_TargetCache.pCurrentRTVs[i]->Release();
            s_TargetCache.pCurrentRTVs[i] = NULL;
        }
        s_TargetCache.pCurrentTargets[i] = NULL;
    }

    if( s_TargetCache.pCurrentDSV )
    {
        s_TargetCache.pCurrentDSV->Release();
        s_TargetCache.pCurrentDSV = NULL;
    }
    s_TargetCache.pCurrentDepthTarget = NULL;
    s_TargetCache.CurrentRTVCount = 0;
}

//==============================================================================

static
xbool rtarget_CreatePrimaryView( rtarget& Target, DXGI_FORMAT ViewFormat, u32 SampleCount )
{
    HRESULT hr;

    if( Target.bIsDepthTarget )
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        ZeroMemory( &dsvDesc, sizeof(dsvDesc) );
        dsvDesc.Format = ViewFormat;
        dsvDesc.ViewDimension = (SampleCount > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
        hr = g_pd3dDevice->CreateDepthStencilView( Target.pTexture, &dsvDesc, &Target.pDepthStencilView );
    }
    else
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        ZeroMemory( &rtvDesc, sizeof(rtvDesc) );
        rtvDesc.Format = ViewFormat;
        rtvDesc.ViewDimension = (SampleCount > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        hr = g_pd3dDevice->CreateRenderTargetView( Target.pTexture, &rtvDesc, &Target.pRenderTargetView );
    }

    if( FAILED(hr) )
    {
        x_DebugMsg( "RTargetMgr: Failed to create primary view, HRESULT 0x%08X\n", hr );
        return FALSE;
    }
    return TRUE;
}

//==============================================================================

static
xbool rtarget_CreateSRV( rtarget& Target, rtarget_format Format, u32 SampleCount )
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory( &srvDesc, sizeof(srvDesc) );
    srvDesc.Format = rtarget_GetSRVFormat( Format );
    srvDesc.ViewDimension = (SampleCount > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
    if( srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D )
    {
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
    }

    HRESULT hr = g_pd3dDevice->CreateShaderResourceView( Target.pTexture, &srvDesc, &Target.pShaderResourceView );
    if( FAILED(hr) )
    {
        x_DebugMsg( "RTargetMgr: Failed to create shader resource view, HRESULT 0x%08X\n", hr );
        return FALSE;
    }
    return TRUE;
}

//==============================================================================

static
s32 rtarget_RegisterInternal( rtarget& Target, const rtarget_registration& Reg )
{
    s32 index = rtarget_FindRegistryEntry( &Target );
    if( index >= 0 )
    {
        rtarget_registration& current = s_TargetRegistry[index].Reg;
        if( current.Policy != Reg.Policy ||
            current.BaseWidth != Reg.BaseWidth ||
            current.BaseHeight != Reg.BaseHeight ||
            current.ScaleX != Reg.ScaleX ||
            current.ScaleY != Reg.ScaleY ||
            current.Format != Reg.Format ||
            current.SampleCount != Reg.SampleCount ||
            current.SampleQuality != Reg.SampleQuality ||
            current.bBindAsTexture != Reg.bBindAsTexture )
        {
            current = Reg;
        }
        return index;
    }

    index = -1;
    for( s32 i = 0; i < RTARGET_REGISTRY_MAX; i++ )
    {
        if( !s_TargetRegistry[i].bInUse )
        {
            index = i;
            break;
        }
    }

    if( index < 0 )
    {
        x_DebugMsg( "RTargetMgr: Registry full, cannot register target\n" );
        return -1;
    }

    s_TargetRegistry[index].pTarget = &Target;
    s_TargetRegistry[index].Reg = Reg;
    s_TargetRegistry[index].bInUse = TRUE;
    return index;
}

//==============================================================================

static
xbool rtarget_RecreateFromRegistration( rtarget_registry_entry& Entry )
{
    u32 width = 0;
    u32 height = 0;
    if( !rtarget_ResolveRegisteredSize( Entry.Reg, width, height ) )
        return FALSE;

    if( Entry.pTarget )
    {
        if( Entry.pTarget->Desc.Width == width &&
            Entry.pTarget->Desc.Height == height &&
            Entry.pTarget->Desc.Format == Entry.Reg.Format &&
            Entry.pTarget->Desc.SampleCount == Entry.Reg.SampleCount &&
            Entry.pTarget->Desc.SampleQuality == Entry.Reg.SampleQuality &&
            Entry.pTarget->Desc.bBindAsTexture == Entry.Reg.bBindAsTexture )
        {
            return TRUE;
        }
    }

    rtarget_desc desc;
    desc.Width = width;
    desc.Height = height;
    desc.Format = Entry.Reg.Format;
    desc.SampleCount = Entry.Reg.SampleCount;
    desc.SampleQuality = Entry.Reg.SampleQuality;
    desc.bBindAsTexture = Entry.Reg.bBindAsTexture;

    if( Entry.pTarget && rtarget_Create( *Entry.pTarget, desc ) )
    {
        x_DebugMsg( "RTargetMgr: Recreated registered target %dx%d, format %s\n",
                    width, height, rtarget_GetFormatName( desc.Format ) );
        return TRUE;
    }

    return FALSE;
}

//==============================================================================

static
xbool rtarget_CreateFromBackBuffer( rtarget& Target, ID3D11Texture2D* pBackBuffer )
{
    if( !g_pd3dDevice || !pBackBuffer )
        return FALSE;

    rtarget_Destroy( Target );

    D3D11_TEXTURE2D_DESC texDesc;
    pBackBuffer->GetDesc( &texDesc );

    rtarget_format internalFormat = RTARGET_FORMAT_RGBA8;
    switch( texDesc.Format )
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            internalFormat = RTARGET_FORMAT_RGBA8;
            break;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            internalFormat = RTARGET_FORMAT_RGB10A2;
            break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            internalFormat = RTARGET_FORMAT_RGBA16F;
            break;
    }

    Target.Desc.Width = texDesc.Width;
    Target.Desc.Height = texDesc.Height;
    Target.Desc.Format = internalFormat;
    Target.Desc.SampleCount = texDesc.SampleDesc.Count;
    Target.Desc.SampleQuality = texDesc.SampleDesc.Quality;
    Target.Desc.bBindAsTexture = FALSE;

    Target.bIsDepthTarget = FALSE;
    Target.pTexture = pBackBuffer;
    Target.pTexture->AddRef();

    HRESULT hr = g_pd3dDevice->CreateRenderTargetView( Target.pTexture, NULL, &Target.pRenderTargetView );

    if( FAILED(hr) )
    {
        x_DebugMsg( "RTargetMgr: Failed to create back buffer RTV, HRESULT 0x%08X\n", hr );
        rtarget_Destroy( Target );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool rtarget_CheckBackBufferResize( void )
{
    if( !g_pSwapChain || !s_TargetCache.bBackBufferValid )
        return FALSE;

    DXGI_SWAP_CHAIN_DESC currentDesc;
    g_pSwapChain->GetDesc( &currentDesc );

    if( s_TargetCache.BackBufferTarget.Desc.Width != currentDesc.BufferDesc.Width ||
        s_TargetCache.BackBufferTarget.Desc.Height != currentDesc.BufferDesc.Height )
    {
        x_DebugMsg( "RTargetMgr: Detected resolution change %dx%d -> %dx%d\n",
                   s_TargetCache.BackBufferTarget.Desc.Width,
                   s_TargetCache.BackBufferTarget.Desc.Height,
                   currentDesc.BufferDesc.Width,
                   currentDesc.BufferDesc.Height );
        return TRUE;
    }

    return FALSE;
}

//==============================================================================

static
xbool rtarget_CreateBackBufferTarget( void )
{
    if( !s_TargetCache.bInitialized )
    {
        x_DebugMsg( "RTargetMgr: System not initialized\n" );
        ASSERT(FALSE);
        return FALSE;
    }

    if( !g_pSwapChain )
    {
        x_DebugMsg( "RTargetMgr: No swap chain available for back buffer creation\n" );
        return FALSE;
    }

    // Clean up existing back buffer
    if( s_TargetCache.bBackBufferValid )
    {
        rtarget_Destroy( s_TargetCache.BackBufferTarget );
        s_TargetCache.bBackBufferValid = FALSE;
    }

    // Clean up existing depth buffer
    if( s_TargetCache.bBackBufferDepthValid )
    {
        rtarget_Destroy( s_TargetCache.BackBufferDepth );
        s_TargetCache.bBackBufferDepthValid = FALSE;
    }

    // Get back buffer from swap chain
    ID3D11Texture2D* pBackBuffer = NULL;
    HRESULT hr = g_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer );
    if( FAILED(hr) )
    {
        x_DebugMsg( "RTargetMgr: Failed to get back buffer from swap chain, HRESULT 0x%08X\n", hr );
        return FALSE;
    }

    // Create rtarget from back buffer
    if( !rtarget_CreateFromBackBuffer( s_TargetCache.BackBufferTarget, pBackBuffer ) )
    {
        x_DebugMsg( "RTargetMgr: Failed to create back buffer target\n" );
        pBackBuffer->Release();
        return FALSE;
    }

    pBackBuffer->Release();
    s_TargetCache.bBackBufferValid = TRUE;

    // Create depth buffer for back buffer
    rtarget_desc depthDesc;
    depthDesc.Width = s_TargetCache.BackBufferTarget.Desc.Width;
    depthDesc.Height = s_TargetCache.BackBufferTarget.Desc.Height;
    depthDesc.Format = RTARGET_FORMAT_DEPTH24_STENCIL8;
    depthDesc.SampleCount = 1;
    depthDesc.SampleQuality = 0;
    depthDesc.bBindAsTexture = TRUE;

    if( !rtarget_Create( s_TargetCache.BackBufferDepth, depthDesc ) )
    {
        x_DebugMsg( "RTargetMgr: Failed to create back buffer depth target\n" );
        rtarget_Destroy( s_TargetCache.BackBufferTarget );
        s_TargetCache.bBackBufferValid = FALSE;
        return FALSE;
    }

    s_TargetCache.bBackBufferDepthValid = TRUE;

    x_DebugMsg( "RTargetMgr: Back buffer target created successfully with depth buffer\n" );

    return TRUE;
}

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void rtarget_Init( void )
{
    if( s_TargetCache.bInitialized )
    {
        x_DebugMsg( "RTargetMgr: Already initialized\n" );
        ASSERT(FALSE);
        return;
    }

    for( u32 i = 0; i < RTARGET_MAX_TARGETS; i++ )
    {
        s_TargetCache.pCurrentRTVs[i] = NULL;
        s_TargetCache.pCurrentTargets[i] = NULL;
    }
    s_TargetCache.pCurrentDSV = NULL;
    s_TargetCache.pCurrentDepthTarget = NULL;
    s_TargetCache.CurrentRTVCount = 0;

    s_StackDepth = -1;

    rtarget_ClearRegistry();

    s_TargetCache.bInitialized = TRUE;

    x_DebugMsg( "RTargetMgr: Initialization complete\n" );
}

//==============================================================================

void rtarget_Kill( void )
{
    if( !s_TargetCache.bInitialized )
    {
        x_DebugMsg( "RTargetMgr: Not initialized\n" );
        ASSERT(FALSE);
        return;
    }

    x_DebugMsg( "RTargetMgr: Shutting down render target management system\n" );

    if( s_TargetCache.bBackBufferValid )
    {
        rtarget_Destroy( s_TargetCache.BackBufferTarget );
        s_TargetCache.bBackBufferValid = FALSE;
    }

    if( s_TargetCache.bBackBufferDepthValid )
    {
        rtarget_Destroy( s_TargetCache.BackBufferDepth );
        s_TargetCache.bBackBufferDepthValid = FALSE;
    }

    rtarget_ReleaseCachedViews();
    rtarget_ClearRegistry();

    s_StackDepth = -1;

    s_TargetCache.bInitialized = FALSE;

    x_DebugMsg( "RTargetMgr: Shutdown complete\n" );
}

//==============================================================================
//  RENDER TARGET CREATION
//==============================================================================

xbool rtarget_Create( rtarget& Target, const rtarget_desc& Desc )
{
    if( !g_pd3dDevice )
        return FALSE;

    if( Desc.Width == 0 || Desc.Height == 0 )
    {
        x_DebugMsg( "RTargetMgr: Invalid dimensions %dx%d\n", Desc.Width, Desc.Height );
        return FALSE;
    }

    // Clean up existing target
    rtarget_Destroy( Target );

    Target.Desc = Desc;
    Target.bIsDepthTarget = rtarget_IsDepthFormat( Desc.Format );

    DXGI_FORMAT textureFormat = rtarget_GetDXGIFormat( Desc.Format, TRUE );
    DXGI_FORMAT viewFormat    = rtarget_GetDXGIFormat( Desc.Format, FALSE );

    // Create texture
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory( &texDesc, sizeof(texDesc) );
    texDesc.Width = Desc.Width;
    texDesc.Height = Desc.Height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = textureFormat;
    texDesc.SampleDesc.Count = Desc.SampleCount;
    texDesc.SampleDesc.Quality = Desc.SampleQuality;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;
    texDesc.BindFlags = Target.bIsDepthTarget ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
    if( Desc.bBindAsTexture )
        texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = g_pd3dDevice->CreateTexture2D( &texDesc, NULL, &Target.pTexture );
    if( FAILED(hr) )
    {
        x_DebugMsg( "RTargetMgr: Failed to create texture, HRESULT 0x%08X\n", hr );
        return FALSE;
    }

    if( !rtarget_CreatePrimaryView( Target, viewFormat, Desc.SampleCount ) )
    {
        rtarget_Destroy( Target );
        return FALSE;
    }

    if( Desc.bBindAsTexture && !rtarget_CreateSRV( Target, Desc.Format, Desc.SampleCount ) )
    {
        rtarget_Destroy( Target );
        return FALSE;
    }

    x_DebugMsg( "RTargetMgr: Created %s target %dx%d, format %s\n",
                Target.bIsDepthTarget ? "depth" : "color",
                Desc.Width, Desc.Height,
                rtarget_GetFormatName( Desc.Format ) );

    return TRUE;
}

//==============================================================================

xbool rtarget_CreateFromTexture( rtarget& Target, ID3D11Texture2D* pTexture, rtarget_format Format )
{
    if( !g_pd3dDevice )
        return FALSE;

    if( !pTexture )
    {
        x_DebugMsg( "RTargetMgr: Invalid parameter for texture target creation\n" );
        ASSERT(FALSE);
        return FALSE;
    }

    // Clean up existing target
    rtarget_Destroy( Target );

    // Get texture description
    D3D11_TEXTURE2D_DESC texDesc;
    pTexture->GetDesc( &texDesc );

    Target.Desc.Width = texDesc.Width;
    Target.Desc.Height = texDesc.Height;
    Target.Desc.Format = Format;
    Target.Desc.SampleCount = texDesc.SampleDesc.Count;
    Target.Desc.SampleQuality = texDesc.SampleDesc.Quality;
    Target.Desc.bBindAsTexture = (texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) ? TRUE : FALSE;

    Target.bIsDepthTarget = rtarget_IsDepthFormat( Format );
    Target.pTexture = pTexture;
    Target.pTexture->AddRef();

    DXGI_FORMAT viewFormat = rtarget_GetDXGIFormat( Format, FALSE );

    if( !rtarget_CreatePrimaryView( Target, viewFormat, texDesc.SampleDesc.Count ) )
    {
        rtarget_Destroy( Target );
        return FALSE;
    }

    if( Target.Desc.bBindAsTexture && !rtarget_CreateSRV( Target, Format, texDesc.SampleDesc.Count ) )
    {
        rtarget_Destroy( Target );
        return FALSE;
    }

    x_DebugMsg( "RTargetMgr: Created target from existing texture\n" );

    return TRUE;
}

//==============================================================================

void rtarget_Destroy( rtarget& Target )
{
    if( Target.pShaderResourceView )
    {
        Target.pShaderResourceView->Release();
        Target.pShaderResourceView = NULL;
    }

    if( Target.pRenderTargetView )
    {
        Target.pRenderTargetView->Release();
        Target.pRenderTargetView = NULL;
    }

    if( Target.pDepthStencilView )
    {
        Target.pDepthStencilView->Release();
        Target.pDepthStencilView = NULL;
    }

    if( Target.pTexture )
    {
        Target.pTexture->Release();
        Target.pTexture = NULL;
    }

    Target.bIsDepthTarget = FALSE;
    Target.Desc = rtarget_desc();
}

//==============================================================================
//  REGISTRATION SYSTEM
//==============================================================================

xbool rtarget_Register( rtarget& Target, const rtarget_registration& Reg )
{
    return rtarget_RegisterInternal( Target, Reg ) >= 0;
}

//==============================================================================

xbool rtarget_GetOrCreate( rtarget& Target, const rtarget_registration& Reg )
{
    s32 index = rtarget_RegisterInternal( Target, Reg );
    if( index < 0 )
        return FALSE;

    return rtarget_RecreateFromRegistration( s_TargetRegistry[index] );
}

//==============================================================================

void rtarget_Unregister( rtarget& Target )
{
    s32 index = rtarget_FindRegistryEntry( &Target );
    if( index < 0 )
        return;

    s_TargetRegistry[index].pTarget = NULL;
    s_TargetRegistry[index].bInUse = FALSE;
    s_TargetRegistry[index].Reg = rtarget_registration();
}

//==============================================================================

void rtarget_NotifyResolutionChanged( void )
{
    for( s32 i = 0; i < RTARGET_REGISTRY_MAX; i++ )
    {
        if( s_TargetRegistry[i].bInUse )
            rtarget_RecreateFromRegistration( s_TargetRegistry[i] );
    }
}

//==============================================================================

void rtarget_ReleaseBackBufferTargets( void )
{
    if( !s_TargetCache.bInitialized )
        return;

    if( g_pd3dContext )
        g_pd3dContext->OMSetRenderTargets( 0, NULL, NULL );

    for( s32 i = 0; i <= s_StackDepth; i++ )
    {
        rtarget_stack_entry& entry = s_TargetStack[i];

        for( u32 j = 0; j < RTARGET_MAX_TARGETS; j++ )
        {
            if( entry.pRTVs[j] )
            {
                entry.pRTVs[j]->Release();
                entry.pRTVs[j] = NULL;
            }
            entry.pTargets[j] = NULL;
        }

        if( entry.pDSV )
        {
            entry.pDSV->Release();
            entry.pDSV = NULL;
        }
        entry.pDepthTarget = NULL;
        entry.RTVCount = 0;
    }

    s_StackDepth = -1;

    #ifdef RTARGET_VERBOSE_MODE
    x_DebugMsg( "RTargetMgr: Cleared target stack during back buffer release\n" );
    #endif

    rtarget_ReleaseCachedViews();

    if( s_TargetCache.bBackBufferValid )
    {
        rtarget_Destroy( s_TargetCache.BackBufferTarget );
        s_TargetCache.bBackBufferValid = FALSE;
    }

    if( s_TargetCache.bBackBufferDepthValid )
    {
        rtarget_Destroy( s_TargetCache.BackBufferDepth );
        s_TargetCache.bBackBufferDepthValid = FALSE;
    }
}

//==============================================================================
//  RENDER TARGET MANAGEMENT
//==============================================================================

xbool rtarget_SetTargets( const rtarget* pTargets, u32 Count, const rtarget* pDepthTarget )
{
    if( !s_TargetCache.bInitialized )
    {
        x_DebugMsg( "RTargetMgr: Not initialized\n" );
        ASSERT(FALSE);
        return FALSE;
    }

    if( !g_pd3dContext )
        return FALSE;

    if( Count > RTARGET_MAX_TARGETS )
    {
        x_DebugMsg( "RTargetMgr: Too many targets requested (%d, max %d)\n", Count, RTARGET_MAX_TARGETS );
        ASSERT(FALSE);
        return FALSE;
    }

    ID3D11RenderTargetView* pRTVs[RTARGET_MAX_TARGETS] = { NULL };
    ID3D11DepthStencilView* pDSV = NULL;

    // Collect RTVs
    for( u32 i = 0; i < Count; i++ )
    {
        if( pTargets[i].bIsDepthTarget )
        {
            x_DebugMsg( "RTargetMgr: Depth target in color target array at index %d\n", i );
            ASSERT(FALSE);
            return FALSE;
        }

        pRTVs[i] = pTargets[i].pRenderTargetView;
        if( !pRTVs[i] )
        {
            x_DebugMsg( "RTargetMgr: Color target %d has NULL RTV (not created?)\n", i );
            ASSERT(FALSE);
            return FALSE;
        }
    }

    // Collect DSV
    if( pDepthTarget )
    {
        if( !pDepthTarget->bIsDepthTarget )
        {
            x_DebugMsg( "RTargetMgr: Color target passed as depth target\n" );
            ASSERT(FALSE);
            return FALSE;
        }
        pDSV = pDepthTarget->pDepthStencilView;
    }

    // Check cache
    if( s_TargetCache.CurrentRTVCount == Count &&
        s_TargetCache.pCurrentDSV == pDSV &&
        s_TargetCache.pCurrentDepthTarget == pDepthTarget )
    {
        xbool bSame = TRUE;
        for( u32 i = 0; i < Count && bSame; i++ )
        {
            if( s_TargetCache.pCurrentRTVs[i] != pRTVs[i] ||
                s_TargetCache.pCurrentTargets[i] != &pTargets[i] )
                bSame = FALSE;
        }

        if( bSame )
        {
            #ifdef RTARGET_VERBOSE_MODE
            x_DebugMsg( "RTargetMgr: Targets already bound (CACHED)\n" );
            #endif
            return TRUE;
        }
    }

    // Set targets
    g_pd3dContext->OMSetRenderTargets( Count, pRTVs, pDSV );

    // Update cache
    for( u32 i = 0; i < RTARGET_MAX_TARGETS; i++ )
    {
        if( s_TargetCache.pCurrentRTVs[i] )
            s_TargetCache.pCurrentRTVs[i]->Release();
        s_TargetCache.pCurrentRTVs[i] = (i < Count) ? pRTVs[i] : NULL;
        if( s_TargetCache.pCurrentRTVs[i] )
            s_TargetCache.pCurrentRTVs[i]->AddRef();

        s_TargetCache.pCurrentTargets[i] = (i < Count) ? &pTargets[i] : NULL;
    }

    if( s_TargetCache.pCurrentDSV )
        s_TargetCache.pCurrentDSV->Release();
    s_TargetCache.pCurrentDSV = pDSV;
    if( s_TargetCache.pCurrentDSV )
        s_TargetCache.pCurrentDSV->AddRef();

    s_TargetCache.pCurrentDepthTarget = pDepthTarget;
    s_TargetCache.CurrentRTVCount = Count;

    #ifdef RTARGET_VERBOSE_MODE
    x_DebugMsg( "RTargetMgr: Set %d color targets with %s\n", Count, pDSV ? "depth" : "no depth" );
    #endif

    return TRUE;
}

//==============================================================================

xbool rtarget_SetBackBuffer( void )
{
    if( !s_TargetCache.bInitialized )
    {
        x_DebugMsg( "RTargetMgr: Not initialized\n" );
        ASSERT(FALSE);
        return FALSE;
    }

    if( !s_TargetCache.bBackBufferValid ||
        !s_TargetCache.bBackBufferDepthValid ||
        rtarget_CheckBackBufferResize() )
    {
        if( !rtarget_CreateBackBufferTarget() )
        {
            x_DebugMsg( "RTargetMgr: Failed to recreate back buffer\n" );
            ASSERT(FALSE);
            return FALSE;
        }
    }

    return rtarget_SetTargets( &s_TargetCache.BackBufferTarget, 1, &s_TargetCache.BackBufferDepth );
}

//==============================================================================

const rtarget* rtarget_GetBackBuffer( void )
{
    if( !s_TargetCache.bInitialized )
    {
        x_DebugMsg( "RTargetMgr: Not initialized\n" );
        ASSERT(FALSE);
        return NULL;
    }

    if( !s_TargetCache.bBackBufferValid || rtarget_CheckBackBufferResize() )
    {
        if( !rtarget_CreateBackBufferTarget() )
        {
            x_DebugMsg( "RTargetMgr: Failed to create back buffer\n" );
            ASSERT(FALSE);
            return NULL;
        }
    }

    return &s_TargetCache.BackBufferTarget;
}

//==============================================================================

const rtarget* rtarget_GetCurrentTarget( u32 Index )
{
    if( Index >= s_TargetCache.CurrentRTVCount )
        return NULL;

    return s_TargetCache.pCurrentTargets[Index];
}

//==============================================================================

u32 rtarget_GetCurrentCount( void )
{
    return s_TargetCache.CurrentRTVCount;
}

//==============================================================================

const rtarget* rtarget_GetCurrentDepth( void )
{
    return s_TargetCache.pCurrentDepthTarget;
}

//==============================================================================

xbool rtarget_PushTargets( void )
{
    if( s_StackDepth >= RTARGET_STACK_DEPTH - 1 )
    {
        x_DebugMsg( "RTargetMgr: Stack overflow (depth %d)\n", s_StackDepth );
        ASSERT(FALSE);
        return FALSE;
    }

    s_StackDepth++;

    rtarget_stack_entry& entry = s_TargetStack[s_StackDepth];
    for( u32 i = 0; i < RTARGET_MAX_TARGETS; i++ )
    {
        entry.pRTVs[i] = s_TargetCache.pCurrentRTVs[i];
        if( entry.pRTVs[i] )
            entry.pRTVs[i]->AddRef();

        entry.pTargets[i] = s_TargetCache.pCurrentTargets[i];
    }
    entry.pDSV = s_TargetCache.pCurrentDSV;
    if( entry.pDSV )
        entry.pDSV->AddRef();
    entry.pDepthTarget = s_TargetCache.pCurrentDepthTarget;
    entry.RTVCount = s_TargetCache.CurrentRTVCount;

    #ifdef RTARGET_VERBOSE_MODE
    x_DebugMsg( "RTargetMgr: Pushed targets (depth %d)\n", s_StackDepth );
    #endif

    return TRUE;
}

//==============================================================================

xbool rtarget_PopTargets( void )
{
    if( s_StackDepth < 0 )
    {
        x_DebugMsg( "RTargetMgr: Stack underflow\n" );
        ASSERT(FALSE);
        return FALSE;
    }

    rtarget_stack_entry& entry = s_TargetStack[s_StackDepth];
    g_pd3dContext->OMSetRenderTargets( entry.RTVCount, entry.pRTVs, entry.pDSV );

    for( u32 i = 0; i < RTARGET_MAX_TARGETS; i++ )
    {
        if( s_TargetCache.pCurrentRTVs[i] )
            s_TargetCache.pCurrentRTVs[i]->Release();
        s_TargetCache.pCurrentRTVs[i] = entry.pRTVs[i];
        entry.pRTVs[i] = NULL; // Transfer ownership

        s_TargetCache.pCurrentTargets[i] = entry.pTargets[i];
        entry.pTargets[i] = NULL;
    }

    if( s_TargetCache.pCurrentDSV )
        s_TargetCache.pCurrentDSV->Release();
    s_TargetCache.pCurrentDSV = entry.pDSV;
    entry.pDSV = NULL; // Transfer ownership

    s_TargetCache.pCurrentDepthTarget = entry.pDepthTarget;
    entry.pDepthTarget = NULL;
    s_TargetCache.CurrentRTVCount = entry.RTVCount;

    s_StackDepth--;

    #ifdef RTARGET_VERBOSE_MODE
    x_DebugMsg( "RTargetMgr: Popped targets (depth %d)\n", s_StackDepth );
    #endif

    return TRUE;
}

//==============================================================================
//  UTILITY FUNCTIONS
//==============================================================================

void rtarget_Clear( u32 ClearFlags, const f32* pColor, f32 Depth, u8 Stencil )
{
    if( !g_pd3dContext )
        return;

    if( ClearFlags & RTARGET_CLEAR_COLOR )
    {
        f32 defaultColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        const f32* clearColor = pColor ? pColor : defaultColor;

        for( u32 i = 0; i < s_TargetCache.CurrentRTVCount; i++ )
        {
            if( s_TargetCache.pCurrentRTVs[i] )
                g_pd3dContext->ClearRenderTargetView( s_TargetCache.pCurrentRTVs[i], clearColor );
        }
    }

    if( (ClearFlags & (RTARGET_CLEAR_DEPTH | RTARGET_CLEAR_STENCIL)) && s_TargetCache.pCurrentDSV )
    {
        UINT d3dClearFlags = 0;
        if( ClearFlags & RTARGET_CLEAR_DEPTH )
            d3dClearFlags |= D3D11_CLEAR_DEPTH;
        if( ClearFlags & RTARGET_CLEAR_STENCIL )
            d3dClearFlags |= D3D11_CLEAR_STENCIL;

        g_pd3dContext->ClearDepthStencilView( s_TargetCache.pCurrentDSV, d3dClearFlags, Depth, Stencil );
    }
}

//==============================================================================

void rtarget_ClearColor( const rtarget& Target, const f32* pColor )
{
    if( !g_pd3dContext || !Target.pRenderTargetView )
        return;

    f32 defaultColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dContext->ClearRenderTargetView( Target.pRenderTargetView, pColor ? pColor : defaultColor );
}

//==============================================================================

void rtarget_ClearDepthStencil( const rtarget& Target, u32 ClearFlags, f32 Depth, u8 Stencil )
{
    if( !g_pd3dContext || !Target.pDepthStencilView )
        return;

    UINT d3dClearFlags = 0;
    if( ClearFlags & RTARGET_CLEAR_DEPTH )
        d3dClearFlags |= D3D11_CLEAR_DEPTH;
    if( ClearFlags & RTARGET_CLEAR_STENCIL )
        d3dClearFlags |= D3D11_CLEAR_STENCIL;

    if( d3dClearFlags )
        g_pd3dContext->ClearDepthStencilView( Target.pDepthStencilView, d3dClearFlags, Depth, Stencil );
}