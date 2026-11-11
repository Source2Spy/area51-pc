//==============================================================================
//
//  x_profile_nvtx.hpp
//
//==============================================================================

#ifndef X_PROFILE_NVTX_HPP
#define X_PROFILE_NVTX_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../x_types.hpp"

//==============================================================================
//  INITIALIZATION AND ZONES
//==============================================================================

// Call once, before starting profiler workers. Runtime opt-in: A51_PROFILE_NVTX=1.
void    x_ProfileNvtxInit     ( void );
u64     x_ProfileNvtxBeginZone( char const* pName, u32 Color );
void    x_ProfileNvtxEndZone  ( u64 Id );

// Emits/resets copy-volume counters for this thread (the main EndFrame thread).
void    x_ProfileNvtxFrame    ( u64 FrameNumber );

//==============================================================================
//  THREAD CPU TIMING
//==============================================================================

// Only coarse phases read the thread CPU clock. Zero timestamps are unavailable,
// never successful zero-cost samples. Begin/end readings must use the same thread.
xbool   x_ProfileNvtxCpuZoneEnabled   ( char const* pName );
u64     x_ProfileNvtxReadThreadCpuTime( void );
void    x_ProfileNvtxCpuTiming        ( char const* pName, u64 BeginNs, u64 EndNs, f64 WallMs );

//==============================================================================
//  MEMORY COPY SCOPES
//==============================================================================

// A51_PROFILE_MEMMOVE_MIN_BYTES defaults to 65536. Invalid/nonpositive values
// (or values greater than INT_MAX) produce a stderr warning and use the default.
// Set A51_PROFILE_MEMMOVE_MIN_BYTES=4096 for more detailed copy ranges. All
// positive copies contribute to thread-local volume regardless of this threshold.
class xprofile_memmove_scope
{
public:
                            xprofile_memmove_scope ( s32 Bytes, void const* pCaller );
                            ~xprofile_memmove_scope( void );

private:
                            xprofile_memmove_scope ( xprofile_memmove_scope const& ) = delete;
    xprofile_memmove_scope&  operator=              ( xprofile_memmove_scope const& ) = delete;

    u64                     m_Id;
};

//==============================================================================
#endif // X_PROFILE_NVTX_HPP
//==============================================================================
