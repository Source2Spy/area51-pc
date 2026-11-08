//==============================================================================
//
//  sdleng_state.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"
#include "e_RenderPipelineKey.hpp"
#include "x_workers.hpp"
#include "x_threads.hpp"
#include "x_atomic.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    SDLENG_MAX_VERTEX_BUFFERS      = 16,
    SDLENG_MAX_VERTEX_ATTRIBUTES   = 16,
    SDLENG_MAX_PIPELINE_CACHE      = 256,
    SDLENG_PIPELINE_HASH_BUCKETS   = 512,
    SDLENG_PIPELINE_CREATE_WORKERS = 8
};

//==============================================================================
//  TYPES
//==============================================================================

struct sdlstate_pipeline_key
{
    u64                            VertexShaderSerial;
    u64                            PixelShaderSerial;
    SDL_GPUVertexBufferDescription VertexBuffers[SDLENG_MAX_VERTEX_BUFFERS];
    u32                            VertexBufferCount;
    SDL_GPUVertexAttribute         VertexAttributes[SDLENG_MAX_VERTEX_ATTRIBUTES];
    u32                            VertexAttributeCount;
    SDL_GPUPrimitiveType           PrimitiveType;
    SDL_GPURasterizerState         RasterizerState;
    SDL_GPUMultisampleState        MultisampleState;
    SDL_GPUDepthStencilState       DepthStencilState;
    SDL_GPUColorTargetDescription  ColorTargets[RTARGET_MAX_TARGETS];
    u32                            ColorTargetCount;
    SDL_GPUTextureFormat           DepthStencilFormat;
    xbool                          bHasDepthStencilTarget;

    sdlstate_pipeline_key( void ) :
        VertexShaderSerial    ( 0 ),
        PixelShaderSerial     ( 0 ),
        VertexBufferCount     ( 0 ),
        VertexAttributeCount  ( 0 ),
        PrimitiveType         ( SDL_GPU_PRIMITIVETYPE_TRIANGLELIST ),
        RasterizerState       (),
        MultisampleState      (),
        DepthStencilState     (),
        ColorTargetCount      ( 0 ),
        DepthStencilFormat    ( SDL_GPU_TEXTUREFORMAT_INVALID ),
        bHasDepthStencilTarget( FALSE )
    {
        x_memset( VertexBuffers,      0, sizeof( VertexBuffers ) );
        x_memset( VertexAttributes,   0, sizeof( VertexAttributes ) );
        x_memset( &RasterizerState,   0, sizeof( RasterizerState ) );
        x_memset( &MultisampleState,  0, sizeof( MultisampleState ) );
        x_memset( &DepthStencilState, 0, sizeof( DepthStencilState ) );
        x_memset( ColorTargets,       0, sizeof( ColorTargets ) );
    }
};

//------------------------------------------------------------------------------

struct render_pipeline_cache_entry
{
    xref<shader_native>          VertexNative;
    xref<shader_native>          PixelNative;
    SDL_GPUGraphicsPipeline*     pPipeline;
    sdlstate_pipeline_key        Key;
    u32                          HandleCount;
    u32                          PendingCount;
    u64                          Serial;
    u64                          LastUse;
    u64                          Hash;
    render_pipeline_cache_entry* pHashNext;
    render_pipeline_cache_entry* pNext;

    render_pipeline_cache_entry( void ) :
        pPipeline   ( NULL ),
        Key         (),
        HandleCount ( 0 ),
        PendingCount( 0 ),
        Serial      ( 0 ),
        LastUse     ( 0 ),
        Hash        ( 0 ),
        pHashNext   ( NULL ),
        pNext       ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct sdlstate_pipeline_work
{
    xref<shader_native>               VertexNative;
    xref<shader_native>               PixelNative;
    sdlstate_pipeline_key             Key;
    SDL_GPUGraphicsPipelineCreateInfo CreateInfo;
    SDL_GPUDevice*                    pDevice;
    SDL_GPUGraphicsPipeline*          pPipeline;
    render_pipeline_cache_entry*      pEntry;
    shader_resource_counts            VertexResources;
    shader_resource_counts            PixelResources;
    x_atomic_s32                      Completed;
    x_atomic_s32                      Cancelled;
    sdlstate_pipeline_work*           pNextWork;
    sdlstate_pipeline_work*           pPrevWork;
    xbool                             Queued;
    u64                               Hash;
    u64                               VertexProgramId;
    u64                               PixelProgramId;
    xtick                             Ticks;
    char*                             pDebugName;
    char                              Error[512];
    u32                               ConsumerCount;
    sdlstate_pipeline_work*           pHashNext;
    sdlstate_pipeline_work*           pNext;

    sdlstate_pipeline_work( void ) :
        Key            (),
        CreateInfo     (),
        pDevice        ( NULL ),
        pPipeline      ( NULL ),
        pEntry         ( NULL ),
        VertexResources(),
        PixelResources (),
        pNextWork      ( NULL ),
        pPrevWork      ( NULL ),
        Queued         ( FALSE ),
        Hash           ( 0 ),
        VertexProgramId( 0 ),
        PixelProgramId ( 0 ),
        Ticks          ( 0 ),
        pDebugName     ( NULL ),
        ConsumerCount  ( 0 ),
        pHashNext      ( NULL ),
        pNext          ( NULL )
    {
        x_AtomicInit( &Completed, 0 );
        x_AtomicInit( &Cancelled, 0 );
        Error[0] = 0;
    }
};

//------------------------------------------------------------------------------

struct render_pipeline_create_backend
{
    sdlstate_pipeline_work*         pWork;
    render_pipeline_create_request* pOwner;
    render_pipeline_create_backend* pNext;
    xbool                           Cancelled;
    xstring                         DebugName;

