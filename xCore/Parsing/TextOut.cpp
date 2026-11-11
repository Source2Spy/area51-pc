//=========================================================================
//
//  TextOut.cpp
//
//=========================================================================

//=========================================================================
// INCLUDES
//=========================================================================

#include "TextOut.hpp"
#include "x_debug.hpp"
#include "x_memory.hpp"
#include "x_string.hpp"

//=========================================================================
// IMPLEMENTATION
//=========================================================================

text_out::text_out( void )
{
    x_memset( this, 0, sizeof( *this ) );
}

//=========================================================================

text_out::~text_out( void )
{
    if( m_pBlock )
    {
        x_free( m_pBlock );
    }

    if( m_pTypeEntry )
    {
        x_free( m_pTypeEntry );
    }

    if( m_Fp )
    {
        x_fclose( m_Fp );
    }

    m_pBlock     = NULL;
    m_pTypeEntry = NULL;
    m_Fp         = NULL;
}

//=========================================================================

void text_out::CloseFile( void )
{
    this->~text_out();
}

//=========================================================================

void text_out::OpenFile( const char* pFileName )
{
    ASSERT( pFileName );
    m_Fp = x_fopen( pFileName, "wt" );
    if( m_Fp == NULL )
    {
        x_throw( xfs( "Unable to open %s For saving", pFileName ) );
    }
}

//=========================================================================

void text_out::AddHeader( const char* pHeaderName, s32 Count )
{
    //
    // Set up
    //
    m_LineCount     = ( Count < 0 ) ? 1 : Count;
    m_iLine         = 0;
    m_iTypeEntry    = 0;
    m_nTypesPerLine = 1024; // Bogus initial guess
    m_iFiled        = 0;
    m_nFields       = 0;
    m_iBlock        = 0;
    m_BlockSize     = m_nTypesPerLine * 1 * 256;
    m_pBlock        = (char*)x_malloc( m_BlockSize );
    m_pTypeEntry    = (type_entry*)x_malloc( sizeof( type_entry ) * m_nTypesPerLine * m_LineCount );
    ASSERT( m_pBlock && m_pTypeEntry );

    //
    // Save the block name
    //
    if( Count < 0 )
    {
        x_sprintf( m_BlockName, "[ %s ]\n", pHeaderName );
    }
    else
    {
        x_sprintf( m_BlockName, "[ %s : %d ]\n", pHeaderName, Count );
    }
    // If header has zero entries then just flush header
    if( Count == 0 )
    {
        AddEndLine();
    }
}

//=========================================================================

void text_out::AddField( const char* pName, ... )
{
    ASSERT( pName );

    const char* pTypes = x_strchr( pName, ':' );

    if( !pTypes )
    {
        x_throw( "Missing field type" );
        return;
    }

    x_va_list Args;
    x_va_start( Args, pName );
    AddFieldValuesV( pName, (s32)( pTypes - pName ), pTypes + 1, Args );
    x_va_end( Args );
}

//=========================================================================
// HELPER FUNCTIONS
//=========================================================================

void text_out::AddFieldValues( const char* pName, const char* pTypes, ... )
{
    x_va_list Args;
    x_va_start( Args, pTypes );
    AddFieldValuesV( pName, x_strlen( pName ), pTypes, Args );
    x_va_end( Args );
}

//=========================================================================

