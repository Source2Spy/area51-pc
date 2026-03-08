#include "Texture.hpp"
#include "x_array.hpp"
#include "e_VRAM.hpp"

//#define texdebug

#ifdef texdebug
xbool      g_OutputTGA  = TRUE;
static s32 s_Count      = 0;
#endif // texdebug

static s32 s_nTexture   = 0;
static s32 s_TextureMem = 0;

//=============================================================================
//    TYPES
//=============================================================================

static struct texture_loader : public rsc_loader
{
    texture_loader( void ) : rsc_loader( "TEXTURE", ".xbmp" ) {}

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* FP )
    {
        MEMORY_OWNER( "TEXTURE DATA" );
        texture* pTexture = new texture;
        ASSERT( pTexture );

        pTexture->m_Bitmap.Load( FP );

        // sanity check
        switch ( pTexture->m_Bitmap.GetFormat() )
        {

        #ifdef TARGET_PC
        case xbitmap::FMT_32_ARGB_8888:
        case xbitmap::FMT_32_URGB_8888:
        case xbitmap::FMT_16_ARGB_4444:
        case xbitmap::FMT_16_ARGB_1555:
        case xbitmap::FMT_16_URGB_1555:
        case xbitmap::FMT_16_RGB_565:
        case xbitmap::FMT_DXT1:
        case xbitmap::FMT_DXT3:
        case xbitmap::FMT_DXT5:
            break;
        #endif

        default:
            x_throw( "Invalid texture format." );
            delete pTexture;
            return NULL;
        }

        s_TextureMem += pTexture->m_Bitmap.GetDataSize();
        s_nTexture++;

        return( pTexture );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData ) 
    {
        texture* pTexture = (texture*)pData;
    
        s32 vramID = vram_Register( pTexture->m_Bitmap );
    #ifdef texdebug
        x_DebugMsg( "texture_loader::Resolve: Registered bitmap %dx%d, VRAM_ID = %d\n", 
                    pTexture->m_Bitmap.GetWidth(), pTexture->m_Bitmap.GetHeight(), vramID );
    #endif            
    
    #ifdef texdebug
        if( g_OutputTGA == TRUE )
        {
            pTexture->m_Bitmap.SaveTGA( xfs( "C:\\GameData\\A51\\Test\\%03d.tga", s_Count++ ) );
        }
    #endif
    
        return( pTexture );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        texture* pTexture = (texture*)pData;
    
        s_TextureMem -= pTexture->m_Bitmap.GetDataSize();
        s_nTexture--;
    
    #ifdef texdebug
        x_DebugMsg( "texture_loader::Unload: Unregistering bitmap %dx%d, VRAM_ID = %d\n", 
                    pTexture->m_Bitmap.GetWidth(), pTexture->m_Bitmap.GetHeight(), 
                    pTexture->m_Bitmap.GetVRAMID() );
    #endif                
    
        vram_Unregister( pTexture->m_Bitmap );
    
        delete pTexture;
    }

} s_Texture_Loader;

//=============================================================================

static struct cubemap_loader : public rsc_loader
{
    cubemap_loader( void ) : rsc_loader( "CUBEMAP", ".envmap" ) {}

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* FP )
    {
        cubemap* pCubemap = new cubemap;
        ASSERT( pCubemap );

        for ( s32 i = 0; i < 6; i++ )
        {
            pCubemap->m_Bitmap[i].Load( FP );
            s_TextureMem += pCubemap->m_Bitmap[i].GetDataSize();
            s_nTexture++;
        }

        return( pCubemap );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        cubemap* pCubemap = (cubemap*)pData;

    #ifdef TARGET_PC
        s32 vramID = vram_Register( pCubemap->m_Bitmap, 6 );
        if( vramID == 0 )
        {
            x_DebugMsg( "cubemap_loader::Resolve: Failed to register cubemap\n" );
        }

        pCubemap->m_hTexture = (void*)vramID;
    #else
        for( s32 i = 0; i < 6; i++ )
            vram_Register( pCubemap->m_Bitmap[i] );
    #endif

        return( pCubemap );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        cubemap* pCubemap = (cubemap*)pData;

        for( s32 i = 0; i < 6; i++ )
        {
            s_TextureMem -= pCubemap->m_Bitmap[i].GetDataSize();
            s_nTexture--;
        }

    #ifdef TARGET_PC
        if( pCubemap->m_hTexture )
        {
            s32 vramID = (s32)pCubemap->m_hTexture;
            vram_Unregister( vramID );
            pCubemap->m_hTexture = NULL;
        }
    #else
        for( s32 i = 0; i < 6; i++ )
        {
            vram_Unregister( pCubemap->m_Bitmap[i] );
        }
    #endif

        delete pCubemap;
    }

} s_Cubemap_Loader;

//=============================================================================

texture::texture( void )
{
}

//=============================================================================

void texture::GetStats( s32* pNumTextureLoaded, s32* pTextureMemorySize )
{
    *pNumTextureLoaded  = s_nTexture;
    *pTextureMemorySize = s_TextureMem;
}

//=============================================================================

cubemap::cubemap( void )
{
}

//=============================================================================
