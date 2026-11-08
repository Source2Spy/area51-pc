//=========================================================================
//
//  BitseryIO.hpp
//
//=========================================================================

#ifndef BITSERY_IO_HPP
#define BITSERY_IO_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "3rdParty/Bitsery/adapter/buffer.h"
#include "3rdParty/Bitsery/traits/string.h"
#include "3rdParty/Bitsery/bitsery.h"
#include "3rdParty/Bitsery/traits/vector.h"

#include "x_files.hpp"

//=========================================================================
//  BITSERY IO
//=========================================================================

namespace bitsery_io
{

enum
{
    FILE_HEADER_SIZE = 19,
};

//------------------------------------------------------------------------

struct config : bitsery::DefaultConfig
{
    static constexpr bitsery::EndiannessType Endianness = bitsery::EndiannessType::LittleEndian;
    static constexpr bool CheckAdapterErrors            = true;
    static constexpr bool CheckDataErrors               = true;
};

//------------------------------------------------------------------------

typedef bitsery::InputBufferAdapter<const u8*, config> input_adapter;
typedef std::vector<u8>                                output_buffer;
typedef bitsery::OutputBufferAdapter<output_buffer, config> output_adapter;

//------------------------------------------------------------------------

struct file_header
{
    u8  Magic[4];
    u16 Version;
    u8  HeaderSize;
    u64 PayloadSize;
    u32 PayloadChecksum;
};

//------------------------------------------------------------------------

struct file_format
{
    u8  Magic[4];
    u16 Version;
};

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, file_header& Value )
{
    for( s32 i = 0; i < 4; i++ )
    {
        Serializer.value1b( Value.Magic[i] );
    }

    Serializer.value2b( Value.Version );
    Serializer.value1b( Value.HeaderSize );
    Serializer.value8b( Value.PayloadSize );
    Serializer.value4b( Value.PayloadChecksum );
}

//=========================================================================

template<class SERIALIZER>
void SetInvalidData( SERIALIZER& Serializer )
{
    Serializer.adapter().error( bitsery::ReaderError::InvalidData );
}

//=========================================================================

template<class SERIALIZER>
void ReadS32( SERIALIZER& Serializer, s32& Value )
{
    u32 WireValue = 0;
    Serializer.value4b( WireValue );

    if( WireValue > 0x7fffffffu )
    {
        SetInvalidData( Serializer );
        Value = 0;
        return;
    }

    Value = (s32)WireValue;
}

//=========================================================================

template<size_t SIZE, class SERIALIZER, class TYPE>
void ReadValueArray( SERIALIZER& Serializer,
                     TYPE*&     pArray,
                     s32&       Count,
                     s32        MaxCount )
{
    u32 WireCount = 0;
    Serializer.value4b( WireCount );

    if( (MaxCount < 0) || (WireCount > (u32)MaxCount) )
    {
        SetInvalidData( Serializer );
        pArray = NULL;
        Count  = 0;
        return;
    }

    Count  = (s32)WireCount;
    pArray = Count > 0 ? new TYPE[Count] : NULL;

    for( s32 i = 0; i < Count; i++ )
    {
        Serializer.template value<SIZE>( pArray[i] );
    }
}

//=========================================================================

template<class SERIALIZER, class TYPE>
void ReadObjectArray( SERIALIZER& Serializer,
                      TYPE*&     pArray,
                      s32&       Count,
                      s32        MaxCount )
{
    u32 WireCount = 0;
    Serializer.value4b( WireCount );

    if( (MaxCount < 0) || (WireCount > (u32)MaxCount) )
    {
        SetInvalidData( Serializer );
        pArray = NULL;
        Count  = 0;
        return;
    }

    Count  = (s32)WireCount;
    pArray = Count > 0 ? new TYPE[Count] : NULL;

    for( s32 i = 0; i < Count; i++ )
    {
        Serializer.object( pArray[i] );
    }
}

//=========================================================================

template<class SERIALIZER>
void ReadEmptyArray( SERIALIZER& Serializer )
{
    u32 Count = 0;
    Serializer.value4b( Count );

    if( Count != 0 )
    {
        SetInvalidData( Serializer );
    }
}

//=========================================================================

inline xbool Fail( xstring& Error, const char* pMessage )
{
    Error = pMessage ? pMessage : "";
    return( FALSE );
}

//=========================================================================

template<class TYPE>
xbool Encode( const TYPE& Value, xarray<u8>& Bytes, xstring& Error )
{
    Error.Clear();
    Bytes.Clear();

    output_buffer Buffer;
    const size_t Size = bitsery::quickSerialization(
        output_adapter( Buffer ), Value );

    if( Size > 0x7fffffffu )
    {
        return( Fail( Error, "Serialized payload is too large." ) );
    }

    Bytes.SetCount( (s32)Size );
    if( Size > 0 )
    {
        x_memcpy( Bytes.GetPtr(), Buffer.data(), (s32)Size );
    }

    return( TRUE );
}

//=========================================================================

