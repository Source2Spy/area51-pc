//=========================================================================
//
//  GeomFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeomFeature.hpp"

#include "../../Pipeline/RenderPipeline.hpp"
#include "../../Backend/RenderBackend.hpp"
#include "../../Backend/Frame/FrameCompiler.hpp"
#include "../../Backend/Decal/DecalEncoder.hpp"
#include "../../Backend/ForwardEncoder.hpp"
#include "../Shadow/ShadowRenderer.hpp"
#include "../../FrameGraph/RenderFailure.hpp"
#include "e_Engine.hpp"
#include "x_debug.hpp"
#include "../../Backend/Primitive/PrimitiveStorage.hpp"
#include "../Bloom/BloomEffect.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

GeomFeature::GeomFeature( void )
    : m_pBackend( NULL ),
      m_pGeom( NULL ),
      m_pPrimitives( NULL ),
      m_pDecals( NULL ),
      m_pShadows( NULL ),
      m_pLights( NULL ),
      m_isScenePrepared( FALSE ),
      m_nRigidLightingQueries( 0 ),
      m_nRigidLightingSetups( 0 ),
      m_nSkinLightingQueries( 0 ),
      m_LightStatsStart()
{
}

//=========================================================================

ForwardEncoder& GeomFeature::Passes( void ) const
{
    ASSERT( m_pBackend );
    return m_pBackend->GetForwardEncoder();
}

//=========================================================================

xbool GeomFeature::Bind( RenderBackend&    Backend,
                         FrameCompiler&    Geometry,
                         PrimitiveStorage& Primitives,
                         DecalEncoder&     Decals,
                         BloomEffect&      Bloom,
                         ShadowRenderer&   Shadows,
                         LightSystem&      Lights )
{
    if( m_pBackend || m_pGeom || m_pPrimitives || m_pDecals || m_pShadows || m_pLights )
    {
        return FALSE;
    }

    m_pBackend    = &Backend;
    m_pGeom       = &Geometry;
    m_pPrimitives = &Primitives;
    m_pDecals     = &Decals;
    m_pShadows    = &Shadows;
    m_pLights     = &Lights;

    return m_pBackend->GetForwardEncoder().Init( Geometry, Primitives, Decals, Bloom, Shadows );
}

//=========================================================================

xbool GeomFeature::Init( void )
{
    return m_pBackend && m_pGeom && m_pPrimitives && m_pDecals && m_pShadows && m_pLights;
}

//=========================================================================

void GeomFeature::Kill( void )
{
    m_pBackend              = NULL;
    m_pGeom                 = NULL;
    m_pPrimitives           = NULL;
    m_pDecals               = NULL;
    m_pShadows              = NULL;
    m_pLights               = NULL;
    m_isScenePrepared       = FALSE;
    m_nRigidLightingQueries = 0;
    m_nRigidLightingSetups  = 0;
    m_nSkinLightingQueries  = 0;
    m_LightStatsStart       = LightSystem::collection_stats();
}

//=========================================================================

void GeomFeature::BeginFrame( const RenderFrameContext& Frame )
{
    ( void )Frame;
    m_isScenePrepared       = FALSE;
    m_nRigidLightingQueries = 0;
    m_nRigidLightingSetups  = 0;
    m_nSkinLightingQueries  = 0;
    m_LightStatsStart       = m_pLights ? m_pLights->GetCollectionStats() : LightSystem::collection_stats();
    Passes().BeginFrame();
}

//=========================================================================

void GeomFeature::AbortFrame( void )
{
    Passes().DiscardQueuedDraws();
}

//=========================================================================

