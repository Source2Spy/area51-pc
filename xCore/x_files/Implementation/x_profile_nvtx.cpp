//==============================================================================
//
//  x_profile_nvtx.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_profile_nvtx.hpp"

#if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX

#include "../x_atomic.hpp"
#include "../x_plus.hpp"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <nvtx3/nvToolsExt.h>

//==============================================================================
//  NVTX IMPLEMENTATION
//==============================================================================

static_assert( sizeof( nvtxRangeId_t ) <= sizeof( u64 ), "NVTX range IDs must fit in u64" );

namespace
{
    x_atomic_s32       s_Enabled      = X_ATOMIC_S32_INIT( FALSE );
    nvtxDomainHandle_t s_Domain       = NULL;
    nvtxDomainHandle_t s_MemoryDomain = NULL;
    nvtxDomainHandle_t s_CpuDomain    = NULL;

    x_atomic_s32 s_CpuClockAvailable = X_ATOMIC_S32_INIT( FALSE );

    s32              s_MemmoveMinBytes = 65536;
    thread_local u64 s_MemmoveBytes    = 0;
    thread_local u64 s_MemmoveCalls    = 0;

    //==========================================================================

    nvtxEventAttributes_t MakeAttributes( char const* pName )
    {
        nvtxEventAttributes_t Attributes = {};
        Attributes.version       = NVTX_VERSION;
        Attributes.size          = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
        Attributes.messageType   = NVTX_MESSAGE_TYPE_ASCII;
        Attributes.message.ascii = pName;

        return Attributes;
    }
}

//==============================================================================
//  INITIALIZATION
//==============================================================================

void x_ProfileNvtxInit( void )
{
    if( x_AtomicLoadAcquire( &s_Enabled ) )
    {
        return;
    }

    char const* pEnabled = x_getenv( "A51_PROFILE_NVTX" );
    if( (pEnabled == NULL) || (x_strcmp( pEnabled, "1" ) != 0) )
    {
        return;
    }

    char const* pMinimum = x_getenv( "A51_PROFILE_MEMMOVE_MIN_BYTES" );
    if( pMinimum != NULL )
    {
        char* pEnd = NULL;
        errno = 0;
        long Minimum = strtol( pMinimum, &pEnd, 10 );

        if( (errno != 0) || (pEnd == pMinimum) || (*pEnd != '\0') ||
            (Minimum <= 0) || (Minimum > INT_MAX) )
        {
            fprintf( stderr,
                     "A51_PROFILE_MEMMOVE_MIN_BYTES must be a positive integer <= INT_MAX; using 65536.\n" );
        }
        else
        {
            s_MemmoveMinBytes = static_cast<s32>( Minimum );
        }
    }

    // Initialize the SDK explicitly here, never lazily on a worker's first event.
    nvtxInitialize( NULL );
    s_Domain       = nvtxDomainCreateA( "A51" );
    s_MemoryDomain = nvtxDomainCreateA( "A51.Memory" );
    s_CpuDomain    = nvtxDomainCreateA( "A51.CPU" );

    #if defined( CLOCK_THREAD_CPUTIME_ID )
    timespec CpuTime;
    if( clock_gettime( CLOCK_THREAD_CPUTIME_ID, &CpuTime ) == 0 )
    {
        x_AtomicStoreRelease( &s_CpuClockAvailable, TRUE );
    }
    #endif

    x_AtomicStoreRelease( &s_Enabled, TRUE );

    fprintf( stderr, "[A51 profiling] NVTX enabled; memmove minimum bytes=%d\n",
             static_cast<int>( s_MemmoveMinBytes ) );
    fprintf( stderr, "[A51 profiling] Coarse phase thread CPU timing: %s\n",
             x_AtomicLoadAcquire( &s_CpuClockAvailable ) ? "enabled" : "unavailable" );
}

//==============================================================================
//  THREAD CPU TIMING
//==============================================================================

