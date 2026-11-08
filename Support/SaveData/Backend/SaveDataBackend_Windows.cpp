//=========================================================================
//
//  SaveDataBackend_Windows.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SaveData/Backend/SaveDataBackend.hpp"
#include <windows.h>

//=========================================================================
//  LOCAL STORAGE
//=========================================================================

static xstring s_RootDirectory;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
xstring SaveDataRoot( void )
{
    return s_RootDirectory.GetLength() ? xstring( xfs( "%s\\SAVES", (const char*)s_RootDirectory ) )
                                       : xstring( "SAVES" );
}

//=========================================================================

static
xstring SaveDataTempRoot( void )
{
    return xstring( xfs( "%s\\.tmp", (const char*)SaveDataRoot() ) );
}

//=========================================================================

static
xstring MakePath( const char* pName )
{
    return xstring( xfs( "%s\\%s", (const char*)SaveDataRoot(), pName ) );
}

//=========================================================================

static
xstring MakeTempPath( const char* pName )
{
    return xstring( xfs( "%s\\%s.tmp", (const char*)SaveDataTempRoot(), pName ) );
}

//=========================================================================

static
SaveDataStatus MapError( DWORD Error )
{
    switch( Error )
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return SaveDataStatus::NotFound;
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL:
            return SaveDataStatus::NoSpace;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return SaveDataStatus::AccessDenied;
        default:
            return SaveDataStatus::IoError;
    }
}

//=========================================================================

static
SaveDataStatus CheckAvailableSpace( s32 RequiredBytes )
{
    if( RequiredBytes <= 0 )
    {
        return SaveDataStatus::Success;
    }

    ULARGE_INTEGER Available;
    if( !GetDiskFreeSpaceExA( (const char*)SaveDataRoot(), &Available, NULL, NULL ) )
    {
        return MapError( GetLastError() );
    }

    return Available.QuadPart < static_cast<u64>( RequiredBytes ) ? SaveDataStatus::NoSpace : SaveDataStatus::Success;
}

//=========================================================================

static
xbool HasTempSuffix( const char* pName )
{
    const s32 Length = pName ? x_strlen( pName ) : 0;
    return ( Length > 4 ) && ( x_stricmp( pName + Length - 4, ".tmp" ) == 0 );
}

//=========================================================================

