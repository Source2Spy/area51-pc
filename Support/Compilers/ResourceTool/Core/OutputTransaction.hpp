//=========================================================================
//
//  OutputTransaction.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_OUTPUT_TRANSACTION_HPP
#define RESOURCE_TOOL_OUTPUT_TRANSACTION_HPP

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

class output_transaction
{
public:
    output_transaction( void ) = default;
    ~output_transaction( void );

    output_transaction( const output_transaction& ) = delete;
    output_transaction& operator=( const output_transaction& ) = delete;

    std::filesystem::path Stage( const std::filesystem::path& FinalPath );
    std::filesystem::path StageOptional( const std::filesystem::path& FinalPath );
    xbool Commit( xstring& Error );
    xbool Commit( std::string& Error );
    void Rollback( void );

private:
    struct staged_file
    {
        std::filesystem::path FinalPath;
        std::filesystem::path StagePath;
        std::filesystem::path BackupPath;
        xbool Required = TRUE;
        xbool HadOriginal = FALSE;
        xbool Committed = FALSE;
    };

    xarray<staged_file> m_Files;
};

} // namespace resource_tool

//=========================================================================

#endif // RESOURCE_TOOL_OUTPUT_TRANSACTION_HPP
//=========================================================================
