//=========================================================================
//
//  GltfReader.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GltfReader.hpp"
#include "GeometrySource.hpp"
#include "Render/Material_Prefs.hpp"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "cgltf.h"

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

struct skeleton_bone
{
    std::string Name;
    s32 Parent;
    vector3 Scale;
    quaternion Rotation;
    vector3 Position;
};

//-------------------------------------------------------------------------

struct mesh_build
{
    std::vector<geometry_vertex> Vertices;
    std::vector<geometry_facet> Facets;
    std::vector<geometry_sub_mesh> SubMeshes;
    cgltf_skin* pSkin;
    geometry_source::asset_role Role;
    f32 UnitScale;
};

//=========================================================================

xbool IsFinite( f32 Value )
{
    return std::isfinite( Value ) != 0;
}

//=========================================================================

vector3 ReadVector3( const cgltf_accessor* pAccessor, cgltf_size Index, xbool& bOK )
{
    cgltf_float Value[3] = { 0.0f, 0.0f, 0.0f };
    if( !pAccessor || pAccessor->count <= Index || !cgltf_accessor_read_float( pAccessor, Index, Value, 3 ) )
    {
        bOK = FALSE;
        return vector3( 0.0f, 0.0f, 0.0f );
    }
    vector3 Result( Value[0], Value[1], Value[2] );
    bOK = IsFinite( Value[0] ) && IsFinite( Value[1] ) && IsFinite( Value[2] );
    return Result;
}

//=========================================================================

vector2 ReadVector2( const cgltf_accessor* pAccessor, cgltf_size Index, xbool& bOK )
{
    cgltf_float Value[2] = { 0.0f, 0.0f };
    if( !pAccessor || pAccessor->count <= Index || !cgltf_accessor_read_float( pAccessor, Index, Value, 2 ) )
    {
        bOK = FALSE;
        return vector2( 0.0f, 0.0f );
    }
    vector2 Result( Value[0], Value[1] );
    bOK = IsFinite( Value[0] ) && IsFinite( Value[1] );
    return Result;
}

//=========================================================================

void ReadNodeTransform( const cgltf_node* pNode, vector3& Scale, quaternion& Rotation, vector3& Position )
{
    Scale.Set( 1.0f, 1.0f, 1.0f );
    Rotation.Identity();
    Position.Set( 0.0f, 0.0f, 0.0f );

    if( !pNode )
        return;

    if( pNode->has_matrix )
    {
        cgltf_float LocalArray[16];
        cgltf_node_transform_local( pNode, LocalArray );
        matrix4 Local;
        for( s32 Row = 0; Row < 4; ++Row )
            for( s32 Column = 0; Column < 4; ++Column )
                Local( Column, Row ) = LocalArray[Column * 4 + Row];
        Local.DecomposeSRT( Scale, Rotation, Position );
        return;
    }

    if( pNode->has_scale )
        Scale.Set( pNode->scale[0], pNode->scale[1], pNode->scale[2] );
    if( pNode->has_translation )
        Position.Set( pNode->translation[0], pNode->translation[1], pNode->translation[2] );
    if( pNode->has_rotation )
    {
        Rotation.X = pNode->rotation[0];
        Rotation.Y = pNode->rotation[1];
        Rotation.Z = pNode->rotation[2];
        Rotation.W = pNode->rotation[3];
    }
}

//=========================================================================

xbool BuildSkeleton( const cgltf_skin* pSkin, std::vector<skeleton_bone>& Bones )
{
    Bones.clear();
    if( !pSkin )
        return TRUE;

    Bones.resize( pSkin->joints_count );
    for( cgltf_size i = 0; i < pSkin->joints_count; ++i )
    {
        const cgltf_node* pNode = pSkin->joints[i];
        skeleton_bone& Bone = Bones[i];
        Bone.Name = ( pNode && pNode->name && pNode->name[0] ) ? pNode->name : "";
        Bone.Parent = -1;
        Bone.Scale.Set( 1.0f, 1.0f, 1.0f );
        Bone.Rotation.Identity();
        Bone.Position.Set( 0.0f, 0.0f, 0.0f );
        ReadNodeTransform( pNode, Bone.Scale, Bone.Rotation, Bone.Position );

        if( pNode && pNode->parent )
        {
            for( cgltf_size j = 0; j < pSkin->joints_count; ++j )
            {
                if( pSkin->joints[j] == pNode->parent )
                {
                    Bone.Parent = (s32)j;
                    break;
                }
            }
        }
    }

    return TRUE;
}

