//=========================================================================
//
//  DecalEncoder.hpp
//
//  Forward renderer for lit surface decals.
//
//=========================================================================

#ifndef DECAL_ENCODER_HPP
#define DECAL_ENCODER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../DecalBatch.hpp"
#include "../../GraphicsPipelineCache.hpp"
#include "../../FrameGraph/FrameGraph.hpp"
#include "../Frame/FrameCompiler.hpp"
#include "../Storage/TransientGeometryStorage.hpp"
#include "../RenderBackendContext.hpp"

#include "e_Engine.hpp"
#include "e_RenderState.hpp"
#include "e_VRAM.hpp"

#include <stddef.h>
#include "a51_shader_contract.hpp"

class cubemap;

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class DecalEncoder
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    using Vertex = render::decal_vertex;

    //-------------------------------------------------------------------------

    struct PassDesc
    {
        rtarget_format  ColorFormat;
        rtarget_format  DepthFormat;
        u32             SampleCount;
        xbool           WriteSceneDepth;
        xbool           EnableShadows = TRUE;
        render_fog_mode SceneFogMode;
        render_fog_mode BloomFogMode;

        PassDesc( void );
    };

    //-------------------------------------------------------------------------

    struct FrameConstants : DecalFrameConstants
    {
        FrameConstants( void );
    };

    static_assert( sizeof( Vertex ) == 24, "Decal submission vertex layout changed" );
    static_assert( offsetof( Vertex, Position ) == 0, "Decal submission position offset changed" );
    static_assert( offsetof( Vertex, Color ) == 12, "Decal submission color offset changed" );
    static_assert( offsetof( Vertex, UV ) == 16, "Decal submission UV offset changed" );
    static_assert( sizeof( FrameConstants ) == 160, "Decal constants must match HLSL" );

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    DecalEncoder( void );

    void  Init( const RenderBackendContext& Context );
    void  SetContext( const RenderBackendContext& Context );
    void  Kill( void );
    void  BeginRender( void );
    xbool BeginSubmission( void );
    xbool EndSubmission( void );
    xbool IsSubmissionOpen( void ) const;
    xbool IsInitialized( void ) const;
    xbool Reserve( s32 nVertices, s32 nIndices, s32 nDraws );

    //-------------------------------------------------------------------------
    //  SUBMISSION
    //-------------------------------------------------------------------------

    xbool Submit( const render::decal_draw_desc& Desc,
                  const cubemap*                 pCubeMap,
                  const view&                    RenderView,
                  const Vertex*                  pVertices,
                  s32                            nVertices,
                  const u16*                     pIndices,
                  s32                            nIndices );

    xbool RequestProjectionTextures( void ) const;
    xbool UploadQueuedDraws( const FrameCompiler& Geometry, FrameGraphBuilder& Graph );
    xbool PrepareQueuedDraw( s32 DrawIndex, const PassDesc& Pass );
    void DeclareQueuedResources( FrameGraphBuilder& Graph, RenderPassId Pass, s32 DrawIndex ) const;
    xbool DrawQueuedDraw( s32 DrawIndex, const PassDesc& PassData, const BackendPassInputs& Inputs );
    void  DiscardQueuedDraws( void );
    xbool QueuePipeline( decal_blend_mode Blend, u32 Flags, const PassDesc& Pass );
    void  ReleaseLevelPipelines( void );

    s32                     GetQueuedDrawCount( void ) const;
    render::decal_blend_mode GetQueuedDrawBlend( s32 DrawIndex ) const;
    xbool                   IsQueuedDrawGlow( s32 DrawIndex ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct PipelineDesc
    {
        PassDesc             Pass;
        decal_blend_mode     SourceBlend  = DECAL_BLEND_ALPHA;
        u32                  ShaderFlags  = 0;
        LocalLightingMode    LightingMode = LocalLightingMode::Ambient;
        rstate_blend_preset  Blend;
        rstate_depth_preset  Depth;
        rstate_raster_preset Raster;
        const char*          pDebugName;

        PipelineDesc( void );
    };

    //-------------------------------------------------------------------------

    struct ShaderBindings
    {
        u32 FrameConstantsVertex;
        u32 FrameConstantsPixel;
        u32 ProjectionConstantsPixel;
        u32 DecalTexturePixel;
        u32 EnvironmentCubeTexturePixel;
        u32 FaceShadowTexturePixel;
        u32 FaceShadowDepthTexturePixel;
        u32 ProjectionAtlasTexturePixel;
        u32 LightingDataPixel;
        u32 ProjectionLightingDataPixel;
        u32 AmbientDataPixel;
        u32 ShadowMatricesPixel;
        u32 ShadowDataPixel;
        u32 FogConstantsPixel;
        u32 FogPalettePixel;

        ShaderBindings( void );
    };

    //-------------------------------------------------------------------------

    struct GpuVertex
    {
        vector3p Position;
        u32      Color;
        vector2  UV;
        vector3p Normal;
        u32      LightingIndex;
    };

    static_assert( sizeof( GpuVertex ) == 40, "Decal GPU vertex layout must match HLSL" );
    static_assert( offsetof( GpuVertex, Position ) == 0, "Decal GPU position offset must match PSO" );
    static_assert( offsetof( GpuVertex, Color ) == 12, "Decal GPU color offset must match PSO" );
    static_assert( offsetof( GpuVertex, UV ) == 16, "Decal GPU UV offset must match PSO" );
    static_assert( offsetof( GpuVertex, Normal ) == 24, "Decal normal offset must match PSO" );
    static_assert( offsetof( GpuVertex, LightingIndex ) == 36, "Decal lighting offset must match PSO" );

    struct DecalProgram
    {
        shader         Pixel;
        shader*        pVertex = NULL;
        ShaderBindings Bindings;
    };

    //-------------------------------------------------------------------------

    struct QueuedDraw
    {
        render_pipeline*         pPipeline = NULL;
        DecalProgram*            pProgram  = NULL;
        FrameConstants           Constants;
        const shader_resource*   pTexture;
        const shader_resource*   pEnvironmentCube;
        render::decal_blend_mode Blend;
        u32                      Flags;
        s32                      StartIndex;
        s32                      BaseVertex;
        s32                      IndexCount;
        s32                      VertexCount;
        LocalLightingMode        LightingMode;

        QueuedDraw( void );
    };

private:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool         LoadShaders( void );
    DecalProgram* GetProgram( const PipelineDesc& Desc, xbool IsPrewarm = FALSE );
    xbool         ResolveBindings( const shader& Vertex, const shader& Pixel, ShaderBindings& Bindings );
    xbool         CreateSamplers( void );
    xbool         CreateBlackCube( void );
    static xbool BuildPipelineState( PipelineDesc&     Desc,
                                     decal_blend_mode  Blend,
                                     u32               Flags,
                                     const PassDesc&   Pass,
                                     LocalLightingMode Lighting );
    void          DestroySamplers( void );
    void          DestroyPipelines( void );

    xbool        BuildSceneLighting( cb_geom_lighting& Lighting, const bbox& Bounds, xcolor Ambient );
    static xbool BlendPresetFromMode( render::decal_blend_mode Blend, rstate_blend_preset& Preset );
    xbool CopyLightingData( GeomLightingConstants&           Destination,
                            GeomProjectionLightingConstants& Projection,
                            GeomAmbientConstants&            Ambient,
                            const cb_geom_lighting&          Source,
                            const FrameCompiler&             Geometry ) const;
    xbool        BuildProjectionData( void );
    xbool BindResources( const QueuedDraw&        Draw,
                         s32                      DrawIndex,
                         const PassDesc&          PassData,
                         const BackendPassInputs& Inputs );

    static xbool     ValidatePass( const PipelineDesc& Desc );
    xbool            BuildPipelineDesc( render_pipeline_desc& Out, const PipelineDesc& Desc, xbool IsPrewarm = FALSE );
    render_pipeline* GetOrCreatePipeline( const PipelineDesc& Desc, xbool isPrewarm = FALSE );

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    TransientGeometryStorage m_VertexBuffer;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    shader        m_Vertices[2][2][2];
    DecalProgram  m_Programs[4][2][2][2][2][3];
    DecalProgram* m_pActiveProgram = NULL;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    GraphicsPipelineCache                   m_Pipelines;
    rstate_sampler                          m_Samplers[RSTATE_SAMPLER_PRESET_COUNT];
    vram_texture                            m_BlackCubeTexture;
    rbuffer                                 m_LightingBuffer;
    rbuffer                                 m_ProjectionLightingBuffer;
    rbuffer                                 m_AmbientBuffer;
    u32                                     m_ProjectionLightingCapacity = 0;
    u32                                     m_AmbientCapacity            = 0;
    xarray<GeomProjectionLightingConstants> m_ProjectionLighting;
    xarray<GeomAmbientConstants>            m_AmbientLighting;
    u32                                     m_LightingCapacity;

    ProjectionTextureConstants    m_ProjectionConstants;
    xarray<GeomLightingConstants> m_FrameLighting;
    xarray<GpuVertex>             m_QueuedVertices;
    xarray<u16>                   m_QueuedIndices;
    xarray<QueuedDraw>            m_QueuedDraws;
    xarray<cb_geom_lighting>      m_QueuedLighting;
    s32                           m_RequestedVertices;
    s32                           m_RequestedIndices;
    s32                           m_RequestedDraws;
    xbool                         m_areQueuedDrawsUploaded;
    xbool                         m_isSubmissionOpen;
    xbool                         m_isInitialized;
    const RenderBackendContext*   m_pContext;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
DecalEncoder::PassDesc::PassDesc( void )
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
void DecalEncoder::SetContext( const RenderBackendContext& Context )
{
    m_pContext = &Context;
}

//=========================================================================

inline
xbool DecalEncoder::IsSubmissionOpen( void ) const
{
    return m_isSubmissionOpen;
}

//=========================================================================

inline
xbool DecalEncoder::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
DecalEncoder::PipelineDesc::PipelineDesc( void )
    : Pass(),
      Blend( RSTATE_BLEND_PRESET_ALPHA ),
      Depth( RSTATE_DEPTH_PRESET_NO_WRITE ),
      Raster( RSTATE_RASTER_PRESET_SOLID_NO_CULL ),
      pDebugName( NULL )
{
}

} // namespace render

#endif // DECAL_ENCODER_HPP
//=========================================================================
