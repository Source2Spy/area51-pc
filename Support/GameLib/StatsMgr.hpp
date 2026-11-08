///////////////////////////////////////////////////////////////////////////////
//
//  stats_mgr
//
//      Keeps up with all performance stats for the game
//
///////////////////////////////////////////////////////////////////////////////

#if !defined(X_RETAIL) || defined(CONFIG_PROFILE)
#define ENABLE_STATS_MGR 1
#else
#define ENABLE_STATS_MGR 0
#endif

///////////////////////////////////////////////////////////////////////////////

#if ENABLE_STATS_MGR

#ifndef STATS_MGR
#define STATS_MGR

#define ENABLE_LOGGING

#include "Obj_mgr/obj_mgr.hpp"

const s32   k_SAMPLE_HISTORY = 120;

enum stat_fields
{
    //  Timer stats
    k_stats_OnAdvance = 0      ,
    k_stats_Debris             ,
    k_stats_AI_Think           ,
    k_stats_AI_Advance         ,
    k_stats_Animation          ,
    k_stats_Collision          ,
    k_stats_Sound              ,
    k_stats_ParticleSystem     ,
    k_stats_DecalSystem        ,
    k_stats_TriggerSystem      ,
    k_stats_HighLevelRender    ,
    k_stats_ObjectRender       ,
    k_stats_OtherRender        ,
    k_stats_Projectiles        ,
    k_stats_Portal             ,
    k_stats_Turret             ,
    k_stats_Physics            ,
    k_stats_Player             ,
    k_stats_CPU_Time           ,
    k_stats_NumTimerStats      ,

    k_stats_FrameTime,
    k_stats_SMem,
    k_stats_Memory,
    k_stats_TotalObjectCount,
    k_stats_TextureMemory,

    k_stats_Last,
};

enum stat_measurement
{
    k_stats_Instant = 0,
    k_stats_Average,
    k_stats_MaxRecent,
    k_stats_MinRecent,
    k_stats_MaxEver,
    k_stats_MinEver,
    k_stats_Measurement_Last,
    k_stats_All
};

#ifdef ENABLE_LOGGING
   
#define LOG_STAT( x )  stat_logger TempStatLoggerVar(x)

#else

#define LOG_STAT( x )  void

#endif

#define STAT_LOGGER(_a_,_b_) stat_logger _a_(_b_)

class stat_logger
{
public:

    enum { MAX_STAT_LEVELS = 8 };
    stat_logger( stat_fields fieldToTrack );
    ~stat_logger();
protected:    
    xtimer        m_Timer;
    stat_fields   m_FieldToTrack;

    static  s32          s_StatLevel;
    static  stat_logger* s_StatStack[MAX_STAT_LEVELS];
};

class stats_mgr
{
public:

                        stats_mgr          ( void );
                       ~stats_mgr          ();

    static stats_mgr*   GetStatsMgr( void )
    {
        if( !s_This )
            s_This = new stats_mgr;

        return s_This;
    }

    //-------------------------------------------------------------------------
    //  SAMPLING
    //-------------------------------------------------------------------------

    void                Reset              ( void );
    void                OnGameUpdate       ( f32 deltaTime );

    void                RegisterStat       ( stat_fields thisField,
                                             f32         thisValue );

    f32                 GetStat            ( stat_fields      thisStat,
                                             stat_measurement thisMeasurement,
                                             f32              lengthOfTimeToAverage = 1.0f );

    static const char*  GetStatName         ( stat_fields thisStat );

    //-------------------------------------------------------------------------
    //  DISPLAY
    //-------------------------------------------------------------------------

    void                DrawFPS            ( void );
    void                DrawSmallBars      ( void );
    void                DrawCPULegend      ( void );
    void                DrawGPULegend      ( void );
    void                DrawSmallBarLegend ( void );

    static xbool        m_bShowNumbers;
    static xbool        m_bShowHorizontalBars;

    static xbool        m_bShowCPUTimeLegend;
    static xbool        m_bShowGPUTimeLegend;
    static xbool        m_bShowVerticalLegend;


protected:

    void                CompleteFrame      ( f32 DeltaTime );

    static stats_mgr*   s_This;

    s32                 m_FrameCount;

    f32                 m_Stats[k_stats_Last][k_SAMPLE_HISTORY];

    f32                 m_StatsMin[k_stats_Last];
    f32                 m_StatsMax[k_stats_Last];
    f32                 m_FrameDurations[k_SAMPLE_HISTORY];
    s32                 m_SampleCount;
};

#endif // STATS_MGR

#else //ENABLE_STATS_MGR

// Stub out the stat_mgr macros to make it all go away in the code
#define LOG_STAT(x)
#define STAT_LOGGER(_a_,_b_)

#endif //ENABLE_STATS_MGR
