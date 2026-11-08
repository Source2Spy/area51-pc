///////////////////////////////////////////////////////////////////////////////
//
//  stats_mgr
//
//      Keeps up with all performance stats for the game
//
///////////////////////////////////////////////////////////////////////////////

//=============================================================================
// Includes
//=============================================================================

#include "Render/Render.hpp"
#include "Entropy/Audio/audio_channel_mgr.hpp"
#include "e_ScratchMem.hpp"
#include "StatsMgr.hpp"
#include "UI/ui_renderer.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"

#ifndef TARGET_WINDOWS
#include "e_Engine.hpp"
#endif

//=============================================================================

#if ENABLE_STATS_MGR

//=============================================================================
// Externs
//=============================================================================

//=============================================================================
// Statics
//=============================================================================

stats_mgr* stats_mgr::s_This                = NULL;
xbool      stats_mgr::m_bShowNumbers        = FALSE;
xbool      stats_mgr::m_bShowHorizontalBars = FALSE;

xbool stats_mgr::m_bShowCPUTimeLegend  = FALSE;
xbool stats_mgr::m_bShowGPUTimeLegend  = FALSE;
xbool stats_mgr::m_bShowVerticalLegend = FALSE;

//=============================================================================
// Text and color display info
//=============================================================================

struct stat_render_info
{
    stat_render_info( const char* pName_, xcolor BarColor_ )
    {
        pName    = pName_;
        BarColor = BarColor_;
    }

    const char* pName;
    xcolor      BarColor;
};

stat_render_info StatRenderInfo[k_stats_Last] =
    {
        stat_render_info( "OnAdvance", xcolor( 128, 128, 0, 255 ) ),       // k_stats_OnAdvance
        stat_render_info( "DebrisLogic", xcolor( 255, 128, 0, 255 ) ),     // k_stats_Debris
        stat_render_info( "AIThink", xcolor( 0, 255, 0, 255 ) ),           // k_stats_AI_Think
        stat_render_info( "AIAdvance", xcolor( 128, 64, 0, 255 ) ),        // k_stats_AI_Advance
        stat_render_info( "Animation", xcolor( 0, 0, 100, 255 ) ),         // k_stats_Animation
        stat_render_info( "Collision", xcolor( 128, 128, 255, 255 ) ),     // k_stats_Collision
        stat_render_info( "Sound", xcolor( 100, 100, 255, 255 ) ),         // k_stats_Sound
        stat_render_info( "ParticleLogic", xcolor( 255, 255, 0, 255 ) ),   // k_stats_ParticleSystem
        stat_render_info( "Decals", xcolor( 0, 255, 255, 255 ) ),          // k_stats_DecalSystem
        stat_render_info( "TriggerLogic", xcolor( 255, 0, 255, 255 ) ),    // k_stats_TriggerSystem
        stat_render_info( "HiLevelRender", xcolor( 64, 64, 64, 255 ) ),    // k_stats_HighLevelRender
        stat_render_info( "ObjectRender", xcolor( 255, 255, 255, 255 ) ),  // k_stats_ObjectRender
        stat_render_info( "OtherRender", xcolor( 128, 128, 128, 255 ) ),   // k_stats_OtherRender
        stat_render_info( "ProjectileLogic", xcolor( 255, 128, 0, 255 ) ), // k_stats_Projectiles
        stat_render_info( "PortalLogic", xcolor( 255, 0, 0, 255 ) ),       // k_stats_Portal
        stat_render_info( "TurretLogic", xcolor( 0, 128, 0, 255 ) ),       // k_stats_Turret
        stat_render_info( "Physics", xcolor( 128, 0, 0, 255 ) ),           // k_stats_Physics
        stat_render_info( "Player", xcolor( 128, 0, 128, 255 ) ),          // k_stats_Player
        stat_render_info( "Other", xcolor( 128, 255, 128, 255 ) ),         // k_stats_CPU_Time
        stat_render_info( "NumTimers", xcolor( 0, 0, 0, 255 ) ),           // k_stats_NumTimerStats

        stat_render_info( "Frame", xcolor( 255, 255, 255, 255 ) ),
        stat_render_info( "Scratch memory", xcolor( 128, 192, 255, 255 ) ),
        stat_render_info( "Memory", xcolor( 255, 192, 128, 255 ) ),
        stat_render_info( "Objects", xcolor( 128, 255, 192, 255 ) ),
        stat_render_info( "Texture memory", xcolor( 192, 128, 255, 255 ) ),
};

