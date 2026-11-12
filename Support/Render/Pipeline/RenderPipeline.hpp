//=========================================================================
//
//  RenderPipeline.hpp
//
//=========================================================================

#ifndef RENDER_PIPELINE_HPP
#define RENDER_PIPELINE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../FrameGraph/FrameGraph.hpp"
#include "../RenderScale.hpp"
#include "../Features/Output/OutputFeature.hpp"
#include "../Render.hpp"
#include "../GeomDraw.hpp"
#include "../RenderLighting.hpp"
#include "../LightSystem.hpp"
#include "../ProjectionTextureRegistry.hpp"
#include "../ShadowMapAtlas.hpp"
#include "../Backend/RenderBackend.hpp"
#include "../Features/Geom/GeomFeature.hpp"
#include "../Features/Cloth/ClothFeature.hpp"
#include "../Features/Cmaa2/Cmaa2Feature.hpp"
#include "../Features/XeGtao/XeGtaoFeature.hpp"
#include "../Features/Shadow/ShadowFeature.hpp"
#include "../Features/PostProcess/PostProcessFeature.hpp"
#include "e_Shader.hpp"

class cubemap;

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class RenderSubmission
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderSubmission( void );

    void Begin( void );
    void Seal( void );
    void Reset( void );
    void ValidateOpen( const char* pOperation ) const;

    xbool          IsOpen( void ) const;
    xbool          IsSealed( void ) const;
    void           MarkSceneSubmitted( void );
    xbool          IsSceneSubmitted( void ) const;
    void           SetEnvironmentCubeMap( const cubemap* pCubeMap );
    const cubemap* GetEnvironmentCubeMap( void ) const;
    void AddRigidInstanceSimple(
        xhandle                          hInst,
        const u32*                       pColor,
        const matrix4*                   pL2W,
        const bbox&                      WorldBBox,
        u32                              Flags,
        render::RenderContributionPolicy ContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT,
        xcolor                           Ambient = XCOLOR_WHITE );
    void AddRigidInstance( xhandle                          hInst,
                           const u32*                       pColor,
                           const matrix4*                   pL2W,
                           u64                              Mask,
                           u32                              Flags,
                           s32                              Alpha,
                           render::RenderContributionPolicy ContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT,
                           xcolor                           Ambient = XCOLOR_WHITE );
    void AddRigidInstance( xhandle                          hInst,
                           const u32*                       pColor,
                           const matrix4*                   pL2W,
                           u64                              Mask,
                           u32                              VTextureMask,
                           u32                              Flags,
                           s32                              Alpha,
                           render::RenderContributionPolicy ContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT,
                           xcolor                           Ambient = XCOLOR_WHITE );
    void AddSkinInstance( xhandle                          hInst,
                          const matrix4*                   pBone,
                          s32                              BoneCount,
                          u64                              Mask,
                          u32                              VTextureMask,
                          u32                              Flags,
                          const xcolor&                    Ambient,
                          render::RenderContributionPolicy ContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT );
    void AddSkinInstanceDistorted( xhandle        hInst,
                                   const matrix4* pBone,
                                   s32            BoneCount,
                                   u64            Mask,
                                   u32            Flags,
                                   const radian3& NormalRot,
                                   xcolor         Ambient );
    void                             AddDynamicGeom( const dynamic_geom_draw& Draw );
    xarray<geom_draw_item>&          Geom( void );
    xarray<dynamic_geom_draw>&       DynamicGeom( void );
    const xarray<geom_draw_item>&    GetGeom( void ) const;
    const xarray<dynamic_geom_draw>& GetDynamicGeom( void ) const;
    dynamic_geom_storage             GetDynamicStorage( void ) const;
    u32&                             NextGeomSequence( void );
    matrix4*                         StoreTransform( const matrix4& Transform );

private:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    void RebindDynamicPointers( void );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<geom_draw_item>      m_Geom;
    xarray<dynamic_geom_draw>   m_DynamicGeom;
    xarray<dynamic_geom_vertex> m_DynamicVertexStorage;
    xarray<u16>                 m_DynamicIndexStorage;
    xarray<u8>                  m_DynamicDamageStorage;
    xarray<s32>                 m_DynamicVertexOffsets;
    xarray<s32>                 m_DynamicIndexOffsets;
    xarray<s32>                 m_DynamicDamageOffsets;
    xarray<matrix4>             m_TransformStorage;
    const cubemap*              m_pEnvironmentCubeMap;
    u32                         m_NextGeomSequence;
    xbool                       m_isSceneSubmitted;
    xbool                       m_isOpen;
    xbool                       m_isSealed;
};

//-------------------------------------------------------------------------

// Shadow draws belong to the consuming scope even when collected before graph setup.
class RenderShadowSubmission
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderShadowSubmission( void );

    void                              Begin( void );
    void                              End( void );
    void                              Reset( void );
    xbool                             IsActive( void ) const;
    s32                               GetSourceCount( void ) const;
    s32&                              SourceCount( void );
    u32&                              NextSequence( void );
    xarray<geom_draw_item>&           Draws( void );
    xarray<const geom_draw_item*>&    OrderedDraws( void );
    xarray<dynamic_geom_shadow_draw>& DynamicDraws( void );
    void                              ClearDynamicDraws( void );

    void AddPointSource( const matrix4& L2W,
                         radian         Fov,
                         f32            LightRadius,
                         f32            LightFalloff,
                         s32            ShadowMapResolution,
                         s32            ShadowPriority,
                         f32            ShadowScore,
                         s32            DynamicLightIndex );
    void AddSpotSource( const matrix4& L2W,
                        radian         Fov,
                        f32            LightRadius,
                        f32            LightFalloff,
                        s32            ShadowMapResolution,
                        s32            ShadowPriority,
                        f32            ShadowScore,
                        s32            DynamicLightIndex );
    void     AddRigidCasterSimple( xhandle hInst, const matrix4* pL2W, u64 ShadowSourceMask );
    void     AddRigidCaster( xhandle hInst, const matrix4* pL2W, u64 Mask, u64 ShadowSourceMask );
    void     AddSkinCaster( xhandle hInst, const matrix4* pBone, s32 nBone, u64 Mask, u64 ShadowSourceMask );
    void     AddDynamicCaster( const dynamic_geom_shadow_draw& Draw );
    matrix4* StoreTransform( const matrix4& Transform );

private:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    void RebindDynamicPointers( void );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<geom_draw_item>           m_ShadowDraws;
    xarray<const geom_draw_item*>    m_OrderedShadowDraws;
    xarray<dynamic_geom_shadow_draw> m_DynamicShadowDraws;
    xarray<dynamic_geom_vertex>      m_DynamicVertexStorage;
    xarray<u16>                      m_DynamicIndexStorage;
    xarray<u8>                       m_DynamicDamageStorage;
    xarray<s32>                      m_DynamicVertexOffsets;
    xarray<s32>                      m_DynamicIndexOffsets;
    xarray<s32>                      m_DynamicDamageOffsets;
    xarray<matrix4>                  m_TransformStorage;
    u32                              m_NextSequence;
    s32                              m_SourceCount;
    xbool                            m_isActive;
};

//-------------------------------------------------------------------------

struct RenderPreparedScene
{
    RenderPreparedScene( void );

    void  Reset( void );
    void  MarkScenePrepared( void );
    xbool IsScenePrepared( void ) const;

private:

    xbool m_isScenePrepared;
};

//=========================================================================

class RenderPipeline
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

