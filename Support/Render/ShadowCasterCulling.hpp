//=========================================================================
//
//  ShadowCasterCulling.hpp
//
//=========================================================================

#ifndef SHADOW_CASTER_CULLING_HPP
#define SHADOW_CASTER_CULLING_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"

namespace render
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool ShadowCasterIntersectsFrustum( const bbox& Bounds, const matrix4& LocalToClip );

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
xbool ShadowCasterIntersectsFrustum( const bbox& Bounds, const matrix4& LocalToClip )
{
    const vector3 Center = Bounds.GetCenter();
    const vector3 Extent = ( Bounds.Max - Bounds.Min ) * 0.5f;

    for( s32 Plane = 0; Plane < 6; ++Plane )
    {
        const s32 Axis = Plane / 2;
        const f32 Sign = ( Plane & 1 ) ? -1.0f : 1.0f;
        f32       Coefficients[4];
        for( s32 Component = 0; Component < 4; ++Component )
        {
            Coefficients[Component] = Sign * LocalToClip( Component, Axis );
            if( Plane != 4 )
            {
                Coefficients[Component] += LocalToClip( Component, 3 );
            }
        }

        const f32 X      = Coefficients[0] * Center.GetX();
        const f32 Y      = Coefficients[1] * Center.GetY();
        const f32 Z      = Coefficients[2] * Center.GetZ();
        const f32 Radius = x_abs( Coefficients[0] ) * Extent.GetX() + x_abs( Coefficients[1] ) * Extent.GetY() +
                           x_abs( Coefficients[2] ) * Extent.GetZ();
        const f32 Distance = X + Y + Z + Coefficients[3];
        // Leave room for CPU/GPU matrix arithmetic at large world coordinates.
        const f32 Tolerance = 1.0e-5f * ( 1.0f + x_abs( X ) + x_abs( Y ) + x_abs( Z ) + x_abs( Coefficients[3] ) + Radius );

        if( Distance + Radius < -Tolerance )
        {
            return FALSE;
        }
    }

    return TRUE;
}

} // namespace render

#endif // SHADOW_CASTER_CULLING_HPP
