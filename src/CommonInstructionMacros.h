/*
 * Copyright (C) 2017 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#ifndef SHAREMIND_LIBVM_COMMONINSTRUCTIONMACROS_H
#define SHAREMIND_LIBVM_COMMONINSTRUCTIONMACROS_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <cstdint>
#include <cstring>
#include <limits>
#include <sharemind/PotentiallyVoidTypeInfo.h>


#define SHAREMIND_T_int8    std::int8_t
#define SHAREMIND_T_int16   std::int16_t
#define SHAREMIND_T_int32   std::int32_t
#define SHAREMIND_T_int64   std::int64_t
#define SHAREMIND_T_uint8   std::uint8_t
#define SHAREMIND_T_uint16  std::uint16_t
#define SHAREMIND_T_uint32  std::uint32_t
#define SHAREMIND_T_uint64  std::uint64_t
#define SHAREMIND_T_float32 sf_float32
#define SHAREMIND_T_float64 sf_float64
#define SHAREMIND_T_size    std::size_t
#define SHAREMIND_T_intptr  std::intptr_t
#define SHAREMIND_T_uintptr std::uintptr_t
#define SHAREMIND_TMIN(t) (std::numeric_limits<t>::min())
#define SHAREMIND_TMIN_int8     SHAREMIND_TMIN(std::int8_t)
#define SHAREMIND_TMIN_int16    SHAREMIND_TMIN(std::int16_t)
#define SHAREMIND_TMIN_int32    SHAREMIND_TMIN(std::int32_t)
#define SHAREMIND_TMIN_int64    SHAREMIND_TMIN(std::int64_t)
#define SHAREMIND_TMIN_uint8    SHAREMIND_TMIN(std::uint8_t)
#define SHAREMIND_TMIN_uint16   SHAREMIND_TMIN(std::uint16_t)
#define SHAREMIND_TMIN_uint32   SHAREMIND_TMIN(std::uint32_t)
#define SHAREMIND_TMIN_uint64   SHAREMIND_TMIN(std::uint64_t)
#define SHAREMIND_TMIN_float32  SHAREMIND_TMIN(float)
#define SHAREMIND_TMIN_float64  SHAREMIND_TMIN(double)
#define SHAREMIND_TMIN_size     SHAREMIND_TMIN(std::size_t)
#define SHAREMIND_TMIN_intptr   SHAREMIND_TMIN(std::intptr_t)
#define SHAREMIND_TMIN_uintptr  SHAREMIND_TMIN(std::uintptr_t)
#define SHAREMIND_TMAX(t) (std::numeric_limits<t>::max())
#define SHAREMIND_TMAX_int8     SHAREMIND_TMAX(std::int8_t)
#define SHAREMIND_TMAX_int16    SHAREMIND_TMAX(std::int16_t)
#define SHAREMIND_TMAX_int32    SHAREMIND_TMAX(std::int32_t)
#define SHAREMIND_TMAX_int64    SHAREMIND_TMAX(std::int64_t)
#define SHAREMIND_TMAX_uint8    SHAREMIND_TMAX(std::uint8_t)
#define SHAREMIND_TMAX_uint16   SHAREMIND_TMAX(std::uint16_t)
#define SHAREMIND_TMAX_uint32   SHAREMIND_TMAX(std::uint32_t)
#define SHAREMIND_TMAX_uint64   SHAREMIND_TMAX(std::uint64_t)
#define SHAREMIND_TMAX_float32  SHAREMIND_TMAX(float)
#define SHAREMIND_TMAX_float64  SHAREMIND_TMAX(double)
#define SHAREMIND_TMAX_size     SHAREMIND_TMAX(std::size_t)
#define SHAREMIND_TMAX_intptr   SHAREMIND_TMAX(std::intptr_t)
#define SHAREMIND_TMAX_uintptr  SHAREMIND_TMAX(std::uintptr_t)

#define SHAREMIND_PTRADD ptrAdd

#define SHAREMIND_VM_STATIC_CAST(type,value) (static_cast<type>(value))

#define SHAREMIND_BITS_ALL(type) (type(type(0) | ~type(0)))
#define SHAREMIND_BITS_NONE(type) (type(~SHAREMIND_BITS_ALL(type)))

#define SHAREMIND_ULOW_BIT_MASK(type,bits,n) \
    (type(SHAREMIND_BITS_ALL(type) >> ((bits) - (n))))
#define SHAREMIND_UHIGH_BIT_MASK(type,bits,n) \
    (type(SHAREMIND_BITS_ALL(type) << ((bits) - (n))))
#define SHAREMIND_ULOW_BIT_FILTER(type,bits,v,n) \
    (type(type(v) & SHAREMIND_ULOW_BIT_MASK(type, (bits), (n))))
#define SHAREMIND_UHIGH_BIT_FILTER(type,bits,v,n) \
    (type(type(v) & SHAREMIND_UHIGH_BIT_MASK(type, (bits), (n))))

#define SHAREMIND_USHIFT_1_LEFT_0(type,v) \
    (type(type(v) << type(1u)))
#define SHAREMIND_USHIFT_1_RIGHT_0(type,v) \
    (type(type(v) >> type(1u)))

#define SHAREMIND_USHIFT_1_LEFT_1(type,bits,v) \
    (type(SHAREMIND_USHIFT_1_LEFT_0(type, (v)) \
          | SHAREMIND_ULOW_BIT_MASK(type, (bits), 1u)))
#define SHAREMIND_USHIFT_1_RIGHT_1(type,bits,v) \
    (type(SHAREMIND_USHIFT_1_RIGHT_0(type, (v)) \
          | SHAREMIND_UHIGH_BIT_MASK(type, (bits), 1u)))

#define SHAREMIND_USHIFT_1_LEFT_EXTEND(type,bits,v) \
    (type(SHAREMIND_USHIFT_1_LEFT_0(type, (v)) \
          | SHAREMIND_ULOW_BIT_FILTER(type, (bits), (v), 1u)))
#define SHAREMIND_USHIFT_1_RIGHT_EXTEND(type,bits,v) \
    (type(SHAREMIND_USHIFT_1_RIGHT_0(type, (v)) \
          | SHAREMIND_UHIGH_BIT_FILTER(type, (bits), (v), 1u)))

#define SHAREMIND_USHIFT_LEFT_0(type,bits,v,s) \
    (type((s) >= (bits) \
          ? SHAREMIND_BITS_NONE(type) \
          : type(type(v) << type(s))))
#define SHAREMIND_USHIFT_RIGHT_0(type,bits,v,s) \
    (type((s) >= (bits) \
          ? SHAREMIND_BITS_NONE(type) \
          : type(type(v) >> type(s))))

#define SHAREMIND_USHIFT_LEFT_1(type,bits,v,s) \
    (type((s) >= (bits) \
          ? SHAREMIND_BITS_ALL(type) \
          : type(type(type(v) << type(s)) \
                 | SHAREMIND_ULOW_BIT_MASK(type, (bits), (s)))))
#define SHAREMIND_USHIFT_RIGHT_1(type,bits,v,s) \
    (type((s) >= (bits) \
          ? SHAREMIND_BITS_ALL(type) \
          : type(type(type(v) >> type(s)) \
                 | SHAREMIND_UHIGH_BIT_MASK(type, (bits), (s)))))

#define SHAREMIND_USHIFT_LEFT_EXTEND(type,bits,v,s) \
    (type(SHAREMIND_ULOW_BIT_FILTER(type, (bits), (v), 1u) \
          ? SHAREMIND_USHIFT_LEFT_1(type, (bits), (v), (s)) \
          : SHAREMIND_USHIFT_LEFT_0(type, (bits), (v), (s))))
#define SHAREMIND_USHIFT_RIGHT_EXTEND(type,bits,v,s) \
    (type(SHAREMIND_UHIGH_BIT_FILTER(type, (bits), (v), 1u) \
          ? SHAREMIND_USHIFT_RIGHT_1(type, (bits), (v), (s)) \
          : SHAREMIND_USHIFT_RIGHT_0(type, (bits), (v), (s))))

#define SHAREMIND_UROTATE_LEFT(type,bits,v,s) \
    (((s) % bits) \
     ? (type((SHAREMIND_UHIGH_BIT_FILTER(type, (bits), (v), \
                                         ((s) % (bits))) \
              >> ((bits) - ((s) % bits))) \
             | (SHAREMIND_ULOW_BIT_FILTER(type, (bits), (v), \
                                          ((bits) - ((s) % (bits)))) \
                << ((s) % bits)))) \
     : type(v))
#define SHAREMIND_UROTATE_RIGHT(type,bits,v,s) \
    (((s) % bits) \
     ? (type((SHAREMIND_UHIGH_BIT_FILTER(type, (bits), (v), \
                                         ((bits) - ((s) % (bits)))) \
              >> ((s) % bits)) \
             | (SHAREMIND_ULOW_BIT_FILTER(type, (bits), (v), \
                                          ((s) % (bits))) \
                << ((bits) - ((s) % bits))))) \
     : type(v))

#define SHAREMIND_MEMCPY std::memcpy
#define SHAREMIND_MEMMOVE std::memmove

#endif /* SHAREMIND_LIBVM_COMMONINSTRUCTIONMACROS_H */
