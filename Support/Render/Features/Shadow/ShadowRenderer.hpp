//=========================================================================
//
//  ShadowRenderer.hpp
//
//  Shadow-map execution backend.
//
//=========================================================================

#ifndef SHADOW_RENDERER_HPP
#define SHADOW_RENDERER_HPP

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "x_array.hpp"

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"

#include "../../ShadowMapAtlas.hpp"
#include "../../ShadowTileCache.hpp"
#include "../../GraphicsPipelineCache.hpp"
#include "../../FrameGraph/FrameGraph.hpp"

#include "e_Engine.hpp"
#include "a51_shader_contract.hpp"

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class material;
class texture;
struct geom_draw_item;
struct dynamic_geom_shadow_draw;

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

struct ShadowCasterSubmission;

//=========================================================================
//  CONSTANT BUFFER LAYOUTS
//=========================================================================

struct CbShadowMapData
{
    vector4 FaceShadowLightPosRadius[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightDirRadial[MAX_SHADOW_SOURCES]; // xyz = direction, w = inverse attenuation range
    // x = coverage cosine, y = receive near z, z = inner radius, w = point-face flag
    vector4 FaceShadowLightData[MAX_SHADOW_SOURCES];
    vector4 FaceShadowProjectionData[MAX_SHADOW_SOURCES]; // xy = near/far z, z = world texel scale, w = resolution
    vector4 FaceShadowAtlasRect[MAX_SHADOW_SOURCES];      // xy/zw = moment-texel inset minimum/maximum
    vector4 PointShadowLightPosRadius[MAX_SHADOW_LIGHTS];
    // x = inner radius, y = receive near z, z = inverse attenuation range
    vector4 PointShadowLightData[MAX_SHADOW_LIGHTS];
    vector4 PointShadowLightParams[MAX_SHADOW_LIGHTS]; // x = first face source, y = face count
    u32     FaceShadowCount;
    u32     PointShadowLightCount;
    f32     Padding[2];
    vector4 ShadowParams;        // x = depth-atlas texel size, y = normal bias, z = seam blend width, w = filter type
    vector4 ShadowFilterParams;  // x = sampled-atlas texel size, y/z = EVSM exponents, w = light-bleed reduction
};

static_assert( sizeof( CbShadowMapData ) == 5552, "ShadowMapData layout must match HLSL" );

//=========================================================================
//  SHADOW PASS BACKEND
//=========================================================================

class ShadowRenderer
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct CasterStats
    {
        u32 InputDrawCount;
        u32 PreparedDrawCount;
        u32 FrustumRejectedDrawCount;
        u32 PacketCount;
        u32 GpuDrawCount;
        u32 RigidInstanceCount;
        u32 SkinInstanceCount;
        u32 RigidPacketCount;
        u32 SkinPacketCount;
        u32 AlphaPacketCount;
        u32 SourceStateChangeCount;
        u32 SkinPaletteCount;
        u32 SkinPaletteMatrixCount;
        u32 MaxPacketInstanceCount;
        u32 RigidIndirectRunCount;
        u32 RigidIndirectCommandCount;
        u32 SkinIndirectRunCount;
        u32 SkinIndirectCommandCount;
        u32 BufferReallocationCount;
        u64 UploadBytes;
        u64 SubmittedIndexCount;
        u32 UpdatedTileCount;
        u32 ReusedTileCount;

        CasterStats( void );
    };

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    ShadowRenderer( void );
    ~ShadowRenderer( void );

    void  Init( void );
    void  Kill( void );
    xbool QueueGeometryPipeline( xbool Skin, xbool Alpha, xbool TwoSided );
    xbool QueueDynamicPipeline( void );
    xbool QueueFilterPipelines( void );
    void  ReleaseLevelPipelines( void );
    xbool IsInitialized( void ) const;

    //-------------------------------------------------------------------------
    //  SHADOW CASTER PIPELINE
    //-------------------------------------------------------------------------

