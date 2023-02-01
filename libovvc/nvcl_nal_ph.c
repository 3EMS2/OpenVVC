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
#include <string.h>

#include "ovutils.h"
#include "overror.h"
#include "ovmem.h"

#include "nvcl.h"
#include "nvcl_utils.h"
#include "nvcl_structures.h"
#include "nvcl_private.h"
#include "hls_structures.h"

//TODO: declare in another header file
enum OVChromaFormat{
    OV_CHROMA_400        = 0,
    OV_CHROMA_420        = 1,
    OV_CHROMA_422        = 2,
    OV_CHROMA_444        = 3,
    OV_NUM_CHROMA_FORMAT = 4,
};

static uint8_t
probe_ph_id(OVNVCLReader *const rdr)
{
    return 0;
}

static struct HLSDataRef **
storage_in_nvcl_ctx(OVNVCLReader *const rdr, OVNVCLCtx *const nvcl_ctx)
{
    return &nvcl_ctx->ph;
}

static int
validate_ph(OVNVCLReader *rdr, const union HLSData *const ph)
{
    /* TODO various check on limitation and max sizes */
    return 1;
}

static void
free_ph(const union HLSData *ph)
{
    /* TODO unref and/or free dynamic structure */
    ov_free((void *)ph);
}

#if 0
static int
replace_ph(const struct HLSReader *const manager,
           struct HLSDataRef **storage,
           const OVHLSData *const hls_data)
{
    /* TODO unref and/or free dynamic structure */
    const union HLSData *to_free = *storage;
    union HLSData *new = ov_malloc(manager->data_size);

    if (!new) {
        return OVVC_ENOMEM;
    }

    memcpy(new, hls_data, manager->data_size);

    *storage = new;

    if (to_free) {
        free_ph(to_free);
    }

    return 0;
}
#endif

