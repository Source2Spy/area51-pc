//=========================================================================
//
//  dlg_LoadGame.cpp
//
//=========================================================================

// TODO: GS: Make better text effects.

#include "Entropy.hpp"
#include "dlg_LoadGame.hpp"
#include "StateMgr/StateMgr.hpp"
#include "InputMgr/GamePad.hpp"

#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"

//=========================================================================
//  Layout
//=========================================================================

#define FINAL_FADE_OUT_TIME         1.0f
#define SLIDESHOW_MAX_AUDIO_ERROR_TIME (2.5f / 30.0f)
#define SLIDESHOW_MAX_AUDIO_ERROR_FIX  (1.0f / 3.0f)

#define LEVEL_NAME_LEFT            -50
#define LEVEL_NAME_TOP             313
#define LEVEL_NAME_RIGHT           462
#define LEVEL_NAME_BOTTOM          377

#define SHADOW_SCALE_BEGIN_TIME     0.0f
#define SHADOW_SCALE_END_TIME       1.3333333f
#define SHADOW_SLIDE_BEGIN_TIME     1.3333333f
#define SHADOW_SLIDE_END_TIME       5.8f
#define SHADOW_SLIDE_BEGIN_X        0.0f
#define SHADOW_SLIDE_END_X         -64.0f
#define SHADOW_SLIDE_BEGIN_Y        0.0f
#define SHADOW_SLIDE_END_Y          20.0f
#define SHADOW_FADEIN_BEGIN_TIME    5.8f
#define SHADOW_FADEIN_END_TIME      6.8f

static xcolor s_LoadNameGlow( 187, 255, 187, 255 );

//=========================================================================
//  Load Game Dialog
//=========================================================================

ui_manager::dialog_tem LoadGameDialog =
{
    "IDS_LOAD_GAME",
    1, 9,
    0,
    NULL,
    0
};

//=========================================================================
//  Registration function
//=========================================================================

void dlg_load_game_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "load game", &LoadGameDialog, &dlg_load_game_factory );
}

//=========================================================================
//  Factory function
//=========================================================================

ui_win* dlg_load_game_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    dlg_load_game* pDialog = new dlg_load_game;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return (ui_win*)pDialog;
}

//=========================================================================
//  dlg_load_game
//=========================================================================

dlg_load_game::dlg_load_game( void )
{
}

//=========================================================================

dlg_load_game::~dlg_load_game( void )
{
    Destroy();
}

//=========================================================================

xbool dlg_load_game::Create( s32                        UserID,
                             ui_manager*                pManager,
                             ui_manager::dialog_tem*    pDialogTem,
                             const irect&               Position,
                             ui_win*                    pParent,
                             s32                        Flags,
                             void*                      pUserData )
{
    (void)pUserData;

    ASSERT( pManager );

    xbool Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_VoiceID               = 0;
    m_nSlides               = 0;
    m_StartTextAnim         = 0.0f;
    m_NameText              = xwstring( "" );
    m_SlideshowState        = STATE_IDLE;
    m_AllowSkip             = FALSE;
    m_LoadingComplete       = FALSE;
    m_FinalFadeoutStarted   = FALSE;
    m_ElapsedTime           = 0.0f;
    m_FadeTimeElapsed       = 0.0f;
    m_AudioPrevTime         = 0.0f;
    m_AudioPredictedTime    = 0.0f;

    ResetSlides();

    m_State = DIALOG_STATE_ACTIVE;
    return Success;
}

//=========================================================================

void dlg_load_game::Destroy( void )
{
    ReleaseSlides();

    if( g_UiMgr )
        g_UiMgr->ResetScreenWipe();

    ui_dialog::Destroy();
}

//=========================================================================

void dlg_load_game::ResetSlides( void )
{
    for( s32 i = 0; i < NUM_SLIDES; i++ )
    {
        ReleaseSlide( m_Slides[i] );
        m_Slides[i].StartFadeIn  = 0.0f;
        m_Slides[i].EndFadeIn    = 1.0f;
        m_Slides[i].StartFadeOut = 2.0f;
        m_Slides[i].EndFadeOut   = 3.0f;
        m_Slides[i].SlideColor   = XCOLOR_WHITE;
    }
}

//=========================================================================

void dlg_load_game::ReleaseSlide( slide_info& Slide )
{
    if( Slide.BitmapName.GetLength() > 0 )
    {
        if( g_UiMgr )
            g_UiMgr->UnloadBitmap( Slide.BitmapName );

        Slide.BitmapName = "";
    }
}

