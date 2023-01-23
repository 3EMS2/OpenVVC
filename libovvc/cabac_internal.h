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

#ifndef CABAC_INTERNAL_H
#define CABAC_INTERNAL_H
#include "ovutils.h"
#include "vcl_cabac.h"

#define NB_CABAC_BITS 16
#define CABAC_MASK ((1 << NB_CABAC_BITS) - 1)

extern const uint8_t lps_renorm_table[64];
extern const uint8_t range_lps_lut[512];

static inline uint8_t
ovcabac_ae_read(OVCABACCtx *const cabac_ctx, uint64_t *const cabac_state)
{
    uint16_t state_0 = (*cabac_state) >> 48;
    uint16_t state_1 = (*cabac_state) >> 32;
    uint8_t   rate_0 = (*cabac_state) >> 16;
    uint8_t   rate_1 = (*cabac_state) >>  0;
    int8_t state = (state_0 + state_1) >> 8;
    uint16_t symbol_mask;
    uint32_t range_lps;
    int32_t lps_mask;
    int log2_renorm;

    symbol_mask = (int16_t)state >> 7;
    state ^= symbol_mask;

    #if 0
    range_lps = ((state >> 2) * ((cabac_ctx->range /*& 0x1E0*/) >> 5) >> 1) + 4;
    #else
    range_lps = range_lps_lut[(cabac_ctx->range & 0x1E0) | (state >> 2)];
    #endif
    cabac_ctx->range -= range_lps;

    lps_mask   = (cabac_ctx->range << (NB_CABAC_BITS + 1)) - cabac_ctx->low_b - 1;
    lps_mask >>= 31;

    symbol_mask ^= lps_mask;

    state_0 -= (state_0 >> rate_0) & 0x7FE0;
    state_1 -= (state_1 >> rate_1) & 0x7FFE;
    state_0 += (0x7fffu >> rate_0) & 0x7FE0 & symbol_mask;
    state_1 += (0x7fffu >> rate_1) & 0x7FFE & symbol_mask;

    *cabac_state &= 0xFFFFFFFF;
    *cabac_state |= (uint64_t)state_0 << 48;
    *cabac_state |= (uint64_t)state_1 << 32;

    cabac_ctx->low_b -= (cabac_ctx->range << (NB_CABAC_BITS + 1)) & (lps_mask);
    cabac_ctx->range += (range_lps - cabac_ctx->range)         & (lps_mask);

    log2_renorm = lps_renorm_table[cabac_ctx->range >> 3];

    cabac_ctx->low_b <<= log2_renorm;
    cabac_ctx->range <<= log2_renorm;

    if (!(cabac_ctx->low_b & CABAC_MASK)){
        int num_bits = ov_ctz(cabac_ctx->low_b) - NB_CABAC_BITS;
        uint32_t tmp_fill = -CABAC_MASK;

        tmp_fill += cabac_ctx->bytestream[0] << 9;
        tmp_fill += cabac_ctx->bytestream[1] << 1;

        cabac_ctx->low_b += tmp_fill << num_bits;

        /* Last read will try to refill CABAC if last read occurs on
         * the bit just before alignment we will refill the CABAC
         * even if it was the last bit to be read in the entry
         * This is why we use <= instead of <.
         * Doing so permits to check for an error at the end
         * of each CTU line based on the position in the entry
         * Note that we can also check at the end of the entry
         * everything has been consumed.
         */
        if (cabac_ctx->bytestream <= cabac_ctx->bytestream_end){
            cabac_ctx->bytestream += NB_CABAC_BITS >> 3;
        } else {
            /* FIXME this permits to check if we needed to refill
             *  after end of entry
             */
#if 1
            cabac_ctx->bytestream = cabac_ctx->bytestream_end + 2;
            //printf("CABAC_EMPTY\n");
#endif
        }
    }
    return symbol_mask & 0x1;
}

static inline uint8_t
ovcabac_bypass_read(OVCABACCtx *const cabac_ctx)
{
  int32_t range, lps_mask;

  cabac_ctx->low_b <<= 1;

  if (!(cabac_ctx->low_b & CABAC_MASK)){
      int num_bits = 0;
      uint32_t tmp_fill = -CABAC_MASK;
      tmp_fill += cabac_ctx->bytestream[0] << 9;
      tmp_fill += cabac_ctx->bytestream[1] << 1;
      cabac_ctx->low_b += tmp_fill << num_bits;
      if (cabac_ctx->bytestream <= cabac_ctx->bytestream_end){
          cabac_ctx->bytestream += NB_CABAC_BITS >> 3;
      } else {
          /* FIXME this permits to check if we needed to refill
           *  after end of entry
           */
#if 1
          cabac_ctx->bytestream = cabac_ctx->bytestream_end + 2;
          //printf("CABAC_EMPTY\n");
#endif
      }
  }

  range = cabac_ctx->range << (NB_CABAC_BITS + 1);

  lps_mask = range - cabac_ctx->low_b - 1;
  lps_mask >>= 31;

  cabac_ctx->low_b -= range & lps_mask;

  return lps_mask & 0x1;
}
#endif

static inline uint8_t
ovcabac_bypass2_read(OVCABACCtx *const cabac_ctx, uint8_t nb_bits)
{
  int32_t lps_mask;

  int32_t range = cabac_ctx->range << (NB_CABAC_BITS + 1);
  int nb_cbits = ov_ctz(cabac_ctx->low_b) - NB_CABAC_BITS;

  if (nb_cbits <= nb_bits) {

      cabac_ctx->low_b <<= nb_bits;
  } else {
      cabac_ctx->low_b <<= nb_cbits;

  }

  if (!(cabac_ctx->low_b & CABAC_MASK)){
      uint32_t tmp_fill = -CABAC_MASK;
      tmp_fill += cabac_ctx->bytestream[0] << 9;
      tmp_fill += cabac_ctx->bytestream[1] << 1;
      cabac_ctx->low_b += tmp_fill << nb_bits;
      if (cabac_ctx->bytestream <= cabac_ctx->bytestream_end){
          cabac_ctx->bytestream += NB_CABAC_BITS >> 3;
      } else {
          /* FIXME this permits to check if we needed to refill
           *  after end of entry
           */
#if 1
          cabac_ctx->bytestream = cabac_ctx->bytestream_end + 2;
          //printf("CABAC_EMPTY\n");
#endif
      }
  }


  lps_mask = range - cabac_ctx->low_b - 1;
  lps_mask >>= 31;

  cabac_ctx->low_b -= range & lps_mask;

  return lps_mask & 0x1;
}


/* FIXME only used by mip_idx */
static inline uint8_t
vvc_get_cabac_truncated(OVCABACCtx *const cabac_ctx, unsigned int max_symbol){
    /* MAX SYMBOL will not be > 16 */
    static const uint8_t threshold_lut[257] = {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        8
    };
    int threshold = threshold_lut[max_symbol];
    uint32_t val = 0;

    int suffix = (1 << (threshold + 1)) - max_symbol;

    while (threshold--) {
        val <<= 1;
        val |= ovcabac_bypass_read(cabac_ctx);
    }

    if (val >= suffix) {
        uint8_t bit = ovcabac_bypass_read(cabac_ctx);
        val <<= 1;
        val |= bit;
        val -= suffix;
    }

    return val;
}
