//=========================================================================
//
//  SaveDataCodec.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SaveDataCodec.hpp"
#include "Auxiliary/MiscUtils/BitseryIO.hpp"

#include <memory>

//=========================================================================
//  CONSTANTS
//=========================================================================

// UTC FILETIME, from 1970 through year 9999.
static const u64 SAVE_DATA_MIN_DATE = 116444736000000000ULL;
static const u64 SAVE_DATA_MAX_DATE = 2650467743999999999ULL;

//-------------------------------------------------------------------------

static const bitsery_io::file_format PROFILE_FILE_FORMAT =
{
    { 'A', '5', '1', 'P' },
    5,
};

//-------------------------------------------------------------------------

static const bitsery_io::file_format SETTINGS_FILE_FORMAT =
{
    { 'A', '5', '1', 'S' },
    12,
};

//=========================================================================
//  DATA
//=========================================================================

template<class SERIALIZER>
static void SerializeMultiplayerPreferences( SERIALIZER& Serializer, multiplayer_preferences& Value );

struct SaveDataCodec::ProfileSaveRecord
{
    player_profile       Profile;
    SaveDataProfileDates Dates;

    template<class SERIALIZER>
    void serialize( SERIALIZER& Serializer )
    {
        SerializeProfile( Serializer, Profile );
        Serializer.value8b( Dates.CreationDate );
        Serializer.value8b( Dates.ModifiedDate );
        ingame_pad::keyboard_bindings& Bindings = Profile.GetKeyboardBindings();
        for( s32 Action = 0; Action < ingame_pad::KEYBOARD_ACTION_COUNT; Action++ )
            for( s32 Slot = 0; Slot < ingame_pad::KEYBOARD_SLOT_COUNT; Slot++ )
                Serializer.value4b( Bindings.Keys[Action][Slot] );
        Serializer.value4b( Profile.m_CareerStats.KillsAsHuman );
        Serializer.value4b( Profile.m_CareerStats.KillsAsMutant );
        Serializer.value4b( Profile.m_CareerStats.Deaths );
        Serializer.value4b( Profile.m_CareerStats.PlayTime );
        Serializer.value4b( Profile.m_CareerStats.Games );
        Serializer.value4b( Profile.m_CareerStats.Wins );
        Serializer.value4b( Profile.m_CareerStats.Gold );
        Serializer.value4b( Profile.m_CareerStats.Silver );
        Serializer.value4b( Profile.m_CareerStats.Bronze );
        Serializer.value4b( Profile.m_CareerStats.Kicks );
        Serializer.value4b( Profile.m_CareerStats.VotesStarted );
        SerializeMultiplayerPreferences( Serializer, Profile.m_MultiplayerPreferences );
    }
};

//-------------------------------------------------------------------------

struct SaveDataCodec::SettingsSaveRecord
{
    global_settings Settings;

    template<class SERIALIZER>
    void serialize( SERIALIZER& Serializer )
    {
        SerializeSettings( Serializer, Settings );
    }
};

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
xbool IsValidDate( u64 Date )
{
    return ( Date >= SAVE_DATA_MIN_DATE ) && ( Date <= SAVE_DATA_MAX_DATE );
}

//=========================================================================

template<class SERIALIZER>
static
void SerializeMapSettings( SERIALIZER& Serializer, map_settings& Value )
{
    for( s32 i = 0; i < MAP_CYCLE_SIZE; i++ )
    {
        Serializer.value4b( Value.m_MapCycle[i] );
    }
    Serializer.value4b( Value.m_MapCycleIdx );
    Serializer.value4b( Value.m_MapCycleCount );
    Serializer.value4b( Value.m_bUseDefault );
}

//=========================================================================

template<class SERIALIZER>
static
void SerializeMultiSettings( SERIALIZER& Serializer, multi_settings& Value )
{
    Serializer.value4b( Value.m_GameTypeID );
    Serializer.value4b( Value.m_ScoreLimit );
    Serializer.value4b( Value.m_TimeLimit );
    s32 MutationMode = (s32)Value.m_MutationMode;
    Serializer.value4b( MutationMode );
    Value.m_MutationMode = (mutation_mode)MutationMode;
    SerializeMapSettings( Serializer, Value.m_MapSettings );
}

