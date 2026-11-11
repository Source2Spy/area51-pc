//=========================================================================
//
//  PlayerAim.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Render/PrimitiveDebug.hpp"
#include "Player.hpp"
#include "Objects/ParticleEmiter.hpp"
#include "Objects/Render/PostEffectMgr.hpp"
#include "Objects/SpawnPoint.hpp"
#include "Objects/Event.hpp"
#include "Sound/EventSoundEmitter.hpp"
#include "TemplateMgr/TemplateMgr.hpp"
#include "Characters/Character.hpp"
#include "GameLib/StatsMgr.hpp"
#include "GameLib/RenderContext.hpp"
#include "Objects/WeaponBBG.hpp"
#include "Objects/WeaponMutation.hpp"
#include "Objects/WeaponSniper.hpp"
#include "Objects/WeaponSMP.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Objects/HudObject.hpp"
#include "Configuration/GameConfig.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

#define AIM_LOGGING_ENABLED 0
#define MAX_AUTO_AIM_DISTANCE 4500.0f

#if !defined( CONFIG_RETAIL ) && !defined( CONFIG_PROFILE )
#define RENDER_AIM_ASSIST 1
#else
#define RENDER_AIM_ASSIST 0
#endif

static const f32    LargeReticleSpeed          = 10.0f;
static const f32    SmallReticleSpeed          = 300.0f;
static const f32    WeaponCollisionRadius      = 5.0f;
static const radian s_AimYawOffsetRateOfChange = R_180;

#if !defined( CONFIG_RETAIL )
extern xbool g_AimAssist_Render_Reticle;
extern xbool g_AimAssist_Render_Bullet;
extern xbool g_AimAssist_Render_Turn;
extern xbool g_AimAssist_Render_Bullet_Angle;
extern xbool g_AimAssist_Render_Player_Pills;
#endif

extern f32 AimAssist_LOF_Dist;
extern f32 AimAssist_Reticle_Near_Dist;
extern f32 AimAssist_Reticle_Far_Dist;
extern f32 AimAssist_Reticle_Near_Radius;
extern f32 AimAssist_Reticle_Far_Radius;
extern f32 AimAssist_Bullet_Inner_Near_Dist;
extern f32 AimAssist_Bullet_Inner_Far_Dist;
extern f32 AimAssist_Bullet_Inner_Near_Radius;
extern f32 AimAssist_Bullet_Inner_Far_Radius;
extern f32 AimAssist_Bullet_Outer_Near_Dist;
extern f32 AimAssist_Bullet_Outer_Far_Dist;
extern f32 AimAssist_Bullet_Outer_Near_Radius;
extern f32 AimAssist_Bullet_Outer_Far_Radius;
extern f32 AimAssist_Bullet_Angle;
extern f32 AimAssist_Turn_Inner_Near_Dist;
extern f32 AimAssist_Turn_Inner_Far_Dist;
extern f32 AimAssist_Turn_Inner_Near_Radius;
extern f32 AimAssist_Turn_Inner_Far_Radius;
extern f32 AimAssist_Turn_Outer_Near_Dist;
extern f32 AimAssist_Turn_Outer_Far_Dist;
extern f32 AimAssist_Turn_Outer_Near_Radius;
extern f32 AimAssist_Turn_Outer_Far_Radius;

#if !defined( CONFIG_RETAIL )

static void RenderPill( const vector3& Top, const vector3& Bot, f32 Radius, xcolor Color )
{
    const view* View = eng_GetView();
    vector3 Axis     = -View->GetViewZ();
    vector3 SpineDir = Top - Bot;
    plane EyePlane;
    EyePlane.Setup( View->GetPosition(), View->GetViewZ() );
    vector3 Parallel;
    vector3 Perpendicular;
    EyePlane.GetComponents( SpineDir, Parallel, Perpendicular );
    SpineDir = Parallel;
    SpineDir.Normalize();
    SpineDir *= Radius;

    const s32 SegmentCount = 16;
    vector3 PreviousTop;
    vector3 CurrentTop;
    vector3 PreviousBottom;
    vector3 CurrentBottom;
    for( s32 i = 0; i <= SegmentCount; i++ )
    {
        PreviousTop    = CurrentTop;
        PreviousBottom = CurrentBottom;
        radian Rotation = (f32)i * (R_180 / (f32)SegmentCount);
        quaternion TopRotation   ( Axis, +Rotation - R_90 );
        quaternion BottomRotation( Axis, -Rotation - R_90 );
        CurrentTop    = (TopRotation * SpineDir) + Top;
        CurrentBottom = (BottomRotation * SpineDir) + Bot;

        if( (i == 0) || (i == SegmentCount) )
        {
            render::debug::Line( CurrentTop, CurrentBottom, Color );
        }

        if( i > 0 )
        {
            render::debug::Line( PreviousTop, CurrentTop, Color );
            render::debug::Line( PreviousBottom, CurrentBottom, Color );
        }
    }
}

#endif

static const f32 s_MinYawModifier         =   0.42f;
static const f32 s_AimAssistDownVelocity  = 100.00f;
static const f32 s_AimAssistUpVelocity    =   1.00f;
static const f32 s_AimAssistPerpSpeedMax  = 300.00f;
static const f32 s_AimAssistBulletScale   =   2.50f;
static const f32 s_AimAssistTurnDampScale =   0.10f;
static const f32 s_AimAssistMPScale       =   1.50f;

//===========================================================================

static inline
f32 ComputeInterpValue( f32 Dist, f32 NearDist, f32 FarDist, f32 NearValue, f32 FarValue )
{
    f32 T = (Dist - NearDist) / (FarDist - NearDist);            
    if( T < 0.0f )
    {
        T = 0.0f;
    }
    if( T > 1.0f )
    {
        T = 1.0f;
    }
    return NearValue + T*(FarValue - NearValue);
}

//=============================================================================

struct aim_target_info
{
    vector3 TargetPos;
    vector3 PerpVelocity;
    vector3 SpinePt;
    vector3 LOFPt;
    f32     TargetDist;
    f32     PerpSpeed;
    f32     BulletAssistPerpSpeedScale;
    f32     TurnDampenPerpSpeedScale;
    f32     LOFPtT;
    f32     SpinePtT;
    f32     LOFPtDist;
    f32     ClampedLOFPtDist;
    f32     LOFSpineDist;
    xbool   bLOFPtBlocked;
};

//=============================================================================

