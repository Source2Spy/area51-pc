//==============================================================================
//  
//  ui_maplist.hpp
//  
//==============================================================================

#ifndef UI_MAP_LIST_HPP
#define UI_MAP_LIST_HPP

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

extern ui_win* ui_maplist_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

//==============================================================================
//  CLASSES
//==============================================================================

class ui_maplist : public ui_listbox
{
public:
                    ui_maplist             ( void );
    virtual        ~ui_maplist             ( void );

    void            RenderString            ( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const char* pString );
    void            RenderString            ( irect r, u32 Flags, const xcolor& c1, const xcolor& c2, const xwchar* pString );
    virtual void    RenderItem              ( irect r, const item& Item, const xcolor& c1, const xcolor& c2 );
};

//==============================================================================
#endif // UI_MAP_LIST_HPP
//==============================================================================