    render_pipeline_create_backend( void ) :
        pWork    ( NULL ),
        pOwner   ( NULL ),
        pNext    ( NULL ),
        Cancelled( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct sdlstate_pipeline_worker
{
    xhandle Job;
    xbool   Running;

    sdlstate_pipeline_worker( void ) :
        Job    ( HNULL ),
        Running( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct rstate_sampler_native
{
    SDL_GPUSampler*          pSampler;
    SDL_GPUSamplerCreateInfo Key;
    u32                      References;
    rstate_sampler_native*   pNext;
};

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static render_pipeline_create_backend* s_pPipelineRequests                                 = NULL;
static sdlstate_pipeline_work*         s_pRetiredPipelineWork                              = NULL;
static sdlstate_pipeline_work*         s_PipelineTaskBuckets[SDLENG_PIPELINE_HASH_BUCKETS] = {};
static sdlstate_pipeline_work*         s_pPipelineWork                                     = NULL;
static sdlstate_pipeline_work*         s_pPipelineWorkTail                                 = NULL;
static xmutex*                         s_pPipelineWorkMutex                                = NULL;
static SDL_Mutex*                      s_pPipelineCompletionMutex                          = NULL;
static SDL_Condition*                  s_pPipelineCompletion                               = NULL;

//------------------------------------------------------------------------------

static sdlstate_pipeline_worker s_PipelineWorkers[SDLENG_PIPELINE_CREATE_WORKERS];

//------------------------------------------------------------------------------

static rstate_blend_desc   s_BlendPresets[RSTATE_BLEND_PRESET_COUNT];
static rstate_depth_desc   s_DepthPresets[RSTATE_DEPTH_PRESET_COUNT];
static rstate_raster_desc  s_RasterPresets[RSTATE_RASTER_PRESET_COUNT];
static rstate_sampler_desc s_SamplerPresets[RSTATE_SAMPLER_PRESET_COUNT];
static xbool               s_bPresetsBuilt = FALSE;

static render_pipeline_backend*           s_pPipelineHandleList                           = NULL;
static render_pipeline_cache_entry*       s_pPipelineCache                                = NULL;
static render_pipeline_cache_entry*       s_PipelineBuckets[SDLENG_PIPELINE_HASH_BUCKETS] = {};
static u32                                s_PipelineHandleCount                           = 0;
static u32                                s_PipelineCount                                 = 0;
static x_atomic_u64                       s_PipelineCreateCalls                           = X_ATOMIC_U64_INIT( 0 );
static u64                                s_NextPipelineSerial                            = 0;
static u64                                s_NextPipelineUse                               = 0;
static const render_pipeline_cache_entry* s_pBoundPipeline                                = NULL;

//------------------------------------------------------------------------------

static rstate_sampler_native* s_pNativeSamplers = NULL;

static rstate_sampler_backend*  s_pSamplerList     = NULL;
static rstate_texture_filtering s_TextureFiltering = RSTATE_TEXTURE_FILTER_ANISOTROPIC_16;
static u32                      s_SamplerCount     = 0;

//==============================================================================
//  HELPERS
//==============================================================================

static
SDL_GPUBlendFactor sdlstate_ToSDLBlendFactor( rstate_blend_factor Factor )
{
    switch( Factor )
    {
        case RSTATE_BLEND_FACTOR_ZERO:             return SDL_GPU_BLENDFACTOR_ZERO;                     break;
        case RSTATE_BLEND_FACTOR_ONE:              return SDL_GPU_BLENDFACTOR_ONE;                      break;
        case RSTATE_BLEND_FACTOR_SRC_COLOR:        return SDL_GPU_BLENDFACTOR_SRC_COLOR;                break;
        case RSTATE_BLEND_FACTOR_INV_SRC_COLOR:    return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;      break;
        case RSTATE_BLEND_FACTOR_DEST_COLOR:       return SDL_GPU_BLENDFACTOR_DST_COLOR;                break;
        case RSTATE_BLEND_FACTOR_INV_DEST_COLOR:   return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;      break;
        case RSTATE_BLEND_FACTOR_SRC_ALPHA:        return SDL_GPU_BLENDFACTOR_SRC_ALPHA;                break;
        case RSTATE_BLEND_FACTOR_INV_SRC_ALPHA:    return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;      break;
        case RSTATE_BLEND_FACTOR_DEST_ALPHA:       return SDL_GPU_BLENDFACTOR_DST_ALPHA;                break;
        case RSTATE_BLEND_FACTOR_INV_DEST_ALPHA:   return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;      break;
        case RSTATE_BLEND_FACTOR_BLEND_FACTOR:     return SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;           break;
        case RSTATE_BLEND_FACTOR_INV_BLEND_FACTOR: return SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR; break;
        case RSTATE_BLEND_FACTOR_SRC_ALPHA_SAT:    return SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE;       break;
        default:                                   return SDL_GPU_BLENDFACTOR_ONE;                      break;
    }
}

//==============================================================================

static
SDL_GPUBlendOp sdlstate_ToSDLBlendOp( rstate_blend_op Op )
{
    switch( Op )
    {
        case RSTATE_BLEND_OP_ADD:          return SDL_GPU_BLENDOP_ADD;              break;
        case RSTATE_BLEND_OP_SUBTRACT:     return SDL_GPU_BLENDOP_SUBTRACT;         break;
        case RSTATE_BLEND_OP_REV_SUBTRACT: return SDL_GPU_BLENDOP_REVERSE_SUBTRACT; break;
        case RSTATE_BLEND_OP_MIN:          return SDL_GPU_BLENDOP_MIN;              break;
        case RSTATE_BLEND_OP_MAX:          return SDL_GPU_BLENDOP_MAX;              break;
        default:                           return SDL_GPU_BLENDOP_ADD;              break;
    }
}

//==============================================================================

static
SDL_GPUColorComponentFlags sdlstate_ToSDLColorMask( u8 Mask )
{
    SDL_GPUColorComponentFlags SDLMask = 0;

    if( Mask & RSTATE_COLOR_WRITE_RED )
    {
        SDLMask |= SDL_GPU_COLORCOMPONENT_R;
    }

    if( Mask & RSTATE_COLOR_WRITE_GREEN )
    {
        SDLMask |= SDL_GPU_COLORCOMPONENT_G;
    }

    if( Mask & RSTATE_COLOR_WRITE_BLUE )
    {
        SDLMask |= SDL_GPU_COLORCOMPONENT_B;
    }

    if( Mask & RSTATE_COLOR_WRITE_ALPHA )
    {
        SDLMask |= SDL_GPU_COLORCOMPONENT_A;
    }

    return SDLMask;
}

//==============================================================================

static
SDL_GPUCompareOp sdlstate_ToSDLCompareOp( rstate_compare_func Func )
{
    switch( Func )
    {
        case RSTATE_COMPARE_NEVER:         return SDL_GPU_COMPAREOP_NEVER;            break;
        case RSTATE_COMPARE_LESS:          return SDL_GPU_COMPAREOP_LESS;             break;
        case RSTATE_COMPARE_EQUAL:         return SDL_GPU_COMPAREOP_EQUAL;            break;
        case RSTATE_COMPARE_LESS_EQUAL:    return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;    break;
        case RSTATE_COMPARE_GREATER:       return SDL_GPU_COMPAREOP_GREATER;          break;
        case RSTATE_COMPARE_NOT_EQUAL:     return SDL_GPU_COMPAREOP_NOT_EQUAL;        break;
        case RSTATE_COMPARE_GREATER_EQUAL: return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL; break;
        case RSTATE_COMPARE_ALWAYS:        return SDL_GPU_COMPAREOP_ALWAYS;           break;
        default:                           return SDL_GPU_COMPAREOP_ALWAYS;           break;
    }
}

//==============================================================================

static
SDL_GPUStencilOp sdlstate_ToSDLStencilOp( rstate_stencil_op Op )
{
    switch( Op )
    {
        case RSTATE_STENCIL_KEEP:     return SDL_GPU_STENCILOP_KEEP;                break;
        case RSTATE_STENCIL_ZERO:     return SDL_GPU_STENCILOP_ZERO;                break;
        case RSTATE_STENCIL_REPLACE:  return SDL_GPU_STENCILOP_REPLACE;             break;
        case RSTATE_STENCIL_INCR_SAT: return SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP; break;
        case RSTATE_STENCIL_DECR_SAT: return SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP; break;
        case RSTATE_STENCIL_INVERT:   return SDL_GPU_STENCILOP_INVERT;              break;
        case RSTATE_STENCIL_INCR:     return SDL_GPU_STENCILOP_INCREMENT_AND_WRAP;  break;
        case RSTATE_STENCIL_DECR:     return SDL_GPU_STENCILOP_DECREMENT_AND_WRAP;  break;
        default:                      return SDL_GPU_STENCILOP_KEEP;                break;
    }
}

//==============================================================================

static
SDL_GPUFillMode sdlstate_ToSDLFillMode( rstate_fill_mode Mode )
{
    switch( Mode )
    {
        case RSTATE_FILL_SOLID:     return SDL_GPU_FILLMODE_FILL; break;
        case RSTATE_FILL_WIREFRAME: return SDL_GPU_FILLMODE_LINE; break;
        default:                    return SDL_GPU_FILLMODE_FILL; break;
    }
}

//==============================================================================

static
SDL_GPUCullMode sdlstate_ToSDLCullMode( rstate_cull_mode Mode )
{
    switch( Mode )
    {
        case RSTATE_CULL_NONE:  return SDL_GPU_CULLMODE_NONE;  break;
        case RSTATE_CULL_FRONT: return SDL_GPU_CULLMODE_FRONT; break;
        case RSTATE_CULL_BACK:  return SDL_GPU_CULLMODE_BACK;  break;
        default:                return SDL_GPU_CULLMODE_NONE;  break;
    }
}

//==============================================================================

static
SDL_GPUFrontFace sdlstate_ToSDLFrontFace( rstate_front_face Face )
{
    switch( Face )
    {
        case RSTATE_FRONT_FACE_CCW: return SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE; break;
        case RSTATE_FRONT_FACE_CW:  return SDL_GPU_FRONTFACE_CLOCKWISE;         break;
        default:                    return SDL_GPU_FRONTFACE_CLOCKWISE;         break;
    }
}

//==============================================================================

static
SDL_GPUPrimitiveType sdlstate_ToSDLPrimitiveType( shader_topology Topology )
{
    switch( Topology )
    {
        case SHADER_TOPOLOGY_POINT_LIST:     return SDL_GPU_PRIMITIVETYPE_POINTLIST;     break;
        case SHADER_TOPOLOGY_LINE_LIST:      return SDL_GPU_PRIMITIVETYPE_LINELIST;      break;
        case SHADER_TOPOLOGY_LINE_STRIP:     return SDL_GPU_PRIMITIVETYPE_LINESTRIP;     break;
        case SHADER_TOPOLOGY_TRIANGLE_LIST:  return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;  break;
        case SHADER_TOPOLOGY_TRIANGLE_STRIP: return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP; break;
        default:                             return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;  break;
    }
}

//==============================================================================

static
SDL_GPUVertexElementFormat sdlstate_ToSDLVertexFormat( shader_vertex_format Format )
{
    switch( Format )
    {
        case SHADER_VERTEX_FORMAT_FLOAT1:       return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;       break;
        case SHADER_VERTEX_FORMAT_FLOAT2:       return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;      break;
        case SHADER_VERTEX_FORMAT_FLOAT3:       return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;      break;
        case SHADER_VERTEX_FORMAT_FLOAT4:       return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;      break;
        case SHADER_VERTEX_FORMAT_UINT1:        return SDL_GPU_VERTEXELEMENTFORMAT_UINT;        break;
        case SHADER_VERTEX_FORMAT_UBYTE4N_BGRA: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM; break;
        default:                                return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;     break;
    }
}

//==============================================================================

static
u32 sdlstate_GetVertexFormatSize( shader_vertex_format Format )
{
    switch( Format )
    {
        case SHADER_VERTEX_FORMAT_FLOAT1:       return sizeof( f32 );     break;
        case SHADER_VERTEX_FORMAT_FLOAT2:       return sizeof( f32 ) * 2; break;
        case SHADER_VERTEX_FORMAT_FLOAT3:       return sizeof( f32 ) * 3; break;
        case SHADER_VERTEX_FORMAT_FLOAT4:       return sizeof( f32 ) * 4; break;
        case SHADER_VERTEX_FORMAT_UINT1:        return sizeof( u32 );     break;
        case SHADER_VERTEX_FORMAT_UBYTE4N_BGRA: return sizeof( u8 ) * 4;  break;
        default:                                return 0;                 break;
    }
}

//==============================================================================

static
xbool sdlstate_ToSDLSamplerFilter( rstate_filter             Filter,
                                   SDL_GPUFilter&            MinFilter,
                                   SDL_GPUFilter&            MagFilter,
                                   SDL_GPUSamplerMipmapMode& MipmapMode,
                                   xbool&                    bAnisotropy,
                                   xbool&                    bCompare )
{
    bAnisotropy = FALSE;
    bCompare    = FALSE;

    switch( Filter )
    {
        case RSTATE_FILTER_POINT:
        {
            MinFilter  = SDL_GPU_FILTER_NEAREST;
            MagFilter  = SDL_GPU_FILTER_NEAREST;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            return TRUE;
        }
        break;

        case RSTATE_FILTER_BILINEAR:
        {
            MinFilter  = SDL_GPU_FILTER_LINEAR;
            MagFilter  = SDL_GPU_FILTER_LINEAR;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            return TRUE;
        }
        break;

        case RSTATE_FILTER_LINEAR:
        {
            MinFilter  = SDL_GPU_FILTER_LINEAR;
            MagFilter  = SDL_GPU_FILTER_LINEAR;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            return TRUE;
        }
        break;

        case RSTATE_FILTER_ANISOTROPIC:
        {
            MinFilter   = SDL_GPU_FILTER_LINEAR;
            MagFilter   = SDL_GPU_FILTER_LINEAR;
            MipmapMode  = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            bAnisotropy = TRUE;
            return TRUE;
        }
        break;

        case RSTATE_FILTER_COMPARISON_POINT:
        {
            MinFilter  = SDL_GPU_FILTER_NEAREST;
            MagFilter  = SDL_GPU_FILTER_NEAREST;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            bCompare   = TRUE;
            return TRUE;
        }
        break;

        case RSTATE_FILTER_COMPARISON_LINEAR:
        {
            MinFilter  = SDL_GPU_FILTER_LINEAR;
            MagFilter  = SDL_GPU_FILTER_LINEAR;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            bCompare   = TRUE;
            return TRUE;
        }
        break;

        case RSTATE_FILTER_COMPARISON_ANISOTROPIC:
        {
            MinFilter   = SDL_GPU_FILTER_LINEAR;
            MagFilter   = SDL_GPU_FILTER_LINEAR;
            MipmapMode  = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            bAnisotropy = TRUE;
            bCompare    = TRUE;
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
xbool sdlstate_ToSDLAddressMode( rstate_texture_address Address, SDL_GPUSamplerAddressMode& SDLAddress )
{
    switch( Address )
    {
        case RSTATE_TEXTURE_ADDRESS_WRAP:
        {
            SDLAddress = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            return TRUE;
        }
        break;

        case RSTATE_TEXTURE_ADDRESS_MIRROR:
        {
            SDLAddress = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            return TRUE;
        }
        break;

        case RSTATE_TEXTURE_ADDRESS_CLAMP:
        {
            SDLAddress = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            return TRUE;
        }
        break;

        case RSTATE_TEXTURE_ADDRESS_BORDER:
        {
            x_DebugMsg( "SDLState: border sampler addressing is not supported by SDL GPU\n" );
            return FALSE;
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
void sdlstate_FillStencilFace( SDL_GPUStencilOpState& Dst, const rstate_stencil_face_desc& Src )
{
    Dst.fail_op       = sdlstate_ToSDLStencilOp( Src.StencilFailOp );
    Dst.pass_op       = sdlstate_ToSDLStencilOp( Src.StencilPassOp );
    Dst.depth_fail_op = sdlstate_ToSDLStencilOp( Src.StencilDepthFailOp );
    Dst.compare_op    = sdlstate_ToSDLCompareOp( Src.StencilFunc );
}

//==============================================================================

static
void sdlstate_BuildPresets( void )
{
    if( s_bPresetsBuilt )
    {
        return;
    }

    rstate_blend_desc Blend;
    s_BlendPresets[RSTATE_BLEND_PRESET_NONE]             = Blend;
    Blend.ColorWriteMask                                 = RSTATE_COLOR_WRITE_ALPHA;
    s_BlendPresets[RSTATE_BLEND_PRESET_ALPHA_WRITE_ONLY] = Blend;

    Blend                                     = rstate_blend_desc();
    Blend.bBlendEnable                        = TRUE;
    Blend.SrcBlend                            = RSTATE_BLEND_FACTOR_SRC_ALPHA;
    Blend.DestBlend                           = RSTATE_BLEND_FACTOR_INV_SRC_ALPHA;
    s_BlendPresets[RSTATE_BLEND_PRESET_ALPHA] = Blend;

    Blend                                   = rstate_blend_desc();
    Blend.bBlendEnable                      = TRUE;
    Blend.SrcBlend                          = RSTATE_BLEND_FACTOR_SRC_ALPHA;
    Blend.DestBlend                         = RSTATE_BLEND_FACTOR_ONE;
    Blend.SrcBlendAlpha                     = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha                    = RSTATE_BLEND_FACTOR_ONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_ADD] = Blend;

    Blend                                   = rstate_blend_desc();
    Blend.bBlendEnable                      = TRUE;
    Blend.SrcBlend                          = RSTATE_BLEND_FACTOR_SRC_ALPHA;
    Blend.DestBlend                         = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOp                           = RSTATE_BLEND_OP_REV_SUBTRACT;
    Blend.SrcBlendAlpha                     = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha                    = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOpAlpha                      = RSTATE_BLEND_OP_REV_SUBTRACT;
    s_BlendPresets[RSTATE_BLEND_PRESET_SUB] = Blend;

    Blend                                             = rstate_blend_desc();
    Blend.bBlendEnable                                = TRUE;
    Blend.SrcBlend                                    = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlend                                   = RSTATE_BLEND_FACTOR_INV_SRC_ALPHA;
    Blend.SrcBlendAlpha                               = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha                              = RSTATE_BLEND_FACTOR_INV_SRC_ALPHA;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ALPHA] = Blend;

    Blend                                           = rstate_blend_desc();
    Blend.bBlendEnable                              = TRUE;
    Blend.SrcBlend                                  = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlend                                 = RSTATE_BLEND_FACTOR_ONE;
    Blend.SrcBlendAlpha                             = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha                            = RSTATE_BLEND_FACTOR_ONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ADD] = Blend;

    Blend                                           = rstate_blend_desc();
    Blend.bBlendEnable                              = TRUE;
    Blend.SrcBlend                                  = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlend                                 = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOp                                   = RSTATE_BLEND_OP_REV_SUBTRACT;
    Blend.SrcBlendAlpha                             = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha                            = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOpAlpha                              = RSTATE_BLEND_OP_REV_SUBTRACT;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_SUB] = Blend;

    Blend                                        = rstate_blend_desc();
    Blend.bBlendEnable                           = TRUE;
    Blend.SrcBlend                               = RSTATE_BLEND_FACTOR_DEST_COLOR;
    Blend.DestBlend                              = RSTATE_BLEND_FACTOR_ZERO;
    Blend.SrcBlendAlpha                          = RSTATE_BLEND_FACTOR_DEST_ALPHA;
    Blend.DestBlendAlpha                         = RSTATE_BLEND_FACTOR_ZERO;
    s_BlendPresets[RSTATE_BLEND_PRESET_MULTIPLY] = Blend;

    Blend                                         = rstate_blend_desc();
    Blend.bBlendEnable                            = TRUE;
    Blend.SrcBlend                                = RSTATE_BLEND_FACTOR_DEST_COLOR;
    Blend.DestBlend                               = RSTATE_BLEND_FACTOR_SRC_COLOR;
    Blend.SrcBlendAlpha                           = RSTATE_BLEND_FACTOR_ZERO;
    Blend.DestBlendAlpha                          = RSTATE_BLEND_FACTOR_ONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_INTENSITY] = Blend;

    Blend                                                   = rstate_blend_desc();
    Blend.ColorWriteMask                                    = RSTATE_COLOR_WRITE_NONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE] = Blend;

    Blend                                        = s_BlendPresets[RSTATE_BLEND_PRESET_NONE];
    Blend.ColorWriteMask                         = RSTATE_COLOR_WRITE_RGB;
    s_BlendPresets[RSTATE_BLEND_PRESET_NONE_RGB] = Blend;

    Blend                                         = s_BlendPresets[RSTATE_BLEND_PRESET_ALPHA];
    Blend.ColorWriteMask                          = RSTATE_COLOR_WRITE_RGB;
    s_BlendPresets[RSTATE_BLEND_PRESET_ALPHA_RGB] = Blend;

    Blend                                                 = s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ALPHA];
    Blend.ColorWriteMask                                  = RSTATE_COLOR_WRITE_RGB;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ALPHA_RGB] = Blend;

    Blend                                               = s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ADD];
    Blend.ColorWriteMask                                = RSTATE_COLOR_WRITE_RGB;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ADD_RGB] = Blend;

