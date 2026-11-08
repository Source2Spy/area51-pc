//==========================================================================
//
//  io_device_host_stdio.cpp
//
//  Host filesystem device backed by the standard C file API.
//
//==========================================================================

//==========================================================================
//  PLATFORM CHECK
//==========================================================================

#include "x_types.hpp"

#if !defined( TARGET_DESKTOP ) && !defined( TARGET_MOBILE )
#error "This host filesystem backend requires a desktop or mobile target."
#endif

//==========================================================================
//  INCLUDES
//==========================================================================

#include "x_memory.hpp"

#include "../io_mgr.hpp"
#include "../io_filesystem.hpp"
#include "io_device_host.hpp"

#include <stdio.h>

#if defined( TARGET_ANDROID )
#include "SDL3/SDL_iostream.h"
#endif

//==========================================================================
//  DEFINES
//==========================================================================

#define HOSTFS_CACHE_SIZE    (32 * 1024)
#define HOSTFS_NUM_FILES     (32)
#define HOSTFS_INFO_SIZE     (32)
#define HOSTFS_BUFFER_ALIGN  (32)
#define HOSTFS_OFFSET_ALIGN  (4)
#define HOSTFS_LENGTH_ALIGN  (32)

//==========================================================================
//  DATA
//==========================================================================

static char           s_HostCache[ HOSTFS_CACHE_SIZE ];
static io_device_file s_HostFiles[ HOSTFS_NUM_FILES ];

static io_device::device_data s_DeviceData =
{
#if defined( TARGET_POSIX )
#if defined( TARGET_ANDROID )
    "Android HostFS",
#else
    "Linux HostFS",
#endif
#else
    "Windows HostFS",
#endif
    TRUE,
    TRUE,
    FALSE,
    HOSTFS_CACHE_SIZE,
    HOSTFS_BUFFER_ALIGN,
    HOSTFS_OFFSET_ALIGN,
    HOSTFS_LENGTH_ALIGN,
    HOSTFS_NUM_FILES,
    HOSTFS_INFO_SIZE,
    s_HostCache,
    s_HostFiles
};

//==========================================================================
//  HELPER FUNCTIONS
//==========================================================================

static void ReadCallback( s32 Result, void* pFileInfo )
{
    (void)pFileInfo;

    g_IODeviceHost.EnterCallback();
    ProcessEndOfRequest( &g_IODeviceHost,
                         (Result >= 0) ? io_request::COMPLETED : io_request::FAILED );
    g_IODeviceHost.LeaveCallback();
}

//==========================================================================
//  IMPLEMENTATION
//==========================================================================

void io_device_host::CleanFilename( char* pClean, const char* pFilename )
{
    ASSERT( pClean );
    ASSERT( pFilename );
    ASSERT( x_strlen( pFilename ) < IO_DEVICE_FILENAME_LIMIT );

#if defined( TARGET_POSIX )
    const char Separator = '/';
#else
    const char Separator = '\\';
#endif

    while( *pFilename )
    {
        if( ( *pFilename == '\\' ) || ( *pFilename == '/' ) )
        {
            *pClean++ = Separator;
            pFilename++;

            while( *pFilename &&
                   ( ( *pFilename == '\\' ) || ( *pFilename == '/' ) ) )
            {
                pFilename++;
            }
        }
        else
        {
            *pClean++ = *pFilename++;
        }
    }

    *pClean = 0;
}

//==========================================================================

void io_device_host::Init( void )
{
    io_device::Init();
}

//==========================================================================

void io_device_host::Kill( void )
{
    io_device::Kill();
}

//==========================================================================

io_device::device_data* io_device_host::GetDeviceData( void )
{
    return &s_DeviceData;
}

//==========================================================================

