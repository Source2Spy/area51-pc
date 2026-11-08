//=========================================================================
//
//  PrimitiveStorage.hpp
//
//=========================================================================

#ifndef PRIMITIVE_STORAGE_HPP
#define PRIMITIVE_STORAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_array.hpp"
#include "a51_shader_contract.hpp"
#include "../../GraphicsPipelineCache.hpp"
#include "../../ShaderContract.hpp"

#include <stddef.h>

#include "e_Engine.hpp"
#include "e_RenderState.hpp"
#include "e_VRAM.hpp"

#include "../../PrimitiveBatch.hpp"
#include "../../FrameGraph/FrameGraph.hpp"
#include "../RenderBackendContext.hpp"
#include "../Storage/TransientGeometryStorage.hpp"

namespace render
{

//=========================================================================
//  CLASS
//=========================================================================

class PrimitiveStorage
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum
    {
        MAX_BATCH_VERTICES = render::MAX_PRIMITIVE_VERTICES
    };

    using Vertex = render::primitive_vertex;
    static_assert( sizeof( primitive_sprite_instance ) == 56, "Sprite instance stride must match HLSL" );
    static_assert( offsetof( primitive_sprite_instance, Color ) == 12, "Sprite color offset" );
    static_assert( offsetof( primitive_sprite_instance, Right ) == 16, "Sprite right offset" );
    static_assert( offsetof( primitive_sprite_instance, Up ) == 28, "Sprite up offset" );
    static_assert( offsetof( primitive_sprite_instance, UVRect ) == 40, "Sprite UV offset" );

    //-------------------------------------------------------------------------

    struct DrawConstants : PrimitiveDrawConstants
    {
        DrawConstants( void );
    };

    //-------------------------------------------------------------------------

    static_assert( sizeof( Vertex ) == 24, "Primitive Vertex layout must match HLSL" );
    static_assert( offsetof( Vertex, Position ) == 0, "Primitive position offset must match PSO" );
    static_assert( offsetof( Vertex, Color ) == 12, "Primitive color offset must match PSO" );
    static_assert( offsetof( Vertex, UV ) == 16, "Primitive UV offset must match PSO" );
    static_assert( sizeof( DrawConstants ) == 160, "Primitive constants must match HLSL" );

    //-------------------------------------------------------------------------

    struct PassDesc
    {
        rtarget_format  ColorFormat;
        rtarget_format  DepthFormat;
        u32             SampleCount;
        u32             Width  = 1;
        u32             Height = 1;
        xbool           WriteSceneDepth;
        render_fog_mode SceneFogMode;
        render_fog_mode BloomFogMode;

        PassDesc( void );
    };

    //-------------------------------------------------------------------------

    struct PipelineDesc
    {
        PassDesc              Pass;
        xbool                 InstancedSprite = FALSE;
        primitive_output_mode Output          = PRIMITIVE_OUTPUT_COLOR;
        primitive_blend_mode  SourceBlend     = PRIMITIVE_BLEND_OPAQUE;
        shader_topology       Topology;
        rstate_blend_preset   Blend;
        rstate_depth_preset   Depth;
        rstate_raster_preset  Raster;
        rstate_sampler_preset Sampler;
        const char*           pDebugName;

        PipelineDesc( void );
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    struct BatchDesc
    {
        PipelineDesc                  Pipeline;
        DrawConstants                 Constants;
        const shader_resource*        pTexture;
        render::primitive_blend_mode  Blend;
        render::primitive_output_mode Output;
        f32                           SortDepth;

        BatchDesc( void );
    };

protected:

    //-------------------------------------------------------------------------

    struct PixelProgram
    {
        shader  Shader;
        shader* pVertex    = NULL;
        u32     DrawVertex = 0xFFFFFFFFu;
        u32     Draw       = 0xFFFFFFFFu;
        u32     Texture    = 0xFFFFFFFFu;
        u32     Scene      = 0xFFFFFFFFu;
        u32     Fog        = 0xFFFFFFFFu;
        u32     Palette    = 0xFFFFFFFFu;
    };

    //-------------------------------------------------------------------------

