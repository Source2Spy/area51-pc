//==============================================================================
//
//  audio_package_registry.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_package_registry.hpp"
#include "Audio/audio_runtime.hpp"
#include "x_log.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_package_registry::audio_package_registry( void )
{
    m_pRuntime = NULL;
    ResetPackageList();
}

//==============================================================================

audio_package_registry::~audio_package_registry( void )
{
}

//==============================================================================

void audio_package_registry::Init( audio_runtime& Runtime )
{
    m_pRuntime = &Runtime;
    ResetPackageList();

    // Nuke the identifier table.
    m_pIdentifiers.Clear();
    m_pIdentifiers.SetCapacity( 2048 );
}

//==============================================================================

void audio_package_registry::Kill( void )
{
    UnloadAllPackages();

    m_pIdentifiers.Clear();
    m_pIdentifiers.SetCapacity( 0 );
    m_pIdentifiers.FreeExtra();

    ResetPackageList();
    m_pRuntime = NULL;
}

//==============================================================================

void audio_package_registry::ResetPackageList( void )
{
    m_Link.pPrev    = &m_Link;
    m_Link.pNext    = &m_Link;
    m_Link.pPackage = NULL;
}

//==============================================================================

xbool audio_package_registry::LoadPackage( const char* pFilename, const char* pLocalizedName )
{
    xtimer t;

    t.Start();

    // Create a new audio package.
    audio_package* pPackage = new audio_package;

    // Load the package.
    if( pPackage->Init( Runtime(), pLocalizedName, pFilename ) )
    {
        // Set the package.
        pPackage->m_Link.pPackage = pPackage;

        // Now insert the package into the package list.
        pPackage->m_Link.pNext = m_Link.pNext;
        pPackage->m_Link.pPrev = &m_Link;
        m_Link.pNext->pPrev    = &pPackage->m_Link;
        m_Link.pNext           = &pPackage->m_Link;

        // Re-merge the identifier tables.
        MergeIdentifierTables();

        t.Stop();
        LOG_MESSAGE("audio_mgr::LoadPackage","Loaded package %s in %2.02fms",pFilename, t.ReadMs());
        // Its all good!

        return TRUE;
    }
    else
    {
        // Nuke it.
        delete pPackage;

        // Oops...
        return FALSE;
    }
}

//==============================================================================

xbool audio_package_registry::IsPackageLoaded( const char* pFilename )
{
    return( FindPackageByName( pFilename ) != NULL );
}

//==============================================================================

xbool audio_package_registry::UnloadPackage( const char* pFilename )
{
    // Find the package by its name.
    audio_package* pPackage = FindPackageByName( pFilename );

    if( (pPackage ) != NULL )
    {
        // Take it out of the list
        pPackage->m_Link.pPrev->pNext = pPackage->m_Link.pNext;
        pPackage->m_Link.pNext->pPrev = pPackage->m_Link.pPrev;

        // Kill it.
        pPackage->Kill();

        // Nuke it.
        delete pPackage;

        // Re-merge the identifier tables.
        MergeIdentifierTables();

        // All good!
        return TRUE;
    }
    else
    {
        // Oops couldn't find it.
        return FALSE;
    }
}

//==============================================================================

xbool audio_package_registry::LoadPackageStrings( const char* pFilename, const char* pLocalizedName, xarray<xstring>& Strings )
{
    xbool          bNeedToLoad;
    xbool          bNeedToUnload;
    audio_package* pPackage;

    // Find the package by its name.
    pPackage    = FindPackageByName( pFilename );
    bNeedToLoad = (pPackage == NULL);

    // Need to load it?
    if( bNeedToLoad )
    {
        // Load it and find the package.
        bNeedToUnload = LoadPackage( pFilename, pLocalizedName );
        pPackage      = FindPackageByName( pFilename );
    }
    else
    {
        // No need to unload it.
        bNeedToUnload = FALSE;
    }

    // Found it?
    if( pPackage )
    {
        for( s32 i=0 ; i < pPackage->m_Header.nIdentifiers ; i++ )
        {
            u32   Offset;
            char* pString;

            // Calculate the string table offsets.
            Offset = pPackage->m_IdentifierTable[ i ].StringOffset;
            pString = pPackage->m_IdentifierStringTable + Offset;
            Strings.Append() = pString;
        }

        // Unload the package.
        if( bNeedToUnload )
        {
            UnloadPackage( pFilename );
        }

        // Its all good!
        return TRUE;
    }
    else
    {
        // Could not find it!
        return FALSE;
    }
}

