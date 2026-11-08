//==============================================================================
//
//  ecs_package.hpp
//
//==============================================================================

#ifndef ECS_PACKAGE_HPP
#define ECS_PACKAGE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ecs_block.hpp"
#include "x_hash.hpp"
#include "x_array.hpp"
#include "x_string.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum ecs_package_constant
{
    ECS_PACKAGE_MAJOR         = 4,
    ECS_PACKAGE_ABI           = 4,
    ECS_PACKAGE_HEADER_SIZE   = 32,
    ECS_PACKAGE_CHUNK_SIZE    = 32,
    ECS_PACKAGE_COMBO_SIZE    = 32,
    ECS_PACKAGE_PROGRAM_SIZE  = 64,
    ECS_PACKAGE_BINARY_SIZE   = 64,
    ECS_PACKAGE_BLOCK_SIZE    = 32,
    ECS_PACKAGE_MAX_FILE_SIZE = 0x7fffffff,
    ECS_PACKAGE_MAX_METADATA  = 64 * 1024 * 1024,
    ECS_PACKAGE_MAX_RECORDS   = 1024 * 1024
};

//------------------------------------------------------------------------------

enum ecs_package_header_offset
{
    ECS_PACKAGE_HEADER_MAGIC        = 0,
    ECS_PACKAGE_HEADER_VERSION      = 4,
    ECS_PACKAGE_HEADER_SIZE_FIELD   = 8,
    ECS_PACKAGE_HEADER_FILE_SIZE    = 12,
    ECS_PACKAGE_HEADER_CHUNK_COUNT  = 16,
    ECS_PACKAGE_HEADER_CHUNK_STRIDE = 20,
    ECS_PACKAGE_HEADER_FLAGS        = 24,
    ECS_PACKAGE_HEADER_RESERVED     = 28
};

//------------------------------------------------------------------------------

enum ecs_package_chunk_offset
{
    ECS_PACKAGE_CHUNK_TYPE         = 0,
    ECS_PACKAGE_CHUNK_ABI          = 4,
    ECS_PACKAGE_CHUNK_FLAGS        = 8,
    ECS_PACKAGE_CHUNK_OFFSET       = 12,
    ECS_PACKAGE_CHUNK_STORED_SIZE  = 16,
    ECS_PACKAGE_CHUNK_DECODED_SIZE = 20,
    ECS_PACKAGE_CHUNK_CRC          = 24,
    ECS_PACKAGE_CHUNK_RESERVED     = 28
};

//------------------------------------------------------------------------------

enum ecs_package_program_offset
{
    ECS_PACKAGE_PROGRAM_ID           = 0,
    ECS_PACKAGE_PROGRAM_STAGE        = 8,
    ECS_PACKAGE_PROGRAM_FORMAT       = 12,
    ECS_PACKAGE_PROGRAM_ENTRY        = 16,
    ECS_PACKAGE_PROGRAM_LAYOUT       = 20,
    ECS_PACKAGE_PROGRAM_BINARY       = 24,
    ECS_PACKAGE_PROGRAM_NAME         = 28,
    ECS_PACKAGE_PROGRAM_CAPABILITIES = 32,
    ECS_PACKAGE_PROGRAM_RESERVED     = 36
};

//------------------------------------------------------------------------------

enum ecs_package_binary_offset
{
    ECS_PACKAGE_BINARY_CONTENT    = 0,
    ECS_PACKAGE_BINARY_FORMAT     = 32,
    ECS_PACKAGE_BINARY_BLOCK      = 36,
    ECS_PACKAGE_BINARY_OFFSET     = 40,
    ECS_PACKAGE_BINARY_SIZE_FIELD = 44,
    ECS_PACKAGE_BINARY_CRC        = 48,
    ECS_PACKAGE_BINARY_RESERVED   = 52
};

//------------------------------------------------------------------------------

enum ecs_shader_capability
{
    ECS_SHADER_CAPABILITY_FLOAT16 = 1u << 0
};

//==============================================================================
//  STRUCTURES
//==============================================================================

struct ecs_package_binding
{
    u32     Kind     = 0;
    u32     Slot     = 0;
    u32     Count    = 0;
    u32     ByteSize = 0;
    xstring Name;
};

//------------------------------------------------------------------------------

