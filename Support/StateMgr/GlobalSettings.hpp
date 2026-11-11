//==============================================================================
//  
//  GlobalSettings.hpp
//  
//==============================================================================

#ifndef GLOBALSETTINGS_HPP
#define GLOBALSETTINGS_HPP

//==============================================================================
// INCLUDES 
//==============================================================================

#include "x_types.hpp"
#include "Entropy.hpp"
#include "NetworkMgr/NetLimits.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "Render/RenderScale.hpp"
#include "Render/AmbientOcclusion.hpp"
#include "Render/ShadowTypes.hpp"


//==============================================================================
// CONST 
//==============================================================================

constexpr s32               MAP_CYCLE_SIZE                      = 32;
constexpr s32               UI_SCALE_MIN_PERCENT                = 60;
constexpr s32               UI_SCALE_MAX_PERCENT                = 140;
constexpr s32               UI_SCALE_DEFAULT_PERCENT            = 120;
constexpr s32               HUD_SCALE_MIN_PERCENT               = 60;
constexpr s32               HUD_SCALE_MAX_PERCENT               = 140;
constexpr s32               HUD_SCALE_DEFAULT_PERCENT           = 80;
constexpr s32               BRIGHTNESS_MIN_PERCENT              = -25;
constexpr s32               BRIGHTNESS_MAX_PERCENT              = 25;
constexpr s32               BRIGHTNESS_DEFAULT_PERCENT          = 0;
constexpr s32               CONTRAST_MIN_PERCENT                = 75;
constexpr s32               CONTRAST_MAX_PERCENT                = 125;
constexpr s32               CONTRAST_DEFAULT_PERCENT            = 100;
constexpr s32               FIELD_OF_VIEW_MIN_DEGREES           = 60;
constexpr s32               FIELD_OF_VIEW_MAX_DEGREES           = 120;
constexpr s32               FIELD_OF_VIEW_DEFAULT_DEGREES       = 100;
constexpr s32               FILM_GRAIN_MIN_STRENGTH             = 0;
constexpr s32               FILM_GRAIN_MAX_STRENGTH             = 150;
constexpr s32               FILM_GRAIN_DEFAULT_STRENGTH         = 100;
constexpr s32               VOLUME_MIN_PERCENT                  = 0;
constexpr s32               VOLUME_MAX_PERCENT                  = 100;
constexpr xbool             POST_EFFECT_BACKGROUND_BLUR_DEFAULT = FALSE;
constexpr xbool             PHONG_SHADING_DEFAULT               = FALSE;
constexpr xbool             TESSELLATION_DEFAULT                = FALSE;
constexpr xbool             XBOX_FOG_DEFAULT                    = FALSE;
constexpr ShadowMode        SHADOW_MODE_DEFAULT                  = ShadowMode::Enabled;
constexpr ShadowFilterType  SHADOW_FILTER_TYPE_DEFAULT          = ShadowFilterType::Evsm;

//------------------------------------------------------------------------------

enum class FrameRateLimit : s32
{
    Auto   = 0,
    Fps30  = 30,
    Fps60  = 60,
    Fps90  = 90,
    Fps120 = 120,
    Fps144 = 144,
    Fps165 = 165,
    Fps240 = 240,
};

constexpr FrameRateLimit    FRAME_RATE_LIMIT_DEFAULT             = FrameRateLimit::Auto;

//------------------------------------------------------------------------------

enum class AntiAliasingType : s32
{
    None,
    Cmaa2,
    Msaa,
};

constexpr AntiAliasingType   ANTI_ALIASING_TYPE_DEFAULT           = AntiAliasingType::None;

constexpr rstate_texture_filtering TEXTURE_FILTERING_DEFAULT = RSTATE_TEXTURE_FILTER_ANISOTROPIC_16;

//------------------------------------------------------------------------------

enum volume_controls
{
    VOLUME_SFX,
    VOLUME_MUSIC,
    VOLUME_SPEECH,
    VOLUME_SPEAKER,
    VOLUME_MIC,
    VOLUME_LAST,
};

//------------------------------------------------------------------------------

enum headset_mode
{
    HEADSET_HEADSET_ONLY,
    HEADSET_THROUGH_SPEAKERS,
    HEADSET_DISABLED,
};

//==============================================================================
// MAP SETTINGS
//==============================================================================

struct map_settings
{
    void            Reset( void );

    s32             m_MapCycle[MAP_CYCLE_SIZE];   
    s32             m_MapCycleIdx;                
    s32             m_MapCycleCount;              
    xbool           m_bUseDefault;                
};

//==============================================================================
// MP SETTINGS
//==============================================================================

struct multi_settings
{
    s32             m_GameTypeID;
    s32             m_ScoreLimit;
    s32             m_TimeLimit;
    mutation_mode   m_MutationMode;
    map_settings    m_MapSettings;
};

