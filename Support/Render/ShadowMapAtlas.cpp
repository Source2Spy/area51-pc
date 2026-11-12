//=========================================================================
//
//  ShadowMapAtlas.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ShadowMapAtlas.hpp"
#include "Pipeline/RenderPipeline.hpp"
#include "x_array.hpp"
#include "../Objects/object.hpp"
#include "../Objects/Player/Player.hpp"
#include "../Obj_mgr/obj_mgr.hpp"
#include "../PlaySurfaceMgr/PlaySurfaceMgr.hpp"
#include "../ZoneMgr/ZoneMgr.hpp"
#include "LightSystem.hpp"
#include "Render.hpp"
#include "Entropy.hpp"

namespace render
{

//=========================================================================
//  PIPELINE-OWNED SERVICE
//=========================================================================

//=========================================================================
//  TYPES
//=========================================================================

struct ShadowPointFace
{
    radian Pitch;
    radian Yaw;
};

//-------------------------------------------------------------------------

struct ShadowLight
{
    s32     DynamicLightIndex;
    s32     Shape;
    vector3 Pos;
    vector3 Direction;
    f32     Radius;
    f32     Falloff;
    radian  FOV;
    s32     ShadowMapResolution;
    s32     ShadowPriority;
    s32     SourceCount;
    s32     RelevantCasterStart;
    s32     RelevantCasterCount;
    f32     Score;
};

//-------------------------------------------------------------------------

struct ShadowConeTest
{
    vector3 Apex;
    vector3 Axis;
    f32     CosOuter;
    f32     SinOuter;
    f32     TanOuter;
    f32     Length;
};

//-------------------------------------------------------------------------

struct DynamicShadowLight
{
    vector3 Pos;
    vector3 Direction;
    xcolor  Color;
    f32     Radius;
    f32     Falloff;
    f32     OuterAngle;
    s32     Shape;
    s32     ShadowMapResolution;
    xbool   IsFlashlight;
};

//-------------------------------------------------------------------------

struct ShadowCasterRecord
{
    enum CasterType
    {
        CASTER_OBJECT,
        CASTER_PLAY_SURFACE,
    };

