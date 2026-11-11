//==============================================================================
//
//  io_dfs.cpp
//
//  DFS table parsing and lookup helpers.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "io_dfs.hpp"

#if defined( TARGET_WINDOWS )
    #include <windows.h>
#elif defined( TARGET_POSIX )
    #include <dirent.h>
    #include <sys/stat.h>
#endif

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dfs_header* dfs_InitHeaderFromRawPtr( void* pRawHeaderData, s32 Length )
{
    ASSERT( pRawHeaderData );
    ASSERT( Length >= (s32)sizeof(dfs_header) );    
    if( !pRawHeaderData || Length < (s32)sizeof(dfs_header) )
    {
        return NULL;
    }

    dfs_header* pHeader = (dfs_header*)pRawHeaderData;

    // Endian swap the header...
    pHeader->Magic              = LITTLE_ENDIAN_32( pHeader->Magic              );
    pHeader->Version            = LITTLE_ENDIAN_32( pHeader->Version            );
    pHeader->SectorSize         = LITTLE_ENDIAN_32( pHeader->SectorSize         );
    pHeader->SplitSize          = LITTLE_ENDIAN_32( pHeader->SplitSize          );
    pHeader->nFiles             = LITTLE_ENDIAN_32( pHeader->nFiles             );
    pHeader->nSubFiles          = LITTLE_ENDIAN_32( pHeader->nSubFiles          );
    pHeader->StringsLength      = LITTLE_ENDIAN_32( pHeader->StringsLength      );
    pHeader->SubFileTableOffset = LITTLE_ENDIAN_32( pHeader->SubFileTableOffset );
    pHeader->FilesOffset        = LITTLE_ENDIAN_32( pHeader->FilesOffset        );
    pHeader->ChecksumsOffset    = LITTLE_ENDIAN_32( pHeader->ChecksumsOffset    );
    pHeader->StringsOffset      = LITTLE_ENDIAN_32( pHeader->StringsOffset      );

    // Make sure its valid!
    if( (pHeader->Magic   == DFS_MAGIC) &&
        (pHeader->Version == DFS_VERSION) )
    {
        dfs_file* pEntry;
        s32       i;

        // Validate counts.
        ASSERT( pHeader->nFiles        >= 0 );
        ASSERT( pHeader->nSubFiles     >= 0 );
        ASSERT( pHeader->StringsLength >= 0 );    
        if( pHeader->nFiles < 0 || pHeader->nSubFiles < 0 || pHeader->StringsLength < 0 )
        {
            return NULL;
        }

        usize SubFileEnd = (usize)pHeader->SubFileTableOffset + (usize)pHeader->nSubFiles * sizeof(dfs_subfile);
        usize FilesEnd   = (usize)pHeader->FilesOffset        + (usize)pHeader->nFiles    * sizeof(dfs_file);
        usize StringsEnd = (usize)pHeader->StringsOffset      + (usize)pHeader->StringsLength;

        ASSERT( SubFileEnd <= (usize)Length );
        ASSERT( FilesEnd   <= (usize)Length );
        ASSERT( StringsEnd <= (usize)Length );
        if( SubFileEnd > (usize)Length || FilesEnd > (usize)Length || StringsEnd > (usize)Length )
        {
            return NULL;
        }

        if( (pHeader->ChecksumsOffset != 0) && ((usize)pHeader->ChecksumsOffset > (usize)Length) )
        {
            return NULL;
        }

        // Byte swap the filesize / checksum index table.
        dfs_subfile* pTable = dfs_GetSubFileTable( pHeader );
        for( i=0 ; i<pHeader->nSubFiles ; i++ )
        {
            pTable[i].Offset        = LITTLE_ENDIAN_32( pTable[i].Offset );
            pTable[i].ChecksumIndex = LITTLE_ENDIAN_32( pTable[i].ChecksumIndex );
        }

        // Byte swap the file entries.
        for( i=0, pEntry=dfs_GetFiles( pHeader ) ; i<pHeader->nFiles ; i++,pEntry++ )
        {
            // Byte swap the 32-bit values.
            pEntry->FileNameOffset1 = LITTLE_ENDIAN_32( pEntry->FileNameOffset1 );
            pEntry->FileNameOffset2 = LITTLE_ENDIAN_32( pEntry->FileNameOffset2 );
            pEntry->PathNameOffset  = LITTLE_ENDIAN_32( pEntry->PathNameOffset  );
            pEntry->ExtNameOffset   = LITTLE_ENDIAN_32( pEntry->ExtNameOffset   );

            // Byte swap the 32-bit values.
            pEntry->DataOffset = LITTLE_ENDIAN_32( pEntry->DataOffset );
            pEntry->Length     = LITTLE_ENDIAN_32( pEntry->Length     );  
        }

        // Woot!
        return pHeader;
    }

    return NULL;
}

