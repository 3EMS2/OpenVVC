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

#include <stdint.h>
#include <string.h>

#if !_WIN32
#include "sys/resource.h"
#endif

#include "ovversion.h"
#include "ovutils.h"
#include "ovmem.h"
#include "overror.h"

#include "ovunits.h"

#include "nvcl.h"
#include "nvcl_utils.h"
#include "ovdec.h"
#include "ctudec.h"
#include "decinit.h"
#include "slicedec.h"
#include "dec_structures.h"
#include "ovdec_internal.h"
#include "post_proc.h"
/* FIXME
 * To be removed includes
 */
#include "ovdpb.h"
#include "ovthreads.h"

static const char *const decname = "Open VVC Decoder";

static const char *option_names[OVDEC_NB_OPTIONS] =
{
    "frame threads",
    "entry threads",
    "upscale_rpr",
    "brightness"
};

static const struct OVOption ovdecopt[] = {
   {"brightness", "Define the target peak luminance of SLHDR library", .type=OVOPT_INT, .min=100, .max=10000,  .offset= offsetof(struct OVDec, ppctx.brightness)},
   {"upscale", "Define if the decoder is responsible of upscaling output pictures when RPR is present", .type=OVOPT_FLAG, .min=0, .max=1,  .offset= offsetof(struct OVDec, ppctx.upscale_flag)},
   {"nopostproc", "Disable picture post processing", .type=OVOPT_FLAG, .min=0, .max=1,  .offset= offsetof(struct OVDec, ppctx.pp_disable)},
   { NULL },
};

static const struct OVOption *
find_opt(const char *const opt_str)
{
    const struct OVOption *opt_list = ovdecopt;
    while (opt_list->opt_str) {
        if (!strcmp(opt_str, opt_list->opt_str)) {
            return opt_list;
        }
        opt_list++;
    }
    return NULL;
}

static int
set_opt_int(OVDec *const ovdec, const struct OVOption *const opt, void *opt_val)
{
    int32_t *dst = (int32_t*)((uint8_t *)ovdec + opt->offset);
    *dst = *((int32_t *)opt_val);
    ov_log(ovdec, OVLOG_TRACE, "Option %s set to %d.\n", opt->opt_str, *dst);
    return 0;
}

static int
set_opt_str(OVDec *const ovdec, const struct OVOption *const opt, void *opt_val)
{
    const char **const dst = (const char **const)((uint8_t *)ovdec + opt->offset);

    *dst = (const char *) opt_val;
    ov_log(ovdec, OVLOG_TRACE, "Option %s set to %s.\n", opt->opt_str, *dst);
    return 0;
}

static int
set_opt_flag(OVDec *const ovdec, const struct OVOption *const opt, void *opt_val)
{
    uint8_t *dst = (uint8_t *)ovdec + opt->offset;
    *dst = *((uint8_t *)opt_val);
    ov_log(ovdec, OVLOG_TRACE, "Option %s set to %d.\n", opt->opt_str, *dst);
    return 0;
}

static int
set_opt(OVDec *const ovdec, const struct OVOption *const opt, void *opt_val)
{
    enum OVOptionType type = opt->type;
    if (!opt_val) goto novalue;
    switch (type) {
        case OVOPT_INT:
            return set_opt_int(ovdec, opt, opt_val);
            break;
        case OVOPT_STR:
            return set_opt_str(ovdec, opt, opt_val);
            break;
        case OVOPT_FLAG:
            return set_opt_flag(ovdec, opt, opt_val);
            break;
    }
    return 0;

novalue:
    ov_log(ovdec, OVLOG_ERROR, "No argument provided for option %s.\n", opt->opt_str);
    return OVVC_EINDATA;
}

