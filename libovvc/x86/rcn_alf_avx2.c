/**
 *
 *   OpenVVC is open-source real time software decoder compliant with the 
 *   ITU-T H.266- MPEG-I - Part 3 VVC standard. OpenVVC is developed from 
 *   scratch in C as a library that provides consumers with real time and
 *   energy-aware decoding capabilities under different OS including MAC OS,
 *   Windows, Linux and Android targeting low energy real-time decoding of
 *   4K VVC videos on Intel x86 and ARM platforms.
 * 
 *   Copyright (C) 2020-2022  IETR-INSA Rennes :
 *   
 *   Pierre-Loup CABARAT
 *   Wassim HAMIDOUCHE
 *   Guillaume GAUTIER
 *   Thomas AMESTOY
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *   USA
 * 
 **/

#if defined _MSC_VER
#include <immintrin.h>
#else
#include <x86intrin.h>
#endif


#include <stdint.h>
#include "rcn_alf.h"
#include "rcn_structures.h"

#define process2coeffs5x5SSE(i, ptr0, ptr1, ptr2, ptr3) { \
                    const __m128i val00 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr0)), cur);\
                    const __m128i val10 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr2)), cur);\
                    const __m128i val01 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr1)), cur);\
                    const __m128i val11 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr3)), cur);\
                    \
                    __m128i val01A = _mm_unpacklo_epi16(val00, val10);\
                    __m128i val01B = _mm_unpackhi_epi16(val00, val10);\
                    __m128i val01C = _mm_unpacklo_epi16(val01, val11);\
                    __m128i val01D = _mm_unpackhi_epi16(val01, val11);\
                    \
                    __m128i limit01A = params[1][i];\
                    \
                    val01A = _mm_min_epi16(val01A, limit01A);\
                    val01B = _mm_min_epi16(val01B, limit01A);\
                    val01C = _mm_min_epi16(val01C, limit01A);\
                    val01D = _mm_min_epi16(val01D, limit01A);\
                    \
                    limit01A = _mm_sub_epi16(_mm_setzero_si128(), limit01A);\
                    \
                    val01A = _mm_max_epi16(val01A, limit01A);\
                    val01B = _mm_max_epi16(val01B, limit01A);\
                    val01C = _mm_max_epi16(val01C, limit01A);\
                    val01D = _mm_max_epi16(val01D, limit01A);\
                    \
                    val01A = _mm_add_epi16(val01A, val01C);\
                    val01B = _mm_add_epi16(val01B, val01D);\
                    \
                    __m128i coeff01A = params[0][i];\
                    \
                    accumA = _mm_add_epi32(accumA, _mm_madd_epi16(val01A, coeff01A));\
                    accumB = _mm_add_epi32(accumB, _mm_madd_epi16(val01B, coeff01A));\
                };\

#define process2coeffs7x7SSE(i, ptr0, ptr1, ptr2, ptr3) { \
        const __m128i val00 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr0)), cur); \
        const __m128i val10 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr2)), cur); \
        const __m128i val01 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr1)), cur); \
        const __m128i val11 = _mm_sub_epi16(_mm_loadu_si128((const __m128i *) (ptr3)), cur); \
        \
        __m128i val01A = _mm_unpacklo_epi16(val00, val10); \
        __m128i val01B = _mm_unpackhi_epi16(val00, val10); \
        __m128i val01C = _mm_unpacklo_epi16(val01, val11); \
        __m128i val01D = _mm_unpackhi_epi16(val01, val11); \
        \
        __m128i limit01A = params[0][1][i]; \
        __m128i limit01B = params[1][1][i]; \
        \
        val01A = _mm_min_epi16(val01A, limit01A); \
        val01B = _mm_min_epi16(val01B, limit01B); \
        val01C = _mm_min_epi16(val01C, limit01A); \
        val01D = _mm_min_epi16(val01D, limit01B); \
        \
        limit01A = _mm_sub_epi16(_mm_setzero_si128(), limit01A); \
        limit01B = _mm_sub_epi16(_mm_setzero_si128(), limit01B); \
        \
        val01A = _mm_max_epi16(val01A, limit01A); \
        val01B = _mm_max_epi16(val01B, limit01B); \
        val01C = _mm_max_epi16(val01C, limit01A); \
        val01D = _mm_max_epi16(val01D, limit01B); \
        \
        val01A = _mm_add_epi16(val01A, val01C); \
        val01B = _mm_add_epi16(val01B, val01D); \
        \
        const __m128i coeff01A = params[0][0][i]; \
        const __m128i coeff01B = params[1][0][i]; \
        \
        accumA = _mm_add_epi32(accumA, _mm_madd_epi16(val01A, coeff01A)); \
        accumB = _mm_add_epi32(accumB, _mm_madd_epi16(val01B, coeff01B)); \
}; \