void GeomFeature::EndFrame( void )
{
    if( !m_pGeom || !m_pShadows )
    {
        return;
    }

    if( !m_isScenePrepared )
    {
        Passes().DiscardQueuedDraws();
        return;
    }

    static xprofile_counter GeomPackets = x_GetProfiler().RegisterCounter( "GeomPackets", "RenderCounter" );
    static xprofile_counter GeomRigidInstances =
        x_GetProfiler().RegisterCounter( "GeomRigidInstances", "RenderCounter" );
    static xprofile_counter GeomSkinInstances = x_GetProfiler().RegisterCounter( "GeomSkinInstances", "RenderCounter" );
    static xprofile_counter GeomLitInstances  = x_GetProfiler().RegisterCounter( "GeomLitInstances", "RenderCounter" );
    static xprofile_counter GeomInstanceLights =
        x_GetProfiler().RegisterCounter( "GeomInstanceLights", "RenderCounter" );
    static xprofile_counter GeomLightingRecords =
        x_GetProfiler().RegisterCounter( "GeomLightingRecords", "RenderCounter" );
    static xprofile_counter GeomForwardOpaquePackets =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaquePackets", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueGpuDraws =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueGpuDraws", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueRigidIndirectRuns =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueRigidIndirectRuns", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueRigidIndirectCommands =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueRigidIndirectCommands", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueSkinIndirectRuns =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueSkinIndirectRuns", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueSkinIndirectCommands =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueSkinIndirectCommands", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueInstances =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueInstances", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueSkinSectionDraws =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueSkinSectionDraws", "RenderCounter" );
    static xprofile_counter GeomForwardOpaqueSubmittedIndices =
        x_GetProfiler().RegisterCounter( "GeomForwardOpaqueSubmittedIndices", "RenderCounter" );
    static xprofile_gauge GeomForwardOpaqueInstancesPerPacket =
        x_GetProfiler().RegisterGauge( "GeomForwardOpaqueInstancesPerPacket", XPROFILE_UNIT_COUNT, "RenderGauge" );
    static xprofile_gauge GeomForwardOpaqueDrawsPerPacket =
        x_GetProfiler().RegisterGauge( "GeomForwardOpaqueDrawsPerPacket", XPROFILE_UNIT_COUNT, "RenderGauge" );
    static xprofile_gauge GeomForwardOpaqueIndicesPerDraw =
        x_GetProfiler().RegisterGauge( "GeomForwardOpaqueIndicesPerDraw", XPROFILE_UNIT_COUNT, "RenderGauge" );
    static xprofile_gauge GeomInstancesPerPacket =
        x_GetProfiler().RegisterGauge( "GeomInstancesPerPacket", XPROFILE_UNIT_COUNT, "RenderGauge" );
    static xprofile_gauge GeomLightsPerLitInstance =
        x_GetProfiler().RegisterGauge( "GeomLightsPerLitInstance", XPROFILE_UNIT_COUNT, "RenderGauge" );
    static xprofile_gauge GeomLightingRecordReuse =
        x_GetProfiler().RegisterGauge( "GeomLightingRecordReuse", XPROFILE_UNIT_COUNT, "RenderGauge" );

    GeomPackets.Add( m_pGeom->GetPacketCount() );
    GeomRigidInstances.Add( m_pGeom->GetRigidInstanceCount() );
    GeomSkinInstances.Add( m_pGeom->GetSkinInstanceCount() );
    GeomLitInstances.Add( m_pGeom->GetLitInstanceCount() );
    GeomInstanceLights.Add( m_pGeom->GetInstanceLightCount() );

    const s32 GeomInstanceCount = m_pGeom->GetRigidInstanceCount() + m_pGeom->GetSkinInstanceCount();
    GeomInstancesPerPacket.Set(
        m_pGeom->GetPacketCount() ? static_cast<f64>( GeomInstanceCount ) / m_pGeom->GetPacketCount() : 0.0 );
    GeomLightsPerLitInstance.Set( m_pGeom->GetLitInstanceCount()
                                      ? static_cast<f64>( m_pGeom->GetInstanceLightCount() ) /
                                            m_pGeom->GetLitInstanceCount()
                                      : 0.0 );

    RecordShadowStats();

    GeomLightingRecords.Add( m_pGeom->GetLightingRecordCount() );
    GeomLightingRecordReuse.Set( m_pGeom->GetLightingRecordCount()
                                     ? static_cast<f64>( m_pGeom->GetLitInstanceCount() ) /
                                           m_pGeom->GetLightingRecordCount()
                                     : 0.0 );

    u32 ForwardOpaquePacketCount = 0;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/PrepareForward" );

        for( s32 i = 0; i < m_pGeom->GetPacketCount(); ++i )
        {
            if( m_pGeom->GetPacketPass( i ) == GEOM_PASS_FORWARD_OPAQUE )
            {
                ++ForwardOpaquePacketCount;
            }
        }
    }

    const u32 ForwardOpaqueGpuDrawCount        = m_pGeom->GetForwardOpaqueGpuDrawCount();
    const u32 ForwardOpaqueInstanceCount       = m_pGeom->GetForwardOpaqueInstanceCount();
    const u64 ForwardOpaqueSubmittedIndexCount = m_pGeom->GetForwardOpaqueSubmittedIndexCount();
    GeomForwardOpaquePackets.Add( ForwardOpaquePacketCount );
    GeomForwardOpaqueGpuDraws.Add( ForwardOpaqueGpuDrawCount );
    GeomForwardOpaqueRigidIndirectRuns.Add( m_pGeom->GetForwardOpaqueRigidIndirectRunCount() );
    GeomForwardOpaqueRigidIndirectCommands.Add( m_pGeom->GetForwardOpaqueRigidIndirectCommandCount() );
    GeomForwardOpaqueSkinIndirectRuns.Add( m_pGeom->GetForwardOpaqueSkinIndirectRunCount() );
    GeomForwardOpaqueSkinIndirectCommands.Add( m_pGeom->GetForwardOpaqueSkinIndirectCommandCount() );
    GeomForwardOpaqueInstances.Add( ForwardOpaqueInstanceCount );
    GeomForwardOpaqueSkinSectionDraws.Add( m_pGeom->GetForwardOpaqueSkinSectionDrawCount() );
    GeomForwardOpaqueSubmittedIndices.Add( static_cast<f64>( ForwardOpaqueSubmittedIndexCount ) );
    GeomForwardOpaqueInstancesPerPacket.Set(
        ForwardOpaquePacketCount ? static_cast<f64>( ForwardOpaqueInstanceCount ) / ForwardOpaquePacketCount : 0.0 );
    GeomForwardOpaqueDrawsPerPacket.Set(
        ForwardOpaquePacketCount ? static_cast<f64>( ForwardOpaqueGpuDrawCount ) / ForwardOpaquePacketCount : 0.0 );
    GeomForwardOpaqueIndicesPerDraw.Set(
        ForwardOpaqueGpuDrawCount ? static_cast<f64>( ForwardOpaqueSubmittedIndexCount ) / ForwardOpaqueGpuDrawCount
                                  : 0.0 );

    Passes().DiscardQueuedDraws();
}