//=============================================================================
//  HELPER FUNCTIONS
//=============================================================================

static
void DrawStatsText( s32 Font, s32 X, s32 Y, const char* pText, xcolor Color )
{
    const xwstring Text( pText );
    irect          Bounds;
    g_UiMgr->TextSize( Font, Bounds, Text, Text.GetLength() );
    Bounds.Translate( X, Y );
    g_UiMgr->RenderText( Font, Bounds, ui_font::h_left | ui_font::v_top, Color, Text, TRUE, FALSE );
}

//=============================================================================

//=============================================================================
// Implentation of the stat_logger class
//=============================================================================

s32          stat_logger::s_StatLevel = 0;
stat_logger* stat_logger::s_StatStack[stat_logger::MAX_STAT_LEVELS];

//=============================================================================

stat_logger::stat_logger( stat_fields fieldToTrack ) : m_FieldToTrack( fieldToTrack )
{
    // pause the current hierarchy timer and start up the next one
    ASSERT( s_StatLevel < stat_logger::MAX_STAT_LEVELS );

    if( s_StatLevel > 0 )
    {
        stats_mgr::GetStatsMgr()->RegisterStat( s_StatStack[s_StatLevel - 1]->m_FieldToTrack,
                                                s_StatStack[s_StatLevel - 1]->m_Timer.StopMs() );
    }

    s_StatStack[s_StatLevel] = this;
    s_StatLevel++;

    m_Timer.Reset();
    m_Timer.Start();
}

//=============================================================================

stat_logger::~stat_logger()
{
    // read out the timer, and restart the next one up in the hierarchy
    stats_mgr::GetStatsMgr()->RegisterStat( m_FieldToTrack, m_Timer.StopMs() );
    ASSERT( s_StatLevel > 0 );
    s_StatLevel--;

    if( s_StatLevel > 0 )
    {
        s_StatStack[s_StatLevel - 1]->m_Timer.Reset();
        s_StatStack[s_StatLevel - 1]->m_Timer.Start();
    }
}

//=============================================================================
// implementation of the stats manager
//=============================================================================

stats_mgr::stats_mgr( void )
{
    ASSERT( !s_This );
    s_This = this;
    Reset();
}

//=============================================================================

stats_mgr::~stats_mgr()
{
    s_This = NULL;
}

//=============================================================================

void stats_mgr::Reset( void )
{
    m_FrameCount  = 0;
    m_SampleCount = 0;

    x_memset( m_Stats, 0, sizeof( m_Stats ) );
    x_memset( m_FrameDurations, 0, sizeof( m_FrameDurations ) );

    for( s32 i = 0; i < k_stats_Last; i++ )
    {
        m_StatsMin[i] = F32_MAX;
        m_StatsMax[i] = 0.0f;
    }
}

//=============================================================================

void stats_mgr::OnGameUpdate( f32 DeltaTime )
{
    if( !x_isvalid( DeltaTime ) || DeltaTime <= 0.0f )
        return;

    s32 TextureCount, TextureMemory;
    texture::GetStats( &TextureCount, &TextureMemory );

    // Capture frame-wide counters before advancing the history slot.
    RegisterStat( k_stats_FrameTime, DeltaTime * 1000.0f );
    RegisterStat( k_stats_TextureMemory, (f32)TextureMemory );
    RegisterStat( k_stats_Memory, (f32)x_MemGetUsed() );
    RegisterStat( k_stats_SMem, (f32)smem_GetMaxUsed() );

    s32 ObjectCount = 0;

    for( s32 i = object::TYPE_NULL + 1; i < object::TYPE_END_OF_LIST; i++ )
        ObjectCount += g_ObjMgr.GetNumInstances( (object::type)i );

    RegisterStat( k_stats_TotalObjectCount, (f32)ObjectCount );

    CompleteFrame( DeltaTime );
}

//=============================================================================

void stats_mgr::CompleteFrame( f32 DeltaTime )
{
    // Finish the current sample before moving to the next circular-buffer slot.
    m_FrameDurations[m_FrameCount] = DeltaTime;

    for( s32 i = 0; i < k_stats_Last; i++ )
    {
        m_StatsMin[i] = x_min( m_StatsMin[i], m_Stats[i][m_FrameCount] );
        m_StatsMax[i] = x_max( m_StatsMax[i], m_Stats[i][m_FrameCount] );
    }

    m_SampleCount = x_min( m_SampleCount + 1, k_SAMPLE_HISTORY - 1 );
    m_FrameCount  = ( m_FrameCount + 1 ) % k_SAMPLE_HISTORY;

    // The new slot accumulates statistics for the next frame.
    for( s32 i = 0; i < k_stats_Last; i++ )
        m_Stats[i][m_FrameCount] = 0.0f;

    m_FrameDurations[m_FrameCount] = 0.0f;
}