template<class TYPE>
xbool Decode( const void* pBytes,
              s32         Size,
              TYPE&       Value,
              xstring&    Error )
{
    Error.Clear();

    if( (Size < 0) || ((Size > 0) && !pBytes) )
    {
        return( Fail( Error, "Serialized payload has an invalid buffer." ) );
    }

    const u8  Empty = 0;
    const u8* pData = pBytes ? (const u8*)pBytes : &Empty;
    const auto Result = bitsery::quickDeserialization(
        input_adapter( pData, (size_t)Size ), Value );

    if( (Result.first != bitsery::ReaderError::NoError) || !Result.second )
    {
        return( Fail( Error, "Bitsery rejected the serialized payload." ) );
    }

    return( TRUE );
}

//=========================================================================

inline u32 Checksum( const void* pBytes, s32 Size );

//=========================================================================

template<class TYPE>
xbool EncodeFile( const file_format& Format,
                  const TYPE&        Value,
                  xarray<u8>&        Bytes,
                  xstring&           Error )
{
    xarray<u8> Payload;
    if( !Encode( Value, Payload, Error ) )
    {
        return( FALSE );
    }

    file_header Header = {};
    for( s32 i = 0; i < 4; i++ )
    {
        Header.Magic[i] = Format.Magic[i];
    }
    Header.Version         = Format.Version;
    Header.HeaderSize      = FILE_HEADER_SIZE;
    Header.PayloadSize     = (u64)Payload.GetCount();
    Header.PayloadChecksum = Checksum( Payload.GetPtr(), Payload.GetCount() );

    xarray<u8> HeaderBytes;
    if( !Encode( Header, HeaderBytes, Error ) ||
        (HeaderBytes.GetCount() != FILE_HEADER_SIZE) )
    {
        return( Fail( Error, "BitseryIO produced an invalid file header." ) );
    }

    Bytes.SetCount( FILE_HEADER_SIZE + Payload.GetCount() );
    x_memcpy( Bytes.GetPtr(), HeaderBytes.GetPtr(), FILE_HEADER_SIZE );
    if( Payload.GetCount() > 0 )
    {
        x_memcpy( Bytes.GetPtr() + FILE_HEADER_SIZE,
                  Payload.GetPtr(),
                  Payload.GetCount() );
    }
    return( TRUE );
}

//=========================================================================

template<class TYPE>
xbool DecodeFile( const void*        pBytes,
                  s32                Size,
                  const file_format& Format,
                  TYPE&              Value,
                  xstring&           Error )
{
    if( !pBytes || (Size < FILE_HEADER_SIZE) )
    {
        return( Fail( Error, "Serialized file is smaller than its header." ) );
    }

    file_header Header = {};
    if( !Decode( pBytes, FILE_HEADER_SIZE, Header, Error ) )
    {
        return( Fail( Error, "Bitsery rejected the file header." ) );
    }

    for( s32 i = 0; i < 4; i++ )
    {
        if( Header.Magic[i] != Format.Magic[i] )
        {
            return( Fail( Error, "Serialized file has an unknown signature." ) );
        }
    }

    if( (Header.Version != Format.Version) ||
        (Header.HeaderSize != FILE_HEADER_SIZE) ||
        (Header.PayloadSize > 0x7fffffffu) )
    {
        return( Fail( Error, "Serialized file version or header is not supported." ) );
    }

    const s32 PayloadSize = (s32)Header.PayloadSize;
    if( PayloadSize != (Size - FILE_HEADER_SIZE) )
    {
        return( Fail( Error,
                      "Serialized payload size does not match the file size." ) );
    }

    const u8* pPayload = (const u8*)pBytes + FILE_HEADER_SIZE;
    if( Header.PayloadChecksum != Checksum( pPayload, PayloadSize ) )
    {
        return( Fail( Error,
                      "Serialized payload checksum does not match." ) );
    }

    return( Decode( pPayload, PayloadSize, Value, Error ) );
}

//=========================================================================

inline u32 Checksum( const void* pBytes, s32 Size )
{
    if( (Size <= 0) || !pBytes )
    {
        return( 0 );
    }

    const u8* pData = (const u8*)pBytes;
    u32       Crc   = 0xffffffffu;

    for( s32 i = 0; i < Size; i++ )
    {
        Crc ^= pData[i];
        for( s32 Bit = 0; Bit < 8; Bit++ )
        {
            const u32 Mask = (u32)-(s32)(Crc & 1u);
            Crc = (Crc >> 1) ^ (0xedb88320u & Mask);
        }
    }

    return( Crc ^ 0xffffffffu );
}

//=========================================================================

