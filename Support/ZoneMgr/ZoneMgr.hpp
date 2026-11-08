#ifndef ZONE_MGR_HPP
#define ZONE_MGR_HPP

//=========================================================================
// INCLUDES
//=========================================================================
#include "Entropy.hpp"
#include "Auxiliary/MiscUtils/Guid.hpp"
#include "../../MiscUtils/PriorityQueue.hpp"
#include "x_bitstream.hpp"

//=========================================================================
// DEFINES
//=========================================================================

#define MAX_ZONE_DISTANCE_TRAVELED  65535.0f
#define ZONE_MANAGER_VERSION        1003

//=========================================================================
// FORWARD DECLARATIONS
//=========================================================================
class object;

//=========================================================================
// TYPES
//=========================================================================

class zone_mgr
{
public:

    //---------------------------------------------------------------------
    enum portal_flags
    {
        PFLAGS_DISABLE = (1<<0),
    };

    //---------------------------------------------------------------------
    enum zone_flags
    {
        ZFLAGS_VISITED = (1<<0),
    };


    //---------------------------------------------------------------------
    using zone_id = u8;

    //---------------------------------------------------------------------

    struct zone
    {
        bbox            BBox;               // BBox containing the hold zone
        u32             Flags;
        s32             nPortals;           // How many portals does this zone has
        s32             iPortal2Portal;     // start index to the array of indices to portals
        xbool           bStack;             // Flags we have push this zone already in the stack
        f32             SndWeight;          // Sound multiplier per zone
        u16             MinPlayers;
        u16             MaxPlayers;
        char            EnvMapName[128];
        char            FogName[128];
        xbool           QuickFog;           // fog transition is immediate
    };
    
    //---------------------------------------------------------------------
    struct portal
    {
        bbox        BBox;               // BBox 
        vector3     Edges[4];           // A portal is always a square.
        plane       Plane;              // Plane of the actual portal.
        guid        Guid;
        zone_id     iZone[2];           // The two zones that this portal connects
        u32         Flags;              // Flags about the portal.
        f32         Occlusion;          // The amount of sound this protal is occluding.
        f32         BaseOcclusion;      // The fixed amount of sound this protal is occluding, not changed in runtime.
    };

    //---------------------------------------------------------------------
    struct tracker
    {
    public:
                        tracker                 ( void );

        zone_id         GetMainZone              ( void ) const         { return iCurrentZone; }
        zone_id         GetZone2                 ( void ) const         { return iTempZone; }
        xbool           IsZoneCandidate          ( zone_id Zone ) const
        {
            return ( ZoneCandidates[Zone / 32] & ( 1U << ( Zone % 32 ) ) ) != 0;
        }

        xbool           HasZoneCandidates        ( void ) const
        {
            for( s32 i = 0; i < 8; i++ )
                if( ZoneCandidates[i] )
                    return TRUE;

            return FALSE;
        }

        u32             GetSeedRevision          ( void ) const         { return SeedRevision; }
        const vector3&  GetPosition              ( void ) const         { return LastPosition; }
        void            SetBBox                  ( const bbox& aBBox )   { BBox = aBBox; }
        const bbox&     GetBBox                  ( void ) const         { return BBox; }

    protected:

        vector3         LastPosition;           // Last known position.
        vector3         LastDirection;          // Last non-zero movement direction.
        zone_id         iCurrentZone;           // Current zone that the tracker is in.
        zone_id         iTempZone;              // Temporary zone which the object may also be in.
        bbox            BBox;                   // Local space BBox.
        u32             SeedRevision;           // Explicit zone discontinuities.
        u32             ZoneCandidates[8];      // Possible zones after a cut.

        friend class zone_mgr;
    };

    //---------------------------------------------------------------------
    struct node
    {
        s32 iParentZone;                // Parent zone.
        s32 iParentPortal;              // Parent portal.
        s32 iZone;                      // Current zone.
        s32 iPortal;                    // Current portal.
        f32 SndWeight;                  // Sound weight accumalated so far.
        f32 Distance;                   // Distance accumalated this far.
        s32 ZonePortalIndex;            // Zone to portal index.
    };


public:

