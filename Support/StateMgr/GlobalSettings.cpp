//==============================================================================
//  
//  GlobalSettings.cpp
//  
//==============================================================================
// INCLUDES 
//==============================================================================

#include "GlobalSettings.hpp"
#include "Configuration/GameConfig.hpp"
#include "AudioMgr/AudioMgr.hpp"
#include "StateMgr.hpp"
#include "NetworkMgr/Voice/VoiceMgr.hpp"
#include "NetworkMgr/Messages/Messages.hpp"
#include "StringMgr/StringMgr.hpp"
#include "MoviePlayer/MoviePlayer.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_renderer.hpp"
#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/Player/Player.hpp"
#include "Objects/Render/PostEffectMgr.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

//==============================================================================
// CONST 
//==============================================================================

static x_language s_DefaultTextLanguage  = XL_LANG_ENGLISH;
static x_language s_DefaultAudioLanguage = XL_LANG_ENGLISH;
static x_language s_DefaultVideoLanguage = XL_LANG_ENGLISH;

//==============================================================================
// IMPLEMENTATION 
//==============================================================================

global_settings::global_settings( void )
{
    Reset( RESET_ALL );
}

//==============================================================================

void global_settings::Reset( s32 ResetFlags )
{
    if( ResetFlags == RESET_ALL )
    {
        x_memset( this, 0, sizeof(*this) );

        ResetMultiplayerDefaults();
        ResetHostDefaults();
    }

    if( ResetFlags & RESET_HEADSET )
    {
        ResetHeadsetDefaults();
    }

    if( ResetFlags & RESET_AUDIO )
    {
        ResetAudioDefaults();
    }

    if( ResetFlags & RESET_LOCALIZATION )
    {
        ResetLocalizationDefaults();
    }

    if( ResetFlags & RESET_GRAPHICS )
    {
        ResetGraphicsDefaults();
    }

    if( ResetFlags & RESET_DISPLAY )
    {
        ResetDisplayDefaults();
    }

    if( ResetFlags & RESET_INTERFACE )
    {
        ResetInterfaceDefaults();
    }

    if( ResetFlags == RESET_ALL )
    {
        m_Checksum = 0;
        m_Checksum = x_chksum( this, sizeof(*this) );
    }
}

//==============================================================================

void map_settings::Reset( void )
{
    m_bUseDefault   = TRUE;
    m_MapCycleCount = 0;
    m_MapCycleIdx   = 0;

    for( s32 i = 0; i < MAP_CYCLE_SIZE; i++ )
    {
        m_MapCycle[i] = -1;
    }
}

//==============================================================================

void global_settings::ResetMultiplayerDefaults( void )
{
    m_MultiplayerSettings.m_GameTypeID   = GAME_DM;
    m_MultiplayerSettings.m_ScoreLimit   = -1;
    m_MultiplayerSettings.m_TimeLimit    = -1;
    m_MultiplayerSettings.m_MutationMode = MUTATE_CHANGE_AT_WILL;
    m_MultiplayerSettings.m_MapSettings.Reset();
}

//==============================================================================

void global_settings::ResetHostDefaults( void )
{
    m_HostSettings.Reset();
}

//==============================================================================

void host_settings::Reset( void )
{
    x_memset( this, 0, sizeof(*this) );

    m_GameTypeID   = GAME_DM;
    m_ScoreLimit   = -1;
    m_TimeLimit    = -1;
    m_MaxPlayers   = 16;
    m_VotePassPct  = 50;
    m_Flags        = SERVER_ENABLE_MAP_SCALING | SERVER_VOICE_ENABLED;
    m_MutationMode = MUTATE_CHANGE_AT_WILL;
    m_MapSettings.Reset();
}

//==============================================================================

void global_settings::ResetAudioDefaults( void )
{
    SetVolume( VOLUME_SFX, 100 );
    SetVolume( VOLUME_MUSIC, 75 );
    SetVolume( VOLUME_SPEECH, 100 );
    SetVideoVolume( 100 );
    SetSpeakerSet( SPEAKERS_STEREO );
}

//==============================================================================

