//=========================================================================
//
//  MaterialStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "MaterialStage.hpp"

#include "Diagnostics.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::material_stage::Execute( geometry_asset& Asset, diagnostic_log& Diagnostics,
                                                        const char* pSourceFile ) const
{
    xbool bValid = TRUE;
    for( s32 iFacet = 0; iFacet < (s32)Asset.Render.Facets.size(); ++iFacet )
    {
        const geometry_facet& Facet = Asset.Render.Facets[iFacet];
        if( Facet.MaterialName.empty() )
        {
            Diagnostics.Error( xfs( "facet %d has no material identity", iFacet ), "GeometryMaterial", "facet",
                               pSourceFile );
            bValid = FALSE;
        }
    }
    return bValid;
}
