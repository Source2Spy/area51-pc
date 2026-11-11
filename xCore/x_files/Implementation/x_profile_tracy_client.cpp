//==============================================================================
//
//  x_profile_tracy_client.cpp
//
//  Tracy client implementation translation unit.
//
//==============================================================================

#include "x_profile_tracy.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#if X_PROFILE_TRACY

#ifndef TRACY_ENABLE
#define TRACY_ENABLE
#endif

#ifndef TRACY_ON_DEMAND
#define TRACY_ON_DEMAND
#endif
#ifdef max
    #undef max
#endif
#ifdef min
    #undef min
#endif
#include "../../3rdParty/Tracy/public/tracy/TracyC.h"

namespace
{
    enum
    {
        TRACY_METRIC_CAPACITY = 2048,
        TRACY_NAME_CAPACITY = 128,
        TRACY_FILE_CAPACITY = 192,
        TRACY_FUNCTION_CAPACITY = 96
    };

    struct tracy_metric
    {
        ___tracy_source_location_data SourceLocation;
        char Name[TRACY_NAME_CAPACITY];
        char File[TRACY_FILE_CAPACITY];
        char Function[TRACY_FUNCTION_CAPACITY];
    };

    tracy_metric s_Metrics[TRACY_METRIC_CAPACITY];
    u32          s_MetricCount = 0;

    void CopyString( char* pDestination, u32 Capacity, const char* pSource )
    {
        if( !pDestination || (Capacity == 0) )
            return;

        const char* pRead = pSource ? pSource : "";
        u32 i = 0;
        while( (i + 1 < Capacity) && pRead[i] )
        {
            pDestination[i] = pRead[i];
            ++i;
        }
        pDestination[i] = 0;
    }
}

//==============================================================================

u32 x_ProfileTracyRegisterMetric( const char* pName,
                                  const char* pFile,
                                  const char* pFunction,
                                  u32         Line,
                                  u32         Color )
{
    if( s_MetricCount >= TRACY_METRIC_CAPACITY )
        return 0;

    const u32 Id = ++s_MetricCount;
    tracy_metric& Metric = s_Metrics[Id - 1];
    CopyString( Metric.Name,     TRACY_NAME_CAPACITY,     pName );
    CopyString( Metric.File,     TRACY_FILE_CAPACITY,     pFile );
    CopyString( Metric.Function, TRACY_FUNCTION_CAPACITY, pFunction );

    Metric.SourceLocation.name     = Metric.Name;
    Metric.SourceLocation.function = Metric.Function;
    Metric.SourceLocation.file     = Metric.File;
    Metric.SourceLocation.line     = Line;
    Metric.SourceLocation.color    = Color;
    return Id;
}

//==============================================================================

xprofile_tracy_scope x_ProfileTracyBeginZone( u32 MetricId )
{
    xprofile_tracy_scope Scope = { 0, 0, 0 };
    if( (MetricId == 0) || (MetricId > s_MetricCount) )
        return Scope;

    const TracyCZoneCtx TracyScope = ___tracy_emit_zone_begin(
        &s_Metrics[MetricId - 1].SourceLocation,
        1 );
    Scope.Id     = TracyScope.id;
    Scope.Active = TracyScope.active;
#ifdef TRACY_ON_DEMAND
    Scope.ConnectionId = TracyScope.connectionId;
#endif
    return Scope;
}

//==============================================================================

void x_ProfileTracyEndZone( xprofile_tracy_scope Scope )
{
    TracyCZoneCtx TracyScope = {};
    TracyScope.id     = Scope.Id;
    TracyScope.active = Scope.Active;
#ifdef TRACY_ON_DEMAND
    TracyScope.connectionId = Scope.ConnectionId;
#endif
    ___tracy_emit_zone_end( TracyScope );
}

//==============================================================================

void x_ProfileTracySetThreadName( const char* pName )
{
    TracyCSetThreadName( pName ? pName : "unknown" );
}

//==============================================================================

void x_ProfileTracyFrameMark( void )
{
    TracyCFrameMarkNamed( "Area51 Frame" );
}

//==============================================================================

void x_ProfileTracyPlot( u32 MetricId, f64 Value )
{
    if( (MetricId == 0) || (MetricId > s_MetricCount) )
        return;

    TracyCPlot( s_Metrics[MetricId - 1].Name, Value );
}

#include "../../3rdParty/Tracy/public/TracyClient.cpp"

#endif