void global_settings::ResetHeadsetDefaults( void )
{
    SetVolume( VOLUME_MIC, 50 );
    SetVolume( VOLUME_SPEAKER, 100 );
    SetHeadsetMode( HEADSET_HEADSET_ONLY );
}

//==============================================================================

void global_settings::ResetLocalizationDefaults( void )
{
    SetTextLanguage ( s_DefaultTextLanguage );
    SetAudioLanguage( s_DefaultAudioLanguage );
    SetVideoLanguage( s_DefaultVideoLanguage );
}

//==============================================================================

void global_settings::SetDefaultLocalizationLanguages( x_language TextLanguage,
                                                       x_language AudioLanguage,
                                                       x_language VideoLanguage )
{
    ASSERT( (TextLanguage >= XL_LANG_ENGLISH) && (TextLanguage < XL_NUM_LANGUAGES) );
    ASSERT( (AudioLanguage >= XL_LANG_ENGLISH) && (AudioLanguage < XL_NUM_LANGUAGES) );
    ASSERT( (VideoLanguage >= XL_LANG_ENGLISH) && (VideoLanguage < XL_NUM_LANGUAGES) );
    s_DefaultTextLanguage  = TextLanguage;
    s_DefaultAudioLanguage = AudioLanguage;
    s_DefaultVideoLanguage = VideoLanguage;
}

//==============================================================================

x_language global_settings::GetDefaultLocalizationLanguage( void )
{
    return s_DefaultTextLanguage;
}

//==============================================================================

void global_settings::ResetGraphicsDefaults( void )
{
    SetFieldOfView( FIELD_OF_VIEW_DEFAULT_DEGREES );
    SetFilmGrainStrength( FILM_GRAIN_DEFAULT_STRENGTH );
    SetBackgroundBlurEnabled( POST_EFFECT_BACKGROUND_BLUR_DEFAULT );
    SetAntiAliasingType( ANTI_ALIASING_TYPE_DEFAULT );
    SetAmbientOcclusionQuality( render::AMBIENT_OCCLUSION_DEFAULT );
    SetTextureFiltering( TEXTURE_FILTERING_DEFAULT );
    SetRenderScale( RENDER_SCALE_DEFAULT_PERCENT );
    SetPhongShadingEnabled( PHONG_SHADING_DEFAULT );
    SetTessellationEnabled( TESSELLATION_DEFAULT );
    SetXboxFogEnabled( XBOX_FOG_DEFAULT );
    SetShadowMode( SHADOW_MODE_DEFAULT );
    SetShadowFilterType( SHADOW_FILTER_TYPE_DEFAULT );
}

//==============================================================================

void global_settings::ResetDisplayDefaults( void )
{
    SetBrightness( BRIGHTNESS_DEFAULT_PERCENT );
    SetContrast( CONTRAST_DEFAULT_PERCENT );
    SetDisplayResolution( 0, 0 );
    SetDisplayMode( ENG_DISPLAY_BORDERLESS );
    SetPresentMode( ENG_PRESENT_VSYNC );
    SetFrameRateLimit( FRAME_RATE_LIMIT_DEFAULT );
}

//==============================================================================

void global_settings::ResetInterfaceDefaults( void )
{
    SetUIScale( UI_SCALE_DEFAULT_PERCENT );
    SetHUDScale( HUD_SCALE_DEFAULT_PERCENT );
}

//==============================================================================

void global_settings::Commit( void )
{
    if( !m_HostSettings.m_ServerName[0] )
    {
        x_wstrncpy( m_HostSettings.m_ServerName,
                    g_StringTableMgr( "ui", "IDS_HOST_SERVER_NAME" ), NET_SERVER_NAME_LENGTH - 1 );
        m_HostSettings.m_ServerName[NET_SERVER_NAME_LENGTH-1] = 0;
    }

    // Commit any settings required while hosting a game.
    g_PendingConfig.SetServerName( m_HostSettings.m_ServerName );

    CommitAudio();
    CommitLocalization();
    CommitGraphics();

    if( g_NetworkMgr.IsServer() )
    {
        g_VoiceMgr.SetIsGameVoiceEnabled( m_HostSettings.m_Flags & SERVER_VOICE_ENABLED );
    }

    UpdateHeadsetMode( GetHeadsetMode() );

    Checksum();
}