    void  BeginFrame( s32 ScopeId );
    void  AbortFrame( void );
    void  EndFrame( void );
    xbool PrepareCasters( const xarray<const geom_draw_item*>&    Draws,
                          const xarray<dynamic_geom_shadow_draw>& DynamicDraws,
                          s32                                     ScopeId );
    xbool DeclareGraph( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    void  DeclareConsumer( FrameGraphBuilder& Graph, RenderPassId PassData ) const;

    //-------------------------------------------------------------------------
    //  RUNTIME QUERIES
    //-------------------------------------------------------------------------

    RenderResourceHandle GetShadowSampleAtlasHandle( void ) const;
    RenderResourceHandle GetShadowDepthAtlasHandle( void ) const;
    f32                  GetShadowNormalBiasTexels( void ) const;
    f32                  GetShadowSeamBlendTexels( void ) const;
    f32                  GetAtlasTexelSize( void ) const;
    vector4              GetShadowFilterParams( void ) const;
    const CasterStats&   GetLastCasterStats( void ) const;

private:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool BuildCasterSubmission( FrameGraphBuilder& Graph );
    xbool RequestPipelines( s32 Caster, xbool Alpha, xbool TwoSided, xbool IsPrewarm );

    //-------------------------------------------------------------------------
    //  INTERNAL HELPERS
    //-------------------------------------------------------------------------

    xbool        CreateClearGeometry( void );
    xbool        CreateAtlasResources( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    void         PrepareTileUpdates( const RenderFrameContext& Frame );
    xbool        ClearUpdatedTiles( void );
    xbool        ExecuteCasterPass( RenderPassContext& Context );
    xbool        ExecuteEVSMConvertPass( RenderPassContext& Context );
    xbool        ExecuteEVSMBlurPass( RenderPassContext& Context, xbool Horizontal );
    static xbool ExecuteCasterPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend );
    static xbool ExecuteEVSMConvertPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend );
    static xbool ExecuteEVSMBlurHorizontalPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend );
    static xbool ExecuteEVSMBlurVerticalPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend );
    xbool        SetShadowCastConstants( u32 UniformSlot, const ShadowMapAtlas::ShadowSource& Source,
                                        xbool CoverageUsesTextureAlpha = FALSE );
    xbool        SetShadowAlphaConstants( const texture& DiffuseTexture, xbool isPunchThru, u8 UOffset, u8 VOffset );
    void         ApplySource( s32 SourceIndex, s32 CasterShader, xbool isAlphaTest, xbool isTwoSided );
    void         ApplyDynamicSource( s32 SourceIndex );

