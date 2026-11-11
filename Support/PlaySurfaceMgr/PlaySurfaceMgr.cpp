#include "PlaySurfaceMgr.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/PlaySurface.hpp"
#include "Objects/ProxyPlaySurface.hpp"
#include "GameLib/RigidGeomCollision.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

//=========================================================================
// typedefs and structures
//=========================================================================

struct file_header
{
    s32 Version;
    s32 NZones;
    s32 NPortals;
    s32 NGeoms;
};

//=========================================================================
// globals
//=========================================================================

playsurface_mgr g_PlaySurfaceMgr;

//=========================================================================
// zone_info implementation
//=========================================================================

playsurface_mgr::zone_info::zone_info( void ) :
    Resolved            (FALSE),
    FileOffset          (0),
    NSurfaces           (0),
    pSurfaces           (NULL),
    NColors             (0),
    pColorData          (NULL),
    hColorData          (NULL)
{
    // surface MUST be 16-byte aligned.
    ASSERT( (sizeof(surface) & 0xf) == 0 );
}

//=========================================================================

playsurface_mgr::zone_info::~zone_info( void )
{
    Unload();
}

//=========================================================================

void playsurface_mgr::zone_info::Unload( void )
{
    if ( pColorData )   x_free( pColorData );
    if ( pSurfaces  )   x_free( pSurfaces  );
    pSurfaces  = NULL;
    pColorData = NULL;
}

//=========================================================================
// playsurface_mgr implementation
//=========================================================================

playsurface_mgr::playsurface_mgr( void ) :
    m_Version           (VERSION),
    m_Zones             (),
    m_Portals           (),
    m_Geoms             (),
    m_File              (NULL),
    m_Loading           (FALSE),
    m_NextLoadZone      (-1),
    m_SpatialDBase      (),
    m_QueryNumber       (0),
    m_ProxyPlaySurface  (0)
{
};

//=========================================================================

playsurface_mgr::~playsurface_mgr( void )
{
    Reset();
}

//=========================================================================

void playsurface_mgr::Reset( void )
{
    s32 i;

    m_QueryNumber = 0;
    m_SpatialDBase.m_QueryNumber = 0;

    // unload all zones and portals
    for ( i = 0; i < m_Zones.GetCount(); i++ )
        UnloadZone(i);
    for ( i = 0; i < m_Portals.GetCount(); i++ )
        UnloadZone( m_Portals[i] );

    // clear out the data
    m_Zones.Clear();
    m_Portals.Clear();
    m_Geoms.Clear();

    m_SpatialDBase.Reset();

    // if there are no more playsurfaces, then there is no more need
    // for the proxy playsurface
    if( m_ProxyPlaySurface )
    {
        g_ObjMgr.DestroyObjectEx( m_ProxyPlaySurface, TRUE );
        g_ObjMgr.SetProxyPlaySurface( NULL );
        m_ProxyPlaySurface  = 0;
    }
}

//=========================================================================

void playsurface_mgr::Init( void )
{
    Reset();
}

//=========================================================================

void playsurface_mgr::Kill( void )
{
    Reset();
}

//=========================================================================

#ifdef X_DEBUG
void playsurface_mgr::SanityCheck( void )
{
    ASSERT( m_Version == VERSION );
    m_SpatialDBase.SanityCheck();
}
#endif

//=========================================================================

s32 playsurface_mgr::FindPortalIndex( u16 Zone1, u16 Zone2 )
{
    if ( Zone1 && Zone2 )
    {
        // this guy belongs in a portal, but which one?
        for ( s32 iPortal = 0; iPortal < g_ZoneMgr.GetPortalCount(); iPortal++ )
        {
            zone_mgr::portal& Portal = g_ZoneMgr.GetPortal(iPortal);

            if ( ((Portal.iZone[0] == Zone1) && (Portal.iZone[1] == Zone2)) ||
                 ((Portal.iZone[0] == Zone2) && (Portal.iZone[1] == Zone1)) )
            {
                return iPortal;
            }
        }
    }

    return -1;
}

//=========================================================================

s32 playsurface_mgr::FindZoneIndex( u16 Zone1, u16 Zone2 )
{
    // place it in the first zone?
    if ( Zone1 && (Zone1 < g_ZoneMgr.GetZoneCount()) )
    {
        return (s32)Zone1;
    }

    // place it in the second zone?
    if ( Zone2 && (Zone2 < g_ZoneMgr.GetZoneCount()) )
    {
        return (s32)Zone2;
    }

    // if all else fails, put it in the default/global zone
    return 0;
}

//=========================================================================

s32 playsurface_mgr::GetNameIndex( const char* GeomName )
{
    // is this name already in the list?
    for ( s32 i = 0; i < m_Geoms.GetCount(); i++ )
    {
        if ( !x_strcmp( m_Geoms[i].Name, GeomName ) )
        {
            return i;
        }
    }

    // if not, add it!
    geom_name& Geom = m_Geoms.Append();
    x_strsavecpy( Geom.Name, GeomName, RESOURCE_NAME_SIZE );
    return (m_Geoms.GetCount() - 1);
}

//=========================================================================

s32 SurfaceCompareFn( const void* p1, const void* p2 )
{
    playsurface_mgr::surface* pSurf1 = (playsurface_mgr::surface*)p1;
    playsurface_mgr::surface* pSurf2 = (playsurface_mgr::surface*)p2;

    if ( pSurf1->GeomNameIndex > pSurf2->GeomNameIndex )    return  1;
    if ( pSurf1->GeomNameIndex < pSurf2->GeomNameIndex )    return -1;

    return 0;
}