//=========================================================================

xbool GeomFeature::BuildOpaquePass( const RenderFrameContext& Frame )
{
    return Passes().BuildOpaquePass( Frame );
}

//=========================================================================

xbool GeomFeature::BuildPreFogPass( const RenderFrameContext& Frame )
{
    return Passes().BuildPreFogPass( Frame );
}

//=========================================================================

xbool GeomFeature::BuildPostFogPass( const RenderFrameContext& Frame )
{
    return Passes().BuildPostFogPass( Frame );
}

//=========================================================================

xbool GeomFeature::BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame )
{
    ( void )Graph;

    if( !Frame.SceneRendered )
    {
        return TRUE;
    }

    switch( Stage )
    {
        case RENDER_FEATURE_STAGE_GEOMETRY_DEPTH:
        {
            return Passes().BuildDepthPass( Frame );
        }
        case RENDER_FEATURE_STAGE_GEOMETRY_OPAQUE:
        {
            return BuildOpaquePass( Frame );
        }
        case RENDER_FEATURE_STAGE_GEOMETRY_PRE_FOG:
        {
            return BuildPreFogPass( Frame );
        }
        case RENDER_FEATURE_STAGE_GEOMETRY_POST_FOG:
        {
            return BuildPostFogPass( Frame );
        }
        case RENDER_FEATURE_STAGE_GEOMETRY_FIRST_PERSON:
        {
            for( s32 i = 0; i < m_pGeom->GetPacketCount(); i++ )
            {
                if( m_pGeom->IsFirstPersonPacket( i ) )
                {
                    return Passes().BuildFirstPersonPass( Frame ) &&
                           GetRenderPipeline().DeclareSceneDepthResolve( Frame );
                }
            }

            return TRUE;
        }
        default:
        {
            return TRUE;
        }
    }
}