//=========================================================================

template<class SERIALIZER>
static
void SerializeHostSettings( SERIALIZER& Serializer, host_settings& Value )
{
    for( s32 i = 0; i < NET_SERVER_NAME_LENGTH; i++ )
    {
        Serializer.value2b( Value.m_ServerName[i] );
    }
    for( s32 i = 0; i < NET_PASSWORD_LENGTH; i++ )
    {
        Serializer.value1b( Value.m_Password[i] );
    }
    Serializer.value4b( Value.m_GameTypeID );
    Serializer.value4b( Value.m_ScoreLimit );
    Serializer.value4b( Value.m_TimeLimit );
    Serializer.value4b( Value.m_MaxPlayers );
    Serializer.value4b( Value.m_VotePassPct );
    Serializer.value4b( Value.m_FFirePct );
    Serializer.value4b( Value.m_Flags );

    s32 MutationMode = (s32)Value.m_MutationMode;
    Serializer.value4b( MutationMode );
    Value.m_MutationMode = (mutation_mode)MutationMode;
    SerializeMapSettings( Serializer, Value.m_MapSettings );

    s32 SkillLevel = (s32)Value.m_SkillLevel;
    Serializer.value4b( SkillLevel );
    Value.m_SkillLevel = (skill_level)SkillLevel;
}

//=========================================================================

template<class SERIALIZER>
static
void SerializeMultiplayerPreferences( SERIALIZER& Serializer, multiplayer_preferences& Value )
{
    SerializeHostSettings( Serializer, Value.Host );
    Serializer.value4b( Value.HostPort );

    for( s32 i = 0; i < MATCH_ENDPOINT_LENGTH; i++ )
        Serializer.value1b( Value.Directory[i] );

    for( s32 i = 0; i < MATCH_ENDPOINT_LENGTH; i++ )
        Serializer.value1b( Value.Address[i] );

    for( s32 i = 0; i < 64; i++ )
        Serializer.value2b( Value.FilterName[i] );

    for( s32 i = 0; i < 32; i++ )
        Serializer.value2b( Value.FilterMap[i] );

    Serializer.value4b( Value.Source );
    Serializer.value4b( Value.GameType );
    Serializer.value4b( Value.MaxPing );

    s32 Sort = (s32)Value.Sort;
    Serializer.value4b( Sort );
    Value.Sort = (match_sort)Sort;

    Serializer.value4b( Value.Descending );
    Serializer.value4b( Value.HideFull );
    Serializer.value4b( Value.HideEmpty );
    Serializer.value4b( Value.HidePassword );
}

//=========================================================================

template<class SERIALIZER>
static
void SerializeCheckPoint( SERIALIZER& Serializer, check_point& Value )
{
    Serializer.value4b( Value.bIsValid );
    Serializer.value4b( Value.TableName );
    Serializer.value4b( Value.TitleName );

    u64 RespawnGUID = Value.RespawnGUID.Guid;
    u64 AdvanceGUID = Value.AdvanceGUID.Guid;
    Serializer.value8b( RespawnGUID );
    Serializer.value8b( AdvanceGUID );
    Value.RespawnGUID = guid( RespawnGUID );
    Value.AdvanceGUID = guid( AdvanceGUID );

    Serializer.value4b( Value.CurrWeapon );
    Serializer.value4b( Value.PrevWeapon );
    Serializer.value4b( Value.NextWeapon );
    Serializer.value4b( Value.MutantMelee );
    Serializer.value4b( Value.MutantPrimary );
    Serializer.value4b( Value.MutantSecondary );
    Serializer.value4b( Value.Mutagen );
    Serializer.value4b( Value.Health );
    Serializer.value4b( Value.MaxHealth );

    for( s32 i = 0; i < INVEN_COUNT; i++ )
    {
        Serializer.value4b( Value.Inventory[i] );
    }
    for( s32 i = 0; i < INVEN_NUM_WEAPONS * 2; i++ )
    {
        Serializer.value4b( Value.Ammo[i].Amount );
        Serializer.value4b( Value.Ammo[i].CurrentClip );
    }
}

//=========================================================================

