//=========================================================================
//
//  ShadowFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ShadowFeature.hpp"

#include "../../FrameGraph/RenderFailure.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

ShadowFeature::ShadowFeature( void )
    : m_Backend()
{
}

//=========================================================================

xbool ShadowFeature::Init( void )
{
    m_Backend.Init();
    return m_Backend.IsInitialized();
}

//=========================================================================

void ShadowFeature::Kill( void )
{
    m_Backend.Kill();
}

//=========================================================================

void ShadowFeature::BeginFrame( const RenderFrameContext& Frame )
{
    ( void )Frame;
    m_Backend.BeginFrame( Frame.Desc.ScopeId );
}

//=========================================================================

void ShadowFeature::AbortFrame( void )
{
    m_Backend.AbortFrame();
}

//=========================================================================

void ShadowFeature::EndFrame( void )
{
    m_Backend.EndFrame();
}

//=========================================================================

xbool ShadowFeature::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Graph;
    ( void )Frame;
    return TRUE;
}

//=========================================================================

xbool ShadowFeature::BuildGraphStage( RenderFeatureGraphStage Stage,
                                      FrameGraphBuilder&      Graph,
                                      RenderFrameContext&     Frame )
{
    if( Stage != RENDER_FEATURE_STAGE_SHADOWS )
    {
        return TRUE;
    }

    if( !Frame.SceneRendered || !Frame.Desc.EnableShadows )
    {
        return TRUE;
    }

    if( !m_Backend.IsInitialized() )
    {
        RENDER_THROW( "ShadowFeature", "shadow services are not initialized" );
        return FALSE;
    }

    return m_Backend.DeclareGraph( Graph, Frame );
}

} // namespace render
