//=========================================================================
//
//  GeometrySemanticDiff.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometrySemanticDiff.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

namespace resource_tool::geometry
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

const f32 kEpsilon = 0.0001f;
const f32 kPhysicsEpsilon = 0.001f;
const f32 kSemanticQuantization = 0.01f;

xbool Fail( std::string& Error, const std::string& Path )
{
    Error = "Geometry semantic mismatch at ";
    Error += Path.c_str();
    return FALSE;
}

//=========================================================================

xbool SameFloat( f32 A, f32 B )
{
    return std::fabs( A - B ) <= kEpsilon;
}

//=========================================================================

xbool SamePhysicsFloat( f32 A, f32 B )
{
    return std::fabs( A - B ) <= kPhysicsEpsilon;
}

//=========================================================================

xbool SameVector2( const vector2& A, const vector2& B )
{
    return A.Difference( B ) <= kEpsilon;
}

//=========================================================================

xbool SameVector3( const vector3& A, const vector3& B )
{
    return A.Difference( B ) <= kEpsilon;
}

//=========================================================================

xbool SameQuaternion( const quaternion& A, const quaternion& B )
{
    const f32 Direct = ( A.X - B.X ) * ( A.X - B.X ) + ( A.Y - B.Y ) * ( A.Y - B.Y ) + ( A.Z - B.Z ) * ( A.Z - B.Z ) +
                       ( A.W - B.W ) * ( A.W - B.W );
    const f32 Negated = ( A.X + B.X ) * ( A.X + B.X ) + ( A.Y + B.Y ) * ( A.Y + B.Y ) + ( A.Z + B.Z ) * ( A.Z + B.Z ) +
                        ( A.W + B.W ) * ( A.W + B.W );
    return x_min( Direct, Negated ) <= kEpsilon * kEpsilon;
}

//=========================================================================

xbool SameBBox( const bbox& A, const bbox& B )
{
    return SameVector3( A.Min, B.Min ) && SameVector3( A.Max, B.Max );
}

//=========================================================================

template <typename T>
xbool SameCount( const std::vector<T>& A, const std::vector<T>& B, std::string& Error, const std::string& Path )
{
    return A.size() == B.size() || !Fail( Error, Path + ".count" );
}

//=========================================================================

std::string FormatIndex( usize Value )
{
    return std::string( (const char*)xfs( "%llu", (u64)Value ) );
}

//=========================================================================

std::string FormatInteger( s32 Value )
{
    return std::string( (const char*)xfs( "%d", Value ) );
}

//=========================================================================

std::string FormatInteger( s64 Value )
{
    return std::string( (const char*)xfs( "%lld", (long long)Value ) );
}

//=========================================================================

std::string Quantized( f32 Value )
{
    if( !std::isfinite( Value ) )
        return "nan";
    return FormatInteger( (s64)std::llround( (f64)Value / kSemanticQuantization ) );
}

//=========================================================================

std::string TextureReferenceKey( const std::string& Value )
{
    std::string Result;
    Result.reserve( Value.size() );
    xbool bSkipChecksum = FALSE;
    for( char Character : Value )
    {
        if( Character == '[' )
        {
            bSkipChecksum = TRUE;
            continue;
        }
        if( bSkipChecksum )
        {
            if( Character == ']' )
                bSkipChecksum = FALSE;
            continue;
        }
        Result += (char)x_tolower( (u8)Character );
    }
    return Result;
}

//=========================================================================

std::string VertexKey( const geometry_vertex& Vertex )
{
    std::ostringstream Result;
    Result << "p:" << Quantized( Vertex.Position.GetX() ) << ',' << Quantized( Vertex.Position.GetY() ) << ','
           << Quantized( Vertex.Position.GetZ() );
    Result << "|uv:" << Vertex.UV.size();
    for( const vector2& UV : Vertex.UV )
        Result << ':' << Quantized( UV.X ) << ',' << Quantized( UV.Y );
    Result << "|n:" << Vertex.Normal.size();
    for( const vector3& Normal : Vertex.Normal )
        Result << ':' << Quantized( Normal.GetX() ) << ',' << Quantized( Normal.GetY() ) << ','
               << Quantized( Normal.GetZ() );

    std::vector<std::string> Weights;
    for( const geometry_weight& Weight : Vertex.Weight )
    {
        const std::string Bone = Weight.BoneName.empty() ? "#" + FormatInteger( Weight.iBone ) : Weight.BoneName;
        Weights.push_back( Bone + ":" + Quantized( Weight.Weight ) );
    }
    std::sort( Weights.begin(), Weights.end() );
    Result << "|w:";
    for( const std::string& Weight : Weights )
        Result << Weight << ';';
    return Result.str();
}

