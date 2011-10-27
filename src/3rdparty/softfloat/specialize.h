/*
 * This file is derivative of part of the SoftFloat IEC/IEEE
 * Floating-point Arithmetic Package, Release 2b.
*/

/*============================================================================

This C source fragment is part of the SoftFloat IEC/IEEE Floating-point
Arithmetic Package, Release 2b.

Written by John R. Hauser.  This work was made possible in part by the
International Computer Science Institute, located at Suite 600, 1947 Center
Street, Berkeley, California 94704.  Funding was partially provided by the
National Science Foundation under grant MIP-9311980.  The original version
of this code was written as part of a project to build a fixed-point vector
processor in collaboration with the University of California at Berkeley,
overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
is available through the Web page `http://www.cs.berkeley.edu/~jhauser/
arithmetic/SoftFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) the source code for the derivative work includes prominent notice that
the work is derivative, and (2) the source code includes prominent notice with
these four paragraphs for those parts of this code that are retained.

=============================================================================*/

#ifndef SHAREMIND_SOFTFLOAT_SPECIALIZE_H
#define SHAREMIND_SOFTFLOAT_SPECIALIZE_H

#include "softfloat.h"


#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
| Underflow tininess-detection mode, statically initialized to default value.
| (The declaration in `softfloat.h' must match the `int8' type here.)
*----------------------------------------------------------------------------*/
sf_int8 sf_float_detect_tininess = sf_float_tininess_after_rounding;


/*----------------------------------------------------------------------------
| Internal canonical NaN format.
*----------------------------------------------------------------------------*/
typedef struct {
    sf_flag sign;
    sf_bits64 high, low;
} sf_commonNaNT;

/*----------------------------------------------------------------------------
| The pattern for a default generated single-precision NaN.
*----------------------------------------------------------------------------*/
#define sf_float32_default_nan 0xFFC00000

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is a NaN;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_is_nan( sf_float32 a )
{

    return ( 0xFF000000 < (sf_bits32) ( a<<1 ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is a signaling
| NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_is_signaling_nan( sf_float32 a )
{

    return ( ( ( a>>22 ) & 0x1FF ) == 0x1FE ) && ( a & 0x003FFFFF );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

static sf_commonNaNT sf_float32ToCommonNaN( sf_float32 a )
{
    sf_commonNaNT z;

    if ( sf_float32_is_signaling_nan( a ) ) sf_float_raise( sf_float_flag_invalid );
    z.sign = (sf_flag) (a>>31);
    z.low = 0;
    z.high = ( (sf_bits64) a )<<41;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the single-
| precision floating-point format.
*----------------------------------------------------------------------------*/

static sf_float32 sf_commonNaNToFloat32( sf_commonNaNT a )
{

    return (sf_float32) (( ( (sf_bits32) a.sign )<<31 ) | 0x7FC00000 | ( a.high>>41 ));

}

/*----------------------------------------------------------------------------
| Takes two single-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

static sf_float32 sf_propagateFloat32NaN( sf_float32 a, sf_float32 b )
{
    sf_flag aIsNaN, aIsSignalingNaN, bIsNaN, bIsSignalingNaN;

    aIsNaN = sf_float32_is_nan( a );
    aIsSignalingNaN = sf_float32_is_signaling_nan( a );
    bIsNaN = sf_float32_is_nan( b );
    bIsSignalingNaN = sf_float32_is_signaling_nan( b );
    a |= 0x00400000;
    b |= 0x00400000;
    if ( aIsSignalingNaN | bIsSignalingNaN ) sf_float_raise( sf_float_flag_invalid );
    if ( aIsSignalingNaN ) {
        if ( bIsSignalingNaN ) goto returnLargerSignificand;
        return bIsNaN ? b : a;
    }
    else if ( aIsNaN ) {
        if ( bIsSignalingNaN | ! bIsNaN ) return a;
 returnLargerSignificand:
        if ( (sf_bits32) ( a<<1 ) < (sf_bits32) ( b<<1 ) ) return b;
        if ( (sf_bits32) ( b<<1 ) < (sf_bits32) ( a<<1 ) ) return a;
        return ( a < b ) ? a : b;
    }
    else {
        return b;
    }

}

/*----------------------------------------------------------------------------
| The pattern for a default generated double-precision NaN.
*----------------------------------------------------------------------------*/
#define sf_float64_default_nan SF_LIT64( 0xFFF8000000000000 )

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a NaN;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_is_nan( sf_float64 a )
{

    return ( SF_LIT64( 0xFFE0000000000000 ) < (sf_bits64) ( a<<1 ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a signaling
| NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_is_signaling_nan( sf_float64 a )
{

    return
           ( ( ( a>>51 ) & 0xFFF ) == 0xFFE )
        && ( a & SF_LIT64( 0x0007FFFFFFFFFFFF ) );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point NaN
| `a' to the canonical NaN format.  If `a' is a signaling NaN, the invalid
| exception is raised.
*----------------------------------------------------------------------------*/

static sf_commonNaNT sf_float64ToCommonNaN( sf_float64 a )
{
    sf_commonNaNT z;

    if ( sf_float64_is_signaling_nan( a ) ) sf_float_raise( sf_float_flag_invalid );
    z.sign = (sf_flag) (a>>63);
    z.low = 0;
    z.high = a<<12;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of converting the canonical NaN `a' to the double-
| precision floating-point format.
*----------------------------------------------------------------------------*/

static sf_float64 sf_commonNaNToFloat64( sf_commonNaNT a )
{

    return
          ( ( (sf_bits64) a.sign )<<63 )
        | SF_LIT64( 0x7FF8000000000000 )
        | ( a.high>>12 );

}

/*----------------------------------------------------------------------------
| Takes two double-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

static sf_float64 sf_propagateFloat64NaN( sf_float64 a, sf_float64 b )
{
    sf_flag aIsNaN, aIsSignalingNaN, bIsNaN, bIsSignalingNaN;

    aIsNaN = sf_float64_is_nan( a );
    aIsSignalingNaN = sf_float64_is_signaling_nan( a );
    bIsNaN = sf_float64_is_nan( b );
    bIsSignalingNaN = sf_float64_is_signaling_nan( b );
    a |= SF_LIT64( 0x0008000000000000 );
    b |= SF_LIT64( 0x0008000000000000 );
    if ( aIsSignalingNaN | bIsSignalingNaN ) sf_float_raise( sf_float_flag_invalid );
    if ( aIsSignalingNaN ) {
        if ( bIsSignalingNaN ) goto returnLargerSignificand;
        return bIsNaN ? b : a;
    }
    else if ( aIsNaN ) {
        if ( bIsSignalingNaN | ! bIsNaN ) return a;
 returnLargerSignificand:
        if ( (sf_bits64) ( a<<1 ) < (sf_bits64) ( b<<1 ) ) return b;
        if ( (sf_bits64) ( b<<1 ) < (sf_bits64) ( a<<1 ) ) return a;
        return ( a < b ) ? a : b;
    }
    else {
        return b;
    }

}

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_SOFTFLOAT_SPECIALIZE_H */
