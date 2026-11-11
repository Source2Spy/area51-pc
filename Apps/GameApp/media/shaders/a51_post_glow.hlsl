//==============================================================================
//
//  a51_post_glow.hlsl
//
//  Post-processing glow shaders.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "common/shader_bindings.hlsl"
#include "a51_shader_contract.hlsl"

#include "common/color_precision.hlsl"
typedef A51Color3 GlowColor;

//==============================================================================
//  RESOURCES
//==============================================================================

A51_CBUFFER_ATTR(4, 0) cbuffer GlowParams A51_CBUFFER_BIND(4, 0)
{
    A51_ABI_FIELDS_PostGlowData
};

//------------------------------------------------------------------------------

A51_SAMPLED_TEXTURE_ATTR(0, 0) Texture2D GlowSource A51_SAMPLED_TEXTURE_BIND(0, 0);
A51_SAMPLED_TEXTURE_ATTR(1, 1) Texture2D GlowAux A51_SAMPLED_TEXTURE_BIND(1, 1);

A51_SAMPLER_ATTR(0, 0) SamplerState samGlowSource A51_SAMPLER_BIND(0, 0);
#if defined(A51_SHADER_BINDING_SDL)
    A51_SAMPLER_ATTR(1, 1) SamplerState samGlowAux A51_SAMPLER_BIND(1, 1);
#else
    #define samGlowAux samGlowSource
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