    CasterType                Type;
    object*                   pObject;
    playsurface_mgr::surface* pPlaySurface;
    vector3                   WorldBBoxCenter;
    f32                       WorldBBoxRadius;
};

//-------------------------------------------------------------------------

struct ShadowCasterHashEntry
{
    ShadowCasterRecord Caster;
    u64                SourceMask;
    xbool              Occupied;
};

//-------------------------------------------------------------------------

struct ShadowAtlasEntry
{
    s32 SourceIndex;
    s32 TileSize;
    s32 ShadowPriority;
    f32 ShadowScore;
    s32 AtlasTileX;
    s32 AtlasTileY;
};

//-------------------------------------------------------------------------

struct ShadowAtlasFreeNode
{
    s32 AtlasTileX;
    s32 AtlasTileY;
    s32 TileSize;
};

//-------------------------------------------------------------------------

struct ShadowMapAtlas::ScratchData
{
    xarray<ShadowCasterRecord>    RelevantCasters;
    xarray<ShadowCasterRecord>    ShadowCasters;
    xarray<u64>                   CasterMasks;
    xarray<ShadowCasterHashEntry> CasterHash;
    xarray<ShadowAtlasFreeNode>   AtlasFreeNodes;
};

//=========================================================================
//  CONSTANTS
//=========================================================================

static ShadowPointFace const PointShadowFaces[POINT_SHADOW_FACE_COUNT] =
{
    { -R_90,  R_0   },
    {  R_90,  R_0   },
    {  R_0,   R_0   },
    {  R_0,   R_180 },
    {  R_0,   R_90  },
    {  R_0,  -R_90  },
};

//-------------------------------------------------------------------------

static radian const PointShadowFOV                      = R_90;
static radian const MinSpotShadowFOV                    = DEG_TO_RAD( 1.0f );
static s32 const    MinShadowPriority                   = 0;
static s32 const    MaxShadowPriority                   = 4;
static f32 const    PlayerShadowScoreBonus              = 8192.0f;
static f32 const    ShadowRadiusScorePenalty            = 0.25f;
static f32 const    ShadowSourceTypeSpot                = 0.0f;
static f32 const    ShadowSourceTypePointFace           = 1.0f;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
s32 ClampShadowMapResolution( s32 ShadowMapResolution )
{
    if( ShadowMapResolution >= 4096 )
    {
        return 4096;
    }
    if( ShadowMapResolution >= 2048 )
    {
        return 2048;
    }
    if( ShadowMapResolution >= 1024 )
    {
        return 1024;
    }
    if( ShadowMapResolution >= 512 )
    {
        return 512;
    }
    return 256;
}

//=========================================================================

static
s32 ReduceShadowMapResolution( s32 ShadowMapResolution )
{
    if( ShadowMapResolution > 2048 )
    {
        return 2048;
    }
    if( ShadowMapResolution > 1024 )
    {
        return 1024;
    }
    if( ShadowMapResolution > 512 )
    {
        return 512;
    }
    if( ShadowMapResolution > 256 )
    {
        return 256;
    }
    return 256;
}

//=========================================================================

static
s32 ClampShadowPriority( s32 ShadowPriority )
{
    if( ShadowPriority < MinShadowPriority )
    {
        return MinShadowPriority;
    }
    if( ShadowPriority > MaxShadowPriority )
    {
        return MaxShadowPriority;
    }
    return ShadowPriority;
}

//=========================================================================

static
f32 ComputePointShadowNearZ( f32 LightRadius )
{
    return MAX( 0.5f, LightRadius * 0.0025f );
}

//=========================================================================

static
f32 ComputePointShadowReceiveNearZ( f32 LightRadius )
{
    return MAX( 0.05f, LightRadius * 0.00025f );
}

//=========================================================================

static
f32 ComputeSpotShadowReceiveNearZ( f32 LightRadius )
{
    return MAX( 0.05f, LightRadius * 0.0005f );
}

//=========================================================================

static
f32 ComputePointFaceCoverageCos( f32 ProjectionScale )
{
    f32 const TanHalfFOV = 1.0f / ProjectionScale;
    f32 const CornerLengthSq = 1.0f + ( 2.0f * TanHalfFOV * TanHalfFOV );
    return 1.0f / x_sqrt( CornerLengthSq );
}

//=========================================================================

static
radian GetShadowLightFOV( DynamicShadowLight const& DynamicLightData )
{
    if( DynamicLightData.Shape == LightSystem::LIGHT_SHAPE_OMNI )
    {
        return PointShadowFOV;
    }

    return MAX( MinSpotShadowFOV, DEG_TO_RAD( DynamicLightData.OuterAngle ) );
}

//=========================================================================

static
s32 GetShadowLightSourceCount( s32 LightShape )
{
    if( LightShape == LightSystem::LIGHT_SHAPE_OMNI )
    {
        return POINT_SHADOW_FACE_COUNT;
    }

    return 1;
}

//=========================================================================

static
s32 GetAtlasGuardTexels( f32 FilterRadius )
{
    s32 const GuardTexels = static_cast<s32>( x_ceil( FilterRadius ) ) + 1;
    return ( GuardTexels > 0 ) ? GuardTexels : 1;
}

//=========================================================================

static
f32 ComputeBBoxBoundingSphereRadius( bbox const& WorldBBox )
{
    vector3 const Extents = ( WorldBBox.Max - WorldBBox.Min ) * 0.5f;
    return x_sqrt( Extents.Dot( Extents ) );
}

//=========================================================================

static
void SetShadowCasterBounds( ShadowCasterRecord& Caster, bbox const& WorldBBox )
{
    Caster.WorldBBoxCenter = WorldBBox.GetCenter();
    Caster.WorldBBoxRadius = ComputeBBoxBoundingSphereRadius( WorldBBox );
}

//=========================================================================

static
ShadowCasterRecord MakeObjectShadowCaster( object* pCaster, bbox const& WorldBBox )
{
    ShadowCasterRecord Caster;
    Caster.Type = ShadowCasterRecord::CASTER_OBJECT;
    Caster.pObject = pCaster;
    Caster.pPlaySurface = nullptr;
    SetShadowCasterBounds( Caster, WorldBBox );
    return Caster;
}

//=========================================================================

static
ShadowCasterRecord MakePlaySurfaceShadowCaster( playsurface_mgr::surface* pSurface, bbox const& WorldBBox )
{
    ShadowCasterRecord Caster;
    Caster.Type = ShadowCasterRecord::CASTER_PLAY_SURFACE;
    Caster.pObject = nullptr;
    Caster.pPlaySurface = pSurface;
    SetShadowCasterBounds( Caster, WorldBBox );
    return Caster;
}

//=========================================================================

static
xbool SameShadowCaster( ShadowCasterRecord const& A, ShadowCasterRecord const& B )
{
    if( A.Type != B.Type )
    {
        return FALSE;
    }

    if( A.Type == ShadowCasterRecord::CASTER_OBJECT )
    {
        return ( A.pObject == B.pObject );
    }

    return ( A.pPlaySurface == B.pPlaySurface );
}

//=========================================================================

static
uaddr GetShadowCasterKey( ShadowCasterRecord const& Caster )
{
    if( Caster.Type == ShadowCasterRecord::CASTER_OBJECT )
    {
        return reinterpret_cast<uaddr>( Caster.pObject );
    }

    return reinterpret_cast<uaddr>( Caster.pPlaySurface );
}

//=========================================================================

static
u32 GetShadowCasterHash( ShadowCasterRecord const& Caster )
{
    uaddr const Key = GetShadowCasterKey( Caster );
    return static_cast<u32>( Key ^ ( Key >> 16 ) ^ static_cast<uaddr>( Caster.Type ) );
}

//=========================================================================

static
void SetupShadowConeTest( ShadowConeTest& Cone, vector3 const& Apex, vector3 const& Direction, f32 CosOuter, f32 Length )
{
    Cone.Apex = Apex;
    Cone.Axis = Direction;
    if( !Cone.Axis.SafeNormalize() )
    {
        Cone.Axis.Set( 0.0f, 0.0f, 1.0f );
    }

    Cone.CosOuter = MAX( CosOuter, 0.0001f );
    Cone.SinOuter = x_sqrt( MAX( 1.0f - ( Cone.CosOuter * Cone.CosOuter ), 0.0f ) );
    Cone.TanOuter = Cone.SinOuter / Cone.CosOuter;
    Cone.Length = Length;
}

//=========================================================================

static
xbool SphereIntersectsFiniteCone( vector3 const& SphereCenter, f32 SphereRadius, ShadowConeTest const& Cone )
{
    vector3 const ToCenter = SphereCenter - Cone.Apex;
    f32 const     CenterDistSq = ToCenter.Dot( ToCenter );
    f32 const     AxialDistance = Cone.Axis.Dot( ToCenter );
    f32 const     SphereRadiusSq = SphereRadius * SphereRadius;

    if( AxialDistance < -SphereRadius )
    {
        return FALSE;
    }

    if( AxialDistance > ( Cone.Length + SphereRadius ) )
    {
        return FALSE;
    }

    if( AxialDistance <= 0.0f )
    {
        return ( CenterDistSq <= SphereRadiusSq );
    }

    f32 const RadialDistanceSq = MAX( CenterDistSq - ( AxialDistance * AxialDistance ), 0.0f );
    f32 const RadialDistance = x_sqrt( RadialDistanceSq );

    if( AxialDistance < Cone.Length )
    {
        f32 const SideDistance = ( RadialDistance * Cone.CosOuter ) - ( AxialDistance * Cone.SinOuter );
        if( SideDistance <= SphereRadius )
        {
            return TRUE;
        }
    }

    f32 const ConeCapRadius = Cone.Length * Cone.TanOuter;
    f32 const CapAxialDelta = MAX( AxialDistance - Cone.Length, 0.0f );
    f32 const CapRadialDelta = MAX( RadialDistance - ConeCapRadius, 0.0f );
    return ( ( CapAxialDelta * CapAxialDelta ) + ( CapRadialDelta * CapRadialDelta ) ) <= SphereRadiusSq;
}

//=========================================================================

static
xbool SourceConeAffectsBBox( ShadowConeTest const& Cone, bbox const& WorldBBox )
{
    return SphereIntersectsFiniteCone( WorldBBox.GetCenter(), ComputeBBoxBoundingSphereRadius( WorldBBox ), Cone );
}

//=========================================================================

static
xbool ShadowCasterBoundsAreRelevant( bbox const& CasterBBox, u8 Zone1, u8 Zone2, vector3 const& LightPos,
                                     f32 LightRadius, ShadowConeTest const* pConeTest = nullptr )
{

    if( !g_ZoneMgr.IsZoneVisible( Zone1 ) && !g_ZoneMgr.IsZoneVisible( Zone2 ) )
    {
        return FALSE;
    }

    if( !CasterBBox.Intersect( LightPos, LightRadius ) )
    {
        return FALSE;
    }

    if( pConeTest )
    {
        return SourceConeAffectsBBox( *pConeTest, CasterBBox );
    }

    return TRUE;
}

//=========================================================================

static
vector3 const& GetPointShadowFaceDirection( s32 FaceIndex )
{
    ASSERT( FaceIndex >= 0 );
    ASSERT( FaceIndex < POINT_SHADOW_FACE_COUNT );

    static vector3 const s_FaceDirections[POINT_SHADOW_FACE_COUNT] =
    {
        vector3( 0.0f,  1.0f,  0.0f ),
        vector3( 0.0f, -1.0f,  0.0f ),
        vector3( 0.0f,  0.0f,  1.0f ),
        vector3( 0.0f,  0.0f, -1.0f ),
        vector3( 1.0f,  0.0f,  0.0f ),
        vector3( -1.0f, 0.0f,  0.0f )
    };

    return s_FaceDirections[FaceIndex];
}

//=========================================================================

static
xbool PointShadowFaceAffectsCaster( ShadowConeTest const& Cone, ShadowCasterRecord const& Caster )
{
    return SphereIntersectsFiniteCone( Caster.WorldBBoxCenter, Caster.WorldBBoxRadius, Cone );
}

//=========================================================================

static
f32 GetShadowLightScore( DynamicShadowLight const& DynamicLightData, player* pActivePlayer )
{
    f32 LightScore = static_cast<f32>( DynamicLightData.Color.R + DynamicLightData.Color.G + DynamicLightData.Color.B );
    LightScore -= DynamicLightData.Radius * ShadowRadiusScorePenalty;

    if( pActivePlayer && pActivePlayer->GetBBox().Intersect( DynamicLightData.Pos, DynamicLightData.Radius ) )
    {
        LightScore += PlayerShadowScoreBonus;
    }

    return LightScore;
}

//=========================================================================

static
xbool BuildShadowLightCandidate( ShadowLight& ShadowLightData, s32 LightIndex, player* pActivePlayer )
{
    DynamicShadowLight DynamicLightData;
    xbool              CastShadows;
    f32                UnusedInnerRadius;
    f32                UnusedInnerAngle;
    s32                ShadowMapResolution;
    s32                ShadowPriority;

    if( !GetRenderPipeline().GetLightSystem().GetShadowLight( LightIndex, DynamicLightData.Pos, DynamicLightData.Radius,
                                DynamicLightData.Color, DynamicLightData.Shape, CastShadows, UnusedInnerRadius,
                                DynamicLightData.Direction, DynamicLightData.Falloff, UnusedInnerAngle,
                                DynamicLightData.OuterAngle, ShadowMapResolution, ShadowPriority,
                                DynamicLightData.IsFlashlight ) )
    {
        return FALSE;
    }

    static_cast<void>( UnusedInnerRadius );
    static_cast<void>( UnusedInnerAngle );

    if( !CastShadows || ( DynamicLightData.Radius <= 0.0f ) )
    {
        return FALSE;
    }

    ShadowMode const Mode = GetRenderPipeline().GetShadowMapAtlas().GetShadowMode();
    if( (Mode == ShadowMode::Disabled) ||
        ((Mode == ShadowMode::FlashlightOnly) && !DynamicLightData.IsFlashlight) )
    {
        return FALSE;
    }

    DynamicLightData.ShadowMapResolution = ClampShadowMapResolution( ShadowMapResolution );
    ShadowLightData.DynamicLightIndex = LightIndex;

    ShadowLightData.Shape = DynamicLightData.Shape;
    ShadowLightData.Pos = DynamicLightData.Pos;
    ShadowLightData.Direction = DynamicLightData.Direction;
    ShadowLightData.Radius = DynamicLightData.Radius;
    ShadowLightData.Falloff = DynamicLightData.Falloff;
    ShadowLightData.FOV = GetShadowLightFOV( DynamicLightData );
    ShadowLightData.ShadowMapResolution = DynamicLightData.ShadowMapResolution;
    ShadowLightData.ShadowPriority = ClampShadowPriority( ShadowPriority );
    ShadowLightData.SourceCount = GetShadowLightSourceCount( DynamicLightData.Shape );
    ShadowLightData.RelevantCasterStart = 0;
    ShadowLightData.RelevantCasterCount = 0;
    ShadowLightData.Score = GetShadowLightScore( DynamicLightData, pActivePlayer );
    return TRUE;
}

//=========================================================================

static
xbool SortsBeforeShadowLight( ShadowLight const& A, ShadowLight const& B )
{
    if( A.ShadowPriority != B.ShadowPriority )
    {
        return ( A.ShadowPriority > B.ShadowPriority );
    }

    if( A.Score != B.Score )
    {
        return ( A.Score > B.Score );
    }

    if( A.ShadowMapResolution != B.ShadowMapResolution )
    {
        return ( A.ShadowMapResolution > B.ShadowMapResolution );
    }

    if( A.SourceCount != B.SourceCount )
    {
        return ( A.SourceCount < B.SourceCount );
    }

    return FALSE;
}

//=========================================================================

static
void InsertShadowLight( ShadowLight* pLights, s32& LightCount, ShadowLight const& Light )
{
    if( LightCount >= LightSystem::MAX_SHADOW_LIGHTS )
    {
        if( !SortsBeforeShadowLight( Light, pLights[LightCount - 1] ) )
        {
            return;
        }

        LightCount--;
    }

    s32 InsertAt = LightCount;
    while( InsertAt > 0 )
    {
        if( !SortsBeforeShadowLight( Light, pLights[InsertAt - 1] ) )
        {
            break;
        }

        pLights[InsertAt] = pLights[InsertAt - 1];
        InsertAt--;
    }

    pLights[InsertAt] = Light;
    LightCount++;
}

//=========================================================================

static
s32 SelectShadowLights( ShadowLight const* pShadowLightCandidates, s32 CandidateShadowLightCount, ShadowLight* pShadowLights )
{
    s32 PointShadowLightCount = 0;
    s32 ShadowLightCount = 0;
    s32 ShadowSourceCount = 0;

    for( s32 LightIndex = 0; LightIndex < CandidateShadowLightCount; LightIndex++ )
    {
        ShadowLight const& Candidate = pShadowLightCandidates[LightIndex];

        if( ( Candidate.Shape == LightSystem::LIGHT_SHAPE_OMNI ) && ( PointShadowLightCount >= MAX_SHADOW_LIGHTS ) )
        {
            continue;
        }

        if( ( ShadowSourceCount + Candidate.SourceCount ) > MAX_SHADOW_SOURCES )
        {
            continue;
        }

        pShadowLights[ShadowLightCount++] = Candidate;
        ShadowSourceCount += Candidate.SourceCount;

        if( Candidate.Shape == LightSystem::LIGHT_SHAPE_OMNI )
        {
            PointShadowLightCount++;
        }
    }

    return ShadowLightCount;
}

//=========================================================================

static
xbool IsSelectedShadowLight( ShadowLight const* pShadowLights, s32 ShadowLightCount, s32 DynamicLightIndex )
{
    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        if( pShadowLights[LightIndex].DynamicLightIndex == DynamicLightIndex )
        {
            return TRUE;
        }
    }
    return FALSE;
}

