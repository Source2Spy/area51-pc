//==============================================================================
//
//  StringTable.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "StringTable.hpp"
#include "Parsing/TextIn.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool DecodeUTF8( const char* pText, s32 Length, xwstring& Result, s32* pErrorByte = NULL )
{
    Result.Clear();

    for( s32 i = 0; i < Length; )
    {
        if( pErrorByte )
        {
            *pErrorByte = i;
        }

        u32 Code      = (u8)pText[i++];
        u32 Minimum   = 0;
        s32 Remaining = 0;

        if( Code >= 0xC2 && Code <= 0xDF )
        {
            Code &= 0x1F;
            Minimum = 0x80;
            Remaining = 1;
        }
        else if( Code >= 0xE0 && Code <= 0xEF )
        {
            Code &= 0x0F;
            Minimum = 0x800;
            Remaining = 2;
        }
        else if( Code >= 0xF0 && Code <= 0xF4 )
        {
            Code &= 0x07;
            Minimum = 0x10000;
            Remaining = 3;
        }
        else if( Code >= 0x80 || Code == 0 )
        {
            return FALSE;
        }

        if( i + Remaining > Length )
        {
            return FALSE;
        }

        while( Remaining-- )
        {
            u32 Byte = (u8)pText[i++];

            if( (Byte & 0xC0) != 0x80 )
            {
                return FALSE;
            }

            Code = (Code << 6) | (Byte & 0x3F);
        }

        if( Code < Minimum || Code > 0x10FFFF || (Code >= 0xD800 && Code <= 0xDFFF) )
        {
            return FALSE;
        }

        if( Code >= 0x10000 )
        {
            Code -= 0x10000;
            Result += (xwchar)(0xD800 + (Code >> 10));
            Result += (xwchar)(0xDC00 + (Code & 0x3FF));
        }
        else
        {
            Result += (xwchar)Code;
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool ReadText( text_in& Input, const char* pField, xwstring& Text )
{
    xstring Bytes;
    return Input.GetString( pField, Bytes ) && DecodeUTF8( Bytes, Bytes.GetLength(), Text );
}

//==============================================================================

static
s32 CompareEntries( const void* pLeft, const void* pRight )
{
    const string_entry* pA = *static_cast<string_entry* const*>( pLeft );
    const string_entry* pB = *static_cast<string_entry* const*>( pRight );
    return x_stricmp( pA->ID, pB->ID );
}

//==============================================================================

static
void BuildTestText( string_entry& Entry )
{
    Entry.TestText = Entry.Text;
    xwchar Closing = 0;

    for( s32 i = 0; i < Entry.TestText.GetLength(); i++ )
    {
        xwchar Character = Entry.TestText[i];

        if( Closing )
        {
            if( Character == Closing )
            {
                Closing = 0;
            }
        }
        else if( Character == '{' || Character == '<' )
        {
            Closing = Character == '{' ? '}' : '>';
        }
        else if( Character > 0x20 )
        {
            Entry.TestText[i] = 'x';
        }
    }
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

xbool string_table::Load( X_FILE* pFile, const char* pFileName, x_language Language )
{
    const char* FileName = pFileName;

    if( !pFile )
    {
        return FALSE;
    }

    s32 Length = x_flength( pFile );
    xarray<char> Bytes;
    Bytes.SetCount( MAX( Length, 0 ) + 1 );

    xbool Read = Length > 0 && x_fread( Bytes.GetPtr(), 1, Length, pFile ) == Length;
    Bytes[MAX( Length, 0 )] = 0;

    xwstring Validation;
    s32      ErrorByte = 0;

    if( !Read || !DecodeUTF8( Bytes.GetPtr(), Length, Validation, &ErrorByte ) )
    {
        s32 Line = 1;

        for( s32 i = 0; i < ErrorByte; i++ )
        {
            if( Bytes[i] == '\n' )
            {
                Line++;
            }
        }

        x_DebugMsg( "%s(%d): invalid UTF-8 string table\n", (const char*)FileName, Line );
        return FALSE;
    }

    const char* pText = Bytes.GetPtr();

    text_in Input;
    Input.SetStringMode( TRUE, TRUE );
    Input.OpenText( pText, FileName );

    s32 Version = 0;
    xstring Locale;

    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "StringTable" ) ||
        Input.GetHeaderCount() != -1 || Input.GetFieldCount() != 2 ||
        !Input.ReadFields() || !Input.GetS32( "Version", Version ) || Version != 1 ||
        !Input.GetString( "Locale", Locale ) ||
        x_strcmp( Locale, x_GetLocaleString( Language, XL_LOCALE_CODE_ISO_639_1 ) ) )
    {
        x_DebugMsg( "%s(%d): invalid StringTable version or locale\n", (const char*)FileName, Input.GetLineNumber() );
        return FALSE;
    }

    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "Strings" ) ||
        Input.GetHeaderCount() < 0 || Input.GetFieldCount() != 2 )
    {
        x_DebugMsg( "%s(%d): invalid Strings section\n", (const char*)FileName, Input.GetLineNumber() );
        return FALSE;
    }

    s32 Count = Input.GetHeaderCount();

    if( Count > Length )
    {
        x_DebugMsg( "%s(%d): section count exceeds file size\n", (const char*)FileName, Input.GetLineNumber() );
        return FALSE;
    }

    m_Entries.SetCount( Count );
    m_Index.SetCount( Count );

    for( s32 i = 0; i < Count; i++ )
    {
        string_entry& Entry = m_Entries[i];

        if( !Input.ReadFields() || !Input.GetString( "ID", Entry.ID ) || Entry.ID.IsEmpty() ||
            !ReadText( Input, "Text", Entry.Text ) )
        {
            x_DebugMsg( "%s(%d): invalid string record %d\n", (const char*)FileName, Input.GetLineNumber(), i );
            return FALSE;
        }

        for( s32 j = 0; j < Entry.ID.GetLength(); j++ )
        {
            if( (u8)Entry.ID[j] < 0x20 || (u8)Entry.ID[j] >= 0x7F )
            {
                x_DebugMsg( "%s(%d): string ID must be printable ASCII\n", (const char*)FileName, Input.GetLineNumber() );
                return FALSE;
            }
        }

        Entry.DisplayID = Entry.ID;
        Entry.LineNumber = Input.GetLineNumber();
        BuildTestText( Entry );
        m_Index[i] = &Entry;
    }

    if( Count > 1 )
    {
        x_qsort( m_Index.GetPtr(), Count, sizeof(string_entry*), CompareEntries );
    }

    for( s32 i = 1; i < Count; i++ )
    {
        if( x_stricmp( m_Index[i-1]->ID, m_Index[i]->ID ) == 0 )
        {
            x_DebugMsg( "%s(%d): duplicate string ID '%s'\n", (const char*)FileName,
                        m_Index[i]->LineNumber, (const char*)m_Index[i]->ID );
            return FALSE;
        }
    }

    if( Input.ReadHeader() )
    {
        if( x_strcmp( Input.GetHeaderName(), "Subtitles" ) || Input.GetHeaderCount() < 0 ||
            Input.GetHeaderCount() > Count || Input.GetFieldCount() != 3 )
        {
            x_DebugMsg( "%s(%d): invalid Subtitles section\n", (const char*)FileName, Input.GetLineNumber() );
            return FALSE;
        }

        xarray<xstring> SubtitleIDs;

        for( s32 i = 0; i < Input.GetHeaderCount(); i++ )
        {
            xstring ID;
            xwstring Speaker;
            xwstring SoundDesc;

            if( !Input.ReadFields() || !Input.GetString( "ID", ID ) ||
                !ReadText( Input, "Speaker", Speaker ) || !ReadText( Input, "SoundDesc", SoundDesc ) )
            {
                return FALSE;
            }

            const string_entry* pEntry = Find( ID );

            if( !pEntry )
            {
                x_DebugMsg( "%s(%d): unknown subtitle ID '%s'\n", (const char*)FileName,
                            Input.GetLineNumber(), (const char*)ID );
                return FALSE;
            }

            for( s32 j = 0; j < SubtitleIDs.GetCount(); j++ )
            {
                if( x_stricmp( SubtitleIDs[j], ID ) == 0 )
                {
                    x_DebugMsg( "%s(%d): duplicate subtitle ID '%s'\n", (const char*)FileName,
                                Input.GetLineNumber(), (const char*)ID );
                    return FALSE;
                }
            }

            SubtitleIDs.Append() = ID;
            s32 Index = (s32)(pEntry - m_Entries.GetPtr());
            m_Entries[Index].Speaker   = Speaker;
            m_Entries[Index].SoundDesc = SoundDesc;
        }

        if( Input.ReadHeader() )
        {
            x_DebugMsg( "%s(%d): unexpected section\n", (const char*)FileName, Input.GetLineNumber() );
            return FALSE;
        }
    }

    if( Input.HasError() || !Input.IsEOF() )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

const string_entry* string_table::Find( const char* pID ) const
{
    s32 First = 0;
    s32 Last  = m_Index.GetCount();

    while( First < Last )
    {
        s32 Middle = First + (Last - First) / 2;
        s32 Result = x_stricmp( m_Index[Middle]->ID, pID );

        if( Result == 0 )
        {
            return m_Index[Middle];
        }

        if( Result < 0 )
        {
            First = Middle + 1;
        }
        else
        {
            Last = Middle;
        }
    }

    return NULL;
}