//==============================================================================

void dfs_DumpFileListing( const dfs_header* pHeader, const char* pFileName )
{
    X_FILE* f;
    f = x_fopen( pFileName, "w+t" );
    if( f )
    {
        const dfs_file* pEntry   = dfs_GetFiles( pHeader );
        const char*     pStrings = dfs_GetStrings( pHeader );

        for( s32 i=0 ; i<pHeader->nFiles ; i++, pEntry++ )
        {
            ASSERT( (u32)pEntry->PathNameOffset  < pHeader->StringsLength );
            ASSERT( (u32)pEntry->FileNameOffset1 < pHeader->StringsLength );
            ASSERT( (u32)pEntry->FileNameOffset2 < pHeader->StringsLength );
            ASSERT( (u32)pEntry->ExtNameOffset   < pHeader->StringsLength );
            if( (u32)pEntry->PathNameOffset  >= pHeader->StringsLength ||
                (u32)pEntry->FileNameOffset1 >= pHeader->StringsLength ||
                (u32)pEntry->FileNameOffset2 >= pHeader->StringsLength ||
                (u32)pEntry->ExtNameOffset   >= pHeader->StringsLength )
            {
                continue;
            }

            x_fprintf( f,"%8d\t%8d\t%8d\t%s\t%s%s\t%s\n",
                i,
                pEntry->Length,
                pEntry->DataOffset,
                pStrings + pEntry->PathNameOffset,
                pStrings + pEntry->FileNameOffset1,
                pStrings + pEntry->FileNameOffset2,
                pStrings + pEntry->ExtNameOffset);
        }

        x_fclose( f );
    }
}

//==============================================================================

void dfs_BuildFileName( const dfs_header* pHeader, s32 iFile, char* pFileName )
{
    ASSERT( (iFile>=0) && (iFile<pHeader->nFiles) );

    const dfs_file* pEntry   = &dfs_GetFiles( pHeader )[ iFile ];
    const char*     pStrings = dfs_GetStrings( pHeader );

    x_sprintf(pFileName,"%s%s%s%s",
        pStrings + pEntry->PathNameOffset,
        pStrings + pEntry->FileNameOffset1,
        pStrings + pEntry->FileNameOffset2,
        pStrings + pEntry->ExtNameOffset);
}

//==============================================================================
// DFS EMULATION
//==============================================================================

#if defined( TARGET_DESKTOP ) || defined( TARGET_MOBILE )
struct dfs_emulated_entry
{
    xstring RelPath;
    u32     Length;
    xstring Path;
    xstring Name;
    xstring Ext;
    u32     PathOffset;
    u32     Name1Offset;
    u32     ExtOffset;
};

//------------------------------------------------------------------------------

struct dfs_string_entry
{
    xstring Str;
    u32     Offset;
};
#endif

//==============================================================================