#ifndef X_RETAIL
    struct lighting_debug_stats
    {
        xbool                         Valid                               = FALSE;
        u64                           FrameSerial                         = 0;
        u32                           DynamicLights                       = 0;
        u32                           CharLights                          = 0;
        u32                           FadingLights                        = 0;
        u32                           OmniLights                          = 0;
        u32                           SpotLights                          = 0;
        u32                           ShadowRequests                      = 0;
        u32                           Cookies                             = 0;
        u32                           LightingSets                        = 0;
        u32                           AssignedLights                      = 0;
        u32                           MaxAssignedLights                   = 0;
        u32                           LightHistogram[MAX_GEOM_LIGHTS + 1] = {};
        LightSystem::collection_stats Collection                          = {};
        u32                           ProjectedLights                     = 0;
        u32                           ProjectedShadows                    = 0;
        u32                           PointShadowLights                   = 0;
        u32                           PointShadowFaces                    = 0;
        u32                           SpotShadowSources                   = 0;
        u32                           AtlasSize                           = 0;
        f32                           ActiveAtlasArea                     = 0.0f;
        xbool                         ShadowsEnabled                      = FALSE;
        ShadowMode                    Mode                                = ShadowMode::Disabled;
        ShadowFilterType              Filter                              = ShadowFilterType::Hard;
        ShadowRenderer::CasterStats   Casters;
    };
#endif // X_RETAIL

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    RenderPipeline( void );
    ~RenderPipeline( void );

    xbool Init( void );
    void  Kill( void );
    xbool QueueCommonPipelines( const RenderScopeDesc& Scope );
    void  ReleaseCommonPipelines( void );

    //-------------------------------------------------------------------------
    //  FRAME LIFECYCLE
    //-------------------------------------------------------------------------

    xbool BeginFrame( const RenderFrameDesc& Desc );
    void  BeginEngineFrame( void );
    void  AbortFrame( void );
    xbool IsFrameAborted( void ) const;
    xbool BeginUIOnlyFrame( void );

    //-------------------------------------------------------------------------
    //  FEATURE REGISTRY
    //-------------------------------------------------------------------------

    xbool RegisterFeature( IRenderFeature& Feature );
    xbool UnregisterFeature( IRenderFeature& Feature );
    u32   GetFeatureSceneTextureRequirements( void ) const;
    xbool BeginFeatureFrames( void );
    xbool InitFeatures( void );
    void  KillFeatures( void );

    //-------------------------------------------------------------------------
    //  RENDER OPTIONS
    //-------------------------------------------------------------------------

    void  SetRenderScale( f32 Scale );
    f32   GetRenderScale( void ) const;
    void  SetSampleCount( u32 SampleCount );
    void  ResolveSampleCountForDevice( void );
    u32   GetSampleCount( void ) const;
    void  SetCmaa2Enabled( xbool Enabled );
    xbool IsCmaa2Enabled( void ) const;
    void  SetPhongShadingEnabled( xbool Enabled );
    xbool IsPhongShadingEnabled( void ) const;
    void  SetTessellationEnabled( xbool Enabled );
    xbool IsTessellationEnabled( void ) const;

    //-------------------------------------------------------------------------
    //  FEATURE ACCESS
    //-------------------------------------------------------------------------

    GeomFeature&              GetGeomFeature( void );
    const GeomFeature&        GetGeomFeature( void ) const;
    PostProcessFeature&       GetPostProcessFeatureRef( void );
    const PostProcessFeature& GetPostProcessFeatureRef( void ) const;
    PostProcessFeature*       GetPostProcessFeature( void );
    const PostProcessFeature* GetPostProcessFeature( void ) const;
    ShadowFeature&            GetShadowFeatureRef( void );
    const ShadowFeature&      GetShadowFeatureRef( void ) const;
    ShadowFeature*            GetShadowFeature( void );
    const ShadowFeature*      GetShadowFeature( void ) const;

    //-------------------------------------------------------------------------
    //  FRAME AND BACKEND ACCESS
    //-------------------------------------------------------------------------

    const RenderFrameContext*     GetFrame( void ) const;
    RenderSubmission&             GetSubmission( void );
    const RenderSubmission&       GetSubmission( void ) const;
    RenderShadowSubmission&       GetShadowSubmission( void );
    const RenderShadowSubmission& GetShadowSubmission( void ) const;
    ClothFeature&                GetClothFeature( void );
    Cmaa2Feature&                GetCmaa2Feature( void );
    XeGtaoFeature&               GetXeGtaoFeature( void );
    RenderBackend&                GetBackend( void );
    const RenderBackend&          GetBackend( void ) const;
    FrameStorage&                 GetFrameStorage( void );
    const FrameStorage&           GetFrameStorage( void ) const;
    FrameCompiler&                GetFrameCompiler( void );
    PrimitiveStorage&             GetPrimitiveStorage( void );
    PrimitiveEncoder&             GetPrimitiveEncoder( void );
    GeometryRepository&           GetGeometryRepository( void );
    ProjectionAtlas&              GetProjectionAtlas( void );
    DecalEncoder&                 GetDecalEncoder( void );
    LightSystem&                  GetLightSystem( void );
    RenderLightingTable&          GetLightingTable( void );
    const RenderLightingTable&    GetLightingTable( void ) const;
    ProjectionTextureRegistry&    GetProjectionTextureRegistry( void );
    ShadowMapAtlas&               GetShadowMapAtlas( void );
    const ShadowMapAtlas&         GetShadowMapAtlas( void ) const;