//=========================================================================

std::string FacetKey( const geometry_render_model& Model, const geometry_facet& Facet )
{
    std::ostringstream Result;
    Result << Facet.MeshName << '|' << Facet.SubMeshName << '|' << Facet.SectionName << '|' << Facet.MaterialName << '|'
           << Facet.BoneName << '|' << Quantized( Facet.WorldPixelSize );
    for( s32 iVertex = 0; iVertex < 3; ++iVertex )
    {
        const s32 Index = Facet.iVertex[iVertex];
        if( Index < 0 || Index >= (s32)Model.Vertices.size() )
            return std::string();
        Result << "|v:" << VertexKey( Model.Vertices[Index] );
    }
    return Result.str();
}

//=========================================================================

template <typename T>
xbool CompareSortedKeys( const std::vector<std::string>& Left, const std::vector<std::string>& Right,
                         std::string& Error, const char* pPath )
{
    (void)sizeof( T );
    if( Left.size() != Right.size() )
        return Fail( Error, std::string( pPath ) + ".count" );
    for( usize i = 0; i < Left.size(); ++i )
    {
        if( Left[i] != Right[i] )
            return Fail( Error, std::string( pPath ) + "[" + FormatIndex( i ) + "]" );
    }
    return TRUE;
}

//=========================================================================

xbool CompareRender( const geometry_render_model& A, const geometry_render_model& B, std::string& Error )
{
    std::vector<std::string> MeshesA;
    std::vector<std::string> MeshesB;
    for( const geometry_sub_mesh& Mesh : A.SubMeshes )
        MeshesA.push_back( Mesh.Name + "|" + FormatInteger( Mesh.nBones ) );
    for( const geometry_sub_mesh& Mesh : B.SubMeshes )
        MeshesB.push_back( Mesh.Name + "|" + FormatInteger( Mesh.nBones ) );
    std::sort( MeshesA.begin(), MeshesA.end() );
    std::sort( MeshesB.begin(), MeshesB.end() );
    if( !CompareSortedKeys<s32>( MeshesA, MeshesB, Error, "render.meshes" ) )
        return FALSE;

    std::vector<std::string> FacetsA;
    std::vector<std::string> FacetsB;
    for( const geometry_facet& Facet : A.Facets )
    {
        const std::string Key = FacetKey( A, Facet );
        if( Key.empty() )
            return Fail( Error, "render.facets.invalidVertex" );
        FacetsA.push_back( Key );
    }
    for( const geometry_facet& Facet : B.Facets )
    {
        const std::string Key = FacetKey( B, Facet );
        if( Key.empty() )
            return Fail( Error, "render.facets.invalidVertex" );
        FacetsB.push_back( Key );
    }
    std::sort( FacetsA.begin(), FacetsA.end() );
    std::sort( FacetsB.begin(), FacetsB.end() );
    if( FacetsA.size() == FacetsB.size() )
    {
        for( usize i = 0; i < FacetsA.size(); ++i )
        {
            if( FacetsA[i] != FacetsB[i] )
            {
                Error = "Geometry semantic mismatch at render.facets[";
                Error += (const char*)xfs( "%d", (s32)i );
                Error += "] left=";
                Error += FacetsA[i].c_str();
                Error += " right=";
                Error += FacetsB[i].c_str();
                return FALSE;
            }
        }
    }
    return CompareSortedKeys<s32>( FacetsA, FacetsB, Error, "render.facets" );
}

//=========================================================================

