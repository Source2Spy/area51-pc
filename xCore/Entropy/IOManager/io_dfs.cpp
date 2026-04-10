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

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

dfs_header* dfs_InitHeaderFromRawPtr( void* pRawHeaderData, s32 Length )
{
    ASSERT( pRawHeaderData );
    ASSERT( Length >= (s32)sizeof(dfs_header) );	
    if( !pRawHeaderData || Length < (s32)sizeof(dfs_header) )
        return NULL;

    dfs_header* pHeader = (dfs_header*)pRawHeaderData;

    // Endian swap the header...
    pHeader->Magic          = LITTLE_ENDIAN_32( pHeader->Magic          );
    pHeader->Version        = LITTLE_ENDIAN_32( pHeader->Version        );
    pHeader->SectorSize     = LITTLE_ENDIAN_32( pHeader->SectorSize     );
    pHeader->SplitSize      = LITTLE_ENDIAN_32( pHeader->SplitSize      );
    pHeader->nFiles         = LITTLE_ENDIAN_32( pHeader->nFiles         );
    pHeader->nSubFiles      = LITTLE_ENDIAN_32( pHeader->nSubFiles      );
    pHeader->StringsLength  = LITTLE_ENDIAN_32( pHeader->StringsLength  );
    pHeader->pSubFileTable  = (dfs_subfile*)LITTLE_ENDIAN_32( pHeader->pSubFileTable  );
    pHeader->pFiles         = (dfs_file*)   LITTLE_ENDIAN_32( pHeader->pFiles         );     
    pHeader->pChecksums     = (u16*)        LITTLE_ENDIAN_32( pHeader->pChecksums     );     
    pHeader->pStrings       = (char*)       LITTLE_ENDIAN_32( pHeader->pStrings       );

    // Make sure its valid!
    if( (pHeader->Version == DFS_VERSION) )
    {
        dfs_file* pEntry;
        s32       i;

        // Validate counts.
        ASSERT( pHeader->nFiles        >= 0 );
        ASSERT( pHeader->nSubFiles     >= 0 );
        ASSERT( pHeader->StringsLength >= 0 );	
        if( pHeader->nFiles < 0 || pHeader->nSubFiles < 0 || pHeader->StringsLength < 0 )
            return NULL;

        u32 SubFileEnd = (u32)pHeader->pSubFileTable + (u32)pHeader->nSubFiles * sizeof(dfs_subfile);
        u32 FilesEnd   = (u32)pHeader->pFiles        + (u32)pHeader->nFiles    * sizeof(dfs_file);
        u32 StringsEnd = (u32)pHeader->pStrings      + (u32)pHeader->StringsLength;

        ASSERT( SubFileEnd <= (u32)Length );
        ASSERT( FilesEnd   <= (u32)Length );
        ASSERT( StringsEnd <= (u32)Length );
        if( SubFileEnd > (u32)Length || FilesEnd > (u32)Length || StringsEnd > (u32)Length )
            return NULL;

        // Now fixup the offsets in the header.
        u32 BaseAddr = (u32)pHeader;
        pHeader->pSubFileTable  = (dfs_subfile*)(BaseAddr + (u32)pHeader->pSubFileTable  );
        pHeader->pFiles         = (dfs_file*)   (BaseAddr + (u32)pHeader->pFiles         );
        if( pHeader->pChecksums )
        {
            pHeader->pChecksums = (u16*)        (BaseAddr + (u32)pHeader->pChecksums     );
        }
        pHeader->pStrings       = (char*)       (BaseAddr + (u32)pHeader->pStrings       );

        // Byte swap the filesize / checksum index table.
        dfs_subfile* pTable = pHeader->pSubFileTable;
        for( i=0 ; i<pHeader->nSubFiles ; i++ )
        {
            pTable[i].Offset        = LITTLE_ENDIAN_32( pTable[i].Offset );
            pTable[i].ChecksumIndex = LITTLE_ENDIAN_32( pTable[i].ChecksumIndex );
        }

        // Byte swap the file entries.
        for( i=0, pEntry=pHeader->pFiles ; i<pHeader->nFiles ; i++,pEntry++ )
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
        dfs_file*   pEntry  = pHeader->pFiles;

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
                continue;
        
            x_fprintf( f,"%8d\t%8d\t%8d\t%s\t%s%s\t%s\n",
                i,
                pEntry->Length,
                pEntry->DataOffset,
                pHeader->pStrings + (u32)pEntry->PathNameOffset,
                pHeader->pStrings + (u32)pEntry->FileNameOffset1,
                pHeader->pStrings + (u32)pEntry->FileNameOffset2,
                pHeader->pStrings + (u32)pEntry->ExtNameOffset);
        }

        x_fclose( f );
    }
}