    Blend                                               = s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_SUB];
    Blend.ColorWriteMask                                = RSTATE_COLOR_WRITE_RGB;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_SUB_RGB] = Blend;

    Blend                                       = s_BlendPresets[RSTATE_BLEND_PRESET_ADD];
    Blend.ColorWriteMask                        = RSTATE_COLOR_WRITE_RGB;
    s_BlendPresets[RSTATE_BLEND_PRESET_ADD_RGB] = Blend;

    Blend                                       = rstate_blend_desc();
    Blend.bBlendEnable                          = TRUE;
    Blend.SrcBlend                              = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlend                             = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOp                               = RSTATE_BLEND_OP_MAX;
    Blend.ColorWriteMask                        = RSTATE_COLOR_WRITE_RGB;
    s_BlendPresets[RSTATE_BLEND_PRESET_MAX_RGB] = Blend;

    rstate_depth_desc Depth;
    s_DepthPresets[RSTATE_DEPTH_PRESET_NORMAL] = Depth;

    Depth                                        = rstate_depth_desc();
    Depth.bDepthWrite                            = FALSE;
    s_DepthPresets[RSTATE_DEPTH_PRESET_NO_WRITE] = Depth;

    Depth                                            = rstate_depth_desc();
    Depth.DepthFunc                                  = RSTATE_COMPARE_ALWAYS;
    s_DepthPresets[RSTATE_DEPTH_PRESET_WRITE_ALWAYS] = Depth;

    Depth                                        = rstate_depth_desc();
    Depth.bDepthEnable                           = FALSE;
    s_DepthPresets[RSTATE_DEPTH_PRESET_DISABLED] = Depth;

    Depth                                                 = rstate_depth_desc();
    Depth.bDepthEnable                                    = FALSE;
    Depth.bDepthWrite                                     = FALSE;
    s_DepthPresets[RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE] = Depth;

    rstate_raster_desc Raster;
    s_RasterPresets[RSTATE_RASTER_PRESET_SOLID] = Raster;

    Raster                                     = rstate_raster_desc();
    Raster.FillMode                            = RSTATE_FILL_WIREFRAME;
    s_RasterPresets[RSTATE_RASTER_PRESET_WIRE] = Raster;

    Raster                                              = rstate_raster_desc();
    Raster.CullMode                                     = RSTATE_CULL_NONE;
    s_RasterPresets[RSTATE_RASTER_PRESET_SOLID_NO_CULL] = Raster;

    Raster                                             = rstate_raster_desc();
    Raster.FillMode                                    = RSTATE_FILL_WIREFRAME;
    Raster.CullMode                                    = RSTATE_CULL_NONE;
    s_RasterPresets[RSTATE_RASTER_PRESET_WIRE_NO_CULL] = Raster;

    Raster                                             = rstate_raster_desc();
    Raster.CullMode                                    = RSTATE_CULL_NONE;
    Raster.DepthBias                                   = -1.0f;
    Raster.SlopeScaledDepthBias                        = -1.0f;
    Raster.bDepthBiasEnable                            = TRUE;
    s_RasterPresets[RSTATE_RASTER_PRESET_DEPTH_BIASED] = Raster;

    rstate_sampler_desc Sampler;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_WRAP] = Sampler;

    Sampler                                              = rstate_sampler_desc();
    Sampler.AddressU                                     = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressV                                     = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressW                                     = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] = Sampler;

    Sampler                                                       = rstate_sampler_desc();
    Sampler.AddressV                                              = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_WRAP_U_CLAMP_V] = Sampler;

    Sampler                                                       = rstate_sampler_desc();
    Sampler.AddressU                                              = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP_U_WRAP_V] = Sampler;

    Sampler                                            = rstate_sampler_desc();
    Sampler.Filter                                     = RSTATE_FILTER_POINT;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_POINT_WRAP] = Sampler;

    Sampler                                             = rstate_sampler_desc();
    Sampler.Filter                                      = RSTATE_FILTER_POINT;
    Sampler.AddressU                                    = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressV                                    = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressW                                    = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_POINT_CLAMP] = Sampler;

    Sampler                                                  = rstate_sampler_desc();
    Sampler.Filter                                           = RSTATE_FILTER_ANISOTROPIC;
    Sampler.MaxAnisotropy                                    = 16;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP] = Sampler;

    Sampler                                                   = rstate_sampler_desc();
    Sampler.Filter                                            = RSTATE_FILTER_ANISOTROPIC;
    Sampler.MaxAnisotropy                                     = 16;
    Sampler.AddressU                                          = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressV                                          = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressW                                          = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_ANISOTROPIC_CLAMP] = Sampler;

    Sampler                                               = rstate_sampler_desc();
    Sampler.UseTextureFiltering                           = TRUE;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_MATERIAL_WRAP] = Sampler;

    Sampler.AddressU                                       = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressV                                       = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressW                                       = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_MATERIAL_CLAMP] = Sampler;

    s_bPresetsBuilt = TRUE;
}

//==============================================================================

static
void sdlstate_ReleaseNativeSampler( rstate_sampler_native* pNative )
{
    if( !pNative )
    {
        return;
    }

    ASSERT( pNative->References > 0 );

    if( --pNative->References )
    {
        return;
    }

    rstate_sampler_native** ppNative = &s_pNativeSamplers;
    while( *ppNative != pNative )
    {
        ppNative = &( *ppNative )->pNext;
    }

    *ppNative = pNative->pNext;

    if( g_pSDLGPUDevice )
    {
        SDL_ReleaseGPUSampler( g_pSDLGPUDevice, pNative->pSampler );
    }

    delete pNative;
}

//==============================================================================

static
void sdlstate_ReleaseSamplerBackend( rstate_sampler_backend* pBackend )
{
    if( !pBackend )
    {
        return;
    }

    sdlstate_ReleaseNativeSampler( pBackend->pNative );
    pBackend->pNative  = NULL;
    pBackend->pSampler = NULL;
}

//==============================================================================