void text_out::AddFieldValuesV( const char* pName, s32 NameLength,
                                const char* pTypes, x_va_list Args )
{
    s32 i;

    ASSERT( pName );
    ASSERT( pTypes );
    ASSERT( m_iLine < m_LineCount );

    s32 TypeCount = x_strlen( pTypes );

    if( NameLength <= 0 || TypeCount <= 0 || TypeCount > 64 ||
        NameLength + TypeCount + 1 >= sizeof( m_Field[0].Name ) )
    {
        x_throw( "Invalid field name or type count" );
        return;
    }

    // Validate names and types before registering or writing the field.
    for( i = 0; i < NameLength; i++ )
    {
        if( (u8)pName[i] <= 32 || pName[i] == ':' )
        {
            x_throw( "Invalid field name" );
            return;
        }
    }

    for( i = 0; i < TypeCount; i++ )
    {
        char Type = x_tolower( pTypes[i] );

        if( Type != 'f' && Type != 'd' && Type != 's' && Type != 'g' )
        {
            x_throw( "Invalid field type" );
            return;
        }
    }

    if( m_iLine == 0 )
    {
        if( m_nFields >= 256 )
        {
            x_throw( "Too many fields" );
            return;
        }

        // Store the name and types once, when registering the first row.
        x_memset( &m_Field[m_nFields], 0, sizeof( field ) );
        x_memcpy( m_Field[m_nFields].Name, pName, NameLength );
        m_Field[m_nFields].Name[NameLength] = 0;

        for( i = 0; i < m_nFields; i++ )
        {
            if( x_stricmp( m_Field[m_nFields].Name, m_Field[i].Name ) == 0 )
            {
                x_throw( "Duplicate field name" );
                return;
            }
        }

        for( i = 0; i < TypeCount; i++ )
        {
            m_Field[m_nFields].Type[i] = x_tolower( pTypes[i] );
        }

        m_Field[m_nFields].nTypes = TypeCount;
        m_nFields++;
    }

    if( m_iFiled >= m_nFields )
    {
        x_throw( "Too many fields in row" );
        return;
    }

    field& Field = m_Field[m_iFiled];

    if( x_strlen( Field.Name ) != NameLength || x_strncmp( Field.Name, pName, NameLength ) ||
        x_stricmp( Field.Type, pTypes ) )
    {
        x_throw( "Field does not match its first row" );
        return;
    }

    m_iFiled++;

    // Write values using the registered type description.
    for( i = 0; i < Field.nTypes; i++ )
    {
        switch( Field.Type[i] )
        {
            case 'f':
            {
                double p = ( x_va_arg( Args, double ) ); // get the type
            
                if( p < 0 )
                {
                    Field.HasNegative[i] = true;
                }
            
                m_pTypeEntry[m_iTypeEntry].Length = x_sprintf( &m_pBlock[m_iBlock], "%f", p );
                m_pTypeEntry[m_iTypeEntry].bDigit = true;
            
                break;
            }
        
            case 'd':
            {
                s32 p = ( x_va_arg( Args, s32 ) ); // get the type
            
                if( p < 0 )
                {
                    Field.HasNegative[i] = true;
                }
            
                m_pTypeEntry[m_iTypeEntry].Length = x_sprintf( &m_pBlock[m_iBlock], "%d", p );
                m_pTypeEntry[m_iTypeEntry].bDigit = true;
            
                break;
            }
        
            case 's':
            {
                const char* p = ( x_va_arg( Args, const char* ) ); // get the type
            
                m_pTypeEntry[m_iTypeEntry].Length = x_sprintf( &m_pBlock[m_iBlock], "\"%s\"", p );
                m_pTypeEntry[m_iTypeEntry].bDigit = false;
            
                break;
            }
        
            case 'g':
            {
                u64 g = ( x_va_arg( Args, u64 ) );
            
                m_pTypeEntry[m_iTypeEntry].Length = x_sprintf( &m_pBlock[m_iBlock], "\"%08X:%08X\"", (u32)( ( g >> 32 ) & 0xFFFFFFFF ), (u32)( ( g >> 0 ) & 0xFFFFFFFF ) );
                m_pTypeEntry[m_iTypeEntry].bDigit = true;
            
                break;
            }
        
            default:
            {
                ASSERT( 0 && "Wrong type" );
            }
        }

        //
        // Fill up the rest of the properties
        //

        // Record the rest of the info
        m_pTypeEntry[m_iTypeEntry].iOffset     = m_iBlock;
        m_pTypeEntry[m_iTypeEntry].iField      = m_iFiled - 1;
        m_pTypeEntry[m_iTypeEntry].iType       = i;
        m_pTypeEntry[m_iTypeEntry].iBackOffset = ( ( i + 1 ) == Field.nTypes ) ? 0 : 1;

        // Track the total spaces
        s32 TS = m_pTypeEntry[m_iTypeEntry].Length +
                 m_pTypeEntry[m_iTypeEntry].iBackOffset +
                 ( m_pTypeEntry[m_iTypeEntry].bDigit && m_pBlock[m_iBlock] != '-' );

        if( Field.TotalSpace[i] < TS )
        {
            Field.TotalSpace[i] = TS;
        }

        // move the block
        m_iBlock += m_pTypeEntry[m_iTypeEntry].Length;

        // move the entry
        m_iTypeEntry++;

        ASSERT( m_iBlock <= m_BlockSize );
        ASSERT( m_iTypeEntry <= m_nTypesPerLine * m_LineCount );
    }
}