//==============================================================================

void global_settings::CommitStartup( void )
{
    NormalizeGraphicsSettings();
    rstate_SetTextureFiltering( GetTextureFiltering() );
    render::GetRenderPipeline().SetRenderScale( static_cast<f32>( GetRenderScale() ) / 100.0f );
    render::GetRenderPipeline().SetSampleCount(
        (GetAntiAliasingType() == AntiAliasingType::Msaa) ? 4 : 1 );
    render::GetRenderPipeline().SetCmaa2Enabled(
        GetAntiAliasingType() == AntiAliasingType::Cmaa2 );
    render::GetRenderPipeline().GetXeGtaoFeature().SetQuality( GetAmbientOcclusionQuality() );
    render::GetRenderPipeline().SetPhongShadingEnabled( GetPhongShadingEnabled() );
    render::GetRenderPipeline().SetTessellationEnabled( GetTessellationEnabled() );
    render::GetRenderPipeline().GetPostProcessFeatureRef().SetXboxFogEnabled( GetXboxFogEnabled() );

    x_SetLocale( GetTextLanguage() );
    Movie.SetLanguage( GetVideoLanguage() );
    Movie.SetVolume( static_cast<f32>( GetVideoVolume() ) / 100.0f );

    CommitDisplaySettings();
}

//==============================================================================

void global_settings::CommitAudio( void )
{
    // set up audio controls
    g_AudioMgr.SetSFXVolume  ( static_cast<f32>( GetVolume( VOLUME_SFX    ) ) / 100.0f );
    g_AudioMgr.SetMusicVolume( static_cast<f32>( GetVolume( VOLUME_MUSIC  ) ) / 100.0f );
    g_AudioMgr.SetVoiceVolume( static_cast<f32>( GetVolume( VOLUME_SPEECH ) ) / 100.0f );
    Movie.SetVolume( static_cast<f32>( GetVideoVolume() ) / 100.0f );

    // Setup headset volumes
    f32 const HeadsetVolume    = static_cast<f32>( GetVolume( VOLUME_SPEAKER ) ) / 100.0f;
    f32 const MicrophoneVolume = static_cast<f32>( GetVolume( VOLUME_MIC     ) ) / 100.0f;
    g_VoiceMgr.GetHeadset().SetVolume( HeadsetVolume, MicrophoneVolume );
}

//==============================================================================

void global_settings::CommitLocalization( void )
{
    x_language const PreviousTextLanguage  = x_GetLocale();
    x_language const PreviousAudioLanguage = g_AudioMgr.GetLanguage();

    x_SetLocale( GetTextLanguage() );
    g_AudioMgr.SetLanguage( GetAudioLanguage() );
    Movie.SetLanguage( GetVideoLanguage() );

    if( PreviousTextLanguage != GetTextLanguage() )
    {
        if( !g_StringTableMgr.ReloadLocalizedTables() )
        {
            x_SetLocale( PreviousTextLanguage );
            SetTextLanguage( PreviousTextLanguage );
            LOG_ERROR( "global_settings::CommitLocalization",
                       "Unable to reload localized string tables." );
        }
        else
        {
            if( g_UiMgr && !g_UiMgr->ReloadFonts() )
            {
                LOG_ERROR( "global_settings::CommitLocalization",
                           "Unable to reload localized fonts." );
            }

            if( g_StringTableMgr.GetStringCount( "Msg" ) > 0 )
            {
                InitMessages();
            }
        }
    }

    if( PreviousAudioLanguage != GetAudioLanguage() )
    {
        xarray<xstring> Packages;
        g_AudioMgr.GetLoadedPackageLookupNames( Packages );

        for( s32 i = 0; i < Packages.GetCount(); i++ )
        {
            char const* pPackageName = Packages[i];
            char const* pFilename = pPackageName;

            for( char const* p = pPackageName; *p; p++ )
            {
                if( (*p == '\\') || (*p == '/') )
                {
                    pFilename = p + 1;
                }
            }

            if( x_strncmp( pFilename, "DX_", 3 ) == 0 )
            {
                g_RscMgr.Refresh( pPackageName );
            }
        }
    }
}

