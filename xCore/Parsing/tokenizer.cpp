//==============================================================================
//  
//  tokenizer.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "x_stdio.hpp"
#include "x_memory.hpp"
#include "tokenizer.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#if !defined( TARGET_DESKTOP )
#define USING_STREAMING
#endif

//==============================================================================
//  STORAGE
//==============================================================================

static const s32 BUFFER_SIZE = 1024 * 32;

//==============================================================================
// USING STREAMING
//==============================================================================

#ifdef USING_STREAMING

char token_stream::CHAR( s32 FilePos )
{
    if( (FilePos < 0) || (FilePos >= m_FileSize) )
    {
        return 0;
    }

    if( m_bBuffered )
    {
        if( (FilePos >= m_CurBufferStart) && (FilePos <= m_CurBufferEnd) )
        {
            return m_CurBuffer[ FilePos - m_CurBufferStart ];
        }
    
        // ELSE: reload a new buffer chunk
        //
        // Determine which BUFFER_SIZE'd block the filepos falls into
        s32 iChunk           = FilePos / BUFFER_SIZE;
        
        m_CurBufferStart     = iChunk * BUFFER_SIZE;

        s32 Size             = MIN( BUFFER_SIZE, m_FileSize - m_CurBufferStart );

        m_CurBufferEnd       = m_CurBufferStart + Size - 1;

        if( x_fseek( m_pFile, m_CurBufferStart + m_StartPosition, X_SEEK_SET ) != 0 ||
            x_fread( m_CurBuffer, 1, Size, m_pFile ) != Size )
        {
            m_CurBufferStart = -1;
            m_CurBufferEnd = -1;
            ReportError( "Unable to read input file" );
            return 0;
        }

        return m_CurBuffer[ FilePos - m_CurBufferStart ];
    }
    else
    {
        return m_FileBuffer[FilePos];
    }

}

//==============================================================================

xbool token_stream::OpenFile( const char* pFileName )
{
    CloseFile();
    Rewind();

    // Clear class
    m_FileSize   = 0;
    m_FilePos    = 0;
    m_LineNumber = 1;
    m_type = TOKEN_NONE;
    
    if( x_strlen(pFileName)>63 )
    {
        x_strcpy(m_Filename,pFileName+x_strlen(pFileName)-63);
    }
    else
    {
        x_strcpy(m_Filename,pFileName);
    }

    // Open file
    m_pFile = x_fopen( pFileName, "rb" );

    if( m_pFile == NULL )
    {
        ReportError( "Unable to open input file" );
        return FALSE;
    }

    m_bBuffered      = TRUE;
    m_CurBufferStart = -1;
    m_CurBufferEnd   = -1;
    m_CurBuffer      = (char*)x_malloc(BUFFER_SIZE);
    if( m_CurBuffer == NULL )
    {
        x_fclose( m_pFile );
        m_pFile = NULL;
        ReportError( "Unable to allocate input buffer" );
        return FALSE;
    }

    // Find how large the file is
    m_FileSize = x_flength(m_pFile);

    if( m_FileSize < 0 )
    {
        CloseFile();
        ReportError( "Invalid input file size" );
        return FALSE;
    }

    Rewind();

    m_StartPosition = 0;

    return( TRUE );
}

//==============================================================================

xbool token_stream::OpenFile( X_FILE* fp )
{
    CloseFile();
    Rewind();
    x_strcpy( m_Filename, "<file>" );

    // Clear class
    m_FileSize          = 0;
    m_FilePos           = 0;
    m_LineNumber        = 1;
    m_type              = TOKEN_NONE;
    
    m_pFile = fp;

    if( m_pFile == NULL )
    {
        ReportError( "Invalid input file" );
        return FALSE;
    }

    m_bBuffered         = TRUE;
    m_CurBufferStart    = -1;
    m_CurBufferEnd      = -1;
    m_CurBuffer         = (char*)x_malloc(BUFFER_SIZE);
    if( m_CurBuffer == NULL )
    {
        x_fclose( m_pFile );
        m_pFile = NULL;
        ReportError( "Unable to allocate input buffer" );
        return FALSE;
    }

    // Find how large the file is
    m_FileSize          = x_flength(m_pFile);
    m_StartPosition     = x_ftell(m_pFile);
    m_FileSize         -= m_StartPosition;

    if( m_StartPosition < 0 || m_FileSize < 0 )
    {
        CloseFile();
        ReportError( "Invalid input file size or position" );
        return FALSE;
    }

    Rewind();

    return( TRUE );
}

