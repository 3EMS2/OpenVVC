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

#include <string.h>
#include <stddef.h>
#include "overror.h"

#include "ovutils.h"
#include "ovmem.h"
#include "ovunits.h"

#include "nvcl.h"
#include "nvcl_utils.h"
#include "nvcl_structures.h"
#include "nvcl_private.h"
#include "hls_structures.h"

enum SliceType {
     B = 0,
     P = 1,
     I = 2
};

static int
validate_sh(OVNVCLReader *rdr, const union HLSData *const data)
{
    const OVSH *const sh = (const OVSH *)&data->sh;
    if (sh->sh_slice_type > 3) return OVVC_EINDATA;

    return 0;
}

static void
free_sh(const union HLSData *const data)
{
    const OVSH *const sh = (const OVSH *)&data->sh;
    ov_free((void *)sh);
}

static struct HLSDataRef **
storage_in_nvcl_ctx(OVNVCLReader *const rdr, OVNVCLCtx *const nvcl_ctx)
{
    struct HLSDataRef **storage = &nvcl_ctx->sh;

    return storage;
}

static void
hls_replace_ref(const struct HLSReader *const hls_hdl, struct HLSDataRef **storage, const union HLSData *const data)
{
    union HLSData *tmp = ov_malloc(hls_hdl->data_size);
    if (!tmp) {
        return;
    }
    memcpy(tmp, data, hls_hdl->data_size);

    hlsdata_unref(storage);
    *storage = hlsdataref_create(tmp, NULL, NULL);

    if (!*storage) {
        return;
    }
}

const struct HLSReader sh_manager =
{
    .name = "Slice Header",
    .data_size    = sizeof(struct OVSH),
    .find_storage = &storage_in_nvcl_ctx,
    .read         = &nvcl_sh_read,
    .validate     = &validate_sh,
    .free         = &free_sh
};

extern const struct HLSReader ph_manager;

static int
nvcl_decode_ph(OVNVCLReader *const rdr, OVNVCLCtx *const nvcl_ctx,
               const struct HLSReader *const hls_hdl, uint8_t nalu_type)
{
    struct HLSDataRef **storage = hls_hdl->find_storage(rdr, nvcl_ctx);
    union HLSData data;
    int ret;

    /* FIXME PH cannot be identical since it cntains POC info */
    if (*storage) {
        /* TODO compare RBSP data to avoid new read */
        uint8_t identical_rbsp = 0;
        if (identical_rbsp) goto duplicated;
    }

    memset(&data, 0, hls_hdl->data_size);

    ov_log(NULL, OVLOG_TRACE, "Reading new %s\n", hls_hdl->name);

    ret = hls_hdl->read(rdr, &data, nvcl_ctx, nalu_type);
    if (ret < 0)  goto failread;

    ret = hls_hdl->validate(rdr, &data);
    if (ret < 0)  goto invalid;

    hls_replace_ref(hls_hdl, storage, &data);

    return ret;

invalid:
    ov_log(NULL, OVLOG_ERROR, "Invalid %s\n", hls_hdl->name);
    return ret;

failread:
    ov_log(NULL, OVLOG_ERROR, "Error while reading %s\n", hls_hdl->name);
    return ret;

duplicated:
    ov_log(NULL, OVLOG_TRACE, "Ignored duplicated %s\n", hls_hdl->name);
    return 0;
}

