//=========================================================================
//
//  Diagnostics.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_DIAGNOSTICS_HPP
#define RESOURCE_TOOL_DIAGNOSTICS_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

namespace resource_tool
{

//=========================================================================
//  TYPES
//=========================================================================

enum class diagnostic_severity
{
    Info,
    Warning,
    Error,
    Fatal,
};

//-------------------------------------------------------------------------

struct diagnostic
{
    diagnostic_severity Severity = diagnostic_severity::Info;
    xstring Stage;
    xstring Asset;
    xstring Source;
    xstring Message;
};

//-------------------------------------------------------------------------

class diagnostic_log
{
public:
    void Add( diagnostic_severity Severity, const char* pMessage, const char* pStage = NULL, const char* pAsset = NULL,
              const char* pSource = NULL );
    void Info( const char* pMessage, const char* pStage = NULL, const char* pAsset = NULL, const char* pSource = NULL );
    void Warning( const char* pMessage, const char* pStage = NULL, const char* pAsset = NULL,
                  const char* pSource = NULL );
    void Error( const char* pMessage, const char* pStage = NULL, const char* pAsset = NULL,
                const char* pSource = NULL );
    void Fatal( const char* pMessage, const char* pStage = NULL, const char* pAsset = NULL,
                const char* pSource = NULL );

    xbool HasErrors( void ) const;
    const xarray<diagnostic>& Entries( void ) const;

private:
    xarray<diagnostic> m_Entries;
};

} // namespace resource_tool

//=========================================================================

#endif // RESOURCE_TOOL_DIAGNOSTICS_HPP
//=========================================================================