//=========================================================================
// IMPLEMENTATION
//=========================================================================

void text_out::AddEndLine( void )
{
    // Increment the line count
    m_iLine++;
    m_iFiled = 0;

    //
    // dump the hold thing to file if we are done
    //
    if( ( m_LineCount == 0 ) || ( m_iLine == m_LineCount ) )
    {
        static const char SpaceArray[] =
            { "                                                                                                                                                                                 " };
        char BlockInfo[6000] = { 0 };
        char OutLine[6000]   = { 0 };

        // Prs32 the very to header
        x_fprintf( m_Fp, "%s", m_BlockName );

        // Get how big each block is going to be
        s32 j;

        for( j = 0; j < m_nFields; j++ )
        {
            field& Field = m_Field[j];

            Field.TotalSpaceBlock = 0;
            for( s32 k = 0; k < Field.nTypes; k++ )
            {
                Field.TotalSpaceBlock += Field.TotalSpace[k];
            }

            // Add one space to separe the fields
            m_Field[j].TotalSpaceBlock += 3;

            // The minimun size is the size of the string descriving the block
            s32 l1 = x_strlen( Field.Name ) + 1 + Field.nTypes + 1;
            if( Field.TotalSpaceBlock < l1 )
            {
                Field.TotalSpaceBlock = l1;
            }

            // Copy the block infd stuff
            s32 l2 = x_strlen( BlockInfo );
            s32 l3 = x_sprintf( &BlockInfo[l2], "%s:%s", Field.Name, Field.Type );
            s32 l4 = Field.TotalSpaceBlock - l3;
            s32 m;

            for( m = 0; m < l4; m++ )
            {
                ASSERT( l2 + l3 + m < 6000 );
                BlockInfo[l2 + l3 + m] = ' ';
            }

            // End string for now
            BlockInfo[l2 + l3 + m] = 0;
        }

        // Create the outline
        {
            s32 l1 = x_strlen( BlockInfo );
            s32 bO = 0;
            for( j = 0; j < l1; j++ )
            {
                OutLine[j] = '-';

                if( bO == 0 && BlockInfo[j + 1] == ' ' )
                {
                    bO = 1;
                }
                if( bO == 1 && BlockInfo[j + 1] != ' ' )
                {
                    bO = 2;
                }

                if( bO == 2 && BlockInfo[j + 1] != 0 )
                {
                    OutLine[j] = ' ';
                    bO         = 0;
                }
            }

            OutLine[j] = 0;
        }

        x_fprintf( m_Fp, " { %s }\n", BlockInfo );
        x_fprintf( m_Fp, "// %s\n   ", OutLine );

        s32 iTypeEntry = 0;
        for( s32 i = 0; i < m_LineCount; i++ )
        {
            for( s32 j = 0; j < m_nFields; j++ )
            {
                const field& Field      = m_Field[j];
                s32          FieldTotal = 0;

                for( s32 k = 0; k < Field.nTypes; k++, iTypeEntry++ )
                {
                    const type_entry& Entry = m_pTypeEntry[iTypeEntry];
                    // UNUSED(SH): s32                iField   = Entry.iField;
                    s32 iType = Entry.iType;
                    s32 Count = 0;
                    s32 TotalSpace;

                    // Write spaces at front
                    if( Entry.bDigit &&
                        Field.HasNegative[k] &&
                        m_pBlock[Entry.iOffset] != '-' )
                    {
                        Count += x_fwrite( SpaceArray, 1, 1, m_Fp );
                    }

                    // Write field in file
                    Count += x_fwrite( &m_pBlock[Entry.iOffset], 1, Entry.Length, m_Fp );

                    // Set the spaces to separeate the filed blocks
                    TotalSpace = Field.TotalSpace[iType];

                    // Write spaces for the field
                    if( Count < TotalSpace )
                    {
                        Count += x_fwrite( SpaceArray, 1, TotalSpace - Count, m_Fp );
                    }

                    // Set the total so far
                    FieldTotal += Count;
                }

                ASSERT( FieldTotal <= Field.TotalSpaceBlock );

                // Make sure that we have all the requiere spaces
                if( FieldTotal < Field.TotalSpaceBlock )
                {
                    x_fwrite( SpaceArray, 1, Field.TotalSpaceBlock - FieldTotal, m_Fp );
                }
            }

            // Add from time to time a info block
            if( ( ( i + 1 ) % 80 ) == 0 && ( m_LineCount - i ) > 10 )
            {
                x_fprintf( m_Fp, "\n" );
                x_fprintf( m_Fp, "// %s\n", OutLine );
                x_fprintf( m_Fp, "// %s\n", BlockInfo );
                x_fprintf( m_Fp, "// %s\n   ", OutLine );
            }
            else
            {
                x_fprintf( m_Fp, "\n   " );
            }
        }

        // done here
        x_fprintf( m_Fp, "\n" );

        x_free( m_pBlock );
        x_free( m_pTypeEntry );
        m_pBlock     = NULL;
        m_pTypeEntry = NULL;
        // done
        return;
    }

    //
    // Okay now we should have a pretty good idea on how many types per line
    // are gos32 to be we can allocate the right amount
    //

    if( m_iLine == 1 )
    {
        m_nTypesPerLine = m_iTypeEntry;
        m_pTypeEntry    = (type_entry*)x_realloc( m_pTypeEntry, m_nTypesPerLine * m_LineCount * sizeof( type_entry ) );
        m_BlockSize     = m_nTypesPerLine * m_LineCount * 256;
        m_pBlock        = (char*)x_realloc( m_pBlock, m_BlockSize );
        ASSERT( m_pBlock && m_pTypeEntry );
    }
}

