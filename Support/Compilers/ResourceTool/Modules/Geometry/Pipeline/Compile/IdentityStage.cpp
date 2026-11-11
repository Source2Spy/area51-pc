//=========================================================================
//
//  IdentityStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "IdentityStage.hpp"

#include "Diagnostics.hpp"
#include "GeometryNames.hpp"

#include <string>
#include <vector>

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool NamesEqual( const std::string& A, const std::string& B )
{
    return geometry_names_match( A.c_str(), B.c_str() ) != FALSE;
}

//=========================================================================

template <typename Item> s32 FindNamed( const std::vector<Item>& Items, const std::string& Name )
{
    s32 Found = -1;
    for( s32 i = 0; i < (s32)Items.size(); ++i )
    {
        if( !geometry_names_match( Items[i].Name.c_str(), Name.c_str() ) )
            continue;
        if( Found >= 0 )
            return -2;
        Found = i;
    }
    return Found;
}

//=========================================================================

template <typename Item>
xbool ValidateUniqueNames( const std::vector<Item>& Items, const char* pKind,
                           resource_tool::diagnostic_log& Diagnostics, const char* pSourceFile )
{
    xbool bValid = TRUE;
    for( usize i = 0; i < Items.size(); ++i )
    {
        if( Items[i].Name.empty() )
        {
            Diagnostics.Error( xfs( "%s %d has no semantic name", pKind, (s32)i ), "GeometryIdentity", pKind,
                               pSourceFile );
            bValid = FALSE;
            continue;
        }

        for( usize j = 0; j < i; ++j )
        {
            if( NamesEqual( Items[i].Name, Items[j].Name ) )
            {
                Diagnostics.Error( xfs( "duplicate %s name [%s]", pKind, Items[i].Name.c_str() ), "GeometryIdentity",
                                   Items[i].Name.c_str(), pSourceFile );
                bValid = FALSE;
                break;
            }
        }
    }
    return bValid;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::identity_stage::Execute( geometry_asset& Asset, diagnostic_log& Diagnostics,
                                                        const char* pSourceFile ) const
{
    xbool bValid = TRUE;

    // Assign deterministic names before later stages use object identity.
    for( usize i = 0; i < Asset.Render.SubMeshes.size(); ++i )
    {
        if( Asset.Render.SubMeshes[i].Name.empty() )
            Asset.Render.SubMeshes[i].Name = (const char*)xfs( "Mesh_%d", (s32)i );
    }
    for( usize i = 0; i < Asset.Materials.Items.size(); ++i )
    {
        if( Asset.Materials.Items[i].Name.empty() )
            Asset.Materials.Items[i].Name = (const char*)xfs( "Material_%d", (s32)i );
    }
    for( usize i = 0; i < Asset.Skeleton.Bones.size(); ++i )
    {
        if( Asset.Skeleton.Bones[i].Name.empty() )
            Asset.Skeleton.Bones[i].Name = (const char*)xfs( "Bone_%d", (s32)i );
    }

    bValid = ValidateUniqueNames( Asset.Render.SubMeshes, "mesh", Diagnostics, pSourceFile ) && bValid;
    bValid = ValidateUniqueNames( Asset.Materials.Items, "material", Diagnostics, pSourceFile ) && bValid;
    bValid = ValidateUniqueNames( Asset.Skeleton.Bones, "bone", Diagnostics, pSourceFile ) && bValid;

    for( s32 iBone = 0; iBone < (s32)Asset.Skeleton.Bones.size(); ++iBone )
    {
        geometry_bone& Bone = Asset.Skeleton.Bones[iBone];
        if( !Bone.ParentName.empty() )
        {
            const s32 Parent = FindNamed( Asset.Skeleton.Bones, Bone.ParentName );
            if( Parent == -2 )
            {
                Diagnostics.Error(
                    xfs( "bone [%s] has an ambiguous parent name [%s]", Bone.Name.c_str(), Bone.ParentName.c_str() ),
                    "GeometryIdentity", Bone.Name.c_str(), pSourceFile );
                bValid = FALSE;
            }
            else if( Parent < 0 )
            {
                Diagnostics.Error(
                    xfs( "bone [%s] references missing parent [%s]", Bone.Name.c_str(), Bone.ParentName.c_str() ),
                    "GeometryIdentity", Bone.Name.c_str(), pSourceFile );
                bValid = FALSE;
            }
            else
            {
                Bone.iParent = Parent;
            }
        }
        else if( Bone.iParent >= 0 && Bone.iParent < (s32)Asset.Skeleton.Bones.size() )
        {
            Bone.ParentName = Asset.Skeleton.Bones[Bone.iParent].Name;
        }
    }

    for( geometry_rigid_body& Body : Asset.Physics.RigidBodies )
    {
        if( Body.iParent >= 0 && Body.iParent < (s32)Asset.Physics.RigidBodies.size() )
            Body.ParentName = Asset.Physics.RigidBodies[Body.iParent].Name;
        if( Body.iBone >= 0 && Body.iBone < (s32)Asset.Skeleton.Bones.size() )
            Body.BoneName = Asset.Skeleton.Bones[Body.iBone].Name;
    }

    for( geometry_facet& Facet : Asset.Render.Facets )
    {
        if( !Facet.MeshName.empty() )
        {
            const s32 Mesh = FindNamed( Asset.Render.SubMeshes, Facet.MeshName );
            if( Mesh == -2 )
            {
                Diagnostics.Error( xfs( "facet references an ambiguous mesh name [%s]", Facet.MeshName.c_str() ),
                                   "GeometryIdentity", Facet.MeshName.c_str(), pSourceFile );
                bValid = FALSE;
            }
            else if( Mesh < 0 )
            {
                Diagnostics.Error( xfs( "facet references missing mesh name [%s]", Facet.MeshName.c_str() ),
                                   "GeometryIdentity", Facet.MeshName.c_str(), pSourceFile );
                bValid = FALSE;
            }
            else
            {
                Facet.iMesh = Mesh;
            }
        }
        else if( Facet.iMesh >= 0 && Facet.iMesh < (s32)Asset.Render.SubMeshes.size() )
        {
            Facet.MeshName = Asset.Render.SubMeshes[Facet.iMesh].Name;
        }

        if( !Facet.MaterialName.empty() )
        {
            const s32 Material = FindNamed( Asset.Materials.Items, Facet.MaterialName );
            if( Material == -2 )
            {
                Diagnostics.Error(
                    xfs( "facet references an ambiguous material name [%s]", Facet.MaterialName.c_str() ),
                    "GeometryIdentity", Facet.MaterialName.c_str(), pSourceFile );
                bValid = FALSE;
            }
            else if( Material < 0 )
            {
                Diagnostics.Error( xfs( "facet references missing material name [%s]", Facet.MaterialName.c_str() ),
                                   "GeometryIdentity", Facet.MaterialName.c_str(), pSourceFile );
                bValid = FALSE;
            }
            else
            {
                Facet.iMaterial = Material;
            }
        }
        else if( Facet.iMaterial >= 0 && Facet.iMaterial < (s32)Asset.Materials.Items.size() )
        {
            Facet.MaterialName = Asset.Materials.Items[Facet.iMaterial].Name;
        }

        if( Facet.MeshName.empty() )
        {
            Diagnostics.Error( xfs( "facet has no mesh identity (local mesh %d)", Facet.iMesh ), "GeometryIdentity",
                               pSourceFile, pSourceFile );
            bValid = FALSE;
        }
    }

    for( geometry_vertex& Vertex : Asset.Render.Vertices )
    {
        for( geometry_weight& Weight : Vertex.Weight )
        {
            if( !Weight.BoneName.empty() )
            {
                const s32 Bone = FindNamed( Asset.Skeleton.Bones, Weight.BoneName );
                if( Bone == -2 )
                {
                    Diagnostics.Error(
                        xfs( "vertex weight references an ambiguous bone name [%s]", Weight.BoneName.c_str() ),
                        "GeometryIdentity", "weight", pSourceFile );
                    bValid = FALSE;
                }
                else if( Bone < 0 )
                {
                    Diagnostics.Error(
                        xfs( "vertex weight references missing bone name [%s]", Weight.BoneName.c_str() ),
                        "GeometryIdentity", "weight", pSourceFile );
                    bValid = FALSE;
                }
                else
                {
                    Weight.iBone = Bone;
                }
            }
            else if( Weight.iBone >= 0 && Weight.iBone < (s32)Asset.Skeleton.Bones.size() )
            {
                Weight.BoneName = Asset.Skeleton.Bones[Weight.iBone].Name;
            }
        }
    }

    return bValid;
}
