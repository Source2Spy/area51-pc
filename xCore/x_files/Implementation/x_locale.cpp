//==============================================================================
//  x_locale.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for generalizing basic localization functions.
//  Note that unlike many of the other x_file functions, these functions do not
//  adhere closely to any standard functions such as setLocal(). Because consoles
//  and game apps have very simple needs, these functions generally only support 
//  language settings and time/date formats. 
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../x_types.hpp"

#ifndef X_LOCALE_HPP
#include "../x_locale.hpp"
#endif

#ifndef X_DEBUG_HPP
#include "../x_debug.hpp"
#endif

//==============================================================================
//  VARIABLES
//==============================================================================

static  x_language          s_LocaleLang = XL_LANG_ENGLISH;
static  x_console_territory s_Territory = XL_TERRITORY_AMERICA;


//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

// GS: Let's continue to obtain the system language this way for now; 
// in the future, the user will have to specify it in the UI themselves.

#ifdef TARGET_WINDOWS
static 
x_language x_MapWindowsLanguage( LANGID const LangId )
{
    WORD const Primary = PRIMARYLANGID( LangId );

    switch( Primary )
    {
        case LANG_ENGLISH:     return XL_LANG_ENGLISH;
        case LANG_FRENCH:      return XL_LANG_FRENCH;
        case LANG_GERMAN:      return XL_LANG_GERMAN;
        case LANG_ITALIAN:     return XL_LANG_ITALIAN;
        case LANG_SPANISH:     return XL_LANG_SPANISH;    
        case LANG_JAPANESE:    return XL_LANG_JAPANESE;
        case LANG_CZECH:       return XL_LANG_CZECH;
        //case LANG_DUTCH:       return XL_LANG_DUTCH;    
        //case LANG_KOREAN:      return XL_LANG_KOREAN;   
        //case LANG_PORTUGUESE:  return XL_LANG_PORTUGUESE; 
        //case LANG_RUSSIAN:      return XL_LANG_RUSSIAN;
        //case LANG_CHINESE:
        //{
        //    WORD const SubLang = SUBLANGID( LangId );
        //
        //    switch( SubLang )
        //    {
        //        case SUBLANG_CHINESE_TRADITIONAL:
        //        case SUBLANG_CHINESE_HONGKONG:
        //        case SUBLANG_CHINESE_MACAU:
        //            return XL_LANG_TCHINESE;
        //        default:
        //            break;
        //    }
        //    break;
        //}
        default:
            return XL_LANG_ENGLISH;
    }
    
    return XL_LANG_ENGLISH;
}
#endif

//==============================================================================

#ifdef TARGET_POSIX
// TODO:
#endif

//==============================================================================

// these strings correspond to the ISO 639 language codes.
// please stick to this standard when adding new languages.
// Note also, that these must appear in the same order as the language enums!!

static 
const char* const s_pLanguageStrISO639_2[] =
{
    "ENG",      // XL_LANG_ENGLISH
    "FRE",      // XL_LANG_FRENCH
    "GER",      // XL_LANG_GERMAN
    "ITA",      // XL_LANG_ITALIAN
    "SPA",      // XL_LANG_SPANISH
    "DUT",      // XL_LANG_DUTCH
    "JPN",      // XL_LANG_JAPANESE
    "KOR",      // XL_LANG_KOREAN
    "POR",      // XL_LANG_PORTUGUESE
    "CHI",      // XL_LANG_TCHINESE
    "RUS",      // XL_LANG_RUSSIAN
    "CZE"       // XL_LANG_CZECH
};

//==============================================================================

static 
const char* const s_pLanguageStrISO639_1[] =
{
    "en",       // XL_LANG_ENGLISH
    "fr",       // XL_LANG_FRENCH
    "de",       // XL_LANG_GERMAN
    "it",       // XL_LANG_ITALIAN
    "es",       // XL_LANG_SPANISH
    "nl",       // XL_LANG_DUTCH
    "ja",       // XL_LANG_JAPANESE
    "ko",       // XL_LANG_KOREAN
    "pt",       // XL_LANG_PORTUGUESE
    "zh",       // XL_LANG_TCHINESE
    "ru",       // XL_LANG_RUSSIAN
    "cs"        // XL_LANG_CZECH
};

//==============================================================================

