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

#include <pthread.h>
/* FIXME tmp*/
#include <stdatomic.h>

#include "slicedec.h"
#include "overror.h"
#include "ovutils.h"
#include "ovmem.h"
#include "ovthreads.h"
#include "ovdpb.h"

static int
init_pic_border_info(struct RectEntryInfo *einfo, const OVPS *const prms, int entry_idx)
{
    /* Various info on pic_border */
    /* TODO check entry is border pic */
    /*TODO use derived value instead */
    const OVPPS *const pps = prms->pps;
    uint16_t pic_w = pps->pps_pic_width_in_luma_samples;
    uint16_t pic_h = pps->pps_pic_height_in_luma_samples;

    const OVSPS *const sps = prms->sps;
    uint8_t log2_ctb_s = sps->sps_log2_ctu_size_minus5 + 5;

    const int last_ctu_w = pic_w & ((1 << log2_ctb_s) - 1);
    const int last_ctu_h = pic_h & ((1 << log2_ctb_s) - 1);

    int nb_ctb_pic_w = (pic_w + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
    int nb_ctb_pic_h = (pic_h + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;

    /* FIXME report an error when > nb_ctb_pic earlier */
    int pic_last_ctb_x = (einfo->ctb_x + einfo->nb_ctu_w) == nb_ctb_pic_w;
    int pic_last_ctb_y = (einfo->ctb_y + einfo->nb_ctu_h) == nb_ctb_pic_h;

    uint8_t full_ctb_w = (!pic_last_ctb_x || !last_ctu_w);
    uint8_t full_ctb_h = (!pic_last_ctb_y || !last_ctu_h);

    einfo->implicit_w = !full_ctb_w;
    einfo->implicit_h = !full_ctb_h;
    einfo->last_ctu_w = full_ctb_w ? (1 << log2_ctb_s) : last_ctu_w;
    einfo->last_ctu_h = full_ctb_h ? (1 << log2_ctb_s) : last_ctu_h;
    einfo->nb_ctb_pic_w = nb_ctb_pic_w;

    return 0;
}

struct SliceInfo
{
    uint16_t tile_idx;
    uint16_t nb_tiles;

    uint16_t ctb_x;
    uint16_t ctb_y;
    uint16_t w;
    uint16_t h;
};

/* FIXME only once */
static void
setup_slice_prms(OVPPS *const pps, struct PicPartitionInfo *const part_info, const struct TileInfo *const tinfo, uint8_t log2_ctb_s)
{
    struct SliceInfo slice_info[300];
    int tile_id = 0;
    int i;
    uint16_t pic_w = pps->pps_pic_width_in_luma_samples;

    int nb_ctb_pic_w = (pic_w + ((1 << log2_ctb_s) - 1)) >> log2_ctb_s;
    int entry_idx = 0;

    for (i = 0; i < pps->pps_num_slices_in_pic_minus1 + 1; i++) {
        struct SliceInfo *sl = &slice_info[i];

        int tile_x = tile_id % part_info->nb_tile_w;
        int tile_y = tile_id / part_info->nb_tile_w;
        int ctu_x = tinfo->ctu_x[tile_x];
        int ctu_y = tinfo->ctu_y[tile_y];

        uint16_t tile_start = ctu_x + ctu_y * nb_ctb_pic_w;

        sl->tile_idx = tile_id;

        int sl_w = 0;
        int sl_h = 0;

        int sl_w_tile = pps->pps_num_slices_in_pic_minus1 ? pps->pps_slice_width_in_tiles_minus1[i] + 1 : part_info->nb_tile_w;
        int sl_h_tile = pps->pps_num_slices_in_pic_minus1 ? pps->pps_slice_height_in_tiles_minus1[i] + 1 : part_info->nb_tile_h;

        //int sl_w_tile =  pps->pps_slice_width_in_tiles_minus1[i] + 1;
        //int sl_h_tile =  pps->pps_slice_height_in_tiles_minus1[i] + 1;
        int nb_tiles_entries = sl_w_tile * sl_h_tile;

        for (int j = 0; j < sl_w_tile; ++j) {
                for (int k = 0; k < sl_h_tile; ++k) {
                    int tmp_tile_x = tile_x + j;
                    int tmp_tile_y = tile_y + k;
                    int tmp_tile_id = tile_id + j + k * part_info->nb_tile_w;
                    int x = tinfo->ctu_x[tmp_tile_x];
                    int y = tinfo->ctu_y[tmp_tile_y];
                    int w = part_info->tile_col_w[tmp_tile_x];
                    int h = part_info->tile_row_h[tmp_tile_y];
                    if (nb_tiles_entries == 1 && pps->pps_num_exp_slices_in_tile[tmp_tile_id]) {
                        int nb_exp_slices = pps->pps_num_exp_slices_in_tile[tmp_tile_id];
                        int pos_y = y;
                        for (int l = 0; l < nb_exp_slices; l++) {
                            int tmp_h = pps->pps_exp_slice_height_in_ctus_minus1[i + l] + 1;
                            printf("slice %d in tile %d : x %d y %d, wxh %dx%d\n", i + l, tmp_tile_id, x, pos_y, w, tmp_h);
                            pos_y += pps->pps_exp_slice_height_in_ctus_minus1[i + l] + 1;
                            /* store height , w = tile_w*/
                        }
                        int rem_h = h - (pos_y - y);
                        if (rem_h) {
                            int last_read = pps->pps_exp_slice_height_in_ctus_minus1[i + nb_exp_slices - 1] + 1;
                            int nb_implicit_slices = rem_h / last_read + !!(rem_h % last_read);
                            for (int l = 0; l < nb_implicit_slices; l++) {
                            printf("Implicit slice %d in tile %d : x %d y %d, wxh %dx%d\n", i + nb_exp_slices + l, tmp_tile_id, x, pos_y, w, rem_h / last_read);
                            pos_y += rem_h/last_read;
                            }
                        }

                        //i += nb_implicit_slices + nb_exp_slices - 1;
                    } else {
                        //for (int l = 0; l < nb_tiles_entries; ++l) {
                        printf("slice %d: tile : %d x %d, y %d, wxh %dx%d\n", i, tmp_tile_id, x, y, w, h);
                        //}
                    }
                }
        }
#if 0
        /* Each new tile column read slice width exept for implicit last column */
        if (tile_x != part_info->nb_tile_w - 1) {
            for (int k = 0; k <= pps->pps_slice_width_in_tiles_minus1[i]; ++k) {
                sl_w += part_info->tile_col_w[tile_x + k];
            }
            sl_h = part_info->tile_row_h[tile_y];
        }

        /* Each new tile row read slice height except for implicit last row */
        if (tile_y !=  part_info->nb_tile_h - 1) {
            if (pps->pps_tile_idx_delta_present_flag || tile_x == 0) {
                for (int k = 0; k <= pps->pps_slice_height_in_tiles_minus1[i]; ++k) {
                    sl_h += part_info->tile_row_h[tile_y + k];
                }
            }
        }
#endif

        /* Multiple slices in tiles */
        if (nb_tiles_entries == 1) {

            if (part_info->tile_row_h[tile_y] > 1) {
                int sum = 0;
                int j;
                for (j = 0; j < pps->pps_num_exp_slices_in_tile[tile_id]; j++) {
                    sum += pps->pps_exp_slice_height_in_ctus_minus1[i + j] + 1;
                    /* store height , w = tile_w*/
                }

                if (sum < part_info->tile_row_h[tile_y]) {
                    int last_read = pps->pps_exp_slice_height_in_ctus_minus1[i + j - 1] + 1;
                    int rem_ctu_h = part_info->tile_row_h[tile_y] - sum;
                    int nb_implicit_slices = rem_ctu_h / last_read + !!(rem_ctu_h % last_read);
                    /* store height , w = tile_w*/
                    j += nb_implicit_slices;
                }

                i += (j - 1);
            } else {
                 /* height = 1 */
            }
        }

        if (pps->pps_tile_idx_delta_present_flag && i < pps->pps_num_slices_in_pic_minus1) {
            tile_id += pps->pps_tile_idx_delta_val[i];
        } else {
            int offset_y;
            tile_id  += pps->pps_slice_width_in_tiles_minus1[i] + 1;
            if (tile_id % part_info->nb_tile_w == 0) {
                tile_id += pps->pps_slice_height_in_tiles_minus1[i] * part_info->nb_tile_w;
            }
        }
    }
}

static void
slicedec_init_rect_entry(struct RectEntryInfo *einfo, const OVPS *const prms, int entry_idx)
{
    const struct SHInfo *const sh_info     = &prms->sh_info;
    const struct PPSInfo *const pps_info   = &prms->pps_info;
    const struct OVPPS *const pps   = prms->pps;
    const struct TileInfo *const tile_info = &pps_info->tile_info;

    const OVSPS *const sps = prms->sps;
    uint8_t log2_ctb_s = sps->sps_log2_ctu_size_minus5 + 5;

    int tile_x = (entry_idx + prms->sh->sh_slice_address) % tile_info->nb_tile_cols;
    int tile_y = (entry_idx + prms->sh->sh_slice_address) / tile_info->nb_tile_cols;
    tile_y = OVMIN(tile_info->nb_tile_rows - 1, tile_y);

    setup_slice_prms(pps, &pps->part_info, tile_info, log2_ctb_s);

    einfo->tile_x = tile_x;
    einfo->tile_y = tile_y;

    einfo->nb_ctu_w = tile_info->nb_ctu_w[tile_x];
    einfo->nb_ctu_h = tile_info->nb_ctu_h[tile_y];

    einfo->ctb_x = tile_info->ctu_x[tile_x];
    einfo->ctb_y = tile_info->ctu_y[tile_y];

    einfo->entry_start = sh_info->rbsp_entry[entry_idx];
    einfo->entry_end   = sh_info->rbsp_entry[entry_idx + 1];

    init_pic_border_info(einfo, prms, entry_idx);
}

static int
ovthread_decode_entry(struct EntryJob *entry_job, struct EntryThread *entry_th)
{   
    struct SliceSynchro* slice_sync = entry_job->slice_sync;
    uint8_t entry_idx               = entry_job->entry_idx;

    uint16_t nb_entries  = slice_sync->nb_entries;
    ov_log(NULL, OVLOG_DEBUG, "Decoder with POC %d, start entry %d\n", slice_sync->owner->pic->poc, entry_idx);
    
    OVCTUDec *const ctudec  = entry_th->ctudec;
    OVSliceDec *const sldec = slice_sync->owner;
    const OVPS *const prms  = &sldec->active_params;

    slice_sync->decode_entry(sldec, ctudec, prms, entry_idx, &entry_job->einfo);

    uint16_t nb_entries_decoded = atomic_fetch_add_explicit(&slice_sync->nb_entries_decoded, 1, memory_order_acq_rel);

    /* Last thread to exit loop will have entry_idx set to nb_entry - 1*/
    return nb_entries_decoded == nb_entries - 1 ;
}

void
fifo_flush(struct MainThread* main_thread)
{
    struct EntriesFIFO *fifo = &main_thread->entries_fifo;
    struct EntryJob *entry_job = NULL;
    pthread_mutex_lock(&main_thread->io_mtx);

    fifo->first = fifo->last;

    pthread_mutex_unlock(&main_thread->io_mtx);
}

static struct EntryJob *
fifo_pop_entry(struct EntriesFIFO *fifo)
{
    struct EntryJob *entry_job = NULL;

    if (fifo->first != fifo->last) {
        ptrdiff_t position = fifo->first - fifo->entries;
        uint16_t size = fifo->size;

        entry_job = fifo->first;

        fifo->first = &fifo->entries[(position + 1) % size];

    }
    return entry_job;
}

static void *
entry_thread_main_function(void *opaque)
{
    struct EntryThread *entry_th = (struct EntryThread *)opaque;
    struct MainThread* main_thread = entry_th->main_thread;

    pthread_mutex_lock(&entry_th->entry_mtx);
    entry_th->state = ACTIVE;
    pthread_mutex_unlock(&entry_th->entry_mtx);

    while (!entry_th->kill){
        struct EntriesFIFO *fifo = &main_thread->entries_fifo;

        pthread_mutex_lock(&main_thread->io_mtx);

        struct EntryJob entry_jobtmp;

        struct EntryJob *entry_job = NULL;
        struct EntryJob *entry_job2 = fifo_pop_entry(fifo);
        if (entry_job2) {
           entry_jobtmp = *entry_job2;
           entry_job = &entry_jobtmp;
        }

        pthread_cond_signal(&main_thread->entry_threads_cnd);
        pthread_mutex_unlock(&main_thread->io_mtx);

        if (entry_job) {
            slicedec_update_entry_decoder(entry_job->slice_sync->owner, entry_th->ctudec);
            pthread_mutex_lock(&entry_th->entry_mtx);
            pthread_mutex_unlock(&entry_th->entry_mtx);

            uint8_t is_last = ovthread_decode_entry(entry_job, entry_th);

            /* Check if the entry was the last of the slice */
            if (is_last) {
                slicedec_finish_decoding(entry_job->slice_sync->owner);
            }
        } else {
            pthread_mutex_lock(&main_thread->entry_threads_mtx);
            pthread_mutex_lock(&entry_th->entry_mtx);
            entry_th->state = IDLE;

            pthread_cond_signal(&main_thread->entry_threads_cnd);
            pthread_mutex_unlock(&main_thread->entry_threads_mtx);

            pthread_cond_wait(&entry_th->entry_cnd, &entry_th->entry_mtx);
            entry_th->state = ACTIVE;
            pthread_mutex_unlock(&entry_th->entry_mtx);
        }
    }
    return NULL;
}

int
ovthread_init_entry_thread(struct EntryThread *entry_th)
{
    entry_th->state = IDLE;
    entry_th->kill  = 0;

    int ret = ctudec_init(&entry_th->ctudec);
    if (ret < 0) {
        ov_log(NULL, OVLOG_ERROR, "Failed line decoder initialisation\n");
        ctudec_uninit(entry_th->ctudec);
        return OVVC_ENOMEM;
    }

    pthread_mutex_init(&entry_th->entry_mtx, NULL);
    pthread_cond_init(&entry_th->entry_cnd, NULL);
    pthread_mutex_lock(&entry_th->entry_mtx);

#if USE_THREADS
    if (pthread_create(&entry_th->thread, NULL, entry_thread_main_function, entry_th)) {
        pthread_mutex_unlock(&entry_th->entry_mtx);
        ov_log(NULL, OVLOG_ERROR, "Thread creation failed at decoder init\n");
        return OVVC_ENOMEM;
    }
#endif
    pthread_mutex_unlock(&entry_th->entry_mtx);
    return 1;
}

void
ovthread_uninit_entry_thread(struct EntryThread *entry_th)
{       
    pthread_mutex_destroy(&entry_th->entry_mtx);
    pthread_cond_destroy(&entry_th->entry_cnd);

    ctudec_uninit(entry_th->ctudec);
}

static int
fifo_push_entry(struct EntriesFIFO *fifo,
                struct SliceSynchro *slice_sync, int entry_idx)
{
    ptrdiff_t position = fifo->last - fifo->entries;
    ptrdiff_t next_pos = (position + 1) % fifo->size;
    do {
        if (&fifo->entries[next_pos] != fifo->first) {
            struct EntryJob *entry_job = &fifo->entries[position];

            slicedec_init_rect_entry(&entry_job->einfo, &slice_sync->owner->active_params, entry_idx);

            entry_job->entry_idx  = entry_idx;
            entry_job->slice_sync = slice_sync;

            fifo->last = &fifo->entries[next_pos];
            return 0;
        } else {
            struct EntryThread *entry_threads_list = slice_sync->main_thread->entry_threads_list;
            for (int i = 0; i < slice_sync->main_thread->nb_entry_th; ++i){
                struct EntryThread *th_entry = &entry_threads_list[i];
                pthread_mutex_lock(&th_entry->entry_mtx);
                pthread_cond_signal(&th_entry->entry_cnd);
                pthread_mutex_unlock(&th_entry->entry_mtx);
            }
            pthread_cond_wait(&slice_sync->main_thread->entry_threads_cnd, &slice_sync->main_thread->io_mtx);
        }
    }while (1);
}

/*
Functions needed for the synchro of threads decoding the slice
*/
int
ovthread_slice_add_entry_jobs(struct SliceSynchro *slice_sync, DecodeFunc decode_entry, int nb_entries)
{
    struct MainThread* main_thread = slice_sync->main_thread;
    struct EntriesFIFO *entry_fifo = &main_thread->entries_fifo;

    slice_sync->nb_entries = nb_entries;
    slice_sync->decode_entry = decode_entry;

    atomic_store_explicit(&slice_sync->nb_entries_decoded, 0, memory_order_relaxed);

    pthread_mutex_lock(&main_thread->io_mtx);
    for (int i = 0; i < nb_entries; ++i) {
        fifo_push_entry(entry_fifo, slice_sync, i);
        ov_log(NULL, OVLOG_DEBUG, "Main adds POC %d entry %d\n", slice_sync->owner->pic->poc, i);
    }
    pthread_mutex_unlock(&main_thread->io_mtx);

    struct EntryThread *entry_threads_list = main_thread->entry_threads_list;
    for (int i = 0; i < main_thread->nb_entry_th; ++i){
        struct EntryThread *th_entry = &entry_threads_list[i];
        pthread_mutex_lock(&th_entry->entry_mtx);
        pthread_cond_signal(&th_entry->entry_cnd);
        pthread_mutex_unlock(&th_entry->entry_mtx);
    }

    return 0;
}

int
ovthread_slice_sync_init(struct SliceSynchro *slice_sync)
{   
    atomic_init(&slice_sync->nb_entries_decoded, 0);

    pthread_mutex_init(&slice_sync->gnrl_mtx, NULL);
    pthread_cond_init(&slice_sync->gnrl_cnd,  NULL);

    return 0;
}

void
ovthread_slice_sync_uninit(struct SliceSynchro *slice_sync)
{   
    OVSliceDec * slicedec = slice_sync->owner;

    pthread_mutex_destroy(&slice_sync->gnrl_mtx);
    pthread_cond_destroy(&slice_sync->gnrl_cnd);

}

