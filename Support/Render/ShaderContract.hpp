//=========================================================================
//
//  ShaderContract.hpp
//
//  Surface semantics and shader program identity.
//
//=========================================================================

#ifndef RENDER_SHADER_CONTRACT_HPP
#define RENDER_SHADER_CONTRACT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "Material_Prefs.hpp"
#include "RenderLighting.hpp"

namespace render
{

//=========================================================================
//  ENUMS
//=========================================================================

enum class SurfaceRecipe : u8
{
    Diffuse,
    Distortion
};

//-------------------------------------------------------------------------

enum class EnvironmentSource : u8
{
    None,
    Texture2D,
    Cube
};

//-------------------------------------------------------------------------

enum class EnvironmentCoordinates : u8
{
    Reflection,
    View,
    World
};

//-------------------------------------------------------------------------

enum class EnvironmentBlend : u8
{
    Diffuse,
    Alpha,
    Distortion
};

//-------------------------------------------------------------------------

enum class IlluminationMode : u8
{
    None,
    DiffusePixel,
    AlphaPixel,
    AlphaPolygon
};

//-------------------------------------------------------------------------

enum class VertexContract : u8
{
    Rigid,
    Skin,
    Dynamic,
    Cloth,
    Decal,
    Primitive,
    Fullscreen,
    UI
};

//-------------------------------------------------------------------------

enum class ShaderFamily : u8
{
    Surface,
    Decal,
    Primitive,
    Shadow,
    UI,
    Composite,
    PostProcess
};

//-------------------------------------------------------------------------

enum class ShaderOutputs : u8
{
    SceneColor,
    SceneColorDepth,
    GlowMask,
    Depth,
    ShadowDepth,
    ShadowEVSM,
    LinearDepth,
    LinearDepthNormals
};

//-------------------------------------------------------------------------

enum class LightingFunctions : u8
{
    Ambient,
    Surface
};

//-------------------------------------------------------------------------

enum class LocalLightingMode : u8
{
    Ambient,
    Local,
    Cookies
};

//-------------------------------------------------------------------------

enum class ShaderBlend : u8
{
    Opaque,
    Alpha,
    Additive,
    Subtractive,
    Intensity
};

//=========================================================================
//  STRUCTS
//=========================================================================

struct MaterialDesc
{
    SurfaceRecipe          Surface                 = SurfaceRecipe::Diffuse;
    EnvironmentSource      Environment             = EnvironmentSource::None;
    EnvironmentCoordinates EnvironmentUV           = EnvironmentCoordinates::Reflection;
    EnvironmentBlend       EnvironmentMix          = EnvironmentBlend::Diffuse;
    IlluminationMode       Illumination            = IlluminationMode::None;
    xbool                  Detail                  = FALSE;
    xbool                  AlphaTest               = FALSE;
    xbool                  AlphaBlend              = FALSE;
    xbool                  TwoSidedShading         = FALSE;
    xbool                  IlluminationUsesDiffuse = FALSE;
    xbool                  AdditiveEnvironment     = FALSE;
    xbool                  ReceivesProjection      = FALSE;
    xbool                  ReceivesShadow          = FALSE;
    xbool                  Valid                   = FALSE;
};

//-------------------------------------------------------------------------

struct DrawShaderFeatures
{
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    void SetLocalLighting( LocalLightingMode Mode );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    LightingFunctions Lighting       = LightingFunctions::Surface;
    xbool             Cookies        = FALSE;
    xbool             Projection     = FALSE;
    xbool             Shadows        = FALSE;
    xbool             Fog            = FALSE;
    xbool             ForcedGlow     = FALSE;
    xbool             DamageCoverage = FALSE;
    xbool             Fade           = FALSE;
};

//-------------------------------------------------------------------------

struct ShaderPassContract
{
    ShaderOutputs Outputs = ShaderOutputs::SceneColor;
};

//-------------------------------------------------------------------------

struct ShaderProgramKey
{
    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    u64 Hash( void ) const;

