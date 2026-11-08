//=========================================================================
//
//  PlayerProfile.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PlayerProfile.hpp"
#include "StateMgr.hpp"
#include "../../Apps/GameApp/Config.hpp"    

//=========================================================================
//  Control Settings Packing
//=========================================================================

namespace
{
    u32 GetAxisIndex( profile_control_axis Axis )
    {
        return static_cast<u32>( Axis );
    }

    u32 GetSensitivityMaximum( profile_control_device Device )
    {
        return (Device == profile_control_device::Mouse)
             ? MOUSE_SENSITIVITY_MAX
             : GAMEPAD_SENSITIVITY_MAX;
    }

}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void multiplayer_preferences::Reset( void )
{
    x_memset( this, 0, sizeof(*this) );
    Host.Reset();
    HostPort = NET_GAME_PORT;
    SetFilter( match_browser_filter() );
    Sort = MATCH_SORT_NAME;
}

//=========================================================================

match_browser_filter multiplayer_preferences::GetFilter( void ) const
{
    match_browser_filter Filter;
    Filter.Name         = FilterName;
    Filter.Map          = FilterMap;
    Filter.Source       = Source;
    Filter.GameType     = GameType;
    Filter.MaxPing      = MaxPing;
    Filter.HideFull     = HideFull;
    Filter.HideEmpty    = HideEmpty;
    Filter.HidePassword = HidePassword;
    return Filter;
}

//=========================================================================

void multiplayer_preferences::SetFilter( const match_browser_filter& Filter )
{
    x_wstrncpy( FilterName, Filter.Name, 63 );
    FilterName[63] = 0;
    x_wstrncpy( FilterMap, Filter.Map, 31 );
    FilterMap[31] = 0;

    Source       = Filter.Source;
    GameType     = Filter.GameType;
    MaxPing      = Filter.MaxPing;
    HideFull     = Filter.HideFull;
    HideEmpty    = Filter.HideEmpty;
    HidePassword = Filter.HidePassword;
}

//=========================================================================

player_profile::player_profile( void )
{
    Reset();
}

//=========================================================================

void player_profile::Reset( void )
{
    x_memset( this, 0, sizeof(*this) );
    m_MultiplayerPreferences.Reset();

    if( CONFIG_IS_AUTOSERVER )
    {
        x_strcpy( m_pProfileName, "Server" );
    }
    if( CONFIG_IS_AUTOCLIENT )
    {
        x_strcpy( m_pProfileName, "Client" );
    }

    m_HashString            = 0;
    SetAvatarID( 0 );
    RestoreControlDefaults();
    ingame_pad::ResetKeyboardBindings( m_KeyboardBindings );
    m_LoreTotal             = 0;
    m_bNewLoreUnlocked      = FALSE;
    m_NumSecretsUnlocked    = 0;
    m_bNewSecretUnlocked    = FALSE;
    SetAutosaveOn( FALSE );
    SetAlienAvatarsOn( FALSE );
    m_UniqueIdLength        = 0;
    m_CinemaMutatedMsgCount = 3;
    SetPlayerIsMutated( FALSE );
    SetDifficultyLevel( static_cast<u8>( DIFFICULTY_MEDIUM ) );
    m_bSecretAwarded        = FALSE;
    SetHardUnlocked( FALSE );
    SetDifficultyChanged( FALSE );
    SetGameFinished( FALSE );

    // Clear lore collected
    for( s32 i = 0; i < NUM_VAULTS; i++ )
    {
        m_Lore[i] = 0;
    }

    // Mark all checkpoints as having invalid level id, this will make it
    // available for use.
    for( s32 i = 0; i < MAX_SAVED_LEVELS; i++ )
    {
        m_Checkpoints[i].Init( -1 );
    }
}

//=========================================================================

void player_profile::SetHash( void )
{
    xstring TempString ( m_pProfileName );
    char const* pString     = TempString;
    u32 Hash                = 5381;

    // get the time and date
    datestamp Time = eng_GetDate();

    // convert the time to a string and add it to the hashstring
    TempString += xfs( "%d", Time );

    // Process each character to generate the hash key
    while( *pString )
    {
        Hash = (Hash * 33) ^ *pString++;
    }

    m_HashString = Hash;

}

//=========================================================================

void player_profile::RestoreControlDefaults( void )
{
    RestoreMouseControlDefaults();
    RestoreGamepadControlDefaults();
    RestoreCommonControlDefaults();
}

//=========================================================================

void player_profile::RestoreMouseControlDefaults( void )
{
    SetSensitivity( profile_control_device::Mouse, profile_control_axis::X, MOUSE_SENSITIVITY_DEFAULT );
    SetSensitivity( profile_control_device::Mouse, profile_control_axis::Y, MOUSE_SENSITIVITY_DEFAULT );
    SetAxisInverted( profile_control_device::Mouse, profile_control_axis::X, FALSE );
    SetAxisInverted( profile_control_device::Mouse, profile_control_axis::Y, FALSE );
}

//=========================================================================

