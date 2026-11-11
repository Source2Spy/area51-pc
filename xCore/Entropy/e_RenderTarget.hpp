//==============================================================================
//
//  e_RenderTarget.hpp
//
//  Explicit render target and render pass API for PSO render backends.
//
//==============================================================================

#ifndef E_RENDERTARGET_HPP
#define E_RENDERTARGET_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

struct shader_resource;

//==============================================================================
//  CONSTANTS
//==============================================================================

#define RTARGET_MAX_TARGETS 4

//==============================================================================
//  ENUMS
//==============================================================================

enum rtarget_format
{
    RTARGET_FORMAT_RGBA8 = 0,
    RTARGET_FORMAT_BGRA8,
    RTARGET_FORMAT_RGBA16F,
    RTARGET_FORMAT_RGBA32F,
    RTARGET_FORMAT_RGB10A2,
    RTARGET_FORMAT_R8,
    RTARGET_FORMAT_RG16F,
    RTARGET_FORMAT_R32F,
    RTARGET_FORMAT_DEPTH_STENCIL,
    RTARGET_FORMAT_DEPTH32F,
    RTARGET_FORMAT_R8_UINT,
    RTARGET_FORMAT_R32_UINT,
    RTARGET_FORMAT_COUNT
};

//------------------------------------------------------------------------------

enum rtarget_load_op
{
    RTARGET_LOAD_LOAD = 0,
    RTARGET_LOAD_CLEAR,
    RTARGET_LOAD_DONT_CARE
};

//------------------------------------------------------------------------------

enum rtarget_store_op
{
    RTARGET_STORE_STORE = 0,
    RTARGET_STORE_DONT_CARE,
    RTARGET_STORE_RESOLVE,
    RTARGET_STORE_RESOLVE_AND_STORE
};

//------------------------------------------------------------------------------

enum rtarget_size_policy
{
    RTARGET_SIZE_ABSOLUTE = 0,
    RTARGET_SIZE_BACKBUFFER,
    RTARGET_SIZE_RELATIVE_TO_BACKBUFFER,
    RTARGET_SIZE_RELATIVE_TO_VIEW
};

//==============================================================================
//  STRUCTURES
//==============================================================================

struct rtarget_backend;

//------------------------------------------------------------------------------

struct rtarget_desc
{
    u32             Width;
    u32             Height;
    rtarget_format  Format;
    u32             MipCount;
    u32             SampleCount;
    u32             SampleQuality;
    xbool           bBindAsTexture;
    xbool           bRenderTarget;
    u32             StorageUsage;
    f32             ClearColor[4];
    f32             ClearDepth;
    u8              ClearStencil;
    const char*     pDebugName;

    rtarget_desc( void ) :
        Width         ( 0 ),
        Height        ( 0 ),
        Format        ( RTARGET_FORMAT_RGBA8 ),
        MipCount      ( 1 ),
        SampleCount   ( 1 ),
        SampleQuality ( 0 ),
        bBindAsTexture( TRUE ),
        bRenderTarget ( TRUE ),
        StorageUsage  ( 0 ),
        ClearDepth    ( 1.0f ),
        ClearStencil  ( 0 ),
        pDebugName    ( NULL )
    {
        ClearColor[0] = 0.0f;
        ClearColor[1] = 0.0f;
        ClearColor[2] = 0.0f;
        ClearColor[3] = 1.0f;
    }
};

//------------------------------------------------------------------------------

struct rtarget
{
    rtarget_desc      Desc;
    xbool             bIsDepthTarget;
    rtarget_backend*  pBackend;

    rtarget( void ) :
        Desc          (),
        bIsDepthTarget( FALSE ),
        pBackend      ( NULL )
    {
    }

    operator xbool( void ) const { return pBackend != NULL; }
};

//------------------------------------------------------------------------------

struct rtarget_registration
{
    rtarget_size_policy   Policy;
    u32                   BaseWidth;
    u32                   BaseHeight;
    f32                   ScaleX;
    f32                   ScaleY;
    rtarget_format        Format;
    u32                   SampleCount;
    u32                   SampleQuality;
    xbool                 bBindAsTexture;
    const char*           pDebugName;

    rtarget_registration( void ) :
        Policy        ( RTARGET_SIZE_ABSOLUTE ),
        BaseWidth     ( 0 ),
        BaseHeight    ( 0 ),
        ScaleX        ( 1.0f ),
        ScaleY        ( 1.0f ),
        Format        ( RTARGET_FORMAT_RGBA8 ),
        SampleCount   ( 1 ),
        SampleQuality ( 0 ),
        bBindAsTexture( TRUE ),
        pDebugName    ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rtarget_color_attachment_desc
{
    const rtarget*  pTarget;
    const rtarget*  pResolveTarget;
    u32             MipLevel;
    u32             Layer;
    u32             ResolveMipLevel;
    u32             ResolveLayer;
    rtarget_load_op LoadOp;
    rtarget_store_op StoreOp;
    f32             ClearColor[4];
    xbool           bCycle;
    xbool           bCycleResolve;

    rtarget_color_attachment_desc( void ) :
        pTarget         ( NULL ),
        pResolveTarget  ( NULL ),
        MipLevel        ( 0 ),
        Layer           ( 0 ),
        ResolveMipLevel ( 0 ),
        ResolveLayer    ( 0 ),
        LoadOp          ( RTARGET_LOAD_LOAD ),
        StoreOp         ( RTARGET_STORE_STORE ),
        bCycle          ( FALSE ),
        bCycleResolve   ( FALSE )
    {
        ClearColor[0] = 0.0f;
        ClearColor[1] = 0.0f;
        ClearColor[2] = 0.0f;
        ClearColor[3] = 1.0f;
    }
};

//------------------------------------------------------------------------------

struct rtarget_depth_attachment_desc
{
    const rtarget*   pTarget;
    u32              MipLevel;
    u32              Layer;
    rtarget_load_op  DepthLoadOp;
    rtarget_store_op DepthStoreOp;
    rtarget_load_op  StencilLoadOp;
    rtarget_store_op StencilStoreOp;
    f32              ClearDepth;
    u8               ClearStencil;
    xbool            bCycle;

