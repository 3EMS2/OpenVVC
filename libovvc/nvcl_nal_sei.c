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

#include "ovutils.h"
#include "overror.h"
#include "ovmem.h"
#include "ovconfig.h"

#include "nvcl.h"
#include "nvcl_utils.h"
#include "nvcl_structures.h"
#if HAVE_SLHDR
#include "pp_wrapper_slhdr.h"
#endif

enum SEIPayloadtype
{
    BUFFERING_PERIOD                     = 0,
    PICTURE_TIMING                       = 1,
    FILLER_PAYLOAD                       = 3,
    USER_DATA_REGISTERED_ITU_T_T35       = 4,
    USER_DATA_UNREGISTERED               = 5,
    FILM_GRAIN_CHARACTERISTICS           = 19,
    FRAME_PACKING                        = 45,
    DISPLAY_ORIENTATION                  = 47,
    PARAMETER_SETS_INCLUSION_INDICATION  = 129,
    DECODING_UNIT_INFO                   = 130,
    DECODED_PICTURE_HASH                 = 132,
    SCALABLE_NESTING                     = 133,
    MASTERING_DISPLAY_COLOUR_VOLUME      = 137,
    COLOUR_TRANSFORM_INFORMATION         = 142,
    CONTENT_LIGHT_LEVEL_INFO             = 144,
    DEPENDENT_RAP_INDICATION             = 145,
    ALTERNATIVE_TRANSFERT_CHARACTERICS   = 147,
    AMBIENT_VIEWING_ENVIRONMENT          = 148,
    CONTENT_COLOUR_VOLUME                = 149,
    EQUIRECTANGULAR_PROJECTION           = 150,
    GENERALIZED_CUBEMAP_PROJECTION       = 153,
    SPHERE_ROTATION                      = 154,
    REGION_WISE_PACKING                  = 155,
    OMNI_VIEWPORT                        = 156,
    ALPHA_CHANNEL_INFO                   = 165,
    FRAME_FIELD_INFO                     = 168,
    DEPTH_REPRESENTATION_INFO            = 177,
    MULTIVIEW_ACQUISITION_INFO           = 179,
    MULTIVIEW_POSITION_INFO              = 180,
    SEI_MANIFEST                         = 200,
    SEI_PREFIX_INDICATION                = 201,
    ANNOTATED_REGIONS                    = 202,
    SUBPICTURE_LEVEL_INFO                = 203,
    SAMPLE_ASPECT_RATIO_INFO             = 204,
    SCALABILITY_DIMENSION_INFO           = 205,
    EXTENDED_DRAP_INDICATION             = 206,
    CONSTRAINED_RASL_ENCODING_INDICATION = 207,
};

struct OVSEIPayload
{
    uint32_t type;
    uint32_t size;
};

/* FIXME find other spec */
struct OVSEIPayload
nvcl_sei_payload(OVNVCLReader *const rdr) {
    struct OVSEIPayload payload;

    uint32_t pl_type = 0;
    uint32_t pl_size = 0;

    uint8_t val = 0;

    do {
        val = nvcl_read_bits(rdr, 8);
        pl_type += val;
    } while (val == 0xFF);

    do {
        val = nvcl_read_bits(rdr, 8);
        pl_size += val;
    } while (val == 0xFF);

    payload.type = pl_type;
    payload.size = pl_size;

    return payload;
}

void
nvcl_film_grain_read(OVNVCLReader *const rdr, struct OVSEIFGrain *const fg)
{
    fg->fg_characteristics_cancel_flag = nvcl_read_flag(rdr);

    if (!fg->fg_characteristics_cancel_flag) {

        fg->fg_model_id = nvcl_read_bits(rdr, 2);

        fg->fg_separate_colour_description_present_flag = nvcl_read_flag(rdr);
        if (fg->fg_separate_colour_description_present_flag) {

            fg->fg_bit_depth_luma_minus8   = nvcl_read_bits(rdr, 3);
            fg->fg_bit_depth_chroma_minus8 = nvcl_read_bits(rdr, 3);

            fg->fg_full_range_flag         = nvcl_read_flag(rdr);

            fg->fg_colour_primaries         = nvcl_read_bits(rdr, 8);
            fg->fg_transfer_characteristics = nvcl_read_bits(rdr, 8);
            fg->fg_matrix_coeffs            = nvcl_read_bits(rdr, 8);
        }

        fg->fg_blending_mode_id = nvcl_read_bits(rdr, 2);
        fg->fg_log2_scale_factor = nvcl_read_bits(rdr, 4);

        for (int c = 0; c < 3; c++) {
            fg->fg_comp_model_present_flag[c] = nvcl_read_flag(rdr);
        }

        for (int c = 0; c < 3; c++) {
            if (fg->fg_comp_model_present_flag[c]) {
                fg->fg_num_intensity_intervals_minus1[c] = nvcl_read_bits(rdr, 8);
                fg->fg_num_model_values_minus1[c]        = nvcl_read_bits(rdr, 3);

                for (uint32_t i = 0; i< fg->fg_num_intensity_intervals_minus1[c] + 1 ; i++) {
                    fg->fg_intensity_interval_lower_bound[c][i] = nvcl_read_bits(rdr, 8);
                    fg->fg_intensity_interval_upper_bound[c][i] = nvcl_read_bits(rdr, 8);
                    for (uint32_t j = 0; j < fg->fg_num_model_values_minus1[c] + 1; j++) {
                        fg->fg_comp_model_value[c][i][j] = nvcl_read_s_expgolomb(rdr);
                    }
                }
            }
        }
        fg->fg_characteristics_persistence_flag = nvcl_read_flag(rdr);
    }
}