private:

    //-------------------------------------------------------------------------
    //  RUNTIME STATE
    //-------------------------------------------------------------------------

    xbool                   m_isInitialized;
    s32                     m_CurrentSource;
    s32                     m_CurrentCasterVariant;
    s32                     m_ShadowAtlasSize;
    s32                     m_ShadowMomentAtlasSize;
    CasterStats             m_LastCasterStats;
    ShadowCasterSubmission* m_pSubmission;
    s32                     m_ScopeId;
    s32                     m_SubmissionScopeId;
    RenderResourceHandle    m_ShadowAtlas;
    RenderResourceHandle    m_ShadowMomentAtlas;
    RenderResourceHandle    m_ShadowMomentTempAtlas;
    u64                     m_HistoryResetToken = 0;
    ShadowTileCache         m_TileCache;
    xbool                   m_isTileUpdatesExecuted;

    //-------------------------------------------------------------------------
    //  GPU RESOURCES
    //-------------------------------------------------------------------------

    shader                m_RigidVertexShader;
    shader                m_RigidMaskedVertexShader;
    shader                m_SkinVertexShader;
    shader                m_SkinMaskedVertexShader;
    shader                m_DynamicVertexShader;
    shader                m_DynamicPixelShader;
    shader                m_OpaqueDepthPixelShader;
    shader                m_AlphaDepthPixelShader;
    shader                m_EvsmVertexShader;
    shader                m_EvsmConvertPixelShader;
    shader                m_EvsmBlurHorizontalPixelShader;
    shader                m_EvsmBlurVerticalPixelShader;
    GraphicsPipelineCache m_CasterPipelines;
    GraphicsPipelineCache m_EvsmPipelines;
    render_pipeline*      m_pCasterHandles[10]    = {};
    render_pipeline*      m_pSpotCasterHandles[9] = {};
    render_pipeline*      m_pEvsmHandles[3]       = {};
    rstate_sampler        m_DiffuseSampler;
    rstate_sampler        m_EvsmPointSampler;
    rstate_sampler        m_EvsmLinearSampler;
    rstate_raster_desc    m_ShadowSingleSidedRasterizerDesc;
    rstate_raster_desc    m_ShadowTwoSidedRasterizerDesc;
    rbuffer               m_RigidInstanceBuffer;
    rbuffer               m_ClearVertices;
    rbuffer               m_ClearWorld;
    rbuffer               m_RigidInstanceIndexBuffer;
    rbuffer               m_RigidIndirectBuffer;
    rbuffer               m_SkinBoneBuffer;
    rbuffer               m_SkinBoneBaseBuffer;
    rbuffer               m_SkinIndirectBuffer;
    rbuffer               m_DynamicVertexBuffer;
    rbuffer               m_DynamicIndexBuffer;
    xarray<u32>           m_InstanceIndexScratch;
    u32                   m_RigidInstanceCapacity;
    u32                   m_RigidInstanceIndexCapacity;
    u32                   m_RigidIndirectCapacity;
    u32                   m_SkinBoneCapacity;
    u32                   m_SkinBoneBaseCapacity;
    u32                   m_SkinIndirectCapacity;
    u32                   m_DynamicVertexCapacity;
    u32                   m_DynamicIndexCapacity;

    //-------------------------------------------------------------------------
    //  REFLECTED SHADER SLOTS
    //-------------------------------------------------------------------------

    u32 m_RigidCastConstantsSlot;
    u32 m_RigidInstanceSlot;
    u32 m_SkinCastConstantsSlot;
    u32 m_SkinBoneSlot;
    u32 m_DynamicCastConstantsSlot;
    u32 m_DynamicDiffuseTextureSlot;
    u32 m_DynamicDamageTextureSlot;
    u32 m_AlphaConstantsSlot;
    u32 m_AlphaTextureSlot;
    u32 m_EvsmConvertConstantsSlot;
    u32 m_EvsmConvertTextureSlot;
    u32 m_EvsmBlurHorizontalConstantsSlot;
    u32 m_EvsmBlurHorizontalTextureSlot;
    u32 m_EvsmBlurVerticalConstantsSlot;
    u32 m_EvsmBlurVerticalTextureSlot;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
ShadowRenderer::CasterStats::CasterStats( void )
    : InputDrawCount( 0 ),
      PreparedDrawCount( 0 ),
      FrustumRejectedDrawCount( 0 ),
      PacketCount( 0 ),
      GpuDrawCount( 0 ),
      RigidInstanceCount( 0 ),
      SkinInstanceCount( 0 ),
      RigidPacketCount( 0 ),
      SkinPacketCount( 0 ),
      AlphaPacketCount( 0 ),
      SourceStateChangeCount( 0 ),
      SkinPaletteCount( 0 ),
      SkinPaletteMatrixCount( 0 ),
      MaxPacketInstanceCount( 0 ),
      RigidIndirectRunCount( 0 ),
      RigidIndirectCommandCount( 0 ),
      SkinIndirectRunCount( 0 ),
      SkinIndirectCommandCount( 0 ),
      BufferReallocationCount( 0 ),
      UploadBytes( 0 ),
      SubmittedIndexCount( 0 ),
      UpdatedTileCount( 0 ),
      ReusedTileCount( 0 )
{
}

//=========================================================================

inline
xbool ShadowRenderer::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
RenderResourceHandle ShadowRenderer::GetShadowDepthAtlasHandle( void ) const
{
    return m_ShadowAtlas;
}

} // namespace render

#endif // SHADOW_RENDERER_HPP
