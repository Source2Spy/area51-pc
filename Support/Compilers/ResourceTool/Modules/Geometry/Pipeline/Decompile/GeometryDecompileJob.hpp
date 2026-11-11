//=========================================================================
//
//  GeometryDecompileJob.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_DECOMPILE_JOB_HPP
#define RESOURCE_TOOL_GEOMETRY_DECOMPILE_JOB_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "ArtifactManifest.hpp"
#include "Diagnostics.hpp"
#include "GeometryDescriptor.hpp"
#include "Model/AnimationAsset.hpp"
#include "Model/GeometryAsset.hpp"
#include "Model/GeometryTextureMap.hpp"
#include "OutputTransaction.hpp"

#include <filesystem>
#include <string>
#include <vector>

//=========================================================================
//  TYPES
//=========================================================================

class geometry_decompile_job
{
public:
    struct output_paths
    {
        std::filesystem::path Geometry;
        std::filesystem::path GeometryBinary;
        std::filesystem::path GeometryBinaryUri;
        std::filesystem::path Collision;
        std::filesystem::path CollisionBinary;
        std::filesystem::path CollisionBinaryUri;
        std::filesystem::path RawGeom;
        std::filesystem::path DescriptorGeometry;
        std::filesystem::path DescriptorCollision;
        resource_tool::output_transaction* Transaction = NULL;
        resource_tool::artifact_manifest* Artifacts = NULL;
        resource_tool::diagnostic_log* Diagnostics = NULL;
    };

    void Decompile( const geometry_descriptor& Settings, const output_paths* pOutputs = NULL );

    xbool HasCollisionOutput( void ) const;

private:
    void Begin( const geometry_descriptor& Settings, const output_paths* pOutputs = NULL );
    void BuildSemanticModel( void );
    void ExportTextures( void );
    void ExportRenderGeometry( void );
    void ExportCollisionGeometry( void );
    void WriteRawGeom( void );

    geometry_descriptor m_Settings;
    output_paths m_Outputs;
    geometry_descriptor::geometry_type m_Type;
    geometry_asset m_Asset;
    resource_tool::animation::asset m_SkeletonSource;
    resource_tool::geometry::texture_export_map m_TextureMap;
    xbool m_bCollisionExported;
};

#endif // RESOURCE_TOOL_GEOMETRY_DECOMPILE_JOB_HPP
//=========================================================================
