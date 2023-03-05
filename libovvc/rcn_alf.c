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

#include <stdint.h>
#include <stdlib.h>

#include "ovutils.h"
#include "ovmem.h"
#include "nvcl_structures.h"

#include "ctudec.h"
#include "rcn_alf.h"
#include "rcn_structures.h"

#include "bitdepth.h"

enum Direction
{
  HOR,
  VER,
  DIAG0,
  DIAG1,
  NUM_DIRECTIONS
};

struct ALFilterIdx
{
    uint8_t class_idx;
    uint8_t tr_idx;
};

static const int8_t fixed_filter_coeff[ALF_FIXED_FILTER_NUM][MAX_NUM_ALF_LUMA_COEFF] =
{
    { 0,   0,   2,  -3,   1,  -4,   1,   7,  -1,   1,  -1,   5, 0 },
    { 0,   0,   0,   0,   0,  -1,   0,   1,   0,   0,  -1,   2, 0 },
    { 0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0, 0 },
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  -1,   1, 0 },
    { 2,   2,  -7,  -3,   0,  -5,  13,  22,  12,  -3,  -3,  17,  0 },
    { -1,   0,   6,  -8,   1,  -5,   1,  23,   0,   2,  -5,  10,  0 },
    { 0,   0,  -1,  -1,   0,  -1,   2,   1,   0,   0,  -1,   4, 0 },
    { 0,   0,   3, -11,   1,   0,  -1,  35,   5,   2,  -9,   9,  0 },
    { 0,   0,   8,  -8,  -2,  -7,   4,   4,   2,   1,  -1,  25,  0 },
    { 0,   0,   1,  -1,   0,  -3,   1,   3,  -1,   1,  -1,   3, 0 },
    { 0,   0,   3,  -3,   0,  -6,   5,  -1,   2,   1,  -4,  21,  0 },
    { -7,   1,   5,   4,  -3,   5,  11,  13,  12,  -8,  11,  12,  0 },
    { -5,  -3,   6,  -2,  -3,   8,  14,  15,   2,  -7,  11,  16,  0 },
    { 2,  -1,  -6,  -5,  -2,  -2,  20,  14,  -4,   0,  -3,  25,  0 },
    { 3,   1,  -8,  -4,   0,  -8,  22,   5,  -3,   2, -10,  29,  0 },
    { 2,   1,  -7,  -1,   2, -11,  23,  -5,   0,   2, -10,  29,  0 },
    { -6,  -3,   8,   9,  -4,   8,   9,   7,  14,  -2,   8,   9,  0 },
    { 2,   1,  -4,  -7,   0,  -8,  17,  22,   1,  -1,  -4,  23,  0 },
    { 3,   0,  -5,  -7,   0,  -7,  15,  18,  -5,   0,  -5,  27,  0 },
    { 2,   0,   0,  -7,   1, -10,  13,  13,  -4,   2,  -7,  24,  0 },
    { 3,   3, -13,   4,  -2,  -5,   9,  21,  25,  -2,  -3,  12,  0 },
    { -5,  -2,   7,  -3,  -7,   9,   8,   9,  16,  -2,  15,  12,  0 },
    { 0,  -1,   0,  -7,  -5,   4,  11,  11,   8,  -6,  12,  21,  0 },
    { 3,  -2,  -3,  -8,  -4,  -1,  16,  15,  -2,  -3,   3,  26,  0 },
    { 2,   1,  -5,  -4,  -1,  -8,  16,   4,  -2,   1,  -7,  33,  0 },
    { 2,   1,  -4,  -2,   1, -10,  17,  -2,   0,   2, -11,  33,  0 },
    { 1,  -2,   7, -15, -16,  10,   8,   8,  20,  11,  14,  11,  0 },
    { 2,   2,   3, -13, -13,   4,   8,  12,   2,  -3,  16,  24,  0 },
    { 1,   4,   0,  -7,  -8,  -4,   9,   9,  -2,  -2,   8,  29,  0 },
    { 1,   1,   2,  -4,  -1,  -6,   6,   3,  -1,  -1,  -3,  30,  0 },
    { -7,   3,   2,  10,  -2,   3,   7,  11,  19,  -7,   8,  10, 0 },
    { 0,  -2,  -5,  -3,  -2,   4,  20,  15,  -1,  -3,  -1,  22,  0 },
    { 3,  -1,  -8,  -4,  -1,  -4,  22,   8,  -4,   2,  -8,  28,  0 },
    { 0,   3, -14,   3,   0,   1,  19,  17,   8,  -3,  -7,  20,  0 },
    { 0,   2,  -1,  -8,   3,  -6,   5,  21,   1,   1,  -9,  13,  0 },
    { -4,  -2,   8,  20,  -2,   2,   3,   5,  21,   4,   6,   1, 0 },
    { 2,  -2,  -3,  -9,  -4,   2,  14,  16,   3,  -6,   8,  24,  0 },
    { 2,   1,   5, -16,  -7,   2,   3,  11,  15,  -3,  11,  22,  0 },
    { 1,   2,   3, -11,  -2,  -5,   4,   8,   9,  -3,  -2,  26,  0 },
    { 0,  -1,  10,  -9,  -1,  -8,   2,   3,   4,   0,   0,  29,  0 },
    { 1,   2,   0,  -5,   1,  -9,   9,   3,   0,   1,  -7,  20,  0 },
    { -2,   8,  -6,  -4,   3,  -9,  -8,  45,  14,   2, -13,   7, 0 },
    { 1,  -1,  16, -19,  -8,  -4,  -3,   2,  19,   0,   4,  30,  0 },
    { 1,   1,  -3,   0,   2, -11,  15,  -5,   1,   2,  -9,  24,  0 },
    { 0,   1,  -2,   0,   1,  -4,   4,   0,   0,   1,  -4,   7,  0 },
    { 0,   1,   2,  -5,   1,  -6,   4,  10,  -2,   1,  -4,  10,  0 },
    { 3,   0,  -3,  -6,  -2,  -6,  14,   8,  -1,  -1,  -3,  31,  0 },
    { 0,   1,   0,  -2,   1,  -6,   5,   1,   0,   1,  -5,  13,  0 },
    { 3,   1,   9, -19, -21,   9,   7,   6,  13,   5,  15,  21,  0 },
    { 2,   4,   3, -12, -13,   1,   7,   8,   3,   0,  12,  26,  0 },
    { 3,   1,  -8,  -2,   0,  -6,  18,   2,  -2,   3, -10,  23,  0 },
    { 1,   1,  -4,  -1,   1,  -5,   8,   1,  -1,   2,  -5,  10,  0 },
    { 0,   1,  -1,   0,   0,  -2,   2,   0,   0,   1,  -2,   3,  0 },
    { 1,   1,  -2,  -7,   1,  -7,  14,  18,   0,   0,  -7,  21,  0 },
    { 0,   1,   0,  -2,   0,  -7,   8,   1,  -2,   0,  -3,  24,  0 },
    { 0,   1,   1,  -2,   2, -10,  10,   0,  -2,   1,  -7,  23,  0 },
    { 0,   2,   2, -11,   2,  -4,  -3,  39,   7,   1, -10,   9,  0 },
    { 1,   0,  13, -16,  -5,  -6,  -1,   8,   6,   0,   6,  29,  0 },
    { 1,   3,   1,  -6,  -4,  -7,   9,   6,  -3,  -2,   3,  33,  0 },
    { 4,   0, -17,  -1,  -1,   5,  26,   8,  -2,   3, -15,  30,  0 },
    { 0,   1,  -2,   0,   2,  -8,  12,  -6,   1,   1,  -6,  16,  0 },
    { 0,   0,   0,  -1,   1,  -4,   4,   0,   0,   0,  -3,  11,  0 },
    { 0,   1,   2,  -8,   2,  -6,   5,  15,   0,   2,  -7,   9,  0 },
    { 1,  -1,  12, -15,  -7,  -2,   3,   6,   6,  -1,   7,  30,  0 },
};

static const uint8_t class_to_filter_mapping[NUM_FIXED_FILTER_SETS][MAX_NUM_ALF_CLASSES] =
{
    {  8,   2,   2,   2,   3,   4,  53,   9,   9,  52,   4,   4,   5,   9,   2,   8,  10,   9,   1,   3,  39,  39,  10,   9,  52 },
    { 11,  12,  13,  14,  15,  30,  11,  17,  18,  19,  16,  20,  20,   4,  53,  21,  22,  23,  14,  25,  26,  26,  27,  28,  10 },
    { 16,  12,  31,  32,  14,  16,  30,  33,  53,  34,  35,  16,  20,   4,   7,  16,  21,  36,  18,  19,  21,  26,  37,  38,  39 },
    { 35,  11,  13,  14,  43,  35,  16,   4,  34,  62,  35,  35,  30,  56,   7,  35,  21,  38,  24,  40,  16,  21,  48,  57,  39 },
    { 11,  31,  32,  43,  44,  16,   4,  17,  34,  45,  30,  20,  20,   7,   5,  21,  22,  46,  40,  47,  26,  48,  63,  58,  10 },
    { 12,  13,  50,  51,  52,  11,  17,  53,  45,   9,  30,   4,  53,  19,   0,  22,  23,  25,  43,  44,  37,  27,  28,  10,  55 },
    { 30,  33,  62,  51,  44,  20,  41,  56,  34,  45,  20,  41,  41,  56,   5,  30,  56,  38,  40,  47,  11,  37,  42,  57,   8 },
    { 35,  11,  23,  32,  14,  35,  20,   4,  17,  18,  21,  20,  20,  20,   4,  16,  21,  36,  46,  25,  41,  26,  48,  49,  58 },
    { 12,  31,  59,  59,   3,  33,  33,  59,  59,  52,   4,  33,  17,  59,  55,  22,  36,  59,  59,  60,  22,  36,  59,  25,  55 },
    { 31,  25,  15,  60,  60,  22,  17,  19,  55,  55,  20,  20,  53,  19,  55,  22,  46,  25,  43,  60,  37,  28,  10,  55,  52 },
    { 12,  31,  32,  50,  51,  11,  33,  53,  19,  45,  16,   4,   4,  53,   5,  22,  36,  18,  25,  43,  26,  27,  27,  28,  10 },
    {  5,   2,  44,  52,   3,   4,  53,  45,   9,   3,   4,  56,   5,   0,   2,   5,  10,  47,  52,   3,  63,  39,  10,   9,  52 },
    { 12,  34,  44,  44,   3,  56,  56,  62,  45,   9,  56,  56,   7,   5,   0,  22,  38,  40,  47,  52,  48,  57,  39,  10,   9 },
    { 35,  11,  23,  14,  51,  35,  20,  41,  56,  62,  16,  20,  41,  56,   7,  16,  21,  38,  24,  40,  26,  26,  42,  57,  39 },
    { 33,  34,  51,  51,  52,  41,  41,  34,  62,   0,  41,  41,  56,   7,   5,  56,  38,  38,  40,  44,  37,  42,  57,  39,  10 },
    { 16,  31,  32,  15,  60,  30,   4,  17,  19,  25,  22,  20,   4,  53,  19,  21,  22,  46,  25,  55,  26,  48,  63,  58,  55 },
};