template<class SERIALIZER>
static
void SerializeLevelCheckPoints( SERIALIZER& Serializer, level_check_points& Value )
{
    Serializer.value4b( Value.MapID );
    Serializer.value4b( Value.nValidCheckPoints );
    Serializer.value4b( Value.iCurrentCheckPoint );
    for( s32 i = 0; i < MAX_CHECKPOINTS; i++ )
    {
        SerializeCheckPoint( Serializer, Value.CheckPoints[i] );
    }
}

//=========================================================================

static
xbool HasNullTerminator( const char* pText, s32 Capacity )
{
    for( s32 i = 0; i < Capacity; i++ )
    {
        if( pText[i] == '\0' )
        {
            return TRUE;
        }
    }
    return FALSE;
}

//=========================================================================

static
xbool HasNullTerminator( const xwchar* pText, s32 Capacity )
{
    for( s32 i = 0; i < Capacity; i++ )
    {
        if( !pText[i] )
            return TRUE;
    }

    return FALSE;
}

//=========================================================================

static
xbool MultiplayerPreferencesAreValid( const multiplayer_preferences& Value )
{
    const host_settings& Host = Value.Host;
    const map_settings& Maps = Host.m_MapSettings;

    return HasNullTerminator( Host.m_ServerName, NET_SERVER_NAME_LENGTH ) &&
           HasNullTerminator( Host.m_Password, NET_PASSWORD_LENGTH ) &&
           HasNullTerminator( Value.Directory, MATCH_ENDPOINT_LENGTH ) &&
           HasNullTerminator( Value.Address, MATCH_ENDPOINT_LENGTH ) &&
           HasNullTerminator( Value.FilterName, 64 ) && HasNullTerminator( Value.FilterMap, 32 ) &&
           (Value.HostPort > 0) && (Value.HostPort <= 65535) &&
           (Host.m_GameTypeID >= GAME_DM) && (Host.m_GameTypeID <= GAME_CNH) &&
           (Host.m_MaxPlayers >= 1) && (Host.m_MaxPlayers <= NET_MAX_PLAYERS) &&
           (Host.m_ScoreLimit >= -1) && (Host.m_TimeLimit >= -1) &&
           (Host.m_VotePassPct >= 0) && (Host.m_VotePassPct <= 100) &&
           (Host.m_FFirePct >= 0) && (Host.m_FFirePct <= 100) &&
           (Host.m_MutationMode >= MUTATE_CHANGE_AT_WILL) && (Host.m_MutationMode <= MUTATE_MUTANT_HUNT) &&
           (Host.m_SkillLevel >= SKILL_LEVEL_ANY) && (Host.m_SkillLevel <= SKILL_LEVEL_ADVANCED) &&
           (Maps.m_MapCycleCount >= 0) && (Maps.m_MapCycleCount <= MAP_CYCLE_SIZE) &&
           (Maps.m_MapCycleIdx >= 0) && (Maps.m_MapCycleIdx < MAX( Maps.m_MapCycleCount, 1 )) &&
           (Value.Source > 0) && ((Value.Source & ~MATCH_SOURCE_ALL) == 0) &&
           (Value.GameType >= GAME_MP) && (Value.GameType <= GAME_CNH) &&
           ((Value.MaxPing == -1) || (Value.MaxPing == 50) || (Value.MaxPing == 100) ||
            (Value.MaxPing == 150) || (Value.MaxPing == 250)) &&
           (Value.Sort >= MATCH_SORT_NAME) && (Value.Sort < MATCH_SORT_COUNT);
}

//=========================================================================

