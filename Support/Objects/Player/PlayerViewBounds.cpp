//=========================================================================
//
//  PlayerViewBounds.cpp
//
//=========================================================================

#include "PlayerView.hpp"

//=========================================================================

f32 PlayerView::GetNearPlaneRadius( f32 NearClip, radian XFOV, radian YFOV )
{
    ASSERT( NearClip > 0.0f );
    ASSERT( (XFOV > 0.0f) && (XFOV < R_180) );
    ASSERT( (YFOV > 0.0f) && (YFOV < R_180) );

    const f32 HalfWidth  = x_tan( XFOV * 0.5f );
    const f32 HalfHeight = x_tan( YFOV * 0.5f );
    return NearClip * x_sqrt( 1.0f + SQR( HalfWidth ) + SQR( HalfHeight ) );
}

//=========================================================================

void PlayerView::ComputeFirstPersonClearance( f32 Radius, radian XFOV, radian YFOV,
                                             f32& NearClip, f32& Clearance )
{
    ASSERT( Radius > ClearanceMargin );
    const f32 UnitRadius = GetNearPlaneRadius( 1.0f, XFOV, YFOV );

    // A very wide/tall projection may not fit at the usual near distance.
    // Shorten that distance rather than silently letting the near plane escape.
    NearClip = MIN( DefaultNearClip, (Radius - ClearanceMargin) / UnitRadius );
    Clearance = MIN( Radius, NearClip * UnitRadius + ClearanceMargin );
}

//=========================================================================

vector3 PlayerView::GetFirstPersonPivotCorrection( const vector3& BindOffset,
                                                    const radian3& Rotation )
{
    const matrix4 RigRotation( Rotation );
    const matrix4 EyeRotation( radian3( 0.0f, Rotation.Yaw, 0.0f ) );
    return EyeRotation * BindOffset - RigRotation * BindOffset;
}

//=========================================================================

vector3 PlayerView::ConstrainFirstPersonPosition( const vector3& Position,
                                                  const vector3& CapsuleBottom,
                                                  f32 Height, f32 Radius, f32 Clearance )
{
    ASSERT( Radius > 0.0f );
    ASSERT( Height >= Radius * 2.0f );
    ASSERT( (Clearance >= 0.0f) && (Clearance <= Radius) );

    // Erode the physics capsule by a sphere containing the eye and every near
    // plane corner. This bounds animation without another world collision solver.
    vector3 AxisPosition = CapsuleBottom;
    AxisPosition.GetY() = x_clamp( Position.GetY(), CapsuleBottom.GetY() + Radius,
                                                    CapsuleBottom.GetY() + Height - Radius );
    const vector3 Offset = Position - AxisPosition;
    const f32 DistanceSquared = Offset.LengthSquared();
    const f32 AllowedRadius = Radius - Clearance;
    if( DistanceSquared > SQR( AllowedRadius ) )
    {
        return AxisPosition + Offset * (AllowedRadius / x_sqrt( DistanceSquared ));
    }
    return Position;
}

//=========================================================================
