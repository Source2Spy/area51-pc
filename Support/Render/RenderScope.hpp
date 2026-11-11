//=========================================================================
//
//  RenderScope.hpp
//
//=========================================================================

#ifndef RENDER_SCOPE_HPP
#define RENDER_SCOPE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "e_RenderTarget.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

enum render_view_purpose
{
    RENDER_VIEW_GAMEPLAY,
    RENDER_VIEW_SCENE_CAPTURE,
    RENDER_VIEW_EDITOR
};

//-------------------------------------------------------------------------

struct RenderScopeDesc
{
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderScopeDesc( void );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    const rtarget*      pColorTarget;
    const rtarget*      pDepthTarget;
    render_view_purpose Purpose;
    s32                 ScopeId;
    u32                 SampleCount;
    xbool               EnableShadows;
    xbool               EnableTemporalHistory;
    xbool               EnablePostProcessing;
    mutable s32         SpriteCount;
};

//-------------------------------------------------------------------------

struct RenderTargetSurface
{
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderTargetSurface( void );
    xbool Create( s32 Width, s32 Height );
    void  Destroy( void );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    rtarget ColorTarget;
    rtarget DepthTarget;
    s32     Width;
    s32     Height;
    s32     ScopeId;
    xbool   bValid;
};

//=========================================================================
//  CLASSES
//=========================================================================

class ScopedRenderView
{
public:

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    explicit ScopedRenderView( const RenderScopeDesc& Desc );
    ~ScopedRenderView( void );

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    const RenderScopeDesc* m_pPrevious;
    xbool                  m_wasFrameActive;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

const RenderScopeDesc& GetActiveRenderScopeDesc( void );
void                   ResetActiveRenderScopeCounters( void );
void                   AddActiveRenderScopeSprites( s32 Count );

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
RenderScopeDesc::RenderScopeDesc( void )
    : pColorTarget( NULL ),
      pDepthTarget( NULL ),
      Purpose( RENDER_VIEW_GAMEPLAY ),
      ScopeId( -1 ),
      SampleCount( 0 ),
      EnableShadows( TRUE ),
      EnableTemporalHistory( TRUE ),
      EnablePostProcessing( TRUE ),
      SpriteCount( 0 )
{
}

//=========================================================================

inline
RenderTargetSurface::RenderTargetSurface( void )
    : Width( 0 ),
      Height( 0 ),
      ScopeId( -1 ),
      bValid( FALSE )
{
}

} // namespace render

#endif // RENDER_SCOPE_HPP
