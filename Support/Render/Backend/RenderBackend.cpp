//=========================================================================
//
//  RenderBackend.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RenderBackend.hpp"

#include "../LightSystem.hpp"
#include "../RenderLighting.hpp"
#include "../ProjectionTextureRegistry.hpp"
#include "../ShadowMapAtlas.hpp"
#include "../Features/Shadow/ShadowFeature.hpp"
#include "../Features/PostProcess/PostProcessFeature.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

RenderBackend::RenderBackend( void )
    : m_FrameStorage(),
      m_GeometryRepository(),
      m_ProjectionAtlas(),
      m_FrameCompiler(),
      m_PrimitiveStorage(),
      m_PrimitiveEncoder(),
      m_DecalEncoder(),
      m_ForwardEncoder(),
      m_Context(),
      m_isInitialized( FALSE )
{
}

//=========================================================================

RenderBackend::~RenderBackend( void )
{
    Kill();
}

//=========================================================================

void RenderBackend::ConfigureServices( LightSystem&               Lights,
                                       ::RenderLightingTable&     Lighting,
                                       ProjectionTextureRegistry& Projections,
                                       ShadowMapAtlas&            ShadowAtlas,
                                       ShadowFeature&             Shadows,
                                       PostProcessFeature&        PostProcess,
                                       u32                        SampleCount,
                                       xbool                      PhongShadingEnabled )
{
    m_Context.pGeometry           = &m_GeometryRepository;
    m_Context.pStorage            = &m_FrameStorage;
    m_Context.pProjection         = &m_ProjectionAtlas;
    m_Context.pFrame              = &m_FrameCompiler;
    m_Context.pLights             = &Lights;
    m_Context.pLighting           = &Lighting;
    m_Context.pProjections        = &Projections;
    m_Context.pShadowAtlas        = &ShadowAtlas;
    m_Context.pShadows            = &Shadows;
    m_Context.pPostProcess        = &PostProcess;
    m_Context.SampleCount         = SampleCount;
    m_Context.PhongShadingEnabled = PhongShadingEnabled;

    m_FrameCompiler.SetContext( m_Context );
    m_PrimitiveStorage.SetContext( m_Context );
    m_DecalEncoder.SetContext( m_Context );
}

//=========================================================================

void RenderBackend::UpdateRenderState( u32 SampleCount, xbool PhongShadingEnabled )
{
    m_Context.SampleCount         = SampleCount;
    m_Context.PhongShadingEnabled = PhongShadingEnabled;
}

//=========================================================================

xbool RenderBackend::Init( u32 FramesInFlight )
{
    if( m_isInitialized )
    {
        return TRUE;
    }

    if( !m_FrameStorage.Init( FramesInFlight ) )
    {
        return FALSE;
    }

    if( !m_ProjectionAtlas.Init() )
    {
        m_FrameStorage.Kill();
        return FALSE;
    }

    m_isInitialized = TRUE;

    return TRUE;
}

//=========================================================================

void RenderBackend::Kill( void )
{
    m_ForwardEncoder.Kill();
    m_GeometryRepository.Kill();
    m_ProjectionAtlas.Kill();
    m_FrameStorage.Kill();

    m_isInitialized = FALSE;
}

//=========================================================================

xbool RenderBackend::BeginFrame( u32 FrameSlot, u64 FrameSerial )
{
    if( !m_isInitialized || !m_FrameStorage.BeginFrame( FrameSlot, FrameSerial ) )
    {
        return FALSE;
    }

    m_ProjectionAtlas.BeginFrame();
    m_FrameCompiler.BeginFrameStorage();

    return TRUE;
}

//=========================================================================

xbool RenderBackend::BeginReadOnlyFrame( u32 FrameSlot, u64 FrameSerial )
{
    if( !m_isInitialized || !m_FrameStorage.BeginReadOnlyFrame( FrameSlot, FrameSerial ) )
    {
        return FALSE;
    }

    m_FrameCompiler.BeginFrameStorage();
    return TRUE;
}

//=========================================================================

xhandle RenderBackend::RegisterRigid( const rigid_geom& Geom )
{
    return m_GeometryRepository.RegisterRigid( Geom );
}

//=========================================================================

xhandle RenderBackend::RegisterSkin( const skin_geom& Geom )
{
    return m_GeometryRepository.RegisterSkin( Geom );
}

//=========================================================================

xbool RenderBackend::UnregisterRigid( xhandle Handle )
{
    return m_GeometryRepository.UnregisterRigid( Handle );
}

//=========================================================================

xbool RenderBackend::UnregisterSkin( xhandle Handle )
{
    return m_GeometryRepository.UnregisterSkin( Handle );
}

//=========================================================================

xbool RenderBackend::SealAndPrepare( const xarray<geom_draw_item>&    Draws,
                                     const xarray<dynamic_geom_draw>& DynamicDraws,
                                     const dynamic_geom_storage&      DynamicStorage,
                                     const cubemap*                   pEnvironmentCubeMap,
                                     xbool                            SceneSubmitted,
                                     const RenderFrameContext&        Frame )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Render/Prepare" );

    if( !m_isInitialized )
    {
        return FALSE;
    }

    if( m_PrimitiveEncoder.IsSubmissionActive() || m_DecalEncoder.IsSubmissionOpen() )
    {
        return FALSE;
    }

    if( !SceneSubmitted )
    {
        if( m_PrimitiveStorage.HasQueuedDraws() || ( m_DecalEncoder.GetQueuedDrawCount() > 0 ) )
        {
            return FALSE;
        }

        return m_FrameStorage.Seal();
    }

    // Resolve geometry and projection resources before uploading draw data.
    if( !m_FrameCompiler.BuildPackets( Draws, DynamicDraws, DynamicStorage, pEnvironmentCubeMap, Frame ) )
    {
        return FALSE;
    }

    if( !m_DecalEncoder.RequestProjectionTextures() || !m_FrameCompiler.PrepareProjectionAtlas( *Frame.pGraph ) )
    {
        return FALSE;
    }

    // Queue frame uploads before sealing the frame storage.
    if( !m_PrimitiveStorage.UploadQueuedDraws( *Frame.pGraph ) )
    {
        return FALSE;
    }

    if( !m_FrameCompiler.UploadPackets( *Frame.pGraph ) )
    {
        return FALSE;
    }

    if( !m_DecalEncoder.UploadQueuedDraws( m_FrameCompiler, *Frame.pGraph ) )
    {
        return FALSE;
    }

    return m_FrameStorage.Seal();
}

//=========================================================================

xbool RenderBackend::EndFrame( void )
{
    return m_isInitialized && m_FrameStorage.EndFrame();
}

//=========================================================================

void RenderBackend::AbortFrame( void )
{
    m_FrameCompiler.ClearPackets();
    if( m_FrameStorage.IsCollecting() || m_FrameStorage.IsSealed() )
    {
        m_ProjectionAtlas.Invalidate();
    }
    m_FrameStorage.AbortFrame();
}

} // namespace render
