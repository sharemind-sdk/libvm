/*
 * This file is derivative of part of the SoftFloat IEC/IEEE
 * Floating-point Arithmetic Package, Release 2b.
*/

/*============================================================================

This C source file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
Package, Release 2b.

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

#include "softfloat.h"

/*----------------------------------------------------------------------------
| Primitive arithmetic functions, including multi-word arithmetic, and
| division and square root approximations.  (Can be specialized to target if
| desired.)
*----------------------------------------------------------------------------*/
#include "macros.h"

/*----------------------------------------------------------------------------
| Functions and definitions to determine:  (1) whether tininess for underflow
| is detected before or after rounding by default, (2) what (if anything)
| happens when exceptions are raised, (3) how signaling NaNs are distinguished
| from quiet NaNs, (4) the default generated quiet NaNs, and (5) how NaNs
| are propagated from function inputs to output.  These details are target-
| specific.
*----------------------------------------------------------------------------*/
#include "specialize.h"


/*----------------------------------------------------------------------------
| Floating-point rounding mode, extended double-precision rounding precision,
| and exception flags.
*----------------------------------------------------------------------------*/
sf_int8 sf_float_rounding_mode = sf_float_round_nearest_even;
sf_int8 sf_float_exception_flags = 0;

/*----------------------------------------------------------------------------
| Takes a 64-bit fixed-point value `absZ' with binary point between bits 6
| and 7, and returns the properly rounded 32-bit integer corresponding to the
| input.  If `zSign' is 1, the input is negated before being converted to an
| integer.  Bit 63 of `absZ' must be zero.  Ordinarily, the fixed-point input
| is simply rounded to an integer, with the inexact exception raised if the
| input cannot be represented exactly as an integer.  However, if the fixed-
| point input is too large, the invalid exception is raised and the largest
| positive or negative integer is returned.
*----------------------------------------------------------------------------*/

static sf_int32 sf_roundAndPackInt32( sf_flag zSign, sf_bits64 absZ )
{
    sf_int8 roundingMode;
    sf_flag roundNearestEven;
    sf_int8 roundIncrement, roundBits;
    sf_int32 z;

    roundingMode = sf_float_rounding_mode;
    roundNearestEven = ( roundingMode == sf_float_round_nearest_even );
    roundIncrement = 0x40;
    if ( ! roundNearestEven ) {
        if ( roundingMode == sf_float_round_to_zero ) {
            roundIncrement = 0;
        }
        else {
            roundIncrement = 0x7F;
            if ( zSign ) {
                if ( roundingMode == sf_float_round_up ) roundIncrement = 0;
            }
            else {
                if ( roundingMode == sf_float_round_down ) roundIncrement = 0;
            }
        }
    }
    roundBits = absZ & 0x7F;
    absZ = ( absZ + (sf_bits64) roundIncrement )>>7;
    absZ &= ~ (sf_bits64) ( ( ( roundBits ^ 0x40 ) == 0 ) & roundNearestEven );
    z = (sf_int32) absZ;
    if ( zSign ) z = - z;
    if ( ( absZ>>32 ) || ( z && ( ( z < 0 ) ^ zSign ) ) ) {
        sf_float_raise( sf_float_flag_invalid );
        return zSign ? (sf_sbits32) 0x80000000 : 0x7FFFFFFF;
    }
    if ( roundBits ) sf_float_exception_flags |= sf_float_flag_inexact;
    return z;

}

/*----------------------------------------------------------------------------
| Takes the 128-bit fixed-point value formed by concatenating `absZ0' and
| `absZ1', with binary point between bits 63 and 64 (between the input words),
| and returns the properly rounded 64-bit integer corresponding to the input.
| If `zSign' is 1, the input is negated before being converted to an integer.
| Ordinarily, the fixed-point input is simply rounded to an integer, with
| the inexact exception raised if the input cannot be represented exactly as
| an integer.  However, if the fixed-point input is too large, the invalid
| exception is raised and the largest positive or negative integer is
| returned.
*----------------------------------------------------------------------------*/