#if HAVE_SLHDR
void
nvcl_slhdr_read(OVNVCLReader *const rdr, struct OVSEISLHDR* sei_slhdr, uint32_t payloadSize)
{
    uint8_t* payload_array = sei_slhdr->payload_array;
    for (int i = 0; i < payloadSize; i++) {
        payload_array[i] = nvcl_read_bits(rdr, 8);
    }
}
#endif

#if 0
int
nvcl_sei_read(OVNVCLReader *const rdr, OVHLSData *const hls_data,
              const OVNVCLCtx *const nvcl_ctx, uint8_t nalu_type)
{
    int ret = 0;
    return ret;
}
#endif

int
nvcl_decode_nalu_sei2(OVSEI **sei_p, OVNVCLReader *const rdr, uint8_t nalu_type)
{
    OVSEI *sei = *sei_p;
    if (!sei) {
        sei = ov_mallocz(sizeof(struct OVSEI));
        if (!sei) goto fail;
    }

    struct OVSEIPayload payload = nvcl_sei_payload(rdr);

    switch (payload.type) {
        uint8_t sei_byte;
        case FILM_GRAIN_CHARACTERISTICS:

        ov_log(NULL, OVLOG_DEBUG, "SEI: FILM_GRAIN_CHARACTERISTICS"
                                  "(type = %d) with size %d.\n",
                                  payload.type, payload.size);

        if(!sei->sei_fg) {
            sei->sei_fg = ov_mallocz(sizeof(struct OVSEIFGrain));
        }

        nvcl_film_grain_read(rdr, sei->sei_fg);

        break;

        case USER_DATA_REGISTERED_ITU_T_T35:
        ov_log(NULL, OVLOG_DEBUG, "SEI: USER_DATA_REGISTERED_ITU_T_T35"
                                  " (type = %d) with size %d.\n",
                                  payload.type, payload.size);

#if HAVE_SLHDR

        if (!sei->sei_slhdr) {
            sei->sei_slhdr = ov_mallocz(sizeof(struct OVSEISLHDR));
            pp_init_slhdr_lib(&sei->sei_slhdr->slhdr_context);
        }

        nvcl_slhdr_read(rdr, sei->sei_slhdr, payload.size);
#endif
        break;

        default:

        for (int i = 0; i < payload.size; i++) {
            sei_byte = nvcl_read_bits(rdr, 8);
            sei_byte++;
        }

        ov_log(NULL, OVLOG_INFO, "SEI: Unknown prefix message (type = %d) was found!\n", payload.type);

        break;
    }

    *sei_p = sei;

    return 0;

fail:
    return OVVC_ENOMEM;
}

#if 1
int 
nvcl_decode_nalu_sei(OVNVCLCtx *const nvcl_ctx, OVNVCLReader *const rdr, uint8_t nalu_type)
{   
#if 0
    if(!nvcl_ctx->sei)
        nvcl_ctx->sei = ov_mallocz(sizeof(struct OVSEI));    

    struct OVSEI* sei = nvcl_ctx->sei;
#else
    struct OVSEI* sei = ov_mallocz(sizeof(struct OVSEI));
#endif

    struct OVSEIPayload payload = nvcl_sei_payload(rdr);
    switch (payload.type)
    {
        uint8_t sei_byte;
        case FILM_GRAIN_CHARACTERISTICS:

        ov_log(NULL, OVLOG_DEBUG, "SEI: FILM_GRAIN_CHARACTERISTICS"
                                  " (type = %d) with size %d.\n",
                                  payload.type, payload.size);

        if (!sei->sei_fg) {
            sei->sei_fg = ov_mallocz(sizeof(struct OVSEIFGrain));
        }

        nvcl_film_grain_read(rdr, sei->sei_fg);

        break;

        case USER_DATA_REGISTERED_ITU_T_T35:

        ov_log(NULL, OVLOG_DEBUG, "SEI: USER_DATA_REGISTERED_ITU_T_T35"
                                  " (type = %d) with size %d.\n",
                                  payload.type, payload.size);

#if HAVE_SLHDR
        if (!sei->sei_slhdr) {
            sei->sei_slhdr = ov_mallocz(sizeof(struct OVSEISLHDR));
            pp_init_slhdr_lib(&sei->sei_slhdr->slhdr_context);
        }

        nvcl_slhdr_read(rdr, sei->sei_slhdr, payload.size);
#endif
        break;
        default:

        for (int i = 0; i < payload.size; i++) {
            sei_byte = nvcl_read_bits(rdr, 8);
            sei_byte++;
        }

        ov_log(NULL, OVLOG_INFO, "SEI: Unknown prefix message"
                                 " (type = %d) was found!\n",
                                 payload.type);
        break;
    }

    return 0;
}
#endif

#if 0
const struct HLSReader sei_manager =
{
    .name = "SEI",
    .data_size    = sizeof(struct OVSPS),
    .find_storage = &storage_in_nvcl_ctx,
    .read         = &nvcl_sei_read,
    .validate     = &validate_sps,
    .free         = &free_sps
};
#endif
