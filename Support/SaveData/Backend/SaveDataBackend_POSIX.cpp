//=========================================================================
//
//  SaveDataBackend_POSIX.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SaveData/Backend/SaveDataBackend.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <sys/statvfs.h>
#include <unistd.h>

//=========================================================================
//  LOCAL STORAGE
//=========================================================================

static std::string s_RootDirectory;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
std::filesystem::path SaveDataRoot( void )
{
    return s_RootDirectory.empty() ? std::filesystem::path( "SAVES" )
                                   : std::filesystem::path( s_RootDirectory ) / "SAVES";
}

//=========================================================================

static
std::filesystem::path SaveDataTempRoot( void )
{
    return SaveDataRoot() / ".tmp";
}

//=========================================================================

static
std::filesystem::path MakePath( const char* pName )
{
    return SaveDataRoot() / pName;
}

//=========================================================================

static
std::filesystem::path MakeTempPath( const char* pName )
{
    return SaveDataTempRoot() / ( std::string( pName ) + ".tmp" );
}

//=========================================================================

static
xbool HasTempSuffix( const std::string& Name )
{
    return ( Name.size() > 4 ) && ( Name.compare( Name.size() - 4, 4, ".tmp" ) == 0 );
}

//=========================================================================

static
xbool HasTempSuffix( const std::filesystem::path& Path )
{
    return HasTempSuffix( Path.filename().string() );
}

//=========================================================================

static
xbool IsValidFileName( const char* pName )
{
    if( ( pName == NULL ) || ( pName[0] == '\0' ) )
        return FALSE;

    const std::string           FileName( pName );
    const std::filesystem::path Name( pName );
    return ( Name != "." ) && ( Name != ".." ) && ( Name.filename() == Name ) && !Name.has_root_path() &&
           ( FileName.find( '\\' ) == std::string::npos ) && !HasTempSuffix( FileName );
}

//=========================================================================

static
SaveDataStatus MapError( const std::error_code& Error )
{
    if( Error == std::errc::no_such_file_or_directory )
        return SaveDataStatus::NotFound;

    if( Error == std::errc::no_space_on_device )
        return SaveDataStatus::NoSpace;

    if( ( Error == std::errc::permission_denied ) || ( Error == std::errc::read_only_file_system ) )
    {
        return SaveDataStatus::AccessDenied;
    }

    return SaveDataStatus::IoError;
}

//=========================================================================

static
SaveDataStatus EnsureRootDirectory( void )
{
    std::error_code Error;
    std::filesystem::create_directories( SaveDataRoot(), Error );
    if( Error )
        return MapError( Error );

    Error.clear();
    std::filesystem::create_directories( SaveDataTempRoot(), Error );
    return Error ? MapError( Error ) : SaveDataStatus::Success;
}

//=========================================================================

static
SaveDataStatus CheckAvailableSpace( s32 RequiredBytes )
{
    if( RequiredBytes <= 0 )
        return SaveDataStatus::Success;

    struct statvfs Info;
    if( statvfs( SaveDataRoot().c_str(), &Info ) != 0 )
    {
        return MapError( std::error_code( errno, std::generic_category() ) );
    }

    const std::uintmax_t Available = static_cast<std::uintmax_t>( Info.f_bavail ) * Info.f_frsize;
    return Available < static_cast<std::uintmax_t>( RequiredBytes ) ? SaveDataStatus::NoSpace : SaveDataStatus::Success;
}

//=========================================================================

static
xbool SyncFile( const std::filesystem::path& Path )
{
    const s32 FileDescriptor = ::open( Path.c_str(), O_RDONLY | O_CLOEXEC );
    if( FileDescriptor < 0 )
        return FALSE;

    const s32 SyncResult  = ::fsync( FileDescriptor );
    const s32 CloseResult = ::close( FileDescriptor );
    return ( SyncResult == 0 ) && ( CloseResult == 0 );
}

//=========================================================================

static
xbool SyncDirectory( const std::filesystem::path& Path )
{
    const s32 FileDescriptor = ::open( Path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC );
    if( FileDescriptor < 0 )
        return FALSE;

    const s32 SyncResult  = ::fsync( FileDescriptor );
    const s32 CloseResult = ::close( FileDescriptor );
    return ( SyncResult == 0 ) && ( CloseResult == 0 );
}

//=========================================================================

