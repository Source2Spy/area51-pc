//=========================================================================
//
//  NormalizeStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "NormalizeStage.hpp"

#include "Diagnostics.hpp"

#include <vector>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

void RemoveRigidAuthoringHelpers( geometry_asset& Asset )
{
    std::vector<xbool> RemoveMesh( Asset.Render.SubMeshes.size(), FALSE );
    for( s32 iMesh = 0; iMesh < (s32)Asset.Render.SubMeshes.size(); ++iMesh )
    {
        RemoveMesh[iMesh] = x_stristr( Asset.Render.SubMeshes[iMesh].Name.c_str(), "Human_" ) != NULL;
    }

    std::vector<geometry_facet> KeptFacets;
    KeptFacets.reserve( Asset.Render.Facets.size() );
    for( const geometry_facet& Facet : Asset.Render.Facets )
    {
        if( Facet.iMesh >= 0 && Facet.iMesh < (s32)RemoveMesh.size() && RemoveMesh[Facet.iMesh] )
            continue;
        KeptFacets.push_back( Facet );
    }
    Asset.Render.Facets.swap( KeptFacets );
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::normalize_stage::Execute( geometry_asset& Asset, const normalize_options& Options,
                                                         diagnostic_log& Diagnostics, const char* pSourceFile ) const
{
    (void)Diagnostics;
    (void)pSourceFile;
    if( Options.Rigid )
    {
        // Human_* meshes are editor-only helpers.
        RemoveRigidAuthoringHelpers( Asset );
        Asset.CleanWeights( 1, 0.00001f );
    }
    else
    {
        Asset.CleanWeights( 2, 0.00001f );
    }
    if( Options.Verbose )
        Asset.PrintHierarchy();
    return TRUE;
}
