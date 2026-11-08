//==============================================================================
//  Declarative contract generation and identities. Generated files live in build.
//==============================================================================
#include "ShaderTool.hpp"
#include "../../Support/Render/ShaderContract.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <map>
#include <string>
#include <vector>

static
u64 HashBytes( u64 Hash, const void* Bytes, size_t Size )
{
    const byte* Data = static_cast<const byte*>( Bytes );

    for( size_t i = 0; i < Size; ++i )
        Hash = ( Hash ^ Data[i] ) * 1099511628211ull;
    return Hash;
}

//==============================================================================

static
u64 HashText( u64 Hash, const xstring& Text )
{
    return HashBytes( Hash, (const char*)Text, Text.GetLength() + 1 );
}

//==============================================================================

static
xbool HashSource( const xstring& File, const xstring& Root, u64& Hash, std::set<std::string>& Seen, shader_source_snapshot& Snapshot, x_content_hash& ContentHash )
{
    const std::string Canonical = std::filesystem::weakly_canonical( (const char*)File ).string();

    if( !Seen.insert( Canonical ).second )
        return TRUE;

    s32 Index = -1;

    for( s32 i = 0; i < Snapshot.Files.GetCount(); ++i )
        if( Snapshot.Files[i].Path == Canonical.c_str() )
        {
            Index = i;
            break;
        }

    if( Index < 0 )
    {
        Index                      = Snapshot.Files.GetCount();
        shader_source_file& Source = Snapshot.Files.Append();
        Source.Path                = Canonical.c_str();

        if( !LoadBinaryFile( File, Source.Bytes ) )
            return FALSE;
    }

    const xarray<byte>& Bytes = Snapshot.Files[Index].Bytes;
    Hash                      = HashBytes( Hash, Bytes.GetPtr(), Bytes.GetCount() );
    byte Length[4];
    x_store_le32( Length, Bytes.GetCount() );
    ContentHash.Update( Length, sizeof( Length ) );
    ContentHash.Update( Bytes.GetPtr(), Bytes.GetCount() );
    const std::string Source( reinterpret_cast<const char*>( Bytes.GetPtr() ), Bytes.GetCount() );
    size_t            Position = 0;

    while( ( Position = Source.find( "#include", Position ) ) != std::string::npos )
    {
        Position += 8;
        const size_t End   = Source.find( '\n', Position );
        const size_t Quote = Source.find( '"', Position );

        if( Quote == std::string::npos || ( End != std::string::npos && Quote > End ) )
            continue;
        const size_t Close = Source.find( '"', Quote + 1 );

        if( Close == std::string::npos || ( End != std::string::npos && Close > End ) )
            return FALSE;

        const xstring Include( Source.substr( Quote + 1, Close - Quote - 1 ).c_str() );

        if( Include == "a51_shader_contract.hlsl" )
            continue;
        xstring IncludeFile = MakeRelativePath( GetDirectory( File ), Include );

        if( !command_line::FileExists( IncludeFile ) )
            IncludeFile = MakeRelativePath( Root, Include );
        if( !command_line::FileExists( IncludeFile ) )
        {
            x_printf( "Error: cannot hash shader include '%s' from '%s'\n", (const char*)Include, (const char*)File );
            return FALSE;
        }

        if( !HashSource( IncludeFile, Root, Hash, Seen, Snapshot, ContentHash ) )
            return FALSE;

        Position = Close + 1;
    }

    return TRUE;
}

//==============================================================================

static
xbool WriteGenerated( const xstring& File, const xstring& Text )
{
    xarray<byte> Existing;

    if( LoadBinaryFile( File, Existing ) && Existing.GetCount() == Text.GetLength() &&
        !x_memcmp( Existing.GetPtr(), (const char*)Text, Text.GetLength() ) )
        return TRUE;

    const xstring Temporary = TempPathFor( File );
    return WriteBinaryFile( Temporary, (const char*)Text, Text.GetLength() ) && ReplaceFile( Temporary, File );
}

//==============================================================================

u32 ShaderInterfaceId( const xarray<shader_interface_variable>& Variables )
{
    std::vector<std::string> Keys;

    for( s32 i = 0; i < Variables.GetCount(); ++i )
    {
        const shader_interface_variable& Variable = Variables[i];
        Keys.push_back( (const char*)xfs( "%s:%u:%u:%u", (const char*)Variable.Semantic, Variable.Index, Variable.Components, Variable.ComponentType ) );
    }

    std::sort( Keys.begin(), Keys.end() );
    u64 Hash = 14695981039346656037ull;

    for( const std::string& Key : Keys )
        Hash = HashBytes( Hash, Key.c_str(), Key.size() + 1 );
    u32 Result = static_cast<u32>( Hash ^ ( Hash >> 32 ) );
    return Result ? Result : 1;
}

