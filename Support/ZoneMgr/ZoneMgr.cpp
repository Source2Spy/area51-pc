
#include "Render/Render.hpp"
#include "ZoneMgr.hpp"
#include "UI/ui_renderer.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "Objects/object.hpp"

#ifndef X_EDITOR
#include "NetworkMgr/Game/GameMgr.hpp"
#endif

//=========================================================================
// VARIABLES
//=========================================================================

zone_mgr g_ZoneMgr;

#define FLOAT1_TO_U8BIT( n ) ( (u8)( ( n * 255.0f ) + 0.5f ) )
#define U8BIT_TO_FLOAT1( n ) ( (f32)n / 255.0f )
#define MAX_FARCLIP_SCALE 10.0f
#define MIN_ZONE_VALUE 0.01f

static const f32 s_PortalPlaneEpsilon        = 0.01f;
static const f32 s_PortalIntersectionEpsilon = 0.00001f;

//==============================================================================
// DEBUG
//==============================================================================

s32     s_ParentZone      = -1;
s32     s_ParentPortal    = -1;
X_FILE* pFile             = NULL;
xbool   s_PrintPortalWalk = FALSE;

//==============================================================================
// SORTING
//==============================================================================

/*
static
s32 WeightSort( const void* pItem1, const void* pItem2 )
{
    zone_mgr::node* pNode1 = (zone_mgr::node*)pItem1;
    zone_mgr::node* pNode2 = (zone_mgr::node*)pItem2;

    f32 NearClip, FarClip;
    g_AudioManager.GetClipRange( NearClip, FarClip );
    
    // New nodes the volume weight.
    f32 Percent1 = MAX( 1.0f-(pNode1->Distance / (FarClip*MAX_FARCLIP_SCALE)), 0.0f );

    f32 Weight1  = pNode1->SndWeight * Percent1;

    // New nodes the volume weight.
    f32 Percent2 = MAX( 1.0f-(pNode2->Distance / (FarClip*MAX_FARCLIP_SCALE)), 0.0f );

    f32 Weight2  = pNode2->SndWeight * Percent2;

    // The lowest weight goes first.
    if( Weight1 > Weight2 )
        return -1;
    else if( Weight1 == Weight2 )
        return 0;
    else
        return 1;
}
*/
//-------------------------------------------------------------------------------

/*
static
s32 DistanceSort( const void* pItem1, const void* pItem2 )
{
    zone_mgr::node* pNode1 = (zone_mgr::node*)pItem1;
    zone_mgr::node* pNode2 = (zone_mgr::node*)pItem2;

    // The lowest distance goes first.
    if( pNode1->Distance < pNode2->Distance )
        return -1;
    else if( pNode1->Distance == pNode2->Distance )
        return 0;
    else
        return 1;
}
*/

//-------------------------------------------------------------------------------

/*
static
s32 ZoneIndexSort( const void* pItem1, const void* pItem2 )
{
    zone_mgr::node* pNode1 = (zone_mgr::node*)pItem1;
    zone_mgr::node* pNode2 = (zone_mgr::node*)pItem2;

    // Smallest zone index goes first.
    if( pNode1->iZone < pNode2->iZone )
        return -1;
    else if( pNode1->iZone == pNode2->iZone )
        return 0;
    else
        return 1;
}
*/

//=========================================================================
// FUNCTIONS
//=========================================================================

void zone_mgr::Reset( void )
{
    if( m_pPortal )
        delete[] m_pPortal;
    if( m_pZone )
        delete[] m_pZone;
    if( m_pZone2Portal )
        delete[] m_pZone2Portal;
    if( m_pVisivilityBits )
        delete[] m_pVisivilityBits;

    m_nPortals        = 0;
    m_pPortal         = NULL;
    m_nZones          = 0;
    m_pZone           = NULL;
    m_nZone2Portal    = 0;
    m_pZone2Portal    = NULL;
    m_pVisivilityBits = NULL;
    m_bAddMode        = FALSE;
    m_MaxPortals      = 0;
    m_Frustum.Clear();
    m_bPortalCullingEnabled    = FALSE;
    m_bHasValidVisibilityState = FALSE;

    m_GuidLookup.Clear();
}

//=========================================================================

zone_mgr::zone_mgr( void )
{
    m_nPortals        = 0;
    m_pPortal         = NULL;
    m_nZones          = 0;
    m_pZone           = NULL;
    m_nZone2Portal    = 0;
    m_pZone2Portal    = NULL;
    m_pVisivilityBits = NULL;
    m_bAddMode        = FALSE;
    m_Frustum.Clear();
    m_MaxPortals               = 0;
    m_bPortalCullingEnabled    = FALSE;
    m_bHasValidVisibilityState = FALSE;
}

//=========================================================================

zone_mgr::~zone_mgr( void )
{
    if( m_pPortal )
        delete[] m_pPortal;
    if( m_pZone )
        delete[] m_pZone;
    if( m_pZone2Portal )
        delete[] m_pZone2Portal;
    if( m_pVisivilityBits )
        delete[] m_pVisivilityBits;
}

//=========================================================================

void zone_mgr::AddStart( s32 nZones, s32 nPortals )
{
    (void)nZones;

    ASSERT( m_bAddMode == FALSE );
    ASSERT( m_pPortal == NULL );
    ASSERT( m_pZone == NULL );

    // Allocate zones and portals
    m_nPortals   = 0;
    m_MaxPortals = nPortals;
    m_pPortal    = new portal[m_MaxPortals];

    if( m_pPortal == NULL )
        x_throw( "out of memory" );

    m_nZones = 256; // +1 because zone 0 actually is an empty zone
    m_pZone  = new zone[m_nZones];

    if( m_pZone == NULL )
        x_throw( "out of memory" );
    x_memset( m_pZone, 0, sizeof( zone ) * m_nZones );

    // Initialize zone zero
    m_pZone[0].BBox.Clear();
    m_pZone[0].nPortals       = 0;
    m_pZone[0].iPortal2Portal = 0;

    // indicate that we are in adding mode
    m_bAddMode = TRUE;

    // Set the capacity for the guid lookup
    m_GuidLookup.SetCapacity( nPortals, FALSE );
}

//=========================================================================

void zone_mgr::AddPortal( guid Guid, const bbox& BBox, vector3* pEdges, s32 ZoneA, s32 ZoneB, f32 SoundOcclusion )
{
    ASSERT( m_bAddMode );
    ASSERT( m_nPortals < m_MaxPortals );

    s32     Index  = m_nPortals;
    portal& Portal = m_pPortal[m_nPortals++];

    Portal.Guid     = Guid;
    Portal.BBox     = BBox;
    Portal.iZone[0] = ZoneA;
    Portal.iZone[1] = ZoneB;
    Portal.Edges[0] = pEdges[0];
    Portal.Edges[1] = pEdges[1];
    Portal.Edges[2] = pEdges[2];
    Portal.Edges[3] = pEdges[3];
    Portal.Flags    = 0;
    Portal.Plane.Setup( pEdges[0], pEdges[1], pEdges[3] );
    Portal.Occlusion     = SoundOcclusion;
    Portal.BaseOcclusion = SoundOcclusion;

    m_GuidLookup.Add( Guid, Index );
}

//=========================================================================

