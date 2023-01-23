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

#ifndef DRV_UTILS_H
#define DRV_UTILS_H

/*
 * This header is intended for macros and/or inline functions
 * used for derivation operation such as bit fields manipulations
 * etc.
 */

/*FIXME Must be included after ctudec*/

/* Set bit fields to 1 for a PB at coordinates x_pb y_pb
 */
static inline void
ctu_field_set_rect_bitfield(struct CTUBitField *const ctu_map,
                            uint8_t x_pb, uint8_t y_pb,
                            uint8_t nb_pb_w, uint8_t nb_pb_h)
{
    uint64_t mask;
    uint64_t mask_map;
    uint8_t pb_pos_shift;
    int i;

    mask = ((uint64_t)1 << (nb_pb_w)) - 1;
    pb_pos_shift = 1 + x_pb;
    mask_map = mask << pb_pos_shift;

    for (i = 1; i < nb_pb_h + 1; i++) {
        ctu_map->hfield[y_pb + i] |= mask_map;
    }

    mask = ((uint64_t)1 << (nb_pb_h)) - 1;
    pb_pos_shift = 1 + y_pb;
    mask_map = mask << pb_pos_shift;

    for (i = 1; i < nb_pb_w + 1; i++) {
        ctu_map->vfield[x_pb + i] |= mask_map;
    }
}

#define GEO_MIN_CU_LOG2         3
#define GEO_MAX_CU_LOG2         6
#define GEO_MAX_CU_SIZE         (1 << GEO_MAX_CU_LOG2)
#define GEO_NUM_CU_SIZE         ((GEO_MAX_CU_LOG2 - GEO_MIN_CU_LOG2) + 1)
#define GEO_NUM_PARTITION_MODE  64
#define GEO_NUM_ANGLES          32
#define GEO_NUM_PRESTORED_MASK  6
#define GEO_WEIGHT_MASK_SIZE    (3 * (GEO_MAX_CU_SIZE >> 3) * 2 + GEO_MAX_CU_SIZE)

extern const int16_t   g_GeoParams[GEO_NUM_PARTITION_MODE][2];
extern const int8_t    g_Dis[GEO_NUM_ANGLES];
extern uint8_t g_globalGeoWeights [GEO_NUM_PRESTORED_MASK][GEO_WEIGHT_MASK_SIZE * GEO_WEIGHT_MASK_SIZE];

extern const int8_t g_weightOffset_x     [GEO_NUM_PARTITION_MODE][GEO_NUM_CU_SIZE][GEO_NUM_CU_SIZE];
extern const int8_t g_weightOffset_y     [GEO_NUM_PARTITION_MODE][GEO_NUM_CU_SIZE][GEO_NUM_CU_SIZE];


#define MMVD_REFINE_STEP        8 ///< max number of distance step
#define MMVD_MAX_REFINE_NUM     (MMVD_REFINE_STEP * 4) ///< max number of candidate from a base candidate
#define MMVD_BASE_MV_NUM        2 ///< max number of base candidate

enum MvPrecision
{
  MV_PRECISION_4PEL     = 0,      // 4-pel
  MV_PRECISION_INT      = 2,      // 1-pel, shift 2 bits from 4-pel
  MV_PRECISION_HALF     = 3,      // 1/2-pel
  MV_PRECISION_QUARTER  = 4,      // 1/4-pel (the precision of regular MV difference signaling), shift 4 bits from 4-pel
  MV_PRECISION_SIXTEENTH = 6,     // 1/16-pel (the precision of internal MV), shift 6 bits from 4-pel
  MV_PRECISION_INTERNAL = MV_PRECISION_SIXTEENTH,
};


