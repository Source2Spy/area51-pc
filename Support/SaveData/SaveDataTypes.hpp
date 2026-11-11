//=========================================================================
//
//  SaveDataTypes.hpp
//
//=========================================================================

#ifndef SAVE_DATA_TYPES_HPP
#define SAVE_DATA_TYPES_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
//  DATA
//=========================================================================

// UTC FILETIME timestamps; both dates are required in every profile record.
struct SaveDataProfileDates
{
    u64 CreationDate = 0;
    u64 ModifiedDate = 0;
};

//-------------------------------------------------------------------------

enum class SaveDataOperation
{
    None,
    RefreshProfiles,
    CreateProfile,
    LoadProfile,
    SaveProfile,
    DeleteProfile,
    SaveSettings,
};

//-------------------------------------------------------------------------

enum class SaveDataStatus
{
    Success,
    NotFound,
    Corrupt,
    NoSpace,
    AccessDenied,
    IoError,
    Busy,
};

//-------------------------------------------------------------------------

struct SaveDataResult
{
    SaveDataResult( void ) :
        Operation ( SaveDataOperation::None ),
        Status    ( SaveDataStatus::Success ),
        ProfileID( -1 )
    {
    }

    xbool Succeeded( void ) const
    {
        return Status == SaveDataStatus::Success;
    }

    SaveDataOperation Operation;
    SaveDataStatus    Status;
    s32               ProfileID;
};

//=========================================================================
#endif // SAVE_DATA_TYPES_HPP
//=========================================================================