static
xbool BuildAimTargetInfo( actor&         Actor,
                          const vector3& PlayerPosition,
                          const vector3& PlayerVelocity,
                          const vector3& LOFStart,
                          const vector3& LOFEnd,
                          const vector3& LOFDir,
                          f32            LOFCollisionDist,
                          f32            TargetCullDot,
                          xbool          bUsePreciseSpine,
                          aim_target_info& Info )
{
    Info.TargetPos = Actor.GetPosition();

    vector3 TargetDelta = Info.TargetPos - PlayerPosition;
    Info.TargetDist     = TargetDelta.Length();

    if( LOFDir.Dot( TargetDelta ) <= 0.0f )
        return FALSE;

    vector3 TargetDeltaDir = TargetDelta;
    TargetDeltaDir.Normalize();
    if( Info.TargetDist > 1000.0f )
    {
        if( LOFDir.Dot( TargetDeltaDir ) <= TargetCullDot )
            return FALSE;
    }

    vector3 TargetVelocity( 0, 0, 0 );
    loco* pLoco = Actor.GetLocoPointer();
    if( pLoco )
    {
        TargetVelocity = pLoco->m_Physics.GetContactVelocity();
    }

    vector3 RelativeVelocity = TargetVelocity - PlayerVelocity;
    Info.PerpVelocity        = RelativeVelocity - ( LOFDir.Dot( RelativeVelocity ) * LOFDir );
    Info.PerpSpeed           = Info.PerpVelocity.Length();

    f32 PerpSpeedT = Info.PerpSpeed / s_AimAssistPerpSpeedMax;
    if( PerpSpeedT > 1 ) PerpSpeedT = 1;

    Info.BulletAssistPerpSpeedScale = 1.0f + PerpSpeedT * ( s_AimAssistBulletScale - 1.0f );
    Info.TurnDampenPerpSpeedScale   = 1.0f + PerpSpeedT * ( s_AimAssistTurnDampScale - 1.0f );

    vector3 SpineTop;
    vector3 SpineBot;
    if( bUsePreciseSpine )
    {
        Actor.GetHeadAndRootPosition( SpineTop, SpineBot );
        SpineTop += vector3( 0, 20.0f, 0 );
    }
    else
    {
        SpineBot = Actor.GetPosition();
        SpineTop = SpineBot + vector3( 0, 150, 0 );
    }

    vector3 SpineLOFOffset;
    x_ClosestPtsOnLineSegs( LOFStart,
                            LOFEnd,
                            SpineTop,
                            SpineBot,
                            Info.LOFPt,
                            Info.SpinePt,
                            Info.LOFPtT,
                            Info.SpinePtT );

    Info.LOFPtDist        = AimAssist_LOF_Dist * Info.LOFPtT;
    Info.ClampedLOFPtDist = x_clamp( Info.LOFPtDist, 1.0f, F32_MAX );
    SpineLOFOffset        = Info.SpinePt - Info.LOFPt;
    Info.LOFSpineDist     = SpineLOFOffset.Length();
    Info.bLOFPtBlocked    = ( Info.LOFPtDist > LOFCollisionDist );

    return TRUE;
}

//=============================================================================

static
void StoreAimTargetInfo( AimAssistData& AimData, const aim_target_info& Info )
{
    AimData.LOFSpineDist = Info.LOFSpineDist;
    AimData.SpinePt      = Info.SpinePt;
    AimData.SpinePtT     = Info.SpinePtT;
    AimData.LOFPt        = Info.LOFPt;
    AimData.LOFPtT       = Info.LOFPtT;
    AimData.LOFPtDist    = Info.ClampedLOFPtDist;
}

//=============================================================================

static
void UpdateEnemyReticleTarget( AimAssistData&        AimData,
                               actor&                Actor,
                               const aim_target_info& Info,
                               f32&                  ReticleBestDist,
                               xbool&                bReticleOn )
{
    if( Info.bLOFPtBlocked )
        return;

    f32 Radius = ComputeInterpValue( Info.TargetDist,
                                     AimAssist_Reticle_Near_Dist,
                                     AimAssist_Reticle_Far_Dist,
                                     AimAssist_Reticle_Near_Radius * Info.BulletAssistPerpSpeedScale,
                                     AimAssist_Reticle_Far_Radius  * Info.BulletAssistPerpSpeedScale );

    AimData.ReticleRadius = Radius;

    if( (Info.LOFSpineDist <= Radius) && (Info.LOFSpineDist < ReticleBestDist) )
    {
        ReticleBestDist            = Info.LOFSpineDist;
        bReticleOn                 = TRUE;
        AimData.ReticleEnemyGuid   = Actor.GetGuid();
        StoreAimTargetInfo( AimData, Info );
    }
}

//=============================================================================

static
xbool UpdateFriendlyReticleTarget( AimAssistData&        AimData,
                                   actor&                Actor,
                                   const aim_target_info& Info,
                                   xbool&                bReticleOn )
{
    if( Info.bLOFPtBlocked )
        return FALSE;

    f32 Radius = ComputeInterpValue( Info.TargetDist,
                                     AimAssist_Reticle_Near_Dist,
                                     AimAssist_Reticle_Far_Dist,
                                     AimAssist_Reticle_Near_Radius,
                                     AimAssist_Reticle_Far_Radius );

    if( Info.LOFSpineDist <= Radius )
    {
        bReticleOn                         = TRUE;
        AimData.OnlineFriendlyTargetGuid   = Actor.GetGuid();
        StoreAimTargetInfo( AimData, Info );
        return TRUE;
    }

    return FALSE;
}

//=============================================================================

static
void UpdateBulletAssistTarget( AimAssistData&        AimData,
                               player&               Player,
                               actor&                Actor,
                               const view&           View,
                               const vector3&        LOFStart,
                               const vector3&        LOFDir,
                               f32                   MultiplayerRadiiScale,
                               f32                   BulletAssistLeadSpeed,
                               const aim_target_info& Info )
{
    if( Info.bLOFPtBlocked || (Info.LOFSpineDist >= AimData.BulletAssistBestDist) )
        return;

    f32 InnerRadius = ComputeInterpValue( Info.TargetDist,
                                          AimAssist_Bullet_Inner_Near_Dist,
                                          AimAssist_Bullet_Inner_Far_Dist,
                                          AimAssist_Bullet_Inner_Near_Radius * Info.BulletAssistPerpSpeedScale * MultiplayerRadiiScale,
                                          AimAssist_Bullet_Inner_Far_Radius  * Info.BulletAssistPerpSpeedScale * MultiplayerRadiiScale );

    f32 OuterRadius = ComputeInterpValue( Info.TargetDist,
                                          AimAssist_Bullet_Outer_Near_Dist,
                                          AimAssist_Bullet_Outer_Far_Dist,
                                          AimAssist_Bullet_Outer_Near_Radius * Info.BulletAssistPerpSpeedScale * MultiplayerRadiiScale,
                                          AimAssist_Bullet_Outer_Far_Radius  * Info.BulletAssistPerpSpeedScale * MultiplayerRadiiScale );

    AimData.BulletInnerRadius = InnerRadius;
    AimData.BulletOuterRadius = OuterRadius;

    f32 T = x_parametric( Info.LOFSpineDist, OuterRadius, InnerRadius, TRUE );
    if( T <= 0 )
        return;

    radian AssistAngle = T * AimAssist_Bullet_Angle;
    vector3 AimAtPoint = Info.SpinePt;

    if( (BulletAssistLeadSpeed > 0) && (Info.PerpSpeed > 0.0f) )
    {
        f32 FlightTime = Info.TargetDist / BulletAssistLeadSpeed;
        f32 LeadDist   = Info.PerpSpeed * FlightTime;
        if( LeadDist > 100.0f ) LeadDist = 100.0f;
        AimAtPoint += Info.PerpVelocity * ( LeadDist / Info.PerpSpeed );
    }

    vector3 ToSpine = AimAtPoint - View.GetPosition();
    radian Angle = v3_AngleBetween( LOFDir, ToSpine );
    if( Angle > AssistAngle ) Angle = AssistAngle;

    vector3 Axis = LOFDir.Cross( ToSpine );
    Axis.Normalize();
    quaternion Q( Axis, Angle );

    vector3 NewBulletAssistDir = Q * LOFDir;

    s32 nSegs = 4;
    for( s32 i = 0; i < nSegs; i++ )
    {
        f32 SegT = (f32)i / (f32)(nSegs - 1);
        vector3 Dir = NewBulletAssistDir + SegT * ( LOFDir - NewBulletAssistDir );
        Dir.Normalize();

        vector3 EndPos = LOFStart + Dir * Info.ClampedLOFPtDist;
        g_CollisionMgr.LineOfSightSetup( Player.GetGuid(), LOFStart, EndPos );
        g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES,
                                        object::ATTR_BLOCKS_PLAYER_LOS,
                                        object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );

        if( g_CollisionMgr.m_nCollisions == 0 )
        {
            AimData.BulletAssistBestDist = Info.LOFSpineDist;
            AimData.TargetGuid           = Actor.GetGuid();
            AimData.BulletAssistDir      = Dir;

            vector3 AbsoluteDelta = Info.LOFPt - Actor.GetPosition();
            AimData.AimDelta      = AbsoluteDelta.Length() *
                                    vector3( AbsoluteDelta.GetPitch(),
                                             AbsoluteDelta.GetYaw() - Actor.GetYaw() );

            StoreAimTargetInfo( AimData, Info );
            break;
        }
    }
}

