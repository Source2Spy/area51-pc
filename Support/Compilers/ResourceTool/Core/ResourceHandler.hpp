//=========================================================================
//
//  ResourceHandler.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RESOURCE_HANDLER_HPP
#define RESOURCE_TOOL_RESOURCE_HANDLER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <filesystem>
#include <memory>

#include "OutputTransaction.hpp"

namespace resource_tool
{

//=========================================================================
//  TYPES
//=========================================================================

enum class operation
{
    Compile,
    Decompile,
};

//-------------------------------------------------------------------------

struct resource_request
{
    operation Operation = operation::Compile;
    std::filesystem::path InputPath;
    std::filesystem::path AnimationPath;
    xbool Verbose = FALSE;
};

class diagnostic_log;
class dependency_graph;
class artifact_manifest;

//-------------------------------------------------------------------------

class resource_context
{
public:
    explicit resource_context( const resource_request& Request );
    ~resource_context( void );

    resource_context( const resource_context& ) = delete;
    resource_context& operator=( const resource_context& ) = delete;

    const resource_request& GetRequest( void ) const;
    const std::filesystem::path& GetInputPath( void ) const;
    std::filesystem::path GetInputDirectory( void ) const;

    output_transaction& Transaction( void );
    const output_transaction& Transaction( void ) const;

    diagnostic_log& Diagnostics( void );
    const diagnostic_log& Diagnostics( void ) const;
    dependency_graph& Dependencies( void );
    const dependency_graph& Dependencies( void ) const;
    artifact_manifest& Artifacts( void );
    const artifact_manifest& Artifacts( void ) const;

private:
    resource_request m_Request;
    output_transaction m_Transaction;
    std::unique_ptr<diagnostic_log> m_pDiagnostics;
    std::unique_ptr<dependency_graph> m_pDependencies;
    std::unique_ptr<artifact_manifest> m_pArtifacts;
};

//-------------------------------------------------------------------------

class resource_handler
{
public:
    virtual ~resource_handler( void ) = default;

    virtual xbool CanHandle( const resource_request& Request ) const = 0;
    virtual xbool DiscoverDependencies( resource_context& Context ) = 0;
    virtual xbool Execute( resource_context& Context ) = 0;
};

} // namespace resource_tool

#endif // RESOURCE_TOOL_RESOURCE_HANDLER_HPP
//=========================================================================