static const uint8_t shuffle_lut[4][13] =
{
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12},
    {  9,  4, 10,  8,  1,  5, 11,  7,  3,  0,  2,  6, 12},
    {  0,  3,  2,  1,  8,  7,  6,  5,  4,  9, 10, 11, 12},
    {  9,  8, 10,  4,  3,  7, 11,  5,  1,  0,  2,  6, 12}
};

static const int16_t alf_clip_lut[4] =
{
    1 << BITDEPTH,
    1 << (BITDEPTH - 8 + 7 - 2),
    1 << (BITDEPTH - 8 + 7 - 4),
    1 << (BITDEPTH - 8 + 7 - 6)
};

static inline int
alf_clip(const int clip, const int32_t ref, const int32_t val0, const int32_t val1)
{
    int clip1 = ov_clip(val0 - ref, -clip, clip);
    int clip2 = ov_clip(val1 - ref, -clip, clip);
    return clip1 + clip2;
}

static void
rcn_alf_init_fixed_filter_sets(RCNALF* alf)
{
    for (int i = 0; i < NUM_FIXED_FILTER_SETS; i++) {
        for (int j = 0; j < MAX_NUM_ALF_CLASSES; j++) {
            for (int t = 0; t < ALF_CTB_MAX_NUM_TRANSPOSE; t++) {
                int tmp_offset = t * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + j * MAX_NUM_ALF_LUMA_COEFF;
                for (int k = 0; k < MAX_NUM_ALF_LUMA_COEFF - 1; k++) {
                    alf->coeff_l[i][tmp_offset + k] = fixed_filter_coeff[class_to_filter_mapping[i][j]][shuffle_lut[t][k]];
                    alf->clip_l [i][tmp_offset + k] = 1 << BITDEPTH;
                }
                alf->coeff_l[i][tmp_offset + MAX_NUM_ALF_LUMA_COEFF - 1] = (1 << (NUM_BITS - 1));
                alf->clip_l [i][tmp_offset + MAX_NUM_ALF_LUMA_COEFF - 1] = 1 << BITDEPTH;
            }
        }
    }
}

static void
alf_init_filter_l(const struct OVALFData* alf_data, int16_t *dst_coeff, int16_t *dst_clip)
{
    int16_t tmp_coeff[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];
    int16_t tmp_clip[MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF];

    int nb_coeff_min1 = 13 - 1;
    if (!alf_data) return;

    int nb_filters = alf_data->alf_luma_num_filters_signalled_minus1 + 1;
    const int8_t* coeff = alf_data->alf_luma_coeff;
    const uint8_t* clip = alf_data->alf_luma_clip_idx;

    for (int class_idx = 0; class_idx < MAX_NUM_ALF_CLASSES; class_idx++) {
        int filter_idx = alf_data->alf_luma_coeff_delta_idx[class_idx];
        int filter_offset = filter_idx * MAX_NUM_ALF_LUMA_COEFF;
        int class_offset  = class_idx  * MAX_NUM_ALF_LUMA_COEFF;
        for (int coeff_idx = 0; coeff_idx < nb_coeff_min1; ++coeff_idx) {
            int clip_idx = alf_data->alf_luma_clip_flag ? clip[filter_offset + coeff_idx] : 0;
            tmp_coeff[class_offset + coeff_idx] = coeff[filter_offset + coeff_idx];
            tmp_clip[class_offset + coeff_idx] = alf_clip_lut[clip_idx];
        }
        tmp_coeff[class_offset + nb_coeff_min1] = 1 << (NUM_BITS - 1);
        tmp_clip[class_offset + nb_coeff_min1] = 1 << BITDEPTH;
    }

    for (int j = 0; j < MAX_NUM_ALF_CLASSES; j++) {
        int class_offset = j * MAX_NUM_ALF_LUMA_COEFF;
        for (int k = 0; k < MAX_NUM_ALF_LUMA_COEFF; k++) {
            for (int t = 0; t < ALF_CTB_MAX_NUM_TRANSPOSE; t++) {
                int tr_offset = t * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF;
                dst_coeff[tr_offset + class_offset + k] = tmp_coeff[class_offset + shuffle_lut[t][k]];
                dst_clip[tr_offset + class_offset + k] = tmp_clip[class_offset + shuffle_lut[t][k]];
            }
        }
    }
}

static void
alf_init_filter_c(RCNALF* alf, const struct OVALFData* alf_data)
{
    int nb_coeffs_min1 = 7 - 1;
    const int nb_alternatives = alf_data->alf_chroma_num_alt_filters_minus1 + 1;

    for (int alt_idx = 0; alt_idx < nb_alternatives; ++alt_idx) {
        const int8_t *coeff = alf_data->alf_chroma_coeff[alt_idx];
        const uint8_t *clip  = alf_data->alf_chroma_clip_idx[alt_idx];
        for (int coeff_idx = 0; coeff_idx < nb_coeffs_min1; ++coeff_idx) {
            int clip_idx = alf_data->alf_chroma_clip_flag ? clip[coeff_idx] : 0;
            alf->coeff_c[alt_idx][coeff_idx] = coeff[coeff_idx];
            alf->clip_c [alt_idx][coeff_idx] = alf_clip_lut[clip_idx];
        }
        alf->coeff_c[alt_idx][nb_coeffs_min1] = 1 << (NUM_BITS - 1);
        alf->clip_c [alt_idx][nb_coeffs_min1] = 1 << BITDEPTH;
    }
}

static void
ccalf_init_filter_cb(RCNALF* alf, const struct OVALFData* alf_data)
{
    int nb_coeffs_min1 = 8 - 1;
    const int nb_alternatives = alf_data->alf_cc_cb_filters_signalled_minus1 + 1;

    for (int alt_idx = 0; alt_idx < nb_alternatives; ++alt_idx) {
        const int8_t *coeff = alf_data->alf_cc_mapped_coeff[0][alt_idx];
        const uint8_t *clip  = alf_data->alf_chroma_clip_idx[alt_idx];
        for (int coeff_idx = 0; coeff_idx < nb_coeffs_min1; ++coeff_idx) {
            int clip_idx = alf_data->alf_chroma_clip_flag ? clip[coeff_idx] : 0;
            //alf->coeff_c[alt_idx][coeff_idx] = coeff[coeff_idx];
            //alf->clip_c [alt_idx][coeff_idx] = alf_clip_lut[clip_idx];

            alf->ccalf_coef_cb[alt_idx][coeff_idx] = coeff[coeff_idx];
            alf->ccalf_clip_cb[alt_idx][coeff_idx] = alf_clip_lut[clip_idx];
        }
        alf->ccalf_coef_cb[alt_idx][nb_coeffs_min1] = 1 << (NUM_BITS - 1);
        alf->ccalf_clip_cb[alt_idx][nb_coeffs_min1] = 1 << BITDEPTH;
    }
}

static void
ccalf_init_filter_cr(RCNALF* alf, const struct OVALFData* alf_data)
{
    int nb_coeffs_min1 = 8 - 1;
    const int nb_alternatives = alf_data->alf_cc_cr_filters_signalled_minus1 + 1;

    for (int alt_idx = 0; alt_idx < nb_alternatives; ++alt_idx) {
        const int8_t *coeff = alf_data->alf_cc_mapped_coeff[1][alt_idx];
        const uint8_t *clip  = alf_data->alf_chroma_clip_idx[alt_idx];
        for (int coeff_idx = 0; coeff_idx < nb_coeffs_min1; ++coeff_idx) {
            int clip_idx = alf_data->alf_chroma_clip_flag ? clip[coeff_idx] : 0;
            //alf->coeff_c[alt_idx][coeff_idx] = coeff[coeff_idx];
            //alf->clip_c [alt_idx][coeff_idx] = alf_clip_lut[clip_idx];

            alf->ccalf_coef_cr[alt_idx][coeff_idx] = coeff[coeff_idx];
            alf->ccalf_clip_cr[alt_idx][coeff_idx] = alf_clip_lut[clip_idx];
        }
        alf->ccalf_coef_cr[alt_idx][nb_coeffs_min1] = 1 << (NUM_BITS - 1);
        alf->ccalf_clip_cr[alt_idx][nb_coeffs_min1] = 1 << BITDEPTH;
    }
}

static void
rcn_alf_reconstruct_coeff_APS(RCNALF* alf, OVCTUDec *const ctudec, const OVPS *const prms)
{
    struct ToolsInfo *tools = &ctudec->tools;
    uint8_t luma_flag   = tools->alf_luma_enabled_flag;
    uint8_t chroma_flag = tools->alf_cb_enabled_flag || tools->alf_cr_enabled_flag;

    if (luma_flag) {
        rcn_alf_init_fixed_filter_sets(alf);
        for (int i = 0; i < ctudec->tools.num_alf_aps_ids_luma; i++) {
            const struct OVALFData* alf_data = ctudec->alf_info.aps_alf_data[i];

            alf_init_filter_l(alf_data, alf->coeff_l[NUM_FIXED_FILTER_SETS + i],
                              alf->clip_l[NUM_FIXED_FILTER_SETS + i]);
        }
    }

    if (chroma_flag) {
        const struct OVALFData* alf_data_c = prms->aps_alf_c ? &prms->aps_alf_c->aps_alf_data : NULL;
        if (alf_data_c) {
            alf_init_filter_c(alf, alf_data_c);
            ctudec->alf_info.nb_alt_c = alf_data_c->alf_chroma_num_alt_filters_minus1;
        }
    }

    if (tools->cc_alf_cb_enabled_flag || tools->cc_alf_cr_enabled_flag) {
        const OVALFData *const ccalf_data_cb = prms->aps_cc_alf_cb ? &prms->aps_cc_alf_cb->aps_alf_data : NULL;
        const OVALFData *const ccalf_data_cr = prms->aps_cc_alf_cr ? &prms->aps_cc_alf_cr->aps_alf_data : NULL;
        if (ccalf_data_cb) {
            ctudec->alf_info.nb_ccalf_alt_cb = ccalf_data_cb->alf_cc_cb_filters_signalled_minus1 + 1;
            ccalf_init_filter_cb(alf, ccalf_data_cb);
        }

        if (ccalf_data_cr) {
            ctudec->alf_info.nb_ccalf_alt_cr = ccalf_data_cr->alf_cc_cr_filters_signalled_minus1 + 1;
            ccalf_init_filter_cr(alf, ccalf_data_cr);
        }
    }
}

