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
    "upscale_rpr"
};

static void ovdec_uninit_subdec_list(OVVCDec *vvcdec);

static int
ovdec_init_subdec_list(OVVCDec *dec)
{
    int ret;
    ov_log(NULL, OVLOG_TRACE, "Creating %d Slice decoders\n", dec->nb_frame_th);
    if (!dec->subdec_list)
        dec->subdec_list = ov_mallocz(sizeof(OVSliceDec*) * dec->nb_frame_th);

    for (int i = 0; i < dec->nb_frame_th; ++i){
        dec->subdec_list[i] = ov_mallocz(sizeof(OVSliceDec));
        ret = slicedec_init(dec->subdec_list[i]);
        if (ret < 0) {
            return OVVC_ENOMEM;
        }
        dec->subdec_list[i]->slice_sync.main_thread = &dec->main_thread;
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
init_vcl_decoder(OVVCDec *const dec, OVSliceDec *sldec, const OVNVCLCtx *const nvcl_ctx,
                OVNALUnit * nalu, uint32_t nb_sh_bytes)
{

    int ret;

    ret = decinit_update_params(&dec->active_params, nvcl_ctx);
    if (ret < 0) {
        ov_log(dec, OVLOG_ERROR, "Failed to activate parameters\n");
        return ret;
    }

    if (!dec->dpb) {
         ret = ovdpb_init(&dec->dpb, &dec->active_params);
         if (ret < 0) {
             return ret;
         }
    } else if (dec->active_params.sps_info.req_dpb_realloc) {
         dpbpriv_uninit_framepool(&dec->dpb->internal);
         ret = dpbpriv_init_framepool(&dec->dpb->internal, dec->active_params.sps);
         if (ret < 0) {
             return ret;
         }
         dec->active_params.sps_info.req_dpb_realloc = 0;
    }

    if (dec->active_params.sps_info.req_mvpool_realloc) {
        if (dec->mv_pool) {
            mvpool_uninit(&dec->mv_pool);
        }
        struct PicPartInfo pic_info_max;
        set_max_pic_part_info(&pic_info_max, dec->active_params.sps, dec->active_params.pps);
        ret = mvpool_init(&dec->mv_pool, &pic_info_max);
        if (ret < 0) {
            return ret;
        }
        dec->active_params.sps_info.req_mvpool_realloc = 0;
    }

    //Temporary: copy active parameters
    slicedec_ref_params(sldec, &dec->active_params);

    ret = ovdpb_init_picture(dec->dpb, &sldec->pic, &sldec->active_params, nalu->type, sldec, dec);
    if (ret < 0) {
        return ret;
    }

    ov_nalu_new_ref(&sldec->slice_nalu, nalu);

    ret = slicedec_init_lines(sldec, &sldec->active_params);
    if (ret < 0) {
        return ret;
    }

    ret = decinit_set_entry_points(&sldec->active_params, nalu, nb_sh_bytes);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static void
ovdec_wait_entry_thread(OVVCDec *const dec, int i)
{
    #if USE_THREADS
    struct MainThread* th_main = &dec->main_thread;
    struct EntryThread *entry_th_list = th_main->entry_threads_list;
    struct EntryThread *entry_th;
    int nb_threads = dec->nb_frame_th;
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
ovdec_select_subdec(OVVCDec *const dec)
{
    OVSliceDec **sldec_list = dec->subdec_list;
    int nb_threads = dec->nb_frame_th;
    struct MainThread* th_main = &dec->main_thread;

    OVSliceDec * slicedec;
    struct SliceSynchro* slice_sync;
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
ovdec_init_entry_fifo(OVVCDec *vvcdec, int nb_entry_th)
{
    struct MainThread* main_thread = &vvcdec->main_thread;
    struct EntriesFIFO *fifo = &main_thread->entries_fifo;

    pthread_mutex_lock(&main_thread->io_mtx);

    fifo->size       = 512;
    fifo->entries    = ov_mallocz(fifo->size * sizeof(struct EntryJob));

    fifo->first = fifo->entries;
    fifo->last  = fifo->entries;

    pthread_mutex_unlock(&main_thread->io_mtx);
}

static void
ovdec_uninit_entry_jobs(OVVCDec *vvcdec)
{
    struct MainThread* main_thread = &vvcdec->main_thread;
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
ovdec_uninit_entry_threads(OVVCDec *vvcdec)
{
    int i;
    void *ret;
    ov_log(NULL, OVLOG_TRACE, "Deleting %d entry threads\n", vvcdec->nb_entry_th);
    struct MainThread *th_main = &vvcdec->main_thread;

    /* Wait for the job fifo to be empty before joining entry thread.
    */
    ovdec_wait_entries(vvcdec);

    struct EntryThread *entry_threads_list = th_main->entry_threads_list;
    for (i = 0; i < vvcdec->nb_entry_th; ++i){
        struct EntryThread *th_entry = &entry_threads_list[i];

        /* Signal and join entry thread.  */
        ovdec_wait_entry_thread(vvcdec, i);

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
ovdec_init_entry_threads(OVVCDec *vvcdec, int nb_entry_th)
{
    int i, ret;
    ov_log(NULL, OVLOG_TRACE, "Creating %d entry threads\n", nb_entry_th);
    vvcdec->main_thread.entry_threads_list = ov_mallocz(nb_entry_th*sizeof(struct EntryThread));
    for (i = 0; i < nb_entry_th; ++i){
        struct EntryThread *entry_th = &vvcdec->main_thread.entry_threads_list[i];

        entry_th->main_thread = &vvcdec->main_thread;

        ret = ovthread_init_entry_thread(entry_th);
        if (ret < 0)
            goto failthread;
    }

    return 0;

failthread:
    ov_log(NULL, OVLOG_ERROR,  "Entry threads creation failed\n");
    ovdec_uninit_entry_threads(vvcdec);

    return OVVC_ENOMEM;
}

static int
ovdec_init_main_thread(OVVCDec *vvcdec)
{
    struct MainThread* main_thread = &vvcdec->main_thread;
    int nb_entry_th = vvcdec->nb_entry_th;

    main_thread->nb_entry_th = nb_entry_th;

    pthread_mutex_init(&main_thread->entry_threads_mtx, NULL);
    pthread_cond_init(&main_thread->entry_threads_cnd,  NULL);
    pthread_mutex_init(&main_thread->io_mtx, NULL);
    pthread_cond_init(&main_thread->io_cnd,  NULL);

    ovdec_init_entry_fifo(vvcdec, nb_entry_th);
    ovdec_init_entry_threads(vvcdec, nb_entry_th);
    return 0;

}

static int
ovdec_uninit_main_thread(OVVCDec *vvcdec)
{
    ovdec_uninit_entry_threads(vvcdec);
    ovdec_uninit_entry_jobs(vvcdec);

    return 0;
}

static int
decode_nal_unit(OVVCDec *const vvcdec, OVNALUnit * nalu)
{
    OVNVCLCtx *const nvcl_ctx = &vvcdec->nvcl_ctx;
    enum OVNALUType nalu_type = nalu->type;
    OVNVCLReader rdr;

    int ret;

    switch (nalu_type) {
    case OVNALU_TRAIL:
    case OVNALU_STSA:
    case OVNALU_RADL:
    case OVNALU_RASL:
    case OVNALU_IDR_W_RADL:
    case OVNALU_IDR_N_LP:
    case OVNALU_CRA:
    case OVNALU_GDR:
        nvcl_reader_init(&rdr, nalu->rbsp_data, nalu->rbsp_size);

        /* FIXME properly read NAL Unit header */
        nvcl_skip_bits(&rdr, 16);

        ret = nvcl_decode_nalu_sh(&rdr, nvcl_ctx, nalu_type);

        if (ret < 0) {
            if (vvcdec->dpb && vvcdec->dpb->active_pic) {
                ovdpb_report_decoded_frame(vvcdec->dpb->active_pic);
            }
            return ret;
        } else {
            /* Select the first available slice decoder, or wait until one is available */
            OVSliceDec *sldec = ovdec_select_subdec(vvcdec);

            uint32_t nb_sh_bytes = nvcl_nb_bytes_read(&rdr);

            /* Beyond this point unref current picture on failure */
            ret = init_vcl_decoder(vvcdec, sldec, nvcl_ctx, nalu, nb_sh_bytes);

            if (ret < 0) {
                ov_log(NULL, OVLOG_ERROR, "Error in slice init.\n");
                if (!sldec->pic && vvcdec->dpb && vvcdec->dpb->active_pic) {
                    ovdpb_report_decoded_frame(vvcdec->dpb->active_pic);
                }
                slicedec_finish_decoding(sldec);
                goto failvcl;
            }

            ret = slicedec_submit_rect_entries(sldec, &sldec->active_params, vvcdec->main_thread.entry_threads_list);
        }

        break;
    case OVNALU_PREFIX_SEI:
    case OVNALU_SUFFIX_SEI:
    case OVNALU_SUFFIX_APS:
    case OVNALU_PREFIX_APS:
    case OVNALU_VPS:
    case OVNALU_SPS:
    case OVNALU_PPS:
    case OVNALU_PH:
    case OVNALU_EOS:
    case OVNALU_EOB:
    case OVNALU_AUD:
    default:
        ret = nvcl_decode_nalu_hls_data(nvcl_ctx, nalu);
        if (ret < 0) {
            goto fail;
        }
        break;
    }

    return 0;

fail:
    return ret;

failvcl:

    return ret;
}

static int
vvc_decode_picture_unit(OVVCDec *dec, const OVPictureUnit *pu)
{
    int i;
    int ret;
    ovpu_new_ref(&dec->pu, pu);
    for (i = 0; i < pu->nb_nalus; ++i) {
        ret = decode_nal_unit(dec, pu->nalus[i]);
        if (ret < 0) {
            //goto fail;
        }
    }
    if (dec->dpb)
        dec->dpb->active_pic = NULL;
    ovpu_unref(&dec->pu);
    return 0;

fail:
    /* Error processing if needed */
    dec->dpb->active_pic = NULL;
    ovpu_unref(&dec->pu);
    return ret;
}

int
ovdec_submit_picture_unit(OVVCDec *vvcdec, const OVPictureUnit *const pu)
{
    int ret = 0;

    ret = vvc_decode_picture_unit(vvcdec, pu);

    return ret;
}

int
ovdec_receive_picture(OVVCDec *dec, OVFrame **frame_p)
{
    struct OVPictureUnit *punit;
    OVDPB *dpb = dec->dpb;
    int ret = 0;

    if (!dpb) {
        ov_log(dec, OVLOG_TRACE, "No DPB on output request.\n");
        return 0;
    }

    ret = ovdpb_output_pic(dpb, frame_p, &punit);

    if (*frame_p) {
        pp_process_frame(&dec->ppctx, punit, frame_p);
        ovpu_unref(&punit);
    }

    if (*frame_p) {
        (*frame_p)->frame_info.color_desc.colour_primaries = dec->active_params.sps_info.color_desc.colour_primaries;
        (*frame_p)->frame_info.color_desc.transfer_characteristics = dec->active_params.sps_info.color_desc.transfer_characteristics;
        (*frame_p)->frame_info.color_desc.matrix_coeffs = dec->active_params.sps_info.color_desc.matrix_coeffs;
        (*frame_p)->frame_info.color_desc.full_range = dec->active_params.sps_info.color_desc.full_range;
    }

    return ret;
}

static void
ovdec_wait_entry_threads(OVVCDec *vvcdec)
{
    struct MainThread *th_main = &vvcdec->main_thread;
    int i;

    ovdec_wait_entries(vvcdec);

    for (i = 0; i < vvcdec->nb_entry_th; ++i){
        ovdec_wait_entry_thread(vvcdec, i);
    }
}

int
ovdec_drain_picture(OVVCDec *dec, OVFrame **frame_p)
{
    struct OVPictureUnit *punit;
    OVDPB *dpb = dec->dpb;
    int ret;

    ovdec_wait_entry_threads(dec);

    if (!dpb) {
        ov_log(dec, OVLOG_TRACE, "No DPB on output request.\n");
        return 0;
    }

    ret = ovdpb_drain_frame(dpb, frame_p, &punit);

    if (*frame_p) {
        pp_process_frame(&dec->ppctx, punit, frame_p);
        ovpu_unref(&punit);
    }

    return ret;
}

int
ovdec_flush(OVVCDec *dec)
{
    //ovdec_wait_entry_threads(dec);
    struct OVPictureUnit *punit;
    OVFrame *frame;
    OVDPB *dpb = dec->dpb;

    fifo_flush(&dec->main_thread);

    ovdec_wait_entry_threads(dec);
    if (dpb) {
        while (ovdpb_drain_frame(dpb, &frame, &punit)) {
            if (frame) {
                ovpu_unref(&punit);
                ovframe_unref(&frame);
            }
        }
        ovdpb_uninit(&dec->dpb);
    }

    decinit_unref_params(&dec->active_params);


    return 0;
}

static int
set_nb_entry_threads(OVVCDec *ovdec, int nb_threads)
{
    ovdec->nb_entry_th = nb_threads;
    ovdec->main_thread.nb_entry_th = nb_threads;

    return 0;
}

static int
set_nb_frame_threads(OVVCDec *ovdec, int nb_threads)
{
    ovdec->nb_frame_th = nb_threads;

    return 0;
}

int
ovdec_set_option(OVVCDec *ovdec, enum OVOptions opt_id, int value)
{

    switch (opt_id) {
        case OVDEC_RPR_UPSCALE:
            ovdec->upscale_flag = !!value;
            break;
        case OVDEC_NB_ENTRY_THREADS:
            set_nb_entry_threads(ovdec, value);
            break;
        case OVDEC_NB_FRAME_THREADS:
            set_nb_frame_threads(ovdec, value);
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
ovdec_init(OVVCDec **ovdec_p)
{

    *ovdec_p = ov_mallocz(sizeof(OVVCDec));

    if (*ovdec_p == NULL) goto fail;

    (*ovdec_p)->name = decname;

    ov_log(NULL, OVLOG_TRACE, "OpenVVC init at %p\n", *ovdec_p);
    return 0;

fail:
    ov_log(NULL, OVLOG_ERROR, "Failed OpenVVC init\n");
    /* TODO proper error management (ENOMEM)*/
    return -1;
}

static void
ovdec_uninit_subdec_list(OVVCDec *vvcdec)
{
    OVSliceDec *sldec;

    if (vvcdec != NULL)
    {
        if (vvcdec->subdec_list) {

            ovdec_uninit_main_thread(vvcdec);

            for (int i = 0; i < vvcdec->nb_frame_th; ++i){
                sldec = vvcdec->subdec_list[i];
                slicedec_uninit(&sldec);
            }
            ov_freep(&vvcdec->subdec_list);

        }
    }
}

int
ovdec_close(OVVCDec *vvcdec)
{
    int not_dec;
    if (vvcdec != NULL) {

        not_dec = vvcdec->name != decname;

        if (not_dec) goto fail;

        nvcl_free_ctx(&vvcdec->nvcl_ctx);
        decinit_unref_params(&vvcdec->active_params);

        ovdec_uninit_subdec_list(vvcdec);

        ovdpb_uninit(&vvcdec->dpb);

        if (vvcdec->mv_pool) {
            mvpool_uninit(&vvcdec->mv_pool);
        }

        ov_free(vvcdec);

        return 0;
    }

fail:
    ov_log(vvcdec, 3, "Trying to close a something not a decoder.\n");
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