xbool x_ProfileNvtxCpuZoneEnabled( char const* pName )
{
    if( !x_AtomicLoadAcquire( &s_Enabled ) ||
        !x_AtomicLoadAcquire( &s_CpuClockAvailable ) )
    {
        return FALSE;
    }

    static char const* const s_Phases[] =
    {
        "Frame/Frame.Update",
        "Frame/Frame.WaitForSwapchain",
        "Context/AdvanceSimulation",
        "Context/obj_mgr::AdvanceSimulation",
        "Context/Render",
        "Renderer/Render/Prepare",
        "Renderer/FrameGraph/Execute",
        "Frame/Frame.SubmitPresent"
    };

    for( char const* pPhase : s_Phases )
    {
        if( x_strcmp( pName, pPhase ) == 0 )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

u64 x_ProfileNvtxReadThreadCpuTime( void )
{
    if( !x_AtomicLoadAcquire( &s_CpuClockAvailable ) )
    {
        return 0;
    }

    #if defined( CLOCK_THREAD_CPUTIME_ID )
    timespec CpuTime;
    if( clock_gettime( CLOCK_THREAD_CPUTIME_ID, &CpuTime ) == 0 )
    {
        return static_cast<u64>( CpuTime.tv_sec ) * 1000000000ull + static_cast<u64>( CpuTime.tv_nsec );
    }

    if( x_AtomicExchange( &s_CpuClockAvailable, FALSE ) )
    {
        fprintf( stderr, "[A51 profiling] Thread CPU clock failed; CPU samples disabled.\n" );
    }
    #endif

    return 0;
}

//==============================================================================

void x_ProfileNvtxCpuTiming( char const* pName, u64 BeginNs, u64 EndNs, f64 WallMs )
{
    if( !BeginNs || !EndNs || EndNs < BeginNs || !x_AtomicLoadAcquire( &s_Enabled ) )
    {
        return;
    }

    char Message[160];
    snprintf( Message, sizeof( Message ), "Wall ms/%s", pName );

    nvtxEventAttributes_t Attributes = MakeAttributes( Message );
    Attributes.payloadType    = NVTX_PAYLOAD_TYPE_DOUBLE;
    Attributes.payload.dValue = WallMs;
    nvtxDomainMarkEx( s_CpuDomain, &Attributes );

    snprintf( Message, sizeof( Message ), "CPU ms/%s", pName );
    Attributes.message.ascii  = Message;
    Attributes.payload.dValue = static_cast<f64>( EndNs - BeginNs ) / 1000000.0;
    nvtxDomainMarkEx( s_CpuDomain, &Attributes );
}

//==============================================================================
//  ZONES AND FRAME MARKERS
//==============================================================================

u64 x_ProfileNvtxBeginZone( char const* pName, u32 Color )
{
    if( !x_AtomicLoadAcquire( &s_Enabled ) )
    {
        return 0;
    }

    nvtxEventAttributes_t Attributes = MakeAttributes( pName );
    Attributes.colorType = NVTX_COLOR_ARGB;
    Attributes.color     = Color;

    return static_cast<u64>( nvtxDomainRangeStartEx( s_Domain, &Attributes ) );
}

//==============================================================================

void x_ProfileNvtxEndZone( u64 Id )
{
    if( Id != 0 )
    {
        nvtxDomainRangeEnd( s_Domain, static_cast<nvtxRangeId_t>( Id ) );
    }
}

//==============================================================================

void x_ProfileNvtxFrame( u64 FrameNumber )
{
    if( !x_AtomicLoadAcquire( &s_Enabled ) )
    {
        return;
    }

    nvtxEventAttributes_t Attributes = MakeAttributes( "Frame" );
    Attributes.payloadType      = NVTX_PAYLOAD_TYPE_UNSIGNED_INT64;
    Attributes.payload.ullValue = FrameNumber;
    nvtxDomainMarkEx( s_Domain, &Attributes );

    // EndFrame flushes only the calling (main) thread, not worker-thread totals.
    Attributes                  = MakeAttributes( "x_memmove frame bytes (main thread)" );
    Attributes.payloadType      = NVTX_PAYLOAD_TYPE_UNSIGNED_INT64;
    Attributes.payload.ullValue = s_MemmoveBytes;
    nvtxDomainMarkEx( s_MemoryDomain, &Attributes );

    Attributes                  = MakeAttributes( "x_memmove frame calls (main thread)" );
    Attributes.payloadType      = NVTX_PAYLOAD_TYPE_UNSIGNED_INT64;
    Attributes.payload.ullValue = s_MemmoveCalls;
    nvtxDomainMarkEx( s_MemoryDomain, &Attributes );
    s_MemmoveBytes = 0;
    s_MemmoveCalls = 0;
}

//==============================================================================
//  MEMORY COPY SCOPES
//==============================================================================

xprofile_memmove_scope::xprofile_memmove_scope( s32 Bytes, void const* pCaller )
    : m_Id( 0 )
{
    if( !x_AtomicLoadAcquire( &s_Enabled ) || (Bytes <= 0) )
    {
        return;
    }

    s_MemmoveBytes += static_cast<u64>( Bytes );
    ++s_MemmoveCalls;

    if( Bytes < s_MemmoveMinBytes )
    {
        return;
    }

    char Message[64];
    snprintf( Message, sizeof( Message ), "x_memmove caller=0x%llx",
              static_cast<unsigned long long>( reinterpret_cast<uaddr>( pCaller ) ) );

    nvtxEventAttributes_t Attributes = MakeAttributes( Message );
    Attributes.payloadType     = NVTX_PAYLOAD_TYPE_INT64;
    Attributes.payload.llValue = Bytes;

    m_Id = static_cast<u64>( nvtxDomainRangeStartEx( s_MemoryDomain, &Attributes ) );
}

//==============================================================================

xprofile_memmove_scope::~xprofile_memmove_scope( void )
{
    if( m_Id != 0 )
    {
        nvtxDomainRangeEnd( s_MemoryDomain, static_cast<nvtxRangeId_t>( m_Id ) );
    }
}

#else

//==============================================================================
//  DISABLED IMPLEMENTATION
//==============================================================================

void x_ProfileNvtxInit( void )
{
}

//==============================================================================

u64 x_ProfileNvtxBeginZone( char const*, u32 )
{
    return 0;
}

//==============================================================================

void x_ProfileNvtxEndZone( u64 )
{
}

//==============================================================================

void x_ProfileNvtxFrame( u64 )
{
}

//==============================================================================

xbool x_ProfileNvtxCpuZoneEnabled( char const* )
{
    return FALSE;
}

//==============================================================================

u64 x_ProfileNvtxReadThreadCpuTime( void )
{
    return 0;
}

//==============================================================================

void x_ProfileNvtxCpuTiming( char const*, u64, u64, f64 )
{
}

//==============================================================================

xprofile_memmove_scope::xprofile_memmove_scope( s32, void const* )
    : m_Id( 0 )
{
}

//==============================================================================

xprofile_memmove_scope::~xprofile_memmove_scope( void )
{
}

#endif

//==============================================================================
