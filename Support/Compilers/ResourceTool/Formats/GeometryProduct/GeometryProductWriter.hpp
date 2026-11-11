//=========================================================================
//
//  GeometryProductWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_WRITER_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_WRITER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "Model/GeometryAsset.hpp"
#include "TextureInfo.hpp"
#include "Render/geom.hpp"
#include "Render/CollisionVolume.hpp"
#include "Auxiliary/MiscUtils/dictionary.hpp"
#include "GeometryProductEncodingPipeline.hpp"
#include "GeometryProductEncodingTypes.hpp"
#include "BitmapProduct.hpp"

#include <memory>

struct skin_geom;
struct rigid_geom;

namespace resource_tool
{
class diagnostic_log;
namespace bitmap
{
class product_sink;
}
namespace geometry_product_format
{
class geometry_product_prepare_stage;
class geometry_product_model_stage;
class geometry_product_stage;
} // namespace geometry_product_format
} // namespace resource_tool

struct geometry_product_compile_options
{
    const char* OutputFile = NULL;
    const char* TexturePath = NULL;
    const char* FastCollision = NULL;
    const geometry_asset* FastCollisionAsset = NULL;
    xbool DoCollision = TRUE;
    xbool Verbose = FALSE;
    resource_tool::bitmap::product_sink* BitmapSink = NULL;
};

//=========================================================================
//  MATERIAL CONSTANTS
//=========================================================================

