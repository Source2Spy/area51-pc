//==============================================================================
//
//  audio_helpers.hpp
//
//==============================================================================

#ifndef AUDIO_HELPERS_HPP
#define AUDIO_HELPERS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"
#include "Audio/audio_package.hpp"

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline void RemoveChannelFromList( channel* pChannel )
{
    // Remove from the list.
    pChannel->Link.pPrev->Link.pNext = pChannel->Link.pNext;
    pChannel->Link.pNext->Link.pPrev = pChannel->Link.pPrev;
}

//==============================================================================

inline void InsertChannelIntoList( channel* pChannel, channel* pInsertionPoint )
{
    // Insert into list.
    pChannel->Link.pPrev             = pInsertionPoint->Link.pPrev;
    pChannel->Link.pNext             = pInsertionPoint;
    pInsertionPoint->Link.pPrev      = pChannel;
    pChannel->Link.pPrev->Link.pNext = pChannel;
}

//==============================================================================

inline void RemoveVoiceFromList( voice* pVoice )
{
    // Remove from the list.
    pVoice->Link.pPrev->Link.pNext = pVoice->Link.pNext;
    pVoice->Link.pNext->Link.pPrev = pVoice->Link.pPrev;
}

//==============================================================================

inline void InsertVoiceIntoList( voice* pVoice, voice* pInsertionPoint )
{
    // Insert into list.
    pVoice->Link.pPrev             = pInsertionPoint->Link.pPrev;
    pVoice->Link.pNext             = pInsertionPoint;
    pInsertionPoint->Link.pPrev    = pVoice;
    pVoice->Link.pPrev->Link.pNext = pVoice;
}

//==============================================================================

inline void RemoveElementFromList( element* pElement )
{
    // Remove from the list.
    pElement->Link.pPrev->Link.pNext = pElement->Link.pNext;
    pElement->Link.pNext->Link.pPrev = pElement->Link.pPrev;
}

//==============================================================================

inline void InsertElementIntoList( element* pElement, element* pInsertionPoint )
{
    // Insert into list.
    pElement->Link.pPrev             = pInsertionPoint->Link.pPrev;
    pElement->Link.pNext             = pInsertionPoint;
    pInsertionPoint->Link.pPrev      = pElement;
    pElement->Link.pPrev->Link.pNext = pElement;
}

//==============================================================================

inline f32 CalculateVoiceVolume( voice* pVoice, xbool AudioDuckingEnabled )
{
    f32 Result = pVoice->UserVolume * pVoice->Params.Volume * pVoice->pPackage->GetComputedVolume();
    if( AudioDuckingEnabled )
    {
        Result *= pVoice->Params.VolumeDuck;
    }
    return Result;
}

//==============================================================================

inline f32 CalculateVoicePitch( voice* pVoice )
{
    return pVoice->UserPitch * pVoice->Params.Pitch * pVoice->pPackage->GetComputedPitch();
}

//==============================================================================

inline f32 CalculateVoiceNearFalloff( voice* pVoice )
{
    return pVoice->UserNearFalloff * pVoice->Params.NearFalloff * pVoice->pPackage->GetComputedNearFalloff();
}

//==============================================================================

inline f32 CalculateVoiceFarFalloff( voice* pVoice )
{
    return pVoice->UserFarFalloff * pVoice->Params.FarFalloff * pVoice->pPackage->GetComputedFarFalloff();
}

//==============================================================================

inline f32 CalculateVoiceNearDiffuse( voice* pVoice )
{
    return pVoice->UserNearDiffuse * pVoice->Params.NearDiffuse * pVoice->pPackage->GetComputedNearDiffuse();
}

//==============================================================================

inline f32 CalculateVoiceFarDiffuse( voice* pVoice )
{
    return pVoice->UserFarDiffuse * pVoice->Params.FarDiffuse * pVoice->pPackage->GetComputedFarDiffuse();
}

//==============================================================================

