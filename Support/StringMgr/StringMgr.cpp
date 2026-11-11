//==============================================================================
//
//  StringMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "StringMgr.hpp"
#include "StringTable.hpp"

//==============================================================================
//  STORAGE
//==============================================================================

string_mgr g_StringTableMgr;

static const xwstring s_EmptyString;

#if !defined(X_RETAIL) || defined(X_QA)
xbool g_bStringTest   = FALSE;
xbool g_bShowStringID = FALSE;
#endif

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

string_mgr::string_mgr( void )
{
    RegisterResourceLoader();
}

//==============================================================================

string_mgr::~string_mgr( void )
{
    ClearDiagnostics();

    for( s32 i = 0; i < m_Tables.GetCount(); i++ )
    {
        delete m_Tables[i];
    }
}

//==============================================================================

xstring string_mgr::GetLocalizedName( const char* pFileName, x_language Language )
{
    char Drive[X_MAX_DRIVE];
    char Path[X_MAX_PATH];
    char Name[X_MAX_FNAME];
    char Extension[X_MAX_EXT];

    x_splitpath( pFileName, Drive, Path, Name, Extension );

    if( x_strlen( Name ) < 4 || Name[3] != '_' || x_stricmp( Extension, ".string" ) )
    {
        return xstring();
    }

    return xstring( xfs( "%s%s%s%s%s", Drive, Path, x_GetLocaleString( Language ), Name + 3, Extension ) );
}

//==============================================================================

xbool string_mgr::LoadTable( const char* pTableName, const char* pFileName )
{
    if( FindTable( pTableName ) )
    {
        return TRUE;
    }

    X_FILE* pFile  = NULL;
    xbool   Result = LoadTable( pTableName, pFileName, pFile );

    if( pFile )
    {
        x_fclose( pFile );
    }

    return Result;
}

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

X_FILE* string_mgr::OpenLocalizedFile( const char* pFileName, x_language& Language,
                                       xstring& LocalizedName )
{
    LocalizedName = GetLocalizedName( pFileName, Language );

    if( LocalizedName.IsEmpty() )
    {
        return NULL;
    }

    X_FILE* pFile = x_fopen( LocalizedName, "rb" );

    // Fall back only when the requested locale file is absent.
    if( !pFile && Language != XL_LANG_ENGLISH )
    {
        Language = XL_LANG_ENGLISH;
        LocalizedName = GetLocalizedName( pFileName, Language );
        pFile = x_fopen( LocalizedName, "rb" );
    }

    return pFile;
}

//==============================================================================

string_table* string_mgr::LoadLocalizedTable( const char* pTableName, const char* pFileName,
                                             x_language Language, X_FILE*& pFile ) const
{
    xstring LocalizedName;
    pFile = OpenLocalizedFile( pFileName, Language, LocalizedName );

    if( !pFile )
    {
        return NULL;
    }

    string_table* pTable = new string_table;

    if( !pTable->Load( pFile, LocalizedName, Language ) )
    {
        delete pTable;
        return NULL;
    }

    pTable->m_TableName  = pTableName;
    pTable->m_SourceName = pFileName;
    return pTable;
}

//==============================================================================

xbool string_mgr::LoadTable( const char* pTableName, const char* pFileName,
                             X_FILE*& pFile )
{
    if( FindTable( pTableName ) )
    {
        x_language Language = x_GetLocale();
        xstring LocalizedName;

        // ResourceMgr retains the open file until Resolve completes.
        pFile = OpenLocalizedFile( pFileName, Language, LocalizedName );
        return pFile != NULL;
    }

    string_table* pTable = LoadLocalizedTable( pTableName, pFileName, x_GetLocale(), pFile );

    if( !pTable )
    {
        x_DebugMsg( "StringMgr: failed to load '%s' from '%s'\n", pTableName, pFileName );
        return FALSE;
    }

    m_Tables.Append() = pTable;
    return TRUE;
}

//==============================================================================

