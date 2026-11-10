//=========================================================================
//
//  Cmaa2Feature.hpp
//
//=========================================================================

#ifndef CMAA2_FEATURE_HPP
#define CMAA2_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../FrameGraph/FrameGraph.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class Cmaa2Feature : public IRenderFeature
{
  public:
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    Cmaa2Feature( void );

    virtual const char* GetName( void ) const;
    virtual u32         GetGraphStageMask( void ) const;
    virtual u32         GetSceneTextureRequirements( void ) const;
    virtual xbool       Init( void );
    virtual void        Kill( void );
    virtual xbool       Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool       Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );

    void  SetEnabled( xbool Enabled );
    xbool IsEnabled( void ) const;

  private:
    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum Kernel
    {
        KERNEL_INIT,
        KERNEL_EDGES,
        KERNEL_ARGS,
        KERNEL_CANDIDATES,
        KERNEL_APPLY,
        KERNEL_COUNT
    };

    struct PassData;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    static xbool Execute( RenderPassContext& Context, const PassData& Data );
    static xbool ExecuteResolve( RenderPassContext& Context, const RenderResourceHandle& Color );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    shader         m_Shaders[KERNEL_COUNT];
    rstate_sampler m_PointSampler;
    xbool          m_isEnabled;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline const char* Cmaa2Feature::GetName( void ) const
{
    return "CMAA2";
}

//=========================================================================

inline u32 Cmaa2Feature::GetGraphStageMask( void ) const
{
    return RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_ANTI_ALIASING );
}

//=========================================================================

inline u32 Cmaa2Feature::GetSceneTextureRequirements( void ) const
{
    return 0;
}

//=========================================================================

inline xbool Cmaa2Feature::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    (void)Graph;
    (void)Frame;
    return TRUE;
}

//=========================================================================

inline xbool Cmaa2Feature::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    (void)Graph;
    (void)Frame;
    return TRUE;
}

//=========================================================================

inline void Cmaa2Feature::SetEnabled( xbool Enabled )
{
    m_isEnabled = Enabled;
}

//=========================================================================

inline xbool Cmaa2Feature::IsEnabled( void ) const
{
    return m_isEnabled;
}

} // namespace render

#endif // CMAA2_FEATURE_HPP