//=========================================================================

void playsurface_mgr::SortSurfaces( playsurface_mgr::zone_info& Zone )
{
    x_qsort( Zone.pSurfaces, Zone.NSurfaces, sizeof(surface), SurfaceCompareFn );
}

//=========================================================================

void playsurface_mgr::RebuildList( const xarray<guid>& lstGuidsToExport,asset_platform PlatformType )
{
    s32     i;
    s32     iGuid;

    // this routine is kinda nasty, but it is only done at export time, so it
    // shouldn't be that big of a problem
    Reset();

    // make the arrays big enough for our zones and portals
    s32 ZoneCount   = g_ZoneMgr.GetZoneCount();
    s32 PortalCount = g_ZoneMgr.GetPortalCount();
    ZoneCount = MAX( 1, ZoneCount );    // must have the default zone at least
    m_Zones.SetCapacity ( ZoneCount   );
    m_Portals.SetCapacity( PortalCount );

    // initialize the zones and portals
    for ( i = 0; i < ZoneCount; i++ )
    {
        zone_info& ZoneInfo       = m_Zones.Append();
        ZoneInfo.Resolved         = FALSE;
        ZoneInfo.FileOffset       = 0;
        ZoneInfo.NSurfaces        = 0;
        ZoneInfo.pSurfaces        = NULL;
        ZoneInfo.NColors          = 0;
        ZoneInfo.pColorData       = NULL;
    }

    for ( i = 0; i < PortalCount; i++ )
    {
        zone_info& ZoneInfo       = m_Portals.Append();
        ZoneInfo.Resolved         = FALSE;
        ZoneInfo.FileOffset       = 0;
        ZoneInfo.NSurfaces        = 0;
        ZoneInfo.pSurfaces        = NULL;
        ZoneInfo.NColors          = 0;
        ZoneInfo.pColorData       = NULL;
    }

    // how many playsurfaces will we need to store? And how much color data?
    for ( iGuid = 0; iGuid < lstGuidsToExport.GetCount(); iGuid++ )
    {
        // grab the relevant pointers
        object*       pObject      = g_ObjMgr.GetObjectByGuid( lstGuidsToExport[iGuid] );
        play_surface* pPlaySurface = &play_surface::GetSafeType( *pObject );
        rigid_inst&   RigidInst    = pPlaySurface->GetRigidInst();

        // which zone or portal does this belong to?
        u16 Zone1 = pObject->GetZone1();
        u16 Zone2 = pObject->GetZone2();
        ASSERT( Zone1 ? TRUE : (Zone2==0) );

        // does this guy belong in a portal?        
        s32 PortalIndex = FindPortalIndex( Zone1, Zone2 );
        if ( PortalIndex != -1 )
        {
            ASSERT( (PortalIndex >= 0) && (PortalIndex < PortalCount) );
            m_Portals[PortalIndex].NSurfaces++;
            m_Portals[PortalIndex].NColors += RigidInst.GetNumColors();
        }
        else
        {
            // it must belong in a zone
            s32 ZoneIndex = FindZoneIndex( Zone1, Zone2 );
            ASSERT( (ZoneIndex >= 0) && (ZoneIndex < ZoneCount) );
            m_Zones[ZoneIndex].NSurfaces++;
            m_Zones[ZoneIndex].NColors += RigidInst.GetNumColors();
        }
    }

    // now that we have our counts, allocate space for the surfaces and
    // color data (zones and portals), and as long as we're looping, clear out
    // the NSurfaces and NColors members (don't worry, they'll get added back in,
    // it's just a handy way to add the actual surface data).
    for ( i = 0; i < ZoneCount; i++ )
    {
        zone_info& ZoneInfo = m_Zones[i];
        if( ZoneInfo.NSurfaces )
            ZoneInfo.pSurfaces  = (surface*)x_malloc(sizeof(surface)*ZoneInfo.NSurfaces);
        if( ZoneInfo.NColors )
        {
            u32 Size;
            switch( PlatformType )
            {
                case ASSET_PLATFORM_XBOX:
                    Size = sizeof(u32)*ZoneInfo.NColors;
                    break;
                case ASSET_PLATFORM_PS2:
                    Size = sizeof(u16)*ZoneInfo.NColors;
                    break;
                case ASSET_PLATFORM_DESKTOP:
                    Size = sizeof(u32)*ZoneInfo.NColors;
                    break;

                default:
                    Size = 0xFFFFFFFF;
                    ASSERT(0);
                    break;
            }
            ZoneInfo.pColorData = x_malloc( Size );
        }

        ZoneInfo.NSurfaces = 0;
        ZoneInfo.NColors   = 0;
    }

    for ( i = 0; i < PortalCount; i++ )
    {
        zone_info& ZoneInfo = m_Portals[i];
        if( ZoneInfo.NSurfaces )
            ZoneInfo.pSurfaces  = (surface*)x_malloc(sizeof(surface)*ZoneInfo.NSurfaces);
        if( ZoneInfo.NColors )
        {
            u32 Size;
            switch( PlatformType )
            {
                case ASSET_PLATFORM_XBOX:
                    Size = sizeof(u32)*ZoneInfo.NColors;
                    break;
                case ASSET_PLATFORM_PS2:
                    Size = sizeof(u16)*ZoneInfo.NColors;
                    break;
                case ASSET_PLATFORM_DESKTOP:
                    Size = sizeof(u32)*ZoneInfo.NColors;
                    break;

                default:
                    ASSERT(0);
                    Size = 0xFFFFFFFF;
                    break;
            }
            ZoneInfo.pColorData = x_malloc( Size );
        }

        ZoneInfo.NSurfaces = 0;
        ZoneInfo.NColors   = 0;
    }

    // create the surface and color data
    for ( iGuid = 0; iGuid < lstGuidsToExport.GetCount(); iGuid++ )
    {
        // grab the relevant pointers
        object*             pObject      = g_ObjMgr.GetObjectByGuid( lstGuidsToExport[iGuid] );
        play_surface*       pPlaySurface = &play_surface::GetSafeType( *pObject );
        rigid_inst&         RigidInst    = pPlaySurface->GetRigidInst();

        // which zone or portal does this belong to?
        u16 Zone1 = pObject->GetZone1();
        u16 Zone2 = pObject->GetZone2();
        ASSERT( Zone1 ? TRUE : (Zone2==0) );

        // does this guy belong in a portal?        
        s32 PortalIndex = FindPortalIndex( Zone1, Zone2 );
        zone_info* pZoneInfo;
        if ( PortalIndex != -1 )
        {
            ASSERT( (PortalIndex >= 0) && (PortalIndex < PortalCount) );
            pZoneInfo = &m_Portals[PortalIndex];
        }
        else
        {
            // it must belong in a zone
            s32 ZoneIndex = FindZoneIndex( Zone1, Zone2 );
            ASSERT( (ZoneIndex >= 0) && (ZoneIndex < ZoneCount) );
            pZoneInfo = &m_Zones[ZoneIndex];
        }

        static const u32 AttrBits = object::ATTR_DISABLE_PROJ_SHADOWS     |
                                    object::ATTR_CAST_SHADOWS            |
                                    object::ATTR_RECEIVE_SHADOWS          |
                                    object::ATTR_COLLIDABLE               |
                                    object::ATTR_BLOCKS_PLAYER            |
                                    object::ATTR_BLOCKS_CHARACTER         |
                                    object::ATTR_BLOCKS_RAGDOLL           |
                                    object::ATTR_BLOCKS_SMALL_DEBRIS      |
                                    object::ATTR_BLOCKS_SMALL_PROJECTILES |
                                    object::ATTR_BLOCKS_LARGE_PROJECTILES |
                                    object::ATTR_BLOCKS_CHARACTER_LOS     |
                                    object::ATTR_BLOCKS_PLAYER_LOS        |
                                    object::ATTR_BLOCKS_PAIN_LOS;

        // add in the surface data
        surface* pSurface = &pZoneInfo->pSurfaces[pZoneInfo->NSurfaces];
        pSurface->L2W       = pObject->GetL2W();
        pSurface->WorldBBox = pObject->GetBBox();
        pSurface->AttrBits  = pObject->GetAttrBits() & AttrBits;
        switch( PlatformType )
        {
            case ASSET_PLATFORM_XBOX:
                pSurface->ColorOffset   = pZoneInfo->NColors*sizeof(u32);
                break;
            case ASSET_PLATFORM_PS2:
                pSurface->ColorOffset   = pZoneInfo->NColors*sizeof(u16);
                break;
            case ASSET_PLATFORM_DESKTOP:
                pSurface->ColorOffset   = pZoneInfo->NColors*sizeof(u32);
                break;

            default:
                ASSERT(0);
                break;
        }
        pSurface->GeomNameIndex = GetNameIndex(RigidInst.GetRigidGeomName());
        pSurface->DBaseQuery    = 0;
        pSurface->ZoneInfo      = (Zone1&0xff) | ((Zone2&0xff)<<8);
        pSurface->RenderFlags   = 0;
        if( pSurface->AttrBits & object::ATTR_DISABLE_PROJ_SHADOWS )
        {
            pSurface->RenderFlags |= render::DISABLE_PROJ_SHADOWS;
        }

        // add in the color data
        const u32* pInstColors = RigidInst.GetColorTable( PlatformType );
        if ( RigidInst.GetNumColors() && pInstColors )
        {
            void* pColorData = ((byte*)pZoneInfo->pColorData)+pSurface->ColorOffset;
            u32 Size;
            switch( PlatformType )
            {
                case ASSET_PLATFORM_XBOX:
                    Size = RigidInst.GetNumColors()*sizeof(u32);
                    break;
                case ASSET_PLATFORM_PS2:
                    Size = RigidInst.GetNumColors()*sizeof(u16);
                    break;
                case ASSET_PLATFORM_DESKTOP:
                    Size = RigidInst.GetNumColors()*sizeof(u32);
                    break;

                default:
                    ASSERT(0);
                    Size = 0xFFFFFFFF;
                    break;
            }
            x_memcpy( pColorData,pInstColors,Size );
        }

        // keep our counts current
        pZoneInfo->NSurfaces++;
        pZoneInfo->NColors   += RigidInst.GetNumColors();
    }

    // now we have enough information to build up the spatial database
    static const s32 kSpatialCellSize = 800; // 8 meters
    m_SpatialDBase.StartNewDBase( kSpatialCellSize );
    for ( i = 0; i < ZoneCount; i++ )
    {
        zone_info& ZoneInfo = m_Zones[i];
        for ( s32 iSurface = 0; iSurface < ZoneInfo.NSurfaces; iSurface++ )
            m_SpatialDBase.AddToNewDBase( ZoneInfo.pSurfaces[iSurface] );
    }
    for ( i = 0; i < PortalCount; i++ )
    {
        zone_info& ZoneInfo = m_Portals[i];
        for ( s32 iSurface = 0; iSurface < ZoneInfo.NSurfaces; iSurface++ )
            m_SpatialDBase.AddToNewDBase( ZoneInfo.pSurfaces[iSurface] );
    }
    m_SpatialDBase.EndNewDBase();

    // done! that was easy, right?
}