void zone_mgr::AddZone( const bbox& BBox,
                        s32         ZoneID,
                        f32         SndWeight,
                        s32         MinPlayers,
                        s32         MaxPlayers,
                        const char* EnvMap,
                        const char* FogName,
                        xbool       QuickFog )
{
    ASSERT( m_bAddMode );
    ASSERT( ZoneID < m_nZones );

    zone& Zone          = m_pZone[ZoneID];
    Zone.BBox           = BBox;
    Zone.nPortals       = 0;
    Zone.iPortal2Portal = 0;
    Zone.bStack         = FALSE;
    Zone.SndWeight      = SndWeight;

    Zone.MinPlayers = MinPlayers;
    Zone.MaxPlayers = MaxPlayers;

    x_strsavecpy( Zone.EnvMapName, EnvMap, 128 );
    x_strsavecpy( Zone.FogName, FogName, 128 );
    Zone.QuickFog = QuickFog;

#ifndef X_EDITOR
    GameMgr.SetZoneLimits( ZoneID, MinPlayers, MaxPlayers );
#else
    (void)MinPlayers;
    (void)MaxPlayers;
#endif
}

//=========================================================================

void zone_mgr::AddEnd( void )
{
    s32 i, j;

    ASSERT( m_bAddMode );
    ASSERT( m_nPortals == m_MaxPortals );

    //
    // First lets find how many zones to portals
    //
    m_nZone2Portal = 0;

    for( i = 0; i < m_nZones; i++ )
    {
        zone& Zone = m_pZone[i];

        for( j = 0; j < m_nPortals; j++ )
        {
            portal& Portal = m_pPortal[j];

            if( Portal.iZone[0] == i || Portal.iZone[1] == i )
            {
                m_nZone2Portal++;
                Zone.nPortals++;
            }
        }
    }

    //
    // Okay we are really to create the zone 2 portal mapping
    //

    // Allocate the zone to portal buffer
    m_pZone2Portal = new s32[m_nZone2Portal];

    if( m_pZone2Portal == NULL )
        x_throw( "out of memory" );

    // Set the zones indices
    s32 iCursor = 0;

    for( i = 0; i < m_nZones; i++ )
    {
        zone& Zone = m_pZone[i];

        Zone.iPortal2Portal = iCursor;

        for( j = 0; j < m_nPortals; j++ )
        {
            portal& Portal = m_pPortal[j];

            if( Portal.iZone[0] == i || Portal.iZone[1] == i )
            {
                m_pZone2Portal[iCursor++] = j;
                ASSERT( iCursor <= m_nZone2Portal );
            }
        }

        ASSERT( Zone.nPortals == ( iCursor - Zone.iPortal2Portal ) );
    }

    ASSERT( iCursor == m_nZone2Portal );

    //
    // TODO: Compute the zone visibility
    // Given two portals try to see if you can see portal #3 This will tell whether
    // zone a can see zone b.
    //
}

//=========================================================================

xbool zone_mgr::ComputeFrustum(
    const zone&    Zone,
    frustum&       NewFrustum,
    const frustum& CurrentFrustum,
    const vector3& EyePosition,
    const portal&  Portal ) const
{
    s32 i;
    s32 Count[2];

    //
    // Compute the zone far plane from the furthest corner of its bounds.
    //
    const f32* pF      = (f32*)&Zone.BBox;
    f32 const  MaxDist = m_Far.Normal.GetX() * pF[m_FarMinIndex[0]] +
                        m_Far.Normal.GetY() * pF[m_FarMinIndex[1]] +
                        m_Far.Normal.GetZ() * pF[m_FarMinIndex[2]] + m_Far.D;

    plane ZoneFar = m_Far;
    ZoneFar.D -= MaxDist;

    // Check both the portal plane and the aperture before taking the coplanar path.
    xbool EyeInPortal = x_abs( Portal.Plane.Distance( EyePosition ) ) <= s_PortalPlaneEpsilon;

    for( i = 0; EyeInPortal && ( i < 4 ); i++ )
    {
        vector3 const Normal = Portal.Plane.Normal.Cross(
            Portal.Edges[( i + 1 ) % 4] - Portal.Edges[i] );
        EyeInPortal = Normal.Dot( EyePosition - Portal.Edges[i] ) >=
                      -s_PortalPlaneEpsilon * Normal.Length();
    }

    // At the aperture, retain the incoming view without angular clipping.
    if( EyeInPortal )
    {
        NewFrustum.nEdges = 4;
        NewFrustum.Edges.SetCount( 4 );

        for( i = 0; i < 4; i++ )
            NewFrustum.Edges[i] = Portal.Edges[i];

        NewFrustum.nPlanes = MAX( 1, CurrentFrustum.nPlanes );
        NewFrustum.Plane   = CurrentFrustum.Plane;
        NewFrustum.Plane.SetCount( NewFrustum.nPlanes );
        NewFrustum.Plane[NewFrustum.nPlanes - 1] = ZoneFar;

        return TRUE;
    }

    //
    // Order the portal edges for the side containing the eye. The winding must
    // stay consistent when building the planes through the clipped aperture.
    //
    xarray<vector3> Edges[2];
    Count[1] = 4;
    Edges[1].SetCount( 4 );

    for( i = 0; i < 4; i++ )
    {
        Edges[1][i] = Portal.Edges[Portal.Plane.InBack( EyePosition ) ? i : 3 - i];
    }

    //
    // Start clipping away
    //
    for( i = 0; i < CurrentFrustum.nPlanes; i++ )
    {
        const s32 iNew = i & 1;
        const s32 iOld = 1 - ( i & 1 );

        // Clipping a convex polygon adds at most one vertex.
        Edges[iNew].SetCount( Count[iOld] + 1 );
        CurrentFrustum.Plane[i].ClipNGon( Edges[iNew].GetPtr(), Count[iNew],
                                          Edges[iOld].GetPtr(), Count[iOld] );

        // Did we clip all away?
        if( Count[iNew] < 3 )
            return FALSE;
    }

    //
    // Copy the final edges
    //
    const s32 iNew = 1 - ( i & 1 );

    NewFrustum.nEdges = Count[iNew];
    NewFrustum.Edges.SetCount( NewFrustum.nEdges );
    NewFrustum.Plane.SetCount( NewFrustum.nEdges + 2 );

    for( i = 0; i < NewFrustum.nEdges; i++ )
    {
        NewFrustum.Edges[i] = Edges[iNew][i];
    }

    //
    // Compute the new planes
    //

    // Compute all the general planes
    NewFrustum.nPlanes = 0;

    for( i = 0; i < NewFrustum.nEdges; i++ )
    {
        const vector3& V1    = EyePosition;
        const vector3& V2    = Edges[iNew][i];
        const vector3& V3    = Edges[iNew][( i + 1 ) % NewFrustum.nEdges];
        plane&         Plane = NewFrustum.Plane[NewFrustum.nPlanes];

        Plane.Normal = v3_Cross( V2 - V1, V3 - V1 );

        if( Plane.Normal.LengthSquared() < 0.00001f )
            continue;

        Plane.Normal.Normalize();
        Plane.D = -Plane.Normal.Dot( V1 );

        // Okay ready for the next
        NewFrustum.nPlanes++;
    }

    if( NewFrustum.nPlanes < 3 )
        return FALSE;

    //
    // Use the portal itself as the near plane, facing into the destination zone.
    //
    plane& Near = NewFrustum.Plane[NewFrustum.nPlanes++];
    Near        = Portal.Plane;

    if( !Portal.Plane.InBack( EyePosition ) )
    {
        Near.Normal = -Near.Normal;
        Near.D      = -Near.D;
    }

    // Complete the frustum with the zone far plane computed above.
    NewFrustum.Plane[NewFrustum.nPlanes++] = ZoneFar;
    NewFrustum.Plane.SetCount( NewFrustum.nPlanes );

    return TRUE;
}

