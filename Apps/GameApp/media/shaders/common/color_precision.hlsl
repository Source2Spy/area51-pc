//==============================================================================
//  Local color arithmetic keeps shared storage and stage interfaces in FP32.
//==============================================================================

#ifndef A51_COLOR_PRECISION_HLSL
#define A51_COLOR_PRECISION_HLSL

#if A51_NATIVE_FLOAT16
    typedef float16_t  A51Color;
    typedef float16_t3 A51Color3;
    typedef float16_t4 A51Color4;
#elif A51_COLOR_ARITHMETIC
    typedef min16float  A51Color;
    typedef min16float3 A51Color3;
    typedef min16float4 A51Color4;
#else
    typedef float  A51Color;
    typedef float3 A51Color3;
    typedef float4 A51Color4;
#endif

//==============================================================================
#endif // A51_COLOR_PRECISION_HLSL
//==============================================================================
