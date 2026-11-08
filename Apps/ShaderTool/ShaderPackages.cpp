//==============================================================================
//  Family packages are self-contained. Shared stages are duplicated on disk,
//  while the native registry uses their content identity to share GPU objects.
//==============================================================================
#include "ShaderTool.hpp"
#include <cstring>
#include <map>
#include <set>
#include "x_threads.hpp"

namespace
{
struct shader_package_intermediate : public xref_counted
{
    xarray<shader_compile_output> Outputs;
};

struct shader_package_statistics
{
    u32 Blocks = 0;
    u32 Zstd   = 0;
    u32 Cached = 0;
    u32 Raw    = 0;
};

struct shader_block_cache
{
    xstring                    Root;
    xmutex                     Lock;
    xstring                    Name;
    ecs_package_build_options* pBuild     = NULL;
    xtick                      Start      = 0;
    xtick                      LastReport = 0;
};

//==============================================================================

void ReportPackedBlocks( void* pContext, u32 Completed, u32 Total )
{
    shader_block_cache& Cache = *static_cast<shader_block_cache*>( pContext );
    const xtick         Now   = x_GetTime();

    if( !Completed || Completed == Total || x_TicksToMs( Now - Cache.LastReport ) >= 1000 )
    {
        UpdateShaderPackageProgress( Cache.Name, Completed, Total, Cache.pBuild->ZstdBlocks,
                                     Cache.pBuild->CachedBlocks, Cache.pBuild->RawBlocks, "compressing" );
        Cache.LastReport = Now;
    }
}

//==============================================================================

xstring BlockCachePath( const shader_block_cache& Cache, const ecs_content_id& Key )
{
    char              Name[71];
    static const char Hex[] = "0123456789abcdef";

    for( s32 i = 0; i < 32; ++i )
    {
        Name[i * 2]     = Hex[Key[i] >> 4];
        Name[i * 2 + 1] = Hex[Key[i] & 15];
    }

    x_strcpy( Name + 64, ".block" );
    return JoinPath( Cache.Root, Name );
}

//==============================================================================

xbool LoadPackedBlock( void* pContext, const ecs_content_id& Key, ecs_package_block& Block, xarray<byte>& Stored )
{
    shader_block_cache& Cache = *static_cast<shader_block_cache*>( pContext );
    xarray<byte>        File;
    Cache.Lock.Enter();
    const xbool Loaded = LoadBinaryFile( BlockCachePath( Cache, Key ), File );
    Cache.Lock.Exit();

    if( !Loaded || File.GetCount() < 96 )
        return FALSE;

    const byte* pHeader = File.GetPtr();

    if( x_load_le32( pHeader ) != 0x314b4245 || x_load_le32( pHeader + 4 ) != Block.DecodedSize ||
        x_load_le32( pHeader + 8 ) != static_cast<u32>( File.GetCount() - 96 ) ||
        x_memcmp( pHeader + 32, Key.GetPtr(), 32 ) )
        return FALSE;

    ecs_content_hash Hash;
    Hash.Update( pHeader, 64 );
    Hash.Update( pHeader + 96, File.GetCount() - 96 );
    const ecs_content_id Digest = Hash.Finish();

    if( x_memcmp( Digest.GetPtr(), pHeader + 64, 32 ) )
        return FALSE;

    const u32 Codec = x_load_le32( pHeader + 12 );

    if( ( Codec != ECS_BLOCK_RAW && Codec != ECS_BLOCK_ZSTD ) ||
        x_load_le32( pHeader + 8 ) > ecs_CompressBound( Block.DecodedSize ) ||
        ( Codec == ECS_BLOCK_RAW && x_load_le32( pHeader + 8 ) != Block.DecodedSize ) )
        return FALSE;

    Block.StoredSize = File.GetCount() - 96;
    Block.Codec      = static_cast<ecs_block_codec>( Codec );
    Block.StoredCRC  = x_load_le32( pHeader + 16 );
    Block.DecodedCRC = x_load_le32( pHeader + 20 );

    if( x_chksum( pHeader + 96, Block.StoredSize ) != Block.StoredCRC )
        return FALSE;

    Stored.SetCount( Block.StoredSize );
    x_memcpy( Stored.GetPtr(), pHeader + 96, Block.StoredSize );
    return TRUE;
}

//==============================================================================

xbool SavePackedBlock( void* pContext, const ecs_content_id& Key, const ecs_package_block& Block,
                       const xarray<byte>& Stored )
{
    shader_block_cache& Cache = *static_cast<shader_block_cache*>( pContext );
    xarray<byte>        File;
    File.SetCount( 96 + Stored.GetCount() );
    byte* pHeader = File.GetPtr();
    x_memset( pHeader, 0, 96 );
    x_store_le32( pHeader, 0x314b4245 );
    x_store_le32( pHeader + 4, Block.DecodedSize );
    x_store_le32( pHeader + 8, Block.StoredSize );
    x_store_le32( pHeader + 12, Block.Codec );
    x_store_le32( pHeader + 16, Block.StoredCRC );
    x_store_le32( pHeader + 20, Block.DecodedCRC );
    x_memcpy( pHeader + 32, Key.GetPtr(), 32 );
    x_memcpy( pHeader + 96, Stored.GetPtr(), Stored.GetCount() );
    ecs_content_hash Hash;
    Hash.Update( pHeader, 64 );
    Hash.Update( pHeader + 96, Stored.GetCount() );
    const ecs_content_id Digest = Hash.Finish();
    x_memcpy( pHeader + 64, Digest.GetPtr(), 32 );
    const xstring Path      = BlockCachePath( Cache, Key );
    const xstring Temporary = Path + ".tmp";
    Cache.Lock.Enter();
    const xbool Saved = WriteBinaryFile( Temporary, File.GetPtr(), File.GetCount() ) && ReplaceFile( Temporary, Path );
    Cache.Lock.Exit();
    return Saved;
}

//==============================================================================

ecs_package_layout MakeLayout( const shader_compile_output& Output )
{
    ecs_package_layout Layout;
    Layout.InputInterface  = ShaderInterfaceId( Output.Inputs );
    Layout.OutputInterface = ShaderInterfaceId( Output.Outputs );
    Layout.Resources[0]    = Output.Resources.SamplerCount;
    Layout.Resources[1]    = Output.Resources.UniformBufferCount;
    Layout.Resources[2]    = Output.Resources.StorageTextureCount;
    Layout.Resources[3]    = Output.Resources.StorageBufferCount;
    Layout.Bindings.SetCapacity( Output.Bindings.GetCount() );
    Layout.Members.SetCapacity( Output.Members.GetCount() );
    Layout.Inputs.SetCapacity( Output.Inputs.GetCount() );
    Layout.Outputs.SetCapacity( Output.Outputs.GetCount() );

    for( s32 i = 0; i < Output.Bindings.GetCount(); ++i )
    {
        const auto& Binding = Output.Bindings[i];
        Layout.Bindings.Append( { static_cast<u32>( Binding.Type ), Binding.Slot, Binding.Count, Binding.ByteSize,
                                  (const char*)Binding.Name } );
    }

    for( s32 i = 0; i < Output.Members.GetCount(); ++i )
    {
        const auto& Member = Output.Members[i];
        Layout.Members.Append( { (const char*)Member.Buffer, (const char*)Member.Name, Member.Offset, Member.Size,
                                 Member.ArrayStride, Member.MatrixStride } );
    }

    for( s32 i = 0; i < Output.Inputs.GetCount(); ++i )
    {
        const auto& Variable = Output.Inputs[i];
        Layout.Inputs.Append(
            { (const char*)Variable.Semantic, Variable.Index, Variable.Components, Variable.ComponentType } );
    }

    for( s32 i = 0; i < Output.Outputs.GetCount(); ++i )
    {
        const auto& Variable = Output.Outputs[i];
        Layout.Outputs.Append(
            { (const char*)Variable.Semantic, Variable.Index, Variable.Components, Variable.ComponentType } );
    }

    return Layout;
}

struct shader_package_catalog
{
    std::map<std::string, const shader_entry*> Stages;
    xarray<u64>                                FixedIds;

