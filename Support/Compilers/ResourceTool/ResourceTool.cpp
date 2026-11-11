//=========================================================================
//
//  ResourceTool.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "ResourceHandler.hpp"
#include "ResourceRegistry.hpp"
#include "Diagnostics.hpp"
#include "ArtifactManifest.hpp"
#include "DependencyGraph.hpp"
#include "ModuleRegistration.hpp"

#include <filesystem>

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool IsCommand( const char* pCommand, const char* pExpected )
{
    return pCommand && x_stricmp( pCommand, pExpected ) == 0;
}

//=========================================================================

resource_tool::operation ParseOperation( const char* pCommand )
{
    if( IsCommand( pCommand, "compile" ) )
        return resource_tool::operation::Compile;
    if( IsCommand( pCommand, "decompile" ) )
        return resource_tool::operation::Decompile;

    x_throw( xfs( "Unknown ResourceTool command [%s]", pCommand ? pCommand : "" ) );
    return resource_tool::operation::Compile;
}

//=========================================================================

const char* SeverityName( resource_tool::diagnostic_severity Severity )
{
    switch( Severity )
    {
    case resource_tool::diagnostic_severity::Info:
        return "info";
    case resource_tool::diagnostic_severity::Warning:
        return "warning";
    case resource_tool::diagnostic_severity::Error:
        return "error";
    case resource_tool::diagnostic_severity::Fatal:
        return "fatal";
    }
    return "unknown";
}

//=========================================================================

void PrintDiagnostics( const resource_tool::diagnostic_log& Diagnostics )
{
    const xarray<resource_tool::diagnostic>& Entries = Diagnostics.Entries();
    for( s32 iEntry = 0; iEntry < Entries.GetCount(); ++iEntry )
    {
        const resource_tool::diagnostic& Entry = Entries[iEntry];
        x_printf( "ResourceTool: %s\n", SeverityName( Entry.Severity ) );
        if( !Entry.Stage.IsEmpty() )
            x_printf( "  stage: %s\n", (const char*)Entry.Stage );
        if( !Entry.Asset.IsEmpty() )
            x_printf( "  asset: %s\n", (const char*)Entry.Asset );
        if( !Entry.Source.IsEmpty() )
            x_printf( "  source: %s\n", (const char*)Entry.Source );
        x_printf( "  message: %s\n", (const char*)Entry.Message );
    }
}

//=========================================================================

void PrintExecutionSummary( const resource_tool::resource_context& Context )
{
    if( !Context.GetRequest().Verbose )
        return;

    x_printf( "\n-- ResourceTool artifacts ------------------\n" );
    const xarray<resource_tool::artifact>& Artifacts = Context.Artifacts().Entries();
    for( s32 iArtifact = 0; iArtifact < Artifacts.GetCount(); ++iArtifact )
    {
        const resource_tool::artifact& Artifact = Artifacts[iArtifact];
        x_printf( "%-12s %s\n", (const char*)Artifact.Role, Artifact.Path.generic_string().c_str() );
    }

    x_printf( "\n-- ResourceTool dependencies ----------------\n" );
    const xarray<resource_tool::dependency>& Dependencies = Context.Dependencies().Entries();
    for( s32 iDependency = 0; iDependency < Dependencies.GetCount(); ++iDependency )
    {
        const resource_tool::dependency& Dependency = Dependencies[iDependency];
        const char* pKind = "source";
        if( Dependency.Kind == resource_tool::dependency_kind::Pipeline )
            pKind = "pipeline";
        else if( Dependency.Kind == resource_tool::dependency_kind::Product )
            pKind = "product";
        x_printf( "%-12s %s\n", pKind, Dependency.Path.generic_string().c_str() );
    }
}

//=========================================================================

void PrintHelp( void )
{
    x_printf( "ResourceTool - asset compiler/decompiler\n\n" );
    x_printf( "Usage:\n" );
    x_printf( "  ResourceTool compile   <asset> [--verbose]\n" );
    x_printf( "  ResourceTool decompile <asset> [--animation <asset>] [--verbose]\n\n" );
    x_printf( "The descriptor owns source, output and resource-specific settings.\n" );
    x_printf( "Bitmap compile input is .rawbitmap with a sibling editable image; "
              "products are .XBMP.\n" );
    x_printf( "Geometry compile input is .rawgeom with a sibling glTF 2.0 source; "
              "decompile input is .RIGIDGEOM or .SKINGEOM. Skin geometry requires "
              "a compiled .ANIM skeleton source.\n" );
    x_printf( "Animation compile input is .rawanim with a sibling glTF source; "
              "decompile input is .ANIM.\n" );

    x_printf( "Product geometry decompilation writes glTF, rawgeom and collision artifacts.\n" );
    x_printf( "Product animation decompilation writes glTF, rawanim and binary artifacts.\n" );
}

//=========================================================================

s32 RunRequest( const resource_tool::resource_request& Request )
{
    resource_tool::resource_registry Registry;
    resource_tool::RegisterBuiltInModules( Registry );

    resource_tool::resource_handler* pHandler = Registry.Resolve( Request );
    if( !pHandler )
    {
        x_throw( xfs( "No ResourceTool module handles input [%s]", Request.InputPath.generic_string().c_str() ) );
    }

    resource_tool::resource_context Context( Request );
    const xbool bDiscovered = pHandler->DiscoverDependencies( Context );
    const xbool bSuccess = bDiscovered && pHandler->Execute( Context );
    PrintDiagnostics( Context.Diagnostics() );
    PrintExecutionSummary( Context );
    return bSuccess ? 0 : 1;
}

} // namespace

//=========================================================================
//  ENTRY POINT
//=========================================================================

s32 main( s32 argc, char* argv[] )
{
    x_Init( argc, argv );
    s32 Result = 1;

    x_try;
    if( argc <= 1 )
    {
        PrintHelp();
    }
    else if( IsCommand( argv[1], "--help" ) || IsCommand( argv[1], "-h" ) )
    {
        PrintHelp();
        Result = 0;
    }
    else if( argc < 3 )
    {
        PrintHelp();
    }
    else
    {
        resource_tool::resource_request Request;
        Request.Operation = ParseOperation( argv[1] );
        Request.InputPath = std::filesystem::path( argv[2] ).lexically_normal();

        for( s32 i = 3; i < argc; ++i )
        {
            if( IsCommand( argv[i], "--verbose" ) || IsCommand( argv[i], "-v" ) )
            {
                Request.Verbose = TRUE;
            }
            else if( IsCommand( argv[i], "--animation" ) )
            {
                if( i + 1 >= argc )
                    x_throw( "--animation requires an asset path" );
                Request.AnimationPath = std::filesystem::path( argv[++i] ).lexically_normal();
            }
            else
            {
                x_throw( xfs( "Unknown option [%s]", argv[i] ) );
            }
        }

        Result = RunRequest( Request );
    }
    x_catch_begin;
#ifdef X_EXCEPTIONS
    x_printf( "ResourceTool: fatal\n  message: %s\n", xExceptionGetErrorString() );
#endif
    Result = 1;
    x_catch_end;

    x_Kill();
    return Result;
}