void player_profile::RestoreGamepadControlDefaults( void )
{
    SetSensitivity( profile_control_device::Gamepad, profile_control_axis::X, GAMEPAD_SENSITIVITY_DEFAULT );
    SetSensitivity( profile_control_device::Gamepad, profile_control_axis::Y, GAMEPAD_SENSITIVITY_DEFAULT );
    SetAxisInverted( profile_control_device::Gamepad, profile_control_axis::X, FALSE );
    SetAxisInverted( profile_control_device::Gamepad, profile_control_axis::Y, FALSE );
    SetVibration( TRUE );
}

//=========================================================================

void player_profile::RestoreCommonControlDefaults( void )
{
    SetLookspringOn( FALSE );
    SetCrouchOn( FALSE );
    SetWeaponAutoSwitch( TRUE );
    SetAimToggleEnabled( TRUE );
}

//=========================================================================

u32 player_profile::GetSensitivity( profile_control_device Device, profile_control_axis Axis ) const
{
    u32 const AxisIndex = GetAxisIndex( Axis );
    ASSERT( AxisIndex < 2 );

    return (Device == profile_control_device::Mouse)
         ? m_MouseSensitivity[AxisIndex]
         : m_GamepadSensitivity[AxisIndex];
}

//=========================================================================

void player_profile::SetSensitivity( profile_control_device Device,
                                     profile_control_axis   Axis,
                                     u32                    Sensitivity )
{
    u32 const AxisIndex = GetAxisIndex( Axis );
    u32 const Maximum   = GetSensitivityMaximum( Device );
    ASSERT( AxisIndex < 2 );
    ASSERT( Sensitivity <= Maximum );

    if( Device == profile_control_device::Mouse )
    {
        m_MouseSensitivity[AxisIndex] = static_cast<u8>( MIN( Sensitivity, Maximum ) );
    }
    else
    {
        m_GamepadSensitivity[AxisIndex] = static_cast<u8>( MIN( Sensitivity, Maximum ) );
    }
}

//=========================================================================

xbool player_profile::IsAxisInverted( profile_control_device Device, profile_control_axis Axis ) const
{
    if( Axis == profile_control_axis::X )
    {
        return (Device == profile_control_device::Mouse)
             ? m_bMouseInvertX
             : m_bGamepadInvertX;
    }

    return (Device == profile_control_device::Mouse)
         ? m_bMouseInvertY
         : m_bGamepadInvertY;
}

//=========================================================================

void player_profile::SetAxisInverted( profile_control_device Device,
                                      profile_control_axis   Axis,
                                      xbool                  IsInverted )
{
    if( Device == profile_control_device::Mouse )
    {
        if( Axis == profile_control_axis::X )
        {
            m_bMouseInvertX = IsInverted;
        }
        else
        {
            m_bMouseInvertY = IsInverted;
        }
    }
    else if( Axis == profile_control_axis::X )
    {
        m_bGamepadInvertX = IsInverted;
    }
    else
    {
        m_bGamepadInvertY = IsInverted;
    }
}

//=========================================================================

xbool player_profile::IsAimToggleEnabled( void ) const
{
    return m_bAimToggleOn;
}

//=========================================================================

void player_profile::SetAimToggleEnabled( xbool IsEnabled )
{
    m_bAimToggleOn = IsEnabled;
}

//=========================================================================

void player_profile::SetProfileName( char const* pProfileName )
{
    ASSERT( x_strlen( pProfileName ) < SM_PROFILE_NAME_LENGTH );
    x_strcpy( m_pProfileName, pProfileName );
}


//=========================================================================

xbool player_profile::GetLoreAcquired( u32 Vault, s32 Index ) const
{
    ASSERT( Vault < NUM_VAULTS );
    ASSERT( (Index == -1) || ((Index >= 0) && (Index < NUM_PER_VAULT)) );

    // check if this is a general inquiry
    if( Index == -1 )
    {
        return ( m_Lore[Vault] != 0 );
    }

    // return specific lore acquired
    return (m_Lore[Vault] & (1 << Index)) != 0;
}

//=========================================================================

void player_profile::SetLoreAcquired( u32 Vault, u32 Index )
{
    // range checks
    ASSERT( Vault < NUM_VAULTS );
    ASSERT( Index < NUM_PER_VAULT );

    // make sure we're not already acquired
    if( !GetLoreAcquired( Vault, Index ) )
    {
        m_Lore[Vault] |= (1 << Index);
        m_LoreTotal++;
        m_bNewLoreUnlocked = TRUE;
    }

    // check for unlocking secrets
    if( (m_LoreTotal % 5) == 0 )
    {
        if( m_LoreTotal == 90 )
        {
            m_NumSecretsUnlocked = 21;
        }
        else
        {
            m_NumSecretsUnlocked++;
        }
        m_bNewSecretUnlocked = TRUE;
    }

#if !defined(X_EDITOR)
    // Auto save 
    g_StateMgr.SilentSaveProfile();
#endif
}

