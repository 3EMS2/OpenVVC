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
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "ovdec.h"
#include "ovio.h"
#include "ovdefs.h"
#include "ovdmx.h"
#include "ovframe.h"
#include "ovdpb.h"
#include "ovutils.h"
#include "ovversion.h"
#include "ovmem.h"

#include "nvcl.h"
#include "nvcl_utils.h"
#include "decinit.h"
#include "nvcl_structures.h"
#include "hls_structures.h"

typedef struct OVVCHdl
{
    OVVCDmx *dmx;
    OVVCDec *dec;
    OVIO *io;
} OVVCHdl;

static int dmx_attach_file(OVVCHdl *const vvc_hdl, const char *const ifile);

static int init_openvvc_hdl(OVVCHdl *const ovvc_hdl);

static int probe_stream(OVVCHdl *const hdl);

static int close_openvvc_hdl(OVVCHdl *const ovvc_hdl);

static void print_version(void);

static void print_usage(void);


int
main(int argc, char** argv)
{
    OVVCHdl ovvc_hdl;
    const char *ifile = NULL;
    int log_lvl = OVLOG_INFO;
    int ret = 0;

    while (1) {
        int c;

        const static struct option long_options[] =
        {
            {"version",   no_argument,       0, 'v'},
            {"help",      no_argument,       0, 'h'},
            {"log-level", required_argument, 0, 'l'},
        };

        int option_index = 0;

        c = getopt_long(argc, argv, "-vhl:", long_options,
                        &option_index);

        if (c == -1) {
            break;
        }

        switch (c)
        {
            case 'v':
                print_version();
                return 0;

            case 'h':
                print_usage();
                return 0;

            case 'l':
                log_lvl = optarg[0]-'0';
                break;

            case 1 :
                ifile = optarg;
                break;

            default:
                print_usage();
                return -1;
        }
    }

    if (OVLOG_ERROR <= log_lvl && log_lvl <= OVLOG_DEBUG) {
        ovlog_set_log_level(log_lvl);
    }

    ret = init_openvvc_hdl(&ovvc_hdl);

    if (ret < 0) goto failinit;

    ret = dmx_attach_file(&ovvc_hdl, ifile);

    if (ret < 0) goto failattach;

    probe_stream(&ovvc_hdl);

    ovdmx_detach_stream(ovvc_hdl.dmx);

failattach:
    ret = close_openvvc_hdl(&ovvc_hdl);

failinit:
    return ret;
}

static int
derive_poc(int poc_lsb, int log2_max_poc_lsb, int prev_poc)
{
    int max_poc_lsb  = 1 << log2_max_poc_lsb;
    int prev_poc_lsb = prev_poc & (max_poc_lsb - 1);
    int poc_msb = prev_poc - prev_poc_lsb;
    if((poc_lsb < prev_poc_lsb) &&
      ((prev_poc_lsb - poc_lsb) >= (max_poc_lsb >> 1))){
        poc_msb += max_poc_lsb;
    } else if((poc_lsb > prev_poc_lsb) &&
             ((poc_lsb - prev_poc_lsb) > (max_poc_lsb >> 1))){
        poc_msb -= max_poc_lsb;
    }
    return poc_msb + poc_lsb;
}

static int
dmx_attach_file(OVVCHdl *const vvc_hdl, const char *const ifile)
{
    OVFileIO *file_io = ovio_new_fileio(ifile, "rb");
    int ret;

    if (file_io == NULL) {
        perror(ifile);
        vvc_hdl->io = NULL;
        return -1;
    }

    vvc_hdl->io = (OVIO*) file_io;

    ret = ovdmx_attach_stream(vvc_hdl->dmx, (OVIO*) vvc_hdl->io);

    return ret;
}

static int
init_openvvc_hdl(OVVCHdl *const ovvc_hdl)
{
    OVVCDmx **vvcdmx = &ovvc_hdl->dmx;
    int ret;

    ret = ovdmx_init(vvcdmx);

    if (ret < 0) goto faildmx;

    ov_log(vvcdmx, OVLOG_TRACE, "Demuxer init.\n");

    return 0;

faildmx:
    ov_log(NULL, OVLOG_ERROR, "Demuxer failed at init.\n");

    return ret;
}

static int
close_openvvc_hdl(OVVCHdl *const ovvc_hdl)
{
    OVVCDec *vvcdec = ovvc_hdl->dec;
    OVVCDmx *vvcdmx = ovvc_hdl->dmx;
    OVIO* io = ovvc_hdl->io;
    int ret;

    if (io != NULL) {
        io->close(io);
    }

    ret = ovdmx_close(vvcdmx);

    if (ret < 0) goto faildmxclose;

    return 0;

faildmxclose:
    return ret;
}

struct PUSummary {
    uint64_t dts;
    uint64_t data_size;
    uint64_t hls_size;
    uint64_t vcl_size;
    uint8_t type;
};