                zone_mgr                ( void );
               ~zone_mgr                ( void );

    void        TurnOff                 ( void );
    void        Reset                   ( void );
    void        AddStart                ( s32 nZones, s32 nPortals );
    void        AddZone                 ( const bbox& BBox, 
                                          s32         ZoneID, 
                                          f32         SndWeight,
                                          s32         MinPlayers, 
                                          s32         MaxPlayers,
                                          const char* EnvMap,
                                          const char* FogName,
                                          xbool       QuickFog );
    void        AddPortal               ( guid Guid, const bbox& BBox, vector3* pEdges, s32 ZoneA, s32 ZoneB, 
                                            f32 SoundOcclusion );
    void        AddEnd                  ( void );

    void        Save                    ( const char* pFileName );
    void        Load                    ( const char* pFileName );
    void        UpdateEar               ( s32 EarID );
    void        Search                  ( f32* pVolumes, f32 Volume, s32 ZoneID, s32 Depth );
    void        PortalWalk              ( const view& View, const tracker& Tracker );
    s32         GetLastPortalWalkZone   ( void ) const;

    void        Render                  ( void ) const;
    #if !defined( CONFIG_RETAIL )
    zone_id     FindZoneByBoundsDebug   ( const vector3& Position ) const;
    #endif
    void        GetBBoxMaxNormalMasks   ( const plane& Plane, vector3& Mask0, vector3& Mask1 ) const;
    xbool       IsBBoxVisible           ( const bbox& BBox, zone_id Zone1, zone_id Zone2 ) const; 
    xbool       IsZoneVisible           ( zone_id iZone ) const;
    xbool       IsValidZone             ( zone_id Zone ) const;
    
private:
    xbool       LineCrossPortal         ( const portal& Portal,
                                          const vector3& P0,
                                          const vector3& P1,
                                          const vector3& LastDirection,
                                          f32& T ) const;
    void        MoveTracker             ( tracker& Tracker,
                                          const vector3& NewPosition ) const;
    void        MoveTrackerMain         ( tracker& Tracker,
                                          const vector3& NewPosition ) const;
    void        NormalizeZoneCandidates ( tracker& Tracker ) const;
    void        UpdateTemporaryZone     ( tracker& Tracker ) const;

public:
    void        AdvanceZoneTracking     ( tracker& Tracker,
                                          const vector3& NewPosition ) const;
    void        AdvanceZoneTracking     ( object& Object,
                                          tracker& Tracker,
                                          const vector3& NewPosition ) const;
    void        RelocateZoneTracking    ( tracker& Tracker,
                                          const vector3& NewPosition ) const;
    void        RelocateZoneTracking    ( object& Object,
                                          tracker& Tracker,
                                          const vector3& NewPosition ) const;
    void        RebaseZoneTracking      ( tracker& Tracker,
                                          const vector3& Position,
                                          zone_id Zone1,
                                          zone_id Zone2 ) const;
    void        RebaseZoneTracking      ( tracker& Tracker,
                                          const tracker& SourceTracker ) const;
    void        RebaseZoneTracking      ( object& Object,
                                          tracker& Tracker,
                                          const vector3& Position,
                                          zone_id Zone1,
                                          zone_id Zone2 ) const;

    s32         GetPortalCount          ( void ) const;
    s32         GetZoneCount            ( void ) const;
    portal&     GetPortal               ( s32 PortalID );
    portal&     GetPortal               ( guid Guid );
    portal&     GetPortal               ( s32 ZoneID, s32 PortalIndex );
    const zone& GetZone                 ( s32 ZoneID );
    void        TurnPortalOff           ( guid Guid );
    void        TurnPortalOn            ( guid Guid );
    xbool       IsPortalOn              ( guid Guid );
    void        SetPortalOcclusion      ( guid Guid, f32 Occlusion );
    const char* GetZoneEnvMap           ( s32 ZoneID );
    const char* GetZoneFog              ( s32 ZoneID, xbool& QuickFog );
    
