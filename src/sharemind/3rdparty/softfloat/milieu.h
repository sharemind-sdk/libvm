/*
 * This file is derivative of part of the SoftFloat IEC/IEEE
 * Floating-point Arithmetic Package, Release 2b.
*/

/*============================================================================

This C header file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
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

#ifndef SHAREMIND_SOFTFLOAT_MILIEU_H
#define SHAREMIND_SOFTFLOAT_MILIEU_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
| Each of the following `typedef's defines the most convenient type that holds
| integers of at least as many bits as specified.  For example, `uint8' should
| be the most convenient type that can hold unsigned integers of as many as
| 8 bits.  The `flag' type must be able to hold either a 0 or 1.  For most
| implementations of C, `flag', `uint8', and `int8' should all be `typedef'ed
| to the same as `int'.
*----------------------------------------------------------------------------*/

typedef   int_fast8_t sf_flag;
typedef  uint_fast8_t sf_uint8;
typedef   int_fast8_t sf_int8;
typedef uint_fast16_t sf_uint16;
typedef  int_fast16_t sf_int16;
typedef uint_fast32_t sf_uint32;
typedef  int_fast32_t sf_int32;
typedef uint_fast64_t sf_uint64;
typedef  int_fast64_t sf_int64;

/*----------------------------------------------------------------------------
| Each of the following `typedef's defines a type that holds integers
| of _exactly_ the number of bits specified.  For instance, for most
| implementation of C, `bits16' and `sbits16' should be `typedef'ed to
| `unsigned short int' and `signed short int' (or `short int'), respectively.
*----------------------------------------------------------------------------*/
typedef  uint8_t sf_bits8;
typedef   int8_t sf_sbits8;
typedef uint16_t sf_bits16;
typedef  int16_t sf_sbits16;
typedef uint32_t sf_bits32;
typedef  int32_t sf_sbits32;
typedef uint64_t sf_bits64;
typedef  int64_t sf_sbits64;

/*----------------------------------------------------------------------------
| The `SF_LIT64' macro takes as its argument a textual integer literal and
| if necessary ``marks'' the literal as having a 64-bit integer type.
| For example, the GNU C Compiler (`gcc') requires that 64-bit literals be
| appended with the letters `LL' standing for `long long', which is `gcc's
| name for the 64-bit integer type.  Some compilers may allow `SF_LIT64' to be
| defined as the identity macro:  `#define SF_LIT64( a ) a'.
*----------------------------------------------------------------------------*/
#define SF_LIT64( a ) INT64_C( a )

/*----------------------------------------------------------------------------
| Same as SF_LIT64, but for unsigned 64-bit integer types.
*----------------------------------------------------------------------------*/
#define SF_ULIT64( a ) UINT64_C( a )


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_SOFTFLOAT_MILIEU_H */
