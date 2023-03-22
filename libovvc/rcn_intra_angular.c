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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ovutils.h"

#include "rcn_intra_angular.h"
#include "data_rcn_angular.h"

static const int8_t chroma_filter[4 * 32] =
{
     0, 64,  0,  0,
    -1, 63,  2,  0,
    -2, 62,  4,  0,
    -2, 60,  7, -1,
    -2, 58, 10, -2,
    -3, 57, 12, -2,
    -4, 56, 14, -2,
    -4, 55, 15, -2,
    -4, 54, 16, -2,
    -5, 53, 18, -2,
    -6, 52, 20, -2,
    -6, 49, 24, -3,
    -6, 46, 28, -4,
    -5, 44, 29, -4,
    -4, 42, 30, -4,
    -4, 39, 33, -4,
    -4, 36, 36, -4,
    -4, 33, 39, -4,
    -4, 30, 42, -4,
    -4, 29, 44, -5,
    -4, 28, 46, -6,
    -3, 24, 49, -6,
    -2, 20, 52, -6,
    -2, 18, 53, -5,
    -2, 16, 54, -4,
    -2, 15, 55, -4,
    -2, 14, 56, -4,
    -2, 12, 57, -3,
    -2, 10, 58, -2,
    -1,  7, 60, -2,
     0,  4, 62, -2,
     0,  2, 63, -1
};

