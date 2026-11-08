//=========================================================================
//
//  RenderFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RenderFeature.hpp"
#include "../FrameGraph/FrameGraph.hpp"
#include "../FrameGraph/RenderFailure.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

RenderFeatureRegistry::RenderFeatureRegistry( void )
    : m_Features(),
      m_OrderedFeatures(),
      m_isFrameActive( FALSE ),
      m_isInitialized( FALSE ),
      m_isPrepared( FALSE ),
      m_isDeclared( FALSE ),
      m_isBuilt( FALSE ),
      m_BuiltGraphStageMask( 0 )
{
}

//=========================================================================

xbool RenderFeatureRegistry::Register( IRenderFeature& Feature )
{
    if( m_isFrameActive || m_isInitialized )
    {
        RENDER_THROW( "RenderFeatureRegistry", "cannot register a feature while a frame is active" );
        return FALSE;
    }

    for( s32 i = 0; i < m_Features.GetCount(); ++i )
    {
        if( m_Features[i] == &Feature )
        {
            return TRUE;
        }

        if( m_Features[i] && Feature.GetName() && m_Features[i]->GetName() &&
            ( x_strcmp( m_Features[i]->GetName(), Feature.GetName() ) == 0 ) )
        {
            RENDER_THROW( "RenderFeatureRegistry", "duplicate feature name '%s'", Feature.GetName() );
            return FALSE;
        }
    }

    m_Features.Append() = &Feature;

    return TRUE;
}

//=========================================================================