#ifndef X_RETAIL
    //-------------------------------------------------------------------------
    //  DEBUG QUERIES
    //-------------------------------------------------------------------------

    const lighting_debug_stats& GetLightingDebugStats( void ) const;

    debug_options&       GetDebugOptions( void );
    const debug_options& GetDebugOptions( void ) const;
#endif
    //-------------------------------------------------------------------------
    //  FRAME GRAPH
    //-------------------------------------------------------------------------

    FrameGraphBuilder& GetGraph( void );

    xbool DeclareFrame( void );
    xbool CompileFrame( void );
    xbool ExecuteFrame( void );
    xbool EndFrame( void );
    xbool FinishRenderScope( void );
    xbool DeclareSceneDepthResolve( const RenderFrameContext& Frame );
    xbool CopySceneColorRegion( rtarget& Destination, s32 SourceX, s32 SourceY, s32 Width, s32 Height );

    xbool IsInitialized( void ) const;

private:

    friend class OutputFeature;

#ifndef X_RETAIL
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    void CaptureLightingDebugStats( void );
#endif

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct CopyRegionRequest;
    struct OutputHistoryState;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    static xbool ExecutePresentPass( RenderPassContext& Context, RenderPipeline* pRenderer );
    static xbool ExecutePresentTypedPass( RenderPassContext& Context, RenderPipeline* pRenderer );
    static xbool ExecuteTonemapPass( RenderPassContext& Context, RenderPipeline* pRenderer );
    static xbool ExecuteClearPass( RenderPassContext& Context, RenderPipeline* pRenderer );
    static xbool ExecuteAttachmentResolvePass( RenderPassContext& Context, RenderPipeline* pRenderer );
    static xbool ExecuteCopyRegionPass( RenderPassContext& Context, CopyRegionRequest* pRequest );
    void         RecycleCopyRequests( void );
    xbool        BuildTonemapPass( void );
    xbool        BuildPresentPass( void );
    xbool        BuildClearPass( void );
    xbool        SetupOutputHistory( const RenderExtent& Extent, const rtarget& Output, u64 HistoryKey, s32 ScopeId );
    xbool        BuildSceneColorResolvePass( xbool PreserveMSAA = FALSE );
    xbool        DeclareCopyPasses( void );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

#ifndef X_RETAIL
    lighting_debug_stats m_LightingDebugStats;
#endif // X_RETAIL

    FrameResourcePool         m_Resources;
    FrameGraphBuilder         m_Graph;
    RenderFeatureRegistry     m_Features;
    RenderBackend             m_Backend;
    LightSystem               m_LightSystem;
    RenderLightingTable       m_LightingTable;
    ProjectionTextureRegistry m_ProjectionTextureRegistry;
    ShadowMapAtlas            m_ShadowMapAtlas;
