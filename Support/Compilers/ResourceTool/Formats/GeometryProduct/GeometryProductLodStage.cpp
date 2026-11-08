//=========================================================================
//
//  GeometryProductLodStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductLodStage.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_lod_stage::Execute( const geometry_product_data& Source, geometry_asset& Asset,
                                           std::string& Error ) const
{
    (void)Error;
    Asset.LODs.Groups.clear();
    for( s32 iVMesh = 0; iVMesh < Source.VirtualMeshCount; ++iVMesh )
    {
        const geometry_product_data::virtual_mesh& SourceVMesh = Source.VirtualMeshes[iVMesh];
        geometry_lod_group& Group = Asset.LODs.Groups.emplace_back();
        Group.Name = Source.GetVMeshName( iVMesh );
        for( s32 iLOD = 0; iLOD < SourceVMesh.nLODs; ++iLOD )
        {
            const s32 ProductLOD = SourceVMesh.iLOD + iLOD;
            if( ProductLOD < 0 || ProductLOD >= Source.LODCount )
                continue;
            geometry_lod_level& Level = Group.Levels.emplace_back();
            Level.ScreenSize = Source.LODSizes[ProductLOD];
            const u64 MeshMask = Source.LODMasks[ProductLOD];
            for( s32 iMesh = 0; iMesh < Source.MeshCount && iMesh < 64; ++iMesh )
            {
                if( MeshMask & ( u64( 1 ) << iMesh ) )
                    Level.MeshNames.emplace_back( Asset.Render.SubMeshes[iMesh].Name );
            }
        }
    }
    return TRUE;
}

} // namespace resource_tool::geometry_product_format
