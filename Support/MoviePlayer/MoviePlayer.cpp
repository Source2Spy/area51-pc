//==============================================================================
//
//  MoviePlayer.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "MoviePlayer.hpp"
#include "MoviePlayer_WebM/MoviePlayer_WebM_Private.hpp"
#include "StateMgr/StateMgr.hpp"
#include "InputMgr/GamePad.hpp"
#include "Render/Render.hpp"

//=========================================================================
// FUNCTIONS
//=========================================================================

static
xbool movieplayer_BeginTimedFrame( xtimer& FrameTimer, f32& DeltaTime )
{
    static const f32 k_MaxFrameDeltaSeconds = 0.1f;

    while( eng_BeginFrame() )
    {
        render::BeginFrame();
        DeltaTime = FrameTimer.TripSec();

        if( x_isvalid( DeltaTime ) &&
            ( DeltaTime > 0.0f ) &&
            ( DeltaTime <= k_MaxFrameDeltaSeconds ) )
        {
            return TRUE;
        }

        // Balance both frame scopes before retrying an invalid timer sample.
        x_DebugMsg( "MoviePlayer: discarding frame with invalid delta %.6f\n", DeltaTime );

        if( !render::EndFrame() || !eng_EndFrame() )
        {
            return FALSE;
        }
    }

    return FALSE;
}

//==============================================================================

movie_player::movie_player( void )
{
    m_pPrivate = new movie_private;
    m_IsLooped = FALSE;
    m_Finished = FALSE;
    m_Volume   = 1.0f;
    m_Language = XL_LANG_ENGLISH;
}

//==============================================================================

movie_player::~movie_player( void )
{
    Kill();
    delete m_pPrivate;
    m_pPrivate = NULL;
}

//==============================================================================

void movie_player::Init( void )
{
    m_pPrivate->Init();
}

//==============================================================================

xbool movie_player::Open( const char* pFilename, xbool IsResident, xbool IsLooped )
{
    m_pPrivate->SetVolume( m_Volume );

    if( !m_pPrivate->Open( pFilename, IsResident, IsLooped, m_Language ) )
    {
        Kill();
        m_Finished = TRUE;
        return FALSE;
    }

    m_IsLooped = IsLooped;
    m_Finished = FALSE;

    return TRUE;
}

//==============================================================================

void movie_player::SetVolume( f32 Volume )
{
    m_Volume = x_clamp( Volume, 0.0f, 1.0f );
    m_pPrivate->SetVolume( m_Volume );
}

//==============================================================================

void movie_player::SetLanguage( x_language Language )
{
    ASSERT( ( Language >= XL_LANG_ENGLISH ) && ( Language < XL_NUM_LANGUAGES ) );
    m_Language = Language;
}

//==============================================================================

void movie_player::Close( void )
{
    if( !m_pPrivate->IsRunning() )
    {
        return;
    }

    m_pPrivate->Close();
}

//==============================================================================

void movie_player::Kill( void )
{
    m_pPrivate->Kill();
}

//==============================================================================

void movie_player::Render( xbool InRenderLoop )
{
    if( !InRenderLoop )
    {
        if( eng_Begin( "Movie" ) )
        {
            m_pPrivate->Render();
            eng_End();
        }
    }
    else
    {
        m_pPrivate->Render();
    }
}

//==============================================================================

void movie_player::Pause( void )
{
    m_pPrivate->Pause();
}

//==============================================================================

void movie_player::Resume( void )
{
    m_pPrivate->Resume();
}

//==============================================================================

xbool movie_player::IsPlaying( void )
{
    return !m_pPrivate->IsFinished();
}

//==============================================================================

s32 movie_player::GetWidth( void )
{
    return m_pPrivate->GetWidth();
}

//==============================================================================

s32 movie_player::GetHeight( void )
{
    return m_pPrivate->GetHeight();
}

//==============================================================================

s32 PlaySimpleMovie( const char* movieName )
{
    xbool ret = FALSE;

    Movie.Init();
    ret = Movie.Open( movieName, FALSE, FALSE );

    if( ret )
    {
        xbool  done      = FALSE;
        xbool  allowSkip = FALSE;
        xtimer FrameTimer;
        FrameTimer.Start();

        while( !done )
        {
            if( !Movie.IsPlaying() )
            {
                break;
            }

            f32 DeltaTime = 0.0f;

            if( !movieplayer_BeginTimedFrame( FrameTimer, DeltaTime ) )
            {
                break;
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.Update" );

                g_FrontendInput.Update( DeltaTime );
                g_NetworkMgr.UpdateFrame( DeltaTime );

                // Wait until the user releases all keys, otherwise a movie can
                // be skipped by the input that opened it.
                xbool anyHeld = FALSE;

                for( s32 i = 0; i < frontend_input::MAX_CONTROLLERS; i++ )
                {
                    const frontend_pad& Pad = g_FrontendInput.GetPad( i );

                    if( ( Pad.GetFrameLogical( frontend_pad::UI_SELECT ).GetIsValue() > 0.25f ) ||
                        ( Pad.GetFrameLogical( frontend_pad::UI_BACK ).GetIsValue() > 0.25f ) ||
                        ( Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetIsValue() > 0.25f ) )
                    {
                        anyHeld = TRUE;
                    }

                    if( allowSkip )
                    {
                        if( ( Pad.GetFrameLogical( frontend_pad::UI_SELECT ).GetWasValue() > 0.25f ) ||
                            ( Pad.GetFrameLogical( frontend_pad::UI_BACK ).GetWasValue() > 0.25f ) ||
                            ( Pad.GetFrameLogical( frontend_pad::UI_ACTIVATE ).GetWasValue() > 0.25f ) )
                        {
                            done = TRUE;
                        }
                    }
                }

                if( !anyHeld )
                {
                    allowSkip = TRUE;
                }
            }

            Movie.Render();

            if( !render::EndFrame() || !eng_EndFrame() )
            {
                break;
            }
        }

        // Submit the final clear frame before closing the movie resources.
        if( eng_BeginFrame() )
        {
            render::BeginFrame();
            xbool const renderReady = render::EndFrame();
            xbool const engineReady = eng_EndFrame();

            if( renderReady && !engineReady )
                x_throw( "MoviePlayer: failed to submit final clear frame" );
        }

        Movie.Close();
    }

    Movie.Kill();

    return ( ret );
}