inline f32 CalculateVoiceEffectSend( voice* pVoice )
{
    return pVoice->UserEffectSend * pVoice->Params.EffectSend * pVoice->pPackage->GetComputedEffectSend();
}

//==============================================================================

inline f32 CalculateElementVolume( element* pElement, xbool AudioDuckingEnabled )
{
    f32 Result = pElement->Params.Volume * pElement->PositionalVolume * pElement->VolumeVariance * pElement->pVoice->Volume;
    if( AudioDuckingEnabled )
    {
        Result *= pElement->Params.VolumeDuck;
    }
    return Result;
}

//==============================================================================

inline f32 CalculateElementPitch( element* pElement )
{
    return pElement->Params.Pitch * pElement->PitchVariance * pElement->pVoice->Pitch;
}

//==============================================================================

// HACK HACK HACK!!! NEED TO FIND A BETTER WAY TO DO THIS.
// THIS IS A TEMPORARY FIX FOR THE HUD SOUNDS AND MUSIC THAT NEED TO BE PITCH LOCKED FOR MUTANT MODE.

inline xbool IsPitchLockedDescriptor( const char* pPackageName, const char* pDescriptorName )
{
    char Filename[X_MAX_FNAME];
    x_splitpath( pPackageName, NULL, NULL, Filename, NULL );

    if( ( x_stristr( Filename, "MUSIC_" ) == Filename ) ||
        ( x_stricmp( Filename, "SFX_FrontEnd" ) == 0 ) )
    {
        return TRUE;
    }

    if( !pDescriptorName )
    {
        return FALSE;
    }

    if( ( x_stristr( pDescriptorName, "HUD_" ) == pDescriptorName ) ||
        ( x_stristr( pDescriptorName, "Scanner_HUD_" ) == pDescriptorName ) )
    {
        return TRUE;
    }

    static const char* s_UISounds[] =
        {
            "Ammo_Low",
            "Ammo_Out_Loop",
            "Backup",
            "Bars_In",
            "Bars_Out",
            "CursorMove",
            "Cursor_VKB",
            "Cusor_Norm",
            "Delete_VKB",
            "Dialog",
            "Fader_SFX",
            "InvalidEntry",
            "JanDemo",
            "MUSIC_StartScreen",
            "Music_Slider",
            "OptionBack",
            "ResizeLarge",
            "ResizeSmall",
            "ScreenWipe",
            "SelectNorm",
            "Select_Norm",
            "Select_VKB",
            "Slider",
            "Toggle",
            "Voice_Slider" };

    for( s32 i = 0; i < (s32)( sizeof( s_UISounds ) / sizeof( s_UISounds[0] ) ); i++ )
    {
        if( x_stricmp( pDescriptorName, s_UISounds[i] ) == 0 )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

inline f32 CalculateChannelPitch( channel* pChannel, f32 PitchFactor )
{
    if( pChannel->pElement && pChannel->pElement->pVoice && pChannel->pElement->pVoice->bPitchLock )
    {
        return pChannel->Pitch;
    }

    return pChannel->Pitch * PitchFactor;
}

//==============================================================================

inline f32 CalculateElementNearFalloff( element* pElement )
{
    return pElement->Params.NearFalloff * pElement->pVoice->NearFalloff;
}

//==============================================================================

inline f32 CalculateElementFarFalloff( element* pElement )
{
    return pElement->Params.FarFalloff * pElement->pVoice->FarFalloff;
}

//==============================================================================

inline f32 CalculateElementNearDiffuse( element* pElement )
{
    return pElement->Params.NearDiffuse * pElement->pVoice->NearDiffuse;
}

//==============================================================================

inline f32 CalculateElementFarDiffuse( element* pElement )
{
    return pElement->Params.FarDiffuse * pElement->pVoice->FarDiffuse;
}

//==============================================================================

inline f32 CalculateElementEffectSend( element* pElement )
{
    return pElement->Params.EffectSend * pElement->pVoice->EffectSend;
}

//==============================================================================
#endif // AUDIO_HELPERS_HPP
//==============================================================================