float2 ClampGlowUV( float2 UV )
{
    const float2 HalfTexel = max( GlowParams1.xy * 0.5f, float2( 1.0e-6f, 1.0e-6f ) );
    return clamp( UV, HalfTexel, float2( 1.0f, 1.0f ) - HalfTexel );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_Prefilter( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 Texel = GlowParams1.xy;
    const float2 Offsets[13] =
    {
        float2( -2.0f, -2.0f ), float2( 0.0f, -2.0f ), float2( 2.0f, -2.0f ),
        float2( -2.0f,  0.0f ), float2( 0.0f,  0.0f ), float2( 2.0f,  0.0f ),
        float2( -2.0f,  2.0f ), float2( 0.0f,  2.0f ), float2( 2.0f,  2.0f ),
        float2( -1.0f, -1.0f ), float2( 1.0f, -1.0f ),
        float2( -1.0f,  1.0f ), float2( 1.0f,  1.0f )
    };
    const float Weights[13] =
    {
        0.03125f, 0.0625f, 0.03125f,
        0.0625f,  0.125f,  0.0625f,
        0.03125f, 0.0625f, 0.03125f,
        0.125f, 0.125f, 0.125f, 0.125f
    };
    GlowColor Color = 0.0f;
    float Alpha = 0.0f;
    [unroll]
    for( int I = 0; I < 13; ++I )
    {
        const float4 Sample = GlowSource.SampleLevel( samGlowSource, UV + Offsets[I] * Texel, 0.0f );
        const float Mask = saturate( Sample.a );
        Color = (GlowColor)( Color + (GlowColor)Sample.rgb * (GlowColor)( Mask * Weights[I] ) );
        Alpha += Mask * Weights[I];
    }
    return float4( Color, Alpha );
}

//==============================================================================

float4 PS_Downsample( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    const float2 Offset = GlowParams1.xy * 0.5f;
    const float2 Offsets[5] =
    {
        float2( 0.0f, 0.0f ),
        float2(  Offset.x,  Offset.y ),
        float2( -Offset.x,  Offset.y ),
        float2( -Offset.x, -Offset.y ),
        float2(  Offset.x, -Offset.y )
    };
    const float Weights[5] = { 0.5f, 0.125f, 0.125f, 0.125f, 0.125f };
    GlowColor ColorAccum = 0.0f;
    float AlphaAccum = 0.0f;

    [unroll]
    for( int I = 0; I < 5; ++I )
    {
        const float4 Sample = GlowSource.SampleLevel( samGlowSource, UV + Offsets[I], 0.0f );
        ColorAccum = (GlowColor)( ColorAccum + (GlowColor)Sample.rgb * (A51Color)Weights[I] );
        AlphaAccum += Sample.a * Weights[I];
    }
    return float4( ColorAccum, AlphaAccum );
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

float GlowBlurWeight( float Distance )
{
    const float Weights[6] =
    {
        0.2270270270f, 0.1945945946f, 0.1216216216f,
        0.0540540541f, 0.0162162162f, 0.0f
    };
    const float Position = clamp( Distance, 0.0f, 5.0f );
    const int Index = min( (int)Position, 4 );
    return lerp( Weights[Index], Weights[Index + 1], Position - Index );
}

//==============================================================================

float4 SampleGlowBlur( float2 UV, float2 Direction )
{
    uint Width, Height;
    GlowSource.GetDimensions( Width, Height );
    const float Scale = max( max( abs( Direction.x ) * Width, abs( Direction.y ) * Height ), 1.0e-4f );
    const float2 TexelDirection = Direction / Scale;
    const int Radius = (int)ceil( 5.0f * Scale );
    float WeightSum = GlowBlurWeight( 0.0f );
    GlowColor Color = (GlowColor)GlowSource.SampleLevel( samGlowSource, UV, 0.0f ).rgb * (A51Color)WeightSum;

    [loop]
    for( int I = 1; I <= Radius; I += 2 )
    {
        const float First = GlowBlurWeight( float( I ) / Scale );
        const float Second = GlowBlurWeight( float( I + 1 ) / Scale );
        const float Weight = First + Second;
        if( Weight > 0.0f )
        {
            const float2 Offset = TexelDirection * ( float( I ) + Second / Weight );
            Color += (GlowColor)GlowSource.SampleLevel( samGlowSource, UV + Offset, 0.0f ).rgb * (A51Color)Weight;
            Color += (GlowColor)GlowSource.SampleLevel( samGlowSource, UV - Offset, 0.0f ).rgb * (A51Color)Weight;
            WeightSum += 2.0f * Weight;
        }
    }
    Color = (GlowColor)( Color / WeightSum );

    // Combine generates accumulator alpha; blur alpha has no consumers.
    return float4( Color, 0.0f );
}

//==============================================================================
//  SHADERS
//==============================================================================

float4 PS_BlurHorizontal( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return SampleGlowBlur( UV, float2( GlowParams1.x, 0.0f ) );
}

//==============================================================================

float4 PS_BlurVertical( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    return SampleGlowBlur( UV, float2( 0.0f, GlowParams1.y ) );
}

//==============================================================================

float4 PS_Combine( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float  Retention = saturate( GlowParams0.y );
    float2 CurrentUV = ClampGlowUV( UV + ( GlowParams1.w > 0.5f ? GlowParams1.xy * 0.5f : 0.0f ) );
    float4 Current   = GlowSource.SampleLevel( samGlowSource, CurrentUV, 0.0f );
    float4 History   = GlowParams0.w > 0.5f
                           ? GlowAux.SampleLevel( samGlowAux, UV, 0.0f ) : 0.0f;

    const GlowColor CurrentColor = (GlowColor)Current.rgb;
    const GlowColor HistoryColor = (GlowColor)History.rgb;
    const A51Color HistoryWeight = (A51Color)Retention;
    float4 Result;
    Result.rgb = GlowParams1.w > 0.5f
                     ? saturate( CurrentColor * (A51Color)GlowParams0.z + HistoryColor * HistoryWeight )
                     : saturate( CurrentColor + saturate( CurrentColor + HistoryColor ) * HistoryWeight );
    Result.a   = GlowParams1.w > 0.5f ? saturate( GlowParams1.z + History.a * Retention )
                                    : saturate( GlowParams1.z * 2.0f );
    return Result;
}

//==============================================================================

float4 PS_Composite( float4 Pos : SV_POSITION, float2 UV : TEXCOORD0 ) : SV_Target
{
    float4 Glow = GlowSource.SampleLevel( samGlowSource, UV, 0.0f );
    return float4( Glow.rgb, 1.0f );
}
