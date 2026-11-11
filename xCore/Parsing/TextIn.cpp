//=========================================================================
//
//  TextIn.cpp
//
//=========================================================================

//=========================================================================
// INCLUDES
//=========================================================================

#include "TextIn.hpp"
#include "x_plus.hpp"
#include "x_stdio.hpp"
#include "x_string.hpp"

//=========================================================================
// IMPLEMENTATION
//=========================================================================

text_in::text_in( void )
{
    m_pData                 = NULL;
    m_nDataEntriesAllocated = 0;

    m_Record.Name[0]          = 0;
    m_Record.Count            = -1;
    m_Record.nFields          = 0;
    m_Record.nFieldsAllocated = 0;
    m_Record.Field            = NULL;
    m_RecordLineNumber        = 0;
}

//=========================================================================

text_in::~text_in( void )
{
    CloseFile();
    delete[] m_pData;
    x_free( m_Record.Field );
}

//=========================================================================

xbool text_in::ReportError( const char* pStr, ... )
{
    // Keep the first diagnostic for this reader.
    if( !m_Error.IsEmpty() )
    {
        return FALSE;
    }

    xstring   Message;
    x_va_list Args;
    x_va_start( Args, pStr );
    Message.FormatV( pStr, Args );
    x_va_end( Args );

    if( m_Record.Name[0] )
    {
        m_Error.Format( "%s(%d) : error : In Record %s, %s\n",
                        m_Tokenizer.GetFilename(),
                        m_Tokenizer.GetLineNumber(),
                        GetHeaderName(),
                        (const char*)Message );
    }
    else
    {
        m_Error.Format( "%s(%d) : error : %s\n",
                        m_Tokenizer.GetFilename(),
                        m_Tokenizer.GetLineNumber(),
                        (const char*)Message );
    }

    x_DebugMsg( "%s", (const char*)m_Error );

    return FALSE;
}

//=========================================================================

void text_in::CloseFile( void )
{
    m_Tokenizer.CloseFile();
    m_Record.Name[0]   = 0;
    m_Record.Count     = -1;
    m_Record.nFields   = 0;
    m_RecordLineNumber = 0;
}

//=========================================================================

xbool text_in::OpenFile( const char* pFileName )
{
    // Reset record state before opening the next input.
    m_Error.Clear();

    m_Record.nFields   = 0;
    m_Record.Name[0]   = 0;
    m_RecordLineNumber = 0;

    // Set our list of delimeters
    m_Tokenizer.SetDelimeter( ":,[]{}()<>" );

    // Open the file
    if( m_Tokenizer.OpenFile( pFileName ) == FALSE )
    {
        return ReportError( "%s", m_Tokenizer.GetError() );
    }

    return TRUE;
}

//=========================================================================

void text_in::OpenText( const char* pText, const char* pFileName )
{
    // Memory input follows the same record state as file input.
    m_Error.Clear();
    m_Record.nFields   = 0;
    m_Record.Name[0]   = 0;
    m_RecordLineNumber = 0;
    m_Tokenizer.SetDelimeter( ":,[]{}()<>" );
    m_Tokenizer.OpenText( pText, pFileName );
}

//=========================================================================

void text_in::SetStringMode( xbool Escaping, xbool Multiline )
{
    m_Tokenizer.SetStringMode( Escaping, Multiline );
}

//=========================================================================

xbool text_in::HasError( void ) const
{
    return !m_Error.IsEmpty() || m_Tokenizer.GetError()[0] != 0;
}

//=========================================================================

s32 text_in::GetLineNumber( void ) const
{
    return m_Tokenizer.GetLineNumber();
}

//=========================================================================