enum material_parameter
{
    DETAIL_SCALE,
    ENV_TYPE,
    ENV_BLEND,
    FIXED_ALPHA,
    FORCE_ZFILL,
    USE_DIFFUSE,
};

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_writer
{
public:
    ~geometry_product_writer( void );

    using product_kind = resource_tool::geometry_product_format::geometry_product_kind;

    geometry_product_writer( resource_tool::diagnostic_log* pDiagnostics );

private:
    void SetUserInfo( const char* pUserName, const char* pComputerName );
    void SetExportDate( s32 Month, s32 Day, s32 Year );
    void ReportWarning( const char* pWarning );
    void ReportError( const char* pError );
    void ThrowError( const char* pError );

public:
    void Export( const geometry_asset& Asset, const geometry_asset* pPhysicsAsset, product_kind Kind,
                 const geometry_product_compile_options& Options );

private:
    friend class resource_tool::geometry_product_format::geometry_product_encoding_pipeline;
    friend class resource_tool::geometry_product_format::geometry_product_prepare_stage;
    friend class resource_tool::geometry_product_format::geometry_product_model_stage;
    friend class resource_tool::geometry_product_format::geometry_product_stage;

    void SetDiagnostics( resource_tool::diagnostic_log* pDiagnostics );

    // Internal encoding phases run after import and validation.
    void BeginExport( const geometry_asset& Asset, const geometry_asset* pPhysicsAsset, product_kind Kind,
                      const geometry_product_compile_options& Options );
    void BuildProductModel( product_kind Kind );
    void WriteProduct( product_kind Kind );

    void CompileLowCollision( rigid_geom& RigidGeom, geometry_asset& LowMesh, geometry_asset& HighMesh );

    void CompileLowCollisionFromBBox( rigid_geom& RigidGeom, geometry_asset& HighMesh );

    void CompileHighCollisionFromGeometry( rigid_geom& RigidGeom, collision_data::mat_info* pMatList );

public:
    //
    // Fixed layout of "Map" buttons in 3DS Max material editor
    //

    enum
    {
        Max_Diffuse1,         // 3DS Max Defaults
        Max_Diffuse2,         // 3DS Max Defaults
        Max_Blend,            // 3DS Max Defaults
        Max_LightMap,         // 3DS Max Defaults
        Max_Opacity,          // 3DS Max Defaults
        Max_Intensity,        // Environment Intensity Map
        Max_Environment,      // Environment Map
        Max_SelfIllumination, // Per-Pixel Self-Illumination
        Max_DetailMap,        // Detail Map
        Max_PunchThrough,     // Punch-Through Map
        NumMaps
    };

private:
    struct surface_section
    {
        s32 iSection;
        xarray<s32> Facets;
    };

    struct surface
    {
        s32 iMaterial;
        s32 iBone;
        s32 iPrimitive;
        s32 iSubMesh;
        f32 WorldPixelSize;
        xarray<s32> Facets;
        xarray<surface_section> Sections;
    };

    struct sub_mesh
    {
        char Name[256];
        s32 iAssetMesh;
        xarray<surface> Surfaces;
    };

    struct map_info
    {
        resource_tool::bitmap::image Bitmap;
        xstring InputBitmapName;
        xstring OutputBitmapName;
        xstring CompiledBitmapName;
        xbool bPassThrough = FALSE;
    };

    struct map_slot
    {
        xarray<map_info> MapList;
    };

    struct material
    {
        s32 iAssetMaterial;
        tex_info TexInfo;
        map_slot Maps[NumMaps];
    };

    struct mesh
    {
        xarray<material> Material;
        xarray<sub_mesh> SubMesh;
    };

    // Keep mutable state local to one Export request.
    struct encoding_context
    {
        char m_OutputFile[X_MAX_PATH];
        char m_TexturePath[X_MAX_PATH];
        char m_FastCollision[X_MAX_PATH];
        dictionary m_Dictionary;
        xarray<s32> m_MeshToCompiled;
        geometry_asset m_ActiveAsset;
        mesh m_CompiledMesh;
        std::unique_ptr<rigid_geom> m_pRigidGeom;
        std::unique_ptr<skin_geom> m_pSkinGeom;
        geometry_asset m_PhysicsAsset;
        xbool m_bHasPhysicsAsset;
        geometry_asset m_CollisionAsset;
        xbool m_bHasCollisionAsset;
        char m_UserName[256];
        char m_ComputerName[256];
        s32 m_ExportDate[3];
        xbool m_bVerbose;
        xbool m_bDoCollision;
        resource_tool::bitmap::product_sink* m_pBitmapSink;
    };

    encoding_context& Context( void );
    const encoding_context& Context( void ) const;

protected:
    void BuildBone( geom::bone& Bone, const geometry_bone& RawBone );

    void BuildBones( geom& Geom, const geometry_asset& GeomRawMesh, const geometry_asset& PhysicsRawMesh );

    void BuildRigidBody( geom::rigid_body& RigidBody, const geometry_rigid_body& RawRigidBody );

    void BuildRigidBodies( geom& Geom, const geometry_asset& GeomRawMesh, const geometry_asset& PhysicsRawMesh );

    void BuildSettings( geom& Geom, const geometry_asset& GeomRawMesh );

    xbool IsSameMaterial( s32 iMaterialA, s32 iMaterialB, const geometry_asset& Asset );
    void BuildCompileModel( geom& Geom, const geometry_asset& Asset, mesh& Mesh, xbool IsRigid );

    void BuildRigidGeometry( mesh& Mesh, rigid_geom& RigidGeom );

    void BuildSkinGeometry( mesh& Mesh, skin_geom& SkinGeom );
    void ExportUVAnimation( const geometry_material& RawMat, geom::material& Material, geom& Geom );
    void ReadBitmap( map_info& MapInfo );
    void ComputeMapNames( map_slot& Map, s32 MapType, u32 CheckSum, pref_bpp PreferredBPP, const char* pInputFile );
    void FillMapSlots( mesh& Mesh );
    void AddTextureMaps( material& Material, const geometry_material& Source );
    void ExportMaterial( mesh& Mesh, geom& Geom );
    void ExportVirtualMeshes( const geometry_asset& Asset, geom& Geom );
    void ApplyTextureDescriptors( geom& Geom, const geometry_asset& Asset, const s32* pAssetMaterialIndices,
                                  s32 nAssetMaterialIndices );
    xbool ApplyCollisionMetadata( rigid_geom& Geom, const geometry_asset& Asset ) const;
    void ExportDiffuse( const resource_tool::bitmap::image& Bitmap, const char* pName, pref_bpp BPP, s32 nMips );
    void ExportEnvironment( const resource_tool::bitmap::image& Bitmap, const char* pName );
    void ExportDetail( const resource_tool::bitmap::image& Bitmap, const char* pName );

    void ProcessPunchThruMap( map_info& DiffuseMap, const char* pPunchThruMap );

    void CompileDictionary( geom& Geom );

    void PrintSummary( geom& Geom );

protected:
    std::unique_ptr<encoding_context> m_pContext;
    resource_tool::diagnostic_log* m_pDiagnostics;
};

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_WRITER_HPP
//=========================================================================
