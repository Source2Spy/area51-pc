//==============================================================================
//
//  StringTable.hpp
//
//==============================================================================

#ifndef STRING_TABLE_HPP
#define STRING_TABLE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"

//==============================================================================
//  STRUCTURES
//==============================================================================

struct string_entry
{
    xstring     ID;
    xwstring    Text;
    xwstring    Speaker;
    xwstring    SoundDesc;
    xwstring    DisplayID;
    xwstring    TestText;
    s32         LineNumber;
};

//==============================================================================
//  CLASSES
//==============================================================================

class string_table
{
    friend class string_mgr;

protected:

    xbool                   Load            ( X_FILE* pFile, const char* pFileName,
                                              x_language Language );
    const string_entry*     Find            ( const char* pID ) const;

protected:

    xstring                 m_TableName;
    xstring                 m_SourceName;
    xarray<string_entry>    m_Entries;
    xarray<string_entry*>   m_Index;
};

//==============================================================================
#endif // STRING_TABLE_HPP
//==============================================================================
