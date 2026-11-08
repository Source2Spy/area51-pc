//=========================================================================
//
//  GeometryAsset.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_ASSET_HPP
#define RESOURCE_TOOL_GEOMETRY_ASSET_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "GeometryMesh.hpp"
#include "GeometryMaterial.hpp"
#include "GeometrySkeleton.hpp"
#include "GeometryPhysics.hpp"
#include "GeometryLod.hpp"
#include "GeometryCollision.hpp"
#include "GeometryMetadata.hpp"

#include <string>

//=========================================================================
//  TYPES
//=========================================================================

struct geometry_source_metadata
{
    std::string SourceFile;
    std::string UserName;
    std::string ComputerName;
    std::array<s32, 3> ExportDate = { 0, 0, 0 };

    void Clear( void )
    {
        SourceFile.clear();
        UserName.clear();
        ComputerName.clear();
        ExportDate = { 0, 0, 0 };
    }
};

//-------------------------------------------------------------------------

struct geometry_asset
{
    geometry_source_metadata Source;
    geometry_render_model Render;
    geometry_material_model Materials;
    geometry_skeleton_model Skeleton;
    geometry_lod_model LODs;
    geometry_collision_model Collision;
    geometry_physics_model Physics;
    geometry_engine_metadata EngineMetadata;

    void Clear( void );
    void CleanWeights( s32 MaxNumWeights, f32 MinWeightValue );
    bbox GetBBox( void ) const;
    s32 GetBoneIDFromName( const char* pBoneName, xbool bAnywhere = FALSE ) const;
    s32 GetRigidBodyIDFromName( const char* pRigidBodyName, xbool bAnywhere = FALSE ) const;
    void PrintHierarchy( void ) const;
    void PrintRigidBodies( void ) const;
};

#endif // RESOURCE_TOOL_GEOMETRY_ASSET_HPP
//=========================================================================