xbool RenderFeatureRegistry::Unregister( IRenderFeature& Feature )
{
    if( m_isFrameActive || m_isInitialized )
    {
        RENDER_THROW( "RenderFeatureRegistry", "cannot unregister a feature while a frame is active" );
        return FALSE;
    }

    for( s32 i = 0; i < m_Features.GetCount(); ++i )
    {
        if( m_Features[i] == &Feature )
        {
            m_Features.Delete( i );
            m_OrderedFeatures.Clear();
            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

s32 RenderFeatureRegistry::FindFeatureByName( const char* pName ) const
{
    if( !pName )
    {
        return -1;
    }

    for( s32 i = 0; i < m_Features.GetCount(); ++i )
    {
        if( m_Features[i] && m_Features[i]->GetName() && ( x_strcmp( m_Features[i]->GetName(), pName ) == 0 ) )
        {
            return i;
        }
    }

    return -1;
}

//=========================================================================

xbool RenderFeatureRegistry::BuildFeatureOrder( void )
{
    m_OrderedFeatures.Clear();

    xarray<u32>   Indegree;
    xarray<xbool> Emitted;
    Indegree.SetCount( m_Features.GetCount() );
    Emitted.SetCount( m_Features.GetCount() );

    for( s32 i = 0; i < m_Features.GetCount(); ++i )
    {
        Indegree[i] = 0;
        Emitted[i]  = FALSE;
    }

    // Reject missing dependencies instead of falling back to registration order.
    for( s32 i = 0; i < m_Features.GetCount(); ++i )
    {
        IRenderFeature* pFeature = m_Features[i];

        if( !pFeature )
        {
            continue;
        }

        for( u32 Dependency = 0; Dependency < pFeature->GetDependencyCount(); ++Dependency )
        {
            const char* pDependency = pFeature->GetDependency( Dependency );

            if( FindFeatureByName( pDependency ) < 0 )
            {
                RENDER_THROW( "RenderFeatureRegistry", "feature '%s' depends on missing feature '%s'",
                              pFeature->GetName(), pDependency ? pDependency : "<null>" );
                m_OrderedFeatures.Clear();

                return FALSE;
            }

            ++Indegree[i];
        }
    }

    while( m_OrderedFeatures.GetCount() < m_Features.GetCount() )
    {
        s32 Selected = -1;

        for( s32 i = 0; i < m_Features.GetCount(); ++i )
        {
            if( !Emitted[i] && ( Indegree[i] == 0 ) )
            {
                Selected = i;
                break;
            }
        }

        if( Selected < 0 )
        {
            RENDER_THROW( "RenderFeatureRegistry", "feature dependency cycle detected" );
            m_OrderedFeatures.Clear();

            return FALSE;
        }

        Emitted[Selected]          = TRUE;
        m_OrderedFeatures.Append() = m_Features[Selected];

        // Registration order breaks ties between independent features.
        const char* pSelectedName = m_Features[Selected] ? m_Features[Selected]->GetName() : NULL;

        for( s32 i = 0; i < m_Features.GetCount(); ++i )
        {
            if( Emitted[i] || !m_Features[i] )
            {
                continue;
            }

            IRenderFeature* pFeature = m_Features[i];

            for( u32 Dependency = 0; Dependency < pFeature->GetDependencyCount(); ++Dependency )
            {
                const char* pDependency = pFeature->GetDependency( Dependency );

                if( pSelectedName && pDependency && ( x_strcmp( pSelectedName, pDependency ) == 0 ) )
                {
                    ASSERT( Indegree[i] > 0 );
                    --Indegree[i];
                }
            }
        }
    }

    return TRUE;
}

//=========================================================================

xbool RenderFeatureRegistry::Init( void )
{
    if( m_isInitialized || m_isFrameActive )
    {
        return m_isInitialized;
    }

    if( !BuildFeatureOrder() )
    {
        return FALSE;
    }

    for( s32 i = 0; i < m_OrderedFeatures.GetCount(); ++i )
    {
        if( m_OrderedFeatures[i] && !m_OrderedFeatures[i]->Init() )
        {
            for( s32 j = i; j >= 0; --j )
            {
                if( m_OrderedFeatures[j] )
                {
                    m_OrderedFeatures[j]->Kill();
                }
            }

            m_OrderedFeatures.Clear();
            return FALSE;
        }
    }

    m_isInitialized = TRUE;

    return TRUE;
}

//=========================================================================

void RenderFeatureRegistry::Kill( void )
{
    AbortFrame();

    for( s32 i = m_OrderedFeatures.GetCount() - 1; i >= 0; --i )
    {
        if( m_OrderedFeatures[i] )
        {
            m_OrderedFeatures[i]->Kill();
        }
    }

    m_OrderedFeatures.Clear();
    m_isInitialized = FALSE;
}

//=========================================================================

u32 RenderFeatureRegistry::GetSceneTextureRequirements( void ) const
{
    u32                            Requirements = 0;
    const xarray<IRenderFeature*>& Features     = m_OrderedFeatures.GetCount() ? m_OrderedFeatures : m_Features;

    for( s32 i = 0; i < Features.GetCount(); ++i )
    {
        if( Features[i] )
        {
            Requirements |= Features[i]->GetSceneTextureRequirements();
        }
    }

    return Requirements;
}

//=========================================================================

xbool RenderFeatureRegistry::BeginFrame( const RenderFrameContext& Frame )
{
    if( !m_isInitialized || m_isFrameActive || m_isDeclared || m_isBuilt || !Frame.pGraph )
    {
        RENDER_THROW( "RenderFeatureRegistry", "invalid feature frame begin lifecycle" );
        return FALSE;
    }

    m_isFrameActive       = TRUE;
    m_isPrepared          = FALSE;
    m_isDeclared          = FALSE;
    m_isBuilt             = FALSE;
    m_BuiltGraphStageMask = 0;

    for( s32 i = 0; i < m_OrderedFeatures.GetCount(); ++i )
    {
        if( m_OrderedFeatures[i] )
        {
            m_OrderedFeatures[i]->BeginFrame( Frame );
        }
    }

    return TRUE;
}

//=========================================================================

void RenderFeatureRegistry::AbortFrame( void )
{
    if( m_isFrameActive )
    {
        for( s32 i = m_OrderedFeatures.GetCount() - 1; i >= 0; --i )
        {
            if( m_OrderedFeatures[i] )
            {
                m_OrderedFeatures[i]->AbortFrame();
            }
        }
    }

    m_isFrameActive       = FALSE;
    m_isPrepared          = FALSE;
    m_isDeclared          = FALSE;
    m_isBuilt             = FALSE;
    m_BuiltGraphStageMask = 0;
}

//=========================================================================

void RenderFeatureRegistry::EndFrame( void )
{
    if( !m_isFrameActive )
    {
        return;
    }

    for( s32 i = m_OrderedFeatures.GetCount() - 1; i >= 0; --i )
    {
        if( m_OrderedFeatures[i] )
        {
            m_OrderedFeatures[i]->EndFrame();
        }
    }

    m_isFrameActive       = FALSE;
    m_isPrepared          = FALSE;
    m_isDeclared          = FALSE;
    m_isBuilt             = FALSE;
    m_BuiltGraphStageMask = 0;
}

//=========================================================================

xbool RenderFeatureRegistry::Prepare( RenderFeaturePrepareContext& Context )
{
    if( !m_isFrameActive || m_isPrepared || m_isDeclared || m_isBuilt || !Context.Frame.pGraph )
    {
        RENDER_THROW( "RenderFeatureRegistry", "invalid feature prepare lifecycle" );
        return FALSE;
    }

    for( s32 i = 0; i < m_OrderedFeatures.GetCount(); ++i )
    {
        if( m_OrderedFeatures[i] &&
            ( !m_OrderedFeatures[i]->Prepare( Context ) || Context.Frame.pGraph->GetLastError() ) )
        {
            return FALSE;
        }
    }

    m_isPrepared = TRUE;

    return TRUE;
}

//=========================================================================

xbool RenderFeatureRegistry::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !m_isFrameActive || !m_isPrepared || m_isDeclared || m_isBuilt || ( Frame.pGraph != &Graph ) )
    {
        RENDER_THROW( "RenderFeatureRegistry", "invalid feature declaration lifecycle" );
        return FALSE;
    }

    for( s32 i = 0; i < m_OrderedFeatures.GetCount(); ++i )
    {
        if( m_OrderedFeatures[i] && ( !m_OrderedFeatures[i]->Declare( Graph, Frame ) || Graph.GetLastError() ) )
        {
            return FALSE;
        }
    }

    m_isDeclared = TRUE;

    return TRUE;
}

//=========================================================================

xbool RenderFeatureRegistry::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !m_isFrameActive || !m_isPrepared || !m_isDeclared || m_isBuilt || ( Frame.pGraph != &Graph ) )
    {
        RENDER_THROW( "RenderFeatureRegistry", "invalid feature build lifecycle" );
        return FALSE;
    }

    for( s32 i = 0; i < m_OrderedFeatures.GetCount(); ++i )
    {
        if( m_OrderedFeatures[i] && ( !m_OrderedFeatures[i]->Build( Graph, Frame ) || Graph.GetLastError() ) )
        {
            return FALSE;
        }
    }

    m_isBuilt = TRUE;

    return TRUE;
}

