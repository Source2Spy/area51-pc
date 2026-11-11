//=========================================================================
//
//  PrimitiveEncoder.hpp
//
//=========================================================================

#ifndef PRIMITIVE_ENCODER_HPP
#define PRIMITIVE_ENCODER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PrimitiveStorage.hpp"
#include "../../PrimitiveBatch.hpp"

namespace render
{

//=========================================================================
//  CLASS
//=========================================================================

class PrimitiveEncoder
{
public:

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    PrimitiveEncoder( void );

    void Init( PrimitiveStorage& Primitives );
    void Kill( void );

    //-------------------------------------------------------------------------
    //  SUBMISSION
    //-------------------------------------------------------------------------

    xbool BeginSubmission( void );
    xbool EndSubmission( void );
    void  DiscardSubmission( void );
    xbool IsSubmissionActive( void ) const;
    xbool SubmitSprites( const primitive_draw_desc&       Desc,
                         const view&                      View,
                         const primitive_sprite_instance* pSprites,
                         s32                              Count );

    xbool SubmitPrimitives( const render::primitive_draw_desc& Desc,
                            const matrix4&                     LocalToWorld,
                            const view&                        RenderView,
                            const render::primitive_vertex*    pVertices,
                            s32                                nVertices,
                            const u16*                         pIndices,
                            s32                                nIndices );
    xbool SubmitDepthRect( const irect& Rect, f32 Depth, const view& RenderView );

    //-------------------------------------------------------------------------
    //  PIPELINE HELPERS
    //-------------------------------------------------------------------------

    static void  BuildDepthRectPipelineState( PrimitiveStorage::PipelineDesc& Out );
    static xbool BuildPipelineState( PrimitiveStorage::PipelineDesc& Out, const primitive_draw_desc& Desc );

protected:

    //-------------------------------------------------------------------------
    //  HELPER FUNCTIONS
    //-------------------------------------------------------------------------

    static xbool ComputeSortDepth( f32&                            OutDepth,
                                   const matrix4&                  LocalToWorld,
                                   const view&                     RenderView,
                                   const render::primitive_vertex* pVertices,
                                   s32                             nVertices );
    static xbool TopologyFromMode( render::primitive_topology Topology, shader_topology& Result );
    static xbool BlendPresetFromMode( render::primitive_blend_mode Blend, rstate_blend_preset& Preset );
    static xbool DepthPresetFromMode( render::primitive_depth_mode Depth, rstate_depth_preset& Preset );
    static xbool RasterPresetFromMode( render::primitive_raster_mode Raster, rstate_raster_preset& Preset );
    static xbool SamplerPresetFromMode( render::primitive_sampler_mode Sampler, rstate_sampler_preset& Preset );

    void  ResetState( void );
    xbool BuildBatchDesc( PrimitiveStorage::BatchDesc&       Out,
                          const render::primitive_draw_desc& Desc,
                          const matrix4&                     LocalToWorld,
                          const view&                        RenderView ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xbool             m_isSubmissionActive;
    PrimitiveStorage* m_pPrimitives;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
xbool PrimitiveEncoder::IsSubmissionActive( void ) const
{
    return m_isSubmissionActive;
}

} // namespace render

#endif // PRIMITIVE_ENCODER_HPP
//=========================================================================