//==============================================================================

xbool GenerateShaderContract( shader_script& Script, xbool WriteFiles )
{
    if( !Script.Sources )
    {
        Script.Sources          = new shader_source_snapshot;
        const xstring* Inputs[] = { &Script.ScriptPath, &Script.Contract, &Script.Abi };

        for( const xstring* Input : Inputs )
        {
            if( Input->IsEmpty() )
                continue;
            shader_source_file& File = Script.Sources->Files.Append();
            File.Path                = std::filesystem::weakly_canonical( (const char*)*Input ).string().c_str();

            if( !LoadBinaryFile( File.Path, File.Bytes ) )
                return FALSE;
        }
    }

    u64 Catalog = HashText( 14695981039346656037ull, "Area51 shader contract ABI 2" );
    Catalog     = HashBytes( Catalog, &Script.CompilerFingerprint, sizeof( Script.CompilerFingerprint ) );
    xstring Hlsl( "// Generated by ShaderTool. Column-major matrices; 32-bit shared storage.\n#ifndef A51_GENERATED_SHADER_CONTRACT\n#define A51_GENERATED_SHADER_CONTRACT\n" );
    xstring Cpp( "// Generated by ShaderTool. Do not edit.\n#pragma once\n#include \"x_math.hpp\"\n#include <cstddef>\n" );

    for( s32 i = 0; i < Script.Buffers.GetCount(); ++i )
    {
        shader_contract_buffer& Buffer = Script.Buffers[i];
        Buffer.Size                    = ( Buffer.Size + 15 ) & ~15u;
        Catalog                        = HashText( Catalog, Buffer.HlslName );
        Catalog                        = HashText( Catalog, Buffer.CppName );
        Catalog                        = HashText( Catalog, Buffer.Resource );
        xstring HlslFields;
        Hlsl += xstring( xfs( "struct %s\n{\n", (const char*)Buffer.HlslName ) );
        Cpp += xstring( xfs( "struct alignas(16) %s\n{\n", (const char*)Buffer.CppName ) );

        for( s32 j = 0; j < Buffer.Fields.GetCount(); ++j )
        {
            const shader_contract_field& Field = Buffer.Fields[j];
            Catalog                            = HashText( Catalog, Field.Type );
            Catalog                            = HashText( Catalog, Field.Name );
            Catalog                            = HashText( Catalog, Field.CppName );
            Catalog                            = HashBytes( Catalog, &Field.Count, sizeof( Field.Count ) );
            const char*   HlslType             = Field.Type == "s32" ? "int" : Field.Type == "u32"         ? "uint"
                                                                           : Field.Type == "f32"           ? "float"
                                                                           : Field.Type == "matrix4"       ? "column_major float4x4"
                                                                           : Field.Type == "float2_values" ? "float2"
                                                                           : Field.Type == "float4_values" ? "float4"
                                                                                                           : (const char*)Field.Type;
            const char*   CppType              = Field.Type == "float4" ? "vector4" : Field.Type == "matrix4"                                          ? "matrix4"
                                                                                  : ( Field.Type == "uint4" || Field.Type == "uint2" )                 ? "u32"
                                                                                  : ( Field.Type == "float4_values" || Field.Type == "float2_values" ) ? "f32"
                                                                                                                                                       : (const char*)Field.Type;
            const u32     CppCount             = Field.Count * ( ( Field.Type == "uint2" || Field.Type == "float2_values" ) ? 2 : ( Field.Type == "uint4" || Field.Type == "float4_values" ) ? 4
                                                                                                                                                                                             : 1 );
            const xstring HlslArray            = Field.Count > 1 ? xstring( xfs( "[%u]", Field.Count ) ) : xstring();
            const xstring CppArray             = CppCount > 1 ? xstring( xfs( "[%u]", CppCount ) ) : xstring();
            const char*   Alignment            = Field.Type == "u32" || Field.Type == "s32" || Field.Type == "f32" ? "" : ( Field.Type == "uint2" || Field.Type == "float2_values" ) ? "alignas(8) "
                                                                                                                                                                                     : "alignas(16) ";
            Hlsl += xstring( xfs( "    %s %s%s;\n", HlslType, (const char*)Field.Name, (const char*)HlslArray ) );
            Cpp += xstring( xfs( "    %s%s %s%s;\n", Alignment, CppType, (const char*)Field.CppName, (const char*)CppArray ) );
            HlslFields += xstring( xfs( "    %s %s%s; \\\n", HlslType, (const char*)Field.Name, (const char*)HlslArray ) );
        }

        Hlsl += "};\n";
        Hlsl += xstring( xfs( "#define A51_ABI_FIELDS_%s \\\n", (const char*)Buffer.HlslName ) ) + HlslFields + "\n";
        Cpp += "};\n";
        Cpp += xstring( xfs( "static_assert(sizeof(%s) == %u, \"shader record stride\");\n", (const char*)Buffer.CppName, Buffer.Size ) );

        for( s32 j = 0; j < Buffer.Fields.GetCount(); ++j )
        {
            const shader_contract_field& Field = Buffer.Fields[j];
            Cpp += xstring( xfs( "static_assert(offsetof(%s, %s) == %u, \"shader member offset\");\n", (const char*)Buffer.CppName, (const char*)Field.CppName, Field.Offset ) );
            Cpp += xstring( xfs( "static_assert(sizeof(((%s*)0)->%s) == %u, \"shader member size/array stride\");\n", (const char*)Buffer.CppName, (const char*)Field.CppName, Field.Size ) );
        }
    }

    for( s32 i = 0; i < Script.Interfaces.GetCount(); ++i )
    {
        const shader_contract_interface& Interface = Script.Interfaces[i];
        Catalog                                    = HashText( Catalog, Interface.Name );
        Hlsl += xstring( xfs( "struct %s\n{\n", (const char*)Interface.Name ) );

        for( s32 j = 0; j < Interface.Fields.GetCount(); ++j )
        {
            const shader_interface_field& Field = Interface.Fields[j];
            Catalog                             = HashText( Catalog, Field.Type );
            Catalog                             = HashText( Catalog, Field.Name );
            Catalog                             = HashText( Catalog, Field.Semantic );
            Catalog                             = HashText( Catalog, Field.Interpolation );
            Catalog                             = HashText( Catalog, Field.Condition );

            if( Field.Condition != "always" )
                Hlsl += xstring( xfs( "#if %s\n", (const char*)Field.Condition ) );
            Hlsl += xstring( xfs( "    %s%s %s : %s;\n", Field.Interpolation == "flat" ? "nointerpolation " : "",
                                  (const char*)Field.Type, (const char*)Field.Name, (const char*)Field.Semantic ) );
            if( Field.Condition != "always" )
                Hlsl += "#endif\n";
        }

        Hlsl += "};\n";
    }

    if( Script.ResourceSemantics.GetCount() > 128 )
        return FALSE;

    Cpp += "enum A51ResourceSemantic\n{\n";

    for( s32 i = 0; i < Script.ResourceSemantics.GetCount(); ++i )
    {
        Catalog = HashText( Catalog, Script.ResourceSemantics[i] );
        Cpp += xstring( xfs( "    A51Resource_%s,\n", (const char*)Script.ResourceSemantics[i] ) );
    }

    Cpp += "    A51ResourceCount\n};\ninline constexpr const char* A51ResourceNames[] = {\n";

    for( s32 i = 0; i < Script.ResourceSemantics.GetCount(); ++i )
        Cpp += xstring( xfs( "    \"%s\",\n", (const char*)Script.ResourceSemantics[i] ) );
    if( !Script.ResourceSemantics.GetCount() )
        Cpp += "    nullptr,\n";
    Cpp += "};\n";
    Script.AbiId = Catalog;
    xstring CombosCpp( "// Generated by ShaderTool. Do not edit.\n#pragma once\n#include \"Render/ShaderContract.hpp\"\n" );

    for( const auto& Desc : Script.Combos )
    {
        shader_combo_schema               Schema;
        std::string                       Error, Text;
        std::vector<shader_combo_variant> Variants;

        if( !Schema.Compile( Desc, Error ) || ( !Desc.MaterialProfiles && !Schema.Enumerate( Variants, Error ) ) || !Schema.EmitCpp( Text, Error ) )
        {
            x_printf( "Error: combo '%s': %s\n", Desc.Name.c_str(), Error.c_str() );
            return FALSE;
        }

        CombosCpp += Text.c_str();
        // Package layout and rules affect the catalog, never the stage ABI.
        Catalog = HashText( Catalog, Desc.Name.c_str() );
        Catalog = HashText( Catalog, Text.c_str() );

        for( const auto& Variant : Variants )
        {
            Catalog = HashBytes( Catalog, &Variant.Key, sizeof( Variant.Key ) );
            Catalog = HashText( Catalog, Variant.VertexOptions.c_str() );
            Catalog = HashText( Catalog, Variant.PixelOptions.c_str() );
        }
    }

    std::map<std::string, u64>            SourceHashes;
    std::map<std::string, x_content_id> SourceContent;

    for( s32 i = 0; i < Script.ComputeShaders.GetCount(); ++i )
    {
        shader_entry&         Shader  = Script.ComputeShaders[i];
        u64                   Content = 14695981039346656037ull;
        std::set<std::string> Seen;
        x_content_hash      ContentHash;

        if( !HashSource( Shader.Source, GetDirectory( Shader.Source ), Content, Seen, *Script.Sources, ContentHash ) )
        {
            return FALSE;
        }

        Shader.SourceContent = ContentHash.Finish();
        Catalog              = HashText( Catalog, Shader.Name );
        Catalog              = HashText( Catalog, Shader.Entry );
        Catalog              = HashText( Catalog, Shader.Profile );
        Catalog              = HashText( Catalog, Shader.CommonArgs );
        Catalog              = HashText( Catalog, Shader.VulkanArgs );
        Catalog              = HashText( Catalog, Shader.D3D12Args );
        Catalog              = HashBytes( Catalog, &Content, sizeof( Content ) );
    }

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
    {
        shader_entry& Shader  = Script.Shaders[i];
        u64           Program = HashText( Script.AbiId, Shader.Name );
        Program               = HashText( Program, Shader.Stage );
        Program               = HashText( Program, Shader.Entry );
        Program               = HashText( Program, Shader.Profile );
        Program               = HashBytes( Program, &Shader.ColorArithmetic, sizeof( Shader.ColorArithmetic ) );
        Program               = HashText( Program, Shader.CommonArgs );
        Program               = HashText( Program, Shader.VulkanArgs );
        Program               = HashText( Program, Shader.D3D12Args );
        const std::string SourceName( (const char*)Shader.Source );
        auto              SourceHash = SourceHashes.find( SourceName );

        if( SourceHash == SourceHashes.end() )
        {
            u64                   Content = 14695981039346656037ull;
            std::set<std::string> Seen;
            x_content_hash      ContentHash;

            if( !HashSource( Shader.Source, GetDirectory( Shader.Source ), Content, Seen, *Script.Sources, ContentHash ) )
                return FALSE;

            SourceContent.emplace( SourceName, ContentHash.Finish() );
            SourceHash = SourceHashes.emplace( SourceName, Content ).first;
        }

        Program              = HashBytes( Program, &SourceHash->second, sizeof( SourceHash->second ) );
        Shader.ProgramId     = Program ? Program : 1;
        Shader.SourceContent = SourceContent.at( SourceName );
        Catalog              = HashBytes( Catalog, &Shader.ProgramId, sizeof( Shader.ProgramId ) );
    }

    for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
    {
        Catalog = HashText( Catalog, Script.Programs[i].Name );
        Catalog = HashText( Catalog, Script.Programs[i].Vertex );
        Catalog = HashText( Catalog, Script.Programs[i].Pixel );
        Catalog = HashText( Catalog, Script.Programs[i].Package );
        Catalog = HashBytes( Catalog, &Script.Programs[i].Key, sizeof( Script.Programs[i].Key ) );
        Catalog = HashBytes( Catalog, &Script.Programs[i].Kind, sizeof( Script.Programs[i].Kind ) );
    }

    const u32 PackageVersion = ECS_PACKAGE_MAJOR;
    Catalog                  = HashBytes( Catalog, &PackageVersion, sizeof( PackageVersion ) );
    Catalog                  = HashText( Catalog, "zstd-1.5.7-level3-checksum" );
    Script.CatalogId         = Catalog ? Catalog : 1;
    xstring CatalogCpp( "// Generated by ShaderTool. Program identities are separate from buffer layouts.\n#pragma once\n#include \"a51_shader_contract.hpp\"\n" );
    CatalogCpp += xstring( xfs( "inline constexpr u64 A51ShaderCatalogId = 0x%016llxull;\n", (unsigned long long)Script.CatalogId ) );
    xstring ProgramsCpp( "// Generated by ShaderTool. Do not edit.\n#pragma once\n#include \"x_types.hpp\"\n" );

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
        ProgramsCpp += xstring( xfs( "inline constexpr u64 ShaderProgram_%s = 0x%016llxull;\n", (const char*)Script.Shaders[i].Name, (unsigned long long)Script.Shaders[i].ProgramId ) );
    std::vector<const shader_program_recipe*> SurfacePrograms;
    std::vector<const shader_program_recipe*> FamilyPrograms;
    std::vector<const shader_program_recipe*> FixedPrograms;
    std::map<std::string, u64>                StageIds;

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
        StageIds.emplace( (const char*)Script.Shaders[i].Name, Script.Shaders[i].ProgramId );
    for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
    {
        if( Script.Programs[i].Kind == SHADER_COMBO_FIXED )
        {
            FixedPrograms.push_back( &Script.Programs[i] );
            continue;
        }

        if( ( Script.Programs[i].Key & 7 ) == (u64)render::ShaderFamily::Surface )
            SurfacePrograms.push_back( &Script.Programs[i] );
        else
            FamilyPrograms.push_back( &Script.Programs[i] );
    }

    std::sort( FamilyPrograms.begin(), FamilyPrograms.end(), []( const shader_program_recipe* A, const shader_program_recipe* B )
               { return A->Key < B->Key; } );
    for( size_t i = 1; i < FamilyPrograms.size(); ++i )
    {
        if( FamilyPrograms[i - 1]->Key == FamilyPrograms[i]->Key )
        {
            x_printf( "Error: duplicate recipe key for '%s' and '%s'\n",
                      (const char*)FamilyPrograms[i - 1]->Name, (const char*)FamilyPrograms[i]->Name );
            return FALSE;
        }
    }

    CatalogCpp += "struct A51RecipeProgramDesc { u64 Key; u64 VertexId; u64 PixelId; const char* PackageFile; };\n";
    CatalogCpp += xstring( xfs( "inline constexpr u32 A51RecipeProgramCount = %u;\n", (u32)FamilyPrograms.size() ) );
    CatalogCpp += "inline constexpr A51RecipeProgramDesc A51RecipePrograms[] = {\n";

    for( const shader_program_recipe* Program : FamilyPrograms )
    {
        const u64 VertexId = StageIds[(const char*)Program->Vertex];
        const u64 PixelId  = StageIds[(const char*)Program->Pixel];

        if( !VertexId || !PixelId )
            return FALSE;

        CatalogCpp += xstring( xfs( "    {0x%016llxull, 0x%016llxull, 0x%016llxull, \"%s.ecs\"},\n",
                                    (unsigned long long)Program->Key, (unsigned long long)VertexId, (unsigned long long)PixelId,
                                    (const char*)Program->Package ) );
    }

    if( FamilyPrograms.empty() )
        CatalogCpp += "    {0, 0, 0, nullptr},\n";
    CatalogCpp += "};\ninline const A51RecipeProgramDesc* A51FindRecipeProgram(u64 Key)\n{\n"
                  "    u32 Begin = 0, End = A51RecipeProgramCount;\n"
                  "    while(Begin < End) { const u32 Middle = Begin + (End - Begin) / 2;\n"
                  "        if(A51RecipePrograms[Middle].Key < Key) Begin = Middle + 1; else End = Middle; }\n"
                  "    return Begin < A51RecipeProgramCount && A51RecipePrograms[Begin].Key == Key ? &A51RecipePrograms[Begin] : nullptr;\n}\n";
    CatalogCpp += "enum A51FixedProgram\n{\n";

    for( const shader_program_recipe* Program : FixedPrograms )
        CatalogCpp += xstring( xfs( "    A51FixedProgram_%s,\n", (const char*)Program->Name ) );
    CatalogCpp += "    A51FixedProgramCount\n};\ninline constexpr A51RecipeProgramDesc A51FixedPrograms[] = {\n";
    u64 FixedKey = 0;

    for( const shader_program_recipe* Program : FixedPrograms )
    {
        const u64 VertexId = StageIds[(const char*)Program->Vertex];
        const u64 PixelId  = StageIds[(const char*)Program->Pixel];

        if( !VertexId || !PixelId )
            return FALSE;

        CatalogCpp += xstring( xfs( "    {%lluull, 0x%016llxull, 0x%016llxull, \"%s.ecs\"},\n",
                                    (unsigned long long)FixedKey++, (unsigned long long)VertexId, (unsigned long long)PixelId,
                                    (const char*)Program->Package ) );
    }

    if( FixedPrograms.empty() )
        CatalogCpp += "    {0, 0, 0, nullptr},\n";
    CatalogCpp += "};\n";
    std::sort( SurfacePrograms.begin(), SurfacePrograms.end(), []( const shader_program_recipe* A, const shader_program_recipe* B )
               { return A->Key < B->Key; } );
    ProgramsCpp += "#include \"a51_shader_catalog.hpp\"\nusing A51SurfaceProgramDesc = A51RecipeProgramDesc;\n";
    ProgramsCpp += xstring( xfs( "inline constexpr u32 A51SurfaceProgramCount = %u;\n", (u32)SurfacePrograms.size() ) );
    ProgramsCpp += "inline constexpr A51SurfaceProgramDesc A51SurfacePrograms[] = {\n";

    for( const shader_program_recipe* Program : SurfacePrograms )
    {
        const u64 VertexId = StageIds[(const char*)Program->Vertex];
        const u64 PixelId  = StageIds[(const char*)Program->Pixel];

        if( !VertexId || !PixelId )
            return FALSE;

        ProgramsCpp += xstring( xfs( "    {0x%016llxull, 0x%016llxull, 0x%016llxull, \"%s.ecs\"},\n",
                                     (unsigned long long)Program->Key, (unsigned long long)VertexId, (unsigned long long)PixelId,
                                     (const char*)Program->Package ) );
    }

    if( SurfacePrograms.empty() )
        ProgramsCpp += "    {0, 0, 0, nullptr},\n";
    ProgramsCpp += "};\n";
    Hlsl += "#endif\n";
    const xstring Folder = JoinPath( JoinPath( Script.OutRoot, Script.Config ), "generated" );
    Script.Sources->Generated.SetCount( Hlsl.GetLength() );
    x_memcpy( Script.Sources->Generated.GetPtr(), (const char*)Hlsl, Hlsl.GetLength() );

    if( !WriteFiles )
        return TRUE;

    return WriteGenerated( JoinPath( Folder, "a51_shader_contract.hpp" ), Cpp ) &&
           WriteGenerated( JoinPath( Folder, "a51_shader_combos.hpp" ), CombosCpp ) &&
           WriteGenerated( JoinPath( Folder, "a51_shader_catalog.hpp" ), CatalogCpp ) &&
           WriteGenerated( JoinPath( Folder, "a51_shader_contract.hlsl" ), Hlsl ) &&
           WriteGenerated( JoinPath( Folder, "a51_shader_programs.hpp" ), ProgramsCpp );
}