int
ovdec_set_opt(OVDec *const ovdec, const char *const opt_str, void *opt_val)
{
    const struct OVOption *opt = find_opt(opt_str);
    if (opt) {
        ov_log(ovdec, OVLOG_TRACE, "Found option %s.\n", opt_str);
        return set_opt(ovdec, opt, opt_val);
    } else {
        ov_log(ovdec, OVLOG_ERROR, "Could not find options in %s.\n", opt_str);

        return OVVC_EINDATA;
    }
    return 0;
}

static void ovdec_uninit_subdec_list(OVDec *ovdec);

static int
ovdec_init_subdec_list(OVDec *ovdec)
{
    int ret;
    ov_log(NULL, OVLOG_TRACE, "Creating %d Slice decoders\n", ovdec->nb_frame_th);
    if (!ovdec->subdec_list)
        ovdec->subdec_list = ov_mallocz(sizeof(OVSliceDec*) * ovdec->nb_frame_th);

    for (int i = 0; i < ovdec->nb_frame_th; ++i){
        ovdec->subdec_list[i] = ov_mallocz(sizeof(OVSliceDec));
        ret = slicedec_init(ovdec->subdec_list[i]);
        if (ret < 0) {
            return OVVC_ENOMEM;
        }
        ovdec->subdec_list[i]->slice_sync.main_thread = &ovdec->main_thread;
    }

    return 0;
}

