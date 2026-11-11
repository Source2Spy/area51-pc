//=========================================================================
//
//  GeomFeature.hpp
//
//=========================================================================

#ifndef GEOM_FEATURE_HPP
#define GEOM_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../FrameGraph/FrameGraph.hpp"
#include "../../LightSystem.hpp"
#include "../../Backend/ForwardEncoder.hpp"

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class FrameCompiler;
class RenderBackend;
class DecalEncoder;
class ShadowRenderer;
class PrimitiveStorage;
class BloomEffect;

//=========================================================================
//  TYPES
//=========================================================================

class GeomFeature : public IRenderFeature
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    GeomFeature( void );

    virtual const char* GetName( void ) const;
    virtual u32         GetDependencyCount( void ) const;
    virtual const char* GetDependency( u32 Index ) const;
    virtual u32         GetGraphStageMask( void ) const;

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    xbool Bind( RenderBackend&    Backend,
                FrameCompiler&    Geometry,
                PrimitiveStorage& Primitives,
                DecalEncoder&     Decals,
                BloomEffect&      Bloom,
                ShadowRenderer&   Shadows,
                LightSystem&      Lights );
    virtual xbool Init( void );
    virtual void  Kill( void );

    virtual void BeginFrame( const RenderFrameContext& Frame );
    virtual void AbortFrame( void );
    virtual void EndFrame( void );

    //-------------------------------------------------------------------------
    //  SCENE PASSES
    //-------------------------------------------------------------------------

    xbool BuildOpaquePass( const RenderFrameContext& Frame );
    xbool BuildPreFogPass( const RenderFrameContext& Frame );
    xbool BuildPostFogPass( const RenderFrameContext& Frame );

    //-------------------------------------------------------------------------
    //  LIGHTING STATISTICS
    //-------------------------------------------------------------------------

    void RecordRigidLightingQuery( void );
    void RecordRigidLightingSetup( void );
    void RecordSkinLightingQuery( void );
    void DiscardQueuedDraws( void );

    //-------------------------------------------------------------------------
    //  GRAPH CONTRIBUTIONS
    //-------------------------------------------------------------------------

    virtual xbool Prepare( RenderFeaturePrepareContext& Context );
    virtual u32   GetSceneTextureRequirements( void ) const;
    virtual xbool Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );

private:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    void            RecordShadowStats( void ) const;
    void            RecordLightingStats( const LightSystem::collection_stats& LightStats );
    ForwardEncoder& Passes( void ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    RenderBackend*                m_pBackend;
    FrameCompiler*                m_pGeom;
    PrimitiveStorage*             m_pPrimitives;
    DecalEncoder*                 m_pDecals;
    ShadowRenderer*               m_pShadows;
    LightSystem*                  m_pLights;
    xbool                         m_isScenePrepared;
    u32                           m_nRigidLightingQueries;
    u32                           m_nRigidLightingSetups;
    u32                           m_nSkinLightingQueries;
    LightSystem::collection_stats m_LightStatsStart;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
const char* GeomFeature::GetName( void ) const
{
    return "Geometry";
}

//=========================================================================

inline
u32 GeomFeature::GetDependencyCount( void ) const
{
    return 1;
}

//=========================================================================

inline
const char* GeomFeature::GetDependency( u32 Index ) const
{
    return Index == 0 ? "Shadow" : NULL;
}

//=========================================================================

inline
u32 GeomFeature::GetGraphStageMask( void ) const
{
    return RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_GEOMETRY_DEPTH ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_GEOMETRY_OPAQUE ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_GEOMETRY_PRE_FOG ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_GEOMETRY_POST_FOG ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_GEOMETRY_FIRST_PERSON );
}

//=========================================================================

inline
u32 GeomFeature::GetSceneTextureRequirements( void ) const
{
    return SCENE_TEXTURE_DEPTH;
}

//=========================================================================

inline
xbool GeomFeature::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Graph;
    ( void )Frame;
    return TRUE;
}

//=========================================================================

inline
xbool GeomFeature::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Graph;
    ( void )Frame;
    return TRUE;
}

} // namespace render

#endif // GEOM_FEATURE_HPP
