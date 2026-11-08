//==============================================================================
//
//  x_workers.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../x_files.hpp"
#include "../x_workers.hpp"
#include "../x_threads.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define X_WORKER_QUEUE_ENTRIES          (X_WORKERS_MAX_JOBS * X_WORKERS_MAX_WORKERS)
#if defined( TARGET_DESKTOP ) || defined( TARGET_MOBILE )
#define X_WORKER_STACK_SIZE             X_MEGABYTE(8)
#else
#define X_WORKER_STACK_SIZE             X_KILOBYTE(64)
#endif
#define X_WORKER_SERVICE_STACK_SIZE     X_KILOBYTE(64)
#define X_WORKER_PRIORITY               0

//==============================================================================
//  TYPES
//==============================================================================

enum worker_job_state
{
    WORKER_JOB_FREE = 0,
    WORKER_JOB_PENDING,
    WORKER_JOB_RUNNING,
    WORKER_JOB_DONE,
};

//------------------------------------------------------------------------------

enum worker_job_type
{
    WORKER_JOB_SINGLE = 0,
    WORKER_JOB_BATCH,
};

//------------------------------------------------------------------------------

enum worker_service_state
{
    WORKER_SERVICE_FREE = 0,
    WORKER_SERVICE_RUNNING,
    WORKER_SERVICE_DONE,
};

//------------------------------------------------------------------------------

struct worker_job
{
    s32             State;
    s32             Generation;
    s32             Type;
    x_worker_fn*    pFunction;
    x_worker_batch_fn* pBatchFunction;
    void*           pData;
    s32             DataSize;
    s32             nBatchJobs;
    s32             iNextBatchJob;
    s32             nBatchJobsCompleted;
    s32             nBatchWorkers;
    s32             nBatchWorkersDone;
    xbool           AutoRelease;
    s32             RunningThreadIds[X_WORKERS_MAX_WORKERS];
#if X_WORKERS_DEBUG
    const char*     pName;
#endif
};

//------------------------------------------------------------------------------

struct worker_service
{
    s32             State;
    s32             Generation;
    x_worker_fn*    pFunction;
    void*           pData;
    xthread*        pThread;
    s32             RunningThreadId;
#if X_WORKERS_DEBUG
    const char*     pName;
#endif
};

//==============================================================================
//  CONSTANTS
//==============================================================================

static xbool        s_Initialized = FALSE;
static xbool        s_Killing     = FALSE;
static s32          s_nWorkers    = 0;
static s32          s_nSubmits    = 0;
static xthread*     s_pWorkers[X_WORKERS_MAX_WORKERS];
static xmesgq*      s_pQueue      = NULL;
static xmutex*      s_pLock       = NULL;
static xsema*       s_pStateChange = NULL;
static s32          s_nStateWaiters = 0;
static worker_job   s_Jobs[X_WORKERS_MAX_JOBS];
static worker_service s_Services[X_WORKERS_MAX_SERVICES];
static worker_job   s_ShutdownJob;
#if X_WORKERS_DEBUG
static s32          s_nJobsSubmitted = 0;
static s32          s_nJobsCompleted = 0;
static s32          s_nServicesStarted = 0;
static s32          s_nServicesCompleted = 0;
#endif

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static void s_NotifyStateChangedLocked( void );

//==============================================================================

static 
xhandle s_MakeHandle( s32 Index, s32 Generation )
{
    return xhandle( ((Generation & 0x7fff) << 16) | (Index & 0xffff) );
}

//==============================================================================

static 
s32 s_GetHandleIndex( xhandle Job )
{
    return ((u32)Job.Handle) & 0xffff;
}

//==============================================================================

static 
s32 s_GetHandleGeneration( xhandle Job )
{
    return (((u32)Job.Handle) >> 16) & 0x7fff;
}

//==============================================================================

static 
s32 s_GetThreadPriority( x_worker_priority Priority )
{
    switch( Priority )
    {
    case X_WORKER_PRIORITY_LOW:
        return -1;
    case X_WORKER_PRIORITY_HIGH:
        return 1;
    case X_WORKER_PRIORITY_NORMAL:
    default:
        return 0;
    }
}

//==============================================================================

static
void s_ClearRunningThreadsLocked( worker_job* pJob )
{
    s32 i;

    ASSERT( pJob );

    for( i=0; i<X_WORKERS_MAX_WORKERS; i++ )
    {
        pJob->RunningThreadIds[i] = HNULL;
    }
}

//==============================================================================

static
xbool s_IsJobRunningOnCurrentThreadLocked( worker_job* pJob )
{
    s32 i;
    s32 ThreadId;

    ASSERT( pJob );

    ThreadId = x_GetThreadID();

    for( i=0; i<X_WORKERS_MAX_WORKERS; i++ )
    {
        if( pJob->RunningThreadIds[i] == ThreadId )
            return TRUE;
    }

    return FALSE;
}

//==============================================================================

