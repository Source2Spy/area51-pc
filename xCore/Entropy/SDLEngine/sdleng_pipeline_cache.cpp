//==============================================================================
//
//  sdleng_pipeline_cache.cpp
//
//==============================================================================

#include "x_target.hpp"

#if ( defined( TARGET_DESKTOP ) || defined( TARGET_MOBILE ) ) && defined( ENTROPY_RENDER_SDL )

#include "sdleng_private.hpp"
#include "e_Engine.hpp"
#include "../IOManager/io_filesystem.hpp"

#include "x_string.hpp"

#if defined( TARGET_WINDOWS )
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

static const u32 kPipelineCacheMagic   = 0x43503541;
static const u32 kPipelineCacheVersion = 1;
static const u32 kMaxPipelineCacheSize = 256 * 1024 * 1024;

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static xstring s_PipelineCachePath;
static u32     s_PipelineCacheCRC  = 0;
static usize   s_PipelineCacheSize = 0;

//==============================================================================
//  CACHE CONFIGURATION
//==============================================================================

void eng_SetPipelineCacheDirectory( const char* pDirectory )
{
    s_PipelineCachePath.Clear();
    s_PipelineCacheCRC  = 0;
    s_PipelineCacheSize = 0;

    if( !pDirectory || !pDirectory[0] )
    {
        return;
    }

    // Resolve relative paths now, before the process can change its working directory.
#if defined( TARGET_WINDOWS )
    const DWORD Capacity = GetFullPathNameA( pDirectory, 0, NULL, NULL );
    if( !Capacity || Capacity > (DWORD)S32_MAX )
    {
        return;
    }

    char* pPath = (char*)x_malloc( (s32)Capacity );
    if( !pPath )
    {
        return;
    }

    const DWORD Length = GetFullPathNameA( pDirectory, Capacity, pPath, NULL );
    if( Length && Length < Capacity )
    {
        s_PipelineCachePath = pPath;
    }

    x_free( pPath );

    if( s_PipelineCachePath.IsEmpty() )
    {
        return;
    }
#else
    if( pDirectory[0] == '/' )
    {
        s_PipelineCachePath = pDirectory;
    }
    else
    {
        char CurrentDirectory[PATH_MAX];
        if( !getcwd( CurrentDirectory, sizeof( CurrentDirectory ) ) )
        {
            return;
        }

        s_PipelineCachePath = xstring( CurrentDirectory ) + "/" + pDirectory;
    }
#endif

    const char Last = s_PipelineCachePath[s_PipelineCachePath.GetLength() - 1];
    if( Last != '/' && Last != '\\' )
    {
        s_PipelineCachePath += '/';
    }

    s_PipelineCachePath += "pipeline-cache.bin";
}

//==============================================================================
//  CACHE LOADING
//==============================================================================

void* sdleng_LoadPipelineCache( usize& Size )
{
    Size = 0;

    if( s_PipelineCachePath.IsEmpty() || x_strcmp( SDLENG_GPU_DRIVER_NAME, "vulkan" ) != 0 )
    {
        return NULL;
    }

    X_FILE* pFile = g_IOFSMgr.OpenHostFile( s_PipelineCachePath, "rb" );

    if( !pFile )
    {
        return NULL;
    }

    // Reject incompatible or truncated cache files before allocating the payload.
    u32       Header[4];
    const s32 FileSize = x_flength( pFile );

    if( ( FileSize < static_cast<s32>( sizeof( Header ) ) ) ||
        ( x_fread( Header, 1, static_cast<s32>( sizeof( Header ) ), pFile ) != static_cast<s32>( sizeof( Header ) ) ) )
    {
        x_fclose( pFile );
        return NULL;
    }

    const u32 PayloadSize = SDL_Swap32LE( Header[2] );

    if( ( SDL_Swap32LE( Header[0] ) != kPipelineCacheMagic ) ||
        ( SDL_Swap32LE( Header[1] ) != kPipelineCacheVersion ) ||
        ( PayloadSize < 32 ) || ( PayloadSize > kMaxPipelineCacheSize ) ||
        ( (u32)FileSize != (u32)sizeof( Header ) + PayloadSize ) )
    {
        x_fclose( pFile );
        return NULL;
    }

    void* pData = x_malloc( (s32)PayloadSize );
    if( !pData )
    {
        x_fclose( pFile );
        return NULL;
    }

    // Publish the cached size and checksum only after validating the full payload.
    const xbool Read = x_fread( pData, 1, static_cast<s32>( PayloadSize ), pFile ) == static_cast<s32>( PayloadSize );
    x_fclose( pFile );

    const u32 CRC = SDL_Swap32LE( Header[3] );
    if( !Read || SDL_crc32( 0, pData, PayloadSize ) != CRC )
    {
        x_free( pData );
        return NULL;
    }

    s_PipelineCacheCRC  = CRC;
    s_PipelineCacheSize = PayloadSize;
    Size                = PayloadSize;
    x_DebugMsg( "SDLEngine: read Vulkan pipeline cache '%s' (%u bytes)\n", (const char*)s_PipelineCachePath, PayloadSize );

    return pData;
}

//==============================================================================
//  CACHE SAVING
//==============================================================================

xbool eng_SavePipelineCache( void )
{
    if( s_PipelineCachePath.IsEmpty() || !g_pSDLGPUDevice ||
        x_strcmp( SDL_GetGPUDeviceDriver( g_pSDLGPUDevice ), "vulkan" ) != 0 )
    {
        return FALSE;
    }

    usize Size  = 0;
    void* pData = SDL_GetGPUPipelineCacheData( g_pSDLGPUDevice, &Size );

    if( !pData )
    {
        x_DebugMsg( "SDLEngine: cannot export Vulkan pipeline cache: %s\n", SDL_GetError() );
        return FALSE;
    }

    if( Size < 32 || Size > kMaxPipelineCacheSize )
    {
        SDL_free( pData );
        return FALSE;
    }

    // Avoid rewriting the file when the driver exported the same cache data.
    const u32 CRC = SDL_crc32( 0, pData, Size );

    if( Size == s_PipelineCacheSize && CRC == s_PipelineCacheCRC )
    {
        SDL_free( pData );
        return TRUE;
    }

    // Store a little-endian header with the payload for atomic replacement.
    u32 Header[4] =
        {
            SDL_Swap32LE( kPipelineCacheMagic ),
            SDL_Swap32LE( kPipelineCacheVersion ),
            SDL_Swap32LE( (u32)Size ),
            SDL_Swap32LE( CRC ) };

    const s32 FileSize  = (s32)( sizeof( Header ) + Size );
    byte*     pFileData = (byte*)x_malloc( FileSize );

    if( !pFileData )
    {
        SDL_free( pData );
        return FALSE;
    }

    x_memcpy( pFileData, Header, sizeof( Header ) );
    x_memcpy( pFileData + sizeof( Header ), pData, (s32)Size );
    SDL_free( pData );

    const xbool Saved = g_IOFSMgr.WriteHostFileAtomic( s_PipelineCachePath, pFileData, FileSize );

    x_free( pFileData );

    if( Saved )
    {
        s_PipelineCacheCRC  = CRC;
        s_PipelineCacheSize = Size;
        x_DebugMsg( "SDLEngine: saved Vulkan pipeline cache '%s' (%u bytes)\n", (const char*)s_PipelineCachePath, static_cast<u32>( Size ) );
    }
    else
    {
        x_DebugMsg( "SDLEngine: cannot save Vulkan pipeline cache '%s'\n", (const char*)s_PipelineCachePath );
    }

    return Saved;
}

//==============================================================================
#endif
//==============================================================================
