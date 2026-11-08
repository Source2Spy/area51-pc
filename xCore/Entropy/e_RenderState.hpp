//==============================================================================
//
//  e_RenderState.hpp
//
//  Explicit graphics pipeline state for PSO render backends.
//
//==============================================================================

#ifndef E_RENDERSTATE_HPP
#define E_RENDERSTATE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#include "e_RenderTarget.hpp"
#include "e_Shader.hpp"

//==============================================================================
//  BLEND STATE
//==============================================================================

enum rstate_blend_factor
{
    RSTATE_BLEND_FACTOR_ZERO = 0,
    RSTATE_BLEND_FACTOR_ONE,
    RSTATE_BLEND_FACTOR_SRC_COLOR,
    RSTATE_BLEND_FACTOR_INV_SRC_COLOR,
    RSTATE_BLEND_FACTOR_DEST_COLOR,
    RSTATE_BLEND_FACTOR_INV_DEST_COLOR,
    RSTATE_BLEND_FACTOR_SRC_ALPHA,
    RSTATE_BLEND_FACTOR_INV_SRC_ALPHA,
    RSTATE_BLEND_FACTOR_DEST_ALPHA,
    RSTATE_BLEND_FACTOR_INV_DEST_ALPHA,
    RSTATE_BLEND_FACTOR_BLEND_FACTOR,
    RSTATE_BLEND_FACTOR_INV_BLEND_FACTOR,
    RSTATE_BLEND_FACTOR_SRC_ALPHA_SAT
};

//------------------------------------------------------------------------------

enum rstate_blend_op
{
    RSTATE_BLEND_OP_ADD = 0,
    RSTATE_BLEND_OP_SUBTRACT,
    RSTATE_BLEND_OP_REV_SUBTRACT,
    RSTATE_BLEND_OP_MIN,
    RSTATE_BLEND_OP_MAX
};

//------------------------------------------------------------------------------

enum rstate_color_write_mask
{
    RSTATE_COLOR_WRITE_NONE  = 0,
    RSTATE_COLOR_WRITE_RED   = (1 << 0),
    RSTATE_COLOR_WRITE_GREEN = (1 << 1),
    RSTATE_COLOR_WRITE_BLUE  = (1 << 2),
    RSTATE_COLOR_WRITE_ALPHA = (1 << 3),
    RSTATE_COLOR_WRITE_RGB   = RSTATE_COLOR_WRITE_RED |
                               RSTATE_COLOR_WRITE_GREEN |
                               RSTATE_COLOR_WRITE_BLUE,
    RSTATE_COLOR_WRITE_ALL   = RSTATE_COLOR_WRITE_RGB |
                               RSTATE_COLOR_WRITE_ALPHA
};

//------------------------------------------------------------------------------

enum rstate_blend_preset
{
    RSTATE_BLEND_PRESET_NONE = 0,
    RSTATE_BLEND_PRESET_ALPHA,
    RSTATE_BLEND_PRESET_ADD,
    RSTATE_BLEND_PRESET_SUB,
    RSTATE_BLEND_PRESET_PREMULT_ALPHA,
    RSTATE_BLEND_PRESET_PREMULT_ADD,
    RSTATE_BLEND_PRESET_PREMULT_SUB,
    RSTATE_BLEND_PRESET_MULTIPLY,
    RSTATE_BLEND_PRESET_INTENSITY,
    RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE,
    RSTATE_BLEND_PRESET_NONE_RGB,
    RSTATE_BLEND_PRESET_ALPHA_RGB,
    RSTATE_BLEND_PRESET_PREMULT_ALPHA_RGB,
    RSTATE_BLEND_PRESET_PREMULT_ADD_RGB,
    RSTATE_BLEND_PRESET_PREMULT_SUB_RGB,
    RSTATE_BLEND_PRESET_ADD_RGB,
    RSTATE_BLEND_PRESET_ALPHA_WRITE_ONLY,
    RSTATE_BLEND_PRESET_MAX_RGB,
    RSTATE_BLEND_PRESET_COUNT
};

//------------------------------------------------------------------------------

struct rstate_blend_desc
{
    xbool               bBlendEnable;
    rstate_blend_factor SrcBlend;
    rstate_blend_factor DestBlend;
    rstate_blend_op     BlendOp;
    rstate_blend_factor SrcBlendAlpha;
    rstate_blend_factor DestBlendAlpha;
    rstate_blend_op     BlendOpAlpha;
    u8                  ColorWriteMask;

