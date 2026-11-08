//==============================================================================
//  
//  x_bitmap_dxtc.cpp 
//
//
//  Native DXT decoder implementation.
//
//  
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_BITMAP_HPP
#include "../x_bitmap.hpp"
#endif

#ifndef X_MEMORY_HPP
#include "../x_memory.hpp"
#endif

#ifndef X_MATH_HPP
#include "../x_math.hpp"
#endif

//==============================================================================
//  DXT BLOCK STRUCTURES
//==============================================================================

struct dxt1_block
{
    u16 color0;     // First RGB565 color
    u16 color1;     // Second RGB565 color  
    u32 indices;    // 2 bits per pixel, 16 pixels
};

struct dxt3_block
{
    u64 alpha;      // 4 bits per pixel alpha (16 pixels)
    u16 color0;     // First RGB565 color
    u16 color1;     // Second RGB565 color
    u32 indices;    // 2 bits per pixel, 16 pixels
};

struct dxt5_block
{
    u8  alpha0;           // First alpha value
    u8  alpha1;           // Second alpha value
    u8  alpha_indices[6]; // 3 bits per pixel alpha indices
    u16 color0;           // First RGB565 color
    u16 color1;           // Second RGB565 color
    u32 indices;          // 2 bits per pixel, 16 pixels
};

// DXT2 uses same structure as DXT3
typedef dxt3_block dxt2_block;

// DXT4 uses same structure as DXT5  
typedef dxt5_block dxt4_block;

//==============================================================================
//  FUNCTIONS
//==============================================================================

static inline xcolor RGB565ToColor( u16 rgb565 )
{
    u8 r = (u8)((rgb565 >> 11) & 0x1F);
    u8 g = (u8)((rgb565 >>  5) & 0x3F);
    u8 b = (u8)((rgb565 >>  0) & 0x1F);
    
    // Expand to full 8-bit range
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);  
    b = (b << 3) | (b >> 2);
    
    return xcolor( r, g, b, 255 );
}

//==============================================================================

static inline xcolor UnpremultiplyAlpha( const xcolor& c )
{
    if( c.A == 0 )
        return xcolor( 0, 0, 0, 0 );
    
    if( c.A == 255 )
        return c;
    
    // Convert premultiplied back to straight alpha
    f32 alpha = (f32)c.A / 255.0f;
    f32 invAlpha = 1.0f / alpha;
    
    u8 r = (u8)MAX( 0, MIN( 255, (s32)((f32)c.R * invAlpha + 0.5f) ) );
    u8 g = (u8)MAX( 0, MIN( 255, (s32)((f32)c.G * invAlpha + 0.5f) ) );
    u8 b = (u8)MAX( 0, MIN( 255, (s32)((f32)c.B * invAlpha + 0.5f) ) );
    
    return xcolor( r, g, b, c.A );
}

//==============================================================================

static void DecodeDXT1Block( const dxt1_block* pBlock, xcolor* pOutput )
{
    xcolor colors[4];
    
    // Decode base colors
    colors[0] = RGB565ToColor( pBlock->color0 );
    colors[1] = RGB565ToColor( pBlock->color1 );
    
    // Generate interpolated colors
    if( pBlock->color0 > pBlock->color1 )
    {
        // 4-color block: color0 > color1
        colors[2].R = (u8)((2 * colors[0].R + colors[1].R + 1) / 3);
        colors[2].G = (u8)((2 * colors[0].G + colors[1].G + 1) / 3);
        colors[2].B = (u8)((2 * colors[0].B + colors[1].B + 1) / 3);
        colors[2].A = 255;
        
        colors[3].R = (u8)((colors[0].R + 2 * colors[1].R + 1) / 3);
        colors[3].G = (u8)((colors[0].G + 2 * colors[1].G + 1) / 3);
        colors[3].B = (u8)((colors[0].B + 2 * colors[1].B + 1) / 3);
        colors[3].A = 255;
    }
    else
    {
        // 3-color block with transparency: color0 <= color1
        colors[2].R = (u8)((colors[0].R + colors[1].R + 1) / 2);
        colors[2].G = (u8)((colors[0].G + colors[1].G + 1) / 2);
        colors[2].B = (u8)((colors[0].B + colors[1].B + 1) / 2);
        colors[2].A = 255;
        
        colors[3] = xcolor( 0, 0, 0, 0 ); // Transparent black
    }
    
    // Decode pixel indices and output colors
    u32 indices = pBlock->indices;
    for( s32 i = 0; i < 16; i++ )
    {
        s32 index = indices & 0x3;
        pOutput[i] = colors[index];
        indices >>= 2;
    }
}