    struct QueuedDraw
    {
        render_pipeline* pPipeline = NULL;
        PixelProgram*    pProgram  = NULL;
        s32              DescIndex;
        f32              SortDepth;
        s32              StartIndex;
        s32              BaseVertex;
        s32              IndexCount;
        s32              FirstSprite = 0;
        s32              SpriteCount = 0;

        QueuedDraw( void );
    };

public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    PrimitiveStorage( void );

    static ShaderProgramKey ResolveProgram( const PipelineDesc& Desc );

    void  Init( const RenderBackendContext& Context );
    void  SetContext( const RenderBackendContext& Context );
    void  Kill( void );
    void  BeginRender( void );
    xbool IsInitialized( void ) const;

    //-------------------------------------------------------------------------
    //  SPRITE SUBMISSION
    //-------------------------------------------------------------------------

    xarray<primitive_sprite_instance>& GetSpriteScratch( void );
    xbool SubmitSprites( const BatchDesc& Desc, const primitive_sprite_instance* pSprites, s32 Count );
    xbool IsSpriteDraw( s32 DrawIndex ) const;
    void  BeginSpriteBatches( void );
    s32   AppendSpriteBatch( s32 DrawIndex, s32 PreviousDrawIndex );
    xbool UploadSpriteBatches( FrameGraphBuilder& Graph );

    //-------------------------------------------------------------------------
    //  PRIMITIVE SUBMISSION
    //-------------------------------------------------------------------------

    xbool BeginBatch( const BatchDesc& DescData );
    xbool SubmitBatch( const Vertex* pVertices, s32 nVertices, const u16* pIndices, s32 nIndices );
    xbool EndBatch( void );
    xbool HasQueuedDraws( void ) const;
    xbool UploadQueuedDraws( FrameGraphBuilder& Graph );

    //-------------------------------------------------------------------------
    //  QUEUED DRAWS
    //-------------------------------------------------------------------------

    s32                           GetQueuedDrawCount( void ) const;
    render::primitive_blend_mode  GetQueuedDrawBlend( s32 DrawIndex ) const;
    render::primitive_output_mode GetQueuedDrawOutput( s32 DrawIndex ) const;
    xbool                         QueuedDrawReadsSceneColor( s32 DrawIndex ) const;
    f32                           GetQueuedDrawSortDepth( s32 DrawIndex ) const;
    xbool                         PrepareQueuedDraw( s32 DrawIndex, const PassDesc& Pass );
    void DeclareQueuedResources( FrameGraphBuilder& Graph, RenderPassId Pass, s32 DrawIndex ) const;
    xbool DrawQueuedDraw( s32 DrawIndex, const PassDesc& PassData, const BackendPassInputs& Inputs );
    void  DiscardQueuedDraws( void );
    xbool QueuePipeline( const PipelineDesc& Desc );
    void  ReleaseLevelPipelines( void );

    static xbool BuildDrawConstants( DrawConstants& Out, const matrix4& LocalToWorld, const view& RenderView );

protected:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool         LoadShaders( void );
    PixelProgram* GetProgram( const PipelineDesc& Desc, xbool IsPrewarm = FALSE );
    void          DestroyPipelines( void );
    xbool         CreateSamplers( void );
    void          DestroySamplers( void );
    xbool         CreateWhiteTexture( void );

    render_pipeline* GetOrCreatePipeline( const PipelineDesc& Desc, xbool isPrewarm = FALSE );
    xbool            BuildPipelineDesc( render_pipeline_desc& Out, const PipelineDesc& Desc, xbool IsPrewarm = FALSE );
    xbool            BindResources( const BatchDesc& Desc, s32 DrawIndex, const BackendPassInputs& Inputs );
    static xbool     ValidatePipelineState( const PipelineDesc& Desc );
    static xbool     ValidatePipelineDesc( const PipelineDesc& Desc );

