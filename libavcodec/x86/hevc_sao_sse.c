/*
 * Provide SSE sao functions for HEVC decoding
 * Copyright (c) 2013 Pierre-Edouard LEPERE
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

#define CLPI_PIXEL_MAX_10 0x03FF
#define CLPI_PIXEL_MAX_12 0x0FFF

#if HAVE_SSE42
#define _MM_MIN_EPU16 _mm_min_epu16
#else

#if HAVE_SSE2
static av_always_inline __m128i comlt_epu16(__m128i a, __m128i b)
{
    __m128i signMask, mask;

    mask     = _mm_cmplt_epi16(a, b);              // FFFF where a < b (signed)
    signMask = _mm_xor_si128  (a, b);              // Signbit is 1 where signs differ
    signMask = _mm_srai_epi16 (signMask, 15);      // fill all fields with sign bit
    mask     = _mm_xor_si128  (mask, signMask);    // Invert output where signs differed
    return mask;
}

static av_always_inline __m128i logical_bitwise_select(__m128i a, __m128i b, __m128i mask)
{
    a = _mm_and_si128   (a,    mask);                                 // clear a where mask = 0
    b = _mm_andnot_si128(mask, b   );                                 // clear b where mask = 1
    a = _mm_or_si128    (a,    b   );                                 // a = a OR b
    return a;
}

static __m128i _MM_MIN_EPU16(__m128i a, __m128i b)
{
     __m128i mask = comlt_epu16(a, b);
     a = logical_bitwise_select(a, b, mask);
     return a;
}
#endif
#endif

#if HAVE_SSE42
#define _MM_CVTEPI8_EPI16 _mm_cvtepi8_epi16

#else
#if HAVE_SSE2
static inline __m128i _MM_CVTEPI8_EPI16(__m128i m0) {
    return _mm_unpacklo_epi8(m0, _mm_cmplt_epi8(m0, _mm_setzero_si128()));
}
#endif
#endif

#if HAVE_SSSE3
////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define SAO_INIT_8()                                                           \
    uint8_t  *dst       = _dst;                                                \
    uint8_t  *src       = _src;                                                \
    ptrdiff_t stride_dst= _stride_dst;                                         \
    ptrdiff_t stride_src= _stride_src
#define SAO_INIT_10()                                                          \
    uint16_t  *dst      = (uint16_t *) _dst;                                   \
    uint16_t *src       = (uint16_t *) _src;                                   \
    ptrdiff_t stride_dst= _stride_dst >> 1;                                    \
    ptrdiff_t stride_src= _stride_src >> 1
#define SAO_INIT_12() SAO_INIT_10()

#define SAO_BAND_FILTER_INIT()                                                 \
    r0   = _mm_set1_epi16((sao_left_class    ) & 31);                          \
    r1   = _mm_set1_epi16((sao_left_class + 1) & 31);                          \
    r2   = _mm_set1_epi16((sao_left_class + 2) & 31);                          \
    r3   = _mm_set1_epi16((sao_left_class + 3) & 31);                          \
    sao1 = _mm_set1_epi16(sao_offset_val[1]);                                  \
    sao2 = _mm_set1_epi16(sao_offset_val[2]);                                  \
    sao3 = _mm_set1_epi16(sao_offset_val[3]);                                  \
    sao4 = _mm_set1_epi16(sao_offset_val[4])

#define SAO_BAND_FILTER_LOAD_8(x)                                              \
    src0 = _mm_loadl_epi64((__m128i *) &src[x]);                               \
    src0 = _mm_unpacklo_epi8(src0, _mm_setzero_si128());                       \
    src2 = _mm_srai_epi16(src0, shift)
#define SAO_BAND_FILTER_LOAD_10(x)                                             \
    src0 = _mm_loadu_si128((__m128i *) &src[x]);                               \
    src2 = _mm_srai_epi16(src0, shift)
#define SAO_BAND_FILTER_LOAD_12(x) SAO_BAND_FILTER_LOAD_10(x)

#define SAO_BAND_FILTER_COMPUTE()                                              \
    x0   = _mm_cmpeq_epi16(src2, r0);                                          \
    x1   = _mm_cmpeq_epi16(src2, r1);                                          \
    x2   = _mm_cmpeq_epi16(src2, r2);                                          \
    x3   = _mm_cmpeq_epi16(src2, r3);                                          \
    x0   = _mm_and_si128(x0, sao1);                                            \
    x1   = _mm_and_si128(x1, sao2);                                            \
    x2   = _mm_and_si128(x2, sao3);                                            \
    x3   = _mm_and_si128(x3, sao4);                                            \
    x0   = _mm_or_si128(x0, x1);                                               \
    x2   = _mm_or_si128(x2, x3);                                               \
    x0   = _mm_or_si128(x0, x2);                                               \
    src0 = _mm_add_epi16(src0, x0)

#define SAO_BAND_FILTER_STORE_8()                                              \
    src0 = _mm_packus_epi16(src0, src0);                                       \
    _mm_storel_epi64((__m128i *) &dst[x], src0)
#define SAO_BAND_FILTER_STORE(D)                                               \
    src0 = _mm_max_epi16(src0, _mm_setzero_si128());                           \
    src0 = _mm_min_epi16(src0, _mm_set1_epi16(CLPI_PIXEL_MAX_## D));           \
    _mm_store_si128((__m128i *) &dst[x  ], src0)

#define SAO_BAND_FILTER_STORE_10()    SAO_BAND_FILTER_STORE(10)
#define SAO_BAND_FILTER_STORE_12()    SAO_BAND_FILTER_STORE(12)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define SAO_BAND_FILTER(W, D)                                                  \
void ohhevc_sao_band_filter_0_ ## D ##_sse(                                   \
        uint8_t *_dst, uint8_t *_src,                                          \
        ptrdiff_t _stride_dst, ptrdiff_t _stride_src,                          \
        struct SAOParams *sao,                                                 \
        int *borders, int width, int height, int c_idx) {                      \
    int y, x;                                                                  \
    int  shift          = D - 5;                                               \
    int16_t *sao_offset_val = sao->offset_val[c_idx];                          \
    uint8_t  sao_left_class = sao->band_position[c_idx];                       \
    __m128i r0, r1, r2, r3, x0, x1, x2, x3, sao1, sao2, sao3, sao4;            \
    __m128i src0, src2;                                                        \
    SAO_INIT_ ## D();                                                          \
    SAO_BAND_FILTER_INIT();                                                    \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += W) {                                       \
            SAO_BAND_FILTER_LOAD_ ##D(x);                                      \
            SAO_BAND_FILTER_COMPUTE();                                         \
            SAO_BAND_FILTER_STORE_ ##D();                                      \
        }                                                                      \
        dst += stride_dst;                                                     \
        src += stride_src;                                                     \
    }                                                                          \
}
SAO_BAND_FILTER( 8,  8)
SAO_BAND_FILTER( 8, 10)
SAO_BAND_FILTER( 8, 12)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define SAO_EDGE_FILTER_LOAD_8()                                               \
    x0   = _mm_loadl_epi64((__m128i *) (src + x));                             \
    cmp0 = _mm_loadl_epi64((__m128i *) (src + x + a_stride));                  \
    cmp1 = _mm_loadl_epi64((__m128i *) (src + x + b_stride));                  \
    x0   = _mm_unpacklo_epi8(x0  , _mm_setzero_si128());                       \
    cmp0 = _mm_unpacklo_epi8(cmp0, _mm_setzero_si128());                       \
    cmp1 = _mm_unpacklo_epi8(cmp1, _mm_setzero_si128())
#define SAO_EDGE_FILTER_LOAD_10()                                              \
    x0   = _mm_loadu_si128((__m128i *) (src + x));                             \
    cmp0 = _mm_loadu_si128((__m128i *) (src + x + a_stride));                  \
    cmp1 = _mm_loadu_si128((__m128i *) (src + x + b_stride))
#define SAO_EDGE_FILTER_LOAD_12() SAO_EDGE_FILTER_LOAD_10()

#define SAO_EDGE_FILTER_COMPUTE()                                              \
    r2 = _MM_MIN_EPU16(x0, cmp0);                                              \
    x1 = _mm_cmpeq_epi16(cmp0, r2);                                            \
    x2 = _mm_cmpeq_epi16(x0, r2);                                              \
    x1 = _mm_sub_epi16(x2, x1);                                                \
    r2 = _MM_MIN_EPU16(x0, cmp1);                                              \
    x3 = _mm_cmpeq_epi16(cmp1, r2);                                            \
    x2 = _mm_cmpeq_epi16(x0, r2);                                              \
    x3 = _mm_sub_epi16(x2, x3);                                                \
    x1 = _mm_add_epi16(x1, x3);                                                \
    r0 = _mm_cmpeq_epi16(x1, _mm_set1_epi16(-2));                              \
    r1 = _mm_cmpeq_epi16(x1, _mm_set1_epi16(-1));                              \
    r2 = _mm_cmpeq_epi16(x1, _mm_set1_epi16(0));                               \
    r3 = _mm_cmpeq_epi16(x1, _mm_set1_epi16(1));                               \
    r4 = _mm_cmpeq_epi16(x1, _mm_set1_epi16(2));                               \
    r0 = _mm_and_si128(r0, offset0);                                           \
    r1 = _mm_and_si128(r1, offset1);                                           \
    r2 = _mm_and_si128(r2, offset2);                                           \
    r3 = _mm_and_si128(r3, offset3);                                           \
    r4 = _mm_and_si128(r4, offset4);                                           \
    r0 = _mm_add_epi16(r0, r1);                                                \
    r2 = _mm_add_epi16(r2, r3);                                                \
    r0 = _mm_add_epi16(r0, r4);                                                \
    r0 = _mm_add_epi16(r0, r2);                                                \
    r0 = _mm_add_epi16(r0, x0)

#define SAO_EDGE_FILTER_STORE_8()                                              \
    r0 = _mm_packus_epi16(r0, r0);                                             \
    _mm_storel_epi64((__m128i *) (dst + x), r0)
#define SAO_EDGE_FILTER_STORE(D)                                               \
    r1 = _mm_set1_epi16(CLPI_PIXEL_MAX_## D);                                  \
    r0 = _mm_max_epi16(r0, _mm_setzero_si128());                               \
    r0 = _mm_min_epi16(r0, r1);                                                \
    _mm_storeu_si128((__m128i *) (dst + x), r0)
#define SAO_EDGE_FILTER_STORE_10() SAO_EDGE_FILTER_STORE(10)
#define SAO_EDGE_FILTER_STORE_12() SAO_EDGE_FILTER_STORE(12)

#define SAO_EDGE_FILTER_BORDER_LOOP_8(incr_dst, incr_src)                      \
    x1 = _mm_set1_epi8(sao_offset_val[0]);                                     \
    for (x = 0; x < width; x += 8) {                                           \
        x0 = _mm_loadl_epi64((__m128i *) (src + incr_src));                    \
        x0 = _mm_add_epi8(x0, x1);                                             \
        _mm_storel_epi64((__m128i *) (dst + incr_dst), x0);                    \
    }
#define SAO_EDGE_FILTER_BORDER_LOOP_10(incr_dst, incr_src)                     \
    x1 = _mm_set1_epi8(sao_offset_val[0]);                                     \
    for (x = 0; x < width; x += 8) {                                           \
        x0 = _mm_loadu_si128((__m128i *) (src + incr_src));                    \
        x0 = _mm_add_epi16(x0, x1);                                            \
        _mm_storeu_si128((__m128i *) (dst + incr_dst), x0);                    \
    }
#define SAO_EDGE_FILTER_BORDER_LOOP_12(incr_dst, incr_src)                     \
        SAO_EDGE_FILTER_BORDER_LOOP_10(incr_dst, incr_src)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define SAO_EDGE_FILTER(D)                                                     \
void ohhevc_sao_edge_filter_ ## D ##_sse(             \
        uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst,                   \
        ptrdiff_t _stride_src, SAOParams *sao, int width,                      \
        int height, int c_idx) {                                               \
    int x, y;                                                                  \
    int16_t *sao_offset_val = sao->offset_val[c_idx];                          \
    int  eo   = sao->eo_class[c_idx];                                          \
    const uint8_t edge_idx[]  = { 1, 2, 0, 3, 4 };                             \
    const int8_t pos[4][2][2] = {                                              \
        { {-1, 0}, { 1, 0} }, /* horizontal */                                 \
        { { 0,-1}, { 0, 1} }, /* vertical   */                                 \
        { {-1,-1}, { 1, 1} }, /* 45 degree  */                                 \
        { { 1,-1}, {-1, 1} }, /* 135 degree */                                 \
    };                                                                         \
    __m128i x0, x1, x2, x3, offset0, offset1, offset2, offset3, offset4;       \
    __m128i cmp0, cmp1, r0, r1, r2, r3, r4;                                    \
    SAO_INIT_ ## D();                                                          \
    {                                                                          \
        int a_stride = pos[eo][0][0] + pos[eo][0][1] * stride_src;             \
        int b_stride = pos[eo][1][0] + pos[eo][1][1] * stride_src;             \
        offset0 = _mm_set1_epi16(sao_offset_val[edge_idx[0]]);                 \
        offset1 = _mm_set1_epi16(sao_offset_val[edge_idx[1]]);                 \
        offset2 = _mm_set1_epi16(sao_offset_val[edge_idx[2]]);                 \
        offset3 = _mm_set1_epi16(sao_offset_val[edge_idx[3]]);                 \
        offset4 = _mm_set1_epi16(sao_offset_val[edge_idx[4]]);                 \
                                                                               \
        for (y = 0; y < height; y++) {                                         \
            for (x = 0; x < width; x += 8) {                                   \
                SAO_EDGE_FILTER_LOAD_ ## D();                                  \
                SAO_EDGE_FILTER_COMPUTE();                                     \
                SAO_EDGE_FILTER_STORE_ ## D();                                 \
            }                                                                  \
            src += stride_src;                                                 \
            dst += stride_dst;                                                 \
        }                                                                      \
    }                                                                          \
}