//==============================================================================
// HOST SETTINGS
//==============================================================================

struct host_settings
{
    void            Reset( void );

    xwchar          m_ServerName[NET_SERVER_NAME_LENGTH];
    char            m_Password[NET_PASSWORD_LENGTH];
    s32             m_GameTypeID;
    s32             m_ScoreLimit;
    s32             m_TimeLimit;
    s32             m_MaxPlayers;
    s32             m_VotePassPct;
    s32             m_FFirePct;
    s32             m_Flags;
    mutation_mode   m_MutationMode;
    map_settings    m_MapSettings;
    skill_level     m_SkillLevel;
};


//------------------------------------------------------------------------------

enum settings_reset_flags
{
    RESET_HEADSET      = 1,
    RESET_AUDIO        = 2,
    RESET_LOCALIZATION = 4,
    RESET_GRAPHICS     = 8,
    RESET_DISPLAY      = 16,
    RESET_INTERFACE    = 32,
    RESET_ALL          = -1,
};

//==============================================================================
// GLOBAL SETTINGS
//==============================================================================


class global_settings
{
    friend class SaveDataCodec;

public:
                        global_settings         ( void );
    void                Reset                   ( s32 Flags = RESET_ALL );
    void                Commit                  ( void );
    void                CommitStartup           ( void );
    void                CommitAudio             ( void );
    void                CommitLocalization      ( void );
    void                CommitGraphics          ( void );
    void                CaptureGraphics         ( void );
    static void         SetDefaultLocalizationLanguages
                                                ( x_language TextLanguage,
                                                  x_language AudioLanguage,
                                                  x_language VideoLanguage );
    static x_language   GetDefaultLocalizationLanguage
                                                ( void );
    multi_settings&     GetMultiplayerSettings  ( void );
    host_settings&      GetHostSettings         ( void );
    map_settings&       GetMapSettings          ( void );
    xbool               HasChanged              ( void );
    void                MarkDirty               ( void );
    void                Checksum                ( void );
    s32                 GetVolume               ( volume_controls Control ) const;
    void                SetVolume               ( volume_controls Control, s32 Value );
    s32                 GetVideoVolume          ( void ) const;
    void                SetVideoVolume          ( s32 Value );
    x_language          GetTextLanguage         ( void ) const;
    void                SetTextLanguage         ( x_language Language );
    x_language          GetAudioLanguage        ( void ) const;
    void                SetAudioLanguage        ( x_language Language );
    x_language          GetVideoLanguage        ( void ) const;
    void                SetVideoLanguage        ( x_language Language );
    void                SetSpeakerSet           ( s32 SpeakerSet );
    s32                 GetSpeakerSet           ( void ) const;
    void                SetUIScale              ( s32 Percent );
    s32                 GetUIScale              ( void ) const;
    void                SetHUDScale             ( s32 Percent );
    s32                 GetHUDScale             ( void ) const;
    void                SetBrightness           ( s32 Percent );
    s32                 GetBrightness           ( void ) const;
    void                SetContrast             ( s32 Percent );
    s32                 GetContrast             ( void ) const;
    void                SetRenderScale          ( s32 Percent );
    s32                 GetRenderScale          ( void ) const;
    void                SetDisplayResolution    ( s32 Width, s32 Height );
    s32                 GetDisplayWidth         ( void ) const;
    s32                 GetDisplayHeight        ( void ) const;
    void                SetDisplayMode          ( eng_display_mode Mode );
    eng_display_mode    GetDisplayMode          ( void ) const;
    void                SetPresentMode          ( eng_present_mode Mode );
    eng_present_mode    GetPresentMode          ( void ) const;
    void                SetFrameRateLimit       ( FrameRateLimit Limit );
    FrameRateLimit      GetFrameRateLimit       ( void ) const;
    void                SetFieldOfView          ( s32 Degrees );
    s32                 GetFieldOfView          ( void ) const;
    void                SetFilmGrainStrength    ( s32 Strength );
    s32                 GetFilmGrainStrength    ( void ) const;
    void                SetBackgroundBlurEnabled( xbool Enabled );
    xbool               GetBackgroundBlurEnabled( void ) const;
    void                SetAntiAliasingType     ( AntiAliasingType Type );
    AntiAliasingType    GetAntiAliasingType     ( void ) const;
    void                SetAmbientOcclusionQuality( render::AmbientOcclusionQuality Quality );
    render::AmbientOcclusionQuality GetAmbientOcclusionQuality( void ) const;
    void                SetTextureFiltering     ( rstate_texture_filtering Filtering );
    rstate_texture_filtering GetTextureFiltering( void ) const;
    void                SetPhongShadingEnabled  ( xbool Enabled );
    xbool               GetPhongShadingEnabled  ( void ) const;
    void                SetTessellationEnabled  ( xbool Enabled );
    xbool               GetTessellationEnabled  ( void ) const;
    void                SetXboxFogEnabled       ( xbool Enabled );
    xbool               GetXboxFogEnabled       ( void ) const;
    void                SetShadowMode           ( ShadowMode Mode );
    ShadowMode          GetShadowMode           ( void ) const;
    void                SetShadowFilterType     ( ShadowFilterType Type );
    ShadowFilterType    GetShadowFilterType     ( void ) const;
    headset_mode        GetHeadsetMode          ( void ) const;
    void                SetHeadsetMode          ( headset_mode Mode );
    void                UpdateHeadsetMode       ( headset_mode Mode );
    datestamp           GetDateStamp            ( void ) const;
    s32                 GetContentVersion       ( void ) const;
    void                SetContentVersion       ( s32 Version );
    void*               GetPatchBuffer          ( void );
    void                SetPatchData            ( void const* pData, s32 Length, s32 Version );
    void*               GetPatchData            ( s32& Length, s32& Version );

private:
    static xbool        IsFrameRateLimitValid   ( FrameRateLimit Limit );
    void                ResetMultiplayerDefaults( void );
    void                ResetHostDefaults       ( void );
    void                ResetAudioDefaults      ( void );
    void                ResetHeadsetDefaults    ( void );
    void                ResetLocalizationDefaults
                                                ( void );
    void                ResetGraphicsDefaults   ( void );
    void                ResetDisplayDefaults    ( void );
    void                ResetInterfaceDefaults  ( void );
    void                NormalizeGraphicsSettings
                                                ( void );
    void                CommitInterfaceSettings ( void );
    void                CommitRenderSettings    ( void );
    void                CommitDisplaySettings   ( void );

