//==============================================================================
//
//  ShaderConsole.cpp
//
//  Workers queue diagnostics. Only the submission thread redraws the terminal.
//
//==============================================================================

#include "ShaderTool.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined( TARGET_WINDOWS )
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace
{
using console_clock = std::chrono::steady_clock;

struct console_warning
{
    std::string Location;
    std::string Message;
    u32         Count = 1;
};

struct console_package
{
    std::string               Name;
    std::string               Phase     = "preparing";
    u32                       Completed = 0;
    u32                       Total     = 0;
    u32                       Zstd      = 0;
    u32                       Cached    = 0;
    u32                       Raw       = 0;
    f64                       Seconds   = 0;
    xbool                     Finished  = FALSE;
    console_clock::time_point Start     = console_clock::now();
};

std::mutex                   s_Lock;
std::vector<std::string>     s_Messages;
std::vector<std::string>     s_Diagnostics;
std::vector<console_warning> s_Warnings;
std::map<std::string, u32>   s_WarningIndex;
std::vector<std::string>     s_Logs;
std::vector<console_package> s_Packages;
std::string                  s_File;
std::string                  s_Config;
std::string                  s_Targets;
std::string                  s_Output;
u32                          s_Workers          = 0;
u32                          s_CompileCompleted = 0;
u32                          s_CompileTotal     = 0;
f64                          s_CompileSeconds   = 0;
u32                          s_Rows             = 0;
xbool                        s_Interactive      = FALSE;
xbool                        s_Unicode          = FALSE;
xbool                        s_Verbose          = FALSE;
xbool                        s_Active           = FALSE;
xbool                        s_CompileStarted   = FALSE;
xbool                        s_CompileFinished  = FALSE;

void QueueMessage( const char* pText )
{
    std::lock_guard<std::mutex> Lock( s_Lock );

    if( s_Active )
        s_Messages.emplace_back( pText );
    else
    {
        std::fputs( pText, stdout );
        std::fflush( stdout );
    }
}

//==============================================================================

std::string Duration( f64 Seconds )
{
    char Text[48];

    if( Seconds < 1 )
        std::snprintf( Text, sizeof( Text ), "%.0f ms", Seconds * 1000 );
    else
        std::snprintf( Text, sizeof( Text ), "%.2f s", Seconds );
    return Text;
}

//==============================================================================

std::string Bar( u32 Percent )
{
    std::string Text = std::to_string( Percent ) + "%|";

    for( u32 i = 0; i < 24; ++i )
        Text += i < Percent * 24 / 100 ? ( s_Unicode ? "\xE2\x96\x88" : "#" ) : " ";
    return Text + "|";
}

//==============================================================================

std::string PackageLine( const console_package& Package, size_t Width )
{
    const u32          Percent = Package.Finished ? 100 : Package.Total ? std::min( 99u, static_cast<u32>( static_cast<u64>( Package.Completed ) * 100 / Package.Total ) )
                                                                        : 0;
    std::ostringstream Text;
    Text << "  " << Package.Name << ":" << std::string( Width - Package.Name.size() + 1, ' ' )
         << Bar( Percent ) << " " << Package.Completed << "/" << Package.Total << " blocks ["
         << Duration( Package.Seconds ) << ", zstd=" << Package.Zstd << ", cache=" << Package.Cached;
    if( Package.Raw )
        Text << ", raw=" << Package.Raw;
    if( !Package.Finished )
        Text << ", " << Package.Phase;
    return Text.str() + "]";
}

//==============================================================================

void AppendLines( std::vector<std::string>& Lines, const std::string& Text )
{
    std::istringstream Input( Text );
    std::string        Line;

    while( std::getline( Input, Line ) )
        Lines.push_back( Line );
}

void TerminalSize( u32& Columns, u32& Rows );

void WrapLine( std::vector<std::string>& Lines, const std::string& Line, u32 Columns )
{
    const size_t Start = Line.find_first_not_of( ' ' );

    if( Start == std::string::npos )
    {
        Lines.push_back( Line );
        return;
    }

    Columns = std::max( 1u, Columns );
    const std::string Indent( std::min( Start, static_cast<size_t>( Columns - 1 ) ), ' ' );
    size_t            Position = Start;

    while( Position < Line.size() )
    {
        const size_t First = Position;
        size_t       Space = std::string::npos;
        u32          Cells = static_cast<u32>( Indent.size() );

        while( Position < Line.size() && Cells < Columns )
        {
            if( Line[Position] == ' ' )
                Space = Position;
            const unsigned char C    = Line[Position];
            const size_t        Step = C < 0x80 ? 1 : C < 0xe0 ? 2
                                                  : C < 0xf0   ? 3
                                                               : 4;
            Position += std::min( Step, Line.size() - Position );
            ++Cells;
        }

        if( Position < Line.size() && Line[Position] != ' ' && Space != std::string::npos )
            Position = Space;
        Lines.push_back( Indent + Line.substr( First, Position - First ) );

        while( Position < Line.size() && Line[Position] == ' ' )
            ++Position;
    }
}

//==============================================================================

std::vector<std::string> WarningLines( const console_warning& Warning )
{
    u32 Columns, Rows;
    TerminalSize( Columns, Rows );
    std::vector<std::string> Lines;
    WrapLine( Lines, "  " + Warning.Location, std::max( 1u, Columns - 1 ) );
    WrapLine( Lines, "    " + Warning.Message + ( Warning.Count > 1 ? " [repeated " + std::to_string( Warning.Count ) + " times]" : "" ), std::min( 100u, std::max( 1u, Columns - 1 ) ) );
    return Lines;
}

//==============================================================================

void PrintWarning( const console_warning& Warning )
{
    for( const std::string& Line : WarningLines( Warning ) )
        std::printf( "%s\n", Line.c_str() );
}

//==============================================================================

void DrainMessages( void )
{
    // DXC warning context is preserved verbatim in verbose mode.
    static const std::regex Warning( R"(^(.+:[0-9]+:[0-9]+): warning: (.+)$)" );

    for( const std::string& Text : s_Diagnostics )
    {
        std::istringstream Input( Text );
        std::string        Line;
        xbool              Found = FALSE;

        while( std::getline( Input, Line ) )
        {
            std::smatch Match;

            if( !std::regex_match( Line, Match, Warning ) )
                continue;
            Found                      = TRUE;
            const std::string Location = std::filesystem::path( Match[1].str() ).lexically_normal().string();
            const std::string Message  = Match[2].str();
            const std::string Key      = Location + '\n' + Message;
            const auto        Existing = s_WarningIndex.find( Key );

            if( Existing == s_WarningIndex.end() )
            {
                s_WarningIndex.emplace( Key, static_cast<u32>( s_Warnings.size() ) );
                s_Warnings.push_back( { Location, Message, 1 } );

                if( !s_Interactive )
                {
                    std::fputs( "\nWarnings\n\n", stdout );
                    PrintWarning( s_Warnings.back() );
                }
            }
            else
                ++s_Warnings[Existing->second].Count;
        }

        if( s_Verbose || !Found || Text.find( ": error:" ) != std::string::npos )
            s_Logs.push_back( Text + ( Text.empty() || Text.back() == '\n' ? "" : "\n" ) );
    }

    s_Diagnostics.clear();

    for( const std::string& Message : s_Messages )
        s_Logs.push_back( Message );
    s_Messages.clear();

    if( !s_Interactive )
    {
        for( const std::string& Log : s_Logs )
        {
            std::fputs( Log.c_str(), stdout );
        }

        s_Logs.clear();
    }
}

//==============================================================================

std::vector<std::string> Frame( void )
{
    std::vector<std::string> Lines = { "ShaderTool", "", "  Script:   " + s_File,
                                       "  Config:   " + s_Config, "  Targets:  " + s_Targets, "  Workers:  " + std::to_string( s_Workers ), "" };
    if( !s_Warnings.empty() )
    {
        Lines.push_back( "Warnings" );
        Lines.emplace_back();

        for( const console_warning& Warning : s_Warnings )
        {
            const auto Wrapped = WarningLines( Warning );
            Lines.insert( Lines.end(), Wrapped.begin(), Wrapped.end() );
            Lines.emplace_back();
        }
    }

    for( const std::string& Log : s_Logs )
        AppendLines( Lines, Log );
    if( s_CompileStarted )
    {
        Lines.push_back( "Compilation" );
        Lines.emplace_back();
        const u32 Percent = s_CompileFinished ? 100 : s_CompileTotal ? std::min( 99u, static_cast<u32>( static_cast<u64>( s_CompileCompleted ) * 100 / s_CompileTotal ) )
                                                                     : 0;
        char      Rate[32];
        std::snprintf( Rate, sizeof( Rate ), "%.0f", s_CompileSeconds > 0 ? s_CompileCompleted / s_CompileSeconds : 0 );
        Lines.push_back( "  " + Bar( Percent ) + " " + std::to_string( s_CompileCompleted ) + "/" +
                         std::to_string( s_CompileTotal ) + " stages [" + Duration( s_CompileSeconds ) + ", " + Rate + " stages/s]" );
        Lines.emplace_back();
    }

    if( !s_Packages.empty() )
    {
        Lines.push_back( "ECS packaging" );
        Lines.emplace_back();
        size_t Width = 0;

        for( const console_package& Package : s_Packages )
            Width = std::max( Width, Package.Name.size() );
        for( const console_package& Package : s_Packages )
            Lines.push_back( PackageLine( Package, Width ) );
        Lines.emplace_back();
    }

    if( !Lines.empty() && !Lines.back().empty() )
        Lines.emplace_back();
    return Lines;
}

//==============================================================================

void TerminalSize( u32& Columns, u32& Rows )
{
    Columns = 100;
    Rows    = 24;
#if defined( TARGET_WINDOWS )
    CONSOLE_SCREEN_BUFFER_INFO Info;

    if( GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &Info ) )
    {
        Columns = Info.srWindow.Right - Info.srWindow.Left + 1;
        Rows    = Info.srWindow.Bottom - Info.srWindow.Top + 1;
    }
#else
    winsize Size = {};

    if( ioctl( fileno( stdout ), TIOCGWINSZ, &Size ) == 0 )
    {
        if( Size.ws_col )
            Columns = Size.ws_col;
        if( Size.ws_row )
            Rows = Size.ws_row;
    }
#endif
}