//=========================================================================

cgltf_skin* FindPrimarySkin( const cgltf_data* pData )
{
    if( !pData )
        return NULL;
    for( cgltf_size i = 0; i < pData->nodes_count; ++i )
    {
        if( pData->nodes[i].skin )
            return pData->nodes[i].skin;
    }
    return NULL;
}

//=========================================================================

const cgltf_accessor* FindAttribute( const cgltf_primitive& Primitive, cgltf_attribute_type Type, cgltf_int Index )
{
    return cgltf_find_accessor( &Primitive, Type, Index );
}

//=========================================================================

xbool ResolveTexturePath( const char* pSourceFile, const cgltf_material& Material, std::string& Path, s32& UVChannel )
{
    if( !Material.has_pbr_metallic_roughness )
        return FALSE;

    const cgltf_texture_view& View = Material.pbr_metallic_roughness.base_color_texture;
    if( !View.texture || !View.texture->image || !View.texture->image->uri )
        return FALSE;

    const char* pURI = View.texture->image->uri;
    if( !pURI[0] || x_strncmp( pURI, "data:", 5 ) == 0 )
        return FALSE;

    std::filesystem::path SourcePath( pSourceFile ? pSourceFile : "" );
    std::filesystem::path TexturePath = SourcePath.parent_path() / std::filesystem::path( pURI );
    Path = TexturePath.lexically_normal().generic_string();
    UVChannel = View.texcoord >= 0 ? View.texcoord : 0;
    return !Path.empty();
}

//=========================================================================

xbool BuildMaterial( const cgltf_material& Source, const char* pSourceFile, geometry_material& Destination,
                     geometry_texture& Texture )
{
    Destination = geometry_material();
    for( s32 i = 0; i < GEOMETRY_MATERIAL_MAX_MAPS; ++i )
    {
        Destination.Map[i].iTexture = -1;
        Destination.Map[i].nTextures = 0;
        Destination.Map[i].iUVChannel = 0;
        Destination.Map[i].UAddress = GEOMETRY_ADDRESS_WRAP;
        Destination.Map[i].VAddress = GEOMETRY_ADDRESS_WRAP;
        Destination.Map[i].FilterType = GEOMETRY_FILTER_BILINEAR;
    }

    Destination.Name = ( Source.name && Source.name[0] ) ? Source.name : "";
    Destination.Type = ( Source.alpha_mode == cgltf_alpha_mode_opaque ) ? Material_Diff : Material_Alpha;
    Destination.LightingType = GEOMETRY_LIGHTING_STATIC_AND_DYNAMIC;
    Destination.BlendType = GEOMETRY_BLEND_OVERWRITE;
    Destination.TintType = GEOMETRY_TINT_FINAL_OUTPUT;
    Destination.bTwoSided = Source.double_sided;
    Destination.nMaps = 1;

    s32 UVChannel = 0;
    std::string TexturePath;
    if( !ResolveTexturePath( pSourceFile, Source, TexturePath, UVChannel ) )
    {
        // Keep valid texture-less materials with no texture products.
        Destination.nMaps = 0;
        Destination.Map[0].iTexture = -1;
        Destination.Map[0].nTextures = 0;
        return TRUE;
    }

    Texture.FileName = TexturePath;
    Destination.Map[0].RGBASource = GEOMETRY_RGBA_RGB;
    Destination.Map[0].iUVChannel = UVChannel;
    Destination.Map[0].iTexture = 0;
    Destination.Map[0].nTextures = 1;
    return TRUE;
}

//=========================================================================