//=============================================================================

void stats_mgr::RegisterStat( stat_fields Field, f32 Value )
{
    ASSERT( Field >= 0 && Field < k_stats_Last );
    ASSERT( x_isvalid( Value ) );

    m_Stats[Field][m_FrameCount] += Value;
}

//=============================================================================

f32 stats_mgr::GetStat( stat_fields Field, stat_measurement Measurement, f32 AverageTime )
{
    ASSERT( Field >= 0 && Field < k_stats_Last );

    if( m_SampleCount == 0 )
        return 0.0f;

    if( Measurement == k_stats_MaxEver )
        return m_StatsMax[Field];

    if( Measurement == k_stats_MinEver )
        return m_StatsMin[Field];

    f32 Value    = Measurement == k_stats_MinRecent ? F32_MAX : 0.0f;
    f32 Duration = 0.0f;
    s32 Count    = 0;

    // Walk completed frames backwards; the current slot is still being filled.
    for( s32 i = 1; i <= m_SampleCount; i++ )
    {
        const s32 Index  = ( m_FrameCount + k_SAMPLE_HISTORY - i ) % k_SAMPLE_HISTORY;
        const f32 Sample = m_Stats[Field][Index];

        if( Measurement == k_stats_Instant )
            return Sample;

        if( Measurement == k_stats_MaxRecent )
            Value = x_max( Value, Sample );
        else if( Measurement == k_stats_MinRecent )
            Value = x_min( Value, Sample );
        else
        {
            ASSERT( Measurement == k_stats_Average );
            Value += Sample;
            Duration += m_FrameDurations[Index];
            Count++;

            if( Duration >= AverageTime )
                break;
        }
    }

    return Measurement == k_stats_Average ? Value / Count : Value;
}

//=============================================================================

const char* stats_mgr::GetStatName( stat_fields Field )
{
    return Field >= 0 && Field < k_stats_Last ? StatRenderInfo[Field].pName : "?";
}

//=============================================================================

void stats_mgr::DrawFPS( void )
{
    if( !g_UiMgr || ( !m_bShowHorizontalBars && !m_bShowNumbers ) )
        return;

    const s32 Font = g_UiMgr->FindFont( "small" );

    if( Font < 0 )
        return;

    const irect Bounds     = g_UIRenderer.GetViewport().GetLogicalClipBounds();
    const s32   LineHeight = g_UiMgr->GetLineHeight( Font );

    if( m_bShowHorizontalBars )
    {
        f32 Total = 0.0f;

        for( s32 i = 0; i < k_stats_NumTimerStats; i++ )
            Total += GetStat( (stat_fields)i, k_stats_Average, 0.25f );

        const f32 Scale = x_max( 1000.0f / 60.0f, x_max( Total, GetStat( k_stats_FrameTime, k_stats_Average, 0.25f ) ) );
        const s32 Left  = Bounds.l + 10;
        const s32 Right = Bounds.r - 10;
        const s32 Top   = Bounds.b - LineHeight - 20;

        g_UIRenderer.DrawRect( irect( Left, Top, Right, Top + 8 ), xcolor( 40, 40, 40, 192 ) );

        f32 X = (f32)Left;

        for( s32 i = 0; i < k_stats_NumTimerStats; i++ )
        {
            const f32 Width = ( Right - Left ) * GetStat( (stat_fields)i, k_stats_Average, 0.25f ) / Scale;
            g_UIRenderer.DrawRect( rect( X, (f32)Top, X + Width, (f32)Top + 8 ), StatRenderInfo[i].BarColor );
            X += Width;
        }

        DrawStatsText( Font, Left, Top + 10, xfs( "CPU %.2f ms / Frame %.2f ms", Total, GetStat( k_stats_FrameTime, k_stats_Average, 0.25f ) ), XCOLOR_WHITE );
    }

    if( m_bShowNumbers )
    {
        s32 Y = Bounds.t + 40;

        for( s32 i = 0; i < k_stats_NumTimerStats; i++ )
        {
            DrawStatsText( Font, Bounds.l + 10, Y, xfs( "%-16s %6.2f ms", StatRenderInfo[i].pName, GetStat( (stat_fields)i, k_stats_Average, 0.5f ) ), StatRenderInfo[i].BarColor );
            Y += LineHeight;
        }
    }
}

