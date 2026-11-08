//=========================================================================
//
//  PlayerViewPolicy.cpp
//
//=========================================================================

#include "PlayerView.hpp"

//=========================================================================
//  PLAYER VIEW SAMPLE
//=========================================================================

PlayerViewSample::PlayerViewSample( void ) :
    XFOV     ( R_60 ),
    NearClip ( 10.0f ),
    FarClip  ( 8000.0f ),
    Mode     ( PlayerViewMode::FirstPerson ),
    HasZoneSnapshot ( FALSE ),
    ZoneSourceGuid  ( NULL_GUID )
{
    ViewToWorld.Identity();
}

//=========================================================================
//  PLAYER VIEW NODE
//=========================================================================

PlayerViewNode::PlayerViewNode( void ) :
    TimeTo ( -1.0f ),
    Linger ( 0.0f ),
    LookAt ( 0.0f, 0.0f, 0.0f )
{
}

//=========================================================================
//  PLAYER VIEW
//=========================================================================

PlayerView::PlayerView( void ) :
    m_BlendDuration ( 0.0f ),
    m_BlendElapsed  ( 0.0f ),
    m_IsInitialized ( FALSE ),
    m_IsBlending    ( FALSE )
{
}

//=========================================================================

PlayerViewMode PlayerView::SelectMode( const PlayerViewContext& Context )
{
    if( Context.IsCinemaActive )
    {
        return PlayerViewMode::Cinema;
    }

    if( Context.IsLockedViewActive )
    {
        return PlayerViewMode::LockedFirstPerson;
    }

    if( Context.IsDeathCameraActive )
    {
        return PlayerViewMode::Death;
    }

    return PlayerViewMode::FirstPerson;
}

//=========================================================================

PlayerViewSample PlayerView::BlendSamples( const PlayerViewSample& From,
                                           const PlayerViewSample& To,
                                           f32                     T )
{
    T = x_clamp( T, 0.0f, 1.0f );
    T = T * T * (3.0f - (2.0f * T));

    vector3 const FromPosition = From.ViewToWorld.GetTranslation();
    vector3 const ToPosition   = To.ViewToWorld.GetTranslation();
    vector3 const Position     = FromPosition + ((ToPosition - FromPosition) * T);

    quaternion const FromRotation( From.ViewToWorld );
    quaternion const ToRotation( To.ViewToWorld );
    quaternion const Rotation = Blend( FromRotation, ToRotation, T );

    PlayerViewSample Result = To;
    Result.HasZoneSnapshot = To.HasZoneSnapshot && (T == 1.0f);
    Result.ViewToWorld.Setup( vector3( 1.0f, 1.0f, 1.0f ), Rotation, Position );
    Result.XFOV     = From.XFOV     + ((To.XFOV     - From.XFOV)     * T);
    Result.NearClip = From.NearClip + ((To.NearClip - From.NearClip) * T);
    Result.FarClip  = From.FarClip  + ((To.FarClip  - From.FarClip)  * T);
    return Result;
}

//=========================================================================

void PlayerView::Reset( const PlayerViewSample& Sample )
{
    m_Current       = Sample;
    m_BlendFrom     = Sample;
    m_BlendDuration = 0.0f;
    m_BlendElapsed  = 0.0f;
    m_IsInitialized = TRUE;
    m_IsBlending    = FALSE;
}

//=========================================================================

void PlayerView::BeginBlend( const PlayerViewSample& From, f32 Duration )
{
    if( Duration <= 0.0f )
    {
        m_IsBlending = FALSE;
        return;
    }

    m_BlendFrom     = From;
    m_Current       = From;
    m_BlendDuration = Duration;
    m_BlendElapsed  = 0.0f;
    m_IsInitialized = TRUE;
    m_IsBlending    = TRUE;
}

//=========================================================================

PlayerViewSample PlayerView::Evaluate( const PlayerViewSample& Target, f32 DeltaTime )
{
    if( !m_IsInitialized )
    {
        Reset( Target );
        return m_Current;
    }

    if( !m_IsBlending )
    {
        m_Current = Target;
        return m_Current;
    }

    m_BlendElapsed += MAX( 0.0f, DeltaTime );
    f32 const T = x_clamp( m_BlendElapsed / m_BlendDuration, 0.0f, 1.0f );
    m_Current = BlendSamples( m_BlendFrom, Target, T );

    if( T >= 1.0f )
    {
        m_Current    = Target;
        m_IsBlending = FALSE;
    }

    return m_Current;
}