    rstate_blend_desc( void ) :
        bBlendEnable  ( FALSE ),
        SrcBlend      ( RSTATE_BLEND_FACTOR_ONE ),
        DestBlend     ( RSTATE_BLEND_FACTOR_ZERO ),
        BlendOp       ( RSTATE_BLEND_OP_ADD ),
        SrcBlendAlpha ( RSTATE_BLEND_FACTOR_ONE ),
        DestBlendAlpha( RSTATE_BLEND_FACTOR_ZERO ),
        BlendOpAlpha  ( RSTATE_BLEND_OP_ADD ),
        ColorWriteMask( RSTATE_COLOR_WRITE_ALL )
    {
    }
};

//==============================================================================
//  DEPTH/STENCIL STATE
//==============================================================================

enum rstate_compare_func
{
    RSTATE_COMPARE_NEVER = 0,
    RSTATE_COMPARE_LESS,
    RSTATE_COMPARE_EQUAL,
    RSTATE_COMPARE_LESS_EQUAL,
    RSTATE_COMPARE_GREATER,
    RSTATE_COMPARE_NOT_EQUAL,
    RSTATE_COMPARE_GREATER_EQUAL,
    RSTATE_COMPARE_ALWAYS
};

//------------------------------------------------------------------------------

enum rstate_stencil_op
{
    RSTATE_STENCIL_KEEP = 0,
    RSTATE_STENCIL_ZERO,
    RSTATE_STENCIL_REPLACE,
    RSTATE_STENCIL_INCR_SAT,
    RSTATE_STENCIL_DECR_SAT,
    RSTATE_STENCIL_INVERT,
    RSTATE_STENCIL_INCR,
    RSTATE_STENCIL_DECR
};

//------------------------------------------------------------------------------

enum rstate_depth_preset
{
    RSTATE_DEPTH_PRESET_NORMAL = 0,
    RSTATE_DEPTH_PRESET_NO_WRITE,
    RSTATE_DEPTH_PRESET_WRITE_ALWAYS,
    RSTATE_DEPTH_PRESET_DISABLED,
    RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE,
    RSTATE_DEPTH_PRESET_COUNT
};

//------------------------------------------------------------------------------

struct rstate_stencil_face_desc
{
    rstate_stencil_op   StencilFailOp;
    rstate_stencil_op   StencilDepthFailOp;
    rstate_stencil_op   StencilPassOp;
    rstate_compare_func StencilFunc;

    rstate_stencil_face_desc( void ) :
        StencilFailOp     ( RSTATE_STENCIL_KEEP ),
        StencilDepthFailOp( RSTATE_STENCIL_KEEP ),
        StencilPassOp     ( RSTATE_STENCIL_KEEP ),
        StencilFunc       ( RSTATE_COMPARE_ALWAYS )
    {
    }
};

//------------------------------------------------------------------------------

struct rstate_depth_desc
{
    xbool                    bDepthEnable;
    xbool                    bDepthWrite;
    rstate_compare_func      DepthFunc;
    xbool                    bStencilEnable;
    u8                       StencilReadMask;
    u8                       StencilWriteMask;
    rstate_stencil_face_desc FrontFace;
    rstate_stencil_face_desc BackFace;

    rstate_depth_desc( void ) :
        bDepthEnable   ( TRUE ),
        bDepthWrite    ( TRUE ),
        DepthFunc      ( RSTATE_COMPARE_LESS_EQUAL ),
        bStencilEnable ( FALSE ),
        StencilReadMask( 0xff ),
        StencilWriteMask( 0xff ),
        FrontFace      (),
        BackFace       ()
    {
    }
};

//==============================================================================
//  RASTERIZER STATE
//==============================================================================

enum rstate_fill_mode
{
    RSTATE_FILL_SOLID = 0,
    RSTATE_FILL_WIREFRAME
};

//------------------------------------------------------------------------------

enum rstate_cull_mode
{
    RSTATE_CULL_NONE = 0,
    RSTATE_CULL_FRONT,
    RSTATE_CULL_BACK
};

//------------------------------------------------------------------------------

enum rstate_front_face
{
    RSTATE_FRONT_FACE_CCW = 0,
    RSTATE_FRONT_FACE_CW
};

//------------------------------------------------------------------------------

enum rstate_raster_preset
{
    RSTATE_RASTER_PRESET_SOLID = 0,
    RSTATE_RASTER_PRESET_WIRE,
    RSTATE_RASTER_PRESET_SOLID_NO_CULL,
    RSTATE_RASTER_PRESET_WIRE_NO_CULL,
    RSTATE_RASTER_PRESET_DEPTH_BIASED,
    RSTATE_RASTER_PRESET_COUNT
};

//------------------------------------------------------------------------------

