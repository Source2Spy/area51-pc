///////////////////////////////////////////////////////////////////////////
// PostEffectMgr.cpp
///////////////////////////////////////////////////////////////////////////

#include "PostEffectMgr.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/LevelSettings.hpp"
#include "Render/Render.hpp"
#include "GameLib/RenderContext.hpp"
#include "AudioMgr/AudioMgr.hpp"

#include "x_math.hpp"

///////////////////////////////////////////////////////////////////////////
// data
///////////////////////////////////////////////////////////////////////////

post_effect_mgr     g_PostEffectMgr;

static const f32    kNormalBlurOffset = 0.0f;
static const xcolor kNormalBlurColor(128,128,128,16);
static const s32    kDefaultFilmGrainStrength = 100;

static const f32    kSceneBrightness     = 0.0f;
static const f32    kSceneContrast       = 1.0f;

static f32 FadePainChannel( f32 Value, f32 Normal, f32 DeltaTime )
{
    f32 Excess = MAX( 0.0f, Value - Normal );
    if( Excess > 100.0f )
    {
        f32 const ExponentialTime = x_log( Excess / 100.0f ) / 3.0f;
        f32 const Elapsed = MIN( DeltaTime, ExponentialTime );
        Excess *= x_exp( -3.0f * Elapsed );
        DeltaTime -= Elapsed;
    }
    return Normal + MAX( 0.0f, Excess - 300.0f * DeltaTime );
}


///////////////////////////////////////////////////////////////////////////
// Implementation
///////////////////////////////////////////////////////////////////////////

post_effect_mgr::post_effect_mgr( void )
{
    Init();
}

//=========================================================================

post_effect_mgr::~post_effect_mgr( void )
{
}

//=========================================================================

void post_effect_mgr::Init( void )
{
    // clear out everything
    m_StartFadeColor.Set(0,0,0,0);
    m_EndFadeColor.Set(0,0,0,0);
    m_FadeTimeElapsed = 0.0f;
    m_FadeTime        = 0.0f;
    m_bFadeAudio      = FALSE;
    m_FilmGrainStrength = kDefaultFilmGrainStrength;
    m_bBackgroundBlurEnabled = FALSE;
    m_SceneBrightness     = kSceneBrightness;
    m_SceneContrast       = kSceneContrast;

    s32 i;
    for( i = 0; i < MAX_LOCAL_PLAYERS; i++ )
    {
        m_bDoPainBlur[i]    = FALSE;
        m_PainBlurOffset[i] = kNormalBlurOffset;
        m_PainBlurColor[i]  = kNormalBlurColor;
        m_PainBlurRed[i]    = kNormalBlurColor.R;
        m_PainBlurAlpha[i]  = kNormalBlurColor.A;
    }
}

//=========================================================================

void post_effect_mgr::OnUpdate( f32 DeltaTime )
{
    s32 i;
    for( i = 0; i < MAX_LOCAL_PLAYERS; i++ )
    {
        ///////////////////////////////////////////////////////////////////
        // update the pain/filter blur
        ///////////////////////////////////////////////////////////////////
        f32 const Elapsed = MAX( 0.0f, DeltaTime );
        m_PainBlurOffset[i] = MAX( kNormalBlurOffset, m_PainBlurOffset[i] - 30.0f * Elapsed );
        m_PainBlurRed[i] = FadePainChannel( m_PainBlurRed[i], kNormalBlurColor.R, Elapsed );
        m_PainBlurAlpha[i] = FadePainChannel( m_PainBlurAlpha[i], kNormalBlurColor.A, Elapsed );
        m_PainBlurColor[i].R = static_cast<u8>( m_PainBlurRed[i] + 0.5f );
        m_PainBlurColor[i].A = static_cast<u8>( m_PainBlurAlpha[i] + 0.5f );

        if( (m_PainBlurOffset[i] == kNormalBlurOffset) &&
            (m_PainBlurRed[i] == kNormalBlurColor.R) &&
            (m_PainBlurAlpha[i] == kNormalBlurColor.A) )
        {
            m_bDoPainBlur[i] = FALSE;
        }
    }

    // update the screen fade
    if( m_FadeTime > 0.0f )
    {
        xbool FinishedFading = FALSE;

        m_FadeTimeElapsed += DeltaTime;
        if( m_FadeTimeElapsed > m_FadeTime )
        {
            m_FadeTime        = 0.0f;
            m_FadeTimeElapsed = 0.0f;
            m_StartFadeColor  = m_EndFadeColor;
            FinishedFading    = TRUE;
        }

        // HACK HACK HACK - THIS IS A MAJOR HACK-O-RAMA FOR E3 AND OFFENDS ME
        // GREATLY. WE NEED TO HAVE THE AUDIO FADE OUT WITH THE SCREEN AT
        // THE END OF A LEVEL. WE NEED THE AUDIO MANAGER TO HAVE SLIDER
        // CONTROL, AND THEN WE CAN DO THIS FROM AN AUDIO TRIGGER.
        if( m_bFadeAudio )
        {
            f32 T;
            
            if (FinishedFading)
                T = 1.0f;
            else
                T = m_FadeTimeElapsed / m_FadeTime;

            f32 A = (f32)m_StartFadeColor.A + T * ((f32)m_EndFadeColor.A - (f32)m_StartFadeColor.A);
            f32 Volume = 1.0f - (A / 255.0f);

            if( Volume > 0.5f )
                g_AudioMgr.SetMasterVolume( Volume );
            else
                g_AudioMgr.SetMasterVolume( 0.0f );

            if( FinishedFading )
                m_bFadeAudio = FALSE;
        }
    }
}

//=========================================================================