xbool ReadPrimitiveIndices( const cgltf_primitive& Primitive, std::vector<cgltf_size>& Indices )
{
    if( Primitive.indices )
    {
        Indices.resize( Primitive.indices->count );
        for( cgltf_size i = 0; i < Primitive.indices->count; ++i )
            Indices[i] = cgltf_accessor_read_index( Primitive.indices, i );
        return TRUE;
    }

    const cgltf_accessor* pPosition = FindAttribute( Primitive, cgltf_attribute_type_position, 0 );
    if( !pPosition )
        return FALSE;
    Indices.resize( pPosition->count );
    for( cgltf_size i = 0; i < pPosition->count; ++i )
        Indices[i] = i;
    return TRUE;
}

//=========================================================================

void AddTriangle( mesh_build& Mesh, s32 iMesh, s32 iPrimitive, s32 iSubMesh, s32 iSection, f32 WorldPixelSize,
                  s32 iMaterial, s32 A, s32 B, s32 C )
{
    geometry_facet Facet;
    Facet.iMesh = iMesh;
    Facet.iPrimitive = iPrimitive;
    Facet.iSubMesh = iSubMesh;
    Facet.iSection = iSection;
    Facet.WorldPixelSize = WorldPixelSize;
    Facet.nVertices = 3;
    Facet.iVertex[0] = A;
    Facet.iVertex[1] = B;
    Facet.iVertex[2] = C;
    Facet.iMaterial = iMaterial;
    Facet.Plane.Setup( Mesh.Vertices[A].Position, Mesh.Vertices[B].Position, Mesh.Vertices[C].Position );
    Mesh.Facets.push_back( Facet );
}

//=========================================================================

xbool ReadJsonInteger( const char* pExtras, const char* pKey, s32& Value, xbool& bFound )
{
    bFound = FALSE;
    const char* pValue = x_strstr( pExtras, pKey );
    if( !pValue )
        return TRUE;
    bFound = TRUE;
    pValue = x_strchr( pValue, ':' );
    if( !pValue )
        return FALSE;
    while( std::isspace( ( u8 ) * ++pValue ) )
    {
    }
    char* pEnd = NULL;
    const long Parsed = std::strtol( pValue, &pEnd, 10 );
    if( pEnd == pValue || Parsed < 0 || Parsed > S32_MAX )
        return FALSE;
    Value = (s32)Parsed;
    return TRUE;
}

//=========================================================================

xbool ReadJsonString( const char* pExtras, const char* pKey, std::string& Value, xbool& bFound )
{
    bFound = FALSE;
    const char* pValue = x_strstr( pExtras, pKey );
    if( !pValue )
        return TRUE;
    bFound = TRUE;
    pValue = x_strchr( pValue, ':' );
    if( !pValue )
        return FALSE;
    while( std::isspace( ( u8 ) * ++pValue ) )
    {
    }
    if( *pValue != '"' )
        return FALSE;
    ++pValue;
    Value.clear();
    for( ; *pValue; ++pValue )
    {
        if( *pValue == '"' )
            return TRUE;
        if( *pValue == '\\' )
        {
            ++pValue;
            if( !*pValue )
                return FALSE;
            switch( *pValue )
            {
            case '"':
                Value.push_back( '"' );
                break;
            case '\\':
                Value.push_back( '\\' );
                break;
            case '/':
                Value.push_back( '/' );
                break;
            case 'b':
                Value.push_back( '\b' );
                break;
            case 'f':
                Value.push_back( '\f' );
                break;
            case 'n':
                Value.push_back( '\n' );
                break;
            case 'r':
                Value.push_back( '\r' );
                break;
            case 't':
                Value.push_back( '\t' );
                break;
            default:
                return FALSE;
            }
        }
        else
        {
            Value.push_back( *pValue );
        }
    }
    return FALSE;
}

//=========================================================================

