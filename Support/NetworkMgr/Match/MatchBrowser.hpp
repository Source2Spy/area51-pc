#ifndef MATCH_BROWSER_HPP
#define MATCH_BROWSER_HPP

#include "MatchServer.hpp"
#include "x_string.hpp"

//==============================================================================

enum match_sort
{
    MATCH_SORT_NAME,
    MATCH_SORT_PLAYERS,
    MATCH_SORT_MODE,
    MATCH_SORT_MAP,
    MATCH_SORT_PING,
    MATCH_SORT_PASSWORD,
    MATCH_SORT_COUNT,
};

struct match_browser_filter
{
    xwstring Name;
    xwstring Map;
    u32      Source       = MATCH_SOURCE_ALL;
    s32      GameType     = GAME_MP;
    s32      MaxPing      = -1;
    xbool    HideFull     = FALSE;
    xbool    HideEmpty    = FALSE;
    xbool    HidePassword = FALSE;

    static xbool Contains( const xwchar* pText, const xwchar* pSearch )
    {
        if( !pSearch[0] )
            return TRUE;

        for( s32 i = 0; pText[i]; i++ )
        {
            s32 j = 0;

            while( pText[i + j] && pSearch[j] &&
                   ( x_toupper( pText[i + j] ) == x_toupper( pSearch[j] ) ) )
                j++;

            if( !pSearch[j] )
                return TRUE;
        }

        return FALSE;
    }

    xbool Matches( const server_info& Info ) const
    {
        if( ( Info.Flags & SERVER_IS_PRIVATE ) ||
            ( ( Source != MATCH_SOURCE_ALL ) && !( Info.Sources & Source ) ) ||
            ( ( GameType != GAME_MP ) && ( Info.GameTypeID != GameType ) ) ||
            ( HideFull && ( Info.Players >= Info.MaxPlayers ) ) ||
            ( HideEmpty && ( Info.Players == 0 ) ) ||
            ( HidePassword && ( Info.Flags & SERVER_HAS_PASSWORD ) ) ||
            ( ( MaxPing >= 0 ) && ( ( Info.PingDelay < 0.0f ) || ( Info.PingDelay > MaxPing ) ) ) )
            return FALSE;

        return ( Contains( Info.Name, Name ) ||
                 Contains( xwstring( Info.Remote.GetStrAddress() ), Name ) ) &&
               ( Contains( Info.MissionName, Map ) ||
                 Contains( xwstring( xfs( "%d", Info.Level ) ), Map ) );
    }

    xbool operator==( const match_browser_filter& Other ) const
    {
        return ( Name == Other.Name ) && ( Map == Other.Map ) && ( Source == Other.Source ) &&
               ( GameType == Other.GameType ) && ( MaxPing == Other.MaxPing ) &&
               ( HideFull == Other.HideFull ) && ( HideEmpty == Other.HideEmpty ) &&
               ( HidePassword == Other.HidePassword );
    }
};

inline s32 CompareMatchServers( const server_info& Left, const server_info& Right,
                                match_sort Sort, xbool Descending )
{
    s32 Result = 0;

    switch( Sort )
    {
        case MATCH_SORT_NAME: Result = x_wstricmp( Left.Name, Right.Name ); break;
        case MATCH_SORT_MODE: Result = x_wstricmp( Left.ShortGameType, Right.ShortGameType ); break;
        case MATCH_SORT_MAP: Result = x_wstricmp( Left.MissionName, Right.MissionName ); break;
        case MATCH_SORT_PLAYERS: Result = ( Left.Players > Right.Players ) - ( Left.Players < Right.Players ); break;
        case MATCH_SORT_PASSWORD: Result = !!( Left.Flags & SERVER_HAS_PASSWORD ) - !!( Right.Flags & SERVER_HAS_PASSWORD ); break;
        case MATCH_SORT_PING:
            if( ( Left.PingDelay < 0.0f ) != ( Right.PingDelay < 0.0f ) )
                return ( Left.PingDelay < 0.0f ) ? 1 : -1;

            Result = ( Left.PingDelay > Right.PingDelay ) - ( Left.PingDelay < Right.PingDelay );
            break;

        default: break;
    }

    if( Result )
        return Descending ? -Result : Result;

    Result = x_wstricmp( Left.Name, Right.Name );

    if( Result )
        return Result;

    if( Left.Remote.GetIP() != Right.Remote.GetIP() )
        return ( (u32)Left.Remote.GetIP() < (u32)Right.Remote.GetIP() ) ? -1 : 1;

    if( Left.Remote.GetPort() != Right.Remote.GetPort() )
        return ( Left.Remote.GetPort() < Right.Remote.GetPort() ) ? -1 : 1;

    return ( Left.ID > Right.ID ) - ( Left.ID < Right.ID );
}

#endif // MATCH_BROWSER_HPP
