//==============================================================================
//
//  x_profile.cpp
//
//==============================================================================

//==============================================================================
//
//  X-Platform frame profiler.
//
//  Recording uses typed metric handles and per-thread storage. Completed frames
//  are exposed through immutable snapshot handles and batched capture sinks.
//  All producer work for a frame must finish before EndFrame() is called.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../x_profile.hpp"
#include "../x_debug.hpp"
#include "../x_memory.hpp"
#include "../x_plus.hpp"
#include "../x_threads.hpp"
#include "x_profile_tracy.hpp"
#include "x_profile_nvtx.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    XPROFILE_MAX_SINKS = 8,
    XPROFILE_EXTERNAL_NAME_LENGTH = XPROFILE_CATEGORY_LENGTH + XPROFILE_NAME_LENGTH + 2
};

static u32 s_ProfileGeneration = 0;

//==============================================================================
//  PROFILER IMPLEMENTATION
//==============================================================================

struct xprofiler::implementation
{
    struct accumulator
    {
        xtick DurationTicks;
        f64   CounterValue;
        f64   GaugeValue;
        xtick GaugeTimestamp;
        u32   CallCount;
        u32   EventCount;
        xbool Active;
    };

    //---------------------------------------------------------------------

    struct active_scope
    {
        xprofile_id          Id;
        xtick                BeginTick;
        xprofile_tracy_scope TracyScope;
        #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
        u64                  NvtxRange;
        u64                  BeginCpuNs;
        #endif
    };

    //---------------------------------------------------------------------

    struct pending_event
    {
        xtick                        Timestamp;
        s32                          ThreadId;
        u16                          Depth;
        xprofile_timeline_event_type Type;
        xprofile_id                  MetricId;
        f64                          Value;
    };

    //---------------------------------------------------------------------

    struct thread_state
    {
        s32             ThreadId;
        char            Name[XPROFILE_NAME_LENGTH];
        u64             FrameNumber;
        accumulator*    pAccumulators;
        active_scope*   pScopes;
        pending_event*  pEvents;
        u32             ScopeCount;
        u32             EventCount;
        u32             DroppedEventCount;
    };

    //---------------------------------------------------------------------

    struct metric_state
    {
        xprofile_metric_info    Info;
        u32                     Hash;
        u32                     TracyMetricId;
        char                    ExternalName[XPROFILE_EXTERNAL_NAME_LENGTH];
        #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
        xbool                   NvtxCpuTiming;
        #endif
        xprofile_metric_sample* pSamples;
    };

    //---------------------------------------------------------------------

    xprofile_config           Config;
    xprofile_capture_mode     CaptureMode;
    xbool                     FineTimingEnabled;
    xbool                     FrameActive;
    u64                       CurrentFrameNumber;
    u64                       CompletedFrameCount;
    xtick                     FrameBeginTick;
    u32                       MetricCount;
    u32                       PropertyCount;
    u32                       ThreadCount;
    u32                       SinkCount;
    u64                       NextTimelineSequence;
    u32                       Generation;
    xprofile_id               FrameDurationMetric;
    metric_state*             pMetrics;
    xprofile_property_info*   pProperties;
    thread_state*             pThreads;
    accumulator*              pThreadAccumulators;
    active_scope*             pThreadScopes;
    pending_event*            pThreadEvents;
    pending_event*            pMergedEvents;
    xprofile_frame_info*      pFrames;
    xprofile_timeline_event*  pTimelineEvents;
    xprofile_capture_metric*  pCaptureMetrics;
    xprofile_thread_info*     pCaptureThreads;
    xprofile_property_info*   pCaptureProperties;
    xprofile_timeline_event*  pCaptureEvents;
    xprofile_sink*            Sinks[XPROFILE_MAX_SINKS];

    //---------------------------------------------------------------------

    implementation( const xprofile_config& ProfileConfig, u32 ProfileGeneration ) :
        Config                 ( ProfileConfig ),
        CaptureMode            ( ProfileConfig.CaptureMode ),
        FineTimingEnabled      ( ProfileConfig.FineTimingEnabled ),
        FrameActive            ( FALSE ),
        CurrentFrameNumber     ( 0 ),
        CompletedFrameCount    ( 0 ),
        FrameBeginTick         ( 0 ),
        MetricCount            ( 0 ),
        PropertyCount          ( 0 ),
        ThreadCount            ( 0 ),
        SinkCount              ( 0 ),
        NextTimelineSequence   ( 1 ),
        Generation             ( ProfileGeneration ),
        FrameDurationMetric    ( XPROFILE_INVALID_ID ),
        pMetrics               ( NULL ),
        pProperties            ( NULL ),
        pThreads               ( NULL ),
        pThreadAccumulators    ( NULL ),
        pThreadScopes          ( NULL ),
        pThreadEvents          ( NULL ),
        pMergedEvents          ( NULL ),
        pFrames                ( NULL ),
        pTimelineEvents        ( NULL ),
        pCaptureMetrics        ( NULL ),
        pCaptureThreads        ( NULL ),
        pCaptureProperties     ( NULL ),
        pCaptureEvents         ( NULL )
    {
        x_memset( Sinks, 0, sizeof(Sinks) );
    }

    //---------------------------------------------------------------------

    ~implementation( void )
    {
        for( u32 i = 0; i < MetricCount; ++i )
            delete[] pMetrics[i].pSamples;

        delete[] pCaptureEvents;
        delete[] pCaptureProperties;
        delete[] pCaptureThreads;
        delete[] pCaptureMetrics;
        delete[] pTimelineEvents;
        delete[] pFrames;
        delete[] pMergedEvents;
        delete[] pThreadEvents;
        delete[] pThreadScopes;
        delete[] pThreadAccumulators;
        delete[] pThreads;
        delete[] pProperties;
        delete[] pMetrics;
    }

    //---------------------------------------------------------------------

    static void CopyString( char* pDestination, u32 Capacity, const char* pSource )
    {
        if( !pDestination || (Capacity == 0) )
            return;

        x_strncpy( pDestination, pSource ? pSource : "", Capacity );
        pDestination[Capacity - 1] = 0;
    }

    //---------------------------------------------------------------------

    static u32 HashString( u32 Hash, const char* pText )
    {
        const u8* pBytes = (const u8*)(pText ? pText : "");
        while( *pBytes )
        {
            Hash ^= *pBytes++;
            Hash *= 16777619u;
        }
        return Hash;
    }

    //---------------------------------------------------------------------

    static u32 HashMetric( const xprofile_metric_desc& Desc )
    {
        u32 Hash = 2166136261u;
        Hash = HashString( Hash, Desc.pCategory );
        Hash ^= (u32)Desc.Type;
        Hash *= 16777619u;
        if( Desc.pFile && Desc.pFile[0] )
        {
            Hash = HashString( Hash, Desc.pFile );
            Hash ^= Desc.Line;
            Hash *= 16777619u;
        }
        return HashString( Hash, Desc.pName );
    }

    //---------------------------------------------------------------------

    static void BuildExternalName( char*       pDestination,
                                   u32         Capacity,
                                   const char* pCategory,
                                   const char* pName )
    {
        CopyString( pDestination, Capacity, pCategory );
        u32 Length = (u32)x_strlen( pDestination );
        if( Length + 1 < Capacity )
        {
            pDestination[Length++] = '/';
            pDestination[Length] = 0;
        }
        CopyString( pDestination + Length, Capacity - Length, pName );
    }

    //---------------------------------------------------------------------

    static u32 GetCategoryColor( const char* pCategory )
    {
        const u32 Hash = HashString( 2166136261u, pCategory );
        const u32 Red   = 80u + ((Hash >>  0) & 0x7Fu);
        const u32 Green = 80u + ((Hash >>  8) & 0x7Fu);
        const u32 Blue  = 80u + ((Hash >> 16) & 0x7Fu);
        return (Red << 16) | (Green << 8) | Blue;
    }

    //---------------------------------------------------------------------

    xbool IsValidMetric( xprofile_id Id ) const
    {
        return Id < MetricCount;
    }

    //---------------------------------------------------------------------