//=========================================================================
// IMPLEMENTATION
//=========================================================================

void text_out::AddVector3( const char* pName, const vector3& V )
{
    AddFieldValues( pName, "fff", V.GetX(), V.GetY(), V.GetZ() );
}

//=========================================================================

void text_out::AddColor( const char* pName, xcolor C )
{
    AddFieldValues( pName, "dddd", (s32)C.R, (s32)C.G, (s32)C.B, (s32)C.A );
}

//=========================================================================

void text_out::AddF32( const char* pName, f32 F )
{
    AddFieldValues( pName, "f", F );
}

//=========================================================================

void text_out::AddS32( const char* pName, s32 I )
{
    AddFieldValues( pName, "d", I );
}

//=========================================================================

void text_out::AddString( const char* pName, const char* pStr )
{
    AddFieldValues( pName, "s", pStr );
}

//=========================================================================

void text_out::AddBBox( const char* pName, const bbox& BBox )
{
    AddFieldValues( pName, "ffffff",
                    BBox.Min.GetX(), BBox.Min.GetY(), BBox.Min.GetZ(),
                    BBox.Max.GetX(), BBox.Max.GetY(), BBox.Max.GetZ() );
}

//=========================================================================

void text_out::AddRadian3( const char* pName, const radian3& Orient )
{
    f32 P = RAD_TO_DEG( Orient.Pitch );
    f32 Y = RAD_TO_DEG( Orient.Yaw );
    f32 R = RAD_TO_DEG( Orient.Roll );
    AddFieldValues( pName, "fff", P, Y, R );
}

//=========================================================================

void text_out::AddQuaternion( const char* pName, const quaternion& Q )
{
    AddFieldValues( pName, "ffff", Q.X, Q.Y, Q.Z, Q.W );
}

//=========================================================================

void text_out::AddBool( const char* pName, xbool Bool )
{
    AddFieldValues( pName, "d", Bool ? 1 : 0 );
}

//=========================================================================

void text_out::AddGuid( const char* pName, guid Guid )
{
    AddFieldValues( pName, "g", (u64)Guid );
}