//=========================================================================

void playsurface_mgr::OpenFile( const char* Filename, xbool DoLoad )
{
    if ( DoLoad )
    {
        m_Loading = TRUE;
        m_File    = x_fopen( Filename, "rb" );
        if ( !m_File )
        {
            ASSERT( FALSE );
            return;
        }

        // load the info shared by everyone
        LoadBasicInfo();
    }
    else
    {
        m_Loading = FALSE;
        m_File    = x_fopen( Filename, "wb" );
        if ( !m_File )
        {
            ASSERT( FALSE );
            return;
        }
    }
}

//=========================================================================

void playsurface_mgr::UnloadZone( zone_info& Zone )
{
    // unregister the playsurfaces and remove them from the spatial dbase
    if ( Zone.Resolved && Zone.pSurfaces )
    {
        for ( s32 i = 0; i < Zone.NSurfaces; i++ )
        {
            if( Zone.pSurfaces[i].RenderInst.IsNonNull() )
            {
                // unregister the render instance
                render::UnregisterRigidInstance( Zone.pSurfaces[i].RenderInst );
            }

            // and remove it from the spatial database
            m_SpatialDBase.RemoveSurface( Zone.pSurfaces[i] );
        }

        Zone.Resolved = FALSE;
    }

    // delete the associated data
    Zone.Unload();
}