xbool SaveDataCodec::ProfileFieldsAreValid( const player_profile& Profile )
{
    if( !MultiplayerPreferencesAreValid( Profile.m_MultiplayerPreferences ) )
        return FALSE;

    if( !ingame_pad::KeyboardBindingsAreValid( Profile.GetKeyboardBindings() ) )
        return FALSE;
    if( Profile.m_CareerStats.KillsAsHuman < 0 )
        return FALSE;
    if( Profile.m_CareerStats.KillsAsMutant < 0 )
        return FALSE;
    if( Profile.m_CareerStats.Deaths < 0 )
        return FALSE;
    if( Profile.m_CareerStats.PlayTime < 0 )
        return FALSE;
    if( Profile.m_CareerStats.Games < 0 )
        return FALSE;
    if( Profile.m_CareerStats.Wins < 0 )
        return FALSE;
    if( Profile.m_CareerStats.Gold < 0 )
        return FALSE;
    if( Profile.m_CareerStats.Silver < 0 )
        return FALSE;
    if( Profile.m_CareerStats.Bronze < 0 )
        return FALSE;
    if( Profile.m_CareerStats.Kicks < 0 )
        return FALSE;
    if( Profile.m_CareerStats.VotesStarted < 0 )
        return FALSE;
    return HasNullTerminator( Profile.m_pProfileName, (s32)sizeof( Profile.m_pProfileName ) ) &&
           ( Profile.m_MouseSensitivity[0] <= MOUSE_SENSITIVITY_MAX ) &&
           ( Profile.m_MouseSensitivity[1] <= MOUSE_SENSITIVITY_MAX ) &&
           ( Profile.m_GamepadSensitivity[0] <= GAMEPAD_SENSITIVITY_MAX ) &&
           ( Profile.m_GamepadSensitivity[1] <= GAMEPAD_SENSITIVITY_MAX ) && ( Profile.m_UniqueIdLength >= 0 ) &&
           ( Profile.m_UniqueIdLength <= (s32)sizeof( Profile.m_UniqueId ) ) &&
           ( Profile.m_DifficultyLevel <= DIFFICULTY_HARD );
}

//=========================================================================

xbool SaveDataCodec::SettingsFieldsAreValid( const global_settings& Settings )
{
    return ( Settings.m_PatchLength >= 0 ) && ( Settings.m_PatchLength <= NET_MAX_PATCH_SIZE ) &&
           ( Settings.m_TextLanguage >= XL_LANG_ENGLISH ) && ( Settings.m_TextLanguage < XL_NUM_LANGUAGES ) &&
           ( Settings.m_AudioLanguage >= XL_LANG_ENGLISH ) && ( Settings.m_AudioLanguage < XL_NUM_LANGUAGES ) &&
           ( Settings.m_VideoLanguage >= XL_LANG_ENGLISH ) && ( Settings.m_VideoLanguage < XL_NUM_LANGUAGES ) &&
           ( Settings.m_UIScale >= UI_SCALE_MIN_PERCENT ) && ( Settings.m_UIScale <= UI_SCALE_MAX_PERCENT ) &&
           ( Settings.m_HUDScale >= HUD_SCALE_MIN_PERCENT ) && ( Settings.m_HUDScale <= HUD_SCALE_MAX_PERCENT ) &&
           ( Settings.m_Brightness >= BRIGHTNESS_MIN_PERCENT ) && ( Settings.m_Brightness <= BRIGHTNESS_MAX_PERCENT ) &&
           ( Settings.m_Contrast >= CONTRAST_MIN_PERCENT ) && ( Settings.m_Contrast <= CONTRAST_MAX_PERCENT ) &&
           ( Settings.m_RenderScale >= RENDER_SCALE_MIN_PERCENT ) &&
           ( Settings.m_RenderScale <= RENDER_SCALE_MAX_PERCENT ) &&
           ( ( Settings.m_DisplayMode == ENG_DISPLAY_WINDOWED ) ||
             ( Settings.m_DisplayMode == ENG_DISPLAY_BORDERLESS ) ) &&
           ( ( Settings.m_PresentMode == ENG_PRESENT_VSYNC ) || ( Settings.m_PresentMode == ENG_PRESENT_MAILBOX ) ||
             ( Settings.m_PresentMode == ENG_PRESENT_IMMEDIATE ) ) &&
           global_settings::IsFrameRateLimitValid( Settings.m_FrameRateLimit ) &&
           ( ( Settings.m_PresentMode == ENG_PRESENT_IMMEDIATE ) ||
             ( Settings.m_FrameRateLimit == FrameRateLimit::Auto ) ) &&
           ( ( Settings.m_PhongShadingEnabled == FALSE ) || ( Settings.m_PhongShadingEnabled == TRUE ) ) &&
           ( ( Settings.m_ShadowMode == ShadowMode::Disabled ) || ( Settings.m_ShadowMode == ShadowMode::Enabled ) ||
             ( Settings.m_ShadowMode == ShadowMode::FlashlightOnly ) ) &&
           ( ( Settings.m_ShadowFilterType == ShadowFilterType::Hard ) ||
             ( Settings.m_ShadowFilterType == ShadowFilterType::Evsm ) ) &&
           ( ( Settings.m_BackgroundBlurEnabled == FALSE ) || ( Settings.m_BackgroundBlurEnabled == TRUE ) ) &&
           ( ( Settings.m_TessellationEnabled == FALSE ) || ( Settings.m_TessellationEnabled == TRUE ) ) &&
           ( Settings.m_TextureFiltering >= RSTATE_TEXTURE_FILTER_BILINEAR ) &&
           ( Settings.m_TextureFiltering <= RSTATE_TEXTURE_FILTER_ANISOTROPIC_16 ) &&
           ( ( Settings.m_AntiAliasingType == AntiAliasingType::None ) ||
             ( Settings.m_AntiAliasingType == AntiAliasingType::Cmaa2 ) ||
             ( Settings.m_AntiAliasingType == AntiAliasingType::Msaa ) ) &&
           ( Settings.m_AmbientOcclusionQuality >= render::AmbientOcclusionQuality::Disabled ) &&
           ( Settings.m_AmbientOcclusionQuality <= render::AmbientOcclusionQuality::Ultra );
}