//=========================================================================

void dlg_load_game::ReleaseSlides( void )
{
    for( s32 i = 0; i < NUM_SLIDES; i++ )
        ReleaseSlide( m_Slides[i] );
}

//=========================================================================

void dlg_load_game::ResetSlideshowClock( void )
{
    m_ElapsedTime        = 0.0f;
    m_AudioPrevTime      = 0.0f;
    m_AudioPredictedTime = 0.0f;
}

//=========================================================================

void dlg_load_game::AdvanceSlideshowClock( f32 DeltaTime )
{
    if( DeltaTime <= 0.0f )
        return;

    if( g_AudioMgr.IsValidVoiceIdCached( m_VoiceID ) )
    {
        f32 AudioTime = g_AudioMgr.GetCurrentPlayTimeCached( m_VoiceID );

        if( AudioTime <= 0.0f )
        {
            if( m_AudioPrevTime <= 0.0f )
                return;

            AudioTime = m_AudioPrevTime;
        }

        f32 AudioDeltaTime = AudioTime - m_AudioPrevTime;
        if( AudioDeltaTime < 0.0f )
        {
            AudioDeltaTime = 0.0f;
            AudioTime      = m_AudioPrevTime;
        }

        m_AudioPrevTime = AudioTime;

        if( AudioDeltaTime > 0.0f )
            m_AudioPredictedTime = AudioTime;
        else
            m_AudioPredictedTime += DeltaTime;

        if( m_ElapsedTime <= 0.0f )
            m_ElapsedTime = AudioTime;
        else
            m_ElapsedTime += DeltaTime;

        f32 Error = m_AudioPredictedTime - m_ElapsedTime;
        if( x_abs( Error ) > SLIDESHOW_MAX_AUDIO_ERROR_TIME )
        {
            f32 MaxError = DeltaTime * SLIDESHOW_MAX_AUDIO_ERROR_FIX;
            m_ElapsedTime += MINMAX( -MaxError, Error, MaxError );
        }
    }
    else
    {
        m_ElapsedTime += DeltaTime;
    }
}

//=========================================================================

irect dlg_load_game::ScaleBaseRect( s32 L, s32 T, s32 R, s32 B ) const
{
    const irect& Bounds = g_UiMgr->GetUserBounds( g_UiUserID );

    f32 ScaleX = (f32)Bounds.GetWidth()  / (f32)ui_viewport::CONTENT_WIDTH;
    f32 ScaleY = (f32)Bounds.GetHeight() / (f32)ui_viewport::CONTENT_HEIGHT;

    irect Result;
    Result.l = Bounds.l + (s32)((f32)L * ScaleX);
    Result.t = Bounds.t + (s32)((f32)T * ScaleY);
    Result.r = Bounds.l + (s32)((f32)R * ScaleX);
    Result.b = Bounds.t + (s32)((f32)B * ScaleY);
    return Result;
}

//=========================================================================

irect dlg_load_game::GetScreenRect( void ) const
{
    return g_UiMgr->GetUserBounds( g_UiUserID );
}

//=========================================================================

irect dlg_load_game::GetLevelNameRect( void ) const
{
    return ScaleBaseRect( LEVEL_NAME_LEFT, LEVEL_NAME_TOP, LEVEL_NAME_RIGHT, LEVEL_NAME_BOTTOM );
}

//=========================================================================

s32 dlg_load_game::GetSlideAlpha( const slide_info& Slide, f32 Time ) const
{
    if( Time < Slide.StartFadeIn )
        return 0;

    if( Time < Slide.EndFadeIn )
    {
        f32 Duration = Slide.EndFadeIn - Slide.StartFadeIn;
        if( Duration > 0.0f )
            return MINMAX( 0, (s32)(255.0f * (Time - Slide.StartFadeIn) / Duration), 255 );

        return 255;
    }

    if( Time < Slide.StartFadeOut )
        return 255;

    if( Time < Slide.EndFadeOut )
    {
        f32 Duration = Slide.EndFadeOut - Slide.StartFadeOut;
        if( Duration > 0.0f )
            return MINMAX( 0, 255 - (s32)(255.0f * (Time - Slide.StartFadeOut) / Duration), 255 );

        return 0;
    }

    return 0;
}

//=========================================================================

void dlg_load_game::RenderFullscreen( xcolor Color )
{
    g_UIRenderer.DrawRect( GetScreenRect(), Color );
}