//=============================================================================

static
void UpdateTurnAssistTarget( AimAssistData& AimData, const aim_target_info& Info )
{
    if( Info.bLOFPtBlocked )
        return;

    f32 InnerRadius = ComputeInterpValue( Info.TargetDist,
                                          AimAssist_Turn_Inner_Near_Dist,
                                          AimAssist_Turn_Inner_Far_Dist,
                                          AimAssist_Turn_Inner_Near_Radius,
                                          AimAssist_Turn_Inner_Far_Radius );

    f32 OuterRadius = ComputeInterpValue( Info.TargetDist,
                                          AimAssist_Turn_Outer_Near_Dist,
                                          AimAssist_Turn_Outer_Far_Dist,
                                          AimAssist_Turn_Outer_Near_Radius,
                                          AimAssist_Turn_Outer_Far_Radius );

    AimData.TurnInnerRadius = InnerRadius;
    AimData.TurnOuterRadius = OuterRadius;

    f32 T = x_parametric( Info.LOFSpineDist, OuterRadius, InnerRadius, TRUE );
    T *= Info.TurnDampenPerpSpeedScale;

    if( T > AimData.TurnDampeningT )
    {
        AimData.TurnDampeningT = T;
        StoreAimTargetInfo( AimData, Info );
    }
}

// structure constructor
AimAssistData::AimAssistData( void )
{
    BulletAssistDir         = vector3(0.0f, 0.0f, 0.0f);
    bReticleOn              = FALSE;
    BulletAssistBestDist    = F32_MAX;
    TurnDampeningT          = 0.0f;
    TargetGuid              = 0;
    ReticleEnemyGuid        = 0;
    LOFCollisionDist        = 2500.0f;
    LOFSpineDist            = 0.0f;
    SpinePt                 = vector3(0.0f, 0.0f, 0.0f);
    LOFPt                   = vector3(0.0f, 0.0f, 0.0f);
    LOFPtT                  = 0.0f;
    SpinePtT                = 0.0f;
    LOFPtDist               = 1.0f;

    ReticleRadius           = 0.0f;    
    BulletInnerRadius       = 0.0f;
    BulletOuterRadius       = 0.0f;
    TurnInnerRadius         = 0.0f;
    TurnOuterRadius         = 0.0f;  

    // online stuff
    OnlineFriendlyTargetGuid= 0;
    AimDelta                = vector3(0.0f, 0.0f, 0.0f);   
}

// tweak values
f32 AimAssist_LOF_Dist;

f32 AimAssist_Reticle_Near_Dist;
f32 AimAssist_Reticle_Far_Dist;
f32 AimAssist_Reticle_Near_Radius;
f32 AimAssist_Reticle_Far_Radius;

f32 AimAssist_Bullet_Inner_Near_Dist;
f32 AimAssist_Bullet_Inner_Far_Dist;
f32 AimAssist_Bullet_Inner_Near_Radius;
f32 AimAssist_Bullet_Inner_Far_Radius;

f32 AimAssist_Bullet_Outer_Near_Dist;
f32 AimAssist_Bullet_Outer_Far_Dist;
f32 AimAssist_Bullet_Outer_Near_Radius;
f32 AimAssist_Bullet_Outer_Far_Radius;

f32 AimAssist_Bullet_Angle;

f32 AimAssist_Turn_Inner_Near_Dist;
f32 AimAssist_Turn_Inner_Far_Dist;
f32 AimAssist_Turn_Inner_Near_Radius;
f32 AimAssist_Turn_Inner_Far_Radius;

f32 AimAssist_Turn_Outer_Near_Dist;
f32 AimAssist_Turn_Outer_Far_Dist;
f32 AimAssist_Turn_Outer_Near_Radius;
f32 AimAssist_Turn_Outer_Far_Radius;

f32 AimAssist_Turn_Damp_Near_Dist;
f32 AimAssist_Turn_Damp_Far_Dist;


// Tweak handles
tweak_handle AimAssist_LOF_Dist_Tweak;

tweak_handle AimAssist_Reticle_Near_Dist_Tweak ;
tweak_handle AimAssist_Reticle_Far_Dist_Tweak;
tweak_handle AimAssist_Reticle_Near_Radius_Tweak;
tweak_handle AimAssist_Reticle_Far_Radius_Tweak;

tweak_handle AimAssist_Bullet_Inner_Near_Dist_Tweak;
tweak_handle AimAssist_Bullet_Inner_Far_Dist_Tweak;
tweak_handle AimAssist_Bullet_Inner_Near_Radius_Tweak;
tweak_handle AimAssist_Bullet_Inner_Far_Radius_Tweak;

tweak_handle AimAssist_Bullet_Outer_Near_Dist_Tweak;
tweak_handle AimAssist_Bullet_Outer_Far_Dist_Tweak;
tweak_handle AimAssist_Bullet_Outer_Near_Radius_Tweak;
tweak_handle AimAssist_Bullet_Outer_Far_Radius_Tweak;

tweak_handle AimAssist_Bullet_Angle_Tweak;

tweak_handle AimAssist_Turn_Inner_Near_Dist_Tweak;
tweak_handle AimAssist_Turn_Inner_Far_Dist_Tweak;
tweak_handle AimAssist_Turn_Inner_Near_Radius_Tweak;
tweak_handle AimAssist_Turn_Inner_Far_Radius_Tweak;

tweak_handle AimAssist_Turn_Outer_Near_Dist_Tweak;
tweak_handle AimAssist_Turn_Outer_Far_Dist_Tweak;
tweak_handle AimAssist_Turn_Outer_Near_Radius_Tweak;
tweak_handle AimAssist_Turn_Outer_Far_Radius_Tweak;

tweak_handle AimAssist_Turn_Damp_Near_Dist_Tweak;
tweak_handle AimAssist_Turn_Damp_Far_Dist_Tweak;

