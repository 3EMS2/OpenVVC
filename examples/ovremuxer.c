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
#include "ovunits.h"

typedef struct OVVCHdl
{
    OVVCDmx *dmx;
    OVDec *dec;
    OVIO *io;
    FILE *out;
} OVVCHdl;

static int dmx_attach_file(OVVCHdl *const vvc_hdl, const char *const ifile);

static int init_openvvc_hdl(OVVCHdl *const ovvc_hdl);

static int rw_stream(OVVCHdl *const hdl);

static int close_openvvc_hdl(OVVCHdl *const ovvc_hdl);

static void print_version(void);

static void print_usage(void);

static int br_scale = 1;
#include "string.h"
static int
analyse_list(const char *const str)
{ 
    char *tmpstr = strdup(str);
    const char *bck;
    int nb_tkn = 0;
    tmpstr = strtok_r(tmpstr, ",", &bck);
    while (tmpstr) {
        ++nb_tkn;
        printf("%s\n", tmpstr);
        tmpstr = strtok_r(NULL, ",", &bck);
    }
    printf("%d\n", nb_tkn);
    printf("%s\n", str);
    free(tmpstr);
}

int
main(int argc, char** argv)
{
    OVVCHdl ovvc_hdl;
    const char *ifile = NULL;
    int log_lvl = OVLOG_INFO;
    int ret = 0;
    const char *ofile = "test.266";

    while (1) {
        int c;

        const static struct option long_options[] =
        {
            {"version",   no_argument,       0, 'v'},
            {"help",      no_argument,       0, 'h'},
            {"log-level", required_argument, 0, 'l'},
            {"output", required_argument, 0, 'o'},
            {"peak-luminance_scale", required_argument, 0, 's'},
            {"advanced", required_argument, 0, 'a'},
        };

        int option_index = 0;

        c = getopt_long(argc, argv, "-vhl:o:s:a:", long_options,
                        &option_index);

        if (c == -1) {
            break;
        }

        switch (c)
        {
            case 'a':
                analyse_list(optarg);
                return 0;

            case 'v':
                print_version();
                return 0;

            case 'h':
                print_usage();
                return 0;

            case 'l':
                log_lvl = optarg[0]-'0';
                break;

            case 'o':
                ofile = optarg;
                break;

            case 's':
                br_scale = atoi(optarg);
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

    ovvc_hdl.out = fopen(ofile, "wb");
    if (!ovvc_hdl.out) goto failopen;

    rw_stream(&ovvc_hdl);

failopen:
    ovdmx_detach_stream(ovvc_hdl.dmx);

failattach:
    ret = close_openvvc_hdl(&ovvc_hdl);

failinit:
    return ret;
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

static uint8_t start_code[3] =
{ 0, 0, 1 };

static uint8_t sei_header[2] =
{ 0, 0 };

static uint8_t sei_uuid[16] =
{ 0, 0, 8, 0, 0, 8, 0, 8, 0, 0, 8, 0, 0, 8, 0, 8};

static int count = 0;

static int
set_br_scale(int poc)
{
    static uint32_t poc_range[16] =
    { 0, 130, 260, 390, 520, 650, 780, 910, 1040, 1170, 1300, 0, 0, 0, 0, 0};
    static uint32_t scale[16] =
    { 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    int i = 0;
    for (i = 1; i < 16; ++i) {
        if (poc < poc_range[i] && poc > poc_range[i - 1] )
            break;
    }
    printf("POC: %d, scale:%d, \n", poc, i);
    
    int val = scale[i];

    return val;
}

static int
eu(int val, uint32_t * tostr)
{
    uint8_t sfx_sz = 31 - ov_clz(val + !!val);
    uint8_t pfx_sz = sfx_sz + 1;
    uint32_t ue_val = ((1 << sfx_sz) | (val - ((1 << sfx_sz) - 1) )) << (32 - (pfx_sz + sfx_sz));

    *tostr = ue_val;
    return pfx_sz + sfx_sz;
}
static int poc = 0;
static int
write_pu(const OVPictureUnit *const pu, FILE *out, int poc)
{
    int i;
    char tmp_sei[256] = {0};
    uint8_t tmp_val[4];
    for (i = 0; i < pu->nb_nalus; ++i) {
        const OVNALUnit *const nalu = pu->nalus[i];

        if (nalu->type == OVNALU_PREFIX_SEI || nalu->type == OVNALU_SUFFIX_SEI) {

            uint16_t nb_bits = eu(br_scale, (uint32_t *)&tmp_val);
            br_scale = set_br_scale(poc);
#if 0
            uint16_t nb_bytes = (nb_bits + 0x7) >> 3;
#else
            uint16_t nb_bytes = 2;
#endif
            /* Start code */
            fwrite(start_code, 1, 3, out);
            /* NAL Unit header */
            fwrite(nalu->rbsp_data, 1, 2, out);
            /* SEI unregistered */
            fputc(0x5, out);
            /* SEI payload size = 17 */
            fputc(16 + nb_bytes, out);

            /* SEI 16 byte for uuid */
            fwrite(sei_uuid, 1, 16, out);

            /* SEI 16 byte for uuid */
            //fputc(br_scale, out);
#if 0
            int t = 3;
            do {
                uint8_t tmp2 [4];
                tmp2[0] = tmp_val[t--];// >> 7;
#if 0
                tmp2[0] |= tmp_val[nb_bytes] >> 6;
                tmp2[0] |= tmp_val[nb_bytes] >> 5;
                tmp2[0] |= tmp_val[nb_bytes] >> 4;
                tmp2[0] |= tmp_val[nb_bytes] >> 3;
                tmp2[0] |= tmp_val[nb_bytes] >> 2;
                tmp2[0] |= tmp_val[nb_bytes] >> 1;
                tmp2[0] |= tmp_val[nb_bytes];
#endif
                fwrite(tmp2, 1, 1, out);
            } while (--nb_bytes);
#else
	    fputc(((br_scale >> 8) & 0xFF), out);
	    fputc((br_scale & 0xFF), out);

#endif
            /* RBSP stop bit*/
            fputc(0x80, out);
        }

        fwrite(start_code, 1, 3, out);

        if (!nalu->nb_epb) {
            fwrite(nalu->rbsp_data, 1, nalu->rbsp_size, out);
        } else {
            int nb_epb = nalu->nb_epb;
            int cursor = 0;

            do {
                int epb_pos = nalu->epb_pos[nalu->nb_epb - nb_epb];
                const uint8_t *sgmt_start = nalu->rbsp_data + 0 + cursor;
                const uint8_t *sgmt_end = nb_epb != 0 ? nalu->rbsp_data + epb_pos + 1 : nalu->rbsp_data + nalu->rbsp_size + 2;

                fwrite (sgmt_start, 1, sgmt_end - sgmt_start, out);

                fputc(3, out);
                cursor = epb_pos + 1;

            } while (nb_epb--);
        }
    }
    return 0;
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

#if 0
        if (idr_flag){
            /* New IDR involves a POC refresh and mark the start of
             * a new coded video sequence
             */
            dpb->cvs_id = (dpb->cvs_id + 1) & 0xFF;
            if (ps->ph->ph_poc_msb_cycle_present_flag) {
                uint8_t log2_max_poc_lsb = ps->sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
                poc = ps->ph->ph_poc_msb_cycle_val << log2_max_poc_lsb;
            } else {
                poc = 0;
            }
            poc += ps->ph->ph_pic_order_cnt_lsb;
        } else {
            int32_t last_poc = dpb->poc;
            poc = derive_poc(ps->ph->ph_pic_order_cnt_lsb,
                             ps->sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4,
                             last_poc);
        }
#endif

static int
rw_stream(OVVCHdl *const hdl)
{
    int ret;
    int poc = 0;
    int i;
    OVNVCLCtx nvcl_ctx = {0};
    struct OVPS ps = {0};

    fputc(0, hdl->out);

    do {
        OVPictureUnit *pu = NULL;
        uint8_t idr_flag = 0;
        ret = ovdmx_extract_picture_unit(hdl->dmx, &pu);
        if (ret < 0 || !pu) {
            break;
        }

        for (i = 0; i < pu->nb_nalus; ++i) {
            OVNALUnit *const nalu = pu->nalus[i];

            nvcl_decode_nalu_hls_data(&nvcl_ctx, nalu);

            idr_flag |= nalu->type == OVNALU_IDR_W_RADL;
            idr_flag |= nalu->type == OVNALU_IDR_N_LP;
        }

        ret = decinit_update_params(&ps, &nvcl_ctx);

        if (ps.ph) {
        if (idr_flag ){
            if (ps.ph->ph_poc_msb_cycle_present_flag) {
                uint8_t log2_max_poc_lsb = ps.sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
                poc = ps.ph->ph_poc_msb_cycle_val << log2_max_poc_lsb;
            } else {
                poc = 0;
            }
            poc += ps.ph->ph_pic_order_cnt_lsb;
        } else {
            int32_t last_poc = poc;
            poc = derive_poc(ps.ph->ph_pic_order_cnt_lsb,
                             ps.sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4,
                             last_poc);
        }
        }
        printf("%d\n", poc);

        if (pu) {
            write_pu(pu, hdl->out, poc);
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
    printf("usage: ovrate [options] <VVCSTREAM>\n");
    printf("options:\n");
    printf("\t-h, --help\t\t\t\tShow this message.\n");
    printf("\t-v, --version\t\t\t\tShow OpenVVC version information.\n");
    printf("\t-l <level>, --log-level=<level>\t \tDefine the level of verbosity. Value between 0 and 6. (Default: 2)\n");
}
