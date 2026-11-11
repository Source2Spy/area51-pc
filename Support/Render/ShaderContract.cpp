//=========================================================================
//
//  ShaderContract.cpp
//
//  Shared shader rules for drawing and prewarm.
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ShaderContract.hpp"
#include "a51_shader_combos.hpp"
#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

ShaderProgramKey ResolveShaderProgram( const MaterialDesc&       Material,
                                       ShaderFamily              Family,
                                       VertexContract            Vertex,
                                       const DrawShaderFeatures& Draw,
                                       const ShaderPassContract& Pass )
{
    auto        Key   = AssembleShaderProgram( Material, Family, Vertex, Draw, Pass );
    const xbool Valid = A51NormalizeSurfaceCombo( Key );
    ASSERTS( Valid, "Invalid surface combo inputs" );
    if( !Valid )
    {
        Key.Family = static_cast<ShaderFamily>( 7 );
    }

    return Key;
}

//=========================================================================

ShaderProgramKey ResolveShadowProgram( VertexContract Vertex, xbool Masked )
{
    ShaderProgramKey Key;
    Key.Family    = ShaderFamily::Shadow;
    Key.Vertex    = Vertex;
    Key.Outputs   = ShaderOutputs::ShadowDepth;
    Key.AlphaTest = Masked;

    const xbool Valid = Vertex == VertexContract::Rigid  ? A51NormalizeShadowRigidCombo( Key )
                        : Vertex == VertexContract::Skin ? A51NormalizeShadowSkinCombo( Key )
                                                         : FALSE;
    ASSERTS( Valid, "Invalid shadow combo inputs" );
    if( !Valid )
    {
        Key.Family = static_cast<ShaderFamily>( 7 );
    }

    return Key;
}

//=========================================================================

ShaderProgramKey ResolveDecalProgram( ShaderBlend       Blend,
                                      xbool             Environment,
                                      xbool             Glow,
                                      xbool             Fog,
                                      xbool             Shadows,
                                      LocalLightingMode Lighting )
{
    ShaderProgramKey Key;
    Key.Family      = ShaderFamily::Decal;
    Key.Vertex      = VertexContract::Decal;
    Key.Blend       = Blend;
    Key.Environment = Environment ? EnvironmentSource::Cube : EnvironmentSource::None;
    Key.ForcedGlow  = Glow;
    Key.Fog         = Fog;
    Key.Shadows     = Shadows;
    Key.Lighting    = Lighting == LocalLightingMode::Ambient ? LightingFunctions::Ambient : LightingFunctions::Surface;
    Key.Cookies     = Lighting == LocalLightingMode::Cookies;

    const xbool Valid = A51NormalizeDecalCombo( Key );
    ASSERTS( Valid, "Invalid decal combo inputs" );
    if( !Valid )
    {
        Key.Family = static_cast<ShaderFamily>( 7 );
    }

    return Key;
}

//=========================================================================

ShaderProgramKey ResolvePrimitiveProgram( SurfaceRecipe Surface,
                                          xbool         Emission,
                                          ShaderBlend   Blend,
                                          xbool         WriteSceneDepth,
                                          xbool         Fog,
                                          xbool         InstancedSprite )
{
    ShaderProgramKey Key;
    Key.Family          = ShaderFamily::Primitive;
    Key.Vertex          = VertexContract::Primitive;
    Key.Surface         = Surface;
    Key.Blend           = Blend;
    Key.InstancedSprite = InstancedSprite;
    Key.Outputs         = WriteSceneDepth ? ShaderOutputs::SceneColorDepth : ShaderOutputs::SceneColor;
    Key.ForcedGlow      = Emission;
    Key.Fog             = Fog;

    const xbool Valid = A51NormalizePrimitiveCombo( Key );
    ASSERTS( Valid, "Invalid primitive combo inputs" );
    if( !Valid )
    {
        Key.Family = static_cast<ShaderFamily>( 7 );
    }

    return Key;
}

} // namespace render
