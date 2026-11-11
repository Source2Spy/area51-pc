//==============================================================================
//
//  io_dfs.hpp
//
//  DFS table parsing and lookup helpers.
//
//==============================================================================

#ifndef IO_DFS_HPP
#define IO_DFS_HPP

//==============================================================================
//  STRUCTS AND DEFINES
//==============================================================================

#include "x_types.hpp"

#define DFS_MAGIC   'XDFS'
#define DFS_VERSION 3

struct dfs_file
{
    u32         FileNameOffset1;    // Offset of filename part 1 in string table
    u32         FileNameOffset2;    // Offset of filename part 2 in string table
    u32         PathNameOffset;     // Offset of pathname in string table
    u32         ExtNameOffset;      // Offset of extension in string table
    u32         DataOffset;         // Offset of data
    u32         Length;             // Length of file
};

//-------------------------------------------------------------------------------

struct dfs_subfile
{
    u32         Offset;
    u32         ChecksumIndex;
};

//-------------------------------------------------------------------------------

struct dfs_header
{
    u32             Magic;              // Magic number to identify file
    s32             Version;            // Version number of file
    u32             Checksum;           // .DFS file checksum
    s32             SectorSize;         // Sector size in bytes
    u32             SplitSize;          // Split size in bytes (maximum)
    s32             nFiles;             // Total number of files in the filesystem
    s32             nSubFiles;          // Number of sub files (*.000, *.001, etc...)
    s32             StringsLength;      // Length of string table in bytes
    u32             SubFileTableOffset; // Offset to the sub file table
    u32             FilesOffset;        // Offset to file entries
    u32             ChecksumsOffset;    // Offset to checksums
    u32             StringsOffset;      // Offset to the string table
};

static_assert( sizeof(dfs_header) == 48, "DFS v3 header layout must remain 48 bytes" );

//==============================================================================
//  FUNCTIONS
//==============================================================================

dfs_header* dfs_InitHeaderFromRawPtr    ( void* pRawHeaderData, s32 Length  );
dfs_header* dfs_BuildHeaderFromDirectory( const char* pRootPath );
void        dfs_DumpFileListing         ( const dfs_header* pHeader, const char* pFileName );
void        dfs_BuildFileName           ( const dfs_header* pHeader, s32 iFile, char* pFileName );

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline 
dfs_subfile* dfs_GetSubFileTable( dfs_header* pHeader )
{
    if( pHeader->SubFileTableOffset == 0 )
    {
        return NULL;
    }
    return (dfs_subfile*)((byte*)pHeader + pHeader->SubFileTableOffset);
}

//==============================================================================

inline 
const dfs_subfile* dfs_GetSubFileTable( const dfs_header* pHeader )
{
    if( pHeader->SubFileTableOffset == 0 )
    {
        return NULL;
    }
    return (const dfs_subfile*)((const byte*)pHeader + pHeader->SubFileTableOffset);
}

//==============================================================================

inline 
dfs_file* dfs_GetFiles( dfs_header* pHeader )
{
    if( pHeader->FilesOffset == 0 )
    {
        return NULL;
    }
    return (dfs_file*)((byte*)pHeader + pHeader->FilesOffset);
}

//==============================================================================

inline 
const dfs_file* dfs_GetFiles( const dfs_header* pHeader )
{
    if( pHeader->FilesOffset == 0 )
    {
        return NULL;
    }
    return (const dfs_file*)((const byte*)pHeader + pHeader->FilesOffset);
}

//==============================================================================

inline 
u16* dfs_GetChecksums( dfs_header* pHeader )
{
    if( pHeader->ChecksumsOffset == 0 )
    {
        return NULL;
    }
    return (u16*)((byte*)pHeader + pHeader->ChecksumsOffset);
}

//==============================================================================

inline 
const u16* dfs_GetChecksums( const dfs_header* pHeader )
{
    if( pHeader->ChecksumsOffset == 0 )
    {
        return NULL;
    }
    return (const u16*)((const byte*)pHeader + pHeader->ChecksumsOffset);
}

//==============================================================================

inline 
char* dfs_GetStrings( dfs_header* pHeader )
{
    if( pHeader->StringsOffset == 0 )
    {
        return NULL;
    }
    return (char*)((byte*)pHeader + pHeader->StringsOffset);
}

//==============================================================================

inline 
const char* dfs_GetStrings( const dfs_header* pHeader )
{
    if( pHeader->StringsOffset == 0 )
    {
        return NULL;
    }
    return (const char*)((const byte*)pHeader + pHeader->StringsOffset);
}

//==============================================================================
#endif // IO_DFS_HPP
//==============================================================================
