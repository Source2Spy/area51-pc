//=========================================================================
//
//  ResourcePipeline.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ResourcePipeline.hpp"
#include "Diagnostics.hpp"

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void resource_pipeline::AddStage( const char* pName, std::function<xbool( resource_context& )> Execute )
{
    resource_pipeline_stage Stage;
    Stage.Name = pName ? pName : "UnnamedStage";
    Stage.Execute = std::move( Execute );
    m_Stages.Append( Stage );
}

//=========================================================================

xbool resource_pipeline::Execute( resource_context& Context ) const
{
    for( s32 iStage = 0; iStage < m_Stages.GetCount(); ++iStage )
    {
        const resource_pipeline_stage& Stage = m_Stages[iStage];
        if( !Stage.Execute )
        {
            Context.Diagnostics().Error( "Pipeline stage has no implementation", (const char*)Stage.Name );
            return FALSE;
        }
        if( !Stage.Execute( Context ) )
            return FALSE;
    }
    return !Context.Diagnostics().HasErrors();
}

} // namespace resource_tool