    void ResetThreadForFrame( thread_state& Thread, u64 FrameNumber )
    {
        x_memset( Thread.pAccumulators,
                  0,
                  sizeof(accumulator) * MetricCount );
        Thread.FrameNumber       = FrameNumber;
        Thread.EventCount        = 0;
        Thread.DroppedEventCount = 0;
    }

    //---------------------------------------------------------------------

    thread_state* GetThreadState( xbool RequireActiveFrame = TRUE )
    {
        static thread_local thread_state* s_pThread = NULL;
        static thread_local u32           s_ThreadGeneration = 0;

        if( !FrameActive && RequireActiveFrame )
            return NULL;

        if( s_pThread && (s_ThreadGeneration == Generation) )
        {
            if( FrameActive && (s_pThread->FrameNumber != CurrentFrameNumber) )
                ResetThreadForFrame( *s_pThread, CurrentFrameNumber );
            return s_pThread;
        }

        // A thread without registered state cannot own an active scope. Do not
        // create new thread state while frame capture is inactive.
        if( !FrameActive )
            return NULL;

        const s32 ThreadId = x_GetThreadID();
        x_BeginAtomic();
        for( u32 i = 0; i < ThreadCount; ++i )
        {
            if( pThreads[i].ThreadId == ThreadId )
            {
                s_pThread          = &pThreads[i];
                s_ThreadGeneration = Generation;
                x_EndAtomic();
                if( s_pThread->FrameNumber != CurrentFrameNumber )
                    ResetThreadForFrame( *s_pThread, CurrentFrameNumber );
                return s_pThread;
            }
        }

        if( ThreadCount >= Config.ThreadCapacity )
        {
            x_EndAtomic();
            return NULL;
        }

        const u32 Index = ThreadCount;
        thread_state& Thread = pThreads[Index];
        Thread.ThreadId = ThreadId;
        xthread* pCurrentThread = x_GetCurrentThread();
        CopyString( Thread.Name,
                    XPROFILE_NAME_LENGTH,
                    pCurrentThread ? pCurrentThread->GetName() : "unknown" );
        Thread.pAccumulators = pThreadAccumulators + (Index * Config.MetricCapacity);
        Thread.pScopes       = pThreadScopes       + (Index * Config.ScopeDepth);
        Thread.pEvents       = pThreadEvents       + (Index * Config.EventsPerThread);
        ThreadCount++;

        s_pThread          = &Thread;
        s_ThreadGeneration = Generation;
        x_EndAtomic();

        x_ProfileTracySetThreadName( Thread.Name );
        ResetThreadForFrame( Thread, CurrentFrameNumber );
        return &Thread;
    }

    //---------------------------------------------------------------------

    void EmitTimelineEvent( thread_state& Thread,
                            xprofile_timeline_event_type Type,
                            xprofile_id MetricId,
                            xtick Timestamp,
                            u16 Depth,
                            f64 Value )
    {
        if( CaptureMode != XPROFILE_CAPTURE_TIMELINE )
            return;

        if( Thread.EventCount >= Config.EventsPerThread )
        {
            Thread.DroppedEventCount++;
            return;
        }

        pending_event& Event = Thread.pEvents[Thread.EventCount++];
        Event.Timestamp = Timestamp;
        Event.ThreadId  = Thread.ThreadId;
        Event.Depth     = Depth;
        Event.Type      = Type;
        Event.MetricId  = MetricId;
        Event.Value     = Value;
    }

    //---------------------------------------------------------------------

    xprofile_id RegisterMetric( const xprofile_metric_desc& Desc )
    {
        if( !Desc.pName || !Desc.pName[0] )
            return XPROFILE_INVALID_ID;

        const char* pCategory = (Desc.pCategory && Desc.pCategory[0])
                              ? Desc.pCategory
                              : "General";
        xprofile_metric_desc Normalized = Desc;
        Normalized.pCategory = pCategory;
        const u32 Hash = HashMetric( Normalized );
        xprofile_metric_sample* pSamples = new xprofile_metric_sample[Config.FrameCapacity];
        x_memset( pSamples,
                  0,
                  sizeof(xprofile_metric_sample) * Config.FrameCapacity );

        x_BeginAtomic();
        for( u32 i = 0; i < MetricCount; ++i )
        {
            const metric_state& Metric = pMetrics[i];
            if( (Metric.Hash == Hash) &&
                (Metric.Info.Type == Desc.Type) &&
                (Metric.Info.Line == Desc.Line) &&
                (x_strcmp( Metric.Info.Name, Desc.pName ) == 0) &&
                (x_strcmp( Metric.Info.Category, pCategory ) == 0) &&
                (x_strcmp( Metric.Info.File, Desc.pFile ? Desc.pFile : "" ) == 0) )
            {
                x_EndAtomic();
                delete[] pSamples;
                return Metric.Info.Id;
            }
        }

        if( MetricCount >= Config.MetricCapacity )
        {
            x_EndAtomic();
            delete[] pSamples;
            return XPROFILE_INVALID_ID;
        }

        const xprofile_id Id = MetricCount;
        metric_state& Metric = pMetrics[Id];
        Metric.Hash                 = Hash;
        Metric.Info.Id              = Id;
        Metric.Info.Type            = Desc.Type;
        Metric.Info.Unit            = Desc.Unit;
        Metric.Info.Line            = Desc.Line;
        Metric.Info.RegisteredFrame = CompletedFrameCount + 1;
        Metric.pSamples             = pSamples;
        CopyString( Metric.Info.Name, XPROFILE_NAME_LENGTH, Desc.pName );
        CopyString( Metric.Info.Category, XPROFILE_CATEGORY_LENGTH, pCategory );
        CopyString( Metric.Info.File, XPROFILE_FILE_LENGTH, Desc.pFile );
        CopyString( Metric.Info.Function, XPROFILE_FUNCTION_LENGTH, Desc.pFunction );
        BuildExternalName( Metric.ExternalName,
                           XPROFILE_EXTERNAL_NAME_LENGTH,
                           Metric.Info.Category,
                           Metric.Info.Name );
        #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
        Metric.NvtxCpuTiming = x_ProfileNvtxCpuZoneEnabled( Metric.ExternalName );
        #endif
        Metric.TracyMetricId = x_ProfileTracyRegisterMetric(
            Metric.ExternalName,
            Metric.Info.File,
            Metric.Info.Function,
            Metric.Info.Line,
            GetCategoryColor( Metric.Info.Category ) );
        MetricCount++;
        x_EndAtomic();
        return Id;
    }

    //---------------------------------------------------------------------

    void RecordDuration( xprofile_id Id, xtick Ticks )
    {
        if( !IsValidMetric( Id ) || (pMetrics[Id].Info.Type != XPROFILE_METRIC_DURATION) )
            return;

        thread_state* pThread = GetThreadState();
        if( !pThread )
            return;

        accumulator& Accumulator = pThread->pAccumulators[Id];
        Accumulator.DurationTicks += MAX( Ticks, (xtick)0 );
        Accumulator.CallCount++;
        Accumulator.Active = TRUE;
    }

    //---------------------------------------------------------------------

    xbool BeginScope( xprofile_id Id )
    {
        if( !IsValidMetric( Id ) || (pMetrics[Id].Info.Type != XPROFILE_METRIC_DURATION) )
            return FALSE;

        thread_state* pThread = GetThreadState();
        if( !pThread || (pThread->ScopeCount >= Config.ScopeDepth) )
            return FALSE;

        active_scope& Scope = pThread->pScopes[pThread->ScopeCount];
        Scope.TracyScope = x_ProfileTracyBeginZone( pMetrics[Id].TracyMetricId );
        #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
        Scope.NvtxRange = x_ProfileNvtxBeginZone( pMetrics[Id].ExternalName,
                                                GetCategoryColor( pMetrics[Id].Info.Category ) );
        #endif
        const xtick Tick = x_GetTime();
        #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
        Scope.BeginCpuNs = pMetrics[Id].NvtxCpuTiming ? x_ProfileNvtxReadThreadCpuTime() : 0;
        #endif
        Scope.Id         = Id;
        Scope.BeginTick  = Tick;
        EmitTimelineEvent( *pThread,
                           XPROFILE_TIMELINE_SCOPE_BEGIN,
                           Id,
                           Tick,
                           (u16)pThread->ScopeCount,
                           0.0 );
        pThread->ScopeCount++;
        return TRUE;
    }

