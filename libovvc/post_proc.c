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
pp_init_functions(const OVSEI* sei, struct PostProcFunctions *const pp_funcs)
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
            pp_funcs->pp_sdr_to_hdr = pp_sdr_to_hdr;
            pp_funcs->pp_apply_flag = 1;
        } else {
            pp_funcs->pp_sdr_to_hdr = pp_slhdr_no_filter;
        }
#endif
        if (sei->upscale_flag) {
            pp_funcs->pp_apply_flag = 1;
        }
    }
}

int
pp_process_frame2(const OVSEI* sei, OVFrame **frame_p)
{
    int ret=0;
    struct PostProcFunctions pp_funcs;
    uint16_t max_width[3];
    uint16_t max_height[3];

    pp_init_functions(sei, &pp_funcs);

    //TODOpp: switch buffers src and dst when 2 or more post process are applied
    if (pp_funcs.pp_apply_flag){
        OVFrame* frame = *frame_p;
        /* Request a writable picture from same frame pool */
        OVFrame* frame_post_proc = ovframepool_request_frame(frame->internal.frame_pool);
        if (!frame_post_proc) {
            ov_log(NULL, OVLOG_ERROR, "Could not get a writable picture for post processing\n");
            goto no_writable_pic;
        }

        for(int comp = 0; comp < 3; comp++){
            max_width[comp]  = frame_post_proc->width >> !!comp;
            max_height[comp] = frame_post_proc->height >> !!comp;
        }
        frame_post_proc->width = frame->width;
        frame_post_proc->height = frame->height;

        int16_t* srcComp[3] = {(int16_t*)frame->data[0], (int16_t*)frame->data[1], (int16_t*)frame->data[2]};
        int16_t* dstComp[3] = {(int16_t*)frame_post_proc->data[0], (int16_t*)frame_post_proc->data[1], 
            (int16_t*)frame_post_proc->data[2]};

        uint8_t enable_deblock = 1;
        pp_funcs.pp_film_grain(dstComp, srcComp, sei->sei_fg, 
                               frame->width, frame->height, frame->poc, 0, enable_deblock);

#if HAVE_SLHDR
        if(sei->sei_slhdr){
            pp_funcs.pp_sdr_to_hdr(sei->sei_slhdr->slhdr_context, srcComp, dstComp, 
                                   sei->sei_slhdr->payload_array, frame->width, frame->height);
        }
#endif
        if (sei->upscale_flag){
            frame_post_proc->width = max_width[0];
            frame_post_proc->height = max_height[0];
            for(int comp = 0; comp < 3; comp++){
                pp_sample_rate_conv((uint16_t*)frame_post_proc->data[comp], frame_post_proc->linesize[comp]>>1, 
                                    max_width[comp], max_height[comp], 
                                    (uint16_t*)frame->data[comp], frame->linesize[comp]>>1, 
                                    frame->width >> (!!comp), frame->height >> (!!comp), 
                                    &sei->scaling_info, comp == 0 );
            }
        }

        ovframe_unref(frame_p);
        *frame_p = frame_post_proc;
    }

    return ret;

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
#if 1
    OVSEI *sei = NULL;

    for (i = 0; i < pu->nb_nalus; ++i) {
        if (pu->nalus[i]->type == OVNALU_PREFIX_SEI ||
            pu->nalus[i]->type == OVNALU_SUFFIX_SEI) {

            ret = tmp_sei_wrap(pu->nalus[i], &sei);

            if (ret < 0) {
                goto fail;
            }
        }
    }
    /* Check SEI SEI */
    ret = pp_process_frame2(sei, frame_p);

    if (sei) {
        if (sei->sei_fg) {
            ov_freep(&sei->sei_fg);
        }

        if (sei->sei_slhdr) {
            ov_freep(&sei->sei_slhdr);
        }
    }

    ov_free(sei);

fail:
#endif

return ret;
}
