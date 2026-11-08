//==============================================================================
//
//  TweakMgr.cpp
//
//==============================================================================

#ifdef athyssen
#define DO_LOG_TWEAKS 0
#else
#define DO_LOG_TWEAKS 0
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "TweakMgr.hpp"
#include "Parsing/TextIn.hpp"
#include "Auxiliary/MiscUtils/dictionary.hpp"

#ifndef X_EDITOR
#include "NetworkMgr/Game/GameMgr.hpp"
#endif

//==============================================================================
//  DATA_HANDLE
//==============================================================================

tweak_handle::tweak_handle( void )
{
    data_handle::SetType( DATA_TYPE_TWEAK );
}

//==============================================================================

tweak_handle::~tweak_handle( void )
{
}

//==============================================================================

tweak_handle::tweak_handle( const char* pDataDescriptorName )
{
    data_handle::SetType( DATA_TYPE_TWEAK );
    data_handle::SetName( pDataDescriptorName );
}

//==============================================================================

xbool tweak_handle::Exists( void ) const
{
    tweak_data_block* pTweak = (tweak_data_block*)g_DataVault.GetData( *this );
    return( pTweak != NULL );
}

//==============================================================================

f32 tweak_handle::GetF32( void ) const
{ 
    tweak_data_block* pTweak = (tweak_data_block*)g_DataVault.GetData( *this );
    //ASSERT( pTweak );
    f32 V = (pTweak) ? ((f32)pTweak->GetValue()) : (0);

    #ifdef DATA_VAULT_KEEP_NAMES
    CLOG_ERROR((pTweak==NULL),"MISSING TWEAK","(%s)", GetName() );
    CLOG_MESSAGE(DO_LOG_TWEAKS,"tweak_handle::GetF32","(%8.3f) (%s)", V, GetName() );
    #endif

    return V;
}

//==============================================================================

s32 tweak_handle::GetS32( void ) const
{ 
    tweak_data_block* pTweak = (tweak_data_block*)g_DataVault.GetData( *this );
    //ASSERT( pTweak );
    s32 V = (pTweak) ? ((s32)pTweak->GetValue()) : (0);

    #ifdef DATA_VAULT_KEEP_NAMES
    CLOG_ERROR((pTweak==NULL),"MISSING TWEAK","(%s)", GetName() );
    CLOG_MESSAGE(DO_LOG_TWEAKS,"tweak_handle::GetS32","(%8d) (%s)", V, GetName() );
    #endif

    return V;
}

//==============================================================================

radian tweak_handle::GetRadian( void ) const
{ 
    tweak_data_block* pTweak = (tweak_data_block*)g_DataVault.GetData( *this );
    //ASSERT( pTweak );
    radian V = (pTweak) ? ((radian)pTweak->GetValue()) : (0);

    // We're assuming this badboy was typed in degrees in the tables.
    V = DEG_TO_RAD(V);

    #ifdef DATA_VAULT_KEEP_NAMES
    CLOG_ERROR((pTweak==NULL),"MISSING TWEAK","(%s)", GetName() );
    CLOG_MESSAGE(DO_LOG_TWEAKS,"tweak_handle::GetRadian","(%8.3f) (%s)", V, GetName() );
    #endif

    return V;
}

//==============================================================================

xbool tweak_handle::GetBool( void ) const
{ 
    tweak_data_block* pTweak = (tweak_data_block*)g_DataVault.GetData( *this );
    //ASSERT( pTweak );
    xbool V = (pTweak) ? ((xbool)pTweak->GetValue()) : (0);

#ifdef DATA_VAULT_KEEP_NAMES
    CLOG_ERROR((pTweak==NULL),"MISSING TWEAK","(%s)", GetName() );
    CLOG_MESSAGE(DO_LOG_TWEAKS,"tweak_handle::GetBool","(%8d) (%s)", V, GetName() );
#endif

    return V;
}

//==============================================================================
//  DATA_HANDLE
//==============================================================================

tweak_data_block::tweak_data_block( void )
{
    SetType( DATA_TYPE_TWEAK );
}

//==============================================================================

tweak_data_block::~tweak_data_block( void )
{
}

//==============================================================================

f32 tweak_data_block::GetValue( void ) const
{
    return m_Value;
}

//==============================================================================

void tweak_data_block::SetValue( f32 Value )
{
    m_Value = Value;
}

//==============================================================================
// LOAD TWEAKS
//==============================================================================

#ifndef X_RETAIL

struct s_AudioData
{
    char   Desc[64];
    f32    fVolume;
    f32    fNearClip;
    f32    fFarClip;
};