xbool text_in::ReadAllFields( void )
{
    token_stream::type Type;

    // Reset the current field count to zero
    m_Record.nFields = 0;

    //
    // First lets make sure that we start with the right delimeter
    //
    Type = m_Tokenizer.Read();
    if( Type != token_stream::TOKEN_DELIMITER )
    {
        return ReportError( "Expected a field declaration" );
    }

    if( m_Tokenizer.Delimiter() != '{' )
    {
        return ReportError( "Puntuation missing. Expecting { found %c", m_Tokenizer.Delimiter() );
    }

    //
    // Now we are ready to read fields
    //
    while( 1 )
    {
        // Update number of fields allocated
        if( m_Record.nFields >= m_Record.nFieldsAllocated )
        {
            m_Record.nFieldsAllocated = MAX( 32, ( m_Record.nFields + 1 ) + ( m_Record.nFields / 2 ) );
            m_Record.Field            = (field*)x_realloc( m_Record.Field, sizeof( field ) * m_Record.nFieldsAllocated );
            ASSERT( m_Record.Field );
        }
        field& Field = m_Record.Field[m_Record.nFields];

        //
        // First must find a string
        //
        Type = m_Tokenizer.Read();
        if( Type != token_stream::TOKEN_SYMBOL )
        {
            // Are we done reading fields?
            if( Type == token_stream::TOKEN_DELIMITER )
            {
                if( m_Tokenizer.Delimiter() == '}' )
                {
                    break;
                }
            }

            return ReportError( "Expecting to read a string for the name of the field but found something else" );
        }

        // Read the name of the field
        if( x_strlen( m_Tokenizer.String() ) >= TEXTFILE_MAX_STRLENGTH )
        {
            return ReportError( "Field name too long" );
        }

        x_strcpy( Field.Name, m_Tokenizer.String() );

        // Field names must be unique within a section.
        for( s32 i = 0; i < m_Record.nFields; i++ )
        {
            if( x_stricmp( m_Record.Field[i].Name, Field.Name ) == 0 )
            {
                return ReportError( "Duplicate field %s", Field.Name );
            }
        }

        //
        // Now we must find a delimeter
        //
        Type = m_Tokenizer.Read();
        if( Type != token_stream::TOKEN_DELIMITER )
        {
            return ReportError( "Puntuation missing. Expecting : found somthing else" );
        }

        if( m_Tokenizer.Delimiter() != ':' )
        {
            return ReportError( "Puntuation missing. Expecting : found %c", m_Tokenizer.Delimiter() );
        }

        //
        // Now we should read a string
        //
        Type = m_Tokenizer.Read();
        if( Type != token_stream::TOKEN_SYMBOL )
        {
            return ReportError( "Expecting the type of the field but found something else" );
        }

        //
        // We must interpret the string into types
        //
        {
            char* pStr = m_Tokenizer.String();

            ASSERT( pStr );
            Field.nTypes = 0;

            while( *pStr )
            {
                if( Field.nTypes >= TEXTFILE_MAX_TYPES )
                {
                    return ReportError( "Too many field types" );
                }

                switch( *pStr )
                {
                    case 'd':
                    {
                        Field.Type[Field.nTypes++] = TYPE_INTEGER;
                        break;
                    }

                    case 'D':
                    {
                        Field.Type[Field.nTypes++] = TYPE_INTEGER;
                        break;
                    }

                    case 'f':
                    {
                        Field.Type[Field.nTypes++] = TYPE_FLOAT;
                        break;
                    }

                    case 'F':
                    {
                        Field.Type[Field.nTypes++] = TYPE_FLOAT;
                        break;
                    }

                    case 'S':
                    {
                        Field.Type[Field.nTypes++] = TYPE_STRING;
                        break;
                    }

                    case 's':
                    {
                        Field.Type[Field.nTypes++] = TYPE_STRING;
                        break;
                    }

                    case 'G':
                    {
                        Field.Type[Field.nTypes++] = TYPE_GUID;
                        break;
                    }

                    case 'g':
                    {
                        Field.Type[Field.nTypes++] = TYPE_GUID;
                        break;
                    }

                    default:
                    {
                        return ReportError( "Unkown type %c from %s field", *pStr, Field.Name );
                    }
                }

                // Get the next character
                pStr++;
            }
        }

        //
        // Okay we are done with this field
        //
        m_Record.nFields++;
    }

    // DONE
    return TRUE;
}

//=========================================================================