//=========================================================================

void playsurface_mgr::LoadZone( zone_info& Zone )
{
    MEMORY_OWNER( "LOAD ZONE" );

    // make sure we're unloaded so we don't cause memory leaks
    UnloadZone(Zone);

    // empty zone?
    if ( Zone.NSurfaces == 0 )
        return;

    // seek to the surface and color data
    x_fseek( m_File, Zone.FileOffset, X_SEEK_SET );
    
    // load the surfaces
    {
        MEMORY_OWNER( "PLAYSURFACES" );
        Zone.pSurfaces  = (surface*)x_malloc(sizeof(surface)*Zone.NSurfaces);
        ASSERT( Zone.pSurfaces );
#if defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
        const s32 DiskSurface = 128;
        byte* pDisk = (byte*)x_malloc( DiskSurface * Zone.NSurfaces );
        ASSERT( pDisk );
        x_fread( pDisk, DiskSurface, Zone.NSurfaces, m_File );
        for( s32 i = 0; i < Zone.NSurfaces; i++ )
        {
            byte* s = pDisk + i*DiskSurface;
            byte* d = (byte*)&Zone.pSurfaces[i];
            x_memset( d, 0, sizeof(surface) );
            x_memcpy( d,       s,       108 );  // L2W .. RenderInst
            x_memcpy( d + 120, s + 112, 16  );  // DBaseQuery .. RenderFlags
        }
        x_free( pDisk );
#else
        x_fread( Zone.pSurfaces, sizeof(surface), Zone.NSurfaces, m_File );
#endif
    }

    // load the color data
    if ( Zone.NColors )
    {
        MEMORY_OWNER( "COLOR DATA" );
        Zone.pColorData = (byte*)x_malloc(Zone.NColors*sizeof(u32));
        ASSERT ( Zone.pColorData );
        x_fread( Zone.pColorData, 1, Zone.NColors*sizeof(u32), m_File );
    }

    // resolve pointers and geometry handles
    ResolveSurfaceData( Zone );
}

//=========================================================================

void playsurface_mgr::UnloadZone( zone_mgr::zone_id Zone )
{
    ASSERT( Zone<m_Zones.GetCount() );
    zone_info& ZoneInfo = m_Zones[(s32)Zone];
    UnloadZone( ZoneInfo );
}

//=========================================================================

void playsurface_mgr::LoadZone( zone_mgr::zone_id Zone )
{
    ASSERT( m_File && m_Loading );
    ASSERT( Zone<m_Zones.GetCount() );
    zone_info& ZoneToLoad = m_Zones[(s32)Zone];
    LoadZone( ZoneToLoad );
}

//=========================================================================

void playsurface_mgr::BeginLoadAllZones( void )
{
    ASSERT( m_File && m_Loading );
    ASSERT( m_NextLoadZone == -1 );
    m_NextLoadZone = 1;
}

//=========================================================================

xbool playsurface_mgr::UpdateLoadAllZones( void )
{
    ASSERT( m_File && m_Loading );
    ASSERT( m_NextLoadZone >= 1 );

    if( m_NextLoadZone < m_Zones.GetCount() )
    {
        LoadZone( m_NextLoadZone );
        m_NextLoadZone++;
        return FALSE;
    }

    #ifdef X_DEBUG
    m_SpatialDBase.SanityCheck();
    #endif

    m_NextLoadZone = -1;
    return TRUE;
}