static struct ALFilterIdx
alf_derive_filter_idx(uint32_t sum_h, uint32_t sum_v, uint32_t sum_d, uint32_t sum_b, uint8_t shift, uint8_t is_vbnd)
{
    static const int th[16] = { 0, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4 };
    static const uint8_t tr_lut[8] = { 0, 1, 0, 2, 2, 3, 1, 3 };
    const uint8_t max_activity = 15;

    uint32_t scale = is_vbnd ? 96 : 64;

    uint8_t activity = ov_clip(((sum_h + sum_v) * scale) >> shift, 0, max_activity);

    uint32_t max_hv, min_hv, max_db, min_db, max_dir, min_dir;
    uint8_t main_dir, secondary_dir, dir_hv, dir_db;

    struct ALFilterIdx fidx;

    fidx.class_idx = th[activity];


    if (sum_v > sum_h) {
        max_hv = sum_v;
        min_hv = sum_h;
        dir_hv = 1;
    } else {
        max_hv = sum_h;
        min_hv = sum_v;
        dir_hv = 3;
    }

    if (sum_d > sum_b) {
        max_db = sum_d;
        min_db = sum_b;
        dir_db = 0;
    } else {
        max_db = sum_b;
        min_db = sum_d;
        dir_db = 2;
    }

    if (max_db * min_hv > max_hv * min_db) {
        max_dir = max_db;
        min_dir = min_db;
        main_dir      = dir_db;
        secondary_dir = dir_hv;
    } else {
        max_dir = max_hv;
        min_dir = min_hv;
        main_dir      = dir_hv;
        secondary_dir = dir_db;
    }

    if (max_dir * 2 > 9 * min_dir) {
        uint8_t is_hv_main = (main_dir & 0x1);
        fidx.class_idx += ((is_hv_main << 1) + 2) * 5;
    } else if (max_dir > 2 * min_dir) {
        uint8_t is_hv_main = (main_dir & 0x1);
        fidx.class_idx += ((is_hv_main << 1) + 1) * 5;
    }

    fidx.tr_idx    = tr_lut[(main_dir << 1) + (secondary_dir >> 1)];

    return fidx;
}

static void
rcn_alf_classif_novbnd(uint8_t *const class_idx_arr, uint8_t *const transpose_idx_arr,
                       OVSample *const src, const int stride, const Area blk,
                       const int shift);
static void
fill(int *tmp_v, int *tmp_h,int *tmp_d, int *tmp_b,
     const OVSample *l0, const OVSample *l1, const OVSample *l2, const OVSample *l3)
{
    int16_t y10 = l1[0] << 1;
    int16_t y21 = l2[1] << 1;
    int16_t y12 = l1[2] << 1;
    int16_t y22 = l2[3] << 1;

    *tmp_v  = abs(y10 - (l0[ 0] + l2[ 0]));
    *tmp_v += abs(y21 - (l1[ 1] + l3[ 1]));
    *tmp_v += abs(y12 - (l0[ 2] + l2[ 2]));
    *tmp_v += abs(y22 - (l1[ 3] + l3[ 3]));

    *tmp_h  = abs(y10 - (l1[ 1] + l1[-1]));
    *tmp_h += abs(y21 - (l2[ 2] + l2[ 0]));
    *tmp_h += abs(y12 - (l1[ 3] + l1[ 1]));
    *tmp_h += abs(y22 - (l2[ 4] + l2[ 2]));

    *tmp_d  = abs(y10 - (l0[-1] + l2[ 1]));
    *tmp_d += abs(y21 - (l1[ 0] + l3[ 2]));
    *tmp_d += abs(y12 - (l0[ 1] + l2[ 3]));
    *tmp_d += abs(y22 - (l1[ 2] + l3[ 4]));

    *tmp_b  = abs(y10 - (l2[-1] + l0[ 1]));
    *tmp_b += abs(y21 - (l3[ 0] + l1[ 2]));
    *tmp_b += abs(y12 - (l2[ 1] + l0[ 3]));
    *tmp_b += abs(y22 - (l3[ 2] + l1[ 4]));
}

