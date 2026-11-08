//==============================================================================
//
//  GameAppPlatform_Windows.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GameAppPlatform.hpp"
#include "A51Resources.hpp"

#include <windows.h>
#include <cstdlib>
#include <cstring>

#include "Entropy/SDLEngine/sdleng_window.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
xbool CopyPath( char* pBuffer, s32 BufferSize, const char* pPath )
{
    if( (pBuffer == NULL) || (BufferSize <= 0) || (pPath == NULL) )
        return( FALSE );

    size_t Length = std::strlen( pPath );
    if( Length >= (size_t)BufferSize )
    {
        pBuffer[0] = '\0';
        return( FALSE );
    }

    std::memcpy( pBuffer, pPath, Length + 1 );
    return( TRUE );
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

xbool GameAppGetExecutableDirectory( char* pBuffer, s32 BufferSize )
{
    if( (pBuffer == NULL) || (BufferSize <= 0) )
        return( FALSE );

    DWORD Length = GetModuleFileNameA( NULL, pBuffer, (DWORD)BufferSize );
    if( (Length == 0) || (Length >= (DWORD)BufferSize) )
    {
        pBuffer[0] = '\0';
        return( FALSE );
    }

    pBuffer[Length] = '\0';
    char* pLastSlash = std::strrchr( pBuffer, '\\' );
    char* pLastForwardSlash = std::strrchr( pBuffer, '/' );
    if( pLastForwardSlash && (!pLastSlash || (pLastForwardSlash > pLastSlash)) )
        pLastSlash = pLastForwardSlash;

    if( pLastSlash )
        *pLastSlash = '\0';

    return( pBuffer[0] != '\0' );
}

//=============================================================================

xbool GameAppGetDataDirectory( char* pBuffer, s32 BufferSize )
{
    const char* pDataRoot = std::getenv( "A51_DATA_ROOT" );
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
    HWND const hWindow = sdleng_WindowGetHandle();
    if( !hWindow )
    {
        return;
    }

    HINSTANCE const hInstance = GetModuleHandleW( NULL );
    HICON const hIcon = LoadIconW( hInstance, MAKEINTRESOURCEW( A51_ICON_RESOURCE_ID ) );
    if( !hIcon )
    {
        return;
    }

    SendMessageW( hWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( hIcon ) );
    SendMessageW( hWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( hIcon ) );
}
