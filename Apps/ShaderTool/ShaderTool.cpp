//==============================================================================
//
//  ShaderTool.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderTool.hpp"

#include <errno.h>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <atomic>
#include <thread>
#include <map>
#include "x_threads.hpp"

#if defined( TARGET_WINDOWS )
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static
s32 FindOption( command_line& CommandLine, const char* pName )
{
    xstring Name( pName );
    return CommandLine.FindOption( Name );
}

//==============================================================================

static
xbool IsSlash( char C )
{
    return ( C == '/' ) || ( C == '\\' );
}

//==============================================================================

static
xbool CreateDirectoryOne( const xstring& Path )
{
    if( Path.IsEmpty() )
        return TRUE;

#if defined( TARGET_WINDOWS )
    DWORD Attrib = GetFileAttributesA( (const char*)Path );

    if( ( Attrib != INVALID_FILE_ATTRIBUTES ) && ( Attrib & FILE_ATTRIBUTE_DIRECTORY ) )
        return TRUE;

    if( _mkdir( (const char*)Path ) == 0 )
        return TRUE;

    Attrib = GetFileAttributesA( (const char*)Path );
    return ( ( Attrib != INVALID_FILE_ATTRIBUTES ) && ( Attrib & FILE_ATTRIBUTE_DIRECTORY ) );
#else
    struct stat Info;

    if( stat( (const char*)Path, &Info ) == 0 )
        return S_ISDIR( Info.st_mode ) ? TRUE : FALSE;

    if( mkdir( (const char*)Path, 0755 ) == 0 )
        return TRUE;

    if( errno != EEXIST )
        return FALSE;

    return ( stat( (const char*)Path, &Info ) == 0 ) && S_ISDIR( Info.st_mode );
#endif
}

//==============================================================================

static
const char* TargetFolder( const char* pTarget )
{
    if( x_stricmp( pTarget, "vulkan" ) == 0 )
        return "vulkan";

    if( x_stricmp( pTarget, "d3d12" ) == 0 )
        return "d3d12";

    if( x_stricmp( pTarget, "metal" ) == 0 )
        return "metal";

    // Should never get here! How?
    x_printf( "Error: Unknown target '%s' in TargetFolder\n", pTarget );
    return "";
}

//==============================================================================

static
const char* TargetExt( const char* pTarget )
{
    if( x_stricmp( pTarget, "vulkan" ) == 0 )
        return "spv";

    if( x_stricmp( pTarget, "d3d12" ) == 0 )
        return "dxil";

    if( x_stricmp( pTarget, "metal" ) == 0 )
        return "metal";

    // Should never get here! How?
    x_printf( "Error: Unknown target '%s' in TargetExt\n", pTarget );
    return "";
}

//==============================================================================

static
xstring BlobFileName( const shader_entry& Shader, const char* pExt )
{
    return xstring( (const char*)xfs( "%s.%s.%s", (const char*)Shader.Name,
                                      (const char*)Shader.StageShort,
                                      pExt ) );
}

//==============================================================================

static
xstring MetaFileName( const shader_entry& Shader )
{
    return xstring( (const char*)xfs( "%s.%s.meta", (const char*)Shader.Name,
                                      (const char*)Shader.StageShort ) );
}

//==============================================================================

static
void RemoveTempOutputs( const xarray<shader_compile_output>& Outputs )
{
    for( s32 i = 0; i < Outputs.GetCount(); i++ )
        RemoveFile( Outputs[i].TempPdb );
}

//==============================================================================