struct rstate_raster_desc
{
    rstate_fill_mode  FillMode;
    rstate_cull_mode  CullMode;
    rstate_front_face FrontFace;
    f32               DepthBias;
    f32               DepthBiasClamp;
    f32               SlopeScaledDepthBias;
    xbool             bDepthBiasEnable;
    xbool             bDepthClipEnable;

    rstate_raster_desc( void ) :
        FillMode             ( RSTATE_FILL_SOLID ),
        CullMode             ( RSTATE_CULL_FRONT ),
        FrontFace            ( RSTATE_FRONT_FACE_CW ),
        DepthBias            ( 0.0f ),
        DepthBiasClamp       ( 0.0f ),
        SlopeScaledDepthBias ( 0.0f ),
        bDepthBiasEnable     ( FALSE ),
        bDepthClipEnable     ( TRUE )
    {
    }
};

//==============================================================================
//  SAMPLER STATE
//==============================================================================

enum rstate_filter
{
    RSTATE_FILTER_POINT = 0,
    RSTATE_FILTER_LINEAR,
    RSTATE_FILTER_ANISOTROPIC,
    RSTATE_FILTER_COMPARISON_POINT,
    RSTATE_FILTER_COMPARISON_LINEAR,
    RSTATE_FILTER_COMPARISON_ANISOTROPIC,
    RSTATE_FILTER_BILINEAR
};

//------------------------------------------------------------------------------

enum rstate_texture_filtering
{
    RSTATE_TEXTURE_FILTER_BILINEAR = 0,
    RSTATE_TEXTURE_FILTER_TRILINEAR,
    RSTATE_TEXTURE_FILTER_ANISOTROPIC_2,
    RSTATE_TEXTURE_FILTER_ANISOTROPIC_4,
    RSTATE_TEXTURE_FILTER_ANISOTROPIC_8,
    RSTATE_TEXTURE_FILTER_ANISOTROPIC_16
};

//------------------------------------------------------------------------------

enum rstate_texture_address
{
    RSTATE_TEXTURE_ADDRESS_WRAP = 0,
    RSTATE_TEXTURE_ADDRESS_MIRROR,
    RSTATE_TEXTURE_ADDRESS_CLAMP,
    RSTATE_TEXTURE_ADDRESS_BORDER
};

//------------------------------------------------------------------------------

enum rstate_sampler_preset
{
    RSTATE_SAMPLER_PRESET_LINEAR_WRAP = 0,
    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP,
    RSTATE_SAMPLER_PRESET_LINEAR_WRAP_U_CLAMP_V,
    RSTATE_SAMPLER_PRESET_LINEAR_CLAMP_U_WRAP_V,
    RSTATE_SAMPLER_PRESET_POINT_WRAP,
    RSTATE_SAMPLER_PRESET_POINT_CLAMP,
    RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP,
    RSTATE_SAMPLER_PRESET_ANISOTROPIC_CLAMP,
    RSTATE_SAMPLER_PRESET_MATERIAL_WRAP,
    RSTATE_SAMPLER_PRESET_MATERIAL_CLAMP,
    RSTATE_SAMPLER_PRESET_COUNT
};

//------------------------------------------------------------------------------

struct rstate_sampler_desc
{
    rstate_filter          Filter;
    rstate_texture_address AddressU;
    rstate_texture_address AddressV;
    rstate_texture_address AddressW;
    f32                    MipLODBias;
    u32                    MaxAnisotropy;
    rstate_compare_func    ComparisonFunc;
    f32                    MinLOD;
    f32                    MaxLOD;
    const char*            pDebugName;
    xbool                  UseTextureFiltering;