static
xbool sdlstate_PipelineKeysEqual( const sdlstate_pipeline_key& A,
                                  const sdlstate_pipeline_key& B )
{
    if( ( A.VertexShaderSerial != B.VertexShaderSerial ) ||
        ( A.PixelShaderSerial != B.PixelShaderSerial ) ||
        ( A.VertexBufferCount != B.VertexBufferCount ) ||
        ( A.VertexAttributeCount != B.VertexAttributeCount ) ||
        ( A.PrimitiveType != B.PrimitiveType ) ||
        ( A.ColorTargetCount != B.ColorTargetCount ) ||
        ( A.DepthStencilFormat != B.DepthStencilFormat ) ||
        ( A.bHasDepthStencilTarget != B.bHasDepthStencilTarget ) )
    {
        return FALSE;
    }

    // These SDL state records and their explicit padding are zero-initialized
    // before fields are assigned, so their byte representation is deterministic.
    if( x_memcmp( &A.RasterizerState, &B.RasterizerState, sizeof( A.RasterizerState ) ) != 0 ||
        x_memcmp( &A.MultisampleState, &B.MultisampleState, sizeof( A.MultisampleState ) ) != 0 ||
        x_memcmp( &A.DepthStencilState, &B.DepthStencilState, sizeof( A.DepthStencilState ) ) != 0 )
    {
        return FALSE;
    }

    if( A.VertexBufferCount &&
        ( x_memcmp( A.VertexBuffers,
                    B.VertexBuffers,
                    sizeof( SDL_GPUVertexBufferDescription ) * A.VertexBufferCount ) != 0 ) )
    {
        return FALSE;
    }

    if( A.VertexAttributeCount &&
        ( x_memcmp( A.VertexAttributes,
                    B.VertexAttributes,
                    sizeof( SDL_GPUVertexAttribute ) * A.VertexAttributeCount ) != 0 ) )
    {
        return FALSE;
    }

    if( A.ColorTargetCount &&
        ( x_memcmp( A.ColorTargets,
                    B.ColorTargets,
                    sizeof( SDL_GPUColorTargetDescription ) * A.ColorTargetCount ) != 0 ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
u64 sdlstate_HashPipelineKey( const sdlstate_pipeline_key& Key )
{
    u64        Hash      = 14695981039346656037ull;
    const auto HashBytes = [&Hash]( const void* pData, u32 Size )
    {
        const u8* pBytes = (const u8*)pData;

        for( u32 i = 0; i < Size; ++i )
        {
            Hash ^= pBytes[i];
            Hash *= 1099511628211ull;
        }
    };

    HashBytes( &Key.VertexShaderSerial, sizeof( Key.VertexShaderSerial ) );
    HashBytes( &Key.PixelShaderSerial, sizeof( Key.PixelShaderSerial ) );
    HashBytes( &Key.VertexBufferCount, sizeof( Key.VertexBufferCount ) );
    HashBytes( &Key.VertexAttributeCount, sizeof( Key.VertexAttributeCount ) );
    HashBytes( Key.VertexBuffers, sizeof( SDL_GPUVertexBufferDescription ) * Key.VertexBufferCount );
    HashBytes( Key.VertexAttributes, sizeof( SDL_GPUVertexAttribute ) * Key.VertexAttributeCount );
    HashBytes( &Key.PrimitiveType, sizeof( Key.PrimitiveType ) );
    HashBytes( &Key.RasterizerState, sizeof( Key.RasterizerState ) );
    HashBytes( &Key.MultisampleState, sizeof( Key.MultisampleState ) );
    HashBytes( &Key.DepthStencilState, sizeof( Key.DepthStencilState ) );
    HashBytes( &Key.ColorTargetCount, sizeof( Key.ColorTargetCount ) );
    HashBytes( Key.ColorTargets, sizeof( SDL_GPUColorTargetDescription ) * Key.ColorTargetCount );
    HashBytes( &Key.DepthStencilFormat, sizeof( Key.DepthStencilFormat ) );
    HashBytes( &Key.bHasDepthStencilTarget, sizeof( Key.bHasDepthStencilTarget ) );

    return Hash;
}

//==============================================================================

static
render_pipeline_cache_entry* sdlstate_FindPipeline( const sdlstate_pipeline_key& Key, u64 Hash )
{
    for( render_pipeline_cache_entry* pEntry = s_PipelineBuckets[Hash % SDLENG_PIPELINE_HASH_BUCKETS];
         pEntry;
         pEntry = pEntry->pHashNext )
    {
        if( ( pEntry->Hash == Hash ) && sdlstate_PipelineKeysEqual( pEntry->Key, Key ) )
        {
            return pEntry;
        }
    }

    return NULL;
}

//==============================================================================

static
void sdlstate_LinkPipelineCacheEntry( render_pipeline_cache_entry* pEntry )
{
    const u32 Bucket          = (u32)( pEntry->Hash % SDLENG_PIPELINE_HASH_BUCKETS );
    pEntry->pHashNext         = s_PipelineBuckets[Bucket];
    s_PipelineBuckets[Bucket] = pEntry;
    pEntry->pNext             = s_pPipelineCache;
    s_pPipelineCache          = pEntry;
    s_PipelineCount++;
}

//==============================================================================

static
void sdlstate_ReleasePipelineCacheEntry( render_pipeline_cache_entry* pEntry )
{
    if( !pEntry )
    {
        return;
    }

    if( pEntry->pPipeline && g_pSDLGPUDevice )
    {
        SDL_ReleaseGPUGraphicsPipeline( g_pSDLGPUDevice, pEntry->pPipeline );
    }

    pEntry->pPipeline                     = NULL;
    render_pipeline_cache_entry** ppEntry = &s_PipelineBuckets[pEntry->Hash % SDLENG_PIPELINE_HASH_BUCKETS];

    while( *ppEntry && ( *ppEntry != pEntry ) )
    {
        ppEntry = &( *ppEntry )->pHashNext;
    }

    if( *ppEntry )
    {
        *ppEntry = pEntry->pHashNext;
    }
}

//==============================================================================

static
void sdlstate_EvictUnusedPipelineEntries( void )
{
    static xprofile_counter PipelineCacheEvictionMetric =
        x_GetProfiler().RegisterCounter( "PipelineCacheEvictions", "Renderer" );

    while( s_PipelineCount > SDLENG_MAX_PIPELINE_CACHE )
    {
        render_pipeline_cache_entry* pPrevious       = NULL;
        render_pipeline_cache_entry* pOldest         = NULL;
        render_pipeline_cache_entry* pOldestPrevious = NULL;
        u64                          OldestUse       = 0xffffffffffffffffull;

        for( render_pipeline_cache_entry* pEntry = s_pPipelineCache;
             pEntry;
             pEntry = pEntry->pNext )
        {
            if( ( pEntry->HandleCount == 0 ) && ( pEntry->PendingCount == 0 ) &&
                ( pEntry != s_pBoundPipeline ) &&
                ( pEntry->LastUse < OldestUse ) )
            {
                pOldest         = pEntry;
                pOldestPrevious = pPrevious;
                OldestUse       = pEntry->LastUse;
            }

            pPrevious = pEntry;
        }

        if( !pOldest )
        {
            return;
        }

        if( pOldestPrevious )
        {
            pOldestPrevious->pNext = pOldest->pNext;
        }
        else
        {
            s_pPipelineCache = pOldest->pNext;
        }

        pOldest->pNext = NULL;
        sdlstate_ReleasePipelineCacheEntry( pOldest );
        delete pOldest;
        --s_PipelineCount;
        PipelineCacheEvictionMetric.Add();
    }
}

//==============================================================================

void sdlstate_InvalidateShaderPipelines( u64 ShaderSerial )
{
    render_pipeline_cache_entry* pPrevious = NULL;

    render_pipeline_cache_entry* pEntry = s_pPipelineCache;

    while( pEntry )
    {
        render_pipeline_cache_entry* pNext = pEntry->pNext;

        if( ( pEntry->Key.VertexShaderSerial == ShaderSerial ) ||
            ( pEntry->Key.PixelShaderSerial == ShaderSerial ) )
        {
            if( pEntry->HandleCount != 0 || pEntry->PendingCount != 0 )
            {
                pPrevious = pEntry;
                pEntry    = pNext;
                continue;
            }

            if( pPrevious )
            {
                pPrevious->pNext = pNext;
            }
            else
            {
                s_pPipelineCache = pNext;
            }

            pEntry->pNext = NULL;

            if( s_pBoundPipeline == pEntry )
            {
                s_pBoundPipeline = NULL;
            }

            sdlstate_ReleasePipelineCacheEntry( pEntry );
            delete pEntry;
            --s_PipelineCount;
        }
        else
        {
            pPrevious = pEntry;
        }

        pEntry = pNext;
    }
}

//==============================================================================

static
void sdlstate_DeletePipelineHandle( render_pipeline_backend* pBackend )
{
    if( !pBackend )
    {
        return;
    }

    if( pBackend->pDebugName )
    {
        x_free( pBackend->pDebugName );
    }

    pBackend->pDebugName = NULL;
    delete pBackend;
}

//==============================================================================

static
xbool sdlstate_FillVertexInputState( const shader_pipeline_desc& Desc,
                                     sdlstate_pipeline_key&      Key,
                                     SDL_GPUVertexInputState&    State )
{
    x_memset( &State, 0, sizeof( State ) );

    if( Desc.VertexBufferCount > SDLENG_MAX_VERTEX_BUFFERS ||
        Desc.InputElementCount > SDLENG_MAX_VERTEX_ATTRIBUTES ||
        ( Desc.VertexBufferCount && !Desc.pVertexBuffers ) ||
        ( Desc.InputElementCount && ( !Desc.pInputElements || !Desc.VertexBufferCount ) ) )
    {
        return FALSE;
    }

    for( u32 i = 0; i < Desc.VertexBufferCount; ++i )
    {
        const shader_vertex_buffer_desc& Src = Desc.pVertexBuffers[i];
        if( ( Src.Slot >= SDLENG_MAX_VERTEX_BUFFERS ) || !Src.Stride || Src.InstanceStepRate )
        {
            return FALSE;
        }

        for( u32 j = 0; j < i; ++j )
        {
            if( Key.VertexBuffers[j].slot == Src.Slot )
            {
                return FALSE;
            }
        }

        SDL_GPUVertexBufferDescription& Dst = Key.VertexBuffers[i];
        Dst.slot                            = Src.Slot;
        Dst.pitch                           = Src.Stride;
        Dst.input_rate                      = Src.bPerInstance ? SDL_GPU_VERTEXINPUTRATE_INSTANCE : SDL_GPU_VERTEXINPUTRATE_VERTEX;

        for( u32 j = i; j > 0 && ( Key.VertexBuffers[j].slot < Key.VertexBuffers[j - 1].slot ); --j )
        {
            const SDL_GPUVertexBufferDescription Temp = Key.VertexBuffers[j];
            Key.VertexBuffers[j]                      = Key.VertexBuffers[j - 1];
            Key.VertexBuffers[j - 1]                  = Temp;
        }
    }

    for( u32 i = 0; i < Desc.InputElementCount; ++i )
    {
        const shader_vertex_element& Src    = Desc.pInputElements[i];
        u32                          Stride = 0;

        for( u32 j = 0; j < Desc.VertexBufferCount; ++j )
        {
            if( Key.VertexBuffers[j].slot == Src.InputSlot )
            {
                Stride = Key.VertexBuffers[j].pitch;
            }
        }

        const SDL_GPUVertexElementFormat Format      = sdlstate_ToSDLVertexFormat( Src.Format );
        const u32                        ElementSize = sdlstate_GetVertexFormatSize( Src.Format );

        if( ( Format == SDL_GPU_VERTEXELEMENTFORMAT_INVALID ) || !ElementSize ||
            ( ElementSize > Stride ) || ( Src.AlignedByteOffset > Stride - ElementSize ) ||
            ( Src.Location >= SDLENG_MAX_VERTEX_ATTRIBUTES ) )
        {
            return FALSE;
        }

        for( u32 j = 0; j < i; ++j )
        {
            if( Key.VertexAttributes[j].location == Src.Location )
            {
                return FALSE;
            }
        }

        SDL_GPUVertexAttribute& Dst = Key.VertexAttributes[i];
        Dst.location                = Src.Location;
        Dst.buffer_slot             = Src.InputSlot;
        Dst.format                  = Format;
        Dst.offset                  = Src.AlignedByteOffset;

        for( u32 j = i; j > 0 && ( Key.VertexAttributes[j].location < Key.VertexAttributes[j - 1].location ); --j )
        {
            const SDL_GPUVertexAttribute Temp = Key.VertexAttributes[j];
            Key.VertexAttributes[j]           = Key.VertexAttributes[j - 1];
            Key.VertexAttributes[j - 1]       = Temp;
        }
    }

    Key.VertexBufferCount            = Desc.VertexBufferCount;
    Key.VertexAttributeCount         = Desc.InputElementCount;
    State.vertex_buffer_descriptions = Desc.VertexBufferCount ? Key.VertexBuffers : NULL;
    State.num_vertex_buffers         = Desc.VertexBufferCount;
    State.vertex_attributes          = Desc.InputElementCount ? Key.VertexAttributes : NULL;
    State.num_vertex_attributes      = Desc.InputElementCount;

    return TRUE;
}

//==============================================================================

static
void sdlstate_FillBlendState( SDL_GPUColorTargetBlendState& Dst, const rstate_blend_desc& Src )
{
    x_memset( &Dst, 0, sizeof( Dst ) );

    Dst.src_color_blendfactor   = sdlstate_ToSDLBlendFactor( Src.SrcBlend );
    Dst.dst_color_blendfactor   = sdlstate_ToSDLBlendFactor( Src.DestBlend );
    Dst.color_blend_op          = sdlstate_ToSDLBlendOp( Src.BlendOp );
    Dst.src_alpha_blendfactor   = sdlstate_ToSDLBlendFactor( Src.SrcBlendAlpha );
    Dst.dst_alpha_blendfactor   = sdlstate_ToSDLBlendFactor( Src.DestBlendAlpha );
    Dst.alpha_blend_op          = sdlstate_ToSDLBlendOp( Src.BlendOpAlpha );
    Dst.color_write_mask        = sdlstate_ToSDLColorMask( Src.ColorWriteMask );
    Dst.enable_blend            = Src.bBlendEnable ? true : false;
    Dst.enable_color_write_mask = true;
}

//==============================================================================

static
void sdlstate_FillDepthStencilState( SDL_GPUDepthStencilState& Dst,
                                     const rstate_depth_desc&  Src,
                                     xbool                     bHasDepthStencilTarget )
{
    x_memset( &Dst, 0, sizeof( Dst ) );

    Dst.compare_op          = sdlstate_ToSDLCompareOp( Src.DepthFunc );
    Dst.compare_mask        = Src.StencilReadMask;
    Dst.write_mask          = Src.StencilWriteMask;
    Dst.enable_depth_test   = ( bHasDepthStencilTarget && Src.bDepthEnable ) ? true : false;
    Dst.enable_depth_write  = ( Dst.enable_depth_test && Src.bDepthWrite ) ? true : false;
    Dst.enable_stencil_test = ( bHasDepthStencilTarget && Src.bStencilEnable ) ? true : false;

    sdlstate_FillStencilFace( Dst.front_stencil_state, Src.FrontFace );
    sdlstate_FillStencilFace( Dst.back_stencil_state, Src.BackFace );
}

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void rstate_Init( void )
{
    sdlstate_BuildPresets();

    if( !s_pPipelineWorkMutex )
    {
        s_pPipelineWorkMutex       = new xmutex;
        s_pPipelineCompletionMutex = SDL_CreateMutex();
        s_pPipelineCompletion      = SDL_CreateCondition();
        ASSERT( s_pPipelineCompletionMutex && s_pPipelineCompletion );
    }
}

//==============================================================================

void rstate_Kill( void )
{
    for( render_pipeline_create_backend* pRequest = s_pPipelineRequests; pRequest; pRequest = pRequest->pNext )
    {
        render_CancelPipelineCreate( *pRequest->pOwner );
    }

    while( s_pPipelineRequests )
    {
        render_pipeline_create_request& Request = *s_pPipelineRequests->pOwner;
        render_CancelPipelineCreate( Request );
        render_ReleasePipelineCreate( Request );
    }

    for( auto& Worker : s_PipelineWorkers )
    {
        if( Worker.Job.IsNonNull() )
        {
            x_WorkerJobWait( Worker.Job );
            x_WorkerJobRelease( Worker.Job );
            Worker.Job = HNULL;
        }

        ASSERT( !Worker.Running );
    }

    sdleng_CollectPipelineWork();
    ASSERT( !s_pRetiredPipelineWork );
    SDL_DestroyCondition( s_pPipelineCompletion );
    SDL_DestroyMutex( s_pPipelineCompletionMutex );
    s_pPipelineCompletion      = NULL;
    s_pPipelineCompletionMutex = NULL;
    ASSERT( !s_pPipelineWork );
    delete s_pPipelineWorkMutex;
    s_pPipelineWorkMutex = NULL;
    s_pBoundPipeline     = NULL;

    while( s_pSamplerList )
    {
        rstate_sampler_backend* pBackend = s_pSamplerList;
        sdleng_UnlinkBackend( s_pSamplerList, pBackend, s_SamplerCount );
        sdlstate_ReleaseSamplerBackend( pBackend );

        if( pBackend->pOwner )
        {
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        delete pBackend;
    }

    while( s_pPipelineHandleList )
    {
        render_pipeline_backend* pBackend = s_pPipelineHandleList;
        sdleng_UnlinkBackend( s_pPipelineHandleList, pBackend, s_PipelineHandleCount );
        sdleng_ClearGraphicsPipelineDebug( pBackend );

        if( pBackend->pEntry )
        {
            ASSERT( pBackend->pEntry->HandleCount > 0 );
            pBackend->pEntry->HandleCount--;
            pBackend->pEntry = NULL;
        }

        if( pBackend->pOwner )
        {
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        sdlstate_DeletePipelineHandle( pBackend );
    }

    while( s_pPipelineCache )
    {
        render_pipeline_cache_entry* pEntry = s_pPipelineCache;
        s_pPipelineCache                    = pEntry->pNext;
        ASSERT( pEntry->HandleCount == 0 );
        sdlstate_ReleasePipelineCacheEntry( pEntry );
        delete pEntry;
    }

    s_PipelineCount = 0;
}

//==============================================================================
//  PRESET ACCESS
//==============================================================================

const rstate_blend_desc& rstate_GetBlendDesc( rstate_blend_preset Preset )
{
    sdlstate_BuildPresets();

    if( ( Preset < 0 ) || ( Preset >= RSTATE_BLEND_PRESET_COUNT ) )
    {
        ASSERT( FALSE );
        return s_BlendPresets[RSTATE_BLEND_PRESET_NONE];
    }

    return s_BlendPresets[Preset];
}

//==============================================================================

const rstate_depth_desc& rstate_GetDepthDesc( rstate_depth_preset Preset )
{
    sdlstate_BuildPresets();

    if( ( Preset < 0 ) || ( Preset >= RSTATE_DEPTH_PRESET_COUNT ) )
    {
        ASSERT( FALSE );
        return s_DepthPresets[RSTATE_DEPTH_PRESET_NORMAL];
    }

    return s_DepthPresets[Preset];
}

//==============================================================================

const rstate_raster_desc& rstate_GetRasterDesc( rstate_raster_preset Preset )
{
    sdlstate_BuildPresets();

    if( ( Preset < 0 ) || ( Preset >= RSTATE_RASTER_PRESET_COUNT ) )
    {
        ASSERT( FALSE );
        return s_RasterPresets[RSTATE_RASTER_PRESET_SOLID];
    }

    return s_RasterPresets[Preset];
}

//==============================================================================

const rstate_sampler_desc& rstate_GetSamplerDesc( rstate_sampler_preset Preset )
{
    sdlstate_BuildPresets();

    if( ( Preset < 0 ) || ( Preset >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        ASSERT( FALSE );
        return s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_WRAP];
    }

    return s_SamplerPresets[Preset];
}

//==============================================================================
//  PIPELINE OBJECTS
//==============================================================================

static
xbool sdlstate_PreparePipeline( sdlstate_pipeline_key&             Key,
                                SDL_GPUGraphicsPipelineCreateInfo& CreateInfo,
                                const render_pipeline_desc&        Source )
{
    render_pipeline_desc Desc;
    render_NormalizePipelineState( Source, Desc );

    if( !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    if( Desc.ColorCount > RTARGET_MAX_TARGETS )
    {
        return FALSE;
    }

    if( ( Desc.ColorCount == 0 ) && ( Desc.DepthFormat == RTARGET_FORMAT_COUNT ) )
    {
        return FALSE;
    }

    if( Desc.SampleMask != 0xffffffff )
    {
        x_DebugMsg( "SDLState: custom sample masks are not supported by SDL GPU yet\n" );
        return FALSE;
    }

    SDL_GPUSampleCount SDLSampleCount;
    if( !sdleng_ToSDLSampleCount( Desc.SampleCount, SDLSampleCount ) )
    {
        return FALSE;
    }

    if( !Desc.Shader.pVertexShader || !Desc.Shader.pPixelShader )
    {
        return FALSE;
    }

    if( Desc.Shader.pVertexShader->Stage != SHADER_STAGE_VERTEX ||
        Desc.Shader.pPixelShader->Stage != SHADER_STAGE_PIXEL )
    {
        return FALSE;
    }

    SDL_GPUShader* pVertexShader = sdleng_GetGPUShader( Desc.Shader.pVertexShader );
    SDL_GPUShader* pPixelShader  = sdleng_GetGPUShader( Desc.Shader.pPixelShader );
    if( !pVertexShader || !pPixelShader )
    {
        return FALSE;
    }

    const shader_backend* pVertexShaderBackend = Desc.Shader.pVertexShader->pBackend;
    const shader_backend* pPixelShaderBackend  = Desc.Shader.pPixelShader->pBackend;
    if( !pVertexShaderBackend || !pPixelShaderBackend )
    {
        return FALSE;
    }

    if( Desc.Shader.Topology == SHADER_TOPOLOGY_UNDEFINED )
    {
        return FALSE;
    }

    SDL_GPUVertexInputState VertexInput;
    if( !sdlstate_FillVertexInputState( Desc.Shader, Key, VertexInput ) )
    {
        return FALSE;
    }

    SDL_GPUColorTargetDescription* ColorTargets = Key.ColorTargets;
    for( u32 i = 0; i < Desc.ColorCount; i++ )
    {
        if( sdleng_IsDepthFormat( Desc.ColorTargets[i].Format ) )
        {
            return FALSE;
        }

        SDL_GPUTextureFormat Format = sdleng_ToSDLTextureFormat( Desc.ColorTargets[i].Format );

        if( Format == SDL_GPU_TEXTUREFORMAT_INVALID )
        {
            return FALSE;
        }

        if( !SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, Format, SDLSampleCount ) )
        {
            x_DebugMsg( "SDLState: sample count %d is not supported for color target format %d\n",
                        Desc.SampleCount,
                        Desc.ColorTargets[i].Format );

            return FALSE;
        }

        if( !SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice,
                                           Format,
                                           SDL_GPU_TEXTURETYPE_2D,
                                           SDL_GPU_TEXTUREUSAGE_COLOR_TARGET ) )
        {
            x_DebugMsg( "SDLState: color target format %d is not supported\n",
                        Desc.ColorTargets[i].Format );

            return FALSE;
        }

        ColorTargets[i].format = Format;
        sdlstate_FillBlendState( ColorTargets[i].blend_state, Desc.ColorTargets[i].Blend );
    }

    const xbool          bHasDepth   = ( Desc.DepthFormat != RTARGET_FORMAT_COUNT );
    SDL_GPUTextureFormat DepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    if( bHasDepth )
    {
        if( !sdleng_IsDepthFormat( Desc.DepthFormat ) )
        {
            return FALSE;
        }

        DepthFormat = sdleng_ToSDLTextureFormat( Desc.DepthFormat );

        if( DepthFormat == SDL_GPU_TEXTUREFORMAT_INVALID )
        {
            return FALSE;
        }

        if( !SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, DepthFormat, SDLSampleCount ) )
        {
            x_DebugMsg( "SDLState: sample count %d is not supported for depth target format %d\n",
                        Desc.SampleCount,
                        Desc.DepthFormat );

            return FALSE;
        }

        if( !SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice,
                                           DepthFormat,
                                           SDL_GPU_TEXTURETYPE_2D,
                                           SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET ) )
        {
            x_DebugMsg( "SDLState: depth target format %d is not supported\n", Desc.DepthFormat );
            return FALSE;
        }

        if( Desc.Depth.bStencilEnable && !sdleng_HasStencil( Desc.DepthFormat ) )
        {
            x_DebugMsg( "SDLState: stencil state requires a stencil-capable depth target\n" );
            return FALSE;
        }
    }

    if( Desc.bAlphaToCoverage &&
        ( ( Desc.ColorCount == 0 ) || !sdleng_HasAlpha( Desc.ColorTargets[0].Format ) ) )
    {
        x_DebugMsg( "SDLState: alpha-to-coverage requires an alpha color target\n" );
        return FALSE;
    }

    x_memset( &CreateInfo, 0, sizeof( CreateInfo ) );
    CreateInfo.vertex_shader                               = pVertexShader;
    CreateInfo.fragment_shader                             = pPixelShader;
    CreateInfo.vertex_input_state                          = VertexInput;
    CreateInfo.primitive_type                              = sdlstate_ToSDLPrimitiveType( Desc.Shader.Topology );
    CreateInfo.rasterizer_state.fill_mode                  = sdlstate_ToSDLFillMode( Desc.Raster.FillMode );
    CreateInfo.rasterizer_state.cull_mode                  = sdlstate_ToSDLCullMode( Desc.Raster.CullMode );
    CreateInfo.rasterizer_state.front_face                 = sdlstate_ToSDLFrontFace( Desc.Raster.FrontFace );
    CreateInfo.rasterizer_state.depth_bias_constant_factor = Desc.Raster.DepthBias;
    CreateInfo.rasterizer_state.depth_bias_clamp           = Desc.Raster.DepthBiasClamp;
    CreateInfo.rasterizer_state.depth_bias_slope_factor    = Desc.Raster.SlopeScaledDepthBias;
    CreateInfo.rasterizer_state.enable_depth_bias          = Desc.Raster.bDepthBiasEnable ? true : false;
    CreateInfo.rasterizer_state.enable_depth_clip          = Desc.Raster.bDepthClipEnable ? true : false;
    CreateInfo.multisample_state.sample_count              = SDLSampleCount;
    CreateInfo.multisample_state.sample_mask               = 0;
    CreateInfo.multisample_state.enable_mask               = false;
    CreateInfo.multisample_state.enable_alpha_to_coverage  = Desc.bAlphaToCoverage ? true : false;
    sdlstate_FillDepthStencilState( CreateInfo.depth_stencil_state, Desc.Depth, bHasDepth );
    CreateInfo.target_info.color_target_descriptions = ( Desc.ColorCount > 0 ) ? ColorTargets : NULL;
    CreateInfo.target_info.num_color_targets         = Desc.ColorCount;
    CreateInfo.target_info.depth_stencil_format      = DepthFormat;
    CreateInfo.target_info.has_depth_stencil_target  = bHasDepth ? true : false;

    Key.VertexShaderSerial     = pVertexShaderBackend->Serial;
    Key.PixelShaderSerial      = pPixelShaderBackend->Serial;
    Key.VertexBufferCount      = VertexInput.num_vertex_buffers;
    Key.VertexAttributeCount   = VertexInput.num_vertex_attributes;
    Key.PrimitiveType          = CreateInfo.primitive_type;
    Key.RasterizerState        = CreateInfo.rasterizer_state;
    Key.MultisampleState       = CreateInfo.multisample_state;
    Key.DepthStencilState      = CreateInfo.depth_stencil_state;
    Key.ColorTargetCount       = CreateInfo.target_info.num_color_targets;
    Key.DepthStencilFormat     = CreateInfo.target_info.depth_stencil_format;
    Key.bHasDepthStencilTarget = CreateInfo.target_info.has_depth_stencil_target ? TRUE : FALSE;

    return TRUE;
}

//==============================================================================

static
xbool sdlstate_AttachPipeline( render_pipeline& Pipeline, render_pipeline_cache_entry* pEntry, const shader_resource_counts& VertexResources, const shader_resource_counts& PixelResources, const char* pDebugName )
{
    if( Pipeline.pBackend && Pipeline.pBackend->pOwner == &Pipeline && Pipeline.pBackend->pEntry == pEntry )
    {
        return TRUE;
    }

    render_pipeline_backend* pBackend = new render_pipeline_backend;

    if( !pBackend )
    {
        return FALSE;
    }

    if( pDebugName && pDebugName[0] )
    {
        pBackend->pDebugName = x_strdup( pDebugName );

        if( !pBackend->pDebugName )
        {
            sdlstate_DeletePipelineHandle( pBackend );
            return FALSE;
        }
    }

    pBackend->VertexResources = VertexResources;
    pBackend->PixelResources  = PixelResources;

    ++s_NextPipelineUse;

    if( s_NextPipelineUse == 0 )
    {
        s_NextPipelineUse = 1;
    }

    pEntry->LastUse = s_NextPipelineUse;

    ++pEntry->HandleCount;
    render_DestroyPipeline( Pipeline );

    pBackend->pOwner = &Pipeline;
    pBackend->pEntry = pEntry;
    sdlstate_EvictUnusedPipelineEntries();
    Pipeline.pBackend = pBackend;
    sdleng_LinkBackend( s_pPipelineHandleList, pBackend, s_PipelineHandleCount );

    return TRUE;
}

//==============================================================================

static
void sdlstate_CreatePipelineJob( void* pData )
{
    sdlstate_pipeline_work& Request = *static_cast<sdlstate_pipeline_work*>( pData );

    if( !x_AtomicLoadAcquire( &Request.Cancelled ) )
    {
        const xtick             Start = x_GetTime();
        static xprofile_counter PipelineCreateCalls =
            x_GetProfiler().RegisterCounter( "PipelineCreateCalls", "Renderer" );

        PipelineCreateCalls.Add();
        s_pPipelineWorkMutex->Enter();
        const u64 RequestIndex = x_AtomicLoadRelaxed( &s_PipelineCreateCalls ) + 1;
        x_AtomicStoreRelaxed( &s_PipelineCreateCalls, RequestIndex );
        s_pPipelineWorkMutex->Exit();
        SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION,
                     "A51 pipeline begin: request=%llu name='%s' vs=%016llx ps=%016llx",
                     static_cast<unsigned long long>( RequestIndex ),
                     Request.pDebugName ? Request.pDebugName : "Unnamed",
                     static_cast<unsigned long long>( Request.VertexProgramId ),
                     static_cast<unsigned long long>( Request.PixelProgramId ) );

        Request.pPipeline = SDL_CreateGPUGraphicsPipeline( Request.pDevice, &Request.CreateInfo );
        Request.Ticks     = x_GetTime() - Start;

        if( !Request.pPipeline )
        {
            SDL_snprintf( Request.Error, sizeof( Request.Error ), "%s", SDL_GetError() );
            SDL_LogError( SDL_LOG_CATEGORY_GPU,
                          "A51 pipeline: name='%s' vs=%016llx ps=%016llx ms=%.2f error='%s'",
                          Request.pDebugName ? Request.pDebugName : "Unnamed",
                          static_cast<unsigned long long>( Request.VertexProgramId ),
                          static_cast<unsigned long long>( Request.PixelProgramId ),
                          x_TicksToMs( Request.Ticks ),
                          Request.Error );
        }
        else
        {
            SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "A51 pipeline ready: request=%llu ms=%.2f", static_cast<unsigned long long>( RequestIndex ), x_TicksToMs( Request.Ticks ) );
        }
    }

    SDL_LockMutex( s_pPipelineCompletionMutex );
    x_AtomicStoreRelease( &Request.Completed, 1 );
    SDL_BroadcastCondition( s_pPipelineCompletion );
    SDL_UnlockMutex( s_pPipelineCompletionMutex );
}