static
xbool RemovePublishedPdbs( const xarray<shader_compile_output>& Outputs )
{
    for( s32 i = 0; i < Outputs.GetCount(); i++ )
    {
        if( ( Outputs[i].Target == "d3d12" ) && !RemoveFile( Outputs[i].Pdb ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool CommitPdbOutputs( const xarray<shader_compile_output>& Outputs )
{
    for( s32 i = 0; i < Outputs.GetCount(); i++ )
    {
        const shader_compile_output& Output = Outputs[i];

        if( ( Output.Target == "d3d12" ) && Output.HasPdb &&
            !ReplaceFile( Output.TempPdb, Output.Pdb ) )
        {
            x_printf( "Error: Failed to replace PDB %s\n", (const char*)Output.Pdb );
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xstring ToLower( const xstring& Text )
{
    xstring Result = Text;
    Result.MakeLower();
    return Result;
}

//==============================================================================

xbool IsAbsolutePath( const xstring& Path )
{
    if( ( Path.GetLength() >= 2 ) && ( Path[1] == ':' ) )
        return TRUE;

    if( ( Path.GetLength() >= 1 ) && IsSlash( Path[0] ) )
        return TRUE;

    return FALSE;
}

//==============================================================================

xstring JoinPath( const xstring& Path, const xstring& File )
{
    if( Path.IsEmpty() )
        return File;

    if( File.IsEmpty() )
        return Path;

    if( IsSlash( Path[Path.GetLength() - 1] ) )
        return Path + File;

#if defined( TARGET_WINDOWS )
    return Path + "\\" + File;
#else
    return Path + "/" + File;
#endif
}

//==============================================================================

xstring NormalizePath( const xstring& Path )
{
#if defined( TARGET_WINDOWS )
    char  Buffer[MAX_PATH];
    DWORD Length = GetFullPathNameA( (const char*)Path, MAX_PATH, Buffer, NULL );

    if( ( Length > 0 ) && ( Length < MAX_PATH ) )
        return xstring( Buffer );
#endif

    return Path;
}

//==============================================================================

xstring MakeRelativePath( const xstring& BasePath, const xstring& Path )
{
    if( IsAbsolutePath( Path ) )
        return NormalizePath( Path );

    return NormalizePath( JoinPath( BasePath, Path ) );
}

//==============================================================================

xstring GetDirectory( const xstring& PathName )
{
    xstring Path;
    xstring File;

    command_line::SplitPath( PathName, Path, File );
    return Path;
}

//==============================================================================

xbool EnsureParentDir( const xstring& PathName )
{
    xstring Path = GetDirectory( PathName );

    if( Path.IsEmpty() )
        return TRUE;

    xstring Current;
    s32     Start = 0;

    if( ( Path.GetLength() >= 2 ) && ( Path[1] == ':' ) )
    {
        Current = Path.Left( 2 );
        Start   = 2;
    }
    else if( ( Path.GetLength() > 0 ) && IsSlash( Path[0] ) )
    {
        Current = Path.Left( 1 );
        Start   = 1;
    }

    for( s32 i = Start; i < Path.GetLength(); i++ )
    {
        Current += Path[i];

        if( IsSlash( Path[i] ) )
        {
            if( !CreateDirectoryOne( Current ) )
                return FALSE;
        }
    }

    return CreateDirectoryOne( Path );
}

//==============================================================================

xbool LoadBinaryFile( const xstring& PathName, xarray<byte>& Data )
{
    X_FILE* pFile = x_fopen( PathName, "rb" );

    if( !pFile )
        return FALSE;

    const s32 Size = x_flength( pFile );

    if( Size <= 0 )
    {
        x_fclose( pFile );
        return FALSE;
    }

    Data.SetCount( Size );
    const s32 Read = x_fread( Data.GetPtr(), 1, Size, pFile );
    x_fclose( pFile );

    return ( Read == Size );
}

//==============================================================================

xbool WriteBinaryFile( const xstring& PathName, const void* pData, s32 Size )
{
    if( !EnsureParentDir( PathName ) )
        return FALSE;

    X_FILE* pFile = x_fopen( PathName, "wb" );

    if( !pFile )
        return FALSE;

    const s32 Written = x_fwrite( pData, Size, 1, pFile );
    x_fclose( pFile );

    return ( Written == 1 );
}

//==============================================================================

xbool RemoveFile( const xstring& PathName )
{
#if defined( TARGET_WINDOWS )
    if( DeleteFileA( (const char*)PathName ) )
        return TRUE;

    const DWORD Error = GetLastError();
    return ( Error == ERROR_FILE_NOT_FOUND ) || ( Error == ERROR_PATH_NOT_FOUND );
#else
    if( remove( (const char*)PathName ) == 0 )
        return TRUE;

    X_FILE* pFile = x_fopen( PathName, "rb" );

    if( !pFile )
        return TRUE;

    x_fclose( pFile );
    return FALSE;
#endif
}

//==============================================================================

xbool ReplaceFile( const xstring& TempPath, const xstring& FinalPath )
{
#if defined( TARGET_WINDOWS )
    return MoveFileExA( (const char*)TempPath,
                        (const char*)FinalPath,
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH );
#else
    RemoveFile( FinalPath );
    return ( rename( (const char*)TempPath, (const char*)FinalPath ) == 0 );
#endif
}

//==============================================================================

xstring BlobPath( const shader_script& Script,
                  const shader_entry&  Shader,
                  const char*          pTarget )
{
    return JoinPath( JoinPath( JoinPath( Script.OutRoot, Script.Config ), TargetFolder( pTarget ) ),
                     BlobFileName( Shader, TargetExt( pTarget ) ) );
}

//==============================================================================

//==============================================================================

xstring MetaPath( const shader_script& Script, const shader_entry& Shader )
{
    return JoinPath( JoinPath( Script.OutRoot, Script.Config ), MetaFileName( Shader ) );
}

//==============================================================================

xstring PdbPath( const shader_script& Script, const shader_entry& Shader )
{
    return JoinPath( JoinPath( JoinPath( Script.OutRoot, Script.Config ), "d3d12" ),
                     BlobFileName( Shader, "pdb" ) );
}

//==============================================================================

xstring TempPathFor( const xstring& PathName )
{
#if defined( TARGET_WINDOWS )
    return xstring( (const char*)xfs( "%s.tmp.%u", (const char*)PathName,
                                      (u32)GetCurrentProcessId() ) );
#else
    return xstring( (const char*)xfs( "%s.tmp.%u", (const char*)PathName, (u32)getpid() ) );
#endif
}

//==============================================================================

const char* BindingModelName( shader_binding_model Model )
{
    if( Model == SHADER_BINDING_SDL )
        return "sdl";

    if( Model == SHADER_BINDING_NATIVE )
        return "native";

    // Should never get here! How?
    x_printf( "Error: Unknown model '%d' in BindingModelName\n", Model );
    return "";
}

//==============================================================================

void DisplayHelp( void )
{
    x_printf( "\n" );
    x_printf( "ShaderTool (c)2002 Inevitable Entertainment Inc.\n" );
    x_printf( "\n" );
    x_printf( "  usage:\n" );
    x_printf( "         ShaderTool [-clean] [-v] <script.shaderscript>\n" );
    x_printf( "\n" );
    x_printf( "options:\n" );
    x_printf( "         -clean      - Remove old blobs before writing new ones\n" );
    x_printf( "         -v          - Verbose output\n" );
    x_printf( "         -stage name[*] - Compile selected stages without publishing a kit\n" );
    x_printf( "         -config debug|release - Override the script build configuration\n" );
    x_printf( "         -jobs 1..12  - Shader compilation workers (default: 1)\n" );
    x_printf( "         -generate   - Generate shared ABI and identities without compiling stages\n" );
    x_printf( "\n" );
}

//==============================================================================

xbool ParseCommandLine( s32 argc, char** argv, compile_options& Options )
{
    command_line CommandLine;

    CommandLine.AddOptionDef( "CLEAN" );
    CommandLine.AddOptionDef( "V" );
    CommandLine.AddOptionDef( "GENERATE" );
    CommandLine.AddOptionDef( "STAGE", command_line::STRING );
    CommandLine.AddOptionDef( "JOBS", command_line::STRING );
    CommandLine.AddOptionDef( "CONFIG", command_line::STRING );

    xbool NeedHelp = CommandLine.Parse( argc, argv );

    if( NeedHelp || ( CommandLine.GetNumArguments() == 0 ) )
        return FALSE;

    const s32 Config = FindOption( CommandLine, "CONFIG" );

    if( Config >= 0 )
    {
        Options.Config = CommandLine.GetOptionString( Config );

        if( Options.Config != "debug" && Options.Config != "release" )
        {
            x_printf( "Error: config must be debug or release\n" );
            return FALSE;
        }
    }

    const s32 Stage = FindOption( CommandLine, "STAGE" );

    if( Stage >= 0 )
        Options.StageFilter = CommandLine.GetOptionString( Stage );
    const s32 Jobs = FindOption( CommandLine, "JOBS" );

    if( Jobs >= 0 )
    {
        const xstring& Count       = CommandLine.GetOptionString( Jobs );
        const s32      WorkerCount = x_atoi( Count );

        if( !( ( Count.GetLength() == 1 && Count[0] >= '1' && Count[0] <= '9' ) ||
               ( Count.GetLength() == 2 && Count[0] == '1' && Count[1] >= '0' && Count[1] <= '2' ) ) )
        {
            x_printf( "Error: -jobs must be between 1 and 12\n" );
            return FALSE;
        }

        Options.WorkerCount = WorkerCount;
    }

    Options.GenerateOnly = ( FindOption( CommandLine, "GENERATE" ) != -1 );
    Options.Clean        = ( FindOption( CommandLine, "CLEAN" ) != -1 );
    Options.Verbose      = ( FindOption( CommandLine, "V" ) != -1 );

    for( s32 i = 0; i < CommandLine.GetNumArguments(); i++ )
        Options.Scripts.Append( CommandLine.GetArgument( i ) );

    return TRUE;
}

//==============================================================================

static
void RetainReflection( shader_script& Script, shader_compile_output const& Output )
{
    shader_compile_output& Final = Script.FinalOutputs.Append();
    Final.Target                 = Output.Target;
    Final.RequiredCapabilities   = Output.RequiredCapabilities;
    Final.ShaderName             = Output.ShaderName;
    Final.BinarySize             = Output.BinarySize ? Output.BinarySize : Output.Code.GetCount();
    Final.Resources              = Output.Resources;
    Final.Bindings               = Output.Bindings;
    Final.Inputs                 = Output.Inputs;
    Final.Outputs                = Output.Outputs;
    Final.Members                = Output.Members;
}

//==============================================================================

xbool CompileShader( shader_script&         Script,
                     const shader_entry&    SourceShader,
                     const compile_options& Options )
{
    shader_entry Shader = SourceShader;

    if( Options.Verbose )
        x_printf( "Shader: %s\n", (const char*)Shader.Name );
    xarray<shader_compile_output> Cached;

    if( LoadShaderCache( Script, Shader, Options, Cached, FALSE ) && Cached.GetCount() )
    {
        for( s32 i = 0; i < Cached.GetCount(); ++i )
            RetainReflection( Script, Cached[i] );
        if( Options.Verbose )
            x_printf( "  content cache hit\n" );
        return TRUE;
    }

    const u32             TargetMask = Script.TargetMask;
    shader_compile_output CachedDxil, CachedSpirv;
    xbool                 HaveDxil = FALSE, HaveSpirv = FALSE;

    for( u32 Target = 1; Target <= 2; Target <<= 1 )
    {
        Script.TargetMask = Target;
        xarray<shader_compile_output> TargetOutput;

        if( LoadShaderCache( Script, Shader, Options, TargetOutput, FALSE ) )
        {
            if( Target == SHADER_TARGET_D3D12 )
            {
                CachedDxil = TargetOutput[0];
                HaveDxil   = TRUE;
            }
            else
            {
                CachedSpirv = TargetOutput[0];
                HaveSpirv   = TRUE;
            }
        }
    }

    Script.TargetMask = TargetMask;
    shader_script Discovery;
    Discovery.Sources          = Script.Sources;
    Discovery.Metrics          = Script.Metrics;
    Discovery.SourceRoot       = Script.SourceRoot;
    Discovery.OutRoot          = Script.OutRoot;
    Discovery.Config           = Script.Config;
    Discovery.BindingModel     = Script.BindingModel;
    Discovery.DiscoverBindings = TRUE;
    Discovery.TargetMask       = SHADER_TARGET_D3D12;
    Discovery.AbiId            = Script.AbiId;
    Discovery.CacheRoot        = JoinPath( Script.CacheRoot.IsEmpty() ? Script.OutRoot : Script.CacheRoot, "discovery" );
    xarray<shader_compile_output> DiscoveryOutputs;

    if( !LoadShaderCache( Discovery, Shader, Options, DiscoveryOutputs, TRUE, TRUE ) )
    {
        DiscoveryOutputs.Clear();
        shader_compile_output& Output = DiscoveryOutputs.Append();

        if( !CompileDxcTarget( Discovery, Shader, Options, "d3d12", Output ) ||
            !WriteShaderCache( Discovery, Shader, Options, DiscoveryOutputs ) )
            return FALSE;
    }

    shader_compile_output&                        Discovered          = DiscoveryOutputs[0];
    const shader_resource_counts&                 DiscoveredResources = Discovered.Resources;
    const xarray<shader_reflection_binding>&      DiscoveredBindings  = Discovered.Bindings;
    xbool                                         DenseBindings       = TRUE;
    std::vector<const shader_reflection_binding*> Ordered;

    for( s32 i = 0; i < DiscoveredBindings.GetCount(); ++i )
        Ordered.push_back( &DiscoveredBindings[i] );
    std::sort( Ordered.begin(), Ordered.end(), []( const shader_reflection_binding* A, const shader_reflection_binding* B )
               { return A->Type == B->Type ? A->Slot < B->Slot : A->Type < B->Type; } );
    u32 ResourceSlot = 0;
    u32 UniformSlot  = 0;

    for( const shader_reflection_binding* Binding : Ordered )
    {
        u32 Original = Binding->Slot;

        if( Binding->Type == SHADER_REFLECT_STORAGE_TEXTURE )
            Original += DiscoveredResources.SamplerCount;
        if( Binding->Type == SHADER_REFLECT_STORAGE_BUFFER )
            Original += DiscoveredResources.SamplerCount + DiscoveredResources.StorageTextureCount;
        if( Original + Binding->Count > 64 )
            return FALSE;

        for( u32 i = 0; i < Binding->Count; ++i )
        {
            if( Binding->Type == SHADER_REFLECT_UNIFORM_BUFFER )
            {
                if( Original + i != UniformSlot )
                    DenseBindings = FALSE;
                Shader.UniformSlots[Original + i] = UniformSlot++;
            }
            else
            {
                if( Original + i != ResourceSlot )
                    DenseBindings = FALSE;
                Shader.ResourceSlots[Original + i] = ResourceSlot++;
            }
        }
    }

    if( Options.Clean )
    {
        RemoveFile( MetaPath( Script, Shader ) );
        RemoveFile( BlobPath( Script, Shader, "vulkan" ) );
        RemoveFile( BlobPath( Script, Shader, "d3d12" ) );
        RemoveFile( BlobPath( Script, Shader, "metal" ) );
        RemoveFile( PdbPath( Script, Shader ) );
    }

    // TODO: GS: Implement this.
    if( Script.TargetMask & SHADER_TARGET_METAL )
    {
        x_printf( "Error: Metal target is not implemented yet\n" );
        return FALSE;
    }

    shader_resource_counts Resources;
    xarray<shader_reflection_binding>
                                  Bindings;
    xarray<shader_compile_output> Outputs;

    shader_compile_output Dxil;

    if( HaveDxil )
    {
        if( Discovered.HasPdb )
            RemoveFile( Discovered.TempPdb );
        Dxil = CachedDxil;
    }
    else if( Script.BindingModel == SHADER_BINDING_NATIVE || DenseBindings )
    {
        if( Script.BindingModel == SHADER_BINDING_SDL && Discovered.Resources.UniformBufferCount > 4 )
        {
            x_printf( "Error: SDL supports at most four uniform buffers per graphics stage\n" );

            if( Discovered.HasPdb )
                RemoveFile( Discovered.TempPdb );
            return FALSE;
        }

        Dxil = Discovered;

        if( Options.Verbose )
            x_printf( "  discovery DXIL reused\n" );
    }
    else
    {
        if( Discovered.HasPdb )
            RemoveFile( Discovered.TempPdb );
        if( !CompileDxcTarget( Script, Shader, Options, "d3d12", Dxil ) )
            return FALSE;
    }

    if( !ValidateDeclaredBuffers( Script, Dxil ) )
    {
        if( Dxil.HasPdb )
            RemoveFile( Dxil.TempPdb );
        return FALSE;
    }

    if( Script.TargetMask & SHADER_TARGET_D3D12 )
        Outputs.Append() = Dxil;
    else if( Dxil.HasPdb )
        RemoveFile( Dxil.TempPdb );
    Resources = Dxil.Resources;
    Bindings  = Dxil.Bindings;

    if( Script.TargetMask & SHADER_TARGET_VULKAN )
    {
        shader_compile_output& Output = Outputs.Append();

        if( HaveSpirv )
            Output = CachedSpirv;
        if( ( !HaveSpirv && !CompileDxcTarget( Script, Shader, Options, "vulkan", Output ) ) ||
            !ValidateTargetReflections( Shader, Dxil, Output ) ||
            !ValidateDeclaredBuffers( Script, Output ) )
        {
            RemoveTempOutputs( Outputs );
            return FALSE;
        }

        Resources = Output.Resources;
        Bindings  = Output.Bindings;

        if( Shader.ColorArithmetic )
        {
            shader_entry Variant        = Shader;
            Variant.CompileCapabilities = ECS_SHADER_CAPABILITY_FLOAT16;
            shader_compile_output Native;

            if( !CompileDxcTarget( Script, Variant, Options, "vulkan", Native ) ||
                !ValidateTargetReflections( Shader, Dxil, Native ) || !ValidateDeclaredBuffers( Script, Native ) )
            {
                RemoveTempOutputs( Outputs );
                return FALSE;
            }

            Outputs.Append() = Native;
        }
    }

    for( s32 i = 0; i < Outputs.GetCount(); ++i )
        RetainReflection( Script, Outputs[i] );

    if( !RemovePublishedPdbs( Outputs ) )
    {
        RemoveTempOutputs( Outputs );
        return FALSE;
    }

    if( !CommitPdbOutputs( Outputs ) )
    {
        RemoveTempOutputs( Outputs );
        return FALSE;
    }

    if( !WriteShaderCache( Script, Shader, Options, Outputs ) )
        return FALSE;

    return TRUE;
}

//==============================================================================

namespace
{
struct CompileJobs
{
    shader_script const*                       Script;
    compile_options const*                     Options;
    std::atomic<s32>                           Next{ 0 };
    std::atomic<u32>                           Completed{ 0 };
    std::atomic<bool>                          Failed{ false };
    std::vector<xarray<shader_compile_output>> Results;
    std::vector<s32>                           Canonical;
    std::vector<shader_compile_metrics>        Metrics;
};

//==============================================================================

void CompileWorker( s32, char** Arguments )
{
    CompileJobs&  Jobs = *reinterpret_cast<CompileJobs*>( Arguments[0] );
    shader_script Local;
    Local.Sources      = Jobs.Script->Sources;
    Local.SourceRoot   = Jobs.Script->SourceRoot;
    Local.OutRoot      = Jobs.Script->OutRoot;
    Local.CacheRoot    = Jobs.Script->CacheRoot;
    Local.Config       = Jobs.Script->Config;
    Local.BindingModel = Jobs.Script->BindingModel;
    Local.TargetMask   = Jobs.Script->TargetMask;
    Local.CatalogId    = Jobs.Script->CatalogId;
    Local.AbiId        = Jobs.Script->AbiId;
    Local.Buffers      = Jobs.Script->Buffers;

    while( !Jobs.Failed.load() )
    {
        const s32 Index = Jobs.Next.fetch_add( 1 );

        if( Index >= Jobs.Script->Shaders.GetCount() )
            break;
        if( Jobs.Canonical[Index] != Index )
            continue;
        Local.Metrics = &Jobs.Metrics[Index];
        Local.FinalOutputs.Clear();

        if( !CompileShader( Local, Jobs.Script->Shaders[Index], *Jobs.Options ) )
        {
            Jobs.Failed.store( true );
            break;
        }

        Jobs.Results[Index] = Local.FinalOutputs;
        Jobs.Completed.fetch_add( 1 );
    }
}

//==============================================================================

xbool CompileCatalog( shader_script& Script, compile_options const& Options )
{
    CompileJobs Jobs;
    Jobs.Script  = &Script;
    Jobs.Options = &Options;
    Jobs.Results.resize( Script.Shaders.GetCount() );
    Jobs.Canonical.resize( Script.Shaders.GetCount() );
    Jobs.Metrics.resize( Script.Shaders.GetCount() );
    std::map<std::string, s32> Inputs;

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
    {
        const shader_entry& Shader   = Script.Shaders[i];
        const xstring*      Fields[] = { &Shader.Source, &Shader.Stage, &Shader.Entry, &Shader.Profile,
                                         &Shader.CommonArgs, &Shader.VulkanArgs, &Shader.D3D12Args };
        std::string         Key;

        for( const xstring* Field : Fields )
        {
            Key.append( (const char*)*Field, Field->GetLength() );
            Key.push_back( 0 );
        }

        Key.push_back( Shader.ColorArithmetic ? 1 : 0 );

        if( Script.Config != "release" )
            Key.append( (const char*)Shader.Name, Shader.Name.GetLength() );
        Jobs.Canonical[i] = Inputs.emplace( Key, i ).first->second;
    }

    char*       Arguments[] = { reinterpret_cast<char*>( &Jobs ) };
    const u32   Count       = std::min( std::max( 1u, std::min( 12u, Options.WorkerCount ) ),
                                        std::max( 1u, std::thread::hardware_concurrency() ) );
    xthread*    Workers[12] = {};
    const xtick Start       = x_GetTime();
    xtick       LastReport  = Start;
    DisplayShaderProgress( "Compile", 0, Inputs.size(), 0 );

    for( u32 i = 0; i < Count; ++i )
        Workers[i] = new xthread( CompileWorker, "ShaderTool compile", 2 * 1024 * 1024, 0, 1, Arguments );
    for( ;; )
    {
        xbool Active = FALSE;

        for( u32 i = 0; i < Count; ++i )
            Active |= Workers[i]->IsActive();
        const xtick Now = x_GetTime();

        if( !Active || x_TicksToMs( Now - LastReport ) >= 1000 )
        {
            DisplayShaderProgress( "Compile", Jobs.Completed.load(), Inputs.size(), x_TicksToMs( Now - Start ) );
            LastReport = Now;
        }

        if( !Active )
            break;
        x_DelayThread( 10 );
    }

    for( u32 i = 0; i < Count; ++i )
        delete Workers[i];
    shader_compile_metrics Totals;

    for( const shader_compile_metrics& Metric : Jobs.Metrics )
    {
        Totals.DiscoveryCalls += Metric.DiscoveryCalls;
        Totals.DxilCalls += Metric.DxilCalls;
        Totals.SpirvCalls += Metric.SpirvCalls;
        Totals.DiscoveryMs += Metric.DiscoveryMs;
        Totals.DxilMs += Metric.DxilMs;
        Totals.SpirvMs += Metric.SpirvMs;
    }

    if( Options.Verbose )
        x_printf( "DXC calls: discovery=%u DXIL=%u SPIR-V=%u; accumulated worker ms: %.3f %.3f %.3f\n",
                  Totals.DiscoveryCalls, Totals.DxilCalls, Totals.SpirvCalls,
                  Totals.DiscoveryMs, Totals.DxilMs, Totals.SpirvMs );
    if( Jobs.Failed.load() )
        return FALSE;

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
    {
        if( Jobs.Canonical[i] == i )
            continue;
        compile_options CacheOptions = Options;
        CacheOptions.Clean           = FALSE;
        xarray<shader_compile_output> Outputs;

        if( !LoadShaderCache( Script, Script.Shaders[Jobs.Canonical[i]], CacheOptions, Outputs ) )
            return FALSE;

        for( s32 j = 0; j < Outputs.GetCount(); ++j )
            Outputs[j].ShaderName = Script.Shaders[i].Name;
        if( !WriteShaderCache( Script, Script.Shaders[i], Options, Outputs ) )
            return FALSE;

        shader_script Local;

        for( s32 j = 0; j < Outputs.GetCount(); ++j )
            RetainReflection( Local, Outputs[j] );
        Jobs.Results[i] = Local.FinalOutputs;
    }

    if( Options.Verbose )
        x_printf( "Compile tasks: %u canonical inputs, %d logical stages\n", (u32)Inputs.size(), Script.Shaders.GetCount() );
    for( const auto& Result : Jobs.Results )
        for( s32 i = 0; i < Result.GetCount(); ++i )
            Script.FinalOutputs.Append() = Result[i];
    DisplayShaderProgress( "Compile", Script.Shaders.GetCount(), Script.Shaders.GetCount(), x_TicksToMs( x_GetTime() - Start ), TRUE );
    return TRUE;
}
}

//==============================================================================

xbool WriteComputeShaderHeaders( const shader_script& Script, const compile_options& Options )
{
    for( s32 i = 0; i < Script.ComputeShaders.GetCount(); ++i )
    {
        const shader_entry& Shader = Script.ComputeShaders[i];

        if( Shader.Name.IsEmpty() )
        {
            return FALSE;
        }

        for( s32 j = 0; j < Shader.Name.GetLength(); ++j )
        {
            const char Character = Shader.Name[j];

            if( !( ( Character >= 'a' && Character <= 'z' ) ||
                   ( Character >= '0' && Character <= '9' ) || Character == '_' ) )
            {
                return FALSE;
            }
        }

        xstring               Header( "// Generated by ShaderTool. Do not edit.\n#pragma once\n#include \"e_Shader.hpp\"\n" );
        shader_compile_output Layout;
        xbool                 HaveLayout = FALSE;

        for( u32 Target = 0; Target < 2; ++Target )
        {
            if( !( Script.TargetMask & ( 1u << Target ) ) )
            {
                continue;
            }

            shader_compile_output Output;

            if( !CompileDxcTarget( Script, Shader, Options, Target == 0 ? "vulkan" : "d3d12", Output ) )
            {
                return FALSE;
            }

            if( HaveLayout && !ValidateTargetReflections( Shader, Layout, Output ) )
            {
                return FALSE;
            }

            if( !HaveLayout )
            {
                Layout.Target    = Output.Target;
                Layout.Resources = Output.Resources;
                Layout.Compute   = Output.Compute;
                Layout.Bindings  = Output.Bindings;
                Layout.Members   = Output.Members;
                HaveLayout       = TRUE;
            }

            if( Output.HasPdb && !ReplaceFile( Output.TempPdb, Output.Pdb ) )
            {
                return FALSE;
            }

            Header += xstring( xfs( "alignas( 4 ) inline constexpr u8 A51Compute_%s_%s[] =\n{\n",
                                    (const char*)Shader.Name, Target == 0 ? "SPIRV" : "DXIL" ) );
            for( s32 Byte = 0; Byte < Output.Code.GetCount(); ++Byte )
            {
                if( ( Byte & 15 ) == 0 )
                {
                    Header += "    ";
                }

                Header += xstring( xfs( "0x%02x,%s", static_cast<u8>( Output.Code[Byte] ),
                                        ( Byte & 15 ) == 15 ? "\n" : " " ) );
            }

            Header += "\n};\n";
            Header += xstring( xfs( "#define A51_COMPUTE_%s_%s 1\n", (const char*)Shader.Name,
                                    Target == 0 ? "SPIRV" : "DXIL" ) );
        }

        if( !HaveLayout )
        {
            return FALSE;
        }

        Header += xstring( xfs( "\ninline xbool A51Compute_%s_GetDesc( shader_desc& Desc, shader_format Format )\n{\n",
                                (const char*)Shader.Name ) );
        Header += "    Desc = shader_desc();\n    Desc.Stage = SHADER_STAGE_COMPUTE;\n    Desc.Format = Format;\n";
        Header += xstring( xfs( "    Desc.pEntryPoint = \"%s\";\n    Desc.pDebugName = \"%s\";\n",
                                (const char*)Shader.Entry, (const char*)Shader.Name ) );
        Header += xstring( xfs( "    Desc.Resources.SamplerCount = %u;\n"
                                "    Desc.Resources.StorageTextureCount = %u;\n"
                                "    Desc.Resources.StorageBufferCount = %u;\n"
                                "    Desc.Resources.UniformBufferCount = %u;\n"
                                "    Desc.Compute.ReadWriteTextureCount = %u;\n"
                                "    Desc.Compute.ReadWriteBufferCount = %u;\n"
                                "    Desc.Compute.ThreadCountX = %u;\n"
                                "    Desc.Compute.ThreadCountY = %u;\n"
                                "    Desc.Compute.ThreadCountZ = %u;\n",
                                Layout.Resources.SamplerCount, Layout.Resources.StorageTextureCount,
                                Layout.Resources.StorageBufferCount, Layout.Resources.UniformBufferCount,
                                Layout.Compute.ReadWriteTextureCount, Layout.Compute.ReadWriteBufferCount,
                                Layout.Compute.ThreadCountX, Layout.Compute.ThreadCountY, Layout.Compute.ThreadCountZ ) );
        Header += "    switch( Format )\n    {\n";

        for( u32 Target = 0; Target < 2; ++Target )
        {
            if( !( Script.TargetMask & ( 1u << Target ) ) )
            {
                continue;
            }

            Header += xstring( xfs( "        case SHADER_FORMAT_%s:\n        {\n"
                                    "            Desc.Blob = shader_blob( A51Compute_%s_%s, sizeof( A51Compute_%s_%s ) );\n"
                                    "            return TRUE;\n        }\n",
                                    Target == 0 ? "SPIRV" : "DXIL", (const char*)Shader.Name,
                                    Target == 0 ? "SPIRV" : "DXIL", (const char*)Shader.Name,
                                    Target == 0 ? "SPIRV" : "DXIL" ) );
        }

        Header += "        default:\n        {\n            return FALSE;\n        }\n    }\n}\n";

        const xstring FileName = JoinPath( JoinPath( JoinPath( Script.OutRoot, Script.Config ), "generated" ),
                                           xstring( "a51_compute_" ) + Shader.Name + ".hpp" );
        if( !WriteBinaryFile( FileName, (const char*)Header, Header.GetLength() ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

xbool ProcessScript( const char* pFileName, const compile_options& Options )
{
    const xtick   ProcessStart = x_GetTime();
    shader_script Script;

    if( !ParseScript( pFileName, Script ) )
        return FALSE;

    if( !Options.Config.IsEmpty() )
        Script.Config = Options.Config;
    BeginShaderConsole( pFileName, Script, Options );

    if( !ExpandSurfaceRecipes( Script ) )
        return FALSE;

    Script.CompilerFingerprint = Options.CompilerFingerprint;

    if( !GenerateShaderContract( Script, FALSE ) )
        return FALSE;

    if( Options.Verbose )
        x_printf( "Catalog preparation: %.3f ms\n", x_TicksToMs( x_GetTime() - ProcessStart ) );
    if( Options.GenerateOnly )
    {
        Script.OutRoot = JoinPath( Script.OutRoot, ".preview" );

        if( !GenerateShaderContract( Script, TRUE ) )
            return FALSE;

        SetShaderConsoleOutput( JoinPath( JoinPath( Script.OutRoot, Script.Config ), "generated" ) );
        return TRUE;
    }

    if( !Options.StageFilter.IsEmpty() )
    {
        const xstring PublishedRoot = Script.OutRoot;
        Script.CacheRoot            = JoinPath( PublishedRoot, ".cache" );
        Script.OutRoot              = JoinPath( PublishedRoot, ".stage_checks" );

        if( !GenerateShaderContract( Script, TRUE ) )
            return FALSE;

        const xbool   Prefix = Options.StageFilter[Options.StageFilter.GetLength() - 1] == '*';
        const xstring Filter = Prefix ? Options.StageFilter.Left( Options.StageFilter.GetLength() - 1 ) : Options.StageFilter;

        for( s32 i = Script.Shaders.GetCount() - 1; i >= 0; --i )
            if( ( Prefix && Script.Shaders[i].Name.GetLength() < Filter.GetLength() ) ||
                ( Prefix ? Script.Shaders[i].Name.Left( Filter.GetLength() ) : Script.Shaders[i].Name ) != Filter )
                Script.Shaders.Delete( i );
        for( s32 i = Script.ComputeShaders.GetCount() - 1; i >= 0; --i )
        {
            const xstring& Name = Script.ComputeShaders[i].Name;

            if( ( Prefix && Name.GetLength() < Filter.GetLength() ) ||
                ( Prefix ? Name.Left( Filter.GetLength() ) : Name ) != Filter )
            {
                Script.ComputeShaders.Delete( i );
            }
        }

        if( !Script.Shaders.GetCount() && !Script.ComputeShaders.GetCount() )
        {
            x_printf( "Error: stage filter matched no shaders\n" );
            return FALSE;
        }

        // Partial compilation can populate the content cache, never a runtime kit.
        return ( !Script.Shaders.GetCount() || CompileCatalog( Script, Options ) ) &&
               WriteComputeShaderHeaders( Script, Options ) && ShaderSourcesUnchanged( Script );
    }

    if( !Options.Clean && ShaderKitIsCurrent( Script ) && ShaderSourcesUnchanged( Script ) )
    {
        x_printf( "Up to date: compilation and ECS packaging skipped\n" );
        SetShaderConsoleOutput( JoinPath( Script.OutRoot, Script.Config ) );
        return TRUE;
    }

    // A failed stage or VS/PS validation must leave the published kit intact.
    const xstring   PublishedRoot = Script.OutRoot;
    const xstring   StagingRoot   = JoinPath( PublishedRoot, xstring( ".staging_" ) + Script.Config );
    std::error_code Error;
    std::filesystem::remove_all( (const char*)StagingRoot, Error );

    if( Error )
    {
        x_printf( "Error: cannot clear staging directory: %s\n", Error.message().c_str() );
        return FALSE;
    }

    Script.CacheRoot = JoinPath( PublishedRoot, ".cache" );
    Script.OutRoot   = StagingRoot;

    if( !GenerateShaderContract( Script, TRUE ) )
    {
        x_printf( "Error: cannot generate staged shader contract\n" );
        return FALSE;
    }

    const xtick CompileStart = x_GetTime();

    if( !CompileCatalog( Script, Options ) )
        return FALSE;

    if( Options.Verbose )
        x_printf( "Compilation and cache: %.3f ms\n", x_TicksToMs( x_GetTime() - CompileStart ) );
    const xtick CatalogStart = x_GetTime();

    if( !WriteShaderCatalog( Script ) )
        return FALSE;

    Script.FinalOutputs.Clear();

    if( Options.Verbose )
        x_printf( "Catalog serialization: %.3f ms\n", x_TicksToMs( x_GetTime() - CatalogStart ) );
    const xtick PackStart = x_GetTime();

    if( !WriteShaderPackages( Script, Options ) || !WriteComputeShaderHeaders( Script, Options ) ||
        !WriteShaderKitManifest( Script ) || !WriteShaderKitState( Script ) || !ShaderSourcesUnchanged( Script ) )
        return FALSE;

    const xtick   PackEnd   = x_GetTime();
    const xstring Published = JoinPath( PublishedRoot, Script.Config );
    const xstring Staged    = JoinPath( StagingRoot, Script.Config );
    const xstring Backup    = Published + ".previous";
    std::filesystem::remove_all( (const char*)Backup, Error );

    if( Error )
        return FALSE;

    const bool HadPublished = std::filesystem::exists( (const char*)Published );

    if( HadPublished )
        std::filesystem::rename( (const char*)Published, (const char*)Backup, Error );
    if( Error )
        return FALSE;

    std::filesystem::rename( (const char*)Staged, (const char*)Published, Error );

    if( Error )
    {
        std::error_code Rollback;

        if( HadPublished )
            std::filesystem::rename( (const char*)Backup, (const char*)Published, Rollback );
        x_printf( "Error: failed to publish shader kit: %s\n", Error.message().c_str() );
        return FALSE;
    }

    std::filesystem::remove_all( (const char*)Backup, Error );
    std::filesystem::remove_all( (const char*)StagingRoot, Error );
    SetShaderConsoleOutput( Published );

    if( Options.Verbose )
        x_printf( "Final ECS kit: packing/writing/verification %.3f s; publication %.3f s\n",
                  x_TicksToMs( PackEnd - PackStart ) / 1000.0, x_TicksToMs( x_GetTime() - PackEnd ) / 1000.0 );
    if( Options.Verbose )
        x_printf( "Total shader build: %.3f ms\n", x_TicksToMs( x_GetTime() - ProcessStart ) );
    if( Options.Verbose )
        x_printf( "Published catalog %016llx: %d recipes, %d stages\n", (unsigned long long)Script.CatalogId, Script.Programs.GetCount(), Script.Shaders.GetCount() );
    return TRUE;
}

//==============================================================================