//==============================================================================

void dfs_BuildFileName( const dfs_header* pHeader, s32 iFile, char* pFileName )
{
    ASSERT( (iFile>=0) && (iFile<pHeader->nFiles) );

    dfs_file*   pEntry  = &pHeader->pFiles[ iFile ];

    x_sprintf(pFileName,"%s%s%s%s",
        pHeader->pStrings + (u32)pEntry->PathNameOffset,
        pHeader->pStrings + (u32)pEntry->FileNameOffset1,
        pHeader->pStrings + (u32)pEntry->FileNameOffset2,
        pHeader->pStrings + (u32)pEntry->ExtNameOffset);
}

//==============================================================================
// DFS EMULATION
//==============================================================================

#ifdef TARGET_PC
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

#ifdef TARGET_PC
static 
void dfs_SplitRelativePath( const char* pRelativePath, xstring& Path, xstring& Name, xstring& Ext )
{
    const char* pSlash = x_strrchr( pRelativePath, '\\' );
    const char* pBase = pRelativePath;
    Path.Clear();
    Name.Clear();
    Ext.Clear();

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

    if( Path.GetLength() > 0 ) Path.MakeUpper();
    if( Name.GetLength() > 0 ) Name.MakeUpper();
    if( Ext.GetLength()  > 0 ) Ext.MakeUpper();
}
#endif

//==============================================================================

#ifdef TARGET_PC

static 
u32 dfs_FindOrAddString( xarray<dfs_string_entry>& Table, u32& StringsLength, const xstring& Str )
{
    if( Str.GetLength() == 0 )
        return 0;

    for( s32 i = 0; i < Table.GetCount(); i++ )
    {
        if( Table[i].Str == Str )
            return Table[i].Offset;
    }

    dfs_string_entry& Entry = Table.Append();
    Entry.Str    = Str;
    Entry.Offset = StringsLength;
    StringsLength += (u32)Str.GetLength() + 1;
    return Entry.Offset;
}
#endif

//==============================================================================

#ifdef TARGET_PC
static 
void dfs_CollectFiles( const char* pRootPath, const char* pRelativePath, xarray<dfs_emulated_entry>& Entries, s32 Depth = 0 )
{
    ASSERT( Depth <= 64 );
    if( Depth > 64 )
        return;
	
    char SearchPath[X_MAX_PATH];
    WIN32_FIND_DATA FindData;
    HANDLE hFind;

    if( pRelativePath && *pRelativePath )
        x_sprintf( SearchPath, "%s\\%s\\*", pRootPath, pRelativePath );
    else
        x_sprintf( SearchPath, "%s\\*", pRootPath );

    hFind = FindFirstFile( SearchPath, &FindData );
    if( hFind == INVALID_HANDLE_VALUE )
        return;

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
                x_sprintf( NextRelative, "%s\\%s", pRelativePath, FindData.cFileName );
            else
                x_sprintf( NextRelative, "%s", FindData.cFileName );

            dfs_CollectFiles( pRootPath, NextRelative, Entries, Depth + 1 );
        }
        else
        {
            dfs_emulated_entry& Entry = Entries.Append();
            if( pRelativePath && *pRelativePath )
                Entry.RelPath = xfs( "%s\\%s", pRelativePath, FindData.cFileName );
            else
                Entry.RelPath = FindData.cFileName;

            ASSERT( FindData.nFileSizeHigh == 0 );  // Files >4 GB not supported.
            Entry.Length = FindData.nFileSizeLow;
            Entry.PathOffset  = 0;
            Entry.Name1Offset = 0;
            Entry.ExtOffset   = 0;
        }
    } while( FindNextFile( hFind, &FindData ) );

    FindClose( hFind );
}
#endif

