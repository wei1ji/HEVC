/*
 * Provide SSE MC functions for HEVC decoding
 * Copyright (c) 2013 Pierre-Edouard LEPERE
 * Copyright (c) 2014 Gildas COCHEREL
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcdsp.h"

#if HAVE_SSE2
#include <emmintrin.h>
#endif
#if HAVE_SSSE3
#include <tmmintrin.h>
#endif
#if HAVE_SSE42
#include <smmintrin.h>
#endif
#if HAVE_AVX2
#include <immintrin.h>
#endif

#define CLPI_PIXEL_MAX_10 0x03FF
#define CLPI_PIXEL_MAX_12 0x0FFF

//#ifndef OPTI_ASM
DECLARE_ALIGNED(32, const int8_t, ohhevc_epel_filters_avx2[7][2][32]) = {
    { { -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58,
        -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58},
      { 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2,
        10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2} },
    { { -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54,
        -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54},
      { 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2,
        16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2} },
    { { -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46,
        -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46},
      { 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4,
        28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4} },
    { { -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36,
        -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36},
      { 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4,
        36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4} },
    { { -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28,
        -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28},
      { 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6,
        46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6} },
    { { -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16,
        -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16},
      { 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4,
        54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4} },
    { { -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10,
        -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10},
      { 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2,
        58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2} }
};
DECLARE_ALIGNED(32, const int16_t, ohhevc_epel_filters_avx2_16[7][2][16]) = {
    { { -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58},
      { 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2} },
    { { -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54},
      { 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2} },
    { { -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46},
      { 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4} },
    { { -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36},
      { 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4} },
    { { -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28},
      { 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6} },
    { { -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16},
      { 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4} },
    { { -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10},
      { 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2} }
};

DECLARE_ALIGNED(32, const int8_t, ohhevc_qpel_filters_avx2[3][4][32]) = {
    { { -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4,
        -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4},
      {-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,
       -10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58},
      { 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5,
        17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5},
      {  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,
         1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0} },
    { { -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4,
        -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4},
      {-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,
       -11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40},
      { 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11,
        40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11},
      {  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,
         4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1} },
    { {  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,
         0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1},
      { -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17,
        -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17},
      { 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10,
        58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10},
      {  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,
         4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1} }
};
DECLARE_ALIGNED(32, const int16_t, ohhevc_qpel_filters_avx2_16[3][4][16]) = {
    { { -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4},
      {-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58},
      { 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5},
      {  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0} },
    { { -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4},
      {-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40},
      { 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11},
      {  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1} },
    { {  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1},
      { -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17},
      { 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10},
      {  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1} }
};

////////////////////////////////////////////////////////////////////////////////
// Filters loading
////////////////////////////////////////////////////////////////////////////////
#define QPEL_FILTER_8(idx)                                                                   \
    const __m256i c1 = _mm256_load_si256((( __m256i *) ohhevc_qpel_filters_avx2[idx][0]));  \
    const __m256i c2 = _mm256_load_si256((( __m256i *) ohhevc_qpel_filters_avx2[idx][1]));  \
    const __m256i c3 = _mm256_load_si256((( __m256i *) ohhevc_qpel_filters_avx2[idx][2]));  \
    const __m256i c4 = _mm256_load_si256((( __m256i *) ohhevc_qpel_filters_avx2[idx][3]))

#define QPEL_FILTER_16(idx)                                                                  \
    const __m256i c1 = _mm256_load_si256((( __m256i *)ohhevc_qpel_filters_avx2_16[idx][0]));\
    const __m256i c2 = _mm256_load_si256((( __m256i *)ohhevc_qpel_filters_avx2_16[idx][1]));\
    const __m256i c3 = _mm256_load_si256((( __m256i *)ohhevc_qpel_filters_avx2_16[idx][2]));\
    const __m256i c4 = _mm256_load_si256((( __m256i *)ohhevc_qpel_filters_avx2_16[idx][3]))

#define QPEL_FILTER_10(idx)              QPEL_FILTER_16(idx)
#define QPEL_FILTER_12(idx)              QPEL_FILTER_16(idx)
#define QPEL_FILTER_14(idx)              QPEL_FILTER_16(idx)

#define EPEL_FILTER_8(idx)                                                                    \
    const __m256i f1 = _mm256_load_si256((__m256i *) ohhevc_epel_filters_avx2[idx][0]);      \
    const __m256i f2 = _mm256_load_si256((__m256i *) ohhevc_epel_filters_avx2[idx][1])

#define EPEL_FILTER_16(idx)                                                                    \
    const __m256i f1 = _mm256_set1_epi32(*((int32_t *) ohhevc_epel_filters_avx2_16[idx][0])); \
    const __m256i f2 = _mm256_set1_epi32(*((int32_t *) ohhevc_epel_filters_avx2_16[idx][1]))

#define EPEL_FILTER_10(idx)  EPEL_FILTER_16(idx)
#define EPEL_FILTER_12(idx)  EPEL_FILTER_16(idx)
#define EPEL_FILTER_14(idx)  EPEL_FILTER_16(idx)

////////////////////////////////////////////////////////////////////////////////
// Variables declaration
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_PEL_PIXELS_VAR2_8()                                           \
    __m256i x1;                                                                \
    __m256i c0 = _mm256_setzero_si256()

#define PUT_HEVC_PEL_PIXELS_VAR4_8()  PUT_HEVC_PEL_PIXELS_VAR2_8()
#define PUT_HEVC_PEL_PIXELS_VAR6_8()  PUT_HEVC_PEL_PIXELS_VAR2_8()
#define PUT_HEVC_PEL_PIXELS_VAR16_8() PUT_HEVC_PEL_PIXELS_VAR2_8()

#define PUT_HEVC_PEL_PIXELS_VAR32_8()                                          \
    __m256i x1, x2, x9;                                                        \
    const __m256i c0    = _mm256_setzero_si256()

#define PUT_HEVC_PEL_PIXELS_VAR2_16()                                          \
    __m256i x1

#define PUT_HEVC_PEL_PIXELS_VAR2_10()   PUT_HEVC_PEL_PIXELS_VAR2_16()
#define PUT_HEVC_PEL_PIXELS_VAR4_10()   PUT_HEVC_PEL_PIXELS_VAR2_16()
#define PUT_HEVC_PEL_PIXELS_VAR6_10()   PUT_HEVC_PEL_PIXELS_VAR2_16()
#define PUT_HEVC_PEL_PIXELS_VAR16_10()  PUT_HEVC_PEL_PIXELS_VAR2_16()
//???
#define PUT_HEVC_PEL_PIXELS_VAR32_10()  PUT_HEVC_PEL_PIXELS_VAR32_8()

#define PUT_HEVC_PEL_PIXELS_VAR2_12()   PUT_HEVC_PEL_PIXELS_VAR2_16()
#define PUT_HEVC_PEL_PIXELS_VAR4_12()   PUT_HEVC_PEL_PIXELS_VAR2_16()
#define PUT_HEVC_PEL_PIXELS_VAR6_12()   PUT_HEVC_PEL_PIXELS_VAR2_16()
#define PUT_HEVC_PEL_PIXELS_VAR16_12()  PUT_HEVC_PEL_PIXELS_VAR2_16()
//???
#define PUT_HEVC_PEL_PIXELS_VAR32_12()  PUT_HEVC_PEL_PIXELS_VAR32_8()

//EPEL V VAR
#define PUT_HEVC_EPEL_V_VAR2_8()                                               \
    __m256i x1, x2, x3, x4, f1, f2

#define PUT_HEVC_EPEL_V_VAR4_8()  PUT_HEVC_EPEL_V_VAR2_8()
#define PUT_HEVC_EPEL_V_VAR6_8()  PUT_HEVC_EPEL_V_VAR2_8()
#define PUT_HEVC_EPEL_V_VAR16_8() PUT_HEVC_EPEL_V_VAR2_8()

#define PUT_HEVC_EPEL_V_VAR2_16()                                              \
    __m256i x1, x2, x3, x4, t1, t2, f1, f2

#define PUT_HEVC_EPEL_V_VAR2_10()  PUT_HEVC_EPEL_V_VAR2_16()
#define PUT_HEVC_EPEL_V_VAR4_10()  PUT_HEVC_EPEL_V_VAR2_16()
#define PUT_HEVC_EPEL_V_VAR6_10()  PUT_HEVC_EPEL_V_VAR2_16()
#define PUT_HEVC_EPEL_V_VAR16_10() PUT_HEVC_EPEL_V_VAR2_16()

#define PUT_HEVC_EPEL_V_VAR2_12()  PUT_HEVC_EPEL_V_VAR2_16()
#define PUT_HEVC_EPEL_V_VAR4_12()  PUT_HEVC_EPEL_V_VAR2_16()
#define PUT_HEVC_EPEL_V_VAR6_12()  PUT_HEVC_EPEL_V_VAR2_16()
#define PUT_HEVC_EPEL_V_VAR16_12() PUT_HEVC_EPEL_V_VAR2_16()

//EPEL H VAR
#define PUT_HEVC_EPEL_H_VAR2_8()                                               \
    __m256i x1, x2, x3, x4, f1, f2

#define PUT_HEVC_EPEL_H_VAR4_8()  PUT_HEVC_EPEL_H_VAR2_8()
#define PUT_HEVC_EPEL_H_VAR6_8()  PUT_HEVC_EPEL_H_VAR2_8()
#define PUT_HEVC_EPEL_H_VAR16_8() PUT_HEVC_EPEL_H_VAR2_8()

#define PUT_HEVC_EPEL_H_VAR2_16()                                              \
    __m256i x1, x2, x3, x4, t1, t2, f1, f2

#define PUT_HEVC_EPEL_H_VAR2_10()  PUT_HEVC_EPEL_H_VAR2_16()
#define PUT_HEVC_EPEL_H_VAR4_10()  PUT_HEVC_EPEL_H_VAR2_16()
#define PUT_HEVC_EPEL_H_VAR6_10()  PUT_HEVC_EPEL_H_VAR2_16()
#define PUT_HEVC_EPEL_H_VAR16_10() PUT_HEVC_EPEL_H_VAR2_16()

#define PUT_HEVC_EPEL_H_VAR2_12()  PUT_HEVC_EPEL_H_VAR2_16()
#define PUT_HEVC_EPEL_H_VAR4_12()  PUT_HEVC_EPEL_H_VAR2_16()
#define PUT_HEVC_EPEL_H_VAR6_12()  PUT_HEVC_EPEL_H_VAR2_16()
#define PUT_HEVC_EPEL_H_VAR16_12() PUT_HEVC_EPEL_H_VAR2_16()

//QPEL V VAR
#define PUT_HEVC_QPEL_V_VAR8_8()                                               \
    __m256i x1, x2, x3, x4, x5, x6, x7, x8, c1, c2, c3, c4

#define PUT_HEVC_QPEL_V_VAR16_8()     PUT_HEVC_QPEL_V_VAR8_8()

#define PUT_HEVC_QPEL_V_VAR32_8()                                              \
    __m256i x1, x2, x3, x4, x5, x6, x7, x8, x9, c1, c2, c3, c4

#define PUT_HEVC_QPEL_V_VAR8_16()                                              \
    const __m256i c0    = _mm256_setzero_si256();                              \
    __m256i x1, x2, x3, x4, x5, x6, x7, x8, c1, c2, c3, c4

#define PUT_HEVC_QPEL_V_VAR8_10()     PUT_HEVC_QPEL_V_VAR8_16()
#define PUT_HEVC_QPEL_V_VAR8_12()     PUT_HEVC_QPEL_V_VAR8_16()
#define PUT_HEVC_QPEL_V_VAR8_14()     PUT_HEVC_QPEL_V_VAR8_16()

#define PUT_HEVC_QPEL_V_VAR16_16()                                             \
    __m256i x1, x2, x3, x4, x5, x6, x7, x8, x9, c1, c2, c3, c4

#define PUT_HEVC_QPEL_V_VAR16_10()    PUT_HEVC_QPEL_V_VAR16_16()
#define PUT_HEVC_QPEL_V_VAR16_12()    PUT_HEVC_QPEL_V_VAR16_16()
#define PUT_HEVC_QPEL_V_VAR16_14()    PUT_HEVC_QPEL_V_VAR16_16()

//QPEL H VAR
#define PUT_HEVC_QPEL_H_VAR8_8()                                               \
    __m256i x1, x2, x3, x4, x5, x6, x7, x8, c1, c2, c3, c4

#define PUT_HEVC_QPEL_H_VAR16_8()   PUT_HEVC_QPEL_H_VAR8_8()

#define PUT_HEVC_QPEL_H_VAR32_8()                                              \
    __m256i x1, x2, x3, x4, x5, x6, x7, x8, x9, c1, c2, c3, c4

//QPEL H 10 VAR ???
#define PUT_HEVC_QPEL_H_10_VAR4_16()                                           \
    const __m128i c0    = _mm_setzero_si128();                                 \
    __m128i x1, x2, x3, x4, r1, r3, c1, c2, c3, c4

#define PUT_HEVC_QPEL_H_10_VAR4_10() PUT_HEVC_QPEL_H_10_VAR4_16()
#define PUT_HEVC_QPEL_H_10_VAR4_12() PUT_HEVC_QPEL_H_10_VAR4_16()

#define PUT_HEVC_QPEL_H_10_VAR16_16()                                          \
    __m256i x1, x2, x3, x4, r1, r2, r3, r4, c1, c2, c3, c4, t1, t2

#define PUT_HEVC_QPEL_H_10_VAR16_10() PUT_HEVC_QPEL_H_10_VAR16_16()
#define PUT_HEVC_QPEL_H_10_VAR16_12() PUT_HEVC_QPEL_H_10_VAR16_16()


//LOOP VAR
#define PUT_HEVC_PEL_PIXELS_LOOP_VAR_R5()                                      \
    __m256i r5

#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR8_8()    PUT_HEVC_PEL_PIXELS_LOOP_VAR_R5()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR16_8()   PUT_HEVC_PEL_PIXELS_LOOP_VAR_R5()

#define PUT_HEVC_PEL_PIXELS_LOOP_VAR_R5_R6()                                   \
    __m256i r5, r6


#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR6_8()    PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_8()

#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR32_8()   PUT_HEVC_PEL_PIXELS_LOOP_VAR_R5_R6()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_10()   PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_8()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR6_10()   PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_8()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR8_10()   PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_8()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR16_10()  PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR16_8()

#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_12()   PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_8()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR6_12()   PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_8()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR8_12()   PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR4_8()
#define PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR16_12()  PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR16_8()

////////////////////////////////////////////////////////////////////////////////
// Source and destination initialization
////////////////////////////////////////////////////////////////////////////////
#define SRC_INIT_8()                                                           \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    const int srcstride = _srcstride

#define SRC_INIT_16()                                                          \
    uint16_t *src       = (uint16_t*) _src;                                    \
    const int srcstride = _srcstride >> 1

#define SRC_INIT_10() SRC_INIT_16()
#define SRC_INIT_12() SRC_INIT_16()
#define SRC_INIT_14() SRC_INIT_16()

#define SRC_INIT_H_8()                                                         \
    uint8_t  *src = ((uint8_t*) _src) - 1;                                     \
    ptrdiff_t srcstride = _srcstride

#define SRC_INIT_H_16()                                                        \
    uint16_t *src = ((uint16_t*) _src) - 1;                                    \
    ptrdiff_t srcstride  = _srcstride >> 1

#define SRC_INIT_H_10() SRC_INIT_H_16()
#define SRC_INIT_H_12() SRC_INIT_H_16()
#define SRC_INIT_H_14() SRC_INIT_H_16()

#define SRC_INIT_V_8()                                                         \
    ptrdiff_t srcstride = _srcstride;                                          \
    uint8_t  *src       = ((uint8_t*) _src) - srcstride

#define SRC_INIT_V_16()                                                        \
    ptrdiff_t srcstride = _srcstride >> 1;                                     \
    uint16_t *src       = ((uint16_t*) _src) - srcstride

#define SRC_INIT_V_10() SRC_INIT_V_16()
#define SRC_INIT_V_12() SRC_INIT_V_16()
#define SRC_INIT_V_14() SRC_INIT_V_16()

#define SRC_INIT_HV_8()                                                        \
    uint8_t  *src_bis, *src = ((uint8_t*) _src) - 1;                           \
    ptrdiff_t srcstride = _srcstride

#define SRC_INIT_HV_16()                                                       \
    uint16_t *src_bis, *src = ((uint16_t*) _src) - 1;                          \
    ptrdiff_t srcstride  = _srcstride >> 1

#define SRC_INIT_HV_10() SRC_INIT_HV_16()
#define SRC_INIT_HV_12() SRC_INIT_HV_16()
#define SRC_INIT_HV_14() SRC_INIT_HV_16()

#define SRC_INIT_WEIGHTED_HV_8()                                               \
    uint8_t *dst_bis  = dst;                                                   \
    uint8_t  *src_bis, *src = ((uint8_t*) _src) - 1;                           \
    ptrdiff_t srcstride = _srcstride

#define SRC_INIT_WEIGHTED_HV_16()                                              \
    uint16_t *dst_bis  = dst;                                                  \
    uint16_t *src_bis, *src = ((uint16_t*) _src) - 1;                          \
    ptrdiff_t srcstride  = _srcstride >> 1

#define SRC_INIT_WEIGHTED_HV_10() SRC_INIT_WEIGHTED_HV_16()
#define SRC_INIT_WEIGHTED_HV_12() SRC_INIT_WEIGHTED_HV_16()
#define SRC_INIT_WEIGHTED_HV_14() SRC_INIT_WEIGHTED_HV_16()

#define DST_INIT_8()                                                           \
    uint8_t *dst        = (uint8_t *) _dst;                                    \
    const int dststride = _dststride

#define DST_INIT_16()                                                          \
    uint16_t *dst       = (uint16_t *) _dst;                                   \
    const int dststride = _dststride >> 1

#define DST_INIT_10() DST_INIT_16()
#define DST_INIT_12() DST_INIT_16()
#define DST_INIT_14() DST_INIT_16()

#define UNI_INIT(D)                                                            \
    const __m256i offset = _mm256_set1_epi16(1 << (D + 1));                    \
    DST_INIT_ ## D()

#define BI_INIT(D)                                                             \
    const __m256i offset = _mm256_set1_epi16(1 << D);                          \
    DST_INIT_ ## D()

#define UNI_WEIGHTED_INIT(D)                                                   \
    const int shift2     = denom + 14 - D;                                     \
    const __m256i ox     = _mm256_set1_epi32(_ox << (D - 8));                  \
    const __m256i wx     = _mm256_set1_epi16(_wx);                             \
    const __m256i offset = _mm256_set1_epi32(1 << (shift2 - 1));               \
    DST_INIT_ ## D()

#define BI_WEIGHTED_INIT(D)                                                    \
    const int log2Wd     = denom + 14 - D;                                     \
    const int shift2     = log2Wd + 1;                                         \
    const int ox0        = _ox0 << (D - 8);                                    \
    const int ox1        = _ox1 << (D - 8);                                    \
    const __m256i wx0    = _mm256_set1_epi16(_wx0);                            \
    const __m256i wx1    = _mm256_set1_epi16(_wx1);                            \
    const __m256i offset = _mm256_set1_epi32( (ox0 + ox1 + 1) << log2Wd);      \
    DST_INIT_ ## D()

////////////////////////////////////////////////////////////////////////////////
// LOAD macros 
////////////////////////////////////////////////////////////////////////////////
#define MC_LOAD_PIXEL()                                                        \
    x1 = _mm256_loadu_si256((__m256i *) &src[x])

//EPEL LOAD
#define EPEL_LOAD_8(src, stride)                                               \
    x1 = _mm_loadl_epi64((__m128i *) &src[x             ]);                    \
    x2 = _mm_loadl_epi64((__m128i *) &src[x +     stride]);                    \
    x3 = _mm_loadl_epi64((__m128i *) &src[x + 2 * stride]);                    \
    x4 = _mm_loadl_epi64((__m128i *) &src[x + 3 * stride]);                    \
    x1 = _mm_unpacklo_epi8(x1, x2);                                            \
    x2 = _mm_unpacklo_epi8(x3, x4)

#define EPEL_LOAD_16(src, stride)                                              \
    x1 = _mm256_loadu_si256((__m256i *) &src[x             ]);                 \
    x2 = _mm256_loadu_si256((__m256i *) &src[x +     stride]);                 \
    x3 = _mm256_loadu_si256((__m256i *) &src[x + 2 * stride]);                 \
    x4 = _mm256_loadu_si256((__m256i *) &src[x + 3 * stride]);                 \
    t1 = _mm256_unpackhi_epi16(x1, x2);                                        \
    x1 = _mm256_unpacklo_epi16(x1, x2);                                        \
    t2 = _mm256_unpackhi_epi16(x3, x4);                                        \
    x2 = _mm256_unpacklo_epi16(x3, x4)

#define EPEL_LOAD_10(src, stride) EPEL_LOAD_16(src, stride)
#define EPEL_LOAD_12(src, stride) EPEL_LOAD_16(src, stride)
#define EPEL_LOAD_14(src, stride) EPEL_LOAD_16(src, stride)

//QPEL LOAD
#define QPEL_H_LOAD()                                                          \
    x1 = _mm256_loadu_si256((__m256i *) &src[x - 3]);                          \
    x2 = _mm256_loadu_si256((__m256i *) &src[x - 2]);                          \
    x3 = _mm256_loadu_si256((__m256i *) &src[x - 1]);                          \
    x4 = _mm256_loadu_si256((__m256i *) &src[x    ]);                          \
    x5 = _mm256_loadu_si256((__m256i *) &src[x + 1]);                          \
    x6 = _mm256_loadu_si256((__m256i *) &src[x + 2]);                          \
    x7 = _mm256_loadu_si256((__m256i *) &src[x + 3]);                          \
    x8 = _mm256_loadu_si256((__m256i *) &src[x + 4])

#define QPEL_V_LOAD()                                                          \
    x1 = _mm256_loadu_si256((__m256i *) &src[x - 3 * srcstride]);              \
    x2 = _mm256_loadu_si256((__m256i *) &src[x - 2 * srcstride]);              \
    x3 = _mm256_loadu_si256((__m256i *) &src[x - 1 * srcstride]);              \
    x4 = _mm256_loadu_si256((__m256i *) &src[x                ]);              \
    x5 = _mm256_loadu_si256((__m256i *) &src[x +     srcstride]);              \
    x6 = _mm256_loadu_si256((__m256i *) &src[x + 2 * srcstride]);              \
    x7 = _mm256_loadu_si256((__m256i *) &src[x + 3 * srcstride]);              \
    x8 = _mm256_loadu_si256((__m256i *) &src[x + 4 * srcstride])

#define QPEL_LOAD_LO_8(inst, src, srcstride)                                   \
    x1 = inst((__m256i *) &src[x - 3 * srcstride]);                            \
    x2 = inst((__m256i *) &src[x - 2 * srcstride]);                            \
    x3 = inst((__m256i *) &src[x - 1 * srcstride]);                            \
    x4 = inst((__m256i *) &src[x                ]);                            \
    x1 = _mm256_unpacklo_epi8(x1, x2);                                         \
    x2 = _mm256_unpacklo_epi8(x3, x4)

#define QPEL_LOAD_LO4_8(src, srcstride)  QPEL_LOAD_LO_8(_mm_loadl_epi64 , src, srcstride)
#define QPEL_LOAD_LO8_8(src, srcstride)  QPEL_LOAD_LO_8(_mm_loadl_epi64 , src, srcstride)

#define QPEL_LOAD_LO_16(inst, src, srcstride)                                  \
    x1 = inst((__m256i *) &src[x - 3 * srcstride]);                            \
    x2 = inst((__m256i *) &src[x - 2 * srcstride]);                            \
    x3 = inst((__m256i *) &src[x - 1 * srcstride]);                            \
    x4 = inst((__m256i *) &src[x                ]);                            \
    t1 = _mm256_unpackhi_epi16(x1, x2);                                        \
    x1 = _mm256_unpacklo_epi16(x1, x2);                                        \
    t2 = _mm256_unpackhi_epi16(x3, x4);                                        \
    x2 = _mm256_unpacklo_epi16(x3, x4)

#define QPEL_LOAD_LO_10(src, srcstride) QPEL_LOAD_LO_16(_mm256_loadu_si256, src, srcstride)

#define QPEL_LOAD_LO16_10(src, srcstride) QPEL_LOAD_LO_16(_mm256_loadu_si256, src, srcstride)
#define QPEL_LOAD_LO16_12(src, srcstride) QPEL_LOAD_LO_16(_mm256_loadu_si256, src, srcstride)
//???
#define QPEL_LOAD_LO8_14(src, srcstride)  QPEL_LOAD_LO_16(_mm_load_si128 , src, srcstride)

#define QPEL_LOAD_HI_8(inst, src, srcstride)                                   \
    x1 = inst((__m128i *) &src[x +     srcstride]);                            \
    x2 = inst((__m128i *) &src[x + 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x + 3 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x + 4 * srcstride]);                            \
    x1 = _mm_unpacklo_epi8(x1, x2);                                            \
    x2 = _mm_unpacklo_epi8(x3, x4)

#define QPEL_LOAD_HI4_8(src, srcstride)  QPEL_LOAD_HI_8(_mm_loadl_epi64 , src, srcstride)
#define QPEL_LOAD_HI8_8(src, srcstride)  QPEL_LOAD_HI_8(_mm_loadl_epi64 , src, srcstride)

#define QPEL_LOAD_HI_16(inst, src, srcstride)                                  \
    x1 = inst((__m256i *) &src[x +     srcstride]);                            \
    x2 = inst((__m256i *) &src[x + 2 * srcstride]);                            \
    x3 = inst((__m256i *) &src[x + 3 * srcstride]);                            \
    x4 = inst((__m256i *) &src[x + 4 * srcstride]);                            \
    t1 = _mm256_unpackhi_epi16(x1, x2);                                        \
    x1 = _mm256_unpacklo_epi16(x1, x2);                                        \
    t2 = _mm256_unpackhi_epi16(x3, x4);                                        \
    x2 = _mm256_unpacklo_epi16(x3, x4)

#define QPEL_LOAD_HI_10(src, srcstride) QPEL_LOAD_HI_16(_mm256_loadu_si256, src, srcstride)

#define QPEL_LOAD_HI16_10(src, srcstride) QPEL_LOAD_HI_16(_mm256_loadu_si256, src, srcstride)
#define QPEL_LOAD_HI16_12(src, srcstride) QPEL_LOAD_HI_16(_mm256_loadu_si256, src, srcstride)

#define QPEL_LOAD_HI8_14(src, srcstride)  QPEL_LOAD_HI_16(_mm_load_si128 ,    src, srcstride)

//QPEL LOAD 4
#define QPEL_LOAD_LO_4_10(inst, src, srcstride)                                \
    x1 = inst((__m128i *) &src[x - 3 * srcstride]);                            \
    x2 = inst((__m128i *) &src[x - 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x - 1 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x                ]);                            \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4)

#define QPEL_LOAD_LO4_10(src, srcstride) QPEL_LOAD_LO_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_LO4_12(src, srcstride) QPEL_LOAD_LO_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_LO4_14(src, srcstride) QPEL_LOAD_LO_4_10(_mm_loadl_epi64, src, srcstride)

#define QPEL_LOAD_HI_4_10(inst, src, srcstride)                                \
    x1 = inst((__m128i *) &src[x +     srcstride]);                            \
    x2 = inst((__m128i *) &src[x + 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x + 3 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x + 4 * srcstride]);                            \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4)

#define QPEL_LOAD_HI4_12(src, srcstride) QPEL_LOAD_HI_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_HI4_10(src, srcstride) QPEL_LOAD_HI_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_HI4_14(src, srcstride) QPEL_LOAD_HI_4_10(_mm_loadl_epi64, src, srcstride)

// Fixme; have a look at what it does considering each case
#define WEIGHTED_LOAD2_1()                                                     \
    r5 = _mm_loadl_epi64((__m128i *) &src2[x    ])

#define WEIGHTED_LOAD4_1()                                                     \
    WEIGHTED_LOAD2_1()

#define WEIGHTED_LOAD6_1()                                                     \
    r5 = _mm_load_si128((__m128i *) &src2[x    ])

#define WEIGHTED_LOAD16_1()                                                    \
    r5 = _mm256_load_si256((__m256i *) &src2[x    ])

#define WEIGHTED_LOAD32_1()                                                    \
    x9 = _mm256_load_si256((__m256i *) &src2[x    ]);                          \
    r6 = _mm256_load_si256((__m256i *) &src2[x + 16]);                         \
    r5 = _mm256_inserti128_si256(x9, _mm256_extracti128_si256(r6, 0), 1);      \
    r6 = _mm256_permute2f128_si256(r6, x9, 19)

////////////////////////////////////////////////////////////////////////////////
// COMPUTE macros
////////////////////////////////////////////////////////////////////////////////
#define MC_PIXEL_COMPUTE2_8()                                                  \
    x1 = _mm256_unpacklo_epi8(x1, c0);                                         \
    x1 = _mm256_slli_epi16(x1, 14 - 8)

#define MC_PIXEL_COMPUTE4_8()       MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE6_8()       MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE16_8()      MC_PIXEL_COMPUTE2_8()

#define MC_PIXEL_COMPUTE32_8()                                                 \
    x2 = _mm256_unpackhi_epi8(x1, c0);                                         \
    MC_PIXEL_COMPUTE2_8();                                                     \
    x2 = _mm256_slli_epi16(x2, 14 - 8)

#define MC_PIXEL_COMPUTE2_10()  x1 = _mm256_slli_epi16(x1, 14 - 10)

#define MC_PIXEL_COMPUTE4_10()  MC_PIXEL_COMPUTE2_10()
#define MC_PIXEL_COMPUTE6_10()  MC_PIXEL_COMPUTE2_10()
#define MC_PIXEL_COMPUTE16_10() MC_PIXEL_COMPUTE2_10()

#define MC_PIXEL_COMPUTE2_12()  x1 = _mm256_slli_epi16(x1, 14 - 12)

#define MC_PIXEL_COMPUTE4_12()  MC_PIXEL_COMPUTE2_12()
#define MC_PIXEL_COMPUTE6_12()  MC_PIXEL_COMPUTE2_12()
#define MC_PIXEL_COMPUTE16_12() MC_PIXEL_COMPUTE2_12()

// QPEL COMPUTE
#define QPEL_COMPUTE4_8(dst1, dst2, f1, f2)                                    \
    x1   = _mm_maddubs_epi16(x1, f1);                                          \
    x2   = _mm_maddubs_epi16(x2, f2);                                          \
    dst1 = _mm_add_epi16(x1, x2)

#define QPEL_COMPUTE8_8(dst1, dst2, f1, f2)   QPEL_COMPUTE4_8(dst1, dst2, f1, f2)

#define QPEL_COMPUTE4_10(dst1, dst2, f1, f2)                                   \
    x1   = _mm256_madd_epi16(x1, f1);                                          \
    x2   = _mm256_madd_epi16(x2, f2);                                          \
    dst1 = _mm256_add_epi32(x1, x2)

#define QPEL_COMPUTE8_10(dst1, dst2, f1, f2)  QPEL_COMPUTE4_10(dst1, dst2, f1, f2)

#define QPEL_COMPUTE2_12(dst1, dst2, f1, f2)  QPEL_COMPUTE2_10(dst1, dst2, f1, f2)
#define QPEL_COMPUTE4_12(dst1, dst2, f1, f2)  QPEL_COMPUTE4_10(dst1, dst2, f1, f2)

#define QPEL_COMPUTE2_14(dst1, dst2, f1, f2)  QPEL_COMPUTE2_10(dst1, dst2, f1, f2)
#define QPEL_COMPUTE4_14(dst1, dst2, f1, f2)  QPEL_COMPUTE4_10(dst1, dst2, f1, f2)

#define QPEL_COMPUTE16_10(dst1, dst2, f1, f2)                                  \
    t1   = _mm256_madd_epi16(t1, f1);                                          \
    t2   = _mm256_madd_epi16(t2, f2);                                          \
    dst2 = _mm256_add_epi32(t1, t2);                                           \
    QPEL_COMPUTE8_10(dst1, dst2, f1, f2)

#define QPEL_COMPUTE16_12(dst1, dst2, f1, f2)   QPEL_COMPUTE16_10(dst1, dst2, f1, f2)

#define QPEL_COMPUTE16_14(dst1, dst2, f1, f2)   QPEL_COMPUTE16_10(dst1, dst2, f1, f2)

#define QPEL_H_COMPUTE4_8() QPEL_H_COMPUTE8_8()

#define QPEL_H_COMPUTE16_8()                                                    \
    x1 = _mm256_unpacklo_epi8(x1, x2);                                          \
    x2 = _mm256_unpacklo_epi8(x3, x4);                                          \
    x3 = _mm256_unpacklo_epi8(x5, x6);                                          \
    x4 = _mm256_unpacklo_epi8(x7, x8);                                          \
    x2 = _mm256_maddubs_epi16(x2,c2);                                           \
    x3 = _mm256_maddubs_epi16(x3,c3);                                           \
    x1 = _mm256_maddubs_epi16(x1,c1);                                           \
    x4 = _mm256_maddubs_epi16(x4,c4);                                           \
    x1 = _mm256_add_epi16(x1, x2);                                              \
    x2 = _mm256_add_epi16(x3, x4);                                              \
    x1 = _mm256_add_epi16(x1, x2)

#define QPEL_H_COMPUTE4(shift)                                                 \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4);                                           \
    x3 = _mm_unpacklo_epi16(x5, x6);                                           \
    x4 = _mm_unpacklo_epi16(x7, x8);                                           \
    x2 = _mm_madd_epi16(x2,c2);                                                \
    x3 = _mm_madd_epi16(x3,c3);                                                \
    x1 = _mm_madd_epi16(x1,c1);                                                \
    x4 = _mm_madd_epi16(x4,c4);                                                \
    x1 = _mm_add_epi32(x1, x2);                                                \
    x2 = _mm_add_epi32(x3, x4);                                                \
    x1 = _mm_add_epi32(x1, x2);                                                \
    x1 = _mm_srai_epi32(x1, shift);                                            \
    x1 = _mm_packs_epi32(x1, c0)

#define QPEL_H_COMPUTE4_10()    QPEL_H_COMPUTE4(2)
#define QPEL_H_COMPUTE4_12()    QPEL_H_COMPUTE4(4)
#define QPEL_H_COMPUTE4_14()    QPEL_H_COMPUTE4(6)

#define QPEL_H_COMPUTE32_8()                                                   \
    x9 = x1;                                                                   \
    x1 = _mm256_unpacklo_epi8(x9, x2);                                         \
    x2 = _mm256_unpackhi_epi8(x9, x2);                                         \
    x9 = x3;                                                                   \
    x3 = _mm256_unpacklo_epi8(x9, x4);                                         \
    x4 = _mm256_unpackhi_epi8(x9, x4);                                         \
    x9 = x5;                                                                   \
    x5 = _mm256_unpacklo_epi8(x9, x6);                                         \
    x6 = _mm256_unpackhi_epi8(x9, x6);                                         \
    x9 = x7;                                                                   \
    x7 = _mm256_unpacklo_epi8(x9, x8);                                         \
    x8 = _mm256_unpackhi_epi8(x9, x8);                                         \
    x1 = _mm256_maddubs_epi16(x1,c1);                                          \
    x3 = _mm256_maddubs_epi16(x3,c2);                                          \
    x5 = _mm256_maddubs_epi16(x5,c3);                                          \
    x7 = _mm256_maddubs_epi16(x7,c4);                                          \
    x2 = _mm256_maddubs_epi16(x2,c1);                                          \
    x4 = _mm256_maddubs_epi16(x4,c2);                                          \
    x6 = _mm256_maddubs_epi16(x6,c3);                                          \
    x8 = _mm256_maddubs_epi16(x8,c4);                                          \
    x1 = _mm256_add_epi16(x1, x3);                                             \
    x3 = _mm256_add_epi16(x5, x7);                                             \
    x2 = _mm256_add_epi16(x2, x4);                                             \
    x4 = _mm256_add_epi16(x6, x8);                                             \
    x1 = _mm256_add_epi16(x1, x3);                                             \
    x2 = _mm256_add_epi16(x2, x4)

#define QPEL_H_COMPUTE16(shift)                                                \
    x9 = x1;                                                                   \
    x1 = _mm256_unpacklo_epi16(x9, x2);                                        \
    x2 = _mm256_unpackhi_epi16(x9, x2);                                        \
    x9 = x3;                                                                   \
    x3 = _mm256_unpacklo_epi16(x9, x4);                                        \
    x4 = _mm256_unpackhi_epi16(x9, x4);                                        \
    x9 = x5;                                                                   \
    x5 = _mm256_unpacklo_epi16(x9, x6);                                        \
    x6 = _mm256_unpackhi_epi16(x9, x6);                                        \
    x9 = x7;                                                                   \
    x7 = _mm256_unpacklo_epi16(x9, x8);                                        \
    x8 = _mm256_unpackhi_epi16(x9, x8);                                        \
    x1 = _mm256_madd_epi16(x1,c1);                                             \
    x3 = _mm256_madd_epi16(x3,c2);                                             \
    x5 = _mm256_madd_epi16(x5,c3);                                             \
    x7 = _mm256_madd_epi16(x7,c4);                                             \
    x2 = _mm256_madd_epi16(x2,c1);                                             \
    x4 = _mm256_madd_epi16(x4,c2);                                             \
    x6 = _mm256_madd_epi16(x6,c3);                                             \
    x8 = _mm256_madd_epi16(x8,c4);                                             \
    x1 = _mm256_add_epi32(x1, x3);                                             \
    x3 = _mm256_add_epi32(x5, x7);                                             \
    x2 = _mm256_add_epi32(x2, x4);                                             \
    x4 = _mm256_add_epi32(x6, x8);                                             \
    x1 = _mm256_add_epi32(x1, x3);                                             \
    x2 = _mm256_add_epi32(x2, x4);                                             \
    x1 = _mm256_srai_epi32(x1, shift);                                         \
    x2 = _mm256_srai_epi32(x2, shift);                                         \
    x1 = _mm256_packs_epi32(x1, x2)

#define QPEL_H_COMPUTE16_10()    QPEL_H_COMPUTE16(2)
#define QPEL_H_COMPUTE16_12()    QPEL_H_COMPUTE16(4)
#define QPEL_H_COMPUTE16_14()    QPEL_H_COMPUTE16(6)

#define EPEL_COMPUTE_8(dst, f1, f2)                                            \
    x1  = _mm_maddubs_epi16(x1, f1);                                           \
    x2  = _mm_maddubs_epi16(x2, f2);                                           \
    dst = _mm_add_epi16(x1, x2)

#define EPEL_COMPUTE(dst, f1, f2, shift)                                       \
    x1  = _mm256_madd_epi16(x1, f1);                                           \
    t1  = _mm256_madd_epi16(t1, f1);                                           \
    x2  = _mm256_madd_epi16(x2, f2);                                           \
    t2  = _mm256_madd_epi16(t2, f2);                                           \
    x1  = _mm256_add_epi32(x1, x2);                                            \
    t1  = _mm256_add_epi32(t1, t2);                                            \
    t1  = _mm256_srai_epi32(t1, shift);                                        \
    x1  = _mm256_srai_epi32(x1, shift);                                        \
    dst = _mm256_packs_epi32(x1, t1)

#define EPEL_COMPUTE_10(dst, f1, f2)    EPEL_COMPUTE(dst, f1, f2, 2)
#define EPEL_COMPUTE_12(dst, f1, f2)    EPEL_COMPUTE(dst, f1, f2, 4)
#define EPEL_COMPUTE_14(dst, f1, f2)    EPEL_COMPUTE(dst, f1, f2, 6)

//WEIGHTED COMPUTE
#define UNI_UNWEIGHTED_COMPUTE2(H)                                             \
    x1 = _mm256_mulhrs_epi16(x1, offset)

#define UNI_UNWEIGHTED_COMPUTE4(H)      UNI_UNWEIGHTED_COMPUTE2(H)
#define UNI_UNWEIGHTED_COMPUTE6(H)      UNI_UNWEIGHTED_COMPUTE2(H)
#define UNI_UNWEIGHTED_COMPUTE16(H)     UNI_UNWEIGHTED_COMPUTE2(H)

#define UNI_UNWEIGHTED_COMPUTE32(H)                                            \
    UNI_UNWEIGHTED_COMPUTE2(H);                                                \
    x2 = _mm256_mulhrs_epi16(x2, offset)

#define BI_UNWEIGHTED_COMPUTE2(H)                                              \
    WEIGHTED_LOAD ## H ## _1();                                                \
    x1 = _mm256_adds_epi16(x1, r5);                                            \
    x1 = _mm256_mulhrs_epi16(x1, offset)

#define BI_UNWEIGHTED_COMPUTE4(H)     BI_UNWEIGHTED_COMPUTE2(H)
#define BI_UNWEIGHTED_COMPUTE6(H)     BI_UNWEIGHTED_COMPUTE2(H)
#define BI_UNWEIGHTED_COMPUTE16(H)    BI_UNWEIGHTED_COMPUTE2(H)

#define BI_UNWEIGHTED_COMPUTE32(H)                                             \
    BI_UNWEIGHTED_COMPUTE2(H);                                                 \
    x2 = _mm256_adds_epi16(x2, r6);                                            \
    x2 = _mm256_mulhrs_epi16(x2, offset)

#define UNI_WEIGHTED_COMPUTE2(H)                                               \
    {                                                                          \
        __m256i x3, x4;                                                        \
        x3 = _mm256_mulhi_epi16(x1, wx);                                       \
        x1 = _mm256_mullo_epi16(x1, wx);                                       \
        x4 = _mm256_unpackhi_epi16(x1, x3);                                    \
        x1 = _mm256_unpacklo_epi16(x1, x3);                                    \
        x3 = _mm256_srai_epi32(_mm256_add_epi32(x4, offset), shift2);          \
        x1 = _mm256_srai_epi32(_mm256_add_epi32(x1, offset), shift2);          \
        x3 = _mm256_add_epi32(x3, ox);                                         \
        x1 = _mm256_add_epi32(x1, ox);                                         \
        x1 = _mm256_packs_epi32(x1, x3);                                       \
    }

#define UNI_WEIGHTED_COMPUTE4(H)      UNI_WEIGHTED_COMPUTE2(H)
#define UNI_WEIGHTED_COMPUTE6(H)      UNI_WEIGHTED_COMPUTE2(H)
#define UNI_WEIGHTED_COMPUTE16(H)     UNI_WEIGHTED_COMPUTE2(H)

#define UNI_WEIGHTED_COMPUTE32(H)                                              \
    UNI_WEIGHTED_COMPUTE2(H);                                                  \
    {                                                                          \
        __m256i x3, x4;                                                        \
        x3 = _mm256_mulhi_epi16(x2, wx);                                       \
        x2 = _mm256_mullo_epi16(x2, wx);                                       \
        x4 = _mm256_unpackhi_epi16(x2, x3);                                    \
        x2 = _mm256_unpacklo_epi16(x2, x3);                                    \
        x3 = _mm256_srai_epi32(_mm256_add_epi32(x4, offset), shift2);          \
        x2 = _mm256_srai_epi32(_mm256_add_epi32(x2, offset), shift2);          \
        x3 = _mm256_add_epi32(x3, ox);                                         \
        x2 = _mm256_add_epi32(x2, ox);                                         \
        x2 = _mm256_packs_epi32(x2, x3);                                       \
    }

#define BI_WEIGHTED_COMPUTE2(H)                                                \
     WEIGHTED_LOAD ## H ## _1();                                               \
    {                                                                          \
        __m256i x3, x4, r7, r8;                                                \
        x3 = _mm256_mulhi_epi16(x1, wx1);                                      \
        x1 = _mm256_mullo_epi16(x1, wx1);                                      \
        r7 = _mm256_mulhi_epi16(r5, wx0);                                      \
        r5 = _mm256_mullo_epi16(r5, wx0);                                      \
        x4 = _mm256_unpackhi_epi16(x1, x3);                                    \
        x1 = _mm256_unpacklo_epi16(x1, x3);                                    \
        r8 = _mm256_unpackhi_epi16(r5, r7);                                    \
        r5 = _mm256_unpacklo_epi16(r5, r7);                                    \
        x4 = _mm256_add_epi32(x4, r8);                                         \
        x1 = _mm256_add_epi32(x1, r5);                                         \
        x4 = _mm256_srai_epi32(_mm256_add_epi32(x4, offset), shift2);          \
        x1 = _mm256_srai_epi32(_mm256_add_epi32(x1, offset), shift2);          \
        x1 = _mm256_packs_epi32(x1, x4);                                       \
    }
#define BI_WEIGHTED_COMPUTE4(H)      BI_WEIGHTED_COMPUTE2(H)
#define BI_WEIGHTED_COMPUTE6(H)      BI_WEIGHTED_COMPUTE2(H)
#define BI_WEIGHTED_COMPUTE16(H)     BI_WEIGHTED_COMPUTE2(H)

#define BI_WEIGHTED_COMPUTE32(H)                                               \
    BI_WEIGHTED_COMPUTE2(H);                                                   \
    {                                                                          \
        __m256i x3, x4, r7, r8;                                                \
        x3 = _mm256_mulhi_epi16(x2, wx1);                                      \
        x2 = _mm256_mullo_epi16(x2, wx1);                                      \
        r7 = _mm256_mulhi_epi16(r6, wx0);                                      \
        r6 = _mm256_mullo_epi16(r6, wx0);                                      \
        x4 = _mm256_unpackhi_epi16(x2, x3);                                    \
        x2 = _mm256_unpacklo_epi16(x2, x3);                                    \
        r8 = _mm256_unpackhi_epi16(r6, r7);                                    \
        r6 = _mm256_unpacklo_epi16(r6, r7);                                    \
        x4 = _mm256_add_epi32(x4, r8);                                         \
        x2 = _mm256_add_epi32(x2, r6);                                         \
        x4 = _mm256_srai_epi32(_mm256_add_epi32(x4, offset), shift2);          \
        x2 = _mm256_srai_epi32(_mm256_add_epi32(x2, offset), shift2);          \
        x2 = _mm256_packs_epi32(x2, x4);                                       \
    }

////////////////////////////////////////////////////////////////////////////////
// MERGE MACROS
////////////////////////////////////////////////////////////////////////////////
#define QPEL_MERGE4_8()                                                        \
    x1 = _mm_add_epi16(r1, r3)

#define QPEL_MERGE8_8()                         QPEL_MERGE4_8()

#define QPEL_MERGE2_16()                                                       \
    x1 = _mm_add_epi32(r1,r3);                                                 \
    x1 = _mm_srai_epi32(x1, shift);                                            \
    x1 = _mm_packs_epi32(x1, c0)

#define QPEL_MERGE2_10()                        QPEL_MERGE2_16()
#define QPEL_MERGE4_10()                        QPEL_MERGE2_16()

#define QPEL_MERGE2_12()                        QPEL_MERGE2_16()
#define QPEL_MERGE4_12()                        QPEL_MERGE2_16()

#define QPEL_MERGE2_14()                        QPEL_MERGE2_16()
#define QPEL_MERGE4_14()                        QPEL_MERGE2_16()

#define QPEL_MERGE16_16()                                                      \
    x1 = _mm256_add_epi32(r1, r3);                                             \
    x2 = _mm256_add_epi32(r2, r4);                                             \
    x1 = _mm256_srai_epi32(x1, shift);                                         \
    x2 = _mm256_srai_epi32(x2, shift);                                         \
    x1 = _mm256_packs_epi32(x1, x2)

#define QPEL_MERGE16_10()                       QPEL_MERGE16_16()
#define QPEL_MERGE16_12()                       QPEL_MERGE16_16()
#define QPEL_MERGE16_14()                       QPEL_MERGE16_16()


////////////////////////////////////////////////////////////////////////////////
// PEL STORE //fixme: some macros names are irrelevant
////////////////////////////////////////////////////////////////////////////////
#define PEL_STORE_2(tab)                                                       \
    *((uint32_t *) &tab[x]) = _mm_cvtsi128_si32(x1)

#define PEL_STORE_4(tab)                                                       \
    _mm_store_si128((__m128i *) &tab[x], _mm256_extracti128_si256(x1, 0))

#define PEL_STORE_6(tab)                                                       \
    _mm_storel_epi64((__m128i *) &tab[x], x1);                                 \
    *((uint32_t *) &tab[x+4]) = _mm_cvtsi128_si32(_mm_srli_si128(x1, 8))

#define PEL_STORE_16(tab)                                                      \
    _mm256_store_si256((__m256i *) &tab[x], x1)

#define PEL_STORE_32(tab)                                                      \
    x9 = _mm256_inserti128_si256(x1, _mm256_extracti128_si256(x2, 0), 1);      \
    x2 = _mm256_permute2f128_si256(x2, x1, 19);                                \
    _mm256_store_si256((__m256i *) &tab[x    ], x9);                           \
    _mm256_store_si256((__m256i *) &tab[x + 16], x2)

////////////////////////////////////////////////////////////////////////////////
// Fixme; have a look at what it does considering each case
////////////////////////////////////////////////////////////////////////////////
#define WEIGHTED_STORE2_8()                                                    \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    *((short *) (dst + x)) = _mm_extract_epi16(x1, 0)

#define WEIGHTED_STORE4_8()                                                    \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    PEL_STORE_2(dst)

#define WEIGHTED_STORE6_8()                                                    \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    PEL_STORE_2(dst);                                                          \
    *((short *) (dst + x + 4)) = _mm_extract_epi16(x1, 2)

#define WEIGHTED_STORE16_8()                                                   \
    x1 = _mm256_packus_epi16(x1, x1);                                          \
    PEL_STORE_4(dst)

#define WEIGHTED_STORE32_8()                                                   \
    x1 = _mm256_packus_epi16(x1, x2);                                          \
    PEL_STORE_16(dst)

#define WEIGHTED_STORE14_16_8()                                                \
    x1 = _mm256_packus_epi16(x1, x1);                                          \
    x1 = _mm256_permute4x64_epi64(x1, 0xD8);                                   \
    PEL_STORE_4(dst)

#define WEIGHTED_STORE14_32_8()                                                \
    x1 = _mm256_packus_epi16(x1, x2);                                          \
    x1 = _mm256_permute4x64_epi64(x1, 0xD8);                                   \
    PEL_STORE_16(dst)

#define WEIGHTED_STORE2_8()                                                    \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    *((short *) (dst + x)) = _mm_extract_epi16(x1, 0)

#define WEIGHTED_STORE4_8()                                                    \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    PEL_STORE_2(dst)

#define WEIGHTED_STORE6_8()                                                    \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    PEL_STORE_2(dst);                                                          \
    *((short *) (dst + x + 4)) = _mm_extract_epi16(x1, 2)

#define WEIGHTED_STORE16_8()                                                   \
    x1 = _mm256_packus_epi16(x1, x1);                                          \
    PEL_STORE_4(dst)

#define WEIGHTED_STORE32_8()                                                   \
    x1 = _mm256_packus_epi16(x1, x2);                                          \
    PEL_STORE_16(dst)


#define WEIGHTED_STORE2(D)                                                     \
    x1 = _mm_max_epi16(x1, _mm_setzero_si128());                               \
    x1 = _mm_min_epi16(x1, _mm_set1_epi16(CLPI_PIXEL_MAX_## D));               \
    PEL_STORE_2(dst)

#define WEIGHTED_STORE4(D)                                                     \
    x1 = _mm_max_epi16(x1, _mm_setzero_si128());                               \
    x1 = _mm_min_epi16(x1, _mm_set1_epi16(CLPI_PIXEL_MAX_## D));               \
    PEL_STORE_4(dst)

#define WEIGHTED_STORE6(D)                                                     \
    x1 = _mm_max_epi16(x1, _mm_setzero_si128());                               \
    x1 = _mm_min_epi16(x1, _mm_set1_epi16(CLPI_PIXEL_MAX_## D));               \
    PEL_STORE_6(dst)

#define WEIGHTED_STORE16(D)                                                    \
    x1 = _mm256_max_epi16(x1, _mm256_setzero_si256());                         \
    x1 = _mm256_min_epi16(x1, _mm256_set1_epi16(CLPI_PIXEL_MAX_## D));         \
    PEL_STORE_16(dst)

#define WEIGHTED_STORE32(D)                                                    \
    x1 = _mm256_max_epi16(x1, _mm256_setzero_si256());                         \
    x1 = _mm256_min_epi16(x1, _mm256_set1_epi16(CLPI_PIXEL_MAX_## D));         \
    x2 = _mm256_max_epi16(x2, _mm256_setzero_si256());                         \
    x2 = _mm256_min_epi16(x2, _mm256_set1_epi16(CLPI_PIXEL_MAX_## D));         \
    PEL_STORE_32(dst)

#define WEIGHTED_STORE4_10()      WEIGHTED_STORE4(10)
#define WEIGHTED_STORE6_10()      WEIGHTED_STORE6(10)
#define WEIGHTED_STORE16_10()     WEIGHTED_STORE16(10)
#define WEIGHTED_STORE14_16_10()  WEIGHTED_STORE16(10)

#define WEIGHTED_STORE4_12()      WEIGHTED_STORE4(12)
#define WEIGHTED_STORE6_12()      WEIGHTED_STORE6(12)
#define WEIGHTED_STORE16_12()     WEIGHTED_STORE16(12)
#define WEIGHTED_STORE14_16_12()  WEIGHTED_STORE16(12)

#define WEIGHTED_STORE4_14()      WEIGHTED_STORE4_10()
#define WEIGHTED_STORE6_14()      WEIGHTED_STORE6_10()
#define WEIGHTED_STORE16_14()     WEIGHTED_STORE16_10()
#define WEIGHTED_STORE14_16_14()  WEIGHTED_STORE16_10()

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define MUL_ADD_H_2(mul, add, dst, src)                                        \
    src ## 1 = mul(src ## 1, r0);                                              \
    src ## 2 = mul(src ## 2, r0);                                              \
    dst      = add(src ## 1, src ## 2);                                        \
    dst      = add(dst, c0)

////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_mc_pixelsX_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_PEL_PIXELS(H, D)                                              \
void ohhevc_put_hevc_pel_pixels ## H ## _ ## D ## _avx2_ (                    \
                                   int16_t *dst,                               \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int height,                                 \
                                   intptr_t mx, intptr_t my, int width) {      \
    int x, y;                                                                  \
    PUT_HEVC_PEL_PIXELS_VAR ## H ## _ ## D();                                  \
    SRC_INIT_ ## D();                                                          \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += MAX_PB_SIZE;                                                    \
    }                                                                          \
}

#define PUT_HEVC_BI_PEL_PIXELS(H, D)                                           \
void ohhevc_put_hevc_bi_pel_pixels ## H ## _ ## D ## _avx2_ (                 \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_PEL_PIXELS_VAR ## H ## _ ## D();                                  \
    SRC_INIT_ ## D();                                                          \
    BI_INIT(D);                                                                \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            BI_UNWEIGHTED_COMPUTE ## H(H);                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_PEL_PIXELS(H, D)                                          \
void ohhevc_put_hevc_uni_pel_pixels ## H ## _ ## D ## _avx2_ (                \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int y;                                                                     \
    SRC_INIT_ ## D();                                                          \
    DST_INIT_ ## D();                                                          \
    for (y = 0; y < height; y++) {                                             \
        memcpy(dst, src, width * ((D+7) / 8));                                 \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_PEL_PIXELS(H, D)                                        \
void ohhevc_put_hevc_uni_w_pel_pixels ## H ## _ ## D ## _avx2_ (              \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_PEL_PIXELS_VAR ## H ## _ ## D();                                  \
    SRC_INIT_ ## D();                                                          \
    UNI_WEIGHTED_INIT(D);                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            UNI_WEIGHTED_COMPUTE ## H(H);                                      \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_W_PEL_PIXELS(H, D)                                         \
void ohhevc_put_hevc_bi_w_pel_pixels ## H ## _ ## D ## _avx2_ (               \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_PEL_PIXELS_VAR ## H ## _ ## D();                                  \
    SRC_INIT_ ## D();                                                          \
    BI_WEIGHTED_INIT(D);                                                       \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            BI_WEIGHTED_COMPUTE ## H(H);                                       \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_epel_hX_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_EPEL_H(H, D)                                                  \
void ohhevc_put_hevc_epel_h ## H ## _ ## D ## _avx2_ (                        \
        int16_t *dst,                                                          \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_H_ ## D();                                                        \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += MAX_PB_SIZE;                                                    \
    }                                                                          \
}

#define PUT_HEVC_BI_EPEL_H(H, D)                                               \
void ohhevc_put_hevc_bi_epel_h ## H ## _ ## D ## _avx2_ (                     \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_H_ ## D();                                                        \
    BI_INIT(D);                                                                \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            BI_UNWEIGHTED_COMPUTE ## H(H);                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_EPEL_H(H, D)                                              \
void ohhevc_put_hevc_uni_epel_h ## H ## _ ## D ## _avx2_ (                    \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_H_ ## D();                                                        \
    UNI_INIT(D);                                                               \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            UNI_UNWEIGHTED_COMPUTE ## H(H);                                    \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_EPEL_H(H, D)                                            \
void ohhevc_put_hevc_uni_w_epel_h ## H ## _ ## D ## _avx2_ (                  \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_H_ ## D();                                                        \
    UNI_WEIGHTED_INIT(D);                                                      \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            UNI_WEIGHTED_COMPUTE ## H(H);                                      \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_W_EPEL_H(H, D)                                             \
void ohhevc_put_hevc_bi_w_epel_h ## H ## _ ## D ## _avx2_ (                   \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_H_ ## D();                                                        \
    BI_WEIGHTED_INIT(D);                                                       \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            BI_WEIGHTED_COMPUTE ## H(H);                                       \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_epel_vX_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_EPEL_V(H, D)                                                  \
void ohhevc_put_hevc_epel_v ## H ## _ ## D ## _avx2_ (                        \
        int16_t *dst,                                                          \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_V_VAR ## H ## _ ## D();                                      \
    SRC_INIT_V_ ## D();                                                        \
    EPEL_FILTER_ ## D(f1, f2, my - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, srcstride);                                   \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += MAX_PB_SIZE;                                                    \
    }                                                                          \
}

#define PUT_HEVC_BI_EPEL_V(H, D)                                               \
void ohhevc_put_hevc_bi_epel_v ## H ## _ ## D ## _avx2_ (                     \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_V_VAR ## H ## _ ## D();                                      \
    SRC_INIT_V_ ## D();                                                        \
    BI_INIT(D);                                                                \
    EPEL_FILTER_ ## D(f1, f2, my - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            EPEL_LOAD_ ## D(src, srcstride);                                   \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            BI_UNWEIGHTED_COMPUTE ## H(H);                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_EPEL_V(H, D)                                              \
void ohhevc_put_hevc_uni_epel_v ## H ## _ ## D ## _avx2_ (                    \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_V_VAR ## H ## _ ## D();                                      \
    SRC_INIT_V_ ## D();                                                        \
    UNI_INIT(D);                                                               \
    EPEL_FILTER_ ## D(f1, f2, my - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, srcstride);                                   \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            UNI_UNWEIGHTED_COMPUTE ## H(H);                                    \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_EPEL_V(H, D)                                            \
void ohhevc_put_hevc_uni_w_epel_v ## H ## _ ## D ## _avx2_ (                  \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_V_VAR ## H ## _ ## D();                                      \
    SRC_INIT_V_ ## D();                                                        \
    UNI_WEIGHTED_INIT(D);                                                      \
    EPEL_FILTER_ ## D(f1, f2, my - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, srcstride);                                   \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            UNI_WEIGHTED_COMPUTE ## H(H);                                      \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_W_EPEL_V(H, D)                                             \
void ohhevc_put_hevc_bi_w_epel_v ## H ## _ ## D ## _avx2_ (                   \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_V_VAR ## H ## _ ## D();                                      \
    SRC_INIT_V_ ## D();                                                        \
    BI_WEIGHTED_INIT(D);                                                       \
    EPEL_FILTER_ ## D(f1, f2, my - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            EPEL_LOAD_ ## D(src, srcstride);                                   \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            BI_WEIGHTED_COMPUTE ## H(H);                                       \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_epel_hvX_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_EPEL_HV(H, D)                                                 \
void ohhevc_put_hevc_epel_hv ## H ## _ ## D ## _avx2_ (                       \
        int16_t *dst,                                                          \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    int x, y;                                                                  \
    __m256i x1, x2, x3, x4, t1, t2, f1, f2, f3, f4, r1, r2, r3, r4;            \
    int16_t *dst_bis = dst;                                                    \
    SRC_INIT_HV_ ## D();                                                       \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
    src_bis = src;                                                             \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    EPEL_FILTER_10(f3, f4, my - 1);                                            \
    for (x = 0; x < width; x += H) {                                           \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r1, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r2, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r3, f1, f2);                                        \
        src += srcstride;                                                      \
        for (y = 0; y < height; y++) {                                         \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(r4, f1, f2);                                    \
            src += srcstride;                                                  \
                                                                               \
            t1 = _mm256_unpackhi_epi16(r1, r2);                                \
            x1 = _mm256_unpacklo_epi16(r1, r2);                                \
            t2 = _mm256_unpackhi_epi16(r3, r4);                                \
            x2 = _mm256_unpacklo_epi16(r3, r4);                                \
            EPEL_COMPUTE_14(x1, f3, f4);                                       \
            PEL_STORE_ ## H(dst);                                              \
            dst += MAX_PB_SIZE;                                                \
                                                                               \
            r1 = r2;                                                           \
            r2 = r3;                                                           \
            r3 = r4;                                                           \
        }                                                                      \
        src = src_bis;                                                         \
        dst = dst_bis;                                                         \
    }                                                                          \
}

#define PUT_HEVC_BI_EPEL_HV(H, D)                                              \
void ohhevc_put_hevc_bi_epel_hv ## H ## _ ## D ## _avx2_ (                    \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    __m256i x1, x2, x3, x4, t1, t2, f1, f2, f3, f4, r1, r2, r3, r4;            \
    BI_INIT(D);                                                                \
    int16_t *src2_bis = src2;                                                  \
    SRC_INIT_WEIGHTED_HV_ ## D();                                              \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
    src_bis = src;                                                             \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    EPEL_FILTER_10(f3, f4, my - 1);                                            \
    for (x = 0; x < width; x += H) {                                           \
        PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r1, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r2, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r3, f1, f2);                                        \
        src += srcstride;                                                      \
        for (y = 0; y < height; y++) {                                         \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(r4, f1, f2);                                    \
            src += srcstride;                                                  \
                                                                               \
            t1 = _mm256_unpackhi_epi16(r1, r2);                                \
            x1 = _mm256_unpacklo_epi16(r1, r2);                                \
            t2 = _mm256_unpackhi_epi16(r3, r4);                                \
            x2 = _mm256_unpacklo_epi16(r3, r4);                                \
            EPEL_COMPUTE_14(x1, f3, f4);                                       \
            BI_UNWEIGHTED_COMPUTE ## H(H);                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
            src2 += MAX_PB_SIZE;                                               \
            dst  += dststride;                                                 \
                                                                               \
            r1 = r2;                                                           \
            r2 = r3;                                                           \
            r3 = r4;                                                           \
        }                                                                      \
        src  = src_bis;                                                        \
        src2 = src2_bis;                                                       \
        dst  = dst_bis;                                                        \
    }                                                                          \
}

#define PUT_HEVC_UNI_EPEL_HV(H, D)                                             \
void ohhevc_put_hevc_uni_epel_hv ## H ## _ ## D ## _avx2_ (                   \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    __m256i x1, x2, x3, x4, t1, t2, f1, f2, f3, f4, r1, r2, r3, r4;            \
    UNI_INIT(D);                                                               \
    SRC_INIT_WEIGHTED_HV_ ## D();                                              \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
    src_bis = src;                                                             \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    EPEL_FILTER_10(f3, f4, my - 1);                                            \
    for (x = 0; x < width; x += H) {                                           \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r1, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r2, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r3, f1, f2);                                        \
        src += srcstride;                                                      \
        for (y = 0; y < height; y++) {                                         \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(r4, f1, f2);                                    \
            src += srcstride;                                                  \
                                                                               \
            t1 = _mm256_unpackhi_epi16(r1, r2);                                \
            x1 = _mm256_unpacklo_epi16(r1, r2);                                \
            t2 = _mm256_unpackhi_epi16(r3, r4);                                \
            x2 = _mm256_unpacklo_epi16(r3, r4);                                \
            EPEL_COMPUTE_14(x1, f3, f4);                                       \
            UNI_UNWEIGHTED_COMPUTE ## H(H);                                    \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
            dst  += dststride;                                                 \
                                                                               \
            r1 = r2;                                                           \
            r2 = r3;                                                           \
            r3 = r4;                                                           \
        }                                                                      \
        src  = src_bis;                                                        \
        dst  = dst_bis;                                                        \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_EPEL_HV(H, D)                                           \
void ohhevc_put_hevc_uni_w_epel_hv ## H ## _ ## D ## _avx2_ (                 \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    __m256i x1, x2, x3, x4, t1, t2, f1, f2, f3, f4, r1, r2, r3, r4;            \
    UNI_WEIGHTED_INIT(D);                                                      \
    SRC_INIT_WEIGHTED_HV_ ## D();                                              \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
    src_bis = src;                                                             \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    EPEL_FILTER_10(f3, f4, my - 1);                                            \
    for (x = 0; x < width; x += H) {                                           \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r1, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r2, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r3, f1, f2);                                        \
        src += srcstride;                                                      \
        for (y = 0; y < height; y++) {                                         \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(r4, f1, f2);                                    \
            src += srcstride;                                                  \
                                                                               \
            t1 = _mm256_unpackhi_epi16(r1, r2);                                \
            x1 = _mm256_unpacklo_epi16(r1, r2);                                \
            t2 = _mm256_unpackhi_epi16(r3, r4);                                \
            x2 = _mm256_unpacklo_epi16(r3, r4);                                \
            EPEL_COMPUTE_14(x1, f3, f4);                                       \
            UNI_WEIGHTED_COMPUTE ## H(H);                                      \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
            dst  += dststride;                                                 \
                                                                               \
            r1 = r2;                                                           \
            r2 = r3;                                                           \
            r3 = r4;                                                           \
        }                                                                      \
        src  = src_bis;                                                        \
        dst  = dst_bis;                                                        \
    }                                                                          \
}

#define PUT_HEVC_BI_W_EPEL_HV(H, D)                                            \
void ohhevc_put_hevc_bi_w_epel_hv ## H ## _ ## D ## _avx2_ (                  \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    __m256i x1, x2, x3, x4, t1, t2, f1, f2, f3, f4, r1, r2, r3, r4;            \
    BI_WEIGHTED_INIT(D);                                                       \
    int16_t *src2_bis = src2;                                                  \
    SRC_INIT_WEIGHTED_HV_ ## D();                                              \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
    src_bis = src;                                                             \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    EPEL_FILTER_10(f3, f4, my - 1);                                            \
    for (x = 0; x < width; x += H) {                                           \
        PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r1, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r2, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r3, f1, f2);                                        \
        src += srcstride;                                                      \
        for (y = 0; y < height; y++) {                                         \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(r4, f1, f2);                                    \
            src += srcstride;                                                  \
                                                                               \
            t1 = _mm256_unpackhi_epi16(r1, r2);                                \
            x1 = _mm256_unpacklo_epi16(r1, r2);                                \
            t2 = _mm256_unpackhi_epi16(r3, r4);                                \
            x2 = _mm256_unpacklo_epi16(r3, r4);                                \
            EPEL_COMPUTE_14(x1, f3, f4);                                       \
            BI_WEIGHTED_COMPUTE ## H(H);                                       \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
            src2 += MAX_PB_SIZE;                                               \
            dst  += dststride;                                                 \
                                                                               \
            r1 = r2;                                                           \
            r2 = r3;                                                           \
            r3 = r4;                                                           \
        }                                                                      \
        src  = src_bis;                                                        \
        src2 = src2_bis;                                                       \
        dst  = dst_bis;                                                        \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_qpel_hX_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_H(H, D)                                                  \
void ohhevc_put_hevc_qpel_h ## H ## _ ## D ## _avx2_ (                        \
                                    int16_t *dst,                              \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int height, intptr_t mx, intptr_t my, int width) {   \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    QPEL_H_FILTER_ ## D(mx - 1);                                               \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_H_LOAD();                                                     \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += MAX_PB_SIZE;                                                    \
    }                                                                          \
}

#define PUT_HEVC_BI_QPEL_H(H, D)                                               \
void ohhevc_put_hevc_bi_qpel_h ## H ## _ ## D ## _avx2_ (                     \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    BI_INIT(D);                                                                \
    QPEL_H_FILTER_ ## D(mx - 1);                                               \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            QPEL_H_LOAD();                                                     \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            BI_UNWEIGHTED_COMPUTE ## H(H);                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_QPEL_H(H, D)                                              \
void ohhevc_put_hevc_uni_qpel_h ## H ## _ ## D ## _avx2_ (                    \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    UNI_INIT(D);                                                               \
    QPEL_H_FILTER_ ## D(mx - 1);                                               \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_H_LOAD();                                                     \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            UNI_UNWEIGHTED_COMPUTE ## H(H);                                    \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_QPEL_H(H, D)                                            \
void ohhevc_put_hevc_uni_w_qpel_h ## H ## _ ## D ## _avx2_ (                  \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    UNI_WEIGHTED_INIT(D);                                                      \
    QPEL_H_FILTER_ ## D(mx - 1);                                               \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_H_LOAD();                                                     \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            UNI_WEIGHTED_COMPUTE ## H(H);                                      \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_W_QPEL_H(H, D)                                             \
void ohhevc_put_hevc_bi_w_qpel_h ## H ## _ ## D ## _avx2_ (                   \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    BI_WEIGHTED_INIT(D);                                                       \
    QPEL_H_FILTER_ ## D(mx - 1);                                               \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            QPEL_H_LOAD();                                                     \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            BI_WEIGHTED_COMPUTE ## H(H);                                       \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_qpel_hX_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_H_10(H, D)                                               \
void ohhevc_put_hevc_qpel_h ## H ## _ ## D ## _avx2_ (                        \
                                    int16_t *dst,                              \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int height, intptr_t mx, intptr_t my, int width) {   \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    PUT_HEVC_QPEL_H_10_VAR ## H ## _ ## D();                                   \
    SRC_INIT_ ## D();                                                          \
    QPEL_FILTER_ ## D(mx - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_LOAD_LO ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r1, r2, c1, c2);                       \
            QPEL_LOAD_HI ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r3, r4, c3, c4);                       \
            QPEL_MERGE ## H ## _ ## D();                                       \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += MAX_PB_SIZE;                                                    \
    }                                                                          \
}

#define PUT_HEVC_BI_QPEL_H_10(H, D)                                            \
void ohhevc_put_hevc_bi_qpel_h ## H ## _ ## D ## _avx2_ (                     \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                 \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    PUT_HEVC_QPEL_H_10_VAR ## H ## _ ## D();                                   \
    SRC_INIT_ ## D();                                                          \
    BI_INIT(D);                                                                \
    QPEL_FILTER_ ## D(mx - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            QPEL_LOAD_LO ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r1, r2, c1, c2);                       \
            QPEL_LOAD_HI ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r3, r4, c3, c4);                       \
            QPEL_MERGE ## H ## _ ## D();                                       \
            BI_UNWEIGHTED_COMPUTE ## H(H);                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_QPEL_H_10(H, D)                                           \
void ohhevc_put_hevc_uni_qpel_h ## H ## _ ## D ## _avx2_ (                    \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    PUT_HEVC_QPEL_H_10_VAR ## H ## _ ## D();                                   \
    SRC_INIT_ ## D();                                                          \
    UNI_INIT(D);                                                               \
    QPEL_FILTER_ ## D(mx - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_LOAD_LO ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r1, r2, c1, c2);                       \
            QPEL_LOAD_HI ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r3, r4, c3, c4);                       \
            QPEL_MERGE ## H ## _ ## D();                                       \
            UNI_UNWEIGHTED_COMPUTE ## H(H);                                    \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_QPEL_H_10(H, D)                                         \
void ohhevc_put_hevc_uni_w_qpel_h ## H ## _ ## D ## _avx2_ (                  \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    PUT_HEVC_QPEL_H_10_VAR ## H ## _ ## D();                                   \
    SRC_INIT_ ## D();                                                          \
    UNI_WEIGHTED_INIT(D);                                                      \
    QPEL_FILTER_ ## D(mx - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_LOAD_LO ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r1, r2, c1, c2);                       \
            QPEL_LOAD_HI ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r3, r4, c3, c4);                       \
            QPEL_MERGE ## H ## _ ## D();                                       \
            UNI_WEIGHTED_COMPUTE ## H(H);                                      \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_W_QPEL_H_10(H, D)                                          \
void ohhevc_put_hevc_bi_w_qpel_h ## H ## _ ## D ## _avx2_ (                   \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    PUT_HEVC_QPEL_H_10_VAR ## H ## _ ## D();                                   \
    SRC_INIT_ ## D();                                                          \
    BI_WEIGHTED_INIT(D);                                                       \
    QPEL_FILTER_ ## D(mx - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## H ## _ ## D();                  \
            QPEL_LOAD_LO ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r1, r2, c1, c2);                       \
            QPEL_LOAD_HI ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r3, r4, c3, c4);                       \
            QPEL_MERGE ## H ## _ ## D();                                       \
            BI_WEIGHTED_COMPUTE ## H(H);                                       \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_qpel_vX_X_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_V(V, D)                                                  \
void ohhevc_put_hevc_qpel_v ## V ## _ ## D ## _avx2_ (                        \
                                    int16_t *dst,                              \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int height, intptr_t mx, intptr_t my, int width) {   \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    QPEL_FILTER_ ## D(my - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _ ## D();                                   \
            PEL_STORE_ ## V(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += MAX_PB_SIZE;                                                    \
    }                                                                          \
}

#define PUT_HEVC_BI_QPEL_V(V, D)                                               \
void ohhevc_put_hevc_bi_qpel_v ## V ## _ ## D ## _avx2_ (                     \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    BI_INIT(D);                                                                \
    QPEL_FILTER_ ## D(my - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## V ## _ ## D();                  \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _ ## D();                                   \
            BI_UNWEIGHTED_COMPUTE ## V(V);                                     \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_QPEL_V(V, D)                                              \
void ohhevc_put_hevc_uni_qpel_v ## V ## _ ## D ## _avx2_ (                    \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    UNI_INIT(D);                                                               \
    QPEL_FILTER_ ## D(my - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _ ## D();                                   \
            UNI_UNWEIGHTED_COMPUTE ## V(V);                                    \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_QPEL_V(V, D)                                            \
void ohhevc_put_hevc_uni_w_qpel_v ## V ## _ ## D ## _avx2_ (                  \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    UNI_WEIGHTED_INIT(D);                                                      \
    QPEL_FILTER_ ## D(my - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _ ## D();                                   \
            UNI_WEIGHTED_COMPUTE ## V(V);                                      \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_W_QPEL_V(V, D)                                             \
void ohhevc_put_hevc_bi_w_qpel_v ## V ## _ ## D ## _avx2_ (                   \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    BI_WEIGHTED_INIT(D);                                                       \
    QPEL_FILTER_ ## D(my - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## V ## _ ## D();                  \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _ ## D();                                   \
            BI_WEIGHTED_COMPUTE ## V(V);                                       \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_QPEL_V14(V, D)                                             \
void ohhevc_put_hevc_bi_qpel_v ## V ## _14_ ## D ## _avx2_ (                  \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _14();                                         \
    SRC_INIT_14();                                                             \
    BI_INIT(D);                                                                \
    QPEL_FILTER_14(my - 1);                                                    \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## V ## _ ## D();                  \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _14();                                      \
            BI_UNWEIGHTED_COMPUTE ## V(V);                                     \
            WEIGHTED_STORE14_ ## V ## _ ## D();                                \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_QPEL_V14(V, D)                                            \
void ohhevc_put_hevc_uni_qpel_v ## V ## _14_ ## D ## _avx2_ (                 \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _14();                                         \
    SRC_INIT_14();                                                             \
    UNI_INIT(D);                                                               \
    QPEL_FILTER_14(my - 1);                                                    \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _14();                                      \
            UNI_UNWEIGHTED_COMPUTE ## V(V);                                    \
            WEIGHTED_STORE14_ ## V ## _ ## D();                                \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_UNI_W_QPEL_V14(V, D)                                          \
void ohhevc_put_hevc_uni_w_qpel_v ## V ## _14_ ## D ## _avx2_ (               \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int _wx, int _ox,                      \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _14();                                         \
    SRC_INIT_14();                                                             \
    UNI_WEIGHTED_INIT(D);                                                      \
    QPEL_FILTER_14(my - 1);                                                    \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _14();                                      \
            UNI_WEIGHTED_COMPUTE ## V(V);                                      \
            WEIGHTED_STORE14_ ## V ## _ ## D();                                \
        }                                                                      \
        src  += srcstride;                                                     \
        dst  += dststride;                                                     \
    }                                                                          \
}

#define PUT_HEVC_BI_W_QPEL_V14(V, D)                                           \
void ohhevc_put_hevc_bi_w_qpel_v ## V ## _14_ ## D ## _avx2_ (                \
                                        uint8_t *_dst, ptrdiff_t _dststride,   \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int _wx0,       \
                                        int _wx1, int _ox0, int _ox1,          \
                                        intptr_t mx, intptr_t my, int width) { \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _14();                                         \
    SRC_INIT_14();                                                             \
    BI_WEIGHTED_INIT(D);                                                       \
    QPEL_FILTER_14(my - 1);                                                    \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            PUT_HEVC_PEL_PIXELS_BI_LOOP_VAR ## V ## _ ## D();                  \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _14();                                      \
            BI_WEIGHTED_COMPUTE ## V(V);                                       \
            WEIGHTED_STORE14_ ## V ## _ ## D();                                \
        }                                                                      \
        src  += srcstride;                                                     \
        src2 += MAX_PB_SIZE;                                                   \
        dst  += dststride;                                                     \
    }                                                                          \
}
////////////////////////////////////////////////////////////////////////////////
// ohhevc_put_hevc_qpel_hvX_X_avx2
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_HV(H, D)                                                 \
void ohhevc_put_hevc_qpel_hv ## H ## _ ## D ## _avx2_ (                       \
        int16_t *dst,                                                          \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    DECLARE_ALIGNED(32, int16_t, tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];) \
    int16_t *tmp = tmp_array;                                                  \
    SRC_INIT_ ## D();                                                          \
    src -= QPEL_EXTRA_BEFORE * srcstride;                                      \
    ohhevc_put_hevc_qpel_h ## H ## _ ## D ## _avx2_(                          \
        tmp, (uint8_t *)src, _srcstride, height + QPEL_EXTRA, mx, my, width);  \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                      \
    ohhevc_put_hevc_qpel_v16 ## _14_avx2_(                                    \
        dst, (uint8_t *) tmp, MAX_PB_SIZE <<1 , height, mx, my, width);        \
}

#define PUT_HEVC_BI_QPEL_HV(H, D)                                              \
void ohhevc_put_hevc_bi_qpel_hv ## H ## _ ## D ## _avx2_ (                    \
                                        uint8_t *dst, ptrdiff_t dststride,     \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height,                 \
                                        intptr_t mx, intptr_t my, int width) { \
DECLARE_ALIGNED(32, int16_t, tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];)  \
    int16_t *tmp = tmp_array;                                                  \
    SRC_INIT_ ## D();                                                          \
    src -= QPEL_EXTRA_BEFORE * srcstride;                                      \
    ohhevc_put_hevc_qpel_h ## H ## _ ## D ## _avx2_(                          \
        tmp, (uint8_t *)src, _srcstride, height + QPEL_EXTRA, mx, my, width);  \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                      \
    ohhevc_put_hevc_bi_qpel_v16 ## _14_ ## D ## _avx2_(                       \
        dst, dststride, (uint8_t *) tmp, MAX_PB_SIZE <<1 , src2, height, mx, my, width);\
}

#define PUT_HEVC_UNI_QPEL_HV(H, D)                                             \
void ohhevc_put_hevc_uni_qpel_hv ## H ## _ ## D ## _avx2_ (                   \
                                        uint8_t *dst, ptrdiff_t dststride,     \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height,                            \
                                        intptr_t mx, intptr_t my, int width) { \
DECLARE_ALIGNED(32, int16_t, tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];) \
    int16_t *tmp = tmp_array;                                                  \
    SRC_INIT_ ## D();                                                          \
    src -= QPEL_EXTRA_BEFORE * srcstride;                                      \
    ohhevc_put_hevc_qpel_h ## H ## _ ## D ## _avx2_(                          \
        tmp, (uint8_t *)src, _srcstride, height + QPEL_EXTRA, mx, my, width);  \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                      \
    ohhevc_put_hevc_uni_qpel_v16 ## _14_ ## D ## _avx2_(                      \
        dst, dststride, (uint8_t *) tmp, MAX_PB_SIZE <<1 , height, mx, my, width);\
}

#define PUT_HEVC_UNI_W_QPEL_HV(H, D)                                           \
void ohhevc_put_hevc_uni_w_qpel_hv ## H ## _ ## D ## _avx2_ (                 \
                                        uint8_t *dst, ptrdiff_t dststride,     \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int height, int denom,                 \
                                        int wx, int ox,                        \
                                        intptr_t mx, intptr_t my, int width) { \
    DECLARE_ALIGNED(32, int16_t, tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];) \
    int16_t *tmp = tmp_array;                                                  \
    SRC_INIT_ ## D();                                                          \
    src -= QPEL_EXTRA_BEFORE * srcstride;                                      \
    ohhevc_put_hevc_qpel_h ## H ## _ ## D ## _avx2_(                          \
        tmp, (uint8_t *)src, _srcstride, height + QPEL_EXTRA, mx, my, width);  \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                      \
    ohhevc_put_hevc_uni_w_qpel_v16 ## _14_ ## D ## _avx2_(                    \
        dst, dststride, (uint8_t *) tmp, MAX_PB_SIZE <<1 , height, denom, wx, ox, mx, my, width);\
}

#define PUT_HEVC_BI_W_QPEL_HV(H, D)                                            \
void ohhevc_put_hevc_bi_w_qpel_hv ## H ## _ ## D ## _avx2_ (                  \
                                        uint8_t *dst, ptrdiff_t dststride,     \
                                        uint8_t *_src, ptrdiff_t _srcstride,   \
                                        int16_t *src2,                         \
                                        int height, int denom, int wx0,        \
                                        int wx1, int ox0, int ox1,             \
                                        intptr_t mx, intptr_t my, int width) { \
    DECLARE_ALIGNED(32, int16_t, tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];) \
    int16_t *tmp = tmp_array;                                                  \
    SRC_INIT_ ## D();                                                          \
    src -= QPEL_EXTRA_BEFORE * srcstride;                                      \
    ohhevc_put_hevc_qpel_h ## H ## _ ## D ## _avx2_(                          \
        tmp, (uint8_t *)src, _srcstride, height + QPEL_EXTRA, mx, my, width);  \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                      \
    ohhevc_put_hevc_bi_w_qpel_v16 ## _14_ ## D ## _avx2_(                     \
        dst, dststride, (uint8_t *) tmp, MAX_PB_SIZE <<1 , src2, height, denom, wx0, wx1, ox0, ox1, mx, my, width);\
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define GEN_FUNC(FUNC, H, D)                                                   \
PUT_HEVC_ ## FUNC(H,D)                                                         \
PUT_HEVC_BI_ ## FUNC(H,D)                                                      \
PUT_HEVC_UNI_ ## FUNC(H,D)                                                     \
PUT_HEVC_UNI_W_ ## FUNC(H,D)                                                   \
PUT_HEVC_BI_W_ ## FUNC(H,D)

#define GEN_FUNC_STATIC(FUNC, H, D)                                            \
static PUT_HEVC_ ## FUNC(H,D)                                                  \
static PUT_HEVC_BI_ ## FUNC ##14(H, 8)                                         \
static PUT_HEVC_BI_ ## FUNC ##14(H,10)                                         \
static PUT_HEVC_BI_ ## FUNC ##14(H,12)                                         \
static PUT_HEVC_UNI_ ## FUNC ##14(H, 8)                                        \
static PUT_HEVC_UNI_ ## FUNC ##14(H,10)                                        \
static PUT_HEVC_UNI_ ## FUNC ##14(H,12)                                        \
static PUT_HEVC_UNI_W_ ## FUNC ##14(H, 8)                                      \
static PUT_HEVC_UNI_W_ ## FUNC ##14(H,10)                                      \
static PUT_HEVC_UNI_W_ ## FUNC ##14(H,12)                                      \
static PUT_HEVC_BI_W_ ## FUNC ##14(H, 8)                                       \
static PUT_HEVC_BI_W_ ## FUNC ##14(H,10)                                       \
static PUT_HEVC_BI_W_ ## FUNC ##14(H,12)

// ohhevc_put_hevc_mc_pixelsX_X_avx2
#if HAVE_AVX2
//GEN_FUNC(PEL_PIXELS, 32, 8)
//GEN_FUNC(PEL_PIXELS,  16, 10)
//GEN_FUNC(PEL_PIXELS,  16, 12)
//
//// ohhevc_put_hevc_epel_hX_X_avx2
//GEN_FUNC(EPEL_H,  16, 10)
//GEN_FUNC(EPEL_H,  16, 12)
//
//// ohhevc_put_hevc_epel_vX_X_avx2
//GEN_FUNC(EPEL_V,  16, 10)
//GEN_FUNC(EPEL_V,  16, 12)
//
//// ohhevc_put_hevc_epel_hvX_X_avx2
////GEN_FUNC(EPEL_HV,  8,  8)
//GEN_FUNC(EPEL_HV,  16, 10)
//
//GEN_FUNC(EPEL_HV,  16, 12)
//
//
//// ohhevc_put_hevc_qpel_hX_X_X_avx2
//GEN_FUNC(QPEL_H,  4,  8)
//GEN_FUNC(QPEL_H,  8,  8)
//GEN_FUNC(QPEL_H, 16,  8)
//GEN_FUNC(QPEL_H, 32,  8)
//GEN_FUNC(QPEL_H_10,  16, 10)
//GEN_FUNC(QPEL_H_10,  16, 12)
//
// ohhevc_put_hevc_qpel_vX_X_X_avx2
//GEN_FUNC(QPEL_V, 32,  8)
//
//GEN_FUNC(QPEL_V,  16, 10)
//
//GEN_FUNC(QPEL_V,  16, 12)
//
//GEN_FUNC_STATIC(QPEL_V, 16, 14)
//
//// ohhevc_put_hevc_qpel_hvX_X_avx2
//GEN_FUNC(QPEL_HV,  32, 8)
//GEN_FUNC(QPEL_HV,  16, 10)
//GEN_FUNC(QPEL_HV,  16, 12)

#define mc_red_func(name, bitd, step, W)                                       \
void ohhevc_put_hevc_##name##W##_##bitd##_avx2_(                              \
                                int16_t *dst,                                  \
                                uint8_t *_src, ptrdiff_t _srcstride,           \
                                int height, intptr_t mx, intptr_t my, int width) { \
    ohhevc_put_hevc_##name##step##_##bitd##_avx2_(dst, _src, _srcstride, height, mx, my, width);  \
}                                                                              \
void ohhevc_put_hevc_bi_##name##W##_##bitd##_avx2_(                           \
                                uint8_t *dst, ptrdiff_t dststride,             \
                                uint8_t *_src, ptrdiff_t _srcstride,           \
                                int16_t *src2,                                 \
                                int height,                                    \
                                intptr_t mx, intptr_t my, int width) {         \
    ohhevc_put_hevc_bi_##name##step##_##bitd##_avx2_(dst, dststride, _src, _srcstride, src2, height, mx, my, width);\
}                                                                              \
void ohhevc_put_hevc_uni_##name##W##_##bitd##_avx2_(                          \
                                uint8_t *dst, ptrdiff_t dststride,             \
                                uint8_t *_src, ptrdiff_t _srcstride,           \
                                int height,                                    \
                                intptr_t mx, intptr_t my, int width) {         \
    ohhevc_put_hevc_uni_##name##step##_##bitd##_avx2_(dst, dststride, _src, _srcstride, height, mx, my, width);\
}                                                                              \
void ohhevc_put_hevc_uni_w_##name##W##_##bitd##_avx2_(                        \
                                uint8_t *dst, ptrdiff_t dststride,             \
                                uint8_t *_src, ptrdiff_t _srcstride,           \
                                int height,  int denom, int wx,                \
                                int ox, intptr_t mx, intptr_t my, int width) { \
    ohhevc_put_hevc_uni_w_##name##step##_##bitd##_avx2_(dst, dststride, _src, _srcstride, height, denom, wx, ox, mx, my, width);\
}                                                                              \
void ohhevc_put_hevc_bi_w_##name##W##_##bitd##_avx2_(                         \
                                uint8_t *dst, ptrdiff_t dststride,             \
                                uint8_t *_src, ptrdiff_t _srcstride,           \
                                int16_t *src2,                                 \
                                int height, int denom, int wx0,                \
                                int wx1, int ox0, int ox1,                     \
                                intptr_t mx, intptr_t my, int width) {         \
    ohhevc_put_hevc_bi_w_##name##step##_##bitd##_avx2_(dst, dststride, _src, _srcstride, src2, height, denom, wx0, wx1, ox0, ox1, mx, my, width);\
}


/*mc_red_func(pel_pixels, 8, 4, 12);
mc_red_func(pel_pixels, 8, 8, 24);
mc_red_func(pel_pixels, 8,16, 32);
mc_red_func(pel_pixels, 8,16, 48);
mc_red_func(pel_pixels, 8,16, 64);

mc_red_func(pel_pixels,10, 4, 12);
mc_red_func(pel_pixels,10, 8, 16);
mc_red_func(pel_pixels,10, 8, 24);
mc_red_func(pel_pixels,10, 8, 32);
mc_red_func(pel_pixels,10, 8, 48);
mc_red_func(pel_pixels,10, 8, 64);

mc_red_func(pel_pixels,12, 4, 12);
mc_red_func(pel_pixels,12, 8, 16);
mc_red_func(pel_pixels,12, 8, 24);
mc_red_func(pel_pixels,12, 8, 32);
mc_red_func(pel_pixels,12, 8, 48);
mc_red_func(pel_pixels,12, 8, 64);

mc_red_func(qpel_h, 8, 4, 12);
mc_red_func(qpel_h, 8, 8, 24);
mc_red_func(qpel_h, 8,16, 32);
mc_red_func(qpel_h, 8,16, 48);
mc_red_func(qpel_h, 8,16, 64);

mc_red_func(qpel_h,10, 4, 12);
mc_red_func(qpel_h,10, 8, 16);
mc_red_func(qpel_h,10, 8, 24);
mc_red_func(qpel_h,10, 8, 32);
mc_red_func(qpel_h,10, 8, 48);
mc_red_func(qpel_h,10, 8, 64);

mc_red_func(qpel_h,12, 4, 12);
mc_red_func(qpel_h,12, 8, 16);
mc_red_func(qpel_h,12, 8, 24);
mc_red_func(qpel_h,12, 8, 32);
mc_red_func(qpel_h,12, 8, 48);
mc_red_func(qpel_h,12, 8, 64);

mc_red_func(qpel_v, 8, 4, 12);
mc_red_func(qpel_v, 8, 8, 24);
mc_red_func(qpel_v, 8,16, 32);
mc_red_func(qpel_v, 8,16, 48);
mc_red_func(qpel_v, 8,16, 64);

mc_red_func(qpel_v,10, 4, 12);
mc_red_func(qpel_v,10, 8, 16);
mc_red_func(qpel_v,10, 8, 24);
mc_red_func(qpel_v,10, 8, 32);
mc_red_func(qpel_v,10, 8, 48);
mc_red_func(qpel_v,10, 8, 64);

mc_red_func(qpel_v,12, 4, 12);
mc_red_func(qpel_v,12, 8, 16);
mc_red_func(qpel_v,12, 8, 24);
mc_red_func(qpel_v,12, 8, 32);
mc_red_func(qpel_v,12, 8, 48);
mc_red_func(qpel_v,12, 8, 64);

mc_red_func(qpel_hv, 8, 4, 12);
mc_red_func(qpel_hv, 8, 8, 16);
mc_red_func(qpel_hv, 8, 8, 24);
mc_red_func(qpel_hv, 8, 8, 32);
mc_red_func(qpel_hv, 8, 8, 48);
mc_red_func(qpel_hv, 8, 8, 64);

mc_red_func(qpel_hv,10, 4, 12);
mc_red_func(qpel_hv,10, 8, 16);
mc_red_func(qpel_hv,10, 8, 24);
mc_red_func(qpel_hv,10, 8, 32);
mc_red_func(qpel_hv,10, 8, 48);
mc_red_func(qpel_hv,10, 8, 64);

mc_red_func(qpel_hv,12, 4, 12);
mc_red_func(qpel_hv,12, 8, 16);
mc_red_func(qpel_hv,12, 8, 24);
mc_red_func(qpel_hv,12, 8, 32);
mc_red_func(qpel_hv,12, 8, 48);
mc_red_func(qpel_hv,12, 8, 64);

mc_red_func(epel_h, 8, 4, 12);
mc_red_func(epel_h, 8, 8, 16);
mc_red_func(epel_h, 8, 8, 24);
mc_red_func(epel_h, 8, 8, 32);
mc_red_func(epel_h, 8, 8, 48);
mc_red_func(epel_h, 8, 8, 64);

mc_red_func(epel_h,10, 4, 12);
mc_red_func(epel_h,10, 8, 16);
mc_red_func(epel_h,10, 8, 24);
mc_red_func(epel_h,10, 8, 32);
mc_red_func(epel_h,10, 8, 48);
mc_red_func(epel_h,10, 8, 64);

mc_red_func(epel_h,12, 4, 12);
mc_red_func(epel_h,12, 8, 16);
mc_red_func(epel_h,12, 8, 24);
mc_red_func(epel_h,12, 8, 32);
mc_red_func(epel_h,12, 8, 48);
mc_red_func(epel_h,12, 8, 64);

mc_red_func(epel_v, 8, 4, 12);
mc_red_func(epel_v, 8, 8, 16);
mc_red_func(epel_v, 8, 8, 24);
mc_red_func(epel_v, 8, 8, 32);
mc_red_func(epel_v, 8, 8, 48);
mc_red_func(epel_v, 8, 8, 64);

mc_red_func(epel_v,10, 4, 12);
mc_red_func(epel_v,10, 8, 16);
mc_red_func(epel_v,10, 8, 24);
mc_red_func(epel_v,10, 8, 32);
mc_red_func(epel_v,10, 8, 48);
mc_red_func(epel_v,10, 8, 64);

mc_red_func(epel_v,12, 4, 12);
mc_red_func(epel_v,12, 8, 16);
mc_red_func(epel_v,12, 8, 24);
mc_red_func(epel_v,12, 8, 32);
mc_red_func(epel_v,12, 8, 48);
mc_red_func(epel_v,12, 8, 64);

mc_red_func(epel_hv, 8, 4, 12);
mc_red_func(epel_hv, 8, 8, 16);
mc_red_func(epel_hv, 8, 8, 24);
mc_red_func(epel_hv, 8, 8, 32);
mc_red_func(epel_hv, 8, 8, 48);
mc_red_func(epel_hv, 8, 8, 64);

mc_red_func(epel_hv,10, 4, 12);
mc_red_func(epel_hv,10, 8, 16);
mc_red_func(epel_hv,10, 8, 24);
mc_red_func(epel_hv,10, 8, 32);
mc_red_func(epel_hv,10, 8, 48);
mc_red_func(epel_hv,10, 8, 64);

mc_red_func(epel_hv,12, 4, 12);
mc_red_func(epel_hv,12, 8, 16);
mc_red_func(epel_hv,12, 8, 24);
mc_red_func(epel_hv,12, 8, 32);
mc_red_func(epel_hv,12, 8, 48);
mc_red_func(epel_hv,12, 8, 64);
*/
//#endif //OPTI_ASM

#endif // HAVE_AVX2