    s32                 m_Checksum;
    datestamp           m_DateStamp;
    s32                 m_Volume[VOLUME_LAST];
    s32                 m_SpeakerSet;
    s32                 m_ContentVersion;
    s32                 m_PatchVersion;
    s32                 m_PatchLength;
    byte                m_PatchData[NET_MAX_PATCH_SIZE];
    headset_mode        m_HeadsetMode;

    multi_settings      m_MultiplayerSettings;
    host_settings       m_HostSettings;
    map_settings        m_MapSettings;

    // Persisted interface, display, and presentation settings.
    s32                 m_UIScale;
    s32                 m_HUDScale;
    s32                 m_Brightness;
    s32                 m_Contrast;
    s32                 m_RenderScale;
    s32                 m_ResolutionWidth;
    s32                 m_ResolutionHeight;
    eng_display_mode    m_DisplayMode;
    eng_present_mode    m_PresentMode;
    FrameRateLimit      m_FrameRateLimit;
    s32                 m_FieldOfView;
    s32                 m_FilmGrainStrength;
    xbool               m_BackgroundBlurEnabled;
    AntiAliasingType    m_AntiAliasingType;
    render::AmbientOcclusionQuality m_AmbientOcclusionQuality;
    rstate_texture_filtering m_TextureFiltering;
    xbool               m_PhongShadingEnabled;
    xbool               m_TessellationEnabled;
    xbool               m_XboxFogEnabled;
    ShadowMode          m_ShadowMode;
    ShadowFilterType    m_ShadowFilterType;
    s32                 m_VideoVolume;
    x_language          m_TextLanguage;
    x_language          m_AudioLanguage;
    x_language          m_VideoLanguage;
};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline multi_settings& global_settings::GetMultiplayerSettings( void )
{
    return m_MultiplayerSettings;
}

//==============================================================================

inline host_settings& global_settings::GetHostSettings( void )
{
    return m_HostSettings;
}

//==============================================================================

inline map_settings& global_settings::GetMapSettings( void )
{
    return m_MapSettings;
}

//==============================================================================

inline s32 global_settings::GetVolume( volume_controls Control ) const
{
    ASSERT( (Control >= VOLUME_SFX) && (Control < VOLUME_LAST) );
    return m_Volume[Control];
}

//==============================================================================

inline void global_settings::SetVolume( volume_controls Control, s32 Value )
{
    ASSERT( (Control >= VOLUME_SFX) && (Control < VOLUME_LAST) );
    m_Volume[Control] = x_clamp( Value, VOLUME_MIN_PERCENT, VOLUME_MAX_PERCENT );
}

//==============================================================================

inline s32 global_settings::GetVideoVolume( void ) const
{
    return m_VideoVolume;
}

//==============================================================================

inline void global_settings::SetVideoVolume( s32 Value )
{
    m_VideoVolume = Value;
}

//==============================================================================

