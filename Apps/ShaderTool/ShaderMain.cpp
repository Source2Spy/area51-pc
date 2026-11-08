//==============================================================================
//  ShaderTool command-line entry point.
//==============================================================================
#include "ShaderTool.hpp"
#include <cstdio>
#include <chrono>

static
int RunShaderTool( int argc, char** argv )
{
    compile_options Options;

    if( !ParseCommandLine( argc, argv, Options ) )
    {
        DisplayHelp();
        return 10;
    }

    SetShaderConsoleVerbose( Options.Verbose );
    Options.CompilerFingerprint = ShaderCompilerFingerprint();

    if( !Options.CompilerFingerprint )
    {
        x_printf( "Error: cannot fingerprint the DXC compiler\n" );
        return 1;
    }

    xbool Success = TRUE;

    for( s32 i = 0; i < Options.Scripts.GetCount(); i++ )
        Success = ProcessScript( (const char*)Options.Scripts[i], Options ) && Success;

    return Success ? 0 : 1;
}

//==============================================================================

int main( int argc, char** argv )
{
    const auto Start = std::chrono::steady_clock::now();
    setvbuf( stdout, NULL, _IOLBF, 0 );
    x_Init( argc, argv );
    InitializeShaderConsole();
    const int Result = RunShaderTool( argc, argv );
    x_Kill();
    const f64 Seconds = std::chrono::duration<f64>( std::chrono::steady_clock::now() - Start ).count();
    FinishShaderConsole( Result == 0, Seconds );
    return Result;
}
