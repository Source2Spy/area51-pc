//=========================================================================
//
//  BitmapCompileJob.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapCompileJob.hpp"

#include "BitmapFormat.hpp"
#include "BitmapValidation.hpp"

#include <utility>

namespace resource_tool::bitmap
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool bitmap_compile_job::Execute( const std::filesystem::path& SourcePath, const std::filesystem::path& OutputPath,
                                   const build_profile& Profile, std::string& Error ) const
{
    image Source;
    if( !format_io::LoadSource( SourcePath, Source, Error ) )
        return FALSE;
    if( !ValidateBuildProfile( Profile, Error ) || !ValidateImage( Source, Error ) )
    {
        return FALSE;
    }

    image Processed;
    if( !ApplyBuildProfile( Source, Profile, Processed, Error ) )
        return FALSE;
    if( !ValidateImage( Processed, Error ) )
        return FALSE;

    return format_io::SaveXbmp( OutputPath, Processed, Profile, Error );
}

} // namespace resource_tool::bitmap