//=========================================================================
void player::RenderAimAssistDebugInfo( void )
{
#if( RENDER_AIM_ASSIST )

    // Render AimAssist
    const view* View = eng_GetView();

    // technically this extra if doesn't need to be here, but the "GetHeadAndRootPosition"
    // call can be UBER-expensive, and we should just avoid it completely if at all possible
    if( g_AimAssist_Render_Reticle || g_AimAssist_Render_Bullet || g_AimAssist_Render_Turn )
    {
        actor* pActor = actor::m_pFirstActive;
        while( pActor )
        {
            // only draw player pills if requested
            if( g_AimAssist_Render_Player_Pills || (pActor->GetGuid() != GetGuid()) )
            {
                //
                // Get spine information
                //
                vector3 SpineTop;
                vector3 SpineBot;
                {
                    pActor->GetHeadAndRootPosition( SpineTop, SpineBot );
                }

                //
                // Get closest pt between LOF and Spine
                //
                // m_AimAssistData.LOFSpineDist;
                //render::debug::Marker( m_AimAssistData.SpinePt, XCOLOR_WHITE );
                //render::debug::Marker( m_AimAssistData.LOFPt, XCOLOR_WHITE );
                if( g_AimAssist_Render_Reticle )
                {
                    f32 Radius = m_AimAssistData.ReticleRadius;
                    RenderPill( SpineTop, SpineBot, Radius, XCOLOR_RED );
                }

                if( g_AimAssist_Render_Bullet )
                {                        
                    f32 InnerRadius = m_AimAssistData.BulletInnerRadius;
                    f32 OuterRadius = m_AimAssistData.BulletOuterRadius;

                    RenderPill( SpineTop, SpineBot, InnerRadius, XCOLOR_BLUE );
                    RenderPill( SpineTop, SpineBot, OuterRadius, XCOLOR_BLUE );
                }

                if( g_AimAssist_Render_Turn )
                {
                    f32 InnerRadius = m_AimAssistData.TurnInnerRadius;
                    f32 OuterRadius = m_AimAssistData.TurnOuterRadius;

                    RenderPill( SpineTop, SpineBot, InnerRadius, XCOLOR_GREEN );
                    RenderPill( SpineTop, SpineBot, OuterRadius, XCOLOR_GREEN );
                }
            }

            pActor = pActor->m_pNextActive;
        }
    }

    if( g_AimAssist_Render_Bullet )
    {
        vector3 Pos;
        Pos = View->GetPosition() + View->GetViewZ()*150.0f;
        render::debug::Marker(Pos, XCOLOR_WHITE);
        Pos = View->GetPosition() + m_AimAssistData.BulletAssistDir*150.0f;
        render::debug::Marker(Pos, XCOLOR_WHITE);
    }

    if( g_AimAssist_Render_Bullet_Angle )
    {
        quaternion Q( View->GetViewY(), -AimAssist_Bullet_Angle );
        vector3 Dir = Q * View->GetViewZ() * 200.0f;
        s32 nSegs=32;
        s32 i;
        vector3 TP0,TP1;
        for( i=0; i<=nSegs; i++ )
        {
            TP0 = TP1;
            radian R = (f32)i * (R_360 / (f32)nSegs);
            quaternion Q(View->GetViewZ(),R);
            TP1 = (Q * Dir) + View->GetPosition();
            if( i>0 )
            {
                render::debug::Line(TP0,TP1,XCOLOR_YELLOW);
            }
        }
    }

#endif // #if( RENDER_AIM_ASSIST )
}


//=========================================================================
vector3 player::GetWeaponCollisionOffset( guid WeaponGuid, const vector3& FirePos )
{
    const vector3 Dir      ( m_Pitch, m_Yaw );

    //
    // Come up with the point to use for our collision start.
    // It will be along the gun's aim axis
    //
    static const f32 Dist = 100.0f;
    vector3 WeaponStalk( FirePos - (Dir * Dist) );
    vector3 PtOnA;
    vector3 PtOnB;
    x_ClosestPtsOnLineSegs( WeaponStalk, FirePos, GetPosition(), GetPosition() + vector3( 0.0f, 200.0f, 0.0f ), PtOnA, PtOnB );

    vector3 Start( PtOnA );
    vector3 End( FirePos - (Dir * WeaponCollisionRadius) ); // back off by our radius

    g_CollisionMgr.SphereSetup( WeaponGuid, Start, End, WeaponCollisionRadius );   
    g_CollisionMgr.UseLowPoly();
    g_CollisionMgr.SetMaxCollisions( 1 );
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_BLOCKS_PLAYER, object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );

    // If we hit anything, figure out where we want to be
    f32 DesiredCollisionOffsetScalar = 0.0f;
    if ( g_CollisionMgr.m_nCollisions > 0 )
    {
        DesiredCollisionOffsetScalar = (Start - End).Length();
        DesiredCollisionOffsetScalar *= (1.01f - g_CollisionMgr.m_Collisions[0].T);
    }

    // Move towards our goal
    static const f32 s_WeaponSlideSpeed = 150.0f;

    f32 Delta = DesiredCollisionOffsetScalar - m_WeaponCollisionOffsetScalar;
    if ( x_abs( Delta ) > 0.01f )
    {
        if ( m_LastWeaponCollisionOffsetScalar > DesiredCollisionOffsetScalar )
        {
            // We're moving the weapon forward, towards neutral
            // move this direction smoothly
            const f32 Distance = s_WeaponSlideSpeed * m_DeltaTime;
            if ( x_abs( Delta ) < Distance )
            {
                // we'd overshoot it, so just go there
                m_WeaponCollisionOffsetScalar = DesiredCollisionOffsetScalar;
            }
            else
            {
                if ( DesiredCollisionOffsetScalar > m_WeaponCollisionOffsetScalar )
                {
                    Delta = Distance;
                }
                else
                {
                    Delta = -Distance;
                }

                m_WeaponCollisionOffsetScalar += Delta;
            }
        }
        else
        {
            // We're moving the weapon back out of a wall, do this instantly
            m_WeaponCollisionOffsetScalar = DesiredCollisionOffsetScalar;
        }
    }
    else
    {
        m_WeaponCollisionOffsetScalar = DesiredCollisionOffsetScalar;
    }

    vector3 Pullback( -Dir );
    Pullback *= m_WeaponCollisionOffsetScalar;
    return Pullback;
}

