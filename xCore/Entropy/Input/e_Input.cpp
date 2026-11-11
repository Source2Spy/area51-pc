//==============================================================================
//  
//  e_Input.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../e_Input.hpp"
#include "Input/backend/input_backend.hpp"
#include "x_plus.hpp"

//==============================================================================
//  LOCAL TABLE TYPES
//==============================================================================

struct input_gadget_lookup
{
    input_gadget    Gadget;
    const char*     pName;
};

//------------------------------------------------------------------------------

struct input_gadget_range
{
    input_gadget        First;
    input_gadget        Last;
    input_gadget_info   Info;
};

//==============================================================================
//  GADGET NAME TABLE
//==============================================================================

#define BEGIN_GADGETS                               static const input_gadget_lookup s_GadgetLookupTable[] = {
#define DEFINE_GADGET(__gadget__)                   { __gadget__, #__gadget__ },
#define DEFINE_GADGET_VALUE(__gadget__, __value__)  { __gadget__, #__gadget__ },
#define DEFINE_GADGET_RANGE(__first__, __last__, __platform__, __device__, __value_kind__, __control_kind__, __flags__)
#define END_GADGETS                                 };

#include "e_Input_Gadget_Defines.hpp"

//==============================================================================
//  GADGET METADATA TABLE
//==============================================================================

#define BEGIN_GADGETS
#define DEFINE_GADGET(__gadget__)
#define DEFINE_GADGET_VALUE(__gadget__, __value__)
#define DEFINE_GADGET_RANGE(__first__, __last__, __platform__, __device__, __value_kind__, __control_kind__, __flags__) \
    { __first__, __last__, { __platform__, __device__, __value_kind__, __control_kind__, __flags__ } },
#define END_GADGETS

static
const input_gadget_range s_GadgetInfoRanges[] =
{
    #include "e_Input_Gadget_Defines.hpp"
};

//==============================================================================
//  CONSTANTS
//==============================================================================

static
const input_gadget_info s_InvalidGadgetInfo =
{
    INPUT_PLATFORM_NONE,
    INPUT_DEVICE_NONE,
    INPUT_VALUE_NONE,
    INPUT_CONTROL_NONE,
    INPUT_GADGET_FLAG_NONE
};

static const f32 s_InputActivityThreshold = 0.25f;

//------------------------------------------------------------------------------

static
s32 GetMouseLookAxis( input_event const& Event )
{
    if( Event.Type != INPUT_EVENT_RELATIVE )
    {
        return -1;
    }

    if( Event.GadgetID == INPUT_MOUSE_X_REL )
    {
        return 0;
    }

    if( Event.GadgetID == INPUT_MOUSE_Y_REL )
    {
        return 1;
    }

    return -1;
}

//------------------------------------------------------------------------------

static
void AddMouseDebugEvent( input_event const& Event, u32& Count, s32& DeltaX, s32& DeltaY )
{
    s32 const Axis = GetMouseLookAxis( Event );
    if( Axis < 0 )
    {
        return;
    }

    Count++;
    if( Axis == 0 )
    {
        DeltaX += static_cast<s32>( Event.Value );
    }
    else
    {
        DeltaY += static_cast<s32>( Event.Value );
    }
}

//------------------------------------------------------------------------------