//=========================================================================

static
void RestoreShadowLightResolutions( ShadowLight* pShadowLights, s32 ShadowLightCount, ShadowLight const* pShadowLightCandidates,
                                    s32 CandidateShadowLightCount )
{
    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        for( s32 CandidateIndex = 0; CandidateIndex < CandidateShadowLightCount; CandidateIndex++ )
        {
            if( pShadowLights[LightIndex].DynamicLightIndex ==
                pShadowLightCandidates[CandidateIndex].DynamicLightIndex )
            {
                pShadowLights[LightIndex].ShadowMapResolution =
                    pShadowLightCandidates[CandidateIndex].ShadowMapResolution;
                break;
            }
        }
    }
}

//=========================================================================

static
void AddRelevantObjectCaster( object* pCandidate, ShadowLight const& ShadowLightData, ShadowConeTest const* pLightConeTest,
                              xarray<ShadowCasterRecord>& RelevantCasters )
{
    if( !pCandidate || ( pCandidate->GetType() == object::TYPE_PLAY_SURFACE ) )
    {
        return;
    }

#ifdef X_EDITOR
    if( pCandidate->IsHidden() )
    {
        return;
    }
#endif

    bbox const& CandidateBBox = pCandidate->GetBBox();
    if( !ShadowCasterBoundsAreRelevant( CandidateBBox, static_cast<u8>( pCandidate->GetZone1() ),
                                         static_cast<u8>( pCandidate->GetZone2() ), ShadowLightData.Pos,
                                         ShadowLightData.Radius, pLightConeTest ) )
    {
        return;
    }

    RelevantCasters.Append( MakeObjectShadowCaster( pCandidate, CandidateBBox ) );
}

//=========================================================================

static
xbool CollectShadowLightCasters( ShadowLight& ShadowLightData, xarray<ShadowCasterRecord>& RelevantCasters )
{
    s32 const RelevantCasterStart = RelevantCasters.GetCount();
    bbox const LightBBox( ShadowLightData.Pos, ShadowLightData.Radius );
    ShadowConeTest        LightConeTest;
    ShadowConeTest const* pLightConeTest = nullptr;
    if( ShadowLightData.Shape == LightSystem::LIGHT_SHAPE_SPOT )
    {
        f32 const LightConeOuterCos = x_cos( ShadowLightData.FOV * 0.5f );
        if( LightConeOuterCos > 0.0f )
        {
            SetupShadowConeTest( LightConeTest, ShadowLightData.Pos, ShadowLightData.Direction, LightConeOuterCos,
                                 ShadowLightData.Radius );
            pLightConeTest = &LightConeTest;
        }
    }

    g_ObjMgr.SelectBBox( object::ATTR_ALL, LightBBox, object::TYPE_ALL_TYPES );
    for( slot_id SlotId = g_ObjMgr.StartLoop(); SlotId != SLOT_NULL; SlotId = g_ObjMgr.GetNextResult( SlotId ) )
    {
        AddRelevantObjectCaster( g_ObjMgr.GetObjectBySlot( SlotId ), ShadowLightData, pLightConeTest, RelevantCasters );
    }
    g_ObjMgr.EndLoop();

    g_PlaySurfaceMgr.CollectSurfaces( LightBBox, object::ATTR_ALL, 0 );
    for( playsurface_mgr::surface* pSurface = g_PlaySurfaceMgr.GetNextSurface(); pSurface;
          pSurface = g_PlaySurfaceMgr.GetNextSurface() )
    {
        if( !pSurface->RenderInst.IsNull() &&
             ShadowCasterBoundsAreRelevant( pSurface->WorldBBox, static_cast<u8>( pSurface->ZoneInfo & 0xff ),
                                            static_cast<u8>( pSurface->ZoneInfo >> 8 ), ShadowLightData.Pos,
                                            ShadowLightData.Radius, pLightConeTest ) )
        {
            RelevantCasters.Append( MakePlaySurfaceShadowCaster( pSurface, pSurface->WorldBBox ) );
        }
    }

    ShadowLightData.RelevantCasterStart = RelevantCasterStart;
    ShadowLightData.RelevantCasterCount = RelevantCasters.GetCount() - RelevantCasterStart;
    return ( ShadowLightData.RelevantCasterCount > 0 );
}

//=========================================================================

static
void RefillShadowLights( ShadowLight* pShadowLightCandidates, s32 CandidateShadowLightCount, ShadowLight* pShadowLights, s32& ShadowLightCount,
                         xbool* pCasterTested, xbool* pHasCasters, xbool const* pAtlasRejected, xarray<ShadowCasterRecord>& RelevantCasters )
{
    s32 ShadowSourceCount = 0;
    s32 PointShadowLightCount = 0;
    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        ShadowSourceCount += pShadowLights[LightIndex].SourceCount;
        if( pShadowLights[LightIndex].Shape == LightSystem::LIGHT_SHAPE_OMNI )
        {
            PointShadowLightCount++;
        }
    }

    for( s32 CandidateIndex = 0; CandidateIndex < CandidateShadowLightCount; CandidateIndex++ )
    {
        ShadowLight const& Candidate = pShadowLightCandidates[CandidateIndex];
        s32 const DynamicLightIndex = Candidate.DynamicLightIndex;
        if( pAtlasRejected[DynamicLightIndex] ||
            IsSelectedShadowLight( pShadowLights, ShadowLightCount, DynamicLightIndex ) )
        {
            continue;
        }
        if( ( Candidate.Shape == LightSystem::LIGHT_SHAPE_OMNI ) && ( PointShadowLightCount >= MAX_SHADOW_LIGHTS ) )
        {
            continue;
        }
        if( ( ShadowSourceCount + Candidate.SourceCount ) > MAX_SHADOW_SOURCES )
        {
            continue;
        }

        if( !pCasterTested[DynamicLightIndex] )
        {
            ShadowLight TestedCandidate = Candidate;
            pHasCasters[DynamicLightIndex] = CollectShadowLightCasters( TestedCandidate, RelevantCasters );
            pShadowLightCandidates[CandidateIndex].RelevantCasterStart = TestedCandidate.RelevantCasterStart;
            pShadowLightCandidates[CandidateIndex].RelevantCasterCount = TestedCandidate.RelevantCasterCount;
            pCasterTested[DynamicLightIndex] = TRUE;
        }
        if( !pHasCasters[DynamicLightIndex] )
        {
            continue;
        }

        pShadowLights[ShadowLightCount++] = pShadowLightCandidates[CandidateIndex];
        ShadowSourceCount += Candidate.SourceCount;
        if( Candidate.Shape == LightSystem::LIGHT_SHAPE_OMNI )
        {
            PointShadowLightCount++;
        }
    }
}

//=========================================================================

static
u64 BuildFullShadowLightSourceMask( ShadowLight const& ShadowLightData, s32 SourceBase )
{
    u64 SourceMask = 0;

    for( s32 SourceIndex = 0; SourceIndex < ShadowLightData.SourceCount; SourceIndex++ )
    {
        SourceMask |= ( static_cast<u64>( 1 ) << ( SourceBase + SourceIndex ) );
    }

    return SourceMask;
}

//=========================================================================

static
u64 BuildShadowCasterSourceMask( ShadowLight const& ShadowLightData, s32 SourceBase, ShadowCasterRecord const& Caster, u64 LightSourceMask,
                                 ShadowConeTest const* pPointFaceCones )
{
    if( ShadowLightData.SourceCount <= 0 )
    {
        return 0;
    }

    if( ShadowLightData.Shape != LightSystem::LIGHT_SHAPE_OMNI )
    {
        return LightSourceMask;
    }

    s32 const FaceCount = ( ShadowLightData.SourceCount < POINT_SHADOW_FACE_COUNT )
                              ? ShadowLightData.SourceCount
                              : POINT_SHADOW_FACE_COUNT;
    u64 SourceMask = 0;

    ASSERT( pPointFaceCones );
    for( s32 FaceIndex = 0; FaceIndex < FaceCount; FaceIndex++ )
    {
        if( PointShadowFaceAffectsCaster( pPointFaceCones[FaceIndex], Caster ) )
        {
            SourceMask |= ( static_cast<u64>( 1 ) << ( SourceBase + FaceIndex ) );
        }
    }

    return SourceMask;
}

//=========================================================================

