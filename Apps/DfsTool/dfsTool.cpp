//==============================================================================
//
//  dfsTool.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Auxiliary/CommandLine/CommandLine.hpp"
#include "Auxiliary/Bitmap/aux_Bitmap.hpp"
#include "x_bitmap.hpp"

#include "dfs.hpp"

#if defined(TARGET_WINDOWS)
    #include <windows.h>
    #include <direct.h>
#else
    #include <unistd.h>
#endif

//==============================================================================
//  DATA
//==============================================================================

u32             s_SplitSize     = 240 * 1024*1024;  // Default split size of 240MB
u32             s_SectorSize    = 2048;             // Default sector size of 2048 bytes
u32             s_ChunkSize     = 32 * 1024;        // Default chunk size
xstring         s_dfsPathName;                      // PathName of .dfs file
xstring         s_ExtractPath;                      // Path for extract option
xarray<xstring> s_ScriptFiles;                      // Array of PathNames to script files to process
xbool           s_DoMake        = FALSE;            // Perform make type date checking
xbool           s_bEnableCRC    = FALSE;
xstring         s_RootPath;                         // Base path to strip from inputs

//==============================================================================
//  FUNCTIONS
//==============================================================================

void DisplayHelp( void )
{
    x_printf( "\n"
              "dfsTool (c)2002 Inevitable Entertainment Inc.\n"
              "\n"
              "  usage:\n"
              "         dfsTool [-opt [param]] [<script file>]\n"
              "\n"
              "options:\n"
              "         -b      <dfs file>  - Build dfs file\n"
              "         -m                  - Perform make, only build dfs if source files changed\n"
              "         -u      <dfs file>  - Update dfs file\n"
              "         -o      <dfs_file>  - Optimize dfs file\n"
              "         -l      <dfs file>  - List contents of dfs file\n"
              "         -x      <dfs_file>  - Extract contents of dfs file\n"
              "         -p      <path>      - Override path for Extract to <path>\n"
              "         -v      <dfs_file>  - Verify Checksums for dfs file\n"
              "         -split  Split       - Split size for files (default = 249036800)\n"
              "         -sector Sector      - Set sector size (default = 2048)\n"
              "         -chunk  ChunkSize   - Set chunk size (default = 32k bytes)\n"
              "         -crc    CRC Enable  - Enable CRC's in the dfs file!\n"
              "         -root   BasePath    - Strip this base path from entries when building dfs\n"
              "\n"
              "<script files> are text file with filenames on each line, they are only\n"
              "valid with the -b and -u options.\n" );
}

//==============================================================================

void DoBuild( void )
{
    dfs_Build( s_dfsPathName, s_ScriptFiles, s_DoMake, s_SectorSize, s_SplitSize, s_ChunkSize, s_bEnableCRC );
}

//==============================================================================

void DoUpdate( void )
{
// TODO:    dfs_Update( s_dfsPathName, s_ScriptFiles );
}

//==============================================================================

void DoOptimize( void )
{
// TODO:    dfs_Optimize( s_dfsPathName );
}

//==============================================================================

void DoList( void )
{
// TODO:    dfs_List( s_dfsPathName );
    X_FILE* fp = x_fopen(s_dfsPathName,"rb");
    if( !fp )
        return;

    s32 FSize = x_flength(fp);
    if( FSize < (s32)sizeof(dfs_header) )
    {
        x_fclose(fp);
        return;
    }

    byte* pDFS = (byte*)x_malloc(FSize);
    if(!pDFS)
    {
        x_fclose(fp);
        return;
    }

    x_fread( pDFS, FSize, 1, fp );
    x_fclose(fp);

    dfs_header* pHeader = dfs_InitHeaderFromRawPtr( pDFS );
    if( !pHeader )
    {
        x_free( pDFS );
        return;
    }

    dfs_DumpFileListing( pHeader, "dfs_listing.txt" );
    x_free( pDFS );
}

//==============================================================================

void DoExtract( void )
{
    dfs_Extract( s_dfsPathName, s_ExtractPath );
}

//==============================================================================

void DoVerify( void )
{
    dfs_Verify( s_dfsPathName );
}

//==============================================================================