//=========================================================================

void playsurface_mgr::SaveFile( asset_platform PlatformType )
{
    ASSERT( m_File && !m_Loading );

    // save out the header
    file_header Hdr;
    Hdr.Version  = VERSION;
    Hdr.NZones   = m_Zones.GetCount();
    Hdr.NPortals = m_Portals.GetCount();
    Hdr.NGeoms   = m_Geoms.GetCount();
    x_fwrite( &Hdr, sizeof(file_header), 1, m_File );

    // save out the spatial database
    m_SpatialDBase.Save( m_File );

    // save out the pieces of unique geometry
    x_fwrite( m_Geoms.GetPtr(), sizeof(geom_name), m_Geoms.GetCount(), m_File );

    // where would playsurfaces and colors begin?
    s32 StartOffset = x_ftell( m_File );
    StartOffset    += m_Zones.GetCount() * sizeof(zone_info);
    StartOffset    += m_Portals.GetCount() * sizeof(zone_info);

    // save out zones with the appropriate pointers cleared out, and the
    // file offsets figured out
    s32 i;
    for ( i = 0; i < m_Zones.GetCount(); i++ )
    {
        zone_info ZoneToSave   = m_Zones[i];
        ZoneToSave.pColorData  = NULL;
        ZoneToSave.pSurfaces   = NULL;
        ZoneToSave.Resolved    = FALSE;
        ZoneToSave.FileOffset  = StartOffset;
        StartOffset           += m_Zones[i].NSurfaces*sizeof(surface);
        switch( PlatformType )
        {
            case ASSET_PLATFORM_XBOX:
                StartOffset   += m_Zones[i].NColors*sizeof(u32);
                break;
            case ASSET_PLATFORM_PS2:
                StartOffset   += m_Zones[i].NColors*sizeof(u16);
                break;
            case ASSET_PLATFORM_DESKTOP:
                StartOffset   += m_Zones[i].NColors*sizeof(u32);
                break;

            default:
                ASSERT(0);
                break;
        }
        x_fwrite( &ZoneToSave, sizeof(zone_info), 1, m_File );
    }

    // save out portals with the appropriate pointers cleared out, and the
    // file offsets figured out
    for ( i = 0; i < m_Portals.GetCount(); i++ )
    {
        zone_info ZoneToSave   = m_Portals[i];
        ZoneToSave.pColorData  = NULL;
        ZoneToSave.pSurfaces   = NULL;
        ZoneToSave.Resolved    = FALSE;
        ZoneToSave.FileOffset  = StartOffset;
        StartOffset           += m_Portals[i].NSurfaces*sizeof(surface);
        switch( PlatformType )
        {
            case ASSET_PLATFORM_XBOX:
                StartOffset   += m_Portals[i].NColors*sizeof(u32);
                break;
            case ASSET_PLATFORM_PS2:
                StartOffset   += m_Portals[i].NColors*sizeof(u16);
                break;
            case ASSET_PLATFORM_DESKTOP:
                StartOffset   += m_Portals[i].NColors*sizeof(u32);
                break;

            default:
                ASSERT(0);
                break;
        }
        x_fwrite( &ZoneToSave, sizeof(zone_info), 1, m_File );
    }

    // save out the playsurfaces and colorsfor zones and portals
    for ( i = 0; i < m_Zones.GetCount(); i++ )
    {
        zone_info& ZoneToSave = m_Zones[i];
        if ( ZoneToSave.NSurfaces )
            x_fwrite( ZoneToSave.pSurfaces, sizeof(surface), ZoneToSave.NSurfaces, m_File );
        if ( ZoneToSave.NColors )
        {
            switch( PlatformType )
            {
                case ASSET_PLATFORM_XBOX:
                    x_fwrite( ZoneToSave.pColorData, sizeof(u32), ZoneToSave.NColors, m_File );
                    break;
                case ASSET_PLATFORM_PS2:
                    x_fwrite( ZoneToSave.pColorData, sizeof(u16), ZoneToSave.NColors, m_File );
                    break;
                case ASSET_PLATFORM_DESKTOP:
                    x_fwrite( ZoneToSave.pColorData, sizeof(u32), ZoneToSave.NColors, m_File );
                    break;

                default:
                    ASSERT(0);
                    break;
            }
        }
    }

    for ( i = 0; i < m_Portals.GetCount(); i++ )
    {
        zone_info& ZoneToSave = m_Portals[i];
        if ( ZoneToSave.NSurfaces )
            x_fwrite( ZoneToSave.pSurfaces, sizeof(surface), ZoneToSave.NSurfaces, m_File );
        if ( ZoneToSave.NColors )
        {
            switch( PlatformType )
            {
                case ASSET_PLATFORM_XBOX:
                    x_fwrite( ZoneToSave.pColorData, sizeof(u32), ZoneToSave.NColors, m_File );
                    break;
                case ASSET_PLATFORM_PS2:
                    x_fwrite( ZoneToSave.pColorData, sizeof(u16), ZoneToSave.NColors, m_File );
                    break;
                case ASSET_PLATFORM_DESKTOP:
                    x_fwrite( ZoneToSave.pColorData, sizeof(u32), ZoneToSave.NColors, m_File );
                    break;

                default:
                    ASSERT(0);
                    break;
            }
        }
    }

    // sanity check
    ASSERT( StartOffset == x_ftell(m_File) );
}