//==============================================================================

static void DecodeDXT3Block( const dxt3_block* pBlock, xcolor* pOutput )
{
    xcolor colors[4];
    
    // Decode base colors (always 4-color mode for DXT3)
    colors[0] = RGB565ToColor( pBlock->color0 );
    colors[1] = RGB565ToColor( pBlock->color1 );
    
    colors[2].R = (u8)((2 * colors[0].R + colors[1].R + 1) / 3);
    colors[2].G = (u8)((2 * colors[0].G + colors[1].G + 1) / 3);
    colors[2].B = (u8)((2 * colors[0].B + colors[1].B + 1) / 3);
    
    colors[3].R = (u8)((colors[0].R + 2 * colors[1].R + 1) / 3);
    colors[3].G = (u8)((colors[0].G + 2 * colors[1].G + 1) / 3);
    colors[3].B = (u8)((colors[0].B + 2 * colors[1].B + 1) / 3);
    
    // Decode color indices
    u32 indices = pBlock->indices;
    for( s32 i = 0; i < 16; i++ )
    {
        s32 index = indices & 0x3;
        pOutput[i] = colors[index];
        indices >>= 2;
    }
    
    // Decode explicit alpha (4 bits per pixel)
    u64 alpha = pBlock->alpha;
    for( s32 i = 0; i < 16; i++ )
    {
        u8 a = (u8)(alpha & 0xF);
        a = (a << 4) | a; // Expand 4-bit to 8-bit
        pOutput[i].A = a;
        alpha >>= 4;
    }
}

//==============================================================================

static void DecodeDXT2Block( const dxt2_block* pBlock, xcolor* pOutput )
{
    // DXT2 is DXT3 with premultiplied alpha
    DecodeDXT3Block( pBlock, pOutput );
    
    // Convert from premultiplied to straight alpha
    for( s32 i = 0; i < 16; i++ )
    {
        pOutput[i] = UnpremultiplyAlpha( pOutput[i] );
    }
}

//==============================================================================

static void DecodeDXT5Block( const dxt5_block* pBlock, xcolor* pOutput )
{
    xcolor colors[4];
    
    // Decode base colors (always 4-color mode for DXT5)
    colors[0] = RGB565ToColor( pBlock->color0 );
    colors[1] = RGB565ToColor( pBlock->color1 );
    
    colors[2].R = (u8)((2 * colors[0].R + colors[1].R + 1) / 3);
    colors[2].G = (u8)((2 * colors[0].G + colors[1].G + 1) / 3);
    colors[2].B = (u8)((2 * colors[0].B + colors[1].B + 1) / 3);
    
    colors[3].R = (u8)((colors[0].R + 2 * colors[1].R + 1) / 3);
    colors[3].G = (u8)((colors[0].G + 2 * colors[1].G + 1) / 3);
    colors[3].B = (u8)((colors[0].B + 2 * colors[1].B + 1) / 3);
    
    // Decode color indices
    u32 indices = pBlock->indices;
    for( s32 i = 0; i < 16; i++ )
    {
        s32 index = indices & 0x3;
        pOutput[i] = colors[index];
        indices >>= 2;
    }
    
    // Build alpha palette
    u8 alphas[8];
    alphas[0] = pBlock->alpha0;
    alphas[1] = pBlock->alpha1;
    
    if( alphas[0] > alphas[1] )
    {
        // 8-alpha block
        for( s32 i = 1; i < 7; i++ )
        {
            alphas[i+1] = (u8)(((7-i) * alphas[0] + i * alphas[1] + 3) / 7);
        }
    }
    else
    {
        // 6-alpha block
        for( s32 i = 1; i < 5; i++ )
        {
            alphas[i+1] = (u8)(((5-i) * alphas[0] + i * alphas[1] + 2) / 5);
        }
        alphas[6] = 0;
        alphas[7] = 255;
    }
    
    // Decode alpha indices (3 bits per pixel)
    u64 alpha_bits = 0;
    for( s32 i = 0; i < 6; i++ )
    {
        alpha_bits |= ((u64)pBlock->alpha_indices[i]) << (i * 8);
    }
    
    for( s32 i = 0; i < 16; i++ )
    {
        s32 alpha_index = (s32)(alpha_bits & 0x7);
        pOutput[i].A = alphas[alpha_index];
        alpha_bits >>= 3;
    }
}

