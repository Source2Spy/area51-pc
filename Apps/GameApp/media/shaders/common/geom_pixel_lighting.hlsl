//==============================================================================
//
//  geom_pixel_lighting.hlsl
//
//  Lighting, specular and projected light helpers for geometry shading.
//
//==============================================================================

#ifndef GEOM_PIXEL_LIGHTING_HLSL
#define GEOM_PIXEL_LIGHTING_HLSL

//==============================================================================
//  TYPES
//==============================================================================

#include "geom_world_gradients.hlsl"

#include "color_precision.hlsl"

typedef A51Color3 GeomLightingColor3;

struct GeomLightingResult
{
    GeomLightingColor3 Diffuse;
    GeomLightingColor3 Specular;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

#if A51_RECIPE_LIGHTING
float GeomComputeRadialAttenuation( float LightDistance, float2 Radial )
{
    return 1.0f - saturate( ( LightDistance - Radial.x ) * Radial.y );
}

//==============================================================================

bool GeomIsCharFillLight( float4 LightDir )
{
    return LightDir.w >= 1.5f;
}

//==============================================================================

float GeomComputeSpotAttenuation( GEOM_PIXEL_INPUT Input, uint LightIndex, float3 LightToPointDir )
{
    float4 LightDir = GeomGetLightDir( Input, LightIndex );
    if( GeomIsCharFillLight( LightDir ) || ( LightDir.w < 0.5f ) )
    {
        return 1.0f;
    }

    const float3 SpotDir = LightDir.xyz;

    const float ConeCos  = dot( SpotDir, LightToPointDir );
    float4 LightCone     = GeomGetLightCone( Input, LightIndex );
    return saturate( ( ConeCos - LightCone.y ) * LightCone.w );
}
#endif

//==============================================================================

#if A51_RECIPE_COOKIES || A51_RECIPE_PROJECTION
float SampleProjectionAtlas( float2 LocalUV,
                             float2 LocalDx,
                             float2 LocalDy,
                             float4 AtlasRegion,
                             float4 TileBounds,
                             float4 Sampling )
{
    const float2 AtlasUV = LocalUV * AtlasRegion.xy + AtlasRegion.zw;

    const float2 Dx = LocalDx * Sampling.zw;
    const float2 Dy = LocalDy * Sampling.zw;
    const float Footprint = max( dot( Dx, Dx ), dot( Dy, Dy ) );
    const float Lod = min( Sampling.y,
                           max( 0.0f, 0.5f * log2( max( Footprint, 1e-8f ) ) ) );

    const float2 HalfMipTexel = ( 0.5f * exp2( ceil( Lod ) ) ) * ProjectionAtlasParams.xy;
    const float2 SafeAtlasUV = clamp( AtlasUV, TileBounds.xy + HalfMipTexel,
                                      TileBounds.zw - HalfMipTexel );

    return txProjectionAtlas.SampleLevel( samProjectionAtlas,
                                          float3( SafeAtlasUV,
                                                  Sampling.x ),
                                          Lod ).r;
}
#endif

//==============================================================================

#if A51_RECIPE_COOKIES
float GeomSampleLightCookie( GEOM_PIXEL_INPUT Input,
                             uint             LightIndex,
                             float3           LightToPoint,
                             GeomWorldGradients Gradients )
{
    float4 Sampling = GeomGetLightCookieInfo( Input, LightIndex );
    if( Sampling.x <= 0.0f )
    {
        return 1.0f;
    }

    float4 LightDir = GeomGetLightDir( Input, LightIndex );
    if( LightDir.w < 0.5f || GeomIsCharFillLight( LightDir ) )
    {
        return 1.0f;
    }

    const float3 SpotDir = LightDir.xyz;
    const float DistAlong = dot( LightToPoint, SpotDir );
    if( DistAlong <= 1e-4f )
    {
        return 0.0f;
    }

    const float4 LightCone = GeomGetLightCone( Input, LightIndex );
    const float ConeScale = LightCone.z;
    const float ConeRadius = max( DistAlong * ConeScale, 1e-4f );

    const float4 CookieU = GeomGetLightCookieU( Input, LightIndex );
    const float4 CookieV = GeomGetLightCookieV( Input, LightIndex );
    const float2 UV = float2( dot( LightToPoint, CookieU.xyz ),
                              dot( LightToPoint, CookieV.xyz ) ) /
                      (2.0f * ConeRadius) + 0.5f;

    if( any( UV < 0.0f ) || any( UV > 1.0f ) )
    {
        return 0.0f;
    }

    const float RadiusDx = DistAlong * ConeScale > 1e-4f ? dot( Gradients.Dx, SpotDir ) * ConeScale : 0.0f;
    const float RadiusDy = DistAlong * ConeScale > 1e-4f ? dot( Gradients.Dy, SpotDir ) * ConeScale : 0.0f;
    const float2 UVDx = ( float2( dot( Gradients.Dx, CookieU.xyz ), dot( Gradients.Dx, CookieV.xyz ) ) * 0.5f -
                          ( UV - 0.5f ) * RadiusDx ) / ConeRadius;
    const float2 UVDy = ( float2( dot( Gradients.Dy, CookieU.xyz ), dot( Gradients.Dy, CookieV.xyz ) ) * 0.5f -
                          ( UV - 0.5f ) * RadiusDy ) / ConeRadius;

    Sampling.x -= 1.0f;
    return saturate( SampleProjectionAtlas( UV, UVDx, UVDy,
                                             GeomGetLightCookieAtlas( Input, LightIndex ),
                                             GeomGetLightCookieTileBounds( Input, LightIndex ),
                                             Sampling ) );
}
#endif

//==============================================================================

#if A51_RECIPE_LIGHTING
float GeomComputeLocalLightAttenuation( GEOM_PIXEL_INPUT Input,
                                       uint LightIndex,
                                       float3 WorldPos,
                                       out float3 PointToLightDir,
                                       out float LightDistance )
{
    LightDistance = 0.0f;
    float4 LightVec = GeomGetLightVec( Input, LightIndex );
    float4 LightCol = GeomGetLightCol( Input, LightIndex );
    float4 LightDir = GeomGetLightDir( Input, LightIndex );

    if( GeomIsCharFillLight( LightDir ) )
    {
        const float DirLenSq = dot( LightVec.xyz, LightVec.xyz );
        if( DirLenSq <= 1e-8f )
        {
            PointToLightDir = 0.0f;
            return 0.0f;
        }

        PointToLightDir = -LightVec.xyz * rsqrt( DirLenSq );
        return 1.0f;
    }

    const float3 LightToPoint = WorldPos - LightVec.xyz;
    const float3 ToLight = -LightToPoint;
    const float  DistSq  = dot( ToLight, ToLight );

    if( DistSq <= 1e-8f )
    {
        PointToLightDir = float3( 0.0f, 0.0f, 1.0f );
        return 1.0f;
    }

    const float InverseDistance = rsqrt(DistSq);
    const float Dist = DistSq * InverseDistance;
    LightDistance = Dist;
    const float Radial = GeomComputeRadialAttenuation( Dist, GeomGetLightRadial( Input, LightIndex ) );
    if( Radial <= 0.0f )
    {
        PointToLightDir = 0.0f;
        return 0.0f;
    }

    PointToLightDir = ToLight * InverseDistance;
    return Radial * GeomComputeSpotAttenuation( Input, LightIndex, -PointToLightDir );
}

//==============================================================================

float GeomComputeLocalLightShadowVisibility( GEOM_PIXEL_INPUT Input,
                                             uint LightIndex,
                                             float3 GeometricNormal,
                                             float3 PointToLightDir,
                                             float LightDistance );

GeomLightingResult GeomComputeLighting( GEOM_PIXEL_INPUT Input,
                                        uint MaterialFlags,
                                        float DiffuseAlpha,
                                        float3 GeometricNormal,
                                        GeomWorldGradients Gradients )
{
    GeomLightingResult Result;
    Result.Diffuse  = (GeomLightingColor3)GeomGetLightAmbCol( Input ).rgb;
    Result.Specular = 0.0f;

#if GEOM_HAS_VERTEX_COLOR
    Result.Diffuse = (GeomLightingColor3)( Result.Diffuse + Input.Color.rgb );
#endif
#if A51_GEOM_SCREEN_AO
    const float AmbientVisibility = GeomAmbientVisibility( Input );
    Result.Diffuse = (GeomLightingColor3)( Result.Diffuse * AmbientVisibility );
#endif

    float SpecBlend = 0.0f;
#if !A51_GEOM_SURFACE_ONLY
    if( MaterialFlags & MATERIAL_FLAG_ENVIRONMENT )
    {
        float EnvStrength = 1.0f;

        if( MaterialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
        {
            if( MaterialFlags & MATERIAL_FLAG_ENV_CUBEMAP )
            {
                EnvStrength = 4.0f * EnvParams.y;
            }
            else
            {
                EnvStrength = EnvParams.y;
            }
        }

        if( MaterialFlags & MATERIAL_FLAG_DIFF_PERPIXEL_ENV )
        {
            SpecBlend = DiffuseAlpha * EnvStrength;
        }
        else if( MaterialFlags & MATERIAL_FLAG_ALPHA_PERPOLY_ENV )
        {
            SpecBlend = EnvParams.x * EnvStrength;
        }
    }

    SpecBlend *= saturate( EnvParams.z );
#endif
    const bool   ComputeSpecular = SpecBlend > 0.0f;
#if !A51_GEOM_SURFACE_ONLY
    const float3 ViewDir = ComputeSpecular ? normalize( -Input.ViewVector ) : 0.0f;
#else
    const float3 ViewDir = 0.0f;
#endif
    const uint   LightCount     = GeomGetLightCount( Input );

    [fastopt]
    [loop]
    for( uint I = 0; I < LightCount; ++I )
    {
        float3 L;
        float LightDistance;
        const float Atten = GeomComputeLocalLightAttenuation( Input, I, Input.WorldPos, L, LightDistance );
        if( Atten > 0.0f )
        {
            const float Ndotl = saturate( dot( Input.Normal, L ) );
            if( Ndotl > 0.0f )
            {
#if A51_RECIPE_COOKIES
                const float Cookie = LightDistance > 0.0f
                    ? GeomSampleLightCookie( Input, I,
                        Input.WorldPos - GeomGetLightVec( Input, I ).xyz, Gradients ) : 1.0f;
#else
                const float Cookie = 1.0f;
#endif
                if( Cookie <= 0.0f )
                {
                    continue;
                }
                const GeomLightingColor3 LightColor = (GeomLightingColor3)GeomGetLightCol( Input, I ).rgb;
#if A51_RECIPE_SHADOWS
                const float Visibility = GeomComputeLocalLightShadowVisibility( Input, I, GeometricNormal, L, LightDistance );
#else
                const float Visibility = 1.0f;
#endif
                const float LightScale   = Atten * Cookie * Ndotl * Visibility;
                float       DiffuseScale = LightScale;
#if A51_GEOM_SCREEN_AO
                if( GeomIsCharFillLight( GeomGetLightDir( Input, I ) ) )
                {
                    DiffuseScale *= AmbientVisibility;
                }
#endif
                Result.Diffuse = (GeomLightingColor3)( Result.Diffuse + LightColor * (A51Color)DiffuseScale );

                if( ComputeSpecular )
                {
                    const float3 H        = normalize( L + ViewDir );
                    const float  SpecTerm = pow( saturate( dot( Input.Normal, H ) ), 16.0f );
                    Result.Specular = (GeomLightingColor3)( Result.Specular + LightColor * (A51Color)( SpecTerm * LightScale ) );
                }
            }
        }
    }

    Result.Specular = (GeomLightingColor3)( Result.Specular * (A51Color)SpecBlend );
    return Result;
}
#endif

//==============================================================================

#if A51_RECIPE_PROJECTION
float3 ApplyProjLights( float3 Color, float3 WorldPos, GeomWorldGradients Gradients )
{
    for( uint I = 0; I < ProjLightCount; ++I )
    {
        const float4 ProjPos = mul( ProjLightMatrix[I], float4( WorldPos, 1.0f ) );
        if( ProjPos.w <= 0.0f )
        {
            continue;
        }

        const float2 UV = ProjPos.xy / ProjPos.w;
        if( any( UV < 0.0f ) || any( UV > 1.0f ) )
        {
            continue;
        }

        const float4 ProjDx = mul( ProjLightMatrix[I], float4( Gradients.Dx, 0.0f ) );
        const float4 ProjDy = mul( ProjLightMatrix[I], float4( Gradients.Dy, 0.0f ) );
        const float2 UVDx = ( ProjDx.xy - UV * ProjDx.w ) / ProjPos.w;
        const float2 UVDy = ( ProjDy.xy - UV * ProjDy.w ) / ProjPos.w;
        const float Projection = SampleProjectionAtlas( UV, UVDx, UVDy,
                                                         ProjLightAtlas[I],
                                                         ProjLightTileBounds[I],
                                                         ProjLightInfo[I] );
        Color = lerp( Color, Color * 2.0f, saturate( Projection ) );
    }

    return Color;
}

//==============================================================================

float3 ApplyProjShadows( float3 Color, float3 WorldPos, GeomWorldGradients Gradients )
{
    for( uint I = 0; I < ProjShadowCount; ++I )
    {
        const float4 ProjPos = mul( ProjShadowMatrix[I], float4( WorldPos, 1.0f ) );
        if( ProjPos.w <= 0.0f )
        {
            continue;
        }

        const float2 UV = ProjPos.xy / ProjPos.w;
        if( any( UV < 0.0f ) || any( UV > 1.0f ) )
        {
            continue;
        }

        const float4 ProjDx = mul( ProjShadowMatrix[I], float4( Gradients.Dx, 0.0f ) );
        const float4 ProjDy = mul( ProjShadowMatrix[I], float4( Gradients.Dy, 0.0f ) );
        const float2 UVDx = ( ProjDx.xy - UV * ProjDx.w ) / ProjPos.w;
        const float2 UVDy = ( ProjDy.xy - UV * ProjDy.w ) / ProjPos.w;
        const float Shade = SampleProjectionAtlas( UV, UVDx, UVDy,
                                                    ProjShadowAtlas[I],
                                                    ProjShadowTileBounds[I],
                                                    ProjShadowInfo[I] );
        Color *= Shade * 2.0f;
    }

    return Color;
}
#endif

//==============================================================================
#endif // GEOM_PIXEL_LIGHTING_HLSL
//==============================================================================