std::string MaterialKey( const geometry_material& Material )
{
    std::ostringstream Result;
    Result << Material.Name << '|' << Material.Type << '|' << Material.ProductFlags << '|' << (s32)Material.bTwoSided
           << '|' << (s32)Material.bPunchthrough << '|' << Quantized( Material.DetailScale ) << '|'
           << Quantized( Material.FixedAlpha ) << '|';

    std::vector<std::string> Textures;
    for( const geometry_texture& Texture : Material.Textures )
    {
        Textures.push_back( FormatInteger( (s32)Texture.Slot ) + '|' + Texture.Name + '|' +
                            TextureReferenceKey( Texture.FileName ) );
    }
    std::sort( Textures.begin(), Textures.end() );
    for( const std::string& Texture : Textures )
        Result << Texture << ';';
    Result << "|uv:" << Material.UVAnimation.Type << ':' << Material.UVAnimation.StartFrame << ':'
           << Material.UVAnimation.FPS << ':' << Material.UVAnimation.Keys.size();
    for( const geometry_uv_key& Key : Material.UVAnimation.Keys )
        Result << ':' << (s32)Key.U << ',' << (s32)Key.V;
    return Result.str();
}

//=========================================================================

xbool CompareMaterials( const geometry_material_model& A, const geometry_material_model& B, std::string& Error )
{
    std::vector<std::string> ItemsA;
    std::vector<std::string> ItemsB;
    for( const geometry_material& Material : A.Items )
        ItemsA.push_back( MaterialKey( Material ) );
    for( const geometry_material& Material : B.Items )
        ItemsB.push_back( MaterialKey( Material ) );
    std::sort( ItemsA.begin(), ItemsA.end() );
    std::sort( ItemsB.begin(), ItemsB.end() );
    if( !CompareSortedKeys<s32>( ItemsA, ItemsB, Error, "materials.items" ) )
    {
        for( usize i = 0; i < ItemsA.size() && i < ItemsB.size(); ++i )
        {
            if( ItemsA[i] != ItemsB[i] )
            {
                Error = "Geometry semantic mismatch at materials.items[";
                Error += (const char*)xfs( "%d", (s32)i );
                Error += "] left=";
                Error += ItemsA[i].c_str();
                Error += " right=";
                Error += ItemsB[i].c_str();
                break;
            }
        }
        return FALSE;
    }

    std::vector<std::string> TexturesA;
    std::vector<std::string> TexturesB;
    for( const geometry_texture& Texture : A.Textures )
        TexturesA.push_back( Texture.Name + "|" + TextureReferenceKey( Texture.FileName ) );
    for( const geometry_texture& Texture : B.Textures )
        TexturesB.push_back( Texture.Name + "|" + TextureReferenceKey( Texture.FileName ) );
    std::sort( TexturesA.begin(), TexturesA.end() );
    std::sort( TexturesB.begin(), TexturesB.end() );
    if( !CompareSortedKeys<s32>( TexturesA, TexturesB, Error, "materials.textures" ) )
        return FALSE;

    std::vector<std::string> VirtualTexturesA;
    std::vector<std::string> VirtualTexturesB;
    for( const geometry_virtual_texture& Texture : A.VirtualTextures )
        VirtualTexturesA.push_back( Texture.Name + "|" + Texture.OverrideFileName );
    for( const geometry_virtual_texture& Texture : B.VirtualTextures )
        VirtualTexturesB.push_back( Texture.Name + "|" + Texture.OverrideFileName );
    std::sort( VirtualTexturesA.begin(), VirtualTexturesA.end() );
    std::sort( VirtualTexturesB.begin(), VirtualTexturesB.end() );
    return CompareSortedKeys<s32>( VirtualTexturesA, VirtualTexturesB, Error, "materials.virtualTextures" );
}

//=========================================================================

