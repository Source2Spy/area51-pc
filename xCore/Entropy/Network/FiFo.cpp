//=============================================================================
//
//  FiFo.cpp
//
//=============================================================================

//=============================================================================
//  INCLUDES
//=============================================================================

#include "x_files.hpp"
#include "FiFo.hpp"

//==============================================================================

fifo::fifo( void )
{
    m_Initialized = FALSE;
}

//==============================================================================

fifo::~fifo( void )
{
    ASSERT( m_Initialized==FALSE );
}

//==============================================================================

void fifo::Init( void* pBuffer, s32 Length )
{
    ASSERT( !m_Initialized );
    m_Initialized   = TRUE;
    m_pData         = (byte*)pBuffer;
    m_Length        = Length;
    m_WriteIndex    = 0;
    m_ReadIndex     = 0;
    m_ValidBytes    = 0;
}

//==============================================================================

void fifo::Kill(void)
{
    ASSERT( m_Initialized );
    m_Initialized = FALSE;
}

//==============================================================================

void fifo::Delete( s32 Amount )
{
    ASSERT( m_ValidBytes >= Amount );
    m_ReadIndex += Amount;
    if( m_ReadIndex > m_Length )
    {
        m_ReadIndex -= m_Length;
    }
    m_ValidBytes -= Amount;
}

//==============================================================================

xbool fifo::Remove( void* pBuffer, s32 nBytes, s32 Modulo )
{
    (void)Modulo;

    s32 BytesToCopy;
    byte* pDest;

    (void)Modulo;
    
    ASSERT( Modulo > 0 );

    pDest = (byte*)pBuffer;

    if( m_ValidBytes < nBytes )
    {
        return FALSE;
    }

    while( nBytes )
    {
        BytesToCopy = MIN( nBytes,m_Length - m_ReadIndex );

        x_memcpy( pDest, m_pData+m_ReadIndex, BytesToCopy );
        pDest       += BytesToCopy;
        m_ReadIndex += BytesToCopy;
        m_ValidBytes-= BytesToCopy;
        nBytes      -= BytesToCopy;
        if( m_ReadIndex >= m_Length )
        {
            m_ReadIndex = 0;
        }

    }
    return TRUE;
}

//==============================================================================

xbool fifo::Insert( const void* pBuffer, s32 Length, s32 Modulo )
{
    s32 BytesToCopy;
    const byte* pSource;

    ASSERT( Modulo > 0 );
    while( GetBytesFree() < Length )
    {
        Delete( Modulo );
    }
    pSource = (const byte*)pBuffer;

    while( Length )
    {
        BytesToCopy = MIN(Length,m_Length - m_WriteIndex);

        x_memcpy(m_pData+m_WriteIndex,pSource,BytesToCopy);
        pSource     += BytesToCopy;
        m_WriteIndex+= BytesToCopy;
        m_ValidBytes+= BytesToCopy;
        Length      -= BytesToCopy;
        if( m_WriteIndex >= m_Length )
        {
            m_WriteIndex = 0;
        }
    }
    return TRUE;
}

//==============================================================================

s32 fifo::GetBytesFree(void)
{
    return m_Length - m_ValidBytes;
}

//==============================================================================

s32     fifo::GetBytesUsed(void)
{
    return m_ValidBytes;
}

//==============================================================================

void    fifo::Clear(void)
{
    m_WriteIndex = 0;
    m_ReadIndex  = 0;
    m_ValidBytes = 0;

}

//==============================================================================

byte*   fifo::GetData(void)
{
    return m_pData;
}

//==============================================================================

void fifo::ProvideUpdate( netstream& BitStream, s32 MaxLength, s32 Modulo )
{
    char    Buffer[256];
    s32     BytesAvailableInStream;
    s32     BytesAvailableInFifo;
    s32     Length;
    s32     TrailerBits = 8;

#if defined(X_DEBUG)
    TrailerBits += 32;
#endif

    BitStream.WriteMarker();

    ASSERT( Modulo > 0 );
    MaxLength = MIN( MaxLength, (s32)sizeof(Buffer) - 1 );

    MaxLength = MaxLength - ( MaxLength % Modulo );
    BytesAvailableInStream = MAX( 0, (BitStream.GetNBitsFree() - TrailerBits) / 8 );
    BytesAvailableInStream = MIN( BytesAvailableInStream, MaxLength );
    BytesAvailableInFifo   = GetBytesUsed();

    // Get maximum space we'd want in the buffer
    Length = MIN( BytesAvailableInFifo, BytesAvailableInStream );
    Length -= Length % Modulo;
    BitStream.WriteRangedS32(Length,0,255);

    if( Length )
    {
        LOG_MESSAGE("fifo::ProvideUpdate","Removed %d bytes from fifo",Length);
        Remove(Buffer, Length, Modulo);
        BitStream.WriteBits(Buffer,Length*8);
    }

    BitStream.WriteMarker();
}

//==============================================================================

xbool fifo::ReadUpdate( netstream& BitStream, update& Update, s32 Modulo ) const
{
    BitStream.ReadMarker();
    BitStream.ReadRangedS32( Update.Length, 0, sizeof(Update.Data) );

    if( BitStream.ReadError() )
    {
        return FALSE;
    }

    if( Update.Length && ((Modulo <= 0) || (Update.Length > m_Length) ||
                          (Modulo > m_Length) || (Update.Length % Modulo)) )
    {
        BitStream.SetReadError();
        return FALSE;
    }

    BitStream.ReadBits( Update.Data, Update.Length * 8 );
    BitStream.ReadMarker();

    return !BitStream.ReadError();
}

//==============================================================================

void fifo::AcceptUpdate( const update& Update, s32 Modulo )
{
    if( Update.Length )
    {
        Insert( Update.Data, Update.Length, Modulo );
    }
}