    explicit shader_package_catalog( const shader_script& Script )
    {
        for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
            Stages.emplace( (const char*)Script.Shaders[i].Name, &Script.Shaders[i] );
        FixedIds.SetCount( Script.Programs.GetCount() );
        u64 FixedId = 0;

        for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
            if( Script.Programs[i].Kind == SHADER_COMBO_FIXED )
                FixedIds[i] = FixedId++;
    }
};

//==============================================================================

xbool BuildShaderPackage( const shader_script& Script, const compile_options& Options,
                          const shader_package_catalog& Catalog, const xstring& PackageName,
                          const xarray<s32>& Recipes, xarray<byte>& File, xstring& Error, shader_package_statistics* pStatistics = NULL )
{
    File.Clear();
    Error.Clear();

    if( PackageName.IsEmpty() || !Recipes.GetCount() )
    {
        Error = "package has no name or recipes";
        return FALSE;
    }

    ecs_package_index Package;
    Package.CatalogId             = Script.CatalogId;
    Package.Name                  = (const char*)PackageName;
    const ecs_content_id Identity = ecs_HashContent( reinterpret_cast<const byte*>( (const char*)Package.Name ),
                                                     static_cast<u32>( Package.Name.GetLength() ) );
    for( u32 i = 0; i < 8; ++i )
        Package.PackageId |= static_cast<u64>( Identity[i] ) << ( i * 8 );
    Package.TargetMask             = Script.TargetMask;
    const auto&           Stages   = Catalog.Stages;
    const auto&           FixedIds = Catalog.FixedIds;
    std::set<std::string> RequiredStages;
    Package.Combos.SetCapacity( Recipes.GetCount() );

    for( s32 i = 0; i < Recipes.GetCount(); ++i )
    {
        const s32 RecipeIndex = Recipes[i];

        if( RecipeIndex < 0 || RecipeIndex >= Script.Programs.GetCount() )
        {
            Error = "invalid package recipe index";
            return FALSE;
        }

        const auto& Recipe = Script.Programs[RecipeIndex];

        if( Recipe.Package != PackageName )
        {
            Error = "recipe belongs to another package";
            return FALSE;
        }

        const auto Vertex = Stages.find( (const char*)Recipe.Vertex );
        const auto Pixel  = Stages.find( (const char*)Recipe.Pixel );

        if( Vertex == Stages.end() || Pixel == Stages.end() )
        {
            Error = "package references an undeclared stage";
            return FALSE;
        }

        Package.Combos.Append( { Recipe.Kind == SHADER_COMBO_KEYED ? Recipe.Key : FixedIds[RecipeIndex],
                                 Vertex->second->ProgramId, Pixel->second->ProgramId, static_cast<u32>( Recipe.Kind ),
                                 0 } );
        RequiredStages.emplace( Vertex->first );
        RequiredStages.emplace( Pixel->first );
    }

    xarray<const xarray<byte>*>               Bytecode;
    xarray<xref<shader_package_intermediate>> Intermediates;
    const u32                                 Targets = !!( Script.TargetMask & SHADER_TARGET_D3D12 ) + 2 * !!( Script.TargetMask & SHADER_TARGET_VULKAN );

    if( !Targets || RequiredStages.size() > ECS_PACKAGE_MAX_RECORDS / Targets )
    {
        Error = "package stage count exceeds format limit";
        return FALSE;
    }

    const s32 ProgramCount = static_cast<s32>( RequiredStages.size() * Targets );
    Bytecode.SetCapacity( ProgramCount );
    Intermediates.SetCapacity( RequiredStages.size() );
    Package.Programs.SetCapacity( ProgramCount );
    Package.Layouts.SetCapacity( ProgramCount );
    Package.Binaries.SetCapacity( ProgramCount );
    compile_options CacheOptions = Options;
    CacheOptions.Clean           = FALSE;

    for( const auto& Name : RequiredStages )
    {
        const shader_entry&               Stage = *Stages.at( Name );
        xref<shader_package_intermediate> Intermediate( new shader_package_intermediate );
        Intermediates.Append( Intermediate );
        xarray<shader_compile_output>& Outputs = Intermediate->Outputs;

        if( !LoadShaderCache( Script, Stage, CacheOptions, Outputs ) )
        {
            Error = "missing validated intermediate shader cache for " + xstring( Name.c_str() );
            return FALSE;
        }

        for( s32 i = 0; i < Outputs.GetCount(); ++i )
        {
            auto&     Output = Outputs[i];
            const u32 Format = Output.Target == "vulkan" ? 1 : Output.Target == "d3d12" ? 2
                                                                                        : 0;
            if( !Format || ( Stage.StageShort != "vs" && Stage.StageShort != "ps" ) )
            {
                Error = "unsupported package stage or target";
                return FALSE;
            }

            ecs_package_program Program;
            Program.Id                   = Stage.ProgramId;
            Program.Stage                = Stage.StageShort == "vs" ? 0 : 1;
            Program.Format               = Format;
            Program.RequiredCapabilities = Output.RequiredCapabilities;
            Program.Name                 = Name.c_str();
            Program.Entry                = (const char*)Stage.Entry;
            Program.Layout               = static_cast<u32>( Package.Layouts.GetCount() );
            Program.Binary               = static_cast<u32>( Package.Binaries.GetCount() );
            Package.Layouts.Append( MakeLayout( Output ) );
            ecs_package_binary Binary;
            Binary.Format = Format;
            Package.Binaries.Append( Binary );
            Package.Programs.Append( std::move( Program ) );
            Bytecode.Append( &Output.Code );
            Output.Bindings.Clear();
            Output.Members.Clear();
            Output.Inputs.Clear();
            Output.Outputs.Clear();
        }
    }

    shader_block_cache Cache;
    Cache.Root = JoinPath( Script.CacheRoot.IsEmpty() ? JoinPath( Script.OutRoot, ".cache" ) : Script.CacheRoot, "blocks" );
    ecs_package_build_options Build;
    Build.WorkerCount = Options.WorkerCount;
    Build.pContext    = &Cache;
    Build.LoadBlock   = Options.Clean ? NULL : LoadPackedBlock;
    Build.SaveBlock   = SavePackedBlock;
    Build.Progress    = ReportPackedBlocks;
    const xtick Start = x_GetTime();
    Cache.Start = Cache.LastReport = Start;
    Cache.Name                     = PackageName + ".ecs";
    Cache.pBuild                   = &Build;
    const xbool Built              = ecs_BuildPackage( Package, Bytecode, File, Error, &Build );

    if( Options.Verbose )
        x_printf( "  Packing %s: %.3f ms, compressed=%u cached=%u peak temporary=%llu bytes\n",
                  (const char*)PackageName, x_TicksToMs( x_GetTime() - Start ), Build.CompressedBlocks,
                  Build.CachedBlocks, (unsigned long long)Build.PeakTemporaryBytes );
    if( Options.Verbose )
        x_printf( "    prepare=%.3f compress/cache/verify=%.3f serialize=%.3f ms\n",
                  Build.PreparationMs, Build.CompressionMs, Build.SerializationMs );
    if( pStatistics )
    {
        pStatistics->Blocks = Package.Blocks.GetCount();
        pStatistics->Zstd   = Build.ZstdBlocks;
        pStatistics->Cached = Build.CachedBlocks;
        pStatistics->Raw    = Build.RawBlocks;
    }

    return Built;
}
} // namespace

