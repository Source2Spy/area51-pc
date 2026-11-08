//=========================================================================
//
//  SaveDataMgr.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SaveDataMgr.hpp"
#include "Backend/SaveDataBackend.hpp"
#include "SaveDataCodec.hpp"
#include "StateMgr/StateMgr.hpp"
#include "x_files/x_workers.hpp"

#include <algorithm>
#include <deque>
#include <memory>
#include <vector>

//=========================================================================
//  CONSTANTS
//=========================================================================

static const char* SETTINGS_FILE_NAME             = "AREA-51-SETTINGS";
static const char* PROFILE_FILE_PREFIX            = "AREA-51-PROFILE";
static const s32   SAVE_DATA_PROFILE_CAPACITY     = 32;
static const s32   SAVE_DATA_MAX_PENDING_REQUESTS = 16;

//=========================================================================
//  DATA
//=========================================================================

struct SaveDataMgr::Impl
{
    struct Request
    {
        Request( void ) :
            Owner   ( NULL ),
            PlayerID( -1 )
        {
        }

        void*                Owner;
        CompletionCallback   Callback;
        SaveDataResult       Result;
        s32                  PlayerID;
        profile_info         ProfileInfo;
        player_profile       Profile;
        global_settings      Settings;
        xarray<profile_info> Profiles;
    };

    Impl( void ) :
        Initialized      ( FALSE ),
        Job              ( HNULL ),
        CompletionPending( FALSE )
    {
        for( s32 i = 0; i < SM_PROFILE_COUNT; i++ )
        {
            SelectedProfiles[i].bDamaged     = FALSE;
            SelectedProfiles[i].ProfileID    = -1;
            SelectedProfiles[i].Hash         = 0;
            SelectedProfiles[i].CreationDate = 0;
            SelectedProfiles[i].ModifiedDate = 0;
        }
    }

    static void ProcessRequest( void* pData );
    static void StartNext     ( Impl& State );
    static void Add           ( Impl& State, Request& Request );

    SaveDataBackend      Backend;
    xbool                Initialized;
    xhandle              Job;
    xbool                CompletionPending;
    std::deque<Request>  Requests;
    xarray<profile_info> Profiles;
    profile_info         SelectedProfiles[SM_PROFILE_COUNT];
    SaveDataResult       LastResult;
};

SaveDataMgr g_SaveDataMgr;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
xstring GetProfileFileName( s32 ProfileID )
{
    return xstring( xfs( "%s%05d", PROFILE_FILE_PREFIX, ProfileID ) );
}

//=========================================================================

static
xbool ParseProfileID( const char* pName, s32& ProfileID )
{
    const s32 PrefixLength = x_strlen( PROFILE_FILE_PREFIX );
    if( !pName || ( x_strlen( pName ) != PrefixLength + 5 ) ||
        ( x_strncmp( pName, PROFILE_FILE_PREFIX, PrefixLength ) != 0 ) )
    {
        return FALSE;
    }

    s32 Value = 0;
    for( s32 i = 0; i < 5; i++ )
    {
        const char Digit = pName[PrefixLength + i];
        if( ( Digit < '0' ) || ( Digit > '9' ) )
        {
            return FALSE;
        }
        Value = Value * 10 + Digit - '0';
    }

    if( ( Value < 0 ) || ( Value >= SAVE_DATA_PROFILE_CAPACITY ) )
    {
        return FALSE;
    }

    ProfileID = Value;
    return TRUE;
}

//=========================================================================

static
SaveDataStatus ReadProfile( SaveDataBackend& Backend,
                            const char* pName,
                            player_profile& Profile,
                            SaveDataProfileDates& Dates )
{
    xarray<u8>           Bytes;
    const SaveDataStatus Status = Backend.Read( pName, Bytes );
    if( Status != SaveDataStatus::Success )
        return Status;

    xstring Error;
    if( !SaveDataCodec::DecodeProfile( Bytes, Profile, Dates, Error ) )
    {
        LOG_WARNING( "SaveDataMgr", "Rejected profile: %s", (const char*)Error );
        return SaveDataStatus::Corrupt;
    }
    return SaveDataStatus::Success;
}

//=========================================================================

