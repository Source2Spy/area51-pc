//==============================================================================
//
//  Load_TGA.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "aux_Bitmap.hpp"
#include "x_debug.hpp"
#include "x_memory.hpp"

#ifdef EXIT_FAILURE 
#undef EXIT_FAILURE
#endif

//==============================================================================
//  TYPES
//==============================================================================

// 
//  This structure does not match the layout of the header data in a TGA file.
//  It simply has fields for all of the data members needed for this simple
//  loader.
//

struct tga_info
{                                                           
    s32     IDLength;       // Image ID Field Length        
    s32     CMapType;       // Color Map Type               
    s32     ImageType;      // Image Type                   
    s32     CMapLength;     // Color Map Length             
    s32     CMapEntrySize;  // Color Map Entry Size         
    s32     Width;          // Image Width                  
    s32     Height;         // Image Height                 
    s32     PixelDepth;     // Pixel Depth                  
    s32     ImageDesc;      // Image Descriptor             
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

static
xbool tga_ReadEncodedData( byte*    pData, 
                           X_FILE*  pFile, 
                           s32      NPixels, 
                           s32      BytesPerPixel )
{
    xbool   Success    = FALSE;
    s32     PixelsRead = 0;
    s32     BytesRead;
    u8      RunCount;

    // Until we get all the pixels...
    while( PixelsRead < NPixels )
    {
        // Get the pixel run count.
        BytesRead = x_fread( &RunCount, 1, 1, pFile );
        if( BytesRead != 1 )
            goto EXIT;

        // Error checking.
        if( (PixelsRead + (RunCount & 0x7F) + 1) > NPixels )
            goto EXIT;

        // Encoded or not?
        if( RunCount & 0x80 )
        {
            // Encoded! (Simple RLE)

            // Mask off the flag bit.
            RunCount &= 0x7F;

            // Read in the first pixel.
            BytesRead = x_fread( pData, 1, BytesPerPixel, pFile );
            if( BytesRead != BytesPerPixel )
                goto EXIT;

            // Update this counter now, since we will alter RunCount.
            PixelsRead += (RunCount + 1);

            // Replicate this pixel to fill the run.
            {
                byte* pPixel = pData;
                s32   i, j;

                // Skip the data pointer past the reference pixel.
                pData += BytesPerPixel;

                // Copy reference pixel to subsequent pixels.
                for( j = 0; j < RunCount;      j++ )
                for( i = 0; i < BytesPerPixel; i++ )
                {
                    *pData++ = pPixel[i];
                }
            }
        }
        else
        {
            // Not encoded!

            // The number of pixels present is RunCount + 1.
            // Just increment it here to make following code clearer.
            // (It is safe to add one to this u8 since top bit is clear.)
            RunCount++;

            // Read the pixel data.
            BytesRead = x_fread( pData, 1, RunCount * BytesPerPixel, pFile );
            if( BytesRead != RunCount * BytesPerPixel )
                goto EXIT;

            // Advance the data pointer.
            pData += RunCount * BytesPerPixel;

            // Update counter.
            PixelsRead += RunCount;
        }   
    }

    // Success!
    Success = TRUE;
    goto EXIT;

    // Off-ramps.
    EXIT:   return( Success );
}

//==============================================================================

xbool auxbmp_LoadTGA( xbitmap& Bitmap, X_FILE* pFile )
{
    tga_info    Info;
    byte        Buffer[18];
    byte*       pData = NULL;
    s32         NPixels;
    s32         BytesPerPixel;
    s32         DataSize;
    s32         BytesRead;
    s32         BytesToSkip;
    s32         MinimumSize;
    s32         Position;
    s32         Length;

    xbitmap::format Format = xbitmap::FMT_NULL;

    ASSERT( pFile );

    // Read the header data into the buffer.
    BytesRead = x_fread( &Buffer, 1, 18, pFile );
    if( BytesRead != 18 )
        goto EXIT_FAILURE;

    // Pick out the values we need from the header data in the buffer.
    // The file is in PC Little Endian.  Make sure to read it safely.
    Info.IDLength       = Buffer[ 0];
    Info.CMapType       = Buffer[ 1];
    Info.ImageType      = Buffer[ 2];
    Info.CMapLength     = Buffer[ 5] + (Buffer[ 6] * 256);
    Info.CMapEntrySize  = Buffer[ 7];  
    Info.Width          = Buffer[12] + (Buffer[13] * 256);
    Info.Height         = Buffer[14] + (Buffer[15] * 256);
    Info.PixelDepth     = Buffer[16];
    Info.ImageDesc      = Buffer[17];

    // Fail on unsupported formats.
    if( (Info.ImageType != 2) && (Info.ImageType != 10) )
    {
        goto EXIT_FAILURE;
    }

    // Determine number of bytes per pixel.
    BytesPerPixel = ((Info.PixelDepth + 7) >> 3);
    if( !IN_RANGE( 2, BytesPerPixel, 4 ) )
    {
        goto EXIT_FAILURE;
    }

    // xbitmap dimensions are signed 16-bit. Leave room for 32-bit conversion,
    // which computes the data size in bits before converting to bytes.
    if( !IN_RANGE( 1, Info.Width, S16_MAX ) || !IN_RANGE( 1, Info.Height, S16_MAX ) ||
        ( Info.Width * Info.Height > S32_MAX / 32 ) )
    {
        goto EXIT_FAILURE;
    }
    NPixels  = Info.Width * Info.Height;
    DataSize = NPixels * BytesPerPixel;

    // Validate the ID, color map and minimum pixel payload before seeking or
    // allocating. A single RLE packet can encode up to 128 identical pixels.
    BytesToSkip = Info.IDLength;
    if( Info.CMapType )
    {
        BytesToSkip += ( ( Info.CMapEntrySize + 7 ) >> 3 ) * Info.CMapLength;
    }
    MinimumSize = ( Info.ImageType == 10 ) ? ( ( NPixels + 127 ) / 128 ) * ( BytesPerPixel + 1 )
                                           : DataSize;
    Position    = x_ftell( pFile );
    Length      = x_flength( pFile );
    if( ( Position < 0 ) || ( Position > Length ) ||
        ( BytesToSkip + MinimumSize > Length - Position ) ||
        ( x_fseek( pFile, BytesToSkip, X_SEEK_CUR ) != 0 ) )
    {
        goto EXIT_FAILURE;
    }

    pData = static_cast<byte*>( x_malloc( DataSize ) );
    if( !pData )
    {
        goto EXIT_FAILURE;
    }

    // Load the pixel data.
    if( Info.ImageType == 10 )
    {
        if( !tga_ReadEncodedData( pData, 
                                  pFile, 
                                  NPixels, 
                                  BytesPerPixel ) )
        {
            goto EXIT_FAILURE;
        }
    }
    else
    if( Info.ImageType == 2 )
    {
        BytesRead = x_fread( pData, 1, DataSize, pFile );
        if( BytesRead != DataSize )
            goto EXIT_FAILURE;
    }

    // Normalize the origin to the top left.
    if( !( Info.ImageDesc & 0x20 ) )
    {
        s32 const RowBytes = Info.Width * BytesPerPixel;
        for( s32 Y = 0; Y < Info.Height / 2; ++Y )
        {
            byte* pTop    = pData + Y * RowBytes;
            byte* pBottom = pData + ( Info.Height - 1 - Y ) * RowBytes;

            for( s32 I = 0; I < RowBytes; ++I )
            {
                byte const Temp = pTop[I];
                pTop[I]    = pBottom[I];
                pBottom[I] = Temp;
            }
        }
    }
    if( Info.ImageDesc & 0x10 )
    {
        s32 const RowBytes = Info.Width * BytesPerPixel;
        for( s32 Y = 0; Y < Info.Height; ++Y )
        {
            byte* pRow = pData + Y * RowBytes;

            for( s32 X = 0; X < Info.Width / 2; ++X )
            {
                byte* pLeft  = pRow + X * BytesPerPixel;
                byte* pRight = pRow + ( Info.Width - 1 - X ) * BytesPerPixel;

                for( s32 I = 0; I < BytesPerPixel; ++I )
                {
                    byte const Temp = pLeft[I];
                    pLeft[I]  = pRight[I];
                    pRight[I] = Temp;
                }
            }
        }
    }

    // For 32 bit data...
    //  + We have:
    //      - ARGB in Little Endian.
    //      - BGRA in Big Endian.
    if( BytesPerPixel == 4 )
    {
        #ifdef X_LITTLE_ENDIAN
        Format = xbitmap::FMT_32_ARGB_8888;
        #endif

        #ifdef X_BIG_ENDIAN
        Format = xbitmap::FMT_32_BGRA_8888;
        #endif
    }

    // For 24 bit data...
    //  + We have BGR.
    if( BytesPerPixel == 3 )
    {
        Format = xbitmap::FMT_24_BGR_888;
    }

    // For 16 bit data...
    //  + We have:
    //      - URGB_1555 in Little Endian.
    //      - "A Big Mess" in Big Endian.
    //  + We want:
    //      - URGB_1555 in this platform's Endian.
    if( BytesPerPixel == 2 )
    {
        #ifdef X_BIG_ENDIAN
        s32  i;
        u16* p = (u16*)pData;
        for( i = 0; i < NPixels; i++ )
        {
            *p = ENDIAN_SWAP_16( *p );
            p++;
        }
        #endif

        Format = xbitmap::FMT_16_URGB_1555;
    }

    // Set up the xbitmap.
    {
        ASSERT( Format != xbitmap::FMT_NULL );

        Bitmap.Setup( Format, 
                      Info.Width, 
                      Info.Height, 
                      TRUE, 
                      pData );
    }

    // Success!
    return( TRUE );

    // Failure!
    EXIT_FAILURE:
    if( pData )     x_free  ( pData );
    return( FALSE );
}

//==============================================================================

xbool tga_Load( xbitmap& Bitmap, const char* pFileName )
{
    ASSERT( pFileName );

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        return FALSE;
    }

