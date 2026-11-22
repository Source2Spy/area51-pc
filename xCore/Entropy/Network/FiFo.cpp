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
