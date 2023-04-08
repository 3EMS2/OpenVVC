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

#include "overror.h"
#include "ovmem.h"
#include "ovutils.h"

#include "nvcl.h"
#include "nvcl_utils.h"
#include "hls_structures.h"

#define NB_ARRAY_ELEMS(x) sizeof(x)/sizeof(*(x))


typedef int (*NALUnitAction)(OVNVCLCtx *const nvcl_ctx, OVNALUnit *const nalu);
static const char *nalu_name[32] =
{
    "TRAIL",
    "STSA",
    "RADL",
    "RASL",
    "RSVD_VCL",
    "RSVD_VCL",
    "RSVD_VCL",
    "IDR_W_RADL",
    "IDR_N_LP",
    "CRA",
    "GDR",
    "RSVD_IRAP_VCL",
    "OPI",
    "DCI",
    "VPS",
    "SPS",
    "PPS",
    "PREFIX_APS",
    "SUFFIX_APS",
    "PH",
    "AUD",
    "EOS",
    "EOB",
    "PREFIX_SEI",
    "SUFFIX_SEI",
    "FD",
    "RSVD_NVCL",
    "RSVD_NVCL",
    "UNSPEC",
    "UNSPEC",
    "UNSPEC",
    "UNSPEC"
};

static const struct HLSReader todo;
extern const struct HLSReader vps_manager;
extern const struct HLSReader sps_manager;
extern const struct HLSReader pps_manager;
extern const struct HLSReader ph_manager;
extern const struct HLSReader sh_manager;
extern const struct HLSReader aps_rdr;

static const struct HLSReader *nalu_reader[32] =
{
    &sh_manager          , /* TRAIL */
    &sh_manager          , /* STSA */
    &sh_manager          , /* RADL */
    &sh_manager          , /* RASL */
    &todo                , /* RSVD_VCL */
    &todo                , /* RSVD_VCL */
    &todo                , /* RSVD_VCL */
    &sh_manager          , /* IDR_W_RADL */
    &sh_manager          , /* IDR_N_LP */
    &sh_manager          , /* CRA */
    &sh_manager          , /* GDR */
    &todo                , /* RSVD_IRAP_VCL */
    &todo                , /* OPI */
    &todo                , /* DCI */
    &vps_manager         , /* VPS */
    &sps_manager         , /* SPS */
    &pps_manager         , /* PPS */
    &aps_rdr             , /* PREFIX_APS */
    &aps_rdr             , /* SUFFIX_APS */
    &ph_manager          , /* PH */
    &todo                , /* AUD */
    &todo                , /* EOS */
    &todo                , /* EOB */
    &todo                , /* PREFIX_SEI */
    &todo                , /* SUFFIX_SEI */
    &todo                , /* FD */
    &todo                , /* RSVD_NVCL */
    &todo                , /* RSVD_NVCL */
    &todo                , /* UNSPEC */
    &todo                , /* UNSPEC */
    &todo                , /* UNSPEC */
    &todo                  /* UNSPEC */
};

void
hlsdata_unref(struct HLSDataRef **dataref_p)
{
    struct HLSDataRef *dataref = *dataref_p;
    if (dataref) {
        unsigned ref_count = atomic_fetch_add_explicit(&dataref->ref_count, -1, memory_order_acq_rel);
        if (!ref_count) {
            dataref->free(dataref_p, dataref->opaque);
        }
    }

    *dataref_p = NULL;
}

void
hlsdata_ref_default_free(struct HLSDataRef **ref_p, void *opaque)
{
    if (ref_p) {
        struct HLSDataRef *to_free = *ref_p;
        if (to_free->data) {
            ov_freep(&to_free->data);
        }
        ov_freep(ref_p);
    }
}

struct HLSDataRef *
hlsdataref_create(union HLSData *data, void (*free)(struct HLSDataRef **ref, void *opaque), void *opaque)
{
   struct HLSDataRef *new = ov_mallocz(sizeof(struct HLSDataRef));

   if (new) {
       atomic_init(&new->ref_count, 0);
       new->free = free ? free : &hlsdata_ref_default_free;
       new->opaque = opaque;
       new->data = data;
       new->data_ref = new;
   }

   return new;
}

int
hlsdata_newref(struct HLSDataRef **dst_p, struct HLSDataRef *src)
{
    atomic_fetch_add_explicit(&src->ref_count, 1, memory_order_acq_rel);

    *dst_p = src;

    return 0;
}

