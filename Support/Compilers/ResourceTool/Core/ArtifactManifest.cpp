//=========================================================================
//
//  ArtifactManifest.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ArtifactManifest.hpp"
#include "DependencyGraph.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void artifact_manifest::Add( artifact_kind Kind, const char* pRole, const std::filesystem::path& Path )
{
    artifact Entry;
    Entry.Kind = Kind;
    Entry.Role = pRole ? pRole : "";
    Entry.Path = Path.lexically_normal();
    m_Entries.Append( Entry );
}

//=========================================================================

const xarray<artifact>& artifact_manifest::Entries( void ) const
{
    return m_Entries;
}

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

//=========================================================================

const char* ArtifactKindName( artifact_kind Kind )
{
    return Kind == artifact_kind::Main ? "main" : "auxiliary";
}

//=========================================================================

const char* DependencyKindName( dependency_kind Kind )
{
    switch( Kind )
    {
    case dependency_kind::Source:
        return "source";
    case dependency_kind::Pipeline:
        return "pipeline";
    case dependency_kind::Product:
        return "product";
    default:
        return "unknown";
    }
}

//=========================================================================

xstring EscapeJson( const xstring& Value )
{
    std::ostringstream Result;
    for( s32 iCharacter = 0; iCharacter < Value.GetLength(); ++iCharacter )
    {
        const u8 Character = (u8)Value[iCharacter];
        switch( Character )
        {
        case '\\':
            Result << "\\\\";
            break;
        case '"':
            Result << "\\\"";
            break;
        case '\n':
            Result << "\\n";
            break;
        case '\r':
            Result << "\\r";
            break;
        case '\t':
            Result << "\\t";
            break;
        default:
            if( Character < 0x20 )
                Result << "\\u" << std::hex << std::setw( 4 ) << std::setfill( '0' ) << (s32)Character << std::dec
                       << std::setfill( ' ' );
            else
                Result << Character;
            break;
        }
    }
    return xstring( Result.str().c_str() );
}

class manifest_json_reader
{
public:
    explicit manifest_json_reader( const std::string& Text ) : m_Text( Text ) {}

    xbool Parse( artifact_manifest_metadata& Metadata, std::string& Error )
    {
        SkipWhitespace();
        if( !Consume( '{' ) )
            return Fail( Error, "manifest root must be a JSON object" );

        xbool bFirst = TRUE;
        while( TRUE )
        {
            SkipWhitespace();
            if( Consume( '}' ) )
                break;
            if( !bFirst && !Consume( ',' ) )
                return Fail( Error, "expected a comma between manifest fields" );
            bFirst = FALSE;

            xstring Key;
            if( !ParseString( Key, Error ) )
                return FALSE;
            if( !Consume( ':' ) )
                return Fail( Error, "expected ':' after manifest field name" );

            if( Key == "schema" )
            {
                if( !ParseInteger( Metadata.SchemaVersion, Error ) )
                    return FALSE;
            }
            else if( Key == "tool" )
            {
                if( !ParseString( Metadata.ToolFingerprint, Error ) )
                    return FALSE;
            }
            else if( Key == "inputFingerprint" )
            {
                if( !ParseString( Metadata.InputFingerprint, Error ) )
                    return FALSE;
            }
            else if( Key == "artifacts" )
            {
                if( !ParseArtifacts( Metadata.Artifacts, Error ) )
                    return FALSE;
            }
            else if( !SkipValue( Error ) )
            {
                return FALSE;
            }
        }

        SkipWhitespace();
        if( m_Position != m_Text.size() )
            return Fail( Error, "trailing data after manifest JSON" );
        if( Metadata.SchemaVersion < 0 )
            return Fail( Error, "manifest does not contain a schema" );
        if( Metadata.ToolFingerprint.IsEmpty() )
            return Fail( Error, "manifest does not contain a tool fingerprint" );
        if( Metadata.InputFingerprint.IsEmpty() )
            return Fail( Error, "manifest does not contain an input fingerprint" );
        return TRUE;
    }

private:
    xbool Fail( std::string& Error, const char* pMessage ) const
    {
        Error = pMessage;
        return FALSE;
    }