xbool BuildShaderPackage( const shader_script& Script, const compile_options& Options, const xstring& PackageName,
                          const xarray<s32>& Recipes, xarray<byte>& File, xstring& Error )
{
    const shader_package_catalog Catalog( Script );
    return BuildShaderPackage( Script, Options, Catalog, PackageName, Recipes, File, Error );
}

// Publication operates on the staging directory after every stage and pair
// has passed reflection validation. ProcessScript publishes the whole kit.
xbool WriteShaderPackages( const shader_script& Script, const compile_options& Options )
{
    std::map<std::string, xarray<s32>> Families;

    for( s32 i = 0; i < Script.Programs.GetCount(); ++i )
    {
        if( Script.Programs[i].Package.IsEmpty() )
        {
            x_printf( "Error: recipe '%s' has no package assignment\n", (const char*)Script.Programs[i].Name );
            return FALSE;
        }

        Families[(const char*)Script.Programs[i].Package].Append() = i;
    }

    std::string                  Manifest;
    const shader_package_catalog Catalog( Script );

    for( const auto& Family : Families )
    {
        const xtick       PackageStart = x_GetTime();
        const std::string Name         = Family.first + ".ecs";
        BeginShaderPackageProgress( Name.c_str() );
        shader_package_statistics Stats;
        xarray<byte>              File;
        xstring                   Error;

        if( !BuildShaderPackage( Script, Options, Catalog, Family.first.c_str(), Family.second, File, Error, &Stats ) )
        {
            x_printf( "Error: package '%s': %s\n", Family.first.c_str(), (const char*)Error );
            return FALSE;
        }

        const xstring Path =
            JoinPath( JoinPath( Script.OutRoot, Script.Config ), xstring( Family.first.c_str() ) + ".ecs" );
        UpdateShaderPackageProgress( Name.c_str(), Stats.Blocks, Stats.Blocks, Stats.Zstd, Stats.Cached, Stats.Raw, "writing" );

        if( !WriteBinaryFile( Path, File.GetPtr(), static_cast<s32>( File.GetCount() ) ) )
            return FALSE;

        UpdateShaderPackageProgress( Name.c_str(), Stats.Blocks, Stats.Blocks, Stats.Zstd, Stats.Cached, Stats.Raw, "verifying" );
        xarray<byte> Written;

        if( !LoadBinaryFile( Path, Written ) || Written.GetCount() != static_cast<s32>( File.GetCount() ) ||
            std::memcmp( Written.GetPtr(), File.GetPtr(), File.GetCount() ) )
            return FALSE;

        const auto Read = []( void* pContext, u32 Offset, byte* pData, u32 Size ) -> xbool
        {
            const auto& Bytes = *static_cast<const xarray<byte>*>( pContext );

            if( Offset > static_cast<u32>( Bytes.GetCount() ) || Size > static_cast<u32>( Bytes.GetCount() ) - Offset )
                return FALSE;

            std::memcpy( pData, Bytes.GetPtr() + Offset, Size );
            return TRUE;
        };

        ecs_package_index Index;

        if( !ecs_ReadPackageIndex( Read, &Written, Written.GetCount(), Script.CatalogId, Index, Error ) )
            return FALSE;

        u64 Stored = 0, Decoded = 0;

        for( u32 i = 0; i < Index.Blocks.GetCount(); ++i )
        {
            Stored += Index.Blocks[i].StoredSize;
            Decoded += Index.Blocks[i].DecodedSize;
        }

        const auto Digest = ecs_HashContent( File.GetPtr(), static_cast<u32>( File.GetCount() ) );
        UpdateShaderPackageProgress( Name.c_str(), Stats.Blocks, Stats.Blocks, Stats.Zstd, Stats.Cached, Stats.Raw,
                                     "done", TRUE, x_TicksToMs( x_GetTime() - PackageStart ) );
        static const char Hex[] = "0123456789abcdef";

        for( s32 i = 0; i < 32; ++i )
        {
            Manifest += Hex[Digest[i] >> 4];
            Manifest += Hex[Digest[i] & 15];
        }

        Manifest += "  " + Family.first + ".ecs\n";

        if( Options.Verbose )
            x_printf(
                "  Package %s: %d combos, %d unique binaries, %d interfaces, %d blocks, stored=%llu decoded=%llu file=%u\n",
                Family.first.c_str(), Family.second.GetCount(), Index.Binaries.GetCount(), Index.Layouts.GetCount(),
                Index.Blocks.GetCount(), (unsigned long long)Stored, (unsigned long long)Decoded,
                static_cast<u32>( File.GetCount() ) );
    }

    const xstring Checksums = JoinPath( JoinPath( Script.OutRoot, Script.Config ), "SHA256SUMS" );
    return WriteBinaryFile( Checksums, Manifest.data(), static_cast<s32>( Manifest.size() ) );
}