void
nvcl_free_ctx(OVNVCLCtx *const nvcl_ctx)
{
    int i;
    int nb_elems = NB_ARRAY_ELEMS(nvcl_ctx->vps_list);
    for (i = 0; i < nb_elems; ++i) {
        hlsdata_unref(&nvcl_ctx->vps_list[i]);
    }

    nb_elems = NB_ARRAY_ELEMS(nvcl_ctx->sps_list);
    for (i = 0; i < nb_elems; ++i) {
        hlsdata_unref(&nvcl_ctx->sps_list[i]);
    }

    nb_elems = NB_ARRAY_ELEMS(nvcl_ctx->pps_list);
    for (i = 0; i < nb_elems; ++i) {
        hlsdata_unref(&nvcl_ctx->pps_list[i]);
    }

    nb_elems = NB_ARRAY_ELEMS(nvcl_ctx->aps_list[0]);
    for (i = 0; i < nb_elems; ++i) {
        hlsdata_unref(&nvcl_ctx->aps_list[0][i]);
    }

    nb_elems = NB_ARRAY_ELEMS(nvcl_ctx->aps_list[1]);
    for (i = 0; i < nb_elems; ++i) {
        hlsdata_unref(&nvcl_ctx->aps_list[1][i]);
    }

    nb_elems = NB_ARRAY_ELEMS(nvcl_ctx->aps_list[2]);
    for (i = 0; i < nb_elems; ++i) {
        hlsdata_unref(&nvcl_ctx->aps_list[2][i]);
    }

    if (nvcl_ctx->ph) {
        hlsdata_unref(&nvcl_ctx->ph);
    }

    if (nvcl_ctx->sh) {
        hlsdata_unref(&nvcl_ctx->sh);
    }
}

static int
hls_replace_ref(const struct HLSReader *const hls_hdl, struct HLSDataRef **storage, const union HLSData *const data)
{
    union HLSData *tmp = ov_malloc(hls_hdl->data_size);
    if (!tmp) {
        return OVVC_ENOMEM;
    }
    memcpy(tmp, data, hls_hdl->data_size);

    hlsdata_unref(storage);
    *storage = hlsdataref_create(tmp, NULL, NULL);

    if (!*storage) {
        ov_free(tmp);
        return OVVC_ENOMEM;
    }
    return 0;
}

static int
decode_nalu_hls_data(OVNVCLCtx *const nvcl_ctx, struct HLSDataRef **storage, OVNVCLReader *const rdr,
                     const struct HLSReader *const hls_hdl, uint8_t nalu_type)
{
    union HLSData data;
    int ret;

    if (*storage) {
        /* TODO compare RBSP data to avoid new read */
        uint8_t identical_rbsp = 0;
        if (identical_rbsp) goto duplicated;
    }

    memset(&data, 0, hls_hdl->data_size);
    ov_log(NULL, OVLOG_TRACE, "Reading new %s\n", hls_hdl->name);

    ret = hls_hdl->read(rdr, &data, nvcl_ctx, nalu_type);
    if (ret < 0)  goto failread;

    if (nalu_type >= OVNALU_OPI) {
        uint32_t nb_bits_read = nvcl_nb_bits_read(rdr) + 1;
        uint32_t stop_bit_pos = nvcl_find_rbsp_stop_bit(rdr);
        if (stop_bit_pos != nb_bits_read) {

            ov_log(NULL, OVLOG_ERROR, "%s rbsp_stop_bit mismatch: cursor at %d,  expected %d\n", hls_hdl->name, nb_bits_read, stop_bit_pos);
            return OVVC_EINDATA;
        }
    }

    ov_log(NULL, OVLOG_TRACE, "Checking %s\n", hls_hdl->name);
    ret = hls_hdl->validate(rdr, &data);
    if (ret < 0)  goto invalid;

    ov_log(NULL, OVLOG_TRACE, "Replacing new %s\n", hls_hdl->name);
    ret = hls_replace_ref(hls_hdl, storage, &data);
    if (ret < 0)  goto reffail;

    return nvcl_nb_bytes_read(rdr);

invalid:
    ov_log(NULL, OVLOG_ERROR, "Invalid %s\n", hls_hdl->name);
    return ret;

failread:
    ov_log(NULL, OVLOG_ERROR, "Error while reading %s\n", hls_hdl->name);
    return ret;

reffail:
    ov_log(NULL, OVLOG_ERROR, "Error while storing %s\n", hls_hdl->name);
    return ret;

duplicated:
    ov_log(NULL, OVLOG_TRACE, "Ignored duplicated %s\n", hls_hdl->name);
    return 0;

}

