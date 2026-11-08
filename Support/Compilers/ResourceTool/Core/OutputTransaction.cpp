//=========================================================================
//
//  OutputTransaction.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "OutputTransaction.hpp"

#include <system_error>
namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

output_transaction::~output_transaction( void )
{
    Rollback();
}

//=========================================================================

std::filesystem::path output_transaction::Stage( const std::filesystem::path& FinalPath )
{
    const std::filesystem::path Normalized = FinalPath.lexically_normal();
    std::filesystem::path StagePath = Normalized;
    const xstring Extension( Normalized.extension().string().c_str() );
    if( Extension.IsEmpty() )
        StagePath += ".resource_tool_tmp";
    else
    {
        std::string Replacement = ".resource_tool_tmp";
        Replacement += (const char*)Extension;
        StagePath.replace_extension( Replacement );
    }

    std::error_code ErrorCode;
    if( !Normalized.parent_path().empty() )
        std::filesystem::create_directories( Normalized.parent_path(), ErrorCode );
    std::filesystem::remove( StagePath, ErrorCode );

    staged_file File;
    File.FinalPath = Normalized;
    File.StagePath = StagePath;
    File.BackupPath = Normalized;
    File.BackupPath += ".resource_tool_backup";
    m_Files.Append( File );
    return StagePath;
}

//=========================================================================

std::filesystem::path output_transaction::StageOptional( const std::filesystem::path& FinalPath )
{
    const std::filesystem::path StagePath = Stage( FinalPath );
    m_Files[m_Files.GetCount() - 1].Required = FALSE;
    return StagePath;
}

//=========================================================================

xbool output_transaction::Commit( xstring& Error )
{
    for( s32 iFile = 0; iFile < m_Files.GetCount(); ++iFile )
    {
        const staged_file& File = m_Files[iFile];
        std::error_code ErrorCode;
        const xbool bExists = std::filesystem::exists( File.StagePath, ErrorCode );
        if( ErrorCode )
        {
            Error = xfs( "Unable to inspect staged output %s: %s", File.StagePath.generic_string().c_str(),
                         ErrorCode.message().c_str() );
            Rollback();
            return FALSE;
        }
        if( !bExists )
        {
            if( !File.Required )
                continue;
            Error = xfs( "Staged output does not exist: %s", File.StagePath.generic_string().c_str() );
            Rollback();
            return FALSE;
        }
    }

    // Move existing products out of the way before replacing them.
    for( s32 iFile = 0; iFile < m_Files.GetCount(); ++iFile )
    {
        staged_file& File = m_Files[iFile];
        std::error_code ErrorCode;
        File.HadOriginal = std::filesystem::exists( File.FinalPath, ErrorCode );
        if( ErrorCode )
        {
            Error = xfs( "Unable to inspect output %s: %s", File.FinalPath.generic_string().c_str(),
                         ErrorCode.message().c_str() );
            Rollback();
            return FALSE;
        }
        if( !File.HadOriginal )
            continue;

        std::filesystem::remove( File.BackupPath, ErrorCode );
        ErrorCode.clear();
        std::filesystem::rename( File.FinalPath, File.BackupPath, ErrorCode );
        if( ErrorCode )
        {
            Error = xfs( "Unable to preserve output %s: %s", File.FinalPath.generic_string().c_str(),
                         ErrorCode.message().c_str() );
            Rollback();
            return FALSE;
        }
    }

    for( s32 iFile = 0; iFile < m_Files.GetCount(); ++iFile )
    {
        staged_file& File = m_Files[iFile];
        std::error_code ErrorCode;
        if( !std::filesystem::exists( File.StagePath, ErrorCode ) )
        {
            if( ErrorCode )
            {
                Error = xfs( "Unable to inspect staged output %s: %s", File.StagePath.generic_string().c_str(),
                             ErrorCode.message().c_str() );
                Rollback();
                return FALSE;
            }
            continue;
        }
        ErrorCode.clear();
        std::filesystem::rename( File.StagePath, File.FinalPath, ErrorCode );
        if( ErrorCode )
        {
            Error = xfs( "Unable to commit output %s: %s", File.FinalPath.generic_string().c_str(),
                         ErrorCode.message().c_str() );
            Rollback();
            return FALSE;
        }
        File.Committed = TRUE;
    }

    std::error_code ErrorCode;
    for( s32 iFile = 0; iFile < m_Files.GetCount(); ++iFile )
    {
        const staged_file& File = m_Files[iFile];
        std::filesystem::remove( File.BackupPath, ErrorCode );
    }
    m_Files.Clear();
    return TRUE;
}

//=========================================================================

xbool output_transaction::Commit( std::string& Error )
{
    xstring XError;
    const xbool bCommitted = Commit( XError );
    Error = (const char*)XError;
    return bCommitted;
}

//=========================================================================

void output_transaction::Rollback( void )
{
    std::error_code ErrorCode;
    for( s32 iFile = m_Files.GetCount() - 1; iFile >= 0; --iFile )
    {
        staged_file& File = m_Files[iFile];
        std::filesystem::remove( File.StagePath, ErrorCode );
        ErrorCode.clear();
        if( File.Committed )
        {
            std::filesystem::remove( File.FinalPath, ErrorCode );
            ErrorCode.clear();
        }
        if( File.HadOriginal && std::filesystem::exists( File.BackupPath, ErrorCode ) )
        {
            ErrorCode.clear();
            std::filesystem::rename( File.BackupPath, File.FinalPath, ErrorCode );
        }
        ErrorCode.clear();
    }
    m_Files.Clear();
}

} // namespace resource_tool