static void
simdFilter7x7Blk_sse(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr, OVSample *const dst, OVSample *const src, const int dstStride, const int srcStride,
                         Area blk_dst, const int16_t *filter_set, const int16_t *clip_set,
                         const int ctu_height, int virbnd_pos)
{
    const int SHIFT = NUM_BITS - 1;
    const int ROUND = 1 << (SHIFT - 1);

    const size_t STEP_X = 8;
    const size_t STEP_Y = 4;

    const int clpMin = 0;
    const int clpMax = (1<<10) - 1;

    const OVSample * _src = src;
    OVSample * _dst = dst;

    int transpose_idx = 0;
    int class_idx = 0;

    const __m128i mmOffset = _mm_set1_epi32(ROUND);
    const __m128i mmMin = _mm_set1_epi16( clpMin );
    const __m128i mmMax = _mm_set1_epi16( clpMax );

    for (size_t i = 0; i < blk_dst.height; i += STEP_Y)
    {
        for (size_t j = 0; j < blk_dst.width; j += STEP_X)
        {
            __m128i params[2][2][6];

            for (int k = 0; k < 2; ++k)
            {
                transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];
                class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];

                const __m128i rawCoeffLo = _mm_loadu_si128((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawCoeffHi = _mm_loadl_epi64((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));
                const __m128i rawClipLo  = _mm_loadu_si128((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawClipHi  = _mm_loadl_epi64((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));

                params[k][0][0] = _mm_shuffle_epi32(rawCoeffLo, 0x00);
                params[k][0][1] = _mm_shuffle_epi32(rawCoeffLo, 0x55);
                params[k][0][2] = _mm_shuffle_epi32(rawCoeffLo, 0xaa);
                params[k][0][3] = _mm_shuffle_epi32(rawCoeffLo, 0xff);
                params[k][0][4] = _mm_shuffle_epi32(rawCoeffHi, 0x00);
                params[k][0][5] = _mm_shuffle_epi32(rawCoeffHi, 0x55);
                params[k][1][0] = _mm_shuffle_epi32(rawClipLo, 0x00);
                params[k][1][1] = _mm_shuffle_epi32(rawClipLo, 0x55);
                params[k][1][2] = _mm_shuffle_epi32(rawClipLo, 0xaa);
                params[k][1][3] = _mm_shuffle_epi32(rawClipLo, 0xff);
                params[k][1][4] = _mm_shuffle_epi32(rawClipHi, 0x00);
                params[k][1][5] = _mm_shuffle_epi32(rawClipHi, 0x55);
            }

            for (size_t ii = 0; ii < STEP_Y; ii++)
            {
                const uint16_t *pImg0, *pImg1, *pImg2, *pImg3, *pImg4, *pImg5, *pImg6;

                pImg0 = (uint16_t *)_src + j + ii * srcStride;
                pImg1 = pImg0 + srcStride;
                pImg2 = pImg0 - srcStride;
                pImg3 = pImg1 + srcStride;
                pImg4 = pImg2 - srcStride;
                pImg5 = pImg3 + srcStride;
                pImg6 = pImg4 - srcStride;

                __m128i cur = _mm_loadu_si128((const __m128i *) pImg0);

                __m128i accumA = mmOffset;
                __m128i accumB = mmOffset;

                process2coeffs7x7SSE(0, pImg5 + 0, pImg6 + 0, pImg3 + 1, pImg4 - 1);
                process2coeffs7x7SSE(1, pImg3 + 0, pImg4 + 0, pImg3 - 1, pImg4 + 1);
                process2coeffs7x7SSE(2, pImg1 + 2, pImg2 - 2, pImg1 + 1, pImg2 - 1);
                process2coeffs7x7SSE(3, pImg1 + 0, pImg2 + 0, pImg1 - 1, pImg2 + 1);
                process2coeffs7x7SSE(4, pImg1 - 2, pImg2 + 2, pImg0 + 3, pImg0 - 3);
                process2coeffs7x7SSE(5, pImg0 + 2, pImg0 - 2, pImg0 + 1, pImg0 - 1);

                accumA = _mm_srai_epi32(accumA, SHIFT);
                accumB = _mm_srai_epi32(accumB, SHIFT);

                accumA = _mm_packs_epi32(accumA, accumB);
                accumA = _mm_add_epi16(accumA, cur);
                accumA = _mm_min_epi16(mmMax, _mm_max_epi16(accumA, mmMin));

                _mm_storeu_si128((__m128i *) (_dst + ii * dstStride + j), accumA);
            }
        }

        _src += srcStride * STEP_Y;
        _dst += dstStride * STEP_Y;
    }
}

static void
simdFilter7x7BlkVB_sse(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr, OVSample *const dst, OVSample *const src, const int dstStride, const int srcStride,
                         Area blk_dst, const int16_t *filter_set, const int16_t *clip_set,
                         const int ctu_height, int virbnd_pos)
{
    const int SHIFT = NUM_BITS - 1;
    const int ROUND = 1 << (SHIFT - 1);

    const size_t STEP_X = 8;
    const size_t STEP_Y = 4;

    const int clpMin = 0;
    const int clpMax = (1<<10) - 1;

    OVSample * _src = src;
    OVSample * _dst = dst;

    int transpose_idx = 0;
    int class_idx = 0;

    const __m128i mmOffset = _mm_set1_epi32(ROUND);
    const __m128i mmMin = _mm_set1_epi16( clpMin );
    const __m128i mmMax = _mm_set1_epi16( clpMax );

    const __m128i mmOffsetborder = _mm_set1_epi32(1 << ((SHIFT + 3) - 1));
    const int SHIFTborder = SHIFT+3;

    for (size_t i = 0; i < blk_dst.height; i += STEP_Y)
    {
        for (size_t j = 0; j < blk_dst.width; j += STEP_X)
        {
            __m128i params[2][2][6];

            for (int k = 0; k < 2; ++k)
            {
                transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];
                class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];

                const __m128i rawCoeffLo = _mm_loadu_si128((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawCoeffHi = _mm_loadl_epi64((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));
                const __m128i rawClipLo  = _mm_loadu_si128((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawClipHi  = _mm_loadl_epi64((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));

                params[k][0][0] = _mm_shuffle_epi32(rawCoeffLo, 0x00);
                params[k][0][1] = _mm_shuffle_epi32(rawCoeffLo, 0x55);
                params[k][0][2] = _mm_shuffle_epi32(rawCoeffLo, 0xaa);
                params[k][0][3] = _mm_shuffle_epi32(rawCoeffLo, 0xff);
                params[k][0][4] = _mm_shuffle_epi32(rawCoeffHi, 0x00);
                params[k][0][5] = _mm_shuffle_epi32(rawCoeffHi, 0x55);
                params[k][1][0] = _mm_shuffle_epi32(rawClipLo, 0x00);
                params[k][1][1] = _mm_shuffle_epi32(rawClipLo, 0x55);
                params[k][1][2] = _mm_shuffle_epi32(rawClipLo, 0xaa);
                params[k][1][3] = _mm_shuffle_epi32(rawClipLo, 0xff);
                params[k][1][4] = _mm_shuffle_epi32(rawClipHi, 0x00);
                params[k][1][5] = _mm_shuffle_epi32(rawClipHi, 0x55);
            }

            for (size_t ii = 0; ii < STEP_Y; ii++)
            {
                const uint16_t *pImg0, *pImg1, *pImg2, *pImg3, *pImg4, *pImg5, *pImg6;

                pImg0 = (uint16_t *)_src + j + ii * srcStride;
                pImg1 = pImg0 + srcStride;
                pImg2 = pImg0 - srcStride;
                pImg3 = pImg1 + srcStride;
                pImg4 = pImg2 - srcStride;
                pImg5 = pImg3 + srcStride;
                pImg6 = pImg4 - srcStride;

                const int yVb = (blk_dst.y + i + ii) & (ctu_height - 1);
                if (yVb < virbnd_pos && (yVb >= virbnd_pos - 4))   // above
                {
                    pImg1 = (yVb == virbnd_pos - 1) ? pImg0 : pImg1;
                    pImg3 = (yVb >= virbnd_pos - 2) ? pImg1 : pImg3;
                    pImg5 = (yVb >= virbnd_pos - 3) ? pImg3 : pImg5;

                    pImg2 = (yVb == virbnd_pos - 1) ? pImg0 : pImg2;
                    pImg4 = (yVb >= virbnd_pos - 2) ? pImg2 : pImg4;
                    pImg6 = (yVb >= virbnd_pos - 3) ? pImg4 : pImg6;
                }
                else if (yVb >= virbnd_pos && (yVb <= virbnd_pos + 3))   // bottom
                {
                    pImg2 = (yVb == virbnd_pos) ? pImg0 : pImg2;
                    pImg4 = (yVb <= virbnd_pos + 1) ? pImg2 : pImg4;
                    pImg6 = (yVb <= virbnd_pos + 2) ? pImg4 : pImg6;

                    pImg1 = (yVb == virbnd_pos) ? pImg0 : pImg1;
                    pImg3 = (yVb <= virbnd_pos + 1) ? pImg1 : pImg3;
                    pImg5 = (yVb <= virbnd_pos + 2) ? pImg3 : pImg5;
                }

                __m128i cur = _mm_loadu_si128((const __m128i *) pImg0);

                uint8_t isNearVBabove = yVb < virbnd_pos && (yVb >= virbnd_pos - 1);
                uint8_t isNearVBbelow = yVb >= virbnd_pos && (yVb <= virbnd_pos);

                __m128i accumA, accumB;
                if (!(isNearVBabove || isNearVBbelow))
                {
                    accumA = mmOffset;
                    accumB = mmOffset;
                }
                else
                {
                    //Rounding offset fix
                    accumA = mmOffsetborder;
                    accumB = mmOffsetborder;
                }

                process2coeffs7x7SSE(0, pImg5 + 0, pImg6 + 0, pImg3 + 1, pImg4 - 1);
                process2coeffs7x7SSE(1, pImg3 + 0, pImg4 + 0, pImg3 - 1, pImg4 + 1);
                process2coeffs7x7SSE(2, pImg1 + 2, pImg2 - 2, pImg1 + 1, pImg2 - 1);
                process2coeffs7x7SSE(3, pImg1 + 0, pImg2 + 0, pImg1 - 1, pImg2 + 1);
                process2coeffs7x7SSE(4, pImg1 - 2, pImg2 + 2, pImg0 + 3, pImg0 - 3);
                process2coeffs7x7SSE(5, pImg0 + 2, pImg0 - 2, pImg0 + 1, pImg0 - 1);

                if (!(isNearVBabove || isNearVBbelow))
                {
                    accumA = _mm_srai_epi32(accumA, SHIFT);
                    accumB = _mm_srai_epi32(accumB, SHIFT);
                }
                else
                {
                    //Rounding offset fix
                    accumA = _mm_srai_epi32(accumA, SHIFTborder);
                    accumB = _mm_srai_epi32(accumB, SHIFTborder);
                }
                accumA = _mm_packs_epi32(accumA, accumB);
                accumA = _mm_add_epi16(accumA, cur);
                accumA = _mm_min_epi16(mmMax, _mm_max_epi16(accumA, mmMin));

                // if (j>=x0 && i+ii>=y0 && j<=w_max && i+ii<=h_max)
                _mm_storeu_si128((__m128i *) (_dst + ii * dstStride + j), accumA);
            }
        }

        _src += srcStride * STEP_Y;
        _dst += dstStride * STEP_Y;
    }
}

#define selectEvenValuesSSE(dest, src0, src1) {\
  __m128i a0 = _mm_shufflelo_epi16(src0, 0xD8);\
  __m128i a1 = _mm_shufflelo_epi16(src1, 0xD8);\
  a0 = _mm_shufflehi_epi16(a0, 0xD8);\
  a1 = _mm_shufflehi_epi16(a1, 0xD8);\
  __m128i b0 = _mm_unpacklo_epi32(a0, a1);\
  __m128i b1 = _mm_unpackhi_epi32(a0, a1);\
  dest = _mm_unpacklo_epi32(b0, b1);\
}

#define selectOddValuesSSE(dest, src0, src1) {\
  __m128i a0 = _mm_shufflelo_epi16(src0, 0xD8);\
  __m128i a1 = _mm_shufflelo_epi16(src1, 0xD8);\
  a0 = _mm_shufflehi_epi16(a0, 0xD8);\
  a1 = _mm_shufflehi_epi16(a1, 0xD8);\
  afficherVecteur8SSE128(a0);\
  afficherVecteur8SSE128(a1);\
  printf("\n");\
  __m128i b0 = _mm_unpacklo_epi32(a0, a1);\
  __m128i b1 = _mm_unpackhi_epi32(a0, a1);\
  dest = _mm_unpackhi_epi32(b0, b1);\
}

#define selectEvenOddValuesSSE(even, odd, src0, src1) {\
  __m128i a0 = _mm_shufflelo_epi16(src0, 0xD8);\
  __m128i a1 = _mm_shufflelo_epi16(src1, 0xD8);\
  a0 = _mm_shufflehi_epi16(a0, 0xD8);\
  a1 = _mm_shufflehi_epi16(a1, 0xD8);\
  __m128i b0 = _mm_unpacklo_epi32(a0, a1);\
  __m128i b1 = _mm_unpackhi_epi32(a0, a1);\
  even = _mm_unpacklo_epi32(b0, b1);\
  odd = _mm_unpackhi_epi32(b0, b1);\
}

static void simdDeriveClassificationBlk_sse(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr,
                                        OVSample *const src, const int stride, const Area blk,
                                        const int shift, const int ctu_s, int virbnd_pos)
{
    int blk_h = blk.height;
    int blk_w = blk.width;

    uint16_t colSums[18][40];
    int i;
    const uint32_t ctb_msk = ctu_s - 1;
    const OVSample *_src = src - 3 * stride - 3;

    for (i = 0; i < blk_h + 4; i += 2) {
        const OVSample *src0 = &_src[0         ];
        const OVSample *src1 = &_src[stride    ];
        const OVSample *src2 = &_src[stride * 2];
        const OVSample *src3 = &_src[stride * 3];

        const int y = blk.y - 2 + i;
        int j;

        if (y > 0 && (y & ctb_msk) == virbnd_pos - 2) {
            src3 = src2;
        } else if (y > 0 && (y & ctb_msk) == virbnd_pos) {
            src0 = src1;
        }

        __m128i prev = _mm_setzero_si128();

        for (j = 0; j < blk_w + 4; j += 8) {
            const __m128i x0 = _mm_loadu_si128((const __m128i *) (src0 + j));
            const __m128i x1 = _mm_loadu_si128((const __m128i *) (src1 + j));
            const __m128i x2 = _mm_loadu_si128((const __m128i *) (src2 + j));
            const __m128i x3 = _mm_loadu_si128((const __m128i *) (src3 + j));

            const __m128i x4 = _mm_loadu_si128((const __m128i *) (src0 + j + 2));
            const __m128i x5 = _mm_loadu_si128((const __m128i *) (src1 + j + 2));
            const __m128i x6 = _mm_loadu_si128((const __m128i *) (src2 + j + 2));
            const __m128i x7 = _mm_loadu_si128((const __m128i *) (src3 + j + 2));

            const __m128i nw = _mm_blend_epi16(x0, x1, 0xaa);
            const __m128i n  = _mm_blend_epi16(x0, x5, 0x55);
            const __m128i ne = _mm_blend_epi16(x4, x5, 0xaa);
            const __m128i w  = _mm_blend_epi16(x1, x2, 0xaa);
            const __m128i e  = _mm_blend_epi16(x5, x6, 0xaa);
            const __m128i sw = _mm_blend_epi16(x2, x3, 0xaa);
            const __m128i s  = _mm_blend_epi16(x2, x7, 0x55);
            const __m128i se = _mm_blend_epi16(x6, x7, 0xaa);

            __m128i c = _mm_blend_epi16(x1, x6, 0x55);
            c         = _mm_add_epi16(c, c);
            __m128i d = _mm_shuffle_epi8(c, _mm_setr_epi8(2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13));

            const __m128i ver = _mm_abs_epi16(_mm_sub_epi16(c, _mm_add_epi16(n, s)));
            const __m128i hor = _mm_abs_epi16(_mm_sub_epi16(d, _mm_add_epi16(w, e)));
            const __m128i di0 = _mm_abs_epi16(_mm_sub_epi16(d, _mm_add_epi16(nw, se)));
            const __m128i di1 = _mm_abs_epi16(_mm_sub_epi16(d, _mm_add_epi16(ne, sw)));

            const __m128i hv  = _mm_hadd_epi16(ver, hor);
            const __m128i di  = _mm_hadd_epi16(di0, di1);
            const __m128i all = _mm_hadd_epi16(hv, di);

            const __m128i t = _mm_blend_epi16(all, prev, 0xaa);
            _mm_storeu_si128((__m128i *) &colSums[i >> 1][j], _mm_hadd_epi16(t, all));

            prev = all;
        }
        _src += stride << 1;
    }

    for (i = 0; i < (blk_h >> 1); i += 4) {
        __m128i class_idx[4], transpose_idx[4];
        const uint32_t z = (2 * i + blk.y) & ctb_msk;
        const uint32_t z2 = (2 * i + 4 + blk.y) & ctb_msk;

        int sb_y = ((2 * i + blk.y) & ctb_msk) >> 2;
        int sb_x = ((blk.x)         & ctb_msk) >> 2;

        for (size_t k = 0; k < 4; k++) {
            __m128i x0, x1, x2, x3, x4, x5, x6, x7;


            x0 = (z == virbnd_pos) ? _mm_setzero_si128() : _mm_loadu_si128((__m128i *) &colSums[i + 0][(k*8) + 4]);
            x1 = _mm_loadu_si128((__m128i *) &colSums[i + 1][(k*8) + 4]);
            x2 = _mm_loadu_si128((__m128i *) &colSums[i + 2][(k*8) + 4]);
            x3 = (z == virbnd_pos - 4) ? _mm_setzero_si128() : _mm_loadu_si128((__m128i *) &colSums[i + 3][(k*8) + 4]);

            x4 = (z2 == virbnd_pos) ? _mm_setzero_si128() : _mm_loadu_si128((__m128i *) &colSums[i + 2][(k*8) + 4]);
            x5 = _mm_loadu_si128((__m128i *) &colSums[i + 3][(k*8) + 4]);
            x6 = _mm_loadu_si128((__m128i *) &colSums[i + 4][(k*8) + 4]);
            x7 = (z2 == virbnd_pos - 4) ? _mm_setzero_si128() : _mm_loadu_si128((__m128i *) &colSums[i + 5][(k*8) + 4]);

            __m128i x0l = _mm_cvtepu16_epi32(x0);
            __m128i x0h = _mm_unpackhi_epi16(x0, _mm_setzero_si128());
            __m128i x1l = _mm_cvtepu16_epi32(x1);
            __m128i x1h = _mm_unpackhi_epi16(x1, _mm_setzero_si128());
            __m128i x2l = _mm_cvtepu16_epi32(x2);
            __m128i x2h = _mm_unpackhi_epi16(x2, _mm_setzero_si128());
            __m128i x3l = _mm_cvtepu16_epi32(x3);
            __m128i x3h = _mm_unpackhi_epi16(x3, _mm_setzero_si128());
            __m128i x4l = _mm_cvtepu16_epi32(x4);
            __m128i x4h = _mm_unpackhi_epi16(x4, _mm_setzero_si128());
            __m128i x5l = _mm_cvtepu16_epi32(x5);
            __m128i x5h = _mm_unpackhi_epi16(x5, _mm_setzero_si128());
            __m128i x6l = _mm_cvtepu16_epi32(x6);
            __m128i x6h = _mm_unpackhi_epi16(x6, _mm_setzero_si128());
            __m128i x7l = _mm_cvtepu16_epi32(x7);
            __m128i x7h = _mm_unpackhi_epi16(x7, _mm_setzero_si128());

            x0l = _mm_add_epi32(x0l, x1l);
            x2l = _mm_add_epi32(x2l, x3l);
            x4l = _mm_add_epi32(x4l, x5l);
            x6l = _mm_add_epi32(x6l, x7l);
            x0h = _mm_add_epi32(x0h, x1h);
            x2h = _mm_add_epi32(x2h, x3h);
            x4h = _mm_add_epi32(x4h, x5h);
            x6h = _mm_add_epi32(x6h, x7h);

            x0l = _mm_add_epi32(x0l, x2l);
            x4l = _mm_add_epi32(x4l, x6l);
            x0h = _mm_add_epi32(x0h, x2h);
            x4h = _mm_add_epi32(x4h, x6h);

            x2l = _mm_unpacklo_epi32(x0l, x4l);
            x2h = _mm_unpackhi_epi32(x0l, x4l);
            x6l = _mm_unpacklo_epi32(x0h, x4h);
            x6h = _mm_unpackhi_epi32(x0h, x4h);

            __m128i sumV  = _mm_unpacklo_epi32(x2l, x6l);
            __m128i sumH  = _mm_unpackhi_epi32(x2l, x6l);
            __m128i sumD0 = _mm_unpacklo_epi32(x2h, x6h);
            __m128i sumD1 = _mm_unpackhi_epi32(x2h, x6h);

            __m128i tempAct = _mm_add_epi32(sumV, sumH);

            const uint32_t scale  = (z == virbnd_pos - 4 || z == virbnd_pos) ? 96 : 64;
            const uint32_t scale2 = (z2 == virbnd_pos - 4 || z2 == virbnd_pos) ? 96 : 64;

            __m128i activity = _mm_mullo_epi32(tempAct, _mm_unpacklo_epi64(_mm_set1_epi32(scale), _mm_set1_epi32(scale2)));
            activity         = _mm_srl_epi32(activity, _mm_cvtsi32_si128(shift));
            activity         = _mm_min_epi32(activity, _mm_set1_epi32(15));
            __m128i classIdx = _mm_shuffle_epi8(_mm_setr_epi8(0, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4), activity);

            __m128i dirTempHVMinus1 = _mm_cmpgt_epi32(sumV, sumH);
            __m128i hv1             = _mm_max_epi32(sumV, sumH);
            __m128i hv0             = _mm_min_epi32(sumV, sumH);

            __m128i dirTempDMinus1 = _mm_cmpgt_epi32(sumD0, sumD1);
            __m128i d1             = _mm_max_epi32(sumD0, sumD1);
            __m128i d0             = _mm_min_epi32(sumD0, sumD1);

            __m128i a      = _mm_xor_si128(_mm_mullo_epi32(d1, hv0), _mm_set1_epi32(0x80000000));
            __m128i b      = _mm_xor_si128(_mm_mullo_epi32(hv1, d0), _mm_set1_epi32(0x80000000));

            __m128i dirIdx = _mm_cmpgt_epi32(a, b);
            __m128i hvd1   = _mm_blendv_epi8(hv1, d1, dirIdx);
            __m128i hvd0   = _mm_blendv_epi8(hv0, d0, dirIdx);

            __m128i strength1 = _mm_cmpgt_epi32(hvd1, _mm_add_epi32(hvd0, hvd0));
            __m128i strength2 = _mm_cmpgt_epi32(_mm_add_epi32(hvd1, hvd1), _mm_add_epi32(hvd0, _mm_slli_epi32(hvd0, 3)));
            __m128i offset    = _mm_and_si128(strength1, _mm_set1_epi32(5));
            classIdx          = _mm_add_epi32(classIdx, offset);
            classIdx          = _mm_add_epi32(classIdx, _mm_and_si128(strength2, _mm_set1_epi32(5)));
            offset            = _mm_andnot_si128(dirIdx, offset);
            offset            = _mm_add_epi32(offset, offset);
            classIdx          = _mm_add_epi32(classIdx, offset);

            __m128i transposeIdx = _mm_set1_epi32(3);
            transposeIdx         = _mm_add_epi32(transposeIdx, dirTempHVMinus1);
            transposeIdx         = _mm_add_epi32(transposeIdx, dirTempDMinus1);
            transposeIdx         = _mm_add_epi32(transposeIdx, dirTempDMinus1);

            class_idx[k] = _mm_shuffle_epi8(classIdx, _mm_setr_epi8(0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12));

            transpose_idx[k] = _mm_shuffle_epi8(transposeIdx, _mm_setr_epi8(0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12));
        }

        __m128i c1, c2, t1, t2;

        c1 = _mm_unpacklo_epi16(class_idx[0], class_idx[1]);
        c2 = _mm_unpacklo_epi16(class_idx[2], class_idx[3]);
        c1 = _mm_unpacklo_epi32(c1, c2);

        t1 = _mm_unpacklo_epi16(transpose_idx[0], transpose_idx[1]);
        t2 = _mm_unpacklo_epi16(transpose_idx[2], transpose_idx[3]);
        t1 = _mm_unpacklo_epi32(t1, t2);

        _mm_storel_epi64((__m128i *) (class_idx_arr +  sb_y      * CLASSIFICATION_BLK_SIZE + sb_x), c1);
        _mm_storel_epi64((__m128i *) (class_idx_arr + (sb_y + 1) * CLASSIFICATION_BLK_SIZE + sb_x), _mm_bsrli_si128(c1, 8));


        _mm_storel_epi64((__m128i *) (transpose_idx_arr + sb_y * CLASSIFICATION_BLK_SIZE + sb_x), t1);
        _mm_storel_epi64((__m128i *) (transpose_idx_arr + (sb_y + 1) * CLASSIFICATION_BLK_SIZE + sb_x), _mm_bsrli_si128(t1, 8));
    }
}

#define process2coeffs5x5AVX2(i, ptr0, ptr1, ptr2, ptr3) { \
                    const __m256i val00 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr0)), cur);\
                    const __m256i val10 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr2)), cur);\
                    const __m256i val01 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr1)), cur);\
                    const __m256i val11 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr3)), cur);\
                    \
                    __m256i val01A = _mm256_unpacklo_epi16(val00, val10);\
                    __m256i val01B = _mm256_unpackhi_epi16(val00, val10);\
                    __m256i val01C = _mm256_unpacklo_epi16(val01, val11);\
                    __m256i val01D = _mm256_unpackhi_epi16(val01, val11);\
                    \
                    __m256i limit01A = params[1][i];\
                    \
                    val01A = _mm256_min_epi16(val01A, limit01A);\
                    val01B = _mm256_min_epi16(val01B, limit01A);\
                    val01C = _mm256_min_epi16(val01C, limit01A);\
                    val01D = _mm256_min_epi16(val01D, limit01A);\
                    \
                    limit01A = _mm256_sub_epi16(_mm256_setzero_si256(), limit01A);\
                    \
                    val01A = _mm256_max_epi16(val01A, limit01A);\
                    val01B = _mm256_max_epi16(val01B, limit01A);\
                    val01C = _mm256_max_epi16(val01C, limit01A);\
                    val01D = _mm256_max_epi16(val01D, limit01A);\
                    \
                    val01A = _mm256_add_epi16(val01A, val01C);\
                    val01B = _mm256_add_epi16(val01B, val01D);\
                    \
                    __m256i coeff01A = params[0][i];\
                    \
                    accumA = _mm256_add_epi32(accumA, _mm256_madd_epi16(val01A, coeff01A));\
                    accumB = _mm256_add_epi32(accumB, _mm256_madd_epi16(val01B, coeff01A));\
                };\

