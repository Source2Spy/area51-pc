//==============================================================================
//
//  PainMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "PainMgr/PainMgr.hpp"
#include "Parsing/TextIn.hpp"
#include "Auxiliary/MiscUtils/dictionary.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"

#define DO_LOG_PAIN

//==============================================================================
// SUPPORT FOR GENERIC PAIN
//==============================================================================

typedef enum_pair<generic_pain_type> generic_pain_type_enum_pair;
static generic_pain_type_enum_pair s_GenericPainTypeList[] = 
{
    generic_pain_type_enum_pair( "Generic_1",            TYPE_GENERIC_1 ),
    generic_pain_type_enum_pair( "Generic_2",            TYPE_GENERIC_2 ),
    generic_pain_type_enum_pair( "Generic_3",            TYPE_GENERIC_3 ),
    generic_pain_type_enum_pair( "Generic_Lethal",       TYPE_LETHAL    ),
    generic_pain_type_enum_pair( "Generic_Explosive",    TYPE_EXPLOSIVE ),
    generic_pain_type_enum_pair( "Generic_LASER",        TYPE_LASER     ),
    generic_pain_type_enum_pair( "ACID_WEAK",            TYPE_ACID_1    ),
    generic_pain_type_enum_pair( "ACID_STRONG",          TYPE_ACID_2    ),
    generic_pain_type_enum_pair( "ACID_LETHAL",          TYPE_ACID_3    ),

    generic_pain_type_enum_pair( "GOO_WEAK",             TYPE_GOO_1     ),
    generic_pain_type_enum_pair( "GOO_STRONG",           TYPE_GOO_2     ),
    generic_pain_type_enum_pair( "GOO_LETHAL",           TYPE_GOO_3     ),

    generic_pain_type_enum_pair( "FIRE_WEAK",            TYPE_FIRE_1    ),
    generic_pain_type_enum_pair( "FIRE_STRONG",          TYPE_FIRE_2    ),
    generic_pain_type_enum_pair( "FIRE_LETHAL",          TYPE_FIRE_3    ),

    generic_pain_type_enum_pair( "DROWNING",             TYPE_DROWNING  ),

    generic_pain_type_enum_pair( k_EnumEndStringConst,   TYPE_GENERIC_1 )
};
enum_table<generic_pain_type>  g_GenericPainTypeList( s_GenericPainTypeList );              

//==============================================================================

pain_handle GetPainHandleForGenericPain( generic_pain_type Type )
{
    ASSERT( g_GenericPainTypeList.DoesValueExist( Type ) );
    pain_handle Handle( g_GenericPainTypeList.GetString( Type ) );
    return Handle;
}

//==============================================================================
//  PAIN_HANDLE
//==============================================================================

pain_handle::pain_handle( void )
{
    data_handle::SetType( DATA_TYPE_PAIN_PROFILE );
}

//==============================================================================

pain_handle::~pain_handle( void )
{
}

//==============================================================================

pain_handle::pain_handle( const char* pPainDescriptor )
{
    data_handle::SetType( DATA_TYPE_PAIN_PROFILE );
    data_handle::SetName( pPainDescriptor );
}

//==============================================================================

const pain_profile* pain_handle::GetPainProfile( void ) const
{
    const pain_profile* pData = (pain_profile*)GetData();

    #if defined(DATA_VAULT_KEEP_NAMES)
        CLOG_ERROR((pData==NULL),"MISSING PAIN PROFILE","(%s)",GetName());
    #endif

    return pData;
}

//==============================================================================

pain_health_handle pain_handle::BuildPainHealthProfileHandle( const health_handle& HHandle ) const
{
    pain_health_handle PHHandle;
    const pain_profile* pPP = GetPainProfile();
    const health_profile* pHP = HHandle.GetHealthProfile();
    if( pPP && pHP )
    {
        PHHandle.SetName(xfs("PH(%03d)(%03d)",pPP->m_iPainHealthTable,pHP->m_iPainHealthTable));
    }
    return PHHandle;
}

//==============================================================================
// HEALTH_HANDLE
//==============================================================================