//=========================================================================

xbool RenderFeatureRegistry::BuildGraph( FrameGraphBuilder& Graph, RenderFrameContext& Frame )
{
    if( !m_isFrameActive || !m_isPrepared || ( Frame.pGraph != &Graph ) )
    {
        RENDER_THROW( "RenderFeatureRegistry", "invalid feature graph build lifecycle" );
        return FALSE;
    }

    if( !Declare( Graph, Frame ) || !Build( Graph, Frame ) )
    {
        return FALSE;
    }

    for( u32 Stage = 0; Stage < RENDER_FEATURE_STAGE_COUNT; ++Stage )
    {
        if( !BuildGraphStage( static_cast<RenderFeatureGraphStage>( Stage ), Graph, Frame ) )
        {
            return FALSE;
        }
    }

    return Graph.GetLastError() == NULL;
}

//=========================================================================

xbool RenderFeatureRegistry::BuildGraphStage( RenderFeatureGraphStage Stage,
                                              FrameGraphBuilder&      Graph,
                                              RenderFrameContext&     Frame )
{
    if( !m_isFrameActive || !m_isPrepared || !m_isDeclared || !m_isBuilt || ( Frame.pGraph != &Graph ) ||
        ( Stage >= RENDER_FEATURE_STAGE_COUNT ) )
    {
        RENDER_THROW( "RenderFeatureRegistry", "invalid graph stage lifecycle stage=%d", static_cast<s32>( Stage ) );
        return FALSE;
    }

    const u32 StageMask = RenderFeatureGraphStageMask( Stage );

    if( m_BuiltGraphStageMask & StageMask )
    {
        RENDER_THROW( "RenderFeatureRegistry", "graph stage was already built stage=%d", static_cast<s32>( Stage ) );
        return FALSE;
    }

    for( s32 i = 0; i < m_OrderedFeatures.GetCount(); ++i )
    {
        IRenderFeature* pFeature = m_OrderedFeatures[i];

        if( !pFeature || !( pFeature->GetGraphStageMask() & StageMask ) )
        {
            continue;
        }

        if( !pFeature->BuildGraphStage( Stage, Graph, Frame ) || Graph.GetLastError() )
        {
            return FALSE;
        }
    }

    m_BuiltGraphStageMask |= StageMask;

    return TRUE;
}

} // namespace render