static
void DiscardInterruptedWrites( void )
{
    std::error_code                     Error;
    std::filesystem::directory_iterator It( SaveDataTempRoot(), Error );
    if( Error )
        return;

    const std::filesystem::directory_iterator End;
    for( ; It != End; It.increment( Error ) )
    {
        if( Error )
            break;

        const std::filesystem::path TempPath = It->path();
        if( !It->is_regular_file( Error ) || Error || !HasTempSuffix( TempPath ) )
            continue;

        Error.clear();
        std::filesystem::remove( TempPath, Error );
        Error.clear();
    }
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
    const SaveDataStatus Status = EnsureRootDirectory();
    if( Status != SaveDataStatus::Success )
        return Status;

    DiscardInterruptedWrites();
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

    std::error_code                     Error;
    std::filesystem::directory_iterator It( SaveDataRoot(), Error );
    if( Error == std::errc::no_such_file_or_directory )
        return SaveDataStatus::Success;
    if( Error )
        return MapError( Error );

    const std::filesystem::directory_iterator End;
    for( ; It != End; It.increment( Error ) )
    {
        if( Error )
            return MapError( Error );

        const std::filesystem::directory_entry& Entry = *It;
        if( !Entry.is_regular_file( Error ) )
        {
            if( Error )
                return MapError( Error );
            continue;
        }

        if( HasTempSuffix( Entry.path() ) )
            continue;

        const std::uintmax_t Size = Entry.file_size( Error );
        if( Error )
            return MapError( Error );
        if( Size > static_cast<std::uintmax_t>( S32_MAX ) )
            return SaveDataStatus::IoError;

        SaveDataFileInfo& Info = Files.Append();
        Info.Name                 = Entry.path().filename().string().c_str();
        Info.Size                 = static_cast<s32>( Size );
    }

    return SaveDataStatus::Success;
}

//=========================================================================

SaveDataStatus SaveDataBackend::Read( const char* pName, xarray<u8>& Bytes )
{
    Bytes.Clear();
    if( !IsValidFileName( pName ) )
        return SaveDataStatus::IoError;

    const std::filesystem::path Path = MakePath( pName );
    std::error_code             Error;
    const std::uintmax_t        Size = std::filesystem::file_size( Path, Error );
    if( Error )
        return MapError( Error );
    if( Size > static_cast<std::uintmax_t>( S32_MAX ) )
        return SaveDataStatus::IoError;

    std::ifstream File( Path, std::ios::binary );
    if( !File )
        return SaveDataStatus::IoError;

    Bytes.SetCount( static_cast<s32>( Size ) );
    if( Size > 0 )
    {
        File.read( reinterpret_cast<char*>( Bytes.GetPtr() ), static_cast<std::streamsize>( Size ) );
        if( File.gcount() != static_cast<std::streamsize>( Size ) )
        {
            Bytes.Clear();
            return SaveDataStatus::IoError;
        }
    }

    return SaveDataStatus::Success;
}

//=========================================================================

SaveDataStatus SaveDataBackend::WriteAtomic( const char* pName, const xarray<u8>& Bytes )
{
    if( !IsValidFileName( pName ) )
        return SaveDataStatus::IoError;

    const SaveDataStatus RootStatus = EnsureRootDirectory();
    if( RootStatus != SaveDataStatus::Success )
        return RootStatus;

    const SaveDataStatus SpaceStatus = CheckAvailableSpace( Bytes.GetCount() );
    if( SpaceStatus != SaveDataStatus::Success )
        return SpaceStatus;

    const std::filesystem::path TargetPath = MakePath( pName );
    const std::filesystem::path TempPath   = MakeTempPath( pName );

    std::ofstream File( TempPath, std::ios::binary | std::ios::trunc );
    if( !File )
        return SaveDataStatus::IoError;

    if( Bytes.GetCount() > 0 )
    {
        File.write( reinterpret_cast<const char*>( Bytes.GetPtr() ), Bytes.GetCount() );
    }
    File.flush();
    xbool WriteOK = File.good();
    File.close();
    WriteOK = WriteOK && !File.fail();

    if( !WriteOK )
    {
        std::error_code Error;
        std::filesystem::remove( TempPath, Error );
        return SaveDataStatus::IoError;
    }

    if( !SyncFile( TempPath ) )
    {
        std::error_code Error;
        std::filesystem::remove( TempPath, Error );
        return SaveDataStatus::IoError;
    }

    if( std::rename( TempPath.c_str(), TargetPath.c_str() ) != 0 )
    {
        const std::error_code Error( errno, std::generic_category() );
        std::error_code       CleanupError;
        std::filesystem::remove( TempPath, CleanupError );
        return MapError( Error );
    }

    if( !SyncDirectory( SaveDataRoot() ) )
    {
        return SaveDataStatus::IoError;
    }

    return SaveDataStatus::Success;
}

//=========================================================================

SaveDataStatus SaveDataBackend::Delete( const char* pName )
{
    if( !IsValidFileName( pName ) )
        return SaveDataStatus::IoError;

    std::error_code Error;
    if( !std::filesystem::remove( MakePath( pName ), Error ) )
    {
        return Error ? MapError( Error ) : SaveDataStatus::NotFound;
    }

    return SaveDataStatus::Success;
}

//=========================================================================
