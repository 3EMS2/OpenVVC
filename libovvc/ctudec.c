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
#include "ovdefs.h"
#include "ovdpb.h"
#include "ctudec.h"
#include "rcn_lmcs.h"
#include "ovmem.h"
#include "overror.h"

int
ctudec_init_in_loop_filters(OVCTUDec *const ctudec, const OVPS *const prms)
{
    const OVSPS *const sps = prms->sps;
    const OVPPS *const pps = prms->pps;
    const OVSH *const sh = prms->sh;
    const OVPH *const ph = prms->ph;

    uint16_t pic_w = sps->sps_pic_width_max_in_luma_samples;
    uint16_t pic_h = sps->sps_pic_height_max_in_luma_samples;
    uint8_t log2_ctb_s = sps->sps_log2_ctu_size_minus5 + 5;
    int nb_ctb_pic_w = (pic_w + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
    int nb_ctb_pic_h = (pic_h + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
    struct ToolsInfo *tools = &ctudec->tools;

    //Init SAO info and ctu params
    struct SAOInfo* sao_info  = &ctudec->sao_info;

    if (ctudec->tools.sao_luma_flag || ctudec->tools.sao_chroma_flag) {
        if (!sao_info->sao_params) {
            sao_info->sao_params = ov_mallocz(sizeof(SAOParamsCtu) * nb_ctb_pic_w * nb_ctb_pic_h);
        } else {
            memset(sao_info->sao_params,0,sizeof(SAOParamsCtu) * nb_ctb_pic_w * nb_ctb_pic_h);
        }
    }

    if (tools->alf_luma_enabled_flag || tools->alf_cb_enabled_flag || tools->alf_cr_enabled_flag) {

        struct ALFInfo* alf_info  = &ctudec->alf_info;
        RCNALF* alf = &alf_info->rcn_alf;
        uint8_t luma_flag   = tools->alf_luma_enabled_flag;
        uint8_t chroma_flag = tools->alf_cb_enabled_flag || tools->alf_cr_enabled_flag;

        for (int i = 0; i < tools->num_alf_aps_ids_luma; i++) {
            alf_info->aps_alf_data[i] = prms->aps_alf[i] ? &prms->aps_alf[i]->aps_alf_data : NULL;
        }


        if (!alf_info->ctb_alf_params) {
            alf_info->ctb_alf_params = ov_malloc(sizeof(ALFParamsCtu) * nb_ctb_pic_w * nb_ctb_pic_h);
        } else {
            memset(alf_info->ctb_alf_params, 0, sizeof(ALFParamsCtu) * nb_ctb_pic_w * nb_ctb_pic_h);
        }

        ctudec->rcn_funcs.alf.rcn_alf_reconstruct_coeff_APS(alf, ctudec, prms);
    }

#if 0
    if (tools->cc_alf_cb_enabled_flag || tools->cc_alf_cr_enabled_flag) {
        struct ALFInfo* alf_info  = &ctudec->alf_info;
        const OVALFData *const ccalf_data_cb = &prms->aps_cc_alf_cb->aps_alf_data;
        const OVALFData *const ccalf_data_cr = &prms->aps_cc_alf_cr->aps_alf_data;
        alf_info->nb_ccalf_alt_cb = ccalf_data_cb ? ccalf_data_cb->alf_cc_cb_filters_signalled_minus1 + 1 : 1;
        alf_info->nb_ccalf_alt_cr = ccalf_data_cr ? ccalf_data_cr->alf_cc_cr_filters_signalled_minus1 + 1 : 1;
    }
#endif

    struct LMCSInfo* lmcs_info   = &ctudec->lmcs_info;
    lmcs_info->lmcs_enabled_flag = ph->ph_lmcs_enabled_flag;
    lmcs_info->scale_c_flag      = ph->ph_chroma_residual_scale_flag;

    if (sh->sh_lmcs_used_flag || lmcs_info->lmcs_enabled_flag || lmcs_info->scale_c_flag) {
        const struct OVLMCSData* lmcs_data = &prms->aps_lmcs->aps_lmcs_data;
        ctudec->rcn_funcs.rcn_init_lmcs(lmcs_info, lmcs_data);
    }

    return 0;
}

void
ctudec_uninit_in_loop_filters(OVCTUDec *const ctudec)
{
    struct SAOInfo* sao_info  = &ctudec->sao_info;
    struct ALFInfo* alf_info  = &ctudec->alf_info;
    struct LMCSInfo* lmcs_info  = &ctudec->lmcs_info;

    if (sao_info->sao_params) {
        ov_free(sao_info->sao_params);
    }

    if (alf_info->ctb_alf_params) {
        ov_free(alf_info->ctb_alf_params);
    }

    if (lmcs_info->luts) {
        ov_free(lmcs_info->luts);
    }
}

int
ctudec_init(OVCTUDec **ctudec_p)
{
     OVCTUDec *ctudec;

     ctudec = ov_mallocz(sizeof(*ctudec));

     if (!ctudec) {
         return OVVC_ENOMEM;
     }

     *ctudec_p = ctudec;

     ctudec->prev_nb_ctu_w_rect_entry = 0;

     return 0;
}

int
ctudec_uninit(OVCTUDec *ctudec)
{
    ctudec_uninit_in_loop_filters(ctudec);

    if (ctudec->rcn_funcs.rcn_buff_uninit) {
        ctudec->rcn_funcs.rcn_buff_uninit(&ctudec->rcn_ctx);
    }

    ov_freep(&ctudec);

    return 0;
}