//==============================================================================

void global_settings::CommitGraphics( void )
{
    NormalizeGraphicsSettings();
    CommitInterfaceSettings();
    CommitRenderSettings();
    CommitDisplaySettings();
}

//==============================================================================

void global_settings::NormalizeGraphicsSettings( void )
{
    SetUIScale( m_UIScale );
    SetHUDScale( m_HUDScale );
    SetBrightness( m_Brightness );
    SetContrast( m_Contrast );
    SetRenderScale( m_RenderScale );
    SetFieldOfView( m_FieldOfView );
    SetFilmGrainStrength( m_FilmGrainStrength );
    SetBackgroundBlurEnabled( m_BackgroundBlurEnabled );
    SetTessellationEnabled( m_TessellationEnabled );
    SetTextureFiltering( m_TextureFiltering );

    if( m_AmbientOcclusionQuality < render::AmbientOcclusionQuality::Disabled ||
        m_AmbientOcclusionQuality > render::AmbientOcclusionQuality::Ultra )
    {
        m_AmbientOcclusionQuality = render::AMBIENT_OCCLUSION_DEFAULT;
    }

    if( (m_AntiAliasingType != AntiAliasingType::None) &&
        (m_AntiAliasingType != AntiAliasingType::Cmaa2) &&
        (m_AntiAliasingType != AntiAliasingType::Msaa) )
    {
        m_AntiAliasingType = ANTI_ALIASING_TYPE_DEFAULT;
    }

    if( ((m_ResolutionWidth <= 0) && (m_ResolutionHeight > 0)) ||
        ((m_ResolutionWidth > 0) && (m_ResolutionHeight <= 0)) )
    {
        m_ResolutionWidth  = 0;
        m_ResolutionHeight = 0;
    }

    if( (m_DisplayMode != ENG_DISPLAY_WINDOWED) &&
        (m_DisplayMode != ENG_DISPLAY_BORDERLESS) )
    {
        m_DisplayMode = ENG_DISPLAY_BORDERLESS;
    }

    if( (m_PresentMode != ENG_PRESENT_VSYNC) &&
        (m_PresentMode != ENG_PRESENT_MAILBOX) &&
        (m_PresentMode != ENG_PRESENT_IMMEDIATE) )
    {
        m_PresentMode = ENG_PRESENT_VSYNC;
    }

    if( !IsFrameRateLimitValid( m_FrameRateLimit ) ||
        (m_PresentMode != ENG_PRESENT_IMMEDIATE) )
    {
        m_FrameRateLimit = FrameRateLimit::Auto;
    }

    if( (m_ShadowMode != ShadowMode::Disabled) &&
        (m_ShadowMode != ShadowMode::Enabled) &&
        (m_ShadowMode != ShadowMode::FlashlightOnly) )
    {
        m_ShadowMode = SHADOW_MODE_DEFAULT;
    }

    if( (m_ShadowFilterType != ShadowFilterType::Hard) &&
        (m_ShadowFilterType != ShadowFilterType::Evsm) )
    {
        m_ShadowFilterType = SHADOW_FILTER_TYPE_DEFAULT;
    }

}

//==============================================================================

xbool global_settings::IsFrameRateLimitValid( FrameRateLimit Limit )
{
    return (Limit == FrameRateLimit::Auto)   ||
           (Limit == FrameRateLimit::Fps30)  ||
           (Limit == FrameRateLimit::Fps60)  ||
           (Limit == FrameRateLimit::Fps90)  ||
           (Limit == FrameRateLimit::Fps120) ||
           (Limit == FrameRateLimit::Fps144) ||
           (Limit == FrameRateLimit::Fps165) ||
           (Limit == FrameRateLimit::Fps240);
}

//==============================================================================