//==============================================================================

static
void sdlstate_CreatePipelineQueueJob( void* pData )
{
    sdlstate_pipeline_worker& Worker = *static_cast<sdlstate_pipeline_worker*>( pData );

    for( ;; )
    {
        s_pPipelineWorkMutex->Enter();
        sdlstate_pipeline_work* pRequest = s_pPipelineWork;

        if( !pRequest )
        {
            Worker.Running = FALSE;
            s_pPipelineWorkMutex->Exit();
            return;
        }

        s_pPipelineWork = pRequest->pNextWork;

        if( s_pPipelineWork )
        {
            s_pPipelineWork->pPrevWork = NULL;
        }
        else
        {
            s_pPipelineWorkTail = NULL;
        }

        pRequest->pNextWork = NULL;
        pRequest->pPrevWork = NULL;
        pRequest->Queued    = FALSE;
        s_pPipelineWorkMutex->Exit();
        sdlstate_CreatePipelineJob( pRequest );
    }
}

//==============================================================================
//  PIPELINE WORK SCHEDULING
//==============================================================================

static
xbool sdlstate_QueuePipelineCreate( sdlstate_pipeline_work* pRequest )
{
    s_pPipelineWorkMutex->Enter();
    sdlstate_pipeline_worker* pIdleWorker = NULL;
    xbool                     HasWorker   = FALSE;

    for( auto& Worker : s_PipelineWorkers )
    {
        if( Worker.Running )
        {
            HasWorker = TRUE;
        }
        else if( !pIdleWorker )
        {
            pIdleWorker = &Worker;
        }
    }

    if( pIdleWorker )
    {
        if( pIdleWorker->Job.IsNonNull() )
        {
            x_WorkerJobWait( pIdleWorker->Job );
            x_WorkerJobRelease( pIdleWorker->Job );
            pIdleWorker->Job = HNULL;
        }

        pIdleWorker->Running = TRUE;

        if( x_WorkerJobSubmit( sdlstate_CreatePipelineQueueJob, pIdleWorker, "PipelineCreateQueue", pIdleWorker->Job ) )
        {
            HasWorker = TRUE;
        }
        else
        {
            pIdleWorker->Running = FALSE;
        }
    }

    if( !HasWorker )
    {
        s_pPipelineWorkMutex->Exit();
        return FALSE;
    }

    pRequest->Queued    = TRUE;
    pRequest->pPrevWork = s_pPipelineWorkTail;

    if( s_pPipelineWorkTail )
    {
        s_pPipelineWorkTail->pNextWork = pRequest;
    }
    else
    {
        s_pPipelineWork = pRequest;
    }

    s_pPipelineWorkTail = pRequest;
    s_pPipelineWorkMutex->Exit();

    return TRUE;
}

