//=========================================================================
//
//  OutputFeature.hpp
//
//  Adds the renderer's clear, resolve, UI, history and present passes.
//
//=========================================================================

#ifndef CORE_RENDER_FEATURE_HPP
#define CORE_RENDER_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../Pipeline/RenderFeature.hpp"

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class RenderPipeline;

//=========================================================================
//  TYPES
//=========================================================================

class OutputFeature : public IRenderFeature
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    explicit OutputFeature( RenderPipeline& Renderer );

    virtual const char* GetName( void ) const;
    virtual u32         GetGraphStageMask( void ) const;
    virtual u32         GetSceneTextureRequirements( void ) const;
    virtual xbool       Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool       Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    RenderPipeline& m_Renderer;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
const char* OutputFeature::GetName( void ) const
{
    return "Core";
}

//=========================================================================

inline
u32 OutputFeature::GetGraphStageMask( void ) const
{
    return RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_OUTPUT_PREPARE ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_SCENE_DEPTH_RESOLVE ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_SCENE_COLOR_RESOLVE ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_COPY_REQUESTS ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_TONEMAP ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_UI ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_OUTPUT_HISTORY ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_PRESENT );
}

//=========================================================================

inline
u32 OutputFeature::GetSceneTextureRequirements( void ) const
{
    return 0;
}

//=========================================================================

inline
xbool OutputFeature::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Graph;
    ( void )Frame;
    return TRUE;
}

//=========================================================================

inline
xbool OutputFeature::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Graph;
    ( void )Frame;
    return TRUE;
}

} // namespace render

#endif // CORE_RENDER_FEATURE_HPP
