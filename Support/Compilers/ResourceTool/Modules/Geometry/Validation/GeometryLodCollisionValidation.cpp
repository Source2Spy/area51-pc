//=========================================================================
//
//  GeometryLodCollisionValidation.cpp
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

xbool ValidateLodsAndCollision( const geometry_asset& Asset, const validation_options& Options,
                                diagnostic_log& Diagnostics, const char* pSourceFile )
{
    xbool bValid = TRUE;
    for( s32 iLODGroup = 0; iLODGroup < (s32)Asset.LODs.Groups.size(); ++iLODGroup )
    {
        const geometry_lod_group& Group = Asset.LODs.Groups[iLODGroup];
        for( s32 iLevel = 0; iLevel < (s32)Group.Levels.size(); ++iLevel )
        {
            const geometry_lod_level& Level = Group.Levels[iLevel];
            for( const std::string& MeshName : Level.MeshNames )
            {
                if( FindNamed( Asset.Render.SubMeshes, MeshName ) < 0 )
                {
                    bValid =
                        Report( Diagnostics,
                                xfs( "LOD [%s] references missing mesh [%s]", Group.Name.c_str(), MeshName.c_str() ),
                                Group.Name.c_str(), pSourceFile ) &&
                        bValid;
                }
            }
        }
    }

    for( s32 iCollision = 0; iCollision < (s32)Asset.Collision.Meshes.size(); ++iCollision )
    {
        const geometry_collision_mesh& Mesh = Asset.Collision.Meshes[iCollision];
        if( Mesh.Vertices.empty() || Mesh.Normals.size() != Mesh.Vertices.size() || Mesh.Indices.empty() ||
            ( Mesh.Indices.size() % 3 ) != 0 )
        {
            bValid = Report( Diagnostics, xfs( "collision mesh [%s] has invalid topology", Mesh.Name.c_str() ),
                             Mesh.Name.c_str(), pSourceFile ) &&
                     bValid;
            continue;
        }
        for( u32 Index : Mesh.Indices )
        {
            if( Index >= Mesh.Vertices.size() )
            {
                bValid = Report( Diagnostics,
                                 xfs( "collision mesh [%s] references vertex %u outside %u", Mesh.Name.c_str(), Index,
                                      (u32)Mesh.Vertices.size() ),
                                 Mesh.Name.c_str(), pSourceFile ) &&
                         bValid;
            }
        }
    }
    (void)Options;
    return bValid;
}

} // namespace resource_tool::geometry::validation_detail