static int warn_unsupported(OVNVCLCtx *const nvcl_ctx, OVNALUnit *const nalu)
{
    ov_log(NULL, OVLOG_WARNING, "Unsupported %s NAL unit.\n", nalu_name[nalu->type & 0x1F]);
    return 0;
}

static int warn_unspec(OVNVCLCtx *const nvcl_ctx, OVNALUnit *const nalu)
{
    ov_log(NULL, OVLOG_WARNING, "Unspec %s NAL unit.\n", nalu_name[nalu->type & 0x1F]);
    return 0;
}

static int log_ignored(OVNVCLCtx *const nvcl_ctx, OVNALUnit *const nalu)
{
    ov_log(NULL, OVLOG_TRACE, "Ignored %s NAL unit.\n", nalu_name[nalu->type & 0x1F]);
    return 0;
}

static int decode_nvcl_hls(OVNVCLCtx *const nvcl_ctx, OVNALUnit *const nalu)
{
    uint8_t nalu_type = nalu->type & 0x1F;
    const struct HLSReader *const hls_reader = nalu_reader[nalu_type];
    OVNVCLReader rdr;

    nvcl_reader_init(&rdr, nalu->rbsp_data, nalu->rbsp_size);

    nvcl_skip_bits(&rdr, 16);

    if (hls_reader != &todo) {
        struct HLSDataRef **storage = hls_reader->find_storage(&rdr, nvcl_ctx);
        int ret =  decode_nalu_hls_data(nvcl_ctx, storage, &rdr, hls_reader, nalu_type);
        if (*storage) {
            hlsdata_newref((struct HLSDataRef **)&nalu->hls_data, *storage);
        }

        return ret;
    }

    return 0;
}

#if 0
static int tmp_sei_wrap(OVNVCLCtx *const nvcl_ctx, OVNALUnit *const nalu)
{
    uint8_t nalu_type = nalu->type & 0x1F;
    OVNVCLReader rdr;

    nvcl_reader_init(&rdr, nalu->rbsp_data, nalu->rbsp_size);

    nvcl_skip_bits(&rdr, 16);

    return nvcl_decode_nalu_sei(nvcl_ctx, &rdr, nalu_type);
}
#endif

static const NALUnitAction nalu_action[32] =
{
    &decode_nvcl_hls                , /* TRAIL */
    &decode_nvcl_hls                , /* STSA */
    &decode_nvcl_hls                , /* RADL */
    &decode_nvcl_hls                , /* RASL */
    &warn_unspec                , /* RSVD_VCL */
    &warn_unspec                , /* RSVD_VCL */
    &warn_unspec                , /* RSVD_VCL */
    &decode_nvcl_hls                , /* IDR_W_RADL */
    &decode_nvcl_hls                , /* IDR_N_LP */
    &decode_nvcl_hls                , /* CRA */
    &decode_nvcl_hls                , /* GDR */
    &warn_unspec                , /* RSVD_IRAP_VCL */
    &warn_unsupported           , /* OPI */
    &warn_unsupported           , /* DCI */
    &decode_nvcl_hls            , /* VPS */
    &decode_nvcl_hls            , /* SPS */
    &decode_nvcl_hls            , /* PPS */
    &decode_nvcl_hls       , /* PREFIX_APS */
    &decode_nvcl_hls       , /* SUFFIX_APS */
    &decode_nvcl_hls            , /* PH */
    &log_ignored                , /* AUD */
    &log_ignored                , /* EOS */
    &log_ignored                , /* EOB */
    &log_ignored       , /* PREFIX_SEI */
    &log_ignored       , /* SUFFIX_SEI */
    &log_ignored                , /* FD */
    &warn_unspec                , /* RSVD_NVCL */
    &warn_unspec                , /* RSVD_NVCL */
    &warn_unspec                , /* UNSPEC */
    &warn_unspec                , /* UNSPEC */
    &warn_unspec                , /* UNSPEC */
    &warn_unspec                  /* UNSPEC */
};

int
nvcl_decode_nalu_hls_data(OVNVCLCtx *const nvcl_ctx, OVNALUnit *const nalu)
{
    uint8_t nalu_type = nalu->type & 0x1F;

    static int count = 0;
    ov_log(NULL, OVLOG_TRACE, "Received new %s NAL unit %d.\n", nalu_name[nalu_type], count++);

    int ret = nalu_action[nalu_type](nvcl_ctx, nalu);

    return ret;
}