s_AudioData *AudioTweakData = NULL;
xbool        AUDIO_TWEAK    = FALSE;

//=============================================================================
void LoadAudioTweakFile( const char* pDirectory )
{
    X_FILE* aFile;

    if( AUDIO_TWEAK )
    {
        // make sure we get rid of any data in the structure array and zero it.
        if( AudioTweakData )
        {
            x_free( AudioTweakData );
            AudioTweakData = NULL;
        }

        char tempName[128];
        x_sprintf( tempName,"%s\\Tweak_Audio.audiobin", pDirectory );
        aFile = x_fopen ( tempName , "rb" );
        
        // make sure we're good.  File does NOT have to be there.
        if( !aFile )
        {
            return;
        }

        // store of file and number of entries information
        s32 fileLength  = x_flength( aFile );
        s32 theSize     = sizeof(s_AudioData);
        s32 numEntries  = fileLength/theSize;
        s32 totalSize   = fileLength + sizeof(s_AudioData);

        // if you hit this, talk to Kevin or Rob B.
        ASSERTS( numEntries <= 1500, "Too many Audio Tweak data entries.");

        // get some memory
        AudioTweakData = (s_AudioData*)x_malloc( totalSize );
        
        // clear memory
        x_memset( AudioTweakData, 0, totalSize );

        // read data from file into struct array
        x_fread( AudioTweakData, fileLength, 1, aFile );    

        // loop through and null end of padded string (they're padded to 64 chars with spaces)
        for( s32 i = 0; i < numEntries; i++ )
        {
            if( AudioTweakData[i].Desc )
            {
                // found a space...
                char *pEOS = x_strstr(AudioTweakData[i].Desc, " ");

                // put a null in it's place
                if( pEOS )
                {
                    *pEOS = 0;
                }
            }
        }
        
        // shutdown the file
        x_fclose( aFile );
    }
}
#endif // X_RETAIL

//==============================================================================
//  STORAGE
//==============================================================================

static xbool s_TweaksLoaded = FALSE;

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static xbool ReportTweakError( text_in& Input, const char* pMessage )
{
    if( !Input.HasError() )
    {
        x_DebugMsg( "%s(%d) : error : %s\n", Input.GetFileName(), Input.GetLineNumber(), pMessage );
    }

    return FALSE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool LoadTweaks( const char* pDirectory )
{
    xtimer Time;
    Time.Start();

#ifndef X_RETAIL
    LoadAudioTweakFile( pDirectory );
#endif

    const char* pFileName = "Tweak_General.tweak";

#ifndef X_EDITOR
    if( GameMgr.IsGameMultiplayer() )
    {
        pFileName = "Tweak_MP_General.tweak";
    }
#endif

    text_in Input;

    if( !Input.OpenFile( xfs( "%s\\%s", pDirectory, pFileName ) ) )
    {
        return ReportTweakError( Input, "Unable to open tweak file" );
    }

    s32 Version = 0;

    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "TweakTable" ) ||
        Input.GetFieldCount() != 1 || !Input.ReadFields() ||
        !Input.GetS32( "Version", Version ) || Version != 1 )
    {
        return ReportTweakError( Input, "Expected TweakTable version 1" );
    }

    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "Tweaks" ) ||
        Input.GetFieldCount() != 2 || Input.GetHeaderCount() <= 0 )
    {
        return ReportTweakError( Input, "Invalid Tweaks table" );
    }

    s32 Count = Input.GetHeaderCount();
    xarray<tweak_data_block> Tweaks;
    dictionary Names;
    Tweaks.SetCapacity( Count );

    for( s32 I = 0; I < Count; I++ )
    {
        xstring          Name;
        tweak_data_block Tweak;

        if( !Input.ReadFields() || !Input.GetString( "Name", Name ) ||
            !Input.GetF32( "Value", Tweak.m_Value ) )
        {
            return ReportTweakError( Input, "Invalid tweak entry" );
        }

        if( Name.IsEmpty() )
        {
            return ReportTweakError( Input, "Empty tweak name" );
        }

        Name.MakeUpper();

        if( Names.Find( Name ) >= 0 )
        {
            return ReportTweakError( Input, xfs( "Duplicate tweak '%s'", (const char*)Name ) );
        }

        Names.Add( Name );
        Tweak.SetName( Name );
        Tweaks.Append() = Tweak;
    }

    if( Input.ReadHeader() || Input.HasError() || !Input.IsEOF() )
    {
        return ReportTweakError( Input, "Unexpected data after tweak table" );
    }

    tweak_data_block* pTweaks = new tweak_data_block[Count];

    for( s32 I = 0; I < Count; I++ )
    {
        pTweaks[I] = Tweaks[I];
    }

    g_DataVault.AddDataBlocks( "TWDATA", pTweaks, Count, sizeof(tweak_data_block) );
    s_TweaksLoaded = TRUE;

    LOG_MESSAGE( "LoadTweaks", "Loaded %d entries in %2.02f seconds.", Count, Time.ReadSec() );
    return TRUE;
}

