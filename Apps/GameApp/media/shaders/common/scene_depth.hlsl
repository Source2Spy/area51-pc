//==============================================================================
//
//  scene_depth.hlsl
//
//  Sampling normalized linear scene depth before the color resolve.
//
//==============================================================================

#ifndef SCENE_DEPTH_HLSL
#define SCENE_DEPTH_HLSL

//==============================================================================
//  FUNCTIONS
//==============================================================================

float SceneDepthNearestSample( Texture2DMS<float> Depth, int2 Pixel, uint SampleCount )
{
    float NearestDepth = 1.0f;

    [loop]
    for( uint SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex )
    {
        NearestDepth = min( NearestDepth, Depth.Load( Pixel, SampleIndex ) );
    }

    return NearestDepth;
}

//==============================================================================
#endif // SCENE_DEPTH_HLSL
//==============================================================================
