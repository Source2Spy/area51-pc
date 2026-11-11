//==============================================================================
//
//  x_stdio.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_STDIO_HPP
#include "../x_stdio.hpp"
#endif

#ifndef X_STRING_HPP
#include "../x_string.hpp"
#endif

#ifndef X_PLUS_HPP
#include "../x_plus.hpp"
#endif

#ifndef X_DEBUG_HPP
#include "../x_debug.hpp"
#endif

#ifndef X_PROFILE_HPP
#include "../x_profile.hpp"
#endif

//------------------------------------------------------------------------------

#include <stdio.h>

#ifdef TARGET_WINDOWS
#include <windows.h>
#endif

#ifdef TARGET_POSIX
#include <sys/stat.h>
#endif

//==============================================================================
//  DEFINES
//==============================================================================

#define USE_ANSI_IO_TEXT

//#define CAPTURE_FILE_NAMES

//==============================================================================
//  VARIABLES
//==============================================================================
                                    
static     open_fn*     s_pOpen     = NULL;
static    close_fn*     s_pClose    = NULL;
static     read_fn*     s_pRead     = NULL;
static    write_fn*     s_pWrite    = NULL;
static     seek_fn*     s_pSeek     = NULL;
static     tell_fn*     s_pTell     = NULL;
static    flush_fn*     s_pFlush    = NULL;
static      eof_fn*     s_pEOF      = NULL;
static   length_fn*     s_pLength   = NULL;

static    print_fn*     s_pPrint    = NULL;
static print_at_fn*     s_pPrintAt  = NULL;

//==============================================================================

#if defined(ENABLE_LOGGING)
xstring*    s_LoadMapLog    = NULL;
xbool       s_LoadMapEnabled= FALSE;
#endif

#ifdef CAPTURE_FILE_NAMES
#define NUM_FILE_NAME_ENTRIES 16
struct file_name_entry
{
    X_FILE* fp;
    char    FileName[256];
    char    Mode[8];
};
file_name_entry s_FileNameEntry[NUM_FILE_NAME_ENTRIES];
#endif

void AddToLoadMap(const char* pFilename);
//==============================================================================
#ifdef CAPTURE_FILE_NAMES

void AddFileNameEntry( X_FILE* fp, const char* pFileName, const char* pMode )
{
    if( fp )
    {
        s32 i;

        // Find an empty slot and add entry
        for( i=0; i<NUM_FILE_NAME_ENTRIES; i++ )
        if( s_FileNameEntry[i].fp == NULL )
            break;

        if( i<NUM_FILE_NAME_ENTRIES )
        {
            s_FileNameEntry[i].fp = fp;
            x_strncpy(s_FileNameEntry[i].FileName,pFileName,256);
            x_strncpy(s_FileNameEntry[i].Mode,pMode,8);
        }
    }
}

//==============================================================================

void DelFileNameEntry( X_FILE* fp )
{
    if( fp )
    {
        s32 i;

        // Find a match
        for( i=0; i<NUM_FILE_NAME_ENTRIES; i++ )
        if( s_FileNameEntry[i].fp == fp )
        {
            s_FileNameEntry[i].fp = NULL;
            s_FileNameEntry[i].FileName[0] = 0;
            s_FileNameEntry[i].Mode[0] = 0;
            break;
        }
    }
}

//==============================================================================

const file_name_entry* FindFileNameEntry( X_FILE* fp )
{
    if( fp )
    {
        s32 i;

        // Find a match
        for( i=0; i<NUM_FILE_NAME_ENTRIES; i++ )
        if( s_FileNameEntry[i].fp == fp )
            return &s_FileNameEntry[i];

        return NULL;
    }

    return NULL;
}
//==============================================================================

void DisplayFileNameEntries( void )
{
    s32 i;

    x_DebugMsg("FILES STILL OPEN!!!\n");

    // Find a match
    for( i=0; i<NUM_FILE_NAME_ENTRIES; i++ )
    if( s_FileNameEntry[i].fp != NULL )
    {
        x_DebugMsg("FILE: %s\n",s_FileNameEntry[i].FileName);
    }
}

#endif

//==============================================================================
//  CUSTOM FILE I/O AND TEXT OUT HOOK FUNCTIONS
//==============================================================================