//==============================================================================
// NOT USING STREAMING
//==============================================================================

#else

char token_stream::CHAR( s32 FilePos )
{
    if( FilePos < 0 || FilePos >= m_FileSize )
    {
        return 0;
    }

    return m_FileBuffer[FilePos];
}

//==============================================================================

xbool token_stream::OpenFile( const char* pFileName )
{
    CloseFile();
    Rewind();

    X_FILE* fp;

    // Clear class
    m_FileSize          = 0;
    m_FilePos           = 0;
    m_LineNumber        = 1;
    m_type              = TOKEN_NONE;
    m_bBuffered         = FALSE;

    if( x_strlen(pFileName)>63 )
    {
        x_strcpy(m_Filename,pFileName+x_strlen(pFileName)-63);
    }
    else
    {
        x_strcpy(m_Filename,pFileName);
    }
    // Open file
    fp = x_fopen( pFileName, "rb" );
    if( fp == NULL )
    {
        ReportError( "Unable to open input file" );
        return FALSE;
    }

    // Find how large the file is
    m_FileSize = x_flength(fp);

    if( m_FileSize < 0 || m_FileSize == 0x7FFFFFFF )
    {
        x_fclose( fp );
        ReportError( "Invalid input file size" );
        return FALSE;
    }

    // Allocate a buffer to hold the file
    m_FileBuffer = (char*)x_malloc(m_FileSize+1);
    if( m_FileBuffer == NULL )
    {
        x_fclose( fp );
        ReportError( "Unable to allocate input buffer" );
        return FALSE;
    }

    // Load the entire file into the buffer
    m_FileBuffer[m_FileSize] = 0;
    if( x_fread( m_FileBuffer, 1, m_FileSize, fp ) != m_FileSize )
    {
        x_free(m_FileBuffer);
        m_FileBuffer = NULL;
        x_fclose( fp );
        ReportError( "Unable to read input file" );
        return FALSE;
    }
    
    // Close the input file
    x_fclose( fp );

    Rewind();

    return( TRUE );
}

//==============================================================================

xbool token_stream::OpenFile( X_FILE* fp )
{
    CloseFile();
    Rewind();
    x_strcpy( m_Filename, "<file>" );

    // Clear class
    m_FileSize   = 0;
    m_FilePos    = 0;
    m_LineNumber = 1;
    m_type = TOKEN_NONE;
    m_bBuffered = FALSE;

    if( fp == NULL )
    {
        ReportError( "Invalid input file" );
        return FALSE;
    }

    // Find how large the file is
    m_FileSize = x_flength(fp);

    // Avoid characters that have already been read
    s32 Position = x_ftell( fp );

    if( Position < 0 || m_FileSize < Position )
    {
        x_fclose( fp );
        ReportError( "Invalid input file size or position" );
        return FALSE;
    }

    m_FileSize -= Position;

    if( m_FileSize == 0x7FFFFFFF )
    {
        x_fclose( fp );
        ReportError( "Input file is too large" );
        return FALSE;
    }

    // Allocate a buffer to hold the file
    m_FileBuffer = (char*)x_malloc(m_FileSize+1);
    //Make sure the end of the array is touched to keep purify happy

    if( m_FileBuffer == NULL )
    {
        x_fclose( fp );
        ReportError( "Unable to allocate input buffer" );
        return FALSE;
    }

    // Load the entire file into the buffer
    m_FileBuffer[m_FileSize] = 0;
    //ARH I removed this line because it may not be true if the file pointer has been 
    //      partially used.
    //if( x_fread( m_FileBuffer, 1, m_FileSize, fp ) != m_FileSize )
    if( x_fread( m_FileBuffer, 1, m_FileSize, fp ) != m_FileSize )
    {
        x_free(m_FileBuffer);
        m_FileBuffer = NULL;
        x_fclose( fp );
        ReportError( "Unable to read input file" );
        return FALSE;
    }
    
    // Close the input file
    x_fclose( fp );

    Rewind();

    return( TRUE );
}