    void SkipWhitespace( void )
    {
        while( m_Position < m_Text.size() )
        {
            const char Character = m_Text[m_Position];
            if( Character != ' ' && Character != '\n' && Character != '\r' && Character != '\t' )
                break;
            ++m_Position;
        }
    }

    xbool Consume( char Expected )
    {
        SkipWhitespace();
        if( m_Position >= m_Text.size() || m_Text[m_Position] != Expected )
            return FALSE;
        ++m_Position;
        return TRUE;
    }

    xbool ParseString( xstring& Value, std::string& Error )
    {
        SkipWhitespace();
        if( m_Position >= m_Text.size() || m_Text[m_Position] != '"' )
            return Fail( Error, "expected a JSON string" );
        ++m_Position;
        Value.Clear();
        while( m_Position < m_Text.size() )
        {
            const char Character = m_Text[m_Position++];
            if( Character == '"' )
                return TRUE;
            if( Character != '\\' )
            {
                Value += Character;
                continue;
            }
            if( m_Position >= m_Text.size() )
                return Fail( Error, "unterminated JSON escape" );
            const char Escape = m_Text[m_Position++];
            switch( Escape )
            {
            case '"':
                Value += '"';
                break;
            case '\\':
                Value += '\\';
                break;
            case '/':
                Value += '/';
                break;
            case 'b':
                Value += '\b';
                break;
            case 'f':
                Value += '\f';
                break;
            case 'n':
                Value += '\n';
                break;
            case 'r':
                Value += '\r';
                break;
            case 't':
                Value += '\t';
                break;
            case 'u':
                // Reject escaped paths and fingerprints; artifacts use UTF-8.
                return Fail( Error, "unicode JSON escapes are unsupported" );
            default:
                return Fail( Error, "invalid JSON escape in manifest" );
            }
        }
        return Fail( Error, "unterminated JSON string" );
    }

    xbool ParseInteger( s32& Value, std::string& Error )
    {
        SkipWhitespace();
        const usize Begin = m_Position;
        if( m_Position < m_Text.size() && m_Text[m_Position] == '-' )
            ++m_Position;
        const usize Digits = m_Position;
        while( m_Position < m_Text.size() && m_Text[m_Position] >= '0' && m_Text[m_Position] <= '9' )
            ++m_Position;
        if( m_Position == Digits )
            return Fail( Error, "expected an integer in manifest" );
        try
        {
            Value = std::stoi( m_Text.substr( Begin, m_Position - Begin ) );
        }
        catch( ... )
        {
            return Fail( Error, "invalid integer in manifest" );
        }
        return TRUE;
    }

    xbool SkipValue( std::string& Error )
    {
        SkipWhitespace();
        if( m_Position >= m_Text.size() )
            return Fail( Error, "missing JSON value in manifest" );

        if( m_Text[m_Position] == '"' )
        {
            xstring Ignored;
            return ParseString( Ignored, Error );
        }
        if( m_Text[m_Position] == '{' )
        {
            ++m_Position;
            xbool bFirst = TRUE;
            while( TRUE )
            {
                SkipWhitespace();
                if( Consume( '}' ) )
                    return TRUE;
                if( !bFirst && !Consume( ',' ) )
                    return Fail( Error, "invalid JSON object in manifest" );
                bFirst = FALSE;
                xstring IgnoredKey;
                if( !ParseString( IgnoredKey, Error ) || !Consume( ':' ) || !SkipValue( Error ) )
                    return FALSE;
            }
        }
        if( m_Text[m_Position] == '[' )
        {
            ++m_Position;
            xbool bFirst = TRUE;
            while( TRUE )
            {
                SkipWhitespace();
                if( Consume( ']' ) )
                    return TRUE;
                if( !bFirst && !Consume( ',' ) )
                    return Fail( Error, "invalid JSON array in manifest" );
                bFirst = FALSE;
                if( !SkipValue( Error ) )
                    return FALSE;
            }
        }

        const usize Begin = m_Position;
        while( m_Position < m_Text.size() && m_Text[m_Position] != ',' && m_Text[m_Position] != ']' &&
               m_Text[m_Position] != '}' && m_Text[m_Position] != ' ' && m_Text[m_Position] != '\n' &&
               m_Text[m_Position] != '\r' && m_Text[m_Position] != '\t' )
            ++m_Position;
        if( Begin == m_Position )
            return Fail( Error, "invalid JSON value in manifest" );
        return TRUE;
    }