/* FIXME think to reset this in case of ctu size changes? */
static inline void
init_ctu_bitfield(struct OVRCNCtx *const rcn_ctx,
                  uint8_t ctb_ngh_flags, uint8_t log2_ctb_s)
{

    uint8_t nb_ctb_pb = (1 << (log2_ctb_s & 0x7)) >> 2;

    struct CTUBitField *const map_l = &rcn_ctx->progress_field;
    struct CTUBitField *const map_c = &rcn_ctx->progress_field_c;
    int i;

    /* Mask with inner ctb line / column set to zero */
    uint64_t internal_mask = ~(((1llu << nb_ctb_pb) - 1llu) << 1);
    uint64_t tr_mask = ~((1llu << (nb_ctb_pb)) - 1llu);

    uint64_t internal_mask_h = internal_mask;
    uint64_t internal_mask_v = internal_mask;

    uint64_t lft_mask = !!(ctb_ngh_flags & CTU_LFT_FLG);
    uint64_t abv_mask = !!(ctb_ngh_flags & CTU_UP_FLG);
    uint64_t tr = !!(ctb_ngh_flags & CTU_UPRGT_FLG);
    /* Remove first bit if ctb_left/above from mask is not available
     * This way we reset the field whenever left CTU is unavailable
     */
    internal_mask_h &= lft_mask;
    internal_mask_v &= abv_mask;

    for (i = 1; i < nb_ctb_pb + 1; ++i) {
        /* Set internal bits to zero */
        #if 0
        map_l->hfield[i] &= internal_mask_h;
        map_c->hfield[i] &= internal_mask_h;
        map_l->vfield[i] &= internal_mask_v;
        map_c->vfield[i] &= internal_mask_v;
        #else
        map_l->hfield[i] = 0;
        map_c->hfield[i] = 0;
        map_l->vfield[i] = 0;
        map_c->vfield[i] = 0;
        #endif

        /* Set internal bits according to CTU availability */
        map_l->hfield[i] |= lft_mask;
        map_c->hfield[i] |= lft_mask;
        map_l->vfield[i] |= abv_mask;
        map_c->vfield[i] |= abv_mask;
    }

    /* FIXME we consider Top Left available if 
     * both above and left CTU are available
     * this can be false in the case of non rectangular
     * slices.
     */
    map_l->hfield[0] = (~internal_mask) & (-(int64_t)abv_mask);
    map_c->hfield[0] = (~internal_mask) & (-(int64_t)abv_mask);
    map_l->vfield[0] = (~internal_mask) & (-(int64_t)lft_mask);
    map_c->vfield[0] = (~internal_mask) & (-(int64_t)lft_mask);

    map_l->hfield[0] |= abv_mask & lft_mask;
    map_c->hfield[0] |= abv_mask & lft_mask;
    map_l->vfield[0] |= abv_mask & lft_mask;
    map_c->vfield[0] |= abv_mask & lft_mask;

    /* Set Top Right Part if up right CTU is available */

    map_l->hfield[0] |= ((~tr_mask) << (nb_ctb_pb + 1)) & (-(int64_t)tr);
    map_c->hfield[0] |= ((~tr_mask) << (nb_ctb_pb + 1)) & (-(int64_t)tr);
}

static inline void
init_ctu_bitfield_border(struct OVRCNCtx *const rcn_ctx,
                         uint8_t ctb_ngh_flags, uint8_t log2_ctb_s,
                         uint8_t rem_w, uint8_t rem_h)
{

    uint8_t nb_ctb_pb = (1 << (log2_ctb_s & 0x7)) >> 2;

    uint8_t nb_ctb_pb_h = rem_w >> 2;
    uint8_t nb_ctb_pb_v = rem_h >> 2;

    struct CTUBitField *const map_l = &rcn_ctx->progress_field;
    struct CTUBitField *const map_c = &rcn_ctx->progress_field_c;
    int i;

    uint64_t internal_mask_h = ~(((1llu << nb_ctb_pb_h) - 1llu) << 1);
    uint64_t internal_mask_v = ~(((1llu << nb_ctb_pb_v) - 1llu) << 1);
    uint64_t tr_mask = ~((1llu << (nb_ctb_pb)) - 1llu);

    uint64_t lft_mask = !!(ctb_ngh_flags & CTU_LFT_FLG);
    uint64_t abv_mask = !!(ctb_ngh_flags & CTU_UP_FLG);

    uint64_t tr = !!(ctb_ngh_flags & CTU_UPRGT_FLG);

    /* Remove first bit if ctb_left/above from mask is not available
     * This way we reset the field whenever left CTU is unavailable
     */
    #if 1
    internal_mask_h &= ~lft_mask;
    internal_mask_v &= ~abv_mask;
    #endif

    for (i = 1; i < nb_ctb_pb + 1; ++i) {
        /* Set internal bits to zero */
        map_l->hfield[i] = 0;
        map_c->hfield[i] = 0;
        map_l->vfield[i] = 0;
        map_c->vfield[i] = 0;
    }

    for (i = 1; i < nb_ctb_pb_v + 1; ++i) {
        /* Set internal bits according to CTU availability */
        map_l->hfield[i] |= lft_mask;
        map_c->hfield[i] |= lft_mask;
    }

    for (i = 1; i < nb_ctb_pb_h + 1; ++i) {
        /* Set internal bits according to CTU availability */
        map_l->vfield[i] |= abv_mask;
        map_c->vfield[i] |= abv_mask;
    }

    /* FIXME Top Left
     */
    map_l->hfield[0] = (~internal_mask_h) & (-(int64_t)abv_mask);
    map_c->hfield[0] = (~internal_mask_h) & (-(int64_t)abv_mask);
    map_l->vfield[0] = (~internal_mask_v) & (-(int64_t)lft_mask);
    map_c->vfield[0] = (~internal_mask_v) & (-(int64_t)lft_mask);

    map_l->hfield[0] |= abv_mask & lft_mask;
    map_c->hfield[0] |= abv_mask & lft_mask;
    map_l->vfield[0] |= abv_mask & lft_mask;
    map_c->vfield[0] |= abv_mask & lft_mask;

    /* Set Top Right Part if up right CTU is available */

    map_l->hfield[0] |= ((~tr_mask) << (nb_ctb_pb + 1)) & (-(int64_t)tr);
    map_c->hfield[0] |= ((~tr_mask) << (nb_ctb_pb + 1)) & (-(int64_t)tr);
}

#endif