health_handle::health_handle( void )
{
    data_handle::SetType( DATA_TYPE_HEALTH_PROFILE );
}

//==============================================================================

health_handle::~health_handle( void )
{
}

//==============================================================================

health_handle::health_handle( const char* pHealthDescriptor )
{
    data_handle::SetType( DATA_TYPE_HEALTH_PROFILE );
    data_handle::SetName( pHealthDescriptor );
}

//==============================================================================

const health_profile* health_handle::GetHealthProfile( void ) const
{
    const health_profile* pData = (health_profile*)GetData();

    #if defined(DATA_VAULT_KEEP_NAMES)
        CLOG_ERROR((pData==NULL),"MISSING HEALTH PROFILE","(%s)",GetName());
    #endif

    return pData;
}

//==============================================================================
// PAIN_HEALTH_HANDLE
//==============================================================================

pain_health_handle::pain_health_handle( void )
{
    data_handle::SetType( DATA_TYPE_PAIN_HEALTH_PROFILE );
}

//==============================================================================

pain_health_handle::~pain_health_handle( void )
{
}

//==============================================================================

pain_health_handle::pain_health_handle( const char* pPainHealthDescriptor )
{
    data_handle::SetType( DATA_TYPE_PAIN_HEALTH_PROFILE );
    data_handle::SetName( pPainHealthDescriptor );
}

//==============================================================================

const pain_health_profile* pain_health_handle::GetPainHealthProfile( void ) const
{
    const pain_health_profile* pData = (pain_health_profile*)GetData();

    #if defined(DATA_VAULT_KEEP_NAMES)
        CLOG_ERROR((pData==NULL),"MISSING PAIN_HEALTH","(%s)",GetName());
    #endif

    return pData;
}

//==============================================================================
//  DATA_BLOCK
//==============================================================================

pain_profile::pain_profile( void )
{
    SetType( DATA_TYPE_PAIN_PROFILE );
}

//==============================================================================

pain_profile::~pain_profile( void )
{
}

//==============================================================================

health_profile::health_profile( void )
{
    SetType( DATA_TYPE_HEALTH_PROFILE );
}

//==============================================================================

health_profile::~health_profile( void )
{
}

//==============================================================================

pain_health_profile::pain_health_profile( void )
{
    SetType( DATA_TYPE_PAIN_HEALTH_PROFILE );
}

//==============================================================================

pain_health_profile::~pain_health_profile( void )
{
}


//==============================================================================
//  STRUCTURES
//==============================================================================

struct pain_load_data
{
    // Keep all tables local until loading and validation have completed.
    xarray<pain_profile>        Sources;
    xarray<health_profile>      Targets;
    xarray<pain_health_profile> Pairs;

    // Source-major lookup into Pairs. A value of -1 means no damage row exists.
    xarray<s32>                 PairIndices;

    // Dictionary indices match the corresponding source and target arrays.
    dictionary                  SourceNames;
    dictionary                  TargetNames;
};

//------------------------------------------------------------------------------

struct pain_damage_entry
{
    s32 Source;
    s32 Target;
    f32 Value;
};

//==============================================================================
//  STORAGE
//==============================================================================

static xbool s_PainLoaded = FALSE;

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static xbool ReportPainError( text_in& Input, const char* pMessage )
{
    // The text reader already reports its own parse errors. Only add a message
    // when the failure comes from table validation.
    if( !Input.HasError() )
    {
        x_DebugMsg( "%s(%d) : error : %s\n", Input.GetFileName(), Input.GetLineNumber(), pMessage );
    }

    return FALSE;
}

//==============================================================================

static xbool OpenPainTable( text_in& Input, const char* pFileName, const char* pSection, s32 Fields )
{
    // Open the file for reading.
    if( !Input.OpenFile( pFileName ) )
    {
        return ReportPainError( Input, "Unable to open tweak file" );
    }

    s32 Version = 0;

    // Read the format header before accepting the requested table section.
    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "TweakTable" ) ||
        Input.GetFieldCount() != 1 || !Input.ReadFields() ||
        !Input.GetS32( "Version", Version ) || Version != 1 )
    {
        return ReportPainError( Input, "Expected TweakTable version 1" );
    }

    // Each table has a fixed field count and must contain at least one entry.
    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), pSection ) ||
        Input.GetFieldCount() != Fields || Input.GetHeaderCount() <= 0 )
    {
        return ReportPainError( Input, xfs( "Invalid %s table", pSection ) );
    }

    return TRUE;
}