void x_SetFileIOHooks(  open_fn*  pOpen,
                       close_fn*  pClose,
                        read_fn*  pRead,
                       write_fn*  pWrite,
                        seek_fn*  pSeek,
                        tell_fn*  pTell,
                       flush_fn*  pFlush,
                         eof_fn*  pEOF,
                      length_fn*  pLength )
{
    s_pOpen   = pOpen; 
    s_pClose  = pClose;
    s_pRead   = pRead; 
    s_pWrite  = pWrite;
    s_pSeek   = pSeek; 
    s_pTell   = pTell; 
    s_pFlush  = pFlush;
    s_pEOF    = pEOF;
    s_pLength = pLength;
}

//==============================================================================

void x_GetFileIOHooks    (     open_fn*  &pOpen,
                              close_fn*  &pClose,
                               read_fn*  &pRead,
                              write_fn*  &pWrite,
                               seek_fn*  &pSeek,
                               tell_fn*  &pTell,
                              flush_fn*  &pFlush,
                                eof_fn*  &pEOF,
                             length_fn*  &pLength  )
{
    pOpen   = s_pOpen; 
    pClose  = s_pClose;
    pRead   = s_pRead; 
    pWrite  = s_pWrite;
    pSeek   = s_pSeek; 
    pTell   = s_pTell; 
    pFlush  = s_pFlush;
    pEOF    = s_pEOF;
    pLength = s_pLength;
}

//==============================================================================

void x_SetPrintHook( print_fn* pPrint )
{
    s_pPrint = pPrint;
}

//==============================================================================

void x_SetPrintAtHook( print_at_fn* pPrintAt )
{
    s_pPrintAt = pPrintAt;
}

//==============================================================================
//  TEXT OUTPUT FUNCTIONS
//==============================================================================

s32 x_printf( const char* pFormatStr, ... )
{
    s32         NChars;
    x_va_list   Args;
    x_va_start( Args, pFormatStr );

    xvfs XVFS( pFormatStr, Args );
    NChars = *(((s32*)(const char*)XVFS)-1);
    
    if( s_pPrint )  
        s_pPrint( XVFS );

    return( NChars );
}

//==============================================================================

s32 x_printfxy( s32 X, s32 Y, const char* pFormatStr, ... )
{
    s32         NChars;
    x_va_list   Args;
    x_va_start( Args, pFormatStr );

    xvfs XVFS( pFormatStr, Args );
    NChars = *(((s32*)(const char*)XVFS)-1);

    if( s_pPrintAt )  
        s_pPrintAt( XVFS, X, Y );

    return( NChars );
}

//==============================================================================

s32 x_sprintf( char* pStr, const char* pFormatStr, ... )
{
    x_va_list   Args;
    x_va_start( Args, pFormatStr );

    return( x_vsprintf( pStr, pFormatStr, Args ) );
}

//==============================================================================

s32 x_vsprintf( char* pStr, const char* pFormatStr, x_va_list Args )
{
    // TO DO : Implement x_vsprintf.
    return( vsprintf( pStr, pFormatStr, Args ) );
}

/*
s32 x_vsprintf( char* pStr, const char* pFormatStr, x_va_list Args )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_vsprintf" );
    
    ASSERT( pStr );
    ASSERT( pFormatStr );
    
    xvfs XVFS( pFormatStr, Args );
    s32 NChars = *(((s32*)(const char*)XVFS)-1);
    
    // Copy formatted string to output buffer
    x_memcpy( pStr, (const char*)XVFS, NChars );
    pStr[NChars] = '\0';  // Null terminate
    
    return NChars;
}
*/

//==============================================================================
//  STANDARD FILE I/O FUNCTIONS
//==============================================================================

X_FILE* x_fopen( const char* pFileName, const char* pMode ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fopen" );

    X_FILE* fp;

    ASSERT( s_pOpen );
    fp = s_pOpen(pFileName,pMode);

    if (fp)
    {
        AddToLoadMap(pFileName);
    }

#ifdef CAPTURE_FILE_NAMES
    if( fp )
    {
        AddFileNameEntry(fp,pFileName,pMode);
    }
#endif

    return( fp );
}

//==============================================================================

void x_EnableLoadMap (xbool Flag)
{
    (void)Flag;
#if defined(ENABLE_LOGGING)
    s_LoadMapEnabled = Flag;
#endif
}

//==============================================================================

void x_DumpLoadMap(const char* pFilename)
{
    (void)pFilename;
#if defined(ENABLE_LOGGING)
    if (s_LoadMapEnabled)
    {
        if (pFilename)
            s_LoadMapLog->SaveFile( pFilename );
        s_LoadMapLog->Clear();
    }
#endif
}