//==============================================================================

static void DecodeDXT4Block( const dxt4_block* pBlock, xcolor* pOutput )
{
    // DXT4 is DXT5 with premultiplied alpha
    DecodeDXT5Block( pBlock, pOutput );
    
    // Convert from premultiplied to straight alpha
    for( s32 i = 0; i < 16; i++ )
    {
        pOutput[i] = UnpremultiplyAlpha( pOutput[i] );
    }
}

//==============================================================================

xcolor ReadPixelColorDXT1( const xbitmap* pBmp, s32 X, s32 Y, s32 Mip )
{
    ASSERT( pBmp );
    ASSERT( pBmp->GetFormat() == xbitmap::FMT_DXT1 );
    
    s32 blockX = X / 4;
    s32 blockY = Y / 4;
    s32 pixelX = X % 4;
    s32 pixelY = Y % 4;
    
    s32 blocksPerRow = (pBmp->GetWidth(Mip) + 3) / 4;
    const dxt1_block* pBlocks = (const dxt1_block*)pBmp->GetPixelData(Mip);
    const dxt1_block* pBlock = &pBlocks[blockY * blocksPerRow + blockX];
    
    xcolor pixels[16];
    DecodeDXT1Block( pBlock, pixels );
    
    return pixels[pixelY * 4 + pixelX];
}

//==============================================================================

xcolor ReadPixelColorDXT2( const xbitmap* pBmp, s32 X, s32 Y, s32 Mip )
{
    ASSERT( pBmp );
    ASSERT( pBmp->GetFormat() == xbitmap::FMT_DXT2 );
    
    s32 blockX = X / 4;
    s32 blockY = Y / 4;
    s32 pixelX = X % 4;
    s32 pixelY = Y % 4;
    
    s32 blocksPerRow = (pBmp->GetWidth(Mip) + 3) / 4;
    const dxt2_block* pBlocks = (const dxt2_block*)pBmp->GetPixelData(Mip);
    const dxt2_block* pBlock = &pBlocks[blockY * blocksPerRow + blockX];
    
    xcolor pixels[16];
    DecodeDXT2Block( pBlock, pixels );
    
    return pixels[pixelY * 4 + pixelX];
}

//==============================================================================

xcolor ReadPixelColorDXT3( const xbitmap* pBmp, s32 X, s32 Y, s32 Mip )
{
    ASSERT( pBmp );
    ASSERT( pBmp->GetFormat() == xbitmap::FMT_DXT3 );
    
    s32 blockX = X / 4;
    s32 blockY = Y / 4;
    s32 pixelX = X % 4;
    s32 pixelY = Y % 4;
    
    s32 blocksPerRow = (pBmp->GetWidth(Mip) + 3) / 4;
    const dxt3_block* pBlocks = (const dxt3_block*)pBmp->GetPixelData(Mip);
    const dxt3_block* pBlock = &pBlocks[blockY * blocksPerRow + blockX];
    
    xcolor pixels[16];
    DecodeDXT3Block( pBlock, pixels );
    
    return pixels[pixelY * 4 + pixelX];
}

