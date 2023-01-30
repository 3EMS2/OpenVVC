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
#include "ovmem.h"
#include "ovutils.h"
#include "overror.h"

#include "nvcl.h"
#include "nvcl_utils.h"
#include "nvcl_structures.h"
#include "hls_structures.h"


static uint8_t
probe_pps_id(OVNVCLReader *const rdr)
{
    uint8_t pps_id = fetch_bits(rdr, 6);
    return pps_id;
}

static struct HLSDataRef **
storage_in_nvcl_ctx(OVNVCLReader *const rdr, OVNVCLCtx *const nvcl_ctx)
{
    uint8_t id = probe_pps_id(rdr);

    struct HLSDataRef **storage = &nvcl_ctx->pps_list[id];

    return storage;
}

static int
validate_pps(OVNVCLReader *rdr, const union HLSData *const data)
{
    const OVPPS *const pps = &data->pps;

    if ((pps->pps_pic_width_in_luma_samples | pps->pps_pic_height_in_luma_samples) & 0xE003) {
        ov_log(NULL, OVLOG_ERROR, "Invalid picture dimension %dx%d. \n",
               pps->pps_pic_width_in_luma_samples,
               pps->pps_pic_height_in_luma_samples);

        return OVVC_EINDATA;
    }

    return 1;
}

static void
free_pps(const union HLSData *pps)
{
    /* TODO unref and/or free dynamic structure */
    ov_free((void *)pps);
}

#if 0
static int
replace_pps(const struct HLSReader *const manager,
            struct HLSDataRef **storage,
            const OVHLSData *const hls_data)
{
    const union HLSData *to_free = (*storage)->data;
    union HLSData *new = ov_malloc(manager->data_size);

    if (!new) {
        return OVVC_ENOMEM;
    }

    memcpy(new, hls_data, manager->data_size);

    (*storage)->data = new;

    if (to_free) {
        free_pps(to_free);
    }

    return 0;
}
#endif

static void
pps_read_slices_in_subpic(OVNVCLReader *const rdr, OVPPS *const pps,
                          const struct PicPartitionInfo *const part_info)
{
    int i;
    int tile_id = 0;

    if (pps->pps_num_slices_in_pic_minus1 > 1) {
        pps->pps_tile_idx_delta_present_flag = nvcl_read_flag(rdr);
    }

    for (i = 0; i < pps->pps_num_slices_in_pic_minus1; i++) {
        int tile_x = tile_id % part_info->nb_tile_w;
        int tile_y = tile_id / part_info->nb_tile_w;

        /* Each new tile column read slice width exept for implicit last column */
        if (tile_x != part_info->nb_tile_w - 1) {
            pps->pps_slice_width_in_tiles_minus1[i] = nvcl_read_u_expgolomb(rdr);
        }

        /* Each new tile row read slice height except for implicit last row */
        if (tile_y !=  part_info->nb_tile_h - 1) {
            if (pps->pps_tile_idx_delta_present_flag || tile_x == 0) {
                pps->pps_slice_height_in_tiles_minus1[i] = nvcl_read_u_expgolomb(rdr);
            }
        }

        /* Multiple slices in tiles */
        if (!pps->pps_slice_width_in_tiles_minus1[i] && !pps->pps_slice_height_in_tiles_minus1[i]) {
            if (part_info->tile_row_h[tile_y] > 1) {
                pps->pps_num_exp_slices_in_tile[tile_id] = nvcl_read_u_expgolomb(rdr);
                if (pps->pps_num_exp_slices_in_tile[tile_id]) {
                    int j;
                    int sum = 0;
                    for (j = 0; j < pps->pps_num_exp_slices_in_tile[tile_id]; j++) {
                        pps->pps_exp_slice_height_in_ctus_minus1[i + j] = nvcl_read_u_expgolomb(rdr);
                        sum += pps->pps_exp_slice_height_in_ctus_minus1[i + j] + 1;
                    }

                    if (sum < part_info->tile_row_h[tile_y]) {
                        int last_read = pps->pps_exp_slice_height_in_ctus_minus1[i + j - 1] + 1;
                        int rem_ctu_h = part_info->tile_row_h[tile_y] - sum;
                        int nb_implicit_slices = rem_ctu_h / last_read + !!(rem_ctu_h % last_read);
                        j += nb_implicit_slices;
                    }

                    i += (j - 1);
                }
            }
        }

        if (pps->pps_tile_idx_delta_present_flag && i < pps->pps_num_slices_in_pic_minus1) {
            pps->pps_tile_idx_delta_val[i] = nvcl_read_s_expgolomb(rdr);
            tile_id += pps->pps_tile_idx_delta_val[i];
        } else {
            tile_id  += pps->pps_slice_width_in_tiles_minus1[i] + 1;
            if (tile_id % part_info->nb_tile_w == 0) {
                int  offset_y  = pps->pps_slice_height_in_tiles_minus1[i];
                offset_y *= part_info->nb_tile_w;
                tile_id += offset_y;
            }
        }
    }
}