//=========================================================================

void zone_mgr::Search( f32* pVolumes, f32 Volume, s32 ZoneID, s32 Depth )
{
    zone& Zone = m_pZone[ZoneID];

    // Tell the system that we already tried this one.
    Zone.bStack = TRUE;

    // Set the volume.
    pVolumes[ZoneID] = MAX( pVolumes[ZoneID], Volume );

    // Only if we are not too deep...
    if( ( Depth < 4 ) && ( Volume > 0.01f ) )
    {
        // Now lets go thru all its portals and see what we get
        for( s32 i = 0; i < Zone.nPortals; i++ )
        {
            const s32 Index  = m_pZone2Portal[Zone.iPortal2Portal + i];
            portal&   Portal = m_pPortal[Index];

            // Find which is going to be our new zone.
            s32 iNewZone = ( Portal.iZone[0] == ZoneID ) ? Portal.iZone[1] : Portal.iZone[0];

            // Is the next zone already in the stack?
            if( m_pZone[iNewZone].bStack )
                continue;

            // If its really quiet, dont continue.
            if( Portal.Occlusion < 0.01f )
                continue;

            // Recurse.
            Search( pVolumes, Volume * Portal.Occlusion, iNewZone, Depth + 1 );
        }
    }

    // Woot, done!
    Zone.bStack = FALSE;
}

//=========================================================================

void zone_mgr::UpdateEar( s32 EarID )
{
    f32     ZoneVolumes[MAX_ZONES];
    matrix4 W2V;
    vector3 Position;
    s32     ZoneID;
    f32     Volume;

    g_AudioMgr.GetEar( EarID, W2V, Position, ZoneID, Volume );

    if( ( ZoneID >= 0 ) && ( ZoneID < ZONELESS ) )
    {
        // Clear them all.
        for( s32 i = 0; i < MAX_ZONES; i++ )
        {
            ZoneVolumes[i] = 0.0f;
        }

        ZoneVolumes[ZONELESS] = 1.0f;
        ZoneVolumes[ZoneID]   = 1.0f;

        Search( ZoneVolumes, 1.0f, ZoneID, 0 );
        g_AudioMgr.UpdateEarZoneVolumes( EarID, ZoneVolumes );
    }
}

//=========================================================================

void zone_mgr::PortalWalk( s32 iParentFrustum )
{
    static xprofile_counter portalCandidates =
        x_GetProfiler().RegisterCounter( "ZonePortalCandidates", "RenderCounter" );
    static xprofile_counter portalAccepted =
        x_GetProfiler().RegisterCounter( "ZonePortalsAccepted", "RenderCounter" );

    zone& Zone = m_pZone[m_Frustum[iParentFrustum].iZone];

    // Tell the system that we already when throw this zone
    Zone.bStack = TRUE;

    // Now lets go throw all its portals and see what we get
    for( s32 i = 0; i < Zone.nPortals; i++ )
    {
        portalCandidates.Add();

        const s32 Index  = m_pZone2Portal[Zone.iPortal2Portal + i];
        portal&   Portal = m_pPortal[Index];

        // Check whether this portal is disable
        if( ( Portal.Flags & PFLAGS_DISABLE ) == PFLAGS_DISABLE )
            continue;

        //
        // +-------------------------+ If we are trying to solve a portal
        // |           Zone1         | of a zone which we have already
        // |       +------------+    | visited we can skip it. In this example
        // |       | +--------+ |    | the view starts at zone1 then goes throw
        // |       | | Zone2  | |    | Portal1 (P1) amd that takes to an unvisited
        // | View  +-+        +-+    | Zone2 but this zone has a Portal going back
        // |  *<    | P1       | P2  | to Zone1 so this if statement tells the
        // |       +-+        +-+    | system not to go down this portal.
        // |       | |        | |    |
        // +-------+ |        | +----+
        //           +--------+
        //
        if( m_pZone[Portal.iZone[0]].bStack && m_pZone[Portal.iZone[1]].bStack )
            continue;

        //
        // TODO: If zone1 has 2 portals and you can see portal2 throw portal1
        //       portal1 should became an antiportal znd acluded portal2.
        //       this requieres may be a bit too much work.
        //

        // Find which is going to be our new zone
        s32 iNewZone = ( Portal.iZone[0] == m_Frustum[iParentFrustum].iZone ) ? Portal.iZone[1] : Portal.iZone[0];

        // Append before taking references; the array may grow.
        s32 const iNewFrustum = m_Frustum.GetCount();
        frustum&  NewFrustum  = m_Frustum.Append();

        if( ComputeFrustum( m_pZone[iNewZone], NewFrustum,
                            m_Frustum[iParentFrustum], m_EyePosition, Portal ) )
        {
            portalAccepted.Add();

            // OKay here is the new frustum zone and portal
            NewFrustum.iNext   = -1;
            NewFrustum.iPortal = Index;
            NewFrustum.iZone   = iNewZone;

            // Compute the bbox masks for this Frustum
            NewFrustum.NormalMasks.SetCount( NewFrustum.nPlanes * 2 );

            for( s32 j = 0; j < NewFrustum.nPlanes; j++ )
            {
                GetBBoxMaxNormalMasks( NewFrustum.Plane[j], NewFrustum.NormalMasks[j * 2 + 0], NewFrustum.NormalMasks[j * 2 + 1] );
            }

            PortalWalk( iNewFrustum );
        }
        else
        {
            m_Frustum.SetCount( iNewFrustum );
        }
    }

    // OKay we pop this zone out
    Zone.bStack = FALSE;
}

//=========================================================================
// Note that under certain conditions one zone could have multiple frustrums
// Right now we are choose not to mergethe frustrums.
//=========================================================================