#if defined( TARGET_DESKTOP ) || defined( TARGET_MOBILE )
static 
void dfs_SplitRelativePath( const char* pRelativePath, xstring& Path, xstring& Name, xstring& Ext )
{
    const char* pSlash = x_strrchr( pRelativePath, '\\' );
    const char* pForwardSlash = x_strrchr( pRelativePath, '/' );
    const char* pBase = pRelativePath;
    Path.Clear();
    Name.Clear();
    Ext.Clear();

    if( !pSlash || (pForwardSlash && (pForwardSlash > pSlash)) )
    {
        pSlash = pForwardSlash;
    }

    if( pSlash )
    {
        s32 PathLen = (s32)(pSlash - pRelativePath) + 1;
        char Temp[X_MAX_PATH];
        x_strncpy( Temp, pRelativePath, PathLen );
        Temp[PathLen] = 0;
        Path = Temp;
        pBase = pSlash + 1;
    }

    const char* pDot = x_strrchr( pBase, '.' );
    if( pDot && (pDot != pBase) )
    {
        char NameTemp[X_MAX_PATH];
        char ExtTemp[X_MAX_PATH];
        s32 NameLen = (s32)(pDot - pBase);
        x_strncpy( NameTemp, pBase, NameLen );
        NameTemp[NameLen] = 0;
        x_sprintf( ExtTemp, ".%s", pDot + 1 );
        Name = NameTemp;
        Ext  = ExtTemp;
    }
    else
    {
        Name = pBase;
    }

#if defined( TARGET_WINDOWS )
    if( Path.GetLength() > 0 )
    {
        Path.MakeUpper();
    }
    if( Name.GetLength() > 0 )
    {
        Name.MakeUpper();
    }
    if( Ext.GetLength()  > 0 )
    {
        Ext.MakeUpper();
    }
#endif
}

//==============================================================================

static 
xbool dfs_FindOrAddString( xarray<dfs_string_entry>& Table, u64& StringsLength, const xstring& Str, u32& Offset )
{
    const s32 StringLength = Str.GetLength();

    if( StringLength == 0 )
    {
        Offset = 0;
        return TRUE;
    }

    if( StringLength < 0 )
    {
        return FALSE;
    }

    for( s32 i = 0; i < Table.GetCount(); i++ )
    {
        if( Table[i].Str == Str )
        {
            Offset = Table[i].Offset;
            return TRUE;
        }
    }

    const u64 StringBytes = (u64)StringLength + 1;
    if( (StringBytes > (u64)S32_MAX) ||
        (StringsLength > ((u64)S32_MAX - StringBytes)) )
    {
        return FALSE;
    }

    dfs_string_entry& Entry = Table.Append();
    Entry.Str    = Str;
    Entry.Offset = (u32)StringsLength;
    Offset       = Entry.Offset;
    StringsLength += StringBytes;
    return TRUE;
}

//==============================================================================