//==============================================================================

xcolor ReadPixelColorDXT4( const xbitmap* pBmp, s32 X, s32 Y, s32 Mip )
{
    ASSERT( pBmp );
    ASSERT( pBmp->GetFormat() == xbitmap::FMT_DXT4 );
    
    s32 blockX = X / 4;
    s32 blockY = Y / 4;
    s32 pixelX = X % 4;
    s32 pixelY = Y % 4;
    
    s32 blocksPerRow = (pBmp->GetWidth(Mip) + 3) / 4;
    const dxt4_block* pBlocks = (const dxt4_block*)pBmp->GetPixelData(Mip);
    const dxt4_block* pBlock = &pBlocks[blockY * blocksPerRow + blockX];
    
    xcolor pixels[16];
    DecodeDXT4Block( pBlock, pixels );
    
    return pixels[pixelY * 4 + pixelX];
}

//==============================================================================

xcolor ReadPixelColorDXT5( const xbitmap* pBmp, s32 X, s32 Y, s32 Mip )
{
    ASSERT( pBmp );
    ASSERT( pBmp->GetFormat() == xbitmap::FMT_DXT5 );
    
    s32 blockX = X / 4;
    s32 blockY = Y / 4;
    s32 pixelX = X % 4;
    s32 pixelY = Y % 4;
    
    s32 blocksPerRow = (pBmp->GetWidth(Mip) + 3) / 4;
    const dxt5_block* pBlocks = (const dxt5_block*)pBmp->GetPixelData(Mip);
    const dxt5_block* pBlock = &pBlocks[blockY * blocksPerRow + blockX];
    
    xcolor pixels[16];
    DecodeDXT5Block( pBlock, pixels );
    
    return pixels[pixelY * 4 + pixelX];
}

//==============================================================================

xbitmap UnpackImage( const xbitmap& Source )
{
    s32 W = Source.GetWidth();
    s32 H = Source.GetHeight();
    s32 NMips = Source.GetNMips();
    
    // Check if it's a DXT format
    xbitmap::format srcFormat = Source.GetFormat();
    if( !Source.IsDXT() )
    {
        return Source; // Not a DXT format, return as-is
    }
    
    // Create result bitmap
    xbitmap Result;
    Result.Setup( xbitmap::FMT_32_RGBA_8888, W, H, TRUE, NULL, FALSE, NULL, W, NMips );
    
    // Process each mip level
    for( s32 mip = 0; mip <= NMips; mip++ )
    {
        s32 mipW = Source.GetWidth(mip);
        s32 mipH = Source.GetHeight(mip);
        
        for( s32 y = 0; y < mipH; y++ )
        {
            for( s32 x = 0; x < mipW; x++ )
            {
                xcolor pixel;
                
                switch( srcFormat )
                {
                    case xbitmap::FMT_DXT1:
                        pixel = ReadPixelColorDXT1( &Source, x, y, mip );
                        break;
                        
                    case xbitmap::FMT_DXT2:
                        pixel = ReadPixelColorDXT2( &Source, x, y, mip );
                        break;
                        
                    case xbitmap::FMT_DXT3:
                        pixel = ReadPixelColorDXT3( &Source, x, y, mip );
                        break;
                        
                    case xbitmap::FMT_DXT4:
                        pixel = ReadPixelColorDXT4( &Source, x, y, mip );
                        break;
                        
                    case xbitmap::FMT_DXT5:
                        pixel = ReadPixelColorDXT5( &Source, x, y, mip );
                        break;
                        
                    default:
                        pixel = XCOLOR_BLACK;
                        break;
                }
                
                Result.SetPixelColor( pixel, x, y, mip );
            }
        }
    }
    
    return Result;
}
