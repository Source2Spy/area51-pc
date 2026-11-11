//=========================================================================
//
//  BloomEffect.hpp
//
//=========================================================================

#ifndef BLOOM_FEATURE_HPP
#define BLOOM_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../FrameGraph/FrameGraph.hpp"

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class PostProcessFeature;

//=========================================================================
//  TYPES
//=========================================================================

class BloomEffect : public IRenderFeature
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    BloomEffect( void );

    virtual xbool Init( void );
    virtual void  Kill( void );
    void          BeginFrame( xbool Enabled );
    virtual void  BeginFrame( const RenderFrameContext& Frame );
    virtual void  AbortFrame( void );
    virtual void  EndFrame( void );

    virtual xbool Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    xbool DeclarePostPasses( FrameGraphBuilder& Graph, const RenderFrameContext& Frame, PostProcessFeature& Post );
    virtual u32 GetSceneTextureRequirements( void ) const;

    xbool IsEnabled( void ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum State
    {
        STATE_DISABLED,
        STATE_IDLE,
        STATE_DECLARED,
        STATE_BUILT,
        STATE_FAILED,
    };

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xbool m_isEnabled;
    State m_State;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
u32 BloomEffect::GetSceneTextureRequirements( void ) const
{
    return SCENE_TEXTURE_DEPTH;
}

//=========================================================================

inline
xbool BloomEffect::IsEnabled( void ) const
{
    return m_isEnabled;
}

} // namespace render

#endif // BLOOM_FEATURE_HPP
//=========================================================================
