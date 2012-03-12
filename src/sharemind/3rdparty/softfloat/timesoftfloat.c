/*
 * This file is derivative of part of the SoftFloat IEC/IEEE
 * Floating-point Arithmetic Package, Release 2b.
*/

/*============================================================================

This C source file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
Package, Release 2b.

Written by John R. Hauser.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
EFFECTIVELY INDEMNIFY THE AUTHOR, JOHN HAUSER, (possibly via similar legal
warning) AGAINST ALL LOSSES, COSTS, OR OTHER PROBLEMS INCURRED BY THEIR
CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) the source code for the derivative work includes prominent notice that
the work is derivative, and (2) the source code includes prominent notice with
these four paragraphs for those parts of this code that are retained.

=============================================================================*/

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "softfloat.h"

enum {
    minIterations = 1000
};

static char *functionName;
static char *roundingPrecisionName, *roundingModeName, *tininessModeName;

static void reportTime( sf_int32 count, long clocks )
{

    printf(
        "%8.1f kops/s: %s",
        ( count / ( ( (float) clocks ) / CLOCKS_PER_SEC ) ) / 1000,
        functionName
    );
    if ( roundingModeName ) {
        if ( roundingPrecisionName ) {
            fputs( ", precision ", stdout );
            fputs( roundingPrecisionName, stdout );
        }
        fputs( ", rounding ", stdout );
        fputs( roundingModeName, stdout );
        if ( tininessModeName ) {
            fputs( ", tininess ", stdout );
            fputs( tininessModeName, stdout );
            fputs( " rounding", stdout );
        }
    }
    fputc( '\n', stdout );

}

enum {
    numInputs_int32 = 32
};

static const sf_int32 inputs_int32[ numInputs_int32 ] = {
    0xFFFFBB79, 0x405CF80F, 0x00000000, 0xFFFFFD04,
    0xFFF20002, 0x0C8EF795, 0xF00011FF, 0x000006CA,
    0x00009BFE, 0xFF4862E3, 0x9FFFEFFE, 0xFFFFFFB7,
    0x0BFF7FFF, 0x0000F37A, 0x0011DFFE, 0x00000006,
    0xFFF02006, 0xFFFFF7D1, 0x10200003, 0xDE8DF765,
    0x00003E02, 0x000019E8, 0x0008FFFE, 0xFFFFFB5C,
    0xFFDF7FFE, 0x07C42FBF, 0x0FFFE3FF, 0x040B9F13,
    0xBFFFFFF8, 0x0001BF56, 0x000017F6, 0x000A908A
};

