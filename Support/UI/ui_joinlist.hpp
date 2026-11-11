//==============================================================================
//  
//  ui_joinlist.hpp
//  
//==============================================================================

#ifndef UI_JOINLIST_HPP
#define UI_JOINLIST_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "UI/ui_listbox.hpp"
#include "NetworkMgr/Match/MatchServer.hpp"
#include "NetworkMgr/Match/MatchBrowser.hpp"

//==============================================================================
//  defines
//==============================================================================


//==============================================================================
//  ui_joinlist
//==============================================================================

extern ui_win* ui_joinlist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

class ui_joinlist : public ui_listbox
{
public:
    virtual irect   GetListRect             ( void ) const;
    virtual void    Render                  ( s32 ox=0, s32 oy=0 );
    virtual void    OnPointerDown           ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnNavigate              ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void    OnFocusLost             ( ui_win* pWin );
    void            SetSort                 ( match_sort Sort, xbool Descending );
    match_sort      GetSort                 ( void ) const { return m_Sort; }
    xbool           IsDescending            ( void ) const { return m_Descending; }
    void            RenderString            ( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString );
    void            RenderString            ( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString );
    void            RenderTitle             ( irect r, u32 Flags, const xwchar* pString );
    virtual void    RenderHeader            ( irect r );
    virtual void    RenderItem              ( irect r, const item& Item, const xcolor& c1, const xcolor& c2 );
    void            SetServers              ( const xarray<server_info>& Servers ) { m_Servers = Servers; }
    const server_info* GetServerInfo         ( s32 Index ) const;

private:
    xarray<server_info> m_Servers;
    match_sort m_Sort = MATCH_SORT_NAME;
    xbool m_Descending = FALSE;
    s32 m_LastClickID = -1;
    net_address m_LastClickAddress;
};

//==============================================================================
#endif // UI_JOINLIST_HPP
//==============================================================================