static
void AddShadowCastersInLight( xarray<ShadowCasterRecord> const& RelevantCasters, ShadowLight const& ShadowLightData,
                              xarray<ShadowCasterHashEntry>& CasterHash, s32 HashMask, s32 SourceBase,
                              ShadowMapAtlas const& Atlas )
{
    s32 const             EndRelevantCaster = ShadowLightData.RelevantCasterStart + ShadowLightData.RelevantCasterCount;
    u64 const             LightSourceMask = BuildFullShadowLightSourceMask( ShadowLightData, SourceBase );
    ShadowConeTest        PointFaceCones[POINT_SHADOW_FACE_COUNT];
    ShadowConeTest const* pPointFaceCones = nullptr;

    if( ShadowLightData.Shape == LightSystem::LIGHT_SHAPE_OMNI )
    {
        s32 const FaceCount = ( ShadowLightData.SourceCount < POINT_SHADOW_FACE_COUNT )
                                  ? ShadowLightData.SourceCount
                                  : POINT_SHADOW_FACE_COUNT;
        for( s32 FaceIndex = 0; FaceIndex < FaceCount; FaceIndex++ )
        {
            ShadowMapAtlas::ShadowSource const& Source = Atlas.GetSource( SourceBase + FaceIndex );
            SetupShadowConeTest( PointFaceCones[FaceIndex], ShadowLightData.Pos,
                                 GetPointShadowFaceDirection( FaceIndex ), Source.FaceLightData.GetX(),
                                 ShadowLightData.Radius );
        }

        pPointFaceCones = PointFaceCones;
    }

    for( s32 RelevantCasterIndex = ShadowLightData.RelevantCasterStart; RelevantCasterIndex < EndRelevantCaster;
          RelevantCasterIndex++ )
    {
        ShadowCasterRecord const& Caster = RelevantCasters[RelevantCasterIndex];
        u64 const                 SourceMask =
            BuildShadowCasterSourceMask( ShadowLightData, SourceBase, Caster, LightSourceMask, pPointFaceCones );

        if( SourceMask == 0 )
        {
            continue;
        }

        s32 HashIndex = static_cast<s32>( GetShadowCasterHash( Caster ) & static_cast<u32>( HashMask ) );
        while( CasterHash[HashIndex].Occupied && !SameShadowCaster( CasterHash[HashIndex].Caster, Caster ) )
        {
            HashIndex = ( HashIndex + 1 ) & HashMask;
        }

        ShadowCasterHashEntry& Entry = CasterHash[HashIndex];
        if( !Entry.Occupied )
        {
            Entry.Caster = Caster;
            Entry.SourceMask = SourceMask;
            Entry.Occupied = TRUE;
        }
        else
        {
            Entry.SourceMask |= SourceMask;
        }
    }
}

//=========================================================================

static
void CollectShadowCastersForLights( xarray<ShadowCasterRecord> const& RelevantCasters, ShadowLight const* pShadowLights, s32 ShadowLightCount,
                                    xarray<ShadowCasterRecord>& ShadowCasters, xarray<u64>& CasterMasks,
                                    xarray<ShadowCasterHashEntry>& CasterHash, ShadowMapAtlas const& Atlas )
{
    if( RelevantCasters.GetCount() <= 0 )
    {
        return;
    }

    s32 HashSize = 1;
    while( HashSize < ( RelevantCasters.GetCount() * 2 ) )
    {
        HashSize <<= 1;
    }
    CasterHash.SetCount( HashSize );
    x_memset( CasterHash.GetPtr(), 0, sizeof( ShadowCasterHashEntry ) * HashSize );

    s32 SourceBase = 0;
    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        ShadowLight const& ShadowLightData = pShadowLights[LightIndex];
        AddShadowCastersInLight( RelevantCasters, ShadowLightData, CasterHash, HashSize - 1, SourceBase, Atlas );
        SourceBase += ShadowLightData.SourceCount;
    }

    for( s32 EntryIndex = 0; EntryIndex < HashSize; EntryIndex++ )
    {
        ShadowCasterHashEntry const& Entry = CasterHash[EntryIndex];
        if( Entry.Occupied )
        {
            ShadowCasters.Append( Entry.Caster );
            CasterMasks.Append( Entry.SourceMask );
        }
    }
}

//=========================================================================

static
xbool IsPowerOfTwo( s32 Value )
{
    return ( Value > 0 ) && ( ( Value & ( Value - 1 ) ) == 0 );
}

//=========================================================================

static
s32 GetShadowAtlasSplitCount( s32 AtlasSize, s32 MinTileSize )
{
    ASSERT( AtlasSize > 0 );
    ASSERT( MinTileSize > 0 );
    ASSERT( IsPowerOfTwo( AtlasSize ) );
    ASSERT( IsPowerOfTwo( MinTileSize ) );
    ASSERT( AtlasSize >= MinTileSize );

    s32 SplitCount = 0;
    while( AtlasSize > MinTileSize )
    {
        AtlasSize /= 2;
        SplitCount++;
    }

    return SplitCount;
}

//=========================================================================

static
s32 GetMaxShadowAtlasFreeNodeCount( s32 AtlasSize, s32 EntryCount )
{
    s32 const SplitCount = GetShadowAtlasSplitCount( AtlasSize, 256 );
    return 1 + ( MAX( EntryCount, 0 ) * SplitCount * 3 );
}

//=========================================================================

static
void AppendShadowAtlasFreeNode( xarray<ShadowAtlasFreeNode>& FreeNodes, s32 AtlasTileX, s32 AtlasTileY, s32 TileSize )
{
    ShadowAtlasFreeNode& Node = FreeNodes.Append();
    Node.AtlasTileX = AtlasTileX;
    Node.AtlasTileY = AtlasTileY;
    Node.TileSize = TileSize;
}

//=========================================================================

static
void SortShadowAtlasEntries( ShadowAtlasEntry* pEntries, s32 EntryCount )
{
    for( s32 Index = 1; Index < EntryCount; Index++ )
    {
        ShadowAtlasEntry const Entry = pEntries[Index];
        s32                    J = Index - 1;
        while( J >= 0 )
        {
            if( pEntries[J].TileSize > Entry.TileSize )
            {
                break;
            }

            if( ( pEntries[J].TileSize == Entry.TileSize ) && ( pEntries[J].ShadowPriority > Entry.ShadowPriority ) )
            {
                break;
            }

            if( ( pEntries[J].TileSize == Entry.TileSize ) && ( pEntries[J].ShadowPriority == Entry.ShadowPriority ) &&
                 ( pEntries[J].ShadowScore > Entry.ShadowScore ) )
            {
                break;
            }

            if( ( pEntries[J].TileSize == Entry.TileSize ) && ( pEntries[J].ShadowPriority == Entry.ShadowPriority ) &&
                 ( pEntries[J].ShadowScore == Entry.ShadowScore ) && ( pEntries[J].SourceIndex < Entry.SourceIndex ) )
            {
                break;
            }

            pEntries[J + 1] = pEntries[J];
            J--;
        }

        pEntries[J + 1] = Entry;
    }
}

//=========================================================================

static
xbool TryPackShadowAtlasEntries( ShadowAtlasEntry* pEntries, s32 EntryCount, s32 AtlasSize, xarray<ShadowAtlasFreeNode>& FreeNodes )
{
    SortShadowAtlasEntries( pEntries, EntryCount );

    ASSERT( IsPowerOfTwo( AtlasSize ) );
    if( !IsPowerOfTwo( AtlasSize ) )
    {
        return FALSE;
    }

    FreeNodes.SetCount( 0 );
    s32 const RequiredCapacity = GetMaxShadowAtlasFreeNodeCount( AtlasSize, EntryCount );
    if( FreeNodes.GetCapacity() < RequiredCapacity )
    {
        FreeNodes.SetCapacity( RequiredCapacity );
    }
    AppendShadowAtlasFreeNode( FreeNodes, 0, 0, AtlasSize );

    for( s32 Index = 0; Index < EntryCount; Index++ )
    {
        s32 const TileSize = pEntries[Index].TileSize;
        ASSERT( IsPowerOfTwo( TileSize ) );
        if( !IsPowerOfTwo( TileSize ) )
        {
            return FALSE;
        }
        if( ( TileSize <= 0 ) || ( TileSize > AtlasSize ) )
        {
            return FALSE;
        }

        s32 BestNodeIndex = -1;
        for( s32 NodeIndex = 0; NodeIndex < FreeNodes.GetCount(); NodeIndex++ )
        {
            ShadowAtlasFreeNode const& Node = FreeNodes[NodeIndex];
            if( Node.TileSize < TileSize )
            {
                continue;
            }

            if( BestNodeIndex < 0 )
            {
                BestNodeIndex = NodeIndex;
                continue;
            }

            ShadowAtlasFreeNode const& BestNode = FreeNodes[BestNodeIndex];
            if( Node.TileSize < BestNode.TileSize )
            {
                BestNodeIndex = NodeIndex;
                continue;
            }

            if( ( Node.TileSize == BestNode.TileSize ) && ( Node.AtlasTileY < BestNode.AtlasTileY ) )
            {
                BestNodeIndex = NodeIndex;
                continue;
            }

            if( ( Node.TileSize == BestNode.TileSize ) && ( Node.AtlasTileY == BestNode.AtlasTileY ) &&
                 ( Node.AtlasTileX < BestNode.AtlasTileX ) )
            {
                BestNodeIndex = NodeIndex;
            }
        }

        if( BestNodeIndex < 0 )
        {
            return FALSE;
        }

        ShadowAtlasFreeNode Node = FreeNodes[BestNodeIndex];
        FreeNodes[BestNodeIndex] = FreeNodes[FreeNodes.GetCount() - 1];
        FreeNodes.SetCount( FreeNodes.GetCount() - 1 );

        while( Node.TileSize > TileSize )
        {
            s32 const ChildTileSize = Node.TileSize / 2;

            if( ( ChildTileSize <= 0 ) || ( ChildTileSize < TileSize ) )
            {
                return FALSE;
            }

            AppendShadowAtlasFreeNode( FreeNodes, Node.AtlasTileX + ChildTileSize, Node.AtlasTileY, ChildTileSize );
            AppendShadowAtlasFreeNode( FreeNodes, Node.AtlasTileX, Node.AtlasTileY + ChildTileSize, ChildTileSize );
            AppendShadowAtlasFreeNode( FreeNodes, Node.AtlasTileX + ChildTileSize, Node.AtlasTileY + ChildTileSize,
                                       ChildTileSize );

            Node.TileSize = ChildTileSize;
        }

        pEntries[Index].AtlasTileX = Node.AtlasTileX;
        pEntries[Index].AtlasTileY = Node.AtlasTileY;
    }

    return TRUE;
}

