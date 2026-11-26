//==============================================================================
//
//  GameAppPlatform_Linux.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GameAppPlatform.hpp"
#include "x_plus.hpp"

#include <stdlib.h>
#include <unistd.h>

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
xbool CopyPath( char* pBuffer, s32 BufferSize, const char* pPath )
{
    if( (pBuffer == NULL) || (BufferSize <= 0) || (pPath == NULL) )
        return( FALSE );

    s32 Length = x_strlen( pPath );
    if( Length >= BufferSize )
    {
        pBuffer[0] = '\0';
        return( FALSE );
    }

    x_memcpy( pBuffer, pPath, Length + 1 );
    return( TRUE );
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

xbool GameAppGetExecutableDirectory( char* pBuffer, s32 BufferSize )
{
    if( (pBuffer == NULL) || (BufferSize <= 1) )
        return( FALSE );

    s64 Length = (s64)readlink( "/proc/self/exe", pBuffer, (usize)BufferSize - 1 );
    if( (Length <= 0) || (Length >= (s64)(BufferSize - 1)) )
    {
        pBuffer[0] = '\0';
        return( FALSE );
    }

    pBuffer[Length] = '\0';
    char* pLastSlash = x_strrchr( pBuffer, '/' );
    if( pLastSlash )
        *pLastSlash = '\0';

    return( pBuffer[0] != '\0' );
}

//=============================================================================

xbool GameAppGetDataDirectory( char* pBuffer, s32 BufferSize )
{
    const char* pDataRoot = getenv( "A51_DATA_ROOT" );
    if( pDataRoot && pDataRoot[0] )
        return( CopyPath( pBuffer, BufferSize, pDataRoot ) );

    return( GameAppGetExecutableDirectory( pBuffer, BufferSize ) );
}

//=============================================================================

xbool GameAppGetSaveDirectory( char* pBuffer, s32 BufferSize )
{
    return( GameAppGetDataDirectory( pBuffer, BufferSize ) );
}

//=============================================================================

void GameAppSetWindowIcon( void )
{
}
