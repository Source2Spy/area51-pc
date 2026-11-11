//=========================================================================
//
//  PipelinePrewarm.hpp
//
//  Pipeline requirements for loaded scene content.
//
//=========================================================================

#ifndef RENDER_PIPELINE_PREWARM_HPP
#define RENDER_PIPELINE_PREWARM_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PrimitiveBatch.hpp"
#include "DecalBatch.hpp"
#include "Backend/Frame/FrameCompiler.hpp"

namespace render
{

//=========================================================================
//  CLASSES
//=========================================================================

class PipelinePrewarmContext
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    PipelinePrewarmContext( const RenderScopeDesc& Scope = RenderScopeDesc() );

    xbool QueueGeometry( const geom& Geom, xbool Skin );
    xbool QueuePrimitive( const primitive_draw_desc& Desc );
    xbool QueueDecal( decal_blend_mode Blend, u32 Flags );
    xbool QueueDynamic( u32 RenderFlags = 0 );
    xbool QueueBuiltins( void );

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    geom_pass_desc m_ScenePass;
    xbool          m_isShadowsEnabled;
};

} // namespace render

#endif // RENDER_PIPELINE_PREWARM_HPP