//SAO_EDGE_FILTER( 8)

void ohhevc_sao_edge_filter_8_sse(uint8_t *dst, uint8_t *src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  SAOParams *sao,
                                  int width, int height,
                                  int c_idx) {
    static const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };
    static const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t eo = sao->eo_class[c_idx];

    int a_stride, b_stride;
    int x, y;
    __m128i x0, x1, x2, x3, offset0;
    __m128i cmp0, cmp1, r0, r2;
    offset0 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, sao_offset_val[edge_idx[4]],
                           sao_offset_val[edge_idx[3]], sao_offset_val[edge_idx[2]],
                           sao_offset_val[edge_idx[1]],    sao_offset_val[edge_idx[0]]);
    a_stride = pos[eo][0][0] + pos[eo][0][1] * stride_src;
    b_stride = pos[eo][1][0] + pos[eo][1][1] * stride_src;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x += 8) {
                x0   = _mm_loadl_epi64((__m128i *) (src + x));
                cmp0 = _mm_loadl_epi64((__m128i *) (src + x + a_stride));
                cmp1 = _mm_loadl_epi64((__m128i *) (src + x + b_stride));
                r2 = _mm_min_epu8(x0, cmp0);
                x1 = _mm_cmpeq_epi8(cmp0, r2);
                x2 = _mm_cmpeq_epi8(x0, r2);
                x1 = _mm_sub_epi8(x2, x1);
                r2 = _mm_min_epu8(x0, cmp1);
                x3 = _mm_cmpeq_epi8(cmp1, r2);
                x2 = _mm_cmpeq_epi8(x0, r2);
                x3 = _mm_sub_epi8(x2, x3);
                x1 = _mm_add_epi8(x1, x3);
                x1 = _mm_add_epi8(x1, _mm_set1_epi8(2));
                r0 = _mm_shuffle_epi8(offset0, x1);
                r0 = _MM_CVTEPI8_EPI16(r0);
                x0 = _mm_unpacklo_epi8(x0, _mm_setzero_si128());
                r0 = _mm_add_epi16(r0, x0);
                r0 = _mm_packus_epi16(r0, r0);
                _mm_storel_epi64((__m128i *) (dst + x), r0);
        }
        src += stride_src;
        dst += stride_dst;
    }

}

SAO_EDGE_FILTER(10)
SAO_EDGE_FILTER(12)
#endif //HAVE_SSE42
