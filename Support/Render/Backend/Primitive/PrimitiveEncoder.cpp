//=========================================================================
//
//  PrimitiveEncoder.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PrimitiveEncoder.hpp"

#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

PrimitiveEncoder::PrimitiveEncoder( void )
    : m_isSubmissionActive( FALSE ),
      m_pPrimitives( NULL )
{
}

//=========================================================================

void PrimitiveEncoder::Init( PrimitiveStorage& Primitives )
{
    ASSERT( !m_pPrimitives );
    m_pPrimitives = &Primitives;
    ResetState();
}

//=========================================================================

void PrimitiveEncoder::Kill( void )
{
    ASSERTS( !m_isSubmissionActive, "Primitive submission survived renderer shutdown" );
    ResetState();
    m_pPrimitives = NULL;
}

//=========================================================================

void PrimitiveEncoder::ResetState( void )
{
    m_isSubmissionActive = FALSE;
}

//=========================================================================

xbool PrimitiveEncoder::BeginSubmission( void )
{
    ASSERTS( !m_isSubmissionActive, "Primitive submission is already active" );

    if( m_isSubmissionActive )
    {
        return FALSE;
    }

    m_isSubmissionActive = TRUE;

    return TRUE;
}

//=========================================================================

xbool PrimitiveEncoder::EndSubmission( void )
{
    ASSERTS( m_isSubmissionActive, "Primitive submission is not active" );

    if( !m_isSubmissionActive )
    {
        return FALSE;
    }

    ResetState();

    return TRUE;
}

//=========================================================================

void PrimitiveEncoder::DiscardSubmission( void )
{
    ResetState();
}

//=========================================================================