//==============================================================================

xbool ValidateDeclaredBuffers( const shader_script& Script, const shader_compile_output& Output )
{
    for( s32 i = 0; i < Script.Buffers.GetCount(); ++i )
    {
        const shader_contract_buffer& Buffer = Script.Buffers[i];
        xbool                         Used   = FALSE;

        for( s32 j = 0; j < Output.Bindings.GetCount(); ++j )
            if( Output.Bindings[j].Name == Buffer.Resource )
                Used = TRUE;
        if( !Used )
            continue;
        for( s32 j = 0; j < Output.Members.GetCount(); ++j )
        {
            const shader_buffer_member& Member = Output.Members[j];

            if( Member.Buffer == Buffer.Resource && Member.Name.IsEmpty() && Member.Size != Buffer.Size )
            {
                x_printf( "Error: generated ABI record '%s' size %u differs from final %s size %u\n",
                          (const char*)Buffer.Resource, Buffer.Size, (const char*)Output.Target, Member.Size );
                return FALSE;
            }
        }

        for( s32 j = 0; j < Buffer.Fields.GetCount(); ++j )
        {
            const shader_contract_field& Field = Buffer.Fields[j];
            xbool                        Found = FALSE;

            for( s32 k = 0; k < Output.Members.GetCount(); ++k )
            {
                const shader_buffer_member& Member = Output.Members[k];

                if( Member.Buffer == Buffer.Resource && ( Member.Name == Field.Name || ( Member.Name.GetLength() > Field.Name.GetLength() && Member.Name.Right( Field.Name.GetLength() + 1 ) == xstring( "." ) + Field.Name ) ) &&
                    Member.Offset == Field.Offset && Member.Size == Field.Size &&
                    ( Field.Count == 1 || Member.ArrayStride == Field.Size / Field.Count ) &&
                    ( Field.Type != "matrix4" || Member.MatrixStride == 16 ) )
                    Found = TRUE;
            }

            if( !Found )
            {
                x_printf( "Error: generated ABI field '%s.%s' differs from final %s\n",
                          (const char*)Buffer.Resource, (const char*)Field.Name, (const char*)Output.Target );
                return FALSE;
            }
        }
    }

    return TRUE;
}

