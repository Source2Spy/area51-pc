//=========================================================================
//
//  GeometryTextureExport.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryTextureExport.hpp"

#include "BitmapProduct.hpp"

#include <cstdint>
#include <system_error>
#include <vector>

namespace resource_tool::geometry
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

std::string Lower( std::string Value )
{
    for( char& Character : Value )
        Character = (char)x_tolower( Character );
    return Value;
}

//=========================================================================

std::filesystem::path ResolveTexture( const std::filesystem::path& ProductFile, const std::string& Reference )
{
    std::filesystem::path Path( Reference );
    if( Path.is_absolute() )
        return Path.lexically_normal();
    return ( ProductFile.parent_path() / Path ).lexically_normal();
}

//=========================================================================

std::filesystem::path FindCaseInsensitive( const std::filesystem::path& Requested )
{
    std::error_code ErrorCode;
    if( std::filesystem::exists( Requested, ErrorCode ) && !ErrorCode )
        return Requested;

    const std::filesystem::path Directory = Requested.parent_path();
    const std::string FileName = Lower( Requested.filename().string() );
    std::filesystem::directory_iterator It( Directory, ErrorCode );
    const std::filesystem::directory_iterator End;
    for( ; !ErrorCode && It != End; It.increment( ErrorCode ) )
    {
        if( Lower( It->path().filename().string() ) == FileName )
            return It->path().lexically_normal();
    }
    return Requested;
}

//=========================================================================

u32 StableHash( const std::string& Value )
{
    u32 Hash = 2166136261u;
    for( u8 Character : Value )
    {
        Hash ^= Character;
        Hash *= 16777619u;
    }
    return Hash;
}

//=========================================================================

std::filesystem::path RelativeOrFilename( const std::filesystem::path& File, const std::filesystem::path& Base )
{
    std::error_code ErrorCode;
    const std::filesystem::path Relative = std::filesystem::relative( File, Base, ErrorCode );
    if( !ErrorCode && !Relative.empty() )
        return Relative.lexically_normal();
    return File.filename();
}

//=========================================================================

std::string ReferenceKey( const std::string& Value )
{
    return Lower( std::filesystem::path( Value ).lexically_normal().generic_string() );
}

//=========================================================================

void AddReference( std::vector<std::string>& References, const std::string& Reference )
{
    if( Reference.empty() )
        return;
    for( const std::string& Existing : References )
    {
        if( ReferenceKey( Existing ) == ReferenceKey( Reference ) )
        {
            return;
        }
    }
    References.push_back( Reference );
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

std::string texture_export_map::Key( const std::string& Value )
{
    return Lower( std::filesystem::path( Value ).lexically_normal().generic_string() );
}

//=========================================================================

void texture_export_map::Add( const std::string& Reference, const std::filesystem::path& ResolvedSource,
                              const std::filesystem::path& GltfFile, const std::filesystem::path& RawGeomFile,
                              const std::filesystem::path& ExportedFile )
{
    texture_export_entry Entry;
    Entry.SourcePath = ResolvedSource.generic_string();
    Entry.GltfUri = RelativeOrFilename( ExportedFile, GltfFile.parent_path() ).generic_string();
    Entry.RawGeomPath = RelativeOrFilename( ExportedFile, RawGeomFile.parent_path() ).generic_string();
    const usize Index = m_Entries.size();
    m_Entries.push_back( Entry );
    m_References[Key( Reference )] = Index;
    m_References[Key( ResolvedSource.generic_string() )] = Index;
}

//=========================================================================

const texture_export_entry* texture_export_map::Find( const std::string& Reference ) const
{
    const auto It = m_References.find( Key( Reference ) );
    if( It == m_References.end() || It->second >= m_Entries.size() )
        return NULL;
    return &m_Entries[It->second];
}

//=========================================================================

xbool geometry_texture_export_stage::Execute( const geometry_descriptor& Settings, const geometry_asset& Asset,
                                              const std::filesystem::path& GltfFile,
                                              const std::filesystem::path& RawGeomFile, output_transaction& Transaction,
                                              artifact_manifest& Artifacts, diagnostic_log& Diagnostics,
                                              texture_export_map& Result, std::string& Error ) const
{
    std::vector<std::string> References;
    for( const geometry_texture& Texture : Asset.Materials.Textures )
        AddReference( References, Texture.FileName );
    for( const geometry_material& Material : Asset.Materials.Items )
    {
        for( const geometry_texture& Texture : Material.Textures )
            AddReference( References, Texture.FileName );
    }
    for( const geometry_virtual_texture& VirtualTexture : Asset.Materials.VirtualTextures )
    {
        for( const geometry_texture& Texture : VirtualTexture.Textures )
            AddReference( References, Texture.FileName );
        AddReference( References, VirtualTexture.OverrideFileName );
    }

    const std::filesystem::path ProductFile = Settings.GetSourceFileName();
    const std::filesystem::path TextureDirectory = GltfFile.parent_path() / ( GltfFile.stem().string() + ".textures" );
    std::unordered_map<std::string, std::string> UsedNames;

    for( const std::string& Reference : References )
    {
        const std::filesystem::path Requested = ResolveTexture( ProductFile, Reference );
        const std::filesystem::path Source = FindCaseInsensitive( Requested );
        std::error_code ExistsError;
        if( !std::filesystem::is_regular_file( Source, ExistsError ) || ExistsError )
        {
            Diagnostics.Warning(
                ( "compiled texture is unavailable; preserving reference without export [" + Reference + "]" ).c_str(),
                "GeometryTextureExport", Reference.c_str(), Settings.GetSourceFileName() );
            continue;
        }

        const std::string SourceKey = ReferenceKey( Source.generic_string() );
        if( Result.Find( Reference ) || Result.Find( Source.generic_string() ) )
            continue;

        std::string FileStem = Source.stem().string();
        if( FileStem.empty() )
            FileStem = "texture";
        std::filesystem::path ExportedFile = TextureDirectory / ( FileStem + ".tga" );
        const std::string LowerName = Lower( ExportedFile.filename().string() );
        const auto NameIt = UsedNames.find( LowerName );
        if( NameIt != UsedNames.end() && NameIt->second != SourceKey )
        {
            ExportedFile =
                TextureDirectory / ( FileStem + "_" + (const char*)xfs( "%08X", StableHash( SourceKey ) ) + ".tga" );
        }
        UsedNames[Lower( ExportedFile.filename().string() )] = SourceKey;

        const std::filesystem::path StagedOutput = Transaction.Stage( ExportedFile );
        if( Lower( Source.extension().string() ) != ".xbmp" )
        {
            Error = "Geometry texture export expects XBMP compiled input: " + Source.generic_string();
            return FALSE;
        }
        if( !resource_tool::bitmap::DecompileProduct( Source, StagedOutput, Error ) )
        {
            return FALSE;
        }

        Result.Add( Reference, Source, GltfFile, RawGeomFile, ExportedFile );
        Artifacts.Add( artifact_kind::Auxiliary, "geometry-texture", ExportedFile );
    }
    return TRUE;
}

} // namespace resource_tool::geometry
