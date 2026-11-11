//==============================================================================
//
//  FiFo.hpp
//
//==============================================================================

#ifndef FIFO_HPP
#define FIFO_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "Network/NetStream.hpp"

//==============================================================================

class fifo
{
public:
    struct update
    {
        byte    Data[255];
        s32     Length;
    };

                        fifo                ( void );
                       ~fifo                ( void );
        void            Init                ( void* pBuffer, s32 Length );
        void            Kill                ( void );
        void            Delete              ( s32 nBytes );
        xbool           Remove              ( void* pBuffer, s32 nBytes, s32 Modulo );          // Remove nBytes from the fifo, return TRUE if ok
        xbool           Insert              ( const void* pBuffer, s32 Length, s32 Modulo );    // Insert 'Length' in to the fifo, return TRUE if enough space
        s32             GetBytesUsed        ( void );                                           // Number of bytes written so far
        s32             GetBytesFree        ( void );                                           // Number of bytes available for write

        byte*           GetData             ( void );                                           // Return ptr to base of data structure
        void            Clear               ( void );
        void            ProvideUpdate       ( netstream& BitStream, s32 MaxLength=255,s32 Modulo = 0 );
        xbool           ReadUpdate          ( netstream& BitStream, update& Update, s32 Modulo ) const;
        void            AcceptUpdate        ( const update& Update, s32 Modulo );

private:
        xbool           m_Initialized;
        byte*           m_pData;
        s32             m_Length;

        s32             m_WriteIndex;
        s32             m_ReadIndex;

        s32             m_ValidBytes;

};

//==============================================================================
#endif // FIFO_HPP
//==============================================================================