static const uint8_t vvc_pdpc_w[3][128] =
{
    {  32,  8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {  32, 16,  8, 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {  32, 32, 16, 16,  8, 8, 4, 4, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

static void
intra_angular_hdia(const OVSample* const ref_lft, OVSample* const dst,
                   ptrdiff_t dst_stride, int8_t log2_pb_w,
                   int8_t log2_pb_h)
{

    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;

    int delta_pos = 32;

    for (int y = 0; y < pb_w; y++) {
        const int delta_int = delta_pos >> 5;
        for (int x = 0; x < pb_h; x++) {
            _tmp[x] = ref_lft[x + delta_int + 1];
        }
        delta_pos += 32;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_hdia_pdpc(const OVSample* const ref_abv,
                        const OVSample* const ref_lft, OVSample* const dst,
                        ptrdiff_t dst_stride, int8_t log2_pb_w,
                        int8_t log2_pb_h)
{

    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int scale = OVMIN(2, log2_pb_w - 2);

    int delta_pos = 32;

    for (int y = 0; y < pb_w; y++) {
        const int delta_int = delta_pos >> 5;
        for (int x = 0; x < pb_h; x++) {
            _tmp[x] = ref_lft[x + delta_int + 1];
        }
        for (int x = 0; x < OVMIN(3 << scale, pb_h); x++) {
            int wL = 32 >> (2 * x >> scale);
            const int16_t above = ref_abv[y + x + 2];
            _tmp[x] = ov_bdclip(_tmp[x] + ((wL * (above - _tmp[x]) + 32) >> 6));
        }
        delta_pos += 32;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_vdia(const OVSample* const ref_abv, OVSample* const dst,
                   ptrdiff_t dst_stride, int8_t log2_pb_w,
                   int8_t log2_pb_h)
{
    int delta_pos = 32;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;

    for (int y = 0; y < pb_h; y++) {
        const int delta_int = delta_pos >> 5;
        for (int x = 0; x < pb_w; x++) {
            _dst[x] = ref_abv[x + delta_int + 1];
        }
        delta_pos += 32;
        _dst += dst_stride;
    }
}

static void
intra_angular_vdia_pdpc(const OVSample* const ref_abv,
                        const OVSample* const ref_lft, OVSample* const dst,
                        ptrdiff_t dst_stride, int8_t log2_pb_w,
                        int8_t log2_pb_h)
{
    int delta_pos = 32;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int scale = OVMIN(2, log2_pb_h - 2);

    for (int y = 0; y < pb_h; y++) {
        const int delta_int = delta_pos >> 5;
        for (int x = 0; x < pb_w; x++) {
            _dst[x] = ref_abv[x + delta_int + 1];
        }
        for (int x = 0; x < OVMIN(3 << scale, pb_w); x++) {
            int wL = 32 >> (2 * x >> scale);
            const int16_t left = ref_lft[y + x + 2];
            _dst[x] = ov_bdclip(_dst[x] + ((wL * (left - _dst[x]) + 32) >> 6));
        }
        delta_pos += 32;
        _dst += dst_stride;
    }
}

static void
intra_angular_h_c(const OVSample* ref_lft, OVSample* dst,
                  ptrdiff_t dst_stride, int8_t log2_pb_w,
                  int8_t log2_pb_h, int angle_val)
{
    OVSample tmp_dst[128 * 128];
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    const int tmp_stride = 128;

    int delta_pos = angle_val;

    for (int y = 0; y < pb_w; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        const OVSample* ref = ref_lft + delta_int + 1;
        int last_ref_val = *ref++;
        for (int x = 0; x < pb_h; x++) {
            int curr_ref_val = *ref;
            int val;
            val = (int16_t)last_ref_val + (((delta_frac) * (curr_ref_val - last_ref_val) + 16) >> 5);
            _tmp[x] = ov_bdclip(val);
            last_ref_val = curr_ref_val;
            ref++;
        }
        delta_pos += angle_val;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_v_c(const OVSample* ref_abv, OVSample* dst,
                  ptrdiff_t dst_stride, int8_t log2_pb_w,
                  int8_t log2_pb_h, int angle_val)
{
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;

    for (int y = 0, delta_pos = angle_val; y < pb_h; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        const OVSample* ref = ref_abv + delta_int + 1;
        int last_ref_val = *ref++;
        for (int x = 0; x < pb_w; ref++, x++) {
            int curr_ref_val = *ref;
            int val;
            val = (int16_t)last_ref_val + (((delta_frac) * (curr_ref_val - last_ref_val) + 16) >> 5);
            _dst[x] = ov_bdclip(val);
            last_ref_val = curr_ref_val;
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_hor_pdpc(const OVSample* const ref_abv,
                       const OVSample* const ref_lft, OVSample* const dst,
                       ptrdiff_t dst_stride, int8_t log2_pb_w,
                       int8_t log2_pb_h)
{
    OVSample* _dst = dst;
    int pb_width = 1 << log2_pb_w;
    int pb_height = 1 << log2_pb_h;
    int pdpc_scale = (log2_pb_w + log2_pb_h - 2) >> 2;
    const uint8_t* pdpc_w = vvc_pdpc_w[pdpc_scale];

    const uint16_t tl_val = ref_abv[0];
    for (int y = 0; y < pb_height; y++) {
        int l_wgh = pdpc_w[y];
        int32_t l_val = ref_lft[y + 1];
        for (int x = 0; x < pb_width; x++) {
            const int32_t t_val = ref_abv[x + 1];
            int val = (l_wgh * (t_val - tl_val) + (l_val << 6) + 32) >> 6;
            _dst[x] = ov_bdclip(val);
        }
        _dst += dst_stride;
    }
}

static void
intra_angular_ver_pdpc(const OVSample* const ref_abv,
                       const OVSample* const ref_lft, OVSample* const dst,
                       ptrdiff_t dst_stride, int8_t log2_pb_w,
                       int8_t log2_pb_h)
{
    OVSample* _dst = dst;
    const uint16_t tl_val = ref_abv[0];
    int pdpc_scale = (log2_pb_w + log2_pb_h - 2) >> 2;
    const uint8_t* pdpc_w = vvc_pdpc_w[pdpc_scale];

    int pb_width = 1 << log2_pb_w;
    int pb_height = 1 << log2_pb_h;

    for (int y = 0; y < pb_height; y++) {
        const uint16_t l_val = ref_lft[y + 1];
        for (int x = 0; x < pb_width; x++) {
            const int32_t t_val = ref_abv[x + 1];
            int l_wgh = pdpc_w[x];
            int val = (l_wgh * (l_val - tl_val) + (t_val << 6) + 32) >> 6;
            _dst[x] = ov_bdclip(val);
        }
        _dst += dst_stride;
    }
}

static void
intra_angular_hor(const OVSample* const ref_lft, OVSample* const dst,
                  ptrdiff_t dst_stride, int8_t log2_pb_w, int8_t log2_pb_h)
{
    OVSample* _dst = dst;
    int pb_width = 1 << log2_pb_w;
    int pb_height = 1 << log2_pb_h;

    for (int y = 0; y < pb_height; y++) {
        for (int j = 0; j < pb_width; j++) {
            _dst[j] = ref_lft[y + 1];
        }
        _dst += dst_stride;
    }
}

static void
intra_angular_ver(const OVSample* const ref_abv, OVSample* const dst,
                  ptrdiff_t dst_stride, int8_t log2_pb_w, int8_t log2_pb_h)
{
    OVSample* _dst = dst;
    int pb_width = 1 << log2_pb_w;
    int pb_height = 1 << log2_pb_h;

    for (int y = 0; y < pb_height; y++) {
        memcpy(_dst, &ref_abv[1], sizeof(OVSample) * pb_width);
        _dst += dst_stride;
    }
}

static void
intra_angular_h_c_pdpc(const OVSample* const ref_abv,
                       const OVSample* const ref_lft, OVSample* const dst,
                       ptrdiff_t dst_stride, int8_t log2_pb_w,
                       int8_t log2_pb_h, int mode_idx)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int delta_pos = angle_val;
    int scale = OVMIN(2, log2_pb_w - (floor_log2(3 * inv_angle - 2) - 8));

    for (int y = 0; y < pb_w; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        int inv_angle_sum = 256 + inv_angle;

        const OVSample* ref = ref_lft + delta_int + 1;
        int last_ref_val = *ref++;
        for (int x = 0; x < pb_h; ref++, x++) {
            int curr_ref_val = *ref;
            _tmp[x] = (int16_t)last_ref_val + (((delta_frac) * (curr_ref_val - last_ref_val) + 16) >> 5);
            last_ref_val = curr_ref_val;
        }

        for (int x = 0; x < OVMIN(3 << scale, pb_h); x++) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_abv + y + (inv_angle_sum >> 9) + 1;

            int32_t left = p[0];
            _tmp[x] = ov_bdclip(_tmp[x] + ((wL * (left - _tmp[x]) + 32) >> 6));
            inv_angle_sum += inv_angle;
        }
        delta_pos += angle_val;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_v_c_pdpc(const OVSample* const ref_abv,
                       const OVSample* const ref_lft, OVSample* const dst,
                       ptrdiff_t dst_stride, int8_t log2_pb_w,
                       int8_t log2_pb_h, int mode_idx)
{
    OVSample* _dst = dst;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int delta_pos = angle_val;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int scale = OVMIN(2, log2_pb_h - (floor_log2(3 * inv_angle - 2) - 8));

    for (int y = 0; y < pb_h; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        int inv_angle_sum = 256 + inv_angle;

        const OVSample* ref = ref_abv + delta_int + 1;
        int last_ref_val = *ref++;
        for (int x = 0; x < pb_w; ref++, x++) {
            int curr_ref_val = *ref;
            _dst[x] = (int16_t)last_ref_val + (((delta_frac) * (curr_ref_val - last_ref_val) + 16) >> 5);
            last_ref_val = curr_ref_val;
        }
        for (int x = 0; x < OVMIN(3 << scale, pb_w); x++) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_lft + y + (inv_angle_sum >> 9) + 1;

            int32_t left = p[0];
            _dst[x] = ov_bdclip(_dst[x] + ((wL * (left - _dst[x]) + 32) >> 6));
            inv_angle_sum += inv_angle;
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_h_nofrac(const OVSample* ref_lft, OVSample* dst,
                       ptrdiff_t dst_stride, int8_t log2_pb_w,
                       int8_t log2_pb_h, int angle_val)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val >> 5;
    int y;

    for (y = 0; y < pb_w; ++y) {
        memcpy(_tmp, &ref_lft[delta_pos + 1], sizeof(OVSample) * pb_h);
        delta_pos += angle_val >> 5;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_h_nofrac_pdpc(const OVSample* ref_abv, const OVSample* ref_lft,
                            OVSample* dst, ptrdiff_t dst_stride,
                            int8_t log2_pb_w, int8_t log2_pb_h, int mode_idx)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val >> 5;
    int scale = OVMIN(2, log2_pb_w - (floor_log2(3 * inv_angle - 2) - 8));
    int y;

    for (y = 0; y < pb_w; ++y) {
        int inv_angle_sum = 256 + inv_angle;
        memcpy(_tmp, &ref_lft[delta_pos + 1], sizeof(OVSample) * pb_h);
        for (int x = 0; x < OVMIN(3 << scale, pb_h); x++) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_abv + y + (inv_angle_sum >> 9) + 1;

            int16_t left = p[0];
            _tmp[x] = ov_bdclip(_tmp[x] + ((wL * (left - _tmp[x]) + 32) >> 6));
            inv_angle_sum += inv_angle;
        }
        delta_pos += angle_val >> 5;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_h_gauss_pdpc(const OVSample* ref_abv, const OVSample* ref_lft,
                           OVSample* const dst, ptrdiff_t dst_stride,
                           int8_t log2_pb_w, int8_t log2_pb_h, int mode_idx)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val;
    int scale = OVMIN(2, log2_pb_w - (floor_log2(3 * inv_angle - 2) - 8));

    for (int y = 0; y < pb_w; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        int inv_angle_sum = 256 + inv_angle;
        const OVSample* ref = (OVSample *)ref_lft + delta_int;
        for (int x = 0; x < pb_h; x++) {
            _tmp[x] = ((int32_t)(ref[0] * (16 - (delta_frac >> 1))) +
                       (int32_t)(ref[1] * (32 - (delta_frac >> 1))) +
                       (int32_t)(ref[2] * (16 + (delta_frac >> 1))) +
                       (int32_t)(ref[3] * (     (delta_frac >> 1))) + 32) >> 6;
            ref++;
        }
        for (int x = 0; x < OVMIN(3 << scale, pb_h); x++) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_abv + y + (inv_angle_sum >> 9) + 1;

            int16_t left = p[0];
            _tmp[x] = ov_bdclip(_tmp[x] + ((wL * (left - _tmp[x]) + 32) >> 6));
            inv_angle_sum += inv_angle;
        }

        delta_pos += angle_val;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_v_nofrac(const OVSample* ref_abv, OVSample* dst,
                       ptrdiff_t dst_stride, int8_t log2_pb_w,
                       int8_t log2_pb_h, int angle_val)
{
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val;

    int y;
    for (y = 0; y < pb_h; y++) {
        const int delta_int = delta_pos >> 5;
        memcpy(_dst, &ref_abv[delta_int + 1], sizeof(OVSample) * pb_w);
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_v_nofrac_pdpc(const OVSample* ref_abv, const OVSample* ref_lft,
                            OVSample* const dst, ptrdiff_t dst_stride,
                            int8_t log2_pb_w, int8_t log2_pb_h, int mode_idx)
{
    OVSample* _dst = dst;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val;
    int scale = OVMIN(2, log2_pb_h - (floor_log2(3 * inv_angle - 2) - 8));

    int y, x;
    for (y = 0; y < pb_h; ++y) {
        const int delta_int = delta_pos >> 5;
        int inv_angle_sum = 256 + inv_angle;
        memcpy(_dst, &ref_abv[delta_int + 1], sizeof(OVSample) * pb_w);
        for (x = 0; x < OVMIN(3 << scale, pb_w); ++x) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_lft + y + (inv_angle_sum >> 9) + 1;
            int16_t left = p[0];

            _dst[x] = ov_bdclip(_dst[x] + ((wL * (left - _dst[x]) + 32) >> 6));

            inv_angle_sum += inv_angle;
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_v_gauss_pdpc(const OVSample* ref_abv, const OVSample* ref_lft,
                           OVSample* const dst, ptrdiff_t dst_stride,
                           int8_t log2_pb_w, int8_t log2_pb_h, int mode_idx)
{
    OVSample* _dst = dst;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val;
    int scale = OVMIN(2, log2_pb_h - (floor_log2(3 * inv_angle - 2) - 8));

    for (int y = 0; y < pb_h; y++) {
        const int delta_int  = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        int inv_angle_sum = 256 + inv_angle;
        const OVSample* ref = (OVSample*)ref_abv + delta_int;

        for (int x = 0; x < pb_w; x++) {
            _dst[x] = ((int32_t)(ref[0] * (16 - (delta_frac >> 1))) +
                       (int32_t)(ref[1] * (32 - (delta_frac >> 1))) +
                       (int32_t)(ref[2] * (16 + (delta_frac >> 1))) +
                       (int32_t)(ref[3] * (     (delta_frac >> 1))) + 32) >> 6;
            ref++;
        }
        for (int x = 0; x < OVMIN(3 << scale, pb_w); x++) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_lft + y + (inv_angle_sum >> 9) + 1;

            int16_t left = p[0];
            _dst[x] =
                ov_bdclip(_dst[x] + ((wL * (left - _dst[x]) + 32) >> 6));
            inv_angle_sum += inv_angle;
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_h_cubic(const OVSample* ref_lft, OVSample* dst,
                      ptrdiff_t dst_stride, int8_t log2_pb_w,
                      int8_t log2_pb_h, int angle_val)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;

    int delta_pos = angle_val;
    for (int y = 0; y < pb_w; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        const OVSample* ref = (OVSample*)ref_lft + delta_int;
        const int8_t* filter = &chroma_filter[delta_frac << 2];
        for (int x = 0; x < pb_h; x++) {

            int32_t val = ((int32_t)(ref[0] * filter[0]) +
                           (int32_t)(ref[1] * filter[1]) +
                           (int32_t)(ref[2] * filter[2]) +
                           (int32_t)(ref[3] * filter[3]) + 32) >> 6;

            _tmp[x] = ov_bdclip(val);
            ref++;
        }
        delta_pos += angle_val;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_h_gauss(const OVSample* ref_lft, OVSample* dst,
                      ptrdiff_t dst_stride, int8_t log2_pb_w,
                      int8_t log2_pb_h, int angle_val)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;

    int delta_pos = angle_val;
    for (int y = 0; y < pb_w; y++) {
        const int delta_int  = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        const OVSample* ref = (OVSample*)ref_lft + delta_int;
        for (int x = 0; x < pb_h; x++) {
            _tmp[x] = ((int32_t)(ref[0] * (16 - (delta_frac >> 1))) +
                       (int32_t)(ref[1] * (32 - (delta_frac >> 1))) +
                       (int32_t)(ref[2] * (16 + (delta_frac >> 1))) +
                       (int32_t)(ref[3] * ((delta_frac >> 1))) + 32) >> 6;
            ref++;
        }
        delta_pos += angle_val;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_v_cubic(const OVSample* ref_abv, OVSample* dst,
                      ptrdiff_t dst_stride, int8_t log2_pb_w,
                      int8_t log2_pb_h, int angle_val)
{
    int delta_pos = angle_val;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    OVSample* _dst = dst;

    for (int y = 0; y < pb_h; y++) {
        const int delta_int  = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;

        const OVSample* ref = (OVSample*)ref_abv + delta_int;
        const int8_t* filter = &chroma_filter[delta_frac << 2];

        for (int x = 0; x < pb_w; x++) {
            int32_t val = ((int32_t)(ref[0] * filter[0]) +
                           (int32_t)(ref[1] * filter[1]) +
                           (int32_t)(ref[2] * filter[2]) +
                           (int32_t)(ref[3] * filter[3]) + 32) >> 6;
            _dst[x] = ov_bdclip(val);
            ref++;
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_v_gauss(const OVSample* ref_abv, OVSample* dst,
                      ptrdiff_t dst_stride, int8_t log2_pb_w,
                      int8_t log2_pb_h, int angle_val)
{
    int delta_pos = angle_val;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    OVSample* _dst = dst;

    for (int y = 0; y < pb_h; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;

        const OVSample* ref = (OVSample*)ref_abv + delta_int;

        for (int x = 0; x < pb_w; x++) {
            _dst[x] = ((int32_t)(ref[0] * (16 - (delta_frac >> 1))) +
                       (int32_t)(ref[1] * (32 - (delta_frac >> 1))) +
                       (int32_t)(ref[2] * (16 + (delta_frac >> 1))) +
                       (int32_t)(ref[3] * (     (delta_frac >> 1))) + 32) >> 6;
            ref++;
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_h_cubic_pdpc(const OVSample* ref_abv, const OVSample* ref_lft,
                           OVSample* const dst, ptrdiff_t dst_stride,
                           int8_t log2_pb_w, int8_t log2_pb_h, int mode_idx)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val;
    int scale = OVMIN(2, log2_pb_w - (floor_log2(3 * inv_angle - 2) - 8));

    for (int y = 0; y < pb_w; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        int inv_angle_sum = 256 + inv_angle;
        const OVSample* ref = (OVSample*)ref_lft + delta_int;
        const int8_t* filter = &chroma_filter[delta_frac << 2];

        for (int x = 0; x < pb_h; x++) {
            int val = ((int32_t)(ref[0] * filter[0]) +
                       (int32_t)(ref[1] * filter[1]) +
                       (int32_t)(ref[2] * filter[2]) +
                       (int32_t)(ref[3] * filter[3]) + 32) >> 6;
            ref++;
            _tmp[x] = ov_bdclip(val);
        }

        for (int x = 0; x < OVMIN(3 << scale, pb_h); x++) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_abv + y + (inv_angle_sum >> 9) + 1;
            int16_t left = p[0];

            _tmp[x] = ov_bdclip(_tmp[x] + ((wL * (left - _tmp[x]) + 32) >> 6));

            inv_angle_sum += inv_angle;
        }
        delta_pos += angle_val;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_v_cubic_pdpc(const OVSample* ref_abv, const OVSample* ref_lft,
                           OVSample* const dst, ptrdiff_t dst_stride,
                           int8_t log2_pb_w, int8_t log2_pb_h, int mode_idx)
{
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int angle_val = angle_table[mode_idx];
    int inv_angle = inverse_angle_table[mode_idx];
    int delta_pos = angle_val;
    int scale = OVMIN(2, log2_pb_h - (floor_log2(3 * inv_angle - 2) - 8));

    for (int y = 0; y < pb_h; y++) {
        const int delta_int = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        int inv_angle_sum = 256 + inv_angle;
        const OVSample* ref = (OVSample *)ref_abv + delta_int;
        const int8_t* filter = &chroma_filter[delta_frac << 2];
        for (int x = 0; x < pb_w; x++) {
            int val = ((int32_t)(ref[0] * filter[0]) +
                       (int32_t)(ref[1] * filter[1]) +
                       (int32_t)(ref[2] * filter[2]) +
                       (int32_t)(ref[3] * filter[3]) + 32) >> 6;

            ref++;
            _dst[x] = ov_bdclip(val);
        }

        for (int x = 0; x < OVMIN(3 << scale, pb_w); x++) {
            int wL = 32 >> ((x << 1) >> scale);
            const OVSample* p = ref_lft + y + (inv_angle_sum >> 9) + 1;

            int16_t left = p[0];

            _dst[x] = ov_bdclip(_dst[x] + ((wL * (left - _dst[x]) + 32) >> 6));
            inv_angle_sum += inv_angle;
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

static void
intra_angular_h_cubic_mref(const OVSample* const ref_lft, OVSample* const dst,
                           ptrdiff_t dst_stride,
                           int8_t log2_pb_w, int8_t log2_pb_h,
                           int angle_val, uint8_t mrl_idx)
{
    OVSample tmp_dst[128 * 128];
    const int tmp_stride = 128;
    OVSample* _tmp = tmp_dst;
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;
    int delta_pos = angle_val * (mrl_idx + 1);

    for (int y = 0; y < pb_w; y++) {
        const int delta_int  = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        const OVSample* ref = (OVSample *)ref_lft + delta_int;
        const int8_t* filter = &chroma_filter[delta_frac << 2];

        for (int x = 0; x < pb_h; x++) {
            int val = ((int32_t)(ref[0] * filter[0]) +
                       (int32_t)(ref[1] * filter[1]) +
                       (int32_t)(ref[2] * filter[2]) +
                       (int32_t)(ref[3] * filter[3]) + 32) >> 6;
            ref++;
            _tmp[x] = ov_bdclip(val);
        }
        delta_pos += angle_val;
        _tmp += tmp_stride;
    }

    _tmp = tmp_dst;

    for (int y = 0; y < pb_w; y++) {
        _dst = &dst[y];
        for (int x = 0; x < pb_h; x++) {
            _dst[0] = _tmp[x];
            _dst += dst_stride;
        }
        _tmp += tmp_stride;
    }
}

static void
intra_angular_v_cubic_mref(const OVSample* const ref_abv, OVSample* const dst,
                           ptrdiff_t dst_stride, int8_t log2_pb_w,
                           int8_t log2_pb_h, int angle_val,
                           uint8_t mrl_idx)
{
    int delta_pos = angle_val * (mrl_idx + 1);
    OVSample* _dst = dst;
    int pb_w = 1 << log2_pb_w;
    int pb_h = 1 << log2_pb_h;

    for (int y = 0; y < pb_h; y++) {
        const int delta_int  = delta_pos >> 5;
        const int delta_frac = delta_pos & 0x1F;
        const OVSample* ref = (OVSample *)ref_abv + delta_int;
        const int8_t* filter = &chroma_filter[delta_frac << 2];
        for (int x = 0; x < pb_w; x++) {
            int val = ((int32_t)(ref[0] * filter[0]) +
                       (int32_t)(ref[1] * filter[1]) +
                       (int32_t)(ref[2] * filter[2]) +
                       (int32_t)(ref[3] * filter[3]) + 32) >> 6;
            ref++;
            _dst[x] = ov_bdclip(val);
        }
        delta_pos += angle_val;
        _dst += dst_stride;
    }
}

#define FUNC3(a, b, c)  a ## _ ## b ##  c
#define FUNC2(a, b, c)  FUNC3(a, b, c)
#define ANGULAR_DECL(a)  FUNC2(a, BITDEPTH, )

ANGULAR_DECL(const struct IntraAngularFunctions angular_gauss_h) =
{
     .pure = intra_angular_hor,
     .diagonal = intra_angular_hdia,
     .angular = intra_angular_h_gauss,

     .pure_pdpc = intra_angular_hor_pdpc,
     .diagonal_pdpc = intra_angular_hdia_pdpc,
     .angular_pdpc = intra_angular_h_gauss_pdpc,
};

ANGULAR_DECL(const struct IntraAngularFunctions angular_gauss_v) =
{
     .pure = intra_angular_ver,
     .diagonal = intra_angular_vdia,
     .angular = intra_angular_v_gauss,

     .pure_pdpc = intra_angular_ver_pdpc,
     .diagonal_pdpc = intra_angular_vdia_pdpc,
     .angular_pdpc = intra_angular_v_gauss_pdpc,
};

ANGULAR_DECL(const struct IntraAngularFunctions angular_cubic_v) =
{
     .pure = intra_angular_ver,
     .diagonal = intra_angular_vdia,
     .angular = intra_angular_v_cubic,

     .pure_pdpc = intra_angular_ver_pdpc,
     .diagonal_pdpc = intra_angular_vdia_pdpc,
     .angular_pdpc = intra_angular_v_cubic_pdpc,
};

ANGULAR_DECL(const struct IntraAngularFunctions angular_cubic_h) =
{
     .pure = intra_angular_hor,
     .diagonal = intra_angular_hdia,
     .angular = intra_angular_h_cubic,

     .pure_pdpc = intra_angular_hor_pdpc,
     .diagonal_pdpc = intra_angular_hdia_pdpc,
     .angular_pdpc = intra_angular_h_cubic_pdpc,
};

ANGULAR_DECL(const struct IntraAngularFunctions angular_c_h) =
{
     .pure = intra_angular_hor,
     .diagonal = intra_angular_hdia,
     .angular = intra_angular_h_c,

     .pure_pdpc = intra_angular_hor_pdpc,
     .diagonal_pdpc = intra_angular_hdia_pdpc,
     .angular_pdpc = intra_angular_h_c_pdpc,
};

ANGULAR_DECL(const struct IntraAngularFunctions angular_c_v) =
{
     .pure = intra_angular_ver,
     .diagonal = intra_angular_vdia,
     .angular = intra_angular_v_c,

     .pure_pdpc = intra_angular_ver_pdpc,
     .diagonal_pdpc = intra_angular_vdia_pdpc,
     .angular_pdpc = intra_angular_v_c_pdpc,
};

ANGULAR_DECL(const struct IntraMRLFunctions mrl_func) =
{
     .angular_h = intra_angular_h_cubic_mref,
     .angular_v = intra_angular_v_cubic_mref,

};

ANGULAR_DECL(const struct IntraAngularFunctions angular_nofrac_v) =
{
     .pure = intra_angular_ver,
     .diagonal = intra_angular_vdia,
     .angular = intra_angular_v_nofrac,

     .pure_pdpc = intra_angular_ver_pdpc,
     .diagonal_pdpc = intra_angular_vdia_pdpc,
     .angular_pdpc = intra_angular_v_nofrac_pdpc,
};

ANGULAR_DECL(const struct IntraAngularFunctions angular_nofrac_h) =
{
     .pure = intra_angular_hor,
     .diagonal = intra_angular_hdia,
     .angular = intra_angular_h_nofrac,

     .pure_pdpc = intra_angular_hor_pdpc,
     .diagonal_pdpc = intra_angular_hdia_pdpc,
     .angular_pdpc = intra_angular_h_nofrac_pdpc,
};