//==============================================================================

static xbool FinishPainTable( text_in& Input )
{
    // The declared rows must consume the file; extra sections are not allowed.
    if( Input.ReadHeader() || Input.HasError() || !Input.IsEOF() )
    {
        return ReportPainError( Input, "Unexpected data after tweak table" );
    }

    return TRUE;
}

//==============================================================================
//  LOAD PAIN PROFILES
//==============================================================================

static xbool ReadPainProfiles( const char* pFileName, pain_load_data& Data )
{
    text_in Input;

    if( !OpenPainTable( Input, pFileName, "PainProfiles", 7 ) )
    {
        return FALSE;
    }

    s32 Count = Input.GetHeaderCount();

    // Profile indices are stored in the signed 16-bit pain/health table fields.
    if( Count > 32767 )
    {
        return ReportPainError( Input, "Too many source profiles" );
    }

    Data.Sources.SetCapacity( Count );

    // Loop through all the profiles and add them to the array.
    for( s32 I = 0; I < Count; I++ )
    {
        pain_profile Profile;
        xstring      Name;
        xbool        CheckLOS;
        xbool        IsSplash;

        // Fill out the pain profile data from the named fields.
        if( !Input.ReadFields() || !Input.GetString( "Name", Name ) ||
            !Input.GetF32( "DamageNear", Profile.m_DamageNearDist ) ||
            !Input.GetF32( "DamageFar", Profile.m_DamageFarDist ) ||
            !Input.GetF32( "ForceNear", Profile.m_ForceNearDist ) ||
            !Input.GetF32( "ForceFar", Profile.m_ForceFarDist ) ||
            !Input.GetBool( "CheckLOS", CheckLOS ) || !Input.GetBool( "IsSplash", IsSplash ) )
        {
            return ReportPainError( Input, "Invalid source profile" );
        }

        if( Name.IsEmpty() )
        {
            return ReportPainError( Input, "Empty source profile name" );
        }

        // Normalize names before checking for duplicate pain profiles.
        Name.MakeUpper();

        if( Data.SourceNames.Find( Name ) >= 0 )
        {
            return ReportPainError( Input, xfs( "Duplicate source profile '%s'", (const char*)Name ) );
        }

        // Load the pain profile with its name and retain its table index.
        Data.SourceNames.Add( Name );
        Profile.SetName( Name );
        Profile.m_iPainHealthTable = (s16)I;
        Profile.m_bCheckLOS        = CheckLOS;
        Profile.m_bSplash          = IsSplash;

        // Keep zero-width falloff ranges usable by the damage calculation.
        if( Profile.m_DamageFarDist == Profile.m_DamageNearDist )
        {
            Profile.m_DamageFarDist += 1.0f;
        }

        if( Profile.m_ForceFarDist == Profile.m_ForceNearDist )
        {
            Profile.m_ForceFarDist += 1.0f;
        }

        // Build pain bbox to cover both damage and force, with a minimum radius.
        f32 Radius = MAX( MAX( Profile.m_DamageFarDist, Profile.m_ForceFarDist ), 10.0f );
        Profile.m_BBox = bbox( vector3( 0, 0, 0 ), Radius );

        // Add profile to the array.
        Data.Sources.Append() = Profile;
    }

    return FinishPainTable( Input );
}

//==============================================================================
//  LOAD HEALTH AND PAIN_HEALTH PROFILES
//==============================================================================