    xbool const Result = auxbmp_LoadTGA( Bitmap, pFile );
    x_fclose( pFile );
    return Result;
}

//==============================================================================

xbool tga_Info( const char* pFileName, xbitmap::info& BitmapInfo )
{
    tga_info    Info;
    byte        Buffer[18];
    X_FILE*     pFile = NULL;
    s32         BytesRead;
    s32         BytesPerPixel;

    xbitmap::format Format = xbitmap::FMT_NULL;

    ASSERT( pFileName );

    // Attempt to open the specified file for read.
    pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
        goto exit_failure;

    // Read the header data into the buffer.
    BytesRead = x_fread( &Buffer, 1, 18, pFile );
    if( BytesRead != 18 )
        goto exit_failure;

    // Pick out the values we need from the header data in the buffer.
    // The file is in PC Little Endian.  Make sure to read it safely.
    Info.IDLength       = Buffer[ 0];
    Info.CMapType       = Buffer[ 1];
    Info.ImageType      = Buffer[ 2];
    Info.CMapLength     = Buffer[ 5] + (Buffer[ 6] * 256);
    Info.CMapEntrySize  = Buffer[ 7];  
    Info.Width          = Buffer[12] + (Buffer[13] * 256);
    Info.Height         = Buffer[14] + (Buffer[15] * 256);
    Info.PixelDepth     = Buffer[16];
    Info.ImageDesc      = Buffer[17];

    // Fail on unsupported formats.
    if( (Info.ImageType != 2) && (Info.ImageType != 10) )
        goto exit_failure;

    // Determine number of bytes per pixel.
    BytesPerPixel = ((Info.PixelDepth + 7) >> 3);
    if( !IN_RANGE( 2, BytesPerPixel, 4 ) )
        goto exit_failure;

    // Done reading from file.
    x_fclose( pFile );

    // For 32 bit data...
    //  + We have:
    //      - ARGB in Little Endian.
    //      - BGRA in Big Endian.
    if( BytesPerPixel == 4 )
    {
        #ifdef X_LITTLE_ENDIAN
        Format = xbitmap::FMT_32_ARGB_8888;
        #endif

        #ifdef X_BIG_ENDIAN
        Format = xbitmap::FMT_32_BGRA_8888;
        #endif
    }

    // For 24 bit data...
    //  + We have BGR.
    if( BytesPerPixel == 3 )
    {
        Format = xbitmap::FMT_24_BGR_888;
    }

    // For 16 bit data...
    //  + We have:
    //      - URGB_1555 in Little Endian.
    //      - "A Big Mess" in Big Endian.
    //  + We want:
    //      - URGB_1555 in this platform's Endian.
    if( BytesPerPixel == 2 )
    {
        Format = xbitmap::FMT_16_URGB_1555;
    }

    // Fill in the xbitmap::info struct
    BitmapInfo.W      = Info.Width;
    BitmapInfo.H      = Info.Height;
    BitmapInfo.nMips  = 0;
    BitmapInfo.Format = Format;

    // Success!
    return( TRUE );

    // Failure!
    exit_failure:
    if( pFile )     x_fclose( pFile );
    return( FALSE );
}

//==============================================================================