xbool PrimitiveEncoder::BlendPresetFromMode( render::primitive_blend_mode Blend, rstate_blend_preset& Preset )
{
    switch( Blend )
    {
        case render::PRIMITIVE_BLEND_OPAQUE:
        {
            Preset = RSTATE_BLEND_PRESET_NONE;
            return TRUE;
        }

        case render::PRIMITIVE_BLEND_ALPHA:
        {
            Preset = RSTATE_BLEND_PRESET_PREMULT_ALPHA;
            return TRUE;
        }

        case render::PRIMITIVE_BLEND_ADDITIVE:
        {
            Preset = RSTATE_BLEND_PRESET_PREMULT_ADD;
            return TRUE;
        }

        case render::PRIMITIVE_BLEND_SUBTRACTIVE:
        {
            Preset = RSTATE_BLEND_PRESET_PREMULT_SUB;
            return TRUE;
        }

        case render::PRIMITIVE_BLEND_INTENSITY:
        {
            Preset = RSTATE_BLEND_PRESET_INTENSITY;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//=========================================================================

xbool PrimitiveEncoder::TopologyFromMode( render::primitive_topology Topology, shader_topology& Result )
{
    switch( Topology )
    {
        case render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        {
            Result = SHADER_TOPOLOGY_TRIANGLE_LIST;
            return TRUE;
        }

        case render::PRIMITIVE_TOPOLOGY_LINE_LIST:
        {
            Result = SHADER_TOPOLOGY_LINE_LIST;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//=========================================================================

xbool PrimitiveEncoder::DepthPresetFromMode( render::primitive_depth_mode Depth, rstate_depth_preset& Preset )
{
    switch( Depth )
    {
        case render::PRIMITIVE_DEPTH_DISABLED:
        {
            Preset = RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE;
            return TRUE;
        }

        case render::PRIMITIVE_DEPTH_READ_ONLY:
        {
            Preset = RSTATE_DEPTH_PRESET_NO_WRITE;
            return TRUE;
        }

        case render::PRIMITIVE_DEPTH_READ_WRITE:
        {
            Preset = RSTATE_DEPTH_PRESET_NORMAL;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//=========================================================================

xbool PrimitiveEncoder::RasterPresetFromMode( render::primitive_raster_mode Raster, rstate_raster_preset& Preset )
{
    switch( Raster )
    {
        case render::PRIMITIVE_RASTER_SOLID:
        {
            Preset = RSTATE_RASTER_PRESET_SOLID;
            return TRUE;
        }

        case render::PRIMITIVE_RASTER_SOLID_NO_CULL:
        {
            Preset = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
            return TRUE;
        }

        case render::PRIMITIVE_RASTER_WIREFRAME:
        {
            Preset = RSTATE_RASTER_PRESET_WIRE;
            return TRUE;
        }

        case render::PRIMITIVE_RASTER_WIREFRAME_NO_CULL:
        {
            Preset = RSTATE_RASTER_PRESET_WIRE_NO_CULL;
            return TRUE;
        }

        case render::PRIMITIVE_RASTER_COLLISION_BIASED:
        {
            Preset = RSTATE_RASTER_PRESET_DEPTH_BIASED;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//=========================================================================

xbool PrimitiveEncoder::SamplerPresetFromMode( render::primitive_sampler_mode Sampler, rstate_sampler_preset& Preset )
{
    switch( Sampler )
    {
        case render::PRIMITIVE_SAMPLER_LINEAR_WRAP:
        {
            Preset = RSTATE_SAMPLER_PRESET_LINEAR_WRAP;
            return TRUE;
        }

        case render::PRIMITIVE_SAMPLER_LINEAR_CLAMP:
        {
            Preset = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
            return TRUE;
        }

        case render::PRIMITIVE_SAMPLER_ANISOTROPIC_WRAP:
        {
            Preset = RSTATE_SAMPLER_PRESET_MATERIAL_WRAP;
            return TRUE;
        }

        case render::PRIMITIVE_SAMPLER_ANISOTROPIC_CLAMP:
        {
            Preset = RSTATE_SAMPLER_PRESET_MATERIAL_CLAMP;
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

//=========================================================================

xbool PrimitiveEncoder::BuildPipelineState( PrimitiveStorage::PipelineDesc& Out, const primitive_draw_desc& Desc )
{
    if( ( Desc.Topology < render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) ||
        ( Desc.Topology >= render::PRIMITIVE_TOPOLOGY_COUNT ) || ( Desc.Blend < render::PRIMITIVE_BLEND_OPAQUE ) ||
        ( Desc.Blend >= render::PRIMITIVE_BLEND_COUNT ) || ( Desc.Depth < render::PRIMITIVE_DEPTH_DISABLED ) ||
        ( Desc.Depth >= render::PRIMITIVE_DEPTH_COUNT ) || ( Desc.Raster < render::PRIMITIVE_RASTER_SOLID ) ||
        ( Desc.Raster >= render::PRIMITIVE_RASTER_COUNT ) || ( Desc.Sampler < render::PRIMITIVE_SAMPLER_LINEAR_WRAP ) ||
        ( Desc.Sampler >= render::PRIMITIVE_SAMPLER_COUNT ) || ( Desc.Output < render::PRIMITIVE_OUTPUT_COLOR ) ||
        ( Desc.Output >= render::PRIMITIVE_OUTPUT_COUNT ) ||
        ( ( Desc.Blend != render::PRIMITIVE_BLEND_OPAQUE ) && ( Desc.Depth == render::PRIMITIVE_DEPTH_READ_WRITE ) ) ||
        ( ( Desc.Output == render::PRIMITIVE_OUTPUT_DISTORTION ) && ( Desc.Blend != render::PRIMITIVE_BLEND_ALPHA ) ) ||
        !x_isvalid( Desc.DistortionScale ) || ( Desc.DistortionScale < 0.0f ) )
    {
        return FALSE;
    }

    shader_topology       Topology;
    rstate_blend_preset   Blend;
    rstate_depth_preset   Depth;
    rstate_raster_preset  Raster;
    rstate_sampler_preset Sampler;

    if( !TopologyFromMode( Desc.Topology, Topology ) || !BlendPresetFromMode( Desc.Blend, Blend ) ||
        !DepthPresetFromMode( Desc.Depth, Depth ) || !RasterPresetFromMode( Desc.Raster, Raster ) ||
        !SamplerPresetFromMode( Desc.Sampler, Sampler ) )
    {
        return FALSE;
    }

    if( Desc.Output != render::PRIMITIVE_OUTPUT_GLOW )
    {
        switch( Blend )
        {
            case RSTATE_BLEND_PRESET_NONE:
            {
                Blend = RSTATE_BLEND_PRESET_NONE_RGB;
                break;
            }
            case RSTATE_BLEND_PRESET_PREMULT_ALPHA:
            {
                Blend = RSTATE_BLEND_PRESET_PREMULT_ALPHA_RGB;
                break;
            }
            case RSTATE_BLEND_PRESET_PREMULT_ADD:
            {
                Blend = RSTATE_BLEND_PRESET_PREMULT_ADD_RGB;
                break;
            }
            case RSTATE_BLEND_PRESET_PREMULT_SUB:
            {
                Blend = RSTATE_BLEND_PRESET_PREMULT_SUB_RGB;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    Out             = PrimitiveStorage::PipelineDesc();
    Out.Output      = Desc.Output;
    Out.SourceBlend = Desc.Blend;
    Out.Topology    = Topology;
    Out.Blend       = Blend;
    Out.Depth       = Depth;
    Out.Raster      = Raster;
    Out.Sampler     = Sampler;
    Out.pDebugName  = "PrimitivePipeline";

    return TRUE;
}

//=========================================================================

xbool PrimitiveEncoder::BuildBatchDesc( PrimitiveStorage::BatchDesc&       Out,
                                        const render::primitive_draw_desc& Desc,
                                        const matrix4&                     LocalToWorld,
                                        const view&                        RenderView ) const
{
    Out = PrimitiveStorage::BatchDesc();

    if( !BuildPipelineState( Out.Pipeline, Desc ) )
    {
        return FALSE;
    }

    const shader_resource* pTexture = NULL;

    if( Desc.pTexture )
    {
        pTexture = Desc.pTexture->GetShaderResource();

        if( !pTexture || !*pTexture )
        {
            return FALSE;
        }
    }

    Out.pTexture                  = pTexture;
    Out.Blend                     = Desc.Blend;
    Out.Output                    = Desc.Output;
    Out.Constants.DistortionScale = Desc.DistortionScale;

    return PrimitiveStorage::BuildDrawConstants( Out.Constants, LocalToWorld, RenderView );
}

//=========================================================================

xbool PrimitiveEncoder::ComputeSortDepth( f32&                            OutDepth,
                                          const matrix4&                  LocalToWorld,
                                          const view&                     RenderView,
                                          const render::primitive_vertex* pVertices,
                                          s32                             nVertices )
{
    if( !pVertices || ( nVertices <= 0 ) )
    {
        return FALSE;
    }

    bbox Bounds( pVertices[0].Position );

    for( s32 i = 1; i < nVertices; ++i )
    {
        Bounds += pVertices[i].Position;
    }

    const vector3 WorldCenter = LocalToWorld * Bounds.GetCenter();
    const vector3 ViewCenter  = RenderView.GetW2V() * WorldCenter;
    OutDepth                  = ViewCenter.GetZ();

    return x_isvalid( OutDepth );
}

//=========================================================================

xbool PrimitiveEncoder::SubmitSprites( const primitive_draw_desc&       Desc,
                                       const view&                      View,
                                       const primitive_sprite_instance* pSprites,
                                       s32                              Count )
{
    if( !m_isSubmissionActive || !m_pPrimitives || Desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST )
    {
        return FALSE;
    }

    PrimitiveStorage::BatchDesc Batch;

    if( !BuildBatchDesc( Batch, Desc, View.GetV2W(), View ) )
    {
        return FALSE;
    }

    Batch.Pipeline.InstancedSprite = TRUE;

    return m_pPrimitives->SubmitSprites( Batch, pSprites, Count );
}

//=========================================================================

xbool PrimitiveEncoder::SubmitPrimitives( const render::primitive_draw_desc& Desc,
                                          const matrix4&                     LocalToWorld,
                                          const view&                        RenderView,
                                          const render::primitive_vertex*    pVertices,
                                          s32                                nVertices,
                                          const u16*                         pIndices,
                                          s32                                nIndices )
{
    ASSERTS( m_isSubmissionActive, "Primitive geometry submitted outside primitive mode" );
    ASSERT( m_pPrimitives );

    const s32 IndicesPerPrimitive = ( Desc.Topology == render::PRIMITIVE_TOPOLOGY_LINE_LIST ) ? 2 : 3;

    if( !m_isSubmissionActive || !m_pPrimitives || !pVertices || !pIndices || ( nVertices <= 0 ) ||
        ( nVertices > PrimitiveStorage::MAX_BATCH_VERTICES ) || ( nIndices <= 0 ) ||
        ( ( nIndices % IndicesPerPrimitive ) != 0 ) )
    {
        return FALSE;
    }

    PrimitiveStorage::BatchDesc BatchDesc;

    if( !BuildBatchDesc( BatchDesc, Desc, LocalToWorld, RenderView ) ||
        !ComputeSortDepth( BatchDesc.SortDepth, LocalToWorld, RenderView, pVertices, nVertices ) )
    {
        return FALSE;
    }

    if( !m_pPrimitives->BeginBatch( BatchDesc ) )
    {
        return FALSE;
    }

    const xbool Submitted = m_pPrimitives->SubmitBatch( pVertices, nVertices, pIndices, nIndices );
    const xbool Closed    = m_pPrimitives->EndBatch();

    return Submitted && Closed;
}

//=========================================================================

void PrimitiveEncoder::BuildDepthRectPipelineState( PrimitiveStorage::PipelineDesc& Pipeline )
{
    Pipeline.Topology   = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Pipeline.Blend      = RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE;
    Pipeline.Depth      = RSTATE_DEPTH_PRESET_WRITE_ALWAYS;
    Pipeline.Raster     = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
    Pipeline.Sampler    = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    Pipeline.pDebugName = "PrimitiveDepthRectPipeline";
}

//=========================================================================

xbool PrimitiveEncoder::SubmitDepthRect( const irect& Rect, f32 Depth, const view& RenderView )
{
    ASSERTS( m_isSubmissionActive, "Depth rectangle submitted outside primitive mode" );
    ASSERT( m_pPrimitives );

    if( !m_isSubmissionActive || !m_pPrimitives || !x_isvalid( Depth ) || ( Depth < 0.0f ) || ( Depth > 1.0f ) ||
        ( Rect.r <= Rect.l ) || ( Rect.b <= Rect.t ) )
    {
        return FALSE;
    }

    s32 X0;
    s32 Y0;
    s32 X1;
    s32 Y1;
    RenderView.GetViewport( X0, Y0, X1, Y1 );

    const f32 Width  = static_cast<f32>( X1 - X0 );
    const f32 Height = static_cast<f32>( Y1 - Y0 );

    if( ( Width <= 0.0f ) || ( Height <= 0.0f ) )
    {
        return FALSE;
    }

    const f32                      Left        = ( static_cast<f32>( Rect.l - X0 ) / Width ) * 2.0f - 1.0f;
    const f32                      Right       = ( static_cast<f32>( Rect.r - X0 ) / Width ) * 2.0f - 1.0f;
    const f32                      Top         = 1.0f - ( static_cast<f32>( Rect.t - Y0 ) / Height ) * 2.0f;
    const f32                      Bottom      = 1.0f - ( static_cast<f32>( Rect.b - Y0 ) / Height ) * 2.0f;
    const render::primitive_vertex Vertices[4] = {
        render::primitive_vertex( vector3( Left, Top, Depth ), vector2( 0.0f, 0.0f ), XCOLOR_WHITE ),
        render::primitive_vertex( vector3( Right, Top, Depth ), vector2( 1.0f, 0.0f ), XCOLOR_WHITE ),
        render::primitive_vertex( vector3( Right, Bottom, Depth ), vector2( 1.0f, 1.0f ), XCOLOR_WHITE ),
        render::primitive_vertex( vector3( Left, Bottom, Depth ), vector2( 0.0f, 1.0f ), XCOLOR_WHITE )
    };
    const u16 Indices[6] = { 0, 1, 2, 2, 3, 0 };

    PrimitiveStorage::BatchDesc BatchDesc;
    BuildDepthRectPipelineState( BatchDesc.Pipeline );
    BatchDesc.Output = render::PRIMITIVE_OUTPUT_COLOR;
    BatchDesc.Constants.LocalToClip.Identity();
    BatchDesc.SortDepth = 0.0f;

    if( !m_pPrimitives->BeginBatch( BatchDesc ) )
    {
        return FALSE;
    }

    const xbool Submitted =
        m_pPrimitives->SubmitBatch( Vertices, ARRAYSIZE( Vertices ), Indices, ARRAYSIZE( Indices ) );
    const xbool Closed = m_pPrimitives->EndBatch();

    return Submitted && Closed;
}

//=========================================================================

} // namespace render