static 
xbool dfs_CollectFiles( const char* pRootPath, const char* pRelativePath, xarray<dfs_emulated_entry>& Entries, s32 Depth = 0 )
{
    ASSERT( Depth <= 64 );
    if( Depth > 64 )
    {
        return FALSE;
    }

#if defined( TARGET_WINDOWS )
    char SearchPath[X_MAX_PATH];
    WIN32_FIND_DATA FindData;
    HANDLE hFind;

    if( pRelativePath && *pRelativePath )
    {
        x_sprintf( SearchPath, "%s\\%s\\*", pRootPath, pRelativePath );
    }
    else
    {
        x_sprintf( SearchPath, "%s\\*", pRootPath );
    }

    hFind = FindFirstFile( SearchPath, &FindData );
    if( hFind == INVALID_HANDLE_VALUE )
    {
        return FALSE;
    }

    do
    {
        if( (FindData.cFileName[0] == '.') &&
            ((FindData.cFileName[1] == 0) || ((FindData.cFileName[1] == '.') && (FindData.cFileName[2] == 0))) )
        {
            continue;
        }

        if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        {
            char NextRelative[X_MAX_PATH];
            if( pRelativePath && *pRelativePath )
            {
                x_sprintf( NextRelative, "%s\\%s", pRelativePath, FindData.cFileName );
            }
            else
            {
                x_sprintf( NextRelative, "%s", FindData.cFileName );
            }

            if( !dfs_CollectFiles( pRootPath, NextRelative, Entries, Depth + 1 ) )
            {
                FindClose( hFind );
                return FALSE;
            }
        }
        else
        {
            if( FindData.nFileSizeHigh != 0 )  // Files >4 GB not supported.
            {
                ASSERTS( FALSE, "DFS emulation does not support files larger than 4 GiB" );
                FindClose( hFind );
                return FALSE;
            }

            dfs_emulated_entry& Entry = Entries.Append();
            if( pRelativePath && *pRelativePath )
            {
                Entry.RelPath = xfs( "%s\\%s", pRelativePath, FindData.cFileName );
            }
            else
            {
                Entry.RelPath = FindData.cFileName;
            }

            Entry.Length = FindData.nFileSizeLow;
            Entry.PathOffset  = 0;
            Entry.Name1Offset = 0;
            Entry.ExtOffset   = 0;
        }
    } while( FindNextFile( hFind, &FindData ) );

    FindClose( hFind );
    return TRUE;
#elif defined( TARGET_POSIX )
    char SearchPath[X_MAX_PATH];
    DIR* pDirectory;
    struct dirent* pDirectoryEntry;

    if( pRelativePath && *pRelativePath )
    {
        x_sprintf( SearchPath, "%s/%s", pRootPath, pRelativePath );
    }
    else
    {
        x_sprintf( SearchPath, "%s", pRootPath );
    }

    pDirectory = opendir( SearchPath );
    if( !pDirectory )
    {
        return FALSE;
    }

    while( (pDirectoryEntry = readdir( pDirectory )) != NULL )
    {
        const char* pName = pDirectoryEntry->d_name;
        char FullPath[X_MAX_PATH];
        struct stat FileStat;

        if( (pName[0] == '.') &&
            ((pName[1] == 0) || ((pName[1] == '.') && (pName[2] == 0))) )
        {
            continue;
        }

        if( pRelativePath && *pRelativePath )
        {
            x_sprintf( FullPath, "%s/%s/%s", pRootPath, pRelativePath, pName );
        }
        else
        {
            x_sprintf( FullPath, "%s/%s", pRootPath, pName );
        }

        // lstat keeps symlinks from turning into directory recursion loops.
        if( lstat( FullPath, &FileStat ) != 0 )
        {
            continue;
        }

        if( S_ISDIR( FileStat.st_mode ) )
        {
            char NextRelative[X_MAX_PATH];
            if( pRelativePath && *pRelativePath )
            {
                x_sprintf( NextRelative, "%s/%s", pRelativePath, pName );
            }
            else
            {
                x_sprintf( NextRelative, "%s", pName );
            }

            if( !dfs_CollectFiles( pRootPath, NextRelative, Entries, Depth + 1 ) )
            {
                closedir( pDirectory );
                return FALSE;
            }
        }
        else if( S_ISREG( FileStat.st_mode ) )
        {
            // DFS stores lengths as u32, so files larger than 4 GiB are not supported.
            if( (FileStat.st_size < 0) || ((u64)FileStat.st_size > (u64)U32_MAX) )
            {
                ASSERTS( FALSE, "DFS emulation does not support files larger than 4 GiB" );
                closedir( pDirectory );
                return FALSE;
            }

            dfs_emulated_entry& Entry = Entries.Append();
            if( pRelativePath && *pRelativePath )
            {
                Entry.RelPath = xfs( "%s/%s", pRelativePath, pName );
            }
            else
            {
                Entry.RelPath = pName;
            }

            Entry.Length      = (u32)FileStat.st_size;
            Entry.PathOffset  = 0;
            Entry.Name1Offset = 0;
            Entry.ExtOffset   = 0;
        }
    }

    closedir( pDirectory );
    return TRUE;
#endif
}
#endif

//==============================================================================

