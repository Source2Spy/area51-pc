//=========================================================================
//
//  RenderFailure.hpp
//
//  Structured failure reporting for the render pipeline.
//
//=========================================================================

#ifndef RENDER_FAILURE_HPP
#define RENDER_FAILURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_debug.hpp"
#include "x_string.hpp"

namespace render
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

void  render_ReportFailure( const char* pFile, s32 Line, const char* pSubsystem, const char* pFormat, ... );
void  render_ReportWarning( const char* pSubsystem, const char* pFormat, ... );
void  render_ResetFailureState( void );
void  render_BeginFailureScope( xbool isRecoverable, const char* pPassName );
void  render_EndFailureScope( void );
void  render_ClearRecoverableFailure( void );
xbool render_HasRecoverableFailure( void );

#define RENDER_THROW( Subsystem, ... ) ::render::render_ReportFailure( __FILE__, __LINE__, Subsystem, __VA_ARGS__ )

//-------------------------------------------------------------------------

// Warnings are non-fatal; RENDER_THROW records the first fatal frame error.
#define RENDER_WARNING( Subsystem, ... ) ::render::render_ReportWarning( Subsystem, __VA_ARGS__ )

//=========================================================================
} // namespace render

#endif // RENDER_FAILURE_HPP
//=========================================================================