static void                                               
rcn_alf_classif_vbnd(uint8_t *const class_idx_arr, uint8_t *const transpose_idx_arr,
                     OVSample *const src, const int stride, const Area blk,
                     const int shift, int virbnd_pos)
{
    int laplacian[NUM_DIRECTIONS][CLASSIFICATION_BLK_SIZE + 5][(32 >> 2)];

    int tmp_v[(32 + 4) >> 1];
    int tmp_h[(32 + 4) >> 1];
    int tmp_d[(32 + 4) >> 1];
    int tmp_b[(32 + 4) >> 1];

    int blk_h = blk.height;
    int blk_w = blk.width;
    int sb_y_0 = (blk.y >> 2);
    int sb_x_0 = (blk.x >> 2);
    int nb_sb_w = blk_w >> 2;
    int nb_sb_h = blk_h >> 2;

    const OVSample *_src = src - 3 * stride - 3;
    int i;
#if 0
    Area blk2 = blk;
    blk2.height -= 8;
    rcn_alf_classif_novbnd(class_idx_arr, transpose_idx_arr,
                           src, stride, blk2,
                           shift);
    for (i = 0; i < ((virbnd_pos - blk.y) >> 1); ++i) {
        _src += stride << 1;
    }
#else
    for (i = 0; i < ((blk_h - 4) >> 1); ++i) {

        const OVSample *l0 = &_src[1 + 0 * stride];
        const OVSample *l1 = &_src[1 + 1 * stride];
        const OVSample *l2 = &_src[1 + 2 * stride];
        const OVSample *l3 = &_src[1 + 3 * stride];

        int j;

        int* lpl_v = laplacian[VER]  [i];
        int* lpl_h = laplacian[HOR]  [i];
        int* lpl_d = laplacian[DIAG0][i];
        int* lpl_b = laplacian[DIAG1][i];

        fill(tmp_v, tmp_h, tmp_d,tmp_b,
             l0, l1, l2, l3);

        for (j = 1; j < nb_sb_w + 1; ++j) {
            int j4 = j << 2;

            fill(tmp_v + j,tmp_h + j,tmp_d + j ,tmp_b +j,
                 l0 + j4, l1 + j4, l2 + j4, l3 + j4);

            lpl_v[j - 1] = tmp_v[j - 1] + tmp_v[j];
            lpl_h[j - 1] = tmp_h[j - 1] + tmp_h[j];
            lpl_d[j - 1] = tmp_d[j - 1] + tmp_d[j];
            lpl_b[j - 1] = tmp_b[j - 1] + tmp_b[j];
        }

        _src += stride << 1;
    }

    for (i = 0; i < ((blk_h - 8) >> 1); i += 2) {
        const int* lpl_v0 = laplacian[VER][i];
        const int* lpl_v1 = laplacian[VER][i + 1];
        const int* lpl_v2 = laplacian[VER][i + 2];
        const int* lpl_v3 = laplacian[VER][i + 3];

        const int* lpl_h0 = laplacian[HOR][i];
        const int* lpl_h1 = laplacian[HOR][i + 1];
        const int* lpl_h2 = laplacian[HOR][i + 2];
        const int* lpl_h3 = laplacian[HOR][i + 3];

        const int* lpl_d0 = laplacian[DIAG0][i];
        const int* lpl_d1 = laplacian[DIAG0][i + 1];
        const int* lpl_d2 = laplacian[DIAG0][i + 2];
        const int* lpl_d3 = laplacian[DIAG0][i + 3];

        const int* lpl_b0 = laplacian[DIAG1][i];
        const int* lpl_b1 = laplacian[DIAG1][i + 1];
        const int* lpl_b2 = laplacian[DIAG1][i + 2];
        const int* lpl_b3 = laplacian[DIAG1][i + 3];
        int j;

        for (j = 0; j < nb_sb_w; ++j) {
            int sum_v = lpl_v0[j] + lpl_v1[j] + lpl_v2[j] + lpl_v3[j];
            int sum_h = lpl_h0[j] + lpl_h1[j] + lpl_h2[j] + lpl_h3[j];
            int sum_d = lpl_d0[j] + lpl_d1[j] + lpl_d2[j] + lpl_d3[j];
            int sum_b = lpl_b0[j] + lpl_b1[j] + lpl_b2[j] + lpl_b3[j];

            int sb_y = (i >> 1) + sb_y_0;
            int sb_x = j + sb_x_0;

            struct ALFilterIdx fidx = alf_derive_filter_idx(sum_h, sum_v, sum_d, sum_b, shift, 0);

            class_idx_arr    [sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.class_idx;
            transpose_idx_arr[sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.tr_idx;
        }
    }
#endif

    i = blk_h - 4;

    int j;

    const OVSample *l0 = &_src[1 + 0 * stride];
    const OVSample *l1 = &_src[1 + 1 * stride];
    const OVSample *l2 = &_src[1 + 2 * stride];
    const OVSample *l3 = l2;

    for (j = 0; j < nb_sb_w + 1; ++j) {
        int j4 = j << 2;
        fill(tmp_v + j,tmp_h + j,tmp_d + j ,tmp_b +j,
             l0 + j4, l1 + j4, l2 + j4, l3 + j4);
    }

    int* lpl_v = laplacian[VER]  [i >> 1];
    int* lpl_h = laplacian[HOR]  [i >> 1];
    int* lpl_d = laplacian[DIAG0][i >> 1];
    int* lpl_b = laplacian[DIAG1][i >> 1];
    for (j = 0; j < nb_sb_w; ++j) {
        lpl_v[j] = tmp_v[j] + tmp_v[j + 1];
        lpl_h[j] = tmp_h[j] + tmp_h[j + 1];
        lpl_d[j] = tmp_d[j] + tmp_d[j + 1];
        lpl_b[j] = tmp_b[j] + tmp_b[j + 1];
    }
    _src += stride << 1;
    //}
    i = (blk_h - 8) >> 1;

    int sb_y = (i >> 1) + sb_y_0;

    const int* lpl_v0 = laplacian[VER][i];
    const int* lpl_v1 = laplacian[VER][i + 1];
    const int* lpl_v2 = laplacian[VER][i + 2];

    const int* lpl_h0 = laplacian[HOR][i];
    const int* lpl_h1 = laplacian[HOR][i + 1];
    const int* lpl_h2 = laplacian[HOR][i + 2];

    const int* lpl_d0 = laplacian[DIAG0][i];
    const int* lpl_d1 = laplacian[DIAG0][i + 1];
    const int* lpl_d2 = laplacian[DIAG0][i + 2];

    const int* lpl_b0 = laplacian[DIAG1][i];
    const int* lpl_b1 = laplacian[DIAG1][i + 1];
    const int* lpl_b2 = laplacian[DIAG1][i + 2];

    for (j = 0; j < nb_sb_w; ++j) {
        int sum_v = lpl_v0[j] + lpl_v1[j] + lpl_v2[j];
        int sum_h = lpl_h0[j] + lpl_h1[j] + lpl_h2[j];
        int sum_d = lpl_d0[j] + lpl_d1[j] + lpl_d2[j];
        int sum_b = lpl_b0[j] + lpl_b1[j] + lpl_b2[j];

        int sb_x = j + sb_x_0;

        struct ALFilterIdx fidx = alf_derive_filter_idx(sum_h, sum_v, sum_d, sum_b, shift, 1);

        class_idx_arr    [sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.class_idx;
        transpose_idx_arr[sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.tr_idx;
    }

    //for (; i < blk_h; i += 2) {
    i = blk_h - 2;

    l0 = &_src[1 + 0 * stride];
    l1 = &_src[1 + 1 * stride];
    l2 = &_src[1 + 2 * stride];
    l3 = &_src[1 + 3 * stride];

    l0 = l1;

    for (j = 0; j < nb_sb_w + 1; ++j) {
        int j4 = j << 2;
        fill(tmp_v + j,tmp_h + j,tmp_d + j ,tmp_b +j,
             l0 + j4, l1 + j4, l2 + j4, l3 + j4);
    }

    lpl_v = laplacian[VER]  [i >> 1];
    lpl_h = laplacian[HOR]  [i >> 1];
    lpl_d = laplacian[DIAG0][i >> 1];
    lpl_b = laplacian[DIAG1][i >> 1];
    for (j = 0; j < nb_sb_w; ++j) {
        lpl_v[j] = tmp_v[j] + tmp_v[j + 1];
        lpl_h[j] = tmp_h[j] + tmp_h[j + 1];
        lpl_d[j] = tmp_d[j] + tmp_d[j + 1];
        lpl_b[j] = tmp_b[j] + tmp_b[j + 1];
    }
    _src += stride << 1;

    i = blk_h;
    //}

    for (; i < blk_h + 4; i += 2) {
        int j;

        const OVSample *l0 = &_src[1 + 0 * stride];
        const OVSample *l1 = &_src[1 + 1 * stride];
        const OVSample *l2 = &_src[1 + 2 * stride];
        const OVSample *l3 = &_src[1 + 3 * stride];

        for (j = 0; j < nb_sb_w + 1; ++j) {
            int j4 = j << 2;
            fill(tmp_v + j,tmp_h + j,tmp_d + j ,tmp_b +j,
                 l0 + j4, l1 + j4, l2 + j4, l3 + j4);
        }

        int* lpl_v = laplacian[VER]  [i >> 1];
        int* lpl_h = laplacian[HOR]  [i >> 1];
        int* lpl_d = laplacian[DIAG0][i >> 1];
        int* lpl_b = laplacian[DIAG1][i >> 1];
        for (j = 0; j < nb_sb_w; ++j) {
            lpl_v[j] = tmp_v[j] + tmp_v[j + 1];
            lpl_h[j] = tmp_h[j] + tmp_h[j + 1];
            lpl_d[j] = tmp_d[j] + tmp_d[j + 1];
            lpl_b[j] = tmp_b[j] + tmp_b[j + 1];
        }
        _src += stride << 1;
    }

    i = (blk_h - 4) >> 1;

    lpl_v1 = laplacian[VER][i + 1];
    lpl_v2 = laplacian[VER][i + 2];

    lpl_h1 = laplacian[HOR][i + 1];
    lpl_h2 = laplacian[HOR][i + 2];

    lpl_d1 = laplacian[DIAG0][i + 1];
    lpl_d2 = laplacian[DIAG0][i + 2];

    lpl_b1 = laplacian[DIAG1][i + 1];
    lpl_b2 = laplacian[DIAG1][i + 2];

    int *lpl_v3 = laplacian[VER][i + 3];
    int *lpl_h3 = laplacian[HOR][i + 3];
    int *lpl_d3 = laplacian[DIAG0][i + 3];
    int *lpl_b3 = laplacian[DIAG1][i + 3];

    sb_y++;

    for (j = 0; j < nb_sb_w; ++j) {
        int sb_x = j + sb_x_0;

        int sum_v =             lpl_v1[j] + lpl_v2[j] + lpl_v3[j];
        int sum_h =             lpl_h1[j] + lpl_h2[j] + lpl_h3[j];
        int sum_d =             lpl_d1[j] + lpl_d2[j] + lpl_d3[j];
        int sum_b =             lpl_b1[j] + lpl_b2[j] + lpl_b3[j];

        struct ALFilterIdx fidx = alf_derive_filter_idx(sum_h, sum_v, sum_d, sum_b, shift, 1);

        class_idx_arr    [sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.class_idx;
        transpose_idx_arr[sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.tr_idx;
    }
}

static void
rcn_alf_classif_novbnd(uint8_t *const class_idx_arr, uint8_t *const transpose_idx_arr,
                       OVSample *const src, const int stride, const Area blk,
                       const int shift)
{
    int laplacian[NUM_DIRECTIONS][CLASSIFICATION_BLK_SIZE + 5][(32 >> 2)];

    int nb_sb_h = blk.height >> 2;
    int nb_sb_w = blk.width >> 2;
    int sb0_x = blk.x >> 2;
    int sb0_y = blk.y >> 2;

    int tmp_v[(32 + 4) >> 1];
    int tmp_h[(32 + 4) >> 1];
    int tmp_d[(32 + 4) >> 1];
    int tmp_b[(32 + 4) >> 1];

    const OVSample *_src = src - 3 * stride - 3;
    int i;

    for (i = 0; i < nb_sb_h + 1; ++i) {
        const OVSample *l0 = &_src[1 + stride * 0];
        const OVSample *l1 = &_src[1 + stride * 1];
        const OVSample *l2 = &_src[1 + stride * 2];
        const OVSample *l3 = &_src[1 + stride * 3];
        const OVSample *l4 = &_src[1 + stride * 4];
        const OVSample *l5 = &_src[1 + stride * 5];

        int j;

        for (j = 0; j < nb_sb_w + 1; ++j) {
            tmp_v[j]  = abs((l1[0] - l0[0]) + (l1[0] - l2[0]));
            tmp_v[j] += abs((l3[0] - l2[0]) + (l3[0] - l4[0]));
            tmp_v[j] += abs((l1[2] - l0[2]) + (l1[2] - l2[2]));
            tmp_v[j] += abs((l3[2] - l2[2]) + (l3[2] - l4[2]));
            tmp_v[j] += abs((l3[1] - l2[1]) + (l1[1] - l2[1]));
            tmp_v[j] += abs((l5[1] - l4[1]) + (l3[1] - l4[1]));
            tmp_v[j] += abs((l3[3] - l2[3]) + (l1[3] - l2[3]));
            tmp_v[j] += abs((l5[3] - l4[3]) + (l3[3] - l4[3]));

            tmp_h[j]  = abs((l1[0] - l1[1]) + (l1[0] - l1[-1]));
            tmp_h[j] += abs((l2[0] - l2[1]) + (l2[2] - l2[ 1]));
            tmp_h[j] += abs((l1[2] - l1[3]) + (l1[2] - l1[ 1]));
            tmp_h[j] += abs((l2[2] - l2[3]) + (l2[4] - l2[ 3]));
            tmp_h[j] += abs((l3[0] - l3[1]) + (l3[0] - l3[-1]));
            tmp_h[j] += abs((l4[0] - l4[1]) + (l4[2] - l4[ 1]));
            tmp_h[j] += abs((l3[2] - l3[3]) + (l3[2] - l3[ 1]));
            tmp_h[j] += abs((l4[2] - l4[3]) + (l4[4] - l4[ 3]));

            tmp_d[j]  = abs((l1[0] - l0[-1]) + (l1[0] - l2[1]));
            tmp_d[j] += abs((l3[2] - l2[ 1]) + (l1[0] - l2[1]));
            tmp_d[j] += abs((l1[2] - l0[ 1]) + (l1[2] - l2[3]));
            tmp_d[j] += abs((l3[4] - l2[ 3]) + (l1[2] - l2[3]));
            tmp_d[j] += abs((l3[0] - l2[-1]) + (l3[0] - l4[1]));
            tmp_d[j] += abs((l5[2] - l4[ 1]) + (l3[0] - l4[1]));
            tmp_d[j] += abs((l3[2] - l2[ 1]) + (l3[2] - l4[3]));
            tmp_d[j] += abs((l5[4] - l4[ 3]) + (l3[2] - l4[3]));

            tmp_b[j]  = abs((l1[0] - l2[-1]) + (l1[0] - l0[1]));
            tmp_b[j] += abs((l1[2] - l2[ 1]) + (l3[0] - l2[1]));
            tmp_b[j] += abs((l1[2] - l2[ 1]) + (l1[2] - l0[3]));
            tmp_b[j] += abs((l1[4] - l2[ 3]) + (l3[2] - l2[3]));
            tmp_b[j] += abs((l3[0] - l4[-1]) + (l3[0] - l2[1]));
            tmp_b[j] += abs((l3[2] - l4[ 1]) + (l5[0] - l4[1]));
            tmp_b[j] += abs((l3[2] - l4[ 1]) + (l3[2] - l2[3]));
            tmp_b[j] += abs((l3[4] - l4[ 3]) + (l5[2] - l4[3]));

            l0 += 4;
            l1 += 4;
            l2 += 4;
            l3 += 4;
            l4 += 4;
            l5 += 4;
        }

        int* lpl_v = laplacian[VER]  [i];
        int* lpl_h = laplacian[HOR]  [i];
        int* lpl_d = laplacian[DIAG0][i];
        int* lpl_b = laplacian[DIAG1][i];
        for (j = 0; j < nb_sb_w; ++j) {
            lpl_v[j] = tmp_v[j] + tmp_v[j + 1];
            lpl_h[j] = tmp_h[j] + tmp_h[j + 1];
            lpl_d[j] = tmp_d[j] + tmp_d[j + 1];
            lpl_b[j] = tmp_b[j] + tmp_b[j + 1];
        }

        _src += stride << 2;
    }

    for (i = 0; i < nb_sb_h; ++i) {
        const int* lpl_v0 = laplacian[VER][i];
        const int* lpl_v1 = laplacian[VER][i + 1];

        const int* lpl_h0 = laplacian[HOR][i];
        const int* lpl_h1 = laplacian[HOR][i + 1];

        const int* lpl_d0 = laplacian[DIAG0][i];
        const int* lpl_d1 = laplacian[DIAG0][i + 1];

        const int* lpl_b0 = laplacian[DIAG1][i];
        const int* lpl_b1 = laplacian[DIAG1][i + 1];
        int j;

        for (j = 0; j < nb_sb_w; ++j) {
            uint32_t sum_v = lpl_v0[j] + lpl_v1[j];
            uint32_t sum_h = lpl_h0[j] + lpl_h1[j];
            uint32_t sum_d = lpl_d0[j] + lpl_d1[j];
            uint32_t sum_b = lpl_b0[j] + lpl_b1[j];

            int sb_y = i + sb0_y;
            int sb_x = j + sb0_x;

            struct ALFilterIdx fidx = alf_derive_filter_idx(sum_h, sum_v, sum_d, sum_b, shift, 0);

            class_idx_arr    [sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.class_idx;
            transpose_idx_arr[sb_y * CLASSIFICATION_BLK_SIZE + sb_x] = fidx.tr_idx;
        }
    }
}

static void
rcn_alf_derive_classificationBlk(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr,
                                 OVSample *const src, const int stride, const Area blk,
                                 const int shift, const int ctu_s, int virbnd_pos)
{
    if (blk.height + blk.y >= virbnd_pos) {
        rcn_alf_classif_vbnd(class_idx_arr, transpose_idx_arr, src, stride, blk,
                             shift, virbnd_pos);
    } else {
        rcn_alf_classif_novbnd(class_idx_arr, transpose_idx_arr, src, stride, blk,
                               shift);
    }

}


static void
rcn_alf_derive_classification(OVSample *const rcn_img, const int stride,
                              Area blk, int ctu_s, int pic_h,
                              ALFClassifBlkFunc classif_func, uint8_t *class_idx, uint8_t *transpose_idx)
{
    int ctu_h = blk.height;
    int ctu_w = blk.width;

    int virbnd_pos = (ctu_h < ctu_s) ? pic_h : ctu_h - ALF_VB_POS_ABOVE_CTUROW_LUMA;
    int i;

    for (i = 0; i < ctu_h; i += CLASSIFICATION_BLK_SIZE) {
        int blk_h = OVMIN(CLASSIFICATION_BLK_SIZE, ctu_h - i);
        int j;

        for (j = 0; j < ctu_w; j += CLASSIFICATION_BLK_SIZE) {
            int blk_w = OVMIN(CLASSIFICATION_BLK_SIZE, ctu_w - j);
            OVSample* rcn_img_class = rcn_img + i * stride + j;


            Area blk_class = {
                .x = j,
                .y = i,
                .width  = blk_w,
                .height = blk_h
            };

            classif_func(class_idx, transpose_idx, rcn_img_class, stride, blk_class,
                         BITDEPTH + 4, ctu_s,
                         virbnd_pos);
        }
    }
}

static void
cc_alf_filterBlk(OVSample * chroma_dst, OVSample * luma_src, const int chr_stride, const int luma_stride,
                 const Area blk_dst, const int8_t *filt_coeff,
                 const int vbCTUHeight, int vbPos)
{
    const int clsSizeY           = 4;
    const int clsSizeX           = 4;
    //ATTENTION: scaleX et Y fixed to 1 (en 4 2 0)
    const int scaleX             = 1;
    const int scaleY             = 1;
    for (int i = 0; i < blk_dst.height; i += clsSizeY) {
        for (int j = 0; j < blk_dst.width; j += clsSizeX) {
            for (int ii = 0; ii < clsSizeY; ii++) {
                int row       = ii;
                int col       = j;
                OVSample *srcSelf  = chroma_dst + col + row * chr_stride;

                int offset1 = luma_stride;
                int offset2 = -luma_stride;
                int offset3 = 2 * luma_stride;
                row <<= scaleY;
                col <<= scaleX;
                const OVSample *srcCross = luma_src + col + row * luma_stride;

                int pos = ((blk_dst.y + i + ii) << scaleY) & (vbCTUHeight - 1);

                if (pos == (vbPos - 2) || pos == (vbPos + 1)) {
                    offset3 = offset1;
                } else if (pos == (vbPos - 1) || pos == vbPos) {
                    offset1 = 0;
                    offset2 = 0;
                    offset3 = 0;
                }

                for (int jj = 0; jj < clsSizeX; jj++) {
                    const int jj2     = (jj << scaleX);
                    const int offset0 = 0;

                    int sum = 0;
                    const int16_t currSrcCross = srcCross[offset0 + jj2];
                    sum += filt_coeff[0] * (srcCross[offset2 + jj2    ] - currSrcCross);
                    sum += filt_coeff[1] * (srcCross[offset0 + jj2 - 1] - currSrcCross);
                    sum += filt_coeff[2] * (srcCross[offset0 + jj2 + 1] - currSrcCross);
                    sum += filt_coeff[3] * (srcCross[offset1 + jj2 - 1] - currSrcCross);
                    sum += filt_coeff[4] * (srcCross[offset1 + jj2    ] - currSrcCross);
                    sum += filt_coeff[5] * (srcCross[offset1 + jj2 + 1] - currSrcCross);
                    sum += filt_coeff[6] * (srcCross[offset3 + jj2    ] - currSrcCross);

                    const int scale_bits = 7;
                    sum = (sum + ((1 << scale_bits) >> 1)) >> scale_bits;

                    const int offset = 1 << BITDEPTH >> 1;

                    /* FIXME 2 clipping ?*/
                    sum = ov_bdclip(sum + offset);
                    sum += srcSelf[jj] - offset;

                    srcSelf[jj] = ov_bdclip(sum);
                }
            }
        }
        chroma_dst += chr_stride * clsSizeY;
        luma_src += luma_stride * clsSizeY << scaleY;
    }
}

static void
cc_alf_filterBlkVB(OVSample * chroma_dst, OVSample * luma_src, const int chr_stride, const int luma_stride,
                   const Area blk_dst, const int8_t *filt_coeff,
                   const int vbCTUHeight, int vbPos)
{
    const int clsSizeY           = 4;
    const int clsSizeX           = 4;

    const int scaleX             = 1;
    const int scaleY             = 1;

    for (int i = 0; i < blk_dst.height; i += clsSizeY) {
        for (int j = 0; j < blk_dst.width; j += clsSizeX) {
            for (int ii = 0; ii < clsSizeY; ii++) {
                int row       = ii;
                int col       = j;
                OVSample *srcSelf  = chroma_dst + col + row * chr_stride;

                int offset1 = luma_stride;
                int offset2 = -luma_stride;
                int offset3 = 2 * luma_stride;
                row <<= scaleY;
                col <<= scaleX;
                const OVSample *srcCross = luma_src + col + row * luma_stride;

                int pos = ((blk_dst.y + i + ii) << scaleY) & (vbCTUHeight - 1);
                if (!(scaleY == 0 && (pos == vbPos || pos == vbPos + 1))) {
                    if (pos == (vbPos - 2) || pos == (vbPos + 1)) {
                        offset3 = offset1;
                    } else if (pos == (vbPos - 1) || pos == vbPos) {
                        offset1 = 0;
                        offset2 = 0;
                        offset3 = 0;
                    }

                    for (int jj = 0; jj < clsSizeX; jj++) {
                        const int jj2     = (jj << scaleX);
                        const int offset0 = 0;

                        int sum = 0;
                        const int16_t currSrcCross = srcCross[offset0 + jj2];
                        sum += filt_coeff[0] * (srcCross[offset2 + jj2    ] - currSrcCross);
                        sum += filt_coeff[1] * (srcCross[offset0 + jj2 - 1] - currSrcCross);
                        sum += filt_coeff[2] * (srcCross[offset0 + jj2 + 1] - currSrcCross);
                        sum += filt_coeff[3] * (srcCross[offset1 + jj2 - 1] - currSrcCross);
                        sum += filt_coeff[4] * (srcCross[offset1 + jj2    ] - currSrcCross);
                        sum += filt_coeff[5] * (srcCross[offset1 + jj2 + 1] - currSrcCross);
                        sum += filt_coeff[6] * (srcCross[offset3 + jj2    ] - currSrcCross);

                        const int scale_bits = 7;
                        sum = (sum + ((1 << scale_bits) >> 1)) >> scale_bits;

                        const int offset = 1 << BITDEPTH >> 1;

                        /* FIXME 2 clipping ?*/
                        sum = ov_bdclip(sum + offset);
                        sum += srcSelf[jj] - offset;

                        srcSelf[jj] = ov_bdclip(sum);
                    }
                }
            }
        }
        chroma_dst += chr_stride * clsSizeY;
        luma_src += luma_stride * clsSizeY << scaleY;
    }
}

// dst   : buffer Frame output, pointing to the begining of CTU.
// src   : filter buffer pre-ALF (of size CTU)
// blk_dst: location and dimension of destination block in frame
// blk   : location and dimension of destination block in filter buffer
static void
alf_filter_c(OVSample *const dst, const OVSample *const src,
             const int dst_stride, const int src_stride,
             Area blk_dst,
             const int16_t *const filter_set, const int16_t *const clip_set,
             const int ctu_height, int virbnd_pos)
{
    const int shift = NUM_BITS - 1;
    const int offset = 1 << (shift - 1);

    const int blk_h = 4;
    const int blk_w = 4;

    int dst_blk_stride = dst_stride * blk_h;
    int src_blk_stride = src_stride * blk_h;

    OVSample* dst0 = dst ;
    OVSample* dst1 = dst + dst_stride;
    int i;

    const OVSample *ch_src0 = src;
    const OVSample *ch_src1 = ch_src0 + src_stride;
    const OVSample *ch_src2 = ch_src0 - src_stride;
    const OVSample *ch_src3 = ch_src1 + src_stride;
    const OVSample *ch_src4 = ch_src2 - src_stride;


    for (i = 0; i < blk_dst.height; i += blk_h) {
        int j;
        for (j = 0; j < blk_dst.width; j += blk_w) {
            int k;
            for (k = 0; k < blk_h; k++) {
                int l;
                const OVSample *src_0 = ch_src0 + j + k * src_stride;
                const OVSample *src_1 = ch_src1 + j + k * src_stride;
                const OVSample *src_2 = ch_src2 + j + k * src_stride;
                const OVSample *src_3 = ch_src3 + j + k * src_stride;
                const OVSample *src_4 = ch_src4 + j + k * src_stride;


                dst1 = dst0 + j + k * dst_stride;

                for (l = 0; l < blk_w; l++) {
                    int sum = 0;
                    const int16_t curr = src_0[0];
                    sum += filter_set[0] * alf_clip(clip_set[0], curr, src_3[ 0], src_4[ 0]);
                    sum += filter_set[1] * alf_clip(clip_set[1], curr, src_1[ 1], src_2[-1]);
                    sum += filter_set[2] * alf_clip(clip_set[2], curr, src_1[ 0], src_2[ 0]);
                    sum += filter_set[3] * alf_clip(clip_set[3], curr, src_1[-1], src_2[ 1]);
                    sum += filter_set[4] * alf_clip(clip_set[4], curr, src_0[ 2], src_0[-2]);
                    sum += filter_set[5] * alf_clip(clip_set[5], curr, src_0[ 1], src_0[-1]);

                    sum = (sum + offset) >> shift;

                    sum += curr;
                    dst1[l] = ov_bdclip(sum);

                    src_0++;
                    src_1++;
                    src_2++;
                    src_3++;
                    src_4++;

                }
            }
        }

        dst0 += dst_blk_stride;
        dst1 += dst_blk_stride;

        ch_src0 += src_blk_stride;
        ch_src1 += src_blk_stride;
        ch_src2 += src_blk_stride;
        ch_src3 += src_blk_stride;
        ch_src4 += src_blk_stride;

    }
}

static void
alf_filter_cVB(OVSample *const dst, const OVSample *const src,
               const int dst_stride, const int src_stride,
               Area blk_dst,
               const int16_t *const filter_set, const int16_t *const clip_set,
               const int ctu_height, int virbnd_pos)
{
    const int shift = NUM_BITS - 1;
    const int offset = 1 << (shift - 1);

    const int blk_h = 4;
    const int blk_w = 4;

    int dst_blk_stride = dst_stride * blk_h;
    int src_blk_stride = src_stride * blk_h;

    OVSample* dst0 = dst ;
    OVSample* dst1 = dst + dst_stride;
    int i;

    const OVSample *ch_src0 = src;
    const OVSample *ch_src1 = ch_src0 + src_stride;
    const OVSample *ch_src2 = ch_src0 - src_stride;
    const OVSample *ch_src3 = ch_src1 + src_stride;
    const OVSample *ch_src4 = ch_src2 - src_stride;

    for (i = 0; i < blk_dst.height; i += blk_h) {
        int j;
        for (j = 0; j < blk_dst.width; j += blk_w) {
            int k;
            for (k = 0; k < blk_h; k++) {
                int l;
                const OVSample *src_0 = ch_src0 + j + k * src_stride;
                const OVSample *src_1 = ch_src1 + j + k * src_stride;
                const OVSample *src_2 = ch_src2 + j + k * src_stride;
                const OVSample *src_3 = ch_src3 + j + k * src_stride;
                const OVSample *src_4 = ch_src4 + j + k * src_stride;

                dst1 = dst0 + j + k * dst_stride;

                int yVb = (blk_dst.y + i + k) & (ctu_height - 1);
                if (yVb < virbnd_pos && (yVb >= virbnd_pos - 2)) {
                    src_1 = (yVb == virbnd_pos - 1) ? src_0 : src_1;
                    src_3 = (yVb >= virbnd_pos - 2) ? src_1 : src_3;

                    src_2 = (yVb == virbnd_pos - 1) ? src_0 : src_2;
                    src_4 = (yVb >= virbnd_pos - 2) ? src_2 : src_4;
                } else if (yVb >= virbnd_pos && (yVb <= virbnd_pos + 1)) {
                    src_2 = (yVb == virbnd_pos    ) ? src_0 : src_2;
                    src_4 = (yVb <= virbnd_pos + 1) ? src_2 : src_4;

                    src_1 = (yVb == virbnd_pos    ) ? src_0 : src_1;
                    src_3 = (yVb <= virbnd_pos + 1) ? src_1 : src_3;
                }

                uint8_t isNearVBabove = yVb < virbnd_pos && (yVb >= virbnd_pos - 1);
                uint8_t isNearVBbelow = yVb >= virbnd_pos && (yVb <= virbnd_pos);

                for (l = 0; l < blk_w; l++) {
                    int sum = 0;
                    const int16_t curr = src_0[0];
                    sum += filter_set[0] * alf_clip(clip_set[0], curr, src_3[ 0], src_4[ 0]);
                    sum += filter_set[1] * alf_clip(clip_set[1], curr, src_1[ 1], src_2[-1]);
                    sum += filter_set[2] * alf_clip(clip_set[2], curr, src_1[ 0], src_2[ 0]);
                    sum += filter_set[3] * alf_clip(clip_set[3], curr, src_1[-1], src_2[ 1]);
                    sum += filter_set[4] * alf_clip(clip_set[4], curr, src_0[ 2], src_0[-2]);
                    sum += filter_set[5] * alf_clip(clip_set[5], curr, src_0[ 1], src_0[-1]);

                    if (!(isNearVBabove || isNearVBbelow)) {
                        sum = (sum + offset) >> shift;
                    } else {
                        sum = (sum + (1 << ((shift + 3) - 1))) >> (shift + 3);
                    }

                    sum += curr;
                    dst1[l] = ov_bdclip(sum);

                    src_0++;
                    src_1++;
                    src_2++;
                    src_3++;
                    src_4++;
                }
            }
        }

        dst0 += dst_blk_stride;
        dst1 += dst_blk_stride;

        ch_src0 += src_blk_stride;
        ch_src1 += src_blk_stride;
        ch_src2 += src_blk_stride;
        ch_src3 += src_blk_stride;
        ch_src4 += src_blk_stride;
    }
}

static void
alf_filterBlkLuma(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr, OVSample *const dst, OVSample *const src, const int dstStride, const int srcStride,
                  Area blk_dst, const int16_t *filter_set, const int16_t *clip_set,
                  const int ctu_height, int virbnd_pos)
{
    const OVSample *src_l0, *src_l1, *src_l2, *src_l3, *src_l4, *src_l5, *src_l6;

    const int shift = NUM_BITS - 1;
    const int offset = 1 << (shift - 1);

    int transpose_idx = 0;
    int class_idx = 0;
    const int clsSizeY = 4;
    const int clsSizeX = 4;

    int dstStride2 = dstStride * clsSizeY;
    int srcStride2 = srcStride * clsSizeY;

    OVSample * _src = src;
    OVSample * _dst = dst;

    OVSample* pRec0 = dst ;
    OVSample* pRec1 = pRec0 + dstStride;

    for (int i = 0; i < blk_dst.height; i += clsSizeY) {
        for (int j = 0; j < blk_dst.width; j += clsSizeX) {
            transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];
            class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];
            const int16_t *filt_coeff = filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;
            const int16_t *filt_clip = clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;

            for (int ii = 0; ii < clsSizeY; ii++) {
                src_l0 = _src + j + ii * srcStride;
                src_l1 = src_l0 + srcStride;
                src_l2 = src_l0 - srcStride;
                src_l3 = src_l1 + srcStride;
                src_l4 = src_l2 - srcStride;
                src_l5 = src_l3 + srcStride;
                src_l6 = src_l4 - srcStride;

                pRec1 = pRec0 + j + ii * dstStride;

                for (int jj = 0; jj < clsSizeX; jj++) {
                    int sum = 0;
                    const int16_t curr = src_l0[+0];
                    sum += filt_coeff[0] * alf_clip(filt_clip[0], curr, src_l5[+0], src_l6[+0]);
                    sum += filt_coeff[1] * alf_clip(filt_clip[1], curr, src_l3[+1], src_l4[-1]);

                    sum += filt_coeff[2] * alf_clip(filt_clip[2], curr, src_l3[+0], src_l4[+0]);
                    sum += filt_coeff[3] * alf_clip(filt_clip[3], curr, src_l3[-1], src_l4[+1]);

                    sum += filt_coeff[4] * alf_clip(filt_clip[4], curr, src_l1[+2], src_l2[-2]);
                    sum += filt_coeff[5] * alf_clip(filt_clip[5], curr, src_l1[+1], src_l2[-1]);

                    sum += filt_coeff[6] * alf_clip(filt_clip[6], curr, src_l1[+0], src_l2[+0]);
                    sum += filt_coeff[7] * alf_clip(filt_clip[7], curr, src_l1[-1], src_l2[+1]);

                    sum += filt_coeff[8] * alf_clip(filt_clip[8], curr, src_l1[-2], src_l2[+2]);
                    sum += filt_coeff[9] * alf_clip(filt_clip[9], curr, src_l0[+3], src_l0[-3]);

                    sum += filt_coeff[10] * alf_clip(filt_clip[10], curr, src_l0[+2], src_l0[-2]);
                    sum += filt_coeff[11] * alf_clip(filt_clip[11], curr, src_l0[+1], src_l0[-1]);

                    sum = (sum + offset) >> shift;

                    sum += curr;
                    pRec1[jj] = ov_bdclip(sum);

                    src_l0++;
                    src_l1++;
                    src_l2++;
                    src_l3++;
                    src_l4++;
                    src_l5++;
                    src_l6++;
                }
            }
        }

        pRec0 += dstStride2;
        pRec1 += dstStride2;

        _src += srcStride2;
        _dst += dstStride2;
    }
}

static void
alf_filterBlkLumaVB(uint8_t * class_idx_arr, uint8_t * transpose_idx_arr, OVSample *const dst, OVSample *const src, const int dstStride, const int srcStride,
                    Area blk_dst, const int16_t *filter_set, const int16_t *clip_set,
                    const int ctu_height, int virbnd_pos)
{
    const int shift = NUM_BITS - 1;
    const int offset = 1 << (shift - 1);

    int transpose_idx = 0;
    int class_idx = 0;
    const int clsSizeY = 4;
    const int clsSizeX = 4;

    int dstStride2 = dstStride * clsSizeY;
    int srcStride2 = srcStride * clsSizeY;

    OVSample * _src = src;
    OVSample * _dst = dst;

    OVSample* pRec0 = dst ;
    OVSample* pRec1 = pRec0 + dstStride;

    for (int i = 0; i < blk_dst.height - 8; i += clsSizeY) {
        for (int j = 0; j < blk_dst.width; j += clsSizeX) {
            transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];
            class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];

            const int16_t *filt_coeff = filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;
            const int16_t *filt_clip = clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;

            for (int ii = 0; ii < clsSizeY; ii++) {
                const OVSample *src_l0 = _src + j + ii * srcStride;
                const OVSample *src_l1 = src_l0 + srcStride;
                const OVSample *src_l2 = src_l1 + srcStride;
                const OVSample *src_l3 = src_l2 + srcStride;

                const OVSample *src_lb1 = src_l0 - srcStride;
                const OVSample *src_lb2 = src_lb1 - srcStride;
                const OVSample *src_lb3 = src_lb2 - srcStride;

                pRec1 = pRec0 + j + ii * dstStride;

                for (int jj = 0; jj < clsSizeX; jj++) {
                    const int16_t curr = src_l0[+0];

                    int sum = 0;

                    sum += filt_coeff[0] * alf_clip(filt_clip[0], curr, src_l3[+0], src_lb3[+0]);
                    sum += filt_coeff[1] * alf_clip(filt_clip[1], curr, src_l2[+1], src_lb2[-1]);

                    sum += filt_coeff[2] * alf_clip(filt_clip[2], curr, src_l2[+0], src_lb2[+0]);
                    sum += filt_coeff[3] * alf_clip(filt_clip[3], curr, src_l2[-1], src_lb2[+1]);

                    sum += filt_coeff[4] * alf_clip(filt_clip[4], curr, src_l1[+2], src_lb1[-2]);
                    sum += filt_coeff[5] * alf_clip(filt_clip[5], curr, src_l1[+1], src_lb1[-1]);

                    sum += filt_coeff[6] * alf_clip(filt_clip[6], curr, src_l1[+0], src_lb1[+0]);
                    sum += filt_coeff[7] * alf_clip(filt_clip[7], curr, src_l1[-1], src_lb1[+1]);

                    sum += filt_coeff[8] * alf_clip(filt_clip[8], curr, src_l1[-2], src_lb1[+2]);
                    sum += filt_coeff[9] * alf_clip(filt_clip[9], curr, src_l0[+3], src_l0[-3]);

                    sum += filt_coeff[10] * alf_clip(filt_clip[10], curr, src_l0[+2], src_l0[-2]);
                    sum += filt_coeff[11] * alf_clip(filt_clip[11], curr, src_l0[+1], src_l0[-1]);

                    sum = (sum + offset) >> shift;

                    sum += curr;
                    pRec1[jj] = ov_bdclip(sum);

                    src_l0++;
                    src_l1++;
                    src_lb1++;
                    src_l2++;
                    src_lb2++;
                    src_l3++;
                    src_lb3++;
                }
            }
        }

        pRec0 += dstStride2;
        pRec1 += dstStride2;

        _src += srcStride2;
        _dst += dstStride2;
    }

    for (int i = blk_dst.height - 8; i < blk_dst.height - 4; i += clsSizeY) {
        for (int j = 0; j < blk_dst.width; j += clsSizeX) {
            transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];
            class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];

            const int16_t *filt_coeff = filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;
            const int16_t *filt_clip = clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;

            const OVSample *src_vb = _src + j + 4 * srcStride;
            for (int ii = 0; ii < clsSizeY; ii++) {
                const OVSample *src_l0 = _src + j + ii * srcStride;
                const OVSample *src_l1 = src_l0 + srcStride;
                const OVSample *src_l2 = src_l1 + srcStride;
                const OVSample *src_l3 = src_l2 + srcStride;

                const OVSample *src_lb1 = src_l0 - srcStride;
                const OVSample *src_lb2 = src_lb1 - srcStride;
                const OVSample *src_lb3 = src_lb2 - srcStride;

                pRec1 = pRec0 + j + ii * dstStride;

                int y_pos = (blk_dst.y + i + ii) & (ctu_height - 1);
                if (y_pos == virbnd_pos - 3) {
                    src_lb3 = src_vb - 5 * srcStride;
                    src_lb2 = src_vb - 5 * srcStride;
                    src_lb1 = src_vb - 4 * srcStride;

                    src_l0 = src_vb - 3 * srcStride;

                    src_l1 = src_vb - 2 * srcStride;
                    src_l2 = src_vb - srcStride;
                    src_l3 = src_vb - srcStride;

                } else if (y_pos == virbnd_pos - 2) {
                    src_lb3 = src_vb - 3 * srcStride;
                    src_lb2 = src_vb - 3 * srcStride;
                    src_lb1 = src_vb - 3 * srcStride;

                    src_l0 = src_vb - 2 * srcStride;

                    src_l1 = src_vb - srcStride;
                    src_l2 = src_vb - srcStride;
                    src_l3 = src_vb - srcStride;

                } else if (y_pos == virbnd_pos - 1) {
                    src_lb1 = src_vb - srcStride;
                    src_lb2 = src_vb - srcStride;
                    src_lb3 = src_vb - srcStride;

                    src_l0 = src_vb - srcStride;

                    src_l1 = src_vb - srcStride;
                    src_l2 = src_vb - srcStride;
                    src_l3 = src_vb - srcStride;
                }

                for (int jj = 0; jj < clsSizeX; jj++) {
                    const int16_t curr = src_l0[+0];

                    int sum = 0;

                    sum += filt_coeff[0] * alf_clip(filt_clip[0], curr, src_l3[+0], src_lb3[+0]);
                    sum += filt_coeff[1] * alf_clip(filt_clip[1], curr, src_l2[+1], src_lb2[-1]);

                    sum += filt_coeff[2] * alf_clip(filt_clip[2], curr, src_l2[+0], src_lb2[+0]);
                    sum += filt_coeff[3] * alf_clip(filt_clip[3], curr, src_l2[-1], src_lb2[+1]);

                    sum += filt_coeff[4] * alf_clip(filt_clip[4], curr, src_l1[+2], src_lb1[-2]);
                    sum += filt_coeff[5] * alf_clip(filt_clip[5], curr, src_l1[+1], src_lb1[-1]);

                    sum += filt_coeff[6] * alf_clip(filt_clip[6], curr, src_l1[+0], src_lb1[+0]);
                    sum += filt_coeff[7] * alf_clip(filt_clip[7], curr, src_l1[-1], src_lb1[+1]);

                    sum += filt_coeff[8] * alf_clip(filt_clip[8], curr, src_l1[-2], src_lb1[+2]);
                    sum += filt_coeff[9] * alf_clip(filt_clip[9], curr, src_l0[+3], src_l0[-3]);

                    sum += filt_coeff[10] * alf_clip(filt_clip[10], curr, src_l0[+2], src_l0[-2]);
                    sum += filt_coeff[11] * alf_clip(filt_clip[11], curr, src_l0[+1], src_l0[-1]);

                    if (!(y_pos == virbnd_pos - 1)) {
                        sum = (sum + offset) >> shift;
                    } else {
                        int shift2 = shift + 3;
                        int offset2 = 1 << (shift2 - 1);
                        sum = (sum + offset2) >> shift2;
                    }

                    sum += curr;
                    pRec1[jj] = ov_bdclip(sum);

                    src_l0++;
                    src_l1++;
                    src_lb1++;
                    src_l2++;
                    src_lb2++;
                    src_l3++;
                    src_lb3++;
                }
            }
        }

        pRec0 += dstStride2;
        pRec1 += dstStride2;

        _src += srcStride2;
        _dst += dstStride2;
    }

    for (int i = blk_dst.height - 4; i < blk_dst.height; i += clsSizeY) {
        for (int j = 0; j < blk_dst.width; j += clsSizeX) {
            transpose_idx = transpose_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];
            class_idx = class_idx_arr[(i>>2) * CLASSIFICATION_BLK_SIZE + (j>>2)];

            const int16_t *filt_coeff = filter_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;
            const int16_t *filt_clip = clip_set + transpose_idx * MAX_NUM_ALF_CLASSES * MAX_NUM_ALF_LUMA_COEFF + class_idx * MAX_NUM_ALF_LUMA_COEFF;


            const OVSample *src_vb = _src + j;
            for (int ii = 0; ii < clsSizeY; ii++) {
                const OVSample *src_l0 = _src + j + ii * srcStride;
                const OVSample *src_l1 = src_l0 + srcStride;
                const OVSample *src_l2 = src_l1 + srcStride;
                const OVSample *src_l3 = src_l2 + srcStride;

                const OVSample *src_lb1 = src_l0 - srcStride;
                const OVSample *src_lb2 = src_lb1 - srcStride;
                const OVSample *src_lb3 = src_lb2 - srcStride;

                pRec1 = pRec0 + j + ii * dstStride;

                int y_pos = (blk_dst.y + i + ii) & (ctu_height - 1);
                if (y_pos == virbnd_pos) {
                    src_lb3 = src_vb;
                    src_lb2 = src_vb;
                    src_lb1 = src_vb;

                    src_l0 = src_vb;

                    src_l1 = src_vb;
                    src_l2 = src_vb;
                    src_l3 = src_vb;

                } else if (y_pos == virbnd_pos + 1) {
                    src_lb3 = src_vb;
                    src_lb2 = src_vb;
                    src_lb1 = src_vb;

                    src_l0 = src_vb + srcStride;

                    src_l1 = src_vb + 2 * srcStride;
                    src_l2 = src_vb + 2 * srcStride;
                    src_l3 = src_vb + 2 * srcStride;

                } else if (y_pos == virbnd_pos + 2) {
                    src_lb3 = src_vb;
                    src_lb2 = src_vb;
                    src_lb1 = src_vb + srcStride;

                    src_l0 = src_vb +  2 * srcStride;

                    src_l1 = src_vb + 3 * srcStride;
                    src_l2 = src_vb + 4 * srcStride;
                    src_l3 = src_vb + 4 * srcStride;
                }

                for (int jj = 0; jj < clsSizeX; jj++) {
                    const int16_t curr = src_l0[+0];

                    int sum = 0;

                    sum += filt_coeff[0] * alf_clip(filt_clip[0], curr, src_l3[+0], src_lb3[+0]);
                    sum += filt_coeff[1] * alf_clip(filt_clip[1], curr, src_l2[+1], src_lb2[-1]);

                    sum += filt_coeff[2] * alf_clip(filt_clip[2], curr, src_l2[+0], src_lb2[+0]);
                    sum += filt_coeff[3] * alf_clip(filt_clip[3], curr, src_l2[-1], src_lb2[+1]);

                    sum += filt_coeff[4] * alf_clip(filt_clip[4], curr, src_l1[+2], src_lb1[-2]);
                    sum += filt_coeff[5] * alf_clip(filt_clip[5], curr, src_l1[+1], src_lb1[-1]);

                    sum += filt_coeff[6] * alf_clip(filt_clip[6], curr, src_l1[+0], src_lb1[+0]);
                    sum += filt_coeff[7] * alf_clip(filt_clip[7], curr, src_l1[-1], src_lb1[+1]);

                    sum += filt_coeff[8] * alf_clip(filt_clip[8], curr, src_l1[-2], src_lb1[+2]);
                    sum += filt_coeff[9] * alf_clip(filt_clip[9], curr, src_l0[+3], src_l0[-3]);

                    sum += filt_coeff[10] * alf_clip(filt_clip[10], curr, src_l0[+2], src_l0[-2]);
                    sum += filt_coeff[11] * alf_clip(filt_clip[11], curr, src_l0[+1], src_l0[-1]);

                    if (!(y_pos == virbnd_pos)) {
                        sum = (sum + offset) >> shift;
                    } else {
                        int shift2 = shift + 3;
                        int offset2 = 1 << (shift2 - 1);
                        sum = (sum + offset2) >> shift2;
                    }

                    sum += curr;
                    pRec1[jj] = ov_bdclip(sum);

                    src_l0++;
                    src_l1++;
                    src_lb1++;
                    src_l2++;
                    src_lb2++;
                    src_l3++;
                    src_lb3++;
                }
            }
        }

        pRec0 += dstStride2;
        pRec1 += dstStride2;

        _src += srcStride2;
        _dst += dstStride2;
    }
}