#define process2coeffs7x7AVX2(i, ptr0, ptr1, ptr2, ptr3) { \
        const __m256i val00 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr0)), cur); \
        const __m256i val10 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr2)), cur); \
        const __m256i val01 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr1)), cur); \
        const __m256i val11 = _mm256_sub_epi16(_mm256_loadu_si256((const __m256i *) (ptr3)), cur); \
        \
        __m256i val01A = _mm256_unpacklo_epi16(val00, val10); \
        __m256i val01B = _mm256_unpackhi_epi16(val00, val10); \
        __m256i val01C = _mm256_unpacklo_epi16(val01, val11); \
        __m256i val01D = _mm256_unpackhi_epi16(val01, val11); \
        \
        __m256i limit01A = params[0][1][i]; \
        __m256i limit01B = params[1][1][i]; \
        \
        val01A = _mm256_min_epi16(val01A, limit01A); \
        val01B = _mm256_min_epi16(val01B, limit01B); \
        val01C = _mm256_min_epi16(val01C, limit01A); \
        val01D = _mm256_min_epi16(val01D, limit01B); \
        \
        limit01A = _mm256_sub_epi16(_mm256_setzero_si256(), limit01A); \
        limit01B = _mm256_sub_epi16(_mm256_setzero_si256(), limit01B); \
        \
        val01A = _mm256_max_epi16(val01A, limit01A); \
        val01B = _mm256_max_epi16(val01B, limit01B); \
        val01C = _mm256_max_epi16(val01C, limit01A); \
        val01D = _mm256_max_epi16(val01D, limit01B); \
        \
        val01A = _mm256_add_epi16(val01A, val01C); \
        val01B = _mm256_add_epi16(val01B, val01D); \
        \
        const __m256i coeff01A = params[0][0][i]; \
        const __m256i coeff01B = params[1][0][i]; \
        \
        accumA = _mm256_add_epi32(accumA, _mm256_madd_epi16(val01A, coeff01A)); \
        accumB = _mm256_add_epi32(accumB, _mm256_madd_epi16(val01B, coeff01B)); \
        }; \