void global_settings::CommitInterfaceSettings( void )
{
    f32 const UIScale  = static_cast<f32>( GetUIScale() ) / 100.0f;
    f32 const HUDScale = static_cast<f32>( GetHUDScale() ) / 100.0f;

    if( g_UiMgr )
    {
        g_UiMgr->SetUserScale( UIScale );
    }
    g_UIRenderer.SetHudUserScale( HUDScale );
}

//==============================================================================

void global_settings::CommitRenderSettings( void )
{
    g_PostEffectMgr.SetFilmGrainStrength( GetFilmGrainStrength() );
    g_PostEffectMgr.SetBackgroundBlurEnabled( GetBackgroundBlurEnabled() );
    if( !rstate_SetTextureFiltering( GetTextureFiltering() ) )
    {
        LOG_ERROR( "global_settings::CommitRenderSettings", "Failed to apply texture filtering" );
        m_TextureFiltering = rstate_GetTextureFiltering();
    }

    render::GetRenderPipeline().SetRenderScale( static_cast<f32>( GetRenderScale() ) / 100.0f );
    render::GetRenderPipeline().SetSampleCount(
        (GetAntiAliasingType() == AntiAliasingType::Msaa) ? 4 : 1 );
    render::GetRenderPipeline().SetCmaa2Enabled(
        GetAntiAliasingType() == AntiAliasingType::Cmaa2 );
    render::GetRenderPipeline().GetXeGtaoFeature().SetQuality( GetAmbientOcclusionQuality() );
    render::GetRenderPipeline().SetPhongShadingEnabled( GetPhongShadingEnabled() );
    render::GetRenderPipeline().SetTessellationEnabled( GetTessellationEnabled() );
    render::GetRenderPipeline().GetPostProcessFeatureRef().SetXboxFogEnabled( GetXboxFogEnabled() );

    render::SceneColorGradeParams SceneColorGrade;
    SceneColorGrade.Brightness = static_cast<f32>( GetBrightness() ) / 100.0f;
    SceneColorGrade.Contrast   = static_cast<f32>( GetContrast() ) / 100.0f;
    g_PostEffectMgr.SetSceneColorGrade( SceneColorGrade );

    render::GetRenderPipeline().GetShadowMapAtlas().SetShadowMode( GetShadowMode() );
    render::GetRenderPipeline().GetShadowMapAtlas().SetShadowFilterType( GetShadowFilterType() );

    radian const FieldOfView = DEG_TO_RAD( static_cast<f32>( GetFieldOfView() ) );
    for( slot_id PlayerSlot = g_ObjMgr.GetFirst( object::TYPE_PLAYER );
         PlayerSlot != SLOT_NULL;
         PlayerSlot = g_ObjMgr.GetNext( PlayerSlot ) )
    {
        object* pObject = g_ObjMgr.GetObjectBySlot( PlayerSlot );
        if( pObject )
        {
            static_cast<player*>( pObject )->SetFieldOfView( FieldOfView );
        }
    }
}

//==============================================================================

void global_settings::CommitDisplaySettings( void )
{
    s32 CurrentWidth  = 0;
    s32 CurrentHeight = 0;
    eng_GetRes( CurrentWidth, CurrentHeight );

    if( (m_ResolutionWidth <= 0) || (m_ResolutionHeight <= 0) )
    {
        CaptureGraphics();
        return;
    }

    eng_display_mode const CurrentDisplayMode = eng_GetDisplayMode();

    if( (m_ResolutionWidth != CurrentWidth) ||
        (m_ResolutionHeight != CurrentHeight) )
    {
        eng_SetResolution( m_ResolutionWidth, m_ResolutionHeight );
    }

    if( m_DisplayMode != CurrentDisplayMode )
    {
        eng_SetDisplayMode( m_DisplayMode );
    }

    if( (m_PresentMode != eng_GetPresentMode()) &&
        !eng_SetPresentMode( m_PresentMode ) )
    {
        x_DebugMsg( "Global settings: failed to apply present mode\n" );
    }
}

//==============================================================================