static
SaveDataStatus WriteProfile( SaveDataBackend& Backend,
                             const char* pName,
                             const player_profile& Profile,
                             const SaveDataProfileDates& Dates )
{
    xarray<u8> Bytes;
    xstring    Error;
    if( !SaveDataCodec::EncodeProfile( Profile, Dates, Bytes, Error ) )
    {
        LOG_WARNING( "SaveDataMgr", "Rejected profile: %s", (const char*)Error );
        return SaveDataStatus::IoError;
    }
    return Backend.WriteAtomic( pName, Bytes );
}

//=========================================================================

static
SaveDataStatus ScanProfiles( SaveDataBackend& Backend, xarray<profile_info>& Profiles )
{
    xarray<SaveDataFileInfo> Files;
    const SaveDataStatus        ListStatus = Backend.List( Files );
    if( ListStatus != SaveDataStatus::Success )
    {
        return ListStatus;
    }

    std::vector<profile_info> SortedProfiles;
    for( s32 i = 0; i < Files.GetCount(); i++ )
    {
        s32 ProfileID = -1;
        if( !ParseProfileID( Files[i].Name, ProfileID ) )
        {
            continue;
        }

        profile_info Info = {};
        Info.ProfileID    = ProfileID;

        std::unique_ptr<player_profile> Profile( new player_profile );
        SaveDataProfileDates         Dates;
        const SaveDataStatus            Status = ReadProfile( Backend, Files[i].Name, *Profile, Dates );

        if( Status == SaveDataStatus::Success )
        {
            Info.CreationDate = Dates.CreationDate;
            Info.ModifiedDate = Dates.ModifiedDate;
            Info.bDamaged     = FALSE;
            Info.Name         = xwstring( Profile->GetProfileName() );
            Info.Hash         = Profile->GetHash();
        }
        else
        {
            Info.bDamaged = TRUE;
            Info.Name     = xwstring( xfs( "Profile %d", ProfileID + 1 ) );
            Info.Hash     = 0;
        }
        SortedProfiles.push_back( Info );
    }

    std::sort( SortedProfiles.begin(), SortedProfiles.end(),
               []( const profile_info& A, const profile_info& B ) { return A.ProfileID < B.ProfileID; } );

    Profiles.Clear();
    Profiles.SetCapacity( SAVE_DATA_PROFILE_CAPACITY );
    for( const profile_info& Info : SortedProfiles )
    {
        Profiles.Append() = Info;
    }
    return SaveDataStatus::Success;
}

//=========================================================================

static
s32 FindFreeProfileID( const xarray<profile_info>& Profiles )
{
    xbool Used[SAVE_DATA_PROFILE_CAPACITY] = {};
    for( s32 i = 0; i < Profiles.GetCount(); i++ )
    {
        const s32 ID = Profiles[i].ProfileID;
        if( ( ID >= 0 ) && ( ID < SAVE_DATA_PROFILE_CAPACITY ) )
        {
            Used[ID] = TRUE;
        }
    }
    for( s32 ID = 0; ID < SAVE_DATA_PROFILE_CAPACITY; ID++ )
    {
        if( !Used[ID] )
        {
            return ID;
        }
    }
    return -1;
}

//=========================================================================

