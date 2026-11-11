//==============================================================================
//
//  point_shadow_faces.hlsl
//
//==============================================================================

#ifndef POINT_SHADOW_FACES_HLSL
#define POINT_SHADOW_FACES_HLSL

//==============================================================================
//  STRUCTS
//==============================================================================

struct PointShadowFacePair
{
    uint First;
    uint Second;
    float FirstAlignment;
    float SecondAlignment;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

bool PointShadowFacePrecedes( float A, uint IndexA, float B, uint IndexB )
{
    return A > B || ( A == B && IndexA < IndexB );
}

//==============================================================================

PointShadowFacePair PointShadowAxisFaces( float Alignment, uint First, uint FaceCount )
{
    PointShadowFacePair Pair;
    Pair.First = First;
    Pair.Second = First + 1u;
    Pair.FirstAlignment = First < FaceCount ? Alignment : -2.0f;
    Pair.SecondAlignment = First + 1u < FaceCount ? -Alignment : -2.0f;
    if( Pair.SecondAlignment > Pair.FirstAlignment )
    {
        Pair.First = First + 1u;
        Pair.Second = First;
        const float Temporary = Pair.FirstAlignment;
        Pair.FirstAlignment = Pair.SecondAlignment;
        Pair.SecondAlignment = Temporary;
    }
    return Pair;
}

//==============================================================================

PointShadowFacePair MergePointShadowFaces( PointShadowFacePair A, PointShadowFacePair B )
{
    if( PointShadowFacePrecedes( B.FirstAlignment, B.First, A.FirstAlignment, A.First ) )
    {
        const PointShadowFacePair Temporary = A;
        A = B;
        B = Temporary;
    }
    if( PointShadowFacePrecedes( B.FirstAlignment, B.First, A.SecondAlignment, A.Second ) )
    {
        A.Second = B.First;
        A.SecondAlignment = B.FirstAlignment;
    }
    return A;
}

//==============================================================================
#endif // POINT_SHADOW_FACES_HLSL
//==============================================================================