static void
pred_weight_table_monochrome_sh(OVNVCLReader *const rdr, struct RPLWeightInfo *const wgt_info, uint8_t nb_active_refs0, uint8_t nb_active_refs1)
{
    int i;
    wgt_info->num_l0_weights = nb_active_refs0;
    wgt_info->num_l1_weights = nb_active_refs1;

    wgt_info->luma_log2_weight_denom = nvcl_read_u_expgolomb(rdr);

    for (i = 0; i < nb_active_refs0; i++) {
        wgt_info->luma_weight_l0_flag[i] = nvcl_read_flag(rdr);;
    }

    for (i = 0; i < nb_active_refs0; i++) {
        if (wgt_info->luma_weight_l0_flag[i]) {
            wgt_info->delta_luma_weight_l0[i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->luma_offset_l0[i]       = nvcl_read_s_expgolomb(rdr);
        }
    }

    for (i = 0; i < nb_active_refs1; i++) {
        wgt_info->luma_weight_l1_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < nb_active_refs1; i++) {
        if (wgt_info->luma_weight_l1_flag[i]) {
            wgt_info->delta_luma_weight_l1[i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->luma_offset_l1[i]       = nvcl_read_s_expgolomb(rdr);
        }
    }
}

static void
pred_weight_table_sh(OVNVCLReader *const rdr, struct RPLWeightInfo *const wgt_info, uint8_t nb_active_refs0, uint8_t nb_active_refs1)
{
    int i;
    wgt_info->num_l0_weights = nb_active_refs0;
    wgt_info->num_l1_weights = nb_active_refs1;

    wgt_info->luma_log2_weight_denom         = nvcl_read_u_expgolomb(rdr);
    wgt_info->delta_chroma_log2_weight_denom = nvcl_read_s_expgolomb(rdr);

    /* FIXME better flags storage */
    for (i = 0; i < nb_active_refs0; i++) {
        wgt_info->luma_weight_l0_flag[i] = nvcl_read_flag(rdr);;
    }

    for (i = 0; i < nb_active_refs0; i++) {
        wgt_info->chroma_weight_l0_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < nb_active_refs0; i++) {
        if (wgt_info->luma_weight_l0_flag[i]) {
            wgt_info->delta_luma_weight_l0[i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->luma_offset_l0[i]       = nvcl_read_s_expgolomb(rdr);
        }
        if (wgt_info->chroma_weight_l0_flag[i]) {
            wgt_info->delta_chroma_weight_l0[0][i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->delta_chroma_offset_l0[0][i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->delta_chroma_weight_l0[1][i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->delta_chroma_offset_l0[1][i] = nvcl_read_s_expgolomb(rdr);
        }
    }

    for (i = 0; i < nb_active_refs1; i++) {
        wgt_info->luma_weight_l1_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < nb_active_refs1; i++) {
        wgt_info->chroma_weight_l1_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < nb_active_refs1; i++) {

        if (wgt_info->luma_weight_l1_flag[i]) {
            wgt_info->delta_luma_weight_l1[i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->luma_offset_l1[i]       = nvcl_read_s_expgolomb(rdr);
        }

        if (wgt_info->chroma_weight_l1_flag[i]) {
            wgt_info->delta_chroma_weight_l1[0][i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->delta_chroma_offset_l1[0][i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->delta_chroma_weight_l1[1][i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->delta_chroma_offset_l1[1][i] = nvcl_read_s_expgolomb(rdr);
        }
    }
}
#if 0
static int
add_ctb_to_slice(slice_idx, start_x, stop_x, start_y, stop_y)
{
    for(ctb_y = start_y; ctb_y < stop_y; ctb_y++ ) {
        for(ctb_x = start_x; ctb_x < stop_x; ctb_x++ ) {
            CtbAddrInSlice[slice_idx][NumCtusInSlice[slice_idx]] = ctb_y * nb_ctb_pic_w + ctb_x;
            NumCtusInSlice[slice_idx]++;
        }
    }
}
static int
bal()
{
    int i, j, k, l;
    if (pps->pps_single_slice_per_subpic_flag) {
        if (!sps->sps_subpic_info_present_flag) {
            /* There is no subpicture info and only one slice in a picture. */
            /* => TILES_IN_SLICE */
            for (j = 0; j < nb_tile_rows; j++) {
                for (i = 0; i < nb_tile_cols; i++) {
                    add_ctb_to_slice(0, tile_ctb_x[i], tile_ctb_x[i + 1],
                                        tile_ctb_y[j], tile_ctb_y[j + 1]);
                }
            }
        } else {
            /* => MULTIPLE SUBPICTURES ONE SLICE PER SUBPIC */
            for (i = 0; i <= sps->sps_num_subpics_minus1; i++) {
                if (subpicHeightLessThanOneTileFlag[i]) {
                    /* The slice consists of a set of CTU rows in a tile. */
                    /* SUB PIC IN TILE */
                    add_ctb_to_slice(i, sps_subpic_ctu_top_left_x[i], sps_subpic_ctu_top_left_x[i] + sps_subpic_width_minus1[i] + 1,
                                        sps_subpic_ctu_top_left_y[i], sps_subpic_ctu_top_left_y[i] + sps_subpic_height_minus1[i] + 1);
                } else {
                    /* The slice consists of a number of complete tiles covering a rectangular region. */
                    /* TILES IN SUB PIC*/
                    uint16_t tile_x = ctbToTileColIdx[sps_subpic_ctu_top_left_x[i]];
                    uint16_t tile_y = ctbToTileRowIdx[sps_subpic_ctu_top_left_y[i]];
                    for (j = 0; j < SubpicHeightInTiles[i]; j++) {
                        for (k = 0; k < SubpicWidthInTiles[i]; k++) {
                            add_ctb_to_slice(i, tile_ctb_x[tile_x + k], tile_ctb_x[tile_x + k + 1],
                                                tile_ctb_y[tile_y + j], tile_ctb_y[tile_y + j + 1]);
                        }
                    }
                }
            }
        }
    } else {
        uint16_t tile_idx = 0;
        for (i = 0; i <= pps->pps_num_slices_in_pic_minus1; i++) {

            uint16_t tile_x = tile_idx % nb_tile_cols;
            uint16_t tile_y = tile_idx / nb_tile_cols;

            if (i < pps->pps_num_slices_in_pic_minus1) {
                slice_tile_w[i] = pps_slice_width_in_tiles_minus1[i] + 1;
                slice_tile_h[i] = pps_slice_height_in_tiles_minus1[i] + 1;
            } else {
                /* implicit last slice dimension */
                slice_tile_w[i] = nb_tile_cols - tile_x;
                slice_tile_h[i] = nb_tile_rows - tile_y;
                NumSlicesInTile[i] = 1;
            }

            if (slice_tile_w[i] == 1 && slice_tile_h[i] == 1) {

                if (pps->pps_num_exp_slices_in_tile[i] == 0) {
                    /* exactly one tile in slice */
                    slice_ctb_h[i] = tile_ctb_h[tile_idx / nb_tile_cols];
                    NumSlicesInTile[i] = 1;
                } else {
                    /* EXPLICIT RECTANGLE SLICES IN TILES */
                    /* Fill next slices up to last slice in tile and increase slice_index */
                    int16_t rem_ctb_h = tile_ctb_h[tile_idx / nb_tile_cols];

                    for (j = 0; j < pps->pps_num_exp_slices_in_tile[i]; j++) {
                        slice_ctb_h[i + j] = pps->pps_exp_slice_height_in_ctus_minus1[i + j] + 1;
                        rem_ctb_h -= slice_ctb_h[i + j];
                    }

                    /* Implicit last slices rectangles */
                    uniform_slice_ctb_h = slice_ctb_h[i + j - 1];

                    while (rem_ctb_h >= uniform_slice_ctb_h) {
                        slice_ctb_h[i + j] = uniform_slice_ctb_h;
                        rem_ctb_h -= uniform_slice_ctb_h;
                        j++;
                    }

                    if (rem_ctb_h > 0) {
                        slice_ctb_h[i + j] = rem_ctb_h;
                        j++;
                    }

                    NumSlicesInTile[i] = j;
                }

                uint16_t ctb_y = tile_ctb_y[tile_y];
                for (j = 0; j < NumSlicesInTile[i]; j++) {
                    add_ctb_to_slice(i + j, tile_ctb_x[tile_x], tile_ctb_x[tile_x + 1],
                                     ctb_y, ctb_y + slice_ctb_h[i + j]);
                    ctb_y += slice_ctb_h[i + j];
                    slice_tile_w[i + j] = 1;
                    slice_tile_h[i + j] = 1;
                }

                i += NumSlicesInTile[i] - 1;
            } else {
                /* EXPLICIT TILES RECTANGLES in SLICES W H  */
                for (j = 0; j < slice_tile_h[i]; j++) {
                    for (k = 0; k < slice_tile_w[i]; k++) {
                        add_ctb_to_slice(i, tile_ctb_x[tile_x + k], tile_ctb_x[tile_x + k + 1],
                                            tile_ctb_y[tile_y + j], tile_ctb_y[tile_y + j + 1]);
                    }
                }
            }

            /* Tile delta idx resolution */
            if (i < pps->pps_num_slices_in_pic_minus1) {
                if (pps->pps_tile_idx_delta_present_flag) {
                    tile_idx += pps->pps_tile_idx_delta_val[i];
                } else {
                    tile_idx += slice_tile_w[i];
                    if (tile_idx % nb_tile_cols == 0) {
                        tile_idx += (slice_tile_h[i] - 1) * nb_tile_cols;
                    }
                }
            }
        }
    }
}

static int
derive_nb_slice_in_subpic(const OVSPS *const sps, const OVPPS *const pps, uint16_t i)
{
    int j;
    int nb_slices_in_subpic = 0;
    for (j = 0; j <= pps->pps_num_slices_in_pic_minus1; j++) {
        uint16_t ctu_x = CtbAddrInSlice[j][0] % pic_ctb_w;
        uint16_t ctu_y = CtbAddrInSlice[j][0] / pic_ctb_w;
        if ((ctu_x >= sps->sps_subpic_ctu_top_left_x[i]) &&
            (ctu_x  < sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1) &&
            (ctu_y >= sps->sps_subpic_ctu_top_left_y[i]) &&
            (ctu_y  < sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1)) {
            nb_slices_in_subpic++;
        }
    }
}
#endif

struct SliceInfo
{
    uint16_t tile_idx;
    uint16_t nb_tiles;

    uint16_t ctb_x;
    uint16_t ctb_y;
    uint16_t w;
    uint16_t h;
};

static void
init_tile_ctx(struct TileInfo *const tinfo, const OVPPS *const pps)
{
    int nb_cols = pps->part_info.nb_tile_w;
    int nb_rows = pps->part_info.nb_tile_h;

    int rem_ctu_w = pps->part_info.nb_ctu_w;
    int rem_ctu_h = pps->part_info.nb_ctu_h;

    int i;
    for (i = 1; i <=  nb_rows; ++i) {
        const int tile_nb_ctu_h = pps->part_info.tile_row_h[i - 1];
        tinfo->ctu_y[i - 1] = pps->part_info.nb_ctu_h - rem_ctu_h;
        tinfo->nb_ctu_h[i - 1] = tile_nb_ctu_h;
        rem_ctu_h -= tile_nb_ctu_h;
    }

    for (i = 1; i <= nb_cols; ++i) {
        const int tile_nb_ctu_w = pps->part_info.tile_col_w[i - 1];
        tinfo->ctu_x[i - 1] = pps->part_info.nb_ctu_w - rem_ctu_w;
        tinfo->nb_ctu_w[i - 1] = tile_nb_ctu_w;
        rem_ctu_w -= tile_nb_ctu_w;
    }

    tinfo->nb_tile_cols = nb_cols ;
    tinfo->nb_tile_rows = nb_rows ;
}

static void
setup_subpic_prms(const OVSPS *const sps, struct PicPartitionInfo *const part_info, const struct TileInfo *const tinfo, uint8_t log2_ctb_s)
{
    int pic_w = sps->sps_pic_width_max_in_luma_samples;
    int pic_h = sps->sps_pic_height_max_in_luma_samples;
    int nb_ctu_w = (pic_w + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
    int nb_ctu_h = (pic_h + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;

    int subpic_w = sps->sps_subpic_width_minus1[0]  + 1;
    int subpic_h = sps->sps_subpic_height_minus1[0] + 1;

    if (part_info->nb_subpics) return;
    part_info->nb_subpics = sps->sps_num_subpics_minus1 + 1;

    if (sps->sps_subpic_same_size_flag) {
        int nb_subpic_w = (nb_ctu_w / subpic_w) + !!(nb_ctu_w % subpic_w);

        int subpic_x = sps->sps_subpic_ctu_top_left_x[0];
        int subpic_y = sps->sps_subpic_ctu_top_left_y[0];
        int i;

        for (i = 0; i <= (sps->sps_num_subpics_minus1 & 0xF); i++) {
            //printf ("Subpicture %d : (%d, %d) %dx%d\n", i, subpic_x, subpic_y, subpic_w, subpic_h);
            part_info->subpictures[i].x = subpic_x;
            part_info->subpictures[i].y = subpic_y;
            part_info->subpictures[i].w = subpic_w;
            part_info->subpictures[i].h = subpic_h;
            //part_info->subpictures[i].nb_slices = 0;
            subpic_x += subpic_w;
            if (!((i + 1) % nb_subpic_w)) {
                subpic_y += subpic_h;
                subpic_x = 0;
            }
        }

    } else {

        int subpic_x = sps->sps_subpic_ctu_top_left_x[0];
        int subpic_y = sps->sps_subpic_ctu_top_left_y[0];
        int i;
        for (i = 0; i < (sps->sps_num_subpics_minus1 & 0xF); i++) {
            subpic_x = sps->sps_subpic_ctu_top_left_x[i];
            subpic_y = sps->sps_subpic_ctu_top_left_y[i];
            subpic_w = sps->sps_subpic_width_minus1[i] + 1;
            subpic_h = sps->sps_subpic_height_minus1[i] + 1;

            //printf ("Subpicture %d : (%d, %d) %dx%d\n", i, subpic_x, subpic_y, subpic_w, subpic_h);

            part_info->subpictures[i].x = subpic_x;
            part_info->subpictures[i].y = subpic_y;
            part_info->subpictures[i].w = subpic_w;
            part_info->subpictures[i].h = subpic_h;
            //part_info->subpictures[i].nb_slices = 0;
        }

        subpic_x = sps->sps_subpic_ctu_top_left_x[i];
        subpic_y = sps->sps_subpic_ctu_top_left_y[i];
        subpic_w = nb_ctu_w - subpic_x;
        subpic_h = nb_ctu_h - subpic_y;

        part_info->subpictures[i].x = subpic_x;
        part_info->subpictures[i].y = subpic_y;
        part_info->subpictures[i].w = subpic_w;
        part_info->subpictures[i].h = subpic_h;
        //part_info->subpictures[i].nb_slices = 0;
        //printf ("Subpicture %d : (%d, %d) %dx%d\n", i, subpic_x, subpic_y, subpic_w, subpic_h);
    }

}
static int
find_subpic_id(const struct PicPartitionInfo *const part_info, uint8_t ctu_x, uint8_t ctu_y)
{
    for (int i = 0; i <= part_info->nb_subpics; i++) {
        const struct SubpicInfo *subpic = &part_info->subpictures[i];

        if ((ctu_x >= subpic->x && ctu_x < subpic->x + subpic->w) &&
            (ctu_y >= subpic->y && ctu_y < subpic->y + subpic->h)) {
            return i;
        }
    }
    return 0;
}

static void
setup_slice_prms(const OVPPS *const pps, struct PicPartitionInfo *const part_info, struct TileInfo *const tinfo, uint8_t log2_ctb_s)
{
    int tile_id = 0;
    int i;

    if (part_info->nb_entries) return;

    if (!tinfo->nb_tile_cols) tinfo->nb_tile_cols = 1;
    if (!tinfo->nb_tile_rows) tinfo->nb_tile_rows = 1;

    part_info->nb_tile_w = tinfo->nb_tile_cols;
    part_info->nb_tile_h = tinfo->nb_tile_rows;

    part_info->nb_slices = pps->pps_single_slice_per_subpic_flag ? part_info->nb_subpics : pps->pps_rect_slice_flag ? pps->pps_num_slices_in_pic_minus1 + 1 : 1;
    for (i = 0; i < part_info->nb_slices; i++) {
        int tile_x = tile_id % tinfo->nb_tile_cols;
        int tile_y = tile_id / tinfo->nb_tile_cols;

        int sl_w_tile = pps->pps_num_slices_in_pic_minus1 ? pps->pps_slice_width_in_tiles_minus1[i] + 1 : tinfo->nb_tile_cols;
        int sl_h_tile = pps->pps_num_slices_in_pic_minus1 ? pps->pps_slice_height_in_tiles_minus1[i] + 1 : tinfo->nb_tile_rows;

        int nb_tiles_entries = pps->pps_single_slice_per_subpic_flag ? 1 : sl_w_tile * sl_h_tile;
        if (part_info->nb_slices > 1 && pps->pps_single_slice_per_subpic_flag) {
            sl_w_tile = 1;
            sl_h_tile = 1;
        }

        for (int j = 0; j < sl_h_tile; ++j) {
            for (int k = 0; k < sl_w_tile; ++k) {
                int tmp_tile_x = tile_x + k;
                int tmp_tile_y = tile_y + j;
                int tmp_tile_id = tile_id + k + j * tinfo->nb_tile_cols;

                int x = tinfo->ctu_x[tmp_tile_x];
                int y = tinfo->ctu_y[tmp_tile_y];
                int w = tinfo->nb_ctu_w[tmp_tile_x];
                int h = tinfo->nb_ctu_h[tmp_tile_y];

                if (nb_tiles_entries == 1 && pps->pps_num_exp_slices_in_tile[tmp_tile_id]) {
                    int nb_exp_slices = pps->pps_num_exp_slices_in_tile[tmp_tile_id];
                    int pos_y = y;
                    for (int l = 0; l < nb_exp_slices; l++) {
                        int tmp_h = pps->pps_exp_slice_height_in_ctus_minus1[i + l] + 1;
                        int subpic_id = find_subpic_id(&pps->part_info, x, pos_y);

                        part_info->subpictures[subpic_id].nb_slices++;

                        part_info->entries[part_info->nb_entries].x = x;
                        part_info->entries[part_info->nb_entries].y = pos_y;
                        part_info->entries[part_info->nb_entries].w = w;
                        part_info->entries[part_info->nb_entries].h = tmp_h;

                        part_info->entries[part_info->nb_entries].slice_id = i + l;
                        part_info->entries[part_info->nb_entries].subpic_id = subpic_id;

                        part_info->slices[i + l].entry_idx = part_info->nb_entries;

                        pos_y += pps->pps_exp_slice_height_in_ctus_minus1[i + l] + 1;
                        if (!pps->pps_single_slice_per_subpic_flag) {
                            part_info->slices[i + l].subpic_id = subpic_id;
                            part_info->slices[i + l].nb_entries = 1;
                        } else {
                            part_info->slices[i].subpic_id = subpic_id;
                            part_info->slices[i].nb_entries = 1;
                        }
                        part_info->nb_entries++;
                    }

                    i += nb_exp_slices;

                    int rem_h = h - (pos_y - y);
                    if (rem_h) {
                        int last_read = pps->pps_exp_slice_height_in_ctus_minus1[i - 1] + 1;
                        int nb_implicit_slices = rem_h / last_read + !!(rem_h % last_read);
                        for (int l = 0; l < nb_implicit_slices; l++) {
                            int rem_h = h - (pos_y - y);
                            int subpic_id = find_subpic_id(&pps->part_info, x, pos_y);
                            int implicit_h = OVMIN(last_read, rem_h);

                            part_info->subpictures[subpic_id].nb_slices++;

                            part_info->entries[part_info->nb_entries].x = x;
                            part_info->entries[part_info->nb_entries].y = pos_y;
                            part_info->entries[part_info->nb_entries].w = w;
                            part_info->entries[part_info->nb_entries].h = implicit_h;

                            part_info->entries[part_info->nb_entries].slice_id = i + l;
                            part_info->entries[part_info->nb_entries].subpic_id = subpic_id;

                            part_info->slices[i + l].subpic_id = subpic_id;
                            part_info->slices[i + l].nb_entries = 1;
                            part_info->slices[i + l].entry_idx = part_info->nb_entries;

                            pos_y += implicit_h;

                            part_info->nb_entries++;
                        }
                        i += nb_implicit_slices;
                    }

                    --i;

                    //i += nb_implicit_slices + nb_exp_slices - 1;

                } else {
                    int subpic_id =  find_subpic_id(&pps->part_info, x, y);

                    part_info->entries[part_info->nb_entries].x = x;
                    part_info->entries[part_info->nb_entries].y = y;
                    part_info->entries[part_info->nb_entries].w = w;
                    part_info->entries[part_info->nb_entries].h = h;

                    part_info->entries[part_info->nb_entries].slice_id = i;
                    part_info->entries[part_info->nb_entries].subpic_id = subpic_id;

                    if (!j && !k) {
                        part_info->slices[i].entry_idx = part_info->nb_entries;
                        part_info->subpictures[subpic_id].nb_slices++;
                    }

                    part_info->slices[i].subpic_id = subpic_id;
                    part_info->slices[i].nb_entries++;

                    part_info->nb_entries++;
                }
            }
        }

        if (pps->pps_tile_idx_delta_present_flag && i < pps->pps_num_slices_in_pic_minus1) {
            tile_id += pps->pps_tile_idx_delta_val[i];
        } else {
            tile_id += pps->pps_slice_width_in_tiles_minus1[i] + 1;
            if (tile_id % tinfo->nb_tile_cols == 0) {
                tile_id += pps->pps_slice_height_in_tiles_minus1[i] * tinfo->nb_tile_cols;
            }
        }
    }
}

static void
setup_suppic_slice_map(struct PicPartitionInfo *part_info)
{
    uint16_t slice_map_id = 0;
    for (int subpic_id = 0; subpic_id < part_info->nb_subpics; subpic_id++) {
        struct SubpicInfo *subpic = &part_info->subpictures[subpic_id];
        subpic->map_offset = slice_map_id;
        for (int slice_id = 0; slice_id < part_info->nb_slices; slice_id++) {
            if (part_info->slices[slice_id].subpic_id == subpic_id) {
                part_info->slice_id[slice_map_id++] = slice_id;
            }
        }
        part_info->subpic_id[subpic_id] = subpic_id;
    }
}

static void
setup_subpic_id_map(struct PicPartitionInfo *part_info, const OVPPS *const pps, const OVSPS *const sps)
{
    if (sps->sps_subpic_id_mapping_explicitly_signalled_flag) {
        if (sps->sps_subpic_id_mapping_present_flag) {
            for (int subpic_id = 0; subpic_id < part_info->nb_subpics; subpic_id++) {
                part_info->subpic_id[subpic_id] = sps->sps_subpic_id[subpic_id];
            }
        } else if (pps->pps_subpic_id_mapping_present_flag) {
            for (int subpic_id = 0; subpic_id < part_info->nb_subpics; subpic_id++) {
                part_info->subpic_id[subpic_id] = pps->pps_subpic_id[subpic_id];
            }
        } else {
            for (int subpic_id = 0; subpic_id < part_info->nb_subpics; subpic_id++) {
                part_info->subpic_id[subpic_id] = subpic_id;
            }
        }
    } else {
        for (int subpic_id = 0; subpic_id < part_info->nb_subpics; subpic_id++) {
            part_info->subpic_id[subpic_id] = subpic_id;
        }
    }
    return;
}

static int16_t
map_subpic_id(const struct PicPartitionInfo *part_info, uint16_t sh_subpic_id)
{
    for (int subpic_id = 0; subpic_id < part_info->nb_subpics; subpic_id++) {
        if (sh_subpic_id == part_info->subpic_id[subpic_id]) {
            return subpic_id;
        }
    }
    ov_log(NULL, OVLOG_ERROR, "Invalid subpicture id %d .\n", sh_subpic_id);
    return 0;
}


int
nvcl_sh_read(OVNVCLReader *const rdr, OVHLSData *const hls_data,
             const OVNVCLCtx *const nvcl_ctx, uint8_t nalu_type)
{
    int i;
    struct TileInfo tinfo = {0};
    OVSH *sh = &hls_data->sh;
    OVPH *ph = NULL;
    const OVPPS *pps = NULL;
    const OVSPS *sps = NULL;

    sh->sh_picture_header_in_slice_header_flag = nvcl_read_flag(rdr);
    if (sh->sh_picture_header_in_slice_header_flag) {
        ov_log(NULL, OVLOG_DEBUG, "PH in SH%d\n", sh->sh_slice_address);
        int ret = nvcl_decode_ph(rdr, nvcl_ctx, &ph_manager, 0);
        if (ret < 0) {
            ov_log(NULL, 3, "Failed reading PH from SH\n");
            return OVVC_EINDATA;
        }
    }

    /* TODO create proper structure to hold activated parameters sets */
    if (nvcl_ctx->ph) {
        ph = (OVPH *)nvcl_ctx->ph->data;
        pps = (OVPPS *)nvcl_ctx->pps_list[ph->ph_pic_parameter_set_id]->data;
        sps = (OVSPS *)nvcl_ctx->sps_list[pps->pps_seq_parameter_set_id]->data;
    }

    if (!ph || !pps || !sps) {
        ov_log(NULL, 3, "Missing parameter sets while reading SH\n");
        return OVVC_EINDATA;
    }
    /* TODO Once other parameter sets are properly activated we can derive
     * convenience variables from them including :
     *    - Sub Pictures / Tiles contexts
     *    - Numbers of extra sh bits
     */
    init_tile_ctx(&tinfo, pps);
    uint8_t log2_ctb_s = 5 + sps->sps_log2_ctu_size_minus5;
    struct PicPartitionInfo *const part_info = &pps->part_info;
    setup_subpic_prms(sps, part_info, &tinfo, log2_ctb_s);
    setup_slice_prms(pps, part_info, &tinfo, log2_ctb_s);
    setup_suppic_slice_map(part_info);
    setup_subpic_id_map(part_info, pps, sps);

    int nb_slices_subpic = 1;
    if (sps->sps_subpic_info_present_flag) {
        uint8_t subpic_id_len = pps->pps_subpic_id_mapping_present_flag ? pps->pps_subpic_id_len_minus1 + 1 : sps->sps_subpic_id_len_minus1 + 1;
        sh->sh_subpic_id = nvcl_read_bits(rdr, subpic_id_len);
        uint16_t actual_subpic_id = sps->sps_subpic_id_mapping_explicitly_signalled_flag ? map_subpic_id(part_info, sh->sh_subpic_id) : sh->sh_subpic_id;
        nb_slices_subpic = pps->pps_single_slice_per_subpic_flag ? 1 : pps->part_info.subpictures[actual_subpic_id].nb_slices;
    } else {
        nb_slices_subpic = pps->pps_num_slices_in_pic_minus1 + 1;
    }

    int nb_tiles_pic = (uint16_t)pps->part_info.nb_tile_w * pps->part_info.nb_tile_h;

    uint8_t slice_in_tiles = !pps->pps_rect_slice_flag && nb_tiles_pic > 1;
    uint8_t rect_slice_in_tiles = pps->pps_rect_slice_flag && pps->pps_num_slices_in_pic_minus1;
    uint8_t slice_in_subpic = rect_slice_in_tiles || (pps->pps_rect_slice_flag && nb_slices_subpic > 1);
    uint8_t tiles_in_slice = !slice_in_tiles && nb_tiles_pic > 1;

    /* FIXME would be better to distinguish in two different branches */
    if (slice_in_tiles || (slice_in_subpic && nb_slices_subpic > 1)) {
        int nb_bits_in_slice_address = ov_ceil_log2(slice_in_subpic ? nb_slices_subpic : nb_tiles_pic);
        sh->sh_slice_address = nvcl_read_bits(rdr, nb_bits_in_slice_address);
        if (sh->sh_slice_address) {
            ov_log(NULL, OVLOG_TRACE, "Slice address %d , subpic_id %d, subpic idx %d\n", sh->sh_slice_address, sh->sh_subpic_id);
        }
    }
    uint16_t actual_subpic_id = sps->sps_subpic_id_mapping_explicitly_signalled_flag ? map_subpic_id(&pps->part_info, sh->sh_subpic_id) : sh->sh_subpic_id;
    ov_log(NULL, OVLOG_TRACE, "Read slice address %d , subpic_id %d, subpic idx %d\n", sh->sh_slice_address, sh->sh_subpic_id, actual_subpic_id);

    const struct SubpicInfo *subpic = &pps->part_info.subpictures[actual_subpic_id];
    uint16_t slice_address = sh->sh_slice_address;
    uint16_t slice_id =  pps->part_info.slice_id[subpic->map_offset + slice_address];
    struct SliceMap *slice = &part_info->slices[slice_id];
    int nb_extra_sh_bits = sps->sps_num_extra_sh_bytes * 8;

    for (i = 0; i < nb_extra_sh_bits; i++) {
        if (sps->sps_extra_sh_bit_present_flag[i])
            sh->sh_extra_bit[i] = nvcl_read_bits(rdr, 1);
    }

    if (!pps->pps_rect_slice_flag && nb_tiles_pic - sh->sh_slice_address > 1) {
        sh->sh_num_tiles_in_slice_minus1 = nvcl_read_u_expgolomb(rdr);
        slice->nb_entries = sh->sh_num_tiles_in_slice_minus1 + 1;
    } else if (pps->pps_num_slices_in_pic_minus1 > 1 && pps->pps_single_slice_per_subpic_flag && tiles_in_slice) {
        slice->nb_entries = nb_tiles_pic;
    }

    sh->sh_slice_type = I;
    if (ph->ph_inter_slice_allowed_flag) {
        sh->sh_slice_type = nvcl_read_u_expgolomb(rdr);
        if (sh->sh_slice_type > 3) return OVVC_EINDATA;
    }

    if (nalu_type == OVNALU_IDR_W_RADL ||
        nalu_type == OVNALU_IDR_N_LP   ||
        nalu_type == OVNALU_CRA        ||
        nalu_type == OVNALU_GDR) {
        sh->sh_no_output_of_prior_pics_flag = nvcl_read_flag(rdr);
    }

    if (sps->sps_alf_enabled_flag && !pps->pps_alf_info_in_ph_flag) {

        sh->sh_alf_enabled_flag = nvcl_read_flag(rdr);
        if (sh->sh_alf_enabled_flag) {

            sh->sh_num_alf_aps_ids_luma = nvcl_read_bits(rdr, 3);
            for (i = 0; i < sh->sh_num_alf_aps_ids_luma; i++) {
                sh->sh_alf_aps_id_luma[i] = nvcl_read_bits(rdr, 3);
            }

            if (sps->sps_chroma_format_idc) {
                sh->sh_alf_cb_enabled_flag = nvcl_read_flag(rdr);
                sh->sh_alf_cr_enabled_flag = nvcl_read_flag(rdr);

                if (sh->sh_alf_cb_enabled_flag || sh->sh_alf_cr_enabled_flag) {
                    sh->sh_alf_aps_id_chroma = nvcl_read_bits(rdr, 3);
                }

                if (sps->sps_ccalf_enabled_flag) {

                    sh->sh_alf_cc_cb_enabled_flag = nvcl_read_flag(rdr);
                    if (sh->sh_alf_cc_cb_enabled_flag) {
                        sh->sh_alf_cc_cb_aps_id = nvcl_read_bits(rdr, 3);
                    }

                    sh->sh_alf_cc_cr_enabled_flag = nvcl_read_flag(rdr);
                    if (sh->sh_alf_cc_cr_enabled_flag) {
                        sh->sh_alf_cc_cr_aps_id = nvcl_read_bits(rdr, 3);
                    }
                }
            }
        }
    }

    sh->sh_lmcs_used_flag = ph->ph_lmcs_enabled_flag && sh->sh_picture_header_in_slice_header_flag;
    if (ph->ph_lmcs_enabled_flag && !sh->sh_picture_header_in_slice_header_flag) {
        sh->sh_lmcs_used_flag = nvcl_read_flag(rdr);
    }

    if (ph->ph_explicit_scaling_list_enabled_flag && !sh->sh_picture_header_in_slice_header_flag) {
        sh->sh_explicit_scaling_list_used_flag = nvcl_read_flag(rdr);
    }

    if (!pps->pps_rpl_info_in_ph_flag &&
        ((nalu_type != OVNALU_IDR_W_RADL && nalu_type != OVNALU_IDR_N_LP) ||
         sps->sps_idr_rpl_present_flag)) {
         OVHRPL *const hrpl = &sh->hrpl;
         nvcl_read_header_ref_pic_lists(rdr, hrpl, sps, pps);
    }

    if (sh->sh_slice_type != I) {
        const OVHRPL *const hrpl = pps->pps_rpl_info_in_ph_flag ? &ph->hrpl : &sh->hrpl;

        int nb_ref_entries0 = hrpl->rpl0 ? hrpl->rpl_h0.rpl_data.num_ref_entries : 0;
        int nb_ref_entries1 = hrpl->rpl1 ? hrpl->rpl_h1.rpl_data.num_ref_entries : 0;

        if ((nb_ref_entries0 > 1) ||
            (sh->sh_slice_type == B && nb_ref_entries1 > 1)) {
            sh->sh_num_ref_idx_active_override_flag = nvcl_read_flag(rdr);
            if (sh->sh_num_ref_idx_active_override_flag) {
                if (nb_ref_entries0 > 1) {
                    sh->sh_num_ref_idx_active_l0_minus1 = nvcl_read_u_expgolomb(rdr);
                    nb_ref_entries0 = sh->sh_num_ref_idx_active_l0_minus1 + 1;
                }

                if (sh->sh_slice_type == B && nb_ref_entries1 > 1) {
                    sh->sh_num_ref_idx_active_l1_minus1 = nvcl_read_u_expgolomb(rdr);
                    nb_ref_entries1 = sh->sh_num_ref_idx_active_l1_minus1 + 1;
                }
            } else if (nb_ref_entries0 > pps->pps_num_ref_idx_default_active_minus1[0] ||
                       nb_ref_entries1 > pps->pps_num_ref_idx_default_active_minus1[1]) {

                if (nb_ref_entries0 > pps->pps_num_ref_idx_default_active_minus1[0]) {
                    nb_ref_entries0 = pps->pps_num_ref_idx_default_active_minus1[0] + 1;
                }

                if (sh->sh_slice_type == B &&
                    nb_ref_entries1 > pps->pps_num_ref_idx_default_active_minus1[1]) {
                    nb_ref_entries1 = pps->pps_num_ref_idx_default_active_minus1[1] + 1;
                }
            }
        }

        if (pps->pps_cabac_init_present_flag) {
            sh->sh_cabac_init_flag = nvcl_read_flag(rdr);
        }

        if (ph->ph_temporal_mvp_enabled_flag && !pps->pps_rpl_info_in_ph_flag) {
            sh->sh_collocated_from_l0_flag = 1;
            if (sh->sh_slice_type == B) {
                sh->sh_collocated_from_l0_flag = nvcl_read_flag(rdr);
            }

            if ((sh->sh_collocated_from_l0_flag && nb_ref_entries0 > 1) ||
                (sh->sh_slice_type == B && (!sh->sh_collocated_from_l0_flag && nb_ref_entries1 > 1))) {
                sh->sh_collocated_ref_idx = nvcl_read_u_expgolomb(rdr);
            }
        }

        if (!pps->pps_wp_info_in_ph_flag &&
            ((pps->pps_weighted_pred_flag && sh->sh_slice_type == P) ||
             (pps->pps_weighted_bipred_flag && sh->sh_slice_type == B))) {

            if (sps->sps_chroma_format_idc) {
                pred_weight_table_sh(rdr, &sh->wgt_info, nb_ref_entries0, (nb_ref_entries1 & -pps->pps_weighted_bipred_flag));
            } else {
                pred_weight_table_monochrome_sh(rdr, &ph->wgt_info, nb_ref_entries0, (nb_ref_entries1 & -pps->pps_weighted_bipred_flag));
            }

        }
    }

    if (!pps->pps_qp_delta_info_in_ph_flag) {
        sh->sh_qp_delta = nvcl_read_s_expgolomb(rdr);
    }

    if (pps->pps_slice_chroma_qp_offsets_present_flag) {
        sh->sh_cb_qp_offset = nvcl_read_s_expgolomb(rdr);
        sh->sh_cr_qp_offset = nvcl_read_s_expgolomb(rdr);
        if (sps->sps_joint_cbcr_enabled_flag) {
            sh->sh_joint_cbcr_qp_offset = nvcl_read_s_expgolomb(rdr);
        }
    }

    if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
        sh->sh_cu_chroma_qp_offset_enabled_flag = nvcl_read_flag(rdr);
    }

    if (sps->sps_sao_enabled_flag && !pps->pps_sao_info_in_ph_flag) {
        sh->sh_sao_luma_used_flag = nvcl_read_flag(rdr);
        if (sps->sps_chroma_format_idc) {
            sh->sh_sao_chroma_used_flag = nvcl_read_flag(rdr);
        }
    }

    if (pps->pps_deblocking_filter_override_enabled_flag && !pps->pps_dbf_info_in_ph_flag) {
        sh->sh_deblocking_params_present_flag = nvcl_read_flag(rdr);

        if (sh->sh_deblocking_params_present_flag) {
            if (!pps->pps_deblocking_filter_disabled_flag) {
                sh->sh_deblocking_filter_disabled_flag = nvcl_read_flag(rdr);
            }

            if (!sh->sh_deblocking_filter_disabled_flag) {
                sh->sh_luma_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                sh->sh_luma_tc_offset_div2   = nvcl_read_s_expgolomb(rdr);
                if (pps->pps_chroma_tool_offsets_present_flag) {
                    sh->sh_cb_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                    sh->sh_cb_tc_offset_div2   = nvcl_read_s_expgolomb(rdr);
                    sh->sh_cr_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                    sh->sh_cr_tc_offset_div2   = nvcl_read_s_expgolomb(rdr);
                }
            }
        }
    }

    if (sps->sps_dep_quant_enabled_flag) {
        sh->sh_dep_quant_used_flag = nvcl_read_flag(rdr);
    }

    if (sps->sps_sign_data_hiding_enabled_flag && !sh->sh_dep_quant_used_flag) {
        sh->sh_sign_data_hiding_used_flag = nvcl_read_flag(rdr);
    }

    if (sps->sps_transform_skip_enabled_flag && !sh->sh_dep_quant_used_flag && !sh->sh_sign_data_hiding_used_flag) {
        sh->sh_ts_residual_coding_disabled_flag = nvcl_read_flag(rdr);
    }

    if (pps->pps_slice_header_extension_present_flag) {
        sh->sh_slice_header_extension_length = nvcl_read_u_expgolomb(rdr);
        for (i = 0; i < sh->sh_slice_header_extension_length; i++) {
            sh->sh_slice_header_extension_data_byte[i] = nvcl_read_bits(rdr, 8);
        }
    }


    uint16_t nb_entry_points_minus1 = slice->nb_entries - 1;

    //ov_log(NULL, OVLOG_ERROR, "Slice address %d\n", slice_address);

#if 0
    if (pps->pps_single_slice_per_subpic_flag) {
        nb_entry_points_minus1 = 0;
        slice->nb_entries = 1;
    }
#endif

    if (nb_entry_points_minus1) {
        if (nb_entry_points_minus1 > 255) return -1;
        sh->sh_entry_offset_len_minus1 = nvcl_read_u_expgolomb(rdr);
        for (i = 0; i < nb_entry_points_minus1; i++) {
            sh->sh_entry_point_offset_minus1[i] = nvcl_read_bits(rdr, sh->sh_entry_offset_len_minus1 + 1);
        }
    }

    nvcl_read_flag(rdr);
    nvcl_align(rdr);

    return 0;
}
