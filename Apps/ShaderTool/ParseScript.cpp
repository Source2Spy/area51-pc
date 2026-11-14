//==============================================================================
//
//  ParseScript.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderTool.hpp"
#include <sstream>
#include <limits>

//==============================================================================
//  ENUMS
//==============================================================================

enum parse_section
{
    SECTION_NONE,
    SECTION_SETTINGS,
    SECTION_SHADERS,
    SECTION_BUFFERS,
    SECTION_PROGRAMS,
    SECTION_SURFACES,
    SECTION_INTERFACES,
    SECTION_RESOURCES,
    SECTION_PACKAGES,
    SECTION_COMBOS,
};

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static
xbool IsToken( token_stream& Tokenizer, const char* pText )
{
    return ( Tokenizer.Type() == token_stream::TOKEN_SYMBOL ) &&
           ( x_stricmp( Tokenizer.String(), pText ) == 0 );
}

//==============================================================================

static
xbool ReadValue( token_stream& Tokenizer, xstring& Value )
{
    Tokenizer.Read();

    if( ( Tokenizer.Type() != token_stream::TOKEN_SYMBOL ) &&
        ( Tokenizer.Type() != token_stream::TOKEN_STRING ) &&
        ( Tokenizer.Type() != token_stream::TOKEN_NUMBER ) )
    {
        return FALSE;
    }

    Value = Tokenizer.String();
    return TRUE;
}

//==============================================================================

static
xbool ReadUnsigned( token_stream& Tokenizer, u64& Value )
{
    xstring Text;

    if( !ReadValue( Tokenizer, Text ) || Text.IsEmpty() )
        return FALSE;

    Value = 0;

    for( s32 i = 0; i < Text.GetLength(); ++i )
    {
        if( Text[i] < '0' || Text[i] > '9' || Value > ( std::numeric_limits<u64>::max() - ( Text[i] - '0' ) ) / 10 )
            return FALSE;

        Value = Value * 10 + ( Text[i] - '0' );
    }

    return TRUE;
}

//==============================================================================

static
xstring StageShortName( const xstring& Stage )
{
    if( Stage == "vertex" )
        return "vs";

    if( ( Stage == "pixel" ) || ( Stage == "fragment" ) )
        return "ps";

    if( Stage == "compute" )
        return "cs";

    if( Stage == "geometry" )
        return "gs";

    if( ( Stage == "hull" ) || ( Stage == "tess_control" ) )
        return "hs";

    if( ( Stage == "domain" ) || ( Stage == "tess_evaluation" ) )
        return "ds";

    if( Stage == "mesh" )
        return "ms";

    if( ( Stage == "amplification" ) || ( Stage == "task" ) )
        return "as";

    return "";
}

//==============================================================================

static
u32 ParseTargetMask( const xstring& Text )
{
    const xstring Target = ToLower( Text );

    if( Target == "all" )
        return SHADER_TARGET_ALL;

    if( ( Target == "vulkan" ) )
        return SHADER_TARGET_VULKAN;

    if( ( Target == "d3d12" ) || ( Target == "dx12" ) )
        return SHADER_TARGET_D3D12;

    if( ( Target == "metal" ) || ( Target == "govno" ) )
        return SHADER_TARGET_METAL;

    return 0;
}

//==============================================================================

