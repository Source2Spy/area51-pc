//==============================================================================
//
//  Flashlight.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Flashlight.hpp"

#include "Player/Player.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

static const f32       s_FlashlightRange          = 2048.0f;
static const radian    s_FlashlightFOV            = R_75;
static const f32       s_FlashlightIntensity      = 2.0f;
static const f32       s_FlashlightFalloff        = 0.5f;
static const f32       s_FlashlightInnerConeScale = 0.70f;
static const xcolor    s_FlashlightColor          ( 255, 255, 255, 255 );
static const vector3   s_FlashlightOffset         ( 0.0f, 0.0f, -4.0f );
static texture::handle s_FlashlightCookie;

//=========================================================================
//  FLASHLIGHT "OBJECT"
//=========================================================================

xbool flashlight_CalcTransform( player& Player, matrix4& L2W )
{
    new_weapon* pWeapon = Player.GetCurrentWeaponPtr();
    if( !pWeapon || !pWeapon->CheckFlashlightPoint() )
        return FALSE;

    vector3 Offset;
    if( !pWeapon->GetFlashlightTransformInfo( L2W, Offset ) )
        return FALSE;

    L2W.PreTranslate( Offset );
    L2W.PreRotateY( R_180 );
    L2W.PreTranslate( s_FlashlightOffset );
    return TRUE;
}

//=========================================================================

void flashlight_Register( player& Player )
{
    if( !Player.IsFlashlightOn() )
        return;

    matrix4 L2W;
    if( !flashlight_CalcTransform( Player, L2W ) )
        return;

    vector3 Direction = L2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if( !Direction.SafeNormalize() )
    {
        Direction.Set( 0.0f, 0.0f, 1.0f );
    }

    if( !s_FlashlightCookie.GetName()[0] )
    {
        s_FlashlightCookie.SetName( "Flashlight.xbmp" );
    }

    const f32 OuterAngle  = RAD_TO_DEG( s_FlashlightFOV );
    const f32 InnerAngle  = MAX( 1.0f, OuterAngle * s_FlashlightInnerConeScale );
    const f32 InnerRadius = MAX( 0.0f, s_FlashlightRange * ( 1.0f - s_FlashlightFalloff ) );

    render::GetRenderPipeline().GetLightSystem().AddDynamicLight( L2W.GetTranslation(),
                                s_FlashlightColor,
                                s_FlashlightRange,
                                s_FlashlightIntensity,
                                FALSE,
                                render::LightSystem::LIGHT_SHAPE_SPOT,
                                TRUE,
                                InnerRadius,
                                Direction,
                                s_FlashlightFalloff,
                                InnerAngle,
                                OuterAngle,
                                1024,
                                4,
                                s_FlashlightCookie,
                                TRUE );
}