//=========================================================================

void GeomFeature::DiscardQueuedDraws( void )
{
    Passes().DiscardQueuedDraws();
}

//=========================================================================

void GeomFeature::RecordRigidLightingQuery( void )
{
    ++m_nRigidLightingQueries;
}

//=========================================================================

void GeomFeature::RecordRigidLightingSetup( void )
{
    ++m_nRigidLightingSetups;
}

//=========================================================================

void GeomFeature::RecordSkinLightingQuery( void )
{
    ++m_nSkinLightingQueries;
}

//=========================================================================

xbool GeomFeature::Prepare( RenderFeaturePrepareContext& Context )
{
    if( !m_pGeom || !m_pDecals || !m_pShadows )
    {
        RENDER_THROW( "GeomFeature", "geometry services are not initialized" );
        return FALSE;
    }

    if( !Context.Submission.IsSealed() )
    {
        RENDER_THROW( "GeomFeature", "geometry preparation requires a sealed submission" );
        return FALSE;
    }

    if( !Context.Submission.IsSceneSubmitted() )
    {
        return TRUE;
    }

    m_isScenePrepared = TRUE;

    const RenderFrameContext& FrameData = Context.Frame;

    if( !FrameData.pGraph || !FrameData.pGraph->GetDesc( FrameData.SceneColor ) ||
        !FrameData.pGraph->GetDesc( FrameData.SceneDepth ) )
    {
        RENDER_THROW( "GeomFeature", "geometry frame targets are unavailable" );
        return FALSE;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/QueueForward" );

        for( s32 i = 0; i < m_pGeom->GetPacketCount(); ++i )
        {
            const geom_render_pass PassData = m_pGeom->GetPacketPass( i );

            if( PassData == GEOM_PASS_FORWARD_OPAQUE && !m_pGeom->IsFirstPersonPacket( i ) )
            {
                continue;
            }

            ForwardRenderPhase Phase;

            switch( PassData )
            {
                case GEOM_PASS_FORWARD_OPAQUE:
                {
                    Phase = FORWARD_PHASE_SURFACE;
                    break;
                }
                case GEOM_PASS_TRANSPARENT:
                {
                    Phase = FORWARD_PHASE_TRANSPARENT;
                    break;
                }
                case GEOM_PASS_ADDITIVE:
                {
                    Phase = FORWARD_PHASE_ADDITIVE;
                    break;
                }
                case GEOM_PASS_FORCE_LAST:
                {
                    Phase = FORWARD_PHASE_FORCE_LAST;
                    break;
                }
                case GEOM_PASS_ZPRIME:
                {
                    Phase = FORWARD_PHASE_ZPRIME;
                    break;
                }
                case GEOM_PASS_FADING:
                {
                    Phase = FORWARD_PHASE_FADING;
                    break;
                }
                case GEOM_PASS_DISTORTION:
                {
                    Phase = FORWARD_PHASE_DISTORTION;
                    break;
                }
                default:
                {
                    RENDER_THROW( "GeomFeature", "unsupported geometry pass=%d", static_cast<s32>( PassData ) );

                    return FALSE;
                }
            }

            const RenderContributionMask Contributions = RENDER_CONTRIBUTION_SCENE_COLOR | RENDER_CONTRIBUTION_BLOOM;

            if( !Passes().QueueGeom( i, Phase, m_pGeom->GetPacketSortDepth( i ), m_pGeom->GetPacketSequence( i ),
                                     Contributions ) )
            {
                return FALSE;
            }
        }
    }

    RecordLightingStats( m_pLights->GetCollectionStats() );
    Context.Prepared.MarkScenePrepared();

    return TRUE;
}

//=========================================================================
//  STATISTICS
//=========================================================================

