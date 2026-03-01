//==============================================================================
//  
//  d3deng_rtarget.hpp
//  
//  Render target manager for D3DENG
//
//==============================================================================

#ifndef D3DENG_RTARGET_HPP
#define D3DENG_RTARGET_HPP

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

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#include "d3deng_private.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define RTARGET_MAX_TARGETS     8       // Maximum MRT count for DX11
#define RTARGET_STACK_DEPTH     16      // Maximum backup stack depth

//==============================================================================
//  ENUMS
//==============================================================================

enum rtarget_format
{
    RTARGET_FORMAT_RGBA8 = 0,
    RTARGET_FORMAT_RGBA16F,
    RTARGET_FORMAT_RGBA32F,
    RTARGET_FORMAT_RGB10A2,
    RTARGET_FORMAT_R8,
    RTARGET_FORMAT_RG16F,
    RTARGET_FORMAT_R32F,
    RTARGET_FORMAT_DEPTH24_STENCIL8,
    RTARGET_FORMAT_DEPTH32F,
    RTARGET_FORMAT_COUNT
};

//------------------------------------------------------------------------------

enum rtarget_clear_flags
{
    RTARGET_CLEAR_COLOR   = (1 << 0),
    RTARGET_CLEAR_DEPTH   = (1 << 1),
    RTARGET_CLEAR_STENCIL = (1 << 2),
    RTARGET_CLEAR_ALL     = RTARGET_CLEAR_COLOR | RTARGET_CLEAR_DEPTH | RTARGET_CLEAR_STENCIL
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

struct rtarget_desc
{
    u32             Width;
    u32             Height;
    rtarget_format  Format;
    u32             SampleCount;
    u32             SampleQuality;
    xbool           bBindAsTexture;         // Create shader resource view
    
    rtarget_desc( void ) : Width(0), Height(0), Format(RTARGET_FORMAT_RGBA8), 
                          SampleCount(1), SampleQuality(0), bBindAsTexture(TRUE) {}
};

//------------------------------------------------------------------------------

struct rtarget
{
    ID3D11Texture2D*          pTexture;
    ID3D11RenderTargetView*   pRenderTargetView;
    ID3D11DepthStencilView*   pDepthStencilView;
    ID3D11ShaderResourceView* pShaderResourceView;
    rtarget_desc              Desc;
    xbool                     bIsDepthTarget;
    
    rtarget( void ) : pTexture(NULL), pRenderTargetView(NULL), 
                     pDepthStencilView(NULL), pShaderResourceView(NULL),
                     bIsDepthTarget(FALSE) {}
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

    rtarget_registration( void ) : Policy(RTARGET_SIZE_ABSOLUTE),
                                   BaseWidth(0),
                                   BaseHeight(0),
                                   ScaleX(1.0f),
                                   ScaleY(1.0f),
                                   Format(RTARGET_FORMAT_RGBA8),
                                   SampleCount(1),
                                   SampleQuality(0),
                                   bBindAsTexture(TRUE) {}
};

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

// Initialize render target system
void                rtarget_Init                ( void );
void                rtarget_Kill                ( void );

//==============================================================================
//  RENDER TARGET CREATION
//==============================================================================

// Create render targets
xbool               rtarget_Create              ( rtarget& Target, const rtarget_desc& Desc );
xbool               rtarget_CreateFromTexture   ( rtarget& Target, ID3D11Texture2D* pTexture, rtarget_format Format );
void                rtarget_Destroy             ( rtarget& Target );

// Registration system
xbool               rtarget_Register            ( rtarget& Target, const rtarget_registration& Reg );
// Register and create if needed
xbool               rtarget_GetOrCreate         ( rtarget& Target, const rtarget_registration& Reg );
void                rtarget_Unregister          ( rtarget& Target );
void                rtarget_NotifyResolutionChanged( void );
void                rtarget_ReleaseBackBufferTargets( void );

//==============================================================================
//  RENDER TARGET MANAGEMENT
//==============================================================================

// Set active render targets
xbool               rtarget_SetTargets          ( const rtarget* pTargets, u32 Count, const rtarget* pDepthTarget = NULL );
xbool               rtarget_SetBackBuffer       ( void );

// Access current render targets
const rtarget*      rtarget_GetBackBuffer       ( void );
const rtarget*      rtarget_GetCurrentTarget    ( u32 Index );
u32                 rtarget_GetCurrentCount     ( void );
const rtarget*      rtarget_GetCurrentDepth     ( void );

// Backup/restore system
xbool               rtarget_PushTargets         ( void );
xbool               rtarget_PopTargets          ( void );

//==============================================================================
//  UTILITY FUNCTIONS
//==============================================================================

// Clear functions
void                rtarget_Clear               ( u32 ClearFlags, const f32* pColor = NULL, f32 Depth = 1.0f, u8 Stencil = 0 );
void                rtarget_ClearColor          ( const rtarget& Target, const f32* pColor );
void                rtarget_ClearDepthStencil   ( const rtarget& Target, u32 ClearFlags, f32 Depth = 1.0f, u8 Stencil = 0 );

//==============================================================================
#endif // D3DENG_RTARGET_HPP
//==============================================================================