//=============================================================================

void stats_mgr::DrawCPULegend( void )
{
    if( !m_bShowCPUTimeLegend || !g_UiMgr )
        return;

    const s32 Font = g_UiMgr->FindFont( "small" );

    if( Font < 0 )
        return;

    const irect Bounds     = g_UIRenderer.GetViewport().GetLogicalClipBounds();
    const s32   LineHeight = g_UiMgr->GetLineHeight( Font );
    const s32   Left       = Bounds.r - 240;
    s32         Y          = Bounds.t + 100;

    g_UIRenderer.DrawRect( irect( Left - 4, Y - 4, Bounds.r - 6,
                                  Y + ( k_stats_NumTimerStats + 1 ) * LineHeight + 4 ),
                           xcolor( 0, 0, 0, 192 ) );
    DrawStatsText( Font, Left, Y, "CPU time (ms)", XCOLOR_WHITE );
    Y += LineHeight;

    for( s32 i = 0; i < k_stats_NumTimerStats; i++ )
    {
        g_UIRenderer.DrawRect( irect( Left, Y + 2, Left + 6, Y + 8 ), StatRenderInfo[i].BarColor );
        DrawStatsText( Font, Left + 10, Y, xfs( "%-16s %6.2f", StatRenderInfo[i].pName, GetStat( (stat_fields)i, k_stats_Average, 0.5f ) ), XCOLOR_WHITE );
        Y += LineHeight;
    }
}

//=============================================================================

void stats_mgr::DrawGPULegend( void )
{
    if( !m_bShowGPUTimeLegend || !g_UiMgr )
        return;

    const s32 Font = g_UiMgr->FindFont( "small" );

    if( Font < 0 )
        return;

    const irect Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();

    DrawStatsText( Font, Bounds.l + 10, Bounds.t + 40, "GPU timing: unavailable", XCOLOR_WHITE );
}

//=============================================================================

void stats_mgr::DrawSmallBars( void )
{
    if( !m_bShowVerticalLegend || !g_UiMgr )
        return;

    const s32 Font = g_UiMgr->FindFont( "small" );

    if( Font < 0 )
        return;

    const irect Bounds     = g_UIRenderer.GetViewport().GetLogicalClipBounds();
    const s32   LineHeight = g_UiMgr->GetLineHeight( Font );
    s32         Y          = Bounds.t + 40;

    for( s32 i = k_stats_SMem; i < k_stats_Last; i++ )
    {
        const f32   Value   = GetStat( (stat_fields)i, k_stats_Instant );
        const f32   Maximum = x_max( 1.0f, GetStat( (stat_fields)i, k_stats_MaxRecent ) );
        const f32   Width   = 180.0f * x_clamp( Value / Maximum, 0.0f, 1.0f );
        const s32   Left    = Bounds.l + 10;
        const s32   Top     = Y + LineHeight;
        const xbool Memory  = i != k_stats_TotalObjectCount;

        DrawStatsText( Font, Left, Y, xfs( "%s: %.1f %s", StatRenderInfo[i].pName, Memory ? Value / 1048576.0f : Value, Memory ? "MiB" : "" ), XCOLOR_WHITE );
        g_UIRenderer.DrawRect( rect( (f32)Left, (f32)Top, (f32)Left + 180.0f, (f32)Top + 6 ), xcolor( 40, 40, 40, 192 ) );
        g_UIRenderer.DrawRect( rect( (f32)Left, (f32)Top, (f32)Left + Width, (f32)Top + 6 ), StatRenderInfo[i].BarColor );
        Y += LineHeight + 12;
    }
}

//=============================================================================

void stats_mgr::DrawSmallBarLegend( void )
{
    if( !m_bShowVerticalLegend || !g_UiMgr )
        return;

    const s32 Font = g_UiMgr->FindFont( "small" );

    if( Font < 0 )
        return;

    const irect Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();
    DrawStatsText( Font, Bounds.l + 10, Bounds.t + 40 + 4 * ( g_UiMgr->GetLineHeight( Font ) + 12 ),
                   "Bars relative to recent peak", XCOLOR_WHITE );
}

#endif // ENABLE_STATS_MGR