void SaveDataMgr::Impl::ProcessRequest( void* pData )
{
    SaveDataMgr::Impl::Request& Request = *(SaveDataMgr::Impl::Request*)pData;
    SaveDataBackend             Backend;
    Request.Result.Status = SaveDataStatus::IoError;

    if( Backend.Init() != SaveDataStatus::Success )
    {
        return;
    }

    switch( Request.Result.Operation )
    {
        case SaveDataOperation::RefreshProfiles:
        {
            Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
        }
        break;

        case SaveDataOperation::CreateProfile:
        {
            Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
            if( Request.Result.Status == SaveDataStatus::Success )
            {
                const s32 ProfileID = FindFreeProfileID( Request.Profiles );
                if( ProfileID < 0 )
                {
                    Request.Result.Status = SaveDataStatus::NoSpace;
                    break;
                }

                Request.Result.ProfileID            = ProfileID;
                Request.ProfileInfo.ProfileID       = ProfileID;
                const u64                     Now   = eng_GetDate();
                const SaveDataProfileDates Dates = { Now, Now };
                Request.Result.Status =
                    WriteProfile( Backend, GetProfileFileName( ProfileID ), Request.Profile, Dates );
                if( Request.Result.Status == SaveDataStatus::Success )
                {
                    Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
                }
            }
        }
        break;

        case SaveDataOperation::LoadProfile:
        {
            SaveDataProfileDates Dates;
            Request.Result.Status =
                ReadProfile( Backend, GetProfileFileName( Request.ProfileInfo.ProfileID ), Request.Profile, Dates );
            if( Request.Result.Status == SaveDataStatus::Success )
            {
                Request.ProfileInfo.CreationDate = Dates.CreationDate;
                Request.ProfileInfo.ModifiedDate = Dates.ModifiedDate;
            }
        }
        break;

        case SaveDataOperation::SaveProfile:
        {
            std::unique_ptr<player_profile> Previous( new player_profile );
            SaveDataProfileDates         Dates;
            const xstring                   Name = GetProfileFileName( Request.ProfileInfo.ProfileID );
            Request.Result.Status                = ReadProfile( Backend, Name, *Previous, Dates );
            if( Request.Result.Status != SaveDataStatus::Success )
                break;

            Dates.ModifiedDate    = eng_GetDate();
            Request.Result.Status = WriteProfile( Backend, Name, Request.Profile, Dates );
            if( Request.Result.Status == SaveDataStatus::Success )
            {
                Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
            }
        }
        break;

        case SaveDataOperation::DeleteProfile:
        {
            Request.Result.Status = Backend.Delete( GetProfileFileName( Request.ProfileInfo.ProfileID ) );
            if( Request.Result.Status == SaveDataStatus::Success )
            {
                Request.Result.Status = ScanProfiles( Backend, Request.Profiles );
            }
        }
        break;

        case SaveDataOperation::SaveSettings:
        {
            xarray<u8> Bytes;
            xstring    Error;
            if( !SaveDataCodec::EncodeSettings( Request.Settings, Bytes, Error ) )
            {
                Request.Result.Status = SaveDataStatus::IoError;
                break;
            }
            Request.Result.Status = Backend.WriteAtomic( SETTINGS_FILE_NAME, Bytes );
        }
        break;

        default:
        {
            Request.Result.Status = SaveDataStatus::IoError;
        }
        break;
    }
}

//=========================================================================

void SaveDataMgr::Impl::StartNext( Impl& State )
{
    if( State.Job.IsNonNull() || State.Requests.empty() )
    {
        return;
    }

    SaveDataMgr::Impl::Request& Request = State.Requests.front();
    if( !x_WorkerJobSubmit( ProcessRequest, &Request, "SaveData", State.Job ) )
    {
        Request.Result.Status   = SaveDataStatus::Busy;
        State.CompletionPending = TRUE;
    }
}

//=========================================================================