//==============================================================================

xbool WriteShaderCatalog( const shader_script& Script )
{
    std::map<std::pair<std::string, std::string>, const shader_compile_output*> Outputs;
    std::map<std::string, const shader_entry*>                                  Stages;

    for( s32 i = 0; i < Script.FinalOutputs.GetCount(); ++i )
        Outputs.emplace( std::make_pair( (const char*)Script.FinalOutputs[i].ShaderName,
                                         (const char*)Script.FinalOutputs[i].Target ),
                         &Script.FinalOutputs[i] );
    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
        Stages.emplace( (const char*)Script.Shaders[i].Name, &Script.Shaders[i] );

    for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
    {
        const shader_program_recipe& Recipe    = Script.Programs[i];
        const char*                  Targets[] = { "vulkan", "d3d12" };

        for( u32 Target = 0; Target < 2; ++Target )
        {
            if( !( Script.TargetMask & ( Target ? SHADER_TARGET_D3D12 : SHADER_TARGET_VULKAN ) ) )
                continue;
            const shader_compile_output* Vertex = Outputs.count( { (const char*)Recipe.Vertex, Targets[Target] } ) ? Outputs.at( { (const char*)Recipe.Vertex, Targets[Target] } ) : NULL;
            const shader_compile_output* Pixel  = Outputs.count( { (const char*)Recipe.Pixel, Targets[Target] } ) ? Outputs.at( { (const char*)Recipe.Pixel, Targets[Target] } ) : NULL;

            if( !Vertex || !Pixel )
            {
                x_printf( "Error: missing stages for recipe '%s'\n", (const char*)Recipe.Name );
                return FALSE;
            }

            for( s32 Input = 0; Input < Pixel->Inputs.GetCount(); ++Input )
            {
                const shader_interface_variable& Required = Pixel->Inputs[Input];
                xbool                            Found    = FALSE;

                for( s32 Output = 0; Output < Vertex->Outputs.GetCount(); ++Output )
                {
                    const shader_interface_variable& Provided = Vertex->Outputs[Output];

                    if( Provided.Semantic == Required.Semantic && Provided.Index == Required.Index &&
                        Provided.ComponentType == Required.ComponentType && Provided.Components >= Required.Components )
                        Found = TRUE;
                }

                if( !Found )
                {
                    x_printf( "Error: incompatible %s VS/PS recipe '%s': %s%u\n", Targets[Target], (const char*)Recipe.Name, (const char*)Required.Semantic, Required.Index );
                    return FALSE;
                }
            }
        }
    }

    // The staged kit contains family packages; intermediate stages live in
    // the content cache and never become runtime artifacts.
    std::set<std::string> Expected;

    for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
        Expected.insert( std::string( (const char*)Script.Programs[i].Package ) + ".ecs" );
    const xstring OutputFolder = JoinPath( Script.OutRoot, Script.Config );
    std::filesystem::create_directories( (const char*)OutputFolder );

    for( const auto& File : std::filesystem::directory_iterator( (const char*)OutputFolder ) )
    {
        const std::string Extension = File.path().extension().string();

        if( ( Extension == ".ecs" || Extension == ".cache" ) && !Expected.count( File.path().filename().string() ) )
            std::filesystem::remove( File.path() );
    }

    xstring Json( Script.FinalOutputs.GetCount() * 8192 + Script.Programs.GetCount() * 256 );
    Json = xfs( "{\n  \"abi\": 3,\n  \"catalog_id\": \"%016llx\",\n  \"recipes\": %d,\n  \"stages\": [\n", (unsigned long long)Script.CatalogId, Script.Programs.GetCount() );
    xstring               Cpp( "// Generated final resource maps. Slots are relative to their SDL category.\n#pragma once\n#include <cstdint>\n" );
    u64                   BinaryBytes    = 0;
    u64                   BindingEntries = 0;
    std::set<std::string> ResourceInterfaces;

    for( s32 i = 0; i < Script.FinalOutputs.GetCount(); ++i )
    {
        const shader_compile_output& Output     = Script.FinalOutputs[i];
        const auto                   FoundStage = Stages.find( (const char*)Output.ShaderName );
        const shader_entry*          Stage      = FoundStage == Stages.end() ? NULL : FoundStage->second;

        if( !Stage )
            return FALSE;

        xstring ResourceInterface = Stage->StageShort + ":";
        BindingEntries += Output.Bindings.GetCount();

        for( s32 j = 0; j < Output.Bindings.GetCount(); ++j )
        {
            const shader_reflection_binding& Binding = Output.Bindings[j];
            ResourceInterface += xstring( xfs( "%s:%u:%u:%u;", (const char*)Binding.Name,
                                               (u32)Binding.Type, Binding.Slot, Binding.Count ) );
        }

        ResourceInterfaces.insert( (const char*)ResourceInterface );
        BinaryBytes += Output.BinarySize;
        Json += xstring( xfs( "    {\"name\":\"%s\",\"stage\":\"%s\",\"program_id\":\"%016llx\",\"target\":\"%s\",\"bytes\":%d,\"input_id\":%u,\"output_id\":%u,\"bindings\":[", (const char*)Output.ShaderName, (const char*)Stage->StageShort, (unsigned long long)Stage->ProgramId, (const char*)Output.Target, Output.BinarySize, ShaderInterfaceId( Output.Inputs ), ShaderInterfaceId( Output.Outputs ) ) );

        for( s32 j = 0; j < Output.Bindings.GetCount(); ++j )
        {
            const shader_reflection_binding& Binding = Output.Bindings[j];

            if( j )
                Json += ",";
            Json += xstring( xfs( "{\"name\":\"%s\",\"kind\":%u,\"slot\":%u,\"count\":%u,\"bytes\":%u}", (const char*)Binding.Name, (u32)Binding.Type, Binding.Slot, Binding.Count, Binding.ByteSize ) );
        }

        Json += "],\"members\":[";

        for( s32 j = 0; j < Output.Members.GetCount(); ++j )
        {
            const shader_buffer_member& Member = Output.Members[j];

            if( j )
                Json += ",";
            Json += xstring( xfs( "{\"buffer\":\"%s\",\"name\":\"%s\",\"offset\":%u,\"bytes\":%u,\"array_stride\":%u,\"matrix_stride\":%u}", (const char*)Member.Buffer, (const char*)Member.Name, Member.Offset, Member.Size, Member.ArrayStride, Member.MatrixStride ) );
        }

        Json += "]}";
        Json += i + 1 == Script.FinalOutputs.GetCount() ? "\n" : ",\n";
        // Both targets have already been checked against each other.
        if( Output.Target == "vulkan" || !( Script.TargetMask & SHADER_TARGET_VULKAN ) )
        {
            Cpp += xstring( xfs( "struct ShaderBindings_%s\n{\n", (const char*)Output.ShaderName ) );

            for( s32 j = 0; j < Output.Bindings.GetCount(); ++j )
                Cpp += xstring( xfs( "    static constexpr std::uint32_t %s = %u;\n", (const char*)Output.Bindings[j].Name, Output.Bindings[j].Slot ) );
            Cpp += "};\n";
        }
    }

    Json += "  ],\n  \"programs\": [\n";
    u64 FixedId = 0;

    for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
    {
        const shader_program_recipe& Program  = Script.Programs[i];
        const u64                    ComboKey = Program.Kind == SHADER_COMBO_FIXED ? FixedId++ : Program.Key;
        Json += xstring( xfs( "    {\"name\":\"%s\",\"kind\":%u,\"key\":\"%016llx\",\"vertex\":\"%s\",\"pixel\":\"%s\",\"package\":\"%s.ecs\"}", (const char*)Program.Name, static_cast<u32>( Program.Kind ), (unsigned long long)ComboKey, (const char*)Program.Vertex, (const char*)Program.Pixel, (const char*)Program.Package ) );
        Json += i + 1 == Script.Programs.GetCount() ? "\n" : ",\n";
    }

    u32 VertexCount = 0;
    u32 PixelCount  = 0;

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
    {
        VertexCount += Script.Shaders[i].StageShort == "vs";
        PixelCount += Script.Shaders[i].StageShort == "ps";
    }

    Json += xstring( xfs( "  ],\n  \"metrics\": {\"unique_vs\":%u,\"unique_ps\":%u,\"resource_interfaces\":%u,\"binding_entries\":%llu,\"bytecode_bytes\":%llu}\n}\n",
                          VertexCount, PixelCount, (u32)ResourceInterfaces.size(), (unsigned long long)BindingEntries, (unsigned long long)BinaryBytes ) );
    if( ShaderConsoleVerbose() )
        x_printf( "Catalog report: %d recipes, %u VS, %u PS, %u resource interfaces, %d target binaries, %llu bytecode bytes\n",
                  Script.Programs.GetCount(), VertexCount, PixelCount, (u32)ResourceInterfaces.size(),
                  Script.FinalOutputs.GetCount(), (unsigned long long)BinaryBytes );
    const xstring Folder = JoinPath( Script.OutRoot, Script.Config );
    return WriteGenerated( JoinPath( Folder, "catalog.json" ), Json ) &&
           WriteGenerated( JoinPath( Folder, "shader_bindings.hpp" ), Cpp );
}

//==============================================================================

xbool ShaderSourcesUnchanged( const shader_script& Script )
{
    if( !Script.Sources )
        return FALSE;

    for( s32 i = 0; i < Script.Sources->Files.GetCount(); ++i )
    {
        const shader_source_file& File = Script.Sources->Files[i];
        xarray<byte>              Current;

        if( !LoadBinaryFile( File.Path, Current ) || Current.GetCount() != File.Bytes.GetCount() ||
            x_memcmp( Current.GetPtr(), File.Bytes.GetPtr(), Current.GetCount() ) )
        {
            x_printf( "Error: shader source changed during compilation: %s\n", (const char*)File.Path );
            return FALSE;
        }
    }

    return TRUE;
}