    //---------------------------------------------------------------------

    void EndScope( void )
    {
        // Scope lifetime is independent from frame capture lifetime. In
        // particular, worker scopes may span one or more main-thread frames.
        thread_state* pThread = GetThreadState( FALSE );
        if( !pThread || (pThread->ScopeCount == 0) )
            return;

        #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
        const u64 EndCpuNs = pThread->pScopes[pThread->ScopeCount - 1].BeginCpuNs
                                 ? x_ProfileNvtxReadThreadCpuTime() : 0;
        #endif
        const xtick Tick = x_GetTime();
        const u32 ScopeIndex = --pThread->ScopeCount;
        const active_scope Scope = pThread->pScopes[ScopeIndex];
        x_ProfileTracyEndZone( Scope.TracyScope );
        #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
        x_ProfileNvtxEndZone( Scope.NvtxRange );
        if( Scope.BeginCpuNs && EndCpuNs )
        {
            x_ProfileNvtxCpuTiming( pMetrics[Scope.Id].ExternalName, Scope.BeginCpuNs, EndCpuNs,
                                   x_TicksToMs( Tick - Scope.BeginTick ) );
        }
        #endif

        if( FrameActive )
        {
            accumulator& Accumulator = pThread->pAccumulators[Scope.Id];
            Accumulator.DurationTicks += MAX( Tick - Scope.BeginTick, (xtick)0 );
            Accumulator.CallCount++;
            Accumulator.Active = TRUE;
            EmitTimelineEvent( *pThread,
                               XPROFILE_TIMELINE_SCOPE_END,
                               Scope.Id,
                               Tick,
                               (u16)ScopeIndex,
                               x_TicksToMs( Tick - Scope.BeginTick ) );
        }
    }

    //---------------------------------------------------------------------

    void CounterAdd( xprofile_id Id, f64 Value )
    {
        if( !IsValidMetric( Id ) || (pMetrics[Id].Info.Type != XPROFILE_METRIC_COUNTER) )
            return;

        thread_state* pThread = GetThreadState();
        if( !pThread )
            return;

        accumulator& Accumulator = pThread->pAccumulators[Id];
        Accumulator.CounterValue += Value;
        Accumulator.CallCount++;
        Accumulator.Active = TRUE;
    }

    //---------------------------------------------------------------------

    void GaugeSet( xprofile_id Id, f64 Value )
    {
        if( !IsValidMetric( Id ) || (pMetrics[Id].Info.Type != XPROFILE_METRIC_GAUGE) )
            return;

        thread_state* pThread = GetThreadState();
        if( !pThread )
            return;

        const xtick Tick = x_GetTime();
        accumulator& Accumulator = pThread->pAccumulators[Id];
        Accumulator.GaugeValue     = Value;
        Accumulator.GaugeTimestamp = Tick;
        Accumulator.CallCount++;
        Accumulator.Active = TRUE;
        EmitTimelineEvent( *pThread,
                           XPROFILE_TIMELINE_VALUE,
                           Id,
                           Tick,
                           (u16)pThread->ScopeCount,
                           Value );
    }

    //---------------------------------------------------------------------

    void EventEmit( xprofile_id Id, f64 Value )
    {
        if( !IsValidMetric( Id ) || (pMetrics[Id].Info.Type != XPROFILE_METRIC_EVENT) )
            return;

        thread_state* pThread = GetThreadState();
        if( !pThread )
            return;

        accumulator& Accumulator = pThread->pAccumulators[Id];
        Accumulator.EventCount++;
        Accumulator.CallCount++;
        Accumulator.Active = TRUE;
        EmitTimelineEvent( *pThread,
                           XPROFILE_TIMELINE_EVENT,
                           Id,
                           x_GetTime(),
                           (u16)pThread->ScopeCount,
                           Value );
    }

    //---------------------------------------------------------------------

    xbool GetFrameByNumber( u64 FrameNumber,
                            xprofile_frame_info*& pFrame,
                            u32& FrameIndex ) const
    {
        if( !FrameNumber || (FrameNumber > CompletedFrameCount) ||
            ((CompletedFrameCount - FrameNumber) >= Config.FrameCapacity) )
        {
            return FALSE;
        }

        FrameIndex = (u32)((FrameNumber - 1) % Config.FrameCapacity);
        pFrame = &pFrames[FrameIndex];
        return pFrame->FrameNumber == FrameNumber;
    }

    //---------------------------------------------------------------------

    static s32 CompareF64( const void* pA, const void* pB )
    {
        const f64 A = *(const f64*)pA;
        const f64 B = *(const f64*)pB;
        return (A < B) ? -1 : ((A > B) ? 1 : 0);
    }

    //---------------------------------------------------------------------

    static s32 ComparePendingEvents( const void* pA, const void* pB )
    {
        const pending_event& A = *(const pending_event*)pA;
        const pending_event& B = *(const pending_event*)pB;
        if( A.Timestamp != B.Timestamp )
            return (A.Timestamp < B.Timestamp) ? -1 : 1;
        if( A.ThreadId != B.ThreadId )
            return (A.ThreadId < B.ThreadId) ? -1 : 1;
        return (A.Type < B.Type) ? -1 : ((A.Type > B.Type) ? 1 : 0);
    }

    //---------------------------------------------------------------------

    static const char* GetUnitName( xprofile_unit Unit )
    {
        switch( Unit )
        {
            case XPROFILE_UNIT_MILLISECONDS: return "ms";
            case XPROFILE_UNIT_BYTES:        return "bytes";
            case XPROFILE_UNIT_COUNT:        return "count";
            case XPROFILE_UNIT_PERCENT:      return "%";
            default:                         return "value";
        }
    }
};

//==============================================================================
//  PUBLIC VALUE TYPES
//==============================================================================

xprofile_config::xprofile_config( void ) :
    FrameCapacity           ( 2048 ),
    MetricCapacity          ( 1024 ),
    PropertyCapacity        ( 32 ),
    ThreadCapacity          ( 16 ),
    ScopeDepth              ( 64 ),
    EventsPerThread         ( 2048 ),
    TimelineEventCapacity   ( 65536 ),
    AutoReportFrameInterval ( 0 ),
    StutterThresholdMs      ( 0.0 ),
#if X_PROFILE
    CaptureMode             ( XPROFILE_CAPTURE_SUMMARY ),
#else
    CaptureMode             ( XPROFILE_CAPTURE_OFF ),
#endif
    FineTimingEnabled       ( FALSE )
{
}

//==============================================================================

xprofile_metric_desc::xprofile_metric_desc( void ) :
    pName    ( NULL ),
    pCategory( NULL ),
    Type     ( XPROFILE_METRIC_COUNTER ),
    Unit     ( XPROFILE_UNIT_NONE ),
    pFile    ( NULL ),
    pFunction( NULL ),
    Line     ( 0 )
{
}

//==============================================================================

xprofile_metric_desc::xprofile_metric_desc( const char* pMetricName,
                                             const char* pMetricCategory,
                                             xprofile_metric_type MetricType,
                                             xprofile_unit MetricUnit ) :
    pName    ( pMetricName ),
    pCategory( pMetricCategory ),
    Type     ( MetricType ),
    Unit     ( MetricUnit ),
    pFile    ( NULL ),
    pFunction( NULL ),
    Line     ( 0 )
{
}

//==============================================================================

xprofile_token::xprofile_token( void )
{
    x_AtomicInit( &m_Resolved, 0 );
}

//==============================================================================

xprofile_token::xprofile_token( const xprofile_metric_desc& Desc ) :
    m_Desc( Desc )
{
    x_AtomicInit( &m_Resolved, 0 );
}

//==============================================================================

xprofile_token::xprofile_token( const xprofile_token& Token ) :
    m_Desc( Token.m_Desc )
{
    x_AtomicInit( &m_Resolved, x_AtomicLoadAcquire( &Token.m_Resolved ) );
}