//=========================================================================
void player::UpdateReticleRadius( f32 DeltaTime )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();

    if( !pWeapon )
        return;

    new_weapon::reticle_radius_parameters ReticleParams = GetReticleParams();   

    ASSERT( SmallReticleSpeed > LargeReticleSpeed );
    ASSERT( ReticleParams.m_MaxRadius > ReticleParams.m_MinRadius );

    //
    // First, figure out what the movement penalty is
    //
    f32 Speed = m_Physics.GetMotionVelocity().Length();
    f32 DesiredRadius = ReticleParams.m_MaxRadius; // we'll apply penalties for movement and shooting to this

    if ( Speed >= SmallReticleSpeed )
    {
        DesiredRadius -= ReticleParams.m_MaxMovementPenalty;
    }
    else if ( Speed <= LargeReticleSpeed )
    {
        DesiredRadius = ReticleParams.m_MaxRadius; // no penalty from movement
    }
    else
    {
        const f32 RelativeSpeed     = Speed - LargeReticleSpeed;                // Speed relative to reticle speed range
        const f32 SpeedRange        = SmallReticleSpeed - LargeReticleSpeed;

        ASSERT( RelativeSpeed > 0.0f );
        ASSERT( RelativeSpeed <= SpeedRange );

        f32 Penalty = MIN( ReticleParams.m_MaxMovementPenalty, ((RelativeSpeed / SpeedRange) * ReticleParams.m_MaxMovementPenalty) );
        DesiredRadius = ReticleParams.m_MaxRadius - Penalty;
    }


    //
    // Next, figure out what the shooting penalty is
    //

    // Degrade shot penalty
    m_ReticleShotPenalty -= ReticleParams.m_ShotPenaltyDegradeRate * DeltaTime;
    m_ReticleShotPenalty = MAX( 0.0f, m_ReticleShotPenalty );
    DesiredRadius -= m_ReticleShotPenalty;
    DesiredRadius = MAX( ReticleParams.m_MinRadius, DesiredRadius );

    //
    // Add in the crouch bonus as needed
    //
    if ( m_bIsCrouching )
    {
        DesiredRadius += ReticleParams.m_CrouchBonus;
    }


    //
    // Now update radius speed and current radius
    //
    if ( m_ReticleRadius > DesiredRadius)
    {
        if ( m_ReticleGrowSpeed > 0.0f )
        {
            // if growing, stop
            m_ReticleGrowSpeed = 0.0f;
        }

        // we need to shrink
        const f32 ShrinkAccel = (m_ReticleShotPenalty > 0.0f) ? ReticleParams.m_ShotShrinkAccel : ReticleParams.m_MoveShrinkAccel;
        m_ReticleGrowSpeed -= (ShrinkAccel * DeltaTime);
    }
    else if ( m_ReticleRadius < DesiredRadius )
    {
        if ( m_ReticleGrowSpeed < 0.0f )
        {
            // if shrinking, stop
            m_ReticleGrowSpeed = 0.0f;
        }

        // we need to grow
        m_ReticleGrowSpeed += (ReticleParams.m_GrowAccel * DeltaTime);
    }

    f32 GrowAmount = m_ReticleGrowSpeed * DeltaTime;
    f32 NewRadius = m_ReticleRadius + GrowAmount;

    if (   ((m_ReticleRadius < DesiredRadius) && (NewRadius > DesiredRadius))
        || ((m_ReticleRadius > DesiredRadius) && (NewRadius < DesiredRadius)) )
    {
        // we overshot our goal, just end up there
        NewRadius = DesiredRadius;
        m_ReticleGrowSpeed = 0.0f;
    }

    m_ReticleRadius = NewRadius;

    // Clamp
    const f32 Max = ReticleParams.m_MaxRadius + ReticleParams.m_CrouchBonus;
    if ( m_ReticleRadius > Max )
    { 
        m_ReticleRadius = Max;
        m_ReticleGrowSpeed = 0.0f;
    }
    else if ( (m_ReticleRadius < ReticleParams.m_MinRadius) && (GrowAmount < 0.0f) )
    {
        m_ReticleRadius = ReticleParams.m_MinRadius;
        m_ReticleGrowSpeed = 0.0f;
    }
}

//=========================================================================
new_weapon::reticle_radius_parameters player::GetReticleParams( void )
{
    new_weapon* pWeapon = GetCurrentWeaponPtr();

    if( pWeapon )
    {
        if (m_CurrentAnimState == ANIM_STATE_ZOOM_IDLE ||
            m_CurrentAnimState == ANIM_STATE_ZOOM_RUN ||
            m_CurrentAnimState == ANIM_STATE_ZOOM_FIRE)
        {
            //alt fire
            return pWeapon->GetAltReticleRadiusParameters();
        }
        else
        {
            //standard fire
            return pWeapon->GetReticleRadiusParameters();
        }
    }
    else
    {
        new_weapon::reticle_radius_parameters Params;
        Params.m_CrouchBonus            = 0.0f;
        Params.m_GrowAccel              = 0.0f;
        Params.m_MaxMovementPenalty     = 0.0f;
        Params.m_MaxRadius              = 0.0f;
        Params.m_MoveShrinkAccel        = 0.0f;
        Params.m_PenaltyForShot         = 0.0f;
        Params.m_ShotPenaltyDegradeRate = 0.0f;
        Params.m_ShotShrinkAccel        = 0.0f;

        return Params;
    }
}

//=========================================================================
radian player::GetSightYaw( void ) const
{
    return m_EyesYaw;
}

//=========================================================================
radian player::CalculateActualPitchToTarget( object* pObject )
{
    vector3 vViewToBoxCenter = pObject->GetColBBox().GetCenter() - m_EyesPosition;

    // Calculate the differences in pitch
    radian PitchBox;
    PitchBox = vViewToBoxCenter.GetPitch();

    return x_MinAngleDiff( PitchBox, m_EyesPitch );
}

//=========================================================================
radian player::CalculateActualYawToTarget( object* pObject )
{
    vector3 vViewToBoxCenter = pObject->GetColBBox().GetCenter() - m_EyesPosition;

    // Calculate the differences in pitch
    radian YawBox;//, WorldViewYaw;
    YawBox = vViewToBoxCenter.GetYaw();

    return x_MinAngleDiff( YawBox, m_EyesYaw );
}

//=========================================================================
radian  player::CalculateNecessaryAimAssistPitch( object* pObject )
{
    // Get the vector from the view to the center of the object's bbox.
    vector3 vViewToBoxCenter = pObject->GetColBBox().GetCenter() - m_EyesPosition;

    // Calculate the length and width that I'm going to need.
    // We may need to optimize this, because it's expensive.
    // ALSO: need to check vs. width here, not just radius.
    f32 fToCenterLength = vViewToBoxCenter.Length();
    f32 fBoxHeight = pObject->GetColBBox().Max.GetY() - pObject->GetColBBox().Min.GetY();

    radian HalfAngle = x_atan( fBoxHeight / fToCenterLength );

    return HalfAngle;
}

//=========================================================================
radian player::CalculateNecessaryAimAssistYaw( object* pObject )
{
    // Get the vector from the view to the center of the object's bbox.
    vector3 vViewToBoxCenter = pObject->GetColBBox().GetCenter() - m_EyesPosition;

    // Calculate the length and width that I'm going to need.
    // We may need to optimize this, because it's expensive.
    // ALSO: need to check vs. width here, not just radius.
    f32 fToCenterLength = vViewToBoxCenter.Length();
    //    f32 fBoxWidth = pObject->GetColBBox().GetRadius();
    f32 fBoxWidth = pObject->GetColBBox().Max.GetX() - pObject->GetColBBox().Min.GetX();

    radian HalfAngle = x_atan( fBoxWidth / fToCenterLength );

    return HalfAngle;
}

//=========================================================================
void player::UpdateCurrentAimTarget( f32 DeltaTime )
{
    const view& View              = GetSimulationView();
    const vector3 Position        = GetPosition();
    const vector3 PlayerVelocity  = m_Physics.GetContactVelocity();
    const f32 TargetCullDot       = x_cos( R_20 );
    const xbool bAimAssistActive  = IsAimAssistInputActive();
    (void)DeltaTime;

    m_AimAssistData.BulletAssistDir          = View.GetViewZ();
    m_AimAssistData.BulletAssistBestDist     = F32_MAX;
    m_AimAssistData.TurnDampeningT           = 0.0f;
    m_AimAssistData.TargetGuid               = 0;

    if( !bAimAssistActive )
    {
        return;
    }

    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon == NULL )
    {
        return;
    }

    if( AimAssist_LOF_Dist == 0.0f )
    {
        return;
    }

    f32 MultiplayerRadiiScale = 1.0f;