static void time_a_int32_z_float32( sf_float32 function( sf_int32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_int32[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_int32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_int32[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_int32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_a_int32_z_float64( sf_float64 function( sf_int32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_int32[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_int32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_int32[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_int32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}
enum {
    numInputs_int64 = 32
};

static const sf_int64 inputs_int64[ numInputs_int64 ] = {
    SF_LIT64( 0xFBFFC3FFFFFFFFFF ),
    SF_LIT64( 0x0000000003C589BC ),
    SF_LIT64( 0x00000000400013FE ),
    SF_LIT64( 0x0000000000186171 ),
    SF_LIT64( 0xFFFFFFFFFFFEFBFA ),
    SF_LIT64( 0xFFFFFD79E6DFFC73 ),
    SF_LIT64( 0x0000000010001DFF ),
    SF_LIT64( 0xDD1A0F0C78513710 ),
    SF_LIT64( 0xFFFF83FFFFFEFFFE ),
    SF_LIT64( 0x00756EBD1AD0C1C7 ),
    SF_LIT64( 0x0003FDFFFFFFFFBE ),
    SF_LIT64( 0x0007D0FB2C2CA951 ),
    SF_LIT64( 0x0007FC0007FFFFFE ),
    SF_LIT64( 0x0000001F942B18BB ),
    SF_LIT64( 0x0000080101FFFFFE ),
    SF_LIT64( 0xFFFFFFFFFFFF0978 ),
    SF_LIT64( 0x000000000008BFFF ),
    SF_LIT64( 0x0000000006F5AF08 ),
    SF_LIT64( 0xFFDEFF7FFFFFFFFE ),
    SF_LIT64( 0x0000000000000003 ),
    SF_LIT64( 0x3FFFFFFFFF80007D ),
    SF_LIT64( 0x0000000000000078 ),
    SF_LIT64( 0xFFF80000007FDFFD ),
    SF_LIT64( 0x1BBC775B78016AB0 ),
    SF_LIT64( 0xFFF9001FFFFFFFFE ),
    SF_LIT64( 0xFFFD4767AB98E43F ),
    SF_LIT64( 0xFFFFFEFFFE00001E ),
    SF_LIT64( 0xFFFFFFFFFFF04EFD ),
    SF_LIT64( 0x07FFFFFFFFFFF7FF ),
    SF_LIT64( 0xFFFC9EAA38F89050 ),
    SF_LIT64( 0x00000020FBFFFFFE ),
    SF_LIT64( 0x0000099AE6455357 )
};

static void time_a_int64_z_float32( sf_float32 function( sf_int64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_int64[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_int64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_int64[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_int64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_a_int64_z_float64( sf_float64 function( sf_int64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_int64[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_int64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_int64[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_int64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

enum {
    numInputs_float32 = 32
};

static const sf_float32 inputs_float32[ numInputs_float32 ] = {
    0x4EFA0000, 0xC1D0B328, 0x80000000, 0x3E69A31E,
    0xAF803EFF, 0x3F800000, 0x17BF8000, 0xE74A301A,
    0x4E010003, 0x7EE3C75D, 0xBD803FE0, 0xBFFEFF00,
    0x7981F800, 0x431FFFFC, 0xC100C000, 0x3D87EFFF,
    0x4103FEFE, 0xBC000007, 0xBF01F7FF, 0x4E6C6B5C,
    0xC187FFFE, 0xC58B9F13, 0x4F88007F, 0xDF004007,
    0xB7FFD7FE, 0x7E8001FB, 0x46EFFBFF, 0x31C10000,
    0xDB428661, 0x33F89B1F, 0xA3BFEFFF, 0x537BFFBE
};

static void time_a_float32_z_int32( sf_int32 function( sf_float32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float32[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float32[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_a_float32_z_int64( sf_int64 function( sf_float32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float32[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float32[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_a_float32_z_float64( sf_float64 function( sf_float32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float32[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float32[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_az_float32( sf_float32 function( sf_float32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float32[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float32[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_ab_float32_z_flag( sf_flag function( sf_float32, sf_float32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNumA, inputNumB;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                inputs_float32[ inputNumA ], inputs_float32[ inputNumB ] );
            inputNumA = ( inputNumA + 1 ) & ( numInputs_float32 - 1 );
            if ( inputNumA == 0 ) ++inputNumB;
            inputNumB = ( inputNumB + 1 ) & ( numInputs_float32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
            function(
                inputs_float32[ inputNumA ], inputs_float32[ inputNumB ] );
        inputNumA = ( inputNumA + 1 ) & ( numInputs_float32 - 1 );
        if ( inputNumA == 0 ) ++inputNumB;
        inputNumB = ( inputNumB + 1 ) & ( numInputs_float32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_abz_float32( sf_float32 function( sf_float32, sf_float32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNumA, inputNumB;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                inputs_float32[ inputNumA ], inputs_float32[ inputNumB ] );
            inputNumA = ( inputNumA + 1 ) & ( numInputs_float32 - 1 );
            if ( inputNumA == 0 ) ++inputNumB;
            inputNumB = ( inputNumB + 1 ) & ( numInputs_float32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
            function(
                inputs_float32[ inputNumA ], inputs_float32[ inputNumB ] );
        inputNumA = ( inputNumA + 1 ) & ( numInputs_float32 - 1 );
        if ( inputNumA == 0 ) ++inputNumB;
        inputNumB = ( inputNumB + 1 ) & ( numInputs_float32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static const sf_float32 inputs_float32_pos[ numInputs_float32 ] = {
    0x4EFA0000, 0x41D0B328, 0x00000000, 0x3E69A31E,
    0x2F803EFF, 0x3F800000, 0x17BF8000, 0x674A301A,
    0x4E010003, 0x7EE3C75D, 0x3D803FE0, 0x3FFEFF00,
    0x7981F800, 0x431FFFFC, 0x4100C000, 0x3D87EFFF,
    0x4103FEFE, 0x3C000007, 0x3F01F7FF, 0x4E6C6B5C,
    0x4187FFFE, 0x458B9F13, 0x4F88007F, 0x5F004007,
    0x37FFD7FE, 0x7E8001FB, 0x46EFFBFF, 0x31C10000,
    0x5B428661, 0x33F89B1F, 0x23BFEFFF, 0x537BFFBE
};

static void time_az_float32_pos( sf_float32 function( sf_float32 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float32_pos[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float32_pos[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float32 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

enum {
    numInputs_float64 = 32
};

static const sf_float64 inputs_float64[ numInputs_float64 ] = {
    SF_LIT64( 0x422FFFC008000000 ),
    SF_LIT64( 0xB7E0000480000000 ),
    SF_LIT64( 0xF3FD2546120B7935 ),
    SF_LIT64( 0x3FF0000000000000 ),
    SF_LIT64( 0xCE07F766F09588D6 ),
    SF_LIT64( 0x8000000000000000 ),
    SF_LIT64( 0x3FCE000400000000 ),
    SF_LIT64( 0x8313B60F0032BED8 ),
    SF_LIT64( 0xC1EFFFFFC0002000 ),
    SF_LIT64( 0x3FB3C75D224F2B0F ),
    SF_LIT64( 0x7FD00000004000FF ),
    SF_LIT64( 0xA12FFF8000001FFF ),
    SF_LIT64( 0x3EE0000000FE0000 ),
    SF_LIT64( 0x0010000080000004 ),
    SF_LIT64( 0x41CFFFFE00000020 ),
    SF_LIT64( 0x40303FFFFFFFFFFD ),
    SF_LIT64( 0x3FD000003FEFFFFF ),
    SF_LIT64( 0xBFD0000010000000 ),
    SF_LIT64( 0xB7FC6B5C16CA55CF ),
    SF_LIT64( 0x413EEB940B9D1301 ),
    SF_LIT64( 0xC7E00200001FFFFF ),
    SF_LIT64( 0x47F00021FFFFFFFE ),
    SF_LIT64( 0xBFFFFFFFF80000FF ),
    SF_LIT64( 0xC07FFFFFE00FFFFF ),
    SF_LIT64( 0x001497A63740C5E8 ),
    SF_LIT64( 0xC4BFFFE0001FFFFF ),
    SF_LIT64( 0x96FFDFFEFFFFFFFF ),
    SF_LIT64( 0x403FC000000001FE ),
    SF_LIT64( 0xFFD00000000001F6 ),
    SF_LIT64( 0x0640400002000000 ),
    SF_LIT64( 0x479CEE1E4F789FE0 ),
    SF_LIT64( 0xC237FFFFFFFFFDFE )
};

static void time_a_float64_z_int32( sf_int32 function( sf_float64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float64[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float64[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_a_float64_z_int64( sf_int64 function( sf_float64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float64[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float64[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_a_float64_z_float32( sf_float32 function( sf_float64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float64[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float64[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_az_float64( sf_float64 function( sf_float64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float64[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float64[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_ab_float64_z_flag( sf_flag function( sf_float64, sf_float64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNumA, inputNumB;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                inputs_float64[ inputNumA ], inputs_float64[ inputNumB ] );
            inputNumA = ( inputNumA + 1 ) & ( numInputs_float64 - 1 );
            if ( inputNumA == 0 ) ++inputNumB;
            inputNumB = ( inputNumB + 1 ) & ( numInputs_float64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
            function(
                inputs_float64[ inputNumA ], inputs_float64[ inputNumB ] );
        inputNumA = ( inputNumA + 1 ) & ( numInputs_float64 - 1 );
        if ( inputNumA == 0 ) ++inputNumB;
        inputNumB = ( inputNumB + 1 ) & ( numInputs_float64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_abz_float64( sf_float64 function( sf_float64, sf_float64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNumA, inputNumB;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                inputs_float64[ inputNumA ], inputs_float64[ inputNumB ] );
            inputNumA = ( inputNumA + 1 ) & ( numInputs_float64 - 1 );
            if ( inputNumA == 0 ) ++inputNumB;
            inputNumB = ( inputNumB + 1 ) & ( numInputs_float64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
            function(
                inputs_float64[ inputNumA ], inputs_float64[ inputNumB ] );
        inputNumA = ( inputNumA + 1 ) & ( numInputs_float64 - 1 );
        if ( inputNumA == 0 ) ++inputNumB;
        inputNumB = ( inputNumB + 1 ) & ( numInputs_float64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static const sf_float64 inputs_float64_pos[ numInputs_float64 ] = {
    SF_LIT64( 0x422FFFC008000000 ),
    SF_LIT64( 0x37E0000480000000 ),
    SF_LIT64( 0x73FD2546120B7935 ),
    SF_LIT64( 0x3FF0000000000000 ),
    SF_LIT64( 0x4E07F766F09588D6 ),
    SF_LIT64( 0x0000000000000000 ),
    SF_LIT64( 0x3FCE000400000000 ),
    SF_LIT64( 0x0313B60F0032BED8 ),
    SF_LIT64( 0x41EFFFFFC0002000 ),
    SF_LIT64( 0x3FB3C75D224F2B0F ),
    SF_LIT64( 0x7FD00000004000FF ),
    SF_LIT64( 0x212FFF8000001FFF ),
    SF_LIT64( 0x3EE0000000FE0000 ),
    SF_LIT64( 0x0010000080000004 ),
    SF_LIT64( 0x41CFFFFE00000020 ),
    SF_LIT64( 0x40303FFFFFFFFFFD ),
    SF_LIT64( 0x3FD000003FEFFFFF ),
    SF_LIT64( 0x3FD0000010000000 ),
    SF_LIT64( 0x37FC6B5C16CA55CF ),
    SF_LIT64( 0x413EEB940B9D1301 ),
    SF_LIT64( 0x47E00200001FFFFF ),
    SF_LIT64( 0x47F00021FFFFFFFE ),
    SF_LIT64( 0x3FFFFFFFF80000FF ),
    SF_LIT64( 0x407FFFFFE00FFFFF ),
    SF_LIT64( 0x001497A63740C5E8 ),
    SF_LIT64( 0x44BFFFE0001FFFFF ),
    SF_LIT64( 0x16FFDFFEFFFFFFFF ),
    SF_LIT64( 0x403FC000000001FE ),
    SF_LIT64( 0x7FD00000000001F6 ),
    SF_LIT64( 0x0640400002000000 ),
    SF_LIT64( 0x479CEE1E4F789FE0 ),
    SF_LIT64( 0x4237FFFFFFFFFDFE )
};

static void time_az_float64_pos( sf_float64 function( sf_float64 ) )
{
    clock_t startClock, endClock;
    sf_int32 count, i;
    sf_int8 inputNum;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_float64_pos[ inputNum ] );
            inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_float64_pos[ inputNum ] );
        inputNum = ( inputNum + 1 ) & ( numInputs_float64 - 1 );
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

enum {
    INT32_TO_FLOAT32 = 1,
    INT32_TO_FLOAT64,
    INT64_TO_FLOAT32,
    INT64_TO_FLOAT64,
    FLOAT32_TO_INT32,
    FLOAT32_TO_INT32_ROUND_TO_ZERO,
    FLOAT32_TO_INT64,
    FLOAT32_TO_INT64_ROUND_TO_ZERO,
    FLOAT32_TO_FLOAT64,
    FLOAT32_ROUND_TO_INT,
    FLOAT32_ADD,
    FLOAT32_SUB,
    FLOAT32_MUL,
    FLOAT32_DIV,
    FLOAT32_REM,
    FLOAT32_SQRT,
    FLOAT32_EQ,
    FLOAT32_LE,
    FLOAT32_LT,
    FLOAT32_EQ_SIGNALING,
    FLOAT32_LE_QUIET,
    FLOAT32_LT_QUIET,
    FLOAT64_TO_INT32,
    FLOAT64_TO_INT32_ROUND_TO_ZERO,
    FLOAT64_TO_INT64,
    FLOAT64_TO_INT64_ROUND_TO_ZERO,
    FLOAT64_TO_FLOAT32,
    FLOAT64_ROUND_TO_INT,
    FLOAT64_ADD,
    FLOAT64_SUB,
    FLOAT64_MUL,
    FLOAT64_DIV,
    FLOAT64_REM,
    FLOAT64_SQRT,
    FLOAT64_EQ,
    FLOAT64_LE,
    FLOAT64_LT,
    FLOAT64_EQ_SIGNALING,
    FLOAT64_LE_QUIET,
    FLOAT64_LT_QUIET,
    NUM_FUNCTIONS
};

static struct {
    char *name;
    sf_int8 numInputs;
    sf_flag roundingPrecision, roundingMode;
    sf_flag tininessMode, tininessModeAtReducedPrecision;
} functions[ NUM_FUNCTIONS ] = {
    { 0, 0, 0, 0, 0, 0 },
    { "int32_to_float32",                1, FALSE, TRUE,  FALSE, FALSE },
    { "int32_to_float64",                1, FALSE, FALSE, FALSE, FALSE },
    { "int64_to_float32",                1, FALSE, TRUE,  FALSE, FALSE },
    { "int64_to_float64",                1, FALSE, TRUE,  FALSE, FALSE },
    { "float32_to_int32",                1, FALSE, TRUE,  FALSE, FALSE },
    { "float32_to_int32_round_to_zero",  1, FALSE, FALSE, FALSE, FALSE },
    { "float32_to_int64",                1, FALSE, TRUE,  FALSE, FALSE },
    { "float32_to_int64_round_to_zero",  1, FALSE, FALSE, FALSE, FALSE },
    { "float32_to_float64",              1, FALSE, FALSE, FALSE, FALSE },
    { "float32_round_to_int",            1, FALSE, TRUE,  FALSE, FALSE },
    { "float32_add",                     2, FALSE, TRUE,  FALSE, FALSE },
    { "float32_sub",                     2, FALSE, TRUE,  FALSE, FALSE },
    { "float32_mul",                     2, FALSE, TRUE,  TRUE,  FALSE },
    { "float32_div",                     2, FALSE, TRUE,  FALSE, FALSE },
    { "float32_rem",                     2, FALSE, FALSE, FALSE, FALSE },
    { "float32_sqrt",                    1, FALSE, TRUE,  FALSE, FALSE },
    { "float32_eq",                      2, FALSE, FALSE, FALSE, FALSE },
    { "float32_le",                      2, FALSE, FALSE, FALSE, FALSE },
    { "float32_lt",                      2, FALSE, FALSE, FALSE, FALSE },
    { "float32_eq_signaling",            2, FALSE, FALSE, FALSE, FALSE },
    { "float32_le_quiet",                2, FALSE, FALSE, FALSE, FALSE },
    { "float32_lt_quiet",                2, FALSE, FALSE, FALSE, FALSE },
    { "float64_to_int32",                1, FALSE, TRUE,  FALSE, FALSE },
    { "float64_to_int32_round_to_zero",  1, FALSE, FALSE, FALSE, FALSE },
    { "float64_to_int64",                1, FALSE, TRUE,  FALSE, FALSE },
    { "float64_to_int64_round_to_zero",  1, FALSE, FALSE, FALSE, FALSE },
    { "float64_to_float32",              1, FALSE, TRUE,  TRUE,  FALSE },
    { "float64_round_to_int",            1, FALSE, TRUE,  FALSE, FALSE },
    { "float64_add",                     2, FALSE, TRUE,  FALSE, FALSE },
    { "float64_sub",                     2, FALSE, TRUE,  FALSE, FALSE },
    { "float64_mul",                     2, FALSE, TRUE,  TRUE,  FALSE },
    { "float64_div",                     2, FALSE, TRUE,  FALSE, FALSE },
    { "float64_rem",                     2, FALSE, FALSE, FALSE, FALSE },
    { "float64_sqrt",                    1, FALSE, TRUE,  FALSE, FALSE },
    { "float64_eq",                      2, FALSE, FALSE, FALSE, FALSE },
    { "float64_le",                      2, FALSE, FALSE, FALSE, FALSE },
    { "float64_lt",                      2, FALSE, FALSE, FALSE, FALSE },
    { "float64_eq_signaling",            2, FALSE, FALSE, FALSE, FALSE },
    { "float64_le_quiet",                2, FALSE, FALSE, FALSE, FALSE },
    { "float64_lt_quiet",                2, FALSE, FALSE, FALSE, FALSE }
};

enum {
    ROUND_NEAREST_EVEN = 1,
    ROUND_TO_ZERO,
    ROUND_DOWN,
    ROUND_UP,
    NUM_ROUNDINGMODES
};
enum {
    TININESS_BEFORE_ROUNDING = 1,
    TININESS_AFTER_ROUNDING,
    NUM_TININESSMODES
};

static void
 timeFunctionVariety(
     sf_uint8 functionCode,
     sf_int8 roundingPrecision,
     sf_int8 roundingMode,
     sf_int8 tininessMode
 )
{
    sf_uint8 roundingCode;
    sf_int8 tininessCode;

    functionName = functions[ functionCode ].name;
    if ( roundingPrecision == 32 ) {
        roundingPrecisionName = "32";
    }
    else if ( roundingPrecision == 64 ) {
        roundingPrecisionName = "64";
    }
    else if ( roundingPrecision == 80 ) {
        roundingPrecisionName = "80";
    }
    else {
        roundingPrecisionName = 0;
    }
    switch ( roundingMode ) {
     case 0:
        roundingModeName = 0;
        roundingCode = sf_float_round_nearest_even;
        break;
     case ROUND_NEAREST_EVEN:
        roundingModeName = "nearest_even";
        roundingCode = sf_float_round_nearest_even;
        break;
     case ROUND_TO_ZERO:
        roundingModeName = "to_zero";
        roundingCode = sf_float_round_to_zero;
        break;
     case ROUND_DOWN:
        roundingModeName = "down";
        roundingCode = sf_float_round_down;
        break;
     case ROUND_UP:
        roundingModeName = "up";
        roundingCode = sf_float_round_up;
        break;
    }
    sf_float_rounding_mode = roundingCode;
    switch ( tininessMode ) {
     case 0:
        tininessModeName = 0;
        tininessCode = sf_float_tininess_after_rounding;
        break;
     case TININESS_BEFORE_ROUNDING:
        tininessModeName = "before";
        tininessCode = sf_float_tininess_before_rounding;
        break;
     case TININESS_AFTER_ROUNDING:
        tininessModeName = "after";
        tininessCode = sf_float_tininess_after_rounding;
        break;
    }
    sf_float_detect_tininess = tininessCode;
    switch ( functionCode ) {
     case INT32_TO_FLOAT32:
        time_a_int32_z_float32( sf_int32_to_float32 );
        break;
     case INT32_TO_FLOAT64:
        time_a_int32_z_float64( sf_int32_to_float64 );
        break;
     case INT64_TO_FLOAT32:
        time_a_int64_z_float32( sf_int64_to_float32 );
        break;
     case INT64_TO_FLOAT64:
        time_a_int64_z_float64( sf_int64_to_float64 );
        break;
     case FLOAT32_TO_INT32:
        time_a_float32_z_int32( sf_float32_to_int32 );
        break;
     case FLOAT32_TO_INT32_ROUND_TO_ZERO:
        time_a_float32_z_int32( sf_float32_to_int32_round_to_zero );
        break;
     case FLOAT32_TO_INT64:
        time_a_float32_z_int64( sf_float32_to_int64 );
        break;
     case FLOAT32_TO_INT64_ROUND_TO_ZERO:
        time_a_float32_z_int64( sf_float32_to_int64_round_to_zero );
        break;
     case FLOAT32_TO_FLOAT64:
        time_a_float32_z_float64( sf_float32_to_float64 );
        break;
     case FLOAT32_ROUND_TO_INT:
        time_az_float32( sf_float32_round_to_int );
        break;
     case FLOAT32_ADD:
        time_abz_float32( sf_float32_add );
        break;
     case FLOAT32_SUB:
        time_abz_float32( sf_float32_sub );
        break;
     case FLOAT32_MUL:
        time_abz_float32( sf_float32_mul );
        break;
     case FLOAT32_DIV:
        time_abz_float32( sf_float32_div );
        break;
     case FLOAT32_REM:
        time_abz_float32( sf_float32_rem );
        break;
     case FLOAT32_SQRT:
        time_az_float32_pos( sf_float32_sqrt );
        break;
     case FLOAT32_EQ:
        time_ab_float32_z_flag( sf_float32_eq );
        break;
     case FLOAT32_LE:
        time_ab_float32_z_flag( sf_float32_le );
        break;
     case FLOAT32_LT:
        time_ab_float32_z_flag( sf_float32_lt );
        break;
     case FLOAT32_EQ_SIGNALING:
        time_ab_float32_z_flag( sf_float32_eq_signaling );
        break;
     case FLOAT32_LE_QUIET:
        time_ab_float32_z_flag( sf_float32_le_quiet );
        break;
     case FLOAT32_LT_QUIET:
        time_ab_float32_z_flag( sf_float32_lt_quiet );
        break;
     case FLOAT64_TO_INT32:
        time_a_float64_z_int32( sf_float64_to_int32 );
        break;
     case FLOAT64_TO_INT32_ROUND_TO_ZERO:
        time_a_float64_z_int32( sf_float64_to_int32_round_to_zero );
        break;
     case FLOAT64_TO_INT64:
        time_a_float64_z_int64( sf_float64_to_int64 );
        break;
     case FLOAT64_TO_INT64_ROUND_TO_ZERO:
        time_a_float64_z_int64( sf_float64_to_int64_round_to_zero );
        break;
     case FLOAT64_TO_FLOAT32:
        time_a_float64_z_float32( sf_float64_to_float32 );
        break;
     case FLOAT64_ROUND_TO_INT:
        time_az_float64( sf_float64_round_to_int );
        break;
     case FLOAT64_ADD:
        time_abz_float64( sf_float64_add );
        break;
     case FLOAT64_SUB:
        time_abz_float64( sf_float64_sub );
        break;
     case FLOAT64_MUL:
        time_abz_float64( sf_float64_mul );
        break;
     case FLOAT64_DIV:
        time_abz_float64( sf_float64_div );
        break;
     case FLOAT64_REM:
        time_abz_float64( sf_float64_rem );
        break;
     case FLOAT64_SQRT:
        time_az_float64_pos( sf_float64_sqrt );
        break;
     case FLOAT64_EQ:
        time_ab_float64_z_flag( sf_float64_eq );
        break;
     case FLOAT64_LE:
        time_ab_float64_z_flag( sf_float64_le );
        break;
     case FLOAT64_LT:
        time_ab_float64_z_flag( sf_float64_lt );
        break;
     case FLOAT64_EQ_SIGNALING:
        time_ab_float64_z_flag( sf_float64_eq_signaling );
        break;
     case FLOAT64_LE_QUIET:
        time_ab_float64_z_flag( sf_float64_le_quiet );
        break;
     case FLOAT64_LT_QUIET:
        time_ab_float64_z_flag( sf_float64_lt_quiet );
        break;
    }

}

static void timeFunction( sf_uint8 functionCode )
{
    sf_int8 roundingPrecision, roundingMode, tininessMode;

    roundingPrecision = 32;
    for (;;) {
        if ( ! functions[ functionCode ].roundingPrecision ) {
            roundingPrecision = 0;
        }
        for ( roundingMode = 1;
              roundingMode < NUM_ROUNDINGMODES;
              ++roundingMode
            ) {
            if ( ! functions[ functionCode ].roundingMode ) {
                roundingMode = 0;
            }
            for ( tininessMode = 1;
                  tininessMode < NUM_TININESSMODES;
                  ++tininessMode
                ) {
                if (    ( roundingPrecision == 32 )
                     || ( roundingPrecision == 64 ) ) {
                    if ( ! functions[ functionCode ]
                               .tininessModeAtReducedPrecision
                       ) {
                        tininessMode = 0;
                    }
                }
                else {
                    if ( ! functions[ functionCode ].tininessMode ) {
                        tininessMode = 0;
                    }
                }
                timeFunctionVariety(
                    functionCode, roundingPrecision, roundingMode, tininessMode
                );
                if ( ! tininessMode ) break;
            }
            if ( ! roundingMode ) break;
        }
        if ( ! roundingPrecision ) break;
        if ( roundingPrecision == 80 ) {
            break;
        }
        else if ( roundingPrecision == 64 ) {
            roundingPrecision = 80;
        }
        else if ( roundingPrecision == 32 ) {
            roundingPrecision = 64;
        }
    }
}

int main( int argc, char **argv )
{
    sf_uint8 functionCode = 1;
    while ( functionCode < NUM_FUNCTIONS )
        timeFunction( functionCode++ );
    return EXIT_SUCCESS;
}

