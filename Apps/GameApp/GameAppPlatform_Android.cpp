//==============================================================================
//
//  GameAppPlatform_Android.cpp
//
//==============================================================================

#include "GameAppPlatform.hpp"
#include "x_plus.hpp"

#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_system.h"

#include <unistd.h>

//#define A51_ANDROID_USE_APP_DATA_DIR

//==============================================================================

static
xbool CopyPath( char* pBuffer, s32 BufferSize, const char* pPath )
{
    if( (pBuffer == NULL) || (BufferSize <= 0) || (pPath == NULL) )
        return FALSE;

    s32 const Length = x_strlen( pPath );
    if( Length >= BufferSize )
    {
        pBuffer[0] = '\0';
        return FALSE;
    }

    x_memcpy( pBuffer, pPath, Length + 1 );
    return TRUE;
}

//==============================================================================

xbool GameAppGetExecutableDirectory( char* pBuffer, s32 BufferSize )
{
    return CopyPath( pBuffer, BufferSize, SDL_GetBasePath() );
}

//==============================================================================

xbool GameAppGetDataDirectory( char* pBuffer, s32 BufferSize )
{
#if defined( A51_ANDROID_USE_APP_DATA_DIR )
    const char* pDataPath = SDL_GetAndroidExternalStoragePath();
#else
    const char* pDataPath = "/storage/emulated/0/Documents/Area51";
#endif
    if( pDataPath && pDataPath[0] && (access( pDataPath, R_OK | X_OK ) == 0) )
        return CopyPath( pBuffer, BufferSize, pDataPath );

    return CopyPath( pBuffer, BufferSize, "." );
}

//==============================================================================

xbool GameAppGetSaveDirectory( char* pBuffer, s32 BufferSize )
{
    char* pSavePath = SDL_GetPrefPath( "Area51", "Area51" );
    if( !pSavePath )
        return FALSE;

    const xbool Result = CopyPath( pBuffer, BufferSize, pSavePath );
    SDL_free( pSavePath );
    return Result;
}

//==============================================================================

void GameAppSetWindowIcon( void )
{
}