static void simdDeriveClassificationBlk_avx2(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr,
                                        OVSample *const src, const int stride, const Area blk,
                                        const int shift, const int ctu_s, int virbnd_pos)
{
    int blk_h = blk.height;
    int blk_w = blk.width;
    if( blk_w & 15 )
    {
        simdDeriveClassificationBlk_sse(class_idx_arr, transpose_idx_arr, src, stride, blk, shift, ctu_s, virbnd_pos);
        return;
    }

    uint16_t colSums[18][48];
    int i;
    const uint32_t ctb_msk = ctu_s - 1;
    const OVSample *_src = src - 3 * stride - 3;

    for (i = 0; i < blk_h + 4; i += 2) {
        const OVSample *src0 = &_src[0         ];
        const OVSample *src1 = &_src[stride    ];
        const OVSample *src2 = &_src[stride * 2];
        const OVSample *src3 = &_src[stride * 3];

        const int y = blk.y - 2 + i;
        int j;

        if (y > 0 && (y & ctb_msk) == virbnd_pos - 2) {
            src3 = src2;
        } else if (y > 0 && (y & ctb_msk) == virbnd_pos) {
            src0 = src1;
        }

        __m128i prev = _mm_setzero_si128();

        for (j = 0; j < blk_w + 4; j += 16) {
            const __m256i x0 = _mm256_loadu_si256((const __m256i *) (src0 + j));
            const __m256i x1 = _mm256_loadu_si256((const __m256i *) (src1 + j));
            const __m256i x2 = _mm256_loadu_si256((const __m256i *) (src2 + j));
            const __m256i x3 = _mm256_loadu_si256((const __m256i *) (src3 + j));

            const __m256i x4 = _mm256_loadu_si256((const __m256i *) (src0 + j + 2));
            const __m256i x5 = _mm256_loadu_si256((const __m256i *) (src1 + j + 2));
            const __m256i x6 = _mm256_loadu_si256((const __m256i *) (src2 + j + 2));
            const __m256i x7 = _mm256_loadu_si256((const __m256i *) (src3 + j + 2));

            const __m256i nw = _mm256_blend_epi16(x0, x1, 0xaa);
            const __m256i n  = _mm256_blend_epi16(x0, x5, 0x55);
            const __m256i ne = _mm256_blend_epi16(x4, x5, 0xaa);
            const __m256i w  = _mm256_blend_epi16(x1, x2, 0xaa);
            const __m256i e  = _mm256_blend_epi16(x5, x6, 0xaa);
            const __m256i sw = _mm256_blend_epi16(x2, x3, 0xaa);
            const __m256i s  = _mm256_blend_epi16(x2, x7, 0x55);
            const __m256i se = _mm256_blend_epi16(x6, x7, 0xaa);

            __m256i c = _mm256_blend_epi16(x1, x6, 0x55);
            c         = _mm256_add_epi16(c, c);
            __m256i d = _mm256_shuffle_epi8(c, _mm256_setr_epi8(2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13, 2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13));

            const __m256i ver = _mm256_abs_epi16(_mm256_sub_epi16(c, _mm256_add_epi16(n, s)));
            const __m256i hor = _mm256_abs_epi16(_mm256_sub_epi16(d, _mm256_add_epi16(w, e)));
            const __m256i di0 = _mm256_abs_epi16(_mm256_sub_epi16(d, _mm256_add_epi16(nw, se)));
            const __m256i di1 = _mm256_abs_epi16(_mm256_sub_epi16(d, _mm256_add_epi16(ne, sw)));

            const __m256i hv  = _mm256_hadd_epi16(ver, hor);
            const __m256i di  = _mm256_hadd_epi16(di0, di1);
            const __m256i all = _mm256_hadd_epi16(hv, di);

            const __m256i t = _mm256_blend_epi16(all, _mm256_inserti128_si256(_mm256_castsi128_si256(prev), _mm256_extracti128_si256(all, 0), 1), 0xaa);
            _mm256_storeu_si256((__m256i *) &colSums[i >> 1][j], _mm256_hadd_epi16(t, all));

            prev = _mm256_extracti128_si256(all, 1);
        }
        _src += stride << 1;
    }
    for (i = 0; i < (blk_h >> 1); i += 4) {
        __m256i class_idx[2], transpose_idx[2];
        const uint32_t z = (2 * i + blk.y) & ctb_msk;
        const uint32_t z2 = (2 * i + 4 + blk.y) & ctb_msk;

        int sb_y = ((2 * i + blk.y) & ctb_msk) >> 2;
        int sb_x = ((blk.x)         & ctb_msk) >> 2;

        for (size_t k = 0; k < 2; k++) {
            __m256i x0, x1, x2, x3, x4, x5, x6, x7;


            x0 = (z == virbnd_pos) ? _mm256_setzero_si256() : _mm256_loadu_si256((__m256i *) &colSums[i + 0][(k*16) + 4]);
            x1 = _mm256_loadu_si256((__m256i *) &colSums[i + 1][(k*16) + 4]);
            x2 = _mm256_loadu_si256((__m256i *) &colSums[i + 2][(k*16) + 4]);
            x3 = (z == virbnd_pos - 4) ? _mm256_setzero_si256() : _mm256_loadu_si256((__m256i *) &colSums[i + 3][(k*16) + 4]);

            x4 = (z2 == virbnd_pos) ? _mm256_setzero_si256() : _mm256_loadu_si256((__m256i *) &colSums[i + 2][(k*16) + 4]);
            x5 = _mm256_loadu_si256((__m256i *) &colSums[i + 3][(k*16) + 4]);
            x6 = _mm256_loadu_si256((__m256i *) &colSums[i + 4][(k*16) + 4]);
            x7 = (z2 == virbnd_pos - 4) ? _mm256_setzero_si256() : _mm256_loadu_si256((__m256i *) &colSums[i + 5][(k*16) + 4]);

            __m256i x0l = _mm256_unpacklo_epi16(x0, _mm256_setzero_si256());
            __m256i x0h = _mm256_unpackhi_epi16(x0, _mm256_setzero_si256());
            __m256i x1l = _mm256_unpacklo_epi16(x1, _mm256_setzero_si256());
            __m256i x1h = _mm256_unpackhi_epi16(x1, _mm256_setzero_si256());
            __m256i x2l = _mm256_unpacklo_epi16(x2, _mm256_setzero_si256());
            __m256i x2h = _mm256_unpackhi_epi16(x2, _mm256_setzero_si256());
            __m256i x3l = _mm256_unpacklo_epi16(x3, _mm256_setzero_si256());
            __m256i x3h = _mm256_unpackhi_epi16(x3, _mm256_setzero_si256());
            __m256i x4l = _mm256_unpacklo_epi16(x4, _mm256_setzero_si256());
            __m256i x4h = _mm256_unpackhi_epi16(x4, _mm256_setzero_si256());
            __m256i x5l = _mm256_unpacklo_epi16(x5, _mm256_setzero_si256());
            __m256i x5h = _mm256_unpackhi_epi16(x5, _mm256_setzero_si256());
            __m256i x6l = _mm256_unpacklo_epi16(x6, _mm256_setzero_si256());
            __m256i x6h = _mm256_unpackhi_epi16(x6, _mm256_setzero_si256());
            __m256i x7l = _mm256_unpacklo_epi16(x7, _mm256_setzero_si256());
            __m256i x7h = _mm256_unpackhi_epi16(x7, _mm256_setzero_si256());

            x0l = _mm256_add_epi32(x0l, x1l);
            x2l = _mm256_add_epi32(x2l, x3l);
            x4l = _mm256_add_epi32(x4l, x5l);
            x6l = _mm256_add_epi32(x6l, x7l);
            x0h = _mm256_add_epi32(x0h, x1h);
            x2h = _mm256_add_epi32(x2h, x3h);
            x4h = _mm256_add_epi32(x4h, x5h);
            x6h = _mm256_add_epi32(x6h, x7h);

            x0l = _mm256_add_epi32(x0l, x2l);
            x4l = _mm256_add_epi32(x4l, x6l);
            x0h = _mm256_add_epi32(x0h, x2h);
            x4h = _mm256_add_epi32(x4h, x6h);

            x2l = _mm256_unpacklo_epi32(x0l, x4l);
            x2h = _mm256_unpackhi_epi32(x0l, x4l);
            x6l = _mm256_unpacklo_epi32(x0h, x4h);
            x6h = _mm256_unpackhi_epi32(x0h, x4h);

            __m256i sumV  = _mm256_unpacklo_epi32(x2l, x6l);
            __m256i sumH  = _mm256_unpackhi_epi32(x2l, x6l);
            __m256i sumD0 = _mm256_unpacklo_epi32(x2h, x6h);
            __m256i sumD1 = _mm256_unpackhi_epi32(x2h, x6h);

            __m256i tempAct = _mm256_add_epi32(sumV, sumH);

            const uint32_t scale  = (z == virbnd_pos - 4 || z == virbnd_pos) ? 96 : 64;
            const uint32_t scale2 = (z2 == virbnd_pos - 4 || z2 == virbnd_pos) ? 96 : 64;

            __m256i activity = _mm256_mullo_epi32(tempAct, _mm256_unpacklo_epi64(_mm256_set1_epi32(scale), _mm256_set1_epi32(scale2)));
            activity         = _mm256_srli_epi32(activity, shift);
            activity         = _mm256_min_epi32(activity, _mm256_set1_epi32(15));
            __m256i classIdx = _mm256_shuffle_epi8(_mm256_setr_epi8(0, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 0, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4), activity);

            __m256i dirTempHVMinus1 = _mm256_cmpgt_epi32(sumV, sumH);
            __m256i hv1             = _mm256_max_epi32(sumV, sumH);
            __m256i hv0             = _mm256_min_epi32(sumV, sumH);

            __m256i dirTempDMinus1 = _mm256_cmpgt_epi32(sumD0, sumD1);
            __m256i d1             = _mm256_max_epi32(sumD0, sumD1);
            __m256i d0             = _mm256_min_epi32(sumD0, sumD1);

            __m256i a      = _mm256_xor_si256(_mm256_mullo_epi32(d1, hv0), _mm256_set1_epi32(0x80000000));
            __m256i b      = _mm256_xor_si256(_mm256_mullo_epi32(hv1, d0), _mm256_set1_epi32(0x80000000));

            __m256i dirIdx = _mm256_cmpgt_epi32(a, b);
            __m256i hvd1   = _mm256_blendv_epi8(hv1, d1, dirIdx);
            __m256i hvd0   = _mm256_blendv_epi8(hv0, d0, dirIdx);

            __m256i strength1 = _mm256_cmpgt_epi32(hvd1, _mm256_add_epi32(hvd0, hvd0));
            __m256i strength2 = _mm256_cmpgt_epi32(_mm256_add_epi32(hvd1, hvd1), _mm256_add_epi32(hvd0, _mm256_slli_epi32(hvd0, 3)));
            __m256i offset    = _mm256_and_si256(strength1, _mm256_set1_epi32(5));
            classIdx          = _mm256_add_epi32(classIdx, offset);
            classIdx          = _mm256_add_epi32(classIdx, _mm256_and_si256(strength2, _mm256_set1_epi32(5)));
            offset            = _mm256_andnot_si256(dirIdx, offset);
            offset            = _mm256_add_epi32(offset, offset);
            classIdx          = _mm256_add_epi32(classIdx, offset);

            __m256i transposeIdx = _mm256_set1_epi32(3);
            transposeIdx         = _mm256_add_epi32(transposeIdx, dirTempHVMinus1);
            transposeIdx         = _mm256_add_epi32(transposeIdx, dirTempDMinus1);
            transposeIdx         = _mm256_add_epi32(transposeIdx, dirTempDMinus1);

            class_idx[k] = _mm256_shuffle_epi8(classIdx, _mm256_setr_epi8(0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12,
                                                                    0, 4, 8, 12));

            transpose_idx[k] = _mm256_shuffle_epi8(transposeIdx, _mm256_setr_epi8(0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12,
                                                                            0, 4, 8, 12));
        }

        __m128i c1, c2, t1, t2;

        c1 = _mm_unpacklo_epi16(_mm256_extracti128_si256(class_idx[0], 0), _mm256_extracti128_si256(class_idx[0], 1));
        c2 = _mm_unpacklo_epi16(_mm256_extracti128_si256(class_idx[1], 0), _mm256_extracti128_si256(class_idx[1], 1));
        c1 = _mm_unpacklo_epi32(c1, c2);

        t1 = _mm_unpacklo_epi16(_mm256_extracti128_si256(transpose_idx[0], 0), _mm256_extracti128_si256(transpose_idx[0], 1));
        t2 = _mm_unpacklo_epi16(_mm256_extracti128_si256(transpose_idx[1], 0), _mm256_extracti128_si256(transpose_idx[1], 1));
        t1 = _mm_unpacklo_epi32(t1, t2);

        _mm_storel_epi64((__m128i *) (class_idx_arr +  sb_y      * CLASSIFICATION_BLK_SIZE + sb_x), c1);
        _mm_storel_epi64((__m128i *) (class_idx_arr + (sb_y + 1) * CLASSIFICATION_BLK_SIZE + sb_x), _mm_bsrli_si128(c1, 8));


        _mm_storel_epi64((__m128i *) (transpose_idx_arr + sb_y * CLASSIFICATION_BLK_SIZE + sb_x), t1);
        _mm_storel_epi64((__m128i *) (transpose_idx_arr + (sb_y + 1) * CLASSIFICATION_BLK_SIZE + sb_x), _mm_bsrli_si128(t1, 8));
    }

}