//=========================================================================

template<class SERIALIZER>
void SaveDataCodec::SerializeProfile( SERIALIZER& Serializer, player_profile& Value )
{
    for( s32 i = 0; i < (s32)sizeof( Value.m_pProfileName ); i++ )
    {
        Serializer.value1b( Value.m_pProfileName[i] );
    }
    Serializer.value4b( Value.m_HashString );
    Serializer.value4b( Value.m_AvatarID );
    for( s32 i = 0; i < 2; i++ )
    {
        Serializer.value1b( Value.m_MouseSensitivity[i] );
        Serializer.value1b( Value.m_GamepadSensitivity[i] );
    }
    for( s32 i = 0; i < 5; i++ )
    {
        Serializer.value1b( Value.m_Volume[i] );
    }
    for( s32 i = 0; i < NUM_VAULTS; i++ )
    {
        Serializer.value1b( Value.m_Lore[i] );
    }
    Serializer.value4b( Value.m_LoreTotal );
    Serializer.value4b( Value.m_bNewLoreUnlocked );
    Serializer.value4b( Value.m_NumSecretsUnlocked );
    Serializer.value4b( Value.m_bNewSecretUnlocked );
    for( s32 i = 0; i < (s32)sizeof( Value.m_UniqueId ); i++ )
    {
        Serializer.value1b( Value.m_UniqueId[i] );
    }
    Serializer.value4b( Value.m_UniqueIdLength );
    Serializer.value4b( Value.m_bIsMutated );
    Serializer.value1b( Value.m_CinemaMutatedMsgCount );
    Serializer.value1b( Value.m_DifficultyLevel );
    Serializer.value4b( Value.m_bWeaponAutoSwitch );
    for( s32 i = 0; i < MAX_SAVED_LEVELS; i++ )
    {
        SerializeLevelCheckPoints( Serializer, Value.m_Checkpoints[i] );
    }

    u32 Flags = 0;
    Flags |= Value.m_bLookspringOn << 0;
    Flags |= Value.m_bCrouchOn << 1;
    Flags |= Value.m_bIsActive << 2;
    Flags |= Value.m_bGamepadInvertY << 3;
    Flags |= Value.m_bVibration << 4;
    Flags |= Value.m_bAutosaveOn << 5;
    Flags |= Value.m_bAlienAvatarsOn << 6;
    Flags |= Value.m_bSecretAwarded << 7;
    Flags |= Value.m_bHardUnlocked << 8;
    Flags |= Value.m_bDifficultyChanged << 9;
    Flags |= Value.m_bGameFinished << 10;
    Flags |= Value.m_bMouseInvertX << 11;
    Flags |= Value.m_bMouseInvertY << 12;
    Flags |= Value.m_bGamepadInvertX << 13;
    Flags |= Value.m_bAimToggleOn << 14;
    Serializer.value4b( Flags );

    Value.m_bLookspringOn      = ( Flags >> 0 ) & 1;
    Value.m_bCrouchOn          = ( Flags >> 1 ) & 1;
    Value.m_bIsActive          = ( Flags >> 2 ) & 1;
    Value.m_bGamepadInvertY    = ( Flags >> 3 ) & 1;
    Value.m_bVibration         = ( Flags >> 4 ) & 1;
    Value.m_bAutosaveOn        = ( Flags >> 5 ) & 1;
    Value.m_bAlienAvatarsOn    = ( Flags >> 6 ) & 1;
    Value.m_bSecretAwarded     = ( Flags >> 7 ) & 1;
    Value.m_bHardUnlocked      = ( Flags >> 8 ) & 1;
    Value.m_bDifficultyChanged = ( Flags >> 9 ) & 1;
    Value.m_bGameFinished      = ( Flags >> 10 ) & 1;
    Value.m_bMouseInvertX      = ( Flags >> 11 ) & 1;
    Value.m_bMouseInvertY      = ( Flags >> 12 ) & 1;
    Value.m_bGamepadInvertX    = ( Flags >> 13 ) & 1;
    Value.m_bAimToggleOn       = ( Flags >> 14 ) & 1;
}