xbool ReadPrimitiveMetadata( const cgltf_primitive& Primitive, s32& iSubMesh, s32& iSection, std::string& SubMeshName,
                             std::string& SectionName, std::string& BoneName, f32& WorldPixelSize )
{
    iSubMesh = -1;
    iSection = -1;
    SubMeshName.clear();
    SectionName.clear();
    BoneName.clear();
    WorldPixelSize = 0.001f;
    const char* pExtras = Primitive.extras.data;
    if( !pExtras || !pExtras[0] )
        return TRUE;

    xbool bFound = FALSE;
    if( !ReadJsonInteger( pExtras, "\"area51_submesh\"", iSubMesh, bFound ) )
        return FALSE;
    if( !ReadJsonInteger( pExtras, "\"area51_section\"", iSection, bFound ) )
        return FALSE;
    if( !ReadJsonString( pExtras, "\"area51_submesh_name\"", SubMeshName, bFound ) )
        return FALSE;
    if( !ReadJsonString( pExtras, "\"area51_section_name\"", SectionName, bFound ) )
        return FALSE;
    if( !ReadJsonString( pExtras, "\"area51_bone_name\"", BoneName, bFound ) )
        return FALSE;

    const char* pPixelSize = x_strstr( pExtras, "\"area51_world_pixel_size\"" );
    if( pPixelSize )
    {
        pPixelSize = x_strchr( pPixelSize, ':' );
        if( !pPixelSize )
            return FALSE;
        char* pEnd = NULL;
        const f32 Value = std::strtof( pPixelSize + 1, &pEnd );
        if( pEnd == pPixelSize + 1 || !std::isfinite( Value ) || Value <= 0.0f )
            return FALSE;
        WorldPixelSize = Value;
    }
    return TRUE;
}

//=========================================================================