xbool text_in::ReadHeader( void )
{
    if( HasError() )
    {
        return FALSE;
    }

    token_stream::type Type;

    // Clear the record name
    m_Record.Name[0]   = 0;
    m_Record.Count     = -1;
    m_RecordLineNumber = 0;

    // Expect the next header directly; do not skip malformed records.
    Type = m_Tokenizer.Read();

    if( Type == token_stream::TOKEN_ERROR )
    {
        return ReportError( "%s", m_Tokenizer.GetError() );
    }

    if( Type == token_stream::TOKEN_EOF )
    {
        return FALSE;
    }

    if( Type != token_stream::TOKEN_DELIMITER || m_Tokenizer.Delimiter() != '[' )
    {
        return ReportError( "Expected a section header" );
    }

    //
    // Now it should be fallow by a string
    //
    Type = m_Tokenizer.Read();
    if( Type != token_stream::TOKEN_SYMBOL )
    {
        return ReportError( "Expecting a string found something else" );
    }

    if( x_strlen( m_Tokenizer.String() ) >= sizeof( m_Record.Name ) )
    {
        return ReportError( "Header name too long" );
    }

    x_strcpy( m_Record.Name, m_Tokenizer.String() );

    //
    // Now if must be fallow by a delimeter
    //
    Type = m_Tokenizer.Read();
    if( Type != token_stream::TOKEN_DELIMITER )
    {
        return ReportError( "Expecting a ] or : But found something else" );
    }

    // If it is the end of the block we are then done
    if( m_Tokenizer.Delimiter() == ']' )
    {
        return ReadAllFields();
    }

    // We don't know that is going on so just return
    if( m_Tokenizer.Delimiter() != ':' )
    {
        return ReportError( "Expecting a ] or : But found %c", m_Tokenizer.Delimiter() );
    }

    //
    // Must be a integer at this point
    //
    Type = m_Tokenizer.Read();
    if( Type != token_stream::TOKEN_NUMBER || m_Tokenizer.IsFloat() )
    {
        return ReportError( "After : specting an integer but found something else" );
    }

    // Get our count
    m_Record.Count = m_Tokenizer.Int();

    if( m_Record.Count < 0 )
    {
        return ReportError( "Section count must be nonnegative" );
    }

    //
    // Now it must be a nother delimerter
    //
    Type = m_Tokenizer.Read();
    if( Type != token_stream::TOKEN_DELIMITER || m_Tokenizer.Delimiter() != ']' )
    {
        return ReportError( "Expecting ] But stead found something else" );
    }

    //
    // Now start reading fields
    //
    return ReadAllFields();
}

//=========================================================================

xbool text_in::ReadFields( void )
{
    if( HasError() )
    {
        return FALSE;
    }

    if( !m_Record.Name[0] )
    {
        return ReportError( "No section header has been read" );
    }

    if( m_Record.nFields > m_nDataEntriesAllocated )
    {
        delete[] m_pData;
        m_nDataEntriesAllocated = m_Record.nFields;
        m_pData                 = new data[m_nDataEntriesAllocated];
    }
    for( s32 f = 0; f < m_Record.nFields; f++ )
    {
        field& Field = m_Record.Field[f];
        data&  Data  = m_pData[f];

        Data.nFloats   = 0;
        Data.nIntegers = 0;
        Data.nStrings  = 0;
        Data.nGuids    = 0;

        for( s32 i = 0; i < Field.nTypes; i++ )
        {
            token_stream::type Type = m_Tokenizer.Read();

            if( Type == token_stream::TOKEN_ERROR )
            {
                return ReportError( "%s", m_Tokenizer.GetError() );
            }

            // Read the
            switch( Field.Type[i] )
            {
                case TYPE_FLOAT:
                {
                    if( Data.nFloats >= TEXTFILE_MAX_FLOATS )
                    {
                        return ReportError( "Too many floats" );
                    }
                
                    if( Type != token_stream::TOKEN_NUMBER )
                    {
                        return ReportError( "Expecting a FLOAT but found something else" );
                    }
                
                    Data.Float[Data.nFloats++] = m_Tokenizer.Float();
                
                    break;
                }
            
                case TYPE_INTEGER:
                {
                    if( Data.nIntegers >= TEXTFILE_MAX_INTS )
                    {
                        return ReportError( "Too many integers" );
                    }
                
                    if( Type != token_stream::TOKEN_NUMBER || m_Tokenizer.IsFloat() )
                    {
                        return ReportError( "Expecting a INTEGER but found something else" );
                    }
                
                    Data.Integer[Data.nIntegers++] = m_Tokenizer.Int();
                
                    break;
                }
            
                case TYPE_STRING:
                {
                    if( Data.nStrings >= TEXTFILE_MAX_STRINGS )
                    {
                        return ReportError( "Too many strings" );
                    }
                
                    if( Type != token_stream::TOKEN_STRING )
                    {
                        return ReportError( "Expecting a STRING but found something else" );
                    }
                
                    Data.String[Data.nStrings++] = m_Tokenizer.String();
                
                    break;
                }
            
                case TYPE_GUID:
                {
                    if( Data.nGuids >= TEXTFILE_MAX_GUIDS )
                    {
                        return ReportError( "Too many GUIDs" );
                    }
                
                    if( Type != token_stream::TOKEN_STRING )
                    {
                        return ReportError( "Expecting a GUID but found something else" );
                    }
                
                    guid GUID;
                
                    // Read 16 hexadecimal digits with an optional middle colon.
                    {
                        GUID.Guid             = 0;
                        const char* pGUID     = m_Tokenizer.String();
                        s32         Digits    = 0;
                        xbool       Separator = FALSE;
                    
                        while( *pGUID )
                        {
                            char c = *pGUID;
                            pGUID++;
                        
                            if( c == ':' )
                            {
                                if( Digits != 8 || Separator )
                                {
                                    return ReportError( "Invalid GUID separator" );
                                }
                            
                                Separator = TRUE;
                                continue;
                            }
                        
                            c = x_toupper( c );
                        
                            if( !x_ishex( c ) || Digits >= 16 )
                            {
                                return ReportError( "Invalid GUID" );
                            }
                        
                            u32 v = c <= '9' ? c - '0' : c - 'A' + 10;
                        
                            GUID.Guid <<= 4;
                            GUID.Guid |= ( v & 0xF );
                            Digits++;
                        }
                    
                        if( Digits != 16 )
                        {
                            return ReportError( "GUID requires 16 hexadecimal digits" );
                        }
                    }
                
                    Data.Guid[Data.nGuids++] = GUID;
                
                    break;
                }
            
                default:
                {
                    return ReportError( "Invalid field type" );
                }
            }
        }
    }

    // Increase the line number
    m_RecordLineNumber++;

    return TRUE;
}

