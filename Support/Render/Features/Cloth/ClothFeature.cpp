//=========================================================================
//
//  ClothFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ClothFeature.hpp"
#include "ClothTessellation.hpp"
#include "../../Pipeline/RenderPipeline.hpp"
#include "e_Engine.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

struct ClothFeature::Request
{
    ClothTessellation*           pTessellation;
    xarray<dynamic_geom_vertex> Vertices;
    bbox                        Bounds;
    u32                         Revision;

    Request( void ) : pTessellation( NULL ), Bounds(), Revision( 0 )
    {
    }
};

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

ClothFeature::ClothFeature( void )
    : m_FrameSerial( 0 ),
      m_ScopeId( -1 ),
      m_isFrameActive( FALSE ),
      m_isDeclared( FALSE )
{
}

//=========================================================================

ClothFeature::~ClothFeature( void )
{
    Kill();
}

//=========================================================================

void ClothFeature::ClearRequests( void )
{
    for( s32 i = 0; i < m_Requests.GetCount(); ++i )
    {
        Request* pRequest = m_Requests[i];
        pRequest->pTessellation = NULL;
        pRequest->Vertices.SetCount( 0 );
        m_FreeRequests.Append() = pRequest;
    }

    m_Requests.SetCount( 0 );
}

//=========================================================================

void ClothFeature::Kill( void )
{
    AbortFrame();

    for( s32 i = 0; i < m_FreeRequests.GetCount(); ++i )
    {
        delete m_FreeRequests[i];
    }

    m_Requests.Clear();
    m_FreeRequests.Clear();
}

//=========================================================================

xbool ClothFeature::Collect( ClothTessellation* pTessellation, const dynamic_geom_gpu_mesh* pMesh,
                             const dynamic_geom_vertex* pVertices, s32 VertexCount, const bbox& Bounds, u32 Revision )
{
    if( !pTessellation )
    {
        return TRUE;
    }

    if( !pMesh || pMesh != pTessellation->GetMesh() || !pVertices || VertexCount <= 0 || VertexCount > 65535 )
    {
        RENDER_THROW( "ClothFeature", "invalid tessellation request" );
        return FALSE;
    }

    for( s32 i = 0; i < m_Requests.GetCount(); ++i )
    {
        const Request& Existing = *m_Requests[i];
        if( Existing.pTessellation == pTessellation )
        {
            if( Existing.Revision != Revision || Existing.Vertices.GetCount() != VertexCount )
            {
                RENDER_THROW( "ClothFeature", "conflicting cloth revisions within scope=%d", m_ScopeId );
                return FALSE;
            }

            return TRUE;
        }
    }

    Request* pRequest = NULL;
    if( m_FreeRequests.GetCount() )
    {
        const s32 Last = m_FreeRequests.GetCount() - 1;
        pRequest = m_FreeRequests[Last];
        m_FreeRequests.SetCount( Last );
    }
    else
    {
        pRequest = new Request;
    }

    if( !pRequest )
    {
        RENDER_THROW( "ClothFeature", "failed to allocate tessellation request" );
        return FALSE;
    }

    pRequest->pTessellation = pTessellation;
    pRequest->Bounds        = Bounds;
    pRequest->Revision      = Revision;
    pRequest->Vertices.SetCount( VertexCount );
    x_memcpy( pRequest->Vertices.GetPtr(), pVertices, VertexCount * sizeof( dynamic_geom_vertex ) );
    m_Requests.Append() = pRequest;
    return TRUE;
}

//=========================================================================

xbool ClothFeature::CollectShadowRequests( const xarray<dynamic_geom_shadow_draw>& Draws, s32 ScopeId )
{
    if( m_isFrameActive || !eng_GetCurrentFrameSerial() )
    {
        RENDER_THROW( "ClothFeature", "shadow collection requires an engine frame before feature frame begin" );
        return FALSE;
    }

    ClearRequests();
    m_FrameSerial = eng_GetCurrentFrameSerial();
    m_ScopeId     = ScopeId;
    m_isDeclared  = FALSE;

    for( s32 i = 0; i < Draws.GetCount(); ++i )
    {
        const dynamic_geom_shadow_draw& Draw = Draws[i];
        if( !Collect( Draw.pClothTessellation, Draw.pGpuMesh, Draw.pVertices,
                       Draw.VertexCount, Draw.Bounds, Draw.ClothRevision ) )
        {
            AbortFrame();
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void ClothFeature::BeginFrame( const RenderFrameContext& Frame )
{
    const u64 FrameSerial = eng_GetCurrentFrameSerial();
    if( m_FrameSerial != FrameSerial || m_ScopeId != Frame.Desc.ScopeId || !Frame.Desc.EnableShadows )
    {
        ClearRequests();
    }

    m_FrameSerial   = FrameSerial;
    m_ScopeId       = Frame.Desc.ScopeId;
    m_isFrameActive = TRUE;
    m_isDeclared    = FALSE;
}

//=========================================================================

xbool ClothFeature::Prepare( RenderFeaturePrepareContext& Context )
{
    if( !m_isFrameActive || m_isDeclared || m_ScopeId != Context.Frame.Desc.ScopeId ||
        m_FrameSerial != eng_GetCurrentFrameSerial() || !Context.Submission.IsSealed() )
    {
        RENDER_THROW( "ClothFeature", "invalid cloth preparation lifecycle" );
        return FALSE;
    }

    if( !Context.Submission.IsSceneSubmitted() )
    {
        ClearRequests();
        return TRUE;
    }

    const xarray<dynamic_geom_draw>& Draws = Context.Submission.GetDynamicGeom();
    for( s32 i = 0; i < Draws.GetCount(); ++i )
    {
        const dynamic_geom_draw& Draw = Draws[i];
        if( !Collect( Draw.pClothTessellation, Draw.pGpuMesh, Draw.pVertices,
                       Draw.VertexCount, Draw.Bounds, Draw.ClothRevision ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool ClothFeature::Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !m_isFrameActive || m_isDeclared || m_ScopeId != Frame.Desc.ScopeId ||
        m_FrameSerial != eng_GetCurrentFrameSerial() )
    {
        RENDER_THROW( "ClothFeature", "invalid cloth declaration lifecycle" );
        return FALSE;
    }

    if( Frame.SceneRendered )
    {
        for( s32 i = 0; i < m_Requests.GetCount(); ++i )
        {
            const Request& Data = *m_Requests[i];
            if( !Data.pTessellation->Declare( Graph, Data.Vertices.GetPtr(), Data.Vertices.GetCount(),
                                              Data.Bounds, Data.Revision ) )
            {
                RENDER_THROW( "ClothFeature", "failed to declare cloth tessellation" );
                return FALSE;
            }
        }
    }

    m_isDeclared = TRUE;
    return TRUE;
}

//=========================================================================

void ClothFeature::AbortFrame( void )
{
    ClearRequests();
    m_FrameSerial   = 0;
    m_ScopeId       = -1;
    m_isFrameActive = FALSE;
    m_isDeclared    = FALSE;
}

//=========================================================================

void ClothFeature::EndFrame( void )
{
    AbortFrame();
}

} // namespace render