//==============================================================================

void AddToLoadMap(const char* pFilename)
{
    (void)pFilename;
#if defined(ENABLE_LOGGING)
    if (s_LoadMapEnabled)
    {
        if (x_stristr(pFilename,".audiopkg"))
            return;
        if( !s_LoadMapLog )
            s_LoadMapLog = new xstring(128*1024);
        //*s_LoadMapLog += xfs( "[%2.02f] %s\n", (f32)x_GetTimeSec(),pFilename );
        *s_LoadMapLog += xfs( "%s\n", pFilename );
    }
#endif
}
//==============================================================================

void x_fclose( X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fclose" );

    if( pFile == NULL )
        return;

#ifdef CAPTURE_FILE_NAMES
    DelFileNameEntry(pFile);
#endif

    ASSERT( s_pClose );
    s_pClose( pFile );
}

//==============================================================================

s32 x_fread( void* pBuffer, s32 Size, s32 Count, X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fread" );

    s32 Bytes;
    
    if( Count*Size == 0 )
        return 0;

    ASSERT( pBuffer );
    ASSERT( s_pRead );
    Bytes = s_pRead( pFile, (byte*)pBuffer, (Size * Count) );
    return( Bytes / Size );
}

//==============================================================================

s32 x_fwrite( const void* pBuffer, s32 Size, s32 Count, X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fwrite" );

    s32 Bytes;
    
    if( Size*Count == 0 )
        return 0;

    ASSERT( pBuffer );
    ASSERT( s_pWrite );
    Bytes = s_pWrite( pFile, (byte*)pBuffer, (Size * Count) );
    return( Bytes / Size );
}

//==============================================================================

s32 x_fprintf( X_FILE* pFile, const char* pFormatStr, ... )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fprintf" );

    s32         NChars;
    s32         Bytes;
    x_va_list   Args;
    x_va_start( Args, pFormatStr );

    ASSERT( s_pWrite );

    xstring String;
    String.FormatV( pFormatStr, Args );

    NChars = String.GetLength();
    
    Bytes = s_pWrite( pFile, (const byte*)(const char*)String, NChars );
    ASSERT( Bytes == NChars );

    return( Bytes );
}

//==============================================================================

s32 x_fflush( X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fflush" );

    ASSERT( s_pFlush );
    return( s_pFlush( pFile ) );
}

//==============================================================================

s32 x_fseek( X_FILE* pFile, s32 Offset, s32 Origin ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fseek" );

    ASSERT( IN_RANGE( 0, Origin, 2 ) );
    ASSERT( s_pSeek );
    return( s_pSeek( pFile, Offset, Origin ) );
}

//==============================================================================

s32 x_ftell( X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_ftell" );

    ASSERT( s_pTell );
    return( s_pTell( pFile ) );
}

//==============================================================================

s32 x_feof( X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_feof" );

    ASSERT( s_pEOF );
    return( s_pEOF( pFile ) );
}

//==============================================================================

s32 x_fgetc( X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fgetc" );

    char Char;
    s32  Result;
    s32  Bytes;

    Bytes = x_fread( &Char, 1, 1, pFile );
    if( Bytes == 0 )    Result = X_EOF;
    else                Result = Char;
    return( Result );
}

//==============================================================================

s32 x_fputc( s32 C, X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_fputc" );

    char Char = (char)C;
    s32  Result;
    s32  Bytes;

    Bytes = x_fwrite( &Char, 1, 1, pFile );
    if( Bytes == 0 )    Result = X_EOF;
    else                Result = Char;
    return( Result );
}

//==============================================================================

s32 x_flength( X_FILE* pFile ) 
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "x_flength" );

    ASSERT( s_pLength );
    return( s_pLength( pFile ) );
}

//==============================================================================
//  FILE I/O AND TEXT OUTPUT FUNCTIONS WRITTEN USING STANDARD ANSI C
//==============================================================================

//==============================================================================
#ifdef USE_ANSI_IO_TEXT
//==============================================================================

X_FILE* ansi_Open( const char* pFileName, const char* pMode )
{
    return( (X_FILE*)fopen( pFileName, pMode ) );
}

//==============================================================================

void ansi_Close( X_FILE* pFile )
{
    fclose( (FILE*)pFile );
}

//==============================================================================

s32 ansi_Tell( X_FILE* pFile );
xbool ansi_Length( X_FILE* pFile );