//==============================================================================

static
render_pipeline_cache_entry* sdlstate_PublishPipeline( sdlstate_pipeline_work& Request )
{
    if( Request.pEntry )
    {
        return Request.pEntry;
    }

    if( x_AtomicLoadAcquire( &Request.Cancelled ) || !Request.pPipeline )
    {
        return NULL;
    }

    render_pipeline_cache_entry* pEntry = sdlstate_FindPipeline( Request.Key, Request.Hash );

    if( pEntry )
    {
        SDL_ReleaseGPUGraphicsPipeline( Request.pDevice, Request.pPipeline );
    }
    else
    {
        pEntry         = new render_pipeline_cache_entry;
        pEntry->Serial = ++s_NextPipelineSerial;
        ASSERT( pEntry->Serial != 0 );
        pEntry->Key          = Request.Key;
        pEntry->Hash         = Request.Hash;
        pEntry->VertexNative = Request.VertexNative;
        pEntry->PixelNative  = Request.PixelNative;
        pEntry->pPipeline    = Request.pPipeline;
        sdlstate_LinkPipelineCacheEntry( pEntry );
        static xprofile_counter PipelineCreates =
            x_GetProfiler().RegisterCounter( "PipelineCreates", "Renderer" );

        PipelineCreates.Add();
    }

    Request.pPipeline = NULL;
    Request.pEntry    = pEntry;
    ++pEntry->PendingCount;
    static xprofile_zone PipelineCreateAPI =
        x_GetProfiler().RegisterZone( "PipelineCreateAPI", "RendererAPI" );

    PipelineCreateAPI.Record( Request.Ticks );

    return pEntry;
}

//==============================================================================

static
void sdlstate_WaitPipelineWork( sdlstate_pipeline_work& Work )
{
    const xtick Start = x_GetTime();
    SDL_LockMutex( s_pPipelineCompletionMutex );

    while( !x_AtomicLoadAcquire( &Work.Completed ) )
    {
        SDL_WaitCondition( s_pPipelineCompletion, s_pPipelineCompletionMutex );
    }

    SDL_UnlockMutex( s_pPipelineCompletionMutex );
    static xprofile_zone PipelineWait =
        x_GetProfiler().RegisterZone( "PipelineWait", "Renderer" );

    PipelineWait.Record( x_GetTime() - Start );
}

//==============================================================================

void sdleng_CollectPipelineWork( void )
{
    if( !s_pPipelineCompletionMutex )
    {
        return;
    }

    sdlstate_pipeline_work** ppWork = &s_pRetiredPipelineWork;

    while( *ppWork )
    {
        sdlstate_pipeline_work* pWork = *ppWork;
        SDL_LockMutex( s_pPipelineCompletionMutex );
        const xbool Completed = x_AtomicLoadAcquire( &pWork->Completed );
        SDL_UnlockMutex( s_pPipelineCompletionMutex );

        if( pWork->ConsumerCount || !Completed )
        {
            ppWork = &pWork->pNext;
            continue;
        }

        *ppWork                         = pWork->pNext;
        sdlstate_pipeline_work** ppHash = &s_PipelineTaskBuckets[pWork->Hash % SDLENG_PIPELINE_HASH_BUCKETS];

        while( *ppHash != pWork )
        {
            ppHash = &( *ppHash )->pHashNext;
        }

        *ppHash = pWork->pHashNext;

        if( pWork->pPipeline )
        {
            SDL_ReleaseGPUGraphicsPipeline( pWork->pDevice, pWork->pPipeline );
        }

        if( pWork->pEntry )
        {
            ASSERT( pWork->pEntry->PendingCount > 0 );
            --pWork->pEntry->PendingCount;
        }

        if( pWork->CreateInfo.props )
        {
            SDL_DestroyProperties( pWork->CreateInfo.props );
        }

        x_free( pWork->pDebugName );
        delete pWork;
    }
}

//==============================================================================
//  ASYNCHRONOUS PIPELINE CREATION
//==============================================================================