xbool io_device_host::DeviceOpen( const char* pFilename,
                                  io_device_file* pFile,
                                  open_flags OpenFlags )
{
    char CleanFile[ IO_DEVICE_FILENAME_LIMIT ];
    CleanFilename( CleanFile, pFilename );

#if defined( TARGET_ANDROID )
    const char* pMode = (OpenFlags & WRITE)
                      ? ((OpenFlags & APPEND) ? "ab" : "wb")
                      : "rb";
    SDL_IOStream* pStream = SDL_IOFromFile( CleanFile, pMode );
    if( !pStream )
    {
        return FALSE;
    }

    const Sint64 FileLength = SDL_GetIOSize( pStream );
    if( (FileLength < 0) || (FileLength > S32_MAX) )
    {
        SDL_CloseIO( pStream );
        return FALSE;
    }

    if( SDL_SeekIO( pStream, 0, SDL_IO_SEEK_SET ) < 0 )
    {
        SDL_CloseIO( pStream );
        return FALSE;
    }

    pFile->Handle = pStream;
    pFile->Length = (s32)FileLength;
    return TRUE;
#else
    (void)OpenFlags;

    pFile->Handle = fopen( CleanFile, "rb" );
    if( !pFile->Handle )
    {
        return FALSE;
    }

    if( fseek( (FILE*)pFile->Handle, 0, SEEK_END ) != 0 )
    {
        fclose( (FILE*)pFile->Handle );
        pFile->Handle = NULL;
        return FALSE;
    }

    const long FileLength = ftell( (FILE*)pFile->Handle );
    if( (FileLength < 0) || (FileLength > S32_MAX) )
    {
        fclose( (FILE*)pFile->Handle );
        pFile->Handle = NULL;
        return FALSE;
    }

    pFile->Length = (s32)FileLength;

    if( fseek( (FILE*)pFile->Handle, 0, SEEK_SET ) != 0 )
    {
        fclose( (FILE*)pFile->Handle );
        pFile->Handle = NULL;
        return FALSE;
    }

    return TRUE;
#endif
}

//==========================================================================

xbool io_device_host::DeviceRead( io_device_file* pFile,
                                  void*           pBuffer,
                                  s32             Length,
                                  s32             Offset,
                                  s32             AddressSpace )
{
    (void)AddressSpace;

#if !defined( X_RETAIL )
    LogDeviceRead( pFile, Length, Offset );
#endif

#if defined( TARGET_ANDROID )
    if( SDL_SeekIO( (SDL_IOStream*)pFile->Handle, Offset, SDL_IO_SEEK_SET ) < 0 )
    {
        ReadCallback( -1, pFile->pHardwareData );
        return FALSE;
    }

    const s32 ReadLength = (s32)SDL_ReadIO( (SDL_IOStream*)pFile->Handle,
                                            pBuffer,
                                            (usize)Length );
#else
    if( fseek( (FILE*)pFile->Handle, Offset, SEEK_SET ) != 0 )
    {
        ReadCallback( -1, pFile->pHardwareData );
        return FALSE;
    }

    const s32 ReadLength = (s32)fread( pBuffer, 1, Length, (FILE*)pFile->Handle );
#endif
    ReadCallback( (ReadLength == Length) ? ReadLength : -1, pFile->pHardwareData );
    return (ReadLength == Length);
}

//==========================================================================

xbool io_device_host::DeviceWrite( io_device_file* pFile,
                                   void*           pBuffer,
                                   s32             Length,
                                   s32             Offset,
                                   s32             AddressSpace )
{
    (void)pFile;
    (void)pBuffer;
    (void)Length;
    (void)Offset;
    (void)AddressSpace;

    ASSERT( 0 );
    return FALSE;
}

//==========================================================================

void io_device_host::DeviceClose( io_device_file* pFile )
{
    if( pFile && pFile->Handle )
    {
#if defined( TARGET_ANDROID )
        SDL_CloseIO( (SDL_IOStream*)pFile->Handle );
#else
        fclose( (FILE*)pFile->Handle );
#endif
        pFile->Handle = NULL;
    }
}