//=========================================================================

void playsurface_mgr::CloseFile( void )
{
    ASSERT( m_File );
    ASSERT( m_NextLoadZone == -1 );
    x_fclose( m_File );
    m_Loading = FALSE;
    m_File    = NULL;
}

//=========================================================================

void playsurface_mgr::CreateProxyPlaySurfaceObject( void )
{
    // If we're about to start loading playsurfaces, then we need to provide
    // and interface for the game to talk to them.
    m_ProxyPlaySurface  = g_ObjMgr.CreateObject( proxy_playsurface::GetObjectType() );
    object* pProxy      = g_ObjMgr.GetObjectByGuid(m_ProxyPlaySurface);
    ASSERT( pProxy );
    g_ObjMgr.SetProxyPlaySurface( pProxy );
}

//=========================================================================

void playsurface_mgr::ReadZoneInfo( zone_info* pZones, s32 Count )
{
#if defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
    const s32 DiskZone = 28;
    for( s32 i = 0; i < Count; i++ )
    {
        byte Rec[28];
        x_fread( Rec, DiskZone, 1, m_File );
        zone_info& Z = pZones[i];
        x_memcpy( &Z.FileOffset, &Rec[ 4], 4 );
        x_memcpy( &Z.NSurfaces,  &Rec[ 8], 4 );
        x_memcpy( &Z.NColors,    &Rec[16], 4 );
        Z.Resolved   = FALSE;
        Z.pSurfaces  = NULL;
        Z.pColorData = NULL;
        Z.hColorData = NULL;
    }
#else
    x_fread( pZones, sizeof(zone_info), Count, m_File );
#endif
}

//=========================================================================

void playsurface_mgr::LoadBasicInfo()
{
    MEMORY_OWNER("BASIC INFO");
    ASSERT( m_File && m_Loading );
    x_fseek( m_File, 0, X_SEEK_SET );

    // make sure we start with a clean slate
    Reset();

    // Create the proxy play surface.
    CreateProxyPlaySurfaceObject();

    // load the header info
    file_header Hdr;
    x_fread( &Hdr, 1, sizeof(file_header), m_File );
    if ( Hdr.Version != VERSION )
        x_throw( "Playsurface data is wrong version." );

    // load the spatial database information
    {
        MEMORY_OWNER("SPATIAL DBASE");
        m_SpatialDBase.Load( m_File );
    }

    // load the different geometries this level uses
    {
        MEMORY_OWNER("GEOMETRY NAMES");
        m_Geoms.SetCapacity( Hdr.NGeoms );
        m_Geoms.SetCount( Hdr.NGeoms );
        x_fread( m_Geoms.GetPtr(), sizeof(geom_name), Hdr.NGeoms, m_File );
    }

    // load the basic zone information
    {
        MEMORY_OWNER("ZONE INFO");
        m_Zones.SetCapacity( Hdr.NZones );
        m_Zones.SetCount( Hdr.NZones );
        ReadZoneInfo( m_Zones.GetPtr(), Hdr.NZones );
    }

    // load the basic portal information
    {
        MEMORY_OWNER("PORTAL INFO");
        m_Portals.SetCapacity( Hdr.NPortals );
        m_Portals.SetCount( Hdr.NPortals );
        ReadZoneInfo( m_Portals.GetPtr(), Hdr.NPortals );
    }

    // load the globals zone
    if ( m_Zones.GetCount() > 0 )
        LoadZone( 0 );

    // load all of the portals
    for ( s32 i = 0; i < m_Portals.GetCount(); i++ )
    {
        LoadZone( m_Portals[i] );
    }

    // sanity check time
    if ( g_ZoneMgr.GetZoneCount() && (m_Zones.GetCount() != g_ZoneMgr.GetZoneCount()) )
        x_throw( "playsurface data and zones don't match" );
    if ( g_ZoneMgr.GetPortalCount() && (m_Portals.GetCount() != g_ZoneMgr.GetPortalCount()) )
        x_throw( "playsurface data and portals don't match" );
}

//=========================================================================

void playsurface_mgr::ResolveSurfaceData( zone_info& Zone )
{
    ASSERT( Zone.Resolved == FALSE );
    Zone.Resolved = TRUE;

    s32 iSurface;
    for ( iSurface = 0; iSurface < Zone.NSurfaces; iSurface++ )
    {
        surface* pSurface = &Zone.pSurfaces[iSurface];

        // fix up the color pointer
        pSurface->pColor = ((byte*)Zone.pColorData)+pSurface->ColorOffset;

        // fix up the geometry
        s32 iGeomName = pSurface->GeomNameIndex;
        ASSERT( (iGeomName>=0) && (iGeomName < m_Geoms.GetCount()) );
        rhandle<rigid_geom> RigidGeom;
        RigidGeom.SetName( m_Geoms[iGeomName].Name );
        rigid_geom* pGeom = RigidGeom.GetPointer();
        //ASSERT( pGeom );
        if ( pGeom )
        {
            pSurface->RenderInst = render::RegisterRigidInstance( *pGeom );
        }
        else
        {
            pSurface->RenderInst = HNULL;
        }

        // make sure our database queries will work out the first time through
        ASSERT( pSurface->DBaseQuery == 0 );
    }

    // sort the surfaces by their geometry indices. *hopefully* this will
    // improve the data cache slightly for rendering...
    SortSurfaces( Zone );

    // add the surfaces to our spatial database
    for ( iSurface = 0; iSurface < Zone.NSurfaces; iSurface++ )
        m_SpatialDBase.AddSurface( Zone.pSurfaces[iSurface] );
}