//=========================================================================

static
s32 GetShadowAtlasSizeForEntries( ShadowAtlasEntry* pEntries, s32 EntryCount, xarray<ShadowAtlasFreeNode>& FreeNodes )
{
    for( s32 AtlasSize = 256; AtlasSize <= MAX_SHADOW_ATLAS_SIZE; AtlasSize *= 2 )
    {
        if( TryPackShadowAtlasEntries( pEntries, EntryCount, AtlasSize, FreeNodes ) )
        {
            return AtlasSize;
        }
    }

    return 0;
}

//=========================================================================

static
xbool AtlasFitsShadowLights( ShadowLight const* pShadowLights, s32 ShadowLightCount, xarray<ShadowAtlasFreeNode>& FreeNodes )
{
    ShadowAtlasEntry Entries[MAX_SHADOW_SOURCES];
    s32              EntryCount = 0;

    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        ShadowLight const& ShadowLightData = pShadowLights[LightIndex];
        for( s32 SourceIndex = 0; SourceIndex < ShadowLightData.SourceCount; SourceIndex++ )
        {
            ASSERT( EntryCount < MAX_SHADOW_SOURCES );
            Entries[EntryCount].SourceIndex = EntryCount;
            Entries[EntryCount].TileSize = ClampShadowMapResolution( ShadowLightData.ShadowMapResolution );
            Entries[EntryCount].ShadowPriority = ShadowLightData.ShadowPriority;
            Entries[EntryCount].ShadowScore = ShadowLightData.Score;
            Entries[EntryCount].AtlasTileX = 0;
            Entries[EntryCount].AtlasTileY = 0;
            EntryCount++;
        }
    }

    if( EntryCount <= 0 )
    {
        return TRUE;
    }

    return ( GetShadowAtlasSizeForEntries( Entries, EntryCount, FreeNodes ) > 0 );
}

//=========================================================================

static
s32 FindShadowLightForResolutionDrop( ShadowLight const* pShadowLights, s32 ShadowLightCount, s32 ShadowPriority )
{
    s32 BestLightIndex = -1;

    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        ShadowLight const& ShadowLightData = pShadowLights[LightIndex];
        if( ShadowLightData.ShadowPriority != ShadowPriority )
        {
            continue;
        }
        if( ShadowLightData.ShadowMapResolution <= 256 )
        {
            continue;
        }

        if( BestLightIndex < 0 )
        {
            BestLightIndex = LightIndex;
            continue;
        }

        ShadowLight const& BestLight = pShadowLights[BestLightIndex];
        if( ShadowLightData.Score < BestLight.Score )
        {
            BestLightIndex = LightIndex;
            continue;
        }

        if( ( ShadowLightData.Score == BestLight.Score ) &&
             ( ShadowLightData.ShadowMapResolution > BestLight.ShadowMapResolution ) )
        {
            BestLightIndex = LightIndex;
        }
    }

    return BestLightIndex;
}

//=========================================================================

static
s32 FindShadowLightForRemoval( ShadowLight const* pShadowLights, s32 ShadowLightCount, s32 ShadowPriority )
{
    s32 BestLightIndex = -1;

    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        ShadowLight const& ShadowLightData = pShadowLights[LightIndex];
        if( ShadowLightData.ShadowPriority != ShadowPriority )
        {
            continue;
        }

        if( BestLightIndex < 0 )
        {
            BestLightIndex = LightIndex;
            continue;
        }

        ShadowLight const& BestLight = pShadowLights[BestLightIndex];
        if( ShadowLightData.Score < BestLight.Score )
        {
            BestLightIndex = LightIndex;
            continue;
        }

        if( ( ShadowLightData.Score == BestLight.Score ) &&
             ( ShadowLightData.ShadowMapResolution > BestLight.ShadowMapResolution ) )
        {
            BestLightIndex = LightIndex;
        }
    }

    return BestLightIndex;
}

//=========================================================================

static
void RemoveShadowLight( ShadowLight* pShadowLights, s32& ShadowLightCount, s32 RemoveIndex )
{
    ASSERT( ( RemoveIndex >= 0 ) && ( RemoveIndex < ShadowLightCount ) );

    for( s32 LightIndex = RemoveIndex + 1; LightIndex < ShadowLightCount; LightIndex++ )
    {
        pShadowLights[LightIndex - 1] = pShadowLights[LightIndex];
    }

    ShadowLightCount--;
}

//=========================================================================

static
void ResolveAtlasBudget( ShadowLight* pShadowLights, s32& ShadowLightCount, xarray<ShadowAtlasFreeNode>& FreeNodes )
{
    for( s32 ShadowPriority = MinShadowPriority; ShadowPriority <= MaxShadowPriority; ShadowPriority++ )
    {
        while( !AtlasFitsShadowLights( pShadowLights, ShadowLightCount, FreeNodes ) )
        {
            s32 const LightToReduceIndex =
                FindShadowLightForResolutionDrop( pShadowLights, ShadowLightCount, ShadowPriority );
            if( LightToReduceIndex >= 0 )
            {
                pShadowLights[LightToReduceIndex].ShadowMapResolution =
                    ReduceShadowMapResolution( pShadowLights[LightToReduceIndex].ShadowMapResolution );
                continue;
            }

            s32 const LightToRemoveIndex = FindShadowLightForRemoval( pShadowLights, ShadowLightCount, ShadowPriority );
            if( LightToRemoveIndex >= 0 )
            {
                RemoveShadowLight( pShadowLights, ShadowLightCount, LightToRemoveIndex );
                continue;
            }

            break;
        }

        if( AtlasFitsShadowLights( pShadowLights, ShadowLightCount, FreeNodes ) )
        {
            return;
        }
    }
}

//=========================================================================

static
void AddPointShadowSources( vector3 const& LightPos, f32 LightRadius, f32 LightFalloff, s32 ShadowMapResolution,
                            s32 ShadowPriority, f32 ShadowScore, s32 DynamicLightIndex )
{
    for( s32 FaceIndex = 0; FaceIndex < ARRAYSIZE( PointShadowFaces ); FaceIndex++ )
    {
        matrix4 FaceL2W;
        FaceL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ),
                       radian3( PointShadowFaces[FaceIndex].Pitch, PointShadowFaces[FaceIndex].Yaw, R_0 ), LightPos );

        GetRenderPipeline().GetShadowSubmission().AddPointSource( FaceL2W, PointShadowFOV, LightRadius, LightFalloff, ShadowMapResolution,
                                         ShadowPriority, ShadowScore, DynamicLightIndex );
    }
}

//=========================================================================

static
void AddSpotShadowSource( vector3 const& LightPos, vector3 const& LightDirection, radian ShadowFOV, f32 LightRadius, f32 LightFalloff,
                          s32 ShadowMapResolution, s32 ShadowPriority, f32 ShadowScore, s32 DynamicLightIndex )
{
    matrix4 SpotL2W;
    SpotL2W.Setup( vector3( 1.0f, 1.0f, 1.0f ), radian3( LightDirection.GetPitch(), LightDirection.GetYaw(), R_0 ),
                   LightPos );

    GetRenderPipeline().GetShadowSubmission().AddSpotSource( SpotL2W, ShadowFOV, LightRadius, LightFalloff, ShadowMapResolution, ShadowPriority,
                                    ShadowScore, DynamicLightIndex );
}

//=========================================================================