void GeomFeature::RecordLightingStats( const LightSystem::collection_stats& LightStats )
{
    static xprofile_counter LightRigidQueries = x_GetProfiler().RegisterCounter( "LightRigidQueries", "RenderCounter" );
    static xprofile_counter LightRigidSetups  = x_GetProfiler().RegisterCounter( "LightRigidSetups", "RenderCounter" );
    static xprofile_counter LightSkinQueries  = x_GetProfiler().RegisterCounter( "LightSkinQueries", "RenderCounter" );
    static xprofile_counter LightSceneCandidates =
        x_GetProfiler().RegisterCounter( "LightSceneCandidates", "RenderCounter" );
    static xprofile_counter LightCharCandidates =
        x_GetProfiler().RegisterCounter( "LightCharCandidates", "RenderCounter" );
    static xprofile_counter LightSceneHits = x_GetProfiler().RegisterCounter( "LightSceneHits", "RenderCounter" );
    static xprofile_counter LightCharHits  = x_GetProfiler().RegisterCounter( "LightCharHits", "RenderCounter" );

    LightRigidQueries.Add( m_nRigidLightingQueries );
    LightRigidSetups.Add( m_nRigidLightingSetups );
    LightSkinQueries.Add( m_nSkinLightingQueries );
    LightSceneCandidates.Add( LightStats.SceneCandidates - m_LightStatsStart.SceneCandidates );
    LightCharCandidates.Add( LightStats.CharCandidates - m_LightStatsStart.CharCandidates );
    LightSceneHits.Add( LightStats.SceneHits - m_LightStatsStart.SceneHits );
    LightCharHits.Add( LightStats.CharHits - m_LightStatsStart.CharHits );
}

//=========================================================================