//==============================================================================

xprofile_token& xprofile_token::operator=( const xprofile_token& Token )
{
    if( this != &Token )
    {
        m_Desc = Token.m_Desc;
        x_AtomicStoreRelease( &m_Resolved, x_AtomicLoadAcquire( &Token.m_Resolved ) );
    }
    return *this;
}

//==============================================================================

xprofile_zone::xprofile_zone( void )
{
}

//==============================================================================

xprofile_zone::xprofile_zone( const xprofile_token& Token ) :
    m_Token( Token )
{
}

//==============================================================================

void xprofile_zone::Record( xtick Ticks ) const
{
    x_GetProfiler().RecordDuration( m_Token, Ticks );
}

//==============================================================================

xprofile_counter::xprofile_counter( void )
{
}

//==============================================================================

xprofile_counter::xprofile_counter( const xprofile_token& Token ) :
    m_Token( Token )
{
}

//==============================================================================

void xprofile_counter::Add( f64 Value ) const
{
    x_GetProfiler().CounterAdd( m_Token, Value );
}

//==============================================================================

xprofile_gauge::xprofile_gauge( void )
{
}

//==============================================================================

xprofile_gauge::xprofile_gauge( const xprofile_token& Token ) :
    m_Token( Token )
{
}

//==============================================================================

void xprofile_gauge::Set( f64 Value ) const
{
    x_GetProfiler().GaugeSet( m_Token, Value );
}

//==============================================================================

xprofile_event::xprofile_event( void )
{
}

//==============================================================================

xprofile_event::xprofile_event( const xprofile_token& Token ) :
    m_Token( Token )
{
}

//==============================================================================

void xprofile_event::Emit( f64 Value ) const
{
    x_GetProfiler().EventEmit( m_Token, Value );
}

//==============================================================================

xprofile_sink::xprofile_sink( void ) :
    m_pOwner( NULL )
{
}

//==============================================================================

xprofile_sink::~xprofile_sink( void )
{
    Detach();
}

//==============================================================================

xbool xprofile_sink::Attach( void )
{
    if( m_pOwner )
        return FALSE;

    xprofiler& Profiler = x_GetProfiler();
    m_pOwner = &Profiler;
    if( !Profiler.AttachSink( *this ) )
    {
        m_pOwner = NULL;
        return FALSE;
    }
    return TRUE;
}

//==============================================================================

void xprofile_sink::Detach( void )
{
    if( !m_pOwner )
        return;

    xprofiler* pOwner = m_pOwner;
    m_pOwner = NULL;
    pOwner->DetachSink( *this );
}

//==============================================================================

xbool xprofile_sink::IsAttached( void ) const
{
    return m_pOwner != NULL;
}

//==============================================================================
//  LIFETIME AND REGISTRATION
//==============================================================================

xprofiler::xprofiler( void ) :
    m_pImplementation( NULL )
{
}

//==============================================================================

xprofiler::~xprofiler( void )
{
    Kill();
}

//==============================================================================

xprofiler& x_GetProfiler( void )
{
    static xprofiler s_Profiler;
    return s_Profiler;
}

//==============================================================================

void xprofiler::Init( const xprofile_config* pConfig )
{
#if !X_PROFILE
    (void)pConfig;
    return;
#else
    if( m_pImplementation )
        return;

    #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
    x_ProfileNvtxInit();
    #endif

    xprofile_config Config = pConfig ? *pConfig : xprofile_config();
    Config.FrameCapacity         = MAX( Config.FrameCapacity,         1u );
    Config.MetricCapacity        = MAX( Config.MetricCapacity,        1u );
    Config.PropertyCapacity      = MAX( Config.PropertyCapacity,      1u );
    Config.ThreadCapacity        = MAX( Config.ThreadCapacity,        1u );
    Config.ScopeDepth            = MAX( Config.ScopeDepth,            1u );
    Config.EventsPerThread       = MAX( Config.EventsPerThread,       1u );
    Config.TimelineEventCapacity = MAX( Config.TimelineEventCapacity, 1u );

    implementation* pImpl = new implementation( Config, ++s_ProfileGeneration );
    pImpl->pMetrics = new implementation::metric_state[Config.MetricCapacity];
    pImpl->pProperties = new xprofile_property_info[Config.PropertyCapacity];
    pImpl->pThreads = new implementation::thread_state[Config.ThreadCapacity];
    pImpl->pThreadAccumulators = new implementation::accumulator[
        Config.ThreadCapacity * Config.MetricCapacity];
    pImpl->pThreadScopes = new implementation::active_scope[
        Config.ThreadCapacity * Config.ScopeDepth];
    pImpl->pThreadEvents = new implementation::pending_event[
        Config.ThreadCapacity * Config.EventsPerThread];
    pImpl->pMergedEvents = new implementation::pending_event[
        Config.ThreadCapacity * Config.EventsPerThread];
    pImpl->pFrames = new xprofile_frame_info[Config.FrameCapacity];
    pImpl->pTimelineEvents = new xprofile_timeline_event[Config.TimelineEventCapacity];
    pImpl->pCaptureMetrics = new xprofile_capture_metric[Config.MetricCapacity];
    pImpl->pCaptureThreads = new xprofile_thread_info[Config.ThreadCapacity];
    pImpl->pCaptureProperties = new xprofile_property_info[Config.PropertyCapacity];
    pImpl->pCaptureEvents = new xprofile_timeline_event[
        Config.ThreadCapacity * Config.EventsPerThread];

    x_memset( pImpl->pMetrics, 0,
              sizeof(implementation::metric_state) * Config.MetricCapacity );
    x_memset( pImpl->pProperties, 0,
              sizeof(xprofile_property_info) * Config.PropertyCapacity );
    x_memset( pImpl->pThreads, 0,
              sizeof(implementation::thread_state) * Config.ThreadCapacity );
    x_memset( pImpl->pThreadAccumulators, 0,
              sizeof(implementation::accumulator) * Config.ThreadCapacity * Config.MetricCapacity );
    x_memset( pImpl->pThreadScopes, 0,
              sizeof(implementation::active_scope) * Config.ThreadCapacity * Config.ScopeDepth );
    x_memset( pImpl->pThreadEvents, 0,
              sizeof(implementation::pending_event) * Config.ThreadCapacity * Config.EventsPerThread );
    x_memset( pImpl->pMergedEvents, 0,
              sizeof(implementation::pending_event) * Config.ThreadCapacity * Config.EventsPerThread );
    x_memset( pImpl->pFrames, 0,
              sizeof(xprofile_frame_info) * Config.FrameCapacity );
    x_memset( pImpl->pTimelineEvents, 0,
              sizeof(xprofile_timeline_event) * Config.TimelineEventCapacity );

    m_pImplementation = pImpl;
    xprofile_metric_desc Desc( "FrameCPU",
                               "Frame",
                               XPROFILE_METRIC_DURATION,
                               XPROFILE_UNIT_MILLISECONDS );
    pImpl->FrameDurationMetric = pImpl->RegisterMetric( Desc );
#endif
}

//==============================================================================

void xprofiler::Kill( void )
{
    if( !m_pImplementation )
        return;

    m_pImplementation->FrameActive = FALSE;
    for( u32 i = 0; i < m_pImplementation->SinkCount; ++i )
        m_pImplementation->Sinks[i]->m_pOwner = NULL;
    m_pImplementation->SinkCount = 0;
    delete m_pImplementation;
    m_pImplementation = NULL;
    ++s_ProfileGeneration;
}

//==============================================================================

xbool xprofiler::IsInitialized( void ) const
{
    return m_pImplementation != NULL;
}

//==============================================================================

