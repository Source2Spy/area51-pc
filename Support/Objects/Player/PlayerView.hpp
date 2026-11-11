//=========================================================================
//
//  PlayerView.hpp
//
//=========================================================================

#ifndef PLAYER_VIEW_HPP
#define PLAYER_VIEW_HPP

#include "x_math.hpp"
#include "ZoneMgr/ZoneMgr.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum class PlayerViewMode
{
    FirstPerson,
    LockedFirstPerson,
    Cinema,
    Death
};

//-------------------------------------------------------------------------

struct PlayerViewSample
{
                    PlayerViewSample                ( void );

    matrix4         ViewToWorld;
    radian          XFOV;
    f32             NearClip;
    f32             FarClip;
    f32             FirstPersonClearance; // Resolved eye's projection margin; zero for authored cameras.
    PlayerViewMode  Mode;
    xbool           HasZoneSnapshot;
    guid            ZoneSourceGuid;
    zone_mgr::tracker ZoneSnapshot;
};

//-------------------------------------------------------------------------

struct PlayerViewNode
{
                    PlayerViewNode                  ( void );

    f32             TimeTo;
    f32             Linger;
    vector3         LookAt;
};

//=========================================================================
//  DATA CONTRACTS
//=========================================================================

struct PlayerViewContext
{
    xbool HasInputSlot;
    xbool IsLockedViewActive;
    xbool IsCinemaActive;
    xbool IsDeathCameraActive;
    xbool IsInTurret;
    xbool IsDead;
    xbool CanDie;
    xbool IsPaused;
};

//=========================================================================
//  PLAYER VIEW
//=========================================================================

class PlayerStepView
{
public:
                    PlayerStepView  ( void );
    void            Clear           ( void );
    void            Advance         ( f32 GroundOffset, f32 DeltaTime, f32 StepHeight );
    f32             GetOffset       ( void ) const;

private:
    f32             m_Offset;
};

//=========================================================================

class PlayerView
{
public:

    static constexpr f32 DefaultNearClip = 10.0f;
    static constexpr f32 ClearanceMargin = 0.1f;

                    PlayerView                  ( void );

    static f32      GetNearPlaneRadius          ( f32 NearClip, radian XFOV, radian YFOV );
    static void     ComputeFirstPersonClearance ( f32 Radius, radian XFOV, radian YFOV,
                                                   f32& NearClip, f32& Clearance );
    static vector3  GetFirstPersonPivotCorrection ( const vector3& BindOffset,
                                                     const radian3& Rotation );
    static vector3  ConstrainFirstPersonPosition ( const vector3& Position,
                                                   const vector3& CapsuleBottom,
                                                   f32 Height, f32 Radius, f32 Clearance );
    static PlayerViewMode SelectMode            ( const PlayerViewContext& Context );
    static PlayerViewSample BlendSamples        ( const PlayerViewSample& From,
                                                   const PlayerViewSample& To,
                                                   f32 T );

    void            Reset                       ( const PlayerViewSample& Sample );
    void            BeginBlend                  ( const PlayerViewSample& From,
                                                   f32 Duration );
    PlayerViewSample Evaluate                    ( const PlayerViewSample& Target,
                                                   f32 DeltaTime );

    const PlayerViewSample& GetCurrent           ( void ) const;
    xbool           IsBlending                  ( void ) const;

private:

    PlayerViewSample m_Current;
    PlayerViewSample m_BlendFrom;
    f32              m_BlendDuration;
    f32              m_BlendElapsed;
    xbool            m_IsInitialized;
    xbool            m_IsBlending;
};

//-------------------------------------------------------------------------

class PlayerViewSequence
{
public:

    static constexpr s32 MaxNodes = 5;

                    PlayerViewSequence          ( void );

    void            Start                       ( const PlayerViewNode* pNodes,
                                                   s32 NodeCount,
                                                   const vector3& EyePosition,
                                                   const quaternion& StartRotation );
    void            Advance                     ( f32 DeltaTime,
                                                   const vector3& EyePosition );
    void            Stop                        ( void );

    xbool           IsActive                    ( void ) const;
    const quaternion& GetRotation               ( void ) const;

private:

    static quaternion GetLookAtRotation         ( const vector3& EyePosition,
                                                   const vector3& LookAt,
                                                   const quaternion& Fallback );
    void            SelectNode                  ( const vector3& EyePosition );

private:

    PlayerViewNode  m_Nodes[MaxNodes];
    quaternion      m_StartRotation;
    quaternion      m_DesiredRotation;
    quaternion      m_CurrentRotation;
    f32             m_ElapsedTime;
    s32             m_NodeCount;
    s32             m_CurrentNode;
    xbool           m_IsActive;
};

//=========================================================================
#endif // PLAYER_VIEW_HPP