xbool AddPrimitive( const cgltf_primitive& Primitive, const cgltf_node& Node, const char* pSourceFile, mesh_build& Mesh,
                    s32 iSubMesh, s32 iPrimitive, s32 iMaterial )
{
    if( Primitive.type != cgltf_primitive_type_triangles && Primitive.type != cgltf_primitive_type_triangle_strip &&
        Primitive.type != cgltf_primitive_type_triangle_fan )
    {
        return FALSE;
    }

    const cgltf_accessor* pPosition = FindAttribute( Primitive, cgltf_attribute_type_position, 0 );
    const cgltf_accessor* pNormal = FindAttribute( Primitive, cgltf_attribute_type_normal, 0 );
    const cgltf_accessor* pUV = FindAttribute( Primitive, cgltf_attribute_type_texcoord, 0 );
    const cgltf_accessor* pJoints = FindAttribute( Primitive, cgltf_attribute_type_joints, 0 );
    const cgltf_accessor* pWeights = FindAttribute( Primitive, cgltf_attribute_type_weights, 0 );
    if( !pPosition || pPosition->count == 0 )
        return FALSE;
    if( Mesh.Role != geometry_source::ASSET_ROLE_COLLISION && ( !pNormal || !pUV ) )
        return FALSE;
    if( Mesh.Role != geometry_source::ASSET_ROLE_COLLISION &&
        ( Mesh.pSkin != NULL ) != ( pJoints != NULL && pWeights != NULL ) )
        return FALSE;
    if( iSubMesh < 0 || iSubMesh >= (s32)Mesh.SubMeshes.size() )
        return FALSE;

    s32 SubMeshIndex = -1;
    s32 SectionIndex = -1;
    std::string SubMeshName;
    std::string SectionName;
    std::string BoneName;
    f32 WorldPixelSize = 0.001f;
    if( !ReadPrimitiveMetadata( Primitive, SubMeshIndex, SectionIndex, SubMeshName, SectionName, BoneName,
                                WorldPixelSize ) )
        return FALSE;

    cgltf_float WorldArray[16];
    cgltf_node_transform_world( &Node, WorldArray );
    matrix4 World;
    for( s32 Row = 0; Row < 4; ++Row )
        for( s32 Column = 0; Column < 4; ++Column )
            World( Column, Row ) = WorldArray[Column * 4 + Row];

    const s32 BaseVertex = (s32)Mesh.Vertices.size();
    for( cgltf_size i = 0; i < pPosition->count; ++i )
    {
        xbool bOK = TRUE;
        geometry_vertex Vertex;
        Vertex.Position = ReadVector3( pPosition, i, bOK );
        if( pNormal )
            Vertex.Normal.push_back( ReadVector3( pNormal, i, bOK ) );
        if( pUV )
            Vertex.UV.push_back( ReadVector2( pUV, i, bOK ) );
        if( !bOK )
            return FALSE;

        if( Mesh.pSkin == NULL )
        {
            Vertex.Position = World.Transform( Vertex.Position );
            Vertex.Normal[0] = World.RotateVector( Vertex.Normal[0] );
        }
        if( !Vertex.Normal.empty() )
            Vertex.Normal[0].Normalize();
        Vertex.Position *= Mesh.UnitScale;
        Vertex.iFrame = -1;

        if( Mesh.pSkin )
        {
            cgltf_uint JointValues[4] = { 0, 0, 0, 0 };
            cgltf_float WeightValues[4] = { 0, 0, 0, 0 };
            if( !cgltf_accessor_read_uint( pJoints, i, JointValues, 4 ) ||
                !cgltf_accessor_read_float( pWeights, i, WeightValues, 4 ) )
            {
                return FALSE;
            }
            for( s32 j = 0; j < 4; ++j )
            {
                if( JointValues[j] >= Mesh.pSkin->joints_count || WeightValues[j] <= 0.0f )
                    continue;
                geometry_weight Weight;
                Weight.iBone = (s32)JointValues[j];
                Weight.Weight = WeightValues[j];
                Vertex.Weight.push_back( Weight );
            }
            if( Vertex.Weight.empty() )
                return FALSE;
        }
        else if( Mesh.Role != geometry_source::ASSET_ROLE_COLLISION )
        {
            // Give unskinned rigid geometry a synthetic root bone.
            geometry_weight Weight;
            Weight.iBone = 0;
            Weight.Weight = 1.0f;
            Vertex.Weight.push_back( Weight );
        }
        Mesh.Vertices.push_back( Vertex );
    }

    std::vector<cgltf_size> Indices;
    if( !ReadPrimitiveIndices( Primitive, Indices ) || Indices.size() < 3 )
        return FALSE;

    const usize Step = Primitive.type == cgltf_primitive_type_triangles ? 3 : 1;
    for( usize i = 0; i + 2 < Indices.size(); i += Step )
    {
        usize IndexA = i + 0;
        usize IndexB = i + 1;
        usize IndexC = i + 2;
        if( Primitive.type == cgltf_primitive_type_triangle_strip && ( i & 1 ) )
        {
            IndexA = i + 1;
            IndexB = i + 0;
        }
        if( Primitive.type == cgltf_primitive_type_triangle_fan )
        {
            IndexA = 0;
            IndexB = i + 1;
            IndexC = i + 2;
        }
        s32 A = BaseVertex + (s32)Indices[IndexA];
        s32 B = BaseVertex + (s32)Indices[IndexB];
        s32 C = BaseVertex + (s32)Indices[IndexC];
        if( A < BaseVertex || B < BaseVertex || C < BaseVertex || A >= (s32)Mesh.Vertices.size() ||
            B >= (s32)Mesh.Vertices.size() || C >= (s32)Mesh.Vertices.size() )
            return FALSE;
        AddTriangle( Mesh, iSubMesh, iPrimitive, SubMeshIndex, SectionIndex, WorldPixelSize, iMaterial, A, B, C );
        Mesh.Facets.back().SubMeshName = SubMeshName;
        Mesh.Facets.back().SectionName = SectionName;
        Mesh.Facets.back().BoneName = BoneName;
    }
    return TRUE;
}

//=========================================================================