static const char *nalutype2str[32] =
{
   "TRAIL",
   "STSA",
   "RADL",
   "RASL",
   "RSVD_VCL_4",
   "RSVD_VCL_5",
   "RSVD_VCL_6",
   "IDR_W_RADL",
   "IDR_N_LP",
   "CRA",
   "GDR",
   "RSVD_IRAP_VCL_11",
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
   "RSVD_NVCL_26",
   "RSVD_NVCL_27",
   "UNSPEC_28",
   "UNSPEC_29",
   "UNSPEC_30",
   "UNSPEC_31"

};

static int hls(struct OVPS *const ps, OVNVCLCtx *const nvcl_ctx, OVPictureUnit *pu, int32_t poc);

static inline uint8_t
is_vcl(const struct OVNALUnit *const nalu) {
    return (nalu->type < OVNALU_OPI);
}

static int
decode_nal_unit(struct OVPS *ps, OVNVCLCtx *const nvcl_ctx, OVNALUnit * nalu)
{
    int ret = nvcl_decode_nalu_hls_data(nvcl_ctx, nalu);
    if (ret < 0) {
        goto hlsfail;
    }

    printf("%d\n", ret);

    if (is_vcl(nalu)) {
        int nb_sh_bytes = ret;
        ret = decinit_update_params(ps, nvcl_ctx);
        decinit_set_entry_points(ps, nalu, nb_sh_bytes);
    }

    return 0;

hlsfail:
    return ret;
}

static int
hls(struct OVPS *const ps, OVNVCLCtx *const nvcl_ctx, OVPictureUnit *pu, int32_t poc)
{
    int i;
    int ret;

    for (i = 0; i < pu->nb_nalus; ++i) {

        ret = decode_nal_unit(ps, nvcl_ctx, pu->nalus[i]);

        if (is_vcl(pu->nalus[i])) {
            uint8_t pu_type = pu->nalus[i]->type;
            if (!ps->sh->sh_slice_address) {
                if (pu_type == OVNALU_IDR_W_RADL || pu_type == OVNALU_IDR_N_LP) {
                    /* New IDR involves a POC refresh and mark the start of
                     * a new coded video sequence
                     */
                    if (ps->ph->ph_poc_msb_cycle_present_flag) {
                        uint8_t log2_max_poc_lsb = ps->sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
                        poc = ps->ph->ph_poc_msb_cycle_val << log2_max_poc_lsb;
                    } else {
                        poc = 0;
                    }
                    poc += ps->ph->ph_pic_order_cnt_lsb;
                } else {
                    int32_t last_poc = poc;
                    poc = derive_poc(ps->ph->ph_pic_order_cnt_lsb,
                                     ps->sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4,
                                     last_poc);
                }
            }
        }
    }

    pu->dts = poc;

    return 0;

fail:
    /* Error processing if needed */
    return ret;
}

char
slice_type(const OVNALUnit *const nalu)
{
    if (!is_vcl(nalu)) goto end;
    struct HLSDataRef *d = (struct HLSDataRef*)(nalu->hls_data);

    OVSH *sh = (OVSH *)d->data;

    if (sh->sh_slice_type == 2) return 'I';
    if (sh->sh_slice_type == 0) return 'B';
    if (sh->sh_slice_type == 1) return 'P';

end:
    return '\0';
}

static int
probe_stream(OVVCHdl *const hdl)
{
    OVVCDec *const dec = hdl->dec;
    int nb_pic = 0;
    int ret;
    OVNVCLCtx nvcl_ctx = {0};
    struct OVPS ps = {0};
    int poc = 0;

    do {
        OVPictureUnit *pu = NULL;
        ret = ovdmx_extract_picture_unit(hdl->dmx, &pu);
        if (ret < 0) {
            break;
        }

        if (pu) {
	    struct PUSummary pu_summary = {{0}};

	    hls(&ps, &nvcl_ctx, pu, poc);

            poc = pu->dts;

	    fprintf(stdout, "Picture Unit %ld: \n", pu->dts);
            for (int i = 0; i < pu->nb_nalus; ++i) {
		const OVNALUnit *const nalu = pu->nalus[i];
		fprintf(stdout, "NAL Unit %.12s (%ld) bytes, %c\n", nalutype2str[nalu->type & 0x1F], nalu->rbsp_size, slice_type(nalu));
	    }

            ovpu_unref(&pu);

        } else {
            break;
        }

    } while (ret >= 0);

    return 1;
}

static void
print_version(){
    printf("libovvc version %s\n", ovdec_version());
}

static void print_usage(){
  printf("usage: ovrate [options]\n");
  printf("options:\n");
  printf("\t-h, --help\t\t\t\tShow this message.\n");
  printf("\t-v, --version\t\t\t\tShow version information.\n");
  printf("\t-l <level>, --log-level=<level>\t\tDefine the level of verbosity. Value between 0 and 6. (Default: 2)\n");
}