#ifndef X_EDITOR
    if( GameMgr.IsGameMultiplayer() )
    {
        MultiplayerRadiiScale = s_AimAssistMPScale;
    }
#endif

    f32 BulletAssistLeadSpeed = 0.0f;
    if( bAimAssistActive && pWeapon->IsKindOf( weapon_bbg::GetRTTI() ) )
    {
        tweak_handle SpeedTweak( xfs( "%s_SPEED", pWeapon->GetLogicalName() ) );
        BulletAssistLeadSpeed = SpeedTweak.GetF32();
    }

    vector3 LOFDir   = View.GetViewZ();
    vector3 LOFStart = View.GetPosition();
    vector3 LOFEnd   = LOFStart + LOFDir * AimAssist_LOF_Dist;

    m_AimAssistData.LOFCollisionDist = AimAssist_LOF_Dist;

    g_CollisionMgr.LineOfSightSetup( GetGuid(), LOFStart, LOFEnd );
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES,
                                    object::ATTR_BLOCKS_PLAYER_LOS,
                                    object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );

    if( g_CollisionMgr.m_nCollisions > 0 )
    {
        m_AimAssistData.LOFCollisionDist = AimAssist_LOF_Dist * g_CollisionMgr.m_Collisions[0].T;
    }

    actor* pNextActor = actor::m_pFirstActive;
    while( pNextActor )
    {
        actor* pActor = pNextActor;
        pNextActor = pNextActor->m_pNextActive;

        if( (pActor->GetGuid() == GetGuid()) ||
            !pActor->IsKindOf( actor::GetRTTI() ) ||
            pActor->IsDead() )
        {
            continue;
        }

        if( IsEnemyFaction( pActor->GetFaction() ) )
        {
            aim_target_info Info;
            if( !BuildAimTargetInfo( *pActor,
                                     Position,
                                     PlayerVelocity,
                                     LOFStart,
                                     LOFEnd,
                                     LOFDir,
                                     m_AimAssistData.LOFCollisionDist,
                                     TargetCullDot,
                                     TRUE,
                                     Info ) )
            {
                continue;
            }

            UpdateBulletAssistTarget( m_AimAssistData,
                                      *this,
                                      *pActor,
                                      View,
                                      LOFStart,
                                      LOFDir,
                                      MultiplayerRadiiScale,
                                      BulletAssistLeadSpeed,
                                      Info );

            UpdateTurnAssistTarget( m_AimAssistData, Info );
        }
    }
}

//=========================================================================
void player::UpdateReticleTarget( f32 DeltaTime )
{
    const view& View              = GetSimulationView();
    const vector3 Position        = GetPosition();
    const vector3 PlayerVelocity  = m_Physics.GetContactVelocity();
    const f32 TargetCullDot       = x_cos( R_20 );
    xbool bReticleOn              = FALSE;
    f32 ReticleBestDist           = F32_MAX;

    m_AimAssistData.ReticleEnemyGuid         = 0;
    m_AimAssistData.OnlineFriendlyTargetGuid = 0;
    m_AimAssistData.bReticleOn               = FALSE;

    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon == NULL )
    {
        UpdateReticleRadius( DeltaTime );
        return;
    }

    if( AimAssist_LOF_Dist == 0.0f )
    {
        UpdateReticleRadius( DeltaTime );
        return;
    }

    vector3 LOFDir   = View.GetViewZ();
    vector3 LOFStart = View.GetPosition();
    vector3 LOFEnd   = LOFStart + LOFDir * AimAssist_LOF_Dist;

    m_AimAssistData.LOFCollisionDist = AimAssist_LOF_Dist;

    g_CollisionMgr.LineOfSightSetup( GetGuid(), LOFStart, LOFEnd );
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES,
                                    object::ATTR_BLOCKS_PLAYER_LOS,
                                    object::ATTR_COLLISION_PERMEABLE | object::ATTR_LIVING );

    if( g_CollisionMgr.m_nCollisions > 0 )
    {
        m_AimAssistData.LOFCollisionDist = AimAssist_LOF_Dist * g_CollisionMgr.m_Collisions[0].T;
    }

    actor* pNextActor = actor::m_pFirstActive;
    while( pNextActor )
    {
        actor* pActor = pNextActor;
        pNextActor = pNextActor->m_pNextActive;

        if( (pActor->GetGuid() == GetGuid()) ||
            !pActor->IsKindOf( actor::GetRTTI() ) ||
            pActor->IsDead() )
        {
            continue;
        }

        if( IsEnemyFaction( pActor->GetFaction() ) )
        {
            aim_target_info Info;
            if( !BuildAimTargetInfo( *pActor,
                                     Position,
                                     PlayerVelocity,
                                     LOFStart,
                                     LOFEnd,
                                     LOFDir,
                                     m_AimAssistData.LOFCollisionDist,
                                     TargetCullDot,
                                     TRUE,
                                     Info ) )
            {
                continue;
            }

            UpdateEnemyReticleTarget( m_AimAssistData, *pActor, Info, ReticleBestDist, bReticleOn );
        }
        else
        {
            aim_target_info Info;
            if( !BuildAimTargetInfo( *pActor,
                                     Position,
                                     PlayerVelocity,
                                     LOFStart,
                                     LOFEnd,
                                     LOFDir,
                                     m_AimAssistData.LOFCollisionDist,
                                     TargetCullDot,
                                     FALSE,
                                     Info ) )
            {
                continue;
            }

            UpdateFriendlyReticleTarget( m_AimAssistData, *pActor, Info, bReticleOn );
        }
    }

    m_AimAssistData.bReticleOn = bReticleOn;

    UpdateReticleRadius( DeltaTime );
}

//=========================================================================
void player::UpdateAimAssistance( f32 DeltaTime )
{
    // find and set the targeted guid.
    UpdateCurrentAimTarget( DeltaTime );

    if( IsAimAssistInputActive() )
    {
        ApplyAimAssistTurnDampening();
    }
    else
    {
        m_AimAssistData.TurnDampeningT = 0.0f;
    }

    s32     LastNetSlot   = m_TargetNetSlot;
    vector3 LastAimOffset = m_AimOffset;

    m_TargetNetSlot = -1;

    if ( m_AimAssistData.TargetGuid != 0 )
    {
        m_fCurrentYawAimModifier = MAX( m_fCurrentYawAimModifier - s_AimAssistDownVelocity * DeltaTime,
                                        s_MinYawModifier );
        m_fCurrentPitchAimModifier = 0.5f;

        object* pObject = g_ObjMgr.GetObjectByGuid( m_AimAssistData.TargetGuid );

        // Update the aim offset if we're aiming at another human player.
        if( pObject )
        {  
            if( pObject->IsKindOf( actor::GetRTTI() ) )
            {
                actor& Actor = actor::GetSafeType( *pObject ); 
                s32 NetSlot = Actor.net_GetSlot();
                if( IN_RANGE( 0, NetSlot, 31 ) && !Actor.IsDead() )
                {
                    m_TargetNetSlot = NetSlot;
                    m_AimOffset     = m_AimAssistData.AimDelta;
                }
            }
        }
    }
    else
    {
        m_fCurrentYawAimModifier = MIN( m_fCurrentYawAimModifier + s_AimAssistUpVelocity * DeltaTime, 1.0f );
        m_fCurrentPitchAimModifier = 1.0f;
    }

#ifndef X_EDITOR
    if( (m_TargetNetSlot != LastNetSlot) || ((m_TargetNetSlot != -1) && (m_AimOffset != LastAimOffset)) )
    {
        m_NetDirtyBits |= ORIENTATION_BIT;
    }
#endif
}

