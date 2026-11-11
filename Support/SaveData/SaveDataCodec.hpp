//=========================================================================
//
//  SaveDataCodec.hpp
//
//=========================================================================

#ifndef SAVE_DATA_CODEC_HPP
#define SAVE_DATA_CODEC_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "SaveDataTypes.hpp"
#include "StateMgr/GlobalSettings.hpp"
#include "StateMgr/PlayerProfile.hpp"

//=========================================================================
//  SAVE DATA CODEC
//=========================================================================

class SaveDataCodec
{
public:

    static xbool EncodeProfile ( const player_profile& Profile,
                                 const SaveDataProfileDates& Dates,
                                 xarray<u8>& Bytes,
                                 xstring& Error );
    static xbool DecodeProfile ( const xarray<u8>& Bytes,
                                 player_profile& Profile,
                                 SaveDataProfileDates& Dates,
                                 xstring& Error );
    static xbool EncodeSettings( const global_settings& Settings,
                                 xarray<u8>& Bytes,
                                 xstring& Error );
    static xbool DecodeSettings( const xarray<u8>& Bytes,
                                 global_settings& Settings,
                                 xstring& Error );

private:

    struct ProfileSaveRecord;
    struct SettingsSaveRecord;

    template<class SERIALIZER>
    static void SerializeProfile ( SERIALIZER& Serializer, player_profile& Profile );
    template<class SERIALIZER>
    static void SerializeSettings( SERIALIZER& Serializer, global_settings& Settings );

    static xbool ProfileFieldsAreValid ( const player_profile& Profile );
    static xbool SettingsFieldsAreValid( const global_settings& Settings );
};

//=========================================================================
#endif // SAVE_DATA_CODEC_HPP
//=========================================================================
