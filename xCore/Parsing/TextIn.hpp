//=========================================================================
//
//  TextIn.hpp
//
//=========================================================================

#ifndef TEXT_IN_HPP
#define TEXT_IN_HPP

//=========================================================================
// This file reads text file descrive as falls:
//
// [ HeaderName : HeaderCount ]
// { FieldName:ddd  FieldName:fff FieldName:sss FieldName:fds }
//      1 2 3       1.0 2.0 3.0   "1" "2" "3"    1.0 2 "3"
// ...
//
// or also can look like this
//
// [ HeaderName ]
// { FieldName:ddd  FieldName:fff FieldName:sss FieldName:fds }
//      1 2 3       1.0 2.0 3.0   "1" "2" "3"    1.0 2 "3"
// ...
//
//=========================================================================

//=========================================================================
// INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "x_color.hpp"
#include "x_plus.hpp"
#include "tokenizer.hpp"

//=========================================================================
// DEFINES
//=========================================================================

#define TEXTFILE_MAX_FIELDS      32     // a:f b:f c:f d:f e:f f:f g:f h:f ...
#define TEXTFILE_MAX_TYPES       16     // MaxTypes:ffffffffffffffff
#define TEXTFILE_MAX_STRLENGTH   256    // "dssdfsdfsdsdfdsfsdfsfd ..."
#define TEXTFILE_MAX_FLOATS      16
#define TEXTFILE_MAX_INTS        16
#define TEXTFILE_MAX_STRINGS     4
#define TEXTFILE_MAX_GUIDS       1

//=========================================================================
// CLASSES
//=========================================================================

class text_in
{
public:

                            text_in             ( void );
                           ~text_in             ( void ); // do not virtualise

    xbool                   OpenFile            ( const char* pFileName );
    void                    OpenText            ( const char* pText, const char* pFileName = "<internal string>" );
    void                    CloseFile           ( void );
    void                    SetStringMode       ( xbool Escaping, xbool Multiline );

    xbool                   ReadHeader          ( void );
    xbool                   SkipToNextHeader    ( void );

    xbool                   ReadFields          ( void );
    xbool                   GetField            ( const char* FieldName, ... );
    xbool                   GetVector3          ( const char* pName, vector3& V );
    xbool                   GetColor            ( const char* pName, xcolor& C );
    xbool                   GetF32              ( const char* pName, f32& F );
    xbool                   GetS32              ( const char* pName, s32& I );
    xbool                   GetString           ( const char* pName, char* pStr );
    xbool                   GetString           ( const char* pName, xstring& String );
    xbool                   HasError            ( void ) const;
    s32                     GetLineNumber       ( void ) const;
    xbool                   GetBBox             ( const char* pName, bbox& BBox );
    xbool                   GetRadian3          ( const char* pName, radian3& Orient );
    xbool                   GetQuaternion       ( const char* pName, quaternion& Q );
    xbool                   GetBool             ( const char* pName, xbool& Bool );
    xbool                   GetGuid             ( const char* pName, guid& Guid );

    const char*             GetHeaderName       ( void ) const;
    s32                     GetHeaderCount      ( void ) const;
    const char*             GetError            ( void ) const;
    xbool                   IsEOF               ( void ) const;
    const char*             GetFileName         ( void );

    s32                     GetFieldCount       ( void ) const;
    const char*             GetFieldName        ( s32 Index ) const;
    s32                     GetFieldTypeCount   ( s32 Index ) const;
    void                    GetFieldTypeStr     ( s32 nField, char* pString );
    xbool                   GetFieldAsString    ( const char* FieldName, char** pOutStr );

//=========================================================================
// END PUBLIC
//=========================================================================

protected:

    enum type
    {
        TYPE_NULL,
        TYPE_FLOAT,
        TYPE_INTEGER,
        TYPE_STRING,
        TYPE_GUID
    };

    //----------------------------------------------------------------------
    // STRUCTURES
    //----------------------------------------------------------------------

    struct data
    {
        s32 nFloats;
        s32 nIntegers;
        s32 nStrings;
        s32 nGuids;

        f32     Float[TEXTFILE_MAX_FLOATS];
        s32     Integer[TEXTFILE_MAX_INTS];
        xstring String[TEXTFILE_MAX_STRINGS];
        guid    Guid[TEXTFILE_MAX_GUIDS];
    };

    //----------------------------------------------------------------------

    struct field
    {
        char Name[TEXTFILE_MAX_STRLENGTH];
        s32  nTypes;
        type Type[TEXTFILE_MAX_TYPES];
    };

    //----------------------------------------------------------------------

    struct record
    {
        char Name[TEXTFILE_MAX_STRLENGTH];
        s32  Count;

        s32    nFields;
        s32    nFieldsAllocated;
        field* Field;
    };

protected:

    xbool                   ReadAllFields       ( void );
    xbool                   ReportError         ( const char* pStr, ... );
    s32                     FindField           ( const char* pName, s32 Length );
    xbool                   GetFieldValues      ( const char* pName, const char* pTypes, ... );
    xbool                   GetFieldValuesV     ( const char* pName, s32 NameLength,
                                                  const char* pTypes, x_va_list Args );

protected:

    record                  m_Record;
    token_stream            m_Tokenizer;
    s32                     m_RecordLineNumber;

    data*                   m_pData;
    s32                     m_nDataEntriesAllocated;
    xstring                 m_Error;
};

//=========================================================================
// INLINE FUNCTIONS
//=========================================================================

inline const char* text_in::GetHeaderName( void ) const
{
    return m_Record.Name;
}

//=========================================================================

inline s32 text_in::GetHeaderCount( void ) const
{
    return m_Record.Count;
}

//=========================================================================

inline const char* text_in::GetError( void ) const
{
    return m_Error.IsEmpty() ? m_Tokenizer.GetError() : (const char*)m_Error;
}

//=========================================================================

inline xbool text_in::IsEOF( void ) const
{
    return m_Tokenizer.IsEOF();
}

//=========================================================================

inline const char* text_in::GetFileName( void )
{
    return m_Tokenizer.GetFilename();
}

//=========================================================================

inline s32 text_in::GetFieldCount( void ) const
{
    return m_Record.nFields;
}

//=========================================================================

inline const char* text_in::GetFieldName( s32 Index ) const
{
    ASSERT( Index >= 0 && Index < m_Record.nFields );
    return m_Record.Field[Index].Name;
}

//=========================================================================

inline s32 text_in::GetFieldTypeCount( s32 Index ) const
{
    ASSERT( Index >= 0 && Index < m_Record.nFields );
    return m_Record.Field[Index].nTypes;
}

//=========================================================================
#endif
//=========================================================================