//=========================================================================

void dlg_load_game::RenderSlideImage( const slide_info& Slide, xcolor Color )
{
    if( !g_UiMgr )
        return;

    const s32 BitmapID = g_UiMgr->FindBitmap( Slide.BitmapName );
    if( BitmapID >= 0 )
    {
        g_UiMgr->RenderBitmapUV( BitmapID,
                                 GetScreenRect(),
                                 vector2( 0.0f, 0.0f ),
                                 vector2( 1.0f, 1.0f ),
                                 Color );
    }
}

//=========================================================================

void dlg_load_game::RenderLevelName( s32 NameAlpha, f32 ShadowOffsetX, f32 ShadowOffsetY, f32 ShadowAlpha )
{
    s32 FontIndex = g_UiMgr->FindFont( "loadscr" );
    if( FontIndex < 0 )
        return;

    irect TextRect = GetLevelNameRect();
    u32   TextFlags = ui_font::h_right | ui_font::v_bottom;

    f32 ScaleX = (f32)GetScreenRect().GetWidth()  / (f32)ui_viewport::CONTENT_WIDTH;
    f32 ScaleY = (f32)GetScreenRect().GetHeight() / (f32)ui_viewport::CONTENT_HEIGHT;

    //irect ShadowRect = TextRect;
    //ShadowRect.Translate( (s32)(2.0f * ScaleX), (s32)(2.0f * ScaleY) );
    //g_UiMgr->RenderText( FontIndex,
    //                     ShadowRect,
    //                     TextFlags,
    //                     xcolor( 0, 0, 0, (u8)MINMAX( 0, (NameAlpha * 3) / 4, 255 ) ),
    //                     m_NameText );

    //if( ShadowAlpha > 0.0f )
    //{
    //    s32 GlowAlpha = MINMAX( 0, (s32)(255.0f * ShadowAlpha), NameAlpha );
    //
    //    ShadowRect = TextRect;
    //    ShadowRect.Translate( (s32)(ShadowOffsetX * ScaleX), (s32)(ShadowOffsetY * ScaleY) );
    //    g_UiMgr->RenderText( FontIndex,
    //                         ShadowRect,
    //                         TextFlags,
    //                         xcolor( s_LoadNameGlow.R, s_LoadNameGlow.G, s_LoadNameGlow.B, (u8)GlowAlpha ),
    //                         m_NameText );
    //}

    g_UiMgr->RenderText( FontIndex,
                         TextRect,
                         TextFlags,
                         xcolor( 255, 255, 255, (u8)NameAlpha ),
                         m_NameText );
}

//=========================================================================