static
xbool IsValidFileName( const char* pName )
{
    if( ( pName == NULL ) || ( pName[0] == '\0' ) || ( x_stricmp( pName, "." ) == 0 ) ||
        ( x_stricmp( pName, ".." ) == 0 ) || HasTempSuffix( pName ) )
    {
        return FALSE;
    }

    for( const char* pChar = pName; *pChar; ++pChar )
    {
        if( ( *pChar == '\\' ) || ( *pChar == '/' ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

static
SaveDataStatus EnsureDirectory( const char* pPath )
{
    if( CreateDirectoryA( pPath, NULL ) )
    {
        return SaveDataStatus::Success;
    }

    const DWORD Error = GetLastError();
    if( Error != ERROR_ALREADY_EXISTS )
    {
        return MapError( Error );
    }

    const DWORD Attributes = GetFileAttributesA( pPath );
    return ( Attributes != INVALID_FILE_ATTRIBUTES ) && ( Attributes & FILE_ATTRIBUTE_DIRECTORY )
               ? SaveDataStatus::Success
               : SaveDataStatus::IoError;
}

//=========================================================================

static
void RecoverInterruptedWrites( void )
{
    WIN32_FIND_DATAA FindData;
    HANDLE           Find = FindFirstFileA( (const char*)MakeTempPath( "*" ), &FindData );
    if( Find == INVALID_HANDLE_VALUE )
    {
        return;
    }

    do
    {
        if( ( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) || !HasTempSuffix( FindData.cFileName ) )
        {
            continue;
        }

        const xstring TempPath = xstring( xfs( "%s\\%s", (const char*)SaveDataTempRoot(), FindData.cFileName ) );
        DeleteFileA( TempPath );
    } while( FindNextFileA( Find, &FindData ) );

    FindClose( Find );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void SaveDataBackend_SetRootDirectory( const char* pRootDir )
{
    s_RootDirectory = pRootDir ? pRootDir : "";
}

//=========================================================================

SaveDataStatus SaveDataBackend::Init( void )
{
    SaveDataStatus Status = EnsureDirectory( (const char*)SaveDataRoot() );
    if( Status != SaveDataStatus::Success )
    {
        return Status;
    }

    Status = EnsureDirectory( (const char*)SaveDataTempRoot() );
    if( Status != SaveDataStatus::Success )
    {
        return Status;
    }

    RecoverInterruptedWrites();
    return SaveDataStatus::Success;
}

//=========================================================================

void SaveDataBackend::Kill( void )
{
}

//=========================================================================

SaveDataStatus SaveDataBackend::List( xarray<SaveDataFileInfo>& Files )
{
    Files.Clear();

    WIN32_FIND_DATAA FindData;
    HANDLE           Find = FindFirstFileA( (const char*)MakePath( "*" ), &FindData );
    if( Find == INVALID_HANDLE_VALUE )
    {
        const DWORD Error = GetLastError();
        return Error == ERROR_FILE_NOT_FOUND ? SaveDataStatus::Success : MapError( Error );
    }

    do
    {
        if( ( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) || HasTempSuffix( FindData.cFileName ) )
        {
            continue;
        }

        ULARGE_INTEGER Size;
        Size.LowPart  = FindData.nFileSizeLow;
        Size.HighPart = FindData.nFileSizeHigh;
        if( Size.QuadPart > 0x7fffffff )
        {
            FindClose( Find );
            return SaveDataStatus::IoError;
        }
        SaveDataFileInfo& Info = Files.Append();

        Info.Name = FindData.cFileName;
        Info.Size = static_cast<s32>( Size.QuadPart );
    } while( FindNextFileA( Find, &FindData ) );

    const DWORD Error = GetLastError();

    FindClose( Find );

    if( Error != ERROR_NO_MORE_FILES )
    {
        return MapError( Error );
    }

    return SaveDataStatus::Success;
}

//=========================================================================

SaveDataStatus SaveDataBackend::Read( const char* pName, xarray<u8>& Bytes )
{
    Bytes.Clear();

    if( !IsValidFileName( pName ) )
    {
        return SaveDataStatus::IoError;
    }

    const xstring Path = MakePath( pName );
    HANDLE File = CreateFileA( Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( File == INVALID_HANDLE_VALUE )
    {
        return MapError( GetLastError() );
    }

    LARGE_INTEGER Size;
    if( !GetFileSizeEx( File, &Size ) )
    {
        const DWORD Error = GetLastError();
        CloseHandle( File );
        return MapError( Error );
    }
    if( ( Size.QuadPart < 0 ) || ( Size.QuadPart > 0x7fffffff ) )
    {
        CloseHandle( File );
        return SaveDataStatus::IoError;
    }

    Bytes.SetCount( (s32)Size.QuadPart );
    DWORD      BytesRead = 0;
    const BOOL ReadOK =
        ( Bytes.GetCount() == 0 ) || ReadFile( File, Bytes.GetPtr(), (DWORD)Bytes.GetCount(), &BytesRead, NULL );
    const DWORD Error = ReadOK ? ERROR_SUCCESS : GetLastError();
    CloseHandle( File );

    if( !ReadOK || ( BytesRead != (DWORD)Bytes.GetCount() ) )
    {
        Bytes.Clear();
        return MapError( Error );
    }
    return SaveDataStatus::Success;
}

//=========================================================================

SaveDataStatus SaveDataBackend::WriteAtomic( const char* pName, const xarray<u8>& Bytes )
{
    if( !IsValidFileName( pName ) )
    {
        return SaveDataStatus::IoError;
    }

    const SaveDataStatus DirectoryStatus = EnsureDirectory( (const char*)SaveDataRoot() );
    if( DirectoryStatus != SaveDataStatus::Success )
    {
        return DirectoryStatus;
    }

    const SaveDataStatus TempDirectoryStatus = EnsureDirectory( (const char*)SaveDataTempRoot() );
    if( TempDirectoryStatus != SaveDataStatus::Success )
    {
        return TempDirectoryStatus;
    }

    const SaveDataStatus SpaceStatus = CheckAvailableSpace( Bytes.GetCount() );
    if( SpaceStatus != SaveDataStatus::Success )
    {
        return SpaceStatus;
    }

    const xstring TargetPath = MakePath( pName );
    const xstring TempPath   = MakeTempPath( pName );

    HANDLE File = CreateFileA( TempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( File == INVALID_HANDLE_VALUE )
    {
        return MapError( GetLastError() );
    }

    DWORD BytesWritten = 0;
    BOOL  WriteOK =
        ( Bytes.GetCount() == 0 ) || WriteFile( File, Bytes.GetPtr(), (DWORD)Bytes.GetCount(), &BytesWritten, NULL );
    if( WriteOK && ( BytesWritten == (DWORD)Bytes.GetCount() ) )
    {
        WriteOK = FlushFileBuffers( File );
    }

    DWORD Error = WriteOK ? ERROR_SUCCESS : GetLastError();
    if( WriteOK && ( BytesWritten != (DWORD)Bytes.GetCount() ) )
        Error = ERROR_WRITE_FAULT;
    if( !CloseHandle( File ) && WriteOK && Error == ERROR_SUCCESS )
    {
        WriteOK = FALSE;
        Error   = GetLastError();
    }
    if( !WriteOK || ( BytesWritten != (DWORD)Bytes.GetCount() ) )
    {
        DeleteFileA( TempPath );
        return MapError( Error );
    }

    if( !MoveFileExA( TempPath, TargetPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) )
    {
        const DWORD Error = GetLastError();
        DeleteFileA( TempPath );
        return MapError( Error );
    }
    return SaveDataStatus::Success;
}

//=========================================================================

SaveDataStatus SaveDataBackend::Delete( const char* pName )
{
    if( !IsValidFileName( pName ) )
    {
        return SaveDataStatus::IoError;
    }

    if( DeleteFileA( (const char*)MakePath( pName ) ) )
    {
        return SaveDataStatus::Success;
    }
    return MapError( GetLastError() );
}