xprofile_id xprofiler::ResolveToken( const xprofile_token& Token )
{
    if( !m_pImplementation )
        return XPROFILE_INVALID_ID;

    const u64 Resolved = x_AtomicLoadAcquire( &Token.m_Resolved );
    const u32 Generation = (u32)(Resolved >> 32);
    const u32 StoredId = (u32)Resolved;
    if( (Generation == m_pImplementation->Generation) && StoredId )
        return StoredId - 1;

    const xprofile_id Id = m_pImplementation->RegisterMetric( Token.m_Desc );
    if( Id != XPROFILE_INVALID_ID )
    {
        const u64 Packed = ((u64)m_pImplementation->Generation << 32) | ((u64)Id + 1);
        x_AtomicStoreRelease( &Token.m_Resolved, Packed );
    }
    return Id;
}

//==============================================================================

xprofile_zone xprofiler::RegisterZone( const char* pName, const char* pCategory )
{
    return RegisterZoneAt( pName, pCategory, NULL, NULL, 0 );
}

//==============================================================================

xprofile_zone xprofiler::RegisterZoneAt( const char* pName,
                                         const char* pCategory,
                                         const char* pFile,
                                         const char* pFunction,
                                         u32 Line )
{
    xprofile_metric_desc Desc( pName,
                               pCategory,
                               XPROFILE_METRIC_DURATION,
                               XPROFILE_UNIT_MILLISECONDS );
    Desc.pFile     = pFile;
    Desc.pFunction = pFunction;
    Desc.Line      = Line;
    xprofile_token Token( Desc );
    ResolveToken( Token );
    return xprofile_zone( Token );
}

//==============================================================================

xprofile_counter xprofiler::RegisterCounter( const char* pName, const char* pCategory )
{
    const xprofile_metric_desc Desc( pName,
                                     pCategory,
                                     XPROFILE_METRIC_COUNTER,
                                     XPROFILE_UNIT_COUNT );
    xprofile_token Token( Desc );
    ResolveToken( Token );
    return xprofile_counter( Token );
}

//==============================================================================

xprofile_gauge xprofiler::RegisterGauge( const char* pName,
                                         xprofile_unit Unit,
                                         const char* pCategory )
{
    const xprofile_metric_desc Desc( pName,
                                     pCategory,
                                     XPROFILE_METRIC_GAUGE,
                                     Unit );
    xprofile_token Token( Desc );
    ResolveToken( Token );
    return xprofile_gauge( Token );
}

//==============================================================================

xprofile_event xprofiler::RegisterEvent( const char* pName, const char* pCategory )
{
    const xprofile_metric_desc Desc( pName,
                                     pCategory,
                                     XPROFILE_METRIC_EVENT,
                                     XPROFILE_UNIT_COUNT );
    xprofile_token Token( Desc );
    ResolveToken( Token );
    return xprofile_event( Token );
}

//==============================================================================
//  SETTINGS
//==============================================================================

void xprofiler::SetCaptureMode( xprofile_capture_mode Mode )
{
    if( !m_pImplementation )
        return;
    if( (Mode == XPROFILE_CAPTURE_OFF) && m_pImplementation->FrameActive )
        CancelFrame();
    m_pImplementation->CaptureMode = Mode;
}

//==============================================================================

xprofile_capture_mode xprofiler::GetCaptureMode( void ) const
{
    return m_pImplementation ? m_pImplementation->CaptureMode : XPROFILE_CAPTURE_OFF;
}

//==============================================================================

xbool xprofiler::IsFrameActive( void ) const
{
    return m_pImplementation && m_pImplementation->FrameActive &&
           (m_pImplementation->CaptureMode != XPROFILE_CAPTURE_OFF);
}

//==============================================================================

void xprofiler::SetFineTimingEnabled( xbool Enabled )
{
    if( m_pImplementation )
        m_pImplementation->FineTimingEnabled = Enabled;
}

//==============================================================================

xbool xprofiler::IsFineTimingEnabled( void ) const
{
    return IsFrameActive() && m_pImplementation->FineTimingEnabled;
}

//==============================================================================

void xprofiler::SetAutoReportInterval( u32 FrameInterval )
{
    if( m_pImplementation )
        m_pImplementation->Config.AutoReportFrameInterval = FrameInterval;
}

//==============================================================================

void xprofiler::SetStutterThresholdMs( f64 ThresholdMs )
{
    if( m_pImplementation )
        m_pImplementation->Config.StutterThresholdMs = MAX( ThresholdMs, 0.0 );
}

//==============================================================================

void xprofiler::SetProperty( const char* pName, const char* pValue )
{
    implementation* pImpl = m_pImplementation;
    if( !pImpl || !pName || !pName[0] )
        return;

    x_BeginAtomic();
    for( u32 i = 0; i < pImpl->PropertyCount; ++i )
    {
        if( x_strcmp( pImpl->pProperties[i].Name, pName ) == 0 )
        {
            implementation::CopyString( pImpl->pProperties[i].Value,
                                        XPROFILE_VALUE_LENGTH,
                                        pValue );
            x_EndAtomic();
            return;
        }
    }

    if( pImpl->PropertyCount < pImpl->Config.PropertyCapacity )
    {
        xprofile_property_info& Info = pImpl->pProperties[pImpl->PropertyCount++];
        implementation::CopyString( Info.Name, XPROFILE_NAME_LENGTH, pName );
        implementation::CopyString( Info.Value, XPROFILE_VALUE_LENGTH, pValue );
    }
    x_EndAtomic();
}

//==============================================================================
//  FRAME RECORDING
//==============================================================================

void xprofiler::BeginFrame( void )
{
    implementation* pImpl = m_pImplementation;
    if( !pImpl )
        return;

    x_BeginAtomic();
    if( (pImpl->CaptureMode == XPROFILE_CAPTURE_OFF) || pImpl->FrameActive )
    {
        x_EndAtomic();
        return;
    }
    pImpl->CurrentFrameNumber = pImpl->CompletedFrameCount + 1;
    pImpl->FrameBeginTick     = x_GetTime();
    pImpl->FrameActive = TRUE;
    x_EndAtomic();
}

//==============================================================================

