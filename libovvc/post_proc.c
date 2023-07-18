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
pp_uninit(struct PostProcessCtx *ppctx)
{
    if (ppctx->slhdr_ctx) {
#if HAVE_SLHDR
        pp_uninit_slhdr_lib(ppctx->slhdr_ctx);
        ppctx->slhdr_ctx = NULL;
#endif
    }
}

int
pp_function(const struct PostProcessCtx *const ppctx, const struct Frame *const src, struct Frame *dst, void *params);

static int
upscale(const struct PostProcessCtx *const ppctx, const struct Frame *const src, struct Frame *dst, void *params)
{
    for(int comp = 0; comp < 3; comp++){
        uint16_t dst_w = dst->width  >> (!!comp);
        uint16_t dst_h = dst->height >> (!!comp);
        uint16_t src_w = src->width  >> (!!comp);
        uint16_t src_h = src->height >> (!!comp);

        uint16_t dst_stride =  dst->linesize[comp] >> 1;
        uint16_t src_stride = src->linesize[comp] >> 1;

        uint8_t is_luma = comp == 0;

        pp_sample_rate_conv((uint16_t*)dst->data[comp], dst_stride, dst_w, dst_h,
                            (uint16_t*)src->data[comp], src_stride, src_w, src_h,
                            &src->scaling_window, is_luma, &src->frame_info);
    }
    return 0;
}

static int
film_grain(const struct PostProcessCtx *const ppctx, const struct Frame *const src, struct Frame *dst, void *params)
{
    uint8_t enable_deblock = 1;

    fg_grain_apply_pic((int16_t **)dst->data, (int16_t **)src->data, params,
                       src->width, src->height,
                       src->poc, 0, enable_deblock);

    return 0;
}

#if HAVE_SLHDR
static int
pp_slhdr(const struct PostProcessCtx *const ppctx,
         const struct Frame *const src, struct Frame *dst, void *params)
{
    static const struct ColorDescription pq_bt2020 = {.colour_primaries = 9, .matrix_coeffs = 9, .transfer_characteristics=16, .full_range = 0} ;

    struct OVSEISLHDR *const slhdr_sei = params;

    ov_log (NULL, OVLOG_WARNING, "Updating SLHDR peak luminance %d\n", ppctx->brightness);

    pp_set_display_peak(ppctx->slhdr_ctx, ppctx->brightness);

    pp_sdr_to_hdr(ppctx->slhdr_ctx, (int16_t **)src->data, (int16_t **)dst->data,
                  slhdr_sei->payload_array, src->width, src->height);

    dst->frame_info.color_desc = pq_bt2020;

    dst->width  = src->width;
    dst->height = src->height;

    return 0;
}
#endif

int
pp_process_frame2(struct PostProcessCtx *ppctx, const OVSEI* sei, OVFrame **frame_p)
{
    uint8_t pp_apply_flag = (sei && (sei->sei_fg || sei->sei_slhdr)) || ppctx->upscale_flag;

    /* FIXME  find another place to init this */
    if (sei->br_scale) {
        ov_log (NULL, OVLOG_WARNING, "BR SCALE %d\n", sei->br_scale);
        ppctx->brightness =  sei->br_scale;
    }

    if (pp_apply_flag) {
        OVFrame* src_frm = *frame_p;

        /* Request a writable picture from same src_frm pool */
        OVFrame* pp_frm = ovframepool_request_frame(src_frm->internal.frame_pool);
        if (!pp_frm) {
            ov_log(NULL, OVLOG_ERROR, "Could not get a writable picture for post processing\n");
            goto no_writable_pic;
        }

        if (ppctx->upscale_flag) {
            upscale(ppctx, src_frm, pp_frm, NULL);
        }

        if (sei) {

            if (sei->sei_fg) {
                film_grain(ppctx, src_frm, pp_frm, sei->sei_fg);
            } else
#if HAVE_SLHDR
            if (sei->sei_slhdr) {

                if (!ppctx->slhdr_ctx) {
                    ov_log (NULL, OVLOG_DEBUG, "Init SLHDR Post Processor with peak luminance: %d\n",
                            ppctx->brightness);
                    pp_init_slhdr_lib(&ppctx->slhdr_ctx);
                    pp_set_display_peak(ppctx->slhdr_ctx, ppctx->brightness);
                }

                pp_slhdr(ppctx, src_frm, pp_frm, sei->sei_slhdr);

            }
#endif
            else {
                ov_log (NULL, OVLOG_DEBUG, "No SLHDR SEI\n");
            }
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


static int tmp_sei_wrap(OVNALUnit *const nalu, OVSEI *dst)
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

    for (i = 0; i < pu->nb_nalus; ++i) {
        if (pu->nalus[i]->type == OVNALU_PREFIX_SEI ||
            pu->nalus[i]->type == OVNALU_SUFFIX_SEI) {

            OVSEI *sei = ov_mallocz(sizeof(struct OVSEI));
            if (!sei) {
                ov_log(NULL, OVLOG_ERROR, "Could not alloc SEI.\n");
            }

            ret = tmp_sei_wrap(pu->nalus[i], sei);

            if (ret < 0) {
                ov_log(NULL, OVLOG_ERROR, "Error reading SEI\n");
            }

            if (slhdr_sei && sei->sei_slhdr) {
                ov_freep(&sei->sei_slhdr);
                ov_freep(&sei);
                ov_log(NULL, OVLOG_ERROR, "Skip duplicated SLHDR SEI\n");
                continue;
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

    return ret;
}