xbool CompareSkeleton( const geometry_skeleton_model& A, const geometry_skeleton_model& B, std::string& Error )
{
    if( !SameCount( A.Bones, B.Bones, Error, "skeleton.bones" ) )
        return FALSE;
    for( usize i = 0; i < A.Bones.size(); ++i )
    {
        const geometry_bone& Left = A.Bones[i];
        const geometry_bone& Right = B.Bones[i];
        if( Left.Name != Right.Name || Left.ParentName != Right.ParentName || Left.iParent != Right.iParent ||
            Left.iRigidBody != Right.iRigidBody || Left.HitLocation != Right.HitLocation ||
            !SameVector3( Left.Position, Right.Position ) || !SameVector3( Left.Scale, Right.Scale ) ||
            !SameQuaternion( Left.Rotation, Right.Rotation ) || !SameBBox( Left.BBox, Right.BBox ) )
        {
            Error = "Geometry semantic mismatch at skeleton.bones[";
            Error += FormatIndex( i ).c_str();
            Error += "] left=";
            Error += Left.Name.c_str();
            Error += " parent=";
            Error += Left.ParentName.c_str();
            Error += " rot=";
            Error +=
                (const char*)xfs( "%g,%g,%g,%g", Left.Rotation.X, Left.Rotation.Y, Left.Rotation.Z, Left.Rotation.W );
            Error += " pos=";
            Error += (const char*)xfs( "%g,%g,%g", Left.Position.GetX(), Left.Position.GetY(), Left.Position.GetZ() );
            Error += " right=";
            Error += Right.Name.c_str();
            Error += " parent=";
            Error += Right.ParentName.c_str();
            Error += " rot=";
            Error += (const char*)xfs( "%g,%g,%g,%g", Right.Rotation.X, Right.Rotation.Y, Right.Rotation.Z,
                                       Right.Rotation.W );
            Error += " pos=";
            Error +=
                (const char*)xfs( "%g,%g,%g", Right.Position.GetX(), Right.Position.GetY(), Right.Position.GetZ() );
            return FALSE;
        }
    }
    return TRUE;
}

//=========================================================================

xbool CompareLods( const geometry_lod_model& A, const geometry_lod_model& B, std::string& Error )
{
    if( !SameCount( A.Groups, B.Groups, Error, "lod.groups" ) )
        return FALSE;
    for( usize i = 0; i < A.Groups.size(); ++i )
    {
        if( A.Groups[i].Name != B.Groups[i].Name || A.Groups[i].Levels.size() != B.Groups[i].Levels.size() )
            return Fail( Error, "lod.groups[" + FormatIndex( i ) + "]" );
        for( usize j = 0; j < A.Groups[i].Levels.size(); ++j )
        {
            if( A.Groups[i].Levels[j].ScreenSize != B.Groups[i].Levels[j].ScreenSize ||
                A.Groups[i].Levels[j].MeshNames != B.Groups[i].Levels[j].MeshNames )
                return Fail( Error, "lod.groups[" + FormatIndex( i ) + "].levels" );
        }
    }
    return TRUE;
}

//=========================================================================