static xbool ReadDamageTable( const char* pFileName, pain_load_data& Data )
{
    text_in Input;

    if( !OpenPainTable( Input, pFileName, "Damage", 3 ) )
    {
        return FALSE;
    }

    s32 Count = Input.GetHeaderCount();
    xarray<pain_damage_entry> Entries;
    Entries.SetCapacity( Count );

    // Read source/target damage entries first. This establishes the complete
    // target list before allocating the source-major pair lookup.
    for( s32 I = 0; I < Count; I++ )
    {
        xstring           Source;
        xstring           Target;
        pain_damage_entry Entry;

        if( !Input.ReadFields() || !Input.GetString( "Source", Source ) ||
            !Input.GetString( "Target", Target ) || !Input.GetF32( "Value", Entry.Value ) )
        {
            return ReportPainError( Input, "Invalid damage entry" );
        }

        Source.MakeUpper();
        Target.MakeUpper();
        Entry.Source = Data.SourceNames.Find( Source );

        // A damage row must reference an existing pain profile and a named target.
        if( Entry.Source < 0 || Target.IsEmpty() )
        {
            return ReportPainError( Input, xfs( "Invalid damage pair '%s' / '%s'",
                                               (const char*)Source, (const char*)Target ) );
        }

        Entry.Target = Data.TargetNames.Find( Target );

        // Load each health profile once, using its first appearance in the table.
        if( Entry.Target < 0 )
        {
            if( Data.Targets.GetCount() >= 32767 )
            {
                return ReportPainError( Input, "Too many target profiles" );
            }

            Entry.Target = Data.TargetNames.Add( Target );
            health_profile Profile;
            Profile.SetName( Target );
            Profile.m_iPainHealthTable = (s16)Entry.Target;
            Data.Targets.Append() = Profile;
        }

        Entries.Append() = Entry;
    }

    if( !FinishPainTable( Input ) )
    {
        return FALSE;
    }

    // Index pairs once; force and hit-type rows then resolve without a scan.
    s32 TargetCount = Data.Targets.GetCount();
    Data.PairIndices.SetCount( Data.Sources.GetCount() * TargetCount );

    for( s32 I = 0; I < Data.PairIndices.GetCount(); I++ )
    {
        Data.PairIndices[I] = -1;
    }

    // The original loader documented an O(n^2) slowdown when its profile array
    // grew one entry at a time. Reserve enough space before appending pairs;
    // the damage-row count is an upper bound even when pairs are repeated.
    Data.Pairs.SetCapacity( Entries.GetCount() );

    for( s32 I = 0; I < Entries.GetCount(); I++ )
    {
        const pain_damage_entry& Entry = Entries[I];
        s32& Index = Data.PairIndices[Entry.Source * TargetCount + Entry.Target];

        if( Index < 0 )
        {
            // Build a pain_health_profile for this combination. The generated
            // name must match pain_handle::BuildPainHealthProfileHandle().
            Index = Data.Pairs.GetCount();
            pain_health_profile& Profile = Data.Pairs.Append();
            Profile.SetName( xfs( "PH(%03d)(%03d)", Entry.Source, Entry.Target ) );
            Profile.m_Force   = 0.0f;
            Profile.m_HitType = 0;
        }

        // Repeated damage rows update the same pair instead of creating duplicates.
        Data.Pairs[Index].m_Damage = Entry.Value;
    }

    return TRUE;
}

//==============================================================================
//  FILL PAIN_HEALTH PROFILE DATA
//==============================================================================

