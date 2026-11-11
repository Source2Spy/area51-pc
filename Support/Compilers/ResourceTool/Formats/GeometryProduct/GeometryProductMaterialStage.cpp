//=========================================================================
//
//  GeometryProductMaterialStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductMaterialStage.hpp"

#include <string>

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

std::string ProductMaterialName( s32 iMaterial )
{
    return (const char*)xfs( "Material_%d", iMaterial );
}

//=========================================================================

void PopulateBaseMaterials( const geometry_product_data& Source, geometry_asset& Asset )
{
    Asset.Materials.Items.resize( Source.MaterialCount );
    Asset.Materials.Textures.resize( Source.TextureCount );
    Asset.Materials.PreserveMaterialEntries = Source.MaterialCount > 0;
    for( s32 iMaterial = 0; iMaterial < Source.MaterialCount; ++iMaterial )
    {
        const geometry_product_data::material& SourceMaterial = Source.Materials[iMaterial];
        geometry_material& Material = Asset.Materials.Items[iMaterial];
        Material.Name = ProductMaterialName( iMaterial );
        Material.Type = SourceMaterial.Type;
        Material.DetailScale = SourceMaterial.DetailScale;
        Material.FixedAlpha = SourceMaterial.FixedAlpha;
        Material.ProductFlags = SourceMaterial.Flags;
        Material.bTwoSided = ( SourceMaterial.Flags & geometry_product_data::material::FLAG_DOUBLE_SIDED ) != 0;
        Material.bPunchthrough = ( SourceMaterial.Flags & geometry_product_data::material::FLAG_IS_PUNCH_THRU ) != 0;
        Material.nMaps = SourceMaterial.nTextures > 0 ? 1 : 0;
        if( SourceMaterial.iTexture >= 0 && SourceMaterial.iTexture < Source.TextureCount )
            Material.Map[0].iTexture = SourceMaterial.iTexture;

        Material.Textures.clear();
        const xbool bHasEnvironment = ( SourceMaterial.Flags & geometry_product_data::material::FLAG_HAS_ENV_MAP ) != 0;
        const xbool bHasDetail = ( SourceMaterial.Flags & geometry_product_data::material::FLAG_HAS_DETAIL_MAP ) != 0;
        const s32 nDiffuseTextures = SourceMaterial.nVirtualMats > 0
                                         ? SourceMaterial.nVirtualMats
                                         : ( !bHasEnvironment && !bHasDetail ? SourceMaterial.nTextures
                                             : SourceMaterial.nTextures > 0  ? 1
                                                                             : 0 );
        s32 nDiffuseSeen = 0;
        xbool bEnvironmentSeen = FALSE;
        xbool bDetailSeen = FALSE;
        for( s32 iTexture = 0; iTexture < SourceMaterial.nTextures; ++iTexture )
        {
            const s32 TextureIndex = SourceMaterial.iTexture + iTexture;
            if( TextureIndex < 0 || TextureIndex >= Source.TextureCount )
                continue;
            geometry_texture& ProductTexture = Material.Textures.emplace_back();
            if( nDiffuseSeen < nDiffuseTextures )
            {
                ProductTexture.Slot = GEOMETRY_TEXTURE_DIFFUSE;
                ++nDiffuseSeen;
            }
            else if( bHasEnvironment && !bEnvironmentSeen )
            {
                ProductTexture.Slot = GEOMETRY_TEXTURE_ENVIRONMENT;
                bEnvironmentSeen = TRUE;
            }
            else if( bHasDetail && !bDetailSeen )
            {
                ProductTexture.Slot = GEOMETRY_TEXTURE_DETAIL;
                bDetailSeen = TRUE;
            }
            else
            {
                // Keep malformed or legacy textures addressable.
                ProductTexture.Slot = GEOMETRY_TEXTURE_DIFFUSE;
            }
            ProductTexture.FileName = Source.GetTextureName( TextureIndex );
            ProductTexture.Name = Source.GetTextureDesc( TextureIndex );
        }
    }

    for( s32 iTexture = 0; iTexture < Source.TextureCount; ++iTexture )
        Asset.Materials.Textures[iTexture].FileName = Source.GetTextureName( iTexture );
}

//=========================================================================

void PopulateMaterialAnimations( const geometry_product_data& Source, geometry_asset& Asset )
{
    for( s32 iMaterial = 0; iMaterial < Source.MaterialCount; ++iMaterial )
    {
        const geometry_product_data::material::uvanim& SourceAnimation = Source.Materials[iMaterial].UVAnim;
        geometry_material& Material = Asset.Materials.Items[iMaterial];
        Material.UVAnimation.Type = SourceAnimation.Type;
        Material.UVAnimation.StartFrame = SourceAnimation.StartFrame;
        Material.UVAnimation.FPS = SourceAnimation.FPS;
        if( SourceAnimation.nKeys <= 0 )
            continue;
        if( SourceAnimation.iKey < 0 || SourceAnimation.iKey + SourceAnimation.nKeys > Source.UVKeyCount )
        {
            x_throw( xfs( "Material %d has an invalid compiled UV animation range", iMaterial ) );
        }
        Material.UVAnimation.Keys.resize( SourceAnimation.nKeys );
        for( s32 iKey = 0; iKey < SourceAnimation.nKeys; ++iKey )
        {
            Material.UVAnimation.Keys[iKey].U = Source.UVKeys[SourceAnimation.iKey + iKey].OffsetU;
            Material.UVAnimation.Keys[iKey].V = Source.UVKeys[SourceAnimation.iKey + iKey].OffsetV;
        }
    }
}

//=========================================================================

void PopulateVirtualTextures( const geometry_product_data& Source, geometry_asset& Asset )
{
    Asset.Materials.VirtualTextures.clear();
    for( s32 iVTexture = 0; iVTexture < Source.VirtualTextureCount; ++iVTexture )
    {
        const geometry_product_data::virtual_texture& SourceTexture = Source.VirtualTextures[iVTexture];
        geometry_virtual_texture& VirtualTexture = Asset.Materials.VirtualTextures.emplace_back();
        VirtualTexture.Name = Source.GetVTextureName( iVTexture );
        s32 iMaterial = -1;
        for( s32 i = 0; i < Source.MaterialCount && i < 32; ++i )
        {
            if( SourceTexture.MaterialMask & ( u32( 1 ) << i ) )
            {
                iMaterial = i;
                break;
            }
        }
        if( iMaterial < 0 )
            continue;
        const geometry_product_data::material& Material = Source.Materials[iMaterial];
        for( s32 iTexture = 0; iTexture < Material.nVirtualMats; ++iTexture )
        {
            const s32 TextureIndex = Material.iTexture + iTexture;
            if( TextureIndex < 0 || TextureIndex >= Source.TextureCount )
                x_throw( xfs( "Virtual texture %d references invalid texture data", iVTexture ) );
            geometry_texture& Texture = VirtualTexture.Textures.emplace_back();
            Texture.FileName = Source.GetTextureName( TextureIndex );
            Texture.Name = Source.GetTextureDesc( TextureIndex );
        }
    }
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_material_stage::Execute( const geometry_product_data& Source, geometry_asset& Asset,
                                                std::string& Error ) const
{
    (void)Error;
    PopulateBaseMaterials( Source, Asset );
    PopulateMaterialAnimations( Source, Asset );
    PopulateVirtualTextures( Source, Asset );
    return TRUE;
}

} // namespace resource_tool::geometry_product_format