    xbool operator==( const ShaderProgramKey& Other ) const;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    ShaderFamily           Family              = ShaderFamily::Surface;
    VertexContract         Vertex              = VertexContract::Rigid;
    SurfaceRecipe          Surface             = SurfaceRecipe::Diffuse;
    EnvironmentSource      Environment         = EnvironmentSource::None;
    EnvironmentCoordinates EnvironmentUV       = EnvironmentCoordinates::Reflection;
    EnvironmentBlend       EnvironmentMix      = EnvironmentBlend::Diffuse;
    IlluminationMode       Illumination        = IlluminationMode::None;
    ShaderOutputs          Outputs             = ShaderOutputs::SceneColor;
    LightingFunctions      Lighting            = LightingFunctions::Ambient;
    xbool                  Detail              = FALSE;
    xbool                  AlphaTest           = FALSE;
    xbool                  AdditiveEnvironment = FALSE;
    xbool                  Cookies             = FALSE;
    xbool                  Projection          = FALSE;
    xbool                  Shadows             = FALSE;
    xbool                  Fog                 = FALSE;
    xbool                  ForcedGlow          = FALSE;
    xbool                  DamageCoverage      = FALSE;
    xbool                  CoverageAlpha       = FALSE;
    xbool                  InstancedSprite     = FALSE;
    ShaderBlend            Blend               = ShaderBlend::Opaque;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

LocalLightingMode ResolveLocalLighting( const cb_geom_lighting* pLighting );

xbool HasLocalShadows( const cb_geom_lighting* pLighting, const u32* pShadowIndices, u32 IndexCount );

MaterialDesc ResolveMaterialDescription( material_type Type, u32 Flags );

ShaderProgramKey AssembleShaderProgram( const MaterialDesc&       Material,
                                        ShaderFamily              Family,
                                        VertexContract            Vertex,
                                        const DrawShaderFeatures& Draw,
                                        const ShaderPassContract& Pass );

ShaderProgramKey ResolveShaderProgram( const MaterialDesc&       Material,
                                       ShaderFamily              Family,
                                       VertexContract            Vertex,
                                       const DrawShaderFeatures& Draw,
                                       const ShaderPassContract& Pass );
ShaderProgramKey ResolveShadowProgram( VertexContract Vertex, xbool Masked );
ShaderProgramKey ResolveDecalProgram( ShaderBlend       Blend,
                                      xbool             Environment,
                                      xbool             Glow,
                                      xbool             Fog,
                                      xbool             Shadows,
                                      LocalLightingMode Lighting );
ShaderProgramKey ResolvePrimitiveProgram( SurfaceRecipe Surface,
                                          xbool         Emission,
                                          ShaderBlend   Blend,
                                          xbool         WriteSceneDepth,
                                          xbool         Fog,
                                          xbool         InstancedSprite = FALSE );

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
LocalLightingMode ResolveLocalLighting( const cb_geom_lighting* pLighting )
{
    if( !pLighting || pLighting->LightCount <= 0 )
    {
        return LocalLightingMode::Ambient;
    }

    const s32 Count = MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );

    for( s32 i = 0; i < Count; ++i )
    {
        const f32 Type = pLighting->LightDir[i].GetW();

        if( Type >= 0.5f && Type < 1.5f && pLighting->LightCookieU[i].GetW() > 0.0f )
        {
            return LocalLightingMode::Cookies;
        }
    }

    return LocalLightingMode::Local;
}

//=========================================================================

inline
xbool HasLocalShadows( const cb_geom_lighting* pLighting, const u32* pShadowIndices, u32 IndexCount )
{
    if( !pLighting )
    {
        return FALSE;
    }

    const s32 Count = MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );

    for( s32 i = 0; i < Count; ++i )
    {
        const s32 Index = pLighting->DynamicLightIndex[i];

        if( pLighting->LightDir[i].GetW() < 1.5f && Index >= 0 && static_cast<u32>( Index ) < IndexCount &&
            pShadowIndices[Index] != 0xffffffffu )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

inline
MaterialDesc ResolveMaterialDescription( material_type Type, u32 Flags )
{
    MaterialDesc Desc;

    if( Type <= Material_Not_Used || Type >= Material_NumTypes )
    {
        return Desc;
    }

    const xbool Environment = Type == Material_Diff_PerPixelEnv || Type == Material_Alpha_PerPolyEnv ||
                              Type == Material_Distortion_PerPolyEnv;
    const xbool Distortion = Type == Material_Distortion || Type == Material_Distortion_PerPolyEnv;
    const xbool Alpha      = Type == Material_Alpha || Type == Material_Alpha_PerPolyEnv ||
                        Type == Material_Alpha_PerPixelIllum || Type == Material_Alpha_PerPolyIllum || Distortion;
    const xbool Additive    = ( Flags & MATERIAL_FLAG_IS_ADDITIVE ) != 0;
    const xbool Subtractive = ( Flags & MATERIAL_FLAG_IS_SUBTRACTIVE ) != 0;

    if( Additive && Subtractive )
    {
        return Desc;
    }

    if( Environment && ( !( Flags & ( MATERIAL_FLAG_HAS_ENV_MAP | MATERIAL_FLAG_ENV_CUBE_MAP ) ) ||
                         ( ( Flags & MATERIAL_FLAG_ENV_WORLD_SPACE ) && ( Flags & MATERIAL_FLAG_ENV_VIEW_SPACE ) ) ) )
    {
        return Desc;
    }

    // Resolve surface and environment inputs.
    Desc.Surface = Distortion ? SurfaceRecipe::Distortion : SurfaceRecipe::Diffuse;

    if( Environment )
    {
        Desc.Environment =
            ( Flags & MATERIAL_FLAG_ENV_CUBE_MAP ) ? EnvironmentSource::Cube : EnvironmentSource::Texture2D;
        Desc.EnvironmentUV =
            ( Flags & MATERIAL_FLAG_ENV_VIEW_SPACE ) ? EnvironmentCoordinates::View : EnvironmentCoordinates::World;
        Desc.EnvironmentMix = Distortion                          ? EnvironmentBlend::Distortion
                              : Type == Material_Alpha_PerPolyEnv ? EnvironmentBlend::Alpha
                                                                  : EnvironmentBlend::Diffuse;
    }

    switch( Type )
    {
        case Material_Diff_PerPixelIllum:
        {
            Desc.Illumination = IlluminationMode::DiffusePixel;
            break;
        }
        case Material_Alpha_PerPixelIllum:
        {
            Desc.Illumination = IlluminationMode::AlphaPixel;
            break;
        }
        case Material_Alpha_PerPolyIllum:
        {
            Desc.Illumination = IlluminationMode::AlphaPolygon;
            break;
        }
        default:
        {
            break;
        }
    }

    // Derive coverage, lighting and depth policies.
    const xbool PunchThru        = ( Flags & MATERIAL_FLAG_IS_PUNCH_THRU ) != 0;
    const xbool ForceZFill       = ( Flags & MATERIAL_FLAG_FORCE_ZFILL ) != 0;
    Desc.Detail                  = ( Flags & MATERIAL_FLAG_HAS_DETAIL_MAP ) != 0;
    Desc.AlphaTest               = PunchThru || Type == Material_Alpha;
    Desc.AlphaBlend              = Alpha;
    Desc.TwoSidedShading         = ( Flags & MATERIAL_FLAG_DOUBLE_SIDED ) != 0;
    Desc.IlluminationUsesDiffuse = ( Flags & MATERIAL_FLAG_ILLUM_USES_DIFFUSE ) != 0;
    Desc.AdditiveEnvironment     = Additive;
    Desc.ReceivesProjection      = !Alpha || ( ForceZFill && !Subtractive );
    Desc.ReceivesShadow          = ( !Alpha || ForceZFill ) && !PunchThru;
    Desc.Valid                   = TRUE;

    return Desc;
}

//=========================================================================

// Generated rules normalize these inputs for draw and prewarm.
inline
ShaderProgramKey AssembleShaderProgram( const MaterialDesc&       Material,
                                        ShaderFamily              Family,
                                        VertexContract            Vertex,
                                        const DrawShaderFeatures& Draw,
                                        const ShaderPassContract& Pass )
{
    ShaderProgramKey Key;
    Key.Family  = Family;
    Key.Vertex  = Vertex;
    Key.Outputs = Pass.Outputs;

    Key.Surface             = Material.Surface;
    Key.Environment         = Material.Environment;
    Key.EnvironmentUV       = Material.EnvironmentUV;
    Key.EnvironmentMix      = Material.EnvironmentMix;
    Key.AdditiveEnvironment = Material.AdditiveEnvironment;
    Key.Illumination        = Material.Illumination;
    Key.Detail              = Material.Detail;

    Key.AlphaTest      = Material.AlphaTest;
    Key.DamageCoverage = Draw.DamageCoverage;
    Key.CoverageAlpha  = Material.AlphaBlend || Draw.Fade;
    Key.Lighting       = Draw.Lighting;
    Key.Cookies        = Draw.Cookies;

    Key.Projection = Material.ReceivesProjection && Draw.Projection;
    Key.Shadows    = Material.ReceivesShadow && Draw.Shadows;
    Key.Fog        = Draw.Fog;
    Key.ForcedGlow = Draw.ForcedGlow;

    return Key;
}

//=========================================================================

inline
void DrawShaderFeatures::SetLocalLighting( LocalLightingMode Mode )
{
    Lighting = Mode == LocalLightingMode::Ambient ? LightingFunctions::Ambient : LightingFunctions::Surface;
    Cookies  = Mode == LocalLightingMode::Cookies;
}

//=========================================================================

inline
u64 ShaderProgramKey::Hash( void ) const
{
    return static_cast<u64>( Family ) |
           ( static_cast<u64>( Vertex ) << 3 ) |
           ( static_cast<u64>( Surface ) << 6 ) |
           ( static_cast<u64>( Environment ) << 7 ) |
           ( static_cast<u64>( EnvironmentUV ) << 9 ) |
           ( static_cast<u64>( EnvironmentMix ) << 11 ) |
           ( static_cast<u64>( Illumination ) << 13 ) |
           ( static_cast<u64>( Outputs ) << 15 ) |
           ( static_cast<u64>( Lighting ) << 19 ) |
           ( static_cast<u64>( !!Detail ) << 20 ) |
           ( static_cast<u64>( !!AlphaTest ) << 21 ) |
           ( static_cast<u64>( !!Cookies ) << 22 ) |
           ( static_cast<u64>( !!Projection ) << 25 ) |
           ( static_cast<u64>( !!Shadows ) << 26 ) |
           ( static_cast<u64>( !!Fog ) << 27 ) |
           ( static_cast<u64>( !!ForcedGlow ) << 28 ) |
           ( static_cast<u64>( !!DamageCoverage ) << 29 ) |
           ( static_cast<u64>( !!AdditiveEnvironment ) << 30 ) |
           ( static_cast<u64>( !!CoverageAlpha ) << 31 ) |
           ( static_cast<u64>( Blend ) << 32 ) |
           ( static_cast<u64>( !!InstancedSprite ) << 35 );
}

//=========================================================================

inline
xbool ShaderProgramKey::operator==( const ShaderProgramKey& Other ) const
{
    return Family == Other.Family &&
           Vertex == Other.Vertex &&
           Surface == Other.Surface &&
           Environment == Other.Environment &&
           EnvironmentUV == Other.EnvironmentUV &&
           EnvironmentMix == Other.EnvironmentMix &&
           Illumination == Other.Illumination &&
           Outputs == Other.Outputs &&
           Lighting == Other.Lighting &&
           Detail == Other.Detail &&
           AlphaTest == Other.AlphaTest &&
           Cookies == Other.Cookies &&
           Projection == Other.Projection &&
           Shadows == Other.Shadows &&
           Fog == Other.Fog &&
           ForcedGlow == Other.ForcedGlow &&
           DamageCoverage == Other.DamageCoverage &&
           AdditiveEnvironment == Other.AdditiveEnvironment &&
           CoverageAlpha == Other.CoverageAlpha &&
           Blend == Other.Blend &&
           InstancedSprite == Other.InstancedSprite;
}

} // namespace render

#endif // RENDER_SHADER_CONTRACT_HPP