static void
simdFilter5x5Blk_avx2(OVSample *const dst, const OVSample *const src,
                 const int dstStride, const int srcStride,
                 Area blk_dst,
                 const int16_t *const filter_set, const int16_t *const clip_set,
                 const int ctu_height, int virbnd_pos)
{
    const int SHIFT = NUM_BITS - 1;
    const int ROUND = 1 << (SHIFT - 1);

    const size_t STEP_X = 16;
    const size_t STEP_Y = 4;

    const int clpMin = 0;
    const int clpMax = (1<<10) - 1;

    const OVSample * _src =  src;
    OVSample * _dst = dst;

    const __m256i mmOffset = _mm256_set1_epi32(ROUND);
    const __m256i mmMin = _mm256_set1_epi16( clpMin );
    const __m256i mmMax = _mm256_set1_epi16( clpMax );

    __m256i params[2][3];
    __m256i fs   = _mm256_castsi128_si256( _mm_loadu_si128( ( __m128i* ) filter_set ) );
    fs = _mm256_inserti128_si256( fs, _mm256_extracti128_si256( fs, 0 ), 1 );
    params[0][0] = _mm256_shuffle_epi32(fs, 0x00);
    params[0][1] = _mm256_shuffle_epi32(fs, 0x55);
    params[0][2] = _mm256_shuffle_epi32(fs, 0xaa);
    __m256i fc   = _mm256_castsi128_si256( _mm_loadu_si128( ( __m128i* ) clip_set ) );
    fc = _mm256_inserti128_si256( fc, _mm256_extracti128_si256( fc, 0 ), 1 );
    params[1][0] = _mm256_shuffle_epi32( fc, 0x00 );
    params[1][1] = _mm256_shuffle_epi32( fc, 0x55 );
    params[1][2] = _mm256_shuffle_epi32( fc, 0xaa );

    for (size_t i = 0; i < blk_dst.height; i += STEP_Y) {
        for (size_t j = 0; j < blk_dst.width; j += STEP_X) {
            for (size_t ii = 0; ii < STEP_Y; ii++) {
                const uint16_t *pImg0, *pImg1, *pImg2, *pImg3, *pImg4;

                pImg0 = (uint16_t*)_src + j + ii * srcStride ;
                pImg1 = pImg0 + srcStride;
                pImg2 = pImg0 - srcStride;
                pImg3 = pImg1 + srcStride;
                pImg4 = pImg2 - srcStride;

                __m256i cur = _mm256_loadu_si256((const __m256i *) pImg0);

                __m256i accumA, accumB;

                accumA = mmOffset;
                accumB = mmOffset;

                process2coeffs5x5AVX2(0, pImg3 + 0, pImg4 + 0, pImg1 + 1, pImg2 - 1);
                process2coeffs5x5AVX2(1, pImg1 + 0, pImg2 + 0, pImg1 - 1, pImg2 + 1);
                process2coeffs5x5AVX2(2, pImg0 + 2, pImg0 - 2, pImg0 + 1, pImg0 - 1);

                accumA = _mm256_srai_epi32(accumA, SHIFT);
                accumB = _mm256_srai_epi32(accumB, SHIFT);

                accumA = _mm256_packs_epi32(accumA, accumB);
                accumA = _mm256_add_epi16(accumA, cur);
                accumA = _mm256_min_epi16(mmMax, _mm256_max_epi16(accumA, mmMin));

                _mm256_storeu_si256((__m256i *) (_dst + ii * dstStride + j), accumA);
            }
        }

        _src += srcStride * STEP_Y;
        _dst += dstStride * STEP_Y;
    }
}

