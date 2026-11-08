//==============================================================================
//
//  shadow_evsm.hlsl
//
//==============================================================================

#ifndef SHADOW_EVSM_HLSL
#define SHADOW_EVSM_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float2 WarpEVSMDepth( float Depth, float2 Exponents )
{
    const float WarpedDepth = Depth * 2.0f - 1.0f;
    return float2( exp( Exponents.x * WarpedDepth ), -exp( -Exponents.y * WarpedDepth ) );
}

//==============================================================================

float4 ComputeEVSM4Moments( float Depth, float2 Exponents )
{
    const float2 WarpedDepth = WarpEVSMDepth( Depth, Exponents );
    return float4( WarpedDepth.x, WarpedDepth.x * WarpedDepth.x,
                   WarpedDepth.y, WarpedDepth.y * WarpedDepth.y );
}

//==============================================================================

float ComputeEVSMChebyshevBound( float2 Moments, float ReceiverDepth, float MinimumVariance )
{
    if( ReceiverDepth <= Moments.x )
    {
        return 1.0f;
    }

    const float Variance = max( Moments.y - Moments.x * Moments.x, MinimumVariance );
    const float DepthDelta = ReceiverDepth - Moments.x;
    return Variance / ( Variance + DepthDelta * DepthDelta );
}

//==============================================================================
#endif // SHADOW_EVSM_HLSL
//==============================================================================
