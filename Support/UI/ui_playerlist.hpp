//==============================================================================
//  
//  ui_playerlist.hpp
//  
//==============================================================================

#ifndef UI_PLAYER_LIST_HPP
#define UI_PLAYER_LIST_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "UI/ui_listbox.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

extern ui_win* ui_playerlist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

//==============================================================================
//  CLASSES
//==============================================================================

class ui_playerlist : public ui_listbox
{
public:
                    ui_playerlist             ( void );
    virtual        ~ui_playerlist             ( void );

    void            RenderString            ( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString );
    void            RenderString            ( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString );
    void            RenderTitle             ( irect r, u32 Flags, const xwchar* pString );
    virtual void    RenderHeader            ( irect r );
    virtual void    RenderItem              ( irect r, const item& Item, const xcolor& c1, const xcolor& c2 );

    void            SetScoreFieldMask       ( u32 Mask )        { m_ScoreFieldMask = Mask; }
    void            SetMaxPlayerWidth       ( s32 Width )       { m_MaxPlayerWidth = Width; }

private:
    u32             m_ScoreFieldMask;
    s32             m_MaxPlayerWidth;
    s32             m_PlayerWidth;
};

//==============================================================================
#endif // UI_PLAYER_LIST_HPP
//==============================================================================
