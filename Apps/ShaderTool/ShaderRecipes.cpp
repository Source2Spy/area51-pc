// Allowed surface compositions are normalized by the same resolver as draw.
#include "ShaderTool.hpp"
#include "../../Support/Render/ShaderContract.hpp"
#include <map>
#include <algorithm>
#include <filesystem>
#include <set>
#include <string>

namespace
{
using namespace render;

std::vector<shader_combo_profile> MaterialProfiles( VertexContract Vertex )
{
    std::vector<shader_combo_profile> Profiles;
    const auto                        Add = [&]( const MaterialDesc& Material )
    {
        shader_combo_profile Profile;
        Profile["Vertex"]              = { static_cast<s64>( Vertex ) };
        Profile["Surface"]             = { static_cast<s64>( Material.Surface ) };
        Profile["Environment"]         = { static_cast<s64>( Material.Environment ) };
        Profile["EnvironmentUV"]       = { static_cast<s64>( Material.EnvironmentUV ) };
        Profile["EnvironmentMix"]      = { static_cast<s64>( Material.EnvironmentMix ) };
        Profile["Illumination"]        = { static_cast<s64>( Material.Illumination ) };
        Profile["Detail"]              = { !!Material.Detail };
        Profile["AlphaTest"]           = { !!Material.AlphaTest };
        Profile["AdditiveEnvironment"] = { !!Material.AdditiveEnvironment };
        Profile["Projection"]          = Material.ReceivesProjection ? std::vector<s64>{ 0, 1 } : std::vector<s64>{ 0 };
        Profile["Shadows"]             = Material.ReceivesShadow ? std::vector<s64>{ 0, 1 } : std::vector<s64>{ 0 };
        Profile["Lighting"]            = { 0, 1 };
        Profile["Cookies"]             = { 0, 1 };
        Profile["Damage"]              = { Vertex == VertexContract::Dynamic };
        Profile["CoverageAlpha"]       = Material.AlphaBlend ? std::vector<s64>{ 1 } : std::vector<s64>{ 0, 1 };
        Profile["Outputs"]             = Vertex == VertexContract::Dynamic ? std::vector<s64>{ 0, 1, 6, 7 } : std::vector<s64>{ 0, 1, 2, 3, 6, 7 };

        if( Vertex == VertexContract::Dynamic )
            Profile["CoverageAlpha"] = { 0 };
        if( std::find( Profiles.begin(), Profiles.end(), Profile ) == Profiles.end() )
            Profiles.push_back( std::move( Profile ) );
    };

    if( Vertex == VertexContract::Dynamic )
    {
        MaterialDesc Material;
        Material.Valid              = TRUE;
        Material.AlphaTest          = TRUE;
        Material.ReceivesProjection = TRUE;
        Material.ReceivesShadow     = TRUE;
        Add( Material );
    }
    else
    {
        for( u32 Type = Material_Diff; Type < Material_NumTypes; ++Type )
            for( u32 Flags = 0; Flags < ( MATERIAL_FLAG_IS_SUBTRACTIVE << 1 ); ++Flags )
            {
                const auto Material = ResolveMaterialDescription( static_cast<material_type>( Type ), Flags );

                if( Material.Valid )
                    Add( Material );
            }
    }

    return Profiles;
}

//==============================================================================

void AddStage( shader_script& Script, std::set<std::string>& Names, const xstring& Name,
               const xstring& Source, xbool Vertex, const xstring& Options )
{
    if( !Names.insert( (const char*)Name ).second )
        return;

    shader_entry& Stage   = Script.Shaders.Append();
    Stage.Name            = Name;
    Stage.Source          = Source;
    Stage.Stage           = Vertex ? "vertex" : "pixel";
    Stage.StageShort      = Vertex ? "vs" : "ps";
    Stage.Entry           = Vertex ? "VSMain" : "PSMain";
    Stage.Profile         = Vertex ? "vs_6_0" : "ps_6_0";
    Stage.CommonArgs      = Options;
    Stage.ColorArithmetic = !Vertex;
}

//==============================================================================

xbool ExpandDeclaredRecipes( shader_script& Script, std::set<std::string>& Names,
                             const shader_surface_family& Family )
{
    const shader_combo_desc* Desc = nullptr;

    for( const auto& Combo : Script.Combos )
        if( Combo.Name == (const char*)Family.Recipe )
            Desc = &Combo;
    if( !Desc )
    {
        x_printf( "Error: family '%s' has no combo declaration\n", (const char*)Family.Vertex );
        return FALSE;
    }

    shader_combo_schema               Schema;
    std::vector<shader_combo_variant> Variants;
    std::string                       Error;

    if( !Schema.Compile( *Desc, Error ) ||
        !( Desc->MaterialProfiles ? Schema.Enumerate( MaterialProfiles( Family.Vertex == "Skin" ? VertexContract::Skin : Family.Vertex == "Dynamic" ? VertexContract::Dynamic
                                                                                                                                                    : VertexContract::Rigid ),
                                                      Variants, Error )
                                  : Schema.Enumerate( Variants, Error ) ) )
    {
        x_printf( "Error: family '%s': %s\n", (const char*)Family.Vertex, Error.c_str() );
        return FALSE;
    }

    const auto StageName = [&]( const std::string& Options, xbool Vertex ) -> xstring
    {
        std::string Identity = Options;
        Identity += Vertex ? ":vertex" : ":pixel";
        const xstring& Source = Vertex ? Family.Source : Family.PixelSource;
        Identity += std::filesystem::path( (const char*)Source ).lexically_relative( std::filesystem::path( (const char*)Script.SourceRoot ) ).generic_string();
        const auto        Digest   = ecs_HashContent( reinterpret_cast<const byte*>( Identity.data() ), static_cast<u32>( Identity.size() ) );
        std::string       Name     = "combo_";
        static const char Digits[] = "0123456789abcdef";

        for( s32 i = 0; i < 32; ++i )
        {
            Name += Digits[Digest[i] >> 4];
            Name += Digits[Digest[i] & 15];
        }

        Name += Vertex ? "_vs" : "_ps";
        return xstring( Name.c_str() );
    };

    for( const auto& Variant : Variants )
    {
        const xstring VertexName = StageName( Variant.VertexOptions, TRUE );
        const xstring PixelName  = StageName( Variant.PixelOptions, FALSE );
        AddStage( Script, Names, VertexName, Family.Source, TRUE, Variant.VertexOptions.c_str() );
        AddStage( Script, Names, PixelName, Family.PixelSource, FALSE, Variant.PixelOptions.c_str() );
        shader_program_recipe& Program = Script.Programs.Append();
        Program.Name                   = xstring( xfs( "combo_%s_%016llx", (const char*)Family.Vertex, (unsigned long long)Variant.Key ) );
        Program.Kind                   = SHADER_COMBO_KEYED;
        Program.Package                = Family.Package;
        Program.Vertex                 = VertexName;
        Program.Pixel                  = PixelName;
        Program.Key                    = Variant.Key;
    }

    return TRUE;
}

}

//==============================================================================

xbool ExpandSurfaceRecipes( shader_script& Script )
{
    std::set<std::string> StageNames;

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
        StageNames.insert( (const char*)Script.Shaders[i].Name );
    for( s32 FamilyIndex = Script.SurfaceFamilies.GetCount() - 1; FamilyIndex >= 0; --FamilyIndex )
    {
        const shader_surface_family& Family = Script.SurfaceFamilies[FamilyIndex];

        if( !command_line::FileExists( Family.Source ) || !command_line::FileExists( Family.PixelSource ) )
            return FALSE;

        if( !ExpandDeclaredRecipes( Script, StageNames, Family ) )
            return FALSE;
    }

    return TRUE;
}
