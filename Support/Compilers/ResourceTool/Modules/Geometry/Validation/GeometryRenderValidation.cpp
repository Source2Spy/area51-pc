//=========================================================================
//
//  GeometryRenderValidation.cpp
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

xbool ValidateRender( const geometry_asset& Asset, const validation_options& Options, diagnostic_log& Diagnostics,
                      const char* pSourceFile )
{
    xbool bValid = TRUE;
    if( Asset.Render.SubMeshes.empty() )
        bValid = Report( Diagnostics, "source mesh contains no meshes", "", pSourceFile ) && bValid;
    if( Asset.Render.Facets.empty() )
        bValid = Report( Diagnostics, "source mesh contains no facets", "", pSourceFile ) && bValid;
    if( !Options.Collision && Options.Skin && Asset.Skeleton.Bones.empty() )
        bValid = Report( Diagnostics, "skinned source mesh contains no skeleton", "", pSourceFile ) && bValid;

    for( s32 iVertex = 0; iVertex < (s32)Asset.Render.Vertices.size(); ++iVertex )
    {
        const geometry_vertex& Vertex = Asset.Render.Vertices[iVertex];
        if( !IsFinite( Vertex.Position.GetX() ) || !IsFinite( Vertex.Position.GetY() ) ||
            !IsFinite( Vertex.Position.GetZ() ) )
        {
            bValid =
                Report( Diagnostics, xfs( "vertex %d has a non-finite position", iVertex ), "vertex", pSourceFile ) &&
                bValid;
        }

        if( !Options.Collision && ( Vertex.Normal.empty() || Vertex.UV.empty() ) )
        {
            bValid = Report( Diagnostics, xfs( "vertex %d is missing normal or UV data", iVertex ), "vertex",
                             pSourceFile ) &&
                     bValid;
        }
        for( const vector3& Normal : Vertex.Normal )
        {
            if( !IsFinite( Normal.GetX() ) || !IsFinite( Normal.GetY() ) || !IsFinite( Normal.GetZ() ) )
            {
                bValid =
                    Report( Diagnostics, xfs( "vertex %d has a non-finite normal", iVertex ), "vertex", pSourceFile ) &&
                    bValid;
            }
        }
        for( const vector2& UV : Vertex.UV )
        {
            if( !IsFinite( UV.X ) || !IsFinite( UV.Y ) )
            {
                bValid =
                    Report( Diagnostics, xfs( "vertex %d has a non-finite UV", iVertex ), "vertex", pSourceFile ) &&
                    bValid;
            }
        }

        f32 WeightSum = 0.0f;
        for( const geometry_weight& Weight : Vertex.Weight )
        {
            if( Weight.iBone < 0 || Weight.iBone >= (s32)Asset.Skeleton.Bones.size() )
            {
                bValid = Report( Diagnostics, xfs( "vertex %d references invalid bone %d", iVertex, Weight.iBone ),
                                 "vertex", pSourceFile ) &&
                         bValid;
            }
            if( !IsFinite( Weight.Weight ) || Weight.Weight < 0.0f )
            {
                bValid = Report( Diagnostics, xfs( "vertex %d has an invalid bone weight", iVertex ), "vertex",
                                 pSourceFile ) &&
                         bValid;
            }
            if( !Weight.BoneName.empty() )
            {
                const s32 Bone = FindNamed( Asset.Skeleton.Bones, Weight.BoneName );
                if( Bone < 0 || Bone != Weight.iBone )
                {
                    bValid = Report( Diagnostics,
                                     xfs( "vertex %d has a bone identity/index mismatch for [%s]", iVertex,
                                          Weight.BoneName.c_str() ),
                                     "vertex", pSourceFile ) &&
                             bValid;
                }
            }
            WeightSum += Weight.Weight;
        }
        if( !Options.Collision && Options.Skin && ( WeightSum <= 0.0f || Vertex.Weight.empty() ) )
        {
            bValid =
                Report( Diagnostics, xfs( "vertex %d has no usable skin weights", iVertex ), "vertex", pSourceFile ) &&
                bValid;
        }
    }

    for( s32 iFacet = 0; iFacet < (s32)Asset.Render.Facets.size(); ++iFacet )
    {
        const geometry_facet& Facet = Asset.Render.Facets[iFacet];
        if( Facet.iMesh < 0 || Facet.iMesh >= (s32)Asset.Render.SubMeshes.size() )
        {
            bValid = Report( Diagnostics, xfs( "facet %d has invalid mesh reference %d", iFacet, Facet.iMesh ), "facet",
                             pSourceFile ) &&
                     bValid;
        }
        else if( Facet.MeshName.empty() ||
                 !geometry_names_match( Facet.MeshName.c_str(), Asset.Render.SubMeshes[Facet.iMesh].Name.c_str() ) )
        {
            bValid =
                Report( Diagnostics,
                        xfs( "facet %d has a mesh identity/index mismatch for [%s]", iFacet, Facet.MeshName.c_str() ),
                        "facet", pSourceFile ) &&
                bValid;
        }
        if( !Options.Collision && ( Facet.iMaterial < 0 || Facet.iMaterial >= (s32)Asset.Materials.Items.size() ) )
        {
            bValid = Report( Diagnostics, xfs( "facet %d has invalid material reference %d", iFacet, Facet.iMaterial ),
                             "facet", pSourceFile ) &&
                     bValid;
        }
        else if( !Options.Collision &&
                 ( Facet.MaterialName.empty() ||
                   !geometry_names_match( Facet.MaterialName.c_str(),
                                          Asset.Materials.Items[Facet.iMaterial].Name.c_str() ) ) )
        {
            bValid = Report( Diagnostics,
                             xfs( "facet %d has a material identity/index mismatch for [%s]", iFacet,
                                  Facet.MaterialName.c_str() ),
                             "facet", pSourceFile ) &&
                     bValid;
        }
        if( Facet.nVertices != 3 )
        {
            bValid = Report( Diagnostics, xfs( "facet %d is not a triangle", iFacet ), "facet", pSourceFile ) && bValid;
        }
        for( s32 iCorner = 0; iCorner < 3; ++iCorner )
        {
            if( Facet.iVertex[iCorner] < 0 || Facet.iVertex[iCorner] >= (s32)Asset.Render.Vertices.size() )
            {
                bValid =
                    Report( Diagnostics, xfs( "facet %d references invalid vertex %d", iFacet, Facet.iVertex[iCorner] ),
                            "facet", pSourceFile ) &&
                    bValid;
            }
        }
        if( Facet.iPrimitive < 0 )
        {
            bValid = Report( Diagnostics, xfs( "facet %d has no primitive identity", iFacet ), "facet", pSourceFile ) &&
                     bValid;
        }
        if( !IsFinite( Facet.WorldPixelSize ) || Facet.WorldPixelSize <= 0.0f )
        {
            bValid =
                Report( Diagnostics, xfs( "facet %d has invalid world pixel size", iFacet ), "facet", pSourceFile ) &&
                bValid;
        }
    }
    return bValid;
}

} // namespace resource_tool::geometry::validation_detail
