//=========================================================================
//
//  PropertyTree.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PropertyTree.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

//=========================================================================
//  INTERNAL
//=========================================================================

namespace resource_tool::property_tree
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool ReadContentLine( std::ifstream& Input, std::string& Line )
{
    while( std::getline( Input, Line ) )
    {
        const usize First = Line.find_first_not_of( " \t\r" );
        if( First == std::string::npos || Line.compare( First, 2, "//" ) == 0 )
            continue;
        return TRUE;
    }
    return FALSE;
}

//=========================================================================

xbool ReadQuoted( const std::string& Line, usize& Position, std::string& Value )
{
    while( Position < Line.size() && ( Line[Position] == ' ' || Line[Position] == '\t' ) )
    {
        ++Position;
    }

    if( Position >= Line.size() || Line[Position] != '"' )
        return FALSE;

    ++Position;
    Value.clear();
    while( Position < Line.size() )
    {
        const char Character = Line[Position++];
        if( Character == '"' )
            return TRUE;
        if( Character == '\\' && Position < Line.size() && Line[Position] == '"' )
        {
            Value.push_back( Line[Position++] );
        }
        else
            Value.push_back( Character );
    }
    return FALSE;
}

//=========================================================================

std::string Quote( const std::string& Value )
{
    std::string Result;
    Result.reserve( Value.size() + 2 );
    Result.push_back( '"' );
    for( const char Character : Value )
    {
        if( Character == '"' )
            Result.push_back( '\\' );
        Result.push_back( Character );
    }
    Result.push_back( '"' );
    return Result;
}

//=========================================================================

xbool ParseHeader( const std::string& Line, usize& Count )
{
    std::istringstream Header( Line );
    std::string Open;
    std::string Name;
    std::string Colon;
    std::string Close;
    long ParsedCount = 0;
    if( !( Header >> Open >> Name >> Colon >> ParsedCount >> Close ) || Open != "[" || Name != "Properties" ||
        Colon != ":" || Close != "]" || ParsedCount < 0 )
    {
        return FALSE;
    }

    Count = (usize)ParsedCount;
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

//=========================================================================
//  DOCUMENT
//=========================================================================
void document::Clear( void )
{
    m_Records.clear();
}

//=========================================================================

void document::Add( const std::string& Name, const std::string& Type, const std::string& Value )
{
    m_Records.push_back( { Name, Type, Value } );
}

//=========================================================================

const std::vector<property_record>& document::Records( void ) const
{
    return m_Records;
}

//=========================================================================
//  READER
//=========================================================================

xbool reader::Load( const std::filesystem::path& FileName, document& Document, std::string& Error ) const
{
    Document.Clear();

    std::ifstream Input( FileName );
    if( !Input )
    {
        Error = "Unable to open property document: " + FileName.generic_string();
        return FALSE;
    }

    std::string Line;
    usize Count = 0;
    if( !ReadContentLine( Input, Line ) || !ParseHeader( Line, Count ) )
    {
        Error = "Property document does not contain a valid Properties header";
        return FALSE;
    }

    if( !ReadContentLine( Input, Line ) || Line.find( "Name:s" ) == std::string::npos ||
        Line.find( "Type:s" ) == std::string::npos || Line.find( "Value:s" ) == std::string::npos )
    {
        Error = "Property document has no Name/Type/Value declaration";
        return FALSE;
    }

    for( usize Index = 0; Index < Count; ++Index )
    {
        if( !ReadContentLine( Input, Line ) )
        {
            Error = xfs( "Property document ended at record %d", Index );
            return FALSE;
        }

        usize Position = 0;
        property_record Record;
        if( !ReadQuoted( Line, Position, Record.Name ) || !ReadQuoted( Line, Position, Record.Type ) ||
            !ReadQuoted( Line, Position, Record.Value ) )
        {
            Error = xfs( "Property document has an invalid record %d", Index );
            return FALSE;
        }
        Document.Add( Record.Name, Record.Type, Record.Value );
    }

    return TRUE;
}

//=========================================================================
//  WRITER
//=========================================================================

xbool writer::Write( const std::filesystem::path& FileName, const document& Document, std::string& Error ) const
{
    std::ofstream Output( FileName, std::ios::out | std::ios::trunc );
    if( !Output )
    {
        Error = "Unable to write property document: " + FileName.generic_string();
        return FALSE;
    }

    const std::vector<property_record>& Records = Document.Records();
    Output << "[ Properties : " << Records.size() << " ]\n";
    Output << " { Name:s Type:s Value:s }\n";
    Output << "// --------------------------- ------------ ------------------------\n";
    for( const property_record& Record : Records )
    {
        Output << "   " << std::left << std::setw( 29 ) << Quote( Record.Name ) << std::setw( 13 )
               << Quote( Record.Type ) << Quote( Record.Value ) << "\n";
    }

    Output.flush();
    if( !Output )
    {
        Error = "Unable to write property document: " + FileName.generic_string();
        return FALSE;
    }
    return TRUE;
}

} // namespace resource_tool::property_tree