void zone_mgr::PortalWalk( const view& View, const tracker& Tracker )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "zone_mgr::PortalWalk" );

    static xprofile_counter portalWalks =
        x_GetProfiler().RegisterCounter( "ZonePortalWalks", "RenderCounter" );
    static xprofile_counter portalFrustums =
        x_GetProfiler().RegisterCounter( "ZonePortalFrustums", "RenderCounter" );

    portalWalks.Add();

    s32 const iZone = Tracker.GetMainZone();
    s32       i;

    // Match the original renderer: without a valid starting zone there is no
    // portal walk, so render the view with portal culling disabled.
    if( ( iZone <= 0 ) || ( iZone >= m_nZones ) )
    {
        TurnOff();
        return;
    }

    m_bPortalCullingEnabled = TRUE;

    ASSERT( iZone < m_nZones );

    //
    // Clear the zone to frustrum look up
    //
    for( i = 0; i < 256; i++ )
    {
        m_ZoneToFrustum[i] = -1;
    }

    // Save the eye position
    m_EyePosition = View.GetPosition();

    //
    // Build the main frustum
    //
    m_Frustum.SetCount( 1 );
    frustum& RootFrustum = m_Frustum[0];
    RootFrustum.nPlanes  = 5;
    RootFrustum.nEdges   = 0;
    RootFrustum.Edges.SetCount( 0 );
    RootFrustum.iNext = -1;
    RootFrustum.Plane.SetCount( 5 );
    RootFrustum.NormalMasks.SetCount( 10 );
    RootFrustum.iZone   = iZone;
    RootFrustum.iPortal = -1;
    plane Near;
    View.GetViewPlanes( RootFrustum.Plane[0],   // Top
                        RootFrustum.Plane[1],   // Bottom
                        RootFrustum.Plane[2],   // Left
                        RootFrustum.Plane[3],   // Right
                        Near,                   // Near
                        RootFrustum.Plane[4] ); // Far

    // Compute the bbox indices for this Frustum
    for( i = 0; i < RootFrustum.nPlanes; i++ )
    {
        GetBBoxMaxNormalMasks( RootFrustum.Plane[i], RootFrustum.NormalMasks[i * 2 + 0], RootFrustum.NormalMasks[i * 2 + 1] );
    }

    // Compute the max indices for the far plane
    s32 NearMinIndex[3];
    m_Far = RootFrustum.Plane[4];
    m_Far.GetBBoxIndices( m_FarMinIndex, NearMinIndex );

    frustum const ViewFrustum = RootFrustum;

    // Compute the zone far plane base on the zone max bbox
    const f32* pF      = (f32*)&m_pZone[iZone].BBox;
    f32        MaxDist = m_Far.Normal.GetX() * pF[m_FarMinIndex[0]] +
                  m_Far.Normal.GetY() * pF[m_FarMinIndex[1]] +
                  m_Far.Normal.GetZ() * pF[m_FarMinIndex[2]] +
                  m_Far.D;

    RootFrustum.Plane[4].D -= MaxDist;

    //
    // Okay lets start the walk
    //
    PortalWalk( 0 );

    for( s32 Zone = 1; Zone < m_nZones; Zone++ )
    {
        if( ( Zone == iZone ) || !Tracker.IsZoneCandidate( (zone_id)Zone ) )
            continue;

        s32 const Index     = m_Frustum.GetCount();
        frustum&  Candidate = m_Frustum.Append();
        Candidate           = ViewFrustum;
        Candidate.iZone     = Zone;
        const f32* pBounds  = (const f32*)&m_pZone[Zone].BBox;
        f32 const  Distance = m_Far.Normal.GetX() * pBounds[m_FarMinIndex[0]] +
                             m_Far.Normal.GetY() * pBounds[m_FarMinIndex[1]] +
                             m_Far.Normal.GetZ() * pBounds[m_FarMinIndex[2]] + m_Far.D;
        Candidate.Plane[4].D -= Distance;
        PortalWalk( Index );
    }

    //
    // Insert all the frustum into the lookup table
    //
    for( i = 0; i < m_Frustum.GetCount(); i++ )
    {
        m_Frustum[i].iNext                  = m_ZoneToFrustum[m_Frustum[i].iZone];
        m_ZoneToFrustum[m_Frustum[i].iZone] = i;
    }

    portalFrustums.Add( m_Frustum.GetCount() );

    m_bHasValidVisibilityState = TRUE;

    //
    // TODO: here we may choose to merge frustrums that exits in the same zone.
    //       Some case this will work better some cases it will not.
    //       I am not sure what is the way to go here.
    //
}

//=========================================================================

s32 zone_mgr::GetLastPortalWalkZone( void ) const
{
    if( !m_bHasValidVisibilityState )
    {
        return 0;
    }

    // Last zone walked is stored here.
    return m_Frustum[0].iZone;
}

//=========================================================================

#if !defined( CONFIG_RETAIL )

void zone_mgr::Render( void ) const
{
    s32 i;

    if( eng_Begin( "ZoneMgrDebugRender" ) )
    {

        //
        // Render the portals
        //
        if( 1 )
        {
            for( i = 0; i < m_nPortals; i++ )
            {
                portal& Portal = m_pPortal[i];

                render::debug::Line( Portal.Edges[0], Portal.Edges[1] );
                render::debug::Line( Portal.Edges[1], Portal.Edges[2] );
                render::debug::Line( Portal.Edges[2], Portal.Edges[3] );
                render::debug::Line( Portal.Edges[3], Portal.Edges[0] );
            }
        }

        //
        // Render the frustrums
        //
        if( 1 )
        {
            x_srand( 12312 );
            const render::primitive_draw_desc Material( NULL,
                                                        render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                        render::PRIMITIVE_BLEND_ALPHA,
                                                        render::PRIMITIVE_DEPTH_READ_ONLY,
                                                        render::PRIMITIVE_RASTER_SOLID_NO_CULL,
                                                        render::PRIMITIVE_SAMPLER_LINEAR_CLAMP );
            render::PrimitiveBatch            Batch( Material );

            for( i = 0; i < m_Frustum.GetCount(); i++ )
            {
                const frustum& Frustum = m_Frustum[i];

                xcolor C( x_rand() );
                C.A = 64;

                for( s32 j = 0; j < Frustum.nPlanes; j++ )
                {
                    s32             t;
                    s32             Count[2];
                    xarray<vector3> Points[2];
                    Points[1].SetCount( 4 );
                    vector3      V1, V2, V3;
                    const plane& Plane = Frustum.Plane[j];

                    // Compute the ortho vectors
                    Plane.GetOrthoVectors( V1, V2 );

                    // Build a big ass face
                    V3 = Plane.Normal * -Plane.D;
                    V1.Normalize();
                    V2.Normalize();
                    Points[1][0] = V3 + V1 * -100000.0f + V2 * +100000.0f;
                    Points[1][1] = V3 + V1 * -100000.0f + V2 * -100000.0f;
                    Points[1][2] = V3 + V1 * +100000.0f + V2 * -100000.0f;
                    Points[1][3] = V3 + V1 * +100000.0f + V2 * +100000.0f;

                    // Set the initial count
                    Count[1] = 4;

                    // Now lets clip it base on the other planes
                    s32 o = 0;

                    for( t = 0; t < Frustum.nPlanes; t++ )
                    {
                        // Skip this plane
                        if( t == j )
                            continue;

                        // Clip with plane
                        s32 const iNew = o & 1;
                        s32 const iOld = 1 - iNew;

                        if( Count[iOld] < 3 )
                            break;
                        Points[iNew].SetCount( Count[iOld] + 1 );
                        Frustum.Plane[t].ClipNGon( Points[iNew].GetPtr(), Count[iNew],
                                                   Points[iOld].GetPtr(), Count[iOld] );
                        o++;
                    }

                    // Set t to contain the final index
                    t = 1 - ( o & 1 );

                    // Now we are ready to render it
                    for( s32 k = 0; k < ( Count[t] - 2 ); k++ )
                    {
                        Batch.AddTriangle( render::primitive_vertex( Points[t][0], vector2( 0.0f, 0.0f ), C ),
                                           render::primitive_vertex( Points[t][k + 1], vector2( 0.0f, 0.0f ), C ),
                                           render::primitive_vertex( Points[t][k + 2], vector2( 0.0f, 0.0f ), C ) );
                    }
                }
            }

            matrix4 Identity;
            Identity.Identity();
            Batch.Submit( Identity );
        }

        //
        // Render edges of frustrums
        //
        if( 1 )
        {
            x_srand( 12312 );

            for( i = 1; i < m_Frustum.GetCount(); i++ )
            {
                const frustum& Frustum = m_Frustum[i];

                const xcolor Color( x_rand(), x_rand(), x_rand(), 255 );

                for( s32 k = 0; k < Frustum.nEdges; k++ )
                {
                    render::debug::Line( Frustum.Edges[k], Frustum.Edges[( k + 1 ) % Frustum.nEdges], Color );
                }
            }
        }

        //
        // Render bbox for the zones
        //
        if( 1 )
        {
            for( i = 0; i < m_nZones; i++ )
            {
                render::debug::Box( m_pZone[i].BBox );
            }
        }

        //
        // Render bbox for the portals
        //
        if( 1 )
        {
            for( i = 0; i < m_nPortals; i++ )
            {
                render::debug::Box( m_pPortal[i].BBox );
            }
        }

        eng_End();
    }
}