//=========================================================================

void playsurface_mgr::PrepVisCheck( void )
{
}

//=========================================================================

s32 playsurface_mgr::VisCheck( const bbox& BBox, u32 CheckPlaneMask )
{
    return g_ObjMgr.IsBoxInView( BBox, CheckPlaneMask );
}

//=========================================================================

void playsurface_mgr::RenderZone( zone_info& ZoneInfo, zone_mgr::zone_id Zone1, zone_mgr::zone_id Zone2 )
{
    static xprofile_counter playSurfaceCandidates =
        x_GetProfiler().RegisterCounter( "PlaySurfaceCandidates", "RenderCounter" );
    static xprofile_counter playSurfaceZoneCulled =
        x_GetProfiler().RegisterCounter( "PlaySurfaceZoneCulled", "RenderCounter" );
    static xprofile_counter playSurfaceFrustumCulled =
        x_GetProfiler().RegisterCounter( "PlaySurfaceFrustumCulled", "RenderCounter" );
    static xprofile_counter playSurfaceSubmissions =
        x_GetProfiler().RegisterCounter( "PlaySurfaceSubmissions", "RenderCounter" );

    s32 i;

    // if this zone isn't loaded fully, we can't render it
    if ( ZoneInfo.Resolved == FALSE )
        return;

    if ( ZoneInfo.NSurfaces == 0 )
        return;

    playSurfaceCandidates.Add( ZoneInfo.NSurfaces );

    // use the starting zone to determine whether we should do a zone vis check
    s32   StartingZone = g_ZoneMgr.GetStartingZone();
    xbool bNotInStartingZone = !((Zone1==StartingZone) || (Zone2==StartingZone));

    // limit ourselves to 4k of playsurfaces in spad (using 8k total double-buffered)
    const s32 kMaxSurfacesToProcess = 4096 / sizeof(surface);
    s32 NSurfacesProcessed = 0;

    // dma the first batch of surfaces to scratchpad
    s32 SpadOffsets[2] = { 0, 4096 };
    s32 Buffer = 0;
    s32 nSurfacesToDma = MIN(kMaxSurfacesToProcess, ZoneInfo.NSurfaces-NSurfacesProcessed);
    surface* pDmaSource = ZoneInfo.pSurfaces;
    pDmaSource += nSurfacesToDma;
    Buffer = !Buffer;

    // what are the default render flags?
    s32 DefaultFlags = 0;

#ifndef X_RETAIL
    if ( eng_ScreenShotActive() )
        DefaultFlags |= render::CLIPPED;
#endif

    // the surface processing loop
    surface* pSurface = ZoneInfo.pSurfaces;
    while ( NSurfacesProcessed < ZoneInfo.NSurfaces )
    {
        // how many surfaces will this loop handle?
        s32 NSurfacesToProcess = nSurfacesToDma;

        // start dma'ing the next batch
        nSurfacesToDma = MIN(kMaxSurfacesToProcess, ZoneInfo.NSurfaces-NSurfacesProcessed-NSurfacesToProcess);
        if ( nSurfacesToDma )
        {
            pDmaSource += nSurfacesToDma;
        }
        Buffer = !Buffer;

        // handle the current batch of surfaces
        surface* pSpadSurface = pSurface;
        for ( i = 0; i < NSurfacesToProcess; i++, pSurface++, pSpadSurface++ )
        {
            // check if this surface is in the zone
            if ( bNotInStartingZone && !g_ZoneMgr.IsBBoxVisible(pSpadSurface->WorldBBox, Zone1, Zone2 ) )
            {
                playSurfaceZoneCulled.Add();
                continue;
            }

            // check for clipping against the view frustum
            s32 Vis = VisCheck(pSpadSurface->WorldBBox, XBIN(111111));
            if ( Vis == -1 )
            {
                playSurfaceFrustumCulled.Add();
                continue;
            }

            // check if we have a valid instance (bad data could cause this to get hit)
            if ( pSpadSurface->RenderInst.IsNull() )
            {
                continue;
            }

            // accumulate any other flags (Note that we don't use pSpadSurface because
            // we have no guarantee that pSurface was flushed from the cache before
            // our dma. That's okay becauses everything we access besides the render
            // flags is constant.)
            s32 Flags = pSurface->RenderFlags | DefaultFlags;

            // to clip or not to clip?
            if ( Vis )
                Flags |= render::CLIPPED;

            // render it
            render::GetRenderPipeline().GetSubmission().AddRigidInstanceSimple( pSpadSurface->RenderInst,
                                            (const u32*)pSpadSurface->pColor,
                                            &pSurface->L2W,
                                            pSpadSurface->WorldBBox,
                                            Flags );
            playSurfaceSubmissions.Add();

            // clear any accumulated flags for the next frame
            pSurface->RenderFlags &= ~render::CLIPPED;
        }

        // move to the next batch
        NSurfacesProcessed += NSurfacesToProcess;
    }

    ASSERT( pDmaSource == ZoneInfo.pSurfaces + ZoneInfo.NSurfaces );
}

//=========================================================================

