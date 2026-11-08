//==============================================================================
//
//  x_target.hpp
//
//==============================================================================

#ifndef X_TARGET_HPP
#define X_TARGET_HPP

//==============================================================================
//  Runtime platform families
//==============================================================================

enum platform
{
    PLATFORM_NONE    = 0,
    PLATFORM_DESKTOP = (1<<0),
    PLATFORM_MOBILE  = (1<<1),
    PLATFORM_ALL     = PLATFORM_DESKTOP | PLATFORM_MOBILE
};

//==============================================================================
//  Asset export platforms
//------------------------------------------------------------------------------
// These values are serialized in editor/resource data. Keep their numeric
// values stable while the runtime platform family is modernized.
//==============================================================================

enum asset_platform
{
    ASSET_PLATFORM_NONE    = 0,
    ASSET_PLATFORM_DESKTOP = (1<<0), // Former PC asset target.
    ASSET_PLATFORM_GCN     = (1<<1),
    ASSET_PLATFORM_PS2     = (1<<2),
    ASSET_PLATFORM_XBOX    = (1<<3),
    ASSET_PLATFORM_MOBILE  = (1<<4),
    ASSET_PLATFORM_ALL     = 0xffffffff
};

//==============================================================================
//  Targets
//------------------------------------------------------------------------------
// The valid targets are WINDOWS, ANDROID, and LINUX.
//==============================================================================

#if defined( TARGET_ANDROID )
    #ifdef VALID_TARGET
        #define MULTIPLE_TARGETS
    #else
        #define TARGET_MOBILE
        #define TARGET_POSIX
        #define X_LITTLE_ENDIAN
        #define X_EXCEPTIONS
        #define VALID_TARGET
    #endif
#endif

//------------------------------------------------------------------------------

#if defined( TARGET_LINUX )
    #ifdef VALID_TARGET
        #define MULTIPLE_TARGETS
    #else
        #define TARGET_DESKTOP
        #define TARGET_POSIX
        #define X_LITTLE_ENDIAN
        #define X_EXCEPTIONS
        #define VALID_TARGET
    #endif
#endif

//------------------------------------------------------------------------------

// TARGET_WINDOWS will be the default if no TARGET_ macro was defined
#if( defined( TARGET_WINDOWS ) || !defined( VALID_TARGET ) )
    #ifdef VALID_TARGET
        #define MULTIPLE_TARGETS
    #else
        #define TARGET_DESKTOP    
        #define X_LITTLE_ENDIAN
        #define X_EXCEPTIONS
        #define VALID_TARGET
    #endif
#endif

//==============================================================================
// Configs
//------------------------------------------------------------------------------
// Valid configurations are DEBUG,QA,RETAIL,VIEWER,and OPTDEBUG. OPTDEBUG is the
// optimised version of DEBUG. QA is identical to RETAIL but has debugging info,
// a special X_QA define to support the debug menu, etc. but nothing else.
//==============================================================================

#if defined( CONFIG_DEBUG )
    #if defined( VALID_CONFIG )
        #define MULTIPLE_CONFIGS
    #else
        #define VALID_CONFIG
        #define TARGET_DEV
        #define X_DEBUG_MSG
        #define X_PROFILE 1
        #if defined( TARGET_DESKTOP )
            #define X_PROFILE_TRACY 1
        #endif
        #define X_LOGGING
        #define X_ASSERT
        #define X_DEBUG
        #define X_MEM_DEBUG
        #define USE_OWNER_STACK
    #endif
#endif

//------------------------------------------------------------------------------

#if defined( CONFIG_OPTDEBUG )
    #if defined( VALID_CONFIG )
        #define MULTIPLE_CONFIGS
    #else
        #define VALID_CONFIG
        #define TARGET_DEV
        #define X_ASSERT
        #define X_OPTIMIZED
        #define X_DEBUG_MSG
        #define X_LOGGING
        #define X_DEBUG
        #define X_PROFILE 1
        #if defined( TARGET_DESKTOP )
            #define X_PROFILE_TRACY 1
        #endif
        #define X_MEM_DEBUG
        #define USE_OWNER_STACK
    #endif