    rtarget_depth_attachment_desc( void ) :
        pTarget       ( NULL ),
        MipLevel      ( 0 ),
        Layer         ( 0 ),
        DepthLoadOp   ( RTARGET_LOAD_LOAD ),
        DepthStoreOp  ( RTARGET_STORE_STORE ),
        StencilLoadOp ( RTARGET_LOAD_LOAD ),
        StencilStoreOp( RTARGET_STORE_STORE ),
        ClearDepth    ( 1.0f ),
        ClearStencil  ( 0 ),
        bCycle        ( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct rtarget_pass_desc
{
    const rtarget_color_attachment_desc* pColors;
    u32                                  ColorCount;
    const rtarget_depth_attachment_desc* pDepthStencil;
    const char*                          pDebugName;

    rtarget_pass_desc( void ) :
        pColors      ( NULL ),
        ColorCount   ( 0 ),
        pDepthStencil( NULL ),
        pDebugName   ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct rtarget_copy_desc
{
    rtarget*        pDestination;
    const rtarget*  pSource;
    u32             DstMipLevel;
    u32             DstLayer;
    u32             DstX;
    u32             DstY;
    u32             DstZ;
    u32             SrcMipLevel;
    u32             SrcLayer;
    u32             SrcX;
    u32             SrcY;
    u32             SrcZ;
    u32             Width;
    u32             Height;
    u32             Depth;
    xbool           bCycle;

    rtarget_copy_desc( void ) :
        pDestination( NULL ),
        pSource     ( NULL ),
        DstMipLevel ( 0 ),
        DstLayer    ( 0 ),
        DstX        ( 0 ),
        DstY        ( 0 ),
        DstZ        ( 0 ),
        SrcMipLevel ( 0 ),
        SrcLayer    ( 0 ),
        SrcX        ( 0 ),
        SrcY        ( 0 ),
        SrcZ        ( 0 ),
        Width       ( 0 ),
        Height      ( 0 ),
        Depth       ( 1 ),
        bCycle      ( FALSE )
    {
    }
};

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void                rtarget_Init                ( void );
void                rtarget_Kill                ( void );

//==============================================================================
//  RENDER TARGET CREATION
//==============================================================================

xbool               rtarget_Create              ( rtarget& Target, const rtarget_desc& Desc );
void                rtarget_Destroy             ( rtarget& Target );

xbool               rtarget_Register            ( rtarget& Target, const rtarget_registration& Reg );
xbool               rtarget_GetOrCreate         ( rtarget& Target, const rtarget_registration& Reg );
void                rtarget_Unregister          ( rtarget& Target );
xbool               rtarget_NotifyResolutionChanged( void );
void                rtarget_ReleaseBackBufferTargets( void );

//==============================================================================
//  RENDER PASS MANAGEMENT
//==============================================================================

xbool               rtarget_BeginPass           ( const rtarget_pass_desc& Desc );
xbool               rtarget_BeginPass           ( const rtarget_color_attachment_desc* pColors,
                                                  u32 ColorCount,
                                                  const rtarget_depth_attachment_desc* pDepthStencil = NULL );
void                rtarget_EndPass             ( void );
void                rtarget_SetGraphPassOwner   ( const char* pPassName );

rtarget_format     rtarget_GetBackBufferFormat  ( void );
const rtarget*      rtarget_GetBackBuffer       ( void );
const rtarget*      rtarget_GetCurrentTarget    ( u32 Index );
u32                 rtarget_GetCurrentCount     ( void );
const rtarget*      rtarget_GetCurrentDepth     ( void );

//==============================================================================
//  COPY AND RESOURCE ACCESS
//==============================================================================

xbool               rtarget_Copy                ( const rtarget_copy_desc& Desc );
xbool               rtarget_Copy                ( rtarget& Destination, const rtarget& Source );
xbool               rtarget_Copy                ( rtarget& Destination, const rtarget& Source, xbool bCycle );

xbool               rtarget_IsValid             ( const rtarget& Target );
xbool               rtarget_HasTexture          ( const rtarget& Target );
xbool               rtarget_HasRenderTarget     ( const rtarget& Target );
xbool               rtarget_HasDepthStencil     ( const rtarget& Target );
xbool               rtarget_HasShaderResource   ( const rtarget& Target );

const shader_resource*
                    rtarget_GetShaderResource   ( const rtarget& Target );

//==============================================================================
//  FORMAT HELPERS
//==============================================================================

xbool               rtarget_IsDepthFormat       ( rtarget_format Format );
xbool               rtarget_IsSampleCountSupported( rtarget_format Format, u32 SampleCount );
const char*         rtarget_GetFormatName       ( rtarget_format Format );

//==============================================================================
#endif // E_RENDERTARGET_HPP
//==============================================================================
