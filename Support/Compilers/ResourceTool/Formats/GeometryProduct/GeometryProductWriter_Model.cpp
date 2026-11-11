//=========================================================================
//
//  GeometryProductWriter_Model.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductWriter.hpp"

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

const char* TextureName( const geometry_asset& Asset, const geometry_material& Material, s32 MapIndex )
{
    const s32 iTexture = Material.Map[MapIndex].iTexture;
    if( iTexture < 0 || iTexture >= (s32)Asset.Materials.Textures.size() )
        return "";
    return Asset.Materials.Textures[iTexture].FileName.c_str();
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool geometry_product_writer::IsSameMaterial( s32 iMaterialA, s32 iMaterialB, const geometry_asset& Asset )
{
    if( iMaterialA < 0 || iMaterialB < 0 || iMaterialA >= (s32)Asset.Materials.Items.size() ||
        iMaterialB >= (s32)Asset.Materials.Items.size() )
    {
        return FALSE;
    }

    const geometry_material& A = Asset.Materials.Items[iMaterialA];
    const geometry_material& B = Asset.Materials.Items[iMaterialB];
    if( A.Type != B.Type )
        return FALSE;

    if( x_strcmp( TextureName( Asset, A, Max_Diffuse1 ), TextureName( Asset, B, Max_Diffuse1 ) ) )
        return FALSE;
    if( x_strcmp( TextureName( Asset, A, Max_Environment ), TextureName( Asset, B, Max_Environment ) ) )
        return FALSE;
    if( x_strcmp( TextureName( Asset, A, Max_DetailMap ), TextureName( Asset, B, Max_DetailMap ) ) )
        return FALSE;
    if( x_strcmp( TextureName( Asset, A, Max_PunchThrough ), TextureName( Asset, B, Max_PunchThrough ) ) )
        return FALSE;

    if( A.bTwoSided != B.bTwoSided )
        return FALSE;
    if( A.Constants[DETAIL_SCALE].Current[0] != B.Constants[DETAIL_SCALE].Current[0] )
        return FALSE;
    if( A.Constants[FIXED_ALPHA].Current[0] != B.Constants[FIXED_ALPHA].Current[0] )
        return FALSE;
    if( A.Constants[ENV_TYPE].Current[0] != B.Constants[ENV_TYPE].Current[0] )
        return FALSE;
    if( A.Constants[FORCE_ZFILL].Current[0] != B.Constants[FORCE_ZFILL].Current[0] )
        return FALSE;
    if( A.Constants[USE_DIFFUSE].Current[0] != B.Constants[USE_DIFFUSE].Current[0] )
        return FALSE;
    if( A.Constants[ENV_BLEND].Current[0] != B.Constants[ENV_BLEND].Current[0] )
        return FALSE;

    if( A.UVAnimation.Type != B.UVAnimation.Type || A.UVAnimation.StartFrame != B.UVAnimation.StartFrame ||
        A.UVAnimation.FPS != B.UVAnimation.FPS || A.UVAnimation.Keys.size() != B.UVAnimation.Keys.size() )
    {
        return FALSE;
    }
    for( s32 i = 0; i < (s32)A.UVAnimation.Keys.size(); ++i )
    {
        if( A.UVAnimation.Keys[i].U != B.UVAnimation.Keys[i].U || A.UVAnimation.Keys[i].V != B.UVAnimation.Keys[i].V )
        {
            return FALSE;
        }
    }
    return TRUE;
}

//=========================================================================

void geometry_product_writer::BuildCompileModel( geom& Geom, const geometry_asset& Asset, mesh& Mesh, xbool IsRigid )
{
    const geometry_asset* pPhysicsAsset = &Asset;
    if( Context().m_bHasPhysicsAsset && !Context().m_PhysicsAsset.Physics.RigidBodies.empty() )
    {
        pPhysicsAsset = &Context().m_PhysicsAsset;
    }

    if( Context().m_bVerbose )
        pPhysicsAsset->PrintRigidBodies();

    BuildBones( Geom, Asset, *pPhysicsAsset );
    BuildRigidBodies( Geom, Asset, *pPhysicsAsset );
    BuildSettings( Geom, Asset );

    if( Asset.Materials.Items.empty() )
        ThrowError( "No materials are defined in the source mesh" );
    if( Asset.Render.SubMeshes.empty() )
        ThrowError( "No meshes are defined in the source mesh" );

    xarray<s32> MaterialMap;
    xarray<s32> MeshMap;
    xarray<xbool> FacetUsed;
    MaterialMap.SetCount( (s32)Asset.Materials.Items.size() );
    MeshMap.SetCount( (s32)Asset.Render.SubMeshes.size() );
    FacetUsed.SetCount( (s32)Asset.Render.Facets.size() );
    for( s32 i = 0; i < MaterialMap.GetCount(); ++i )
        MaterialMap[i] = -1;
    for( s32 i = 0; i < MeshMap.GetCount(); ++i )
        MeshMap[i] = -1;

    s32 nDegenerateFacets = 0;
    for( s32 iFacet = 0; iFacet < (s32)Asset.Render.Facets.size(); ++iFacet )
    {
        const geometry_facet& Facet = Asset.Render.Facets[iFacet];
        FacetUsed[iFacet] = FALSE;
        if( Facet.iMesh < 0 || Facet.iMesh >= (s32)Asset.Render.SubMeshes.size() )
            ThrowError( xfs( "Facet %d has invalid mesh index %d", iFacet, Facet.iMesh ) );
        if( Facet.iMaterial < 0 || Facet.iMaterial >= (s32)Asset.Materials.Items.size() )
            ThrowError( xfs( "Facet %d has invalid material index %d", iFacet, Facet.iMaterial ) );
        if( Facet.nVertices != 3 )
            ThrowError( "Only triangle facets are supported" );
        for( s32 iCorner = 0; iCorner < 3; ++iCorner )
        {
            if( Facet.iVertex[iCorner] < 0 || Facet.iVertex[iCorner] >= (s32)Asset.Render.Vertices.size() )
            {
                ThrowError( xfs( "Facet %d references invalid vertex %d", iFacet, Facet.iVertex[iCorner] ) );
            }
        }

        const vector3& P0 = Asset.Render.Vertices[Facet.iVertex[0]].Position;
        const vector3& P1 = Asset.Render.Vertices[Facet.iVertex[1]].Position;
        const vector3& P2 = Asset.Render.Vertices[Facet.iVertex[2]].Position;
        if( v3_Cross( P1 - P0, P2 - P0 ).Length() < 0.00001f )
        {
            ++nDegenerateFacets;
            continue;
        }
        MaterialMap[Facet.iMaterial] = 0;
        MeshMap[Facet.iMesh] = 0;
        FacetUsed[iFacet] = TRUE;
    }

    if( nDegenerateFacets > 0 )
        ReportWarning( xfs( "Ignored %d degenerate triangle(s)", nDegenerateFacets ) );
    if( (s32)Asset.Render.Facets.size() - nDegenerateFacets <= 0 )
        ThrowError( "Source mesh contains no renderable triangles" );

    for( s32 iMaterial = 0; iMaterial < (s32)Asset.Materials.Items.size(); ++iMaterial )
    {
        if( MaterialMap[iMaterial] == -1 )
            continue;

        s32 iCompiledMaterial = 0;
        // Preserve generated rawgeom material entries and their settings.
        if( Asset.Materials.PreserveMaterialEntries )
            iCompiledMaterial = Mesh.Material.GetCount();
        for( ; iCompiledMaterial < Mesh.Material.GetCount(); ++iCompiledMaterial )
        {
            if( IsSameMaterial( Mesh.Material[iCompiledMaterial].iAssetMaterial, iMaterial, Asset ) )
            {
                break;
            }
        }

        if( iCompiledMaterial == Mesh.Material.GetCount() )
        {
            material& Material = Mesh.Material.Append();
            Material.iAssetMaterial = iMaterial;

            const geometry_material& SourceMaterial = Asset.Materials.Items[iMaterial];
            const s32 iDiffuse = SourceMaterial.Map[Max_Diffuse1].iTexture;
            if( iDiffuse >= (s32)Asset.Materials.Textures.size() )
                ThrowError( xfs( "Material has an invalid diffuse texture (%s)", SourceMaterial.Name.c_str() ) );

            if( iDiffuse >= 0 )
            {
                char Extension[X_MAX_EXT];
                x_splitpath( Asset.Materials.Textures[iDiffuse].FileName.c_str(), NULL, NULL, NULL, Extension );
                if( !x_stricmp( Extension, ".ifl" ) )
                    ThrowError( xfs( "IFL textures are not supported (%s)",
                                     Asset.Materials.Textures[iDiffuse].FileName.c_str() ) );

                if( !Material.TexInfo.Load( Asset.Materials.Textures[iDiffuse].FileName.c_str() ) )
                    ReportWarning( "Unable to load texinfo; defaults will be used" );
            }
        }
        MaterialMap[iMaterial] = iCompiledMaterial;
    }

    for( s32 iMesh = 0; iMesh < (s32)Asset.Render.SubMeshes.size(); ++iMesh )
    {
        if( MeshMap[iMesh] == -1 )
            continue;

        MeshMap[iMesh] = Mesh.SubMesh.GetCount();
        sub_mesh& Submesh = Mesh.SubMesh.Append();
        Submesh.iAssetMesh = iMesh;
        x_strsavecpy( Submesh.Name, Asset.Render.SubMeshes[iMesh].Name.c_str(), sizeof( Submesh.Name ) );
    }

    Context().m_MeshToCompiled.SetCount( MeshMap.GetCount() );
    for( s32 iMesh = 0; iMesh < MeshMap.GetCount(); ++iMesh )
        Context().m_MeshToCompiled[iMesh] = MeshMap[iMesh];

    for( s32 iFacet = 0; iFacet < (s32)Asset.Render.Facets.size(); ++iFacet )
    {
        if( !FacetUsed[iFacet] )
            continue;

        const geometry_facet& Facet = Asset.Render.Facets[iFacet];
        sub_mesh& Submesh = Mesh.SubMesh[MeshMap[Facet.iMesh]];
        s32 iBone = -1;
        if( IsRigid )
        {
            const geometry_vertex& Vertex = Asset.Render.Vertices[Facet.iVertex[0]];
            // Prefer the semantic bone assignment; use weights as a fallback.
            if( !Facet.BoneName.empty() )
                iBone = Asset.GetBoneIDFromName( Facet.BoneName.c_str() );
            else if( !Vertex.Weight.empty() )
                iBone = Vertex.Weight[0].iBone;
            if( iBone < 0 )
                ThrowError( "Rigid facet has no resolvable bone assignment" );
            if( iBone < 0 || iBone >= (s32)Asset.Skeleton.Bones.size() )
                ThrowError( "Rigid facet references an invalid bone" );
            for( s32 iCorner = 1; iCorner < 3; ++iCorner )
            {
                const geometry_vertex& Corner = Asset.Render.Vertices[Facet.iVertex[iCorner]];
                const s32 CornerBone =
                    !Facet.BoneName.empty() ? iBone : ( Corner.Weight.empty() ? -1 : Corner.Weight[0].iBone );
                if( CornerBone != iBone )
                    ThrowError( "Rigid triangle spans multiple bones" );
            }
        }

        const s32 iMaterial = MaterialMap[Facet.iMaterial];
        s32 iSurface = 0;
        for( ; iSurface < Submesh.Surfaces.GetCount(); ++iSurface )
        {
            const surface& Existing = Submesh.Surfaces[iSurface];
            const xbool bSameSubMeshIndex = Facet.iSubMesh >= 0 && Existing.iSubMesh == Facet.iSubMesh;
            const xbool bSameSourcePrimitive =
                Facet.iSubMesh < 0 && ( Facet.iPrimitive < 0 || Existing.iPrimitive == Facet.iPrimitive );
            if( Existing.iMaterial == iMaterial && Existing.iBone == iBone &&
                ( bSameSubMeshIndex || bSameSourcePrimitive ) )
                break;
        }
        if( iSurface == Submesh.Surfaces.GetCount() )
        {
            surface& Surface = Submesh.Surfaces.Append();
            Surface.iMaterial = iMaterial;
            Surface.iBone = iBone;
            Surface.iPrimitive = Facet.iPrimitive;
            Surface.iSubMesh = Facet.iSubMesh;
            Surface.WorldPixelSize = Facet.WorldPixelSize;
        }
        Submesh.Surfaces[iSurface].Facets.Append( iFacet );

        surface& Surface = Submesh.Surfaces[iSurface];
        s32 iSection = 0;
        for( ; iSection < Surface.Sections.GetCount(); ++iSection )
        {
            if( Surface.Sections[iSection].iSection == Facet.iSection )
                break;
        }
        if( iSection == Surface.Sections.GetCount() )
        {
            surface_section& Section = Surface.Sections.Append();
            Section.iSection = Facet.iSection;
        }
        Surface.Sections[iSection].Facets.Append( iFacet );
    }
}