void GeomFeature::RecordShadowStats( void ) const
{
    const ShadowRenderer::CasterStats& Stats = m_pShadows->GetLastCasterStats();
    static xprofile_counter            ShadowCasterInputDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterInputDraws", "RenderCounter" );
    static xprofile_counter ShadowCasterPreparedDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterPreparedDraws", "RenderCounter" );
    static xprofile_counter ShadowCasterFrustumRejectedDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterFrustumRejectedDraws", "RenderCounter" );
    static xprofile_counter ShadowCasterDroppedDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterDroppedDraws", "RenderCounter" );
    static xprofile_counter ShadowCasterPackets =
        x_GetProfiler().RegisterCounter( "ShadowCasterPackets", "RenderCounter" );
    static xprofile_counter ShadowCasterGpuDraws =
        x_GetProfiler().RegisterCounter( "ShadowCasterGpuDraws", "RenderCounter" );
    static xprofile_counter ShadowTilesUpdated =
        x_GetProfiler().RegisterCounter( "ShadowTilesUpdated", "RenderCounter" );
    static xprofile_counter ShadowTilesReused = x_GetProfiler().RegisterCounter( "ShadowTilesReused", "RenderCounter" );
    static xprofile_counter ShadowRigidInstances =
        x_GetProfiler().RegisterCounter( "ShadowRigidInstances", "RenderCounter" );
    static xprofile_counter ShadowSkinInstances =
        x_GetProfiler().RegisterCounter( "ShadowSkinInstances", "RenderCounter" );
    static xprofile_counter ShadowSkinPaletteMatrices =
        x_GetProfiler().RegisterCounter( "ShadowSkinPaletteMatrices", "RenderCounter" );
    static xprofile_counter ShadowRigidPackets =
        x_GetProfiler().RegisterCounter( "ShadowRigidPackets", "RenderCounter" );
    static xprofile_counter ShadowRigidIndirectRuns =
        x_GetProfiler().RegisterCounter( "ShadowRigidIndirectRuns", "RenderCounter" );
    static xprofile_counter ShadowRigidIndirectCommands =
        x_GetProfiler().RegisterCounter( "ShadowRigidIndirectCommands", "RenderCounter" );
    static xprofile_counter ShadowSkinIndirectRuns =
        x_GetProfiler().RegisterCounter( "ShadowSkinIndirectRuns", "RenderCounter" );
    static xprofile_counter ShadowSkinIndirectCommands =
        x_GetProfiler().RegisterCounter( "ShadowSkinIndirectCommands", "RenderCounter" );
    static xprofile_counter ShadowSkinPackets = x_GetProfiler().RegisterCounter( "ShadowSkinPackets", "RenderCounter" );
    static xprofile_counter ShadowAlphaPackets =
        x_GetProfiler().RegisterCounter( "ShadowAlphaPackets", "RenderCounter" );
    static xprofile_counter ShadowSourceStateChanges =
        x_GetProfiler().RegisterCounter( "ShadowSourceStateChanges", "RenderCounter" );
    static xprofile_counter ShadowSkinPalettes =
        x_GetProfiler().RegisterCounter( "ShadowSkinPalettes", "RenderCounter" );
    static xprofile_counter ShadowCasterUploadBytes =
        x_GetProfiler().RegisterCounter( "ShadowCasterUploadBytes", "RenderCounter" );
    static xprofile_counter ShadowCasterSubmittedIndices =
        x_GetProfiler().RegisterCounter( "ShadowCasterSubmittedIndices", "RenderCounter" );
    static xprofile_counter ShadowCasterBufferReallocs =
        x_GetProfiler().RegisterCounter( "ShadowCasterBufferReallocs", "RenderCounter" );
    static xprofile_gauge ShadowInstancesPerPacket =
        x_GetProfiler().RegisterGauge( "ShadowInstancesPerPacket", XPROFILE_UNIT_COUNT, "RenderGauge" );
    static xprofile_gauge ShadowDrawCompression =
        x_GetProfiler().RegisterGauge( "ShadowDrawCompression", XPROFILE_UNIT_PERCENT, "RenderGauge" );
    static xprofile_gauge ShadowPaletteReuse =
        x_GetProfiler().RegisterGauge( "ShadowPaletteReuse", XPROFILE_UNIT_COUNT, "RenderGauge" );
    static xprofile_gauge ShadowMaxPacketInstances =
        x_GetProfiler().RegisterGauge( "ShadowMaxPacketInstances", XPROFILE_UNIT_COUNT, "RenderGauge" );

    ShadowCasterInputDraws.Add( Stats.InputDrawCount );
    ShadowCasterPreparedDraws.Add( Stats.PreparedDrawCount );
    ShadowCasterFrustumRejectedDraws.Add( Stats.FrustumRejectedDrawCount );
    ShadowCasterDroppedDraws.Add( Stats.InputDrawCount - Stats.PreparedDrawCount );
    ShadowCasterPackets.Add( Stats.PacketCount );
    ShadowCasterGpuDraws.Add( Stats.GpuDrawCount );
    ShadowTilesUpdated.Add( Stats.UpdatedTileCount );
    ShadowTilesReused.Add( Stats.ReusedTileCount );
    ShadowRigidInstances.Add( Stats.RigidInstanceCount );
    ShadowSkinInstances.Add( Stats.SkinInstanceCount );
    ShadowSkinPaletteMatrices.Add( Stats.SkinPaletteMatrixCount );
    ShadowRigidPackets.Add( Stats.RigidPacketCount );
    ShadowRigidIndirectRuns.Add( Stats.RigidIndirectRunCount );
    ShadowRigidIndirectCommands.Add( Stats.RigidIndirectCommandCount );
    ShadowSkinIndirectRuns.Add( Stats.SkinIndirectRunCount );
    ShadowSkinIndirectCommands.Add( Stats.SkinIndirectCommandCount );
    ShadowSkinPackets.Add( Stats.SkinPacketCount );
    ShadowAlphaPackets.Add( Stats.AlphaPacketCount );
    ShadowSourceStateChanges.Add( Stats.SourceStateChangeCount );
    ShadowSkinPalettes.Add( Stats.SkinPaletteCount );
    ShadowCasterUploadBytes.Add( static_cast<f64>( Stats.UploadBytes ) );
    ShadowCasterSubmittedIndices.Add( static_cast<f64>( Stats.SubmittedIndexCount ) );
    ShadowCasterBufferReallocs.Add( Stats.BufferReallocationCount );
    ShadowInstancesPerPacket.Set( Stats.PacketCount ? static_cast<f64>( Stats.PreparedDrawCount ) / Stats.PacketCount
                                                    : 0.0 );
    ShadowDrawCompression.Set(
        Stats.PreparedDrawCount ? 100.0 * static_cast<f64>( Stats.GpuDrawCount ) / Stats.PreparedDrawCount : 0.0 );
    ShadowPaletteReuse.Set(
        Stats.SkinPaletteCount ? static_cast<f64>( Stats.SkinInstanceCount ) / Stats.SkinPaletteCount : 0.0 );
    ShadowMaxPacketInstances.Set( Stats.MaxPacketInstanceCount );
}

} // namespace render