static sf_int64 sf_roundAndPackInt64(
    sf_flag zSign, sf_bits64 absZ0, sf_bits64 absZ1 )
{
    sf_int8 roundingMode;
    sf_flag roundNearestEven, increment;
    sf_int64 z;

    roundingMode = sf_float_rounding_mode;
    roundNearestEven = ( roundingMode == sf_float_round_nearest_even );
    increment = ( (sf_sbits64) absZ1 < 0 );
    if ( ! roundNearestEven ) {
        if ( roundingMode == sf_float_round_to_zero ) {
            increment = 0;
        }
        else {
            if ( zSign ) {
                increment = ( roundingMode == sf_float_round_down ) && absZ1;
            }
            else {
                increment = ( roundingMode == sf_float_round_up ) && absZ1;
            }
        }
    }
    if ( increment ) {
        ++absZ0;
        if ( absZ0 == 0 ) goto overflow;
        absZ0 &= ~ (sf_bits64) ( ( (sf_bits64) ( absZ1<<1 ) == 0 ) & roundNearestEven );
    }
    z = (sf_int64) absZ0;
    if ( zSign ) z = - z;
    if ( z && ( ( z < 0 ) ^ zSign ) ) {
 overflow:
        sf_float_raise( sf_float_flag_invalid );
        return
              zSign ? (sf_sbits64) SF_LIT64( 0x8000000000000000 )
            : SF_LIT64( 0x7FFFFFFFFFFFFFFF );
    }
    if ( absZ1 ) sf_float_exception_flags |= sf_float_flag_inexact;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the fraction bits of the single-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline sf_bits32 sf_extractFloat32Frac(sf_float32 a) {

    return a & 0x007FFFFF;

}

/*----------------------------------------------------------------------------
| Returns the exponent bits of the single-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline sf_int16 sf_extractFloat32Exp(sf_float32 a) {

    return ( a>>23 ) & 0xFF;

}

/*----------------------------------------------------------------------------
| Returns the sign bit of the single-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline sf_flag sf_extractFloat32Sign(sf_float32 a) {

    return (sf_flag) (a>>31);

}

/*----------------------------------------------------------------------------
| Normalizes the subnormal single-precision floating-point value represented
| by the denormalized significand `aSig'.  The normalized exponent and
| significand are stored at the locations pointed to by `zExpPtr' and
| `zSigPtr', respectively.
*----------------------------------------------------------------------------*/

static inline void sf_normalizeFloat32Subnormal(sf_bits32 aSig,
                                                sf_int16 * const restrict zExpPtr,
                                                sf_bits32 * const restrict zSigPtr)
{
    sf_int8 shiftCount;

    shiftCount = (sf_int8) (sf_countLeadingZeros32( aSig ) - 8);
    *zSigPtr = aSig<<shiftCount;
    *zExpPtr = 1 - shiftCount;

}

/*----------------------------------------------------------------------------
| Packs the sign `zSign', exponent `zExp', and significand `zSig' into a
| single-precision floating-point value, returning the result.  After being
| shifted into the proper positions, the three fields are simply added
| together to form the result.  This means that any integer portion of `zSig'
| will be added into the exponent.  Since a properly normalized significand
| will have an integer portion equal to 1, the `zExp' input should be 1 less
| than the desired result exponent whenever `zSig' is a complete, normalized
| significand.
*----------------------------------------------------------------------------*/

static inline sf_float32 sf_packFloat32(sf_flag zSign, sf_int16 zExp, sf_bits32 zSig) {

    return ( ( (sf_bits32) zSign )<<31 ) + ( ( (sf_bits32) zExp )<<23 ) + zSig;

}

/*----------------------------------------------------------------------------
| Takes an abstract floating-point value having sign `zSign', exponent `zExp',
| and significand `zSig', and returns the proper single-precision floating-
| point value corresponding to the abstract input.  Ordinarily, the abstract
| value is simply rounded and packed into the single-precision format, with
| the inexact exception raised if the abstract input cannot be represented
| exactly.  However, if the abstract value is too large, the overflow and
| inexact exceptions are raised and an infinity or maximal finite value is
| returned.  If the abstract value is too small, the input value is rounded to
| a subnormal number, and the underflow and inexact exceptions are raised if
| the abstract input cannot be represented exactly as a subnormal single-
| precision floating-point number.
|     The input significand `zSig' has its binary point between bits 30
| and 29, which is 7 bits to the left of the usual location.  This shifted
| significand must be normalized or smaller.  If `zSig' is not normalized,
| `zExp' must be 0; in that case, the result returned is a subnormal number,
| and it must not require rounding.  In the usual case that `zSig' is
| normalized, `zExp' must be 1 less than the ``true'' floating-point exponent.
| The handling of underflow and overflow follows the IEC/IEEE Standard for
| Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static inline sf_float32 sf_roundAndPackFloat32(sf_flag zSign, sf_int16 zExp, sf_bits32 zSig) {
    sf_int8 roundingMode;
    sf_flag roundNearestEven;
    sf_int8 roundIncrement, roundBits;
    sf_flag isTiny;

    roundingMode = sf_float_rounding_mode;
    roundNearestEven = ( roundingMode == sf_float_round_nearest_even );
    roundIncrement = 0x40;
    if ( ! roundNearestEven ) {
        if ( roundingMode == sf_float_round_to_zero ) {
            roundIncrement = 0;
        }
        else {
            roundIncrement = 0x7F;
            if ( zSign ) {
                if ( roundingMode == sf_float_round_up ) roundIncrement = 0;
            }
            else {
                if ( roundingMode == sf_float_round_down ) roundIncrement = 0;
            }
        }
    }
    roundBits = zSig & 0x7F;
    if ( 0xFD <= (sf_bits16) zExp ) {
        if (    ( 0xFD < zExp )
             || (    ( zExp == 0xFD )
                  && ( (sf_sbits32) ( zSig + (sf_uint32) roundIncrement ) < 0 ) )
           ) {
            sf_float_raise( sf_float_flag_overflow | sf_float_flag_inexact );
            return sf_packFloat32( zSign, 0xFF, 0 ) - ( roundIncrement == 0 );
        }
        if ( zExp < 0 ) {
            isTiny =
                   ( sf_float_detect_tininess == sf_float_tininess_before_rounding )
                || ( zExp < -1 )
                || ( zSig + (sf_uint32) roundIncrement < 0x80000000 );
            sf_shift32RightJamming( zSig, - zExp, &zSig );
            zExp = 0;
            roundBits = zSig & 0x7F;
            if ( isTiny && roundBits ) sf_float_raise( sf_float_flag_underflow );
        }
    }
    if ( roundBits ) sf_float_exception_flags |= sf_float_flag_inexact;
    zSig = (sf_bits32) ( zSig + (sf_uint32) roundIncrement )>>7;
    zSig &= ~ (sf_bits32) ( ( ( roundBits ^ 0x40 ) == 0 ) & roundNearestEven );
    if ( zSig == 0 ) zExp = 0;
    return sf_packFloat32( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Takes an abstract floating-point value having sign `zSign', exponent `zExp',
| and significand `zSig', and returns the proper single-precision floating-
| point value corresponding to the abstract input.  This routine is just like
| `roundAndPackFloat32' except that `zSig' does not have to be normalized.
| Bit 31 of `zSig' must be zero, and `zExp' must be 1 less than the ``true''
| floating-point exponent.
*----------------------------------------------------------------------------*/

static inline sf_float32 sf_normalizeRoundAndPackFloat32(sf_flag zSign, sf_int16 zExp, sf_bits32 zSig) {
    sf_int8 shiftCount;

    shiftCount = (sf_int8) (sf_countLeadingZeros32( zSig ) - 1);
    return sf_roundAndPackFloat32( zSign, zExp - shiftCount, zSig<<shiftCount );

}

/*----------------------------------------------------------------------------
| Returns the fraction bits of the double-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline sf_bits64 sf_extractFloat64Frac(sf_float64 a) {

    return a & SF_LIT64( 0x000FFFFFFFFFFFFF );

}

/*----------------------------------------------------------------------------
| Returns the exponent bits of the double-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline sf_int16 sf_extractFloat64Exp(sf_float64 a) {

    return ( a>>52 ) & 0x7FF;

}

/*----------------------------------------------------------------------------
| Returns the sign bit of the double-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

static inline sf_flag sf_extractFloat64Sign(sf_float64 a) {

    return (sf_flag) (a>>63);

}

/*----------------------------------------------------------------------------
| Normalizes the subnormal double-precision floating-point value represented
| by the denormalized significand `aSig'.  The normalized exponent and
| significand are stored at the locations pointed to by `zExpPtr' and
| `zSigPtr', respectively.
*----------------------------------------------------------------------------*/

static inline void sf_normalizeFloat64Subnormal(sf_bits64 aSig,
                                                sf_int16 * const restrict zExpPtr,
                                                sf_bits64 * const restrict zSigPtr)
{
    sf_int8 shiftCount;

    shiftCount = (sf_int8) (sf_countLeadingZeros64( aSig ) - 11);
    *zSigPtr = aSig<<shiftCount;
    *zExpPtr = 1 - shiftCount;

}

/*----------------------------------------------------------------------------
| Packs the sign `zSign', exponent `zExp', and significand `zSig' into a
| double-precision floating-point value, returning the result.  After being
| shifted into the proper positions, the three fields are simply added
| together to form the result.  This means that any integer portion of `zSig'
| will be added into the exponent.  Since a properly normalized significand
| will have an integer portion equal to 1, the `zExp' input should be 1 less
| than the desired result exponent whenever `zSig' is a complete, normalized
| significand.
*----------------------------------------------------------------------------*/

static inline sf_float64 sf_packFloat64(sf_flag zSign, sf_int16 zExp, sf_bits64 zSig) {

    return ( ( (sf_bits64) zSign )<<63 ) + ( ( (sf_bits64) zExp )<<52 ) + zSig;

}

/*----------------------------------------------------------------------------
| Takes an abstract floating-point value having sign `zSign', exponent `zExp',
| and significand `zSig', and returns the proper double-precision floating-
| point value corresponding to the abstract input.  Ordinarily, the abstract
| value is simply rounded and packed into the double-precision format, with
| the inexact exception raised if the abstract input cannot be represented
| exactly.  However, if the abstract value is too large, the overflow and
| inexact exceptions are raised and an infinity or maximal finite value is
| returned.  If the abstract value is too small, the input value is rounded
| to a subnormal number, and the underflow and inexact exceptions are raised
| if the abstract input cannot be represented exactly as a subnormal double-
| precision floating-point number.
|     The input significand `zSig' has its binary point between bits 62
| and 61, which is 10 bits to the left of the usual location.  This shifted
| significand must be normalized or smaller.  If `zSig' is not normalized,
| `zExp' must be 0; in that case, the result returned is a subnormal number,
| and it must not require rounding.  In the usual case that `zSig' is
| normalized, `zExp' must be 1 less than the ``true'' floating-point exponent.
| The handling of underflow and overflow follows the IEC/IEEE Standard for
| Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static inline sf_float64 sf_roundAndPackFloat64(sf_flag zSign, sf_int16 zExp, sf_bits64 zSig) {
    sf_int8 roundingMode;
    sf_flag roundNearestEven;
    sf_int16 roundIncrement, roundBits;
    sf_flag isTiny;

    roundingMode = sf_float_rounding_mode;
    roundNearestEven = ( roundingMode == sf_float_round_nearest_even );
    roundIncrement = 0x200;
    if ( ! roundNearestEven ) {
        if ( roundingMode == sf_float_round_to_zero ) {
            roundIncrement = 0;
        }
        else {
            roundIncrement = 0x3FF;
            if ( zSign ) {
                if ( roundingMode == sf_float_round_up ) roundIncrement = 0;
            }
            else {
                if ( roundingMode == sf_float_round_down ) roundIncrement = 0;
            }
        }
    }
    roundBits = zSig & 0x3FF;
    if ( 0x7FD <= (sf_bits16) zExp ) {
        if (    ( 0x7FD < zExp )
             || (    ( zExp == 0x7FD )
                  && ( (sf_sbits64) ( zSig + (sf_uint64) roundIncrement ) < 0 ) )
           ) {
            sf_float_raise( sf_float_flag_overflow | sf_float_flag_inexact );
            return sf_packFloat64( zSign, 0x7FF, 0 ) - ( roundIncrement == 0 );
        }
        if ( zExp < 0 ) {
            isTiny =
                   ( sf_float_detect_tininess == sf_float_tininess_before_rounding )
                || ( zExp < -1 )
                || ( zSig + (sf_uint64) roundIncrement < SF_LIT64( 0x8000000000000000 ) );
            sf_shift64RightJamming( zSig, - zExp, &zSig );
            zExp = 0;
            roundBits = zSig & 0x3FF;
            if ( isTiny && roundBits ) sf_float_raise( sf_float_flag_underflow );
        }
    }
    if ( roundBits ) sf_float_exception_flags |= sf_float_flag_inexact;
    zSig = ( zSig + (sf_uint64) roundIncrement )>>10;
    zSig &= ~ (sf_bits64) ( ( ( roundBits ^ 0x200 ) == 0 ) & roundNearestEven );
    if ( zSig == 0 ) zExp = 0;
    return sf_packFloat64( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Takes an abstract floating-point value having sign `zSign', exponent `zExp',
| and significand `zSig', and returns the proper double-precision floating-
| point value corresponding to the abstract input.  This routine is just like
| `roundAndPackFloat64' except that `zSig' does not have to be normalized.
| Bit 63 of `zSig' must be zero, and `zExp' must be 1 less than the ``true''
| floating-point exponent.
*----------------------------------------------------------------------------*/

static inline sf_float64 sf_normalizeRoundAndPackFloat64(sf_flag zSign,
                                                         sf_int16 zExp,
                                                         sf_bits64 zSig)
{
    sf_int8 shiftCount;

    shiftCount = (sf_int8) (sf_countLeadingZeros64( zSig ) - 1);
    return sf_roundAndPackFloat64( zSign, zExp - shiftCount, zSig<<shiftCount );

}



/*----------------------------------------------------------------------------
| Returns the result of converting the 32-bit two's complement integer `a'
| to the single-precision floating-point format.  The conversion is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_int32_to_float32( sf_int32 a )
{
    sf_flag zSign;

    if ( a == 0 ) return 0;
    if ( a == (sf_sbits32) 0x80000000 ) return sf_packFloat32( 1, 0x9E, 0 );
    zSign = ( a < 0 );
    return sf_normalizeRoundAndPackFloat32( zSign, 0x9C, (sf_bits32) (zSign ? - a : a) );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the 32-bit two's complement integer `a'
| to the double-precision floating-point format.  The conversion is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_int32_to_float64( sf_int32 a )
{
    sf_flag zSign;
    sf_uint32 absA;
    sf_int8 shiftCount;
    sf_bits64 zSig;

    if ( a == 0 ) return 0;
    zSign = ( a < 0 );
    absA = (sf_uint32) (zSign ? - a : a);
    shiftCount = (sf_int8) (sf_countLeadingZeros32( (sf_bits32) absA ) + 21);
    zSig = absA;
    return sf_packFloat64( zSign, 0x432 - shiftCount, zSig<<shiftCount );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the 64-bit two's complement integer `a'
| to the single-precision floating-point format.  The conversion is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_int64_to_float32( sf_int64 a )
{
    sf_flag zSign;
    sf_uint64 absA;
    sf_int8 shiftCount;

    if ( a == 0 ) return 0;
    zSign = ( a < 0 );
    absA = (sf_uint64) (zSign ? - a : a);
    shiftCount = (sf_int8) (sf_countLeadingZeros64( absA ) - 40);
    if ( 0 <= shiftCount ) {
        return sf_packFloat32( zSign, 0x95 - shiftCount, (sf_bits32) (absA<<shiftCount) );
    }
    else {
        shiftCount = (sf_int8) (shiftCount + 7);
        if ( shiftCount < 0 ) {
            sf_shift64RightJamming( absA, - shiftCount, &absA );
        }
        else {
            absA <<= shiftCount;
        }
        return sf_roundAndPackFloat32( zSign, 0x9C - shiftCount, (sf_bits32) absA );
    }

}

/*----------------------------------------------------------------------------
| Returns the result of converting the 64-bit two's complement integer `a'
| to the double-precision floating-point format.  The conversion is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_int64_to_float64( sf_int64 a )
{
    sf_flag zSign;

    if ( a == 0 ) return 0;
    if ( a == (sf_sbits64) SF_LIT64( 0x8000000000000000 ) ) {
        return sf_packFloat64( 1, 0x43E, 0 );
    }
    zSign = ( a < 0 );
    return sf_normalizeRoundAndPackFloat64( zSign, 0x43C, (sf_bits64) (zSign ? - a : a) );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point value
| `a' to the 32-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic---which means in particular that the conversion is rounded
| according to the current rounding mode.  If `a' is a NaN, the largest
| positive integer is returned.  Otherwise, if the conversion overflows, the
| largest integer with the same sign as `a' is returned.
*----------------------------------------------------------------------------*/

sf_int32 sf_float32_to_int32( sf_float32 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits32 aSig;
    sf_bits64 aSig64;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    if ( ( aExp == 0xFF ) && aSig ) aSign = 0;
    if ( aExp ) aSig |= 0x00800000;
    shiftCount = 0xAF - aExp;
    aSig64 = aSig;
    aSig64 <<= 32;
    if ( 0 < shiftCount ) sf_shift64RightJamming( aSig64, shiftCount, &aSig64 );
    return sf_roundAndPackInt32( aSign, aSig64 );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point value
| `a' to the 32-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic, except that the conversion is always rounded toward zero.
| If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
| the conversion overflows, the largest integer with the same sign as `a' is
| returned.
*----------------------------------------------------------------------------*/

sf_int32 sf_float32_to_int32_round_to_zero( sf_float32 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits32 aSig;
    sf_int32 z;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    shiftCount = aExp - 0x9E;
    if ( 0 <= shiftCount ) {
        if ( a != 0xCF000000 ) {
            sf_float_raise( sf_float_flag_invalid );
            if ( ! aSign || ( ( aExp == 0xFF ) && aSig ) ) return 0x7FFFFFFF;
        }
        return (sf_sbits32) 0x80000000;
    }
    else if ( aExp <= 0x7E ) {
        if ( aExp | aSig ) sf_float_exception_flags |= sf_float_flag_inexact;
        return 0;
    }
    aSig = ( aSig | 0x00800000 )<<8;
    z = aSig>>( - shiftCount );
    if ( (sf_bits32) ( aSig<<( shiftCount & 31 ) ) ) {
        sf_float_exception_flags |= sf_float_flag_inexact;
    }
    if ( aSign ) z = - z;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point value
| `a' to the 64-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic---which means in particular that the conversion is rounded
| according to the current rounding mode.  If `a' is a NaN, the largest
| positive integer is returned.  Otherwise, if the conversion overflows, the
| largest integer with the same sign as `a' is returned.
*----------------------------------------------------------------------------*/

sf_int64 sf_float32_to_int64( sf_float32 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits32 aSig;
    sf_bits64 aSig64, aSigExtra;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    shiftCount = 0xBE - aExp;
    if ( shiftCount < 0 ) {
        sf_float_raise( sf_float_flag_invalid );
        if ( ! aSign || ( ( aExp == 0xFF ) && aSig ) ) {
            return SF_LIT64( 0x7FFFFFFFFFFFFFFF );
        }
        return (sf_sbits64) SF_LIT64( 0x8000000000000000 );
    }
    if ( aExp ) aSig |= 0x00800000;
    aSig64 = aSig;
    aSig64 <<= 40;
    sf_shift64ExtraRightJamming( aSig64, 0, shiftCount, &aSig64, &aSigExtra );
    return sf_roundAndPackInt64( aSign, aSig64, aSigExtra );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point value
| `a' to the 64-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic, except that the conversion is always rounded toward zero.  If
| `a' is a NaN, the largest positive integer is returned.  Otherwise, if the
| conversion overflows, the largest integer with the same sign as `a' is
| returned.
*----------------------------------------------------------------------------*/

sf_int64 sf_float32_to_int64_round_to_zero( sf_float32 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits32 aSig;
    sf_bits64 aSig64;
    sf_int64 z;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    shiftCount = aExp - 0xBE;
    if ( 0 <= shiftCount ) {
        if ( a != 0xDF000000 ) {
            sf_float_raise( sf_float_flag_invalid );
            if ( ! aSign || ( ( aExp == 0xFF ) && aSig ) ) {
                return SF_LIT64( 0x7FFFFFFFFFFFFFFF );
            }
        }
        return (sf_sbits64) SF_LIT64( 0x8000000000000000 );
    }
    else if ( aExp <= 0x7E ) {
        if ( aExp | aSig ) sf_float_exception_flags |= sf_float_flag_inexact;
        return 0;
    }
    aSig64 = aSig | 0x00800000;
    aSig64 <<= 40;
    z = (sf_int64) (aSig64>>( - shiftCount ));
    if ( (sf_bits64) ( aSig64<<( shiftCount & 63 ) ) ) {
        sf_float_exception_flags |= sf_float_flag_inexact;
    }
    if ( aSign ) z = - z;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of converting the single-precision floating-point value
| `a' to the double-precision floating-point format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float32_to_float64( sf_float32 a )
{
    sf_flag aSign;
    sf_int16 aExp;
    sf_bits32 aSig;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    if ( aExp == 0xFF ) {
        if ( aSig ) return sf_commonNaNToFloat64( sf_float32ToCommonNaN( a ) );
        return sf_packFloat64( aSign, 0x7FF, 0 );
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return sf_packFloat64( aSign, 0, 0 );
        sf_normalizeFloat32Subnormal( aSig, &aExp, &aSig );
        --aExp;
    }
    return sf_packFloat64( aSign, aExp + 0x380, ( (sf_bits64) aSig )<<29 );

}

/*----------------------------------------------------------------------------
| Rounds the single-precision floating-point value `a' to an integer, and
| returns the result as a single-precision floating-point value.  The
| operation is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float32_round_to_int( sf_float32 a )
{
    sf_flag aSign;
    sf_int16 aExp;
    sf_bits32 lastBitMask, roundBitsMask;
    sf_int8 roundingMode;
    sf_float32 z;

    aExp = sf_extractFloat32Exp( a );
    if ( 0x96 <= aExp ) {
        if ( ( aExp == 0xFF ) && sf_extractFloat32Frac( a ) ) {
            return sf_propagateFloat32NaN( a, a );
        }
        return a;
    }
    if ( aExp <= 0x7E ) {
        if ( (sf_bits32) ( a<<1 ) == 0 ) return a;
        sf_float_exception_flags |= sf_float_flag_inexact;
        aSign = sf_extractFloat32Sign( a );
        switch ( sf_float_rounding_mode ) {
         case sf_float_round_nearest_even:
            if ( ( aExp == 0x7E ) && sf_extractFloat32Frac( a ) ) {
                return sf_packFloat32( aSign, 0x7F, 0 );
            }
            break;
         case sf_float_round_down:
            return aSign ? 0xBF800000 : 0;
         case sf_float_round_up:
            return aSign ? 0x80000000 : 0x3F800000;
         default:
            break;
        }
        return sf_packFloat32( aSign, 0, 0 );
    }
    lastBitMask = 1;
    lastBitMask <<= 0x96 - aExp;
    roundBitsMask = lastBitMask - 1;
    z = a;
    roundingMode = sf_float_rounding_mode;
    if ( roundingMode == sf_float_round_nearest_even ) {
        z += lastBitMask>>1;
        if ( ( z & roundBitsMask ) == 0 ) z &= ~ lastBitMask;
    }
    else if ( roundingMode != sf_float_round_to_zero ) {
        if ( sf_extractFloat32Sign( z ) ^ ( roundingMode == sf_float_round_up ) ) {
            z += roundBitsMask;
        }
    }
    z &= ~ roundBitsMask;
    if ( z != a ) sf_float_exception_flags |= sf_float_flag_inexact;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of adding the absolute values of the single-precision
| floating-point values `a' and `b'.  If `zSign' is 1, the sum is negated
| before being returned.  `zSign' is ignored if the result is a NaN.
| The addition is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static sf_float32 sf_addFloat32Sigs( sf_float32 a, sf_float32 b, sf_flag zSign )
{
    sf_int16 aExp, bExp, zExp;
    sf_bits32 aSig, bSig, zSig;
    sf_int16 expDiff;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    bSig = sf_extractFloat32Frac( b );
    bExp = sf_extractFloat32Exp( b );
    expDiff = aExp - bExp;
    aSig <<= 6;
    bSig <<= 6;
    if ( 0 < expDiff ) {
        if ( aExp == 0xFF ) {
            if ( aSig ) return sf_propagateFloat32NaN( a, b );
            return a;
        }
        if ( bExp == 0 ) {
            --expDiff;
        }
        else {
            bSig |= 0x20000000;
        }
        sf_shift32RightJamming( bSig, expDiff, &bSig );
        zExp = aExp;
    }
    else if ( expDiff < 0 ) {
        if ( bExp == 0xFF ) {
            if ( bSig ) return sf_propagateFloat32NaN( a, b );
            return sf_packFloat32( zSign, 0xFF, 0 );
        }
        if ( aExp == 0 ) {
            ++expDiff;
        }
        else {
            aSig |= 0x20000000;
        }
        sf_shift32RightJamming( aSig, - expDiff, &aSig );
        zExp = bExp;
    }
    else {
        if ( aExp == 0xFF ) {
            if ( aSig | bSig ) return sf_propagateFloat32NaN( a, b );
            return a;
        }
        if ( aExp == 0 ) return sf_packFloat32( zSign, 0, ( aSig + bSig )>>6 );
        zSig = 0x40000000 + aSig + bSig;
        zExp = aExp;
        goto roundAndPack;
    }
    aSig |= 0x20000000;
    zSig = ( aSig + bSig )<<1;
    --zExp;
    if ( (sf_sbits32) zSig < 0 ) {
        zSig = aSig + bSig;
        ++zExp;
    }
 roundAndPack:
    return sf_roundAndPackFloat32( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns the result of subtracting the absolute values of the single-
| precision floating-point values `a' and `b'.  If `zSign' is 1, the
| difference is negated before being returned.  `zSign' is ignored if the
| result is a NaN.  The subtraction is performed according to the IEC/IEEE
| Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static sf_float32 sf_subFloat32Sigs( sf_float32 a, sf_float32 b, sf_flag zSign )
{
    sf_int16 aExp, bExp, zExp;
    sf_bits32 aSig, bSig, zSig;
    sf_int16 expDiff;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    bSig = sf_extractFloat32Frac( b );
    bExp = sf_extractFloat32Exp( b );
    expDiff = aExp - bExp;
    aSig <<= 7;
    bSig <<= 7;
    if ( 0 < expDiff ) goto aExpBigger;
    if ( expDiff < 0 ) goto bExpBigger;
    if ( aExp == 0xFF ) {
        if ( aSig | bSig ) return sf_propagateFloat32NaN( a, b );
        sf_float_raise( sf_float_flag_invalid );
        return sf_float32_default_nan;
    }
    if ( aExp == 0 ) {
        aExp = 1;
        bExp = 1;
    }
    if ( bSig < aSig ) goto aBigger;
    if ( aSig < bSig ) goto bBigger;
    return sf_packFloat32( sf_float_rounding_mode == sf_float_round_down, 0, 0 );
 bExpBigger:
    if ( bExp == 0xFF ) {
        if ( bSig ) return sf_propagateFloat32NaN( a, b );
        return sf_packFloat32( zSign ^ 1, 0xFF, 0 );
    }
    if ( aExp == 0 ) {
        ++expDiff;
    }
    else {
        aSig |= 0x40000000;
    }
    sf_shift32RightJamming( aSig, - expDiff, &aSig );
    bSig |= 0x40000000;
 bBigger:
    zSig = bSig - aSig;
    zExp = bExp;
    zSign ^= 1;
    goto normalizeRoundAndPack;
 aExpBigger:
    if ( aExp == 0xFF ) {
        if ( aSig ) return sf_propagateFloat32NaN( a, b );
        return a;
    }
    if ( bExp == 0 ) {
        --expDiff;
    }
    else {
        bSig |= 0x40000000;
    }
    sf_shift32RightJamming( bSig, expDiff, &bSig );
    aSig |= 0x40000000;
 aBigger:
    zSig = aSig - bSig;
    zExp = aExp;
 normalizeRoundAndPack:
    --zExp;
    return sf_normalizeRoundAndPackFloat32( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns the result of adding the single-precision floating-point values `a'
| and `b'.  The operation is performed according to the IEC/IEEE Standard for
| Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float32_add( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign;

    aSign = sf_extractFloat32Sign( a );
    bSign = sf_extractFloat32Sign( b );
    if ( aSign == bSign ) {
        return sf_addFloat32Sigs( a, b, aSign );
    }
    else {
        return sf_subFloat32Sigs( a, b, aSign );
    }

}

/*----------------------------------------------------------------------------
| Returns the result of subtracting the single-precision floating-point values
| `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
| for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float32_sub( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign;

    aSign = sf_extractFloat32Sign( a );
    bSign = sf_extractFloat32Sign( b );
    if ( aSign == bSign ) {
        return sf_subFloat32Sigs( a, b, aSign );
    }
    else {
        return sf_addFloat32Sigs( a, b, aSign );
    }

}

/*----------------------------------------------------------------------------
| Returns the result of multiplying the single-precision floating-point values
| `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
| for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float32_mul( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign, zSign;
    sf_int16 aExp, bExp, zExp;
    sf_bits32 aSig, bSig;
    sf_bits64 zSig64;
    sf_bits32 zSig;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    bSig = sf_extractFloat32Frac( b );
    bExp = sf_extractFloat32Exp( b );
    bSign = sf_extractFloat32Sign( b );
    zSign = aSign ^ bSign;
    if ( aExp == 0xFF ) {
        if ( aSig || ( ( bExp == 0xFF ) && bSig ) ) {
            return sf_propagateFloat32NaN( a, b );
        }
        if ( ( bExp | bSig ) == 0 ) {
            sf_float_raise( sf_float_flag_invalid );
            return sf_float32_default_nan;
        }
        return sf_packFloat32( zSign, 0xFF, 0 );
    }
    if ( bExp == 0xFF ) {
        if ( bSig ) return sf_propagateFloat32NaN( a, b );
        if ( ( aExp | aSig ) == 0 ) {
            sf_float_raise( sf_float_flag_invalid );
            return sf_float32_default_nan;
        }
        return sf_packFloat32( zSign, 0xFF, 0 );
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return sf_packFloat32( zSign, 0, 0 );
        sf_normalizeFloat32Subnormal( aSig, &aExp, &aSig );
    }
    if ( bExp == 0 ) {
        if ( bSig == 0 ) return sf_packFloat32( zSign, 0, 0 );
        sf_normalizeFloat32Subnormal( bSig, &bExp, &bSig );
    }
    zExp = aExp + bExp - 0x7F;
    aSig = ( aSig | 0x00800000 )<<7;
    bSig = ( bSig | 0x00800000 )<<8;
    sf_shift64RightJamming( ( (sf_bits64) aSig ) * bSig, 32, &zSig64 );
    zSig = (sf_bits32) zSig64;
    if ( 0 <= (sf_sbits32) ( zSig<<1 ) ) {
        zSig <<= 1;
        --zExp;
    }
    return sf_roundAndPackFloat32( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns the result of dividing the single-precision floating-point value `a'
| by the corresponding value `b'.  The operation is performed according to the
| IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float32_div( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign, zSign;
    sf_int16 aExp, bExp, zExp;
    sf_bits32 aSig, bSig, zSig;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    bSig = sf_extractFloat32Frac( b );
    bExp = sf_extractFloat32Exp( b );
    bSign = sf_extractFloat32Sign( b );
    zSign = aSign ^ bSign;
    if ( aExp == 0xFF ) {
        if ( aSig ) return sf_propagateFloat32NaN( a, b );
        if ( bExp == 0xFF ) {
            if ( bSig ) return sf_propagateFloat32NaN( a, b );
            sf_float_raise( sf_float_flag_invalid );
            return sf_float32_default_nan;
        }
        return sf_packFloat32( zSign, 0xFF, 0 );
    }
    if ( bExp == 0xFF ) {
        if ( bSig ) return sf_propagateFloat32NaN( a, b );
        return sf_packFloat32( zSign, 0, 0 );
    }
    if ( bExp == 0 ) {
        if ( bSig == 0 ) {
            if ( ( aExp | aSig ) == 0 ) {
                sf_float_raise( sf_float_flag_invalid );
                return sf_float32_default_nan;
            }
            sf_float_raise( sf_float_flag_divbyzero );
            return sf_packFloat32( zSign, 0xFF, 0 );
        }
        sf_normalizeFloat32Subnormal( bSig, &bExp, &bSig );
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return sf_packFloat32( zSign, 0, 0 );
        sf_normalizeFloat32Subnormal( aSig, &aExp, &aSig );
    }
    zExp = aExp - bExp + 0x7D;
    aSig = ( aSig | 0x00800000 )<<7;
    bSig = ( bSig | 0x00800000 )<<8;
    if ( bSig <= ( aSig + aSig ) ) {
        aSig >>= 1;
        ++zExp;
    }
    zSig = (sf_bits32) (( ( (sf_bits64) aSig )<<32 ) / bSig);
    if ( ( zSig & 0x3F ) == 0 ) {
        zSig |= ( (sf_bits64) bSig * zSig != ( (sf_bits64) aSig )<<32 );
    }
    return sf_roundAndPackFloat32( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns the remainder of the single-precision floating-point value `a'
| with respect to the corresponding value `b'.  The operation is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float32_rem( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, /* bSign, */ zSign;
    sf_int16 aExp, bExp, expDiff;
    sf_bits32 aSig, bSig;
    sf_bits32 q;
    sf_bits64 aSig64, bSig64, q64;
    sf_bits32 alternateASig;
    sf_sbits32 sigMean;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    bSig = sf_extractFloat32Frac( b );
    bExp = sf_extractFloat32Exp( b );
    /* bSign = sf_extractFloat32Sign( b ); */
    if ( aExp == 0xFF ) {
        if ( aSig || ( ( bExp == 0xFF ) && bSig ) ) {
            return sf_propagateFloat32NaN( a, b );
        }
        sf_float_raise( sf_float_flag_invalid );
        return sf_float32_default_nan;
    }
    if ( bExp == 0xFF ) {
        if ( bSig ) return sf_propagateFloat32NaN( a, b );
        return a;
    }
    if ( bExp == 0 ) {
        if ( bSig == 0 ) {
            sf_float_raise( sf_float_flag_invalid );
            return sf_float32_default_nan;
        }
        sf_normalizeFloat32Subnormal( bSig, &bExp, &bSig );
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return a;
        sf_normalizeFloat32Subnormal( aSig, &aExp, &aSig );
    }
    expDiff = aExp - bExp;
    aSig |= 0x00800000;
    bSig |= 0x00800000;
    if ( expDiff < 32 ) {
        aSig <<= 8;
        bSig <<= 8;
        if ( expDiff < 0 ) {
            if ( expDiff < -1 ) return a;
            aSig >>= 1;
        }
        q = ( bSig <= aSig );
        if ( q ) aSig -= bSig;
        if ( 0 < expDiff ) {
            q = (sf_bits32) (( ( (sf_bits64) aSig )<<32 ) / bSig);
            q >>= 32 - expDiff;
            bSig >>= 2;
            aSig = ( ( aSig>>1 )<<( expDiff - 1 ) ) - bSig * q;
        }
        else {
            aSig >>= 2;
            bSig >>= 2;
        }
    }
    else {
        if ( bSig <= aSig ) aSig -= bSig;
        aSig64 = ( (sf_bits64) aSig )<<40;
        bSig64 = ( (sf_bits64) bSig )<<40;
        expDiff -= 64;
        while ( 0 < expDiff ) {
            q64 = sf_estimateDiv128To64( aSig64, 0, bSig64 );
            q64 = ( 2 < q64 ) ? q64 - 2 : 0;
            aSig64 = - ( ( bSig * q64 )<<38 );
            expDiff -= 62;
        }
        expDiff += 64;
        q64 = sf_estimateDiv128To64( aSig64, 0, bSig64 );
        q64 = ( 2 < q64 ) ? q64 - 2 : 0;
        q = (sf_bits32) (q64>>( 64 - expDiff ));
        bSig <<= 6;
        aSig = (sf_bits32) (( ( aSig64>>33 )<<( expDiff - 1 ) ) - bSig * q);
    }
    do {
        alternateASig = aSig;
        ++q;
        aSig -= bSig;
    } while ( 0 <= (sf_sbits32) aSig );
    sigMean = (sf_sbits32) (aSig + alternateASig);
    if ( ( sigMean < 0 ) || ( ( sigMean == 0 ) && ( q & 1 ) ) ) {
        aSig = alternateASig;
    }
    zSign = ( (sf_sbits32) aSig < 0 );
    if ( zSign ) aSig = - aSig;
    return sf_normalizeRoundAndPackFloat32( aSign ^ zSign, bExp, aSig );

}

/*----------------------------------------------------------------------------
| Returns the square root of the single-precision floating-point value `a'.
| The operation is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float32_sqrt( sf_float32 a )
{
    sf_flag aSign;
    sf_int16 aExp, zExp;
    sf_bits32 aSig, zSig;
    sf_bits64 rem, term;

    aSig = sf_extractFloat32Frac( a );
    aExp = sf_extractFloat32Exp( a );
    aSign = sf_extractFloat32Sign( a );
    if ( aExp == 0xFF ) {
        if ( aSig ) return sf_propagateFloat32NaN( a, 0 );
        if ( ! aSign ) return a;
        sf_float_raise( sf_float_flag_invalid );
        return sf_float32_default_nan;
    }
    if ( aSign ) {
        if ( ( aExp | aSig ) == 0 ) return a;
        sf_float_raise( sf_float_flag_invalid );
        return sf_float32_default_nan;
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return 0;
        sf_normalizeFloat32Subnormal( aSig, &aExp, &aSig );
    }
    zExp = ( ( aExp - 0x7F )>>1 ) + 0x7E;
    aSig = ( aSig | 0x00800000 )<<8;
    zSig = sf_estimateSqrt32( aExp, aSig ) + 2;
    if ( ( zSig & 0x7F ) <= 5 ) {
        if ( zSig < 2 ) {
            zSig = 0x7FFFFFFF;
            goto roundAndPack;
        }
        aSig >>= aExp & 1;
        term = ( (sf_bits64) zSig ) * zSig;
        rem = ( ( (sf_bits64) aSig )<<32 ) - term;
        while ( (sf_sbits64) rem < 0 ) {
            --zSig;
            rem += ( ( (sf_bits64) zSig )<<1 ) | 1;
        }
        zSig |= ( rem != 0 );
    }
    sf_shift32RightJamming( zSig, 1, &zSig );
 roundAndPack:
    return sf_roundAndPackFloat32( 0, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is equal to
| the corresponding value `b', and 0 otherwise.  The comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_eq( sf_float32 a, sf_float32 b )
{

    if (    ( ( sf_extractFloat32Exp( a ) == 0xFF ) && sf_extractFloat32Frac( a ) )
         || ( ( sf_extractFloat32Exp( b ) == 0xFF ) && sf_extractFloat32Frac( b ) )
       ) {
        if ( sf_float32_is_signaling_nan_( a ) || sf_float32_is_signaling_nan_( b ) ) {
            sf_float_raise( sf_float_flag_invalid );
        }
        return 0;
    }
    return ( a == b ) || ( (sf_bits32) ( ( a | b )<<1 ) == 0 );

}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is less than
| or equal to the corresponding value `b', and 0 otherwise.  The comparison
| is performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_le( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat32Exp( a ) == 0xFF ) && sf_extractFloat32Frac( a ) )
         || ( ( sf_extractFloat32Exp( b ) == 0xFF ) && sf_extractFloat32Frac( b ) )
       ) {
        sf_float_raise( sf_float_flag_invalid );
        return 0;
    }
    aSign = sf_extractFloat32Sign( a );
    bSign = sf_extractFloat32Sign( b );
    if ( aSign != bSign ) return aSign || ( (sf_bits32) ( ( a | b )<<1 ) == 0 );
    return ( a == b ) || ( aSign ^ ( a < b ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is less than
| the corresponding value `b', and 0 otherwise.  The comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_lt( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat32Exp( a ) == 0xFF ) && sf_extractFloat32Frac( a ) )
         || ( ( sf_extractFloat32Exp( b ) == 0xFF ) && sf_extractFloat32Frac( b ) )
       ) {
        sf_float_raise( sf_float_flag_invalid );
        return 0;
    }
    aSign = sf_extractFloat32Sign( a );
    bSign = sf_extractFloat32Sign( b );
    if ( aSign != bSign ) return aSign && ( (sf_bits32) ( ( a | b )<<1 ) != 0 );
    return ( a != b ) && ( aSign ^ ( a < b ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is equal to
| the corresponding value `b', and 0 otherwise.  The invalid exception is
| raised if either operand is a NaN.  Otherwise, the comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_eq_signaling( sf_float32 a, sf_float32 b )
{

    if (    ( ( sf_extractFloat32Exp( a ) == 0xFF ) && sf_extractFloat32Frac( a ) )
         || ( ( sf_extractFloat32Exp( b ) == 0xFF ) && sf_extractFloat32Frac( b ) )
       ) {
        sf_float_raise( sf_float_flag_invalid );
        return 0;
    }
    return ( a == b ) || ( (sf_bits32) ( ( a | b )<<1 ) == 0 );

}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is less than or
| equal to the corresponding value `b', and 0 otherwise.  Quiet NaNs do not
| cause an exception.  Otherwise, the comparison is performed according to the
| IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_le_quiet( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat32Exp( a ) == 0xFF ) && sf_extractFloat32Frac( a ) )
         || ( ( sf_extractFloat32Exp( b ) == 0xFF ) && sf_extractFloat32Frac( b ) )
       ) {
        if ( sf_float32_is_signaling_nan_( a ) || sf_float32_is_signaling_nan_( b ) ) {
            sf_float_raise( sf_float_flag_invalid );
        }
        return 0;
    }
    aSign = sf_extractFloat32Sign( a );
    bSign = sf_extractFloat32Sign( b );
    if ( aSign != bSign ) return aSign || ( (sf_bits32) ( ( a | b )<<1 ) == 0 );
    return ( a == b ) || ( aSign ^ ( a < b ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the single-precision floating-point value `a' is less than
| the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
| exception.  Otherwise, the comparison is performed according to the IEC/IEEE
| Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float32_lt_quiet( sf_float32 a, sf_float32 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat32Exp( a ) == 0xFF ) && sf_extractFloat32Frac( a ) )
         || ( ( sf_extractFloat32Exp( b ) == 0xFF ) && sf_extractFloat32Frac( b ) )
       ) {
        if ( sf_float32_is_signaling_nan_( a ) || sf_float32_is_signaling_nan_( b ) ) {
            sf_float_raise( sf_float_flag_invalid );
        }
        return 0;
    }
    aSign = sf_extractFloat32Sign( a );
    bSign = sf_extractFloat32Sign( b );
    if ( aSign != bSign ) return aSign && ( (sf_bits32) ( ( a | b )<<1 ) != 0 );
    return ( a != b ) && ( aSign ^ ( a < b ) );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point value
| `a' to the 32-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic---which means in particular that the conversion is rounded
| according to the current rounding mode.  If `a' is a NaN, the largest
| positive integer is returned.  Otherwise, if the conversion overflows, the
| largest integer with the same sign as `a' is returned.
*----------------------------------------------------------------------------*/

sf_int32 sf_float64_to_int32( sf_float64 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits64 aSig;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    if ( ( aExp == 0x7FF ) && aSig ) aSign = 0;
    if ( aExp ) aSig |= SF_LIT64( 0x0010000000000000 );
    shiftCount = 0x42C - aExp;
    if ( 0 < shiftCount ) sf_shift64RightJamming( aSig, shiftCount, &aSig );
    return sf_roundAndPackInt32( aSign, aSig );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point value
| `a' to the 32-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic, except that the conversion is always rounded toward zero.
| If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
| the conversion overflows, the largest integer with the same sign as `a' is
| returned.
*----------------------------------------------------------------------------*/

sf_int32 sf_float64_to_int32_round_to_zero( sf_float64 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits64 aSig, savedASig;
    sf_int32 z;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    if ( 0x41E < aExp ) {
        if ( ( aExp == 0x7FF ) && aSig ) aSign = 0;
        goto invalid;
    }
    else if ( aExp < 0x3FF ) {
        if ( aExp || aSig ) sf_float_exception_flags |= sf_float_flag_inexact;
        return 0;
    }
    aSig |= SF_LIT64( 0x0010000000000000 );
    shiftCount = 0x433 - aExp;
    savedASig = aSig;
    aSig >>= shiftCount;
    z = (sf_int32) aSig;
    if ( aSign ) z = - z;
    if ( ( z < 0 ) ^ aSign ) {
 invalid:
        sf_float_raise( sf_float_flag_invalid );
        return aSign ? (sf_sbits32) 0x80000000 : 0x7FFFFFFF;
    }
    if ( ( aSig<<shiftCount ) != savedASig ) {
        sf_float_exception_flags |= sf_float_flag_inexact;
    }
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point value
| `a' to the 64-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic---which means in particular that the conversion is rounded
| according to the current rounding mode.  If `a' is a NaN, the largest
| positive integer is returned.  Otherwise, if the conversion overflows, the
| largest integer with the same sign as `a' is returned.
*----------------------------------------------------------------------------*/

sf_int64 sf_float64_to_int64( sf_float64 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits64 aSig, aSigExtra;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    if ( aExp ) aSig |= SF_LIT64( 0x0010000000000000 );
    shiftCount = 0x433 - aExp;
    if ( shiftCount <= 0 ) {
        if ( 0x43E < aExp ) {
            sf_float_raise( sf_float_flag_invalid );
            if (    ! aSign
                 || (    ( aExp == 0x7FF )
                      && ( aSig != SF_LIT64( 0x0010000000000000 ) ) )
               ) {
                return SF_LIT64( 0x7FFFFFFFFFFFFFFF );
            }
            return (sf_sbits64) SF_LIT64( 0x8000000000000000 );
        }
        aSigExtra = 0;
        aSig <<= - shiftCount;
    }
    else {
        sf_shift64ExtraRightJamming( aSig, 0, shiftCount, &aSig, &aSigExtra );
    }
    return sf_roundAndPackInt64( aSign, aSig, aSigExtra );

}

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point value
| `a' to the 64-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic, except that the conversion is always rounded toward zero.
| If `a' is a NaN, the largest positive integer is returned.  Otherwise, if
| the conversion overflows, the largest integer with the same sign as `a' is
| returned.
*----------------------------------------------------------------------------*/

sf_int64 sf_float64_to_int64_round_to_zero( sf_float64 a )
{
    sf_flag aSign;
    sf_int16 aExp, shiftCount;
    sf_bits64 aSig;
    sf_int64 z;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    if ( aExp ) aSig |= SF_LIT64( 0x0010000000000000 );
    shiftCount = aExp - 0x433;
    if ( 0 <= shiftCount ) {
        if ( 0x43E <= aExp ) {
            if ( a != SF_LIT64( 0xC3E0000000000000 ) ) {
                sf_float_raise( sf_float_flag_invalid );
                if (    ! aSign
                     || (    ( aExp == 0x7FF )
                          && ( aSig != SF_LIT64( 0x0010000000000000 ) ) )
                   ) {
                    return SF_LIT64( 0x7FFFFFFFFFFFFFFF );
                }
            }
            return (sf_sbits64) SF_LIT64( 0x8000000000000000 );
        }
        z = (sf_int64) (aSig<<shiftCount);
    }
    else {
        if ( aExp < 0x3FE ) {
            if ( ((sf_bits64) aExp) | aSig ) sf_float_exception_flags |= sf_float_flag_inexact;
            return 0;
        }
        z = (sf_int64) (aSig>>( - shiftCount ));
        if ( (sf_bits64) ( aSig<<( shiftCount & 63 ) ) ) {
            sf_float_exception_flags |= sf_float_flag_inexact;
        }
    }
    if ( aSign ) z = - z;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point value
| `a' to the single-precision floating-point format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic.
*----------------------------------------------------------------------------*/

sf_float32 sf_float64_to_float32( sf_float64 a )
{
    sf_flag aSign;
    sf_int16 aExp;
    sf_bits64 aSig;
    sf_bits32 zSig;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    if ( aExp == 0x7FF ) {
        if ( aSig ) return sf_commonNaNToFloat32( sf_float64ToCommonNaN( a ) );
        return sf_packFloat32( aSign, 0xFF, 0 );
    }
    sf_shift64RightJamming( aSig, 22, &aSig );
    zSig = (sf_bits32) aSig;
    if ( aExp || zSig ) {
        zSig |= 0x40000000;
        aExp -= 0x381;
    }
    return sf_roundAndPackFloat32( aSign, aExp, zSig );

}

/*----------------------------------------------------------------------------
| Rounds the double-precision floating-point value `a' to an integer, and
| returns the result as a double-precision floating-point value.  The
| operation is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float64_round_to_int( sf_float64 a )
{
    sf_flag aSign;
    sf_int16 aExp;
    sf_bits64 lastBitMask, roundBitsMask;
    sf_int8 roundingMode;
    sf_float64 z;

    aExp = sf_extractFloat64Exp( a );
    if ( 0x433 <= aExp ) {
        if ( ( aExp == 0x7FF ) && sf_extractFloat64Frac( a ) ) {
            return sf_propagateFloat64NaN( a, a );
        }
        return a;
    }
    if ( aExp < 0x3FF ) {
        if ( (sf_bits64) ( a<<1 ) == 0 ) return a;
        sf_float_exception_flags |= sf_float_flag_inexact;
        aSign = sf_extractFloat64Sign( a );
        switch ( sf_float_rounding_mode ) {
         case sf_float_round_nearest_even:
            if ( ( aExp == 0x3FE ) && sf_extractFloat64Frac( a ) ) {
                return sf_packFloat64( aSign, 0x3FF, 0 );
            }
            break;
         case sf_float_round_down:
            return aSign ? SF_LIT64( 0xBFF0000000000000 ) : 0;
         case sf_float_round_up:
            return
            aSign ? SF_LIT64( 0x8000000000000000 ) : SF_LIT64( 0x3FF0000000000000 );
         default:
            break;
        }
        return sf_packFloat64( aSign, 0, 0 );
    }
    lastBitMask = 1;
    lastBitMask <<= 0x433 - aExp;
    roundBitsMask = lastBitMask - 1;
    z = a;
    roundingMode = sf_float_rounding_mode;
    if ( roundingMode == sf_float_round_nearest_even ) {
        z += lastBitMask>>1;
        if ( ( z & roundBitsMask ) == 0 ) z &= ~ lastBitMask;
    }
    else if ( roundingMode != sf_float_round_to_zero ) {
        if ( sf_extractFloat64Sign( z ) ^ ( roundingMode == sf_float_round_up ) ) {
            z += roundBitsMask;
        }
    }
    z &= ~ roundBitsMask;
    if ( z != a ) sf_float_exception_flags |= sf_float_flag_inexact;
    return z;

}

/*----------------------------------------------------------------------------
| Returns the result of adding the absolute values of the double-precision
| floating-point values `a' and `b'.  If `zSign' is 1, the sum is negated
| before being returned.  `zSign' is ignored if the result is a NaN.
| The addition is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static sf_float64 sf_addFloat64Sigs( sf_float64 a, sf_float64 b, sf_flag zSign )
{
    sf_int16 aExp, bExp, zExp;
    sf_bits64 aSig, bSig, zSig;
    sf_int16 expDiff;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    bSig = sf_extractFloat64Frac( b );
    bExp = sf_extractFloat64Exp( b );
    expDiff = aExp - bExp;
    aSig <<= 9;
    bSig <<= 9;
    if ( 0 < expDiff ) {
        if ( aExp == 0x7FF ) {
            if ( aSig ) return sf_propagateFloat64NaN( a, b );
            return a;
        }
        if ( bExp == 0 ) {
            --expDiff;
        }
        else {
            bSig |= SF_LIT64( 0x2000000000000000 );
        }
        sf_shift64RightJamming( bSig, expDiff, &bSig );
        zExp = aExp;
    }
    else if ( expDiff < 0 ) {
        if ( bExp == 0x7FF ) {
            if ( bSig ) return sf_propagateFloat64NaN( a, b );
            return sf_packFloat64( zSign, 0x7FF, 0 );
        }
        if ( aExp == 0 ) {
            ++expDiff;
        }
        else {
            aSig |= SF_LIT64( 0x2000000000000000 );
        }
        sf_shift64RightJamming( aSig, - expDiff, &aSig );
        zExp = bExp;
    }
    else {
        if ( aExp == 0x7FF ) {
            if ( aSig | bSig ) return sf_propagateFloat64NaN( a, b );
            return a;
        }
        if ( aExp == 0 ) return sf_packFloat64( zSign, 0, ( aSig + bSig )>>9 );
        zSig = SF_LIT64( 0x4000000000000000 ) + aSig + bSig;
        zExp = aExp;
        goto roundAndPack;
    }
    aSig |= SF_LIT64( 0x2000000000000000 );
    zSig = ( aSig + bSig )<<1;
    --zExp;
    if ( (sf_sbits64) zSig < 0 ) {
        zSig = aSig + bSig;
        ++zExp;
    }
 roundAndPack:
    return sf_roundAndPackFloat64( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns the result of subtracting the absolute values of the double-
| precision floating-point values `a' and `b'.  If `zSign' is 1, the
| difference is negated before being returned.  `zSign' is ignored if the
| result is a NaN.  The subtraction is performed according to the IEC/IEEE
| Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static sf_float64 sf_subFloat64Sigs( sf_float64 a, sf_float64 b, sf_flag zSign )
{
    sf_int16 aExp, bExp, zExp;
    sf_bits64 aSig, bSig, zSig;
    sf_int16 expDiff;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    bSig = sf_extractFloat64Frac( b );
    bExp = sf_extractFloat64Exp( b );
    expDiff = aExp - bExp;
    aSig <<= 10;
    bSig <<= 10;
    if ( 0 < expDiff ) goto aExpBigger;
    if ( expDiff < 0 ) goto bExpBigger;
    if ( aExp == 0x7FF ) {
        if ( aSig | bSig ) return sf_propagateFloat64NaN( a, b );
        sf_float_raise( sf_float_flag_invalid );
        return sf_float64_default_nan;
    }
    if ( aExp == 0 ) {
        aExp = 1;
        bExp = 1;
    }
    if ( bSig < aSig ) goto aBigger;
    if ( aSig < bSig ) goto bBigger;
    return sf_packFloat64( sf_float_rounding_mode == sf_float_round_down, 0, 0 );
 bExpBigger:
    if ( bExp == 0x7FF ) {
        if ( bSig ) return sf_propagateFloat64NaN( a, b );
        return sf_packFloat64( zSign ^ 1, 0x7FF, 0 );
    }
    if ( aExp == 0 ) {
        ++expDiff;
    }
    else {
        aSig |= SF_LIT64( 0x4000000000000000 );
    }
    sf_shift64RightJamming( aSig, - expDiff, &aSig );
    bSig |= SF_LIT64( 0x4000000000000000 );
 bBigger:
    zSig = bSig - aSig;
    zExp = bExp;
    zSign ^= 1;
    goto normalizeRoundAndPack;
 aExpBigger:
    if ( aExp == 0x7FF ) {
        if ( aSig ) return sf_propagateFloat64NaN( a, b );
        return a;
    }
    if ( bExp == 0 ) {
        --expDiff;
    }
    else {
        bSig |= SF_LIT64( 0x4000000000000000 );
    }
    sf_shift64RightJamming( bSig, expDiff, &bSig );
    aSig |= SF_LIT64( 0x4000000000000000 );
 aBigger:
    zSig = aSig - bSig;
    zExp = aExp;
 normalizeRoundAndPack:
    --zExp;
    return sf_normalizeRoundAndPackFloat64( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns the result of adding the double-precision floating-point values `a'
| and `b'.  The operation is performed according to the IEC/IEEE Standard for
| Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float64_add( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign;

    aSign = sf_extractFloat64Sign( a );
    bSign = sf_extractFloat64Sign( b );
    if ( aSign == bSign ) {
        return sf_addFloat64Sigs( a, b, aSign );
    }
    else {
        return sf_subFloat64Sigs( a, b, aSign );
    }

}

/*----------------------------------------------------------------------------
| Returns the result of subtracting the double-precision floating-point values
| `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
| for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float64_sub( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign;

    aSign = sf_extractFloat64Sign( a );
    bSign = sf_extractFloat64Sign( b );
    if ( aSign == bSign ) {
        return sf_subFloat64Sigs( a, b, aSign );
    }
    else {
        return sf_addFloat64Sigs( a, b, aSign );
    }

}

/*----------------------------------------------------------------------------
| Returns the result of multiplying the double-precision floating-point values
| `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
| for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float64_mul( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign, zSign;
    sf_int16 aExp, bExp, zExp;
    sf_bits64 aSig, bSig, zSig0, zSig1;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    bSig = sf_extractFloat64Frac( b );
    bExp = sf_extractFloat64Exp( b );
    bSign = sf_extractFloat64Sign( b );
    zSign = aSign ^ bSign;
    if ( aExp == 0x7FF ) {
        if ( aSig || ( ( bExp == 0x7FF ) && bSig ) ) {
            return sf_propagateFloat64NaN( a, b );
        }
        if ( ( ((sf_bits64) bExp) | bSig ) == 0 ) {
            sf_float_raise( sf_float_flag_invalid );
            return sf_float64_default_nan;
        }
        return sf_packFloat64( zSign, 0x7FF, 0 );
    }
    if ( bExp == 0x7FF ) {
        if ( bSig ) return sf_propagateFloat64NaN( a, b );
        if ( ( ((sf_bits64) aExp) | aSig ) == 0 ) {
            sf_float_raise( sf_float_flag_invalid );
            return sf_float64_default_nan;
        }
        return sf_packFloat64( zSign, 0x7FF, 0 );
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return sf_packFloat64( zSign, 0, 0 );
        sf_normalizeFloat64Subnormal( aSig, &aExp, &aSig );
    }
    if ( bExp == 0 ) {
        if ( bSig == 0 ) return sf_packFloat64( zSign, 0, 0 );
        sf_normalizeFloat64Subnormal( bSig, &bExp, &bSig );
    }
    zExp = aExp + bExp - 0x3FF;
    aSig = ( aSig | SF_LIT64( 0x0010000000000000 ) )<<10;
    bSig = ( bSig | SF_LIT64( 0x0010000000000000 ) )<<11;
    sf_mul64To128( aSig, bSig, &zSig0, &zSig1 );
    zSig0 |= ( zSig1 != 0 );
    if ( 0 <= (sf_sbits64) ( zSig0<<1 ) ) {
        zSig0 <<= 1;
        --zExp;
    }
    return sf_roundAndPackFloat64( zSign, zExp, zSig0 );

}

/*----------------------------------------------------------------------------
| Returns the result of dividing the double-precision floating-point value `a'
| by the corresponding value `b'.  The operation is performed according to
| the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float64_div( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign, zSign;
    sf_int16 aExp, bExp, zExp;
    sf_bits64 aSig, bSig, zSig;
    sf_bits64 rem0, rem1;
    sf_bits64 term0, term1;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    bSig = sf_extractFloat64Frac( b );
    bExp = sf_extractFloat64Exp( b );
    bSign = sf_extractFloat64Sign( b );
    zSign = aSign ^ bSign;
    if ( aExp == 0x7FF ) {
        if ( aSig ) return sf_propagateFloat64NaN( a, b );
        if ( bExp == 0x7FF ) {
            if ( bSig ) return sf_propagateFloat64NaN( a, b );
            sf_float_raise( sf_float_flag_invalid );
            return sf_float64_default_nan;
        }
        return sf_packFloat64( zSign, 0x7FF, 0 );
    }
    if ( bExp == 0x7FF ) {
        if ( bSig ) return sf_propagateFloat64NaN( a, b );
        return sf_packFloat64( zSign, 0, 0 );
    }
    if ( bExp == 0 ) {
        if ( bSig == 0 ) {
            if ( ( ((sf_bits64) aExp) | aSig ) == 0 ) {
                sf_float_raise( sf_float_flag_invalid );
                return sf_float64_default_nan;
            }
            sf_float_raise( sf_float_flag_divbyzero );
            return sf_packFloat64( zSign, 0x7FF, 0 );
        }
        sf_normalizeFloat64Subnormal( bSig, &bExp, &bSig );
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return sf_packFloat64( zSign, 0, 0 );
        sf_normalizeFloat64Subnormal( aSig, &aExp, &aSig );
    }
    zExp = aExp - bExp + 0x3FD;
    aSig = ( aSig | SF_LIT64( 0x0010000000000000 ) )<<10;
    bSig = ( bSig | SF_LIT64( 0x0010000000000000 ) )<<11;
    if ( bSig <= ( aSig + aSig ) ) {
        aSig >>= 1;
        ++zExp;
    }
    zSig = sf_estimateDiv128To64( aSig, 0, bSig );
    if ( ( zSig & 0x1FF ) <= 2 ) {
        sf_mul64To128( bSig, zSig, &term0, &term1 );
        sf_sub128( aSig, 0, term0, term1, &rem0, &rem1 );
        while ( (sf_sbits64) rem0 < 0 ) {
            --zSig;
            sf_add128( rem0, rem1, 0, bSig, &rem0, &rem1 );
        }
        zSig |= ( rem1 != 0 );
    }
    return sf_roundAndPackFloat64( zSign, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns the remainder of the double-precision floating-point value `a'
| with respect to the corresponding value `b'.  The operation is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float64_rem( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, /* bSign, */ zSign;
    sf_int16 aExp, bExp, expDiff;
    sf_bits64 aSig, bSig;
    sf_bits64 q, alternateASig;
    sf_sbits64 sigMean;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    bSig = sf_extractFloat64Frac( b );
    bExp = sf_extractFloat64Exp( b );
    /* bSign = sf_extractFloat64Sign( b ); */
    if ( aExp == 0x7FF ) {
        if ( aSig || ( ( bExp == 0x7FF ) && bSig ) ) {
            return sf_propagateFloat64NaN( a, b );
        }
        sf_float_raise( sf_float_flag_invalid );
        return sf_float64_default_nan;
    }
    if ( bExp == 0x7FF ) {
        if ( bSig ) return sf_propagateFloat64NaN( a, b );
        return a;
    }
    if ( bExp == 0 ) {
        if ( bSig == 0 ) {
            sf_float_raise( sf_float_flag_invalid );
            return sf_float64_default_nan;
        }
        sf_normalizeFloat64Subnormal( bSig, &bExp, &bSig );
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return a;
        sf_normalizeFloat64Subnormal( aSig, &aExp, &aSig );
    }
    expDiff = aExp - bExp;
    aSig = ( aSig | SF_LIT64( 0x0010000000000000 ) )<<11;
    bSig = ( bSig | SF_LIT64( 0x0010000000000000 ) )<<11;
    if ( expDiff < 0 ) {
        if ( expDiff < -1 ) return a;
        aSig >>= 1;
    }
    q = ( bSig <= aSig );
    if ( q ) aSig -= bSig;
    expDiff -= 64;
    while ( 0 < expDiff ) {
        q = sf_estimateDiv128To64( aSig, 0, bSig );
        q = ( 2 < q ) ? q - 2 : 0;
        aSig = - ( ( bSig>>2 ) * q );
        expDiff -= 62;
    }
    expDiff += 64;
    if ( 0 < expDiff ) {
        q = sf_estimateDiv128To64( aSig, 0, bSig );
        q = ( 2 < q ) ? q - 2 : 0;
        q >>= 64 - expDiff;
        bSig >>= 2;
        aSig = ( ( aSig>>1 )<<( expDiff - 1 ) ) - bSig * q;
    }
    else {
        aSig >>= 2;
        bSig >>= 2;
    }
    do {
        alternateASig = aSig;
        ++q;
        aSig -= bSig;
    } while ( 0 <= (sf_sbits64) aSig );
    sigMean = (sf_sbits64) (aSig + alternateASig);
    if ( ( sigMean < 0 ) || ( ( sigMean == 0 ) && ( q & 1 ) ) ) {
        aSig = alternateASig;
    }
    zSign = ( (sf_sbits64) aSig < 0 );
    if ( zSign ) aSig = - aSig;
    return sf_normalizeRoundAndPackFloat64( aSign ^ zSign, bExp, aSig );

}

/*----------------------------------------------------------------------------
| Returns the square root of the double-precision floating-point value `a'.
| The operation is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_float64 sf_float64_sqrt( sf_float64 a )
{
    sf_flag aSign;
    sf_int16 aExp, zExp;
    sf_bits64 aSig, zSig, doubleZSig;
    sf_bits64 rem0, rem1, term0, term1;

    aSig = sf_extractFloat64Frac( a );
    aExp = sf_extractFloat64Exp( a );
    aSign = sf_extractFloat64Sign( a );
    if ( aExp == 0x7FF ) {
        if ( aSig ) return sf_propagateFloat64NaN( a, a );
        if ( ! aSign ) return a;
        sf_float_raise( sf_float_flag_invalid );
        return sf_float64_default_nan;
    }
    if ( aSign ) {
        if ( ( ((sf_bits64) aExp) | aSig ) == 0 ) return a;
        sf_float_raise( sf_float_flag_invalid );
        return sf_float64_default_nan;
    }
    if ( aExp == 0 ) {
        if ( aSig == 0 ) return 0;
        sf_normalizeFloat64Subnormal( aSig, &aExp, &aSig );
    }
    zExp = ( ( aExp - 0x3FF )>>1 ) + 0x3FE;
    aSig |= SF_LIT64( 0x0010000000000000 );
    zSig = sf_estimateSqrt32( aExp, (sf_bits32) (aSig>>21) );
    aSig <<= 9 - ( aExp & 1 );
    zSig = sf_estimateDiv128To64( aSig, 0, zSig<<32 ) + ( zSig<<30 );
    if ( ( zSig & 0x1FF ) <= 5 ) {
        doubleZSig = zSig<<1;
        sf_mul64To128( zSig, zSig, &term0, &term1 );
        sf_sub128( aSig, 0, term0, term1, &rem0, &rem1 );
        while ( (sf_sbits64) rem0 < 0 ) {
            --zSig;
            doubleZSig -= 2;
            sf_add128( rem0, rem1, zSig>>63, doubleZSig | 1, &rem0, &rem1 );
        }
        zSig |= ( ( rem0 | rem1 ) != 0 );
    }
    return sf_roundAndPackFloat64( 0, zExp, zSig );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is equal to the
| corresponding value `b', and 0 otherwise.  The comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_eq( sf_float64 a, sf_float64 b )
{

    if (    ( ( sf_extractFloat64Exp( a ) == 0x7FF ) && sf_extractFloat64Frac( a ) )
         || ( ( sf_extractFloat64Exp( b ) == 0x7FF ) && sf_extractFloat64Frac( b ) )
       ) {
        if ( sf_float64_is_signaling_nan_( a ) || sf_float64_is_signaling_nan_( b ) ) {
            sf_float_raise( sf_float_flag_invalid );
        }
        return 0;
    }
    return ( a == b ) || ( (sf_bits64) ( ( a | b )<<1 ) == 0 );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is less than or
| equal to the corresponding value `b', and 0 otherwise.  The comparison is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_le( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat64Exp( a ) == 0x7FF ) && sf_extractFloat64Frac( a ) )
         || ( ( sf_extractFloat64Exp( b ) == 0x7FF ) && sf_extractFloat64Frac( b ) )
       ) {
        sf_float_raise( sf_float_flag_invalid );
        return 0;
    }
    aSign = sf_extractFloat64Sign( a );
    bSign = sf_extractFloat64Sign( b );
    if ( aSign != bSign ) return aSign || ( (sf_bits64) ( ( a | b )<<1 ) == 0 );
    return ( a == b ) || ( aSign ^ ( a < b ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is less than
| the corresponding value `b', and 0 otherwise.  The comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_lt( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat64Exp( a ) == 0x7FF ) && sf_extractFloat64Frac( a ) )
         || ( ( sf_extractFloat64Exp( b ) == 0x7FF ) && sf_extractFloat64Frac( b ) )
       ) {
        sf_float_raise( sf_float_flag_invalid );
        return 0;
    }
    aSign = sf_extractFloat64Sign( a );
    bSign = sf_extractFloat64Sign( b );
    if ( aSign != bSign ) return aSign && ( (sf_bits64) ( ( a | b )<<1 ) != 0 );
    return ( a != b ) && ( aSign ^ ( a < b ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is equal to the
| corresponding value `b', and 0 otherwise.  The invalid exception is raised
| if either operand is a NaN.  Otherwise, the comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_eq_signaling( sf_float64 a, sf_float64 b )
{

    if (    ( ( sf_extractFloat64Exp( a ) == 0x7FF ) && sf_extractFloat64Frac( a ) )
         || ( ( sf_extractFloat64Exp( b ) == 0x7FF ) && sf_extractFloat64Frac( b ) )
       ) {
        sf_float_raise( sf_float_flag_invalid );
        return 0;
    }
    return ( a == b ) || ( (sf_bits64) ( ( a | b )<<1 ) == 0 );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is less than or
| equal to the corresponding value `b', and 0 otherwise.  Quiet NaNs do not
| cause an exception.  Otherwise, the comparison is performed according to the
| IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_le_quiet( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat64Exp( a ) == 0x7FF ) && sf_extractFloat64Frac( a ) )
         || ( ( sf_extractFloat64Exp( b ) == 0x7FF ) && sf_extractFloat64Frac( b ) )
       ) {
        if ( sf_float64_is_signaling_nan_( a ) || sf_float64_is_signaling_nan_( b ) ) {
            sf_float_raise( sf_float_flag_invalid );
        }
        return 0;
    }
    aSign = sf_extractFloat64Sign( a );
    bSign = sf_extractFloat64Sign( b );
    if ( aSign != bSign ) return aSign || ( (sf_bits64) ( ( a | b )<<1 ) == 0 );
    return ( a == b ) || ( aSign ^ ( a < b ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is less than
| the corresponding value `b', and 0 otherwise.  Quiet NaNs do not cause an
| exception.  Otherwise, the comparison is performed according to the IEC/IEEE
| Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

sf_flag sf_float64_lt_quiet( sf_float64 a, sf_float64 b )
{
    sf_flag aSign, bSign;

    if (    ( ( sf_extractFloat64Exp( a ) == 0x7FF ) && sf_extractFloat64Frac( a ) )
         || ( ( sf_extractFloat64Exp( b ) == 0x7FF ) && sf_extractFloat64Frac( b ) )
       ) {
        if ( sf_float64_is_signaling_nan_( a ) || sf_float64_is_signaling_nan_( b ) ) {
            sf_float_raise( sf_float_flag_invalid );
        }
        return 0;
    }
    aSign = sf_extractFloat64Sign( a );
    bSign = sf_extractFloat64Sign( b );
    if ( aSign != bSign ) return aSign && ( (sf_bits64) ( ( a | b )<<1 ) != 0 );
    return ( a != b ) && ( aSign ^ ( a < b ) );

}