void xprofiler::EndFrame( void )
{
    if( !IsFrameActive() )
        return;

    implementation* pImpl = m_pImplementation;
    const xtick EndTick = x_GetTime();
    x_ProfileTracyFrameMark();
    #if defined( X_PROFILE_NVTX ) && X_PROFILE_NVTX
    x_ProfileNvtxFrame( pImpl->CurrentFrameNumber );
    #endif
    pImpl->RecordDuration( pImpl->FrameDurationMetric,
                           EndTick - pImpl->FrameBeginTick );
    const u32 FrameIndex = (u32)((pImpl->CurrentFrameNumber - 1) %
                                 pImpl->Config.FrameCapacity);

    for( u32 MetricIndex = 0; MetricIndex < pImpl->MetricCount; ++MetricIndex )
    {
        implementation::metric_state& Metric = pImpl->pMetrics[MetricIndex];
        xprofile_metric_sample& Sample = Metric.pSamples[FrameIndex];
        Sample.FrameNumber = pImpl->CurrentFrameNumber;
        Sample.Value       = 0.0;
        Sample.CallCount   = 0;
        Sample.Active      = FALSE;

        xtick LatestGaugeTimestamp = 0;
        for( u32 ThreadIndex = 0; ThreadIndex < pImpl->ThreadCount; ++ThreadIndex )
        {
            implementation::thread_state& Thread = pImpl->pThreads[ThreadIndex];
            if( Thread.FrameNumber != pImpl->CurrentFrameNumber )
                continue;

            const implementation::accumulator& Accumulator =
                Thread.pAccumulators[MetricIndex];
            if( !Accumulator.Active )
                continue;

            Sample.Active = TRUE;
            Sample.CallCount += Accumulator.CallCount;
            switch( Metric.Info.Type )
            {
                case XPROFILE_METRIC_DURATION:
                    Sample.Value += x_TicksToMs( Accumulator.DurationTicks );
                    break;
                case XPROFILE_METRIC_COUNTER:
                    Sample.Value += Accumulator.CounterValue;
                    break;
                case XPROFILE_METRIC_GAUGE:
                    if( Accumulator.GaugeTimestamp >= LatestGaugeTimestamp )
                    {
                        LatestGaugeTimestamp = Accumulator.GaugeTimestamp;
                        Sample.Value = Accumulator.GaugeValue;
                    }
                    break;
                case XPROFILE_METRIC_EVENT:
                    Sample.Value += Accumulator.EventCount;
                    break;
            }
        }

        if( Sample.Active && (Metric.Info.Type != XPROFILE_METRIC_DURATION) )
            x_ProfileTracyPlot( Metric.TracyMetricId, Sample.Value );
    }

    xprofile_frame_info& Frame = pImpl->pFrames[FrameIndex];
    Frame.FrameNumber               = pImpl->CurrentFrameNumber;
    Frame.BeginTick                 = pImpl->FrameBeginTick;
    Frame.EndTick                   = EndTick;
    Frame.DurationMs                = x_TicksToMs( EndTick - pImpl->FrameBeginTick );
    Frame.FirstTimelineSequence     = pImpl->NextTimelineSequence;
    Frame.TimelineEventCount        = 0;
    Frame.DroppedTimelineEventCount = 0;
    Frame.Stutter = (pImpl->Config.StutterThresholdMs > 0.0) &&
                    (Frame.DurationMs >= pImpl->Config.StutterThresholdMs);

    u32 MergedEventCount = 0;
    for( u32 ThreadIndex = 0; ThreadIndex < pImpl->ThreadCount; ++ThreadIndex )
    {
        implementation::thread_state& Thread = pImpl->pThreads[ThreadIndex];
        if( Thread.FrameNumber != pImpl->CurrentFrameNumber )
            continue;

        Frame.DroppedTimelineEventCount += Thread.DroppedEventCount;
        for( u32 EventIndex = 0; EventIndex < Thread.EventCount; ++EventIndex )
            pImpl->pMergedEvents[MergedEventCount++] = Thread.pEvents[EventIndex];
    }

    x_qsort( pImpl->pMergedEvents,
             MergedEventCount,
             sizeof(implementation::pending_event),
             implementation::ComparePendingEvents );
    for( u32 EventIndex = 0; EventIndex < MergedEventCount; ++EventIndex )
    {
        const implementation::pending_event& Pending = pImpl->pMergedEvents[EventIndex];
        const u64 Sequence = pImpl->NextTimelineSequence++;
        xprofile_timeline_event& Event = pImpl->pTimelineEvents[
            Sequence % pImpl->Config.TimelineEventCapacity];
        Event.Sequence    = Sequence;
        Event.FrameNumber = pImpl->CurrentFrameNumber;
        Event.Timestamp   = Pending.Timestamp;
        Event.ThreadId    = Pending.ThreadId;
        Event.Depth       = Pending.Depth;
        Event.Type        = Pending.Type;
        Event.MetricId    = Pending.MetricId;
        Event.Value       = Pending.Value;
        pImpl->pCaptureEvents[EventIndex] = Event;
        Frame.TimelineEventCount++;
    }

    xprofile_sink* CaptureSinks[XPROFILE_MAX_SINKS];
    u32 CaptureSinkCount;
    x_BeginAtomic();
    pImpl->CompletedFrameCount = pImpl->CurrentFrameNumber;
    pImpl->FrameActive = FALSE;
    CaptureSinkCount = pImpl->SinkCount;
    for( u32 i = 0; i < CaptureSinkCount; ++i )
        CaptureSinks[i] = pImpl->Sinks[i];
    x_EndAtomic();

    for( u32 i = 0; i < pImpl->MetricCount; ++i )
    {
        pImpl->pCaptureMetrics[i].Info   = pImpl->pMetrics[i].Info;
        pImpl->pCaptureMetrics[i].Sample = pImpl->pMetrics[i].pSamples[FrameIndex];
    }
    for( u32 i = 0; i < pImpl->ThreadCount; ++i )
    {
        pImpl->pCaptureThreads[i].ThreadId = pImpl->pThreads[i].ThreadId;
        implementation::CopyString( pImpl->pCaptureThreads[i].Name,
                                    XPROFILE_NAME_LENGTH,
                                    pImpl->pThreads[i].Name );
    }
    for( u32 i = 0; i < pImpl->PropertyCount; ++i )
        pImpl->pCaptureProperties[i] = pImpl->pProperties[i];

    xprofile_capture Capture;
    Capture.Generation         = pImpl->Generation;
    Capture.Frame              = Frame;
    Capture.pMetrics           = pImpl->pCaptureMetrics;
    Capture.MetricCount        = pImpl->MetricCount;
    Capture.pThreads           = pImpl->pCaptureThreads;
    Capture.ThreadCount        = pImpl->ThreadCount;
    Capture.pProperties        = pImpl->pCaptureProperties;
    Capture.PropertyCount      = pImpl->PropertyCount;
    Capture.pTimelineEvents    = pImpl->pCaptureEvents;
    Capture.TimelineEventCount = MergedEventCount;
    for( u32 i = 0; i < CaptureSinkCount; ++i )
        CaptureSinks[i]->OnCapture( Capture );

    const u32 ReportInterval = pImpl->Config.AutoReportFrameInterval;
    if( ReportInterval && ((pImpl->CompletedFrameCount % ReportInterval) == 0) )
        DumpReport( ReportInterval );
}

//==============================================================================

void xprofiler::CancelFrame( void )
{
    implementation* pImpl = m_pImplementation;
    if( !pImpl || !pImpl->FrameActive )
        return;

    for( u32 i = 0; i < pImpl->ThreadCount; ++i )
        pImpl->ResetThreadForFrame( pImpl->pThreads[i], 0 );
    pImpl->FrameActive        = FALSE;
    pImpl->CurrentFrameNumber = pImpl->CompletedFrameCount;
    pImpl->FrameBeginTick     = 0;
}

//==============================================================================

xbool xprofiler::BeginScope( const xprofile_zone& Zone )
{
    if( !IsFrameActive() )
        return FALSE;
    const xprofile_id Id = ResolveToken( Zone.m_Token );
    return (Id != XPROFILE_INVALID_ID) && m_pImplementation->BeginScope( Id );
}

//==============================================================================

void xprofiler::EndScope( void )
{
    if( m_pImplementation )
        m_pImplementation->EndScope();
}

//==============================================================================

void xprofiler::RecordDuration( const xprofile_token& Token, xtick Ticks )
{
    if( !IsFrameActive() )
        return;
    const xprofile_id Id = ResolveToken( Token );
    if( Id != XPROFILE_INVALID_ID )
        m_pImplementation->RecordDuration( Id, Ticks );
}

//==============================================================================

void xprofiler::CounterAdd( const xprofile_token& Token, f64 Value )
{
    if( !IsFrameActive() )
        return;
    const xprofile_id Id = ResolveToken( Token );
    if( Id != XPROFILE_INVALID_ID )
        m_pImplementation->CounterAdd( Id, Value );
}

//==============================================================================

void xprofiler::GaugeSet( const xprofile_token& Token, f64 Value )
{
    if( !IsFrameActive() )
        return;
    const xprofile_id Id = ResolveToken( Token );
    if( Id != XPROFILE_INVALID_ID )
        m_pImplementation->GaugeSet( Id, Value );
}

//==============================================================================

void xprofiler::EventEmit( const xprofile_token& Token, f64 Value )
{
    if( !IsFrameActive() )
        return;
    const xprofile_id Id = ResolveToken( Token );
    if( Id != XPROFILE_INVALID_ID )
        m_pImplementation->EventEmit( Id, Value );
}

//==============================================================================

void xprofiler::RecordDynamicDuration( const char* pName,
                                       xtick Ticks,
                                       const char* pCategory )
{
    RegisterZone( pName, pCategory ).Record( Ticks );
}

//==============================================================================

void xprofiler::AddDynamicCounter( const char* pName,
                                   f64 Value,
                                   const char* pCategory )
{
    RegisterCounter( pName, pCategory ).Add( Value );
}

//==============================================================================
//  SINKS
//==============================================================================