//=========================================================================

s32 text_in::FindField( const char* pName, s32 Length )
{
    if( HasError() )
    {
        return -1;
    }

    if( m_RecordLineNumber <= 0 )
    {
        ReportError( "No field values have been read" );
        return -1;
    }

    // Compare the complete name without constructing a temporary string.
    for( s32 i = 0; i < m_Record.nFields; i++ )
    {
        const char* pField    = m_Record.Field[i].Name;
        s32         Character = 0;

        while( Character < Length && pField[Character] &&
               x_tolower( pField[Character] ) == x_tolower( pName[Character] ) )
        {
            Character++;
        }

        if( Character == Length && !pField[Character] )
        {
            return i;
        }
    }

    xstring Name( pName, Length );
    ReportError( "Field %s was not found", (const char*)Name );
    return -1;
}

//=========================================================================

xbool text_in::GetField( const char* pFieldName, ... )
{
    const char* pType = x_strchr( pFieldName, ':' );

    if( !pType )
    {
        return ReportError( "Missing field type in %s", pFieldName );
    }

    x_va_list Args;
    x_va_start( Args, pFieldName );
    xbool Result = GetFieldValuesV( pFieldName, (s32)( pType - pFieldName ), pType + 1, Args );
    x_va_end( Args );
    return Result;
}

//=========================================================================
// HELPER FUNCTIONS
//=========================================================================

xbool text_in::GetFieldValues( const char* pName, const char* pTypes, ... )
{
    x_va_list Args;
    x_va_start( Args, pTypes );
    xbool Result = GetFieldValuesV( pName, x_strlen( pName ), pTypes, Args );
    x_va_end( Args );
    return Result;
}

//=========================================================================

xbool text_in::GetFieldValuesV( const char* pName, s32 NameLength,
                                const char* pTypes, x_va_list Args )
{
    // Validate the requested signature before writing any values.
    s32 Index = FindField( pName, NameLength );

    if( Index < 0 )
    {
        return FALSE;
    }

    const field& Field = m_Record.Field[Index];
    const data&  Data  = m_pData[Index];

    if( x_strlen( pTypes ) != Field.nTypes )
    {
        return ReportError( "Field %s has a different type count", Field.Name );
    }

    for( s32 i = 0; i < Field.nTypes; i++ )
    {
        type Type = TYPE_NULL;

        switch( x_tolower( pTypes[i] ) )
        {
            case 'd':
            {
                Type = TYPE_INTEGER;
                break;
            }

            case 'f':
            {
                Type = TYPE_FLOAT;
                break;
            }

            case 's':
            {
                Type = TYPE_STRING;
                break;
            }

            case 'g':
            {
                Type = TYPE_GUID;
                break;
            }
        }

        if( Type == TYPE_NULL || Type != Field.Type[i] )
        {
            return ReportError( "Field %s has a different type", Field.Name );
        }
    }

    // Fixed destinations cannot receive strings beyond their declared limit.
    for( s32 i = 0; i < Data.nStrings; i++ )
    {
        if( Data.String[i].GetLength() >= TEXTFILE_MAX_STRLENGTH )
        {
            return ReportError( "String requires a dynamic destination" );
        }
    }

    // Copy validated field values to the caller's destinations.
    s32 I = 0;
    s32 F = 0;
    s32 S = 0;
    s32 G = 0;

    for( s32 i = 0; i < Field.nTypes; i++ )
    {
        switch( Field.Type[i] )
        {
            case TYPE_INTEGER:
            {
                s32* pValue = va_arg( Args, s32* );
                ASSERT( pValue );
                *pValue = Data.Integer[I++];
                break;
            }

            case TYPE_FLOAT:
            {
                f32* pValue = va_arg( Args, f32* );
                ASSERT( pValue );
                *pValue = Data.Float[F++];
                break;
            }

            case TYPE_STRING:
            {
                char* pValue = va_arg( Args, char* );
                ASSERT( pValue );
                x_strcpy( pValue, Data.String[S++] );
                break;
            }

            case TYPE_GUID:
            {
                guid* pValue = va_arg( Args, guid* );
                ASSERT( pValue );
                *pValue = Data.Guid[G++];
                break;
            }

            default:
            {
                return ReportError( "Invalid field type" );
            }
        }
    }

    return TRUE;
}

