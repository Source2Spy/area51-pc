//=========================================================================
//
//  RenderBackendContext.hpp
//
//  Explicit services available to backend preparation and encoders.
//
//=========================================================================

#ifndef RENDER_BACKEND_CONTEXT_HPP
#define RENDER_BACKEND_CONTEXT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

struct rtarget;
struct shader_resource;

class RenderLightingTable;

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class GeometryRepository;
class FrameStorage;
class ProjectionAtlas;
class FrameCompiler;
class LightSystem;
class ProjectionTextureRegistry;
class ShadowMapAtlas;
class ShadowFeature;
class PostProcessFeature;

//=========================================================================
//  TYPES
//=========================================================================

struct RenderBackendContext
{
    GeometryRepository*        pGeometry;
    FrameStorage*              pStorage;
    ProjectionAtlas*           pProjection;
    FrameCompiler*             pFrame;
    LightSystem*               pLights;
    ::RenderLightingTable*     pLighting;
    ProjectionTextureRegistry* pProjections;
    ShadowMapAtlas*            pShadowAtlas;
    ShadowFeature*             pShadows;
    PostProcessFeature*        pPostProcess;
    u32                        SampleCount;
    xbool                      PhongShadingEnabled;

    RenderBackendContext( void );

    xbool IsComplete( void ) const;
};

//-------------------------------------------------------------------------

struct BackendPassInputs
{
    const rtarget*         pShadowSample;
    const rtarget*         pShadowDepth;
    const shader_resource* pDistortionScene;
    const rtarget*         pAmbientVisibility;
    xbool                  EnableShadows;

    BackendPassInputs( void );
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
RenderBackendContext::RenderBackendContext( void )
    : pGeometry( NULL ),
      pStorage( NULL ),
      pProjection( NULL ),
      pFrame( NULL ),
      pLights( NULL ),
      pLighting( NULL ),
      pProjections( NULL ),
      pShadowAtlas( NULL ),
      pShadows( NULL ),
      pPostProcess( NULL ),
      SampleCount( 1 ),
      PhongShadingEnabled( FALSE )
{
}

//=========================================================================

inline
xbool RenderBackendContext::IsComplete( void ) const
{
    return pGeometry && pStorage && pProjection && pFrame && pLights && pLighting && pProjections && pShadowAtlas &&
           pShadows && pPostProcess;
}

//=========================================================================

inline
BackendPassInputs::BackendPassInputs( void )
    : pShadowSample( NULL ),
      pShadowDepth( NULL ),
      pDistortionScene( NULL ),
      pAmbientVisibility( NULL ),
      EnableShadows( TRUE )
{
}

} // namespace render

#endif // RENDER_BACKEND_CONTEXT_HPP
