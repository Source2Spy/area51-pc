//=========================================================================
//
//  RenderFog.hpp
//
//  Shared fog contract for the render pipeline and forward shaders.
//
//=========================================================================

#ifndef RENDER_FOG_HPP
#define RENDER_FOG_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "a51_shader_contract.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

enum render_fog_mode
{
    RENDER_FOG_NONE = 0,
    RENDER_FOG_FORWARD_COLOR,
    RENDER_FOG_FORWARD_EMISSION,
    RENDER_FOG_MODE_COUNT
};

//-------------------------------------------------------------------------

struct RenderFogConstants : RenderFogGpuConstants
{
    RenderFogConstants( void );
    void PreparePolynomial( f32 NearZ, f32 FarZ, f32 FogStart, const vector4& Coefficients );
};

static_assert( sizeof( RenderFogConstants ) == 80, "cbFogConstants layout must match HLSL" );

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
RenderFogConstants::RenderFogConstants( void )
{
    Color.Set( 0, 0, 0, 0 );
    Depth.Set( 0, 1, 0, 0 );
    Polynomial.Set( 0, 0, 0, 0 );
    Prepared.Set( 0, 0, 0, 0 );

    SceneMode = BloomMode = RENDER_FOG_NONE;
    XboxFogEnabled        = 0;
    Padding               = 0;
}

//=========================================================================

inline
void RenderFogConstants::PreparePolynomial( f32 NearZ, f32 FarZ, f32 FogStart, const vector4& Coefficients )
{
    const f32 Range      = MAX( FarZ - NearZ, 1.0e-5f );
    const f32 Q          = FarZ / Range;
    const f32 StartZ2    = FogStart * FogStart;
    const f32 StartAlpha = Coefficients.GetX() + Coefficients.GetY() * FogStart + Coefficients.GetZ() * StartZ2 +
                           Coefficients.GetW() * StartZ2 * FogStart;

    Depth.Set( NearZ, Range, FogStart, 1.0f );
    Polynomial = Coefficients;
    Prepared.Set( Q, NearZ * Q, StartAlpha, 1.0f / MAX( 1.0f - StartAlpha, 1.0e-5f ) );
}

} // namespace render

#endif // RENDER_FOG_HPP
