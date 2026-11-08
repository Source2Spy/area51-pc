//==============================================================================
//
//  ui_scrollbar.hpp
//
//==============================================================================

#ifndef UI_SCROLLBAR_HPP
#define UI_SCROLLBAR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "x_math.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_manager;

//==============================================================================
//  ui_scrollbar
//==============================================================================

class ui_scrollbar
{
public:
                        ui_scrollbar       ( void );

    xbool               Create             ( ui_manager* pManager );
    void                SetBounds          ( irect const& Bounds );
    void                SetRange           ( s32 ItemCount, s32 PageSize, s32 Position );
    void                Render             ( xbool IsEnabled ) const;

    xbool               OnPointerMove      ( s32 X, s32 Y );
    void                OnPointerLeave     ( void );
    xbool               OnPointerDown      ( s32 X, s32 Y );
    xbool               OnPointerUp        ( void );
    xbool               OnWheel            ( s32 Delta, s32 LinesPerNotch = 1 );
    xbool               OnUpdate           ( f32 DeltaTime );
    void                CancelInteraction  ( void );

    xbool               Contains           ( s32 X, s32 Y ) const;
    xbool               IsHovered          ( void ) const;
    xbool               IsInteracting      ( void ) const;
    s32                 GetPosition        ( void ) const;

private:
    enum part
    {
        PART_NONE = 0,
        PART_DECREMENT,
        PART_INCREMENT,
        PART_PAGE_DECREMENT,
        PART_PAGE_INCREMENT,
        PART_THUMB,
    };

    void                UpdateGeometry     ( void );
    part                GetPartAt          ( s32 X, s32 Y ) const;
    s32                 GetPartVisualState ( part Part, xbool IsEnabled ) const;
    xbool               SetPosition        ( s32 Position );
    xbool               ApplyPressedPart   ( void );
    xbool               DragThumb          ( s32 Y );

    //-------------------------------------------------------------------------

    ui_manager*         m_pManager;
    s32                 m_iElementArrowDown;
    s32                 m_iElementArrowUp;
    s32                 m_iElementContainer;
    s32                 m_iElementThumb;
    irect               m_Bounds;
    irect               m_DecrementBounds;
    irect               m_IncrementBounds;
    irect               m_TrackBounds;
    irect               m_ThumbBounds;
    s32                 m_ItemCount;
    s32                 m_PageSize;
    s32                 m_Position;
    s32                 m_MaxPosition;
    s32                 m_PointerX;
    s32                 m_PointerY;
    s32                 m_DragOffset;
    s32                 m_WheelRemainder;
    f32                 m_RepeatTimer;
    part                m_PressedPart;
    part                m_HoveredPart;
};

//==============================================================================
#endif // UI_SCROLLBAR_HPP
//==============================================================================