static inline uint8_t
check_virtual_bound(int y_pos_pic, int height, int virbnd_pos, uint8_t log2_ctb_s)
{
    uint16_t ctu_msk = (1 << log2_ctb_s) - 1;
    int16_t ctu_vb_y = (y_pos_pic + height - 1) & ctu_msk;

    uint8_t req_vb = (ctu_vb_y  < virbnd_pos && (ctu_vb_y >= virbnd_pos - 4)) ||
                     (ctu_vb_y >= virbnd_pos && (ctu_vb_y <= virbnd_pos + 3));
    return req_vb;
}

static void
rcn_alf_filter_line(OVCTUDec *const ctudec, const struct RectEntryInfo *const einfo, uint16_t ctb_y)
{
    struct ALFInfo* alf_info = &ctudec->alf_info;
    struct OVRCNCtx *const rcn_ctx = &ctudec->rcn_ctx;
    const struct ToolsInfo *tools = &ctudec->tools;
    if (!tools->alf_luma_enabled_flag && !tools->alf_cb_enabled_flag && !tools->alf_cr_enabled_flag) {
        return;
    }

    uint8_t class_idx[CLASSIFICATION_BLK_SIZE*CLASSIFICATION_BLK_SIZE];
    uint8_t transpose_idx[CLASSIFICATION_BLK_SIZE*CLASSIFICATION_BLK_SIZE];
    struct OVFilterBuffers fb = ctudec->rcn_ctx.filter_buffers;
    const OVFrame *frame = rcn_ctx->frame_start;

    const OVPartInfo *const pinfo = ctudec->part_ctx;
    uint8_t log2_ctb_s = pinfo->log2_ctu_s;
    int ctu_s  = 1 << log2_ctb_s;
    uint16_t pic_w = frame->width;
    uint16_t pic_h = frame->height;

    RCNALF* alf = &alf_info->rcn_alf;
    for (int ctb_x = 0; ctb_x < einfo->nb_ctu_w; ctb_x++) {
        int ctb_x_pic = ctb_x + einfo->ctb_x;
        int ctb_y_pic = ctb_y + einfo->ctb_y;
        int x_pos_pic = ctu_s * ctb_x_pic;
        int y_pos_pic = ctu_s * ctb_y_pic;

        int ctu_w = (x_pos_pic + ctu_s > pic_w) ? (pic_w - x_pos_pic) : ctu_s;
        int ctu_h = (y_pos_pic + ctu_s > pic_h) ? (pic_h - y_pos_pic) : ctu_s;

        //left | right | up | down
        uint8_t is_border = 0;
        is_border = (ctb_x == 0)                   ? is_border | OV_BOUNDARY_LEFT_RECT: is_border;
        is_border = (ctb_x == einfo->nb_ctu_w - 1) ? is_border | OV_BOUNDARY_RIGHT_RECT: is_border;
        is_border = (ctb_y == 0)                   ? is_border | OV_BOUNDARY_UPPER_RECT: is_border;
        is_border = (ctb_y == einfo->nb_ctu_h - 1) ? is_border | OV_BOUNDARY_BOTTOM_RECT: is_border;

        int ctu_rs_addr = ctb_x + ctb_y * einfo->nb_ctu_w ;

        ALFParamsCtu* alf_params_ctu = &alf_info->ctb_alf_params[ctu_rs_addr];

        OVSample **src        = fb.filter_region;
        OVSample **saved_rows = fb.saved_rows_alf;

        int x_pos = ctu_s * ctb_x;

        ctudec->rcn_funcs.rcn_extend_filter_region(&ctudec->rcn_ctx, saved_rows, x_pos, x_pos_pic, y_pos_pic, is_border);

        if (alf_params_ctu->ctb_alf_flag & 0x4) {
            uint8_t c_idx = 0;
            int stride_src = fb.filter_region_stride[c_idx];
            int stride_dst = frame->linesize[c_idx] / sizeof(OVSample);
            OVSample *src_luma = &src[c_idx][fb.filter_region_offset[c_idx]];
            OVSample *dst_luma = (OVSample *) frame->data[c_idx] + y_pos_pic * stride_dst + x_pos_pic;

            Area blk_dst = {
                .x = x_pos_pic,
                .y = y_pos_pic,
                .width  = ctu_w,
                .height = ctu_h,
            };

            rcn_alf_derive_classification(src_luma, stride_src, blk_dst,
                                          ctu_s, pic_h,
                                          ctudec->rcn_funcs.alf.classif,
                                          class_idx, transpose_idx);

            int16_t filter_idx = alf_params_ctu->ctb_alf_idx;
            int16_t *coeff = alf->coeff_l[filter_idx];
            int16_t *clip  = alf->clip_l [filter_idx];

            int virbnd_pos = (y_pos_pic + ctu_s > pic_h) ? pic_h
                                                         : ctu_h - ALF_VB_POS_ABOVE_CTUROW_LUMA;

            uint8_t req_vb = check_virtual_bound(y_pos_pic, ctu_h, virbnd_pos, log2_ctb_s);

            ctudec->rcn_funcs.alf.luma[req_vb](class_idx, transpose_idx, dst_luma,
                                               src_luma, stride_dst, stride_src,
                                               blk_dst, coeff, clip,
                                               ctu_s, virbnd_pos);
        }

        if (alf_params_ctu->ctb_alf_flag & (0x3 | 0x18)) {

            Area blk_dst = {
                .x = x_pos_pic >> 1,
                .y = y_pos_pic >> 1,
                .width  = ctu_w >> 1,
                .height = ctu_h >> 1
            };

            int stride_dst = frame->linesize[1] / sizeof(OVSample);
            int virbnd_pos = (y_pos_pic + ctu_s > pic_h) ? pic_h
                                                         : (ctu_s - ALF_VB_POS_ABOVE_CTUROW_LUMA);

            uint8_t req_vb = check_virtual_bound(y_pos_pic, ctu_h, virbnd_pos, log2_ctb_s) || ctu_h != ctu_s
                                                                                           || ctu_w != ctu_s;
            int pos_offset = blk_dst.y * stride_dst + blk_dst.x;

            if (alf_params_ctu->ctb_alf_flag & 0x2) {

                int stride_src = fb.filter_region_stride[1];

                OVSample* src_chroma = &src[1][fb.filter_region_offset[1]];
                OVSample* dst_chroma = (OVSample*) frame->data[1] + pos_offset;

                uint8_t alt_num = alf_params_ctu->cb_alternative;

                ctudec->rcn_funcs.alf.chroma[req_vb](dst_chroma, src_chroma,
                                                     stride_dst, stride_src, blk_dst,
                                                     alf->coeff_c[alt_num],
                                                     alf->clip_c[alt_num],
                                                     ctu_s >> 1,
                                                     virbnd_pos >> 1);
            }

            if (alf_params_ctu->ctb_alf_flag & 0x8) {
                //const OVALFData* alf_data = alf_info->aps_cc_alf_data_cb;

                const uint8_t filt_idx = alf_params_ctu->cb_ccalf;

                int stride_src = fb.filter_region_stride[0];
                OVSample*  src_luma = &src[0][fb.filter_region_offset[0]];
                OVSample*  dst_chroma = (OVSample*) frame->data[1] + pos_offset;

                //const int8_t *filt_coeff = alf_data->alf_cc_mapped_coeff[0][filt_idx];
                const int8_t *filt_coeff = alf_info->rcn_alf.ccalf_coef_cb[filt_idx];

                ctudec->rcn_funcs.alf.ccalf[req_vb](dst_chroma, src_luma, stride_dst,
                                                    stride_src, blk_dst, filt_coeff,
                                                    ctu_s, virbnd_pos);

            }

            if (alf_params_ctu->ctb_alf_flag & 0x1) {
                int stride_src = fb.filter_region_stride[2];

                OVSample*  src_chroma = &src[2][fb.filter_region_offset[2]];
                OVSample*  dst_chroma = (OVSample*) frame->data[2] + pos_offset;

                uint8_t alt_num = alf_params_ctu->cr_alternative;

                ctudec->rcn_funcs.alf.chroma[req_vb](dst_chroma, src_chroma,
                                                     stride_dst, stride_src, blk_dst,
                                                     alf->coeff_c[alt_num],
                                                     alf->clip_c[alt_num],
                                                     ctu_s >> 1,
                                                     virbnd_pos >> 1);
            }


            if (alf_params_ctu->ctb_alf_flag & 0x10) {
                //const OVALFData* alf_data = alf_info->aps_cc_alf_data_cr;

                const uint8_t filt_idx = alf_params_ctu->cr_ccalf;

                int stride_src = fb.filter_region_stride[0];

                OVSample*  src_luma = &src[0][fb.filter_region_offset[0]];
                OVSample*  dst_chroma = (OVSample*) frame->data[2] + pos_offset;

                //const int8_t *filt_coeff = alf_data->alf_cc_mapped_coeff[1][filt_idx];
                const int8_t *filt_coeff = alf_info->rcn_alf.ccalf_coef_cr[filt_idx];

                ctudec->rcn_funcs.alf.ccalf[req_vb](dst_chroma, src_luma, stride_dst,
                                                    stride_src, blk_dst, filt_coeff,
                                                    ctu_s, virbnd_pos);

            }
        }

        ctudec->rcn_funcs.rcn_save_last_rows(&ctudec->rcn_ctx, saved_rows, x_pos, x_pos_pic, y_pos_pic, is_border);
        ctudec->rcn_funcs.rcn_save_last_cols(&ctudec->rcn_ctx, x_pos_pic, y_pos_pic, is_border);
    }
}

void
BD_DECL(rcn_init_alf_functions)(struct RCNFunctions *rcn_func)
{
    rcn_func->alf.classif=&rcn_alf_derive_classificationBlk;
    rcn_func->alf.luma[0]=&alf_filterBlkLuma;
    rcn_func->alf.luma[1]=&alf_filterBlkLumaVB;
    rcn_func->alf.chroma[0]=&alf_filter_c;
    rcn_func->alf.chroma[1]=&alf_filter_cVB;
    rcn_func->alf.ccalf[0]=&cc_alf_filterBlk;
    rcn_func->alf.ccalf[1]=&cc_alf_filterBlkVB;
    rcn_func->alf.rcn_alf_reconstruct_coeff_APS = &rcn_alf_reconstruct_coeff_APS;
    rcn_func->alf.rcn_alf_filter_line = &rcn_alf_filter_line;
}