xbool ComparePhysics( const geometry_physics_model& A, const geometry_physics_model& B, std::string& Error )
{
    if( !SameCount( A.RigidBodies, B.RigidBodies, Error, "physics.rigidBodies" ) )
        return FALSE;
    for( usize i = 0; i < A.RigidBodies.size(); ++i )
    {
        const geometry_rigid_body& Left = A.RigidBodies[i];
        const geometry_rigid_body& Right = B.RigidBodies[i];
        const std::string Path = "physics.rigidBodies[" + FormatIndex( i ) + "]";
        if( Left.Name != Right.Name )
            return Fail( Error, Path + ".name" );
        if( Left.Type != Right.Type )
            return Fail( Error, Path + ".type" );
        if( Left.ParentName != Right.ParentName || Left.iParent != Right.iParent )
            return Fail( Error, Path + ".parent" );
        if( Left.BoneName != Right.BoneName || Left.iBone != Right.iBone )
            return Fail( Error, Path + ".bone" );
        if( Left.Flags != Right.Flags )
            return Fail( Error, Path + ".flags" );
        if( Left.CollisionMask != Right.CollisionMask )
            return Fail( Error, Path + ".collisionMask" );
        if( !SameFloat( Left.Mass, Right.Mass ) )
            return Fail( Error, Path + ".mass" );
        if( !SameVector3( Left.PivotPosition, Right.PivotPosition ) )
            return Fail( Error, Path + ".pivotPosition" );
        if( !SameVector3( Left.PivotScale, Right.PivotScale ) )
            return Fail( Error, Path + ".pivotScale" );
        if( !SameQuaternion( Left.PivotRotation, Right.PivotRotation ) )
            return Fail( Error, Path + ".pivotRotation" );
        if( !SameVector3( Left.BodyScale, Right.BodyScale ) )
            return Fail( Error, Path + ".bodyScale" );
        if( !SameQuaternion( Left.BodyRotation, Right.BodyRotation ) )
            return Fail( Error, Path + ".bodyRotation" );
        if( !SameVector3( Left.BodyPosition, Right.BodyPosition ) )
            return Fail( Error, Path + ".bodyPosition" );
        if( !SameFloat( Left.Radius, Right.Radius ) )
            return Fail( Error, Path + ".radius" );
        if( !SameFloat( Left.Width, Right.Width ) )
        {
            Error = Path.c_str();
            Error += ".width left=";
            Error += (const char*)xfs( "%g right=%g", Left.Width, Right.Width );
            return FALSE;
        }
        if( !SameFloat( Left.Height, Right.Height ) )
            return Fail( Error, Path + ".height" );
        if( !SameFloat( Left.Length, Right.Length ) )
            return Fail( Error, Path + ".length" );
        for( s32 iDOF = 0; iDOF < 6; ++iDOF )
        {
            if( Left.DOF[iDOF].bActive != Right.DOF[iDOF].bActive ||
                Left.DOF[iDOF].bLimited != Right.DOF[iDOF].bLimited ||
                !SamePhysicsFloat( Left.DOF[iDOF].Min, Right.DOF[iDOF].Min ) ||
                !SamePhysicsFloat( Left.DOF[iDOF].Max, Right.DOF[iDOF].Max ) )
            {
                Error = (const char*)xfs( "%s.dof[%d] left=", Path.c_str(), iDOF );
                Error += (const char*)xfs( "%d,%d,%.9g,%.9g", Left.DOF[iDOF].bActive, Left.DOF[iDOF].bLimited,
                                           Left.DOF[iDOF].Min, Left.DOF[iDOF].Max );
                Error += " right=";
                Error += (const char*)xfs( "%d,%d,%.9g,%.9g", Right.DOF[iDOF].bActive, Right.DOF[iDOF].bLimited,
                                           Right.DOF[iDOF].Min, Right.DOF[iDOF].Max );
                return FALSE;
            }
        }
    }
    return TRUE;
}

//=========================================================================

xbool CompareCollision( const geometry_collision_model& A, const geometry_collision_model& B, std::string& Error )
{
    if( A.HasCollisionBBox != B.HasCollisionBBox ||
        ( A.HasCollisionBBox && !SameBBox( A.CollisionBBox, B.CollisionBBox ) ) )
        return Fail( Error, "collision.bbox" );
    if( A.Meshes.size() != B.Meshes.size() || A.Clusters.size() != B.Clusters.size() )
    {
        Error = "Geometry semantic mismatch at collision.count leftMeshes=";
        Error += FormatIndex( A.Meshes.size() ).c_str();
        Error += " rightMeshes=";
        Error += FormatIndex( B.Meshes.size() ).c_str();
        Error += " leftClusters=";
        Error += FormatIndex( A.Clusters.size() ).c_str();
        Error += " rightClusters=";
        Error += FormatIndex( B.Clusters.size() ).c_str();
        return FALSE;
    }
    for( usize i = 0; i < A.Meshes.size(); ++i )
    {
        if( A.Meshes[i].Name != B.Meshes[i].Name || A.Meshes[i].Indices != B.Meshes[i].Indices ||
            A.Meshes[i].Vertices.size() != B.Meshes[i].Vertices.size() ||
            A.Meshes[i].Normals.size() != B.Meshes[i].Normals.size() )
            return Fail( Error, "collision.meshes[" + FormatIndex( i ) + "]" );
        for( usize j = 0; j < A.Meshes[i].Vertices.size(); ++j )
            if( !SameVector3( A.Meshes[i].Vertices[j], B.Meshes[i].Vertices[j] ) )
                return Fail( Error, "collision.meshes[" + FormatIndex( i ) + "].vertices" );
        for( usize j = 0; j < A.Meshes[i].Normals.size(); ++j )
            if( !SameVector3( A.Meshes[i].Normals[j], B.Meshes[i].Normals[j] ) )
                return Fail( Error, "collision.meshes[" + FormatIndex( i ) + "].normals" );
    }
    for( usize i = 0; i < A.Clusters.size(); ++i )
    {
        if( A.Clusters[i].MeshName != B.Clusters[i].MeshName || A.Clusters[i].BoneName != B.Clusters[i].BoneName ||
            A.Clusters[i].SectionName != B.Clusters[i].SectionName || A.Clusters[i].High != B.Clusters[i].High ||
            !SameBBox( A.Clusters[i].BBox, B.Clusters[i].BBox ) || A.Clusters[i].Mesh != B.Clusters[i].Mesh ||
            A.Clusters[i].Bone != B.Clusters[i].Bone || A.Clusters[i].Section != B.Clusters[i].Section ||
            A.Clusters[i].Offset != B.Clusters[i].Offset || A.Clusters[i].Count != B.Clusters[i].Count ||
            A.Clusters[i].VectorOffset != B.Clusters[i].VectorOffset || A.Clusters[i].Points != B.Clusters[i].Points ||
            A.Clusters[i].Normals != B.Clusters[i].Normals || A.Clusters[i].QuadOffset != B.Clusters[i].QuadOffset ||
            A.Clusters[i].SoundType != B.Clusters[i].SoundType || A.Clusters[i].Flags != B.Clusters[i].Flags ||
            A.Clusters[i].TriangleOffsets != B.Clusters[i].TriangleOffsets ||
            A.Clusters[i].QuadFlags != B.Clusters[i].QuadFlags )
            return Fail( Error, "collision.clusters[" + FormatIndex( i ) + "]" );
    }
    return TRUE;
}