#ifndef X_RETAIL
void playsurface_mgr::RenderZoneCollision( zone_info& ZoneInfo, xbool bRenderHigh )
{
    s32 i;

    // if this zone isn't loaded fully, we can't render it
    if ( ZoneInfo.Resolved == FALSE )
        return;

    if ( ZoneInfo.NSurfaces == 0 )
        return;

    // Render all of the playsurfaces
    for ( i = 0; i < ZoneInfo.NSurfaces; i++ )
    {
        surface* pSurface = &ZoneInfo.pSurfaces[i];

        if ( pSurface->RenderInst.IsNull() )
            continue;

        matrix4* pL2W = NULL;

        pL2W = &pSurface->L2W;

        // grab the useful pointers out
        rigid_geom* pGeom = (rigid_geom*)render::GetGeom( pSurface->RenderInst );


        RigidGeom_RenderCollision( pL2W, pGeom, bRenderHigh, 0xFFFFFFFF );
    }
}
#endif // X_RETAIL

//=========================================================================

void playsurface_mgr::RenderPlaySurfaces( void )
{
#ifndef X_EDITOR
    s32 i;

    if ( m_Zones.GetCount() == 0 )
        return;

    // render the zones
    PrepVisCheck();
    RenderZone( m_Zones[0], 0, 0 );   // default zone is always visible
    for ( i = 1; i < m_Zones.GetCount(); i++ )
    {
        if ( g_ZoneMgr.IsZoneVisible((u8)i) )
        {
            RenderZone( m_Zones[i], i, 0 );
        }
    }

    // render the portals
    for ( i = 0; i < m_Portals.GetCount(); i++ )
    {
        zone_mgr::portal& Portal = g_ZoneMgr.GetPortal(i);
        if ( g_ZoneMgr.IsZoneVisible(Portal.iZone[0]) ||
             g_ZoneMgr.IsZoneVisible(Portal.iZone[1]) )
        {
            RenderZone( m_Portals[i], Portal.iZone[0], Portal.iZone[1] );
        }
    }
#endif // X_EDITOR
}

#ifndef X_RETAIL
void playsurface_mgr::RenderPlaySurfacesCollision( xbool bRenderHi )
{
#ifndef X_EDITOR
    s32 i;

    if ( m_Zones.GetCount() == 0 )
        return;

    // render the zones
    PrepVisCheck();
    RenderZone( m_Zones[0], 0, 0 );   // default zone is always visible
    for ( i = 1; i < m_Zones.GetCount(); i++ )
    {
        if ( g_ZoneMgr.IsZoneVisible((u8)i) )
        {
            RenderZoneCollision( m_Zones[i], bRenderHi );
        }
    }

    // render the portals
    for ( i = 0; i < m_Portals.GetCount(); i++ )
    {
        zone_mgr::portal& Portal = g_ZoneMgr.GetPortal(i);
        if ( g_ZoneMgr.IsZoneVisible(Portal.iZone[0]) ||
             g_ZoneMgr.IsZoneVisible(Portal.iZone[1]) )
        {
            RenderZoneCollision( m_Portals[i], bRenderHi );
        }
    }
#endif // X_EDITOR
}
#endif // X_RETAIL

//=========================================================================

void playsurface_mgr::ClearDBaseQueries( void )
{
    s32 i, j;
    for ( i = 0; i < m_Zones.GetCount(); i++ )
    {
        zone_info& ZoneInfo = m_Zones[i];
        for ( j = 0; j < ZoneInfo.NSurfaces; j++ )
            ZoneInfo.pSurfaces[j].DBaseQuery = 0;
    }

    for ( i = 0; i < m_Portals.GetCount(); i++ )
    {
        zone_info& ZoneInfo = m_Portals[i];
        for ( j = 0; j < ZoneInfo.NSurfaces; j++ )
            ZoneInfo.pSurfaces[j].DBaseQuery = 0;
    }
}

//=========================================================================

void playsurface_mgr::CollectSurfaces( const bbox&  BBox,
                                       u32          Attributes,
                                       u32          NotTheseAttributes )
{
    if ( m_Zones.GetCount() == 0 )
    {
        m_QueryNumber = 0;
        return;
    }

    m_QueryNumber++;
    if ( m_QueryNumber == 0 )
    {
        // zero is considered a special query, and will force us to reset
        // all of the id's so that we are 100% correct when this number
        // wraps around
        ClearDBaseQueries();
        m_QueryNumber++;
    }

    m_SpatialDBase.CollectSurfaces( BBox, m_QueryNumber, Attributes, NotTheseAttributes );
}

//=========================================================================

void playsurface_mgr::CollectSurfaces( const vector3& RayStart,
                                       const vector3& RayEnd,
                                       u32            Attributes,
                                       u32            NotTheseAttributes )
{
    if ( m_Zones.GetCount() == 0 )
    {
        m_QueryNumber = 0;
        return;
    }

    m_QueryNumber++;
    if ( m_QueryNumber == 0 )
    {
        // zero is considered a special query, and will force us to reset
        // all of the id's so that we are 100% correct when this number
        // wraps around
        ClearDBaseQueries();
        m_QueryNumber++;
    }

    m_SpatialDBase.CollectSurfaces( RayStart, RayEnd, m_QueryNumber, Attributes, NotTheseAttributes );
}

//=========================================================================

playsurface_mgr::surface* playsurface_mgr::GetNextSurface( void )
{
    return m_SpatialDBase.GetNextSurface();
}

//==============================================================================