//==============================================================================
// END OF STREAMING / NONSTREAMING SECTIONS
//==============================================================================

#endif

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

token_stream::token_stream( void )
{
    m_StringEscaping    = FALSE;
    m_MultilineStrings  = FALSE;
    m_FileBuffer        = NULL;
    m_FileSize          = 0;
    m_FilePos           = 0;
    m_LineNumber        = 1;
    m_CurBuffer         = NULL;
    m_bBuffered         = FALSE;
    m_CurBufferStart    = -1;
    m_CurBufferEnd      = -1;
    m_pFile             = NULL;
    m_Filename[0]       = 0;
    m_StartPosition     = 0;
    m_type              = TOKEN_NONE;
    m_String[0]         = 0;
    m_Float             = 0.0f;
    m_Int               = 0;
    m_IsFloat           = FALSE;
    m_Delimiter         = ' ';

    // Fill out delimiters
    x_strcpy(m_DelimiterStr,TOKEN_DELIMITER_STR);

    // Setup number chars
    s32 i;
    char IStr[] = "0123456789-+";
    char FStr[] = "EeDd.#QNABIF";
    for( i=0; i<256; i++ )
        m_IsCharNumber[i] = 0;
    i=0;
    while( IStr[i] )
    {
        m_IsCharNumber[ IStr[i] ] = 1;
        i++;
    }
    i=0;
    while( FStr[i] )
    {
        m_IsCharNumber[ FStr[i] ] = 2;
        i++;
    }

    //x_strcpy(m_NumberStr,"0123456789-+Ee.#QNABIF");
}

//==============================================================================

token_stream::~token_stream( void )
{
    CloseFile();
}

//==============================================================================

xbool token_stream::IsEOF( void ) const 
{
    return m_FilePos >= m_FileSize;
}


//==============================================================================

char* token_stream::GetDelimeter( void )
{
    return m_DelimiterStr;
}

//==============================================================================

void token_stream::SetDelimeter( const char* pStr )
{
    if( !pStr || x_strlen( pStr ) >= sizeof(m_DelimiterStr) )
    {
        ReportError( "Invalid delimiter set" );
        return;
    }

    x_strcpy(m_DelimiterStr, pStr);
}

//==============================================================================

void token_stream::SetStringMode( xbool Escaping, xbool Multiline )
{
    m_StringEscaping   = Escaping;
    m_MultilineStrings = Multiline;
}

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

token_stream::type token_stream::ReportError( const char* pMessage )
{
    // Preserve the first error until the stream is reset.
    if( m_Error.IsEmpty() )
    {
        m_Error = pMessage;
    }

    m_type      = TOKEN_ERROR;
    m_String[0] = 0;
    m_Float     = 0.0f;
    m_Int       = 0;
    m_IsFloat   = FALSE;
    return m_type;
}

//==============================================================================