xbool LoadGltfData( const char* pFileName, cgltf_data*& pData )
{
    pData = NULL;
    cgltf_options Options;
    x_memset( &Options, 0, sizeof( Options ) );
    if( cgltf_parse_file( &Options, pFileName, &pData ) != cgltf_result_success || !pData )
        return FALSE;
    if( cgltf_load_buffers( &Options, pData, pFileName ) != cgltf_result_success ||
        cgltf_validate( pData ) != cgltf_result_success )
    {
        cgltf_free( pData );
        pData = NULL;
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool LoadGltfMesh( const char* pFileName, geometry_asset& Asset, geometry_source::asset_role Role, f32 UnitScale )
{
    cgltf_data* pData = NULL;
    if( !LoadGltfData( pFileName, pData ) )
        return FALSE;

    xbool bOK = TRUE;
    mesh_build Build;
    Build.pSkin = Role == geometry_source::ASSET_ROLE_COLLISION ? NULL : FindPrimarySkin( pData );
    Build.Role = Role;
    Build.UnitScale = UnitScale > 0.0f ? UnitScale : 1.0f;

    if( Role != geometry_source::ASSET_ROLE_COLLISION )
    {
        for( cgltf_size i = 0; i < pData->materials_count; ++i )
        {
            geometry_material Material;
            geometry_texture Texture;
            if( !BuildMaterial( pData->materials[i], pFileName, Material, Texture ) )
            {
                bOK = FALSE;
                break;
            }
        }
    }

    if( bOK )
    {
        for( cgltf_size i = 0; i < pData->nodes_count && bOK; ++i )
        {
            if( pData->nodes[i].parent )
                continue;
            const cgltf_node& Node = pData->nodes[i];
            std::vector<const cgltf_node*> Stack;
            Stack.push_back( &Node );
            while( !Stack.empty() && bOK )
            {
                const cgltf_node* pNode = Stack.back();
                Stack.pop_back();
                if( pNode->mesh )
                {
                    if( Role != geometry_source::ASSET_ROLE_COLLISION && pNode->skin && Build.pSkin &&
                        pNode->skin != Build.pSkin )
                    {
                        bOK = FALSE;
                        break;
                    }
                    geometry_sub_mesh SubMesh;
                    SubMesh.Name = ( pNode->mesh->name && pNode->mesh->name[0] ) ? pNode->mesh->name : "";
                    SubMesh.nBones = Build.pSkin ? (s32)Build.pSkin->joints_count
                                                 : ( Role == geometry_source::ASSET_ROLE_COLLISION ? 0 : 1 );
                    const s32 iSubMesh = (s32)Build.SubMeshes.size();
                    if( SubMesh.Name.empty() )
                        SubMesh.Name = (const char*)xfs( "Mesh_%d", iSubMesh );
                    Build.SubMeshes.push_back( SubMesh );
                    const usize nFacetsBefore = Build.Facets.size();
                    for( cgltf_size p = 0; p < pNode->mesh->primitives_count; ++p )
                    {
                        const cgltf_primitive& Primitive = pNode->mesh->primitives[p];
                        const s32 iMaterial =
                            Role == geometry_source::ASSET_ROLE_COLLISION
                                ? -1
                                : ( Primitive.material ? (s32)( Primitive.material - pData->materials ) : -1 );
                        if( ( Role != geometry_source::ASSET_ROLE_COLLISION &&
                              ( iMaterial < 0 || iMaterial >= (s32)pData->materials_count ) ) ||
                            !AddPrimitive( Primitive, *pNode, pFileName, Build, iSubMesh, (s32)p,
                                           Role == geometry_source::ASSET_ROLE_COLLISION ? -1 : iMaterial ) )
                        {
                            bOK = FALSE;
                            break;
                        }
                    }
                    if( bOK && Build.pSkin )
                    {
                        // Store the highest bone used by this mesh, not the skin-table size.
                        s32 nUsedBones = 0;
                        for( usize iFacet = nFacetsBefore; iFacet < Build.Facets.size(); ++iFacet )
                        {
                            const geometry_facet& Facet = Build.Facets[iFacet];
                            for( s32 iVertex = 0; iVertex < 3; ++iVertex )
                            {
                                const s32 VertexIndex = Facet.iVertex[iVertex];
                                if( VertexIndex < 0 || VertexIndex >= (s32)Build.Vertices.size() )
                                    continue;
                                for( const geometry_weight& Weight : Build.Vertices[VertexIndex].Weight )
                                {
                                    if( Weight.iBone >= 0 )
                                        nUsedBones = x_max( nUsedBones, Weight.iBone + 1 );
                                }
                            }
                        }
                        Build.SubMeshes[iSubMesh].nBones = nUsedBones;
                    }
                    if( bOK && Build.Facets.size() == nFacetsBefore )
                        bOK = FALSE;
                }
                for( cgltf_size c = 0; c < pNode->children_count; ++c )
                    Stack.push_back( pNode->children[c] );
            }
        }
    }

    std::vector<skeleton_bone> Bones;
    if( Role != geometry_source::ASSET_ROLE_COLLISION )
        BuildSkeleton( Build.pSkin, Bones );
    if( Role != geometry_source::ASSET_ROLE_COLLISION && !Build.pSkin )
    {
        skeleton_bone Root;
        Root.Name = "GeomRoot";
        Root.Parent = -1;
        Root.Scale.Set( 1.0f, 1.0f, 1.0f );
        Root.Rotation.Identity();
        Root.Position.Set( 0.0f, 0.0f, 0.0f );
        Bones.push_back( Root );
    }

    if( bOK && Build.Facets.empty() )
        bOK = FALSE;

    if( bOK )
    {
        Asset.Clear();
        Asset.Source.SourceFile = pFileName;
        Asset.Render.Vertices = std::move( Build.Vertices );
        Asset.Render.Facets = std::move( Build.Facets );
        Asset.Render.SubMeshes = std::move( Build.SubMeshes );
        if( Role != geometry_source::ASSET_ROLE_COLLISION )
        {
            Asset.Materials.Items.resize( pData->materials_count );
            Asset.Materials.Textures.resize( pData->materials_count );
            for( s32 i = 0; i < (s32)pData->materials_count; ++i )
            {
                if( !BuildMaterial( pData->materials[i], pFileName, Asset.Materials.Items[i],
                                    Asset.Materials.Textures[i] ) )
                {
                    bOK = FALSE;
                    break;
                }
                if( !Asset.Materials.Textures[i].FileName.empty() )
                {
                    Asset.Materials.Items[i].Map[0].iTexture = i;
                    Asset.Materials.Items[i].nMaps = 1;
                }
                if( Asset.Materials.Items[i].Name.empty() )
                    Asset.Materials.Items[i].Name = (const char*)xfs( "Material_%d", i );
            }
        }
        Asset.Skeleton.Bones.resize( Bones.size() );
        for( s32 i = 0; i < (s32)Bones.size(); ++i )
        {
            geometry_bone& Bone = Asset.Skeleton.Bones[i];
            Bone.Name = Bones[i].Name.empty() ? (const char*)xfs( "Bone_%d", i ) : Bones[i].Name;
            Bone.iParent = Bones[i].Parent;
            Bone.Scale = Bones[i].Scale;
            Bone.Rotation = Bones[i].Rotation;
            Bone.Position = Bones[i].Position;
            Bone.InverseBindMatrix.Identity();
            Bone.nChildren = 0;
            Bone.iRigidBody = -1;
            Bone.LODGroup = -1;
        }
        for( s32 i = 0; i < (s32)Asset.Skeleton.Bones.size(); ++i )
        {
            if( Asset.Skeleton.Bones[i].iParent >= 0 &&
                Asset.Skeleton.Bones[i].iParent < (s32)Asset.Skeleton.Bones.size() )
            {
                ++Asset.Skeleton.Bones[Asset.Skeleton.Bones[i].iParent].nChildren;
            }
        }
    }

    cgltf_free( pData );
    if( !bOK )
        Asset.Clear();
    return bOK;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool gltf_reader::LoadMesh( const char* pFileName, geometry_asset& Asset, geometry_source::asset_role Role,
                             f32 UnitScale, std::string& Error )
{
    if( !LoadGltfMesh( pFileName, Asset, Role, UnitScale ) )
    {
        Error = (const char*)xfs( "glTF reader could not read [%s]", pFileName ? pFileName : "" );
        return FALSE;
    }
    return TRUE;
}