inline x_language global_settings::GetTextLanguage( void ) const
{
    return m_TextLanguage;
}

//==============================================================================

inline void global_settings::SetTextLanguage( x_language Language )
{
    ASSERT( (Language >= XL_LANG_ENGLISH) && (Language < XL_NUM_LANGUAGES) );
    m_TextLanguage = Language;
}

//==============================================================================

inline x_language global_settings::GetAudioLanguage( void ) const
{
    return m_AudioLanguage;
}

//==============================================================================

inline void global_settings::SetAudioLanguage( x_language Language )
{
    ASSERT( (Language >= XL_LANG_ENGLISH) && (Language < XL_NUM_LANGUAGES) );
    m_AudioLanguage = Language;
}

//==============================================================================

inline x_language global_settings::GetVideoLanguage( void ) const
{
    return m_VideoLanguage;
}

//==============================================================================

inline void global_settings::SetVideoLanguage( x_language Language )
{
    ASSERT( (Language >= XL_LANG_ENGLISH) && (Language < XL_NUM_LANGUAGES) );
    m_VideoLanguage = Language;
}

//==============================================================================

inline void global_settings::SetSpeakerSet( s32 SpeakerSet )
{
    m_SpeakerSet = SpeakerSet;
}

//==============================================================================

inline s32 global_settings::GetSpeakerSet( void ) const
{
    return m_SpeakerSet;
}

//==============================================================================

inline s32 global_settings::GetUIScale( void ) const
{
    return m_UIScale;
}

//==============================================================================

inline s32 global_settings::GetHUDScale( void ) const
{
    return m_HUDScale;
}

//==============================================================================

inline s32 global_settings::GetDisplayWidth( void ) const
{
    return m_ResolutionWidth;
}

//==============================================================================

inline s32 global_settings::GetDisplayHeight( void ) const
{
    return m_ResolutionHeight;
}

//==============================================================================

inline void global_settings::SetDisplayMode( eng_display_mode Mode )
{
    ASSERT( (Mode == ENG_DISPLAY_WINDOWED) || (Mode == ENG_DISPLAY_BORDERLESS) );
    m_DisplayMode = Mode;
}

//==============================================================================

inline eng_display_mode global_settings::GetDisplayMode( void ) const
{
    return m_DisplayMode;
}

//==============================================================================

inline void global_settings::SetPresentMode( eng_present_mode Mode )
{
    ASSERT( (Mode == ENG_PRESENT_VSYNC) ||
            (Mode == ENG_PRESENT_MAILBOX) ||
            (Mode == ENG_PRESENT_IMMEDIATE) );
    m_PresentMode = Mode;

    if( Mode != ENG_PRESENT_IMMEDIATE )
    {
        m_FrameRateLimit = FrameRateLimit::Auto;
    }
}

//==============================================================================

inline eng_present_mode global_settings::GetPresentMode( void ) const
{
    return m_PresentMode;
}

//==============================================================================

inline void global_settings::SetFrameRateLimit( FrameRateLimit Limit )
{
    xbool const IsValid = IsFrameRateLimitValid( Limit );
    ASSERT( IsValid );

    if( !IsValid ||
        (m_PresentMode != ENG_PRESENT_IMMEDIATE) )
    {
        m_FrameRateLimit = FrameRateLimit::Auto;
        return;
    }

    m_FrameRateLimit = Limit;
}

//==============================================================================

inline FrameRateLimit global_settings::GetFrameRateLimit( void ) const
{
    return m_FrameRateLimit;
}

//==============================================================================

inline s32 global_settings::GetFieldOfView( void ) const
{
    return m_FieldOfView;
}

//==============================================================================

inline s32 global_settings::GetFilmGrainStrength( void ) const
{
    return m_FilmGrainStrength;
}

//==============================================================================

inline headset_mode global_settings::GetHeadsetMode( void ) const
{
    return m_HeadsetMode;
}

//==============================================================================

inline void global_settings::SetHeadsetMode( headset_mode Mode )
{
    m_HeadsetMode = Mode;
}

//==============================================================================

inline datestamp global_settings::GetDateStamp( void ) const
{
    return m_DateStamp;
}

//==============================================================================

inline s32 global_settings::GetContentVersion( void ) const
{
    return m_ContentVersion;
}

//==============================================================================

inline void global_settings::SetContentVersion( s32 Version )
{
    m_ContentVersion = Version;
}

//==============================================================================

inline void* global_settings::GetPatchBuffer( void )
{
    return m_PatchData;
}

//==============================================================================

inline void* global_settings::GetPatchData( s32& Length, s32& Version )
{
    Length  = m_PatchLength;
    Version = m_PatchVersion;
    return m_PatchData;
}

//==============================================================================
#endif // GLOBALSETTINGS_HPP
//==============================================================================