static
const char* x_GetLocaleStringInternal( x_language const Language, x_locale_code_format const Format )
{
    ASSERT( (Language >= XL_LANG_ENGLISH) && (Language < XL_NUM_LANGUAGES) );
    ASSERT( (Format >= XL_LOCALE_CODE_ISO_639_1) && (Format < XL_NUM_LOCALE_CODE_FORMATS) );

    switch( Format )
    {
        case XL_LOCALE_CODE_ISO_639_1:
            return s_pLanguageStrISO639_1[Language];
        case XL_LOCALE_CODE_ISO_639_2:
            return s_pLanguageStrISO639_2[Language];
        default:
            ASSERT( 0 );
            break;
    }
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

//==============================================================================
// Returns language enumeration for the currently set language on console hardware.
//
// Parameters:
//
// Returns:
//  x_language enumeration of consoles' set language.
//
// Remarks:
//  Use this during application init to determine the platform's language setting.
//  Application is responsible for determining whether the returned language is 
//  supported, and supply a suitable default.
//==============================================================================

const x_language x_GetConsoleLanguage( void )
{      
#if defined( TARGET_WINDOWS )
    LANGID const LangId = GetUserDefaultLangID();
    x_language const Lang = x_MapWindowsLanguage( LangId );
    return Lang;
#elif defined( TARGET_POSIX )
    // TODO:
    return XL_LANG_ENGLISH;
#else
    return XL_LANG_ENGLISH;
#endif
}

//==============================================================================
// Returns territory enumeration for the currently set region on console hardware.
//
// Parameters:
//
// Returns:
//  x_console_territory enumeration of consoles' set region.
//
// Remarks:
//  This is only supported on Xbox - PS2 and PC have no equivelent.
//==============================================================================

const x_console_territory x_GetConsoleRegion  ( void )
{
/* XBOX
    switch( XGetGameRegion() )
    {
        default:
        case XC_GAME_REGION_NA:         return XL_TERRITORY_AMERICA;
        case XC_GAME_REGION_JAPAN:      return XL_TERRITORY_JAPAN; 
        case XC_GAME_REGION_RESTOFWORLD:return XL_TERRITORY_EUROPE;
    }
*/
    ASSERTS(0, "This function is no supported on this platform");
    return XL_TERRITORY_AMERICA;
}

//==============================================================================
// Sets the application's selected language.
//
// Parameters:
//  x_language enumeration.
//
// Returns:
//
// Remarks:
//  Once the application determines the correct default language (or it is changed
//  in the case we have a menu), we set the system language here.
//==============================================================================

void x_SetLocale( x_language const Language )
{
    s_LocaleLang = Language;
}

//==============================================================================
// returns the application's selected language.
//
// Parameters:
//
// Returns:
//  x_language enumeration for current language.
//
// Remarks:
//  Use THIS instead of GetLanguage() for run-time operations that 
//  require the current language.
//==============================================================================

const x_language x_GetLocale( void )
{
    return s_LocaleLang;
}

//==============================================================================
// returns language code string for the current language.
//
// Parameters:
//
// Returns:
//  pointer to string code for current language.
//
// Remarks:
//  Use for filename manipulation to select localized assets.
//==============================================================================

const char* x_GetLocaleString( x_locale_code_format Format )
{
    return x_GetLocaleStringInternal( s_LocaleLang, Format );
}

//==============================================================================

const char* x_GetLocaleString( x_language Language, x_locale_code_format Format )
{
    return x_GetLocaleStringInternal( Language, Format );
}

//==============================================================================
// Sets an enumeration for the console territory
//
// Parameters:
//  x_console_territory enumeration
//
// Returns:
//
// Remarks:
//  
//==============================================================================

void x_SetTerritory( const x_console_territory territory )
{
    s_Territory = territory;
}

//==============================================================================
// returns console territory.
//
// Parameters:
//
// Returns:
//  x_console_territory enumeration
//
// Remarks:
//  
//==============================================================================

const x_console_territory x_GetTerritory( void )
{
    return s_Territory;
}

//==============================================================================
// returns TRUE if build is European
//
// Parameters:
//
// Returns:
//  TRUE if the territory corresponds to a censored build.
//
// Remarks:
//   Currently, European build is censored.
//  
//==============================================================================

const xbool x_IsBuildCensored( void )
{
    return (x_GetTerritory() == XL_TERRITORY_EUROPE);
}
