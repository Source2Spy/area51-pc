//=========================================================================
//
//  BitmapSource.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapSource.hpp"

#include "BitmapFormat.hpp"

#include <cctype>
#include <system_error>

namespace resource_tool::bitmap
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool SameName( const std::string& A, const std::string& B )
{
    if( A.size() != B.size() )
        return FALSE;
    for( usize Index = 0; Index < A.size(); ++Index )
    {
        const u8 Left = static_cast<u8>( A[Index] );
        const u8 Right = static_cast<u8>( B[Index] );
        if( x_tolower( Left ) != x_tolower( Right ) )
            return FALSE;
    }
    return TRUE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool ResolveSourcePath( const std::filesystem::path& RequestedPath, std::filesystem::path& ResolvedPath,
                         std::string& Error )
{
    ResolvedPath = RequestedPath.lexically_normal();
    std::error_code ErrorCode;
    if( std::filesystem::is_regular_file( ResolvedPath, ErrorCode ) )
        return TRUE;
    if( ErrorCode )
    {
        Error = "Unable to inspect bitmap source " + ResolvedPath.generic_string() + ": " + ErrorCode.message();
        return FALSE;
    }

    const std::filesystem::path Directory =
        ResolvedPath.parent_path().empty() ? std::filesystem::path( "." ) : ResolvedPath.parent_path();
    const std::string RequestedName = ResolvedPath.filename().string();
    std::filesystem::directory_iterator It( Directory, ErrorCode );
    const std::filesystem::directory_iterator End;
    for( ; !ErrorCode && It != End; It.increment( ErrorCode ) )
    {
        if( SameName( It->path().filename().string(), RequestedName ) )
        {
            ResolvedPath = It->path().lexically_normal();
            return TRUE;
        }
    }
    if( ErrorCode )
    {
        Error =
            "Unable to enumerate bitmap source directory " + Directory.generic_string() + ": " + ErrorCode.message();
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool LoadSourceImage( const std::filesystem::path& RequestedPath, image& Result, std::filesystem::path& ResolvedPath,
                       std::string& Error )
{
    if( !ResolveSourcePath( RequestedPath, ResolvedPath, Error ) )
        return FALSE;
    if( !format_io::LoadSource( ResolvedPath, Result, Error ) )
        return FALSE;
    return Result.Validate( Error );
}

//=========================================================================

xbool ValidateTextureDimensions( const image& Source, u32 MaximumWidth, u32 MaximumHeight, xbool RequirePowerOfTwo,
                                 const std::filesystem::path& SourcePath, std::string& Error )
{
    if( !Source.Validate( Error ) )
        return FALSE;
    const u32 Width = Source.Width();
    const u32 Height = Source.Height();
    if( Width > MaximumWidth || Height > MaximumHeight )
    {
        Error = xfs( "Texture %s (%u x %u) is too big. Max size is (%u x %u)", SourcePath.generic_string().c_str(),
                     Width, Height, MaximumWidth, MaximumHeight );
        return FALSE;
    }
    if( RequirePowerOfTwo && ( ( Width & ( Width - 1 ) ) != 0 || ( Height & ( Height - 1 ) ) != 0 ) )
    {
        Error = xfs( "Texture %s (%u x %u) must have a power of 2 width and height",
                     SourcePath.generic_string().c_str(), Width, Height );
        return FALSE;
    }
    return TRUE;
}

} // namespace resource_tool::bitmap