#ifndef X_RETAIL
    debug_options m_DebugOptions;
#endif
    GeomFeature                 m_GeomFeature;
    ClothFeature                m_ClothFeature;
    ShadowFeature               m_ShadowFeature;
    PostProcessFeature          m_PostProcessFeature;
    Cmaa2Feature                m_Cmaa2Feature;
    XeGtaoFeature               m_XeGtaoFeature;
    OutputFeature               m_OutputFeature;
    RenderFrameContext          m_Frame;
    RenderSubmission            m_Submission;
    RenderShadowSubmission      m_ShadowSubmission;
    RenderPreparedScene         m_PreparedScene;
    f32                         m_RenderScale;
    u32                         m_nSamples;
    u32                         m_nRequestedSamples;
    xbool                       m_isPhongShadingEnabled;
    xbool                       m_isTessellationEnabled;
    xarray<RenderView>          m_Views;
    xarray<CopyRegionRequest*>  m_CopyRequests;
    xarray<CopyRegionRequest*>  m_FreeCopyRequests;
    RenderResourceHandle        m_OutputHistory;
    xbool                       m_isOutputHistoryValid;
    u64                         m_OutputHistoryKey;
    u32                         m_nOutputHistorySamples;
    s32                         m_OutputHistoryScopeId;
    xarray<OutputHistoryState*> m_OutputHistoryStates;
    OutputHistoryState*         m_pOutputHistoryState;
    GraphicsPipelineCache       m_CommonPipelines;
    shader                      m_PresentPixelShader;
    shader                      m_TonemapPixelShader;
    shader                      m_RegionCopyPixelShader;
    xbool                       m_isInitialized;
    xbool                       m_isFrameValid;
    xbool                       m_isFrameAborted;
    xbool                       m_isFeatureFrameActive;
    xbool                       m_isDeclared;
    xbool                       m_isCompiled;
    xbool                       m_isExecuted;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

RenderPipeline& GetRenderPipeline( void );

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
RenderSubmission::RenderSubmission( void )
    : m_Geom(),
      m_DynamicGeom(),
      m_TransformStorage(),
      m_pEnvironmentCubeMap( NULL ),
      m_NextGeomSequence( 0 ),
      m_isSceneSubmitted( FALSE ),
      m_isOpen( FALSE ),
      m_isSealed( FALSE )
{
    m_Geom.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicGeom.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicVertexOffsets.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicIndexOffsets.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicDamageOffsets.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_TransformStorage.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
}

//=========================================================================

inline
xbool RenderSubmission::IsOpen( void ) const
{
    return m_isOpen && !m_isSealed;
}

//=========================================================================

inline
xbool RenderSubmission::IsSealed( void ) const
{
    return m_isSealed;
}

//=========================================================================

inline
void RenderSubmission::MarkSceneSubmitted( void )
{
    ValidateOpen( "MarkSceneSubmitted" );
    m_isSceneSubmitted = TRUE;
}

//=========================================================================

inline
xbool RenderSubmission::IsSceneSubmitted( void ) const
{
    return m_isSceneSubmitted;
}

//=========================================================================

inline
void RenderSubmission::SetEnvironmentCubeMap( const cubemap* pCubeMap )
{
    ValidateOpen( "SetEnvironmentCubeMap" );
    m_pEnvironmentCubeMap = pCubeMap;
}

//=========================================================================

inline
const cubemap* RenderSubmission::GetEnvironmentCubeMap( void ) const
{
    return m_pEnvironmentCubeMap;
}

//=========================================================================

inline
xarray<geom_draw_item>& RenderSubmission::Geom( void )
{
    ValidateOpen( "Geom" );
    return m_Geom;
}

//=========================================================================

inline
xarray<dynamic_geom_draw>& RenderSubmission::DynamicGeom( void )
{
    ValidateOpen( "DynamicGeom" );
    return m_DynamicGeom;
}

//=========================================================================