void dlg_load_game::Render( s32 ox, s32 oy )
{
    (void)ox;
    (void)oy;

    RenderFullscreen( XCOLOR_BLACK );

    if( (m_SlideshowState == STATE_IDLE) || (m_SlideshowState == STATE_SETUP) )
        return;

    s32 SlideAlphas[NUM_SLIDES];
    x_memset( SlideAlphas, 0, sizeof(SlideAlphas) );

    s32 NameAlpha = 0;
    f32 ShadowOffsetX = SHADOW_SLIDE_BEGIN_X;
    f32 ShadowOffsetY = SHADOW_SLIDE_BEGIN_Y;
    f32 ShadowAlpha   = 0.0f;

    if( m_SlideshowState == STATE_SLIDESHOW )
    {
        for( s32 i = 0; i < m_nSlides; i++ )
        {
            SlideAlphas[i] = GetSlideAlpha( m_Slides[i], m_ElapsedTime );
        }

        if( m_ElapsedTime > m_StartTextAnim )
        {
            f32 TextAnimTime = m_ElapsedTime - m_StartTextAnim;

            if( TextAnimTime < SHADOW_SCALE_END_TIME )
            {
                f32 T = (TextAnimTime - SHADOW_SCALE_BEGIN_TIME) /
                        (SHADOW_SCALE_END_TIME - SHADOW_SCALE_BEGIN_TIME);
                T = MINMAX( 0.0f, T, 1.0f );

                ShadowAlpha = T;
                NameAlpha   = MINMAX( 0, (s32)(255.0f * T), 255 );
            }
            else if( TextAnimTime < SHADOW_SLIDE_END_TIME )
            {
                f32 T = (TextAnimTime - SHADOW_SLIDE_BEGIN_TIME) /
                        (SHADOW_SLIDE_END_TIME - SHADOW_SLIDE_BEGIN_TIME);
                T = MINMAX( 0.0f, T, 1.0f );

                ShadowAlpha   = 1.0f - T;
                ShadowOffsetX = SHADOW_SLIDE_BEGIN_X + T * (SHADOW_SLIDE_END_X - SHADOW_SLIDE_BEGIN_X);
                ShadowOffsetY = SHADOW_SLIDE_BEGIN_Y + T * (SHADOW_SLIDE_END_Y - SHADOW_SLIDE_BEGIN_Y);
                NameAlpha     = 255;
            }
            else if( TextAnimTime < SHADOW_FADEIN_END_TIME )
            {
                f32 T = (TextAnimTime - SHADOW_FADEIN_BEGIN_TIME) /
                        (SHADOW_FADEIN_END_TIME - SHADOW_FADEIN_BEGIN_TIME);
                T = MINMAX( 0.0f, T, 1.0f );

                ShadowAlpha = T;
                NameAlpha   = 255;
            }
            else
            {
                ShadowAlpha = 1.0f;
                NameAlpha   = 255;
            }
        }
    }
    else
    {
        NameAlpha   = 255;
        ShadowAlpha = 1.0f;
    }

    for( s32 i = 0; i < m_nSlides; i++ )
    {
        if( SlideAlphas[i] <= 0 )
            continue;

        xcolor C( m_Slides[i].SlideColor.R,
                  m_Slides[i].SlideColor.G,
                  m_Slides[i].SlideColor.B,
                  (u8)SlideAlphas[i] );

        if( m_Slides[i].BitmapName.GetLength() > 0 )
            RenderSlideImage( m_Slides[i], C );
        else
            RenderFullscreen( C );
    }

    if( NameAlpha > 0 )
    {
        RenderLevelName( NameAlpha, ShadowOffsetX, ShadowOffsetY, ShadowAlpha );
    }

    if( m_FinalFadeoutStarted )
    {
        f32 T = MINMAX( 0.0f, m_FadeTimeElapsed / FINAL_FADE_OUT_TIME, 1.0f );
        RenderFullscreen( xcolor( 0, 0, 0, (u8)(255.0f * T) ) );
    }
}

//=========================================================================

void dlg_load_game::OnAccept( ui_win* pWin )
{
    (void)pWin;

    if( (m_SlideshowState == STATE_SLIDESHOW) && m_AllowSkip )
    {
        if( g_AudioMgr.IsValidVoiceIdCached( m_VoiceID ) )
            g_AudioMgr.Release( m_VoiceID, 0.0f );

        m_SlideshowState = STATE_LOADING;
        ResetSlideshowClock();
    }
}

//=========================================================================

void dlg_load_game::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    if( m_SlideshowState == STATE_SLIDESHOW )
    {
        xbool AnyHeld = FALSE;
        xbool SkipPressed = FALSE;
        for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
        {
            const frontend_pad& Pad = g_FrontendInput.GetPad( i );
            AnyHeld |= (Pad.GetFrameLogical( frontend_pad::UI_SELECT   ).GetIsValue() > 0.25f) ||
                       (Pad.GetFrameLogical( frontend_pad::UI_BACK     ).GetIsValue() > 0.25f) ||
                       (Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetIsValue() > 0.25f);
            SkipPressed |= (Pad.GetFrameLogical( frontend_pad::UI_SELECT   ).GetWasValue() > 0.25f) ||
                           (Pad.GetFrameLogical( frontend_pad::UI_BACK     ).GetWasValue() > 0.25f) ||
                           (Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetWasValue() > 0.25f);
        }

        if( m_AllowSkip && SkipPressed )
            OnAccept( pWin );
        else if( !AnyHeld )
            m_AllowSkip = TRUE;
    }

    if( m_SlideshowState == STATE_SLIDESHOW )
    {
        AdvanceSlideshowClock( DeltaTime );

        if( m_LoadingComplete &&
            !m_FinalFadeoutStarted &&
            (m_ElapsedTime > (m_StartTextAnim + SHADOW_SLIDE_BEGIN_TIME)) )
        {
            m_FinalFadeoutStarted = TRUE;
        }

        f32 EndTime = m_StartTextAnim + SHADOW_FADEIN_END_TIME;
        for( s32 i = 0; i < m_nSlides; i++ )
        {
            EndTime = MAX( EndTime, m_Slides[i].EndFadeOut );
        }

        if( m_ElapsedTime > EndTime )
        {
            if( g_AudioMgr.IsValidVoiceIdCached( m_VoiceID ) )
                g_AudioMgr.Release( m_VoiceID, 0.0f );

            m_SlideshowState = STATE_LOADING;
            m_ElapsedTime    = 0.0f;
        }
    }
    else if( m_SlideshowState == STATE_LOADING )
    {
        m_ElapsedTime += DeltaTime;

        if( m_LoadingComplete && !m_FinalFadeoutStarted )
            m_FinalFadeoutStarted = TRUE;
    }

    if( m_FinalFadeoutStarted )
    {
        m_FadeTimeElapsed += DeltaTime;
        if( m_FadeTimeElapsed > (FINAL_FADE_OUT_TIME + 0.11f) )
            m_State = DIALOG_STATE_EXIT;
    }
}