    xbool       IsAdjacentZone                  ( s32 Zone1, s32 Zone2 );

    void        SanityCheck                     ( void );

    s32         GetZoneCount                    ( void ){ return m_nZones; }
    s32         GetStartingZone                 ( void ) { return m_bHasValidVisibilityState ? m_Frustum[0].iZone : 0; }

protected:

    //---------------------------------------------------------------------
    struct frustum
    {
        xarray<plane>   Plane;          // Frustum clipping planes.
        xarray<vector3> NormalMasks;    // Two masks per plane for bbox visibility checks.
        xarray<vector3> Edges;          // Portal edges remaining after clipping.
        s32             nPlanes;        // Number of clipping planes.
        s32             nEdges;         // Number of clipped edges, also used for debug rendering.
        zone_id         iZone;          // Zone containing this frustum.
        s32             iNext;          // Next frustum in the same zone.
        s32             iPortal;        // Portal that produced this frustum.
    };

protected:

    xbool       ComputeFrustum          ( const zone&     Zone,
                                          frustum&        NewFrustum,
                                          const frustum&  CurrentFrustum,
                                          const vector3&  EyePosition,
                                          const portal&   Portal ) const;
    void        PortalWalk              ( s32 iParentFrustum );
    xbool       BBoxInView              ( const frustum& Frustum, const bbox& BBox ) const;
protected:

    // Data-Base variables
    s32             m_nPortals;
    portal*         m_pPortal;

    s32             m_nZones;
    zone*           m_pZone;

    s32             m_nZone2Portal;
    s32*            m_pZone2Portal;

    // Add mode variables
    xbool           m_bAddMode;
    s32             m_MaxPortals;

    // Portal Walk variables
    xarray<frustum>  m_Frustum;
    vector3         m_EyePosition;
    plane           m_Far;
    s32             m_FarMinIndex[3]; 

    // Quick Frustum look up
    s32             m_ZoneToFrustum[256];
    xbool           m_bPortalCullingEnabled;
    xbool           m_bHasValidVisibilityState;


    // Dome to quicly look up portals
    guid_lookup     m_GuidLookup;        

    //
    // This is an array of bits nZones by nZones big. It tells whether a 
    // zone is visible from another zone. This could be RLE compress if need be ala Quake
    //
    u8*             m_pVisivilityBits;
};

//=========================================================================

inline s32 zone_mgr::GetPortalCount( void ) const
{
    return m_nPortals;
}

//=========================================================================

inline zone_mgr::portal& zone_mgr::GetPortal( s32 PortalID )
{
    ASSERT( (PortalID>=0) && (PortalID < m_nPortals) );
    return m_pPortal[PortalID];
}

//=========================================================================

inline const char* zone_mgr::GetZoneEnvMap( s32 ZoneID )
{
    ASSERT( ZoneID      >= 0        );
    ASSERT( ZoneID      <  256      );
    
    if ( ZoneID < m_nZones )
        return m_pZone[ ZoneID ].EnvMapName;
    else
        return "";
}

//=========================================================================

inline const char* zone_mgr::GetZoneFog( s32 ZoneID, xbool& QuickFog )
{
    ASSERT( ZoneID >= 0   );
    ASSERT( ZoneID <  256 );

    if ( ZoneID < m_nZones )
    {
        QuickFog = m_pZone[ ZoneID ].QuickFog;
        return m_pZone[ ZoneID ].FogName;
    }
    else
    {
        QuickFog = FALSE;
        return "";
    }
}

//=========================================================================

inline xbool zone_mgr::IsZoneVisible( zone_id iZone ) const
{
    if( !m_bPortalCullingEnabled )
    {
        return TRUE;
    }

    if( !m_bHasValidVisibilityState )
    {
        return iZone == 0;
    }

    return m_ZoneToFrustum[iZone] != -1;
}

//=========================================================================

inline s32 zone_mgr::GetZoneCount( void ) const
{
    return m_nZones;
}

//=========================================================================

extern zone_mgr g_ZoneMgr;

//=========================================================================
// END
//=========================================================================
#endif
