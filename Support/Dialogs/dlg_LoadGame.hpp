//==============================================================================
//
//  dlg_LoadGame.hpp
//
//==============================================================================

#ifndef DLG_LOAD_GAME_HPP
#define DLG_LOAD_GAME_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "UI/ui_dialog.hpp"

//==============================================================================
//  dlg_load_game
//==============================================================================

extern void     dlg_load_game_register  ( ui_manager* pManager );
extern ui_win*  dlg_load_game_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class dlg_load_game : public ui_dialog
{
public:
    enum slideshow_state
    {
        STATE_IDLE = 0,
        STATE_SETUP,
        STATE_SLIDESHOW,
        STATE_LOADING,
    };

                        dlg_load_game       ( void );
    virtual            ~dlg_load_game       ( void );

    xbool               Create              ( s32                       UserID,
                                              ui_manager*               pManager,
                                              ui_manager::dialog_tem*   pDialogTem,
                                              const irect&              Position,
                                              ui_win*                   pParent,
                                              s32                       Flags,
                                              void*                     pUserData );
    virtual void        Destroy             ( void );
    virtual void        Render              ( s32 ox=0, s32 oy=0 );

    virtual void        OnAccept         ( ui_win* pWin );
    virtual void        OnUpdate            ( ui_win* pWin, f32 DeltaTime );

            void        StartLoadingProcess ( void );
            void        SetTextAnimInfo     ( f32               StartTextAnim );
            void        SetNSlides          ( s32               nSlides );
            void        SetSlideInfo        ( s32               Index,
                                              const char*       pTextureName,
                                              f32               StartFadeIn,
                                              f32               EndFadeIn,
                                              f32               StartFadeOut,
                                              f32               EndFadeOut,
                                              xcolor            SlideColor );
            void        SetVoiceID          ( s32               VoiceID );
            void        StartSlideshow      ( void );
            void        LoadingComplete     ( void );

 slideshow_state        GetSlideShowState   ( void ) { return m_SlideshowState; }

protected:
    enum { NUM_SLIDES = 16 };

    struct slide_info
    {
        f32     StartFadeIn;
        f32     EndFadeIn;
        f32     StartFadeOut;
        f32     EndFadeOut;
        xcolor  SlideColor;
        xstring BitmapName;
    };

    void                ReleaseSlide          ( slide_info& Slide );
    void                ResetSlides          ( void );
    void                ReleaseSlides        ( void );
    void                ResetSlideshowClock  ( void );
    void                AdvanceSlideshowClock( f32 DeltaTime );
    irect               ScaleBaseRect        ( s32 L, s32 T, s32 R, s32 B ) const;
    irect               GetScreenRect        ( void ) const;
    irect               GetLevelNameRect     ( void ) const;
    s32                 GetSlideAlpha        ( const slide_info& Slide, f32 Time ) const;
    void                RenderFullscreen     ( xcolor Color );
    void                RenderSlideImage     ( const slide_info& Slide, xcolor Color );
    void                RenderLevelName      ( s32 NameAlpha,
                                               f32 ShadowOffsetX,
                                               f32 ShadowOffsetY,
                                               f32 ShadowAlpha );

    s32                 m_VoiceID;
    s32                 m_nSlides;
    slide_info          m_Slides[NUM_SLIDES];
    f32                 m_StartTextAnim;
    xwstring            m_NameText;

    slideshow_state     m_SlideshowState;
    xbool               m_AllowSkip;
    xbool               m_LoadingComplete;
    xbool               m_FinalFadeoutStarted;
    f32                 m_ElapsedTime;
    f32                 m_FadeTimeElapsed;
    f32                 m_AudioPrevTime;
    f32                 m_AudioPredictedTime;
};

//==============================================================================
#endif // DLG_LOAD_GAME_HPP
//==============================================================================
