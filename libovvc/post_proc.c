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

#include "overror.h"
#include "ovlog.h"
#include "nvcl_utils.h"
#include "ovdpb.h"
#include "ovconfig.h"

#include "nvcl_structures.h"
#include "ovframepool.h"

#include "slicedec.h"
#include "post_proc.h"

#if HAVE_SLHDR
#include "pp_wrapper_slhdr.h"
#endif

void
pp_init_functions(const struct PostProcessCtx *ppctx, const OVSEI* sei, struct PostProcFunctions *const pp_funcs)
{
    pp_funcs->pp_apply_flag = 0;

    if (sei) {
        if (sei->sei_fg) {
            pp_funcs->pp_film_grain = fg_grain_apply_pic;
            pp_funcs->pp_apply_flag = 1;
        } else {
            pp_funcs->pp_film_grain = fg_grain_no_filter;
        }

#if HAVE_SLHDR
        if (sei->sei_slhdr) {

            if (!ppctx->slhdr_ctx) {
                ov_log (NULL, OVLOG_DEBUG, "Init SLHDR Post Processor with peak luminance: %d\n", ppctx->brightness);
                pp_init_slhdr_lib(&ppctx->slhdr_ctx);
                pp_set_display_peak(ppctx->slhdr_ctx, ppctx->brightness);
            }

            pp_funcs->pp_sdr_to_hdr = pp_sdr_to_hdr;
            pp_funcs->pp_apply_flag = 1;
        }
#endif
    }

    if (ppctx->upscale_flag) {
        pp_funcs->pp_apply_flag = 1;
    }
}

void
pp_uninit(struct PostProcessCtx *ppctx)
{
    if (ppctx->slhdr_ctx) {
        pp_uninit_slhdr_lib(ppctx->slhdr_ctx);
    }
}

int
pp_process_frame2(const struct PostProcessCtx *ppctx, const OVSEI* sei, OVFrame **frame_p)
{
    struct PostProcFunctions pp_funcs;

    /* FIXME  find another place to init this */
    pp_init_functions(ppctx, sei, &pp_funcs);

    if (pp_funcs.pp_apply_flag) {
        OVFrame* src_frm = *frame_p;

        /* Request a writable picture from same src_frm pool */
        OVFrame* pp_frm = ovframepool_request_frame(src_frm->internal.frame_pool);
        if (!pp_frm) {
            ov_log(NULL, OVLOG_ERROR, "Could not get a writable picture for post processing\n");
            goto no_writable_pic;
        }

        if (ppctx->upscale_flag){
            for(int comp = 0; comp < 3; comp++){
                uint16_t dst_w =  pp_frm->width  >> (!!comp);
                uint16_t dst_h =  pp_frm->height >> (!!comp);
                uint16_t src_w = src_frm->width  >> (!!comp);
                uint16_t src_h = src_frm->height >> (!!comp);

                uint16_t dst_stride =  pp_frm->linesize[comp] >> 1;
                uint16_t src_stride = src_frm->linesize[comp] >> 1;

                uint8_t is_luma = comp == 0;

                pp_sample_rate_conv((uint16_t*) pp_frm->data[comp], dst_stride, dst_w, dst_h,
                                    (uint16_t*)src_frm->data[comp], src_stride, src_w, src_h,
                                    &src_frm->scaling_window, is_luma, &src_frm->frame_info);
            }
        }


        if (sei) {
            int16_t *src_planes[3] = {
                (int16_t*)src_frm->data[0],
                (int16_t*)src_frm->data[1],
                (int16_t*)src_frm->data[2]
            };

            int16_t* dst_planes[3] = {
                (int16_t*)pp_frm->data[0],
                (int16_t*)pp_frm->data[1],
                (int16_t*)pp_frm->data[2]
            };

            uint8_t enable_deblock = 1;

            pp_funcs.pp_film_grain(dst_planes, src_planes, sei->sei_fg,
                                   src_frm->width, src_frm->height,
                                   src_frm->poc, 0, enable_deblock);

#if HAVE_SLHDR
            if(sei->sei_slhdr){
                pp_frm->width = src_frm->width;
                pp_frm->height = src_frm->height;

                static const struct ColorDescription pq_bt2020 = {.colour_primaries = 9, .matrix_coeffs = 9, .transfer_characteristics=16, .full_range = 0} ;

                ov_log (NULL, OVLOG_DEBUG, "Updating SLHDR peak luminance %d\n", ppctx->brightness);
                pp_set_display_peak(ppctx->slhdr_ctx, ppctx->brightness);

                pp_funcs.pp_sdr_to_hdr(ppctx->slhdr_ctx, src_planes, dst_planes,
                                       sei->sei_slhdr->payload_array, src_frm->width, src_frm->height);

                //memcpy(pp_frm->data[0], src_frm->data[0], pp_frm->size[0]);
                //memcpy(pp_frm->data[1], src_frm->data[1], pp_frm->size[1]);
                //memcpy(pp_frm->data[2], src_frm->data[2], pp_frm->size[2]);

                pp_frm->frame_info.color_desc = pq_bt2020;


            } else {
                ov_log (NULL, OVLOG_DEBUG, "No SLHDR SEI\n");
            }
#endif
        }

        /* Replace pointer to output picture by post processed picture. */
        ovframe_unref(frame_p);
        *frame_p = pp_frm;
    }

    return 0;

no_writable_pic:

    ovframe_unref(frame_p);

    return OVVC_ENOMEM;
}


static int tmp_sei_wrap(OVNALUnit *const nalu, OVSEI **dst)
{
    uint8_t nalu_type = nalu->type & 0x1F;
    OVNVCLReader rdr;

    nvcl_reader_init(&rdr, nalu->rbsp_data, nalu->rbsp_size);

    nvcl_skip_bits(&rdr, 16);

    return nvcl_decode_nalu_sei2(dst, &rdr, nalu_type);
}

int
pp_process_frame(struct PostProcessCtx *pctx, const OVPictureUnit * pu, OVFrame **frame_p)
{
    int i;
    int ret = 0;
    int slhdr_sei = 0;
#if 1
    OVSEI *sei = NULL;

    for (i = 0; i < pu->nb_nalus; ++i) {
        if (pu->nalus[i]->type == OVNALU_PREFIX_SEI ||
            pu->nalus[i]->type == OVNALU_SUFFIX_SEI) {

            ret = tmp_sei_wrap(pu->nalus[i], &sei);

            if (ret < 0) {
                goto fail;
            }

            if (slhdr_sei && sei->sei_slhdr) {
                ov_freep(&sei->sei_slhdr);
            }

            if (sei->sei_slhdr) {
                slhdr_sei = 1;
            }

            /* Check SEI SEI */
            ret = pp_process_frame2(pctx, sei, frame_p);

            if (sei) {
                if (sei->sei_fg) {
                    ov_freep(&sei->sei_fg);
                }

                if (sei->sei_slhdr) {
                    ov_freep(&sei->sei_slhdr);
                }
                ov_freep(&sei);
            }
        }

    }
fail:
#endif

    return ret;
}