inline xbool ReadFile( X_FILE*            pFile,
                       const file_format& Format,
                       xarray<u8>&         Payload,
                       xstring&           Error )
{
    Error.Clear();
    Payload.Clear();

    if( !pFile )
    {
        return( Fail( Error, "BitseryIO received a null file." ) );
    }

    const s32 Size = x_flength( pFile );
    if( Size < FILE_HEADER_SIZE )
    {
        return( Fail( Error, "Serialized file is smaller than its header." ) );
    }

    u8 HeaderBytes[FILE_HEADER_SIZE];
    if( x_fread( HeaderBytes, 1, FILE_HEADER_SIZE, pFile ) != FILE_HEADER_SIZE )
    {
        return( Fail( Error, "Failed to read the serialized file header." ) );
    }

    file_header Header = {};
    if( !Decode( HeaderBytes, FILE_HEADER_SIZE, Header, Error ) )
    {
        return( Fail( Error, "Bitsery rejected the file header." ) );
    }

    for( s32 i = 0; i < 4; i++ )
    {
        if( Header.Magic[i] != Format.Magic[i] )
        {
            return( Fail( Error, "Serialized file has an unknown signature." ) );
        }
    }

    if( Header.Version != Format.Version )
    {
        return( Fail( Error, "Serialized file version is not supported." ) );
    }
    if( Header.HeaderSize != FILE_HEADER_SIZE )
    {
        return( Fail( Error, "Serialized file has an unsupported header size." ) );
    }
    if( Header.PayloadSize > 0x7fffffffu )
    {
        return( Fail( Error, "Serialized payload is too large." ) );
    }

    const s32 PayloadSize = (s32)Header.PayloadSize;
    if( PayloadSize != (Size - FILE_HEADER_SIZE) )
    {
        return( Fail( Error,
                      "Serialized payload size does not match the file size." ) );
    }

    Payload.SetCount( PayloadSize );
    if( (PayloadSize > 0) &&
        (x_fread( Payload.GetPtr(), 1, PayloadSize, pFile ) != PayloadSize) )
    {
        Payload.Clear();
        return( Fail( Error, "Failed to read the serialized payload." ) );
    }

    if( Header.PayloadChecksum != Checksum( Payload.GetPtr(), PayloadSize ) )
    {
        Payload.Clear();
        return( Fail( Error, "Serialized payload checksum does not match." ) );
    }

    return( TRUE );
}

//=========================================================================

inline xbool WriteFile( X_FILE*            pFile,
                        const file_format& Format,
                        const void*        pPayload,
                        s32                PayloadSize,
                        xstring&           Error )
{
    Error.Clear();

    if( !pFile )
    {
        return( Fail( Error, "BitseryIO received a null file." ) );
    }
    if( (PayloadSize < 0) || ((PayloadSize > 0) && !pPayload) )
    {
        return( Fail( Error, "Serialized payload has an invalid buffer." ) );
    }

    file_header Header = {};
    for( s32 i = 0; i < 4; i++ )
    {
        Header.Magic[i] = Format.Magic[i];
    }
    Header.Version         = Format.Version;
    Header.HeaderSize      = FILE_HEADER_SIZE;
    Header.PayloadSize     = (u64)PayloadSize;
    Header.PayloadChecksum = Checksum( pPayload, PayloadSize );

    xarray<u8> HeaderBytes;
    if( !Encode( Header, HeaderBytes, Error ) )
    {
        return( FALSE );
    }
    if( HeaderBytes.GetCount() != FILE_HEADER_SIZE )
    {
        return( Fail( Error, "BitseryIO produced an invalid file header." ) );
    }

    if( x_fwrite( HeaderBytes.GetPtr(), 1, FILE_HEADER_SIZE, pFile ) !=
        FILE_HEADER_SIZE )
    {
        return( Fail( Error, "Failed to write the serialized file header." ) );
    }
    if( (PayloadSize > 0) &&
        (x_fwrite( pPayload, 1, PayloadSize, pFile ) != PayloadSize) )
    {
        return( Fail( Error, "Failed to write the serialized payload." ) );
    }

    return( TRUE );
}

//=========================================================================

inline xbool WriteFile( X_FILE*            pFile,
                        const file_format& Format,
                        const xarray<u8>&   Payload,
                        xstring&           Error )
{
    return( WriteFile( pFile,
                       Format,
                       Payload.GetPtr(),
                       Payload.GetCount(),
                       Error ) );
}

//=========================================================================

template<class TYPE>
xbool Read( X_FILE*            pFile,
            const file_format& Format,
            TYPE&              Value,
            xstring&           Error )
{
    xarray<u8> Payload;
    if( !ReadFile( pFile, Format, Payload, Error ) )
    {
        return( FALSE );
    }

    return( Decode( Payload.GetPtr(), Payload.GetCount(), Value, Error ) );
}

//=========================================================================

template<class TYPE>
xbool Write( X_FILE*            pFile,
             const file_format& Format,
             const TYPE&        Value,
             xstring&           Error )
{
    xarray<u8> Payload;
    if( !Encode( Value, Payload, Error ) )
    {
        return( FALSE );
    }

    return( WriteFile( pFile, Format, Payload, Error ) );
}

} // namespace bitsery_io

//=========================================================================
#endif // BITSERY_IO_HPP
//=========================================================================