static xbool ReadPainValues( const char* pFileName, pain_load_data& Data, xbool HitType )
{
    text_in Input;

    if( !OpenPainTable( Input, pFileName, HitType ? "HitType" : "Force", 3 ) )
    {
        return FALSE;
    }

    s32 Count = Input.GetHeaderCount();

    for( s32 I = 0; I < Count; I++ )
    {
        xstring Source;
        xstring Target;
        f32     Force = 0.0f;
        s32     Type  = 0;

        if( !Input.ReadFields() || !Input.GetString( "Source", Source ) ||
            !Input.GetString( "Target", Target ) ||
            ( HitType ? !Input.GetS32( "Value", Type ) : !Input.GetF32( "Value", Force ) ) )
        {
            return ReportPainError( Input, "Invalid force or hit-type entry" );
        }

        // Resolve both profile names against the tables established by damage rows.
        Source.MakeUpper();
        Target.MakeUpper();
        s32 SourceIndex = Data.SourceNames.Find( Source );
        s32 TargetIndex = Data.TargetNames.Find( Target );

        if( SourceIndex < 0 || TargetIndex < 0 )
        {
            return ReportPainError( Input, xfs( "Unknown profile pair '%s' / '%s'",
                                               (const char*)Source, (const char*)Target ) );
        }

        s32 Index = Data.PairIndices[SourceIndex * Data.Targets.GetCount() + TargetIndex];

        // Only damage entries define active source/target pairs.
        if( Index < 0 )
        {
            continue;
        }

        // Fill only the requested field, preserving damage and the other value.
        if( HitType )
        {
            Data.Pairs[Index].m_HitType = Type;
        }
        else
        {
            Data.Pairs[Index].m_Force = Force;
        }
    }

    return FinishPainTable( Input );
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool LoadPain( const char* pDirectory )
{
    xtimer Time;
    Time.Start();

    pain_load_data Data;
    const char* pDamageFile = "Tweak_DamageTable.tweak";

    // Multiplayer supplies its own damage table; source, force and hit-type
    // profiles are shared with the single-player configuration.
#ifndef X_EDITOR
    if( GameMgr.IsGameMultiplayer() )
    {
        pDamageFile = "Tweak_MP_DamageTable.tweak";
    }
#endif

    // Build source and target profiles, then fill force and hit type for their pairs.
    // A failure leaves these temporary arrays unpublished.
    if( !ReadPainProfiles( xfs( "%s\\Tweak_PainProfile.tweak", pDirectory ), Data ) ||
        !ReadDamageTable( xfs( "%s\\%s", pDirectory, pDamageFile ), Data ) ||
        !ReadPainValues( xfs( "%s\\Tweak_ForceTable.tweak", pDirectory ), Data, FALSE ) ||
        !ReadPainValues( xfs( "%s\\Tweak_HitTypeTable.tweak", pDirectory ), Data, TRUE ) )
    {
        return FALSE;
    }

    // Allocate final arrays of pain, health and combined profiles.
    pain_profile*        pSources = new pain_profile[Data.Sources.GetCount()];
    health_profile*      pTargets = new health_profile[Data.Targets.GetCount()];
    pain_health_profile* pPairs   = new pain_health_profile[Data.Pairs.GetCount()];

    for( s32 I = 0; I < Data.Sources.GetCount(); I++ )
    {
        pSources[I] = Data.Sources[I];
    }

    for( s32 I = 0; I < Data.Targets.GetCount(); I++ )
    {
        pTargets[I] = Data.Targets[I];
    }

    for( s32 I = 0; I < Data.Pairs.GetCount(); I++ )
    {
        pPairs[I] = Data.Pairs[I];
    }

    // Publish only after every file has been read successfully.
    // Hand off to the data vault. ..."Keep them out of trouble!"...
    g_DataVault.AddDataBlocks( "P_PROFILE", pSources, Data.Sources.GetCount(), sizeof(pain_profile) );
    g_DataVault.AddDataBlocks( "H_PROFILE", pTargets, Data.Targets.GetCount(), sizeof(health_profile) );
    g_DataVault.AddDataBlocks( "PH_PROFILE", pPairs, Data.Pairs.GetCount(), sizeof(pain_health_profile) );
    s_PainLoaded = TRUE;

    LOG_MESSAGE( "LoadPain", "Loaded %d sources, %d targets and %d pairs in %2.02f seconds.",
                 Data.Sources.GetCount(), Data.Targets.GetCount(), Data.Pairs.GetCount(), Time.ReadSec() );
    return TRUE;
}

//==============================================================================

void UnloadPain( void )
{
    if( !s_PainLoaded )
    {
        return;
    }

    // Remove data from the vault
    pain_profile*        pPP  = (pain_profile*)g_DataVault.DelDataBlocks("P_PROFILE");
    health_profile*      pHP  = (health_profile*)g_DataVault.DelDataBlocks("H_PROFILE");
    pain_health_profile* pPHP = (pain_health_profile*)g_DataVault.DelDataBlocks("PH_PROFILE");

    // Deallocate the arrays
    delete[] pPP;
    delete[] pHP;
    delete[] pPHP;
    s_PainLoaded = FALSE;
}

//==============================================================================