void global_settings::CaptureGraphics( void )
{
    s32 Width  = 0;
    s32 Height = 0;
    eng_GetRes( Width, Height );
    SetDisplayResolution( Width, Height );
    SetDisplayMode( eng_GetDisplayMode() );
    SetPresentMode( eng_GetPresentMode() );
}

//==============================================================================

void global_settings::SetUIScale( s32 Percent )
{
    m_UIScale = x_clamp( Percent, UI_SCALE_MIN_PERCENT, UI_SCALE_MAX_PERCENT );
}

//==============================================================================

void global_settings::SetHUDScale( s32 Percent )
{
    m_HUDScale = x_clamp( Percent, HUD_SCALE_MIN_PERCENT, HUD_SCALE_MAX_PERCENT );
}

//==============================================================================

void global_settings::SetBrightness( s32 Percent )
{
    m_Brightness = x_clamp( Percent, BRIGHTNESS_MIN_PERCENT, BRIGHTNESS_MAX_PERCENT );
}

//==============================================================================

s32 global_settings::GetBrightness( void ) const
{
    return m_Brightness;
}

//==============================================================================

void global_settings::SetContrast( s32 Percent )
{
    m_Contrast = x_clamp( Percent, CONTRAST_MIN_PERCENT, CONTRAST_MAX_PERCENT );
}

//==============================================================================

s32 global_settings::GetContrast( void ) const
{
    return m_Contrast;
}

//==============================================================================

void global_settings::SetRenderScale( s32 Percent )
{
    m_RenderScale = x_clamp( Percent, RENDER_SCALE_MIN_PERCENT, RENDER_SCALE_MAX_PERCENT );
}

//==============================================================================

s32 global_settings::GetRenderScale( void ) const
{
    return m_RenderScale;
}

//==============================================================================

void global_settings::SetDisplayResolution( s32 Width, s32 Height )
{
    m_ResolutionWidth  = Width;
    m_ResolutionHeight = Height;
}

//==============================================================================

void global_settings::SetFieldOfView( s32 Degrees )
{
    m_FieldOfView = x_clamp( Degrees, FIELD_OF_VIEW_MIN_DEGREES, FIELD_OF_VIEW_MAX_DEGREES );
}

//==============================================================================

void global_settings::SetFilmGrainStrength( s32 Strength )
{
    m_FilmGrainStrength = x_clamp( Strength, FILM_GRAIN_MIN_STRENGTH, FILM_GRAIN_MAX_STRENGTH );
}

//==============================================================================

void global_settings::SetBackgroundBlurEnabled( xbool Enabled )
{
    m_BackgroundBlurEnabled = Enabled;
}

//==============================================================================

xbool global_settings::GetBackgroundBlurEnabled( void ) const
{
    return m_BackgroundBlurEnabled;
}

//==============================================================================

void global_settings::SetTessellationEnabled( xbool Enabled )
{
    m_TessellationEnabled = Enabled;
}

//==============================================================================

xbool global_settings::GetTessellationEnabled( void ) const
{
    return m_TessellationEnabled;
}

//==============================================================================

void global_settings::SetAntiAliasingType( AntiAliasingType Type )
{
    ASSERT( (Type == AntiAliasingType::None) ||
            (Type == AntiAliasingType::Cmaa2) ||
            (Type == AntiAliasingType::Msaa) );

    m_AntiAliasingType = Type;
}

//==============================================================================

AntiAliasingType global_settings::GetAntiAliasingType( void ) const
{
    return m_AntiAliasingType;
}

//==============================================================================

void global_settings::SetAmbientOcclusionQuality( render::AmbientOcclusionQuality Quality )
{
    ASSERT( Quality >= render::AmbientOcclusionQuality::Disabled && Quality <= render::AmbientOcclusionQuality::Ultra );
    m_AmbientOcclusionQuality = Quality;
}

//==============================================================================

render::AmbientOcclusionQuality global_settings::GetAmbientOcclusionQuality( void ) const
{
    return m_AmbientOcclusionQuality;
}

//==============================================================================