int main( int argc, char** argv )
{
#if !defined(TARGET_WINDOWS)
    // POSIX builds must initialize x_files explicitly.
    x_Init( argc, argv );
#endif

    command_line    CommandLine;
    s32             iOption;
    s32             Modes       = 0;
    xbool           NeedHelp    = FALSE;
    xbool           bBuild      = FALSE;
    xbool           bUpdate     = FALSE;
    xbool           bOptimize   = FALSE;
    xbool           bList       = FALSE;
    xbool           bExtract    = FALSE;
    xbool           bVerify     = FALSE;

    xtimer Timer;
    Timer.Start();

    // Setup recognized command line options
    CommandLine.AddOptionDef( "_"     , command_line::SWITCH );
    CommandLine.AddOptionDef( "B"     , command_line::STRING );
    CommandLine.AddOptionDef( "M"     , command_line::SWITCH );
    CommandLine.AddOptionDef( "U"     , command_line::STRING );
    CommandLine.AddOptionDef( "O"     , command_line::STRING );
    CommandLine.AddOptionDef( "L"     , command_line::STRING );
    CommandLine.AddOptionDef( "X"     , command_line::STRING );
    CommandLine.AddOptionDef( "P"     , command_line::STRING );
    CommandLine.AddOptionDef( "V"     , command_line::STRING );
    CommandLine.AddOptionDef( "SPLIT" , command_line::STRING );
    CommandLine.AddOptionDef( "SECTOR", command_line::STRING );
    CommandLine.AddOptionDef( "CHUNK" , command_line::STRING );
    CommandLine.AddOptionDef( "CRC"   , command_line::SWITCH );
    CommandLine.AddOptionDef( "ROOT"  , command_line::STRING );

    // Parse command line
    NeedHelp    = CommandLine.Parse( argc, argv );

    if( CommandLine.FindOption( xstring("_") ) != -1 )
    {
        chdir( ".." );

        char Buffer[256];
#if defined(TARGET_WINDOWS)
        GetCurrentDirectory( sizeof(Buffer), &Buffer[0] );
#else
        getcwd( Buffer, sizeof(Buffer) );
#endif
        x_printf( "%s", Buffer );
    }

    // Read B option
    iOption = CommandLine.FindOption( xstring("B") );
    if( iOption != -1 )
    {
        bBuild = TRUE;
        Modes++;
        s_dfsPathName = CommandLine.GetOptionString( iOption );
    }

    // Read U option
    iOption = CommandLine.FindOption( xstring("U") );
    if( iOption != -1 )
    {
        bUpdate = TRUE;
        Modes++;
        s_dfsPathName = CommandLine.GetOptionString( iOption );
    }

    // Read O option
    iOption = CommandLine.FindOption( xstring("O") );
    if( iOption != -1 )
    {
        bOptimize = TRUE;
        Modes++;
        s_dfsPathName = CommandLine.GetOptionString( iOption );
    }

    // Read L option
    iOption = CommandLine.FindOption( xstring("L") );
    if( iOption != -1 )
    {
        bList = TRUE;
        Modes++;
        s_dfsPathName = CommandLine.GetOptionString( iOption );
    }

    // Read X option
    iOption = CommandLine.FindOption( xstring("X") );
    if( iOption != -1 )
    {
        bExtract = TRUE;
        Modes++;
        s_dfsPathName = CommandLine.GetOptionString( iOption );
    }

    iOption = CommandLine.FindOption( xstring("P") );
    if( iOption != -1 )
    {
        s_ExtractPath = CommandLine.GetOptionString( iOption );
        if( s_ExtractPath.GetLength() &&
            (s_ExtractPath.GetAt(s_ExtractPath.GetLength() - 1) != '\\') &&
            (s_ExtractPath.GetAt(s_ExtractPath.GetLength() - 1) != '/') )
        {
#if defined(TARGET_WINDOWS)
            s_ExtractPath += '\\';
#else
            s_ExtractPath += '/';
#endif
        }
    }

    iOption = CommandLine.FindOption( xstring("V") );
    if( iOption != -1 )
    {
        bVerify = TRUE;
        Modes++;
        s_dfsPathName = CommandLine.GetOptionString( iOption );
    }

    // Read SPLIT option
    iOption = CommandLine.FindOption( xstring("SPLIT") );
    if( iOption != -1 )
    {
        s_SplitSize = atoi( CommandLine.GetOptionString( iOption ) );
        
        // Has to fit in 31 bits.
        ASSERT( s_SplitSize < 0x80000000 );
    }

    // Read SECTOR option
    iOption = CommandLine.FindOption( xstring("SECTOR") );
    if( iOption != -1 )
    {
        s_SectorSize = atoi( CommandLine.GetOptionString( iOption ) );
    }

    // Read CHUNK option
    iOption = CommandLine.FindOption( xstring("CHUNK") );
    if( iOption != -1 )
    {
        s_ChunkSize = atoi( CommandLine.GetOptionString( iOption ) );
    }

    // Read CRC option
    iOption = CommandLine.FindOption( xstring("CRC") );
    if( iOption != -1 )
    {
        s_bEnableCRC = TRUE;
    }

    iOption = CommandLine.FindOption( xstring("ROOT") );
    if( iOption != -1 )
    {
        s_RootPath = CommandLine.GetOptionString( iOption );
    }

    // Read M option
    if( CommandLine.FindOption( xstring("M") ) != -1 )
    {
        s_DoMake = TRUE;
    }

    // Build list of script files
    for( s32 i=0 ; i<CommandLine.GetNumArguments() ; i++ )
    {
        s_ScriptFiles.Append( CommandLine.GetArgument( i ) );
    }

    // Display help if needed
    if( (NeedHelp)                                   ||
        (Modes != 1)                                 ||
        (bBuild    && s_ScriptFiles.GetCount() == 0) ||
        (bUpdate   && s_ScriptFiles.GetCount() == 0) ||
        (bOptimize && s_ScriptFiles.GetCount() != 0) ||
        (bList     && s_ScriptFiles.GetCount() != 0) ||
        (bExtract  && s_ScriptFiles.GetCount() != 0) )
    {
        DisplayHelp();
#if !defined(TARGET_WINDOWS)
        x_Kill();
#endif
        return 10;
    }

    // Apply root stripping
    dfs_SetRootPath( s_RootPath );

    // Set the chunk size.
    dfs_SetChunkSize( s_ChunkSize );

    // Align audio packages to sector boundaries.
    dfs_SectorAlign( ".AUDIOPKG" );

    // Execute appropriate mode
    if( bBuild )
        DoBuild();
    if( bUpdate )
        DoUpdate();
    if( bOptimize )
        DoOptimize();
    if( bList )
        DoList();
    if( bExtract )
        DoExtract();
    if( bVerify )
        DoVerify();

    x_printf( "Time = %f\n", Timer.ReadMs() );

#if !defined(TARGET_WINDOWS)
    x_Kill();
#endif

    // Return Success
    return 0;
}