static
void s_AddRunningThreadLocked( worker_job* pJob )
{
    s32 i;
    s32 ThreadId;

    ASSERT( pJob );

    ThreadId = x_GetThreadID();

    for( i=0; i<X_WORKERS_MAX_WORKERS; i++ )
    {
        if( pJob->RunningThreadIds[i] == ThreadId )
            return;
    }

    for( i=0; i<X_WORKERS_MAX_WORKERS; i++ )
    {
        if( pJob->RunningThreadIds[i] == HNULL )
        {
            pJob->RunningThreadIds[i] = ThreadId;
            return;
        }
    }

    ASSERTS( FALSE, "Too many worker threads are running a single job" );
}

//==============================================================================

static
void s_RemoveRunningThreadLocked( worker_job* pJob )
{
    s32 i;
    s32 ThreadId;

    ASSERT( pJob );

    ThreadId = x_GetThreadID();

    for( i=0; i<X_WORKERS_MAX_WORKERS; i++ )
    {
        if( pJob->RunningThreadIds[i] == ThreadId )
        {
            pJob->RunningThreadIds[i] = HNULL;
            return;
        }
    }

    ASSERTS( FALSE, "Worker thread was not registered on this job" );
}

//==============================================================================

static
xbool s_IsAnyJobRunningOnCurrentThreadLocked( void )
{
    s32 i;

    for( i=0; i<X_WORKERS_MAX_JOBS; i++ )
    {
        if( ((s_Jobs[i].State == WORKER_JOB_PENDING) ||
             (s_Jobs[i].State == WORKER_JOB_RUNNING)) &&
            s_IsJobRunningOnCurrentThreadLocked( &s_Jobs[i] ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool s_IsServiceRunningOnCurrentThreadLocked( worker_service* pService )
{
    ASSERT( pService );

    return (pService->State == WORKER_SERVICE_RUNNING) &&
           (pService->RunningThreadId == x_GetThreadID());
}

//==============================================================================

static
xbool s_IsAnyServiceRunningOnCurrentThreadLocked( void )
{
    s32 i;

    for( i=0; i<X_WORKERS_MAX_SERVICES; i++ )
    {
        if( (s_Services[i].State == WORKER_SERVICE_RUNNING) &&
            s_IsServiceRunningOnCurrentThreadLocked( &s_Services[i] ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
void s_ClearJobLocked( worker_job* pJob )
{
    ASSERT( pJob );

    pJob->State                = WORKER_JOB_FREE;
    pJob->Type                 = WORKER_JOB_SINGLE;
    pJob->pFunction            = NULL;
    pJob->pBatchFunction       = NULL;
    pJob->pData                = NULL;
    pJob->DataSize             = 0;
    pJob->nBatchJobs           = 0;
    pJob->iNextBatchJob        = 0;
    pJob->nBatchJobsCompleted  = 0;
    pJob->nBatchWorkers        = 0;
    pJob->nBatchWorkersDone    = 0;
    pJob->AutoRelease          = FALSE;
    s_ClearRunningThreadsLocked( pJob );
    #if X_WORKERS_DEBUG
    pJob->pName                = NULL;
    #endif
}

//==============================================================================

static
void s_CompleteJobLocked( worker_job* pJob )
{
    ASSERT( pJob );
    ASSERT( pJob->State == WORKER_JOB_RUNNING );

    if( pJob->AutoRelease )
    {
        s_ClearJobLocked( pJob );
    }
    else
    {
        pJob->State = WORKER_JOB_DONE;
    }

    #if X_WORKERS_DEBUG
    s_nJobsCompleted++;
    #endif

    s_NotifyStateChangedLocked();
}

//==============================================================================

static
void* s_GetBatchData( worker_job* pJob, s32 iJob )
{
    if( !pJob->pData || (pJob->DataSize == 0) )
        return pJob->pData;

    return ((byte*)pJob->pData) + (pJob->DataSize * iJob);
}

//==============================================================================

static 
worker_job* s_GetJob( xhandle Job )
{
    s32 Index;

    if( Job.IsNull() )
        return NULL;

    Index = s_GetHandleIndex( Job );
    if( (Index < 0) || (Index >= X_WORKERS_MAX_JOBS) )
        return NULL;

    if( s_Jobs[Index].State == WORKER_JOB_FREE )
        return NULL;

    if( s_Jobs[Index].Generation != s_GetHandleGeneration( Job ) )
        return NULL;

    return &s_Jobs[Index];
}

//==============================================================================

static 
worker_service* s_GetService( x_worker_service Service )
{
    s32 Index;

    if( Service.IsNull() )
        return NULL;

    Index = s_GetHandleIndex( xhandle( Service.Handle ) );
    if( (Index < 0) || (Index >= X_WORKERS_MAX_SERVICES) )
        return NULL;

    if( s_Services[Index].State == WORKER_SERVICE_FREE )
        return NULL;

    if( s_Services[Index].Generation != s_GetHandleGeneration( xhandle( Service.Handle ) ) )
        return NULL;

    return &s_Services[Index];
}

//==============================================================================

static 
xbool s_HasActiveJobsLocked( void )
{
    s32   i;
    xbool HasActiveJobs;

    HasActiveJobs = FALSE;

    for( i=0; i<X_WORKERS_MAX_JOBS; i++ )
    {
        if( (s_Jobs[i].State == WORKER_JOB_PENDING) || (s_Jobs[i].State == WORKER_JOB_RUNNING) )
        {
            HasActiveJobs = TRUE;
            break;
        }
    }

    return HasActiveJobs;
}

//==============================================================================

static 
xbool s_HasActiveServicesLocked( void )
{
    s32   i;
    xbool HasActiveServices;

    HasActiveServices = FALSE;

    for( i=0; i<X_WORKERS_MAX_SERVICES; i++ )
    {
        if( s_Services[i].State == WORKER_SERVICE_RUNNING )
        {
            HasActiveServices = TRUE;
            break;
        }
    }

    return HasActiveServices;
}

//==============================================================================

static 
xbool s_HasActiveSubmits( void )
{
    xbool HasActiveSubmits;

    x_BeginAtomic();
    HasActiveSubmits = (s_nSubmits > 0);
    x_EndAtomic();

    return HasActiveSubmits;
}

//==============================================================================

static 
xbool s_HasStateWaiters( void )
{
    xbool HasStateWaiters;

    HasStateWaiters = FALSE;

    s_pLock->Enter();
    HasStateWaiters = (s_nStateWaiters > 0);
    s_pLock->Exit();

    return HasStateWaiters;
}

//==============================================================================

static 
void s_NotifyStateChangedLocked( void )
{
    s32 i;
    s32 nWaiters;

    if( !s_pStateChange )
        return;

    nWaiters = s_nStateWaiters;

    for( i=0; i<nWaiters; i++ )
    {
        s_pStateChange->Release( X_TH_NOBLOCK );
    }
}

//==============================================================================

static 
void s_WaitForStateChangeLocked( void )
{
    xsema* pStateChange;

    pStateChange = s_pStateChange;

    ASSERT( pStateChange );

    if( !pStateChange )
    {
        s_pLock->Exit();
        return;
    }

    s_nStateWaiters++;

    s_pLock->Exit();

    pStateChange->Acquire( X_TH_BLOCK );

    s_pLock->Enter();
    ASSERT( s_nStateWaiters > 0 );
    s_nStateWaiters--;
    s_pLock->Exit();
}

//==============================================================================

static 
void s_EndSubmit( void )
{
    xmutex* pLock;

    x_BeginAtomic();
    pLock = s_pLock;
    x_EndAtomic();

    if( pLock )
        pLock->Enter();

    x_BeginAtomic();
    ASSERT( s_nSubmits > 0 );
    s_nSubmits--;
    x_EndAtomic();

    if( pLock )
    {
        s_NotifyStateChangedLocked();
        pLock->Exit();
    }
}

//==============================================================================

static
void s_RunSingleJob( worker_job* pJob )
{
    x_worker_fn* pFunction;
    void*        pData;

    s_pLock->Enter();
    ASSERT( pJob );
    ASSERT( pJob->Type == WORKER_JOB_SINGLE );
    ASSERT( pJob->State == WORKER_JOB_PENDING );
    ASSERT( pJob->pFunction );

    pJob->State = WORKER_JOB_RUNNING;
    s_AddRunningThreadLocked( pJob );
    pFunction   = pJob->pFunction;
    pData       = pJob->pData;
    s_pLock->Exit();

    pFunction( pData );

    s_pLock->Enter();
    ASSERT( pJob->State == WORKER_JOB_RUNNING );
    s_RemoveRunningThreadLocked( pJob );
    s_CompleteJobLocked( pJob );
    s_pLock->Exit();
}

//==============================================================================

static
void s_RunBatchJob( worker_job* pJob )
{
    while( TRUE )
    {
        x_worker_batch_fn* pFunction;
        void*              pData;
        s32                iJob;

        pFunction = NULL;
        pData     = NULL;
        iJob      = -1;

        s_pLock->Enter();
        ASSERT( pJob );
        ASSERT( pJob->Type == WORKER_JOB_BATCH );
        ASSERT( (pJob->State == WORKER_JOB_PENDING) || (pJob->State == WORKER_JOB_RUNNING) );
        ASSERT( pJob->pBatchFunction );

        if( pJob->State == WORKER_JOB_PENDING )
            pJob->State = WORKER_JOB_RUNNING;

        s_AddRunningThreadLocked( pJob );

        if( pJob->iNextBatchJob < pJob->nBatchJobs )
        {
            iJob      = pJob->iNextBatchJob++;
            pFunction = pJob->pBatchFunction;
            pData     = s_GetBatchData( pJob, iJob );
        }

        s_pLock->Exit();

        if( iJob < 0 )
            break;

        pFunction( pData, iJob );

        s_pLock->Enter();
        ASSERT( pJob->State == WORKER_JOB_RUNNING );
        pJob->nBatchJobsCompleted++;
        s_pLock->Exit();
    }

    s_pLock->Enter();
    ASSERT( pJob->State == WORKER_JOB_RUNNING );
    s_RemoveRunningThreadLocked( pJob );
    pJob->nBatchWorkersDone++;

    if( (pJob->nBatchJobsCompleted == pJob->nBatchJobs) &&
        (pJob->nBatchWorkersDone == pJob->nBatchWorkers) )
    {
        s_CompleteJobLocked( pJob );
    }

    s_pLock->Exit();
}

//==============================================================================

static
void s_RunJob( worker_job* pJob )
{
    ASSERT( pJob );

    if( pJob->Type == WORKER_JOB_BATCH )
        s_RunBatchJob( pJob );
    else
        s_RunSingleJob( pJob );
}

//==============================================================================

static 
void s_WorkerThread( s32 argc, char** argv )
{
    (void)argc;
    (void)argv;

    #if X_WORKERS_DEBUG_LOG
    x_DebugMsg( "x_workers: worker thread started id:%d\n", x_GetThreadID() );
    #endif

    while( TRUE )
    {
        worker_job* pJob = (worker_job*)s_pQueue->Recv( MQ_BLOCK );

        if( pJob == &s_ShutdownJob )
            break;

        ASSERT( pJob );
        s_RunJob( pJob );
    }

    #if X_WORKERS_DEBUG_LOG
    x_DebugMsg( "x_workers: worker thread exiting id:%d\n", x_GetThreadID() );
    #endif
}

//==============================================================================

static 
void s_ServiceThread( s32 argc, char** argv )
{
    worker_service* pService;

    (void)argc;

    pService = (worker_service*)argv;

    ASSERT( pService );
    ASSERT( pService->pFunction );

    s_pLock->Enter();
    if( pService->State == WORKER_SERVICE_RUNNING )
        pService->RunningThreadId = x_GetThreadID();
    s_pLock->Exit();

    pService->pFunction( pService->pData );

    s_pLock->Enter();
    ASSERT( pService->State == WORKER_SERVICE_RUNNING );
    pService->State = WORKER_SERVICE_DONE;
    pService->RunningThreadId = HNULL;
    #if X_WORKERS_DEBUG
    s_nServicesCompleted++;
    #endif
    s_NotifyStateChangedLocked();
    s_pLock->Exit();
}

//==============================================================================

static 
xbool s_WorkerServiceStart( x_worker_fn* pFunction, void* pData, const char* pName, x_worker_priority Priority, x_worker_service& Service )
{
    s32             i;
    worker_service* pService;
    xmutex*         pLock;
    xthread*        pThread;
    xbool           CanStart;

    Service  = x_worker_service( HNULL );
    pService = NULL;
    pLock    = NULL;
    pThread  = NULL;

    if( !pFunction )
        return FALSE;

    x_BeginAtomic();
    CanStart = s_Initialized && !s_Killing && s_pLock && (s_nWorkers > 0);
    if( CanStart )
    {
        s_nSubmits++;
        pLock = s_pLock;
    }
    x_EndAtomic();

    if( !CanStart )
        return FALSE;

    pLock->Enter();

    x_BeginAtomic();
    CanStart = s_Initialized && !s_Killing;
    x_EndAtomic();

    if( CanStart )
    {
        for( i=0; i<X_WORKERS_MAX_SERVICES; i++ )
        {
            if( s_Services[i].State == WORKER_SERVICE_FREE )
            {
                s_Services[i].State      = WORKER_SERVICE_RUNNING;
                s_Services[i].Generation = (s_Services[i].Generation + 1) & 0x7fff;
                if( s_Services[i].Generation == 0 )
                    s_Services[i].Generation = 1;
                s_Services[i].pFunction  = pFunction;
                s_Services[i].pData      = pData;
                s_Services[i].pThread    = NULL;
                s_Services[i].RunningThreadId = HNULL;
                #if X_WORKERS_DEBUG
                s_Services[i].pName      = pName;
                s_nServicesStarted++;
                #else
                (void)pName;
                #endif

                pService = &s_Services[i];
                Service  = x_worker_service( s_MakeHandle( i, s_Services[i].Generation ).Handle );
                break;
            }
        }
    }

    pLock->Exit();

    if( !pService )
    {
        s_EndSubmit();
        return FALSE;
    }

    pThread = new xthread( s_ServiceThread,
                           pName ? pName : "Worker Service",
                           X_WORKER_SERVICE_STACK_SIZE,
                           s_GetThreadPriority( Priority ),
                           1,
                           (char**)pService );

    if( !pThread )
    {
        pLock->Enter();
        pService->State     = WORKER_SERVICE_FREE;
        pService->pFunction = NULL;
        pService->pData     = NULL;
        pService->pThread   = NULL;
        pService->RunningThreadId = HNULL;
        #if X_WORKERS_DEBUG
        pService->pName     = NULL;
        s_nServicesStarted--;
        #endif
        pLock->Exit();

        Service = x_worker_service( HNULL );
        s_EndSubmit();
        return FALSE;
    }

    pLock->Enter();
    pService->pThread = pThread;
    pLock->Exit();

    s_EndSubmit();
    return TRUE;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void x_WorkersInit( s32 nWorkers )
{
    s32 i;

    ASSERT( !s_Initialized );
    ASSERT( nWorkers >= 0 );
    ASSERT( nWorkers <= X_WORKERS_MAX_WORKERS );

    #if X_WORKERS_DEBUG_LOG
    x_DebugMsg( "x_workers: init requested n:%d\n", nWorkers );
    #endif

    if( nWorkers < 0 )
        nWorkers = 0;

    if( nWorkers > X_WORKERS_MAX_WORKERS )
        nWorkers = X_WORKERS_MAX_WORKERS;

    s_pLock = new xmutex;
    ASSERT( s_pLock );

    if( nWorkers > 0 )
    {
        s_pQueue = new xmesgq( X_WORKER_QUEUE_ENTRIES );
        ASSERT( s_pQueue );
    }
    else
    {
        s_pQueue = NULL;
    }

    s_pStateChange = new xsema( X_MAX_THREADS, 0 );
    ASSERT( s_pStateChange );

    s_Killing  = FALSE;
    s_nWorkers = nWorkers;
    s_nSubmits = 0;
    s_nStateWaiters = 0;
    #if X_WORKERS_DEBUG
    s_nJobsSubmitted = 0;
    s_nJobsCompleted = 0;
    s_nServicesStarted = 0;
    s_nServicesCompleted = 0;
    #endif

    for( i=0; i<X_WORKERS_MAX_WORKERS; i++ )
    {
        s_pWorkers[i] = NULL;
    }

    for( i=0; i<X_WORKERS_MAX_JOBS; i++ )
    {
        s_Jobs[i].Generation = 0;
        s_ClearJobLocked( &s_Jobs[i] );
    }

    for( i=0; i<X_WORKERS_MAX_SERVICES; i++ )
    {
        s_Services[i].State      = WORKER_SERVICE_FREE;
        s_Services[i].Generation = 0;
        s_Services[i].pFunction  = NULL;
        s_Services[i].pData      = NULL;
        s_Services[i].pThread    = NULL;
        s_Services[i].RunningThreadId = HNULL;
        #if X_WORKERS_DEBUG
        s_Services[i].pName      = NULL;
        #endif
    }

    s_ShutdownJob.Generation = 0;
    s_ClearJobLocked( &s_ShutdownJob );
    s_ShutdownJob.State = WORKER_JOB_DONE;

    s_Initialized = TRUE;

    for( i=0; i<s_nWorkers; i++ )
    {
        s_pWorkers[i] = new xthread( s_WorkerThread, "Worker Thread", X_WORKER_STACK_SIZE, X_WORKER_PRIORITY );
        ASSERT( s_pWorkers[i] );
        #if X_WORKERS_DEBUG_LOG
        x_DebugMsg( "x_workers: worker created index:%d id:%d sys:%d\n", i, s_pWorkers[i]->GetId(), s_pWorkers[i]->GetSystemId() );
        #endif
    }

    #if X_WORKERS_DEBUG_LOG
    x_DebugMsg( "x_workers: init complete n:%d\n", s_nWorkers );
    #endif
}

//==============================================================================

void x_WorkersKill( void )
{
    s32 i;
    xbool WasInitialized;
    xthread* pThread;

    x_BeginAtomic();
    WasInitialized = s_Initialized;
    if( WasInitialized )
        s_Killing = TRUE;
    x_EndAtomic();

    ASSERT( WasInitialized );

    if( !WasInitialized )
        return;

    s_pLock->Enter();
    if( s_IsAnyJobRunningOnCurrentThreadLocked() ||
        s_IsAnyServiceRunningOnCurrentThreadLocked() )
    {
        ASSERTS( FALSE, "x_WorkersKill cannot be called from a worker callback" );
        s_pLock->Exit();

        x_BeginAtomic();
        s_Killing = FALSE;
        x_EndAtomic();
        return;
    }
    s_pLock->Exit();

    #if X_WORKERS_DEBUG && X_WORKERS_DEBUG_LOG
    x_DebugMsg( "x_workers: kill begin n:%d submitted:%d completed:%d\n", s_nWorkers, s_nJobsSubmitted, s_nJobsCompleted );
    #endif

    while( TRUE )
    {
        xbool HasActiveWork;

        s_pLock->Enter();
        HasActiveWork = s_HasActiveJobsLocked() || s_HasActiveServicesLocked() || s_HasActiveSubmits();

        if( !HasActiveWork )
        {
            s_pLock->Exit();
            break;
        }

        s_WaitForStateChangeLocked();
    }

    while( s_HasStateWaiters() )
    {
        x_DelayThread( 1 );
    }

    for( i=0; i<X_WORKERS_MAX_SERVICES; i++ )
    {
        pThread = NULL;

        s_pLock->Enter();

        if( s_Services[i].State == WORKER_SERVICE_DONE )
        {
            pThread = s_Services[i].pThread;

            s_Services[i].State      = WORKER_SERVICE_FREE;
            s_Services[i].pFunction  = NULL;
            s_Services[i].pData      = NULL;
            s_Services[i].pThread    = NULL;
            s_Services[i].RunningThreadId = HNULL;
            #if X_WORKERS_DEBUG
            s_Services[i].pName      = NULL;
            #endif
        }

        s_pLock->Exit();

        delete pThread;
    }

    x_BeginAtomic();
    s_Initialized = FALSE;
    x_EndAtomic();

    for( i=0; i<s_nWorkers; i++ )
    {
        s_pQueue->Send( &s_ShutdownJob, MQ_BLOCK );
    }

    for( i=0; i<s_nWorkers; i++ )
    {
        delete s_pWorkers[i];
        s_pWorkers[i] = NULL;
    }

    delete s_pQueue;
    s_pQueue = NULL;

    delete s_pStateChange;
    s_pStateChange = NULL;

    delete s_pLock;
    s_pLock = NULL;

    x_BeginAtomic();
    s_nWorkers    = 0;
    s_nSubmits    = 0;
    s_Killing     = FALSE;
    x_EndAtomic();
    #if X_WORKERS_DEBUG
    s_nJobsSubmitted = 0;
    s_nJobsCompleted = 0;
    s_nServicesStarted = 0;
    s_nServicesCompleted = 0;
    #endif

    #if X_WORKERS_DEBUG_LOG
    x_DebugMsg( "x_workers: kill complete\n" );
    #endif
}

//==============================================================================

xbool x_WorkersIsInit( void )
{
    xbool IsInitialized;

    x_BeginAtomic();
    IsInitialized = s_Initialized;
    x_EndAtomic();

    return IsInitialized;
}

//==============================================================================

static 
xbool s_SubmitJob( x_worker_fn*       pFunction,
                   x_worker_batch_fn* pBatchFunction,
                   void*              pData,
                   s32                DataSize,
                   s32                nJobs,
                   const char*        pName,
                   xbool              AutoRelease,
                   xhandle&           Job )
{
    s32         i;
    s32         nWorkers;
    s32         nDispatches;
    s32         nSent;
    worker_job* pJob;
    xmutex*     pLock;
    xmesgq*     pQueue;
    xbool       CanSubmit;
    xbool       IsBatch;

    Job        = xhandle( HNULL );
    pLock      = NULL;
    pQueue     = NULL;
    pJob       = NULL;
    nWorkers   = 0;
    IsBatch    = (pBatchFunction != NULL);

    if( IsBatch == (pFunction != NULL) )
        return FALSE;

    if( IsBatch )
    {
        if( (nJobs <= 0) || (DataSize < 0) )
            return FALSE;

        if( (DataSize > 0) && !pData )
            return FALSE;
    }
    else
    {
        nJobs    = 1;
        DataSize = 0;
    }

    x_BeginAtomic();
    CanSubmit = s_Initialized && !s_Killing && s_pLock && ((s_nWorkers == 0) || s_pQueue);
    if( CanSubmit )
    {
        s_nSubmits++;
        pLock    = s_pLock;
        pQueue   = s_pQueue;
        nWorkers = s_nWorkers;
    }
    x_EndAtomic();

    if( !CanSubmit )
        return FALSE;

    pLock->Enter();

    x_BeginAtomic();
    CanSubmit = s_Initialized && !s_Killing;
    x_EndAtomic();

    if( CanSubmit )
    {
        for( i=0; i<X_WORKERS_MAX_JOBS; i++ )
        {
            if( s_Jobs[i].State == WORKER_JOB_FREE )
            {
                s_Jobs[i].State      = WORKER_JOB_PENDING;
                s_Jobs[i].Generation = (s_Jobs[i].Generation + 1) & 0x7fff;
                if( s_Jobs[i].Generation == 0 )
                    s_Jobs[i].Generation = 1;
                s_Jobs[i].Type                = IsBatch ? WORKER_JOB_BATCH : WORKER_JOB_SINGLE;
                s_Jobs[i].pFunction           = pFunction;
                s_Jobs[i].pBatchFunction      = pBatchFunction;
                s_Jobs[i].pData               = pData;
                s_Jobs[i].DataSize            = DataSize;
                s_Jobs[i].nBatchJobs          = nJobs;
                s_Jobs[i].iNextBatchJob       = 0;
                s_Jobs[i].nBatchJobsCompleted = 0;
                s_Jobs[i].nBatchWorkers       = IsBatch ? ((nWorkers > 0) ? MIN( nWorkers, nJobs ) : 1) : 0;
                s_Jobs[i].nBatchWorkersDone   = 0;
                s_Jobs[i].AutoRelease         = AutoRelease;
                #if X_WORKERS_DEBUG
                s_Jobs[i].pName               = pName;
                #else
                (void)pName;
                #endif

                pJob   = &s_Jobs[i];
                Job    = s_MakeHandle( i, s_Jobs[i].Generation );
                #if X_WORKERS_DEBUG
                s_nJobsSubmitted++;
                #endif
                break;
            }
        }
    }

    pLock->Exit();

    if( !pJob )
    {
        s_EndSubmit();
        return FALSE;
    }

    if( nWorkers == 0 )
    {
        s_RunJob( pJob );
        s_EndSubmit();
        return TRUE;
    }

    nDispatches = IsBatch ? pJob->nBatchWorkers : 1;
    nSent       = 0;

    for( i=0; i<nDispatches; i++ )
    {
        if( !pQueue->Send( pJob, MQ_BLOCK ) )
            break;

        nSent++;
    }

    if( nSent != nDispatches )
    {
        if( nSent == 0 )
        {
            pLock->Enter();
            s_ClearJobLocked( pJob );
            #if X_WORKERS_DEBUG
            s_nJobsSubmitted--;
            #endif
            pLock->Exit();
            Job = xhandle( HNULL );
            s_EndSubmit();
            return FALSE;
        }

        ASSERT( IsBatch );

        pLock->Enter();
        pJob->nBatchWorkers = nSent;
        if( (pJob->State == WORKER_JOB_RUNNING) &&
            (pJob->nBatchJobsCompleted == pJob->nBatchJobs) &&
            (pJob->nBatchWorkersDone == pJob->nBatchWorkers) )
        {
            s_CompleteJobLocked( pJob );
        }
        pLock->Exit();
    }

    s_EndSubmit();
    return TRUE;
}

//==============================================================================

xbool x_WorkerJobSubmit( x_worker_fn* pFunction, void* pData, const char* pName, xhandle& Job )
{
    return s_SubmitJob( pFunction, NULL, pData, 0, 1, pName, FALSE, Job );
}

//==============================================================================

xbool x_WorkerJobSubmitDetached( x_worker_fn* pFunction, void* pData, const char* pName )
{
    xhandle Job;

    return s_SubmitJob( pFunction, NULL, pData, 0, 1, pName, TRUE, Job );
}

//==============================================================================

xbool x_WorkerJobSubmitBatch( x_worker_batch_fn* pFunction, void* pData, s32 DataSize, s32 nJobs, const char* pName, xhandle& Job )
{
    return s_SubmitJob( NULL, pFunction, pData, DataSize, nJobs, pName, FALSE, Job );
}

//==============================================================================

xbool x_WorkerJobSubmitBatchDetached( x_worker_batch_fn* pFunction, void* pData, s32 DataSize, s32 nJobs, const char* pName )
{
    xhandle Job;

    return s_SubmitJob( NULL, pFunction, pData, DataSize, nJobs, pName, TRUE, Job );
}

//==============================================================================

xbool x_WorkerServiceStart( x_worker_fn* pFunction, void* pData, const char* pName, x_worker_service& Service, x_worker_priority Priority )
{
    return s_WorkerServiceStart( pFunction, pData, pName, Priority, Service );
}

//==============================================================================

xbool x_WorkerServiceIsDone( x_worker_service Service )
{
    worker_service* pService;
    xbool           IsDone;

    ASSERT( s_Initialized );

    if( !s_Initialized )
        return TRUE;

    if( Service.IsNull() )
        return TRUE;

    s_pLock->Enter();
    pService = s_GetService( Service );
    ASSERTS( pService, "Invalid worker service handle" );
    IsDone = !pService || (pService->State == WORKER_SERVICE_DONE);
    s_pLock->Exit();

    return IsDone;
}

//==============================================================================

void x_WorkerServiceWait( x_worker_service Service )
{
    worker_service* pService;

    ASSERT( s_Initialized );

    if( !s_Initialized || Service.IsNull() )
        return;

    while( TRUE )
    {
        s_pLock->Enter();
        pService = s_GetService( Service );
        ASSERTS( pService, "Invalid worker service handle" );

        if( pService && s_IsServiceRunningOnCurrentThreadLocked( pService ) )
        {
            ASSERTS( FALSE, "Worker service cannot wait for itself" );
            s_pLock->Exit();
            return;
        }

        if( !pService || (pService->State == WORKER_SERVICE_DONE) )
        {
            s_pLock->Exit();
            return;
        }

        s_WaitForStateChangeLocked();
    }
}

//==============================================================================

void x_WorkerServiceRelease( x_worker_service Service )
{
    worker_service* pService;
    xthread*        pThread;

    ASSERT( s_Initialized );

    if( !s_Initialized || Service.IsNull() )
        return;

    pThread = NULL;

    s_pLock->Enter();
    pService = s_GetService( Service );
    ASSERTS( pService, "Invalid worker service handle" );
    ASSERTS( !pService || (pService->State == WORKER_SERVICE_DONE), "Cannot release a worker service before it is done" );

    if( pService && (pService->State == WORKER_SERVICE_DONE) )
    {
        pThread = pService->pThread;

        pService->State     = WORKER_SERVICE_FREE;
        pService->pFunction = NULL;
        pService->pData     = NULL;
        pService->pThread   = NULL;
        pService->RunningThreadId = HNULL;
        #if X_WORKERS_DEBUG
        pService->pName     = NULL;
        #endif
    }

    s_pLock->Exit();

    delete pThread;
}

//==============================================================================

xbool x_WorkerJobIsDone( xhandle Job )
{
    worker_job* pJob;
    xbool       IsDone;

    ASSERT( s_Initialized );

    if( !s_Initialized )
        return TRUE;

    if( Job.IsNull() )
        return TRUE;

    s_pLock->Enter();
    pJob = s_GetJob( Job );
    ASSERTS( pJob, "Invalid worker job handle" );
    IsDone = !pJob || (pJob->State == WORKER_JOB_DONE);
    s_pLock->Exit();

    return IsDone;
}

//==============================================================================

void x_WorkerJobWait( xhandle Job )
{
    worker_job* pJob;

    ASSERT( s_Initialized );

    if( !s_Initialized || Job.IsNull() )
        return;

    while( TRUE )
    {
        s_pLock->Enter();
        pJob = s_GetJob( Job );
        ASSERTS( pJob, "Invalid worker job handle" );

        if( pJob && s_IsJobRunningOnCurrentThreadLocked( pJob ) )
        {
            ASSERTS( FALSE, "Worker job cannot wait for itself" );
            s_pLock->Exit();
            return;
        }

        if( !pJob || (pJob->State == WORKER_JOB_DONE) )
        {
            s_pLock->Exit();
            return;
        }

        s_WaitForStateChangeLocked();
    }
}

//==============================================================================

void x_WorkerJobWaitAll( void )
{
    ASSERT( s_Initialized );

    if( !s_Initialized )
        return;

    while( TRUE )
    {
        s_pLock->Enter();

        if( s_IsAnyJobRunningOnCurrentThreadLocked() )
        {
            ASSERTS( FALSE, "Worker job cannot wait for all jobs from inside a worker callback" );
            s_pLock->Exit();
            return;
        }

        if( !s_HasActiveJobsLocked() )
        {
            s_pLock->Exit();
            return;
        }

        s_WaitForStateChangeLocked();
    }
}

//==============================================================================

void x_WorkerJobRelease( xhandle Job )
{
    worker_job* pJob;

    ASSERT( s_Initialized );

    if( !s_Initialized )
        return;

    s_pLock->Enter();
    pJob = s_GetJob( Job );
    ASSERTS( pJob, "Invalid worker job handle" );
    ASSERTS( !pJob || (pJob->State == WORKER_JOB_DONE), "Cannot release a worker job before it is done" );

    if( pJob && (pJob->State == WORKER_JOB_DONE) )
    {
        s_ClearJobLocked( pJob );
    }

    s_pLock->Exit();
}

//==============================================================================

#if X_WORKERS_DEBUG
void x_WorkersGetDebugSnapshot( x_worker_debug_snapshot& Snapshot )
{
    s32 i;

    x_memset( &Snapshot, 0, sizeof(Snapshot) );

    if( !s_Initialized || !s_pLock )
        return;

    s_pLock->Enter();

    Snapshot.IsInitialized = s_Initialized;
    Snapshot.IsKilling     = s_Killing;
    Snapshot.nWorkers      = s_nWorkers;

    for( i=0; i<X_WORKERS_MAX_JOBS; i++ )
    {
        switch( s_Jobs[i].State )
        {
        case WORKER_JOB_FREE:
            Snapshot.nJobsFree++;
            break;
        case WORKER_JOB_PENDING:
            Snapshot.nJobsPending++;
            break;
        case WORKER_JOB_RUNNING:
            Snapshot.nJobsRunning++;
            break;
        case WORKER_JOB_DONE:
            Snapshot.nJobsDone++;
            break;
        }

        if( s_Jobs[i].State != WORKER_JOB_FREE )
        {
            x_worker_debug_job& Job = Snapshot.Jobs[Snapshot.nJobs++];

            Job.Job   = s_MakeHandle( i, s_Jobs[i].Generation );
            Job.State = s_Jobs[i].State;
            Job.pName = s_Jobs[i].pName ? s_Jobs[i].pName : "";
        }
    }

    for( i=0; i<X_WORKERS_MAX_SERVICES; i++ )
    {
        switch( s_Services[i].State )
        {
        case WORKER_SERVICE_FREE:
            Snapshot.nServicesFree++;
            break;
        case WORKER_SERVICE_RUNNING:
            Snapshot.nServicesRunning++;
            break;
        case WORKER_SERVICE_DONE:
            Snapshot.nServicesDone++;
            break;
        }

        if( s_Services[i].State != WORKER_SERVICE_FREE )
        {
            x_worker_debug_service& Service = Snapshot.Services[Snapshot.nServices++];

            Service.Service = x_worker_service( s_MakeHandle( i, s_Services[i].Generation ).Handle );
            Service.State   = s_Services[i].State;
            Service.pName   = s_Services[i].pName ? s_Services[i].pName : "";
        }
    }

    Snapshot.nJobsSubmitted = s_nJobsSubmitted;
    Snapshot.nJobsCompleted = s_nJobsCompleted;
    Snapshot.nServicesStarted = s_nServicesStarted;
    Snapshot.nServicesCompleted = s_nServicesCompleted;

    s_pLock->Exit();
}
#endif
