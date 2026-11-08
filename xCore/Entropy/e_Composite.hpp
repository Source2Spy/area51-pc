//==============================================================================
//
//  e_Composite.hpp
//
//==============================================================================

#ifndef E_COMPOSITE_HPP
#define E_COMPOSITE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#include "e_RenderState.hpp"
#include "e_RenderTarget.hpp"

struct rtarget;
struct shader;
struct shader_resource;
struct render_pipeline_desc;

struct composite_extra_source
{
    u32                   ResourceSemantic;
    const shader_resource* pResource;
    rstate_sampler_preset SamplerMode;

    composite_extra_source( void )
        : ResourceSemantic( 0xFFFFFFFFu ), pResource( NULL ), SamplerMode( RSTATE_SAMPLER_PRESET_POINT_CLAMP )
    {
    }
};

//==============================================================================
//  TYPES
//==============================================================================

enum composite_blend_mode
{
    COMPOSITE_BLEND_ALPHA,
    COMPOSITE_BLEND_ADDITIVE,
    COMPOSITE_BLEND_MULTIPLY,
    COMPOSITE_BLEND_OVERLAY,
    COMPOSITE_BLEND_COPY,
    COMPOSITE_BLEND_ALPHA_RGB,
    COMPOSITE_BLEND_COPY_RGB,
    COMPOSITE_BLEND_ADDITIVE_RGB,
    COMPOSITE_BLEND_CLAMP_NEGATIVE_RGB,

    COMPOSITE_BLEND_COUNT
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

void    composite_Init          ( void );
void    composite_Kill          ( void );

// Uses the same descriptor and shader instances as composite submission.
xbool   composite_GetPipelineDesc( render_pipeline_desc& Desc,
                                   composite_blend_mode BlendMode,
                                   rtarget_format ColorFormat,
                                   rtarget_format DepthFormat = RTARGET_FORMAT_COUNT,
                                   u32 SampleCount = 1,
                                   const shader* pCustomShader = NULL );

render_pipeline* composite_QueuePipeline( const render_pipeline_desc& Desc );
xbool   composite_FinishPipeline( render_pipeline& Pipeline );
xbool   composite_FinishPrewarm( void );

xbool   composite_PrewarmPipeline( composite_blend_mode BlendMode,
                                   rtarget_format       ColorFormat,
                                   rtarget_format       DepthFormat = RTARGET_FORMAT_COUNT,
                                   u32                  SampleCount = 1,
                                   const shader*        pCustomShader = NULL );

xbool   composite_Draw          ( const shader& PixelShader, composite_blend_mode BlendMode );

xbool   composite_ClampNegative ( void );

xbool   composite_Blit          ( const rtarget&         Source,
                                  composite_blend_mode   BlendMode = COMPOSITE_BLEND_ALPHA,
                                  f32                    Alpha = 1.0f,
                                  const shader*          pCustomShader = NULL,
                                  rstate_sampler_preset  SamplerMode = RSTATE_SAMPLER_PRESET_POINT_CLAMP,
                                  u32                    SourceSemantic = 0xFFFFFFFFu,
                                  const composite_extra_source* pExtraSources = NULL,
                                  u32                    nExtraSources = 0 );

//==============================================================================
#endif // E_COMPOSITE_HPP
//==============================================================================