#endif // !defined( CONFIG_RETAIL )

//=============================================================================

#if !defined( CONFIG_RETAIL )
zone_mgr::zone_id zone_mgr::FindZoneByBoundsDebug( const vector3& Position ) const
{
    for( s32 i = 1; i < m_nZones; i++ )
    {
        if( m_pZone[i].BBox.Intersect( Position ) )
            return (zone_id)i;
    }

    return (zone_id)0;
}
#endif

//=========================================================================

xbool zone_mgr::BBoxInView( const frustum& Frustum, const bbox& BBox ) const
{
    // Collect all the data
    const plane*   pPlane = Frustum.Plane.GetPtr();
    const vector3* pMask  = Frustum.NormalMasks.GetPtr();

    // Loop through planes looking for a trivial reject.
    for( s32 i = 0; i < Frustum.nPlanes; i++ )
    {
        // Compute max dist along normal
        f32 MaxDist = pMask[0].Dot( BBox.Min ) +
                      pMask[1].Dot( BBox.Max ) +
                      pPlane->D;

        // If outside plane, we are culled.
        if( MaxDist < 0 )
        {
            return FALSE;
        }

        // Move to next plane
        pMask += 2;
        pPlane++;
    }

    return TRUE;
}

//=========================================================================

void zone_mgr::GetBBoxMaxNormalMasks( const plane& Plane, vector3& Mask0, vector3& Mask1 ) const
{
    // normally, to check if a bbox is completely behind a plane, you will find
    // the bbox's max point along the plane normal, and it's usually done like this:
    //
    // BBoxMax.X = Plane.Normal.X>=0 ? BBox.Max.X : BBox.Min.X
    // BBoxMax.Y = Plane.Normal.Y>=0 ? BBox.Max.Y : BBox.Min.Y
    // BBoxMax.Z = Plane.Normal.Z>=0 ? BBox.Max.Z : BBox.Min.Z
    // MaxDist = Plane.Normal.Dot( BBoxMax ) + Plane.D
    // if( MaxDist < 0 )
    //     BBox is behind plane
    //
    // We will try to go with an optimized route that gets rid of the comparisons
    // at run-time, and replaces them with masks, so it looks like this:
    // MaxDist = Plane.Normal DOT (BBox.Min*Mask0) + Plane.Normal DOT (BBox.Max*Mask1) + D
    //         = (Plane.Normal*Mask0) DOT BBox.Min + (Plane.Normal*Mask1) DOT BBox.Max + D
    //
    // Hopefully this will speed up the calculation time since we'll be using the vector unit
    // to be dealing with the dot products.
    //
    // So we just want to pre-calculate (Plane.Normal*Mask0) and (Plane.Normal*Mask1)
    Mask0.Zero();
    Mask1.Zero();

    if( Plane.Normal.GetX() >= 0.0f )
        Mask1.GetX() = Plane.Normal.GetX();
    else
        Mask0.GetX() = Plane.Normal.GetX();
    if( Plane.Normal.GetY() >= 0.0f )
        Mask1.GetY() = Plane.Normal.GetY();
    else
        Mask0.GetY() = Plane.Normal.GetY();
    if( Plane.Normal.GetZ() >= 0.0f )
        Mask1.GetZ() = Plane.Normal.GetZ();
    else
        Mask0.GetZ() = Plane.Normal.GetZ();
}

//=========================================================================

xbool zone_mgr::IsBBoxVisible( const bbox& BBox, zone_id Zone1, zone_id Zone2 ) const
{
    static xprofile_counter bboxVisibilityTests =
        x_GetProfiler().RegisterCounter( "ZoneBBoxVisibilityTests", "RenderCounter" );
    static xprofile_counter bboxVisibilityAccepted =
        x_GetProfiler().RegisterCounter( "ZoneBBoxVisibilityAccepted", "RenderCounter" );
    static xprofile_counter bboxFrustumChecks =
        x_GetProfiler().RegisterCounter( "ZoneBBoxFrustumChecks", "RenderCounter" );

    bboxVisibilityTests.Add();

    if( !m_bPortalCullingEnabled )
    {
        bboxVisibilityAccepted.Add();
        return TRUE;
    }

    if( !m_bHasValidVisibilityState )
    {
        xbool const Visible = ( Zone1 == 0 ) && ( Zone2 == 0 );

        if( Visible )
            bboxVisibilityAccepted.Add();
        return Visible;
    }

    // Must be in the global zone
    if( Zone1 == 0 && Zone2 == 0 )
    {
        bboxVisibilityAccepted.Add();
        return TRUE;
    }

    // Must be insize a portal
    if( Zone1 && Zone2 )
    {
        // if neither zone is visible then you can't really see it
        if( m_ZoneToFrustum[Zone1] == -1 && m_ZoneToFrustum[Zone2] == -1 )
            return FALSE;

        // other wise you should always be able to see it
        bboxVisibilityAccepted.Add();
        return TRUE;
    }

    // Check the bbox with the first zone
    if( Zone1 )
    {
        for( s32 I = m_ZoneToFrustum[Zone1]; I != -1; I = m_Frustum[I].iNext )
        {
            bboxFrustumChecks.Add();

            if( BBoxInView( m_Frustum[I], BBox ) )
            {
                bboxVisibilityAccepted.Add();
                return TRUE;
            }
        }
    }

    // Check bbox witht he second bbox
    if( Zone2 )
    {
        for( s32 I = m_ZoneToFrustum[Zone2]; I != -1; I = m_Frustum[I].iNext )
        {
            bboxFrustumChecks.Add();

            if( BBoxInView( m_Frustum[I], BBox ) )
            {
                bboxVisibilityAccepted.Add();
                return TRUE;
            }
        }
    }

    return FALSE;
}

//=========================================================================

xbool zone_mgr::IsValidZone( zone_id Zone ) const
{
    return ( Zone > 0 ) && ( Zone < m_nZones );
}

//=========================================================================