//==============================================================================

void EraseFrame( void )
{
    if( !s_Rows )
        return;

    u32 Columns, Rows;
    TerminalSize( Columns, Rows );
    const u32 Count = std::min( s_Rows, Rows > 1 ? Rows - 1 : 1u );
    std::printf( "\033[%uA\r\033[J", Count );
    s_Rows = 0;
}

//==============================================================================

void Render( xbool Final = FALSE )
{
    DrainMessages();

    if( !s_Interactive )
        return;

    u32 Columns, Rows;
    TerminalSize( Columns, Rows );
    std::vector<std::string> Lines;

    for( const std::string& Line : Frame() )
        WrapLine( Lines, Line, std::max( 1u, Columns - 1 ) );
    EraseFrame();
    // Keep the live frame in the viewport so cursor-up never targets scrollback.
    const size_t First = !Final && Lines.size() >= Rows ? Lines.size() - std::max( 1u, Rows - 1 ) : 0;

    for( size_t i = First; i < Lines.size(); ++i )
        std::printf( "%s\n", Lines[i].c_str() );
    s_Rows = Final ? 0 : static_cast<u32>( Lines.size() - First );
    std::fflush( stdout );
}
} // namespace

void InitializeShaderConsole( void )
{
#if defined( TARGET_WINDOWS )
    DWORD        Mode;
    const HANDLE Console = GetStdHandle( STD_OUTPUT_HANDLE );
    s_Interactive        = _isatty( _fileno( stdout ) ) && GetConsoleMode( Console, &Mode ) &&
                    SetConsoleMode( Console, Mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
    s_Unicode = GetConsoleOutputCP() == CP_UTF8;
#else
    const char* pTerm   = std::getenv( "TERM" );
    s_Interactive       = isatty( fileno( stdout ) ) && pTerm && std::string( pTerm ) != "dumb";
    const char* pLocale = std::getenv( "LC_ALL" );

    if( !pLocale || !*pLocale )
        pLocale = std::getenv( "LC_CTYPE" );
    if( !pLocale || !*pLocale )
        pLocale = std::getenv( "LANG" );
    s_Unicode = pLocale && ( std::string( pLocale ).find( "UTF-8" ) != std::string::npos ||
                             std::string( pLocale ).find( "utf8" ) != std::string::npos );
#endif
    x_SetPrintHook( QueueMessage );
}

//==============================================================================

void SetShaderConsoleVerbose( xbool Verbose )
{
    s_Verbose = Verbose;

    if( Verbose )
        s_Interactive = FALSE;
}

//==============================================================================

xbool ShaderConsoleVerbose( void )
{
    return s_Verbose;
}

//==============================================================================

void BeginShaderConsole( const char* pFile, const shader_script& Script, const compile_options& Options )
{
    std::lock_guard<std::mutex> Lock( s_Lock );

    if( s_Active )
        Render( TRUE );
    s_Warnings.clear();
    s_WarningIndex.clear();
    s_Logs.clear();
    s_Packages.clear();
    s_CompileStarted = s_CompileFinished = FALSE;
    s_CompileCompleted = s_CompileTotal = 0;
    s_CompileSeconds                    = 0;
    s_File                              = pFile;
    s_Config                            = Script.Config;
    s_Targets.clear();

    if( Script.TargetMask & SHADER_TARGET_D3D12 )
        s_Targets = "DXIL";
    if( Script.TargetMask & SHADER_TARGET_VULKAN )
        s_Targets += s_Targets.empty() ? "SPIR-V" : ", SPIR-V";
    if( Script.TargetMask & SHADER_TARGET_METAL )
        s_Targets += s_Targets.empty() ? "Metal" : ", Metal";
    s_Workers = std::min( std::max( 1u, std::min( 12u, Options.WorkerCount ) ),
                          std::max( 1u, std::thread::hardware_concurrency() ) );
    s_Output.clear();
    s_Active = TRUE;

    if( !s_Interactive )
        std::printf( "ShaderTool\n\n  Script:   %s\n  Config:   %s\n  Targets:  %s\n  Workers:  %u\n\n",
                     s_File.c_str(), s_Config.c_str(), s_Targets.c_str(), s_Workers );
    if( ( Script.TargetMask & SHADER_TARGET_ALL ) == SHADER_TARGET_ALL )
    {
        console_warning Warning;
        Warning.Location = "targets all";
        Warning.Message  = "Both SPIR-V (Vulkan) and DXIL (DirectX 12) are selected. Compiling both formats increases build time. "
                           "Dumbass, DO NOT compile DXIL for mobile platforms. Use 'targets vulkan' for mobile builds.";
        s_Warnings.push_back( Warning );

        if( !s_Interactive )
        {
            std::fputs( "Warnings\n\n", stdout );
            PrintWarning( Warning );
            std::fputc( '\n', stdout );
        }
    }

    Render();
}

//==============================================================================

void ReportShaderDiagnostics( const char* pText )
{
    std::lock_guard<std::mutex> Lock( s_Lock );

    if( s_Active )
        s_Diagnostics.emplace_back( pText );
    else
        std::fputs( pText, stdout );
}

//==============================================================================

void DisplayShaderProgress( const char*, u32 Completed, u32 Total, f64 ElapsedMs, xbool Finished )
{
    std::lock_guard<std::mutex> Lock( s_Lock );

    if( !s_Active )
        return;

    s_CompileStarted   = TRUE;
    s_CompileCompleted = Completed;
    s_CompileTotal     = Total;
    s_CompileSeconds   = ElapsedMs / 1000;
    s_CompileFinished  = Finished;

    if( Finished && !s_Interactive )
    {
        char Rate[32];
        std::snprintf( Rate, sizeof( Rate ), "%.0f", s_CompileSeconds > 0 ? Completed / s_CompileSeconds : 0 );
        std::printf( "Compilation\n\n  %s %u/%u stages [%s, %s stages/s]\n\n", Bar( Total ? static_cast<u64>( Completed ) * 100 / Total : 100 ).c_str(), Completed, Total, Duration( s_CompileSeconds ).c_str(), Rate );
    }

    Render();
}

//==============================================================================

void BeginShaderPackageProgress( const char* pName )
{
    std::lock_guard<std::mutex> Lock( s_Lock );

    if( !s_Active )
        return;

    if( s_Packages.empty() && !s_Interactive )
        std::fputs( "ECS packaging\n\n", stdout );
    console_package Package;
    Package.Name = pName;
    s_Packages.push_back( Package );
    Render();
}

//==============================================================================

void UpdateShaderPackageProgress( const char* pName, u32 Completed, u32 Total, u32 Zstd, u32 Cached, u32 Raw,
                                  const char* pPhase, xbool Finished, f64 ElapsedMs )
{
    std::lock_guard<std::mutex> Lock( s_Lock );

    if( !s_Active )
        return;

    const auto Found = std::find_if( s_Packages.begin(), s_Packages.end(),
                                     [&]( const console_package& Package )
                                     { return Package.Name == pName; } );
    if( Found == s_Packages.end() )
        return;

    console_package& Package = *Found;
    Package.Completed        = Completed;
    Package.Total            = Total;
    Package.Zstd             = Zstd;
    Package.Cached           = Cached;
    Package.Raw              = Raw;
    Package.Phase            = pPhase;
    Package.Finished         = Finished;
    Package.Seconds          = Finished ? ElapsedMs / 1000 : std::chrono::duration<f64>( console_clock::now() - Package.Start ).count();

    if( Finished && !s_Interactive )
    {
        size_t Width = 0;

        for( const console_package& Entry : s_Packages )
            Width = std::max( Width, Entry.Name.size() );
        std::printf( "%s\n", PackageLine( Package, Width ).c_str() );
    }

    Render();
}

//==============================================================================

void SetShaderConsoleOutput( const xstring& Output )
{
    std::lock_guard<std::mutex> Lock( s_Lock );
    s_Output = std::filesystem::path( (const char*)Output ).lexically_normal().string();
    Render();
}

//==============================================================================

void FinishShaderConsole( xbool Success, f64 Seconds )
{
    std::lock_guard<std::mutex> Lock( s_Lock );

    if( s_Active )
        Render( TRUE );
    std::printf( "%sBuild %s\n\n  Total:   %.2f s\n", s_Active && s_Interactive ? "" : "\n",
                 Success ? "completed" : "failed", Seconds );
    if( !s_Output.empty() )
        std::printf( "  Output:  %s\n", s_Output.c_str() );
    std::fflush( stdout );
    s_Active = FALSE;
}