//=========================================================================
void player::ApplyAimAssistTurnDampening( void )
{
    if( m_AimAssistData.TurnDampeningT > 0 )
    {
        // kill aim assist stick dampening at AimAssist_Turn_Damp_Near_Dist
        f32 DampPct = x_parametric( m_AimAssistData.LOFPtDist, AimAssist_Turn_Damp_Near_Dist, AimAssist_Turn_Damp_Far_Dist, TRUE );

        // scale turn dampening
        m_AimAssistData.TurnDampeningT = m_AimAssistData.TurnDampeningT * DampPct;

        tweak_handle StickDampTweak( "TurnDampeningT" ); // 0=no turning, 1=normal turning
        f32 DampMax     = StickDampTweak.GetF32();
        f32 StickyMult  = 1.0f + (DampMax - 1.0f) * m_AimAssistData.TurnDampeningT;
        m_Combat.ApplyAimAssistDampening( m_LookSample, StickyMult );

        CLOG_MESSAGE( AIM_LOGGING_ENABLED, "player::ApplyAimAssistTurnDampening", "T:: %f", m_AimAssistData.TurnDampeningT );
    }
    else
    {
        CLOG_MESSAGE( AIM_LOGGING_ENABLED, "player::ApplyAimAssistTurnDampening *CHECK*", "T: %f", m_AimAssistData.TurnDampeningT );
        m_AimAssistData.TurnDampeningT = 0.0f;
    }
}

//=========================================================================
xbool player::IsAimAssistInputActive( void ) const
{
    return m_Combat.IsAimAssistActive( m_LookSample,
                                       m_Input.GetState(),
                                       m_bIsMutated );
}

//=========================================================================
void player::DegradeAim( f32 fAmountToDegradeBy )
{
    m_AimDegradation = MIN(1.0f, m_AimDegradation + fAmountToDegradeBy);
}

//=========================================================================
void player::UpdateAimOffset( f32 DeltaTime )
{
    // Largest value for AimOffset should be R_25.  Smallest value should be 0.
    if ( m_AimAssistData.TargetGuid != 0 )
    {
        m_YawAimOffset = MIN( R_25, m_YawAimOffset + DeltaTime * s_AimYawOffsetRateOfChange );
    }
    else
    {
        m_YawAimOffset = MAX( R_0, m_YawAimOffset - DeltaTime * s_AimYawOffsetRateOfChange );
    }
}

//=========================================================================
guid player::GetFriendlyOnReticle( void )
{
    return m_AimAssistData.OnlineFriendlyTargetGuid;
}

//=========================================================================
radian3 player::ApplyAimDegredation( radian Pitch, radian Yaw )
{
    vector3 Dir(0,0,1);
    Dir.RotateX( R_6*x_frand(-m_AimDegradation,m_AimDegradation) );   // Pitch Z-axis up or down by spread angle
    Dir.RotateZ( x_frand(0,R_360) );    // Roll dir around Z
    Dir.RotateX( Pitch );                  // Orient around original direction
    Dir.RotateY( Yaw );

    f32 P,Y;
    Dir.GetPitchYaw(P,Y);

    //x_DebugMsg("%f %f %f %f %f\n",m_AimDegradation,Pitch,Yaw,P,Y);

    return radian3(P,Y,0);
}
//=========================================================================
radian3 player::GetProjectileTrajectory( void )
{
    // the view's rotation
    radian Pitch;
    radian Yaw;
    vector3 StartPosition;
    xbool bUseWeaponPos = FALSE;

    new_weapon* pWeaponObj = GetCurrentWeaponPtr();

    // StartPosition will most likely be the "firepoint" of the weapon instead of coming out of your eyes.
    // This is for weapons like the Meson Cannon where you can see the projectile and the weapon.
    // GetFiringStartPosition() will return true for weapons that overload it.  Defaults to FALSE.
    bUseWeaponPos = pWeaponObj->GetFiringStartPosition(StartPosition);

    // if bUseWeaponPos == TRUE, for things like meson cannon and mutation weapon, fire projectile from firing point
    // Otherwise, use bullet assist direction
    if( !bUseWeaponPos )
    {
        if( IsAimAssistInputActive() )
        {
            m_AimAssistData.BulletAssistDir.GetPitchYaw( Pitch, Yaw );
        }
        else
        {
            GetEyesPitchYaw( Pitch, Yaw );
        }

        // apply aim degredation
        radian3 Rot = ApplyAimDegredation( Pitch, Yaw );

        return Rot;
    }

    vector3 EndPosition;
    GetProjectileHitLocation(EndPosition);

    // get the direction vector from the firing start position to where our trace hit
    vector3 ToTarget( EndPosition - StartPosition );

    // reload pitch and yaw from the Target vector
    ToTarget.GetPitchYaw( Pitch, Yaw );

    // We are firing the projectile visually from the weapon.
    radian3 ProjectileRot = ApplyAimDegredation( Pitch, Yaw );

    return ProjectileRot;
}

//=========================================================================
void player::GetProjectileHitLocation(vector3& EndPos, xbool bUseBulletAssist)
{   
    radian Pitch;
    radian Yaw;

    // the view's rotation
    view& View = GetSimulationView();

    if( bUseBulletAssist && IsAimAssistInputActive() )
    {
        m_AimAssistData.BulletAssistDir.GetPitchYaw( Pitch, Yaw );
    }
    else
    {
        GetEyesPitchYaw( Pitch, Yaw );
    }

    vector3 Dest( radian3( Pitch, Yaw, 0.0f ) );
    const vector3 ViewPos = View.GetPosition();
    Dest *= MAX_AUTO_AIM_DISTANCE;
    EndPos = ViewPos + Dest;
    g_CollisionMgr.AddToIgnoreList( GetGuid() );
    g_CollisionMgr.RaySetup( GetGuid(), ViewPos, EndPos );
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, object::ATTR_BLOCKS_SMALL_PROJECTILES, object::ATTR_COLLISION_PERMEABLE);

    // default modifier to full distance in case the collision manager returns no collisions
    f32 DistModifier = 1.0f;

    // if we don't hit anything, T is undefined
    if( g_CollisionMgr.m_nCollisions > 0 )
    {
        DistModifier = g_CollisionMgr.m_Collisions[0].T;
    }

    // get our new end position
    EndPos = ViewPos + (DistModifier*(EndPos-ViewPos));
}

//=========================================================================
guid player::GetEnemyOnReticle( void )
{
    return m_AimAssistData.ReticleEnemyGuid;
}

