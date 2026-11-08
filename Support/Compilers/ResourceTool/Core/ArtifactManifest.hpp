//=========================================================================
//
//  ArtifactManifest.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ARTIFACT_MANIFEST_HPP
#define RESOURCE_TOOL_ARTIFACT_MANIFEST_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <filesystem>
#include <string>

namespace resource_tool
{

//=========================================================================
//  TYPES
//=========================================================================

enum class artifact_kind
{
    Main,
    Auxiliary,
};

//-------------------------------------------------------------------------

struct artifact
{
    artifact_kind Kind = artifact_kind::Main;
    xstring Role;
    std::filesystem::path Path;
};

//-------------------------------------------------------------------------

struct artifact_manifest_metadata
{
    s32 SchemaVersion = -1;
    xstring ToolFingerprint;
    xstring InputFingerprint;
    xarray<artifact> Artifacts;
};

//-------------------------------------------------------------------------

enum class artifact_build_decision
{
    Rebuild,
    UpToDate,
};

//-------------------------------------------------------------------------

struct artifact_build_decision_result
{
    artifact_build_decision Decision = artifact_build_decision::Rebuild;
    std::string Reason;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xstring CalculateInputFingerprint( const class dependency_graph& Dependencies );

xbool ReadJson( const std::filesystem::path& Path, artifact_manifest_metadata& Metadata, std::string& Error );

//-------------------------------------------------------------------------

class artifact_manifest
{
public:
    void Add( artifact_kind Kind, const char* pRole, const std::filesystem::path& Path );
    const xarray<artifact>& Entries( void ) const;

    xbool WriteJson( const std::filesystem::path& Path, const class dependency_graph& Dependencies,
                     const char* pToolFingerprint, s32 SchemaVersion, const char* pInputFingerprint,
                     std::string& Error ) const;

    artifact_build_decision_result Evaluate( const std::filesystem::path& ManifestPath, const char* pToolFingerprint,
                                             s32 SchemaVersion, const char* pInputFingerprint ) const;

private:
    xarray<artifact> m_Entries;
};

} // namespace resource_tool

//=========================================================================

#endif // RESOURCE_TOOL_ARTIFACT_MANIFEST_HPP
//=========================================================================