//=========================================================================

const PlayerViewSample& PlayerView::GetCurrent( void ) const
{
    return m_Current;
}

//=========================================================================

xbool PlayerView::IsBlending( void ) const
{
    return m_IsBlending;
}

//=========================================================================
//  PLAYER VIEW SEQUENCE
//=========================================================================

PlayerViewSequence::PlayerViewSequence( void ) :
    m_ElapsedTime  ( 0.0f ),
    m_NodeCount    ( 0 ),
    m_CurrentNode  ( 0 ),
    m_IsActive     ( FALSE )
{
    m_StartRotation.Identity();
    m_DesiredRotation.Identity();
    m_CurrentRotation.Identity();
}

//=========================================================================

void PlayerViewSequence::Start( const PlayerViewNode* pNodes,
                                s32                   NodeCount,
                                const vector3&        EyePosition,
                                const quaternion&     StartRotation )
{
    ASSERT( pNodes );

    m_NodeCount   = MIN( MAX( NodeCount, 0 ), MaxNodes );
    m_CurrentNode = 0;
    m_ElapsedTime = 0.0f;
    m_IsActive    = (m_NodeCount > 0) && (pNodes[0].TimeTo > 0.0f);

    for( s32 i = 0; i < m_NodeCount; i++ )
    {
        m_Nodes[i] = pNodes[i];
    }

    m_StartRotation   = StartRotation;
    m_CurrentRotation = StartRotation;
    m_DesiredRotation = StartRotation;

    if( m_IsActive )
    {
        SelectNode( EyePosition );
    }
}

//=========================================================================

void PlayerViewSequence::Advance( f32 DeltaTime, const vector3& EyePosition )
{
    if( !m_IsActive )
    {
        return;
    }

    m_ElapsedTime += MAX( 0.0f, DeltaTime );

    while( m_IsActive )
    {
        PlayerViewNode const& Node = m_Nodes[m_CurrentNode];
        f32 const NodeDuration = Node.TimeTo + MAX( 0.0f, Node.Linger );
        if( m_ElapsedTime < NodeDuration )
        {
            break;
        }

        m_ElapsedTime -= NodeDuration;
        m_StartRotation = m_DesiredRotation;
        m_CurrentNode++;

        if( (m_CurrentNode >= m_NodeCount) ||
            (m_Nodes[m_CurrentNode].TimeTo <= 0.0f) )
        {
            Stop();
            return;
        }

        SelectNode( EyePosition );
    }

    PlayerViewNode const& Node = m_Nodes[m_CurrentNode];
    f32 const T = x_clamp( m_ElapsedTime / Node.TimeTo, 0.0f, 1.0f );
    f32 const SmoothT = T * T * (3.0f - (2.0f * T));
    m_CurrentRotation = Blend( m_StartRotation, m_DesiredRotation, SmoothT );
}

//=========================================================================

void PlayerViewSequence::Stop( void )
{
    m_IsActive   = FALSE;
    m_NodeCount  = 0;
    m_CurrentNode = 0;
    m_ElapsedTime = 0.0f;
}

//=========================================================================

xbool PlayerViewSequence::IsActive( void ) const
{
    return m_IsActive;
}

//=========================================================================

const quaternion& PlayerViewSequence::GetRotation( void ) const
{
    return m_CurrentRotation;
}

//=========================================================================

quaternion PlayerViewSequence::GetLookAtRotation( const vector3&    EyePosition,
                                                  const vector3&    LookAt,
                                                  const quaternion& Fallback )
{
    vector3 const LookDirection = LookAt - EyePosition;
    if( LookDirection.LengthSquared() <= F32_MIN )
    {
        return Fallback;
    }

    return quaternion( radian3( LookDirection.GetPitch(),
                                LookDirection.GetYaw(),
                                0.0f ) );
}

//=========================================================================

void PlayerViewSequence::SelectNode( const vector3& EyePosition )
{
    m_DesiredRotation = GetLookAtRotation( EyePosition,
                                           m_Nodes[m_CurrentNode].LookAt,
                                           m_StartRotation );
}