static
void AddShadowSources( ShadowLight const* pShadowLights, s32 ShadowLightCount )
{
    for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
    {
        ShadowLight const& ShadowLightData = pShadowLights[LightIndex];

        if( ShadowLightData.Shape == LightSystem::LIGHT_SHAPE_OMNI )
        {
            AddPointShadowSources( ShadowLightData.Pos, ShadowLightData.Radius, ShadowLightData.Falloff,
                                   ShadowLightData.ShadowMapResolution, ShadowLightData.ShadowPriority,
                                   ShadowLightData.Score, ShadowLightData.DynamicLightIndex );
        }
        else
        {
            AddSpotShadowSource( ShadowLightData.Pos, ShadowLightData.Direction, ShadowLightData.FOV,
                                 ShadowLightData.Radius, ShadowLightData.Falloff, ShadowLightData.ShadowMapResolution,
                                 ShadowLightData.ShadowPriority, ShadowLightData.Score,
                                 ShadowLightData.DynamicLightIndex );
        }
    }
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

ShadowMapAtlas::ShadowMapAtlas( void )
    : m_pScratch( nullptr )
    , m_SourceCount( 0 )
    , m_PointFaceCount( 0 )
    , m_PointLightCount( 0 )
    , m_AtlasSourceCount( 0 )
    , m_AtlasSize( 0 )
    , m_AtlasSizeFloor( 0 )
    , m_AtlasLayoutDirty( FALSE )
    , m_Enabled( TRUE )
    , m_ShadowMode( ShadowMode::Enabled )
    , m_ShadowFilterType( ShadowFilterType::Evsm )
{
    x_memset( m_Sources, 0, sizeof( m_Sources ) );
}

//=========================================================================

ShadowMapAtlas::~ShadowMapAtlas( void )
{
    ClearSources();
    delete m_pScratch;
    m_pScratch = nullptr;
}

//=========================================================================

ShadowMapAtlas::ScratchData& ShadowMapAtlas::GetScratch( void )
{
    if( !m_pScratch )
    {
        m_pScratch = new ScratchData;
    }

    return *m_pScratch;
}

//=========================================================================

void ShadowMapAtlas::ComputePerspectiveSource( ShadowSource& Destination, s32 SourceType,
                                             matrix4 const& LocalToWorld, radian FieldOfView,
                                             f32 LightRadius, f32 LightFalloff, s32 ShadowMapResolution ) const
{
    ShadowMapResolution = ClampShadowMapResolution( ShadowMapResolution );

    f32 ReceiveNearZ = ComputeSpotShadowReceiveNearZ( LightRadius );
    f32 NearZ = MAX( 0.5f, LightRadius * 0.01f );

    if( SourceType == SHADOW_SOURCE_POINT_FACE )
    {
        ReceiveNearZ = ComputePointShadowReceiveNearZ( LightRadius );
        NearZ = ComputePointShadowNearZ( LightRadius );
    }

    NearZ = MAX( 0.01f, NearZ );

    f32 FarZ = MAX( NearZ + 1.0f, LightRadius );

    matrix4 W2Proj = LocalToWorld;
    W2Proj.InvertRT();

    f32 const TanHalfFOV = x_tan( FieldOfView * 0.5f );
    f32 const Scale = ( TanHalfFOV > 0.0f ) ? ( 1.0f / TanHalfFOV ) : 1.0f;

    matrix4 Proj2Clip;
    Proj2Clip.Zero();

    if( SourceType == SHADOW_SOURCE_POINT_FACE )
    {
        Proj2Clip( 0, 0 ) = Scale;
    }
    else
    {
        Proj2Clip( 0, 0 ) = -Scale;
    }

    Proj2Clip( 1, 1 ) = Scale;
    Proj2Clip( 2, 2 ) = FarZ / ( FarZ - NearZ );
    Proj2Clip( 3, 2 ) = -( NearZ * FarZ ) / ( FarZ - NearZ );
    Proj2Clip( 2, 3 ) = 1.0f;

    Destination.Type = SourceType;
    Destination.DynamicLightIndex = -1;
    Destination.PointLightIndex = -1;
    Destination.FaceIndex = -1;
    Destination.RequestedResolution = ShadowMapResolution;
    Destination.ShadowPriority = MinShadowPriority;
    Destination.ShadowScore = 0.0f;
    Destination.WorldToClip = Proj2Clip * W2Proj;
    Destination.WorldToAtlas = Destination.WorldToClip;
    Destination.FaceLightDirFalloff.Zero();
    Destination.FaceLightData.Zero();
    Destination.LightFalloff = LightFalloff;
    Destination.NearZ = NearZ;
    Destination.ReceiveNearZ = ReceiveNearZ;
    Destination.FarZ = FarZ;
    Destination.ProjectionScale = Scale;
    Destination.AtlasX = 0;
    Destination.AtlasY = 0;
    Destination.AtlasWidth = ShadowMapResolution;
    Destination.AtlasHeight = ShadowMapResolution;
    Destination.AtlasTileX = 0;
    Destination.AtlasTileY = 0;
    Destination.AtlasTileWidth = ShadowMapResolution;
    Destination.AtlasTileHeight = ShadowMapResolution;

    vector3 const LightPos = LocalToWorld.GetTranslation();
    vector3 const Extent( FarZ, FarZ, FarZ );
    Destination.LightPosRadius = vector4( LightPos.GetX(), LightPos.GetY(), LightPos.GetZ(), LightRadius );
    Destination.WorldBBox.Set( LightPos - Extent, LightPos + Extent );
}

//=========================================================================

void ShadowMapAtlas::UpdateAtlasLayout( void )
{
    m_AtlasSize = 0;

    if( m_AtlasSourceCount <= 0 )
    {
        return;
    }

    ScratchData& Scratch = GetScratch();

    ShadowAtlasEntry Entries[MAX_SHADOW_SOURCES];
    s32              EntryCount = 0;

    for( s32 Index = 0; Index < m_SourceCount; Index++ )
    {
        ShadowSource const& Source = m_Sources[Index];
        ASSERT( EntryCount < MAX_SHADOW_SOURCES );
        Entries[EntryCount].SourceIndex = Index;
        Entries[EntryCount].TileSize = ClampShadowMapResolution( Source.RequestedResolution );
        Entries[EntryCount].ShadowPriority = Source.ShadowPriority;
        Entries[EntryCount].ShadowScore = Source.ShadowScore;
        Entries[EntryCount].AtlasTileX = 0;
        Entries[EntryCount].AtlasTileY = 0;
        EntryCount++;
    }

    s32 AtlasSize = GetShadowAtlasSizeForEntries( Entries, EntryCount, Scratch.AtlasFreeNodes );
    while( AtlasSize <= 0 )
    {
        s32 LargestEntryIndex = -1;
        for( s32 Index = 0; Index < EntryCount; Index++ )
        {
            if( Entries[Index].TileSize <= 256 )
            {
                continue;
            }

            if( LargestEntryIndex < 0 )
            {
                LargestEntryIndex = Index;
                continue;
            }

            if( Entries[Index].ShadowPriority < Entries[LargestEntryIndex].ShadowPriority )
            {
                LargestEntryIndex = Index;
                continue;
            }

            if( ( Entries[Index].ShadowPriority == Entries[LargestEntryIndex].ShadowPriority ) &&
                 ( Entries[Index].ShadowScore < Entries[LargestEntryIndex].ShadowScore ) )
            {
                LargestEntryIndex = Index;
                continue;
            }

            if( ( Entries[Index].ShadowPriority == Entries[LargestEntryIndex].ShadowPriority ) &&
                 ( Entries[Index].ShadowScore == Entries[LargestEntryIndex].ShadowScore ) &&
                 ( Entries[Index].TileSize > Entries[LargestEntryIndex].TileSize ) )
            {
                LargestEntryIndex = Index;
            }
        }

        if( LargestEntryIndex < 0 )
        {
            ASSERT( FALSE );
            break;
        }

        ShadowSource const& SelectedSource = m_Sources[Entries[LargestEntryIndex].SourceIndex];
        s32 const ReducedTileSize = ReduceShadowMapResolution( Entries[LargestEntryIndex].TileSize );
        if( SelectedSource.Type == SHADOW_SOURCE_POINT_FACE )
        {
            for( s32 Index = 0; Index < EntryCount; Index++ )
            {
                ShadowSource const& Source = m_Sources[Entries[Index].SourceIndex];
                if( ( Source.Type == SHADOW_SOURCE_POINT_FACE ) &&
                     ( Source.PointLightIndex == SelectedSource.PointLightIndex ) )
                {
                    Entries[Index].TileSize = ReducedTileSize;
                }
            }
        }
        else
        {
            Entries[LargestEntryIndex].TileSize = ReducedTileSize;
        }
        AtlasSize = GetShadowAtlasSizeForEntries( Entries, EntryCount, Scratch.AtlasFreeNodes );
    }

    s32 const AtlasFloorCap = 4096;
    if( AtlasSize < m_AtlasSizeFloor )
    {
        AtlasSize = m_AtlasSizeFloor;
    }
    if( AtlasSize <= AtlasFloorCap && AtlasSize > m_AtlasSizeFloor )
    {
        m_AtlasSizeFloor = AtlasSize;
    }

    m_AtlasSize = AtlasSize;

    f32 DepthFilterRadius = 0.0f;
    f32 DepthToMomentScale = 1.0f;
    if( m_ShadowFilterType == ShadowFilterType::Evsm )
    {
        // Scale the blur radius to depth texels at the selected atlas ratio.
        s32 const MomentAtlasSize = MIN( MAX( AtlasSize / 2, 1 ), MAX_SHADOW_MOMENT_ATLAS_SIZE );
        DepthToMomentScale = static_cast<f32>( AtlasSize ) / static_cast<f32>( MomentAtlasSize );
        DepthFilterRadius =
            static_cast<f32>( SHADOW_EVSM_BLUR_RADIUS ) * SHADOW_EVSM_BLUR_SCALE * DepthToMomentScale;
    }
    s32 const GuardTexels = GetAtlasGuardTexels( DepthFilterRadius );

    for( s32 Index = 0; Index < EntryCount; Index++ )
    {
        ShadowSource& Source = m_Sources[Entries[Index].SourceIndex];
        s32 const     TileSize = Entries[Index].TileSize;

        Source.AtlasTileX = Entries[Index].AtlasTileX;
        Source.AtlasTileY = Entries[Index].AtlasTileY;
        Source.AtlasTileWidth = TileSize;
        Source.AtlasTileHeight = TileSize;
        Source.AtlasX = Source.AtlasTileX;
        Source.AtlasY = Source.AtlasTileY;
        Source.AtlasWidth = Source.AtlasTileWidth;
        Source.AtlasHeight = Source.AtlasTileHeight;

        if( GuardTexels > 0 )
        {
            s32 const MaxGuardX = ( Source.AtlasWidth > 2 ) ? ( ( Source.AtlasWidth - 1 ) / 2 ) : 0;
            s32 const MaxGuardY = ( Source.AtlasHeight > 2 ) ? ( ( Source.AtlasHeight - 1 ) / 2 ) : 0;
            s32 const GuardX = ( GuardTexels < MaxGuardX ) ? GuardTexels : MaxGuardX;
            s32 const GuardY = ( GuardTexels < MaxGuardY ) ? GuardTexels : MaxGuardY;

            Source.AtlasX += GuardX;
            Source.AtlasY += GuardY;
            Source.AtlasWidth -= GuardX * 2;
            Source.AtlasHeight -= GuardY * 2;
        }

        if( Source.Type == SHADOW_SOURCE_POINT_FACE )
        {
            f32 const Resolution     = static_cast<f32>( Source.AtlasWidth );
            f32 const FilterPadding  = DepthFilterRadius + DepthToMomentScale * 0.5f + 0.5f;
            f32 const Padding        = x_ceil( FilterPadding + SHADOW_SEAM_BLEND_TEXELS );
            f32 const Coverage       = 1.0f - 2.0f * Padding / Resolution;
            f32 const NormalBias     = m_ShadowFilterType == ShadowFilterType::Hard
                                           ? 2.0f * SHADOW_NORMAL_BIAS_TEXELS / Resolution : 0.0f;
            f32 const BiasedCoverage = Coverage - NormalBias;
            f32 const Discriminant   = BiasedCoverage * BiasedCoverage - 4.0f * Coverage * NormalBias;

            ASSERT( ( Coverage > 0.0f ) && ( Discriminant > 0.0f ) );

            f32 const ProjectionScale    = 0.5f * ( BiasedCoverage + x_sqrt( MAX( Discriminant, 0.0f ) ) );
            f32 const CoverageCorrection = ProjectionScale / Source.ProjectionScale;

            Source.WorldToClip.Scale( vector3( CoverageCorrection, CoverageCorrection, 1.0f ) );
            Source.ProjectionScale = ProjectionScale;
            Source.FaceLightData.GetX() = ComputePointFaceCoverageCos( ProjectionScale );
        }

        Source.WorldToAtlas = Source.WorldToClip;
        Source.WorldToAtlas.Scale( vector3( 0.5f, -0.5f, 1.0f ) );
        Source.WorldToAtlas.Translate( vector3( 0.5f, 0.5f, 0.0f ) );
        Source.WorldToAtlas.Scale( vector3( static_cast<f32>( Source.AtlasWidth ) / static_cast<f32>( m_AtlasSize ),
                                            static_cast<f32>( Source.AtlasHeight ) / static_cast<f32>( m_AtlasSize ),
                                            1.0f ) );
        Source.WorldToAtlas.Translate( vector3( static_cast<f32>( Source.AtlasX ) / static_cast<f32>( m_AtlasSize ),
                                                static_cast<f32>( Source.AtlasY ) / static_cast<f32>( m_AtlasSize ),
                                                0.0f ) );
    }
}

//=========================================================================

void ShadowMapAtlas::ClearSources( void )
{
    m_SourceCount = 0;
    m_PointFaceCount = 0;
    m_PointLightCount = 0;
    m_AtlasSourceCount = 0;
    m_AtlasSize = 0;
    m_AtlasLayoutDirty = FALSE;
}

//=========================================================================

void ShadowMapAtlas::FinalizeSources( void )
{
    if( !m_AtlasLayoutDirty )
    {
        return;
    }

    UpdateAtlasLayout();
    m_AtlasLayoutDirty = FALSE;
}

//=========================================================================

void ShadowMapAtlas::SetEnabled( xbool Enabled )
{
    m_Enabled = Enabled;
    if( !m_Enabled )
    {
        ClearSources();
    }
}

//=========================================================================

void ShadowMapAtlas::SetShadowMode( ShadowMode Mode )
{
    ASSERT( (Mode == ShadowMode::Disabled) ||
            (Mode == ShadowMode::Enabled) ||
            (Mode == ShadowMode::FlashlightOnly) );

    m_ShadowMode = Mode;
    SetEnabled( Mode != ShadowMode::Disabled );
}

//=========================================================================

void ShadowMapAtlas::SetShadowFilterType( ShadowFilterType Type )
{
    if( m_ShadowFilterType == Type )
    {
        return;
    }

    m_ShadowFilterType = Type;
    m_AtlasLayoutDirty = ( m_SourceCount > 0 );
}

//=========================================================================

xbool ShadowMapAtlas::AddPointSource( matrix4 const& LocalToWorld, radian FieldOfView, f32 LightRadius, f32 LightFalloff,
                                    s32 ShadowMapResolution, s32 ShadowPriority, f32 ShadowScore,
                                    s32 DynamicLightIndex )
{
    if( !m_Enabled )
    {
        return FALSE;
    }

    if( m_SourceCount >= MAX_SHADOW_SOURCES )
    {
        return FALSE;
    }

    if( ( m_PointLightCount >= MAX_SHADOW_LIGHTS ) &&
        ( ( m_PointFaceCount % POINT_SHADOW_FACE_COUNT ) == 0 ) )
    {
        return FALSE;
    }

    ShadowSource& Source = m_Sources[m_SourceCount];
    ComputePerspectiveSource( Source, SHADOW_SOURCE_POINT_FACE, LocalToWorld, FieldOfView, LightRadius, LightFalloff,
                              ShadowMapResolution );

    vector3 FaceDirection = LocalToWorld.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );

    if( !FaceDirection.SafeNormalize() )
    {
        ASSERT( FALSE );
        return FALSE;
    }

    s32 const FaceIndex = m_PointFaceCount % POINT_SHADOW_FACE_COUNT;
    vector3 const& CanonicalDirection = GetPointShadowFaceDirection( FaceIndex );
    ASSERT( FaceDirection.Dot( CanonicalDirection ) > 0.9999f );

    if( FaceDirection.Dot( CanonicalDirection ) <= 0.9999f )
    {
        return FALSE;
    }

    if( FaceIndex > 0 )
    {
        ShadowSource const& Previous = m_Sources[m_SourceCount - 1];
        xbool const SameLight = Previous.Type == SHADOW_SOURCE_POINT_FACE &&
                                Previous.FaceIndex == FaceIndex - 1 &&
                                Previous.DynamicLightIndex == DynamicLightIndex &&
                                Previous.LightPosRadius.GetX() == Source.LightPosRadius.GetX() &&
                                Previous.LightPosRadius.GetY() == Source.LightPosRadius.GetY() &&
                                Previous.LightPosRadius.GetZ() == Source.LightPosRadius.GetZ() &&
                                Previous.LightPosRadius.GetW() == Source.LightPosRadius.GetW();
        ASSERT( SameLight );

        if( !SameLight )
        {
            return FALSE;
        }
    }

    Source.FaceLightDirFalloff.Set( CanonicalDirection.GetX(), CanonicalDirection.GetY(),
                                    CanonicalDirection.GetZ(), LightFalloff );
    Source.FaceLightData.Set( ComputePointFaceCoverageCos( Source.ProjectionScale ), Source.ReceiveNearZ, Source.FarZ,
                              ShadowSourceTypePointFace );
    Source.ShadowPriority = ClampShadowPriority( ShadowPriority );
    Source.ShadowScore = ShadowScore;
    Source.DynamicLightIndex = DynamicLightIndex;
    Source.PointLightIndex = m_PointFaceCount / POINT_SHADOW_FACE_COUNT;
    Source.FaceIndex = m_PointFaceCount % POINT_SHADOW_FACE_COUNT;
    m_SourceCount++;
    m_PointFaceCount++;
    m_AtlasSourceCount++;
    m_AtlasLayoutDirty = TRUE;

    if( Source.FaceIndex == 0 )
    {
        m_PointLightCount++;
    }

    return TRUE;
}