xbool render_BeginPipelineCreate( render_pipeline_create_request& Request, const render_pipeline_desc& Desc )
{
    if( Request.pBackend || !x_WorkersIsInit() || !s_pPipelineWorkMutex )
    {
        return FALSE;
    }

    sdleng_CollectPipelineWork();
    sdlstate_pipeline_key             Key;
    SDL_GPUGraphicsPipelineCreateInfo CreateInfo;

    if( !sdlstate_PreparePipeline( Key, CreateInfo, Desc ) )
    {
        return FALSE;
    }

    const u64               Hash  = sdlstate_HashPipelineKey( Key );
    sdlstate_pipeline_work* pWork = s_PipelineTaskBuckets[Hash % SDLENG_PIPELINE_HASH_BUCKETS];

    while( pWork && ( x_AtomicLoadAcquire( &pWork->Cancelled ) || pWork->Hash != Hash ||
                      !sdlstate_PipelineKeysEqual( pWork->Key, Key ) ) )
    {
        pWork = pWork->pHashNext;
    }

    if( !pWork )
    {
        pWork = new sdlstate_pipeline_work;

        if( !sdlstate_PreparePipeline( pWork->Key, pWork->CreateInfo, Desc ) )
        {
            delete pWork;
            return FALSE;
        }

        pWork->Hash            = Hash;
        pWork->pDevice         = g_pSDLGPUDevice;
        pWork->VertexNative    = Desc.Shader.pVertexShader->pBackend->Native;
        pWork->PixelNative     = Desc.Shader.pPixelShader->pBackend->Native;
        pWork->VertexProgramId = Desc.Shader.pVertexShader->ProgramId;
        pWork->PixelProgramId  = Desc.Shader.pPixelShader->ProgramId;
        pWork->VertexResources = Desc.Shader.pVertexShader->pBackend->Resources;
        pWork->PixelResources  = Desc.Shader.pPixelShader->pBackend->Resources;
        pWork->pEntry          = sdlstate_FindPipeline( pWork->Key, Hash );

        if( pWork->pEntry )
        {
            ++pWork->pEntry->PendingCount;
            x_AtomicStoreRelease( &pWork->Completed, 1 );
        }
        else
        {
            if( Desc.pDebugName && Desc.pDebugName[0] )
            {
                pWork->pDebugName       = x_strdup( Desc.pDebugName );
                pWork->CreateInfo.props = SDL_CreateProperties();

                if( !pWork->pDebugName || !pWork->CreateInfo.props ||
                    !SDL_SetStringProperty( pWork->CreateInfo.props,
                                            SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING,
                                            Desc.pDebugName ) )
                {
                    if( pWork->CreateInfo.props )
                    {
                        SDL_DestroyProperties( pWork->CreateInfo.props );
                    }

                    x_free( pWork->pDebugName );
                    delete pWork;
                    return FALSE;
                }
            }

            if( !sdlstate_QueuePipelineCreate( pWork ) )
            {
                if( pWork->CreateInfo.props )
                {
                    SDL_DestroyProperties( pWork->CreateInfo.props );
                }

                x_free( pWork->pDebugName );
                delete pWork;
                return FALSE;
            }
        }

        pWork->pHashNext                                           = s_PipelineTaskBuckets[Hash % SDLENG_PIPELINE_HASH_BUCKETS];
        s_PipelineTaskBuckets[Hash % SDLENG_PIPELINE_HASH_BUCKETS] = pWork;
    }
    else
    {
        static xprofile_counter PipelinePendingJoins =
            x_GetProfiler().RegisterCounter( "PipelinePendingJoins", "Renderer" );

        PipelinePendingJoins.Add();
    }

    render_pipeline_create_backend* pRequest = new render_pipeline_create_backend;
    pRequest->pWork                          = pWork;
    ++pWork->ConsumerCount;

    if( Desc.pDebugName )
    {
        pRequest->DebugName = Desc.pDebugName;
    }

    pRequest->pOwner    = &Request;
    pRequest->pNext     = s_pPipelineRequests;
    s_pPipelineRequests = pRequest;
    Request.pBackend    = pRequest;

    return TRUE;
}

//==============================================================================

render_pipeline_create_status render_PollPipelineCreate( render_pipeline_create_request& Request,
                                                         render_pipeline*                pPipeline )
{
    render_pipeline_create_backend* pRequest = Request.pBackend;

    if( !pRequest )
    {
        return RENDER_PIPELINE_CREATE_FAILED;
    }

    if( pRequest->Cancelled )
    {
        return RENDER_PIPELINE_CREATE_CANCELLED;
    }

    sdlstate_pipeline_work& Work = *pRequest->pWork;

    if( !x_AtomicLoadAcquire( &Work.Completed ) )
    {
        return RENDER_PIPELINE_CREATE_PENDING;
    }

    render_pipeline_cache_entry* pEntry = sdlstate_PublishPipeline( Work );

    if( !pEntry )
    {
        x_DebugMsg( "SDLState: pipeline '%s' failed: %s\n", (const char*)pRequest->DebugName, Work.Error );
        return RENDER_PIPELINE_CREATE_FAILED;
    }

    if( pPipeline && !sdlstate_AttachPipeline( *pPipeline, pEntry, Work.VertexResources, Work.PixelResources, pRequest->DebugName ) )
    {
        return RENDER_PIPELINE_CREATE_FAILED;
    }

    return RENDER_PIPELINE_CREATE_READY;
}

//==============================================================================

xbool render_FinishPipelineCreate( render_pipeline_create_request& Request, render_pipeline& Pipeline )
{
    if( !Request.pBackend || Request.pBackend->Cancelled )
    {
        return FALSE;
    }

    sdlstate_WaitPipelineWork( *Request.pBackend->pWork );

    return render_PollPipelineCreate( Request, &Pipeline ) == RENDER_PIPELINE_CREATE_READY;
}

//==============================================================================

void render_CancelPipelineCreate( render_pipeline_create_request& Request )
{
    render_pipeline_create_backend* pRequest = Request.pBackend;

    if( !pRequest || pRequest->Cancelled )
    {
        return;
    }

    pRequest->Cancelled           = TRUE;
    sdlstate_pipeline_work* pWork = pRequest->pWork;
    ASSERT( pWork->ConsumerCount > 0 );

    if( --pWork->ConsumerCount )
    {
        return;
    }

    x_AtomicStoreRelease( &pWork->Cancelled, 1 );

    if( x_AtomicLoadAcquire( &pWork->Completed ) )
    {
        pWork->pNext           = s_pRetiredPipelineWork;
        s_pRetiredPipelineWork = pWork;
        return;
    }

    s_pPipelineWorkMutex->Enter();

    if( pWork->Queued )
    {
        if( pWork->pPrevWork )
        {
            pWork->pPrevWork->pNextWork = pWork->pNextWork;
        }
        else
        {
            s_pPipelineWork = pWork->pNextWork;
        }

        if( pWork->pNextWork )
        {
            pWork->pNextWork->pPrevWork = pWork->pPrevWork;
        }
        else
        {
            s_pPipelineWorkTail = pWork->pPrevWork;
        }

        pWork->pNextWork = NULL;
        pWork->pPrevWork = NULL;
        pWork->Queued    = FALSE;
        SDL_LockMutex( s_pPipelineCompletionMutex );
        x_AtomicStoreRelease( &pWork->Completed, 1 );
        SDL_BroadcastCondition( s_pPipelineCompletion );
        SDL_UnlockMutex( s_pPipelineCompletionMutex );
    }

    s_pPipelineWorkMutex->Exit();
    pWork->pNext           = s_pRetiredPipelineWork;
    s_pRetiredPipelineWork = pWork;
}

//==============================================================================

void render_ReleasePipelineCreate( render_pipeline_create_request& Request )
{
    render_pipeline_create_backend* pRequest = Request.pBackend;

    if( !pRequest )
    {
        return;
    }

    render_CancelPipelineCreate( Request );
    render_pipeline_create_backend** ppRequest = &s_pPipelineRequests;

    while( *ppRequest != pRequest )
    {
        ppRequest = &( *ppRequest )->pNext;
    }

    *ppRequest = pRequest->pNext;
    delete pRequest;
    Request.pBackend = NULL;
    sdleng_CollectPipelineWork();
}

//==============================================================================
//  PIPELINE LIFETIME
//==============================================================================

xbool render_CreatePipeline( render_pipeline& Pipeline, const render_pipeline_desc& Desc )
{
    sdlstate_pipeline_key             Key;
    SDL_GPUGraphicsPipelineCreateInfo CreateInfo;

    if( !sdlstate_PreparePipeline( Key, CreateInfo, Desc ) )
    {
        return FALSE;
    }

    render_pipeline_cache_entry* pEntry = sdlstate_FindPipeline( Key, sdlstate_HashPipelineKey( Key ) );

    if( pEntry )
    {
        return sdlstate_AttachPipeline( Pipeline, pEntry, Desc.Shader.pVertexShader->pBackend->Resources, Desc.Shader.pPixelShader->pBackend->Resources, Desc.pDebugName );
    }

    render_pipeline_create_request Request;

    if( !render_BeginPipelineCreate( Request, Desc ) )
    {
        return FALSE;
    }

    sdlstate_WaitPipelineWork( *Request.pBackend->pWork );
    const xbool Ready = render_PollPipelineCreate( Request, &Pipeline ) == RENDER_PIPELINE_CREATE_READY;
    render_ReleasePipelineCreate( Request );

    return Ready;
}

//==============================================================================
//  PIPELINE STATUS
//==============================================================================

u64 sdleng_GetPipelineCreateCalls( void )
{
    return x_AtomicLoadRelaxed( &s_PipelineCreateCalls );
}

//==============================================================================

u64 render_GetPipelineSerial( const render_pipeline& Pipeline )
{
    return Pipeline.pBackend && Pipeline.pBackend->pOwner == &Pipeline && Pipeline.pBackend->pEntry
               ? Pipeline.pBackend->pEntry->Serial
               : 0;
}

//==============================================================================

void render_DestroyPipeline( render_pipeline& Pipeline )
{
    render_pipeline_backend* pBackend = Pipeline.pBackend;

    if( !pBackend )
    {
        return;
    }

    if( pBackend->pOwner != &Pipeline )
    {
        ASSERT( FALSE );
        Pipeline.pBackend = NULL;
        return;
    }

    if( s_pBoundPipeline == pBackend->pEntry )
    {
        s_pBoundPipeline = NULL;
        sdleng_ResetShaderBindings();
    }

    sdleng_ClearGraphicsPipelineDebug( pBackend );
    sdleng_UnlinkBackend( s_pPipelineHandleList, pBackend, s_PipelineHandleCount );

    if( pBackend->pEntry )
    {
        ASSERT( pBackend->pEntry->HandleCount > 0 );
        pBackend->pEntry->HandleCount--;
    }

    pBackend->pEntry = NULL;
    pBackend->pOwner = NULL;
    sdlstate_DeletePipelineHandle( pBackend );
    Pipeline.pBackend = NULL;

    sdlstate_EvictUnusedPipelineEntries();
}

//==============================================================================

u32 render_GetPipelineCacheCount( void )
{
    return s_PipelineCount;
}

//==============================================================================

void render_TrimPipelineCache( void )
{
    render_pipeline_cache_entry** ppEntry = &s_pPipelineCache;

    while( *ppEntry )
    {
        render_pipeline_cache_entry* pEntry = *ppEntry;

        if( pEntry->HandleCount || pEntry->PendingCount )
        {
            ppEntry = &pEntry->pNext;
            continue;
        }

        *ppEntry      = pEntry->pNext;
        pEntry->pNext = NULL;

        if( pEntry == s_pBoundPipeline )
        {
            s_pBoundPipeline = NULL;
        }

        sdlstate_ReleasePipelineCacheEntry( pEntry );
        delete pEntry;
        --s_PipelineCount;
    }
}

//==============================================================================
//  PIPELINE BINDING
//==============================================================================

xbool render_BindPipeline( const render_pipeline& Pipeline )
{
    if( !Pipeline.pBackend ||
        ( Pipeline.pBackend->pOwner != &Pipeline ) ||
        !Pipeline.pBackend->pEntry ||
        !Pipeline.pBackend->pEntry->pPipeline )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        x_DebugMsg( "SDLState: no active render pass for pipeline bind\n" );
        return FALSE;
    }

    render_pipeline_cache_entry* pEntry = Pipeline.pBackend->pEntry;

    if( ++s_NextPipelineUse == 0 )
    {
        s_NextPipelineUse = 1;
    }

    pEntry->LastUse                 = s_NextPipelineUse;
    const xbool             bIssued = ( s_pBoundPipeline != pEntry );
    static xprofile_counter PipelineBindRequestMetric =
        x_GetProfiler().RegisterCounter( "PipelineBindRequests", "Renderer" );

    static xprofile_counter PipelineBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "PipelineBindIssued", "Renderer" );

    PipelineBindRequestMetric.Add();

    if( bIssued )
    {
        PipelineBindIssuedMetric.Add();
        const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
        const xtick Start       = bFineTiming ? x_GetTime() : 0;
        SDL_BindGPUGraphicsPipeline( pRenderPass, pEntry->pPipeline );
        const xtick End = bFineTiming ? x_GetTime() : 0;

        if( bFineTiming )
        {
            static xprofile_zone PipelineBindMetric =
                x_GetProfiler().RegisterZone( "PipelineBindAPI", "RendererAPI" );

            PipelineBindMetric.Record( End - Start );
        }

        s_pBoundPipeline = pEntry;
    }

    sdleng_SetGraphicsPipelineDebug( Pipeline.pBackend );

    return TRUE;
}