    rstate_sampler_desc( void ) :
        Filter        ( RSTATE_FILTER_LINEAR ),
        AddressU      ( RSTATE_TEXTURE_ADDRESS_WRAP ),
        AddressV      ( RSTATE_TEXTURE_ADDRESS_WRAP ),
        AddressW      ( RSTATE_TEXTURE_ADDRESS_WRAP ),
        MipLODBias    ( 0.0f ),
        MaxAnisotropy ( 1 ),
        ComparisonFunc( RSTATE_COMPARE_ALWAYS ),
        MinLOD        ( 0.0f ),
        MaxLOD        ( 3.402823466e+38F ),
        pDebugName    ( NULL ),
        UseTextureFiltering( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct rstate_sampler_backend;

//------------------------------------------------------------------------------

struct rstate_sampler
{
    rstate_sampler_backend* pBackend;

    rstate_sampler( void ) : pBackend( NULL ) {}
    operator xbool( void ) const { return pBackend != NULL; }
};

//==============================================================================
//  GRAPHICS PIPELINE STATE
//==============================================================================

struct render_pipeline_backend;

//------------------------------------------------------------------------------

struct render_color_target_desc
{
    rtarget_format    Format;
    rstate_blend_desc Blend;

    render_color_target_desc( void ) :
        Format( RTARGET_FORMAT_COUNT ),
        Blend ()
    {
    }
};

//------------------------------------------------------------------------------

struct render_pipeline_desc
{
    shader_pipeline_desc     Shader;
    rstate_raster_desc       Raster;
    rstate_depth_desc        Depth;
    render_color_target_desc ColorTargets[RTARGET_MAX_TARGETS];
    u32                      ColorCount;
    rtarget_format           DepthFormat;
    u32                      SampleCount;
    u32                      SampleMask;
    xbool                    bAlphaToCoverage;
    const char*              pDebugName;

    render_pipeline_desc( void ) :
        Shader          (),
        Raster          (),
        Depth           (),
        ColorCount      ( 0 ),
        DepthFormat     ( RTARGET_FORMAT_COUNT ),
        SampleCount     ( 1 ),
        SampleMask      ( 0xffffffff ),
        bAlphaToCoverage( FALSE ),
        pDebugName      ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct render_pipeline
{
    render_pipeline_backend* pBackend;

    render_pipeline( void ) : pBackend( NULL ) {}
    operator xbool( void ) const { return pBackend != NULL; }
};

//------------------------------------------------------------------------------

struct render_pipeline_create_backend;

//------------------------------------------------------------------------------

struct render_pipeline_create_request
{
    render_pipeline_create_backend* pBackend;

    render_pipeline_create_request( void ) : pBackend( NULL ) {}
};

//------------------------------------------------------------------------------

enum render_pipeline_create_status
{
    RENDER_PIPELINE_CREATE_PENDING,
    RENDER_PIPELINE_CREATE_READY,
    RENDER_PIPELINE_CREATE_FAILED,
    RENDER_PIPELINE_CREATE_CANCELLED
};

//==============================================================================
//  PRESET DESCRIPTORS
//==============================================================================

void                        rstate_Init               ( void );
void                        rstate_Kill               ( void );

const rstate_blend_desc&    rstate_GetBlendDesc       ( rstate_blend_preset   Preset );
const rstate_depth_desc&    rstate_GetDepthDesc       ( rstate_depth_preset   Preset );
const rstate_raster_desc&   rstate_GetRasterDesc      ( rstate_raster_preset  Preset );
const rstate_sampler_desc&  rstate_GetSamplerDesc     ( rstate_sampler_preset Preset );
xbool                      rstate_SetTextureFiltering( rstate_texture_filtering Filtering );
rstate_texture_filtering   rstate_GetTextureFiltering( void );

//==============================================================================
//  PIPELINE AND SAMPLER OBJECTS
//==============================================================================

xbool                       render_CreatePipeline     ( render_pipeline& Pipeline,
                                                        const render_pipeline_desc& Desc );
void                        render_DestroyPipeline    ( render_pipeline& Pipeline );
xbool                       render_BeginPipelineCreate( render_pipeline_create_request& Request,
                                                        const render_pipeline_desc& Desc );
render_pipeline_create_status render_PollPipelineCreate( render_pipeline_create_request& Request,
                                                         render_pipeline* pPipeline );
xbool                       render_FinishPipelineCreate( render_pipeline_create_request& Request,
                                                          render_pipeline& Pipeline );
void                        render_CancelPipelineCreate( render_pipeline_create_request& Request );
void                        render_ReleasePipelineCreate( render_pipeline_create_request& Request );
void                        render_TrimPipelineCache  ( void );
u64                         render_GetPipelineSerial  ( const render_pipeline& Pipeline );
u32                         render_GetPipelineCacheCount( void );
xbool                       render_BindPipeline       ( const render_pipeline& Pipeline );

xbool                       rstate_CreateSampler      ( rstate_sampler& Sampler,
                                                        const rstate_sampler_desc& Desc );
xbool                       rstate_CreateSampler      ( rstate_sampler&        Sampler,
                                                        rstate_sampler_preset  Preset,
                                                        const char*            pDebugName = NULL );
void                        rstate_DestroySampler     ( rstate_sampler& Sampler );

//==============================================================================
//  DYNAMIC PASS STATE
//==============================================================================

xbool                       render_SetBlendConstants  ( const f32 Color[4] );
xbool                       render_SetStencilRef      ( u8 StencilRef );

//==============================================================================
#endif // E_RENDERSTATE_HPP
//==============================================================================