dfs_header* dfs_BuildHeaderFromDirectory( const char* pRootPath )
{
#if defined( TARGET_DESKTOP ) || defined( TARGET_MOBILE )
    xarray<dfs_emulated_entry> Entries;
    xarray<dfs_string_entry>   StringTable;
    u64 TotalDataSize = 0;
    u64 StringsLength = 1;
    s32 i;

    if( (pRootPath == NULL) || (*pRootPath == 0) )
    {
        return NULL;
    }

    if( !dfs_CollectFiles( pRootPath, "", Entries, 0 ) )
    {
        return NULL;
    }

    if( Entries.GetCount() == 0 )
    {
        return NULL;
    }

    // Split paths, cache results, and build deduplicated string table.
    for( i=0 ; i<Entries.GetCount() ; i++ )
    {
        dfs_SplitRelativePath( Entries[i].RelPath, Entries[i].Path, Entries[i].Name, Entries[i].Ext );

        if( !dfs_FindOrAddString( StringTable, StringsLength, Entries[i].Path, Entries[i].PathOffset ) ||
            !dfs_FindOrAddString( StringTable, StringsLength, Entries[i].Name, Entries[i].Name1Offset ) ||
            !dfs_FindOrAddString( StringTable, StringsLength, Entries[i].Ext,  Entries[i].ExtOffset ) )
        {
            ASSERTS( FALSE, "DFS string table exceeds the signed 32-bit DFS limit" );
            return NULL;
        }

        TotalDataSize += Entries[i].Length;

        // The sub-file offset stores an end sentinel at TotalDataSize + 1.
        if( TotalDataSize >= (u64)U32_MAX )
        {
            ASSERTS( FALSE, "DFS emulated data exceeds the 32-bit DFS offset limit" );
            return NULL;
        }
    }

    const u64 HeaderSize  = sizeof(dfs_header);
    const u64 SubFileSize = sizeof(dfs_subfile);
    const u64 FileSize    = (u64)sizeof(dfs_file) * (u64)Entries.GetCount();
    const u64 TotalSize   = HeaderSize + SubFileSize + FileSize + StringsLength;

    // x_malloc/x_memset take s32 sizes, and DFS offsets are u32 values.
    if( (StringsLength > (u64)S32_MAX) ||
        (TotalSize > (u64)S32_MAX) ||
        (TotalSize > (u64)U32_MAX) )
    {
        ASSERTS( FALSE, "DFS header exceeds the supported allocation or format limit" );
        return NULL;
    }

    const s32 BufferSize = (s32)TotalSize;
    byte* pBuffer = (byte*)x_malloc( BufferSize );
    dfs_header* pHeader;

    if( !pBuffer )
    {
        return NULL;
    }

    x_memset( pBuffer, 0, BufferSize );
    pHeader = (dfs_header*)pBuffer;

    pHeader->Magic         = DFS_MAGIC;
    pHeader->Version       = DFS_VERSION;
    pHeader->Checksum      = 0;
    pHeader->SectorSize    = 32768;
    pHeader->SplitSize     = (u32)TotalDataSize;
    pHeader->nFiles        = Entries.GetCount();
    pHeader->nSubFiles     = 1;
    pHeader->StringsLength = (s32)StringsLength;

    pHeader->SubFileTableOffset = (u32)HeaderSize;
    pHeader->FilesOffset        = (u32)(HeaderSize + SubFileSize);
    pHeader->ChecksumsOffset    = NULL;
    pHeader->StringsOffset      = (u32)(HeaderSize + SubFileSize + FileSize);

    dfs_subfile* pSubFiles = dfs_GetSubFileTable( pHeader );
    dfs_file*    pFiles    = dfs_GetFiles( pHeader );
    char*        pStrings  = dfs_GetStrings( pHeader );

    // Offset is set one past total data size as an end of data sentinel.
    pSubFiles[0].Offset        = (u32)(TotalDataSize + 1);
    pSubFiles[0].ChecksumIndex = 0;

    pStrings[0] = 0;

    // Write deduplicated string table.
    for( i=0 ; i<StringTable.GetCount() ; i++ )
    {
        const dfs_string_entry& SE = StringTable[i];
        x_memcpy( pStrings + SE.Offset, (const char*)SE.Str, SE.Str.GetLength() + 1 );
    }

    // Write file entries using cached offsets from first pass.
    u64 DataOffset = 0;
    for( i=0 ; i<Entries.GetCount() ; i++ )
    {
        dfs_file& File = pFiles[i];

        File.PathNameOffset  = Entries[i].PathOffset;
        File.FileNameOffset1 = Entries[i].Name1Offset;
        File.FileNameOffset2 = 0;
        File.ExtNameOffset   = Entries[i].ExtOffset;
        File.DataOffset      = (u32)DataOffset;
        File.Length          = Entries[i].Length;

        DataOffset += Entries[i].Length;
    }

    return pHeader;
#else
    (void)pRootPath;
    return NULL;
#endif
}