    xbool ParseArtifacts( xarray<artifact>& Artifacts, std::string& Error )
    {
        if( !Consume( '[' ) )
            return Fail( Error, "manifest artifacts must be an array" );
        xbool bFirst = TRUE;
        while( TRUE )
        {
            SkipWhitespace();
            if( Consume( ']' ) )
                return TRUE;
            if( !bFirst && !Consume( ',' ) )
                return Fail( Error, "invalid manifest artifacts array" );
            bFirst = FALSE;
            if( !Consume( '{' ) )
                return Fail( Error, "manifest artifact must be an object" );

            artifact Entry;
            xbool bHasKind = FALSE;
            xbool bHasRole = FALSE;
            xbool bHasPath = FALSE;
            xbool bObjectFirst = TRUE;
            while( TRUE )
            {
                SkipWhitespace();
                if( Consume( '}' ) )
                    break;
                if( !bObjectFirst && !Consume( ',' ) )
                    return Fail( Error, "invalid manifest artifact object" );
                bObjectFirst = FALSE;

                xstring Key;
                if( !ParseString( Key, Error ) || !Consume( ':' ) )
                    return FALSE;
                if( Key == "kind" )
                {
                    xstring Kind;
                    if( !ParseString( Kind, Error ) )
                        return FALSE;
                    if( Kind == "main" )
                        Entry.Kind = artifact_kind::Main;
                    else if( Kind == "auxiliary" )
                        Entry.Kind = artifact_kind::Auxiliary;
                    else
                        return Fail( Error, "unknown artifact kind in manifest" );
                    bHasKind = TRUE;
                }
                else if( Key == "role" )
                {
                    if( !ParseString( Entry.Role, Error ) )
                        return FALSE;
                    bHasRole = TRUE;
                }
                else if( Key == "path" )
                {
                    xstring Path;
                    if( !ParseString( Path, Error ) )
                        return FALSE;
                    Entry.Path = (const char*)Path;
                    bHasPath = TRUE;
                }
                else if( !SkipValue( Error ) )
                {
                    return FALSE;
                }
            }
            if( !bHasKind || !bHasRole || !bHasPath )
                return Fail( Error, "manifest artifact is missing a field" );
            Artifacts.Append( Entry );
        }
    }

    const std::string& m_Text;
    usize m_Position = 0;
};

//=========================================================================

xbool ArtifactLess( const artifact& A, const artifact& B )
{
    if( A.Kind != B.Kind )
        return A.Kind < B.Kind;
    if( A.Role != B.Role )
        return A.Role < B.Role;
    return A.Path.generic_string() < B.Path.generic_string();
}

//=========================================================================

xarray<artifact> SortedArtifacts( const xarray<artifact>& Entries )
{
    xarray<artifact> Sorted = Entries;
    for( s32 i = 1; i < Sorted.GetCount(); ++i )
    {
        artifact Value = Sorted[i];
        s32 j = i;
        while( j > 0 && ArtifactLess( Value, Sorted[j - 1] ) )
        {
            Sorted[j] = Sorted[j - 1];
            --j;
        }
        Sorted[j] = Value;
    }
    return Sorted;
}

//=========================================================================

xbool SameArtifact( const artifact& A, const artifact& B )
{
    return A.Kind == B.Kind && A.Role == B.Role &&
           std::filesystem::absolute( A.Path ).lexically_normal() ==
               std::filesystem::absolute( B.Path ).lexically_normal();
}

} // namespace

//=========================================================================