xbool xprofiler::AttachSink( xprofile_sink& Sink )
{
    implementation* pImpl = m_pImplementation;
    if( !pImpl || pImpl->FrameActive || (pImpl->SinkCount >= XPROFILE_MAX_SINKS) )
        return FALSE;

    x_BeginAtomic();
    if( pImpl->FrameActive || (pImpl->SinkCount >= XPROFILE_MAX_SINKS) )
    {
        x_EndAtomic();
        return FALSE;
    }
    for( u32 i = 0; i < pImpl->SinkCount; ++i )
    {
        if( pImpl->Sinks[i] == &Sink )
        {
            x_EndAtomic();
            return FALSE;
        }
    }
    pImpl->Sinks[pImpl->SinkCount++] = &Sink;
    x_EndAtomic();
    return TRUE;
}

//==============================================================================

void xprofiler::DetachSink( xprofile_sink& Sink )
{
    implementation* pImpl = m_pImplementation;
    if( !pImpl || pImpl->FrameActive )
        return;

    x_BeginAtomic();
    if( pImpl->FrameActive )
    {
        x_EndAtomic();
        return;
    }
    for( u32 i = 0; i < pImpl->SinkCount; ++i )
    {
        if( pImpl->Sinks[i] != &Sink )
            continue;
        for( u32 j = i + 1; j < pImpl->SinkCount; ++j )
            pImpl->Sinks[j - 1] = pImpl->Sinks[j];
        pImpl->SinkCount--;
        x_EndAtomic();
        return;
    }
    x_EndAtomic();
}

//==============================================================================
//  SNAPSHOTS
//==============================================================================

xprofile_snapshot::xprofile_snapshot( void ) :
    m_pOwner             ( NULL ),
    m_Generation         ( 0 ),
    m_CompletedFrameCount( 0 ),
    m_MetricCount        ( 0 ),
    m_ThreadCount        ( 0 ),
    m_PropertyCount      ( 0 )
{
}

//==============================================================================

xprofile_snapshot::xprofile_snapshot( const xprofiler* pOwner,
                                      u32 Generation,
                                      u64 CompletedFrameCount,
                                      u32 MetricCount,
                                      u32 ThreadCount,
                                      u32 PropertyCount ) :
    m_pOwner             ( pOwner ),
    m_Generation         ( Generation ),
    m_CompletedFrameCount( CompletedFrameCount ),
    m_MetricCount        ( MetricCount ),
    m_ThreadCount        ( ThreadCount ),
    m_PropertyCount      ( PropertyCount )
{
}

//==============================================================================

xprofile_snapshot xprofiler::AcquireSnapshot( void ) const
{
    if( !m_pImplementation )
        return xprofile_snapshot();

    x_BeginAtomic();
    const xprofile_snapshot Snapshot( this,
                                      m_pImplementation->Generation,
                                      m_pImplementation->CompletedFrameCount,
                                      m_pImplementation->MetricCount,
                                      m_pImplementation->ThreadCount,
                                      m_pImplementation->PropertyCount );
    x_EndAtomic();
    return Snapshot;
}

//==============================================================================

xbool xprofiler::IsSnapshotValid( const xprofile_snapshot& Snapshot ) const
{
    return m_pImplementation &&
           (Snapshot.m_pOwner == this) &&
           (Snapshot.m_Generation == m_pImplementation->Generation);
}

//==============================================================================

xbool xprofile_snapshot::IsValid( void ) const
{
    return m_pOwner && m_pOwner->IsSnapshotValid( *this );
}

//==============================================================================

u64 xprofile_snapshot::GetCompletedFrameCount( void ) const
{
    return IsValid() ? m_CompletedFrameCount : 0;
}

//==============================================================================

u32 xprofiler::GetStoredFrameCount( const xprofile_snapshot& Snapshot ) const
{
    if( !IsSnapshotValid( Snapshot ) )
        return 0;
    return (u32)MIN( Snapshot.m_CompletedFrameCount,
                     (u64)m_pImplementation->Config.FrameCapacity );
}

//==============================================================================

u32 xprofile_snapshot::GetStoredFrameCount( void ) const
{
    return m_pOwner ? m_pOwner->GetStoredFrameCount( *this ) : 0;
}

//==============================================================================

u32 xprofile_snapshot::GetMetricCount( void ) const
{
    return IsValid() ? m_MetricCount : 0;
}

//==============================================================================

u32 xprofile_snapshot::GetThreadCount( void ) const
{
    return IsValid() ? m_ThreadCount : 0;
}

//==============================================================================

u32 xprofile_snapshot::GetPropertyCount( void ) const
{
    return IsValid() ? m_PropertyCount : 0;
}

//==============================================================================

xbool xprofiler::GetFrameInfo( const xprofile_snapshot& Snapshot,
                               u32 NewestFrameOffset,
                               xprofile_frame_info& Info ) const
{
    if( !IsSnapshotValid( Snapshot ) ||
        (NewestFrameOffset >= GetStoredFrameCount( Snapshot )) )
    {
        return FALSE;
    }

    xprofile_frame_info* pFrame = NULL;
    u32 FrameIndex = 0;
    const u64 FrameNumber = Snapshot.m_CompletedFrameCount - NewestFrameOffset;
    if( !m_pImplementation->GetFrameByNumber( FrameNumber, pFrame, FrameIndex ) )
        return FALSE;
    Info = *pFrame;
    return TRUE;
}

//==============================================================================

xbool xprofile_snapshot::GetFrameInfo( u32 NewestFrameOffset,
                                       xprofile_frame_info& Info ) const
{
    return m_pOwner && m_pOwner->GetFrameInfo( *this, NewestFrameOffset, Info );
}

//==============================================================================

xbool xprofiler::GetThreadInfo( const xprofile_snapshot& Snapshot,
                                u32 ThreadIndex,
                                xprofile_thread_info& Info ) const
{
    if( !IsSnapshotValid( Snapshot ) || (ThreadIndex >= Snapshot.m_ThreadCount) )
        return FALSE;
    Info.ThreadId = m_pImplementation->pThreads[ThreadIndex].ThreadId;
    implementation::CopyString( Info.Name,
                                XPROFILE_NAME_LENGTH,
                                m_pImplementation->pThreads[ThreadIndex].Name );
    return TRUE;
}

//==============================================================================

xbool xprofile_snapshot::GetThreadInfo( u32 ThreadIndex,
                                        xprofile_thread_info& Info ) const
{
    return m_pOwner && m_pOwner->GetThreadInfo( *this, ThreadIndex, Info );
}

//==============================================================================

xbool xprofiler::GetPropertyInfo( const xprofile_snapshot& Snapshot,
                                  u32 PropertyIndex,
                                  xprofile_property_info& Info ) const
{
    if( !IsSnapshotValid( Snapshot ) || (PropertyIndex >= Snapshot.m_PropertyCount) )
        return FALSE;
    Info = m_pImplementation->pProperties[PropertyIndex];
    return TRUE;
}

//==============================================================================

xbool xprofile_snapshot::GetPropertyInfo( u32 PropertyIndex,
                                          xprofile_property_info& Info ) const
{
    return m_pOwner && m_pOwner->GetPropertyInfo( *this, PropertyIndex, Info );
}

//==============================================================================

xbool xprofiler::GetMetricInfo( const xprofile_snapshot& Snapshot,
                                xprofile_id Id,
                                xprofile_metric_info& Info ) const
{
    if( !IsSnapshotValid( Snapshot ) || (Id >= Snapshot.m_MetricCount) )
        return FALSE;
    Info = m_pImplementation->pMetrics[Id].Info;
    return TRUE;
}

//==============================================================================

xbool xprofile_snapshot::GetMetricInfo( xprofile_id Id,
                                        xprofile_metric_info& Info ) const
{
    return m_pOwner && m_pOwner->GetMetricInfo( *this, Id, Info );
}

//==============================================================================

