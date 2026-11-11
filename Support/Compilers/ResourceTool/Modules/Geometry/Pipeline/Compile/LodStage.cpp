//=========================================================================
//
//  LodStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "LodStage.hpp"

#include "Diagnostics.hpp"
#include "GeometryNames.hpp"

#include <vector>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool FindMesh( const geometry_asset& Asset, const char* pName )
{
    for( const geometry_sub_mesh& Mesh : Asset.Render.SubMeshes )
    {
        if( geometry_names_match( Mesh.Name.c_str(), pName ) )
            return TRUE;
    }
    return FALSE;
}

//=========================================================================

void RemoveUnusedMeshes( geometry_asset& Asset )
{
    if( Asset.LODs.Groups.empty() )
        return;

    std::vector<xbool> Used( Asset.Render.SubMeshes.size(), FALSE );
    for( s32 iMesh = 0; iMesh < (s32)Asset.Render.SubMeshes.size(); ++iMesh )
    {
        for( s32 iVMesh = 0; iVMesh < (s32)Asset.LODs.Groups.size() && !Used[iMesh]; ++iVMesh )
        {
            const geometry_lod_group& VMesh = Asset.LODs.Groups[iVMesh];
            for( s32 iLOD = 0; iLOD < (s32)VMesh.Levels.size() && !Used[iMesh]; ++iLOD )
            {
                const geometry_lod_level& LOD = VMesh.Levels[iLOD];
                for( s32 iMember = 0; iMember < (s32)LOD.MeshNames.size(); ++iMember )
                {
                    if( geometry_names_match( LOD.MeshNames[iMember].c_str(),
                                              Asset.Render.SubMeshes[iMesh].Name.c_str() ) )
                    {
                        Used[iMesh] = TRUE;
                        break;
                    }
                }
            }
        }
    }

    std::vector<geometry_facet> Kept;
    Kept.reserve( Asset.Render.Facets.size() );
    for( const geometry_facet& Facet : Asset.Render.Facets )
    {
        if( Facet.iMesh >= 0 && Facet.iMesh < (s32)Used.size() && !Used[Facet.iMesh] )
            continue;
        Kept.push_back( Facet );
    }
    Asset.Render.Facets.swap( Kept );
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::lod_stage::Execute( geometry_asset& Asset, diagnostic_log& Diagnostics,
                                                   const char* pSourceFile ) const
{
    RemoveUnusedMeshes( Asset );

    xbool bValid = TRUE;
    for( s32 iVMesh = 0; iVMesh < (s32)Asset.LODs.Groups.size(); ++iVMesh )
    {
        const geometry_lod_group& VMesh = Asset.LODs.Groups[iVMesh];
        if( VMesh.Name.empty() )
        {
            Diagnostics.Error( xfs( "virtual mesh %d has no semantic name", iVMesh ), "GeometryLOD", "virtual-mesh",
                               pSourceFile );
            bValid = FALSE;
        }
        for( s32 iLOD = 0; iLOD < (s32)VMesh.Levels.size(); ++iLOD )
        {
            const geometry_lod_level& LOD = VMesh.Levels[iLOD];
            if( LOD.MeshNames.empty() )
            {
                Diagnostics.Error( xfs( "virtual mesh [%s] LOD %d has no meshes", VMesh.Name.c_str(), iLOD ),
                                   "GeometryLOD", VMesh.Name.c_str(), pSourceFile );
                bValid = FALSE;
            }
            for( s32 iMesh = 0; iMesh < (s32)LOD.MeshNames.size(); ++iMesh )
            {
                if( !FindMesh( Asset, LOD.MeshNames[iMesh].c_str() ) )
                {
                    Diagnostics.Error(
                        xfs( "LOD member [%s] is missing from source glTF", LOD.MeshNames[iMesh].c_str() ),
                        "GeometryLOD", VMesh.Name.c_str(), pSourceFile );
                    bValid = FALSE;
                }
            }
        }
    }
    return bValid;
}