inline
const xarray<geom_draw_item>& RenderSubmission::GetGeom( void ) const
{
    return m_Geom;
}

//=========================================================================

inline
const xarray<dynamic_geom_draw>& RenderSubmission::GetDynamicGeom( void ) const
{
    return m_DynamicGeom;
}

//=========================================================================

inline
dynamic_geom_storage RenderSubmission::GetDynamicStorage( void ) const
{
    dynamic_geom_storage Storage;
    Storage.pVertices   = m_DynamicVertexStorage.GetCount() ? m_DynamicVertexStorage.GetPtr() : NULL;
    Storage.VertexCount = m_DynamicVertexStorage.GetCount();
    Storage.pIndices    = m_DynamicIndexStorage.GetCount() ? m_DynamicIndexStorage.GetPtr() : NULL;
    Storage.IndexCount  = m_DynamicIndexStorage.GetCount();

    return Storage;
}

//=========================================================================

inline
u32& RenderSubmission::NextGeomSequence( void )
{
    ValidateOpen( "NextGeomSequence" );
    return m_NextGeomSequence;
}

//=========================================================================

inline
RenderShadowSubmission::RenderShadowSubmission( void )
    : m_ShadowDraws(),
      m_OrderedShadowDraws(),
      m_DynamicShadowDraws(),
      m_TransformStorage(),
      m_NextSequence( 0 ),
      m_SourceCount( 0 ),
      m_isActive( FALSE )
{
    m_TransformStorage.SetCapacity( 32768 );
    m_ShadowDraws.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_OrderedShadowDraws.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicShadowDraws.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicVertexOffsets.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicIndexOffsets.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_DynamicDamageOffsets.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
}

//=========================================================================

inline
xbool RenderShadowSubmission::IsActive( void ) const
{
    return m_isActive;
}

//=========================================================================

inline
s32 RenderShadowSubmission::GetSourceCount( void ) const
{
    return m_SourceCount;
}

//=========================================================================

inline
s32& RenderShadowSubmission::SourceCount( void )
{
    return m_SourceCount;
}

//=========================================================================

inline
u32& RenderShadowSubmission::NextSequence( void )
{
    return m_NextSequence;
}

//=========================================================================

inline
xarray<geom_draw_item>& RenderShadowSubmission::Draws( void )
{
    return m_ShadowDraws;
}

//=========================================================================

inline
xarray<const geom_draw_item*>& RenderShadowSubmission::OrderedDraws( void )
{
    return m_OrderedShadowDraws;
}

//=========================================================================

inline
xarray<dynamic_geom_shadow_draw>& RenderShadowSubmission::DynamicDraws( void )
{
    return m_DynamicShadowDraws;
}

//=========================================================================

inline
RenderPreparedScene::RenderPreparedScene( void )
    : m_isScenePrepared( FALSE )
{
}

//=========================================================================

inline
void RenderPreparedScene::Reset( void )
{
    m_isScenePrepared = FALSE;
}

//=========================================================================

inline
void RenderPreparedScene::MarkScenePrepared( void )
{
    m_isScenePrepared = TRUE;
}

//=========================================================================

inline
xbool RenderPreparedScene::IsScenePrepared( void ) const
{
    return m_isScenePrepared;
}

//=========================================================================

inline
void RenderPipeline::ReleaseCommonPipelines( void )
{
    m_CommonPipelines.Reset();
}

//=========================================================================

inline
xbool RenderPipeline::IsFrameAborted( void ) const
{
    return m_isFrameAborted;
}

//=========================================================================

inline
xbool RenderPipeline::InitFeatures( void )
{
    return m_Features.Init();
}

//=========================================================================

inline
void RenderPipeline::KillFeatures( void )
{
    m_CommonPipelines.Reset();
    m_Features.Kill();
}

//=========================================================================

inline
void RenderPipeline::SetRenderScale( f32 Scale )
{
    m_RenderScale = x_clamp( Scale, RENDER_SCALE_MIN, RENDER_SCALE_MAX );
}

