//==============================================================================
//
//  ecs_block.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ecs_block.hpp"
#include "x_plus.hpp"
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

//==============================================================================
//  CONTEXT LIFETIME
//==============================================================================

ecs_block_context::ecs_block_context( void )
    : m_pCompression( NULL ),
      m_pDecompression( NULL )
{
}

//==============================================================================

ecs_block_context::~ecs_block_context( void )
{
    ZSTD_freeCCtx( static_cast<ZSTD_CCtx*>( m_pCompression ) );
    ZSTD_freeDCtx( static_cast<ZSTD_DCtx*>( m_pDecompression ) );
}

//==============================================================================
//  BLOCK COMPRESSION
//==============================================================================

u32 ecs_CompressBound( u32 DecodedSize )
{
    if( !DecodedSize || DecodedSize > ECS_BLOCK_MAX_SIZE )
    {
        return 0;
    }

    return static_cast<u32>( ZSTD_compressBound( DecodedSize ) );
}

//==============================================================================

xbool ecs_block_context::Compress( const byte* pDecoded, u32 DecodedSize, byte* pStored, u32& StoredSize )
{
    if( !pDecoded || !pStored || !DecodedSize || DecodedSize > ECS_BLOCK_MAX_SIZE || !StoredSize )
    {
        return FALSE;
    }

    if( !m_pCompression )
    {
        m_pCompression = ZSTD_createCCtx();
    }

    ZSTD_CCtx* pContext = static_cast<ZSTD_CCtx*>( m_pCompression );

    if( !pContext )
    {
        return FALSE;
    }

    usize Size = ZSTD_CCtx_setParameter( pContext, ZSTD_c_compressionLevel, 3 );

    if( !ZSTD_isError( Size ) )
    {
        Size = ZSTD_CCtx_setParameter( pContext, ZSTD_c_checksumFlag, 1 );
    }

    if( !ZSTD_isError( Size ) )
    {
        Size = ZSTD_compress2( pContext, pStored, StoredSize, pDecoded, DecodedSize );
    }

    if( ZSTD_isError( Size ) )
    {
        return FALSE;
    }

    StoredSize = static_cast<u32>( Size );

    return TRUE;
}

//==============================================================================

xbool ecs_block_context::Decode( ecs_block_codec Codec,
                                 const byte*     pStored,
                                 u32             StoredSize,
                                 byte*           pDecoded,
                                 u32             DecodedSize )
{
    if( !pStored || !pDecoded || !StoredSize || !DecodedSize || DecodedSize > ECS_BLOCK_MAX_SIZE )
    {
        return FALSE;
    }

    if( Codec == ECS_BLOCK_RAW )
    {
        if( StoredSize != DecodedSize )
        {
            return FALSE;
        }

        x_memcpy( pDecoded, pStored, DecodedSize );
        return TRUE;
    }

    if( Codec != ECS_BLOCK_ZSTD || StoredSize > ecs_CompressBound( DecodedSize ) )
    {
        return FALSE;
    }

    ZSTD_FrameHeader Header;
    const usize HeaderSize = ZSTD_getFrameHeader( &Header, pStored, StoredSize );

    if( ZSTD_isError( HeaderSize ) || HeaderSize != 0 || Header.frameType != ZSTD_frame ||
        Header.frameContentSize != DecodedSize || !Header.checksumFlag || Header.dictID ||
        Header.windowSize > ECS_BLOCK_MAX_SIZE )
    {
        return FALSE;
    }

    const usize FrameSize = ZSTD_findFrameCompressedSize( pStored, StoredSize );

    if( ZSTD_isError( FrameSize ) || FrameSize != StoredSize )
    {
        return FALSE;
    }

    if( !m_pDecompression )
    {
        m_pDecompression = ZSTD_createDCtx();
    }

    if( !m_pDecompression )
    {
        return FALSE;
    }

    const usize Size =
        ZSTD_decompressDCtx( static_cast<ZSTD_DCtx*>( m_pDecompression ), pDecoded, DecodedSize, pStored, StoredSize );

    return !ZSTD_isError( Size ) && Size == DecodedSize;
}

//==============================================================================

xbool ecs_CompressBlock( const byte* pDecoded, u32 DecodedSize, byte* pStored, u32& StoredSize )
{
    ecs_block_context Context;
    return Context.Compress( pDecoded, DecodedSize, pStored, StoredSize );
}

//==============================================================================

xbool ecs_DecodeBlock( ecs_block_codec Codec, const byte* pStored, u32 StoredSize, byte* pDecoded, u32 DecodedSize )
{
    ecs_block_context Context;
    return Context.Decode( Codec, pStored, StoredSize, pDecoded, DecodedSize );
}