//=========================================================================
// IMPLEMENTATION
//=========================================================================

xbool text_in::SkipToNextHeader( void )
{
    if( HasError() )
    {
        return FALSE;
    }

    // Skip remaining records; uncounted sections contain one record.
    s32 Count = m_Record.Count < 0 ? 1 : m_Record.Count;

    while( m_RecordLineNumber < Count )
    {
        if( !ReadFields() )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool text_in::GetVector3( const char* pName, vector3& V )
{
    return GetFieldValues( pName, "fff", &V.GetX(), &V.GetY(), &V.GetZ() );
}

//=========================================================================

xbool text_in::GetColor( const char* pName, xcolor& C )
{
    s32 R, G, B, A;

    if( !GetFieldValues( pName, "dddd", &R, &G, &B, &A ) )
    {
        return FALSE;
    }

    if( R < 0 || R > 255 || G < 0 || G > 255 || B < 0 || B > 255 || A < 0 || A > 255 )
    {
        return ReportError( "Color components must be between 0 and 255" );
    }

    C = xcolor( (byte)R, (byte)G, (byte)B, (byte)A );
    return TRUE;
}

//=========================================================================

xbool text_in::GetF32( const char* pName, f32& F )
{
    return GetFieldValues( pName, "f", &F );
}

//=========================================================================

xbool text_in::GetS32( const char* pName, s32& I )
{
    return GetFieldValues( pName, "d", &I );
}

//=========================================================================

xbool text_in::GetString( const char* pName, char* pStr )
{
    return GetFieldValues( pName, "s", pStr );
}

//=========================================================================

xbool text_in::GetString( const char* pName, xstring& String )
{
    s32 Index = FindField( pName, x_strlen( pName ) );

    if( Index < 0 )
    {
        return FALSE;
    }

    const field& Field = m_Record.Field[Index];

    if( Field.nTypes != 1 || Field.Type[0] != TYPE_STRING )
    {
        return ReportError( "Field %s must be a string", pName );
    }

    String = m_pData[Index].String[0];
    return TRUE;
}

//=========================================================================

xbool text_in::GetBBox( const char* pName, bbox& BBox )
{
    return GetFieldValues( pName, "ffffff",
                           &BBox.Min.GetX(), &BBox.Min.GetY(), &BBox.Min.GetZ(),
                           &BBox.Max.GetX(), &BBox.Max.GetY(), &BBox.Max.GetZ() );
}

//=========================================================================

xbool text_in::GetRadian3( const char* pName, radian3& Orient )
{
    f32 P, Y, R;

    if( !GetFieldValues( pName, "fff", &P, &Y, &R ) )
    {
        return FALSE;
    }

    Orient.Pitch = DEG_TO_RAD( P );
    Orient.Yaw   = DEG_TO_RAD( Y );
    Orient.Roll  = DEG_TO_RAD( R );

    return TRUE;
}

//=========================================================================

xbool text_in::GetQuaternion( const char* pName, quaternion& Q )
{
    return GetFieldValues( pName, "ffff", &Q.X, &Q.Y, &Q.Z, &Q.W );
}

//=========================================================================

xbool text_in::GetBool( const char* pName, xbool& Bool )
{
    s32 Value;

    if( !GetFieldValues( pName, "d", &Value ) )
    {
        return FALSE;
    }

    if( Value != 0 && Value != 1 )
    {
        return ReportError( "Boolean must be 0 or 1" );
    }

    Bool = Value != 0;
    return TRUE;
}

//=========================================================================

xbool text_in::GetGuid( const char* pName, guid& Guid )
{
    return GetFieldValues( pName, "g", &Guid );
}
