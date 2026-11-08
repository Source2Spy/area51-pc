//=========================================================================
//
//  OutputFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "OutputFeature.hpp"
#include "../../Pipeline/RenderPipeline.hpp"
#include "../../../UI/ui_renderer.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

OutputFeature::OutputFeature( RenderPipeline& Renderer )
    : m_Renderer( Renderer )
{
}

//=========================================================================

xbool OutputFeature::BuildGraphStage( RenderFeatureGraphStage Stage,
                                      FrameGraphBuilder&      Graph,
                                      RenderFrameContext&     Frame )
{
    ( void )Graph;

    const xbool SceneRendered = Frame.SceneRendered;

    switch( Stage )
    {
        case RENDER_FEATURE_STAGE_OUTPUT_PREPARE:
        {
            if( SceneRendered )
            {
                return TRUE;
            }

            return m_Renderer.m_isOutputHistoryValid ? TRUE : m_Renderer.BuildClearPass();
        }
        case RENDER_FEATURE_STAGE_SCENE_DEPTH_RESOLVE:
        {
            return SceneRendered ? m_Renderer.DeclareSceneDepthResolve( m_Renderer.m_Frame ) : TRUE;
        }
        case RENDER_FEATURE_STAGE_SCENE_COLOR_RESOLVE:
        {
            return SceneRendered ? m_Renderer.BuildSceneColorResolvePass() : TRUE;
        }
        case RENDER_FEATURE_STAGE_COPY_REQUESTS:
        {
            if( !SceneRendered || m_Renderer.m_CopyRequests.GetCount() == 0 )
            {
                return TRUE;
            }

            return m_Renderer.BuildSceneColorResolvePass( TRUE ) && m_Renderer.DeclareCopyPasses();
        }
        case RENDER_FEATURE_STAGE_TONEMAP:
        {
            return SceneRendered ? m_Renderer.BuildTonemapPass() : TRUE;
        }
        case RENDER_FEATURE_STAGE_UI:
        {
            if( Frame.Desc.Purpose == RENDER_VIEW_SCENE_CAPTURE )
            {
                return TRUE;
            }

            const u32 FirstPass = Graph.GetPassCount();
            if( !g_UIRenderer.DeclarePass( Graph, Frame ) )
            {
                return FALSE;
            }
            for( u32 i = FirstPass; i < Graph.GetPassCount(); ++i )
            {
                Graph.SetPassResourceValidation( i, FALSE );
            }
            return TRUE;
        }
        case RENDER_FEATURE_STAGE_OUTPUT_HISTORY:
        {
            return TRUE;
        }
        case RENDER_FEATURE_STAGE_PRESENT:
        {
            return m_Renderer.BuildPresentPass();
        }
        default:
        {
            return TRUE;
        }
    }
}

} // namespace render
