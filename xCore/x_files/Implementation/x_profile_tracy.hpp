//==============================================================================
//
//  x_profile_tracy.hpp
//
//  Tracy adapter for x_profile.
//
//==============================================================================

#ifndef X_PROFILE_TRACY_HPP
#define X_PROFILE_TRACY_HPP

#include "../x_target.hpp"
#include "../x_types.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct xprofile_tracy_scope
{
    u32 Id;
    s32 Active;
    u64 ConnectionId;
};

//==============================================================================
//  X_TRACY
//==============================================================================

#if X_PROFILE_TRACY

u32                     x_ProfileTracyRegisterMetric( const char* pName,
                                                      const char* pFile,
                                                      const char* pFunction,
                                                      u32         Line,
                                                      u32         Color );
xprofile_tracy_scope    x_ProfileTracyBeginZone     ( u32 MetricId );
void                    x_ProfileTracyEndZone       ( xprofile_tracy_scope Scope );
void                    x_ProfileTracySetThreadName ( const char* pName );
void                    x_ProfileTracyFrameMark     ( void );
void                    x_ProfileTracyPlot          ( u32 MetricId, f64 Value );

#else

inline u32 x_ProfileTracyRegisterMetric( const char*, const char*, const char*, u32, u32 )
{
    return 0;
}

inline xprofile_tracy_scope x_ProfileTracyBeginZone( u32 )
{
    xprofile_tracy_scope Scope = { 0, 0 };
    return Scope;
}

inline void x_ProfileTracyEndZone( xprofile_tracy_scope ) {}
inline void x_ProfileTracySetThreadName( const char* ) {}
inline void x_ProfileTracyFrameMark( void ) {}
inline void x_ProfileTracyPlot( u32, f64 ) {}

#endif

//==============================================================================
#endif // X_PROFILE_TRACY_HPP
//==============================================================================