xbool zone_mgr::LineCrossPortal( const portal&  Portal,
                                 const vector3& P0,
                                 const vector3& P1,
                                 const vector3& LastDirection,
                                 f32&           T ) const
{
    f32       Distance0 = Portal.Plane.Distance( P0 );
    f32 const Distance1 = Portal.Plane.Distance( P1 );

    // Recover the approach side only when the sample is on the plane.
    if( Distance0 == 0.0f )
    {
        Distance0 = -Portal.Plane.Normal.Dot( LastDirection );

        if( Distance0 == 0.0f )
            return FALSE;
    }

    if( Distance1 == 0.0f )
        return FALSE;

    if( ( ( Distance0 > 0.0f ) && ( Distance1 > 0.0f ) ) ||
        ( ( Distance0 < 0.0f ) && ( Distance1 < 0.0f ) ) )
    {
        return FALSE;
    }

    if( !Portal.Plane.Intersect( T, P0, P1 ) ||
        ( T < -s_PortalIntersectionEpsilon ) ||
        ( T > 1.0f + s_PortalIntersectionEpsilon ) )
    {
        return FALSE;
    }

    T                   = x_clamp( T, 0.0f, 1.0f );
    vector3 const Point = P0 + ( T * ( P1 - P0 ) );

    for( s32 i = 0; i < 4; i++ )
    {
        vector3 const Normal = Portal.Plane.Normal.Cross(
            Portal.Edges[( i + 1 ) % 4] - Portal.Edges[i] );

        if( Normal.Dot( Point - Portal.Edges[i] ) < -s_PortalPlaneEpsilon )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void zone_mgr::UpdateTemporaryZone( tracker& Tracker ) const
{
    Tracker.iTempZone = 0;

    if( !IsValidZone( Tracker.iCurrentZone ) )
    {
        return;
    }

    bbox WorldBBox;
    WorldBBox.Clear();
    WorldBBox += Tracker.LastPosition + Tracker.BBox.Min;
    WorldBBox += Tracker.LastPosition + Tracker.BBox.Max;

    zone const& Zone            = m_pZone[Tracker.iCurrentZone];
    f32         ClosestDistance = F32_MAX;
    s32         ClosestIndex    = -1;

    for( s32 i = 0; i < Zone.nPortals; i++ )
    {
        s32 const     Index  = m_pZone2Portal[Zone.iPortal2Portal + i];
        portal const& Portal = m_pPortal[Index];

        if( !Portal.BBox.Intersect( WorldBBox ) )
        {
            continue;
        }

        vector3   ClosestPoint;
        f32 const Distance = Tracker.LastPosition.ClosestPointToRectangle(
            Portal.Edges[0],
            Portal.Edges[1] - Portal.Edges[0],
            Portal.Edges[3] - Portal.Edges[0],
            ClosestPoint );

        if( ( Distance < ClosestDistance ) ||
            ( ( x_abs( Distance - ClosestDistance ) <= s_PortalIntersectionEpsilon ) &&
              ( ( ClosestIndex == -1 ) || ( Index < ClosestIndex ) ) ) )
        {
            ClosestDistance = Distance;
            ClosestIndex    = Index;
        }
    }

    if( ClosestIndex != -1 )
    {
        portal const& Portal = m_pPortal[ClosestIndex];
        Tracker.iTempZone    = ( Portal.iZone[0] == Tracker.iCurrentZone )
                                   ? Portal.iZone[1]
                                   : Portal.iZone[0];
    }
}

//=========================================================================

void zone_mgr::NormalizeZoneCandidates( tracker& Tracker ) const
{
    zone_id const MainZone = Tracker.GetMainZone();
    Tracker.ZoneCandidates[MainZone / 32] &= ~( 1U << ( MainZone % 32 ) );

    if( Tracker.HasZoneCandidates() )
        Tracker.ZoneCandidates[MainZone / 32] |= 1U << ( MainZone % 32 );
}

//=========================================================================

void zone_mgr::MoveTracker( tracker&       Tracker,
                            const vector3& NewPosition ) const
{
    if( !Tracker.HasZoneCandidates() )
    {
        MoveTrackerMain( Tracker, NewPosition );
        return;
    }

    tracker const Previous = Tracker;
    MoveTrackerMain( Tracker, NewPosition );
    x_memset( Tracker.ZoneCandidates, 0, sizeof( Tracker.ZoneCandidates ) );

    for( s32 Zone = 1; Zone < m_nZones; Zone++ )
    {
        if( !Previous.IsZoneCandidate( (zone_id)Zone ) )
            continue;

        tracker Candidate      = Previous;
        Candidate.iCurrentZone = (zone_id)Zone;
        MoveTrackerMain( Candidate, NewPosition );
        zone_id const Result = Candidate.GetMainZone();
        Tracker.ZoneCandidates[Result / 32] |= 1U << ( Result % 32 );
    }

    NormalizeZoneCandidates( Tracker );
}

//=========================================================================

void zone_mgr::MoveTrackerMain( tracker&       Tracker,
                                const vector3& NewPosition ) const
{
    vector3 const Segment              = NewPosition - Tracker.LastPosition;
    f32 const     SegmentLengthSquared = Segment.LengthSquared();

    if( SegmentLengthSquared <= F32_MIN )
    {
        UpdateTemporaryZone( Tracker );
        return;
    }

    vector3 const PreviousDirection = Tracker.LastDirection;
    vector3       Direction         = Segment;
    Direction.Normalize();

    if( IsValidZone( Tracker.iCurrentZone ) )
    {
        f32 SegmentT            = 0.0f;
        s32 PreviousPortalIndex = -1;
        s32 CrossingCount       = 0;

        while( TRUE )
        {
            zone const& Zone         = m_pZone[Tracker.iCurrentZone];
            f32         ClosestT     = F32_MAX;
            s32         ClosestIndex = -1;

            for( s32 i = 0; i < Zone.nPortals; i++ )
            {
                s32 const     Index         = m_pZone2Portal[Zone.iPortal2Portal + i];
                portal const& Portal        = m_pPortal[Index];
                f32           IntersectionT = 0.0f;

                if( !LineCrossPortal( Portal,
                                      Tracker.LastPosition,
                                      NewPosition,
                                      PreviousDirection,
                                      IntersectionT ) )
                {
                    continue;
                }

                if( IntersectionT + s_PortalIntersectionEpsilon < SegmentT )
                {
                    continue;
                }

                if( ( Index == PreviousPortalIndex ) &&
                    ( IntersectionT <= SegmentT + s_PortalIntersectionEpsilon ) )
                {
                    continue;
                }

                if( ( IntersectionT < ClosestT - s_PortalIntersectionEpsilon ) ||
                    ( ( x_abs( IntersectionT - ClosestT ) <= s_PortalIntersectionEpsilon ) &&
                      ( ( ClosestIndex == -1 ) || ( Index < ClosestIndex ) ) ) )
                {
                    ClosestT     = IntersectionT;
                    ClosestIndex = Index;
                }
            }

            if( ClosestIndex == -1 )
            {
                break;
            }

            if( CrossingCount >= m_nPortals )
            {
                break;
            }

            portal const& Portal       = m_pPortal[ClosestIndex];
            zone_id const PreviousZone = Tracker.iCurrentZone;
            zone_id const NewZone      = ( Portal.iZone[0] == PreviousZone )
                                             ? Portal.iZone[1]
                                             : Portal.iZone[0];
            Tracker.iCurrentZone       = NewZone;

            PreviousPortalIndex = ClosestIndex;
            SegmentT            = ClosestT + s_PortalIntersectionEpsilon;
            CrossingCount++;

            if( !IsValidZone( Tracker.iCurrentZone ) )
            {
                break;
            }
        }
    }

    if( IsValidZone( Tracker.iCurrentZone ) )
    {
        zone const& Zone = m_pZone[Tracker.iCurrentZone];

        for( s32 i = 0; i < Zone.nPortals; i++ )
        {
            portal const& Portal = m_pPortal[m_pZone2Portal[Zone.iPortal2Portal + i]];

            if( ( Portal.Plane.Distance( Tracker.LastPosition ) == 0.0f ) &&
                ( Portal.Plane.Distance( NewPosition ) == 0.0f ) )
            {
                Direction = PreviousDirection;
                break;
            }
        }
    }

    Tracker.LastPosition  = NewPosition;
    Tracker.LastDirection = Direction;
    UpdateTemporaryZone( Tracker );
}

//=========================================================================

void zone_mgr::AdvanceZoneTracking( object&        Object,
                                    tracker&       Tracker,
                                    const vector3& NewPosition ) const
{
    AdvanceZoneTracking( Tracker, NewPosition );
    Object.SetZone1( Tracker.GetMainZone() );
    Object.SetZone2( Tracker.GetZone2() );
}

//=========================================================================

void zone_mgr::AdvanceZoneTracking( tracker&       Tracker,
                                    const vector3& NewPosition ) const
{
    MoveTracker( Tracker, NewPosition );
}

//=========================================================================

void zone_mgr::RelocateZoneTracking( tracker&       Tracker,
                                     const vector3& NewPosition ) const
{
    tracker Continuous = Tracker;
    MoveTracker( Continuous, NewPosition );
    x_memset( Tracker.ZoneCandidates, 0, sizeof( Tracker.ZoneCandidates ) );

    if( IsValidZone( Tracker.iCurrentZone ) )
    {
        zone const& Zone            = m_pZone[Tracker.iCurrentZone];
        f32         ClosestDistance = F32_MAX;
        s32         ClosestIndex    = -1;

        for( s32 i = 0; i < Zone.nPortals; i++ )
        {
            s32 const     Index  = m_pZone2Portal[Zone.iPortal2Portal + i];
            portal const& Portal = m_pPortal[Index];
            f32           T;

            if( !LineCrossPortal( Portal, Tracker.LastPosition, NewPosition,
                                  Tracker.LastDirection, T ) )
                continue;

            zone_id const Destination = Portal.iZone[0] == Tracker.iCurrentZone
                                            ? Portal.iZone[1]
                                            : Portal.iZone[0];
            Tracker.ZoneCandidates[Destination / 32] |= 1U << ( Destination % 32 );

            vector3   ClosestPoint;
            f32 const Distance = NewPosition.ClosestPointToRectangle(
                Portal.Edges[0], Portal.Edges[1] - Portal.Edges[0],
                Portal.Edges[3] - Portal.Edges[0], ClosestPoint );
            if( ( Distance < ClosestDistance ) ||
                ( ( x_abs( Distance - ClosestDistance ) <= s_PortalIntersectionEpsilon ) &&
                  ( ( ClosestIndex == -1 ) || ( Index < ClosestIndex ) ) ) )
            {
                ClosestDistance = Distance;
                ClosestIndex    = Index;
            }
        }

        // A cut selects the destination boundary without visiting intermediate rooms.
        if( ClosestIndex != -1 )
        {
            portal const& Portal = m_pPortal[ClosestIndex];
            Tracker.iCurrentZone = ( Portal.iZone[0] == Tracker.iCurrentZone )
                                       ? Portal.iZone[1]
                                       : Portal.iZone[0];
        }
    }

    // Geometry bounds add candidates; they do not define room membership.
    for( s32 Zone = 1; Zone < m_nZones; Zone++ )
    {
        if( ( ( m_pZone[Zone].BBox.GetSize().LengthSquared() > F32_MIN ) &&
              m_pZone[Zone].BBox.Intersect( NewPosition ) ) ||
            Continuous.IsZoneCandidate( (zone_id)Zone ) ||
            ( (zone_id)Zone == Continuous.GetMainZone() ) ||
            ( (zone_id)Zone == Tracker.GetMainZone() ) )
        {
            Tracker.ZoneCandidates[Zone / 32] |= 1U << ( Zone % 32 );
        }
    }

    NormalizeZoneCandidates( Tracker );
    Tracker.SeedRevision++;
    Tracker.LastPosition = NewPosition;
    Tracker.LastDirection.Zero();
    UpdateTemporaryZone( Tracker );
}

//=========================================================================

void zone_mgr::RelocateZoneTracking( object&        Object,
                                     tracker&       Tracker,
                                     const vector3& NewPosition ) const
{
    RelocateZoneTracking( Tracker, NewPosition );
    Object.SetZone1( Tracker.GetMainZone() );
    Object.SetZone2( Tracker.GetZone2() );
}

//=========================================================================

void zone_mgr::RebaseZoneTracking( tracker&       Tracker,
                                   const vector3& Position,
                                   zone_id        Zone1,
                                   zone_id        Zone2 ) const
{
    // Keep authored zones until the zone database is loaded.
    xbool const   CanValidateZones = ( m_nZones > 0 );
    xbool const   Zone1Valid       = CanValidateZones ? IsValidZone( Zone1 )
                                                      : ( Zone1 != 0 );
    xbool const   Zone2Valid       = CanValidateZones ? IsValidZone( Zone2 )
                                                      : ( Zone2 != 0 );
    zone_id const SelectedZone     = Zone1Valid ? Zone1 : Zone2Valid ? Zone2
                                                                     : 0;

    Tracker.iCurrentZone = SelectedZone;
    Tracker.iTempZone    = 0;

    if( ( SelectedZone == Zone1 ) && Zone2Valid && ( Zone2 != SelectedZone ) )
    {
        Tracker.iTempZone = Zone2;
    }
    else if( ( SelectedZone == Zone2 ) && Zone1Valid && ( Zone1 != SelectedZone ) )
    {
        Tracker.iTempZone = Zone1;
    }

    x_memset( Tracker.ZoneCandidates, 0, sizeof( Tracker.ZoneCandidates ) );
    Tracker.SeedRevision++;
    Tracker.LastPosition = Position;
    Tracker.LastDirection.Zero();
}

//=========================================================================

void zone_mgr::RebaseZoneTracking( tracker&       Tracker,
                                   const tracker& SourceTracker ) const
{
    tracker const Snapshot = SourceTracker;
    RebaseZoneTracking( Tracker, Snapshot.GetPosition(), Snapshot.GetMainZone(),
                        Snapshot.GetZone2() );
    Tracker.LastDirection = Snapshot.LastDirection;
    x_memcpy( Tracker.ZoneCandidates, Snapshot.ZoneCandidates,
              sizeof( Tracker.ZoneCandidates ) );
}

//=========================================================================

void zone_mgr::RebaseZoneTracking( object&        Object,
                                   tracker&       Tracker,
                                   const vector3& Position,
                                   zone_id        Zone1,
                                   zone_id        Zone2 ) const
{
    RebaseZoneTracking( Tracker, Position, Zone1, Zone2 );
    Object.SetZone1( Tracker.GetMainZone() );
    Object.SetZone2( Tracker.GetZone2() );
}

//=========================================================================

void zone_mgr::TurnOff( void )
{
    m_Frustum.Clear();
    m_bPortalCullingEnabled    = FALSE;
    m_bHasValidVisibilityState = FALSE;
}

//=========================================================================
// The save is kind of hack right now
void zone_mgr::Save( const char* pFileName )
{
    X_FILE* FP;
    FP = x_fopen( pFileName, "wb" );

    if( FP == NULL )
        x_throw( xfs( "Unable to save the zone file[%s]", pFileName ) );

    s32 Version = ZONE_MANAGER_VERSION;
    x_fwrite( &Version, 1, sizeof( s32 ), FP );
    x_fwrite( &m_nPortals, 1, sizeof( m_nPortals ), FP );
    x_fwrite( &m_nZones, 1, sizeof( m_nZones ), FP );
    x_fwrite( &m_nZone2Portal, 1, sizeof( m_nZone2Portal ), FP );

    x_fwrite( m_pPortal, 1, m_nPortals * sizeof( portal ), FP );
    x_fwrite( m_pZone, 1, m_nZones * sizeof( zone ), FP );
    x_fwrite( m_pZone2Portal, 1, m_nZone2Portal * sizeof( s32 ), FP );

    x_fclose( FP );
}

//=========================================================================
// The load is kind of hack right now
void zone_mgr::Load( const char* pFileName )
{
    MEMORY_OWNER( "zone_mgr::Load" );
    X_FILE* FP = NULL;

    // Make sure we are ready to load
    Reset();

    // Start loading
    x_try;

    FP = x_fopen( pFileName, "rb" );

    if( FP == NULL )
        x_throw( xfs( "Unable to load the zone file[%s]", pFileName ) );

    s32 Version;
    x_fread( &Version, 1, sizeof( s32 ), FP );

    if( Version != ZONE_MANAGER_VERSION )
        x_throw( xfs( "Zone file[%s] is outdated. Please re-export the level", pFileName ) );
    x_fread( &m_nPortals, 1, sizeof( m_nPortals ), FP );
    x_fread( &m_nZones, 1, sizeof( m_nZones ), FP );
    x_fread( &m_nZone2Portal, 1, sizeof( m_nZone2Portal ), FP );

    m_pPortal      = new portal[m_nPortals];
    m_pZone        = new zone[m_nZones];
    m_pZone2Portal = new s32[m_nZone2Portal];

    if( m_pPortal == NULL || m_pZone == NULL || m_pZone2Portal == NULL )
        x_throw( xfs( "Out of memory while trying to load the zone file [%s]", pFileName ) );

    x_fread( m_pPortal, 1, m_nPortals * sizeof( portal ), FP );
    x_fread( m_pZone, 1, m_nZones * sizeof( zone ), FP );
    x_fread( m_pZone2Portal, 1, m_nZone2Portal * sizeof( s32 ), FP );

    x_fclose( FP );

    // make sure to add the guid lookups
    m_GuidLookup.SetCapacity( m_nPortals, FALSE );

    for( s32 i = 0; i < m_nPortals; i++ )
    {
        m_GuidLookup.Add( m_pPortal[i].Guid, i );
    }

    // Deal with problems
    x_catch_begin;

    if( FP )
        x_fclose( FP );

    Reset();

    x_catch_end_ret;

#ifndef X_EDITOR
    // Set up the zone limits in GameMgr.
    GameMgr.SetZoneLimits( 0, 0, 32 );

    for( s32 i = 1; i < m_nZones; i++ )
    {
        GameMgr.SetZoneLimits( i, m_pZone[i].MinPlayers, m_pZone[i].MaxPlayers );
    }
#endif
}

//=========================================================================

zone_mgr::portal& zone_mgr::GetPortal( guid Guid )
{
    s32 Index;

    ASSERT( m_pPortal );

    if( m_GuidLookup.Find( Guid, Index ) )
    {
        ASSERT( Index < m_nPortals );
        return m_pPortal[Index];
    }

    ASSERT( 0 );
    x_throw( "Unable to get portal" );
    return m_pPortal[0];
}

//=========================================================================

zone_mgr::portal& zone_mgr::GetPortal( s32 ZoneID, s32 PortalIndex )
{
    ASSERT( ZoneID >= 0 );
    ASSERT( ZoneID < 256 );
    ASSERT( PortalIndex < m_pZone[ZoneID].nPortals );
    ASSERT( PortalIndex >= 0 );

    const zone& Zone    = m_pZone[ZoneID];
    s32         iPortal = m_pZone2Portal[Zone.iPortal2Portal + PortalIndex];

    return m_pPortal[iPortal];
}

//=========================================================================

const zone_mgr::zone& zone_mgr::GetZone( s32 ZoneID )
{
    ASSERT( ZoneID >= 0 );
    ASSERT( ZoneID < 256 );

    return m_pZone[ZoneID];
}

//=========================================================================

void zone_mgr::TurnPortalOff( guid Guid )
{
    s32 Index;

    if( m_nPortals == 0 )
        return;

    ASSERT( m_pPortal );

    if( m_GuidLookup.Find( Guid, Index ) )
    {
        m_pPortal[Index].Flags |= PFLAGS_DISABLE;
    }
}

//=========================================================================

void zone_mgr::TurnPortalOn( guid Guid )
{
    s32 Index;

    if( m_nPortals == 0 )
        return;

    ASSERT( m_pPortal );

    if( m_GuidLookup.Find( Guid, Index ) )
    {
        m_pPortal[Index].Flags &= ~PFLAGS_DISABLE;
    }
}

//=========================================================================

xbool zone_mgr::IsPortalOn( guid Guid )
{
    s32 Index;

    if( m_nPortals == 0 )
        return FALSE;

    ASSERT( m_pPortal );

    if( m_GuidLookup.Find( Guid, Index ) )
    {
        if( !( m_pPortal[Index].Flags & PFLAGS_DISABLE ) )
            return TRUE;
    }

    return FALSE;
}

//=========================================================================

void zone_mgr::SetPortalOcclusion( guid Guid, f32 Occlusion )
{
    s32 Index;

    if( m_nPortals == 0 )
        return;

    ASSERT( m_pPortal );

    if( m_GuidLookup.Find( Guid, Index ) )
    {
        m_pPortal[Index].Occlusion = Occlusion;
    }
}

//=========================================================================

xbool zone_mgr::IsAdjacentZone( s32 Zone1, s32 Zone2 )
{
    if( m_nZones > Zone1 )
    {
        for( s32 i = 0; i < m_pZone[Zone1].nPortals; i++ )
        {
            //for each portal
            s32 index = m_pZone[Zone1].iPortal2Portal + i;

            if( ( m_nZone2Portal > index ) && ( m_nPortals > m_pZone2Portal[index] ) )
            {
                if( m_pPortal[m_pZone2Portal[index]].iZone[0] == Zone2 ||
                    m_pPortal[m_pZone2Portal[index]].iZone[1] == Zone2 )
                {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

//=========================================================================

void zone_mgr::SanityCheck( void )
{
}

//=============================================================================

zone_mgr::tracker::tracker( void )
{
    LastPosition.Zero();
    LastDirection.Zero();
    iTempZone = iCurrentZone = 0;
    SeedRevision             = 0;
    x_memset( ZoneCandidates, 0, sizeof( ZoneCandidates ) );
    BBox.Set( vector3( 0, 0, 0 ), 100 );
}

//=============================================================================