static
xbool ParseSetting( token_stream& Tokenizer, shader_script& Script )
{
    xstring Key = ToLower( xstring( Tokenizer.String() ) );
    xstring Value;

    if( Key == "config" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Value = ToLower( Value );

        if( ( Value != "debug" ) && ( Value != "release" ) )
            return FALSE;

        Script.Config = Value;
    }
    else if( ( Key == "target" ) || ( Key == "targets" ) )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.TargetMask = ParseTargetMask( Value );

        if( !Script.TargetMask )
            return FALSE;
    }
    else if( ( Key == "binding_model" ) || ( Key == "bindings" ) )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Value = ToLower( Value );

        if( Value == "native" )
            Script.BindingModel = SHADER_BINDING_NATIVE;
        else if( Value == "sdl" )
            Script.BindingModel = SHADER_BINDING_SDL;
        else
            return FALSE;
    }
    else if( Key == "source_root" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.SourceRoot = MakeRelativePath( Script.ScriptDir, Value );
    }
    else if( Key == "abi" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.Abi = MakeRelativePath( Script.ScriptDir, Value );
    }
    else if( Key == "contract" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.Contract = MakeRelativePath( Script.ScriptDir, Value );
    }
    else if( Key == "out_root" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.OutRoot = MakeRelativePath( Script.ScriptDir, Value );
    }
    else
    {
        x_printf( "Error: Unknown setting '%s' in %s line %d\n",
                  Tokenizer.String(),
                  Tokenizer.GetFilename(),
                  Tokenizer.GetLineNumber() );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool ExpectOpenBrace( token_stream& Tokenizer )
{
    Tokenizer.Read();
    return ( Tokenizer.Type() == token_stream::TOKEN_DELIMITER ) &&
           ( Tokenizer.Delimiter() == '{' );
}

//==============================================================================

static
xbool ParseShaderProperties( token_stream& Tokenizer, shader_entry& Shader )
{
    while( Tokenizer.Read() != token_stream::TOKEN_EOF )
    {
        if( ( Tokenizer.Type() == token_stream::TOKEN_DELIMITER ) &&
            ( Tokenizer.Delimiter() == '}' ) )
        {
            return TRUE;
        }

        if( Tokenizer.Type() != token_stream::TOKEN_SYMBOL )
            return FALSE;

        xstring Key = ToLower( xstring( Tokenizer.String() ) );

        if( Key == "common_args" )
        {
            if( !ReadValue( Tokenizer, Shader.CommonArgs ) )
                return FALSE;
        }
        else if( Key == "color_arithmetic" )
        {
            xstring Value;

            if( !ReadValue( Tokenizer, Value ) || Value != "float16" )
                return FALSE;

            Shader.ColorArithmetic = TRUE;
        }
        else if( Key == "vulkan_args" )
        {
            if( !ReadValue( Tokenizer, Shader.VulkanArgs ) )
                return FALSE;
        }
        else if( Key == "d3d12_args" )
        {
            if( !ReadValue( Tokenizer, Shader.D3D12Args ) )
                return FALSE;
        }
        else if( Key == "metal_args" )
        {
            if( !ReadValue( Tokenizer, Shader.MetalArgs ) )
                return FALSE;
        }
        else
        {
            x_printf( "Error: Unknown shader property '%s' in %s line %d\n",
                      Tokenizer.String(),
                      Tokenizer.GetFilename(),
                      Tokenizer.GetLineNumber() );
            return FALSE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool ParseShader( token_stream& Tokenizer, shader_script& Script )
{
    shader_entry Shader;

    xstring Value;

    if( !ReadValue( Tokenizer, Shader.Name ) )
        return FALSE;

    if( !ReadValue( Tokenizer, Shader.Stage ) )
        return FALSE;

    Shader.Stage      = ToLower( Shader.Stage );
    Shader.StageShort = StageShortName( Shader.Stage );

    if( Shader.StageShort.IsEmpty() )
        return FALSE;

    if( !ReadValue( Tokenizer, Value ) )
        return FALSE;

    if( Script.SourceRoot.IsEmpty() )
        Shader.Source = MakeRelativePath( Script.ScriptDir, Value );
    else
        Shader.Source = MakeRelativePath( Script.SourceRoot, Value );

    if( !ReadValue( Tokenizer, Shader.Entry ) )
        return FALSE;

    if( !ReadValue( Tokenizer, Shader.Profile ) )
        return FALSE;

    xstring ProfileStage = Shader.Profile.Left( 2 );
    ProfileStage.MakeLower();

    if( ProfileStage != Shader.StageShort )
    {
        x_printf( "Error: Shader stage '%s' does not match profile '%s' in %s line %d\n",
                  (const char*)Shader.Stage,
                  (const char*)Shader.Profile,
                  Tokenizer.GetFilename(),
                  Tokenizer.GetLineNumber() );
        return FALSE;
    }

    if( !ExpectOpenBrace( Tokenizer ) )
        return FALSE;

    if( !ParseShaderProperties( Tokenizer, Shader ) )
        return FALSE;

    if( !command_line::FileExists( (const char*)Shader.Source ) )
    {
        x_printf( "Error: Shader source does not exist: %s\n", (const char*)Shader.Source );
        return FALSE;
    }

    if( Shader.Stage == "compute" )
    {
        Script.ComputeShaders.Append() = Shader;
    }
    else
    {
        Script.Shaders.Append() = Shader;
    }

    return TRUE;
}

//==============================================================================

static
xbool ParseBuffer( token_stream& Tokenizer, shader_script& Script )
{
    shader_contract_buffer& Buffer = Script.Buffers.Append();

    if( !ReadValue( Tokenizer, Buffer.HlslName ) || !ReadValue( Tokenizer, Buffer.CppName ) ||
        !ReadValue( Tokenizer, Buffer.Resource ) || !ExpectOpenBrace( Tokenizer ) )
        return FALSE;

    Buffer.Size = 0;

    while( Tokenizer.Read() != token_stream::TOKEN_EOF )
    {
        if( Tokenizer.Type() == token_stream::TOKEN_DELIMITER && Tokenizer.Delimiter() == '}' )
            return TRUE;

        shader_contract_field& Field = Buffer.Fields.Append();
        Field.Type                   = Tokenizer.String();
        xstring Count;

        if( !ReadValue( Tokenizer, Field.Name ) || !ReadValue( Tokenizer, Count ) )
            return FALSE;

        const s32 Alias = Field.Name.Find( '=' );
        Field.CppName   = Alias >= 0 ? Field.Name.Right( Field.Name.GetLength() - Alias - 1 ) : Field.Name;

        if( Alias >= 0 )
            Field.Name = Field.Name.Left( Alias );
        Field.Count = x_atoi( Count );

        if( Field.Count == 0 || Field.Count > 1024 )
            return FALSE;

        u32 ElementSize = Field.Type == "float4" || Field.Type == "uint4" || Field.Type == "float4_values" ? 16 : Field.Type == "uint2" || Field.Type == "float2_values"          ? 8
                                                                                                              : Field.Type == "matrix4"                                           ? 64
                                                                                                              : Field.Type == "u32" || Field.Type == "s32" || Field.Type == "f32" ? 4
                                                                                                                                                                                  : 0;
        if( !ElementSize )
            return FALSE;

        const u32 Alignment = ElementSize >= 16 ? 16 : ElementSize == 8 ? 8
                                                                        : 4;
        Buffer.Size         = ( Buffer.Size + Alignment - 1 ) & ~( Alignment - 1 );
        Field.Offset        = Buffer.Size;
        Field.Size          = ElementSize * Field.Count;
        Buffer.Size += Field.Size;
    }

    return FALSE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

static
xbool ParseCombo( token_stream& Tokenizer, shader_script& Script )
{
    shader_combo_desc Desc;
    xstring           Name;

    if( !IsToken( Tokenizer, "combo" ) || !ReadValue( Tokenizer, Name ) || !ExpectOpenBrace( Tokenizer ) )
        return FALSE;

    Desc.Name     = (const char*)Name;
    xbool HasBase = FALSE;

    while( Tokenizer.Read() != token_stream::TOKEN_EOF )
    {
        if( Tokenizer.Type() == token_stream::TOKEN_DELIMITER && Tokenizer.Delimiter() == '}' )
            break;
        if( IsToken( Tokenizer, "base" ) )
        {
            if( HasBase || !ReadUnsigned( Tokenizer, Desc.Base ) )
                return FALSE;

            HasBase = TRUE;
        }
        else if( IsToken( Tokenizer, "material_profiles" ) )
        {
            if( Desc.MaterialProfiles || !ReadValue( Tokenizer, Name ) || Name != "assets" )
                return FALSE;

            Desc.MaterialProfiles = TRUE;
        }
        else if( IsToken( Tokenizer, "axis" ) )
        {
            shader_combo_axis Axis;
            xstring           Scope, Field, Domain;
            u64               Shift = 0, Bits = 0;

            if( !ReadValue( Tokenizer, Name ) || !ReadValue( Tokenizer, Scope ) || !ReadValue( Tokenizer, Field ) ||
                !ReadUnsigned( Tokenizer, Shift ) || !ReadUnsigned( Tokenizer, Bits ) ||
                Shift > 63 || Bits > 63 || !ReadValue( Tokenizer, Domain ) )
                return FALSE;

            Axis.Name  = (const char*)Name;
            Axis.Field = (const char*)Field;
            Axis.Shift = static_cast<u32>( Shift );
            Axis.Bits  = static_cast<u32>( Bits );

            if( Scope == "material" )
                Axis.Scope = shader_combo_scope::Material;
            else if( Scope == "draw" )
                Axis.Scope = shader_combo_scope::Draw;
            else if( Scope == "pass" )
                Axis.Scope = shader_combo_scope::Pass;
            else
                return FALSE;

            const struct
            {
                const char* Name;
                u32         Shift;
                u32         Bits;
            } Fields[] = {
                { "Family", 0, 3 }, { "Vertex", 3, 3 }, { "Surface", 6, 1 }, { "Environment", 7, 2 }, { "EnvironmentUV", 9, 2 }, { "EnvironmentMix", 11, 2 }, { "Illumination", 13, 2 }, { "Outputs", 15, 3 }, { "Lighting", 19, 1 }, { "Detail", 20, 1 }, { "AlphaTest", 21, 1 }, { "Projection", 25, 1 }, { "Shadows", 26, 1 }, { "Fog", 27, 1 }, { "ForcedGlow", 28, 1 }, { "DamageCoverage", 29, 1 }, { "AdditiveEnvironment", 30, 1 }, { "CoverageAlpha", 31, 1 }, { "Cookies", 22, 1 }, { "Blend", 32, 3 }, { "InstancedSprite", 35, 1 } };

            xbool Known = FALSE;

            for( const auto& Item : Fields )
                if( Axis.Field == Item.Name && Axis.Shift == Item.Shift && Axis.Bits == Item.Bits )
                    Known = TRUE;
            if( !Known )
            {
                x_printf( "Error: invalid program key field '%s'\n", (const char*)Field );
                return FALSE;
            }

            std::istringstream Values( (const char*)Domain );
            s64                Value = 0;

            while( Values >> Value )
                Axis.Values.push_back( Value );
            if( !Values.eof() )
                return FALSE;

            Desc.Axes.push_back( std::move( Axis ) );
        }
        else if( IsToken( Tokenizer, "rule" ) )
        {
            if( !ReadValue( Tokenizer, Name ) )
                return FALSE;

            Desc.Rules.emplace_back( (const char*)Name );
        }
        else if( IsToken( Tokenizer, "normalize" ) )
        {
            xstring Condition, Axis, Value;

            if( !ReadValue( Tokenizer, Condition ) || !ReadValue( Tokenizer, Axis ) || !ReadValue( Tokenizer, Value ) )
                return FALSE;

            Desc.Normalizations.push_back( { (const char*)Condition, (const char*)Axis, (const char*)Value } );
        }
        else if( IsToken( Tokenizer, "vertex" ) || IsToken( Tokenizer, "pixel" ) )
        {
            const xbool Vertex = IsToken( Tokenizer, "vertex" );
            xstring     Value, When;

            if( !ReadValue( Tokenizer, Name ) || !ReadValue( Tokenizer, Value ) || !ReadValue( Tokenizer, When ) )
                return FALSE;

            ( Vertex ? Desc.Vertex : Desc.Pixel ).push_back( { (const char*)Name, (const char*)Value, (const char*)When } );
        }
        else
            return FALSE;
    }

    if( !HasBase || Tokenizer.Type() == token_stream::TOKEN_EOF )
        return FALSE;

    for( const auto& Combo : Script.Combos )
        if( Combo.Name == Desc.Name )
            return FALSE;

    shader_combo_schema Schema;
    std::string         Error;

    if( !Schema.Compile( Desc, Error ) )
    {
        x_printf( "Error: combo '%s': %s\n", Desc.Name.c_str(), Error.c_str() );
        return FALSE;
    }

    Script.Combos.push_back( std::move( Desc ) );
    return TRUE;
}

//==============================================================================

xbool ParseScript( const char* pFileName, shader_script& Script )
{
    static u32 Depth = 0;

    if( Depth >= 8 )
    {
        x_printf( "Error: cyclic/deep shader contract inclusion\n" );
        return FALSE;
    }

    struct DepthGuard
    {
        u32& Depth;

        DepthGuard( u32& Value ) : Depth( Value )
        {
            ++Depth;
        }

        ~DepthGuard()
        {
            --Depth;
        }
    } Guard( Depth );

    token_stream  Tokenizer;
    parse_section Section = SECTION_NONE;

    Script.ScriptPath = pFileName;
    Script.ScriptDir  = GetDirectory( pFileName );

    char Delimiters[] = " ,{}()=";
    Tokenizer.SetDelimeter( Delimiters );

    if( !Tokenizer.OpenFile( pFileName ) )
    {
        x_printf( "Error: Can't open script '%s'\n", pFileName );
        return FALSE;
    }

    while( Tokenizer.Read() != token_stream::TOKEN_EOF )
    {
        if( IsToken( Tokenizer, "settings:" ) )
        {
            Section = SECTION_SETTINGS;
        }
        else if( IsToken( Tokenizer, "shaders:" ) )
        {
            Section = SECTION_SHADERS;
        }
        else if( IsToken( Tokenizer, "programs:" ) )
        {
            Section = SECTION_PROGRAMS;
        }
        else if( IsToken( Tokenizer, "buffers:" ) )
        {
            Section = SECTION_BUFFERS;
        }
        else if( IsToken( Tokenizer, "surface_families:" ) )
        {
            Section = SECTION_SURFACES;
        }
        else if( IsToken( Tokenizer, "interfaces:" ) )
        {
            Section = SECTION_INTERFACES;
        }
        else if( IsToken( Tokenizer, "resources:" ) )
        {
            Section = SECTION_RESOURCES;
        }
        else if( IsToken( Tokenizer, "packages:" ) )
        {
            Section = SECTION_PACKAGES;
        }
        else if( IsToken( Tokenizer, "combos:" ) )
        {
            Section = SECTION_COMBOS;
        }
        else if( Section == SECTION_COMBOS )
        {
            if( !ParseCombo( Tokenizer, Script ) )
                return FALSE;
        }
        else if( Section == SECTION_PACKAGES )
        {
            shader_package_assignment& Package = Script.Packages.Append();

            if( !IsToken( Tokenizer, "package" ) || !ReadValue( Tokenizer, Package.Name ) || !ExpectOpenBrace( Tokenizer ) )
                return FALSE;

            if( Package.Name.IsEmpty() )
                return FALSE;

            for( s32 i = 0; i < Package.Name.GetLength(); ++i )
            {
                const char Value = Package.Name[i];

                if( !( ( Value >= 'a' && Value <= 'z' ) || ( Value >= '0' && Value <= '9' ) || Value == '_' ) )
                    return FALSE;
            }
            while( Tokenizer.Read() != token_stream::TOKEN_EOF )
            {
                if( Tokenizer.Type() == token_stream::TOKEN_DELIMITER && Tokenizer.Delimiter() == '}' )
                    break;
                const xbool Family = IsToken( Tokenizer, "family" );

                if( !Family && !IsToken( Tokenizer, "program" ) )
                    return FALSE;

                xstring Name;

                if( !ReadValue( Tokenizer, Name ) )
                    return FALSE;

                if( Family )
                    Package.Families.Append() = Name;
                else
                    Package.Programs.Append() = Name;
            }

            if( Tokenizer.Type() == token_stream::TOKEN_EOF )
                return FALSE;
        }
        else if( Section == SECTION_RESOURCES )
        {
            xstring Name;

            if( !IsToken( Tokenizer, "resource" ) || !ReadValue( Tokenizer, Name ) )
                return FALSE;

            for( s32 i = 0; i < Script.ResourceSemantics.GetCount(); ++i )
                if( Script.ResourceSemantics[i] == Name )
                    return FALSE;

            Script.ResourceSemantics.Append() = Name;
        }
        else if( Section == SECTION_SURFACES )
        {
            shader_surface_family& Family = Script.SurfaceFamilies.Append();

            if( !IsToken( Tokenizer, "surface" ) || !ReadValue( Tokenizer, Family.Vertex ) ||
                !ReadValue( Tokenizer, Family.Source ) || !ReadValue( Tokenizer, Family.PixelSource ) ||
                !ReadValue( Tokenizer, Family.Recipe ) )
                return FALSE;

            Family.Source      = MakeRelativePath( Script.SourceRoot, Family.Source );
            Family.PixelSource = MakeRelativePath( Script.SourceRoot, Family.PixelSource );

            if( Family.Vertex != "Rigid" && Family.Vertex != "Skin" && Family.Vertex != "Dynamic" && Family.Vertex != "Primitive" && Family.Vertex != "Decal" && Family.Vertex != "ShadowRigid" && Family.Vertex != "ShadowSkin" )
                return FALSE;
        }
        else if( Section == SECTION_INTERFACES )
        {
            shader_contract_interface& Interface = Script.Interfaces.Append();

            if( !IsToken( Tokenizer, "interface" ) || !ReadValue( Tokenizer, Interface.Name ) || !ExpectOpenBrace( Tokenizer ) )
                return FALSE;

            while( Tokenizer.Read() != token_stream::TOKEN_EOF )
            {
                if( Tokenizer.Type() == token_stream::TOKEN_DELIMITER && Tokenizer.Delimiter() == '}' )
                    break;
                shader_interface_field& Field = Interface.Fields.Append();
                Field.Type                    = Tokenizer.String();

                if( !ReadValue( Tokenizer, Field.Name ) || !ReadValue( Tokenizer, Field.Semantic ) ||
                    !ReadValue( Tokenizer, Field.Interpolation ) || !ReadValue( Tokenizer, Field.Condition ) )
                    return FALSE;

                if( Field.Interpolation != "smooth" && Field.Interpolation != "flat" )
                    return FALSE;
            }

            if( Tokenizer.Type() == token_stream::TOKEN_EOF )
                return FALSE;
        }
        else if( Section == SECTION_PROGRAMS )
        {
            if( !IsToken( Tokenizer, "program" ) )
                return FALSE;

            shader_program_recipe& Program = Script.Programs.Append();

            if( !ReadValue( Tokenizer, Program.Name ) || !ReadValue( Tokenizer, Program.Vertex ) || !ReadValue( Tokenizer, Program.Pixel ) )
                return FALSE;
        }
        else if( Section == SECTION_BUFFERS )
        {
            if( !IsToken( Tokenizer, "buffer" ) || !ParseBuffer( Tokenizer, Script ) )
                return FALSE;
        }
        else if( Section == SECTION_SETTINGS )
        {
            if( !ParseSetting( Tokenizer, Script ) )
            {
                x_printf( "Error: Bad setting in %s line %d\n",
                          Tokenizer.GetFilename(),
                          Tokenizer.GetLineNumber() );
                Tokenizer.CloseFile();
                return FALSE;
            }
        }
        else if( Section == SECTION_SHADERS )
        {
            if( !IsToken( Tokenizer, "shader" ) )
            {
                x_printf( "Error: 'shader' keyword expected in %s line %d\n",
                          Tokenizer.GetFilename(),
                          Tokenizer.GetLineNumber() );
                Tokenizer.CloseFile();
                return FALSE;
            }

            if( !ParseShader( Tokenizer, Script ) )
            {
                x_printf( "Error: Bad shader block in %s line %d\n",
                          Tokenizer.GetFilename(),
                          Tokenizer.GetLineNumber() );
                Tokenizer.CloseFile();
                return FALSE;
            }
        }
        else
        {
            x_printf( "Error: Unknown token '%s' in %s line %d\n",
                      Tokenizer.String(),
                      Tokenizer.GetFilename(),
                      Tokenizer.GetLineNumber() );
            Tokenizer.CloseFile();
            return FALSE;
        }
    }

    Tokenizer.CloseFile();

    if( Script.OutRoot.IsEmpty() )
        Script.OutRoot = MakeRelativePath( Script.ScriptDir, "Build" );

    if( !Script.Contract.IsEmpty() )
    {
        shader_script Contract;

        if( !ParseScript( Script.Contract, Contract ) )
            return FALSE;

        for( s32 i = 0; i < Contract.Programs.GetCount(); ++i )
            Script.Programs.Append() = Contract.Programs[i];
        for( s32 i = 0; i < Contract.Shaders.GetCount(); ++i )
            Script.Shaders.Append() = Contract.Shaders[i];
        for( s32 i = 0; i < Contract.ComputeShaders.GetCount(); ++i )
            Script.ComputeShaders.Append() = Contract.ComputeShaders[i];
        for( s32 i = 0; i < Contract.Buffers.GetCount(); ++i )
            Script.Buffers.Append() = Contract.Buffers[i];
        for( s32 i = 0; i < Contract.SurfaceFamilies.GetCount(); ++i )
            Script.SurfaceFamilies.Append() = Contract.SurfaceFamilies[i];
        for( s32 i = 0; i < Contract.Packages.GetCount(); ++i )
            Script.Packages.Append() = Contract.Packages[i];
        Script.Combos.insert( Script.Combos.end(), Contract.Combos.begin(), Contract.Combos.end() );

        for( s32 i = 0; i < Contract.Interfaces.GetCount(); ++i )
            Script.Interfaces.Append() = Contract.Interfaces[i];
        for( s32 i = 0; i < Contract.ResourceSemantics.GetCount(); ++i )
            Script.ResourceSemantics.Append() = Contract.ResourceSemantics[i];
    }

    if( !Script.Abi.IsEmpty() )
    {
        shader_script Abi;

        if( !ParseScript( Script.Abi, Abi ) )
            return FALSE;

        for( s32 i = 0; i < Abi.Buffers.GetCount(); ++i )
            Script.Buffers.Append() = Abi.Buffers[i];
        for( s32 i = 0; i < Abi.Interfaces.GetCount(); ++i )
            Script.Interfaces.Append() = Abi.Interfaces[i];
        for( s32 i = 0; i < Abi.ResourceSemantics.GetCount(); ++i )
            Script.ResourceSemantics.Append() = Abi.ResourceSemantics[i];
    }

    if( Script.Shaders.GetCount() == 0 && Script.ComputeShaders.GetCount() == 0 && Script.Buffers.GetCount() == 0 && Script.SurfaceFamilies.GetCount() == 0 && Script.Interfaces.GetCount() == 0 && Script.Combos.empty() )
    {
        x_printf( "Error: Script has no shaders: %s\n", pFileName );
        return FALSE;
    }

    for( s32 i = 0; i < Script.Shaders.GetCount(); i++ )
    {
        for( s32 j = i + 1; j < Script.Shaders.GetCount(); j++ )
        {
            if( x_stricmp( Script.Shaders[i].Name, Script.Shaders[j].Name ) == 0 )
            {
                x_printf( "Error: Duplicate shader output '%s.%s' in %s\n",
                          (const char*)Script.Shaders[i].Name,
                          (const char*)Script.Shaders[i].StageShort,
                          pFileName );
                return FALSE;
            }
        }
    }

    for( s32 i = 0; i < Script.ComputeShaders.GetCount(); ++i )
    {
        const xstring& Name = Script.ComputeShaders[i].Name;

        for( s32 j = 0; j < i; ++j )
        {
            if( x_stricmp( Name, Script.ComputeShaders[j].Name ) == 0 )
            {
                return FALSE;
            }
        }

        for( s32 j = 0; j < Script.Shaders.GetCount(); ++j )
        {
            if( x_stricmp( Name, Script.Shaders[j].Name ) == 0 )
            {
                return FALSE;
            }
        }
    }

    for( s32 i = 0; i < Script.Packages.GetCount(); ++i )
    {
        const shader_package_assignment& Package = Script.Packages[i];

        for( s32 j = 0; j < i; ++j )
            if( Script.Packages[j].Name == Package.Name )
                return FALSE;

        for( s32 j = 0; j < Package.Programs.GetCount(); ++j )
        {
            xbool Found = FALSE;

            for( s32 k = 0; k < Script.Programs.GetCount(); ++k )
                if( Script.Programs[k].Name == Package.Programs[j] )
                {
                    if( !Script.Programs[k].Package.IsEmpty() && Script.Programs[k].Package != Package.Name )
                        return FALSE;

                    Script.Programs[k].Package = Package.Name;
                    Found                      = TRUE;
                }

            if( !Found )
            {
                x_printf( "Error: package '%s' references unknown program '%s'\n", (const char*)Package.Name, (const char*)Package.Programs[j] );
                return FALSE;
            }
        }

        for( s32 j = 0; j < Package.Families.GetCount(); ++j )
        {
            xbool Found = FALSE;

            for( s32 k = 0; k < Script.SurfaceFamilies.GetCount(); ++k )
                if( Script.SurfaceFamilies[k].Vertex == Package.Families[j] )
                {
                    if( !Script.SurfaceFamilies[k].Package.IsEmpty() && Script.SurfaceFamilies[k].Package != Package.Name )
                        return FALSE;

                    Script.SurfaceFamilies[k].Package = Package.Name;
                    Found                             = TRUE;
                }

            if( !Found )
            {
                x_printf( "Error: package '%s' references unknown family '%s'\n", (const char*)Package.Name, (const char*)Package.Families[j] );
                return FALSE;
            }
        }
    }

    for( s32 i = 0; i < Script.SurfaceFamilies.GetCount(); ++i )
    {
        if( Script.Packages.GetCount() && Script.SurfaceFamilies[i].Package.IsEmpty() )
            return FALSE;

        for( s32 j = 0; j < i; ++j )
            if( Script.SurfaceFamilies[i].Vertex == Script.SurfaceFamilies[j].Vertex )
            {
                x_printf( "Error: duplicate surface family '%s'\n", (const char*)Script.SurfaceFamilies[i].Vertex );
                return FALSE;
            }
    }

    std::set<std::string> ComboNames;

    for( const auto& Combo : Script.Combos )
        if( !ComboNames.insert( Combo.Name ).second )
        {
            x_printf( "Error: duplicate combo declaration '%s'\n", Combo.Name.c_str() );
            return FALSE;
        }

    for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
    {
        const shader_program_recipe& Program = Script.Programs[i];

        if( Script.Packages.GetCount() && Program.Package.IsEmpty() )
            return FALSE;

        xbool Vertex = FALSE;
        xbool Pixel  = FALSE;

        for( s32 j = 0; j < Script.Shaders.GetCount(); ++j )
        {
            const shader_entry& Shader = Script.Shaders[j];

            if( Shader.Name == Program.Vertex && Shader.StageShort == "vs" )
                Vertex = TRUE;
            if( Shader.Name == Program.Pixel && Shader.StageShort == "ps" )
                Pixel = TRUE;
        }

        if( !Vertex || !Pixel )
        {
            x_printf( "Error: recipe '%s' requires declared vertex and pixel stages\n", (const char*)Program.Name );
            return FALSE;
        }

        for( s32 j = i + 1; j < Script.Programs.GetCount(); ++j )
            if( Program.Name == Script.Programs[j].Name )
            {
                x_printf( "Error: duplicate recipe '%s'\n", (const char*)Program.Name );
                return FALSE;
            }
    }

    return TRUE;
}
