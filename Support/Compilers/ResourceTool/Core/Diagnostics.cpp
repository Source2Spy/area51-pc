//=========================================================================
//
//  Diagnostics.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Diagnostics.hpp"

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void diagnostic_log::Add( diagnostic_severity Severity, const char* pMessage, const char* pStage, const char* pAsset,
                          const char* pSource )
{
    diagnostic Entry;
    Entry.Severity = Severity;
    Entry.Stage = pStage ? pStage : "";
    Entry.Asset = pAsset ? pAsset : "";
    Entry.Source = pSource ? pSource : "";
    Entry.Message = pMessage ? pMessage : "";
    m_Entries.Append( Entry );
}

//=========================================================================

void diagnostic_log::Info( const char* pMessage, const char* pStage, const char* pAsset, const char* pSource )
{
    Add( diagnostic_severity::Info, pMessage, pStage, pAsset, pSource );
}

//=========================================================================

void diagnostic_log::Warning( const char* pMessage, const char* pStage, const char* pAsset, const char* pSource )
{
    Add( diagnostic_severity::Warning, pMessage, pStage, pAsset, pSource );
}

//=========================================================================

void diagnostic_log::Error( const char* pMessage, const char* pStage, const char* pAsset, const char* pSource )
{
    Add( diagnostic_severity::Error, pMessage, pStage, pAsset, pSource );
}

//=========================================================================

void diagnostic_log::Fatal( const char* pMessage, const char* pStage, const char* pAsset, const char* pSource )
{
    Add( diagnostic_severity::Fatal, pMessage, pStage, pAsset, pSource );
}

//=========================================================================

xbool diagnostic_log::HasErrors( void ) const
{
    for( s32 iEntry = 0; iEntry < m_Entries.GetCount(); ++iEntry )
    {
        const diagnostic& Entry = m_Entries[iEntry];
        if( Entry.Severity == diagnostic_severity::Error || Entry.Severity == diagnostic_severity::Fatal )
        {
            return TRUE;
        }
    }
    return FALSE;
}

//=========================================================================

const xarray<diagnostic>& diagnostic_log::Entries( void ) const
{
    return m_Entries;
}

} // namespace resource_tool