//=========================================================================

template<class SERIALIZER>
void SaveDataCodec::SerializeSettings( SERIALIZER& Serializer, global_settings& Value )
{
    Serializer.value8b( Value.m_DateStamp );
    for( s32 i = 0; i < VOLUME_LAST; i++ )
    {
        Serializer.value4b( Value.m_Volume[i] );
    }
    Serializer.value4b( Value.m_SpeakerSet );
    Serializer.value4b( Value.m_ContentVersion );
    Serializer.value4b( Value.m_PatchVersion );
    Serializer.value4b( Value.m_PatchLength );
    for( s32 i = 0; i < NET_MAX_PATCH_SIZE; i++ )
    {
        Serializer.value1b( Value.m_PatchData[i] );
    }

    s32 HeadsetMode = (s32)Value.m_HeadsetMode;
    Serializer.value4b( HeadsetMode );
    Value.m_HeadsetMode = (headset_mode)HeadsetMode;

    SerializeMultiSettings( Serializer, Value.m_MultiplayerSettings );
    SerializeMapSettings( Serializer, Value.m_MapSettings );

    Serializer.value4b( Value.m_UIScale );
    Serializer.value4b( Value.m_HUDScale );
    Serializer.value4b( Value.m_Brightness );
    Serializer.value4b( Value.m_Contrast );
    Serializer.value4b( Value.m_RenderScale );
    Serializer.value4b( Value.m_ResolutionWidth );
    Serializer.value4b( Value.m_ResolutionHeight );

    s32 DisplayMode = (s32)Value.m_DisplayMode;
    s32 PresentMode = (s32)Value.m_PresentMode;
    Serializer.value4b( DisplayMode );
    Serializer.value4b( PresentMode );
    Value.m_DisplayMode = (eng_display_mode)DisplayMode;
    Value.m_PresentMode = (eng_present_mode)PresentMode;

    Serializer.value4b( Value.m_FieldOfView );
    Serializer.value4b( Value.m_FilmGrainStrength );

    s32 ShadowFilter = static_cast<s32>( Value.m_ShadowFilterType );
    Serializer.value4b( ShadowFilter );
    Value.m_ShadowFilterType = static_cast<ShadowFilterType>( ShadowFilter );

    Serializer.value4b( Value.m_VideoVolume );

    s32 TextLanguage  = (s32)Value.m_TextLanguage;
    s32 AudioLanguage = (s32)Value.m_AudioLanguage;
    s32 VideoLanguage = (s32)Value.m_VideoLanguage;
    Serializer.value4b( TextLanguage );
    Serializer.value4b( AudioLanguage );
    Serializer.value4b( VideoLanguage );
    Value.m_TextLanguage  = (x_language)TextLanguage;
    Value.m_AudioLanguage = (x_language)AudioLanguage;
    Value.m_VideoLanguage = (x_language)VideoLanguage;

    s32 FrameLimit = static_cast<s32>( Value.m_FrameRateLimit );
    Serializer.value4b( FrameLimit );
    Value.m_FrameRateLimit = static_cast<FrameRateLimit>( FrameLimit );

    Serializer.value4b( Value.m_BackgroundBlurEnabled );
    Serializer.value4b( Value.m_TessellationEnabled );

    s32 AntiAliasing = static_cast<s32>( Value.m_AntiAliasingType );
    Serializer.value4b( AntiAliasing );
    Value.m_AntiAliasingType = static_cast<AntiAliasingType>( AntiAliasing );

    Serializer.value4b( Value.m_PhongShadingEnabled );
    s32 ShadowModeValue = static_cast<s32>( Value.m_ShadowMode );
    Serializer.value4b( ShadowModeValue );
    Value.m_ShadowMode = static_cast<ShadowMode>( ShadowModeValue );

    s32 TextureFiltering = static_cast<s32>( Value.m_TextureFiltering );
    Serializer.value4b( TextureFiltering );
    Value.m_TextureFiltering = static_cast<rstate_texture_filtering>( TextureFiltering );

    s32 AmbientOcclusion = static_cast<s32>( Value.m_AmbientOcclusionQuality );
    Serializer.value4b( AmbientOcclusion );
    Value.m_AmbientOcclusionQuality = static_cast<render::AmbientOcclusionQuality>( AmbientOcclusion );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool SaveDataCodec::EncodeProfile( const player_profile& Profile, const SaveDataProfileDates& Dates,
                                      xarray<u8>& Bytes, xstring& Error )
{
    std::unique_ptr<ProfileSaveRecord> Record( new ProfileSaveRecord );
    if( !ProfileFieldsAreValid( Profile ) || !IsValidDate( Dates.CreationDate ) || !IsValidDate( Dates.ModifiedDate ) )
    {
        Bytes.Clear();
        Error = "Profile fields or timestamps are invalid.";
        return FALSE;
    }
    Record->Profile = Profile;
    Record->Dates   = Dates;
    return bitsery_io::EncodeFile( PROFILE_FILE_FORMAT, *Record, Bytes, Error );
}

//=========================================================================

xbool SaveDataCodec::DecodeProfile( const xarray<u8>& Bytes, player_profile& Profile, SaveDataProfileDates& Dates,
                                      xstring& Error )
{
    std::unique_ptr<ProfileSaveRecord> Record( new ProfileSaveRecord );
    if( !bitsery_io::DecodeFile( Bytes.GetPtr(), Bytes.GetCount(), PROFILE_FILE_FORMAT, *Record, Error ) )
        return FALSE;
    if( !IsValidDate( Record->Dates.CreationDate ) || !IsValidDate( Record->Dates.ModifiedDate ) )
    {
        Error = "Serialized profile contains invalid timestamps.";
        return FALSE;
    }
    if( !ProfileFieldsAreValid( Record->Profile ) )
    {
        Error = "Serialized profile contains invalid fields.";
        return FALSE;
    }

    Dates              = Record->Dates;
    Profile            = Record->Profile;
    Profile.m_Checksum = 0;
    Profile.m_Checksum = x_chksum( &Profile, sizeof( Profile ) );
    return TRUE;
}

//=========================================================================

xbool SaveDataCodec::EncodeSettings( const global_settings& Settings, xarray<u8>& Bytes, xstring& Error )
{
    std::unique_ptr<SettingsSaveRecord> Record( new SettingsSaveRecord );
    Record->Settings = Settings;
    return bitsery_io::EncodeFile( SETTINGS_FILE_FORMAT, *Record, Bytes, Error );
}

//=========================================================================

xbool SaveDataCodec::DecodeSettings( const xarray<u8>& Bytes, global_settings& Settings, xstring& Error )
{
    std::unique_ptr<SettingsSaveRecord> Record( new SettingsSaveRecord );
    Record->Settings.Reset();
    if( !bitsery_io::DecodeFile( Bytes.GetPtr(), Bytes.GetCount(), SETTINGS_FILE_FORMAT, *Record, Error ) )
    {
        return FALSE;
    }
    if( !SettingsFieldsAreValid( Record->Settings ) )
    {
        Error = "Serialized settings contain invalid fields.";
        return FALSE;
    }

    Settings            = Record->Settings;
    Settings.m_Checksum = 0;
    Settings.m_Checksum = x_chksum( &Settings, sizeof( Settings ) );
    return TRUE;
}