static int
pred_weight_table_monochrome_ph(OVNVCLReader *const rdr, struct RPLWeightInfo *const wgt_info, uint8_t nb_refs_entries1, uint8_t pps_weighted_bipred_flag)
{
    int i;
    wgt_info->luma_log2_weight_denom = nvcl_read_u_expgolomb(rdr);
    wgt_info->num_l0_weights = nvcl_read_u_expgolomb(rdr);

    for (i = 0; i < wgt_info->num_l0_weights; i++) {
        wgt_info->luma_weight_l0_flag[i] = nvcl_read_flag(rdr);;
    }

    for (i = 0; i < wgt_info->num_l0_weights; i++) {
        if (wgt_info->luma_weight_l0_flag[i]) {
            wgt_info->delta_luma_weight_l0[i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->luma_offset_l0[i]       = nvcl_read_s_expgolomb(rdr);
        }
    }

    if (pps_weighted_bipred_flag && nb_refs_entries1 > 0) {
        wgt_info->num_l0_weights = nvcl_read_u_expgolomb(rdr);
    }

    for (i = 0; i < wgt_info->num_l1_weights; i++) {
        wgt_info->luma_weight_l1_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < wgt_info->num_l1_weights; i++) {
        if (wgt_info->luma_weight_l1_flag[i]) {
            wgt_info->delta_luma_weight_l1[i] = nvcl_read_s_expgolomb(rdr);
            wgt_info->luma_offset_l1[i]       = nvcl_read_s_expgolomb(rdr);
        }
    }
}

static int
pred_weight_table_ph(OVNVCLReader *const rdr, struct RPLWeightInfo *const wgt_info, uint8_t nb_refs_entries1, uint8_t pps_weighted_bipred_flag)
{
    int i;
    wgt_info->luma_log2_weight_denom         = nvcl_read_u_expgolomb(rdr);
    wgt_info->delta_chroma_log2_weight_denom = nvcl_read_s_expgolomb(rdr);

    wgt_info->num_l0_weights = nvcl_read_u_expgolomb(rdr);

    for (i = 0; i < wgt_info->num_l0_weights; i++) {
        wgt_info->luma_weight_l0_flag[i] = nvcl_read_flag(rdr);;
    }

    for (i = 0; i < wgt_info->num_l0_weights; i++) {
        wgt_info->chroma_weight_l0_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < wgt_info->num_l0_weights; i++) {

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

    if (pps_weighted_bipred_flag && nb_refs_entries1 > 0) {
        wgt_info->num_l0_weights = nvcl_read_u_expgolomb(rdr);
    }

    for (i = 0; i < wgt_info->num_l1_weights; i++) {
        wgt_info->luma_weight_l1_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < wgt_info->num_l1_weights; i++) {
        wgt_info->chroma_weight_l1_flag[i] = nvcl_read_flag(rdr);
    }

    for (i = 0; i < wgt_info->num_l1_weights; i++) {

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

int
nvcl_ph_read(OVNVCLReader *const rdr, OVHLSData *const hls_data,
             const OVNVCLCtx *const nvcl_ctx, uint8_t nalu_type)
{
    const OVPPS *pps = NULL;
    const OVSPS *sps = NULL;
    int num_ref_entries0 = 0;
    int num_ref_entries1 = 0;
    OVPH *const ph = &hls_data->ph;
    int i;

    ph->ph_gdr_or_irap_pic_flag = nvcl_read_flag(rdr);
    ph->ph_non_ref_pic_flag     = nvcl_read_flag(rdr);
    if (ph->ph_gdr_or_irap_pic_flag) {
        ph->ph_gdr_pic_flag = nvcl_read_flag(rdr);
    }

    ph->ph_intra_slice_allowed_flag = 1;
    ph->ph_inter_slice_allowed_flag = nvcl_read_flag(rdr);
    if (ph->ph_inter_slice_allowed_flag) {
        ph->ph_intra_slice_allowed_flag = nvcl_read_flag(rdr);
    }

    ph->ph_pic_parameter_set_id = nvcl_read_u_expgolomb(rdr);

    if (ph->ph_pic_parameter_set_id < OV_MAX_NUM_PPS) {
        uint8_t pps_id = ph->ph_pic_parameter_set_id & 0xF;
        if (nvcl_ctx->pps_list[pps_id])
        pps = (OVPPS *)nvcl_ctx->pps_list[pps_id]->data;
        if (pps) {
            /* We suppose sps_id already checked by sps reader */
            uint8_t sps_id = pps->pps_seq_parameter_set_id & 0xF;
            if (nvcl_ctx->sps_list[sps_id])
            sps = (OVSPS *)nvcl_ctx->sps_list[sps_id]->data;
        }
        if (!pps || !sps) {
            ov_log(NULL, 3, "SPS or PPS missing when trying to decode PH\n");
            return OVVC_EINDATA;
        }
    }

    ph->ph_pic_order_cnt_lsb = nvcl_read_bits(rdr, sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);

    if (ph->ph_gdr_pic_flag) {
        ph->ph_recovery_poc_cnt = nvcl_read_u_expgolomb(rdr);
    }

    /* FIXME this suppose to count 1 in sps_ph_extra_bits table */
    for (i = 0; i < sps->sps_num_extra_ph_bytes; i++) {
        ph->ph_extra_bit[i] = nvcl_read_bits(rdr, 1);
    }

    if (sps->sps_poc_msb_cycle_flag) {
        ph->ph_poc_msb_cycle_present_flag = nvcl_read_flag(rdr);
        if (ph->ph_poc_msb_cycle_present_flag) {
            ph->ph_poc_msb_cycle_val = nvcl_read_bits(rdr, sps->sps_poc_msb_cycle_len_minus1 + 1);
        }
    }

    if (sps->sps_alf_enabled_flag && pps->pps_alf_info_in_ph_flag) {
        ph->ph_alf_enabled_flag = nvcl_read_flag(rdr);
        if (ph->ph_alf_enabled_flag) {
            ph->ph_num_alf_aps_ids_luma = nvcl_read_bits(rdr, 3);
            for (i = 0; i < ph->ph_num_alf_aps_ids_luma; i++) {
                ph->ph_alf_aps_id_luma[i] = nvcl_read_bits(rdr, 3);
            }

            if (sps->sps_chroma_format_idc != 0) {
                ph->ph_alf_cb_enabled_flag = nvcl_read_flag(rdr);
                ph->ph_alf_cr_enabled_flag = nvcl_read_flag(rdr);
            }

            if (ph->ph_alf_cb_enabled_flag || ph->ph_alf_cr_enabled_flag) {
                ph->ph_alf_aps_id_chroma = nvcl_read_bits(rdr, 3);
            }

            if (sps->sps_ccalf_enabled_flag) {
                ph->ph_alf_cc_cb_enabled_flag = nvcl_read_flag(rdr);
                if (ph->ph_alf_cc_cb_enabled_flag) {
                    ph->ph_alf_cc_cb_aps_id = nvcl_read_bits(rdr, 3);
                }

                ph->ph_alf_cc_cr_enabled_flag = nvcl_read_flag(rdr);
                if (ph->ph_alf_cc_cr_enabled_flag) {
                    ph->ph_alf_cc_cr_aps_id = nvcl_read_bits(rdr, 3);
                }
            }
        }
    }

    if (sps->sps_lmcs_enabled_flag) {
        ph->ph_lmcs_enabled_flag = nvcl_read_flag(rdr);
        if (ph->ph_lmcs_enabled_flag) {
            ph->ph_lmcs_aps_id = nvcl_read_bits(rdr, 2);
            if (sps->sps_chroma_format_idc != 0) {
                ph->ph_chroma_residual_scale_flag = nvcl_read_flag(rdr);
            }
        }
    }

    if (sps->sps_explicit_scaling_list_enabled_flag) {
        ph->ph_explicit_scaling_list_enabled_flag = nvcl_read_flag(rdr);
        if (ph->ph_explicit_scaling_list_enabled_flag) {
            ph->ph_scaling_list_aps_id = nvcl_read_bits(rdr, 3);
        }
    }

    if (sps->sps_virtual_boundaries_enabled_flag && !sps->sps_virtual_boundaries_present_flag) {
        ph->ph_virtual_boundaries_present_flag = nvcl_read_flag(rdr);
        if (ph->ph_virtual_boundaries_present_flag) {
            ph->ph_num_ver_virtual_boundaries = nvcl_read_u_expgolomb(rdr);
            for (i = 0; i < ph->ph_num_ver_virtual_boundaries; i++) {
                ph->ph_virtual_boundary_pos_x_minus1[i] = nvcl_read_u_expgolomb(rdr);
            }

            ph->ph_num_hor_virtual_boundaries = nvcl_read_u_expgolomb(rdr);
            for (i = 0; i < ph->ph_num_hor_virtual_boundaries; i++) {
                ph->ph_virtual_boundary_pos_y_minus1[i] = nvcl_read_u_expgolomb(rdr);
            }
        }
    }

    ph->ph_pic_output_flag = 1;
    if (pps->pps_output_flag_present_flag && !ph->ph_non_ref_pic_flag) {
        ph->ph_pic_output_flag = nvcl_read_flag(rdr);
    }

    if (pps->pps_rpl_info_in_ph_flag) {
         struct OVHRPL *const hrpl = &ph->hrpl;

         nvcl_read_header_ref_pic_lists(rdr, hrpl, sps, pps);

         num_ref_entries0 = hrpl->rpl0->num_ref_entries;
         num_ref_entries1 = hrpl->rpl1->num_ref_entries;
    }

    if (sps->sps_partition_constraints_override_enabled_flag) {
        ph->ph_partition_constraints_override_flag = nvcl_read_flag(rdr);
    }

    if (ph->ph_intra_slice_allowed_flag) {
        if (ph->ph_partition_constraints_override_flag) {
            ph->ph_log2_diff_min_qt_min_cb_intra_slice_luma = nvcl_read_u_expgolomb(rdr);
            ph->ph_max_mtt_hierarchy_depth_intra_slice_luma = nvcl_read_u_expgolomb(rdr);
            if (ph->ph_max_mtt_hierarchy_depth_intra_slice_luma != 0) {
                ph->ph_log2_diff_max_bt_min_qt_intra_slice_luma = nvcl_read_u_expgolomb(rdr);
                ph->ph_log2_diff_max_tt_min_qt_intra_slice_luma = nvcl_read_u_expgolomb(rdr);
            }

            if (sps->sps_qtbtt_dual_tree_intra_flag) {
                ph->ph_log2_diff_min_qt_min_cb_intra_slice_chroma = nvcl_read_u_expgolomb(rdr);
                ph->ph_max_mtt_hierarchy_depth_intra_slice_chroma = nvcl_read_u_expgolomb(rdr);
                if (ph->ph_max_mtt_hierarchy_depth_intra_slice_chroma != 0) {
                    ph->ph_log2_diff_max_bt_min_qt_intra_slice_chroma = nvcl_read_u_expgolomb(rdr);
                    ph->ph_log2_diff_max_tt_min_qt_intra_slice_chroma = nvcl_read_u_expgolomb(rdr);
                }
            }
        }

        if (pps->pps_cu_qp_delta_enabled_flag) {
            ph->ph_cu_qp_delta_subdiv_intra_slice = nvcl_read_u_expgolomb(rdr);
        }

        if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
            ph->ph_cu_chroma_qp_offset_subdiv_intra_slice = nvcl_read_u_expgolomb(rdr);
        }
    }

    if (ph->ph_inter_slice_allowed_flag) {
        if (ph->ph_partition_constraints_override_flag) {
            ph->ph_log2_diff_min_qt_min_cb_inter_slice = nvcl_read_u_expgolomb(rdr);
            ph->ph_max_mtt_hierarchy_depth_inter_slice = nvcl_read_u_expgolomb(rdr);
            if (ph->ph_max_mtt_hierarchy_depth_inter_slice != 0) {
                ph->ph_log2_diff_max_bt_min_qt_inter_slice = nvcl_read_u_expgolomb(rdr);
                ph->ph_log2_diff_max_tt_min_qt_inter_slice = nvcl_read_u_expgolomb(rdr);
            }
        }

        if (pps->pps_cu_qp_delta_enabled_flag) {
            ph->ph_cu_qp_delta_subdiv_inter_slice = nvcl_read_u_expgolomb(rdr);
        }

        if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
            ph->ph_cu_chroma_qp_offset_subdiv_inter_slice = nvcl_read_u_expgolomb(rdr);
        }

        if (sps->sps_temporal_mvp_enabled_flag) {
            ph->ph_temporal_mvp_enabled_flag = nvcl_read_flag(rdr);
            if (ph->ph_temporal_mvp_enabled_flag && pps->pps_rpl_info_in_ph_flag) {
                /* FIXME compute num_ref_entries */
                ph->ph_collocated_from_l0_flag = 1;
                if (num_ref_entries1 > 0) {
                    ph->ph_collocated_from_l0_flag = nvcl_read_flag(rdr);
                }
                if ((ph->ph_collocated_from_l0_flag && num_ref_entries0 > 1) || (!ph->ph_collocated_from_l0_flag && num_ref_entries1 > 1)) {
                    ph->ph_collocated_ref_idx = nvcl_read_u_expgolomb(rdr);
                }
            }
        }

        if (sps->sps_mmvd_fullpel_only_enabled_flag) {
            ph->ph_mmvd_fullpel_only_flag = nvcl_read_flag(rdr);
        }

        if (!pps->pps_rpl_info_in_ph_flag) {

            ph->ph_mvd_l1_zero_flag = nvcl_read_flag(rdr);
            if (sps->sps_bdof_control_present_in_ph_flag) {
                ph->ph_bdof_disabled_flag = nvcl_read_flag(rdr);
            }

            if (sps->sps_dmvr_control_present_in_ph_flag) {
                ph->ph_dmvr_disabled_flag = nvcl_read_flag(rdr);
            }

        } else if (num_ref_entries1 > 0) {

            ph->ph_mvd_l1_zero_flag = nvcl_read_flag(rdr);
            if (sps->sps_bdof_control_present_in_ph_flag) {
                ph->ph_bdof_disabled_flag = nvcl_read_flag(rdr);
            }

            if (sps->sps_dmvr_control_present_in_ph_flag) {
                ph->ph_dmvr_disabled_flag = nvcl_read_flag(rdr);
            }
        }

        if (sps->sps_prof_control_present_in_ph_flag) {
            ph->ph_prof_disabled_flag = nvcl_read_flag(rdr);
        }

        if ((pps->pps_weighted_pred_flag || pps->pps_weighted_bipred_flag) && pps->pps_wp_info_in_ph_flag) {
            if (sps->sps_chroma_format_idc) {
                pred_weight_table_ph(rdr, &ph->wgt_info, num_ref_entries1, pps->pps_weighted_bipred_flag);
            } else {
                pred_weight_table_monochrome_ph(rdr, &ph->wgt_info, num_ref_entries1, pps->pps_weighted_bipred_flag);
            }
        }
    }

    if (pps->pps_qp_delta_info_in_ph_flag) {
        ph->ph_qp_delta = nvcl_read_s_expgolomb(rdr);
    }

    if (sps->sps_joint_cbcr_enabled_flag) {
        ph->ph_joint_cbcr_sign_flag = nvcl_read_flag(rdr);
    }

    if (sps->sps_sao_enabled_flag && pps->pps_sao_info_in_ph_flag) {
        ph->ph_sao_luma_enabled_flag = nvcl_read_flag(rdr);
        if (sps->sps_chroma_format_idc != OV_CHROMA_400) {
            ph->ph_sao_chroma_enabled_flag = nvcl_read_flag(rdr);
        }
    }

    if (pps->pps_dbf_info_in_ph_flag) {
        ph->ph_deblocking_params_present_flag = nvcl_read_flag(rdr);
        if (ph->ph_deblocking_params_present_flag) {
            if (!pps->pps_deblocking_filter_disabled_flag) {
                ph->ph_deblocking_filter_disabled_flag = nvcl_read_flag(rdr);
            }
            if (!ph->ph_deblocking_filter_disabled_flag) {
                ph->ph_luma_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                ph->ph_luma_tc_offset_div2 = nvcl_read_s_expgolomb(rdr);
                if (pps->pps_chroma_tool_offsets_present_flag) {
                    ph->ph_cb_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                    ph->ph_cb_tc_offset_div2 = nvcl_read_s_expgolomb(rdr);
                    ph->ph_cr_beta_offset_div2 = nvcl_read_s_expgolomb(rdr);
                    ph->ph_cr_tc_offset_div2 = nvcl_read_s_expgolomb(rdr);
                }
            }
        }
    }

    if (pps->pps_picture_header_extension_present_flag) {
        ph->ph_extension_length = nvcl_read_u_expgolomb(rdr);
        for (i = 0; i < ph->ph_extension_length; i++) {
            ph->ph_extension_data_byte[i] = nvcl_read_bits(rdr, 8);
        }
    }
    /*TODO decide on return checks and values */
    return 0;
}

const struct HLSReader ph_manager =
{
    .name = "PH",
    .data_size = sizeof(struct OVPH),
    .find_storage = &storage_in_nvcl_ctx,
    .read         = &nvcl_ph_read,
    .validate     = &validate_ph,
//    .replace      = &replace_ph,
    .free         = &free_ph
};