static void
pps_implicit_pic_partition(OVPPS *const pps, struct PicPartitionInfo *const part_info)
{
    int nb_explicit_cols = pps->pps_num_exp_tile_columns_minus1 + 1;
    int nb_explicit_rows = pps->pps_num_exp_tile_rows_minus1 + 1;

    const int log2_ctb_s = pps->pps_log2_ctu_size_minus5 + 5;

    const int pic_w = pps->pps_pic_width_in_luma_samples;
    const int pic_h = pps->pps_pic_height_in_luma_samples;

    const int nb_ctu_w = (pic_w + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
    const int nb_ctu_h = (pic_h + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;

    int rem_ctu_w = nb_ctu_w;
    int rem_ctu_h = nb_ctu_h;

    int i;

    part_info->nb_ctu_w = nb_ctu_w;
    part_info->nb_ctu_h = nb_ctu_h;
    part_info->log2_ctb_s = log2_ctb_s;

    for (i = 0; i <  nb_explicit_rows; ++i) {
        part_info->tile_row_h[i] = pps->pps_tile_row_height_minus1[i] + 1;
        rem_ctu_h -= (int)part_info->tile_row_h[i];
    }

    while (rem_ctu_h > 0) {
        part_info->tile_row_h[i] = OVMIN(rem_ctu_h, part_info->tile_row_h[i - 1]);
        rem_ctu_h -= part_info->tile_row_h[i];
        i++;
    }


    part_info->nb_tile_h = i;

    for (i = 0; i < nb_explicit_cols; ++i) {
        part_info->tile_col_w[i] = pps->pps_tile_column_width_minus1[i] + 1;
        rem_ctu_w -= (int)part_info->tile_col_w[i];
    }

    while (rem_ctu_w > 0) {
        part_info->tile_col_w[i] = OVMIN(rem_ctu_w, part_info->tile_col_w[i - 1]);
        rem_ctu_w -= part_info->tile_col_w[i];
        i++;
    }


    part_info->nb_tile_w = i;
}

static void
pps_read_pic_partition(OVNVCLReader *const rdr, OVPPS *const pps)
{
    int i;

    pps->pps_num_exp_tile_columns_minus1 = nvcl_read_u_expgolomb(rdr);
    pps->pps_num_exp_tile_rows_minus1    = nvcl_read_u_expgolomb(rdr);

    for (i = 0; i <=  pps->pps_num_exp_tile_columns_minus1; i++) {
        pps->pps_tile_column_width_minus1[i] = nvcl_read_u_expgolomb(rdr);
    }

    for (i = 0; i <=  pps->pps_num_exp_tile_rows_minus1; i++) {
        pps->pps_tile_row_height_minus1[i] = nvcl_read_u_expgolomb(rdr);
    }

    pps_implicit_pic_partition(pps, &pps->part_info);

    /* Default initialisation values */
    pps->pps_loop_filter_across_tiles_enabled_flag = 1;
    pps->pps_rect_slice_flag                       = 1;

    uint8_t nb_tiles_gt1  = pps->pps_tile_row_height_minus1  [0] != pps->part_info.nb_ctu_h - 1;
            nb_tiles_gt1 |= pps->pps_tile_column_width_minus1[0] != pps->part_info.nb_ctu_w - 1;

    if (nb_tiles_gt1) {
        pps->pps_loop_filter_across_tiles_enabled_flag = nvcl_read_flag(rdr);
        pps->pps_rect_slice_flag                       = nvcl_read_flag(rdr);
    }

    if (pps->pps_rect_slice_flag) {
        pps->pps_single_slice_per_subpic_flag = nvcl_read_flag(rdr);
        if (!pps->pps_single_slice_per_subpic_flag) {
            pps->pps_num_slices_in_pic_minus1 = nvcl_read_u_expgolomb(rdr);
            pps_read_slices_in_subpic(rdr, pps, &pps->part_info);
        }
    }

    if (!pps->pps_rect_slice_flag || pps->pps_single_slice_per_subpic_flag ||
            pps->pps_num_slices_in_pic_minus1) {
        pps->pps_loop_filter_across_slices_enabled_flag = nvcl_read_flag(rdr);
    }
}

int
nvcl_pps_read(OVNVCLReader *const rdr, OVHLSData *const hls_data,
              const OVNVCLCtx *const nvcl_ctx, uint8_t nalu_type)
{
    int i;
    OVPPS *const pps = &hls_data->pps;

    #if 0
    OVPPS stck_pps = {0};
    OVPPS *const pps = &stck_pps;
    #endif

    pps->pps_pic_parameter_set_id = nvcl_read_bits(rdr, 6);
    pps->pps_seq_parameter_set_id = nvcl_read_bits(rdr, 4);

    pps->pps_mixed_nalu_types_in_pic_flag = nvcl_read_flag(rdr);

    pps->pps_pic_width_in_luma_samples  = nvcl_read_u_expgolomb(rdr);
    pps->pps_pic_height_in_luma_samples = nvcl_read_u_expgolomb(rdr);

    pps->pps_conformance_window_flag = nvcl_read_flag(rdr);
    if (pps->pps_conformance_window_flag) {
        pps->pps_conf_win_left_offset   = nvcl_read_u_expgolomb(rdr);
        pps->pps_conf_win_right_offset  = nvcl_read_u_expgolomb(rdr);
        pps->pps_conf_win_top_offset    = nvcl_read_u_expgolomb(rdr);
        pps->pps_conf_win_bottom_offset = nvcl_read_u_expgolomb(rdr);
    }

    pps->pps_scaling_window_explicit_signalling_flag = nvcl_read_flag(rdr);
    if (pps->pps_scaling_window_explicit_signalling_flag) {
        pps->pps_scaling_win_left_offset   = nvcl_read_s_expgolomb(rdr);
        pps->pps_scaling_win_right_offset  = nvcl_read_s_expgolomb(rdr);
        pps->pps_scaling_win_top_offset    = nvcl_read_s_expgolomb(rdr);
        pps->pps_scaling_win_bottom_offset = nvcl_read_s_expgolomb(rdr);
    }

    pps->pps_output_flag_present_flag = nvcl_read_flag(rdr);

    pps->pps_no_pic_partition_flag = nvcl_read_flag(rdr);

    pps->pps_subpic_id_mapping_present_flag = nvcl_read_flag(rdr);
    if (pps->pps_subpic_id_mapping_present_flag) {
        if (!pps->pps_no_pic_partition_flag) {
            pps->pps_num_subpics_minus1 = nvcl_read_u_expgolomb(rdr);
        }

        pps->pps_subpic_id_len_minus1 = nvcl_read_u_expgolomb(rdr);
        for(i = 0; i <= pps->pps_num_subpics_minus1; i++) {
            pps->pps_subpic_id[i] = nvcl_read_bits(rdr, pps->pps_subpic_id_len_minus1 + 1);
        }
    }

    if (!pps->pps_no_pic_partition_flag) {
        pps->pps_log2_ctu_size_minus5 = nvcl_read_bits(rdr, 2);

        pps_read_pic_partition(rdr, pps);
    } else {
        const OVSPS *sps = (OVSPS *)nvcl_ctx->sps_list[pps->pps_seq_parameter_set_id]->data;
	    const int log2_ctb_s = sps->sps_log2_ctu_size_minus5 + 5;

	    const int pic_w = sps->sps_pic_width_max_in_luma_samples;
	    const int pic_h = sps->sps_pic_height_max_in_luma_samples;

	    const int nb_ctu_w = (pic_w + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
	    const int nb_ctu_h = (pic_h + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;

	    pps->part_info.nb_ctu_w = nb_ctu_w;
	    pps->part_info.nb_ctu_h = nb_ctu_h;
	    pps->part_info.nb_tile_w = 1;
	    pps->part_info.nb_tile_h = 1;
	    pps->part_info.tile_col_w[0] = nb_ctu_w;
	    pps->part_info.tile_row_h[0] = nb_ctu_h;
	    pps->part_info.log2_ctb_s = log2_ctb_s;
    }

    pps->pps_cabac_init_present_flag = nvcl_read_flag(rdr);
    for(i = 0; i < 2; i++) {
        pps->pps_num_ref_idx_default_active_minus1[i] = nvcl_read_u_expgolomb(rdr);
    }

    pps->pps_rpl1_idx_present_flag = nvcl_read_flag(rdr);

    pps->pps_weighted_pred_flag   = nvcl_read_flag(rdr);
    pps->pps_weighted_bipred_flag = nvcl_read_flag(rdr);

    pps->pps_ref_wraparound_enabled_flag = nvcl_read_flag(rdr);
    if (pps->pps_ref_wraparound_enabled_flag) {
        pps->pps_pic_width_minus_wraparound_offset = nvcl_read_u_expgolomb(rdr);
    }

    pps->pps_init_qp_minus26 = nvcl_read_s_expgolomb(rdr);

    pps->pps_cu_qp_delta_enabled_flag = nvcl_read_flag(rdr);

    pps->pps_chroma_tool_offsets_present_flag = nvcl_read_flag(rdr);
    if (pps->pps_chroma_tool_offsets_present_flag) {
        pps->pps_cb_qp_offset = nvcl_read_s_expgolomb(rdr);
        pps->pps_cr_qp_offset = nvcl_read_s_expgolomb(rdr);
        pps->pps_joint_cbcr_qp_offset_present_flag = nvcl_read_flag(rdr);
        if (pps->pps_joint_cbcr_qp_offset_present_flag) {
            pps->pps_joint_cbcr_qp_offset_value = nvcl_read_s_expgolomb(rdr);
        }

        pps->pps_slice_chroma_qp_offsets_present_flag = nvcl_read_flag(rdr);
        pps->pps_cu_chroma_qp_offset_list_enabled_flag = nvcl_read_flag(rdr);
        if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
            pps->pps_chroma_qp_offset_list_len_minus1 = nvcl_read_u_expgolomb(rdr);
            for (i = 0; i <= (pps->pps_chroma_qp_offset_list_len_minus1 & 0x3F); i++) {
                pps->pps_cb_qp_offset_list[i] = nvcl_read_s_expgolomb(rdr);
                pps->pps_cr_qp_offset_list[i] = nvcl_read_s_expgolomb(rdr);
                if (pps->pps_joint_cbcr_qp_offset_present_flag) {
                    pps->pps_joint_cbcr_qp_offset_list[i] = nvcl_read_s_expgolomb(rdr);
                }
            }
        }
    }

    pps->pps_deblocking_filter_control_present_flag = nvcl_read_flag(rdr);
    if (pps->pps_deblocking_filter_control_present_flag) {
        pps->pps_deblocking_filter_override_enabled_flag = nvcl_read_flag(rdr);
        pps->pps_deblocking_filter_disabled_flag = nvcl_read_flag(rdr);
        if (!pps->pps_no_pic_partition_flag && pps->pps_deblocking_filter_override_enabled_flag) {
            pps->pps_dbf_info_in_ph_flag = nvcl_read_flag(rdr);
        }

        if (!pps->pps_deblocking_filter_disabled_flag) {
            pps->pps_luma_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
            pps->pps_luma_tc_offset_div2   = nvcl_read_s_expgolomb(rdr);
            if (pps->pps_chroma_tool_offsets_present_flag) {
                pps->pps_cb_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                pps->pps_cb_tc_offset_div2   = nvcl_read_s_expgolomb(rdr);
                pps->pps_cr_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                pps->pps_cr_tc_offset_div2   = nvcl_read_s_expgolomb(rdr);
            }
        }
    }

    if (!pps->pps_no_pic_partition_flag) {
        pps->pps_rpl_info_in_ph_flag = nvcl_read_flag(rdr);
        pps->pps_sao_info_in_ph_flag = nvcl_read_flag(rdr);
        pps->pps_alf_info_in_ph_flag = nvcl_read_flag(rdr);
        if ((pps->pps_weighted_pred_flag || pps->pps_weighted_bipred_flag) && pps->pps_rpl_info_in_ph_flag) {
            pps->pps_wp_info_in_ph_flag = nvcl_read_flag(rdr);
        }

        pps->pps_qp_delta_info_in_ph_flag = nvcl_read_flag(rdr);
    }

    pps->pps_picture_header_extension_present_flag = nvcl_read_flag(rdr);
    pps->pps_slice_header_extension_present_flag = nvcl_read_flag(rdr);
    pps->pps_extension_flag = nvcl_read_flag(rdr);

    if (pps->pps_extension_flag) {
        int32_t nb_bits_read = nvcl_nb_bits_read(rdr) + 1;
        int32_t stop_bit_pos = nvcl_find_rbsp_stop_bit(rdr);
        int32_t nb_bits_remaining = stop_bit_pos - nb_bits_read;

        if (nb_bits_remaining < 0) {
            ov_log(NULL, OVLOG_ERROR, "Overread PPS %d", nb_bits_read, stop_bit_pos);
            return OVVC_EINDATA;
        }

        nvcl_skip_bits(rdr, nb_bits_remaining);
    }

    return 0;
}

const struct HLSReader pps_manager =
{
    .name = "PPS",
    .data_size = sizeof(struct OVPPS),
    .find_storage = &storage_in_nvcl_ctx,
    .read         = &nvcl_pps_read,
    .validate     = &validate_pps,
//    .replace      = &replace_pps,
    .free         = &free_pps
};