//=========================================================================

xbool ShadowMapAtlas::AddSpotSource( matrix4 const& LocalToWorld, radian FieldOfView, f32 LightRadius, f32 LightFalloff,
                                   s32 ShadowMapResolution, s32 ShadowPriority, f32 ShadowScore, s32 DynamicLightIndex )
{
    if( !m_Enabled )
    {
        return FALSE;
    }

    if( m_SourceCount >= MAX_SHADOW_SOURCES )
    {
        return FALSE;
    }

    ShadowSource& Source = m_Sources[m_SourceCount];
    ComputePerspectiveSource( Source, SHADOW_SOURCE_SPOT, LocalToWorld, FieldOfView, LightRadius,
                              LightFalloff, ShadowMapResolution );

    vector3 SpotDirection = LocalToWorld.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if( !SpotDirection.SafeNormalize() )
    {
        SpotDirection.Set( 0.0f, 0.0f, 1.0f );
    }

    Source.FaceLightDirFalloff.Set( SpotDirection.GetX(), SpotDirection.GetY(), SpotDirection.GetZ(), LightFalloff );
    Source.FaceLightData.Set( x_cos( FieldOfView * 0.5f ), Source.ReceiveNearZ, Source.FarZ, ShadowSourceTypeSpot );
    Source.ShadowPriority = ClampShadowPriority( ShadowPriority );
    Source.ShadowScore = ShadowScore;
    Source.DynamicLightIndex = DynamicLightIndex;
    m_SourceCount++;
    m_AtlasSourceCount++;
    m_AtlasLayoutDirty = TRUE;
    return TRUE;
}

//=========================================================================