//=========================================================================

xbool CompareEngineMetadata( const geometry_engine_metadata& A, const geometry_engine_metadata& B, std::string& Error )
{
    if( A.BoneMasks.size() != B.BoneMasks.size() || A.Properties.size() != B.Properties.size() )
        return Fail( Error, "compiled.metadata.count" );
    for( usize i = 0; i < A.BoneMasks.size(); ++i )
    {
        const geometry_bone_mask& Left = A.BoneMasks[i];
        const geometry_bone_mask& Right = B.BoneMasks[i];
        if( Left.Name != Right.Name || Left.Weights.size() != Right.Weights.size() )
            return Fail( Error, "compiled.boneMasks[" + FormatIndex( i ) + "]" );
        for( usize j = 0; j < Left.Weights.size(); ++j )
        {
            if( Left.Weights[j].BoneName != Right.Weights[j].BoneName ||
                Left.Weights[j].iBone != Right.Weights[j].iBone ||
                !SameFloat( Left.Weights[j].Value, Right.Weights[j].Value ) )
                return Fail( Error, "compiled.boneMasks.weights" );
        }
    }
    for( usize i = 0; i < A.Properties.size(); ++i )
    {
        if( A.Properties[i].Section != B.Properties[i].Section || A.Properties[i].Name != B.Properties[i].Name ||
            A.Properties[i].Type != B.Properties[i].Type || A.Properties[i].Value != B.Properties[i].Value )
            return Fail( Error, "compiled.properties[" + FormatIndex( i ) + "]" );
    }
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool GeometrySemanticEqual( const geometry_asset& Expected, const geometry_asset& Actual, std::string& Error,
                             const geometry_semantic_diff_options& Options )
{
    Error.clear();
    if( !CompareRender( Expected.Render, Actual.Render, Error ) ||
        !CompareMaterials( Expected.Materials, Actual.Materials, Error ) ||
        !CompareSkeleton( Expected.Skeleton, Actual.Skeleton, Error ) ||
        !CompareLods( Expected.LODs, Actual.LODs, Error ) ||
        !ComparePhysics( Expected.Physics, Actual.Physics, Error ) )
        return FALSE;
    if( Options.CompareCollision && !CompareCollision( Expected.Collision, Actual.Collision, Error ) )
        return FALSE;
    if( Options.CompareProductMetadata &&
        !CompareEngineMetadata( Expected.EngineMetadata, Actual.EngineMetadata, Error ) )
        return FALSE;
    return TRUE;
}

} // namespace resource_tool::geometry