//=========================================================================

inline
f32 RenderPipeline::GetRenderScale( void ) const
{
    return m_RenderScale;
}

//=========================================================================

inline
void RenderPipeline::SetSampleCount( u32 SampleCount )
{
    m_nRequestedSamples = ( SampleCount == 4 ) ? 4 : 1;
    m_nSamples          = m_nRequestedSamples;

    if( m_isInitialized )
    {
        m_nSamples = SelectSupportedSampleCount( m_nRequestedSamples );
    }

    m_Backend.UpdateRenderState( m_nSamples, m_isPhongShadingEnabled );
}

//=========================================================================

inline
void RenderPipeline::ResolveSampleCountForDevice( void )
{
    m_nSamples = SelectSupportedSampleCount( m_nRequestedSamples );
    m_Backend.UpdateRenderState( m_nSamples, m_isPhongShadingEnabled );
}

//=========================================================================

inline
u32 RenderPipeline::GetSampleCount( void ) const
{
    return m_nSamples;
}

//=========================================================================

inline
void RenderPipeline::SetCmaa2Enabled( xbool Enabled )
{
    m_Cmaa2Feature.SetEnabled( Enabled );
}

//=========================================================================

inline
xbool RenderPipeline::IsCmaa2Enabled( void ) const
{
    return m_Cmaa2Feature.IsEnabled();
}

//=========================================================================

inline
void RenderPipeline::SetPhongShadingEnabled( xbool Enabled )
{
    m_isPhongShadingEnabled = Enabled;
    m_Backend.UpdateRenderState( m_nSamples, m_isPhongShadingEnabled );
}

//=========================================================================

inline
xbool RenderPipeline::IsPhongShadingEnabled( void ) const
{
    return m_isPhongShadingEnabled;
}

//=========================================================================

inline
void RenderPipeline::SetTessellationEnabled( xbool Enabled )
{
    m_isTessellationEnabled = Enabled;
}

//=========================================================================

inline
xbool RenderPipeline::IsTessellationEnabled( void ) const
{
    return m_isTessellationEnabled;
}

//=========================================================================

inline
GeomFeature& RenderPipeline::GetGeomFeature( void )
{
    return m_GeomFeature;
}

//=========================================================================

inline
const GeomFeature& RenderPipeline::GetGeomFeature( void ) const
{
    return m_GeomFeature;
}

//=========================================================================

inline
PostProcessFeature& RenderPipeline::GetPostProcessFeatureRef( void )
{
    return m_PostProcessFeature;
}

//=========================================================================

inline
const PostProcessFeature& RenderPipeline::GetPostProcessFeatureRef( void ) const
{
    return m_PostProcessFeature;
}

//=========================================================================

inline
PostProcessFeature* RenderPipeline::GetPostProcessFeature( void )
{
    return &m_PostProcessFeature;
}

//=========================================================================

inline
const PostProcessFeature* RenderPipeline::GetPostProcessFeature( void ) const
{
    return &m_PostProcessFeature;
}

//=========================================================================

inline
ShadowFeature& RenderPipeline::GetShadowFeatureRef( void )
{
    return m_ShadowFeature;
}

//=========================================================================

inline
const ShadowFeature& RenderPipeline::GetShadowFeatureRef( void ) const
{
    return m_ShadowFeature;
}

//=========================================================================

inline
ShadowFeature* RenderPipeline::GetShadowFeature( void )
{
    return &m_ShadowFeature;
}

//=========================================================================

inline
const ShadowFeature* RenderPipeline::GetShadowFeature( void ) const
{
    return &m_ShadowFeature;
}

//=========================================================================

inline
const RenderFrameContext* RenderPipeline::GetFrame( void ) const
{
    return m_isFrameValid ? &m_Frame : NULL;
}

//=========================================================================

inline
RenderShadowSubmission& RenderPipeline::GetShadowSubmission( void )
{
    return m_ShadowSubmission;
}

