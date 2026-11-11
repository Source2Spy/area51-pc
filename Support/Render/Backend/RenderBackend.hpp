//=========================================================================
//
//  RenderBackend.hpp
//
//  Single owner for frame render data and backend encoders.
//
//=========================================================================

#ifndef RENDER_BACKEND_HPP
#define RENDER_BACKEND_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameStorage.hpp"
#include "RenderBackendContext.hpp"
#include "Frame/FrameCompiler.hpp"
#include "Resource/GeometryRepository.hpp"
#include "Resource/ProjectionAtlas.hpp"
#include "Primitive/PrimitiveStorage.hpp"
#include "Primitive/PrimitiveEncoder.hpp"
#include "Decal/DecalEncoder.hpp"
#include "../GeomDraw.hpp"
#include "ForwardEncoder.hpp"

class cubemap;

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class RenderBackend
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    RenderBackend( void );
    ~RenderBackend( void );

    xbool Init( u32 FramesInFlight = 0 );
    void  Kill( void );
    void ConfigureServices( LightSystem&               Lights,
                            ::RenderLightingTable&     Lighting,
                            ProjectionTextureRegistry& Projections,
                            ShadowMapAtlas&            ShadowAtlas,
                            ShadowFeature&             Shadows,
                            PostProcessFeature&        PostProcess,
                            u32                        SampleCount,
                            xbool                      PhongShadingEnabled );
    void  UpdateRenderState( u32 SampleCount, xbool PhongShadingEnabled );

    xbool   BeginFrame( u32 FrameSlot, u64 FrameSerial );
    xbool   BeginReadOnlyFrame( u32 FrameSlot, u64 FrameSerial );
    xhandle RegisterRigid( const rigid_geom& Geom );
    xhandle RegisterSkin( const skin_geom& Geom );
    xbool   UnregisterRigid( xhandle Handle );
    xbool   UnregisterSkin( xhandle Handle );
    xbool SealAndPrepare( const xarray<geom_draw_item>&    Draws,
                          const xarray<dynamic_geom_draw>& DynamicDraws,
                          const dynamic_geom_storage&      DynamicStorage,
                          const cubemap*                   pEnvironmentCubeMap,
                          xbool                            SceneSubmitted,
                          const RenderFrameContext&        Frame );
    xbool   EndFrame( void );
    void    AbortFrame( void );

    FrameStorage&             GetFrameStorage( void );
    const FrameStorage&       GetFrameStorage( void ) const;
    GeometryRepository&       GetGeometryRepository( void );
    const GeometryRepository& GetGeometryRepository( void ) const;
    ProjectionAtlas&          GetProjectionAtlas( void );
    const ProjectionAtlas&    GetProjectionAtlas( void ) const;
    FrameCompiler&            GetFrameCompiler( void );
    const FrameCompiler&      GetFrameCompiler( void ) const;
    PrimitiveStorage&         GetPrimitiveStorage( void );
    const PrimitiveStorage&   GetPrimitiveStorage( void ) const;
    PrimitiveEncoder&         GetPrimitiveEncoder( void );
    const PrimitiveEncoder&   GetPrimitiveEncoder( void ) const;
    DecalEncoder&             GetDecalEncoder( void );
    const DecalEncoder&       GetDecalEncoder( void ) const;
    ForwardEncoder&           GetForwardEncoder( void );
    const ForwardEncoder&     GetForwardEncoder( void ) const;

    xbool                       IsInitialized( void ) const;
    const RenderBackendContext& GetContext( void ) const;

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    FrameStorage         m_FrameStorage;
    GeometryRepository   m_GeometryRepository;
    ProjectionAtlas      m_ProjectionAtlas;
    FrameCompiler        m_FrameCompiler;
    PrimitiveStorage     m_PrimitiveStorage;
    PrimitiveEncoder     m_PrimitiveEncoder;
    DecalEncoder         m_DecalEncoder;
    ForwardEncoder       m_ForwardEncoder;
    RenderBackendContext m_Context;
    xbool                m_isInitialized;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
FrameStorage& RenderBackend::GetFrameStorage( void )
{
    return m_FrameStorage;
}

//=========================================================================

inline
const FrameStorage& RenderBackend::GetFrameStorage( void ) const
{
    return m_FrameStorage;
}

//=========================================================================

inline
GeometryRepository& RenderBackend::GetGeometryRepository( void )
{
    return m_GeometryRepository;
}

//=========================================================================

inline
const GeometryRepository& RenderBackend::GetGeometryRepository( void ) const
{
    return m_GeometryRepository;
}

//=========================================================================

inline
ProjectionAtlas& RenderBackend::GetProjectionAtlas( void )
{
    return m_ProjectionAtlas;
}

//=========================================================================

inline
const ProjectionAtlas& RenderBackend::GetProjectionAtlas( void ) const
{
    return m_ProjectionAtlas;
}

//=========================================================================

inline
FrameCompiler& RenderBackend::GetFrameCompiler( void )
{
    return m_FrameCompiler;
}

//=========================================================================

inline
const FrameCompiler& RenderBackend::GetFrameCompiler( void ) const
{
    return m_FrameCompiler;
}

//=========================================================================

inline
PrimitiveStorage& RenderBackend::GetPrimitiveStorage( void )
{
    return m_PrimitiveStorage;
}

//=========================================================================

inline
const PrimitiveStorage& RenderBackend::GetPrimitiveStorage( void ) const
{
    return m_PrimitiveStorage;
}

//=========================================================================

inline
PrimitiveEncoder& RenderBackend::GetPrimitiveEncoder( void )
{
    return m_PrimitiveEncoder;
}

//=========================================================================

inline
const PrimitiveEncoder& RenderBackend::GetPrimitiveEncoder( void ) const
{
    return m_PrimitiveEncoder;
}

//=========================================================================

inline
DecalEncoder& RenderBackend::GetDecalEncoder( void )
{
    return m_DecalEncoder;
}

//=========================================================================

inline
const DecalEncoder& RenderBackend::GetDecalEncoder( void ) const
{
    return m_DecalEncoder;
}

//=========================================================================

inline
ForwardEncoder& RenderBackend::GetForwardEncoder( void )
{
    return m_ForwardEncoder;
}

//=========================================================================

inline
const ForwardEncoder& RenderBackend::GetForwardEncoder( void ) const
{
    return m_ForwardEncoder;
}

//=========================================================================

inline
xbool RenderBackend::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
const RenderBackendContext& RenderBackend::GetContext( void ) const
{
    return m_Context;
}

} // namespace render

#endif // RENDER_BACKEND_HPP