void ShadowMapAtlas::CreateShadowMap( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/Create" );

    ClearSources();
    if( !m_Enabled )
    {
        return;
    }

    static xprofile_counter ShadowMapDynamicLights =
        x_GetProfiler().RegisterCounter( "ShadowMapDynamicLights", "RenderCounter" );
    static xprofile_counter ShadowMapCandidateLights =
        x_GetProfiler().RegisterCounter( "ShadowMapCandidateLights", "RenderCounter" );
    static xprofile_counter ShadowMapSelectedLights =
        x_GetProfiler().RegisterCounter( "ShadowMapSelectedLights", "RenderCounter" );
    static xprofile_counter ShadowMapPointLights =
        x_GetProfiler().RegisterCounter( "ShadowMapPointLights", "RenderCounter" );
    static xprofile_counter ShadowMapSpotLights =
        x_GetProfiler().RegisterCounter( "ShadowMapSpotLights", "RenderCounter" );
    static xprofile_counter ShadowMapRelevantCasterRefs =
        x_GetProfiler().RegisterCounter( "ShadowMapRelevantCasterRefs", "RenderCounter" );
    static xprofile_counter ShadowMapUniqueCasters =
        x_GetProfiler().RegisterCounter( "ShadowMapUniqueCasters", "RenderCounter" );
    static xprofile_counter ShadowMapObjectCasters =
        x_GetProfiler().RegisterCounter( "ShadowMapObjectCasters", "RenderCounter" );
    static xprofile_counter ShadowMapPlaySurfaceCasters =
        x_GetProfiler().RegisterCounter( "ShadowMapPlaySurfaceCasters", "RenderCounter" );
    static xprofile_counter ShadowMapCasterSourceLinks =
        x_GetProfiler().RegisterCounter( "ShadowMapCasterSourceLinks", "RenderCounter" );
    static xprofile_counter ShadowMapActiveSources =
        x_GetProfiler().RegisterCounter( "ShadowMapActiveSources", "RenderCounter" );
    static xprofile_counter ShadowMapAtlasSources =
        x_GetProfiler().RegisterCounter( "ShadowMapAtlasSources", "RenderCounter" );

    ShadowLight                 ShadowLightCandidates[LightSystem::MAX_SHADOW_LIGHTS];
    ShadowLight                 ShadowLights[LightSystem::MAX_SHADOW_LIGHTS];
    s32                         CandidateShadowLightCount = 0;
    player*                     pActivePlayer = SMP_UTIL_GetActivePlayer();
    ScratchData&                Scratch = GetScratch();
    xarray<ShadowCasterRecord>& RelevantCasters = Scratch.RelevantCasters;
    xarray<ShadowCasterRecord>& ShadowCasters = Scratch.ShadowCasters;
    xarray<u64>&                CasterMasks = Scratch.CasterMasks;
    RelevantCasters.SetCount( 0 );
    ShadowCasters.SetCount( 0 );
    CasterMasks.SetCount( 0 );
    RelevantCasters.SetGrowAmount( 128 );
    ShadowCasters.SetGrowAmount( 128 );
    CasterMasks.SetGrowAmount( 128 );

    s32 const DynamicLightCount = GetRenderPipeline().GetLightSystem().GetNDynamicLights();
    ShadowMapDynamicLights.Add( DynamicLightCount );
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/BuildLightCandidates" );
        for( s32 LightIndex = 0; LightIndex < LightSystem::MAX_SHADOW_LIGHTS; LightIndex++ )
        {
            ShadowLight ShadowLightData;
            if( !BuildShadowLightCandidate( ShadowLightData, LightIndex, pActivePlayer ) )
            {
                continue;
            }

            InsertShadowLight( ShadowLightCandidates, CandidateShadowLightCount, ShadowLightData );
        }
    }
    ShadowMapCandidateLights.Add( CandidateShadowLightCount );
    s32 ShadowLightCount;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/SelectAndPackLights" );
        ShadowLightCount = SelectShadowLights( ShadowLightCandidates, CandidateShadowLightCount, ShadowLights );

        ResolveAtlasBudget( ShadowLights, ShadowLightCount, Scratch.AtlasFreeNodes );
    }

    // Query casters only for selected sources; replace empty selections from the ranked list.
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/BuildSelectedLightCasters" );
        xbool CasterTested[LightSystem::MAX_SHADOW_LIGHTS] = { FALSE };
        xbool HasCasters[LightSystem::MAX_SHADOW_LIGHTS] = { FALSE };
        xbool AtlasRejected[LightSystem::MAX_SHADOW_LIGHTS] = { FALSE };
        s32 LightsWithCastersCount = 0;
        for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
        {
            ShadowLight ShadowLightData = ShadowLights[LightIndex];
            s32 const DynamicLightIndex = ShadowLightData.DynamicLightIndex;
            CasterTested[DynamicLightIndex] = TRUE;
            HasCasters[DynamicLightIndex] = CollectShadowLightCasters( ShadowLightData, RelevantCasters );
            if( !HasCasters[DynamicLightIndex] )
            {
                continue;
            }

            ShadowLights[LightsWithCastersCount++] = ShadowLightData;
        }
        ShadowLightCount = LightsWithCastersCount;

        xbool RemovedByAtlas;
        do
        {
            RestoreShadowLightResolutions( ShadowLights, ShadowLightCount, ShadowLightCandidates,
                                           CandidateShadowLightCount );
            RefillShadowLights( ShadowLightCandidates, CandidateShadowLightCount, ShadowLights, ShadowLightCount,
                                CasterTested, HasCasters, AtlasRejected, RelevantCasters );

            xbool SelectedBeforeResolve[LightSystem::MAX_SHADOW_LIGHTS] = { FALSE };
            for( s32 LightIndex = 0; LightIndex < ShadowLightCount; LightIndex++ )
            {
                SelectedBeforeResolve[ShadowLights[LightIndex].DynamicLightIndex] = TRUE;
            }

            ResolveAtlasBudget( ShadowLights, ShadowLightCount, Scratch.AtlasFreeNodes );
            RemovedByAtlas = FALSE;
            for( s32 CandidateIndex = 0; CandidateIndex < CandidateShadowLightCount; CandidateIndex++ )
            {
                s32 const DynamicLightIndex = ShadowLightCandidates[CandidateIndex].DynamicLightIndex;
                if( SelectedBeforeResolve[DynamicLightIndex] &&
                     !IsSelectedShadowLight( ShadowLights, ShadowLightCount, DynamicLightIndex ) )
                {
                    AtlasRejected[DynamicLightIndex] = TRUE;
                    RemovedByAtlas = TRUE;
                }
            }
        } while( RemovedByAtlas );
    }
    ShadowMapRelevantCasterRefs.Add( RelevantCasters.GetCount() );

    s32 PointLightCount = 0;
    s32 SpotLightCount = 0;
    for( s32 Index = 0; Index < ShadowLightCount; Index++ )
    {
        if( ShadowLights[Index].Shape == LightSystem::LIGHT_SHAPE_OMNI )
        {
            PointLightCount++;
        }
        else
        {
            SpotLightCount++;
        }
    }
    ShadowMapSelectedLights.Add( ShadowLightCount );
    ShadowMapPointLights.Add( PointLightCount );
    ShadowMapSpotLights.Add( SpotLightCount );

    if( ShadowLightCount <= 0 )
    {
        ShadowMapUniqueCasters.Add( 0 );
        ShadowMapObjectCasters.Add( 0 );
        ShadowMapPlaySurfaceCasters.Add( 0 );
        ShadowMapCasterSourceLinks.Add( 0 );
        ShadowMapActiveSources.Add( 0 );
        ShadowMapAtlasSources.Add( 0 );
        return;
    }

    if( eng_Begin( "Shadow Map" ) )
    {
        GetRenderPipeline().GetShadowSubmission().Begin();
        AddShadowSources( ShadowLights, ShadowLightCount );
        FinalizeSources();
        ShadowMapActiveSources.Add( GetSourceCount() );
        ShadowMapAtlasSources.Add( GetAtlasSourceCount() );

        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/CollectCasters" );
            CollectShadowCastersForLights( RelevantCasters, ShadowLights, ShadowLightCount, ShadowCasters, CasterMasks,
                                           Scratch.CasterHash, *this );
        }

        s32 ObjectCasterCount = 0;
        s32 PlaySurfaceCasterCount = 0;
        u64 CasterSourceLinkCount = 0;
        for( s32 Index = 0; Index < ShadowCasters.GetCount(); Index++ )
        {
            if( ShadowCasters[Index].Type == ShadowCasterRecord::CASTER_OBJECT )
            {
                ObjectCasterCount++;
            }
            else
            {
                PlaySurfaceCasterCount++;
            }

            u64 Mask = CasterMasks[Index];
            while( Mask )
            {
                CasterSourceLinkCount += Mask & 1;
                Mask >>= 1;
            }
        }
        ShadowMapUniqueCasters.Add( ShadowCasters.GetCount() );
        ShadowMapObjectCasters.Add( ObjectCasterCount );
        ShadowMapPlaySurfaceCasters.Add( PlaySurfaceCasterCount );
        ShadowMapCasterSourceLinks.Add( static_cast<f64>( CasterSourceLinkCount ) );

        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "ShadowMap/SubmitCasters" );
            for( s32 Index = 0; Index < ShadowCasters.GetCount(); Index++ )
            {
                ShadowCasterRecord const& Caster = ShadowCasters[Index];

                if( Caster.Type == ShadowCasterRecord::CASTER_OBJECT )
                {
                    ASSERT( Caster.pObject );
                    Caster.pObject->OnRenderShadowCast( CasterMasks[Index] );
                }
                else
                {
                    ASSERT( Caster.pPlaySurface );
                    if( !Caster.pPlaySurface->RenderInst.IsNull() )
                    {
                        GetRenderPipeline().GetShadowSubmission().AddRigidCasterSimple( Caster.pPlaySurface->RenderInst, &Caster.pPlaySurface->L2W,
                                                      CasterMasks[Index] );
                    }
                }
            }
        }

        GetRenderPipeline().GetShadowSubmission().End();
        eng_End();
    }
    else
    {
        ShadowMapUniqueCasters.Add( 0 );
        ShadowMapObjectCasters.Add( 0 );
        ShadowMapPlaySurfaceCasters.Add( 0 );
        ShadowMapCasterSourceLinks.Add( 0 );
        ShadowMapActiveSources.Add( 0 );
        ShadowMapAtlasSources.Add( 0 );
    }
}

} // namespace render