enum input_activity_priority
{
    INPUT_ACTIVITY_NONE      = 0,
    INPUT_ACTIVITY_AXIS      = 1,
    INPUT_ACTIVITY_MOMENTARY = 2
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

input_system g_Input;

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
xbool IsGadgetInRange( input_gadget GadgetID, input_gadget First, input_gadget Last )
{
    return( (GadgetID >= First) && (GadgetID <= Last) );
}

//==============================================================================

static
s32 GetSampleActivityPriority( const input_gadget_info& Info, f32 Value, xbool WasPressed )
{
    switch( Info.ValueKind )
    {
        case INPUT_VALUE_DIGITAL:
        {
            return WasPressed ? INPUT_ACTIVITY_MOMENTARY : INPUT_ACTIVITY_NONE;
        }
        break;

        case INPUT_VALUE_RELATIVE_AXIS:
        {
            // Mouse motion remains available to mixed-input camera code, but it
            // must not churn UI prompts between keyboard/mouse and gamepad.
            return( Info.Device == INPUT_DEVICE_MOUSE )
                 ? INPUT_ACTIVITY_NONE
                 : ((Value != 0.0f) ? INPUT_ACTIVITY_MOMENTARY : INPUT_ACTIVITY_NONE);
        }
        break;

        case INPUT_VALUE_PULSE:
        {
            return( Value != 0.0f ) ? INPUT_ACTIVITY_MOMENTARY : INPUT_ACTIVITY_NONE;
        }
        break;

        case INPUT_VALUE_ABSOLUTE_AXIS:
        {
            return( x_abs( Value ) > s_InputActivityThreshold ) ? INPUT_ACTIVITY_AXIS : INPUT_ACTIVITY_NONE;
        }
        break;

        default:
        {
            return INPUT_ACTIVITY_NONE;
        }
        break;
    }
}

//==============================================================================
//  INPUT EVENT BUFFER
//==============================================================================

input_event_buffer::input_event_buffer( void )
{
    m_Count           = 0;
    m_StartTimeStamp  = 0;
    m_EndTimeStamp    = 0;
    m_NextSequence    = 0;
    m_HasOverflowed   = FALSE;
}

//==============================================================================

void input_event_buffer::BeginCapture( u32 StartTimeStamp )
{
    m_Count          = 0;
    m_StartTimeStamp = StartTimeStamp;
    m_EndTimeStamp   = StartTimeStamp;
    m_HasOverflowed  = FALSE;
}

//==============================================================================

xbool input_event_buffer::Append( input_gadget     GadgetID,
                                  s32              DeviceID,
                                  input_event_type Type,
                                  f32              Value,
                                  u32              TimeStamp )
{
    if( m_Count >= INPUT_EVENT_CAPACITY )
    {
        m_HasOverflowed = TRUE;
        return FALSE;
    }

    input_event& Event  = m_Events[m_Count++];
    Event.GadgetID      = GadgetID;
    Event.DeviceID      = DeviceID;
    Event.Type          = Type;
    Event.Value         = Value;
    Event.TimeStamp     = TimeStamp;
    Event.Sequence      = m_NextSequence++;
    return TRUE;
}

//==============================================================================

void input_event_buffer::MarkOverflow( void )
{
    m_HasOverflowed = TRUE;
}

//==============================================================================

void input_event_buffer::EndCapture( u32 EndTimeStamp )
{
    m_EndTimeStamp = EndTimeStamp;

}

//==============================================================================

void input_event_buffer::Sort( void )
{
    u32 const StartTimeStamp = m_StartTimeStamp;
    x_qsort( m_Events, m_Count,
             [StartTimeStamp]( input_event const& A, input_event const& B ) -> s32
             {
                 s32 const ATicks = static_cast<s32>( A.TimeStamp - StartTimeStamp );
                 s32 const BTicks = static_cast<s32>( B.TimeStamp - StartTimeStamp );

                 if( ATicks != BTicks )
                 {
                     return ATicks < BTicks ? -1 : 1;
                 }

                 return A.Sequence < B.Sequence ? -1 : ( A.Sequence > B.Sequence ? 1 : 0 );
             } );
}

//==============================================================================

s32 input_event_buffer::GetCount( void ) const
{
    return m_Count;
}

//==============================================================================

input_event& input_event_buffer::GetEvent( s32 Index )
{
    ASSERT( (Index >= 0) && (Index < m_Count) );
    return m_Events[Index];
}

//==============================================================================

input_event const& input_event_buffer::GetEvent( s32 Index ) const
{
    ASSERT( (Index >= 0) && (Index < m_Count) );
    return m_Events[Index];
}

//==============================================================================

u32 input_event_buffer::GetStartTimeStamp( void ) const
{
    return m_StartTimeStamp;
}

//==============================================================================

u32 input_event_buffer::GetEndTimeStamp( void ) const
{
    return m_EndTimeStamp;
}

//==============================================================================

xbool input_event_buffer::HasOverflowed( void ) const
{
    return m_HasOverflowed;
}

//==============================================================================
//  INPUT SNAPSHOT
//==============================================================================

void input_snapshot::Clear( void )
{
    x_memset( m_Gadgets, 0, sizeof( m_Gadgets ) );
}

//==============================================================================

input_gadget_sample const& input_snapshot::GetSample( input_gadget GadgetID, s32 DeviceID ) const
{
    static input_gadget_sample const s_EmptySample = { 0.0f, FALSE, FALSE, FALSE, FALSE };

    if( (GadgetID < 0) || (GadgetID >= INPUT_GADGET_COUNT) ||
        (DeviceID < 0) || (DeviceID >= INPUT_MAX_DEVICES) )
    {
        return s_EmptySample;
    }

    return m_Gadgets[GadgetID][DeviceID];
}

//==============================================================================

xbool input_snapshot::IsPressed( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetSample( GadgetID, DeviceID ).IsDown;
}

//==============================================================================

xbool input_snapshot::WasPressed( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetSample( GadgetID, DeviceID ).WasPressed;
}

//==============================================================================

xbool input_snapshot::WasReleased( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetSample( GadgetID, DeviceID ).WasReleased;
}

//==============================================================================

f32 input_snapshot::GetValue( input_gadget GadgetID, s32 DeviceID ) const
{
    return GetSample( GadgetID, DeviceID ).Value;
}

//==============================================================================
//  INPUT SNAPSHOT BUILDER
//==============================================================================

input_snapshot_builder::input_snapshot_builder( void )
{
    Clear();
}

//==============================================================================

void input_snapshot_builder::Clear( void )
{
    m_Snapshot.Clear();
}

//==============================================================================

void input_snapshot_builder::SetSample( input_gadget GadgetID,
                                      s32 DeviceID,
                                      const input_gadget_sample& Sample )
{
    ASSERT( (GadgetID >= 0) && (GadgetID < INPUT_GADGET_COUNT) );
    ASSERT( (DeviceID >= 0) && (DeviceID < INPUT_MAX_DEVICES) );
    if( (GadgetID < 0) || (GadgetID >= INPUT_GADGET_COUNT) ||
        (DeviceID < 0) || (DeviceID >= INPUT_MAX_DEVICES) )
    {
        return;
    }

    m_Snapshot.m_Gadgets[GadgetID][DeviceID] = Sample;
}

//==============================================================================

input_snapshot const& input_snapshot_builder::GetSnapshot( void ) const
{
    return m_Snapshot;
}

//==============================================================================

xbool input_snapshot::IsPresent( input_gadget GadgetID, s32 DeviceID ) const
{
    if( DeviceID == INPUT_DEVICE_ID_ANY )
    {
        for( s32 i = 0; i < INPUT_MAX_DEVICES; i++ )
        {
            if( GetSample( GadgetID, i ).IsPresent )
            {
                return TRUE;
            }
        }

        return FALSE;
    }

    return GetSample( GadgetID, DeviceID ).IsPresent;
}

//==============================================================================
//  INPUT SYSTEM LIFETIME
//==============================================================================

input_system::input_system( void )
{
    m_CurrentDevice      = INPUT_DEVICE_KEYBOARD;
    m_CurrentPlatform    = INPUT_PLATFORM_DESKTOP;
    m_InitDesc.pWindow = NULL;
    m_pBackend          = NULL;
    m_FrameSnapshot.Clear();
    m_DebugStats = input_debug_stats();
    ClearFrameActivity();
}

//==============================================================================

xbool input_system::Init( const input_init_desc& Desc )
{
    if( Desc.pWindow )
    {
        m_InitDesc.pWindow = Desc.pWindow;
    }

    if( !m_pBackend )
    {
        m_pBackend = input_CreateDefaultBackend();
    }

    if( !m_pBackend )
    {
        return FALSE;
    }

    return m_pBackend->Init( m_InitDesc );
}

//==============================================================================

void input_system::Init( void )
{
    (void)Init( m_InitDesc );
}

//==============================================================================

void input_system::Kill( void )
{
    if( m_pBackend )
    {
        m_pBackend->Kill();
        input_DestroyDefaultBackend( m_pBackend );
        m_pBackend = NULL;
    }
}

//==============================================================================
//  PLATFORM BACKEND
//==============================================================================

xbool input_system::IsRawGadgetDown( input_gadget GadgetID, s32 DeviceID ) const
{
    return m_pBackend ? m_pBackend->IsGadgetDown( GadgetID, DeviceID ) : FALSE;
}

//==============================================================================

f32 input_system::GetRawGadgetValue( input_gadget GadgetID, s32 DeviceID ) const
{
    return m_pBackend ? m_pBackend->GetGadgetValue( GadgetID, DeviceID ) : 0.0f;
}

//==============================================================================

xbool input_system::IsRawGadgetPresent( input_gadget GadgetID, s32 DeviceID ) const
{
    return m_pBackend ? m_pBackend->IsGadgetPresent( GadgetID, DeviceID ) : FALSE;
}

//==============================================================================

xbool input_system::IsDevicePresent( input_device Device, s32 DeviceID ) const
{
    return m_pBackend ? m_pBackend->IsDevicePresent( Device, DeviceID ) : FALSE;
}

//==============================================================================

xbool input_system::FindAvailableDevice( input_device& Device, s32& DeviceID ) const
{
    Device   = INPUT_DEVICE_NONE;
    DeviceID = -1;

    if( !m_pBackend )
    {
        return FALSE;
    }

    // Device classes are data-driven by the input_device enum.  Adding a new
    // backend/device type automatically makes it eligible for fallback.
    for( s32 DeviceIndex = INPUT_DEVICE_NONE + 1;
         DeviceIndex < INPUT_DEVICE_COUNT;
         DeviceIndex++ )
    {
        input_device const Candidate = (input_device)DeviceIndex;
        if( Candidate == INPUT_DEVICE_MESSAGE )
        {
            continue;
        }

        for( s32 CandidateID = 0; CandidateID < INPUT_MAX_DEVICES; CandidateID++ )
        {
            if( m_pBackend->IsDevicePresent( Candidate, CandidateID ) )
            {
                Device   = Candidate;
                DeviceID = CandidateID;
                return TRUE;
            }
        }
    }

    return FALSE;
}

//==============================================================================

#ifdef TARGET_DESKTOP
s32 input_system::GetPadCount( void ) const
{
    return m_pBackend ? m_pBackend->GetPadCount() : 0;
}
#endif

//==============================================================================

void input_system::Feedback( f32 Duration, f32 Intensity, s32 DeviceID )
{
    if( m_pBackend )
    {
        m_pBackend->Feedback( Duration, Intensity, DeviceID );
    }
}

//==============================================================================

void input_system::Feedback( s32 Count, feedback_envelope* pEnvelope, s32 DeviceID )
{
    if( m_pBackend )
    {
        m_pBackend->Feedback( Count, pEnvelope, DeviceID );
    }
}

//==============================================================================

void input_system::EnableFeedback( xbool State, s32 DeviceID )
{
    if( m_pBackend )
    {
        m_pBackend->EnableFeedback( State, DeviceID );
    }
}

//==============================================================================

void input_system::SuppressFeedback( xbool Suppress )
{
    if( m_pBackend )
    {
        m_pBackend->SuppressFeedback( Suppress );
    }
}

//==============================================================================

void input_system::ClearFeedback( void )
{
    if( m_pBackend )
    {
        m_pBackend->ClearFeedback();
    }
}

//==============================================================================
//  FRAME SAMPLING INTERNALS
//==============================================================================

xbool input_system::IsInputDeviceValid( s32 DeviceID ) const
{
    return( (DeviceID >= 0) && (DeviceID < INPUT_MAX_DEVICES) );
}

//==============================================================================
//  INPUT ACTIVITY INTERNALS
//==============================================================================

void input_system::ClearFrameActivity( void )
{
    m_FrameActivityDevice   = INPUT_DEVICE_NONE;
    m_FrameActivityPlatform = INPUT_PLATFORM_NONE;
    m_FrameActivityPriority = INPUT_ACTIVITY_NONE;
    m_FrameActivityConflict = FALSE;
}

//==============================================================================

void input_system::RecordGadgetActivity( const input_gadget_info& Info, f32 Value, s32 Priority )
{
    if( x_abs( Value ) <= s_InputActivityThreshold )
    {
        return;
    }

    if( (Info.ValueKind == INPUT_VALUE_NONE) || (Info.ValueKind == INPUT_VALUE_QUERY) )
    {
        return;
    }

    if( Info.Device == INPUT_DEVICE_NONE )
    {
        return;
    }

    if( Priority <= INPUT_ACTIVITY_NONE )
    {
        return;
    }

    if( Priority > m_FrameActivityPriority )
    {
        m_FrameActivityDevice   = Info.Device;
        m_FrameActivityPlatform = Info.Platform;
        m_FrameActivityPriority = Priority;
        m_FrameActivityConflict = FALSE;
        return;
    }

    if( Priority == m_FrameActivityPriority )
    {
        if(   (m_FrameActivityDevice   != Info.Device)
           || (m_FrameActivityPlatform != Info.Platform) )
        {
            m_FrameActivityConflict = TRUE;
        }
    }
}

//==============================================================================

void input_system::ResolveFrameActivity( void )
{
    if( m_FrameActivityPriority == INPUT_ACTIVITY_NONE )
    {
        return;
    }

    if( !m_FrameActivityConflict )
    {
        m_CurrentDevice   = m_FrameActivityDevice;
        m_CurrentPlatform = m_FrameActivityPlatform;
    }

    ClearFrameActivity();
}

//==============================================================================
//  INPUT TIMELINE
//==============================================================================

void input_system::InitializeSnapshotState( input_snapshot& Snapshot ) const
{
    Snapshot.Clear();

    for( s32 GadgetID = 0; GadgetID < INPUT_GADGET_COUNT; GadgetID++ )
    {
        input_gadget const Gadget = static_cast<input_gadget>( GadgetID );
        input_gadget_info const& Info = GetGadgetInfo( Gadget );

        if( Info.ValueKind == INPUT_VALUE_NONE )
        {
            continue;
        }

        for( s32 DeviceID = 0; DeviceID < INPUT_MAX_DEVICES; DeviceID++ )
        {
            input_gadget_sample& Sample = Snapshot.m_Gadgets[GadgetID][DeviceID];

            Sample.IsPresent = IsRawGadgetPresent( Gadget, DeviceID );
            if( Info.ValueKind == INPUT_VALUE_DIGITAL )
            {
                Sample.IsDown = IsRawGadgetDown( Gadget, DeviceID );
                Sample.Value  = GetRawGadgetValue( Gadget, DeviceID );
            }
            else if( Info.ValueKind == INPUT_VALUE_ABSOLUTE_AXIS )
            {
                Sample.Value = GetRawGadgetValue( Gadget, DeviceID );
            }
        }
    }
}

//==============================================================================

void input_system::ApplyEvent( input_snapshot& Snapshot, input_event const& Event )
{
    if( (Event.GadgetID < 0) || (Event.GadgetID >= INPUT_GADGET_COUNT) ||
        !IsInputDeviceValid( Event.DeviceID ) )
    {
        return;
    }

    input_gadget_sample& Sample = Snapshot.m_Gadgets[Event.GadgetID][Event.DeviceID];
    input_gadget_info const& Info = GetGadgetInfo( Event.GadgetID );

    switch( Event.Type )
    {
        case INPUT_EVENT_PRESSED:
        {
            Sample.IsDown      = TRUE;
            Sample.WasPressed  = TRUE;
            Sample.Value       = Event.Value;
        }
        break;

        case INPUT_EVENT_RELEASED:
        {
            Sample.IsDown       = FALSE;
            Sample.WasReleased  = TRUE;
            Sample.Value        = 0.0f;
        }
        break;

        case INPUT_EVENT_ABSOLUTE:
        {
            Sample.Value = Event.Value;
        }
        break;

        case INPUT_EVENT_RELATIVE:
        {
            Sample.Value += Event.Value;
        }
        break;

        case INPUT_EVENT_EXIT:
        {
            Sample.IsDown      = TRUE;
            Sample.WasPressed  = TRUE;
            Sample.Value       = 1.0f;
        }
        break;
    }

    if( &Snapshot == &m_FrameSnapshot )
    {
        s32 const ActivityPriority = GetSampleActivityPriority( Info,
                                                                Event.Value,
                                                                Event.Type == INPUT_EVENT_PRESSED );
        if( ActivityPriority != INPUT_ACTIVITY_NONE )
        {
            RecordGadgetActivity( Info,
                                  Event.Type == INPUT_EVENT_PRESSED ? 1.0f : Event.Value,
                                  ActivityPriority );
        }
    }
}

//==============================================================================

void input_system::BuildFrameSnapshot( void )
{
    InitializeSnapshotState( m_FrameSnapshot );

    for( s32 i = 0; i < m_CapturedEvents.GetCount(); i++ )
    {
        ApplyEvent( m_FrameSnapshot, m_CapturedEvents.GetEvent( i ) );
    }

    ResolveFrameActivity();
}

//==============================================================================

xbool input_system::CaptureFrameInput( void )
{
    m_DebugStats = input_debug_stats();
    ClearFrameActivity();

    if( !m_pBackend )
    {
        m_FrameSnapshot.Clear();
        return FALSE;
    }

    xbool const ExitRequested = m_pBackend->CaptureFrameInput( m_CapturedEvents );
    m_CapturedEvents.Sort();
    BuildFrameSnapshot();

    m_DebugStats.CaptureMilliseconds = static_cast<f32>( m_CapturedEvents.GetEndTimeStamp() -
                                                         m_CapturedEvents.GetStartTimeStamp() );
    for( s32 i = 0; i < m_CapturedEvents.GetCount(); i++ )
    {
        AddMouseDebugEvent( m_CapturedEvents.GetEvent( i ),
                            m_DebugStats.CapturedMouseEvents,
                            m_DebugStats.CapturedMouseDeltaX,
                            m_DebugStats.CapturedMouseDeltaY );
    }

    return ExitRequested || m_FrameSnapshot.IsPressed( INPUT_MSG_EXIT );
}

//==============================================================================

input_snapshot const& input_system::GetFrameSnapshot( void ) const
{
    return m_FrameSnapshot;
}

//==============================================================================

input_debug_stats const& input_system::GetDebugStats( void ) const
{
    return m_DebugStats;
}

//==============================================================================
//  GADGET METADATA
//==============================================================================

const input_gadget_info& input_system::GetGadgetInfo( input_gadget GadgetID )
{
    if( GadgetID == INPUT_UNDEFINED )
    {
        return s_InvalidGadgetInfo;
    }

    for( s32 i = 0; i < (s32)(sizeof(s_GadgetInfoRanges) / sizeof(s_GadgetInfoRanges[0])); i++ )
    {
        if( IsGadgetInRange( GadgetID, s_GadgetInfoRanges[i].First, s_GadgetInfoRanges[i].Last ) )
        {
            return s_GadgetInfoRanges[i].Info;
        }
    }

    return s_InvalidGadgetInfo;
}

//==============================================================================

input_platform input_system::GetGadgetPlatform( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).Platform;
}