static void
set_max_pic_part_info(struct PicPartInfo *pic_info, const OVSPS *const sps, const OVPPS *const pps)
{
     /* Masks are to ensure log2_size does not exceed standard requirements */
     uint8_t log2_ctb_s    = (sps->sps_log2_ctu_size_minus5 + 5) & 0x7;
     uint8_t log2_min_cb_s = (sps->sps_log2_min_luma_coding_block_size_minus2 + 2) & 0x7;
     /* FIXME assert log2_min < log2_ctb */

     uint16_t pic_w = sps->sps_pic_width_max_in_luma_samples;
     uint16_t pic_h = sps->sps_pic_height_max_in_luma_samples;

     uint16_t nb_ctb_pic_w = (pic_w + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
     uint16_t nb_ctb_pic_h = (pic_h + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;

     uint16_t nb_pb_pic_w = (nb_ctb_pic_w << log2_ctb_s) >> log2_min_cb_s;
     uint16_t nb_pb_pic_h = (nb_ctb_pic_h << log2_ctb_s) >> log2_min_cb_s;

     pic_info->log2_ctu_s = log2_ctb_s;
     pic_info->log2_min_cb_s = log2_min_cb_s;

     pic_info->pic_w = pic_w;
     pic_info->pic_h = pic_h;

     pic_info->nb_ctb_w = nb_ctb_pic_w;
     pic_info->nb_ctb_h = nb_ctb_pic_h;

     pic_info->nb_pb_w = nb_pb_pic_w;
     pic_info->nb_pb_h = nb_pb_pic_h;

}

static int
update_decoder_buffers(OVDec *ovdec)
{
    if (!ovdec->dpb) {
         int ret = ovdpb_init(&ovdec->dpb, &ovdec->active_params);
         ovdec->active_params.sps_info.req_dpb_realloc = 0;
         if (ret < 0) {
             ov_log(NULL, OVLOG_ERROR, "Failed DPB init\n");
             return ret;
         }
    } else if (ovdec->active_params.sps_info.req_dpb_realloc) {
         dpbpriv_uninit_framepool(&ovdec->dpb->internal);
         int ret = dpbpriv_init_framepool(&ovdec->dpb->internal, ovdec->active_params.sps);
         if (ret < 0) {
             ov_log(NULL, OVLOG_ERROR, "Failed frame pool init\n");
             return ret;
         }
         ovdec->active_params.sps_info.req_dpb_realloc = 0;
    }

    if (ovdec->active_params.sps_info.req_mvpool_realloc) {
        if (ovdec->mv_pool) {
            mvpool_uninit(&ovdec->mv_pool);
        }
        struct PicPartInfo pic_info_max;
        set_max_pic_part_info(&pic_info_max, ovdec->active_params.sps, ovdec->active_params.pps);
        int ret = mvpool_init(&ovdec->mv_pool, &pic_info_max);
        if (ret < 0) {
            ov_log(NULL, OVLOG_ERROR, "Failed pool TMVP buffer pool init\n");
            return ret;
        }
        ovdec->active_params.sps_info.req_mvpool_realloc = 0;
    }

    return 0;
}

OVSliceDec * ovdec_select_subdec(OVDec *const ovdec);

static int
init_vcl_decoder(OVDec *const ovdec, OVSliceDec **sldec_p, const OVNVCLCtx *const nvcl_ctx,
                OVNALUnit * nalu, uint32_t nb_sh_bytes)
{
    int ret = decinit_update_params(&ovdec->active_params, nvcl_ctx);
    if (ret < 0) {
        ov_log(ovdec, OVLOG_ERROR, "Failed to activate parameters\n");
        return ret;
    }

    ret = update_decoder_buffers(ovdec);
    if (ret < 0) {
        return ret;
    }

    OVSliceDec *sldec = ovdec_select_subdec(ovdec);

    *sldec_p = sldec;
    //Temporary: copy active parameters
    slicedec_ref_params(sldec, &ovdec->active_params);

    ret = ovdpb_init_picture(ovdec->dpb, &sldec->pic, &sldec->active_params, nalu->type, sldec, ovdec);
    if (ret < 0) {
        ov_log(NULL, OVLOG_ERROR, "Failed picture init\n");
        return ret;
    }

    ovnalu_ref(&sldec->slice_nalu, nalu);

    ret = slicedec_init_lines(sldec, &sldec->active_params);
    if (ret < 0) {
        ov_log(NULL, OVLOG_ERROR, "Failed line init\n");
        return ret;
    }

    ret = decinit_set_entry_points(&sldec->active_params, nalu, nb_sh_bytes);
    if (ret < 0) {
        ov_log(NULL, OVLOG_ERROR, "Failed entry points init\n");
        return ret;
    }

    return 0;
}

static void
ovdec_wait_entry_thread(OVDec *const ovdec, int i)
{
    #if USE_THREADS
    struct MainThread* th_main = &ovdec->main_thread;
    struct EntryThread *entry_th_list = th_main->entry_threads_list;
    struct EntryThread *entry_th;
    do {

        entry_th = &entry_th_list[i];
        pthread_mutex_lock(&entry_th->entry_mtx);
        if (entry_th->state == IDLE) {
            pthread_mutex_unlock(&entry_th->entry_mtx);
            pthread_mutex_unlock(&th_main->entry_threads_mtx);
            return;
        }
        pthread_mutex_unlock(&entry_th->entry_mtx);

        pthread_cond_wait(&th_main->entry_threads_cnd, &th_main->entry_threads_mtx);

    } while (1);

    #endif
    return;
}

OVSliceDec *
ovdec_select_subdec(OVDec *const ovdec)
{
    OVSliceDec **sldec_list = ovdec->subdec_list;
    int nb_threads = ovdec->nb_frame_th;
    struct MainThread* th_main = &ovdec->main_thread;

    do {
        pthread_mutex_lock(&th_main->io_mtx);

        for(int i = nb_threads - 1; i >= 0 ; i--) {
            OVSliceDec *slicedec            = sldec_list[i];
            struct SliceSynchro* slice_sync = &slicedec->slice_sync;

            pthread_mutex_lock(&slice_sync->gnrl_mtx);
            if (slice_sync->active_state != ACTIVE) {
                if (slice_sync->active_state == DECODING_FINISHED) {
                    ov_log(NULL, OVLOG_ERROR, "Slice state is in decoding finished state at start. This should"
                           "not happen. Please report it.\n");
                }

                slice_sync->active_state = ACTIVE;

                pthread_mutex_unlock(&slice_sync->gnrl_mtx);
                pthread_mutex_unlock(&th_main->io_mtx);

                ov_log(NULL, OVLOG_TRACE, "Subdec %d selected\n", i);

                return slicedec;
            }
            pthread_mutex_unlock(&slice_sync->gnrl_mtx);
        }

        pthread_cond_wait(&th_main->io_cnd, &th_main->io_mtx);
        pthread_mutex_unlock(&th_main->io_mtx);

    } while (!th_main->kill);

    return NULL;
}

static void
ovdec_init_entry_fifo(OVDec *ovdec, int nb_entry_th)
{
    struct MainThread* main_thread = &ovdec->main_thread;
    struct EntriesFIFO *fifo = &main_thread->entries_fifo;

    pthread_mutex_lock(&main_thread->io_mtx);

    fifo->size       = 512;
    fifo->entries    = ov_mallocz(fifo->size * sizeof(struct EntryJob));

    fifo->first = fifo->entries;
    fifo->last  = fifo->entries;

    pthread_mutex_unlock(&main_thread->io_mtx);
}

static void
ovdec_uninit_entry_jobs(OVDec *ovdec)
{
    struct MainThread* main_thread = &ovdec->main_thread;
    ov_freep(&main_thread->entries_fifo);
}

static void
ovdec_wait_entries(OVDec *ovdec)
{
    struct MainThread *th_main = &ovdec->main_thread;
    pthread_mutex_lock(&th_main->io_mtx);
    struct EntriesFIFO *fifo = &th_main->entries_fifo;
    while (fifo->first != fifo->last) {
        pthread_cond_wait(&th_main->io_cnd, &th_main->io_mtx);
    }
    pthread_mutex_unlock(&th_main->io_mtx);
}

static void
ovdec_uninit_entry_threads(OVDec *ovdec)
{
    int i;
    void *ret;
    ov_log(NULL, OVLOG_TRACE, "Deleting %d entry threads\n", ovdec->nb_entry_th);
    struct MainThread *th_main = &ovdec->main_thread;

    /* Wait for the job fifo to be empty before joining entry thread.
    */
    ovdec_wait_entries(ovdec);

    struct EntryThread *entry_threads_list = th_main->entry_threads_list;
    for (i = 0; i < ovdec->nb_entry_th; ++i){
        struct EntryThread *th_entry = &entry_threads_list[i];

        /* Signal and join entry thread.  */
        ovdec_wait_entry_thread(ovdec, i);

        pthread_mutex_lock(&th_entry->entry_mtx);
        th_entry->kill = 1;
        pthread_cond_signal(&th_entry->entry_cnd);
        pthread_mutex_unlock(&th_entry->entry_mtx);

        pthread_join(th_entry->thread, &ret);
        ovthread_uninit_entry_thread(th_entry);
    }
    ov_freep(&entry_threads_list);
}

static int
ovdec_init_entry_threads(OVDec *ovdec, int nb_entry_th)
{
    int i, ret;
    ov_log(NULL, OVLOG_TRACE, "Creating %d entry threads\n", nb_entry_th);
    ovdec->main_thread.entry_threads_list = ov_mallocz(nb_entry_th*sizeof(struct EntryThread));
    for (i = 0; i < nb_entry_th; ++i){
        struct EntryThread *entry_th = &ovdec->main_thread.entry_threads_list[i];

        entry_th->main_thread = &ovdec->main_thread;

        ret = ovthread_init_entry_thread(entry_th);
        if (ret < 0)
            goto failthread;
    }

    return 0;

failthread:
    ov_log(NULL, OVLOG_ERROR,  "Entry threads creation failed\n");
    ovdec_uninit_entry_threads(ovdec);

    return OVVC_ENOMEM;
}

static int
ovdec_init_main_thread(OVDec *ovdec)
{
    struct MainThread* main_thread = &ovdec->main_thread;
    int nb_entry_th = ovdec->nb_entry_th;

    main_thread->nb_entry_th = nb_entry_th;

    pthread_mutex_init(&main_thread->entry_threads_mtx, NULL);
    pthread_cond_init(&main_thread->entry_threads_cnd,  NULL);
    pthread_mutex_init(&main_thread->io_mtx, NULL);
    pthread_cond_init(&main_thread->io_cnd,  NULL);

    ovdec_init_entry_fifo(ovdec, nb_entry_th);
    ovdec_init_entry_threads(ovdec, nb_entry_th);
    return 0;
}

static int
ovdec_uninit_main_thread(OVDec *ovdec)
{
    ovdec_uninit_entry_threads(ovdec);
    ovdec_uninit_entry_jobs(ovdec);

    return 0;
}

static int
submit_slice(OVDec *ovdec, OVNALUnit *nalu, int nb_bytes)
{
    /* Select the first available slice decoder, or wait until one is available */
    OVNVCLCtx *const nvcl_ctx = &ovdec->nvcl_ctx;
    OVSliceDec *sldec = NULL;

    /* Beyond this point unref current picture on failure */
    int ret = init_vcl_decoder(ovdec, &sldec, nvcl_ctx, nalu, nb_bytes);

    if (ret < 0) {
        ov_log(NULL, OVLOG_ERROR, "Error in slice init.\n");
        if (!sldec->pic && ovdec->dpb && ovdec->dpb->active_pic) {
            ovdpb_report_decoded_frame(ovdec->dpb->active_pic);
        }
        slicedec_finish_decoding(sldec);
        goto failvcl;
    }

    ret = slicedec_submit_rect_entries(sldec, &sldec->active_params, ovdec->main_thread.entry_threads_list);
failvcl:
    return ret;
}

static int
decode_nal_unit(OVDec *const ovdec, OVNALUnit *nalu)
{
    enum OVNALUType nalu_type = nalu->type;
    OVNVCLCtx *const nvcl_ctx = &ovdec->nvcl_ctx;
    int ret = 0;

    int nb_bytes = nvcl_decode_nalu_hls_data(nvcl_ctx, nalu);
    if (nb_bytes < 0) {
        goto failhls;
    }

    if (ovnalu_is_vcl(nalu_type)) {
        ret = submit_slice(ovdec, nalu, nb_bytes);
        if (ret < 0) return ret;
    }

//failvcl:
    return ret;

failhls:
    if (ovnalu_is_vcl(nalu_type)) {
        ov_log(NULL, OVLOG_ERROR, "Error in slice reading.\n");
        if (ovdec->dpb && ovdec->dpb->active_pic) {
            ovdpb_report_decoded_frame(ovdec->dpb->active_pic);
        }
        return OVVC_EINDATA;
    }
    return nb_bytes;
}

static int
vvc_decode_picture_unit(OVDec *ovdec, const OVPictureUnit *pu)
{
    int i;
    int ret;
    ovpu_ref(&ovdec->pu, (OVPictureUnit *)pu);
    ov_log(NULL, OVLOG_TRACE, "Picture Unit.\n");
    for (i = 0; i < pu->nb_nalus; ++i) {
        ret = decode_nal_unit(ovdec, pu->nalus[i]);
        if (ret < 0) {
            //goto fail;
        }
    }

    if (ovdec->dpb)
        ovdec->dpb->active_pic = NULL;

    ovpu_unref(&ovdec->pu);
    //printf("PU END\n");
    return 0;

//fail:
    /* Error processing if needed */
    ovdec->dpb->active_pic = NULL;
    ovpu_unref(&ovdec->pu);
    return ret;
}

int
ovdec_submit_picture_unit(OVDec *ovdec, const OVPictureUnit *const pu)
{
    int ret = 0;

    ret = vvc_decode_picture_unit(ovdec, pu);

    return ret;
}

int
ovdec_receive_picture(OVDec *ovdec, OVFrame **frame_p)
{
    struct OVPictureUnit *punit;
    OVDPB *dpb = ovdec->dpb;
    int ret = 0;

    if (!dpb) {
        ov_log(ovdec, OVLOG_TRACE, "No DPB on output request.\n");
        return 0;
    }

    ret = ovdpb_output_pic(dpb, frame_p, &punit);

    if (*frame_p) {
        (*frame_p)->frame_info.color_desc.colour_primaries = ovdec->active_params.sps_info.color_desc.colour_primaries;
        (*frame_p)->frame_info.color_desc.transfer_characteristics = ovdec->active_params.sps_info.color_desc.transfer_characteristics;
        (*frame_p)->frame_info.color_desc.matrix_coeffs = ovdec->active_params.sps_info.color_desc.matrix_coeffs;
        (*frame_p)->frame_info.color_desc.full_range = ovdec->active_params.sps_info.color_desc.full_range;
    }

    if (*frame_p) {
        if (punit && !ovdec->ppctx.pp_disable) {
            pp_process_frame(&ovdec->ppctx, punit, frame_p);
        }
        ovpu_unref(&punit);
    }

    return ret;
}

static void
ovdec_wait_entry_threads(OVDec *ovdec)
{
    int i;

    ovdec_wait_entries(ovdec);

    for (i = 0; i < ovdec->nb_entry_th; ++i){
        ovdec_wait_entry_thread(ovdec, i);
    }
}

int
ovdec_drain_picture(OVDec *ovdec, OVFrame **frame_p)
{
    struct OVPictureUnit *punit;
    OVDPB *dpb = ovdec->dpb;
    int ret;

    ovdec_wait_entry_threads(ovdec);

    if (!dpb) {
        ov_log(ovdec, OVLOG_TRACE, "No DPB on output request.\n");
        return 0;
    }

    ret = ovdpb_drain_frame(dpb, frame_p, &punit);

    if (*frame_p) {
        if (punit && !ovdec->ppctx.pp_disable) {
            pp_process_frame(&ovdec->ppctx, punit, frame_p);
        }
        ovpu_unref(&punit);
    }

    return ret;
}

int
ovdec_flush(OVDec *ovdec)
{
    //ovdec_wait_entry_threads(ovdec);
    struct OVPictureUnit *punit;
    OVFrame *frame;
    OVDPB *dpb = ovdec->dpb;

    fifo_flush(&ovdec->main_thread);

    ovdec_wait_entry_threads(ovdec);
    if (dpb) {
        while (ovdpb_drain_frame(dpb, &frame, &punit)) {
            if (frame) {
                ovpu_unref(&punit);
                ovframe_unref(&frame);
            }
        }
        ovdpb_uninit(&ovdec->dpb);
    }

    decinit_unref_params(&ovdec->active_params);


    return 0;
}

static int
set_nb_entry_threads(OVDec *ovdec, int nb_threads)
{
    ovdec->nb_entry_th = nb_threads;
    ovdec->main_thread.nb_entry_th = nb_threads;

    return 0;
}

static int
set_nb_frame_threads(OVDec *ovdec, int nb_threads)
{
    ovdec->nb_frame_th = nb_threads;

    return 0;
}

int
ovdec_set_option(OVDec *ovdec, enum OVOptions opt_id, int value)
{

    switch (opt_id) {
        case OVDEC_RPR_UPSCALE:
            ovdec->ppctx.upscale_flag = !!value;
            break;
        case OVDEC_NB_ENTRY_THREADS:
            set_nb_entry_threads(ovdec, value);
            break;
        case OVDEC_NB_FRAME_THREADS:
            set_nb_frame_threads(ovdec, value);
            break;
        case OVDEC_BRIGHTNESS:
            ovdec->ppctx.brightness = ov_clip(value, 100, 1000);
            break;
        default :
            if (opt_id < OVDEC_NB_OPTIONS) {
                ov_log(ovdec, OVLOG_ERROR, "Invalid option id %d.", opt_id);
                return OVVC_EINDATA;
            }
            break;
    }
    ov_log(ovdec, OVLOG_VERBOSE, "Option %s set to %d.\n", option_names[opt_id], value);

    return 0;
}

static void
derive_thread_ctx(OVDec *ovdec)
{

#if USE_THREADS
    if (ovdec->nb_entry_th < 1) {
        ovdec->nb_entry_th = get_number_of_cores();
        ov_log(NULL, OVLOG_DEBUG, "Physical cores in platform: %i\n", ovdec->nb_entry_th);
    }

    if (ovdec->nb_frame_th < 1) {
        ovdec->nb_frame_th = ovdec->nb_entry_th;
    } else {
        ovdec->nb_frame_th = OVMIN(ovdec->nb_frame_th, ovdec->nb_entry_th);
    }
#else
    ovdec->nb_entry_th = 1;
    ovdec->nb_frame_th = 1;
#endif

}

int
ovdec_config_threads(OVDec *ovdec, int nb_entry_th, int max_nb_frame_th)
{
    ovdec_set_option(ovdec, OVDEC_NB_FRAME_THREADS, max_nb_frame_th);

    ovdec_set_option(ovdec, OVDEC_NB_ENTRY_THREADS, nb_entry_th);

    return 0;
}

int
ovdec_start(OVDec *ovdec)
{
    int ret;

    derive_thread_ctx(ovdec);

    ret = ovdec_init_subdec_list(ovdec);
    if (ret < 0) {
        return ret;
    }

    ret = ovdec_init_main_thread(ovdec);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int
ovdec_init(OVDec **ovdec_p)
{

    *ovdec_p = ov_mallocz(sizeof(OVDec));

    if (*ovdec_p == NULL) goto fail;

    (*ovdec_p)->name = decname;

    ov_log(NULL, OVLOG_TRACE, "OpenVVC init at %p\n", *ovdec_p);

    (*ovdec_p)->ppctx.brightness = 10000;

    return 0;

fail:
    ov_log(NULL, OVLOG_ERROR, "Failed OpenVVC init\n");
    /* TODO proper error management (ENOMEM)*/
    return -1;
}

static void
ovdec_uninit_subdec_list(OVDec *ovdec)
{
    OVSliceDec *sldec;

    if (ovdec != NULL)
    {
        if (ovdec->subdec_list) {

            ovdec_uninit_main_thread(ovdec);

            for (int i = 0; i < ovdec->nb_frame_th; ++i){
                sldec = ovdec->subdec_list[i];
                slicedec_uninit(&sldec);
            }
            ov_freep(&ovdec->subdec_list);

        }
    }
}

int
ovdec_close(OVDec *ovdec)
{
    int not_dec;
    if (ovdec != NULL) {

        not_dec = ovdec->name != decname;

        if (not_dec) goto fail;

        nvcl_free_ctx(&ovdec->nvcl_ctx);
        decinit_unref_params(&ovdec->active_params);

        ovdec_uninit_subdec_list(ovdec);

        ovdpb_uninit(&ovdec->dpb);

        if (ovdec->mv_pool) {
            mvpool_uninit(&ovdec->mv_pool);
        }

        ov_free(ovdec);

        return 0;
    }

fail:
    ov_log(ovdec, 3, "Trying to close a something not a decoder.\n");
    return -1;
}

void
ovdec_set_log_callback(void (*log_function)(void* ctx, int log_level, const char* log_content, va_list vl))
{
    ovlog_set_callback(log_function);
}

const char *
ovdec_version()
{
    static const char *ov_version = OV_VERSION_STR(VER_MAJOR,VER_MINOR,VER_REVISION,VER_BUILD);
    return ov_version;
}

const char* ovdec_get_version()
{
    return OV_VERSION;
}
