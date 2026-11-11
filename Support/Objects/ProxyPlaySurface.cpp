#include "ProxyPlaySurface.hpp"
#include "PlaySurfaceMgr/PlaySurfaceMgr.hpp"
#include "GameLib/RigidGeomCollision.hpp"
#include "Obj_mgr/obj_mgr.hpp"

//=========================================================================
// proxy_playsurface implementation
//=========================================================================

static struct proxy_playsurface_desc : public object_desc
{
    proxy_playsurface_desc( void ) : object_desc( 
            object::TYPE_PLAY_SURFACE_PROXY,
            "Proxy Play Surface",
            "SYSTEM",
            object::ATTR_COLLIDABLE,
            0 ) {}

    virtual object* Create( void ) { return new proxy_playsurface; }

} s_ProxyPlaySurface_Desc;

//=========================================================================

const object_desc& proxy_playsurface::GetTypeDesc( void ) const
{
    return s_ProxyPlaySurface_Desc;
}

//=========================================================================

const object_desc& proxy_playsurface::GetObjectType( void )
{
    return s_ProxyPlaySurface_Desc;
}

//=========================================================================

proxy_playsurface::proxy_playsurface( void ) :
    object          (),
    m_CurrentGuid   (0)
{
}

//=========================================================================

proxy_playsurface::~proxy_playsurface( void )
{
}

//=========================================================================

bbox proxy_playsurface::GetLocalBBox( void ) const
{
    playsurface_mgr::surface* pSurface = g_PlaySurfaceMgr.m_SpatialDBase.GetSurfaceByGuid(m_CurrentGuid);
    if ( !pSurface )
    {
        return bbox( vector3(0.0f,0.0f,0.0f), 10.0f );
    }

    vector3 Translation = pSurface->L2W.GetTranslation();
    return ( bbox( pSurface->WorldBBox.Min-Translation, pSurface->WorldBBox.Max-Translation ) );
}

//=========================================================================

xbool proxy_playsurface::GetColDetails( s32 Key, object::detail_tri& Tri )
{
    if( Key == -1 )
        return( FALSE );

    playsurface_mgr::surface* pSurface = g_PlaySurfaceMgr.m_SpatialDBase.GetSurfaceByGuid(m_CurrentGuid);
    if ( !pSurface )
        return( FALSE );

    rigid_geom* pRigidGeom = (rigid_geom*)render::GetGeom( pSurface->RenderInst );
    if( !pRigidGeom )
        return( FALSE );

    if( !pRigidGeom->m_Collision.nHighClusters )
        return( FALSE );

    return RigidGeom_GetColDetails( pRigidGeom,
                                    &pSurface->L2W,
                                    (const u32*)pSurface->pColor,
                                    Key,
                                    Tri );
}

//=========================================================================

void proxy_playsurface::SetSurface( guid Guid )
{
    m_CurrentGuid = Guid;

    playsurface_mgr::surface* pSurface = g_PlaySurfaceMgr.m_SpatialDBase.GetSurfaceByGuid(m_CurrentGuid);
    if( !pSurface )
    {
        return;
    }

    SetZones( pSurface->ZoneInfo );
    SetTransform( pSurface->L2W );
    SetAttrBits( pSurface->AttrBits );
}
