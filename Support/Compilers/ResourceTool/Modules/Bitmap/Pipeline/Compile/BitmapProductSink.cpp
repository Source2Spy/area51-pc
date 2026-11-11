//=========================================================================
//
//  BitmapProductSink.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapProductSink.hpp"

#include "ArtifactManifest.hpp"
#include "BitmapFormat.hpp"
#include "OutputTransaction.hpp"

#include <system_error>
#include <utility>

namespace resource_tool::bitmap
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

transactional_product_sink::transactional_product_sink( resource_tool::output_transaction& Transaction,
                                                        resource_tool::artifact_manifest& Artifacts )
    : m_Transaction( Transaction ), m_Artifacts( Artifacts )
{
}

//=========================================================================

xbool transactional_product_sink::Write( const product_request& Request, const image& Source, std::string& Error )
{
    const std::filesystem::path OutputPath = Request.OutputPath.lexically_normal();
    if( m_Paths.find( OutputPath ) != m_Paths.end() )
        return TRUE;

    const std::filesystem::path StagedPath = m_Transaction.Stage( OutputPath );
    if( !format_io::SaveXbmp( StagedPath, Source, Request.Profile, Error ) )
        return FALSE;

    m_Paths.insert( OutputPath );
    m_Artifacts.Add( resource_tool::artifact_kind::Auxiliary, Request.Role.c_str(), OutputPath );
    return TRUE;
}

//=========================================================================

xbool filesystem_product_sink::Write( const product_request& Request, const image& Source, std::string& Error )
{
    std::error_code ErrorCode;
    if( !Request.OutputPath.parent_path().empty() )
        std::filesystem::create_directories( Request.OutputPath.parent_path(), ErrorCode );
    if( ErrorCode )
    {
        Error = "Unable to create bitmap output directory " + Request.OutputPath.parent_path().generic_string() + ": " +
                ErrorCode.message();
        return FALSE;
    }
    return format_io::SaveXbmp( Request.OutputPath, Source, Request.Profile, Error );
}

//=========================================================================

std::unique_ptr<product_sink> CreateTransactionalProductSink( resource_tool::output_transaction& Transaction,
                                                              resource_tool::artifact_manifest& Artifacts )
{
    return std::unique_ptr<product_sink>( new transactional_product_sink( Transaction, Artifacts ) );
}

//=========================================================================

std::unique_ptr<product_sink> CreateFilesystemProductSink( void )
{
    return std::unique_ptr<product_sink>( new filesystem_product_sink );
}

} // namespace resource_tool::bitmap