s32 ansi_Read( X_FILE* pFile, byte* pBuffer, s32 Bytes )
{
    return( fread( pBuffer, 1, Bytes, (FILE*)pFile ) );
}

//==============================================================================

s32 ansi_Write( X_FILE* pFile, const byte* pBuffer, s32 Bytes )
{
    return( fwrite( pBuffer, 1, Bytes, (FILE*)pFile ) );
}

//==============================================================================

s32 ansi_Seek( X_FILE* pFile, s32 Offset, s32 Origin )
{
    return( fseek( (FILE*)pFile, Offset, Origin ) );
}

//==============================================================================

s32 ansi_Tell( X_FILE* pFile )
{
    return( ftell( (FILE*)pFile ) );
}

//==============================================================================

s32 ansi_Flush( X_FILE* pFile )
{
    return( fflush( (FILE*)pFile ) );
}

//==============================================================================

xbool ansi_EOF( X_FILE* pFile )
{
    return( feof((FILE*)pFile) ? 1 : 0 );
}

//==============================================================================

xbool ansi_Length( X_FILE* pFile )
{
    s32 Length;
    s32 Cursor;
    
    Cursor = ftell( (FILE*)pFile );   fseek( (FILE*)pFile,      0, SEEK_END );
    Length = ftell( (FILE*)pFile );   fseek( (FILE*)pFile, Cursor, SEEK_SET );

    return( Length );
}

//==============================================================================

void ansi_Print( const char* pString )
{
    printf( "%s", pString );
}

//==============================================================================

void ansi_PrintAt( const char* pString, s32 X, s32 Y )
{
#ifdef TARGET_WINDOWS
    HANDLE                      hConOut;
    CONSOLE_SCREEN_BUFFER_INFO  SBI;
    COORD                       Coord;

    hConOut = GetStdHandle( STD_OUTPUT_HANDLE );
    if( hConOut == INVALID_HANDLE_VALUE )
        return;

    Coord.X = (short)X;
    Coord.Y = (short)Y;

    GetConsoleScreenBufferInfo( hConOut, &SBI );
    SetConsoleCursorPosition  ( hConOut, Coord );
#endif

#ifndef TARGET_WINDOWS
    (void)X;
    (void)Y;
#endif

    printf( "%s", pString );

#ifdef TARGET_WINDOWS
    SetConsoleCursorPosition  ( hConOut, SBI.dwCursorPosition );
#endif
}

//==============================================================================
#endif // USE_ANSI_IO_TEXT
//==============================================================================

//==============================================================================
#if defined(TARGET_WINDOWS)
//==============================================================================

xbool   x_GetFileTime( const char* pFileName, u64& FileTime )
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    hFind = FindFirstFile( pFileName, &FindFileData);
  
    if( hFind == INVALID_HANDLE_VALUE )
        return FALSE;

    x_memcpy( &FileTime, &FindFileData.ftLastWriteTime, sizeof(u64) );
    FindClose(hFind);
  
    return TRUE;
}

//==============================================================================
#elif defined(TARGET_POSIX)
//==============================================================================

xbool x_GetFileTime( const char* pFileName, u64& FileTime )
{
    struct stat FileStat;

    if( stat( pFileName, &FileStat ) != 0 )
        return FALSE;

    FileTime = static_cast<u64>( FileStat.st_mtime );
    return TRUE;
}

//==============================================================================
#endif
//==============================================================================

//==============================================================================
//  INITIALIZATION AND SHUT DOWN FUNCTIONS
//==============================================================================

void x_IOInit( void )
{
#ifdef USE_ANSI_IO_TEXT
        x_SetFileIOHooks( ansi_Open, 
                          ansi_Close, 
                          ansi_Read, 
                          ansi_Write, 
                          ansi_Seek, 
                          ansi_Tell, 
                          ansi_Flush, 
                          ansi_EOF,
                          ansi_Length  );
        x_SetPrintHook  ( ansi_Print   );
        x_SetPrintAtHook( ansi_PrintAt );
#endif // USE_ANSI_IO_TEXT

#ifdef CAPTURE_FILE_NAMES
    for( s32 i=0; i<NUM_FILE_NAME_ENTRIES; i++ )
    {
        s_FileNameEntry[i].fp = NULL;
        s_FileNameEntry[i].FileName[0] = 0;
    }
#endif
}

//==============================================================================

void x_IOKill( void )
{
    x_SetFileIOHooks( NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
    x_SetPrintHook  ( NULL );
    x_SetPrintAtHook( NULL );
}

//==============================================================================