static void
simdFilter5x5BlkVB_avx2(OVSample *const dst, const OVSample *const src,
                 const int dstStride, const int srcStride,
                 Area blk_dst,
                 const int16_t *const filter_set, const int16_t *const clip_set,
                 const int ctu_height, int virbnd_pos)
{
    const int SHIFT = NUM_BITS - 1;
    const int ROUND = 1 << (SHIFT - 1);

    const size_t STEP_X = 16;
    const size_t STEP_Y = 4;

    const int clpMin = 0;
    const int clpMax = (1<<10) - 1;

    const OVSample * _src =  src;
    OVSample * _dst = dst;

    const __m256i mmOffset = _mm256_set1_epi32(ROUND);
    const __m256i mmMin = _mm256_set1_epi16( clpMin );
    const __m256i mmMax = _mm256_set1_epi16( clpMax );

    const __m256i mmOffsetborder = _mm256_set1_epi32(1 << ((SHIFT + 3) - 1));
    const int SHIFTborder = SHIFT+3;


    __m256i params[2][3];
    __m256i fs   = _mm256_castsi128_si256( _mm_loadu_si128( ( __m128i* ) filter_set ) );
    fs = _mm256_inserti128_si256( fs, _mm256_extracti128_si256( fs, 0 ), 1 );
    params[0][0] = _mm256_shuffle_epi32(fs, 0x00);
    params[0][1] = _mm256_shuffle_epi32(fs, 0x55);
    params[0][2] = _mm256_shuffle_epi32(fs, 0xaa);
    __m256i fc   = _mm256_castsi128_si256( _mm_loadu_si128( ( __m128i* ) clip_set ) );
    fc = _mm256_inserti128_si256( fc, _mm256_extracti128_si256( fc, 0 ), 1 );
    params[1][0] = _mm256_shuffle_epi32( fc, 0x00 );
    params[1][1] = _mm256_shuffle_epi32( fc, 0x55 );
    params[1][2] = _mm256_shuffle_epi32( fc, 0xaa );

    for (size_t i = 0; i < blk_dst.height; i += STEP_Y) {
        for (size_t j = 0; j < blk_dst.width; j += STEP_X) {
            for (size_t ii = 0; ii < STEP_Y; ii++) {
                const uint16_t *pImg0, *pImg1, *pImg2, *pImg3, *pImg4;

                pImg0 = (uint16_t*)_src + j + ii * srcStride ;
                pImg1 = pImg0 + srcStride;
                pImg2 = pImg0 - srcStride;
                pImg3 = pImg1 + srcStride;
                pImg4 = pImg2 - srcStride;

                const int yVb = (blk_dst.y + i + ii) & (ctu_height - 1);

                if (yVb < virbnd_pos && (yVb >= virbnd_pos - 2)) {
                    pImg1 = (yVb == virbnd_pos - 1) ? pImg0 : pImg1;
                    pImg3 = (yVb >= virbnd_pos - 2) ? pImg1 : pImg3;

                    pImg2 = (yVb == virbnd_pos - 1) ? pImg0 : pImg2;
                    pImg4 = (yVb >= virbnd_pos - 2) ? pImg2 : pImg4;
                } else if (yVb >= virbnd_pos && (yVb <= virbnd_pos + 1)) {
                    pImg2 = (yVb == virbnd_pos) ? pImg0 : pImg2;
                    pImg4 = (yVb <= virbnd_pos + 1) ? pImg2 : pImg4;

                    pImg1 = (yVb == virbnd_pos) ? pImg0 : pImg1;
                    pImg3 = (yVb <= virbnd_pos + 1) ? pImg1 : pImg3;
                }

                __m256i cur = _mm256_loadu_si256((const __m256i *) pImg0);

                uint8_t isNearVBabove = yVb < virbnd_pos && (yVb >= virbnd_pos - 1);
                uint8_t isNearVBbelow = yVb >= virbnd_pos && (yVb <= virbnd_pos);

                __m256i accumA, accumB;
                if (!(isNearVBabove || isNearVBbelow)) {
                    accumA = mmOffset;
                    accumB = mmOffset;
                } else {
                    //Rounding offset fix
                    accumA = mmOffsetborder;
                    accumB = mmOffsetborder;
                }

                process2coeffs5x5AVX2(0, pImg3 + 0, pImg4 + 0, pImg1 + 1, pImg2 - 1);
                process2coeffs5x5AVX2(1, pImg1 + 0, pImg2 + 0, pImg1 - 1, pImg2 + 1);
                process2coeffs5x5AVX2(2, pImg0 + 2, pImg0 - 2, pImg0 + 1, pImg0 - 1);

                if (!(isNearVBabove || isNearVBbelow)) {
                    accumA = _mm256_srai_epi32(accumA, SHIFT);
                    accumB = _mm256_srai_epi32(accumB, SHIFT);
                } else {
                    //Rounding offset fix
                    accumA = _mm256_srai_epi32(accumA, SHIFTborder);
                    accumB = _mm256_srai_epi32(accumB, SHIFTborder);
                }

                accumA = _mm256_packs_epi32(accumA, accumB);
                accumA = _mm256_add_epi16(accumA, cur);
                accumA = _mm256_min_epi16(mmMax, _mm256_max_epi16(accumA, mmMin));

                if (j + STEP_X <= blk_dst.width)
                {
                    _mm256_storeu_si256((__m256i *) (_dst + ii * dstStride + j), accumA);
                }
                else if(j + 12 <= blk_dst.width)
                {
                    _mm_storeu_si128((__m128i *) (_dst + ii * dstStride + j), _mm256_castsi256_si128(accumA));
                    _mm_storel_epi64((__m128i *) (_dst + ii * dstStride + j + 8), _mm256_extracti128_si256(accumA, 1));
                }
                else if(j + 8 <= blk_dst.width)
                {
                    _mm_storeu_si128((__m128i *) (_dst + ii * dstStride + j), _mm256_castsi256_si128(accumA));
                }
                else
                {
                    _mm_storel_epi64((__m128i *) (_dst + ii * dstStride + j), _mm256_castsi256_si128( accumA ) );
                }
            }
        }

        _src += srcStride * STEP_Y;
        _dst += dstStride * STEP_Y;
    }
}