    s32  AddBatchDesc( const BatchDesc& Desc );
    void ClearBatch( void );
    void ClearQueuedDraws( void );

protected:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    TransientGeometryStorage m_VertexBuffer;
    shader                   m_VertexShaders[2][2];

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    PixelProgram  m_Programs[2][3][5][2][2];
    PixelProgram* m_pActiveProgram = NULL;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    GraphicsPipelineCache             m_Pipelines;
    rstate_sampler                    m_Samplers[RSTATE_SAMPLER_PRESET_COUNT];
    vram_texture                      m_WhiteTexture;
    xbool                             m_isInitialized;
    const RenderBackendContext*       m_pContext;
    xarray<primitive_sprite_instance> m_SpriteScratch;
    xarray<primitive_sprite_instance> m_Sprites;
    xarray<primitive_sprite_instance> m_SortedSprites;
    rbuffer                           m_SpriteBuffer;
    rbuffer                           m_SpriteIndices;
    u32                               m_SpriteCapacity = 0;

    BatchDesc m_BatchDesc;
    s32       m_BatchStartVertex;
    s32       m_iBatchStartIndex;
    s32       m_nBatchVertices;
    s32       m_nBatchIndices;
    xbool     m_isBatchOpen;

    xarray<Vertex> m_QueuedVertices;
    xarray<u16>    m_QueuedIndices;
    // Ordering is owned by ForwardEncoder; this array only retains the
    // immutable upload ranges and draw state produced by each submission.
    xarray<BatchDesc>  m_BatchDescs;
    xarray<QueuedDraw> m_QueuedDraws;
    xbool              m_areQueuedDrawsUploaded;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
PrimitiveStorage::DrawConstants::DrawConstants( void )
{
    LocalToClip.Identity();
    LocalToView.Identity();
    Reserved[0] = Reserved[1] = 0;
    DistortionScale = Padding = 0;
    PassDimensions.Set( 1, 1, 1, 1 );
}

//=========================================================================

inline
PrimitiveStorage::PassDesc::PassDesc( void )
    : ColorFormat( RTARGET_FORMAT_COUNT ),
      DepthFormat( RTARGET_FORMAT_COUNT ),
      SampleCount( 1 ),
      WriteSceneDepth( FALSE ),
      SceneFogMode( RENDER_FOG_NONE ),
      BloomFogMode( RENDER_FOG_NONE )
{
}

//=========================================================================

inline
PrimitiveStorage::PipelineDesc::PipelineDesc( void )
    : Pass(),
      Topology( SHADER_TOPOLOGY_TRIANGLE_LIST ),
      Blend( RSTATE_BLEND_PRESET_ALPHA ),
      Depth( RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE ),
      Raster( RSTATE_RASTER_PRESET_SOLID_NO_CULL ),
      Sampler( RSTATE_SAMPLER_PRESET_LINEAR_CLAMP ),
      pDebugName( NULL )
{
}

//=========================================================================

inline
PrimitiveStorage::BatchDesc::BatchDesc( void )
    : Pipeline(),
      Constants(),
      pTexture( NULL ),
      Blend( render::PRIMITIVE_BLEND_OPAQUE ),
      Output( render::PRIMITIVE_OUTPUT_COLOR ),
      SortDepth( 0.0f )
{
}

//=========================================================================

inline
PrimitiveStorage::QueuedDraw::QueuedDraw( void )
    : DescIndex( -1 ),
      SortDepth( 0.0f ),
      StartIndex( 0 ),
      BaseVertex( 0 ),
      IndexCount( 0 )
{
}

//=========================================================================

inline
void PrimitiveStorage::SetContext( const RenderBackendContext& Context )
{
    m_pContext = &Context;
}

//=========================================================================

inline
xbool PrimitiveStorage::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
xarray<primitive_sprite_instance>& PrimitiveStorage::GetSpriteScratch( void )
{
    return m_SpriteScratch;
}

//=========================================================================

inline
xbool PrimitiveStorage::HasQueuedDraws( void ) const
{
    return m_QueuedDraws.GetCount() > 0;
}

//=========================================================================

inline
s32 PrimitiveStorage::GetQueuedDrawCount( void ) const
{
    return m_QueuedDraws.GetCount();
}

} // namespace render

#endif // PRIMITIVE_STORAGE_HPP
//=========================================================================