//=========================================================================

void dlg_load_game::StartLoadingProcess( void )
{
    ASSERT( m_SlideshowState == STATE_IDLE );

    ReleaseSlides();
    ResetSlides();

    m_SlideshowState      = STATE_SETUP;
    m_LoadingComplete     = FALSE;
    m_FinalFadeoutStarted = FALSE;
    m_FadeTimeElapsed     = 0.0f;
    ResetSlideshowClock();

    m_NameText.Clear();
    m_NameText += g_ActiveConfig.GetLevelName();

    for( s32 i = 0; i < m_NameText.GetLength(); i++ )
    {
        if( m_NameText[i] == '\\' )
            m_NameText[i] = '/';
    }
}

//=========================================================================

void dlg_load_game::SetTextAnimInfo( f32 StartTextAnim )
{
    ASSERT( m_SlideshowState == STATE_SETUP );
    m_StartTextAnim = StartTextAnim;
}

//=========================================================================

void dlg_load_game::SetVoiceID( s32 VoiceID )
{
    ASSERT( m_SlideshowState == STATE_SETUP );
    m_VoiceID = VoiceID;
}

//=========================================================================

void dlg_load_game::SetNSlides( s32 nSlides )
{
    ASSERT( m_SlideshowState == STATE_SETUP );
    ASSERT( nSlides <= NUM_SLIDES );

    ReleaseSlides();
    m_nSlides = MIN( nSlides, NUM_SLIDES );

    for( s32 i = 0; i < m_nSlides; i++ )
    {
        m_Slides[i].StartFadeIn  = 0.0f;
        m_Slides[i].EndFadeIn    = 1.0f;
        m_Slides[i].StartFadeOut = 2.0f;
        m_Slides[i].EndFadeOut   = 3.0f;
        m_Slides[i].SlideColor   = XCOLOR_WHITE;
    }
}

//=========================================================================

void dlg_load_game::SetSlideInfo( s32         Index,
                                  const char* pTextureName,
                                  f32         StartFadeIn,
                                  f32         EndFadeIn,
                                  f32         StartFadeOut,
                                  f32         EndFadeOut,
                                  xcolor      SlideColor )
{
    ASSERT( m_SlideshowState == STATE_SETUP );
    ASSERT( (Index >= 0) && (Index < m_nSlides) );

    if( (Index < 0) || (Index >= m_nSlides) )
        return;

    ReleaseSlide( m_Slides[Index] );

    m_Slides[Index].StartFadeIn  = StartFadeIn;
    m_Slides[Index].EndFadeIn    = EndFadeIn;
    m_Slides[Index].StartFadeOut = StartFadeOut;
    m_Slides[Index].EndFadeOut   = EndFadeOut;
    m_Slides[Index].SlideColor   = SlideColor;

    if( pTextureName && x_stricmp( pTextureName, "<NULL>" ) )
    {
        const xstring BitmapName = (const char*)xfs( "load_game_slide_%02d", Index );
        if( g_UiMgr && (g_UiMgr->LoadBitmap( BitmapName, pTextureName ) >= 0) )
        {
            m_Slides[Index].BitmapName = BitmapName;
        }
    }
}

//=========================================================================

void dlg_load_game::StartSlideshow( void )
{
    ASSERT( m_SlideshowState == STATE_SETUP );

    m_SlideshowState = (m_nSlides > 0) ? STATE_SLIDESHOW : STATE_LOADING;
    m_AllowSkip = FALSE;
    ResetSlideshowClock();
}

//=========================================================================

void dlg_load_game::LoadingComplete( void )
{
    m_LoadingComplete = TRUE;

    if( m_SlideshowState == STATE_IDLE )
        m_State = DIALOG_STATE_EXIT;
}