xbool string_mgr::LoadLocalizedTables( x_language Language, xarray<string_table*>& Replacements ) const
{
    for( s32 i = 0; i < m_Tables.GetCount(); i++ )
    {
        X_FILE* pFile = NULL;
        string_table* pTable = LoadLocalizedTable( m_Tables[i]->m_TableName,
                                                   m_Tables[i]->m_SourceName, Language, pFile );

        if( pFile )
        {
            x_fclose( pFile );
        }

        if( !pTable )
        {
            for( s32 j = 0; j < Replacements.GetCount(); j++ )
            {
                delete Replacements[j];
            }

            Replacements.Clear();
            return FALSE;
        }

        Replacements.Append() = pTable;
    }

    return TRUE;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

xbool string_mgr::ValidateLocalizedTables( x_language Language ) const
{
    xarray<string_table*> Replacements;

    if( !LoadLocalizedTables( Language, Replacements ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < Replacements.GetCount(); i++ )
    {
        delete Replacements[i];
    }

    return TRUE;
}

//==============================================================================

xbool string_mgr::ReloadLocalizedTables( void )
{
    xarray<string_table*> Replacements;

    if( !LoadLocalizedTables( x_GetLocale(), Replacements ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < m_Tables.GetCount(); i++ )
    {
        delete m_Tables[i];
        m_Tables[i] = Replacements[i];
    }

    ClearDiagnostics();
    return TRUE;
}

//==============================================================================

void string_mgr::UnloadTable( const char* pTableName )
{
    for( s32 i = 0; i < m_Tables.GetCount(); i++ )
    {
        if( x_stricmp( m_Tables[i]->m_TableName, pTableName ) == 0 )
        {
            delete m_Tables[i];
            m_Tables.Delete( i );
            return;
        }
    }
}

//==============================================================================

const string_table* string_mgr::FindTable( const char* pTableName ) const
{
    for( s32 i = 0; i < m_Tables.GetCount(); i++ )
    {
        if( x_stricmp( m_Tables[i]->m_TableName, pTableName ) == 0 )
        {
            return m_Tables[i];
        }
    }

    return NULL;
}

//==============================================================================

s32 string_mgr::GetStringCount( const char* pTableName )
{
    const string_table* pTable = FindTable( pTableName );
    return pTable ? pTable->m_Entries.GetCount() : 0;
}

//==============================================================================

const xwstring& string_mgr::MissingString( const char* pTableName, const char* pID, xbool LogMissing ) const
{
    if( LogMissing )
    {
        x_DebugMsg( "StringMgr: missing '%s' in '%s'\n", pID, pTableName );
    }

    xstring Key( xfs( "%s:%s", pTableName, pID ) );

    for( s32 i = 0; i < m_MissingStrings.GetCount(); i++ )
    {
        if( m_MissingStrings[i]->Key == Key )
        {
            return m_MissingStrings[i]->Text;
        }
    }

    string_diagnostic* pDiagnostic = new string_diagnostic;
    pDiagnostic->Key  = Key;
    pDiagnostic->Text = xwstring( (const char*)xfs( "<%s>", (const char*)Key ) );
    m_MissingStrings.Append() = pDiagnostic;
    return pDiagnostic->Text;
}

//==============================================================================

void string_mgr::ClearDiagnostics( void )
{
    for( s32 i = 0; i < m_MissingStrings.GetCount(); i++ )
    {
        delete m_MissingStrings[i];
    }

    m_MissingStrings.Clear();
}

//==============================================================================

const string_entry* string_mgr::FindEntry( const char* pTableName, const char* pID, xbool LogMissing ) const
{
    const string_table* pTable = FindTable( pTableName );
    const string_entry* pEntry = pTable ? pTable->Find( pID ) : NULL;

    if( !pEntry )
    {
        MissingString( pTableName, pID, LogMissing );
    }

    return pEntry;
}

//==============================================================================

const xwstring& string_mgr::GetText( const string_entry& Entry )
{
#if !defined(X_RETAIL) || defined(X_QA)
    if( g_bShowStringID )
    {
        return Entry.DisplayID;
    }

    if( g_bStringTest )
    {
        return Entry.TestText;
    }
#endif

    return Entry.Text;
}

//==============================================================================

const xwstring* string_mgr::FindString( const char* pTableName, const char* pID ) const
{
    const string_table* pTable = FindTable( pTableName );
    const string_entry* pEntry = pTable ? pTable->Find( pID ) : NULL;
    return pEntry ? &GetText( *pEntry ) : NULL;
}

//==============================================================================

const xwstring& string_mgr::operator () ( const char* pTableName, const char* pID, xbool LogMissing ) const
{
    const string_entry* pEntry = FindEntry( pTableName, pID, LogMissing );
    return pEntry ? GetText( *pEntry ) : MissingString( pTableName, pID, FALSE );
}

//==============================================================================

const xwstring& string_mgr::operator () ( const char* pTableName, s32 Index, xbool LogMissing ) const
{
    const string_table* pTable = FindTable( pTableName );

    if( pTable && Index >= 0 && Index < pTable->m_Entries.GetCount() )
    {
        return GetText( *pTable->m_Index[Index] );
    }

    return MissingString( pTableName, xfs( "%d", Index ), LogMissing );
}

//==============================================================================

const xwstring& string_mgr::GetSubTitleSpeaker( const char* pTableName, const char* pID, xbool LogMissing ) const
{
    const string_entry* pEntry = FindEntry( pTableName, pID, LogMissing );
    return pEntry ? pEntry->Speaker : s_EmptyString;
}

//==============================================================================

const xwstring& string_mgr::GetSoundDescString( const char* pTableName, const char* pID, xbool LogMissing ) const
{
    const string_entry* pEntry = FindEntry( pTableName, pID, LogMissing );
    return pEntry ? pEntry->SoundDesc : s_EmptyString;
}
