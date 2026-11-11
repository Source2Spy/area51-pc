//==============================================================================
//  
//  tokenizer.hpp
//
//==============================================================================

#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_math.hpp"
#include "x_types.hpp"
#include "x_stdio.hpp"
#include "x_string.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define TOKEN_STRING_SIZE 1024
#define TOKEN_DELIMITER_STR ",{}()<>;"

//==============================================================================
//  CLASSES
//==============================================================================

class token_stream
{   
public:

    enum type
    {
        TOKEN_NONE,
        TOKEN_NUMBER,
        TOKEN_DELIMITER,
        TOKEN_SYMBOL,
        TOKEN_STRING,
        TOKEN_ERROR,
        TOKEN_EOF = 0x7FFFFFFF,
    };

public:

                        token_stream        ( void );
                       ~token_stream        ( void );

    // Start
    xbool               OpenFile            ( const char* pFileName );
    xbool               OpenFile            ( X_FILE* fp );
    void                OpenText            ( const char* pText, const char* pFileName = "<internal string>" );
    void                CloseText           ( void );
    xbool               IsEOF               ( void ) const;

    void                CloseFile           ( void );
    char*               GetDelimeter        ( void );
    void                SetDelimeter        ( const char* pStr );
    void                SetStringMode       ( xbool Escaping, xbool Multiline );
    const char*         GetError            ( void ) const;

    // Move through tokens in file
    void                Rewind              ( void );
    type                Read                ( s32 NTokens=1 );
    f32                 ReadFloat           ( void );
    s32                 ReadInt             ( void );
    s32                 ReadHex             ( void );
    char*               ReadSymbol          ( void );
    char*               ReadString          ( void );
    char*               ReadLine            ( void );
    char*               ReadToSymbol        ( char Sym );
    f32                 ReadF32FromString   ( void );
    s32                 ReadS32FromString   ( void );
    xbool               ReadBoolFromString  ( void );

    void                SkipToNextLine      ( void );

    // Complex requests
    xbool               Find                ( const char* TokenStr, xbool FromBeginning=FALSE );
    s32                 GetCursor           ( void );
    void                SetCursor           ( s32 Pos );
    s32                 GetLineNumber       ( void ) const;
    char*               GetFilename         ( void );

    // Interrogate about current token
    type                Type                ( void );
    f32                 Float               ( void );
    s32                 Int                 ( void );
    char                Delimiter           ( void );
    char*               String              ( void );
    xbool               IsFloat             ( void );

protected:

    void                SkipWhitespace      ( void );
    xbool               PrepareRead         ( void );
    type                ReportError         ( const char* pMessage );
    type                ReadNumber          ( void );
    type                ConvertNumber       ( const char* pText, xbool IntegerOnly );
    type                ReadRawSymbol       ( void );
    type                ReadQuotedString    ( void );
    char                CHAR                ( s32 FilePos );

protected:

    s32                 m_FileSize;
    char*               m_FileBuffer;
    s32                 m_FilePos;
    s32                 m_LineNumber;

    char                m_Filename[64];
    char                m_DelimiterStr[16];
    byte                m_IsCharNumber[256];

    type                m_type;
    char                m_String[TOKEN_STRING_SIZE];
    xstring             m_QuotedString;
    xstring             m_Error;
    xbool               m_StringEscaping;
    xbool               m_MultilineStrings;
    f32                 m_Float;
    s32                 m_Int;
    xbool               m_IsFloat;
    char                m_Delimiter;

    xbool               m_bBuffered;
    s32                 m_CurBufferStart;
    s32                 m_CurBufferEnd;
    char*               m_CurBuffer;
    s32                 m_StartPosition;
    X_FILE*             m_pFile;

};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline char* token_stream::String( void )
{
    return m_type == TOKEN_STRING ? &m_QuotedString[0] : m_String;
}

//==============================================================================

inline s32 token_stream::GetLineNumber( void ) const
{
    return m_LineNumber;
}

//==============================================================================

inline const char* token_stream::GetError( void ) const
{
    return m_Error;
}

//==============================================================================

inline char* token_stream::GetFilename( void )
{
    return m_Filename;
}

//==============================================================================

inline token_stream::type token_stream::Type( void )
{
    return m_type;
}

//==============================================================================

inline f32 token_stream::Float( void )
{
    return m_Float;
}

//==============================================================================

inline s32 token_stream::Int( void )
{
    return m_Int;
}

//==============================================================================

inline char token_stream::Delimiter( void )
{
    return m_Delimiter;
}

//==============================================================================

inline xbool token_stream::IsFloat( void )
{
    return m_IsFloat;
}

//==============================================================================
#endif
//==============================================================================