void post_effect_mgr::Render( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "post_effect_mgr::Render" );

    if( !render::GetActiveRenderScopeDesc().EnablePostProcessing )
        return;

    // set base screen params
    render::SceneColorGradeParams SceneColorGrade;
    SceneColorGrade.Brightness     = m_SceneBrightness;
    SceneColorGrade.Contrast       = m_SceneContrast;
    render::SetSceneColorGrade( SceneColorGrade );

    // do the self-illum glows
    if ( g_RenderContext.m_bIsMutated )
    {
        render::ApplySelfIllumGlows( 0.8f, 64 );
    }
    else
    {
        render::ApplySelfIllumGlows( 0.0f, 255 );
    }

    // handle pain screen blurring
    s32 LocalPlayerIndex = g_RenderContext.LocalPlayerIndex;
    if ( m_bDoPainBlur[LocalPlayerIndex] )
    {
        // do a pain blur
        render::MipFilter( 2, m_PainBlurOffset[LocalPlayerIndex], render::FALLOFF_CONSTANT, m_PainBlurColor[LocalPlayerIndex], 0.0f, 0.0f, LocalPlayerIndex );
    }

    // handle normal screen blurring
    if( m_bBackgroundBlurEnabled )
    {
        // do a very slight depth-of-field to remove some of the sparkles
        render::MipFilter( 2, 0.0f, render::FALLOFF_EXP, xcolor(128,128,128,128), 1.5f, 0.0f, LocalPlayerIndex );
    }

    // do the "mutant" vision mode
    if( g_RenderContext.m_bIsMutated )
    {
        // do a radial blur
        f32    Zoom       = 0.8f;
        radian Angle      = R_2;
        f32    AlphaSub   = 104.0f;
        f32    AlphaScale = 148.0f;
        render::RadialBlur( Zoom, Angle, AlphaSub, AlphaScale );
    }

    if( m_FilmGrainStrength > 0 )
    {
        render::NoiseFilter( xcolor( 200, 185, 170, m_FilmGrainStrength ) );
    }

}

//=========================================================================

void post_effect_mgr::SetFilmGrainStrength( s32 Strength )
{
    m_FilmGrainStrength = x_clamp( Strength, 0, 255 );
}

//=========================================================================

void post_effect_mgr::SetBackgroundBlurEnabled( xbool Enabled )
{
    m_bBackgroundBlurEnabled = Enabled;
}

//==========================================================================

void post_effect_mgr::SetSceneColorGrade( render::SceneColorGradeParams const& Params )
{
    m_SceneBrightness     = x_clamp( Params.Brightness, -1.0f, 1.0f );
    m_SceneContrast       = x_clamp( Params.Contrast, 0.0f, 4.0f );
}

//=========================================================================

void post_effect_mgr::RenderFog( void )
{
    render::ZFogFilter( render::FALLOFF_CUSTOM, g_RenderContext.LocalPlayerIndex );
}

//=========================================================================

void post_effect_mgr::RenderScreenFade( void )
{
    // screen fading
    xcolor FadeColor( m_EndFadeColor );
    if( m_FadeTime > 0.0f )
    {
        f32 T = m_FadeTimeElapsed / m_FadeTime;
        f32 R = (f32)m_StartFadeColor.R + T * ((f32)m_EndFadeColor.R - (f32)m_StartFadeColor.R);
        f32 G = (f32)m_StartFadeColor.G + T * ((f32)m_EndFadeColor.G - (f32)m_StartFadeColor.G);
        f32 B = (f32)m_StartFadeColor.B + T * ((f32)m_EndFadeColor.B - (f32)m_StartFadeColor.B);
        f32 A = (f32)m_StartFadeColor.A + T * ((f32)m_EndFadeColor.A - (f32)m_StartFadeColor.A);
        FadeColor.R = (u8)R;
        FadeColor.G = (u8)G;
        FadeColor.B = (u8)B;
        FadeColor.A = (u8)A;
    }
    if( FadeColor.A != 0 )
    {
        render::ScreenFade( FadeColor );
    }
}

//=========================================================================

void post_effect_mgr::StartPainBlur( s32 LocalPlayerIndex, f32 InitialOffset, xcolor InitialColor )
{
    if( m_bDoPainBlur[LocalPlayerIndex] )
    {
        InitialOffset  = MAX( InitialOffset, m_PainBlurOffset[LocalPlayerIndex] );
        InitialColor.R = MAX( InitialColor.R, m_PainBlurColor[LocalPlayerIndex].R );
        InitialColor.A = MAX( InitialColor.A, m_PainBlurColor[LocalPlayerIndex].A );
    }

    m_bDoPainBlur[LocalPlayerIndex]    = TRUE;
    m_PainBlurOffset[LocalPlayerIndex] = InitialOffset;
    m_PainBlurColor[LocalPlayerIndex]  = InitialColor;
    m_PainBlurRed[LocalPlayerIndex]    = InitialColor.R;
    m_PainBlurAlpha[LocalPlayerIndex]  = InitialColor.A;
}

//=========================================================================

void post_effect_mgr::StartScreenFade( xcolor FadeColor, f32 TimeToFade, xbool FadeAudio )
{
    m_FadeTimeElapsed = 0.0f;
    m_FadeTime        = TimeToFade;
    m_StartFadeColor  = m_EndFadeColor;
    m_EndFadeColor    = FadeColor;
    m_bFadeAudio      = FadeAudio;
    if( TimeToFade == 0.0f )
    {
        m_StartFadeColor = m_EndFadeColor;

        // HACK HACK HACK HACK HACK - SEE COMMENT IN UPDATE
        if( FadeAudio )
        {
            f32 Volume = 1.0f - ((f32)m_StartFadeColor.A / 255.0f);
            g_AudioMgr.SetMasterVolume( Volume );
            m_bFadeAudio = FALSE;
        }
    }
}
