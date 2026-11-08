//=========================================================================
//
//  ClothFeature.hpp
//
//=========================================================================

#ifndef CLOTH_FEATURE_HPP
#define CLOTH_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../FrameGraph/FrameGraph.hpp"
#include "../../GeomDraw.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class ClothFeature : public IRenderFeature
{
public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    ClothFeature( void );
    virtual ~ClothFeature( void );

    virtual const char* GetName( void ) const;
    virtual u32         GetGraphStageMask( void ) const;
    virtual u32         GetSceneTextureRequirements( void ) const;

    virtual void  Kill( void );
    virtual void  BeginFrame( const RenderFrameContext& Frame );
    virtual xbool Prepare( RenderFeaturePrepareContext& Context );
    virtual xbool Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual void  AbortFrame( void );
    virtual void  EndFrame( void );

    xbool CollectShadowRequests( const xarray<dynamic_geom_shadow_draw>& Draws, s32 ScopeId );

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct Request;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool Collect( ClothTessellation* pTessellation, const dynamic_geom_gpu_mesh* pMesh,
                   const dynamic_geom_vertex* pVertices, s32 VertexCount, const bbox& Bounds, u32 Revision );
    void  ClearRequests( void );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<Request*> m_Requests;
    xarray<Request*> m_FreeRequests;
    u64              m_FrameSerial;
    s32              m_ScopeId;
    xbool            m_isFrameActive;
    xbool            m_isDeclared;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
const char* ClothFeature::GetName( void ) const
{
    return "Cloth";
}

//=========================================================================

inline
u32 ClothFeature::GetGraphStageMask( void ) const
{
    return 0;
}

//=========================================================================

inline
u32 ClothFeature::GetSceneTextureRequirements( void ) const
{
    return 0;
}

//=========================================================================

inline
xbool ClothFeature::Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Graph;
    ( void )Frame;
    return TRUE;
}

} // namespace render

#endif // CLOTH_FEATURE_HPP
