//==============================================================================
//
//  geom_pixel_shading.hlsl
//
//  Final geometry pixel shader composition.
//
//==============================================================================

#ifndef GEOM_PIXEL_SHADING_HLSL
#define GEOM_PIXEL_SHADING_HLSL


//==============================================================================
//  FUNCTIONS
//==============================================================================

void GeomApplyFadeAlpha( inout GeomPixelResult Output, uint MaterialFlags, float FadeAlpha )
{
#if A51_RECIPE_COVERAGE_ALPHA
    if( !CoverageUsesTextureAlpha )
        Output.Color.a = FadeAlpha;
#endif
    if( FadeAlpha < 1.0f )
    {
        const bool BFadeByTextureAlpha = CoverageUsesTextureAlpha != 0;
        const float BlendAlpha = BFadeByTextureAlpha ? saturate( Output.Color.a * FadeAlpha ) : FadeAlpha;

        Output.Color.a = BlendAlpha;
        Output.Bloom.a = saturate( Output.Bloom.a * FadeAlpha );
    }
}

//==============================================================================

GeomPixelResult ShadeGeometryPixel( GEOM_PIXEL_INPUT Input, bool IsFrontFace )
{
#if A51_RECIPE_FORCED_GLOW && !A51_RECIPE_DAMAGE
    const float2 UVDx = ddx(Input.UV);
    const float2 UVDy = ddy(Input.UV);
    const uint MaterialFlags = GeomGetMaterialFlags(Input);
    const GeomDiffuseResult Diffuse = GeomEvaluateDiffuse(Input, MaterialFlags, AlphaRef, UVDx, UVDy);
    GeomPixelResult Output;
    Output.Color = Diffuse.Color;
    Output.Bloom = GeomComputeGlow(Input, MaterialFlags, Output.Color, Diffuse.Sample);
    GeomApplyFadeAlpha(Output, MaterialFlags, GeomGetFadeAlpha(Input));
    return Output;
#else
    GeomWorldGradients Gradients;
    Gradients.Dx = 0;
    Gradients.Dy = 0;
#if A51_RECIPE_COOKIES || A51_RECIPE_SHADOWS || A51_RECIPE_PROJECTION
    Gradients.Dx = ddx( Input.WorldPos );
    Gradients.Dy = ddy( Input.WorldPos );
#endif
    const float2 UVDx = ddx( Input.UV );
    const float2 UVDy = ddy( Input.UV );

#if !A51_GEOM_SURFACE_ONLY
    GeomSurfaceGradients SurfaceGradients = (GeomSurfaceGradients)0;
#if A51_RECIPE_ENV == 1
    SurfaceGradients.NormalDx = ddx( Input.Normal );
    SurfaceGradients.NormalDy = ddy( Input.Normal );
#endif
#if A51_RECIPE_ENV == 2
    // Cube SampleGrad is expensive on some drivers.  Sample before coverage or
    // instance branches so implicit derivatives remain defined for masked draws.
    const float3 EnvironmentColor = txEnvironmentCube.Sample( samEnvironmentCube, Input.ViewVector ).rgb;
#else
    const float3 EnvironmentColor = 0.0f;
#endif
#endif

    GeomPixelResult Output;
    Output.Color = 0.0f;
    Output.Bloom = 0.0f;

    uint  MaterialFlags = GeomGetMaterialFlags( Input );
    float MaterialAlphaRef = AlphaRef;
    float FadeAlpha     = GeomGetFadeAlpha( Input );

#if A51_RECIPE_GLOW_MASK
    if( true )
    {
#if A51_GEOM_ALPHA_TEST
        if( txDiffuse.SampleGrad( samDiffuse, Input.UV, UVDx, UVDy ).a <= 0.0f )
            discard;
#endif
        Output.Color.a = saturate( EnvParams.x );
        return Output;
    }

#endif
#if A51_SURFACE_INTERFACE & 8
    const float InverseNormalLength = rsqrt( dot( Input.Normal, Input.Normal ) );
    Input.Normal *= InverseNormalLength;
#if A51_RECIPE_ENV == 1
    SurfaceGradients.NormalDx = ( SurfaceGradients.NormalDx - Input.Normal *
        dot( Input.Normal, SurfaceGradients.NormalDx ) ) * InverseNormalLength;
    SurfaceGradients.NormalDy = ( SurfaceGradients.NormalDy - Input.Normal *
        dot( Input.Normal, SurfaceGradients.NormalDy ) ) * InverseNormalLength;
#endif

    if( SurfaceTwoSided && IsFrontFace )
    {
        Input.Normal = -Input.Normal;
#if A51_RECIPE_ENV == 1
        SurfaceGradients.NormalDx = -SurfaceGradients.NormalDx;
        SurfaceGradients.NormalDy = -SurfaceGradients.NormalDy;
#endif
    }

#if A51_RECIPE_DISTORTION
    if( MaterialFlags & ( MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_DISTORTION_PERPOLY_ENV ) )
    {
        Output.Color = GeomShadeDistortionPixel( Input, MaterialFlags, FadeAlpha );
        return Output;
    }
#endif

#endif

    float3 SurfaceNormal = 0;
#if A51_SURFACE_INTERFACE & 8
    SurfaceNormal = Input.Normal;
#endif
    float3 GeometricNormal = SurfaceNormal;
#if A51_RECIPE_SHADOWS
    GeometricNormal = cross( Gradients.Dx, Gradients.Dy );
    const float  GeometricNormalLengthSq = dot( GeometricNormal, GeometricNormal );
    if( GeometricNormalLengthSq > 1e-12f )
    {
        GeometricNormal *= rsqrt( GeometricNormalLengthSq );
        if( dot( GeometricNormal, Input.Normal ) < 0.0f )
        {
            GeometricNormal = -GeometricNormal;
        }
    }
    else
    {
        GeometricNormal = Input.Normal;
    }
#endif

    GeomDiffuseResult Diffuse = GeomEvaluateDiffuse( Input, MaterialFlags, MaterialAlphaRef, UVDx, UVDy );
#if A51_RECIPE_DAMAGE
    const bool ForcedGlow = false;
#else
    const bool ForcedGlow = (MaterialFlags & GEOM_INST_FLAG_GLOWING) != 0u;
#endif
    float4 FinalColor = Diffuse.Color;
    if( !ForcedGlow )
    {
#if !A51_GEOM_SURFACE_ONLY
        GeomApplyEnvironment( Diffuse, MaterialFlags, SurfaceNormal, EnvironmentColor, SurfaceGradients );
#endif
        FinalColor = Diffuse.Color;
#if A51_RECIPE_LIGHTING
        const GeomLightingResult Lighting =
            GeomComputeLighting( Input, MaterialFlags, Diffuse.Sample.a, GeometricNormal, Gradients );
        FinalColor = float4( Diffuse.Color.rgb * Lighting.Diffuse + Lighting.Specular, Diffuse.Color.a );
#else
        A51Color3 Ambient = (A51Color3)GeomGetLightAmbCol( Input ).rgb;
#if GEOM_HAS_VERTEX_COLOR
        Ambient = (A51Color3)( Ambient + Input.Color.rgb );
#endif
#if A51_GEOM_SCREEN_AO
        Ambient = (A51Color3)( Ambient * GeomAmbientVisibility( Input ) );
#endif
        FinalColor = float4( Diffuse.Color.rgb * Ambient, Diffuse.Color.a );
#endif
#if A51_RECIPE_PROJECTION
        if( MaterialFlags & GEOM_INST_FLAG_PROJ_LIGHT )
        {
            FinalColor.rgb = ApplyProjLights( FinalColor.rgb, Input.WorldPos, Gradients );
        }

        if( MaterialFlags & GEOM_INST_FLAG_PROJ_SHADOW )
        {
            FinalColor.rgb = ApplyProjShadows( FinalColor.rgb, Input.WorldPos, Gradients );
        }

#endif
        FinalColor.rgb = GeomApplyFilterLight( FinalColor.rgb, MaterialFlags );
        FinalColor.rgb = saturate( FinalColor.rgb );
    }

#if A51_RECIPE_DAMAGE
    float const Fray = ClothDamageEdge( txDamage, samDamage, Input.UV, Diffuse.Damage, UVAnim.xy );
    float2 const ThreadUV = Input.UV / max( UVAnim.xy / 1.5f, float2( 1e-5f, 1e-5f ) );
    float const Yarn = 0.5f + 0.5f * sin( ThreadUV.x * 2.1f + sin( ThreadUV.y * 0.7f ) );

    // A subdued fabric lip keeps the original colour and lighting. Avoid a
    // bright uniform outline, which reads as a burn or a glowing dissolve.
    float3 const EdgeColor = FinalColor.rgb * lerp( 0.72f, 1.12f, Yarn );
    FinalColor.rgb = saturate( lerp( FinalColor.rgb, EdgeColor, Fray * 0.7f ) );
#endif
#if A51_RECIPE_DAMAGE
    Output.Bloom = float4(FinalColor.rgb, A51_RECIPE_FORCED_GLOW ? 1.0f : 0.0f);
#else
    Output.Bloom = GeomComputeGlow(Input, MaterialFlags, FinalColor, Diffuse.Sample);
#endif
    Output.Color = FinalColor;

    GeomApplyFadeAlpha( Output, MaterialFlags, FadeAlpha );

    return Output;
#endif
}

//==============================================================================

float4 GeomComposeSceneColor( GEOM_PIXEL_INPUT Input, bool IsFrontFace, float LinearDepth )
{
    const GeomPixelResult Shaded = ShadeGeometryPixel( Input, IsFrontFace );
#if A51_RECIPE_GLOW_MASK
    return Shaded.Color;
#endif

#if A51_RECIPE_FOG
    float4 Color = ApplyFogToColor( Shaded.Color, LinearDepth );
#else
    float4 Color = Shaded.Color;
#endif
    const uint MaterialFlags = GeomGetMaterialFlags( Input );
    const bool NeedsCoverageAlpha = A51_RECIPE_COVERAGE_ALPHA != 0;
    if( NeedsCoverageAlpha )
    {
        Color.rgb *= Shaded.Color.a;
    }
    Color.a = NeedsCoverageAlpha ? Shaded.Color.a : Shaded.Bloom.a;
    return Color;
}

//==============================================================================
#endif // GEOM_PIXEL_SHADING_HLSL
//==============================================================================