//==============================================================================

void audio_package_registry::UnloadAllPackages( void )
{
#if 0
    audio_package::package_link* pLink;
    audio_package::package_link* pNext;

    // Get first package in the list.
    pLink = m_Link.pNext;

    // While we have packages...
    while( pLink->pPackage )
    {
        pNext = m_Link.pNext;
        UnloadPackage( pLink->pPackage->m_Filename );
        pLink = pNext;
    }

#else
    // who put this code here?
    while (m_Link.pNext->pPackage)
    {
        UnloadPackage(m_Link.pNext->pPackage->m_Filename);
    }
#endif
}

//==============================================================================

void audio_package_registry::GetLoadedPackages( xarray<xstring>& Packages )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    while( pLink->pPackage )
    {
        // search backwards looking for a \ or /
        s32   i = x_strlen( pLink->pPackage->m_Filename );
        char* p = &pLink->pPackage->m_Filename[ i ];

        while( (i--) && (*(p-1) != '\\') && (*(p-1) != '/') )
        {
            p--;
        }

        // Put it in the list
        Packages.Append( (xstring)p );

        // Walk the list.
        pLink = pLink->pNext;
    }
}

//==============================================================================

void audio_package_registry::GetLoadedPackageLookupNames( xarray<xstring>& Packages )
{
    audio_package::package_link* pLink = m_Link.pNext;

    while( pLink->pPackage )
    {
        Packages.Append( pLink->pPackage->m_LookupName );
        pLink = pLink->pNext;
    }
}

//==============================================================================

void audio_package_registry::DisplayPackages( void )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    while( pLink->pPackage )
    {
        x_DebugMsg( (const char*)xfs("%s\n", pLink->pPackage->m_Filename ) );
        // Walk the list.
        pLink = pLink->pNext;
    }
}

//==============================================================================

s32 audio_package_registry::GetPackageARAM( const char* pName )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    while( pLink->pPackage )
    {
        // search backwards looking for a \ or /
        s32   i = x_strlen( pLink->pPackage->m_Filename );
        char* p = &pLink->pPackage->m_Filename[ i ];

        while( (i--) && (*(p-1) != '\\') && (*(p-1) != '/') )
        {
            p--;
        }

        if( x_stricmp( pName, p ) == 0 )
        {
            return pLink->pPackage->m_Header.Aram;
        }

        // Walk the list.
        pLink = pLink->pNext;
    }

    return 0;
}

//==============================================================================

audio_package* audio_package_registry::FindPackageByName( const char* pFilename )
{
    audio_package::package_link* pLink;

    ASSERT( pFilename );

    // Get first package in the list.
    pLink = m_Link.pNext;

    // While we have packages...
    while( pLink->pPackage )
    {
        // Names match?
        if( !x_strncmp( pFilename, pLink->pPackage->m_LookupName, AUDIO_PACKAGE_FILENAME_LENGTH ) ||
            !x_strncmp( pFilename, pLink->pPackage->m_Filename,    AUDIO_PACKAGE_FILENAME_LENGTH ) )
        {
            // All good!
            return pLink->pPackage;
        }

        // Walk the list.
        pLink = pLink->pNext;
    }

    // Oops...couldn't find it...
    return NULL;
}

//==============================================================================