//==============================================================================

input_device input_system::GetGadgetDevice( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).Device;
}

//==============================================================================

input_value_kind input_system::GetGadgetValueKind( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).ValueKind;
}

//==============================================================================

input_control_kind input_system::GetGadgetControlKind( input_gadget GadgetID )
{
    return GetGadgetInfo( GadgetID ).ControlKind;
}

//==============================================================================

xbool input_system::IsGadgetValid( input_gadget GadgetID )
{
    return( GetGadgetInfo( GadgetID ).ValueKind != INPUT_VALUE_NONE );
}

//==============================================================================

xbool input_system::IsPromptDeviceActivity( input_gadget GadgetID,
                                            f32          Value,
                                            xbool        WasPressed )
{
    input_gadget_info const& Info = GetGadgetInfo( GadgetID );
    s32 const Priority = GetSampleActivityPriority( Info, Value, WasPressed );
    f32 const ActivityValue = WasPressed ? 1.0f : Value;
    return( Priority != INPUT_ACTIVITY_NONE ) &&
           (x_abs( ActivityValue ) > s_InputActivityThreshold);
}

//==============================================================================
//  INPUT METADATA QUERIES
//==============================================================================

xbool input_system::WasFrameDeviceButtonPressed( input_device Device ) const
{
    if( Device == INPUT_DEVICE_NONE )
    {
        return FALSE;
    }

    for( s32 iRange = 0; iRange < (s32)(sizeof(s_GadgetInfoRanges) / sizeof(s_GadgetInfoRanges[0])); iRange++ )
    {
        const input_gadget_range& Range = s_GadgetInfoRanges[iRange];
        const input_gadget_info&  Info  = Range.Info;

        if( (Info.Device != Device) || (Info.ValueKind != INPUT_VALUE_DIGITAL) )
        {
            continue;
        }

        s32 First = (s32)Range.First;
        s32 Last  = (s32)Range.Last;

        if( First < 0 )
        {
            First = 0;
        }

        if( Last >= INPUT_GADGET_COUNT )
        {
            Last = INPUT_GADGET_COUNT - 1;
        }

        for( s32 GadgetID = First; GadgetID <= Last; GadgetID++ )
        {
            for( s32 DeviceID = 0; DeviceID < INPUT_MAX_DEVICES; DeviceID++ )
            {
                if( m_FrameSnapshot.WasPressed( static_cast<input_gadget>( GadgetID ), DeviceID ) )
                {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

//==============================================================================
//  INPUT ACTIVITY STATE
//==============================================================================

input_device input_system::GetCurrentInputDevice( void ) const
{
    return m_CurrentDevice;
}

//==============================================================================

input_platform input_system::GetCurrentInputPlatform( void ) const
{
    return m_CurrentPlatform;
}

//==============================================================================
//  MOUSE DELTA QUERIES
//==============================================================================

s32 input_system::GetFrameMouseDeltaX( s32 DeviceID ) const
{
    return static_cast<s32>( m_FrameSnapshot.GetValue( INPUT_MOUSE_X_REL, DeviceID ) );
}

//==============================================================================

s32 input_system::GetFrameMouseDeltaY( s32 DeviceID ) const
{
    return static_cast<s32>( m_FrameSnapshot.GetValue( INPUT_MOUSE_Y_REL, DeviceID ) );
}

//==============================================================================
//  GADGET NAME LOOKUP
//==============================================================================

input_gadget input_system::LookupGadget( const char* pName )
{
    ASSERT( pName );

    if( !pName )
    {
        return( INPUT_UNDEFINED );
    }

    for( s32 i = 0; i < (s32)(sizeof(s_GadgetLookupTable) / sizeof(s_GadgetLookupTable[0])); i++ )
    {
        if( x_strcmp( s_GadgetLookupTable[i].pName, pName ) == 0 )
        {
            return( s_GadgetLookupTable[i].Gadget );
        }
    }

    return( INPUT_UNDEFINED );
}
