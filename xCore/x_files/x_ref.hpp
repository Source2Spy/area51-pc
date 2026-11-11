//==============================================================================
//
//  x_ref.hpp
//
//  Intrusive references for resources shared by submission and worker threads.
//
//==============================================================================

#ifndef X_REF_HPP
#define X_REF_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_atomic.hpp"
#include "x_debug.hpp"

//==============================================================================
//  XREF COUNTED CLASS
//==============================================================================

class xref_counted
{
public:
                        xref_counted       ( xbool NotifyLastReference = FALSE );
    void                AddReference       ( void ) const;
    void                ReleaseReference   ( void ) const;
    s32                 GetReferenceCount  ( void ) const;

protected:
    virtual            ~xref_counted       ( void );
    s32                 RemoveReference    ( void ) const;
    // Observed resources serialize release with their cache/registry lock.
    virtual void        OnReferenceReleased( s32 Count ) const;

private:
                        xref_counted       ( const xref_counted& );
    xref_counted&       operator=          ( const xref_counted& );

    mutable x_atomic_s32 m_References;
    xbool                m_NotifyLastReference;
};

//------------------------------------------------------------------------------

template< class T >
class xref
{
public:
                        xref        ( void );
                        xref        ( T* pObject );
                        xref        ( const xref& Other );
                       ~xref        ( void );

    template< class U > xref        ( const xref<U>& Other );

    xref&               operator=   ( const xref& Other );
    template< class U > xref& operator=( const xref<U>& Other );

                        operator T* ( void ) const;
    T*                  operator->  ( void ) const;
    T&                  operator*   ( void ) const;
    T*                  Get         ( void ) const;
    void                Set         ( T* pObject );
    void                Clear       ( void );

private:
    T*                  m_pObject;
};

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline xref_counted::xref_counted( xbool NotifyLastReference ) : m_NotifyLastReference( NotifyLastReference )
{
    x_AtomicInit( &m_References, 0 );
}

//=============================================================================

inline xref_counted::~xref_counted( void )
{
    ASSERT( GetReferenceCount() == 0 );
}

//=============================================================================

inline s32 xref_counted::GetReferenceCount( void ) const
{
    return x_AtomicLoadAcquire( &m_References );
}

//=============================================================================

inline void xref_counted::AddReference( void ) const
{
    s32 Previous = x_AtomicLoadAcquire( &m_References );
    for( ;; )
    {
        ASSERT( Previous >= 0 && Previous < S32_MAX );
        const s32 Observed = x_AtomicCompareExchange( &m_References, Previous + 1, Previous );
        if( Observed == Previous )
            return;
        Previous = Observed;
    }
}

//=============================================================================

inline s32 xref_counted::RemoveReference( void ) const
{
    s32 Previous = x_AtomicLoadAcquire( &m_References );
    for( ;; )
    {
        ASSERT( Previous > 0 );
        const s32 Observed = x_AtomicCompareExchange( &m_References, Previous - 1, Previous );
        if( Observed == Previous )
            return Previous - 1;
        Previous = Observed;
    }
}

//=============================================================================

inline void xref_counted::ReleaseReference( void ) const
{
    const xbool NotifyLastReference = m_NotifyLastReference;
    const s32 Count = RemoveReference();
    if( Count == 0 )
        delete this;
    else if( Count == 1 && NotifyLastReference )
        OnReferenceReleased( Count );
}

//=============================================================================

inline void xref_counted::OnReferenceReleased( s32 ) const
{
}

//==============================================================================
//  TEMPLATE FUNCTIONS
//==============================================================================

template< class T >
inline xref<T>::xref( void ) : m_pObject( NULL )
{
}

//==============================================================================

template< class T >
inline xref<T>::xref( T* pObject ) : m_pObject( NULL )
{
    Set( pObject );
}

//==============================================================================

template< class T >
inline xref<T>::xref( const xref& Other ) : m_pObject( NULL )
{
    Set( Other.Get() );
}

//==============================================================================

template< class T >
template< class U >
inline xref<T>::xref( const xref<U>& Other ) : m_pObject( NULL )
{
    Set( Other.Get() );
}

//==============================================================================

template< class T >
inline xref<T>::~xref( void )
{
    Clear();
}

//==============================================================================

template< class T >
inline xref<T>& xref<T>::operator=( const xref& Other )
{
    Set( Other.Get() );
    return *this;
}

//==============================================================================

template< class T >
template< class U >
inline xref<T>& xref<T>::operator=( const xref<U>& Other )
{
    Set( Other.Get() );
    return *this;
}

//==============================================================================

template< class T >
inline xref<T>::operator T*( void ) const
{
    return m_pObject;
}

//==============================================================================

template< class T >
inline T* xref<T>::operator->( void ) const
{
    ASSERT( m_pObject );
    return m_pObject;
}

//==============================================================================

template< class T >
inline T& xref<T>::operator*( void ) const
{
    ASSERT( m_pObject );
    return *m_pObject;
}

//==============================================================================

template< class T >
inline T* xref<T>::Get( void ) const
{
    return m_pObject;
}

//==============================================================================

template< class T >
inline void xref<T>::Set( T* pObject )
{
    if( pObject == m_pObject )
        return;
    if( pObject )
        pObject->AddReference();
    T* pPrevious = m_pObject;
    m_pObject = pObject;
    if( pPrevious )
        pPrevious->ReleaseReference();
}

//==============================================================================

template< class T >
inline void xref<T>::Clear( void )
{
    Set( NULL );
}

//==============================================================================
#endif // X_REF_HPP
//==============================================================================