static void
simdFilter7x7Blk_avx2(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr, OVSample *const dst, OVSample *const src, const int dstStride, const int srcStride,
                         Area blk_dst, const int16_t *filter_set, const int16_t *clip_set,
                         const int ctu_height, int virbnd_pos)
{
    if (blk_dst.width & 15){
        simdFilter7x7Blk_sse(class_idx_arr, transpose_idx_arr, dst, src, dstStride, srcStride, blk_dst, filter_set, clip_set, ctu_height, virbnd_pos);
        return;
    }
    const int SHIFT = NUM_BITS - 1;
    const int ROUND = 1 << (SHIFT - 1);

    const size_t STEP_X = 16;
    const size_t STEP_Y = 4;

    const int clpMin = 0;
    const int clpMax = (1<<10) - 1;

    OVSample * _src = src;
    OVSample * _dst = dst;

    int transpose_idx = 0;
    int class_idx = 0;

    const __m256i mmOffset = _mm256_set1_epi32(ROUND);
    const __m256i mmMin = _mm256_set1_epi16( clpMin );
    const __m256i mmMax = _mm256_set1_epi16( clpMax );

    for (size_t i = 0; i < blk_dst.height; i += STEP_Y)
    {
        for (size_t j = 0; j < blk_dst.width; j += STEP_X)
        {
            __m256i params[2][2][6];

            for (int k = 0; k < 2; ++k)
            {
                transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];
                class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];

                const __m128i rawCoeffLo0 = _mm_loadu_si128((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawCoeffHi0 = _mm_loadl_epi64((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));
                const __m128i rawClipLo0  = _mm_loadu_si128((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawClipHi0  = _mm_loadl_epi64((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));

                transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k + 2];
                class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k + 2];

                const __m128i rawCoeffLo1 = _mm_loadu_si128((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawCoeffHi1 = _mm_loadl_epi64((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));
                const __m128i rawClipLo1  = _mm_loadu_si128((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawClipHi1  = _mm_loadl_epi64((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));

                const __m256i rawCoeffLo = _mm256_inserti128_si256( _mm256_castsi128_si256( rawCoeffLo0 ), rawCoeffLo1, 1 );
                const __m256i rawCoeffHi = _mm256_inserti128_si256( _mm256_castsi128_si256( rawCoeffHi0 ), rawCoeffHi1, 1 );

                const __m256i rawClipLo = _mm256_inserti128_si256( _mm256_castsi128_si256( rawClipLo0 ), rawClipLo1, 1 );
                const __m256i rawClipHi = _mm256_inserti128_si256( _mm256_castsi128_si256( rawClipHi0 ), rawClipHi1, 1 );

                params[k][0][0] = _mm256_shuffle_epi32(rawCoeffLo, 0x00);
                params[k][0][1] = _mm256_shuffle_epi32(rawCoeffLo, 0x55);
                params[k][0][2] = _mm256_shuffle_epi32(rawCoeffLo, 0xaa);
                params[k][0][3] = _mm256_shuffle_epi32(rawCoeffLo, 0xff);
                params[k][0][4] = _mm256_shuffle_epi32(rawCoeffHi, 0x00);
                params[k][0][5] = _mm256_shuffle_epi32(rawCoeffHi, 0x55);

                params[k][1][0] = _mm256_shuffle_epi32(rawClipLo, 0x00);
                params[k][1][1] = _mm256_shuffle_epi32(rawClipLo, 0x55);
                params[k][1][2] = _mm256_shuffle_epi32(rawClipLo, 0xaa);
                params[k][1][3] = _mm256_shuffle_epi32(rawClipLo, 0xff);
                params[k][1][4] = _mm256_shuffle_epi32(rawClipHi, 0x00);
                params[k][1][5] = _mm256_shuffle_epi32(rawClipHi, 0x55);
            }

            for (size_t ii = 0; ii < STEP_Y; ii++)
            {
                const uint16_t *pImg0, *pImg1, *pImg2, *pImg3, *pImg4, *pImg5, *pImg6;

                pImg0 = (uint16_t *)_src + j + ii * srcStride;
                pImg1 = pImg0 + srcStride;
                pImg2 = pImg0 - srcStride;
                pImg3 = pImg1 + srcStride;
                pImg4 = pImg2 - srcStride;
                pImg5 = pImg3 + srcStride;
                pImg6 = pImg4 - srcStride;

                __m256i cur = _mm256_loadu_si256((const __m256i *) pImg0);

                __m256i accumA = mmOffset;
                __m256i accumB = mmOffset;

                process2coeffs7x7AVX2(0, pImg5 + 0, pImg6 + 0, pImg3 + 1, pImg4 - 1);
                process2coeffs7x7AVX2(1, pImg3 + 0, pImg4 + 0, pImg3 - 1, pImg4 + 1);
                process2coeffs7x7AVX2(2, pImg1 + 2, pImg2 - 2, pImg1 + 1, pImg2 - 1);
                process2coeffs7x7AVX2(3, pImg1 + 0, pImg2 + 0, pImg1 - 1, pImg2 + 1);
                process2coeffs7x7AVX2(4, pImg1 - 2, pImg2 + 2, pImg0 + 3, pImg0 - 3);
                process2coeffs7x7AVX2(5, pImg0 + 2, pImg0 - 2, pImg0 + 1, pImg0 - 1);

                accumA = _mm256_srai_epi32(accumA, SHIFT);
                accumB = _mm256_srai_epi32(accumB, SHIFT);

                accumA = _mm256_packs_epi32(accumA, accumB);
                accumA = _mm256_add_epi16(accumA, cur);
                accumA = _mm256_min_epi16(mmMax, _mm256_max_epi16(accumA, mmMin));

                _mm256_storeu_si256((__m256i *) (_dst + ii * dstStride + j), accumA);
            }
        }

        _src += srcStride * STEP_Y;
        _dst += dstStride * STEP_Y;
    }
}

static void
simdFilter7x7BlkVB_avx2(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr, OVSample *const dst, OVSample *const src, const int dstStride, const int srcStride,
                         Area blk_dst, const int16_t *filter_set, const int16_t *clip_set,
                         const int ctu_height, int virbnd_pos)
{
    if (blk_dst.width & 15){
        simdFilter7x7BlkVB_sse(class_idx_arr, transpose_idx_arr, dst, src, dstStride, srcStride, blk_dst, filter_set, clip_set, ctu_height, virbnd_pos);
        return;
    }
    const int SHIFT = NUM_BITS - 1;
    const int ROUND = 1 << (SHIFT - 1);

    const size_t STEP_X = 16;
    const size_t STEP_Y = 4;

    const int clpMin = 0;
    const int clpMax = (1<<10) - 1;

    OVSample * _src = src;
    OVSample * _dst = dst;

    int transpose_idx = 0;
    int class_idx = 0;

    const __m256i mmOffset = _mm256_set1_epi32(ROUND);
    const __m256i mmMin = _mm256_set1_epi16( clpMin );
    const __m256i mmMax = _mm256_set1_epi16( clpMax );

    const __m256i mmOffsetborder = _mm256_set1_epi32(1 << ((SHIFT + 3) - 1));
    const int SHIFTborder = SHIFT+3;

    for (size_t i = 0; i < blk_dst.height; i += STEP_Y)
    {
        for (size_t j = 0; j < blk_dst.width; j += STEP_X)
        {
            __m256i params[2][2][6];

            for (int k = 0; k < 2; ++k)
            {
                transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];
                class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k];

                const __m128i rawCoeffLo0 = _mm_loadu_si128((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawCoeffHi0 = _mm_loadl_epi64((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));
                const __m128i rawClipLo0  = _mm_loadu_si128((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawClipHi0  = _mm_loadl_epi64((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));

                transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k + 2];
                class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2) + k + 2];

                const __m128i rawCoeffLo1 = _mm_loadu_si128((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawCoeffHi1 = _mm_loadl_epi64((const __m128i *) (filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));
                const __m128i rawClipLo1  = _mm_loadu_si128((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF));
                const __m128i rawClipHi1  = _mm_loadl_epi64((const __m128i *) (clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF + 8));

                const __m256i rawCoeffLo = _mm256_inserti128_si256( _mm256_castsi128_si256( rawCoeffLo0 ), rawCoeffLo1, 1 );
                const __m256i rawCoeffHi = _mm256_inserti128_si256( _mm256_castsi128_si256( rawCoeffHi0 ), rawCoeffHi1, 1 );

                const __m256i rawClipLo = _mm256_inserti128_si256( _mm256_castsi128_si256( rawClipLo0 ), rawClipLo1, 1 );
                const __m256i rawClipHi = _mm256_inserti128_si256( _mm256_castsi128_si256( rawClipHi0 ), rawClipHi1, 1 );

                params[k][0][0] = _mm256_shuffle_epi32(rawCoeffLo, 0x00);
                params[k][0][1] = _mm256_shuffle_epi32(rawCoeffLo, 0x55);
                params[k][0][2] = _mm256_shuffle_epi32(rawCoeffLo, 0xaa);
                params[k][0][3] = _mm256_shuffle_epi32(rawCoeffLo, 0xff);
                params[k][0][4] = _mm256_shuffle_epi32(rawCoeffHi, 0x00);
                params[k][0][5] = _mm256_shuffle_epi32(rawCoeffHi, 0x55);
                params[k][1][0] = _mm256_shuffle_epi32(rawClipLo, 0x00);
                params[k][1][1] = _mm256_shuffle_epi32(rawClipLo, 0x55);
                params[k][1][2] = _mm256_shuffle_epi32(rawClipLo, 0xaa);
                params[k][1][3] = _mm256_shuffle_epi32(rawClipLo, 0xff);
                params[k][1][4] = _mm256_shuffle_epi32(rawClipHi, 0x00);
                params[k][1][5] = _mm256_shuffle_epi32(rawClipHi, 0x55);
            }

            for (size_t ii = 0; ii < STEP_Y; ii++)
            {
                const uint16_t *pImg0, *pImg1, *pImg2, *pImg3, *pImg4, *pImg5, *pImg6;

                pImg0 = (uint16_t *)_src + j + ii * srcStride;
                pImg1 = pImg0 + srcStride;
                pImg2 = pImg0 - srcStride;
                pImg3 = pImg1 + srcStride;
                pImg4 = pImg2 - srcStride;
                pImg5 = pImg3 + srcStride;
                pImg6 = pImg4 - srcStride;

                const int yVb = (blk_dst.y + i + ii) & (ctu_height - 1);
                if (yVb < virbnd_pos && (yVb >= virbnd_pos - 4))   // above
                {
                    pImg1 = (yVb == virbnd_pos - 1) ? pImg0 : pImg1;
                    pImg3 = (yVb >= virbnd_pos - 2) ? pImg1 : pImg3;
                    pImg5 = (yVb >= virbnd_pos - 3) ? pImg3 : pImg5;

                    pImg2 = (yVb == virbnd_pos - 1) ? pImg0 : pImg2;
                    pImg4 = (yVb >= virbnd_pos - 2) ? pImg2 : pImg4;
                    pImg6 = (yVb >= virbnd_pos - 3) ? pImg4 : pImg6;
                }
                else if (yVb >= virbnd_pos && (yVb <= virbnd_pos + 3))   // bottom
                {
                    pImg2 = (yVb == virbnd_pos) ? pImg0 : pImg2;
                    pImg4 = (yVb <= virbnd_pos + 1) ? pImg2 : pImg4;
                    pImg6 = (yVb <= virbnd_pos + 2) ? pImg4 : pImg6;

                    pImg1 = (yVb == virbnd_pos) ? pImg0 : pImg1;
                    pImg3 = (yVb <= virbnd_pos + 1) ? pImg1 : pImg3;
                    pImg5 = (yVb <= virbnd_pos + 2) ? pImg3 : pImg5;
                }

                __m256i cur = _mm256_loadu_si256((const __m256i *) pImg0);

                uint8_t isNearVBabove = yVb < virbnd_pos && (yVb >= virbnd_pos - 1);
                uint8_t isNearVBbelow = yVb >= virbnd_pos && (yVb <= virbnd_pos);

                __m256i accumA, accumB;
                if (!(isNearVBabove || isNearVBbelow))
                {
                    accumA = mmOffset;
                    accumB = mmOffset;
                }
                else
                {
                    //Rounding offset fix
                    accumA = mmOffsetborder;
                    accumB = mmOffsetborder;
                }

                process2coeffs7x7AVX2(0, pImg5 + 0, pImg6 + 0, pImg3 + 1, pImg4 - 1);
                process2coeffs7x7AVX2(1, pImg3 + 0, pImg4 + 0, pImg3 - 1, pImg4 + 1);
                process2coeffs7x7AVX2(2, pImg1 + 2, pImg2 - 2, pImg1 + 1, pImg2 - 1);
                process2coeffs7x7AVX2(3, pImg1 + 0, pImg2 + 0, pImg1 - 1, pImg2 + 1);
                process2coeffs7x7AVX2(4, pImg1 - 2, pImg2 + 2, pImg0 + 3, pImg0 - 3);
                process2coeffs7x7AVX2(5, pImg0 + 2, pImg0 - 2, pImg0 + 1, pImg0 - 1);

                if (!(isNearVBabove || isNearVBbelow))
                {
                    accumA = _mm256_srai_epi32(accumA, SHIFT);
                    accumB = _mm256_srai_epi32(accumB, SHIFT);
                }
                else
                {
                    //Rounding offset fix
                    accumA = _mm256_srai_epi32(accumA, SHIFTborder);
                    accumB = _mm256_srai_epi32(accumB, SHIFTborder);
                }
                accumA = _mm256_packs_epi32(accumA, accumB);
                accumA = _mm256_add_epi16(accumA, cur);
                accumA = _mm256_min_epi16(mmMax, _mm256_max_epi16(accumA, mmMin));

                // if (j>=x0 && i+ii>=y0 && j<=w_max && i+ii<=h_max)
                _mm256_storeu_si256((__m256i *) (_dst + ii * dstStride + j), accumA);
                // _mm_storeu_si128((__m128i *) (_dst + ii * dstStride + j), _mm256_extracti128_si256(accumA,0));
            }
        }

        _src += srcStride * STEP_Y;
        _dst += dstStride * STEP_Y;
    }
}

void rcn_init_alf_functions_avx2(struct RCNFunctions *rcn_func){
    rcn_func->alf.classif=&simdDeriveClassificationBlk_avx2;
    rcn_func->alf.luma[0]=&simdFilter7x7Blk_avx2;
    rcn_func->alf.luma[1]=&simdFilter7x7BlkVB_avx2;
    rcn_func->alf.chroma[0]=&simdFilter5x5Blk_avx2;
    rcn_func->alf.chroma[1]=&simdFilter5x5BlkVB_avx2;
}