xbool xprofiler::GetMetricSample( const xprofile_snapshot& Snapshot,
                                  xprofile_id Id,
                                  u32 NewestFrameOffset,
                                  xprofile_metric_sample& Sample ) const
{
    if( !IsSnapshotValid( Snapshot ) || (Id >= Snapshot.m_MetricCount) ||
        (NewestFrameOffset >= GetStoredFrameCount( Snapshot )) )
    {
        return FALSE;
    }

    xprofile_frame_info* pFrame = NULL;
    u32 FrameIndex = 0;
    const u64 FrameNumber = Snapshot.m_CompletedFrameCount - NewestFrameOffset;
    if( !m_pImplementation->GetFrameByNumber( FrameNumber, pFrame, FrameIndex ) )
        return FALSE;

    const xprofile_metric_sample& StoredSample =
        m_pImplementation->pMetrics[Id].pSamples[FrameIndex];
    if( StoredSample.FrameNumber != FrameNumber )
        return FALSE;
    Sample = StoredSample;
    return TRUE;
}

//==============================================================================

xbool xprofile_snapshot::GetMetricSample( xprofile_id Id,
                                          u32 NewestFrameOffset,
                                          xprofile_metric_sample& Sample ) const
{
    return m_pOwner &&
           m_pOwner->GetMetricSample( *this, Id, NewestFrameOffset, Sample );
}

//==============================================================================

xbool xprofiler::GetMetricStatistics( const xprofile_snapshot& Snapshot,
                                      xprofile_id Id,
                                      u32 NewestFrameCount,
                                      xprofile_statistics& Statistics ) const
{
    x_memset( &Statistics, 0, sizeof(Statistics) );
    if( !IsSnapshotValid( Snapshot ) || (Id >= Snapshot.m_MetricCount) )
        return FALSE;

    const u32 StoredFrameCount = GetStoredFrameCount( Snapshot );
    const u32 RequestedCount = NewestFrameCount
                             ? MIN( NewestFrameCount, StoredFrameCount )
                             : StoredFrameCount;
    if( RequestedCount == 0 )
        return FALSE;

    f64* pDistributionValues = new f64[RequestedCount];
    f64 Total = 0.0;
    f64 ActiveTotal = 0.0;
    u32 DistributionCount = 0;
    const xprofile_metric_type MetricType = m_pImplementation->pMetrics[Id].Info.Type;
    for( u32 Offset = 0; Offset < RequestedCount; ++Offset )
    {
        xprofile_metric_sample Sample;
        if( !GetMetricSample( Snapshot, Id, Offset, Sample ) )
            continue;

        Statistics.SampleCount++;
        Statistics.TotalCallCount += Sample.CallCount;
        Total += Sample.Value;
        if( Sample.Active )
        {
            Statistics.ActiveSampleCount++;
            ActiveTotal += Sample.Value;
        }
        if( Sample.Active ||
            (MetricType == XPROFILE_METRIC_COUNTER) ||
            (MetricType == XPROFILE_METRIC_EVENT) )
        {
            pDistributionValues[DistributionCount++] = Sample.Value;
        }
    }

    if( Statistics.SampleCount )
    {
        Statistics.CallsPerSample = (f64)Statistics.TotalCallCount /
                                    (f64)Statistics.SampleCount;
        Statistics.Average = Total / Statistics.SampleCount;
    }
    if( Statistics.ActiveSampleCount )
    {
        Statistics.ActivePercent = 100.0 * (f64)Statistics.ActiveSampleCount /
                                    (f64)Statistics.SampleCount;
        Statistics.CallsPerActiveSample = (f64)Statistics.TotalCallCount /
                                          (f64)Statistics.ActiveSampleCount;
        Statistics.ActiveAverage = ActiveTotal / Statistics.ActiveSampleCount;
    }
    if( DistributionCount )
    {
        x_qsort( pDistributionValues,
                 DistributionCount,
                 sizeof(f64),
                 implementation::CompareF64 );
        const u32 Count = DistributionCount;
        Statistics.Minimum = pDistributionValues[0];
        Statistics.Maximum = pDistributionValues[Count - 1];
        Statistics.Median = (Count & 1)
                          ? pDistributionValues[Count / 2]
                          : 0.5 * (pDistributionValues[(Count / 2) - 1] +
                                   pDistributionValues[Count / 2]);
        Statistics.P95 = pDistributionValues[MAX( ((95 * Count + 99) / 100) - 1, 0u )];
        Statistics.P99 = pDistributionValues[MAX( ((99 * Count + 99) / 100) - 1, 0u )];
    }

    delete[] pDistributionValues;
    return Statistics.SampleCount > 0;
}

//==============================================================================

xbool xprofile_snapshot::GetMetricStatistics( xprofile_id Id,
                                              u32 NewestFrameCount,
                                              xprofile_statistics& Statistics ) const
{
    return m_pOwner &&
           m_pOwner->GetMetricStatistics( *this, Id, NewestFrameCount, Statistics );
}

//==============================================================================

xbool xprofiler::GetTimelineEvent( const xprofile_snapshot& Snapshot,
                                   u64 Sequence,
                                   xprofile_timeline_event& Event ) const
{
    if( !IsSnapshotValid( Snapshot ) || !Sequence ||
        (Sequence >= m_pImplementation->NextTimelineSequence) )
    {
        return FALSE;
    }

    const xprofile_timeline_event& StoredEvent = m_pImplementation->pTimelineEvents[
        Sequence % m_pImplementation->Config.TimelineEventCapacity];
    if( (StoredEvent.Sequence != Sequence) ||
        (StoredEvent.FrameNumber > Snapshot.m_CompletedFrameCount) )
    {
        return FALSE;
    }
    Event = StoredEvent;
    return TRUE;
}

//==============================================================================

xbool xprofile_snapshot::GetTimelineEvent( u64 Sequence,
                                           xprofile_timeline_event& Event ) const
{
    return m_pOwner && m_pOwner->GetTimelineEvent( *this, Sequence, Event );
}

//==============================================================================
//  REPORTING
//==============================================================================

void xprofiler::DumpReport( u32 NewestFrameCount ) const
{
    const xprofile_snapshot Snapshot = AcquireSnapshot();
    if( !Snapshot.IsValid() || !Snapshot.GetCompletedFrameCount() )
        return;

    const u32 FrameCount = NewestFrameCount
                         ? MIN( NewestFrameCount, Snapshot.GetStoredFrameCount() )
                         : Snapshot.GetStoredFrameCount();
    x_DebugMsg( "XProfile: frames=%u stored=%u mode=%d fine-timing=%s\n",
                FrameCount,
                Snapshot.GetStoredFrameCount(),
                (s32)GetCaptureMode(),
                m_pImplementation->FineTimingEnabled ? "on" : "off" );

    for( u32 i = 0; i < Snapshot.GetPropertyCount(); ++i )
    {
        xprofile_property_info Info;
        if( Snapshot.GetPropertyInfo( i, Info ) )
            x_DebugMsg( "XProfile: property %s=%s\n", Info.Name, Info.Value );
    }

    for( u32 i = 0; i < Snapshot.GetMetricCount(); ++i )
    {
        xprofile_metric_info Info;
        xprofile_statistics Stats;
        if( !Snapshot.GetMetricInfo( i, Info ) ||
            !Snapshot.GetMetricStatistics( i, FrameCount, Stats ) )
        {
            continue;
        }

        const f64 ReportAverage = ((Info.Type == XPROFILE_METRIC_COUNTER) ||
                                   (Info.Type == XPROFILE_METRIC_EVENT))
                                ? Stats.Average
                                : Stats.ActiveAverage;
        x_DebugMsg( "XProfile: %-16s %-32s active=%5.1f%% calls/frame=%7.2f avg=%8.4f median=%8.4f p95=%8.4f p99=%8.4f max=%8.4f %s\n",
                    Info.Category,
                    Info.Name,
                    Stats.ActivePercent,
                    Stats.CallsPerSample,
                    ReportAverage,
                    Stats.Median,
                    Stats.P95,
                    Stats.P99,
                    Stats.Maximum,
                    implementation::GetUnitName( Info.Unit ) );
    }
    x_DebugMsg( "XProfile: end\n" );
}

//==============================================================================
//  SCOPED API
//==============================================================================

xprofile_scope::xprofile_scope( const xprofile_zone& Zone ) :
    m_Active( x_GetProfiler().BeginScope( Zone ) )
{
}

//==============================================================================

xprofile_scope::~xprofile_scope( void )
{
    if( m_Active )
        x_GetProfiler().EndScope();
}