//=========================================================================

inline
const RenderShadowSubmission& RenderPipeline::GetShadowSubmission( void ) const
{
    return m_ShadowSubmission;
}

//=========================================================================

inline
ClothFeature& RenderPipeline::GetClothFeature( void )
{
    return m_ClothFeature;
}

//=========================================================================

inline
Cmaa2Feature& RenderPipeline::GetCmaa2Feature( void )
{
    return m_Cmaa2Feature;
}

//=========================================================================

inline
XeGtaoFeature& RenderPipeline::GetXeGtaoFeature( void )
{
    return m_XeGtaoFeature;
}

//=========================================================================

inline
RenderBackend& RenderPipeline::GetBackend( void )
{
    return m_Backend;
}

//=========================================================================

inline
const RenderBackend& RenderPipeline::GetBackend( void ) const
{
    return m_Backend;
}

//=========================================================================

inline
FrameStorage& RenderPipeline::GetFrameStorage( void )
{
    return m_Backend.GetFrameStorage();
}

//=========================================================================

inline
const FrameStorage& RenderPipeline::GetFrameStorage( void ) const
{
    return m_Backend.GetFrameStorage();
}

//=========================================================================

inline
FrameCompiler& RenderPipeline::GetFrameCompiler( void )
{
    return m_Backend.GetFrameCompiler();
}

//=========================================================================

inline
PrimitiveStorage& RenderPipeline::GetPrimitiveStorage( void )
{
    return m_Backend.GetPrimitiveStorage();
}

//=========================================================================

inline
PrimitiveEncoder& RenderPipeline::GetPrimitiveEncoder( void )
{
    return m_Backend.GetPrimitiveEncoder();
}

//=========================================================================

inline
GeometryRepository& RenderPipeline::GetGeometryRepository( void )
{
    return m_Backend.GetGeometryRepository();
}

//=========================================================================

inline
ProjectionAtlas& RenderPipeline::GetProjectionAtlas( void )
{
    return m_Backend.GetProjectionAtlas();
}

//=========================================================================

inline
DecalEncoder& RenderPipeline::GetDecalEncoder( void )
{
    return m_Backend.GetDecalEncoder();
}

//=========================================================================

inline
LightSystem& RenderPipeline::GetLightSystem( void )
{
    return m_LightSystem;
}

//=========================================================================

inline
RenderLightingTable& RenderPipeline::GetLightingTable( void )
{
    return m_LightingTable;
}

//=========================================================================

inline
const RenderLightingTable& RenderPipeline::GetLightingTable( void ) const
{
    return m_LightingTable;
}

//=========================================================================

inline
ProjectionTextureRegistry& RenderPipeline::GetProjectionTextureRegistry( void )
{
    return m_ProjectionTextureRegistry;
}

//=========================================================================

inline
ShadowMapAtlas& RenderPipeline::GetShadowMapAtlas( void )
{
    return m_ShadowMapAtlas;
}

//=========================================================================

inline
const ShadowMapAtlas& RenderPipeline::GetShadowMapAtlas( void ) const
{
    return m_ShadowMapAtlas;
}

//=========================================================================

#ifndef X_RETAIL

inline
const RenderPipeline::lighting_debug_stats& RenderPipeline::GetLightingDebugStats( void ) const
{
    return m_LightingDebugStats;
}

//=========================================================================

inline
debug_options& RenderPipeline::GetDebugOptions( void )
{
    return m_DebugOptions;
}

//=========================================================================

inline
const debug_options& RenderPipeline::GetDebugOptions( void ) const
{
    return m_DebugOptions;
}

//=========================================================================

#endif // X_RETAIL

inline
FrameGraphBuilder& RenderPipeline::GetGraph( void )
{
    return m_Graph;
}

//=========================================================================

inline
xbool RenderPipeline::IsInitialized( void ) const
{
    return m_isInitialized;
}

} // namespace render

#endif // RENDER_PIPELINE_HPP
//=========================================================================