//=========================================================================
void player_profile::AcquireSecret( void )
{
    // used to award a secret after deep underground
    if( !m_bSecretAwarded )
    {
        m_NumSecretsUnlocked++;
        m_bSecretAwarded     = TRUE;
        m_bNewSecretUnlocked = TRUE;
    }
}

//=========================================================================

level_check_points* player_profile::GetCheckpointByMapID( s32 MapID )
{
    for( s32 i = 0; i < MAX_SAVED_LEVELS; i++ )
    {
        level_check_points* pCheckpoint = &m_Checkpoints[i];

        // Did we find one the same as the current map id? If so, bail out.
        if( pCheckpoint->MapID == MapID )
        {
            // found it!
            return pCheckpoint;
        }
        else if( pCheckpoint->MapID == -1 )
        {
            // no checkpoint for this MapID
            return NULL;
        }
    }
    // ran out of checkpoints!
    ASSERT( FALSE );
    return NULL;
}

//=========================================================================

void player_profile::Checksum( void )
{
    m_Checksum = 0;
    m_Checksum = x_chksum( this, sizeof(*this) );
}

//=========================================================================

xbool player_profile::HasChanged( void )
{
    s32 const DesiredChecksum = m_Checksum;
    m_Checksum = 0;
    s32 const ActualChecksum = x_chksum( this, sizeof(*this) );
    m_Checksum = DesiredChecksum;
    return ActualChecksum != DesiredChecksum;
}

//=========================================================================
void player_profile::MarkDirty( void )
{
    m_Checksum = 0;
}

//=========================================================================

void player_profile::SetUniqueId( byte const* pUniqueId, s32 Length )
{
    s32 const UniqueIdCapacity = static_cast<s32>( sizeof(m_UniqueId) );
    x_memset( m_UniqueId, 0, sizeof(m_UniqueId) );
    if( Length >= UniqueIdCapacity )
    {
        Length = UniqueIdCapacity - 1;
    }
    x_memcpy( m_UniqueId, pUniqueId, Length );
    m_UniqueIdLength = Length;
}

//=========================================================================

byte const* player_profile::GetUniqueId( s32& Length ) const
{
    Length = m_UniqueIdLength;
    return m_UniqueId;
}

//=========================================================================

xbool player_profile::DisplayCinemaMutatedMsg( void )
{ 
    if( m_CinemaMutatedMsgCount > 0 ) 
    {
        m_CinemaMutatedMsgCount--; 
        return TRUE;
    }

    return FALSE;
}

//=========================================================================

#ifndef CONFIG_RETAIL
void player_profile::UnlockAll( void )
{
    // this function will unlock everything EXCEPT the checkpoints
    
    // unlock all lore 
    for( s32 i = 0; i < NUM_VAULTS; i++ )
    {
        // check for deep underground - no lore for this level
        if( i != 1 )
        {
            // unlock all lore in vault
            m_Lore[i] = 31;
        }
    }
    m_LoreTotal             = 90;
    m_bNewLoreUnlocked      = TRUE;
    
    // unlock all secrets
    m_NumSecretsUnlocked    = 21;
    m_bNewSecretUnlocked    = TRUE;
    m_bSecretAwarded        = TRUE;

    // unlock hard 
    SetHardUnlocked( TRUE );
    // unlock alien avatars
    SetAlienAvatarsOn( TRUE );
    // unlock end game movie
    SetGameFinished( TRUE );
}
#endif

//==============================================================================

void player_profile::AddCareerStats( const player_stats& Stats )
{
    m_CareerStats.KillsAsHuman  += x_clamp( Stats.KillsAsHuman, 0, S32_MAX - m_CareerStats.KillsAsHuman );
    m_CareerStats.KillsAsMutant += x_clamp( Stats.KillsAsMutant, 0, S32_MAX - m_CareerStats.KillsAsMutant );
    m_CareerStats.Deaths        += x_clamp( Stats.Deaths, 0, S32_MAX - m_CareerStats.Deaths );
    m_CareerStats.PlayTime      += x_clamp( Stats.PlayTime, 0, S32_MAX - m_CareerStats.PlayTime );
    m_CareerStats.Games         += x_clamp( Stats.Games, 0, S32_MAX - m_CareerStats.Games );
    m_CareerStats.Wins          += x_clamp( Stats.Wins, 0, S32_MAX - m_CareerStats.Wins );
    m_CareerStats.Gold          += x_clamp( Stats.Gold, 0, S32_MAX - m_CareerStats.Gold );
    m_CareerStats.Silver        += x_clamp( Stats.Silver, 0, S32_MAX - m_CareerStats.Silver );
    m_CareerStats.Bronze        += x_clamp( Stats.Bronze, 0, S32_MAX - m_CareerStats.Bronze );
    m_CareerStats.Kicks         += x_clamp( Stats.Kicks, 0, S32_MAX - m_CareerStats.Kicks );
    m_CareerStats.VotesStarted  += x_clamp( Stats.VotesStarted, 0, S32_MAX - m_CareerStats.VotesStarted );
    MarkDirty();
}
