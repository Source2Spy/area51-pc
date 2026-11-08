//=========================================================================
//
//  ShadowFeature.hpp
//
//=========================================================================

#ifndef SHADOW_FEATURE_HPP
#define SHADOW_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../FrameGraph/FrameGraph.hpp"
#include "ShadowRenderer.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class ShadowFeature : public IRenderFeature
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    ShadowFeature( void );

    virtual const char* GetName( void ) const;
    virtual u32         GetGraphStageMask( void ) const;

    virtual xbool Init( void );
    virtual void  Kill( void );

    virtual void  BeginFrame( const RenderFrameContext& Frame );
    virtual void  AbortFrame( void );
    virtual void  EndFrame( void );
    virtual u32   GetSceneTextureRequirements( void ) const;
    virtual xbool Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );

    ShadowRenderer&       Backend( void );
    const ShadowRenderer& Backend( void ) const;

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    ShadowRenderer m_Backend;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
const char* ShadowFeature::GetName( void ) const
{
    return "Shadow";
}

//=========================================================================

inline
u32 ShadowFeature::GetGraphStageMask( void ) const
{
    return RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_SHADOWS );
}

//=========================================================================

inline
u32 ShadowFeature::GetSceneTextureRequirements( void ) const
{
    return 0;
}

//=========================================================================

inline
xbool ShadowFeature::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Graph;
    ( void )Frame;
    return TRUE;
}

//=========================================================================

inline
ShadowRenderer& ShadowFeature::Backend( void )
{
    return m_Backend;
}

//=========================================================================

inline
const ShadowRenderer& ShadowFeature::Backend( void ) const
{
    return m_Backend;
}

} // namespace render

#endif // SHADOW_FEATURE_HPP
