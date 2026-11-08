//=========================================================================
//
//  Texture.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Texture.hpp"
#include "x_array.hpp"
#include "e_Engine.hpp"

// #define texdebug

#ifdef texdebug
xbool      g_OutputTGA = TRUE;
static s32 s_Count = 0;
#endif // texdebug

static s32 s_nTexture = 0;
static s32 s_TextureMem = 0;

//=========================================================================
//  TYPES
//=========================================================================

static struct texture_loader : public rsc_loader
{
    texture_loader( void ) : rsc_loader( "TEXTURE", ".xbmp" )
    {
    }

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFp )
    {
        MEMORY_OWNER( "TEXTURE DATA" );
        texture* pTexture = new texture;
        ASSERT( pTexture );

        if( !pTexture->m_bitmap.Load( pFp ) )
        {
            delete pTexture;
            x_throw( "Failed to load texture bitmap." );
            return NULL;
        }

        // sanity check
        switch ( pTexture->m_bitmap.GetFormat() )
        {

            case xbitmap::FMT_32_ARGB_8888:
            case xbitmap::FMT_32_URGB_8888:
            case xbitmap::FMT_16_ARGB_4444:
            case xbitmap::FMT_16_ARGB_1555:
            case xbitmap::FMT_16_URGB_1555:
            case xbitmap::FMT_16_RGB_565:
#ifdef TARGET_MOBILE
            case xbitmap::FMT_ASTC_4X4:
            case xbitmap::FMT_ASTC_5X4:
            case xbitmap::FMT_ASTC_5X5:
            case xbitmap::FMT_ASTC_6X5:
            case xbitmap::FMT_ASTC_6X6:
            case xbitmap::FMT_ASTC_8X5:
            case xbitmap::FMT_ASTC_8X6:
            case xbitmap::FMT_ASTC_8X8:
            case xbitmap::FMT_ASTC_10X5:
            case xbitmap::FMT_ASTC_10X6:
            case xbitmap::FMT_ASTC_10X8:
            case xbitmap::FMT_ASTC_10X10:
            case xbitmap::FMT_ASTC_12X10:
            case xbitmap::FMT_ASTC_12X12:
#endif
#ifdef TARGET_DESKTOP
            case xbitmap::FMT_DXT1:
            case xbitmap::FMT_DXT3:
            case xbitmap::FMT_DXT5:
#endif
                {
                }
                break;

            default:
                {
                    x_throw( "Invalid texture format." );
                    delete pTexture;
                    return NULL;
                }
        }

        s_TextureMem += pTexture->m_bitmap.GetDataSize();
        s_nTexture++;

        return ( pTexture );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        texture* pTexture = static_cast<texture*>( pData );

        if ( !vram_CreateTexture( pTexture->m_texture, pTexture->m_bitmap, TRUE, "texture" ) )
        {
            x_throw( "Failed to create GPU texture." );
            return NULL;
        }
#ifdef texdebug
        x_DebugMsg( "texture_loader::Resolve: Created GPU texture %dx%d\n", pTexture->m_bitmap.GetWidth(),
                    pTexture->m_bitmap.GetHeight() );
#endif

#ifdef texdebug
        if ( g_OutputTGA == TRUE )
        {
            pTexture->m_bitmap.SaveTGA( xfs( "C:\\GameData\\A51\\Test\\%03d.tga", s_Count++ ) );
        }
#endif

        return ( pTexture );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        texture* pTexture = static_cast<texture*>( pData );

        s_TextureMem -= pTexture->m_bitmap.GetDataSize();
        s_nTexture--;

#ifdef texdebug
        x_DebugMsg( "texture_loader::Unload: Destroying GPU texture %dx%d\n", pTexture->m_bitmap.GetWidth(),
                    pTexture->m_bitmap.GetHeight() );
#endif

        vram_DestroyTexture( pTexture->m_texture );

        delete pTexture;
    }

} s_Texture_Loader;

//=========================================================================

static struct cubemap_loader : public rsc_loader
{
    cubemap_loader( void ) : rsc_loader( "CUBEMAP", ".envmap" )
    {
    }

    //-------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFp )
    {
        cubemap* pCubemap = new cubemap;
        ASSERT( pCubemap );

        for ( s32 i = 0; i < 6; i++ )
        {
            pCubemap->m_bitmap[i].Load( pFp );
            s_TextureMem += pCubemap->m_bitmap[i].GetDataSize();
            s_nTexture++;
        }

        return ( pCubemap );
    }

    //-------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        cubemap* pCubemap = static_cast<cubemap*>( pData );

        if ( !vram_CreateTextureCube( pCubemap->m_texture, pCubemap->m_bitmap, 6, TRUE, "cubemap" ) )
        {
            x_throw( "Failed to create GPU cubemap." );
            return NULL;
        }

        return ( pCubemap );
    }

    //-------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        cubemap* pCubemap = static_cast<cubemap*>( pData );

        for ( s32 i = 0; i < 6; i++ )
        {
            s_TextureMem -= pCubemap->m_bitmap[i].GetDataSize();
            s_nTexture--;
        }

        vram_DestroyTexture( pCubemap->m_texture );

        delete pCubemap;
    }

} s_Cubemap_Loader;

//=========================================================================

texture::texture( void )
{
    m_texture = vram_texture();
    m_pShaderResourceOverride = NULL;
}

//=========================================================================

vram_texture const& texture::GetTexture( void ) const
{
    return m_texture;
}

//=========================================================================

shader_resource const* texture::GetShaderResource( void ) const
{
    if ( m_pShaderResourceOverride )
    {
        return m_pShaderResourceOverride;
    }

    return vram_GetShaderResource( m_texture );
}

//=========================================================================

shader_resource const* texture::GetShaderResourceOverride( void ) const
{
    return m_pShaderResourceOverride;
}

//=========================================================================

void texture::SetShaderResourceOverride( shader_resource const* pResource )
{
    m_pShaderResourceOverride = pResource;
}

//=========================================================================

void texture::GetStats( s32* pNumTextureLoaded, s32* pTextureMemorySize )
{
    *pNumTextureLoaded = s_nTexture;
    *pTextureMemorySize = s_TextureMem;
}

//=========================================================================

cubemap::cubemap( void )
{
    m_texture = vram_texture();
}

//=========================================================================

vram_texture const& cubemap::GetTexture( void ) const
{
    return m_texture;
}

//=========================================================================

shader_resource const* cubemap::GetShaderResource( void ) const
{
    return vram_GetShaderResource( m_texture );
}