xarray<dependency> SortedDependencies( const xarray<dependency>& Entries )
{
    xarray<dependency> Sorted = Entries;
    for( s32 i = 1; i < Sorted.GetCount(); ++i )
    {
        dependency Value = Sorted[i];
        s32 j = i;
        while( j > 0 )
        {
            const dependency& Previous = Sorted[j - 1];
            const xbool bComesBefore =
                Value.Kind < Previous.Kind ||
                ( Value.Kind == Previous.Kind && Value.Path.generic_string() < Previous.Path.generic_string() );
            if( !bComesBefore )
                break;
            Sorted[j] = Previous;
            --j;
        }
        Sorted[j] = Value;
    }
    return Sorted;
}

//=========================================================================

xstring CalculateInputFingerprint( const dependency_graph& Dependencies )
{
    const xarray<dependency> DependencyEntries = SortedDependencies( Dependencies.Entries() );

    u64 Hash = 1469598103934665603ull;
    auto Mix = [&Hash]( const void* pData, usize nBytes )
    {
        const u8* pBytes = static_cast<const u8*>( pData );
        for( usize i = 0; i < nBytes; ++i )
        {
            Hash ^= pBytes[i];
            Hash *= 1099511628211ull;
        }
    };
    auto MixString = [&Mix]( const std::string& Value )
    {
        Mix( Value.data(), Value.size() );
        const u8 Separator = 0;
        Mix( &Separator, sizeof( Separator ) );
    };

    std::array<u8, 64 * 1024> Buffer;
    for( s32 iDependency = 0; iDependency < DependencyEntries.GetCount(); ++iDependency )
    {
        const dependency& Dependency = DependencyEntries[iDependency];
        const s32 Kind = static_cast<s32>( Dependency.Kind );
        Mix( &Kind, sizeof( Kind ) );
        MixString( Dependency.Path.generic_string() );

        std::ifstream File( Dependency.Path, std::ios::binary );
        if( !File )
        {
            MixString( "missing" );
            continue;
        }
        while( File )
        {
            File.read( reinterpret_cast<char*>( Buffer.data() ), Buffer.size() );
            const std::streamsize nRead = File.gcount();
            if( nRead > 0 )
                Mix( Buffer.data(), (usize)nRead );
        }
    }

    std::ostringstream Fingerprint;
    Fingerprint << std::hex << std::setw( 16 ) << std::setfill( '0' ) << Hash;
    return xstring( Fingerprint.str().c_str() );
}

//=========================================================================

