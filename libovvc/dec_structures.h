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

#ifndef DEC_STRUCTURES_H
#define DEC_STRUCTURES_H

#include <stdio.h>
#include <pthread.h>

#include "ovdefs.h"
#include "nvcl.h"
#include "post_proc.h"

#define OV_BOUNDARY_LEFT_RECT      (1 << 1)
#define OV_BOUNDARY_UPPER_RECT     (1 << 3)
#define OV_BOUNDARY_RIGHT_RECT     (1 << 5)
#define OV_BOUNDARY_BOTTOM_RECT    (1 << 7)

#define RPR_SCALE_BITS 14

struct MVPool;

enum OVOptionType{
    OVOPT_INT,
    OVOPT_STR,
    OVOPT_FLAG,
};

struct OVOption
{
    const char *const opt_str;
    const char *const desc;
    enum OVOptionType type;
    union {
        uint8_t flag;
        const char *const string;
        int32_t integer;
    } default_value;

    int32_t min;
    int32_t max;

    size_t offset;
};

struct RectEntryInfo {
    int tile_x;
    int tile_y;
    int ctb_x;
    int ctb_y;
    int nb_ctu_w;
    int nb_ctu_h;
    const uint8_t *entry_start;
    const uint8_t *entry_end;
    uint8_t implicit_h;
    uint8_t implicit_w;
    int last_ctu_w;
    int last_ctu_h;
    int nb_ctb_pic_w;
};

struct OVPartInfo
{
    /**
     * Global limit on cb size
     */
    uint8_t log2_ctu_s;
    uint8_t log2_min_cb_s;

    /**
     *  Quad tree limits in depth an log2_min size
     */
    uint8_t log2_min_qt_s;

    /**
     *  Multi type tree limits in depth and max size
     */
    uint8_t max_mtt_depth;
    uint8_t log2_max_bt_s;
    uint8_t log2_max_tt_s;

    /**
     * Transform tree limits
     */
    uint8_t log2_max_tb_s;
};

struct OVChromaQPTable{
    int8_t qp[64];
};

/* Main decoder structure */
struct SPSInfo
{
    struct OVPartInfo part_info[4];

    /* Chroma QP tables */
    struct OVChromaQPTable qp_tables_c[3];

    struct {
        uint8_t colour_primaries;
        uint8_t transfer_characteristics;
        uint8_t matrix_coeffs;
        uint8_t full_range;
    } color_desc;

    uint8_t req_dpb_realloc;
    uint8_t req_mvpool_realloc;
};

struct PPSInfo
{
    /* Sub Picture / Tiles overrides */
    struct TileInfo {
        int16_t nb_ctu_w[16];
        int16_t nb_ctu_h[16];
        int16_t ctu_x[16];
        int16_t ctu_y[16];
        uint8_t nb_tile_rows;
        uint8_t nb_tile_cols;
    } tile_info;
};

struct SHInfo
{
    /* Entries points in  RBSP */
    const uint8_t *rbsp_entry[257];
    uint16_t nb_entries;
};

enum SAOType {
    SAO_BAND = 1,
    SAO_EDGE = 2,
};

enum SAOModeMergeTypes
{
    NUM_SAO_MERGE_TYPES=0,
    SAO_MERGE_LEFT,
    SAO_MERGE_ABOVE
};

typedef struct SAOParamsCtu
{
    /* SAO types 2 first bits for luma third and fourth bits for chroma */
    uint8_t sao_ctu_flag;
    /* Edge direction or band position according to SAO type */
    uint8_t mode_info[3];

    int8_t  offset[3][4];

} SAOParamsCtu;

typedef struct ALFParamsCtu
{
    uint8_t ctb_alf_flag;
    uint8_t ctb_alf_idx;
    uint8_t cb_alternative;
    uint8_t cr_alternative;
    uint8_t cb_ccalf;
    uint8_t cr_ccalf;
} ALFParamsCtu;

struct MainThread
{
    int kill;

    /*List of entry threads*/
    int nb_entry_th;
    struct EntryThread *entry_threads_list;
    pthread_mutex_t entry_threads_mtx;
    pthread_cond_t entry_threads_cnd;

    /*FIFO of entry jobs*/
    struct EntriesFIFO {
        struct EntryJob *entries;
        struct EntryJob *first;
        struct EntryJob *last;
        uint16_t size;
    } entries_fifo;
    
    pthread_mutex_t io_mtx;
    pthread_cond_t io_cnd;
};

struct PicPartInfo
{
    uint16_t pic_w;
    uint16_t pic_h;

    uint16_t nb_ctb_w;
    uint16_t nb_ctb_h;

    uint16_t nb_pb_w;
    uint16_t nb_pb_h;

    uint8_t log2_min_cb_s;
    uint8_t log2_ctu_s;
};

struct OVDec
{
    const char *name;

    /* Parameters sets context */
    OVNVCLCtx nvcl_ctx;

    struct OVPS {
        /* Pointers to active parameter sets */
        struct HLSDataRef *sps_ref;
        struct HLSDataRef *pps_ref;
        struct HLSDataRef *ph_ref;
        struct HLSDataRef *sh_ref;
        struct HLSDataRef *aps_alf_ref[8];
        struct HLSDataRef *aps_alf_c_ref;
        struct HLSDataRef *aps_cc_alf_cb_ref;
        struct HLSDataRef *aps_cc_alf_cr_ref;
        struct HLSDataRef *aps_lmcs_ref;
        struct HLSDataRef *aps_scaling_list_ref;

        const OVSPS *sps;
        const OVPPS *pps;
        const OVAPS *aps_alf[8];
        const OVAPS *aps_alf_c;
        const OVAPS *aps_cc_alf_cb;
        const OVAPS *aps_cc_alf_cr;
        const OVAPS *aps_lmcs;
        const OVAPS *aps_scaling_list;
        const OVPH *ph;
        const OVSH *sh;

        /* Human readable information from active parameter sets */
        struct SPSInfo sps_info;
        struct PPSInfo pps_info;
        struct SHInfo sh_info;

    } active_params;

    struct OVPictureUnit* pu;

    struct PostProcessCtx {
        //Boolean: output video upscaled to max resolution
        uint8_t upscale_flag;
        int brightness;

    } ppctx;
    
    OVDPB *dpb;

    struct MVPool *mv_pool;

    /* List of Sub Decoders
     * Contains context for Tile / Slice / Picture / SubPicture
     * decoding
     */
    OVSliceDec **subdec_list;

    /* Number of available threads */
    int nb_frame_th;
    int nb_entry_th;

    struct MainThread main_thread;
    /* Informations on decoder behaviour transmitted by user
     */
    struct {
        int opt1;
    }options;

};

#endif