void global_settings::SetTextureFiltering( rstate_texture_filtering Filtering )
{
    m_TextureFiltering = (Filtering >= RSTATE_TEXTURE_FILTER_BILINEAR &&
                          Filtering <= RSTATE_TEXTURE_FILTER_ANISOTROPIC_16)
                             ? Filtering : TEXTURE_FILTERING_DEFAULT;
}

//==============================================================================

rstate_texture_filtering global_settings::GetTextureFiltering( void ) const
{
    return m_TextureFiltering;
}

//==============================================================================

void global_settings::SetPhongShadingEnabled( xbool Enabled )
{
    m_PhongShadingEnabled = Enabled;
}

//==============================================================================

xbool global_settings::GetPhongShadingEnabled( void ) const
{
    return m_PhongShadingEnabled;
}

//==============================================================================

void global_settings::SetShadowMode( ShadowMode Mode )
{
    ASSERT( (Mode == ShadowMode::Disabled) ||
            (Mode == ShadowMode::Enabled) ||
            (Mode == ShadowMode::FlashlightOnly) );

    m_ShadowMode = Mode;
}

//==============================================================================

ShadowMode global_settings::GetShadowMode( void ) const
{
    return m_ShadowMode;
}

//==============================================================================

void global_settings::SetXboxFogEnabled( xbool Enabled )
{
    m_XboxFogEnabled = Enabled;
}

//==============================================================================

xbool global_settings::GetXboxFogEnabled( void ) const
{
    return m_XboxFogEnabled;
}

//==============================================================================

void global_settings::SetShadowFilterType( ShadowFilterType Type )
{
    ASSERT( (Type == ShadowFilterType::Hard) || (Type == ShadowFilterType::Evsm) );

    m_ShadowFilterType = Type;
}

//==============================================================================

ShadowFilterType global_settings::GetShadowFilterType( void ) const
{
    return m_ShadowFilterType;
}

//==============================================================================

xbool global_settings::HasChanged( void )
{
    s32 const DesiredChecksum = m_Checksum;
    m_Checksum = 0;
    s32 const ActualChecksum = x_chksum( this, sizeof(*this) );
    m_Checksum = DesiredChecksum;
    return DesiredChecksum != ActualChecksum;
}

//==============================================================================

void global_settings::MarkDirty( void )
{
    m_Checksum = 0;
}

//==============================================================================

void global_settings::Checksum( void )
{
    m_Checksum  = 0;
    m_DateStamp = eng_GetDate();
    m_Checksum  = x_chksum( this, sizeof(*this) );
}

//==============================================================================

void global_settings::UpdateHeadsetMode( headset_mode HeadsetMode )
{
    xbool IsVoiceEnabled = TRUE;
    xbool IsVoiceAudible = TRUE;
    xbool IsThroughSpeakers = FALSE;

    switch( HeadsetMode )
    {
        case HEADSET_HEADSET_ONLY:
            IsVoiceAudible    = TRUE;
            IsThroughSpeakers = FALSE;
            IsVoiceEnabled    = TRUE;
            break;

        case HEADSET_THROUGH_SPEAKERS:
            IsVoiceAudible    = TRUE;
            IsThroughSpeakers = TRUE;
            IsVoiceEnabled    = TRUE;
            break;

        case HEADSET_DISABLED:
            IsVoiceAudible    = FALSE;
            IsThroughSpeakers = FALSE;
            IsVoiceEnabled    = FALSE;
            break;

        default:
            ASSERT( FALSE );
            break;
    }

    g_VoiceMgr.SetVoiceEnabled( IsVoiceEnabled );
    g_VoiceMgr.SetVoiceAudible( IsVoiceAudible );
    g_VoiceMgr.SetVoiceThruSpeakers( IsThroughSpeakers );

    // On the server we must update the game manager's voice peripheral status directly
    if( g_NetworkMgr.IsServer() == TRUE )
    {
        GameMgr.SetVoiceCapable( 0, IsVoiceEnabled );
    }
}

//==============================================================================

void global_settings::SetPatchData( void const* pPatchData, s32 Size, s32 Version )
{
    m_PatchVersion = Version;
    m_PatchLength  = Size;
    x_memcpy( m_PatchData, pPatchData, Size );
}