xbool token_stream::PrepareRead( void )
{
    if( !m_Error.IsEmpty() )
    {
        m_type = TOKEN_ERROR;
        return FALSE;
    }

    // Clear token data before advancing the stream.
    m_type      = TOKEN_NONE;
    m_Delimiter = ' ';
    m_Float     = 0.0f;
    m_Int       = 0;
    m_IsFloat   = FALSE;
    m_String[0] = 0;

    SkipWhitespace();

    if( !m_Error.IsEmpty() )
    {
        m_type = TOKEN_ERROR;
        return FALSE;
    }

    if( m_FilePos >= m_FileSize )
    {
        m_type = TOKEN_EOF;
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

token_stream::type token_stream::ConvertNumber( const char* pText, xbool IntegerOnly )
{
    if( !m_Error.IsEmpty() )
    {
        m_type = TOKEN_ERROR;
        return m_type;
    }

    // Validate the sign, mantissa and exponent before conversion.
    const char* pDigit = pText;
    xbool Negative = FALSE;

    if( *pDigit == '-' || *pDigit == '+' )
    {
        Negative = *pDigit == '-';
        pDigit++;
    }

    const char* pFirst = pDigit;

    while( *pDigit >= '0' && *pDigit <= '9' )
    {
        pDigit++;
    }

    s32 Digits = (s32)(pDigit - pFirst);
    xbool IsFloat = FALSE;

    if( *pDigit == '.' )
    {
        IsFloat = TRUE;
        pDigit++;

        while( *pDigit >= '0' && *pDigit <= '9' )
        {
            Digits++;
            pDigit++;
        }
    }

    if( !Digits )
    {
        return ReportError( "Expected a number" );
    }

    if( *pDigit == 'e' || *pDigit == 'E' || *pDigit == 'd' || *pDigit == 'D' )
    {
        IsFloat = TRUE;
        pDigit++;

        if( *pDigit == '-' || *pDigit == '+' )
        {
            pDigit++;
        }

        const char* pExponent = pDigit;

        while( *pDigit >= '0' && *pDigit <= '9' )
        {
            pDigit++;
        }

        if( pDigit == pExponent )
        {
            return ReportError( "Missing number exponent" );
        }
    }

    if( *pDigit || (IntegerOnly && IsFloat) )
    {
        return ReportError( IntegerOnly ? "Expected an integer" : "Invalid number" );
    }

    if( IsFloat )
    {
        // Bound the mantissa and track the remaining decimal scale.
        f64 Value = 0.0;
        s32 Scale = 0;
        s32 Significant = 0;
        xbool Fraction = FALSE;

        for( pDigit = pFirst; *pDigit && *pDigit != 'e' && *pDigit != 'E' &&
                             *pDigit != 'd' && *pDigit != 'D'; pDigit++ )
        {
            if( *pDigit == '.' )
            {
                Fraction = TRUE;
                continue;
            }

            s32 Digit = *pDigit - '0';

            if( Significant || Digit )
            {
                if( Significant < 17 )
                {
                    Value = Value * 10.0 + Digit;
                    Significant++;
                }
                else
                {
                    Scale++;
                }
            }

            if( Fraction )
            {
                Scale--;
            }
        }

        if( *pDigit )
        {
            pDigit++;
            xbool ExponentNegative = *pDigit == '-';

            if( *pDigit == '-' || *pDigit == '+' )
            {
                pDigit++;
            }

            s32 Exponent = 0;

            while( *pDigit )
            {
                Exponent = MIN( Exponent * 10 + (*pDigit - '0'), 10000 );
                pDigit++;
            }

            Scale += ExponentNegative ? -Exponent : Exponent;
        }

        if( Value != 0.0 )
        {
            if( Scale > 38 )
            {
                return ReportError( "Float out of range" );
            }

            if( Scale < -64 )
            {
                Value = 0.0;
            }
            else
            {
                while( Scale > 0 )
                {
                    Value *= 10.0;
                    Scale--;
                }

                while( Scale < 0 )
                {
                    Value /= 10.0;
                    Scale++;
                }
            }
        }

        if( Value > 3.4028234663852886e38 )
        {
            return ReportError( "Float out of range" );
        }

        m_Float = (f32)(Negative ? -Value : Value);
        if( !x_isvalid( m_Float ) )
        {
            return ReportError( "Float out of range" );
        }

        m_Int = (m_Float >= -2147483648.0f && m_Float < 2147483648.0f) ? (s32)m_Float : 0;
    }
    else
    {
        // Check the signed limit before each unsigned accumulation.
        u32 Value = 0;
        u32 Limit = Negative ? 0x80000000u : 0x7FFFFFFFu;

        for( pDigit = pFirst; *pDigit; pDigit++ )
        {
            u32 Digit = (u32)(*pDigit - '0');

            if( Value > (Limit - Digit) / 10 )
            {
                return ReportError( "Integer out of range" );
            }

            Value = Value * 10 + Digit;
        }

        m_Int = Negative ? (s32)(0u - Value) : (s32)Value;
        m_Float = (f32)m_Int;
    }

    m_IsFloat = IsFloat;
    m_type = TOKEN_NUMBER;
    return m_type;
}

//==============================================================================

token_stream::type token_stream::ReadNumber( void )
{
    // Read the complete token before converting its value.
    s32 Length = 0;

    while( m_FilePos < m_FileSize )
    {
        char Character = CHAR( m_FilePos );

        if( !m_IsCharNumber[(u8)Character] )
        {
            break;
        }

        if( Length >= TOKEN_STRING_SIZE - 1 )
        {
            return ReportError( "Numeric token too long" );
        }

        m_String[Length++] = Character;
        m_FilePos++;
    }

    m_String[Length] = 0;
    return ConvertNumber( m_String, FALSE );
}

//==============================================================================

token_stream::type token_stream::ReadRawSymbol( void )
{
    // Symbols end at whitespace or a configured delimiter.
    s32 Length = 0;

    while( m_FilePos < m_FileSize )
    {
        char Character = CHAR( m_FilePos );

        if( !Character )
        {
            return ReportError( "Null character in symbol" );
        }

        if( (u8)Character <= 32 || x_strchr( m_DelimiterStr, Character ) )
        {
            break;
        }

        if( Length >= TOKEN_STRING_SIZE - 1 )
        {
            return ReportError( "Symbol too long" );
        }

        m_String[Length++] = Character;
        m_FilePos++;
    }

    if( !Length )
    {
        return ReportError( "Expected a symbol" );
    }

    m_String[Length] = 0;
    m_type = TOKEN_SYMBOL;
    return m_type;
}

//==============================================================================

token_stream::type token_stream::ReadQuotedString( void )
{
    m_QuotedString.Clear();
    m_FilePos++;

    while( m_FilePos < m_FileSize )
    {
        char Character = CHAR( m_FilePos );
        m_FilePos++;

        if( Character == '"' )
        {
            m_type = TOKEN_STRING;
            return m_type;
        }

        if( Character == '\r' || Character == '\n' )
        {
            if( !m_MultilineStrings )
            {
                return ReportError( "Newline in quoted string" );
            }

            // Normalize physical line endings inside quoted text.
            if( Character == '\r' && CHAR( m_FilePos ) == '\n' )
            {
                m_FilePos++;
            }

            m_LineNumber++;
            Character = '\n';
        }
        else if( Character == '\\' && m_StringEscaping )
        {
            // Decode escapes only when enabled by the text syntax.
            if( m_FilePos >= m_FileSize )
            {
                return ReportError( "Unterminated string escape" );
            }

            Character = CHAR( m_FilePos );
            m_FilePos++;

            switch( Character )
            {
                case 'n':
                {
                    Character = '\n';
                    break;
                }
            
                case 'r':
                {
                    Character = '\r';
                    break;
                }
            
                case 't':
                {
                    Character = '\t';
                    break;
                }
            
                case '"':
                {
                    break;
                }
            
                case '\\':
                {
                    break;
                }
            
                default:
                {
                    return ReportError( "Unknown string escape" );
                }
            }
        }

        if( Character == 0 )
        {
            return ReportError( "Null character in quoted string" );
        }

        m_QuotedString += Character;
    }

    return ReportError( "Unterminated quoted string" );
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void token_stream::CloseFile( void )
{
    if( m_bBuffered )
    {
        if( m_CurBuffer )
        {
            x_free( m_CurBuffer );
            if (m_pFile)
            {
                x_fclose(m_pFile);
            }
            m_pFile             = NULL;
            m_CurBuffer         = NULL;
            m_CurBufferStart    = -1;
            m_CurBufferEnd      = -1;
        }
    }
    else
    {
        x_free(m_FileBuffer);
        m_FileBuffer = NULL;
        m_FileSize = 0;
        m_pFile = NULL;
    }

    m_bBuffered = FALSE;
    m_FileSize = 0;
    m_FilePos = 0;
    m_type = TOKEN_NONE;
}

//==============================================================================

void token_stream::Rewind( void )
{
    m_Error.Clear();
    m_QuotedString.Clear();
    m_FilePos          = 0;
    m_LineNumber       = 1;
    m_type             = TOKEN_NONE;
    m_Delimiter        = ' ';
    m_Float            = 0.0f;
    m_Int              = 0;
    m_IsFloat          = FALSE;
    m_String[0]        = 0;
}

//==============================================================================

void token_stream::SetCursor( s32 Pos )
{
    if( Pos < 0 || Pos > m_FileSize )
    {
        ReportError( "Cursor outside input" );
        return;
    }

    // Recompute the source line for diagnostics at the new cursor.
    Rewind();

    while( m_FilePos < Pos )
    {
        char Character = CHAR( m_FilePos );
        m_FilePos++;

        if( Character == '\n' || (Character == '\r' && CHAR(m_FilePos) != '\n') )
        {
            m_LineNumber++;
        }
    }
}

//==============================================================================

s32 token_stream::GetCursor( void )
{
    return m_FilePos;
}

//==============================================================================

xbool token_stream::Find( const char* TokenStr, xbool FromBeginning )
{
    if( FromBeginning )
        Rewind();

    Read();

    while( m_type != TOKEN_EOF && m_type != TOKEN_ERROR )
    {
        if( x_stricmp(String(),TokenStr)==0 ) return TRUE;
        Read();
    }

    return FALSE;
}

//==============================================================================

void token_stream::SkipToNextLine( void )
{
    if( !m_Error.IsEmpty() )
    {
        m_type = TOKEN_ERROR;
        return;
    }

    while( m_FilePos < m_FileSize )
    {
        char Character = CHAR( m_FilePos );
        m_FilePos++;

        if( !Character )
        {
            ReportError( "Null character in line" );
            return;
        }

        if( Character == '\r' || Character == '\n' )
        {
            if( Character == '\r' && CHAR(m_FilePos) == '\n' )
            {
                m_FilePos++;
            }

            m_LineNumber++;
            return;
        }
    }
}

//==============================================================================

void token_stream::SkipWhitespace( void )
{
    // A UTF-8 BOM is valid only at the start of the input.
    if( m_FilePos == 0 && (u8)CHAR(0) == 0xEF &&
        (u8)CHAR(1) == 0xBB && (u8)CHAR(2) == 0xBF )
    {
        m_FilePos = 3;
    }

    while( 1 )
    {
        if( m_FilePos >= m_FileSize )
            return;

        // Skip whitespace and count EOLNs
        if( CHAR(m_FilePos) == 0 )
        {
            ReportError( "Null character in input" );
            return;
        }

        if( (u8)CHAR(m_FilePos)<=32 )
        {
            while( (m_FilePos<m_FileSize) && CHAR(m_FilePos) && ((u8)CHAR(m_FilePos)<=32) )
            {
                if( CHAR(m_FilePos) == '\n' ||
                    (CHAR(m_FilePos) == '\r' && CHAR(m_FilePos+1) != '\n') )
                {
                    m_LineNumber++;
                }
                m_FilePos++;
            }

            continue;
        }

        // Watch for line comment
        if( (m_FilePos < m_FileSize) &&
            (CHAR(m_FilePos+0) == '/') && (CHAR(m_FilePos+1) == '/') )
        {
            m_FilePos += 2;

            while( m_FilePos < m_FileSize && CHAR(m_FilePos) != '\n' && CHAR(m_FilePos) != '\r' )
            {
                if( !CHAR(m_FilePos) )
                {
                    ReportError( "Null character in comment" );
                    return;
                }

                m_FilePos++;
            }

            continue;
        }

        // Watch for block comment
        if( (m_FilePos < m_FileSize) &&
            (CHAR(m_FilePos+0) == '/') && (CHAR(m_FilePos+1) == '*') )
        {
            m_FilePos += 2;

            xbool Closed = FALSE;

            while( m_FilePos < m_FileSize )
            {
                if( (m_FilePos + 1 < m_FileSize) &&
                    (CHAR(m_FilePos+0) == '*') && (CHAR(m_FilePos+1) == '/') )
                {
                    m_FilePos += 2;
                    Closed = TRUE;
                    break;
                }

                if( !CHAR(m_FilePos) )
                {
                    ReportError( "Null character in comment" );
                    return;
                }

                if( CHAR(m_FilePos) == '\n' ||
                    (CHAR(m_FilePos) == '\r' && CHAR(m_FilePos+1) != '\n') )
                {
                    m_LineNumber++;
                }

                m_FilePos++;
            }

            if( !Closed )
            {
                ReportError( "Unterminated block comment" );
                return;
            }

            continue;
        }

        // No whitespace found
        return;
    }
}

//==============================================================================

token_stream::type token_stream::Read( s32 NTokens )
{
    while( NTokens-- > 0 )
    {
        if( !PrepareRead() )
        {
            return m_type;
        }

        char Character = CHAR( m_FilePos );

        if( !m_Error.IsEmpty() )
        {
            m_type = TOKEN_ERROR;
            return m_type;
        }

        // Dispatch all token reads through the same scanners.
        if( (Character >= '0' && Character <= '9') || Character == '-' || Character == '+' ||
            (Character == '.' && CHAR(m_FilePos+1) >= '0' && CHAR(m_FilePos+1) <= '9') )
        {
            ReadNumber();
        }
        else if( Character == '"' )
        {
            ReadQuotedString();
        }
        else if( x_strchr( m_DelimiterStr, Character ) )
        {
            m_FilePos++;
            m_type       = TOKEN_DELIMITER;
            m_Delimiter  = Character;
            m_String[0]  = Character;
            m_String[1]  = 0;
        }
        else
        {
            ReadRawSymbol();
        }

        if( m_type == TOKEN_ERROR )
        {
            return m_type;
        }
    }

    return m_type;
}

//==============================================================================

f32 token_stream::ReadFloat( void )
{
    if( Read() != TOKEN_NUMBER )
    {
        ReportError( "Expected a float" );
        return 0.0f;
    }

    m_IsFloat = TRUE;
    return m_Float;
}

//==============================================================================

f32 token_stream::ReadF32FromString( void )
{
    ReadString();

    // Quoted numbers use the same syntax and range checks.
    if( m_type != TOKEN_STRING || ConvertNumber( m_QuotedString, FALSE ) == TOKEN_ERROR )
    {
        return 0.0f;
    }

    m_IsFloat = TRUE;
    return m_Float;
}

//==============================================================================

s32 token_stream::ReadS32FromString( void )
{
    ReadString();

    if( m_type != TOKEN_STRING || ConvertNumber( m_QuotedString, TRUE ) == TOKEN_ERROR )
    {
        return 0;
    }

    return m_Int;
}

//==============================================================================

xbool token_stream::ReadBoolFromString( void )
{
    s32 Value = ReadS32FromString();

    if( m_type == TOKEN_ERROR )
    {
        return FALSE;
    }

    if( Value != 0 && Value != 1 )
    {
        ReportError( "Boolean must be 0 or 1" );
        return FALSE;
    }

    return Value != 0;
}

//==============================================================================

s32 token_stream::ReadInt( void )
{
    if( Read() != TOKEN_NUMBER || m_IsFloat )
    {
        ReportError( "Expected an integer" );
        return 0;
    }

    return m_Int;
}

//==============================================================================

s32 token_stream::ReadHex( void )
{
    if( !PrepareRead() )
    {
        ReportError( "Expected a hexadecimal integer" );
        return 0;
    }

    if( CHAR(m_FilePos) != '0' ||
        (CHAR(m_FilePos+1) != 'x' && CHAR(m_FilePos+1) != 'X') )
    {
        ReportError( "Expected a hexadecimal integer" );
        return 0;
    }

    m_FilePos += 2;

    // Preserve all 32 bits, including values with the sign bit set.
    u32 Number = 0;
    s32 Digits = 0;
    m_String[0] = '0';
    m_String[1] = 'x';

    while( m_FilePos < m_FileSize )
    {
        char Character = x_toupper( CHAR(m_FilePos) );

        if( !x_ishex( Character ) )
        {
            break;
        }

        if( Digits >= 8 )
        {
            ReportError( "Hexadecimal integer out of range" );
            return 0;
        }

        u32 Digit = Character <= '9' ? Character - '0' : Character - 'A' + 10;
        Number = (Number << 4) | Digit;
        m_String[2 + Digits++] = Character;
        m_FilePos++;
    }

    if( !m_Error.IsEmpty() )
    {
        m_type = TOKEN_ERROR;
        return 0;
    }

    if( !Digits )
    {
        ReportError( "Missing hexadecimal digits" );
        return 0;
    }

    m_String[2 + Digits] = 0;
    m_Int       = (s32)Number;
    m_Float     = (f32)m_Int;
    m_type      = TOKEN_NUMBER;
    m_IsFloat   = FALSE;

    return m_Int;
}

//==============================================================================

char* token_stream::ReadSymbol( void )
{
    if( !PrepareRead() )
    {
        ReportError( "Expected a symbol" );
        return m_String;
    }

    ReadRawSymbol();
    return m_String;
}

//==============================================================================

char* token_stream::ReadString( void )
{
    if( !PrepareRead() || CHAR(m_FilePos) != '"' )
    {
        ReportError( "Expected a quoted string" );
        return m_String;
    }

    ReadQuotedString();
    return String();
}

//==============================================================================

char* token_stream::ReadLine( void )
{
    if( !m_Error.IsEmpty() )
    {
        m_type = TOKEN_ERROR;
        return m_String;
    }

    s32 Length = 0;
    m_String[0] = 0;

    if( m_FilePos >= m_FileSize )
    {
        m_type = TOKEN_EOF;
        return m_String;
    }

    // Consume one complete line without discarding the next character.
    while( m_FilePos < m_FileSize )
    {
        char Character = CHAR( m_FilePos );

        if( !Character )
        {
            ReportError( "Null character in line" );
            return m_String;
        }

        if( Character == '\r' || Character == '\n' )
        {
            m_FilePos++;

            if( Character == '\r' && CHAR(m_FilePos) == '\n' )
            {
                m_FilePos++;
            }

            m_LineNumber++;
            break;
        }

        if( Length >= TOKEN_STRING_SIZE - 1 )
        {
            ReportError( "Line too long" );
            return m_String;
        }

        m_String[Length++] = Character;
        m_FilePos++;
    }

    m_String[Length] = 0;
    m_type = TOKEN_SYMBOL;
    return m_String;
}

//==============================================================================

char* token_stream::ReadToSymbol( char Sym )
{
    if( !m_Error.IsEmpty() )
    {
        m_type = TOKEN_ERROR;
        return m_String;
    }

    if( !Sym )
    {
        ReportError( "Invalid terminator symbol" );
        return m_String;
    }

    // Delimited text uses dynamic storage, like quoted strings.
    m_QuotedString.Clear();

    while( m_FilePos < m_FileSize )
    {
        char Character = CHAR( m_FilePos );
        m_FilePos++;

        if( Character == Sym )
        {
            if( Character == '\n' || (Character == '\r' && CHAR(m_FilePos) != '\n') )
            {
                m_LineNumber++;
            }

            m_type = TOKEN_STRING;
            return String();
        }

        if( !Character )
        {
            ReportError( "Null character before symbol" );
            return m_String;
        }

        if( Character == '\n' || (Character == '\r' && CHAR(m_FilePos) != '\n') )
        {
            m_LineNumber++;
        }

        m_QuotedString += Character;
    }

    ReportError( "Symbol not found before end of file" );
    return m_String;
}

//==============================================================================

void token_stream::OpenText( const char* pTextString, const char* pFileName )
{
    ASSERT( pTextString );
    CloseFile();
    Rewind();
    // Clear class
    m_FileSize   = 0;
    m_FilePos    = 0;
    m_LineNumber = 1;
    m_type = TOKEN_NONE;

    x_strncpy( m_Filename, pFileName, sizeof(m_Filename)-1 );
    m_Filename[sizeof(m_Filename)-1] = 0;

    m_bBuffered     = FALSE;
    m_FileBuffer    = (char*)x_malloc( x_strlen( pTextString ) + 1 );

    if( !m_FileBuffer )
    {
        ReportError( "Unable to allocate text buffer" );
        return;
    }

    x_strcpy( m_FileBuffer, pTextString );

    // Find how large the file is
    m_FileSize = x_strlen(pTextString);

    Rewind();

    m_StartPosition = 0;
  
}

//==============================================================================

void token_stream::CloseText( void )
{
    ASSERT( m_bBuffered == FALSE );
    ASSERT( m_pFile == NULL );
    ASSERT( m_CurBuffer == NULL );

    CloseFile();
}