//==============================================================================

void sdleng_ResetPipelineBinding( void )
{
    s_pBoundPipeline = NULL;
}

//==============================================================================

u32 sdleng_GetPipelineCount( void )
{
    return s_PipelineCount;
}

//==============================================================================

u32 sdleng_GetPipelineHandleCount( void )
{
    return s_PipelineHandleCount;
}

//==============================================================================
//  SAMPLER OBJECTS
//==============================================================================

static
rstate_sampler_native* sdlstate_CreateGPUSampler( rstate_sampler_desc Desc, rstate_texture_filtering Filtering )
{
    if( Desc.UseTextureFiltering )
    {
        Desc.MaxAnisotropy = 1;

        if( Filtering == RSTATE_TEXTURE_FILTER_BILINEAR )
        {
            Desc.Filter = RSTATE_FILTER_BILINEAR;
        }
        else if( Filtering == RSTATE_TEXTURE_FILTER_TRILINEAR )
        {
            Desc.Filter = RSTATE_FILTER_LINEAR;
        }
        else
        {
            Desc.Filter        = RSTATE_FILTER_ANISOTROPIC;
            Desc.MaxAnisotropy = 2u << ( Filtering - RSTATE_TEXTURE_FILTER_ANISOTROPIC_2 );
        }
    }

    SDL_GPUFilter            MinFilter;
    SDL_GPUFilter            MagFilter;
    SDL_GPUSamplerMipmapMode MipmapMode;
    xbool                    bAnisotropy;
    xbool                    bCompare;

    if( !sdlstate_ToSDLSamplerFilter( Desc.Filter, MinFilter, MagFilter, MipmapMode, bAnisotropy, bCompare ) )
    {
        return NULL;
    }

    SDL_GPUSamplerAddressMode AddressU;
    SDL_GPUSamplerAddressMode AddressV;
    SDL_GPUSamplerAddressMode AddressW;

    if( !sdlstate_ToSDLAddressMode( Desc.AddressU, AddressU ) ||
        !sdlstate_ToSDLAddressMode( Desc.AddressV, AddressV ) ||
        !sdlstate_ToSDLAddressMode( Desc.AddressW, AddressW ) )
    {
        return NULL;
    }

    SDL_GPUSamplerCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof( CreateInfo ) );
    CreateInfo.min_filter        = MinFilter;
    CreateInfo.mag_filter        = MagFilter;
    CreateInfo.mipmap_mode       = MipmapMode;
    CreateInfo.address_mode_u    = AddressU;
    CreateInfo.address_mode_v    = AddressV;
    CreateInfo.address_mode_w    = AddressW;
    CreateInfo.mip_lod_bias      = Desc.MipLODBias;
    CreateInfo.max_anisotropy    = bAnisotropy ? (f32)MAX( Desc.MaxAnisotropy, 1u ) : 1.0f;
    CreateInfo.compare_op        = sdlstate_ToSDLCompareOp( bCompare ? Desc.ComparisonFunc : RSTATE_COMPARE_ALWAYS );
    CreateInfo.min_lod           = Desc.MinLOD;
    CreateInfo.max_lod           = Desc.MaxLOD;
    CreateInfo.enable_anisotropy = bAnisotropy ? true : false;
    CreateInfo.enable_compare    = bCompare ? true : false;

    if( CreateInfo.mip_lod_bias == 0.0f )
    {
        CreateInfo.mip_lod_bias = 0.0f;
    }

    if( CreateInfo.min_lod == 0.0f )
    {
        CreateInfo.min_lod = 0.0f;
    }

    if( CreateInfo.max_lod == 0.0f )
    {
        CreateInfo.max_lod = 0.0f;
    }

    for( rstate_sampler_native* pNative = s_pNativeSamplers; pNative; pNative = pNative->pNext )
    {
        if( x_memcmp( &pNative->Key, &CreateInfo, sizeof( CreateInfo ) ) == 0 )
        {
            ++pNative->References;
            return pNative;
        }
    }

    SDL_PropertiesID Props = 0;

    if( Desc.pDebugName && Desc.pDebugName[0] )
    {
        Props = SDL_CreateProperties();

        if( !Props )
        {
            sdleng_LogError( "SDLState", "SDL_CreateProperties" );
            return NULL;
        }

        if( !SDL_SetStringProperty( Props, SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, Desc.pDebugName ) )
        {
            sdleng_LogError( "SDLState", "SDL_SetStringProperty" );
            SDL_DestroyProperties( Props );
            return NULL;
        }
    }

    const SDL_GPUSamplerCreateInfo Key = CreateInfo;
    CreateInfo.props                   = Props;
    SDL_GPUSampler* pSampler           = SDL_CreateGPUSampler( g_pSDLGPUDevice, &CreateInfo );

    if( Props )
    {
        SDL_DestroyProperties( Props );
    }

    if( !pSampler )
    {
        sdleng_LogError( "SDLState", "SDL_CreateGPUSampler" );
        return NULL;
    }

    rstate_sampler_native* pNative = new rstate_sampler_native;
    pNative->pSampler              = pSampler;
    pNative->Key                   = Key;
    pNative->References            = 1;
    pNative->pNext                 = s_pNativeSamplers;
    s_pNativeSamplers              = pNative;

    return pNative;
}

//==============================================================================

xbool rstate_CreateSampler( rstate_sampler& Sampler, const rstate_sampler_desc& Desc )
{
    if( Sampler.pBackend && sdleng_InRenderPass() )
    {
        return FALSE;
    }

    if( !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    rstate_sampler_native* pNative = sdlstate_CreateGPUSampler( Desc, s_TextureFiltering );

    if( !pNative )
    {
        return FALSE;
    }

    rstate_sampler_backend* pBackend = new rstate_sampler_backend;

    if( !pBackend )
    {
        sdlstate_ReleaseNativeSampler( pNative );
        return FALSE;
    }

    rstate_sampler_backend* pOldBackend = Sampler.pBackend;

    pBackend->pNative  = pNative;
    pBackend->pSampler = pNative->pSampler;
    pBackend->Desc     = Desc;

    if( Desc.pDebugName )
    {
        pBackend->DebugName = Desc.pDebugName;
    }

    pBackend->Desc.pDebugName = (const char*)pBackend->DebugName;
    pBackend->pOwner          = &Sampler;
    Sampler.pBackend          = pBackend;

    if( pOldBackend )
    {
        sdleng_ResetShaderBindings();
        sdleng_UnlinkBackend( s_pSamplerList, pOldBackend, s_SamplerCount );
        sdlstate_ReleaseSamplerBackend( pOldBackend );
        delete pOldBackend;
    }

    sdleng_LinkBackend( s_pSamplerList, pBackend, s_SamplerCount );

    return TRUE;
}

//==============================================================================

xbool rstate_SetTextureFiltering( rstate_texture_filtering Filtering )
{
    if( ( Filtering < RSTATE_TEXTURE_FILTER_BILINEAR ) ||
        ( Filtering > RSTATE_TEXTURE_FILTER_ANISOTROPIC_16 ) )
    {
        return FALSE;
    }

    if( Filtering == s_TextureFiltering )
    {
        return TRUE;
    }

    if( sdleng_InRenderPass() )
    {
        return FALSE;
    }

    struct sampler_update
    {
        rstate_sampler_backend* pBackend;
        rstate_sampler_native*  pNative;
    };

    xarray<sampler_update> Updates;
    Updates.SetCapacity( s_SamplerCount );

    for( rstate_sampler_backend* pBackend = s_pSamplerList; pBackend; pBackend = pBackend->pNext )
    {
        if( !pBackend->Desc.UseTextureFiltering )
        {
            continue;
        }

        rstate_sampler_native* pNative = sdlstate_CreateGPUSampler( pBackend->Desc, Filtering );

        if( !pNative )
        {
            for( s32 i = 0; i < Updates.GetCount(); ++i )
            {
                sdlstate_ReleaseNativeSampler( Updates[i].pNative );
            }

            return FALSE;
        }

        sampler_update& Update = Updates.Append();
        Update.pBackend        = pBackend;
        Update.pNative         = pNative;
    }

    sdleng_ResetShaderBindings();

    for( s32 i = 0; i < Updates.GetCount(); ++i )
    {
        sampler_update& Update = Updates[i];
        sdlstate_ReleaseNativeSampler( Update.pBackend->pNative );
        Update.pBackend->pNative  = Update.pNative;
        Update.pBackend->pSampler = Update.pNative->pSampler;
    }

    s_TextureFiltering = Filtering;

    return TRUE;
}

//==============================================================================

rstate_texture_filtering rstate_GetTextureFiltering( void )
{
    return s_TextureFiltering;
}

//==============================================================================

xbool rstate_CreateSampler( rstate_sampler& Sampler, rstate_sampler_preset Preset, const char* pDebugName )
{
    rstate_sampler_desc Desc = rstate_GetSamplerDesc( Preset );
    Desc.pDebugName          = pDebugName;

    return rstate_CreateSampler( Sampler, Desc );
}

//==============================================================================

void rstate_DestroySampler( rstate_sampler& Sampler )
{
    rstate_sampler_backend* pBackend = Sampler.pBackend;

    if( !pBackend )
    {
        return;
    }

    if( sdleng_InRenderPass() )
    {
        ASSERTS( FALSE, "Cannot destroy a sampler during an active render pass" );
        return;
    }

    sdleng_ResetShaderBindings();
    sdleng_UnlinkBackend( s_pSamplerList, pBackend, s_SamplerCount );
    sdlstate_ReleaseSamplerBackend( pBackend );
    delete pBackend;

    Sampler.pBackend = NULL;
}

//==============================================================================
//  DYNAMIC PASS STATE
//==============================================================================

xbool render_SetBlendConstants( const f32 Color[4] )
{
    if( !Color )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
    {
        return FALSE;
    }

    SDL_FColor SDLColor;
    SDLColor.r = Color[0];
    SDLColor.g = Color[1];
    SDLColor.b = Color[2];
    SDLColor.a = Color[3];
    static xprofile_counter DynamicStateRequests =
        x_GetProfiler().RegisterCounter( "DynamicStateRequests", "Renderer" );

    DynamicStateRequests.Add();

    if( g_SDLDynamicState.BlendValid &&
        x_memcmp( &g_SDLDynamicState.Blend, &SDLColor, sizeof( SDLColor ) ) == 0 )
    {
        return TRUE;
    }

    static xprofile_counter DynamicStateCountMetric =
        x_GetProfiler().RegisterCounter( "DynamicStateCalls", "Renderer" );

    DynamicStateCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_SetGPUBlendConstants( pRenderPass, SDLColor );
    g_SDLDynamicState.Blend      = SDLColor;
    g_SDLDynamicState.BlendValid = TRUE;
    const xtick End              = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone DynamicStateMetric =
            x_GetProfiler().RegisterZone( "DynamicStateAPI", "RendererAPI" );

        DynamicStateMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool render_SetStencilRef( u8 StencilRef )
{
    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        return FALSE;
    }

    static xprofile_counter DynamicStateRequests =
        x_GetProfiler().RegisterCounter( "DynamicStateRequests", "Renderer" );

    DynamicStateRequests.Add();

    if( g_SDLDynamicState.StencilValid && g_SDLDynamicState.Stencil == StencilRef )
    {
        return TRUE;
    }

    static xprofile_counter DynamicStateCountMetric =
        x_GetProfiler().RegisterCounter( "DynamicStateCalls", "Renderer" );

    DynamicStateCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_SetGPUStencilReference( pRenderPass, StencilRef );
    g_SDLDynamicState.Stencil      = StencilRef;
    g_SDLDynamicState.StencilValid = TRUE;
    const xtick End                = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone DynamicStateMetric =
            x_GetProfiler().RegisterZone( "DynamicStateAPI", "RendererAPI" );

        DynamicStateMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
