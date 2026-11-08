//==============================================================================
//
//  x_workers.hpp
//
//==============================================================================

#ifndef X_WORKERS_HPP
#define X_WORKERS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

//==============================================================================
//  DEFINES
//==============================================================================

#define X_WORKERS_MAX_WORKERS       8
#define X_WORKERS_MAX_JOBS          128
#define X_WORKERS_MAX_SERVICES      8

#ifndef X_WORKERS_DEFAULT_WORKERS
#define X_WORKERS_DEFAULT_WORKERS   8
#endif

// Keep worker diagnostics out of the default runtime surface.
#ifndef X_WORKERS_DEBUG
#define X_WORKERS_DEBUG             1
#endif

#ifndef X_WORKERS_DEBUG_LOG
#define X_WORKERS_DEBUG_LOG         0
#endif

//==============================================================================
//  TYPES
//==============================================================================

typedef void x_worker_fn      ( void* pData );
typedef void x_worker_batch_fn( void* pData, s32 iJob );

enum x_worker_priority
{
    X_WORKER_PRIORITY_LOW    = -1,
    X_WORKER_PRIORITY_NORMAL =  0,
    X_WORKER_PRIORITY_HIGH   =  1,
};

//------------------------------------------------------------------------------

struct x_worker_service
{
    s32 Handle;

    inline x_worker_service             ( void  )      {}
    inline x_worker_service             ( s32 I )      { Handle = I;             }
    inline operator const s32           ( void ) const { return Handle;          }
    inline xbool          IsNonNull     ( void ) const { return Handle != HNULL; }
    inline xbool          IsNull        ( void ) const { return Handle == HNULL; }
};

//------------------------------------------------------------------------------

#if X_WORKERS_DEBUG
struct x_worker_debug_job
{
    xhandle     Job;
    s32         State;
    const char* pName;
};

//------------------------------------------------------------------------------

struct x_worker_debug_service
{
    x_worker_service Service;
    s32              State;
    const char*      pName;
};

//------------------------------------------------------------------------------

struct x_worker_debug_snapshot
{
    xbool       IsInitialized;
    xbool       IsKilling;
    s32         nWorkers;
    s32         nJobsFree;
    s32         nJobsPending;
    s32         nJobsRunning;
    s32         nJobsDone;
    s32         nJobsSubmitted;
    s32         nJobsCompleted;
    s32         nJobs;
    s32         nServicesFree;
    s32         nServicesRunning;
    s32         nServicesDone;
    s32         nServicesStarted;
    s32         nServicesCompleted;
    s32         nServices;
    x_worker_debug_job Jobs[X_WORKERS_MAX_JOBS];
    x_worker_debug_service Services[X_WORKERS_MAX_SERVICES];
};
#endif

//==============================================================================
//  FUNCTIONS
//==============================================================================

void        x_WorkersInit   ( s32 nWorkers );
void        x_WorkersKill   ( void );
xbool       x_WorkersIsInit ( void );

// Job payloads are borrowed. The caller must keep pData alive until the job is
// done; retained jobs must also be released with x_WorkerJobRelease.
// Batch jobs receive either the same pData for every item when DataSize is 0, or
// pData + DataSize*iJob for each item when DataSize is greater than 0.
xbool       x_WorkerJobSubmit         ( x_worker_fn* pFunction, void* pData, const char* pName, xhandle& Job );
xbool       x_WorkerJobSubmitDetached ( x_worker_fn* pFunction, void* pData, const char* pName );
xbool       x_WorkerJobSubmitBatch    ( x_worker_batch_fn* pFunction, void* pData, s32 DataSize, s32 nJobs, const char* pName, xhandle& Job );
xbool       x_WorkerJobSubmitBatchDetached( x_worker_batch_fn* pFunction, void* pData, s32 DataSize, s32 nJobs, const char* pName );
xbool       x_WorkerJobIsDone         ( xhandle Job );
void        x_WorkerJobWait           ( xhandle Job );
void        x_WorkerJobWaitAll        ( void );
void        x_WorkerJobRelease        ( xhandle Job );

// Services are long-running callbacks with caller-owned payload. They cannot be
// cancelled by x_workers; provide cooperative stop state in pData, wait for the
// service to return, then release the handle.
xbool       x_WorkerServiceStart   ( x_worker_fn* pFunction, void* pData, const char* pName, x_worker_service& Service, x_worker_priority Priority = X_WORKER_PRIORITY_NORMAL );
xbool       x_WorkerServiceIsDone  ( x_worker_service Service );
void        x_WorkerServiceWait    ( x_worker_service Service );
void        x_WorkerServiceRelease ( x_worker_service Service );

// Wait functions must not be called from the callback that owns the same job or
// service. x_WorkersKill waits for submitted jobs and running services to finish.

#if X_WORKERS_DEBUG
void        x_WorkersGetDebugSnapshot( x_worker_debug_snapshot& Snapshot );
#endif

//==============================================================================
#endif // X_WORKERS_HPP
//==============================================================================
