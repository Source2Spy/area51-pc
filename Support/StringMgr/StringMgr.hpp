//==============================================================================
//
//  StringMgr.hpp
//
//==============================================================================

#ifndef STRINGMGR_HPP
#define STRINGMGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"

//==============================================================================
//  DECLARATIONS
//==============================================================================

struct string_entry;
class  string_table;

//==============================================================================
//  STRUCTURES
//==============================================================================

struct string_diagnostic
{
    xstring     Key;
    xwstring    Text;
};

//==============================================================================
//  CLASSES
//==============================================================================

class string_mgr
{
    friend class string_loader;

public:

                            string_mgr      ( void );
                           ~string_mgr      ( void );

    xbool                   LoadTable       ( const char* pTableName, const char* pFileName );
    void                    UnloadTable     ( const char* pTableName );
    xbool                   ReloadLocalizedTables
                                            ( void );
    xbool                   ValidateLocalizedTables
                                            ( x_language Language ) const;
    s32                     GetStringCount  ( const char* pTableName );
    const xwstring*         FindString      ( const char* pTableName, const char* pID ) const;

    // Retained text must be copied before tables are reloaded.
    const xwstring&         operator ()     ( const char* pTableName, s32 Index,
                                              xbool LogMissing = TRUE ) const;
    const xwstring&         operator ()     ( const char* pTableName, const char* pID,
                                              xbool LogMissing = TRUE ) const;
    const xwstring&         GetSubTitleSpeaker
                                            ( const char* pTableName, const char* pID,
                                              xbool LogMissing = TRUE ) const;
    const xwstring&         GetSoundDescString
                                            ( const char* pTableName, const char* pID,
                                              xbool LogMissing = TRUE ) const;

    static xstring          GetLocalizedName( const char* pFileName, x_language Language );

protected:

    static void             RegisterResourceLoader
                                            ( void );
    xbool                   LoadLocalizedTables
                                            ( x_language Language, xarray<string_table*>& Tables ) const;
    xbool                   LoadTable       ( const char* pTableName, const char* pFileName,
                                              X_FILE*& pFile );
    string_table*           LoadLocalizedTable
                                            ( const char* pTableName, const char* pFileName,
                                              x_language Language, X_FILE*& pFile ) const;
    static X_FILE*          OpenLocalizedFile
                                            ( const char* pFileName, x_language& Language,
                                              xstring& LocalizedName );
    const string_table*     FindTable       ( const char* pTableName ) const;
    const string_entry*     FindEntry       ( const char* pTableName, const char* pID,
                                              xbool LogMissing ) const;
    const xwstring&         MissingString   ( const char* pTableName, const char* pID,
                                              xbool LogMissing ) const;
    static const xwstring&  GetText         ( const string_entry& Entry );
    void                    ClearDiagnostics( void );

protected:

    xarray<string_table*>    m_Tables;

    mutable xarray<string_diagnostic*> m_MissingStrings;
};

//==============================================================================
//  GLOBALS
//==============================================================================

extern string_mgr g_StringTableMgr;

//==============================================================================
#endif // STRINGMGR_HPP
//==============================================================================
