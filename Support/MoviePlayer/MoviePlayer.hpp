//==============================================================================
//
//  MoviePlayer.hpp
//
//==============================================================================

#ifndef MOVIEPLAYER_HPP
#define MOVIEPLAYER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_target.hpp"
#include "x_types.hpp"
#include "x_math.hpp"

s32 PlaySimpleMovie( const char* movieName );

class movie_private;

//==============================================================================
// MOVIE PLAYER CLASS
//==============================================================================

class movie_player
{
public:

                movie_player    ( void );
               ~movie_player    ( void );

    xbool       Open            ( const char* pFilename, xbool PlayResident, xbool IsLooped );
    void        Close           ( void );
    void        Init            ( void );
    void        Kill            ( void );
    void        Play            ( void );
    void        Stop            ( void );
    void        Render          ( xbool InRenderLoop = FALSE );
    void        SetVolume       ( f32 Volume );
    void        SetLanguage     ( x_language Language );
    x_language  GetLanguage     ( void ) const { return m_Language; };
    void        Pause           ( void );
    void        Resume          ( void );

    xbool       IsPlaying       ( void );
    s32         GetWidth        ( void );
    s32         GetHeight       ( void );

private:

    movie_private*  m_pPrivate;
    xbool           m_IsLooped;
    xbool           m_Finished;
    f32             m_Volume;
    x_language      m_Language;
};

extern movie_player Movie;

//==============================================================================

xbool       movie_Play          ( const char* pFilename, const xbool IsLooped = FALSE, const xbool Modal = TRUE );
void        movie_Stop          ( void );
void        movie_Pause         ( void );
xbitmap*    movie_GetBitmap     ( void );
xbool       movie_IsPlaying     ( void );
xbool       movie_LockBitmap    ( void );
void        movie_UnlockBitmap  ( void );

#endif // MOVIEPLAYER_HPP
