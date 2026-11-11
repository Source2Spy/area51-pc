//=========================================================================
//
//  GeometryMaterialValidation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryValidationInternal.hpp"

#include "Diagnostics.hpp"

namespace resource_tool::geometry::validation_detail
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool ValidateMaterials( const geometry_asset& Asset, const validation_options& Options, diagnostic_log& Diagnostics,
                         const char* pSourceFile )
{
    xbool bValid = TRUE;
    if( !Options.Collision && Asset.Materials.Items.empty() )
        bValid = Report( Diagnostics, "source mesh contains no materials", "", pSourceFile ) && bValid;

    for( s32 iMaterial = 0; iMaterial < (s32)Asset.Materials.Items.size(); ++iMaterial )
    {
        const geometry_material& Material = Asset.Materials.Items[iMaterial];
        if( Material.nMaps < 0 || Material.nMaps > GEOMETRY_MATERIAL_MAX_MAPS )
        {
            bValid = Report( Diagnostics,
                             xfs( "material [%s] has invalid map count %d", Material.Name.c_str(), Material.nMaps ),
                             Material.Name.c_str(), pSourceFile ) &&
                     bValid;
        }
        for( s32 iMap = 0; iMap < Material.nMaps; ++iMap )
        {
            const geometry_map& Map = Material.Map[iMap];
            if( Map.iTexture >= (s32)Asset.Materials.Textures.size() || Map.iTexture < -1 )
            {
                bValid = Report( Diagnostics,
                                 xfs( "material [%s] map %d has invalid texture reference %d", Material.Name.c_str(),
                                      iMap, Map.iTexture ),
                                 Material.Name.c_str(), pSourceFile ) &&
                         bValid;
            }
            if( Map.iUVChannel < 0 || Map.iUVChannel >= GEOMETRY_VERTEX_MAX_UV )
            {
                bValid = Report( Diagnostics,
                                 xfs( "material [%s] map %d has invalid UV channel %d", Material.Name.c_str(), iMap,
                                      Map.iUVChannel ),
                                 Material.Name.c_str(), pSourceFile ) &&
                         bValid;
            }
        }
    }
    return bValid;
}

} // namespace resource_tool::geometry::validation_detail
