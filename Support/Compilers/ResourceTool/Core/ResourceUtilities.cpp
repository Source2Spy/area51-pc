//=========================================================================
//
//  ResourceUtilities.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ResourceUtilities.hpp"

#include "Diagnostics.hpp"
#include "ResourceHandler.hpp"

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xstring LowerExtension( const std::filesystem::path& Path )
{
    xstring Extension( Path.extension().string().c_str() );
    for( s32 iCharacter = 0; iCharacter < Extension.GetLength(); ++iCharacter )
        Extension[iCharacter] = (char)x_tolower( Extension[iCharacter] );
    return Extension;
}

//=========================================================================

xbool ReportError( resource_context& Context, const xstring& Message, const char* pStage )
{
    Context.Diagnostics().Error( (const char*)Message, pStage, Context.GetInputPath().generic_string().c_str() );
    return FALSE;
}

//=========================================================================

xbool ReportError( resource_context& Context, const std::string& Message, const char* pStage )
{
    Context.Diagnostics().Error( Message.c_str(), pStage, Context.GetInputPath().generic_string().c_str() );
    return FALSE;
}

} // namespace resource_tool