//==============================================================================

dfs_header* dfs_BuildHeaderFromDirectory( const char* pRootPath )
{
#ifdef TARGET_PC
    xarray<dfs_emulated_entry> Entries;
    xarray<dfs_string_entry>   StringTable;
    u32 TotalDataSize = 0;
    u32 StringsLength = 1;
    s32 i;

    if( (pRootPath == NULL) || (*pRootPath == 0) )
        return NULL;

    dfs_CollectFiles( pRootPath, "", Entries, 0 );

    if( Entries.GetCount() == 0 )
        return NULL;

    // Split paths, cache results, and build deduplicated string table.
    for( i=0 ; i<Entries.GetCount() ; i++ )
    {
        dfs_SplitRelativePath( Entries[i].RelPath, Entries[i].Path, Entries[i].Name, Entries[i].Ext );

        Entries[i].PathOffset  = dfs_FindOrAddString( StringTable, StringsLength, Entries[i].Path );
        Entries[i].Name1Offset = dfs_FindOrAddString( StringTable, StringsLength, Entries[i].Name );
        Entries[i].ExtOffset   = dfs_FindOrAddString( StringTable, StringsLength, Entries[i].Ext  );

        TotalDataSize += Entries[i].Length;
    }

    u32 HeaderSize   = sizeof(dfs_header);
    u32 SubFileSize  = sizeof(dfs_subfile);
    u32 FileSize     = sizeof(dfs_file) * Entries.GetCount();
    u32 TotalSize    = HeaderSize + SubFileSize + FileSize + StringsLength;
    byte* pBuffer    = (byte*)x_malloc( TotalSize );
    dfs_header* pHeader;

    if( !pBuffer )
        return NULL;

    x_memset( pBuffer, 0, TotalSize );
    pHeader = (dfs_header*)pBuffer;

    pHeader->Magic         = DFS_MAGIC;
    pHeader->Version       = DFS_VERSION;
    pHeader->Checksum      = 0;
    pHeader->SectorSize    = 32768;
    pHeader->SplitSize     = TotalDataSize;
    pHeader->nFiles        = Entries.GetCount();
    pHeader->nSubFiles     = 1;
    pHeader->StringsLength = StringsLength;

    pHeader->pSubFileTable = (dfs_subfile*)(pBuffer + HeaderSize);
    pHeader->pFiles        = (dfs_file*)  (pBuffer + HeaderSize + SubFileSize);
    pHeader->pChecksums    = NULL;
    pHeader->pStrings      = (char*)     (pBuffer + HeaderSize + SubFileSize + FileSize);

    // Offset is set one past total data size as an end of data sentinel.
    pHeader->pSubFileTable[0].Offset        = TotalDataSize + 1;
    pHeader->pSubFileTable[0].ChecksumIndex = 0;

    pHeader->pStrings[0] = 0;

    // Write deduplicated string table.
    for( i=0 ; i<StringTable.GetCount() ; i++ )
    {
        const dfs_string_entry& SE = StringTable[i];
        x_memcpy( pHeader->pStrings + SE.Offset, (const char*)SE.Str, SE.Str.GetLength() + 1 );
    }

    // Write file entries using cached offsets from first pass.
    u32 DataOffset = 0;
    for( i=0 ; i<Entries.GetCount() ; i++ )
    {
        dfs_file& File = pHeader->pFiles[i];

        File.PathNameOffset  = Entries[i].PathOffset;
        File.FileNameOffset1 = Entries[i].Name1Offset;
        File.FileNameOffset2 = 0;
        File.ExtNameOffset   = Entries[i].ExtOffset;
        File.DataOffset      = DataOffset;
        File.Length          = Entries[i].Length;

        DataOffset += Entries[i].Length;
    }

    return pHeader;
#else
    (void)pRootPath;
    return NULL;
#endif
}