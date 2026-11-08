//==============================================================================
//
//  fog_functions.hlsl
//
//  Shared depth-based fog math used by post and forward geometry passes.
//
//==============================================================================

#ifndef A51_FOG_FUNCTIONS_HLSL
#define A51_FOG_FUNCTIONS_HLSL

static const uint A51_FOG_MODE_NONE = 0u;
static const uint A51_FOG_MODE_COLOR = 1u;
static const uint A51_FOG_MODE_EMISSION = 2u;

float EvaluatePolynomialFogAlpha( float LinearDepth, float4 Depth, float4 FogCoeff, float4 Prepared )
{
    const float ViewZ = Depth.x + LinearDepth * Depth.y;
    const float XboxZ = ViewZ * Prepared.x - Prepared.y;

    if( XboxZ <= Depth.z )
    {
        return 0.0f;
    }

    const float Z2        = XboxZ * XboxZ;
    const float Z3        = Z2 * XboxZ;
    const float RawAlpha  = FogCoeff.x + FogCoeff.y * XboxZ + FogCoeff.z * Z2 + FogCoeff.w * Z3;
    return saturate( ( RawAlpha - Prepared.z ) * Prepared.w );
}

//==============================================================================

float4 SampleFog( float LinearDepth )
{
    float4 Fog = FogPalette.SampleLevel( samFogPalette, float2( saturate( LinearDepth ), 0.5f ), 0.0f );
    Fog.a = saturate( Fog.a * 2.0f );
    return Fog;
}

//==============================================================================

float4 GetFogSample( float LinearDepth )
{
    float4 Fog;
    if( FogDepth.w > 0.5f )
    {
        Fog = float4( FogColor.rgb,
                      EvaluatePolynomialFogAlpha( LinearDepth, FogDepth, FogPolynomial, FogPrepared ) );
    }
    else
    {
        Fog = SampleFog( LinearDepth );
    }

    return XboxFogEnabled != 0u ? Fog.gbar : Fog;
}

//==============================================================================

float GetFogAlpha( float LinearDepth )
{
    return GetFogSample( LinearDepth ).a;
}

//==============================================================================

float4 ApplyFogToColor( float4 Color, float LinearDepth )
{
    if( SceneFogMode != A51_FOG_MODE_COLOR )
    {
        return Color;
    }

    const float4 Fog = GetFogSample( LinearDepth );
    Color.rgb = lerp( Color.rgb, Fog.rgb, Fog.a );
    return Color;
}

//==============================================================================

float4 ApplyFogToIntensity( float4 Color, float LinearDepth )
{
    if( SceneFogMode != A51_FOG_MODE_COLOR )
    {
        return Color;
    }

    Color.rgb = lerp( Color.rgb, 0.5f, GetFogAlpha( LinearDepth ) );
    return Color;
}

//==============================================================================

float4 ApplyFogToEmission( float4 Color, float LinearDepth )
{
    if( BloomFogMode != A51_FOG_MODE_EMISSION )
    {
        return Color;
    }

    Color.rgb *= 1.0f - GetFogAlpha( LinearDepth );
    return Color;
}

//==============================================================================

float4 ApplySceneFogToEmission( float4 Color, float LinearDepth )
{
    if( SceneFogMode != A51_FOG_MODE_COLOR )
    {
        return Color;
    }

    Color.rgb *= 1.0f - GetFogAlpha( LinearDepth );
    return Color;
}

//==============================================================================
#endif // A51_FOG_FUNCTIONS_HLSL
//==============================================================================