#endif

//------------------------------------------------------------------------------

#if defined( CONFIG_PROFILE )
    #if defined( VALID_CONFIG )
        #define MULTIPLE_CONFIGS
    #else
        #define VALID_CONFIG
        #define TARGET_DEV
        #define X_OPTIMIZED
        #define X_RETAIL
    #endif
#endif

//------------------------------------------------------------------------------

#if defined( CONFIG_QA )
    #if defined( VALID_CONFIG )
        #define MULTIPLE_CONFIGS
    #else
        #define VALID_CONFIG
        #define TARGET_DVD
        #define X_OPTIMIZED
        #define X_RETAIL
        #define X_QA
    #endif
#endif

//------------------------------------------------------------------------------

#if defined( CONFIG_RETAIL )
    #if defined( VALID_CONFIG )
        #define MULTIPLE_CONFIGS
    #else
        #define VALID_CONFIG
        #define TARGET_DVD
        #define X_OPTIMIZED
        #define X_RETAIL
    #endif
#endif

//------------------------------------------------------------------------------

#ifndef X_PROFILE
    #define X_PROFILE 0
#endif

#ifndef X_PROFILE_TRACY
    #define X_PROFILE_TRACY 0
#endif

//==============================================================================
//  Applications
//==============================================================================


//------------------------------------------------------------------------------

#if defined( APP_EDITOR )
    #define __PLACEMENT_NEW_INLINE  // Tells MFC that we are dealing with the placement new/delete
    #define USE_SYSTEM_NEW_DELETE   // Tells x_files not to define new/delete
    #define X_EDITOR
    #if !defined( X_RETAIL )
        #define X_ASSERT
        # undef X_ASSERT_LITE
    #endif
#endif

//------------------------------------------------------------------------------

#if defined( VALID_TARGET )
    #define USE_SYSTEM_NEW_DELETE
#endif

//==============================================================================
//
//  Make sure we found a proper target specification.  If you get a compilation 
//  error here, then your compilation environment is not specifying one of the
//  target macros.
//
//==============================================================================

#if !defined( VALID_TARGET )
    #error Target specification invalid or not found.
    #error The compilation environment must define one of the macros listed in x_targets.hpp.
#endif

//------------------------------------------------------------------------------

#if !defined( VALID_CONFIG )
    #error Config specification invalid or not found.
    #error The compilation environment must define one of the macros listed in x_targets.hpp.
#endif

//==============================================================================
//
//  Make sure we did not somehow get multiple targer platform specifications.
//  *** IF YOU GOT AN ERROR HERE ***, then you have defined more than one of
//  the target specification macros.
//
//==============================================================================

#if defined( MULTIPLE_TARGETS )
    #error Multiple target specification definition macros were detected.
    #error The compilation environment must define only one of the macros listed in x_targets.hpp.
#endif

//==============================================================================
//
//  Make sure Endian is properly defined.
//
//==============================================================================

#if( !defined( X_BIG_ENDIAN ) && !defined( X_LITTLE_ENDIAN ) )
    #error Endian is not defined.
#endif

//------------------------------------------------------------------------------

#if(  defined( X_BIG_ENDIAN ) &&  defined( X_LITTLE_ENDIAN ) )
    #error Both Endian specifications are defined!
#endif

//==============================================================================
//
//  Platform specific data structure alignment.
//
//==============================================================================

#if defined( _MSC_VER )
    #define PC_ALIGNMENT(a) __declspec(align(a))
#elif defined( __GNUC__ ) || defined( __clang__ )
    #define PC_ALIGNMENT(a) __attribute__((aligned(a)))
#else
    #define PC_ALIGNMENT(a)
#endif

//------------------------------------------------------------------------------

#ifndef X_ALIGNMENT
    #if defined( _MSC_VER )
        #define X_ALIGNMENT(a) __declspec(align(a))
    #elif defined( __GNUC__ ) || defined( __clang__ )
        #define X_ALIGNMENT(a) __attribute__((aligned(a)))
    #else
        #define X_ALIGNMENT(a)
    #endif
#endif

//==============================================================================
#endif // X_TARGET_HPP
//==============================================================================