struct ecs_package_member
{
    xstring Buffer;
    xstring Name;
    u32     Offset       = 0;
    u32     Size         = 0;
    u32     ArrayStride  = 0;
    u32     MatrixStride = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_variable
{
    xstring Semantic;
    u32     Index         = 0;
    u32     Components    = 0;
    u32     ComponentType = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_layout
{
    u32                          InputInterface  = 0;
    u32                          OutputInterface = 0;
    u32                          Resources[4]    = {};
    xarray<ecs_package_binding>  Bindings;
    xarray<ecs_package_member>   Members;
    xarray<ecs_package_variable> Inputs;
    xarray<ecs_package_variable> Outputs;
};

//------------------------------------------------------------------------------

struct ecs_package_program
{
    u64     Id     = 0;
    u32     Stage  = 0;
    u32     Format = 0;
    xstring Name;
    xstring Entry;
    u32     Layout               = 0;
    u32     Binary               = 0;
    u32     RequiredCapabilities = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_combo
{
    u64 Key      = 0;
    u64 Vertex   = 0;
    u64 Pixel    = 0;
    u32 Kind     = 0;
    u32 Policies = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_binary
{
    x_content_id Content = {};
    u32            Format  = 0;
    u32            Block   = 0;
    u32            Offset  = 0;
    u32            Size    = 0;
    u32            CRC     = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_block
{
    u32             Offset      = 0;
    u32             StoredSize  = 0;
    u32             DecodedSize = 0;
    ecs_block_codec Codec       = ECS_BLOCK_RAW;
    u32             StoredCRC   = 0;
    u32             DecodedCRC  = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_index
{
    u64                         CatalogId  = 0;
    u64                         PackageId  = 0;
    u32                         TargetMask = 0;
    xstring                     Name;
    xarray<ecs_package_combo>   Combos;
    xarray<ecs_package_program> Programs;
    xarray<ecs_package_layout>  Layouts;
    xarray<ecs_package_binary>  Binaries;
    xarray<ecs_package_block>   Blocks;
    xarray<u32>                 BlockBinaryOffsets;
    xarray<u32>                 BlockBinaries;
    u32                         CodeOffset = 0;
    u32                         CodeSize   = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_build_options
{
    u32   WorkerCount                                                                       = 1;
    u64   TemporaryBudget                                                                   = 256ull * 1024 * 1024;
    void* pContext                                                                          = NULL;
    xbool ( *LoadBlock )( void*, const x_content_id&, ecs_package_block&, xarray<byte>& ) = NULL;
    xbool ( *SaveBlock )( void*, const x_content_id&, const ecs_package_block&, const xarray<byte>& ) = NULL;
    void  ( *Progress )( void*, u32 Completed, u32 Total )                                              = NULL;
    u32   CachedBlocks                                                                                  = 0;
    u32   ZstdBlocks                                                                                    = 0;
    u32   RawBlocks                                                                                     = 0;
    u32   CompressedBlocks                                                                              = 0;
    u64   PeakTemporaryBytes                                                                            = 0;
    f64   PreparationMs                                                                                 = 0;
    f64   CompressionMs                                                                                 = 0;
    f64   SerializationMs                                                                               = 0;
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

// The caller owns the seekable file and provides its physical size.
using ecs_package_read = xbool ( * )( void* pContext, u32 Offset, byte* pData, u32 Size );

// Canonicalizes the owned index in place; bytecode remains owned by the caller.
xbool ecs_BuildPackage( ecs_package_index&                 Index,
                        const xarray<const xarray<byte>*>& Bytecode,
                        xarray<byte>&                      File,
                        xstring&                           Error,
                        ecs_package_build_options*         pOptions = NULL );
xbool ecs_DecodePackageBlock( ecs_block_context&       Context,
                              const ecs_package_index& Index,
                              u32                      Block,
                              const byte*              pStored,
                              u32                      StoredSize,
                              xarray<byte>&            Decoded,
                              xstring&                 Error );
xbool ecs_ReadPackageIndex( ecs_package_read   Read,
                            void*              pContext,
                            u32                FileSize,
                            u64                ExpectedCatalog,
                            ecs_package_index& Index,
                            xstring&           Error );
xbool ecs_ReadPackageBlock( ecs_package_read         Read,
                            void*                    pContext,
                            const ecs_package_index& Index,
                            u32                      Block,
                            xarray<byte>&            Decoded,
                            xstring&                 Error );
const ecs_package_combo*   ecs_FindPackageCombo( const ecs_package_index& Index, u32 Kind, u64 Key );
const ecs_package_program* ecs_FindPackageProgram( const ecs_package_index& Index,
                                                   u64                      Id,
                                                   u32                      Format,
                                                   u32                      Capabilities = 0 );

//==============================================================================
#endif // ECS_PACKAGE_HPP
//==============================================================================
