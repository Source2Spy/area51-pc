//=========================================================================
//
//  RenderFeature.hpp
//
//  Lifecycle interface for graph-contributing renderer features.
//
//=========================================================================

#ifndef RENDER_FEATURE_HPP
#define RENDER_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "x_array.hpp"

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class FrameGraphBuilder;
struct RenderFrameContext;
class RenderSubmission;
struct RenderPreparedScene;
struct RenderFeaturePrepareContext;

//=========================================================================
//  TYPES
//=========================================================================

struct RenderFeaturePrepareContext
{
    const RenderFrameContext& Frame;
    const RenderSubmission&   Submission;
    RenderPreparedScene&      Prepared;

    RenderFeaturePrepareContext( const RenderFrameContext& Frame,
                                 const RenderSubmission&   Submission,
                                 RenderPreparedScene&      Prepared );
};

//-------------------------------------------------------------------------

// The registry orders feature dependencies within each graph stage.
enum RenderFeatureGraphStage
{
    RENDER_FEATURE_STAGE_OUTPUT_PREPARE = 0,
    RENDER_FEATURE_STAGE_SHADOWS,
    RENDER_FEATURE_STAGE_GEOMETRY_DEPTH,
    RENDER_FEATURE_STAGE_SCENE_DEPTH_RESOLVE,
    RENDER_FEATURE_STAGE_AMBIENT_OCCLUSION,
    RENDER_FEATURE_STAGE_GEOMETRY_OPAQUE,
    RENDER_FEATURE_STAGE_GEOMETRY_PRE_FOG,
    RENDER_FEATURE_STAGE_FOG,
    RENDER_FEATURE_STAGE_GEOMETRY_POST_FOG,
    RENDER_FEATURE_STAGE_COPY_REQUESTS,
    RENDER_FEATURE_STAGE_FIRST_PERSON_AMBIENT_OCCLUSION,
    RENDER_FEATURE_STAGE_GEOMETRY_FIRST_PERSON,
    RENDER_FEATURE_STAGE_SCENE_COLOR_RESOLVE,
    RENDER_FEATURE_STAGE_POST_PROCESS,
    RENDER_FEATURE_STAGE_TONEMAP,
    RENDER_FEATURE_STAGE_ANTI_ALIASING,
    RENDER_FEATURE_STAGE_FILM_GRAIN,
    RENDER_FEATURE_STAGE_UI,
    RENDER_FEATURE_STAGE_SCREEN_FADE,
    RENDER_FEATURE_STAGE_OUTPUT_HISTORY,
    RENDER_FEATURE_STAGE_PRESENT,
    RENDER_FEATURE_STAGE_COUNT
};

inline
u32 RenderFeatureGraphStageMask( RenderFeatureGraphStage Stage );

//-------------------------------------------------------------------------

class IRenderFeature
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    virtual ~IRenderFeature( void );

    // Stable names identify feature dependencies; the default supports simple external features.
    virtual const char* GetName( void ) const;
    virtual u32         GetDependencyCount( void ) const;
    virtual const char* GetDependency( u32 Index ) const;
    virtual u32         GetGraphStageMask( void ) const;

    virtual xbool Init( void );
    virtual void  Kill( void );
    virtual void  BeginFrame( const RenderFrameContext& Frame );
    virtual xbool Prepare( RenderFeaturePrepareContext& Context );
    virtual void  AbortFrame( void );
    virtual void  EndFrame( void );
    virtual u32   GetSceneTextureRequirements( void ) const                            = 0;
    virtual xbool Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame ) = 0;
    virtual xbool Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )   = 0;
    virtual xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );
};

//-------------------------------------------------------------------------

class RenderFeatureRegistry
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderFeatureRegistry( void );

    xbool Register( IRenderFeature& Feature );
    xbool Unregister( IRenderFeature& Feature );
    xbool Init( void );
    void  Kill( void );
    u32   GetSceneTextureRequirements( void ) const;
    xbool BeginFrame( const RenderFrameContext& Frame );
    xbool Prepare( RenderFeaturePrepareContext& Context );
    void  AbortFrame( void );
    void  EndFrame( void );
    xbool Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    xbool Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    // Runs declaration, build, then staged graph contributions.
    xbool BuildGraph( FrameGraphBuilder& Graph, RenderFrameContext& Frame );
    xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );

private:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    s32   FindFeatureByName( const char* pName ) const;
    xbool BuildFeatureOrder( void );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<IRenderFeature*> m_Features;
    xarray<IRenderFeature*> m_OrderedFeatures;
    xbool                   m_isFrameActive;
    xbool                   m_isInitialized;
    xbool                   m_isPrepared;
    xbool                   m_isDeclared;
    xbool                   m_isBuilt;
    u32                     m_BuiltGraphStageMask;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
RenderFeaturePrepareContext::RenderFeaturePrepareContext( const RenderFrameContext& Frame,
                                                          const RenderSubmission&   Submission,
                                                          RenderPreparedScene&      Prepared )
    : Frame( Frame ),
      Submission( Submission ),
      Prepared( Prepared )
{
}

//=========================================================================

inline
u32 RenderFeatureGraphStageMask( RenderFeatureGraphStage Stage )
{
    return 1u << static_cast<u32>( Stage );
}

//=========================================================================

inline
IRenderFeature::~IRenderFeature( void )
{
}

//=========================================================================

inline
const char* IRenderFeature::GetName( void ) const
{
    return "RenderFeature";
}

//=========================================================================

inline
u32 IRenderFeature::GetDependencyCount( void ) const
{
    return 0;
}

//=========================================================================

inline
const char* IRenderFeature::GetDependency( u32 Index ) const
{
    ( void )Index;
    return NULL;
}

//=========================================================================

inline
u32 IRenderFeature::GetGraphStageMask( void ) const
{
    return 0;
}

//=========================================================================

inline
xbool IRenderFeature::Init( void )
{
    return TRUE;
}

//=========================================================================

inline
void IRenderFeature::Kill( void )
{
}

//=========================================================================

inline
void IRenderFeature::BeginFrame( const RenderFrameContext& Frame )
{
    ( void )Frame;
}

//=========================================================================

inline
xbool IRenderFeature::Prepare( RenderFeaturePrepareContext& Context )
{
    ( void )Context;
    return TRUE;
}

//=========================================================================

inline
void IRenderFeature::AbortFrame( void )
{
}

//=========================================================================

inline
void IRenderFeature::EndFrame( void )
{
}

//=========================================================================

inline
xbool IRenderFeature::BuildGraphStage( RenderFeatureGraphStage Stage,
                                       FrameGraphBuilder&      Graph,
                                       RenderFrameContext&     Frame )
{
    ( void )Stage;
    ( void )Graph;
    ( void )Frame;

    return TRUE;
}

} // namespace render

#endif // RENDER_FEATURE_HPP
