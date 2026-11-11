//==============================================================================
//  
//  ui_text.hpp
//  
//==============================================================================

#ifndef UI_TEXT_HPP
#define UI_TEXT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_control.hpp"

//==============================================================================
//  ui_text
//==============================================================================

extern ui_win* ui_text_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

class ui_text : public ui_control
{
public:
                    ui_text             ( void );
    virtual        ~ui_text             ( void );

    xbool           Create              ( s32           UserID,
                                          ui_manager*   pManager,
                                          const irect&  Position,
                                          ui_win*       pParent,
                                          s32           Flags );

    virtual void    Render              ( s32 ox=0, s32 oy=0 );

    void            UseSmallText        ( xbool UseSmall ) { m_UseSmallText = UseSmall; }
    void            UseShadow           ( xbool UseShadow ) { m_UseShadow = UseShadow; }

protected:
    xbool           m_UseSmallText;
    xbool           m_UseShadow;
};

//==============================================================================
#endif // UI_TEXT_HPP
//==============================================================================