char* audio_package_registry::GetMusicType( const char* pFilename )
{
    audio_package* pPackage = FindPackageByName( pFilename );

    if( pPackage )
    {
        return pPackage->GetMusicType();
    }
    else
    {
        return NULL;
    }
}

//==============================================================================

s32 audio_package_registry::GetMusicIntensity( const char* pFilename, music_intensity* &Intensity )
{
    audio_package* pPackage = FindPackageByName( pFilename );

    if( pPackage )
    {
        return pPackage->GetMusicIntensity( Intensity );
    }
    else
    {
        Intensity = NULL;
        return      0;
    }
}

//==============================================================================

void audio_package_registry::ReMergeIdentifierTables( void )
{
    m_pIdentifiers.Clear();
    m_pIdentifiers.SetCapacity( 0 );
    m_pIdentifiers.FreeExtra();
    MergeIdentifierTables();
}

//==============================================================================

static s32 s_nMerges = 0;

//==============================================================================

void audio_package_registry::MergeIdentifierTables( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_mgr::MergeIdentifierTables");

    audio_package::package_link* pLink;
    xarray<audio_package*>       pPackages;
    xarray<s32>                  PackageIndices;
    s32                          i;

    // Nuke the identifer table
    m_pIdentifiers.Clear();
    pPackages.Clear();
    PackageIndices.Clear();

    // Calculate the size of the table
    pLink = m_Link.pNext;
    s32 TotalIdentifiers = 0;

    while( pLink->pPackage )
    {
        // If the package is loaded
        if( pLink->pPackage->m_IsLoaded )
        {
            // Add this package to the list
            pPackages.Append() = pLink->pPackage;

            // Initialize the number of indices processd to 0.
            PackageIndices.Append() = 0;

            // Keep track of total number of identifiers
            TotalIdentifiers += pLink->pPackage->m_Header.nIdentifiers;
        }

        // Walk the package list.
        pLink = pLink->pNext;
    }

    // Set the size of the identifier table.
    m_pIdentifiers.SetCapacity( TotalIdentifiers );

    // Perform the merge sort.
    while( pPackages.GetCount() )
    {
        i = 0;

        for( s32 j=1 ; j<pPackages.GetCount() ; j++ )
        {
            char* pSmallest;
            char* pCurrent;
            u32   SmallestOffset;
            u32   CurrentOffset;
            s32   Result;

            // Calculate the string table offsets.
            SmallestOffset = pPackages[ i ]->m_IdentifierTable[ PackageIndices[ i ] ].StringOffset;
            CurrentOffset  = pPackages[ j ]->m_IdentifierTable[ PackageIndices[ j ] ].StringOffset;

            // Get pointer to the string.
            pSmallest = pPackages[ i ]->m_IdentifierStringTable + SmallestOffset;
            pCurrent  = pPackages[ j ]->m_IdentifierStringTable + CurrentOffset;

            Result = x_strcmp( pCurrent, pSmallest );

            if( Result < 0 )
            {
                // It's smaller!
                i = j;
            }
            else if( Result == 0 )
            {
                // BAD! Had a name collision
                // TODO: Put in warning...
            }
        }

        // Merge 'em
        m_pIdentifiers.Append() = &pPackages[ i ]->m_IdentifierTable[ PackageIndices[ i ] ];
        PackageIndices[ i ]++;

        if( PackageIndices[ i ] >= pPackages[ i ]->m_Header.nIdentifiers )
        {
            pPackages.Delete( i );
            PackageIndices.Delete( i );
        }
    }

    s_nMerges++;
}

//==============================================================================

xbool audio_package_registry::IsValidDescriptor( const char* pName )
{
    u16*  pDescriptor;
    char* pString;

    // Find the descriptor.
    pDescriptor = FindDescriptorByName( pName, NULL, pString );

    // Find it?
    return ( pDescriptor ) ? TRUE : FALSE;
}

//==============================================================================

