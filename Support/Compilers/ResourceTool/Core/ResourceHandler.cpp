//=========================================================================
//
//  ResourceHandler.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ResourceHandler.hpp"

#include "ArtifactManifest.hpp"
#include "DependencyGraph.hpp"
#include "Diagnostics.hpp"

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

resource_context::resource_context( const resource_request& Request )
    : m_Request( Request ), m_pDiagnostics( new diagnostic_log ), m_pDependencies( new dependency_graph ),
      m_pArtifacts( new artifact_manifest )
{
}

resource_context::~resource_context( void ) = default;

//=========================================================================

const resource_request& resource_context::GetRequest( void ) const
{
    return m_Request;
}

//=========================================================================

const std::filesystem::path& resource_context::GetInputPath( void ) const
{
    return m_Request.InputPath;
}

//=========================================================================

std::filesystem::path resource_context::GetInputDirectory( void ) const
{
    return m_Request.InputPath.parent_path();
}

//=========================================================================

output_transaction& resource_context::Transaction( void )
{
    return m_Transaction;
}

//=========================================================================

const output_transaction& resource_context::Transaction( void ) const
{
    return m_Transaction;
}

//=========================================================================

diagnostic_log& resource_context::Diagnostics( void )
{
    return *m_pDiagnostics;
}

//=========================================================================

const diagnostic_log& resource_context::Diagnostics( void ) const
{
    return *m_pDiagnostics;
}

//=========================================================================

dependency_graph& resource_context::Dependencies( void )
{
    return *m_pDependencies;
}

//=========================================================================

const dependency_graph& resource_context::Dependencies( void ) const
{
    return *m_pDependencies;
}

//=========================================================================

artifact_manifest& resource_context::Artifacts( void )
{
    return *m_pArtifacts;
}

//=========================================================================

const artifact_manifest& resource_context::Artifacts( void ) const
{
    return *m_pArtifacts;
}

} // namespace resource_tool