void player::LoadAimAssistTweakHandles( void )
{
    // get weapon pointer
    new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( m_WeaponGuids[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)] );
    if( !pWeapon )
    {
        return;
    }

    m_bTweakHandlesLoaded = TRUE;

    // load up strings
    xstring StringPrefix = (const char*)xfs( "%s_%d", pWeapon->GetLogicalName(), pWeapon->GetZoomStep() );
    const char* pString = (const char*)StringPrefix;

    AimAssist_LOF_Dist_Tweak.SetName                ( xfs("%s_LOF_Dist", pString) );

    AimAssist_Reticle_Near_Dist_Tweak.SetName       ( xfs("%s_Reticle_Near_Dist", pString) );
    AimAssist_Reticle_Far_Dist_Tweak.SetName        ( xfs("%s_Reticle_Far_Dist", pString) );
    AimAssist_Reticle_Near_Radius_Tweak.SetName     ( xfs("%s_Reticle_Near_Rad", pString) );
    AimAssist_Reticle_Far_Radius_Tweak.SetName      ( xfs("%s_Reticle_Far_Rad", pString) );

    AimAssist_Bullet_Inner_Near_Dist_Tweak.SetName  ( xfs("%s_Blt_In_Near_Dist", pString) );
    AimAssist_Bullet_Inner_Far_Dist_Tweak.SetName   ( xfs("%s_Blt_In_Far_Dist", pString) );
    AimAssist_Bullet_Inner_Near_Radius_Tweak.SetName( xfs("%s_Blt_In_Near_Rad", pString) );
    AimAssist_Bullet_Inner_Far_Radius_Tweak.SetName ( xfs("%s_Blt_In_Far_Rad", pString) );
       
    AimAssist_Bullet_Outer_Near_Dist_Tweak.SetName  ( xfs("%s_Blt_Out_Near_Dist", pString) );
    AimAssist_Bullet_Outer_Far_Dist_Tweak.SetName   ( xfs("%s_Blt_Out_Far_Dist", pString) );
    AimAssist_Bullet_Outer_Near_Radius_Tweak.SetName( xfs("%s_Blt_Out_Near_Rad", pString) );
    AimAssist_Bullet_Outer_Far_Radius_Tweak.SetName ( xfs("%s_Blt_Out_Far_Rad", pString) );

    AimAssist_Bullet_Angle_Tweak.SetName            ( xfs("%s_Blt_Angle", pString) );

    AimAssist_Turn_Inner_Near_Dist_Tweak.SetName    ( xfs("%s_Turn_In_Near_Dist", pString) );
    AimAssist_Turn_Inner_Far_Dist_Tweak.SetName     ( xfs("%s_Turn_In_Far_Dist", pString) );
    AimAssist_Turn_Inner_Near_Radius_Tweak.SetName  ( xfs("%s_Turn_In_Near_Rad", pString) );
    AimAssist_Turn_Inner_Far_Radius_Tweak.SetName   ( xfs("%s_Turn_In_Far_Rad", pString) );

    AimAssist_Turn_Outer_Near_Dist_Tweak.SetName    ( xfs("%s_Turn_Out_Near_Dist", pString) );
    AimAssist_Turn_Outer_Far_Dist_Tweak.SetName     ( xfs("%s_Turn_Out_Far_Dist", pString) );
    AimAssist_Turn_Outer_Near_Radius_Tweak.SetName  ( xfs("%s_Turn_Out_Near_Rad", pString) );
    AimAssist_Turn_Outer_Far_Radius_Tweak.SetName   ( xfs("%s_Turn_Out_Far_Rad", pString) );

    // how much emphasis on turn dampening at close ranges?
    AimAssist_Turn_Damp_Near_Dist_Tweak.SetName  ( xfs("%s_Turn_Damp_Near_Dist", pString) );
    AimAssist_Turn_Damp_Far_Dist_Tweak.SetName  ( xfs("%s_Turn_Damp_Far_Dist", pString) );
}

void player::LoadAimAssistTweaks( void )
{
    // make sure weapon has been loaded
    new_weapon* pWeapon = (new_weapon*)g_ObjMgr.GetObjectByGuid( m_WeaponGuids[inventory2::ItemToWeaponIndex(m_CurrentWeaponItem)] );
    if( !pWeapon || !m_bTweakHandlesLoaded )
    {
        return;
    }

    AimAssist_LOF_Dist                  = AimAssist_LOF_Dist_Tweak.GetF32();

    AimAssist_Reticle_Near_Dist         = AimAssist_Reticle_Near_Dist_Tweak.GetF32();
    AimAssist_Reticle_Far_Dist          = AimAssist_Reticle_Far_Dist_Tweak.GetF32();
    AimAssist_Reticle_Near_Radius       = AimAssist_Reticle_Near_Radius_Tweak.GetF32();
    AimAssist_Reticle_Far_Radius        = AimAssist_Reticle_Far_Radius_Tweak.GetF32();

    AimAssist_Bullet_Inner_Near_Dist    = AimAssist_Bullet_Inner_Near_Dist_Tweak.GetF32();
    AimAssist_Bullet_Inner_Far_Dist     = AimAssist_Bullet_Inner_Far_Dist_Tweak.GetF32();
    AimAssist_Bullet_Inner_Near_Radius  = AimAssist_Bullet_Inner_Near_Radius_Tweak.GetF32();
    AimAssist_Bullet_Inner_Far_Radius   = AimAssist_Bullet_Inner_Far_Radius_Tweak.GetF32();

    AimAssist_Bullet_Outer_Near_Dist    = AimAssist_Bullet_Outer_Near_Dist_Tweak.GetF32();
    AimAssist_Bullet_Outer_Far_Dist     = AimAssist_Bullet_Outer_Far_Dist_Tweak.GetF32();
    AimAssist_Bullet_Outer_Near_Radius  = AimAssist_Bullet_Outer_Near_Radius_Tweak.GetF32();
    AimAssist_Bullet_Outer_Far_Radius   = AimAssist_Bullet_Outer_Far_Radius_Tweak.GetF32();

    AimAssist_Bullet_Angle              = AimAssist_Bullet_Angle_Tweak.GetF32();

    AimAssist_Turn_Inner_Near_Dist      = AimAssist_Turn_Inner_Near_Dist_Tweak.GetF32();
    AimAssist_Turn_Inner_Far_Dist       = AimAssist_Turn_Inner_Far_Dist_Tweak.GetF32();
    AimAssist_Turn_Inner_Near_Radius    = AimAssist_Turn_Inner_Near_Radius_Tweak.GetF32();
    AimAssist_Turn_Inner_Far_Radius     = AimAssist_Turn_Inner_Far_Radius_Tweak.GetF32();

    AimAssist_Turn_Outer_Near_Dist      = AimAssist_Turn_Outer_Near_Dist_Tweak.GetF32();
    AimAssist_Turn_Outer_Far_Dist       = AimAssist_Turn_Outer_Far_Dist_Tweak.GetF32();
    AimAssist_Turn_Outer_Near_Radius    = AimAssist_Turn_Outer_Near_Radius_Tweak.GetF32();
    AimAssist_Turn_Outer_Far_Radius     = AimAssist_Turn_Outer_Far_Radius_Tweak.GetF32();

    AimAssist_Turn_Damp_Near_Dist       = AimAssist_Turn_Damp_Near_Dist_Tweak.GetF32();
    AimAssist_Turn_Damp_Far_Dist        = AimAssist_Turn_Damp_Far_Dist_Tweak.GetF32();
}