void SaveDataMgr::Impl::Add( Impl& State, Request& Request )
{
    if( (s32)State.Requests.size() >= SAVE_DATA_MAX_PENDING_REQUESTS )
    {
        State.LastResult.Operation = Request.Result.Operation;
        State.LastResult.Status    = SaveDataStatus::Busy;
        State.LastResult.ProfileID = Request.Result.ProfileID;
        if( Request.Callback )
        {
            Request.Callback( State.LastResult );
        }
        return;
    }

    State.Requests.push_back( Request );
    StartNext( State );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

SaveDataMgr::SaveDataMgr( void ) :
    m_pImpl( NULL )
{
}

//=========================================================================

SaveDataMgr::~SaveDataMgr( void )
{
    Kill();
    delete m_pImpl;
    m_pImpl = NULL;
}

//=========================================================================

void SaveDataMgr::Init( void )
{
    if( !m_pImpl )
    {
        m_pImpl = new Impl;
    }
    if( m_pImpl->Initialized )
    {
        return;
    }
    m_pImpl->Initialized = ( m_pImpl->Backend.Init() == SaveDataStatus::Success );
    m_pImpl->Job         = xhandle( HNULL );
}

//=========================================================================

void SaveDataMgr::Kill( void )
{
    if( !m_pImpl )
    {
        return;
    }
    if( m_pImpl->Job.IsNonNull() )
    {
        x_WorkerJobWait( m_pImpl->Job );
        x_WorkerJobRelease( m_pImpl->Job );
        m_pImpl->Job = xhandle( HNULL );
    }
    m_pImpl->Requests.clear();
    m_pImpl->CompletionPending = FALSE;
    m_pImpl->Backend.Kill();
    m_pImpl->Initialized = FALSE;
}

//=========================================================================

void SaveDataMgr::Update( f32 DeltaTime )
{
    (void)DeltaTime;
    ASSERT( m_pImpl && m_pImpl->Initialized );
    Impl::StartNext( *m_pImpl );
    if( m_pImpl->Job.IsNonNull() )
    {
        if( !x_WorkerJobIsDone( m_pImpl->Job ) )
        {
            return;
        }

        x_WorkerJobWait( m_pImpl->Job );
        x_WorkerJobRelease( m_pImpl->Job );
        m_pImpl->Job               = xhandle( HNULL );
        m_pImpl->CompletionPending = TRUE;
    }

    if( !m_pImpl->CompletionPending )
    {
        return;
    }
    m_pImpl->CompletionPending = FALSE;

    Impl::Request& Request = m_pImpl->Requests.front();
    m_pImpl->LastResult    = Request.Result;

    if( Request.Result.Status == SaveDataStatus::Success )
    {
        switch( Request.Result.Operation )
        {
            case SaveDataOperation::RefreshProfiles:
            case SaveDataOperation::CreateProfile:
            case SaveDataOperation::SaveProfile:
            case SaveDataOperation::DeleteProfile:
            {
                m_pImpl->Profiles = Request.Profiles;
            }
            break;

            default:
            {
            }
            break;
        }

        if( ( Request.Result.Operation == SaveDataOperation::LoadProfile ) && ( Request.PlayerID >= 0 ) &&
            ( Request.PlayerID < SM_PROFILE_COUNT ) )
        {
            g_StateMgr.GetPendingProfile()              = Request.Profile;
            m_pImpl->SelectedProfiles[Request.PlayerID] = Request.ProfileInfo;
        }
        else if( ( Request.Result.Operation == SaveDataOperation::CreateProfile ) && ( Request.PlayerID >= 0 ) &&
                 ( Request.PlayerID < SM_PROFILE_COUNT ) )
        {
            for( s32 i = 0; i < m_pImpl->Profiles.GetCount(); i++ )
            {
                if( m_pImpl->Profiles[i].ProfileID == Request.Result.ProfileID )
                {
                    m_pImpl->SelectedProfiles[Request.PlayerID] = m_pImpl->Profiles[i];
                    break;
                }
            }
        }
        else if( ( Request.Result.Operation == SaveDataOperation::SaveProfile ) && ( Request.PlayerID >= 0 ) &&
                 ( Request.PlayerID < SM_PROFILE_COUNT ) )
        {
            for( s32 i = 0; i < m_pImpl->Profiles.GetCount(); i++ )
            {
                if( m_pImpl->Profiles[i].ProfileID == Request.ProfileInfo.ProfileID )
                {
                    m_pImpl->SelectedProfiles[Request.PlayerID] = m_pImpl->Profiles[i];
                    break;
                }
            }
            player_profile& ActiveProfile = g_StateMgr.GetActiveProfile( Request.PlayerID );
            if( x_memcmp(&ActiveProfile, &Request.Profile, sizeof(ActiveProfile)) == 0 )
                ActiveProfile.Checksum();
        }
        else if( Request.Result.Operation == SaveDataOperation::DeleteProfile )
        {
            for( s32 i = 0; i < SM_PROFILE_COUNT; i++ )
            {
                if( m_pImpl->SelectedProfiles[i].ProfileID == Request.ProfileInfo.ProfileID )
                {
                    m_pImpl->SelectedProfiles[i].bDamaged     = FALSE;
                    m_pImpl->SelectedProfiles[i].ProfileID    = -1;
                    m_pImpl->SelectedProfiles[i].Hash         = 0;
                    m_pImpl->SelectedProfiles[i].CreationDate = 0;
                    m_pImpl->SelectedProfiles[i].ModifiedDate = 0;
                    m_pImpl->SelectedProfiles[i].Name.Clear();
                }
            }
        }
        else if( Request.Result.Operation == SaveDataOperation::SaveSettings )
        {
            g_StateMgr.GetPendingSettings().Checksum();
            g_StateMgr.GetActiveSettings().Checksum();
        }
    }

    CompletionCallback    Callback = Request.Callback;
    const SaveDataResult Result   = Request.Result;
    m_pImpl->Requests.pop_front();
    Impl::StartNext( *m_pImpl );
    if( Callback )
    {
        Callback( Result );
    }
}

//=========================================================================

xbool SaveDataMgr::LoadStartupSettings( void )
{
    ASSERT( m_pImpl && m_pImpl->Initialized );
    xarray<u8>           Bytes;
    const SaveDataStatus ReadStatus = m_pImpl->Backend.Read( SETTINGS_FILE_NAME, Bytes );
    if( ReadStatus != SaveDataStatus::Success )
    {
        return FALSE;
    }

    global_settings Settings;
    xstring         Error;
    if( !SaveDataCodec::DecodeSettings( Bytes, Settings, Error ) )
    {
        LOG_WARNING( "SaveDataMgr::LoadStartupSettings", "Rejected settings: %s", (const char*)Error );
        return FALSE;
    }

    g_StateMgr.GetActiveSettings() = Settings;
    return TRUE;
}

//=========================================================================

void SaveDataMgr::RefreshProfiles( void )
{
    QueueRefreshProfiles( NULL, CompletionCallback() );
}

//=========================================================================

void SaveDataMgr::CancelCallbacks( void* pOwner )
{
    for( Impl::Request& Request : m_pImpl->Requests )
    {
        if( Request.Owner == pOwner )
        {
            Request.Callback = CompletionCallback();
            Request.Owner    = NULL;
        }
    }
}

//=========================================================================

void SaveDataMgr::ClearCachedProfiles( void )
{
    m_pImpl->Profiles.Clear();
}

//=========================================================================

xbool SaveDataMgr::IsBusy( void ) const
{
    return m_pImpl && ( !m_pImpl->Requests.empty() || m_pImpl->Job.IsNonNull() );
}

//=========================================================================

const SaveDataResult& SaveDataMgr::GetLastResult( void ) const
{
    return m_pImpl->LastResult;
}

//=========================================================================

profile_info& SaveDataMgr::GetProfileInfo( s32 PlayerID )
{
    ASSERT( ( PlayerID >= 0 ) && ( PlayerID < SM_PROFILE_COUNT ) );
    return m_pImpl->SelectedProfiles[PlayerID];
}

//=========================================================================

void SaveDataMgr::GetProfileNames( xarray<profile_info*>& Result )
{
    Result.SetCount( 0 );
    for( s32 i = 0; i < m_pImpl->Profiles.GetCount(); i++ )
    {
        Result.Append( &m_pImpl->Profiles[i] );
    }
}

//=========================================================================

void SaveDataMgr::QueueRefreshProfiles( void* pOwner, CompletionCallback Callback )
{
    Impl::Request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.Result.Operation = SaveDataOperation::RefreshProfiles;
    Impl::Add( *m_pImpl, Request );
}

//=========================================================================

void SaveDataMgr::QueueCreateProfile( s32 PlayerID, void* pOwner, CompletionCallback Callback )
{
    ASSERT( ( PlayerID >= 0 ) && ( PlayerID < SM_PROFILE_COUNT ) );
    Impl::Request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.PlayerID         = PlayerID;
    Request.Profile          = g_StateMgr.GetPendingProfile();
    Request.Result.Operation = SaveDataOperation::CreateProfile;
    Impl::Add( *m_pImpl, Request );
}

//=========================================================================

void SaveDataMgr::QueueProfileOperation( SaveDataOperation Operation, const profile_info& Info, s32 PlayerID,
                                           void* pOwner, CompletionCallback Callback )
{
    Impl::Request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.PlayerID         = PlayerID;
    Request.ProfileInfo      = Info;
    Request.Result.Operation = Operation;
    Request.Result.ProfileID = Info.ProfileID;
    if( Operation == SaveDataOperation::SaveProfile )
    {
        Request.Profile = g_StateMgr.GetPendingProfile();
    }
    Impl::Add( *m_pImpl, Request );
}

//=========================================================================

void SaveDataMgr::QueueSaveSettings( void* pOwner, CompletionCallback Callback )
{
    Impl::Request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.Settings         = g_StateMgr.GetPendingSettings();
    Request.Result.Operation = SaveDataOperation::SaveSettings;
    Impl::Add( *m_pImpl, Request );
}

//=========================================================================

void SaveDataMgr::QueueProfileSnapshot( const profile_info& Info, s32 PlayerID, const player_profile& Profile,
                                         void* pOwner, CompletionCallback Callback )
{
    ASSERT( (PlayerID >= 0) && (PlayerID < SM_PROFILE_COUNT) );
    Impl::Request Request;
    Request.Owner            = pOwner;
    Request.Callback         = Callback;
    Request.PlayerID         = PlayerID;
    Request.ProfileInfo      = Info;
    Request.Profile          = Profile;
    Request.Result.Operation = SaveDataOperation::SaveProfile;
    Request.Result.ProfileID = Info.ProfileID;
    Impl::Add( *m_pImpl, Request );
}
