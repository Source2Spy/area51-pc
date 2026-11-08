//=========================================================================
//
//  LeastSquares.cpp
//
//  Copyright (c) 2004 Inevitable Entertainment Inc. All rights reserved.
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_plus.hpp"

#include "LeastSquares.hpp"

//=========================================================================
//  CONSTANTS
//=========================================================================

static f64 const kEpsilon = 0.001;

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

LeastSquares::LeastSquares( void ) : m_nSamples( 0 ), m_Degree( 0 ), m_isSolved( FALSE )
{
    x_memset( m_Matrix, 0, sizeof( m_Matrix ) );
    x_memset( m_Vector, 0, sizeof( m_Vector ) );
    x_memset( m_Coeffs, 0, sizeof( m_Coeffs ) );
}

//=========================================================================

LeastSquares::~LeastSquares( void )
{
}

//=========================================================================

void LeastSquares::Setup( s32 PolynomialDegree )
{
    ASSERT( ( PolynomialDegree > 0 ) && ( PolynomialDegree <= MAX_POLYNOMIAL_DEGREE ) );
    m_nSamples = 0;
    m_Degree = PolynomialDegree;
    m_isSolved = FALSE;

    // clear out the matrix and vector in preparation for solving the approximation
    x_memset( m_Matrix, 0, sizeof( m_Matrix ) );
    x_memset( m_Vector, 0, sizeof( m_Vector ) );
    x_memset( m_Coeffs, 0, sizeof( m_Coeffs ) );
}

//=========================================================================

void LeastSquares::AddSample( f32 x, f32 y )
{
    ASSERT( m_isSolved == FALSE );
    ASSERT( m_Degree );

    f64 xPowers[( MAX_POLYNOMIAL_DEGREE * 2 ) + 1];

    // figure out the powers of x that we'll need
    s32 i;
    xPowers[0] = 1.0;
    xPowers[1] = static_cast<f64>( x );
    for ( i = 2; i <= m_Degree * 2; i++ )
    {
        xPowers[i] = x * xPowers[i - 1];
    }

    // now update the matrix sums
    s32 row, col;
    for ( row = 0; row <= m_Degree; row++ )
    {
        for ( col = 0; col <= m_Degree; col++ )
        {
            m_Matrix[row][col] += xPowers[row + col];
        }
    }

    // now update the vector sums
    for ( i = 0; i <= m_Degree; i++ )
    {
        m_Vector[i] += xPowers[i] * static_cast<f64>( y );
    }

    // and mark that we've added one more sample
    m_nSamples++;
}

//=========================================================================

xbool LeastSquares::Solve( void )
{
    // we can only solve if we have one more sample than the degree of our
    // polynomial
    if ( m_nSamples < ( m_Degree + 1 ) )
    {
        ASSERTS( FALSE, "More samples needed to get a decent result!" );
        return FALSE;
    }

    // perform gaussian elimination until we get an upper triangular matrix
    s32 i, j, k;
    for ( i = 0; i < m_Degree; i++ )
    {
        // find the biggest element and pivot using that (it turns out that
        // in most cases the biggest element is the best for dealing with
        // accumulated rounding errors)
        s32 biggest = i;
        f64 AbsBiggest = x_abs( m_Matrix[biggest][i] );
        for ( j = i + 1; j <= m_Degree; j++ )
        {
            f64 AbsJ = x_abs( m_Matrix[j][i] );
            if ( AbsJ > AbsBiggest )
            {
                biggest = j;
                AbsBiggest = AbsJ;
            }
        }

        // if the pivot element is zero, then we have a singular matrix,
        // which won't have a solution
        if ( AbsBiggest < kEpsilon )
        {
            // ASSERTS( FALSE, "Singular matrix" );
            m_isSolved = TRUE;
            return FALSE;
        }

        // now swap the pivot row with row i
        if ( biggest != i )
        {
            f64 temp;
            for ( j = i; j <= m_Degree; j++ )
            {
                temp = m_Matrix[i][j];
                m_Matrix[i][j] = m_Matrix[biggest][j];
                m_Matrix[biggest][j] = temp;
            }

            temp = m_Vector[i];
            m_Vector[i] = m_Vector[biggest];
            m_Vector[biggest] = temp;
        }

        // now do the elimination step for this pivot on each of the rows
        for ( j = i + 1; j <= m_Degree; j++ )
        {
            f64 scale = m_Matrix[j][i] / m_Matrix[i][i];
            for ( k = i; k <= m_Degree; k++ )
            {
                m_Matrix[j][k] -= scale * m_Matrix[i][k];
            }

            m_Vector[j] -= scale * m_Vector[i];

            // Sanity check...make sure we maintain zeros to the left...if
            // we've implemented the gaussian elimination correctly, and if
            // we haven't run into horrendous rounding errors, this should
            // be the case.
            /*#ifdef X_DEBUG
            for( k = 0; k <= i; k++ )
            {
                ASSERT( x_abs(m_Matrix[j][k]) < kEpsilon );
            }
            #endif*/
        }
    }

    // now we should have an upper triangular matrix, we can use back
    // substitution to figure out the coefficients
    for ( i = m_Degree; i >= 0; i-- )
    {
        f64 total = 0.0f;
        for ( j = i + 1; j <= m_Degree; j++ )
        {
            total += m_Matrix[i][j] * m_Coeffs[j];
        }
        m_Coeffs[i] = ( m_Vector[i] - total ) / m_Matrix[i][i];
    }

    m_isSolved = TRUE;
    return TRUE;
}

//=========================================================================

f32 LeastSquares::GetCoeff( s32 index )
{
    ASSERT( m_isSolved );
    ASSERT( ( index >= 0 ) && ( index <= m_Degree ) );
    return static_cast<f32>( m_Coeffs[index] );
}

//=========================================================================

void LeastSquares::SetCoeff( s32 index, f32 value )
{
    ASSERT( m_isSolved );
    ASSERT( ( index >= 0 ) && ( index <= m_Degree ) );

    m_Coeffs[index] = value;
}

//=========================================================================

f32 LeastSquares::Evaluate( f32 x )
{
    s32 i;

    // figure out the powers of x
    f64 xPowers[MAX_POLYNOMIAL_DEGREE + 1];
    xPowers[0] = 1.0;
    xPowers[1] = static_cast<f64>( x );
    for ( i = 2; i <= m_Degree; i++ )
    {
        xPowers[i] = static_cast<f64>( x ) * xPowers[i - 1];
    }

    // evaluate the polynomial
    f64 total = 0.0;
    for ( i = 0; i <= m_Degree; i++ )
    {
        total += xPowers[i] * m_Coeffs[i];
    }

    return static_cast<f32>( total );
}