u16* audio_package_registry::FindDescriptorByName( const char* pName, audio_package** pPackageResult, char* &DescriptorName )
{
    s32  Left  = 0;
    s32  Right = m_pIdentifiers.GetCount()-1;
    s32  Mid;
    char ucName[128];

    ASSERT( x_strlen(pName) < 128 );
    x_strncpy( ucName, pName, 128 );
    ucName[127] = 0;
    x_strtoupper( ucName );

    if( Right < 0 )
    {
        if( pPackageResult )
        {
            *pPackageResult = NULL;
        }
        return NULL;
    }

    while( 1 )
    {
        descriptor_identifier* pIdentifier;
        audio_package*         pPackage;
        char*                  pString;
        s32                    Result;

        // Get package, index and offset from table
        pIdentifier = m_pIdentifiers[ Mid = (Left+Right) >> 1 ];
        pPackage    = pIdentifier->pPackage;
        pString     = pPackage->m_IdentifierStringTable + pIdentifier->StringOffset;

        // Exact match?
        Result = x_strcmp( ucName, pString );

        // Smaller?
        if( Result < 0 )
        {
            if( Right == Left )
            {
                if( pPackageResult )
                {
                    *pPackageResult = NULL;
                }
                return NULL;
            }
            else if (Right == Mid )
            {
                Mid = Left;
            }

            Right = Mid;
        }

        // Bigger?
        else if( Result > 0 )
        {
            if( Left == Right )
            {
                if( pPackageResult )
                {
                    *pPackageResult = NULL;
                }
                return NULL;
            }
            else if (Left == Mid )
            {
                Mid = Right;
            }

            Left = Mid;
        }

        // Oooh! Found it!
        else
        {
            DescriptorName = pString;

            if( pPackageResult )
            {
                *pPackageResult = pPackage;
            }
            return( (u16*)pPackage->m_DescriptorTable[ pIdentifier->Index ] );
        }
    }

    return NULL;
}

//==============================================================================

void audio_package_registry::SetMasterVolume( f32 Volume )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    // While we have packages...
    while( pLink->pPackage )
    {
        // Set the volume
        pLink->pPackage->SetUserVolume( Volume );

        // Walk the list.
        pLink = pLink->pNext;
    }
}

//==============================================================================

void audio_package_registry::SetMusicVolume( f32 Volume )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    // While we have packages...
    while( pLink->pPackage )
    {
        // Names match?
        if( x_strstr( pLink->pPackage->m_Filename, "MUSIC_" ) )
        {
            // All good!
            pLink->pPackage->SetUserVolume( Volume );
        }

        // Walk the list.
        pLink = pLink->pNext;
    }
}

//==============================================================================

void audio_package_registry::SetSFXVolume( f32 Volume )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    // While we have packages...
    while( pLink->pPackage )
    {
        // Names match?
        // for SFX, if it's not music and not voice, it has to be a SFX
        if( (x_strstr( pLink->pPackage->m_Filename, "MUSIC_" ) == 0) &&
            (x_strstr( pLink->pPackage->m_Filename, "DX_"    ) == 0) )
        {
            // All good!
            pLink->pPackage->SetUserVolume( Volume );
        }

        // Walk the list.
        pLink = pLink->pNext;
    }
}

//==============================================================================

void audio_package_registry::SetVoiceVolume( f32 Volume )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    // While we have packages...
    while( pLink->pPackage )
    {
        // Names match?
        if( x_strstr( pLink->pPackage->m_Filename, "DX_" ) )
        {
            // All good!
            pLink->pPackage->SetUserVolume( Volume );
        }

        // Walk the list.
        pLink = pLink->pNext;
    }
}

//==============================================================================

void audio_package_registry::ComputePackageVolumes( void )
{
    audio_package::package_link* pLink;

    // Get first package in the list.
    pLink = m_Link.pNext;

    // While we have packages...
    while( pLink->pPackage )
    {
        // Compute the package volume.
        pLink->pPackage->ComputeVolume();

        // Walk the list.
        pLink = pLink->pNext;
    }
}