//==============================================================================

void UnloadTweaks( void )
{
    if( !s_TweaksLoaded )
    {
        return;
    }

    tweak_data_block* pTW  = (tweak_data_block*)g_DataVault.DelDataBlocks("TWDATA");
    delete[] pTW;
    s_TweaksLoaded = FALSE;
}

//==============================================================================
// RUNTIME TWEAK OVERRIDE
//==============================================================================

#define MAX_SCRIPT_TWEAKS 128

static tweak_data_block s_ScriptTweaks[ MAX_SCRIPT_TWEAKS ];
static s32              s_nScriptTweaks = 0;

xbool SetTweakF32( const char* pName, f32 Value )
{
    tweak_handle      Handle( pName );
    tweak_data_block* pTweak = (tweak_data_block*)g_DataVault.GetData( Handle );
    if( pTweak )
    {
        pTweak->SetValue( Value );
        return TRUE;
    }

    if( s_nScriptTweaks >= MAX_SCRIPT_TWEAKS )
        return FALSE;

    tweak_data_block& Blk = s_ScriptTweaks[ s_nScriptTweaks ];
    Blk.SetName ( pName );
    Blk.SetType ( DATA_TYPE_TWEAK );
    Blk.SetValue( Value );
    s_nScriptTweaks++;

    if( s_nScriptTweaks > 1 )
        g_DataVault.DelDataBlocks( "SCRIPTTW" );
    g_DataVault.AddDataBlocks( "SCRIPTTW", s_ScriptTweaks, s_nScriptTweaks, sizeof(tweak_data_block) );

    return TRUE;
}

//==============================================================================
// TWEAK UTIL FUNCTIONS
//==============================================================================

f32 GetTweakF32( const char* pName )
{
    tweak_handle hTweak( pName );
    return hTweak.GetF32();
}

//==============================================================================

f32 GetTweakF32( const char* pName, f32 Default )
{
    tweak_handle hTweak( pName );
    if( hTweak.Exists() )
    {
        return hTweak.GetF32();
    }
    else
    {
        CLOG_MESSAGE( DO_LOG_TWEAKS, "tweak_handle::GetF32", "(%8.3f) (%s) (not present so using code specified default value)", Default, pName );
        return Default;
    }     
}

//==============================================================================

s32 GetTweakS32( const char* pName )
{
    tweak_handle hTweak( pName );
    return hTweak.GetS32();
}

//==============================================================================

s32 GetTweakS32( const char* pName, s32 Default )
{
    tweak_handle hTweak( pName );
    if( hTweak.Exists() )
    {
        return hTweak.GetS32();
    }
    else
    {
        CLOG_MESSAGE( DO_LOG_TWEAKS, "tweak_handle::GetS32", "(%8d) (%s) (not present so using code specified default value)", Default, pName );
        return Default;
    }     
}

//==============================================================================

radian GetTweakRadian( const char* pName )
{
    tweak_handle hTweak( pName );
    return hTweak.GetRadian();
}

//==============================================================================

radian GetTweakRadian( const char* pName, radian Default )
{
    tweak_handle hTweak( pName );
    if( hTweak.Exists() )
    {
        return hTweak.GetRadian();
    }
    else
    {
        CLOG_MESSAGE( DO_LOG_TWEAKS, "tweak_handle::GetRadian", "(%8.3f) (%s) (not present so using code specified default value)", RAD_TO_DEG( Default ), pName );
        return Default;
    }     
}

//==============================================================================

xbool GetTweakBool( const char* pName )
{
    tweak_handle hTweak( pName );
    return hTweak.GetBool();
}

//==============================================================================

xbool GetTweakBool( const char* pName, xbool Default )
{
    tweak_handle hTweak( pName );
    if( hTweak.Exists() )
    {
        return hTweak.GetBool();
    }
    else
    {
        CLOG_MESSAGE( DO_LOG_TWEAKS, "tweak_handle::GetBool","(%8d) (%s) (not present so using code specified default value)", Default, pName );
        return Default;
    }     
}

//==============================================================================