xbool ReadJson( const std::filesystem::path& Path, artifact_manifest_metadata& Metadata, std::string& Error )
{
    std::ifstream File( Path, std::ios::binary );
    if( !File )
    {
        Error = xfs( "Unable to read artifact manifest: %s", Path.generic_string().c_str() );
        return FALSE;
    }
    const std::string Text( ( std::istreambuf_iterator<char>( File ) ), std::istreambuf_iterator<char>() );
    manifest_json_reader Reader( Text );
    if( !Reader.Parse( Metadata, Error ) )
    {
        Error = xfs( "Invalid artifact manifest %s: %s", Path.generic_string().c_str(), Error.c_str() );
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

artifact_build_decision_result artifact_manifest::Evaluate( const std::filesystem::path& ManifestPath,
                                                            const char* pToolFingerprint, s32 SchemaVersion,
                                                            const char* pInputFingerprint ) const
{
    artifact_manifest_metadata Existing;
    std::string Error;
    if( !ReadJson( ManifestPath, Existing, Error ) )
        return { artifact_build_decision::Rebuild, Error };

    if( Existing.SchemaVersion != SchemaVersion )
    {
        return { artifact_build_decision::Rebuild, "artifact manifest schema changed" };
    }
    if( Existing.ToolFingerprint != ( pToolFingerprint ? pToolFingerprint : "" ) )
    {
        return { artifact_build_decision::Rebuild, "resource tool fingerprint changed" };
    }
    if( Existing.InputFingerprint != ( pInputFingerprint ? pInputFingerprint : "" ) )
    {
        return { artifact_build_decision::Rebuild, "input dependency fingerprint changed" };
    }

    const xarray<artifact> Expected = SortedArtifacts( m_Entries );
    const xarray<artifact> Current = SortedArtifacts( Existing.Artifacts );
    for( s32 iEntry = 0; iEntry < Expected.GetCount(); ++iEntry )
    {
        const artifact& Entry = Expected[iEntry];
        xbool bFound = FALSE;
        for( s32 iCurrent = 0; iCurrent < Current.GetCount(); ++iCurrent )
        {
            const artifact& ExistingEntry = Current[iCurrent];
            if( SameArtifact( Entry, ExistingEntry ) )
            {
                bFound = TRUE;
                break;
            }
        }
        if( !bFound )
        {
            return { artifact_build_decision::Rebuild,
                     std::string( (const char*)xfs( "expected artifact is missing from manifest: %s",
                                                    (const char*)Entry.Role ) ) };
        }
        std::error_code ErrorCode;
        if( !std::filesystem::exists( Entry.Path, ErrorCode ) || ErrorCode )
        {
            return { artifact_build_decision::Rebuild,
                     std::string( (const char*)xfs( "expected artifact is missing: %s",
                                                    Entry.Path.generic_string().c_str() ) ) };
        }
    }
    for( s32 iEntry = 0; iEntry < Current.GetCount(); ++iEntry )
    {
        const artifact& Entry = Current[iEntry];
        std::error_code ErrorCode;
        if( !std::filesystem::exists( Entry.Path, ErrorCode ) || ErrorCode )
        {
            return { artifact_build_decision::Rebuild,
                     std::string( (const char*)xfs( "manifest artifact is missing: %s",
                                                    Entry.Path.generic_string().c_str() ) ) };
        }
    }
    return { artifact_build_decision::UpToDate, "manifest fingerprint and artifacts are up to date" };
}

//=========================================================================

xbool artifact_manifest::WriteJson( const std::filesystem::path& Path, const dependency_graph& Dependencies,
                                    const char* pToolFingerprint, s32 SchemaVersion, const char* pInputFingerprint,
                                    std::string& Error ) const
{
    const xarray<artifact> Artifacts = SortedArtifacts( m_Entries );

    const xarray<dependency> DependencyEntries = SortedDependencies( Dependencies.Entries() );

    std::ofstream File( Path, std::ios::binary | std::ios::trunc );
    if( !File )
    {
        Error = xfs( "Unable to write artifact manifest: %s", Path.generic_string().c_str() );
        return FALSE;
    }

    File << "{\n"
         << "  \"schema\": " << SchemaVersion << ",\n"
         << "  \"tool\": \"" << (const char*)EscapeJson( xstring( pToolFingerprint ? pToolFingerprint : "" ) )
         << "\",\n"
         << "  \"inputFingerprint\": \""
         << (const char*)EscapeJson( xstring( pInputFingerprint ? pInputFingerprint : "" ) ) << "\",\n"
         << "  \"artifacts\": [\n";
    for( s32 i = 0; i < Artifacts.GetCount(); ++i )
    {
        const artifact& Entry = Artifacts[i];
        File << "    {\"kind\": \"" << ArtifactKindName( Entry.Kind ) << "\", \"role\": \""
             << (const char*)EscapeJson( Entry.Role ) << "\", \"path\": \""
             << (const char*)EscapeJson( xstring( Entry.Path.generic_string().c_str() ) ) << "\"}"
             << ( i + 1 == Artifacts.GetCount() ? "\n" : ",\n" );
    }
    File << "  ],\n  \"dependencies\": [\n";
    for( s32 i = 0; i < DependencyEntries.GetCount(); ++i )
    {
        const dependency& Entry = DependencyEntries[i];
        File << "    {\"kind\": \"" << DependencyKindName( Entry.Kind ) << "\", \"path\": \""
             << (const char*)EscapeJson( xstring( Entry.Path.generic_string().c_str() ) ) << "\"}"
             << ( i + 1 == DependencyEntries.GetCount() ? "\n" : ",\n" );
    }
    File << "  ]\n}\n";
    if( !File )
    {
        Error = xfs( "Unable to finish artifact manifest: %s", Path.generic_string().c_str() );
        return FALSE;
    }
    return TRUE;
}

} // namespace resource_tool
