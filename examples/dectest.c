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
#include "ovtime.h"

struct PicInfoFormat
{
    const char *fmt_str;
    FILE *file;
};

typedef struct OVVCHdl
{
    OVVCDmx *dmx;
    OVDec *dec;
    OVIO *io;
    struct PicInfoFormat *pinfo_fmt;
} OVVCHdl;

static int dmx_attach_file(OVVCHdl *const vvc_hdl, const char *const input_file_name);

static int init_openvvc_hdl(OVVCHdl *const ovvc_hdl, const char *output_file_name, int nb_frame_th, int nb_entry_th, int upscale_flag);

static int close_openvvc_hdl(OVVCHdl *const ovvc_hdl);

static int read_write_stream(OVVCHdl *const hdl, FILE *fout);

static int write_decoded_frame_to_file(OVFrame *const frame, FILE *out_file);

static void print_version(void);

static void print_usage(void);

int
main(int argc, char** argv)
{
    /* basic options parser and assign
       filenames into a functions*/
    int c;
    const char *input_file_name = NULL, *output_file_name = NULL;
    int ov_log_level = OVLOG_INFO;
    FILE *fout = NULL;
    int nb_frame_th = 0;
    int nb_entry_th = 0;
    int upscale_flag = 0;
    int pp_disable = 0;
    static const char *const default_info_fmt = "POC : %P %T µs  %R bytes %V\n";
    struct PicInfoFormat pinfo_fmt =
    {
        .fmt_str = NULL,
        .file = stdout,
    };

    uint8_t options_flag=0;

    OVVCHdl ovvc_hdl;
    ovvc_hdl.pinfo_fmt = &pinfo_fmt;
    int ret = 0;

    while (1) {

        const static struct option long_options[] =
        {
            {"version",   no_argument,       0, 'v'},
            {"help",      no_argument,       0, 'h'},
            {"log-level", required_argument, 0, 'l'},
            {"outfile",   required_argument, 0, 'o'},
            {"framethr",  required_argument, 0, 't'},
            {"entrythr",  required_argument, 0, 'e'},
            {"upscale",   required_argument, 0, 'u'},
            {"info",      optional_argument, 0, 'i'},
            {"nopostproc", no_argument,       0, 'p'},
        };

        int option_index = 0;

        c = getopt_long(argc, argv, "-vhl:o:t:e:u:", long_options,
                        &option_index);

        if (c == -1) {
            break;
        }

        switch (c)
        {
            case 'v':
                options_flag += 0x01;
                break;

            case 'h':
                options_flag += 0x10;
                break;

            case 'l':
                ov_log_level = optarg[0]-'0';
                break;

            case 'o':
                output_file_name = optarg;
                break;

            case 'u':
                upscale_flag = atoi(optarg);
                break;

            case 't':
                nb_frame_th = atoi(optarg);
                break;

            case 'e':
                nb_entry_th = atoi(optarg);
                break;

            case 'i':
                if (optarg)
                    pinfo_fmt.fmt_str = strdup(optarg);
                else
                    pinfo_fmt.fmt_str = default_info_fmt;
                break;
            case 'p':
                pp_disable = 1;
                break;

            case '?':
                options_flag += 0x10;
                break;

            case 1:
                input_file_name = optarg;
                break;

            default:
                abort();
        }
    }

    if (OVLOG_ERROR <= ov_log_level && ov_log_level <= OVLOG_DEBUG){
        ovlog_set_log_level(ov_log_level);
    }

    if (output_file_name == NULL) {
        output_file_name ="test.yuv";
    }

    if (options_flag) {

        if (options_flag & 0x01) {
            print_version();
        }

        if (options_flag & 0x10) {
            print_usage();
        }

        return 0;
    }

    fout = fopen(output_file_name, "wb");

    if (fout == NULL) {
        ov_log(NULL, OVLOG_ERROR, "Failed to open output file '%s'.\n", output_file_name);
        goto failinit;
    } else {
        ov_log(NULL, OVLOG_INFO, "Decoded stream will be written to '%s'.\n", output_file_name);
    }

    ret = init_openvvc_hdl(&ovvc_hdl, output_file_name, nb_frame_th, nb_entry_th, upscale_flag);
    if (pp_disable) {
        int val = 1;
        ovdec_set_opt(ovvc_hdl.dec, "nopostproc", &val);
    }

    if (ret < 0) goto failinit;

    ret = dmx_attach_file(&ovvc_hdl, input_file_name);

    if (ret < 0) goto failattach;

    read_write_stream(&ovvc_hdl, fout);

    ovdmx_detach_stream(ovvc_hdl.dmx);

failattach:
    ret = close_openvvc_hdl(&ovvc_hdl);
    fclose(fout);

failinit:
    if (pinfo_fmt.fmt_str && pinfo_fmt.fmt_str != default_info_fmt) {
        free((void *)pinfo_fmt.fmt_str);
    }
    return ret;
}

static int
dmx_attach_file(OVVCHdl *const vvc_hdl, const char *const input_file_name)
{
    OVFileIO *file_io = ovio_new_fileio(input_file_name, "rb");
    int ret;

    if (file_io == NULL) {
        perror(input_file_name);
        vvc_hdl->io = NULL;
        return -1;
    }

    vvc_hdl->io = (OVIO*) file_io;

    ret = ovdmx_attach_stream(vvc_hdl->dmx, (OVIO*) vvc_hdl->io);

    return ret;
}

static int
init_openvvc_hdl(OVVCHdl *const ovvc_hdl, const char *output_file_name, int nb_frame_th, int nb_entry_th, int upscale_flag)
{
    OVDec **vvcdec = &ovvc_hdl->dec;
    OVVCDmx **vvcdmx = &ovvc_hdl->dmx;
    int ret;

    ret = ovdec_init(vvcdec);

    if (ret < 0) goto faildec;

    ret = ovdec_config_threads(*vvcdec, nb_entry_th, nb_frame_th);

    ovdec_set_option(*vvcdec, OVDEC_RPR_UPSCALE, upscale_flag);

    ret = ovdec_start(*vvcdec);

    if (ret < 0) goto failstart;

    ov_log(vvcdec, OVLOG_TRACE, "Decoder init.\n");

    ret = ovdmx_init(vvcdmx);

    if (ret < 0) goto faildmx;

    ov_log(vvcdmx, OVLOG_TRACE, "Demuxer init.\n");

    return 0;

failstart:
faildec:
    ov_log(NULL, OVLOG_ERROR, "Decoder failed at init.\n");
    return ret;

faildmx:
    ov_log(NULL, OVLOG_ERROR, "Demuxer failed at init.\n");
    ovdec_close(*vvcdec);
    return ret;
}

static int
close_openvvc_hdl(OVVCHdl *const ovvc_hdl)
{
    OVDec *vvcdec = ovvc_hdl->dec;
    OVVCDmx *vvcdmx = ovvc_hdl->dmx;
    OVIO* io = ovvc_hdl->io;
    int ret;

    if (io != NULL) {
        io->close(io);
    }

    ret = ovdec_close(vvcdec);

    if (ret < 0) goto faildecclose;

    ret = ovdmx_close(vvcdmx);

    if (ret < 0) goto faildmxclose;

    return 0;

faildecclose:
    /* Do not check for dmx failure since it might override
       return value to a correct one in either success or
       failure we already raised an error */
    ov_log(NULL, OVLOG_ERROR, "Decoder failed at cloture.\n");
    ovdmx_close(vvcdmx);

faildmxclose:
    return ret;
}

static inline uint8_t
is_vcl(const struct OVNALUnit *const nalu) {
    return (nalu->type < OVNALU_OPI);
}

static uint64_t
derive_pu_size(OVPictureUnit *pu)
{
    uint64_t sum = 0;
    int i;
    for (i = 0; i < pu->nb_nalus; ++i) {
        if (is_vcl(pu->nalus[i])) {
            sum += pu->nalus[i]->rbsp_size;
        }
    }
    return sum;
}

static inline uint64_t 
compute_decoding_time(OVFrame *frame)
{
    uint64_t start = frame->time_info.start;
    uint64_t end = frame->time_info.end;
    uint64_t out = frame->time_info.out;

    return NS_TO_US((int64_t)end - start);
}

static void
output_pictures_info(const struct PicInfoFormat *pinfo_fmt, OVFrame *frame)
{
    uint64_t pu_size = 0;

    if (frame->pu)  pu_size = derive_pu_size(frame->pu);

    const char *fmt = pinfo_fmt->fmt_str;
    while (*fmt) {
        switch (*fmt) {
            case '%':
                if (*++fmt)
                    switch (*fmt++) {
                        case 'R':
                            fprintf(pinfo_fmt->file, "%ld", pu_size);
                            break;
                        case 'T':
                            fprintf(pinfo_fmt->file, "%ld", compute_decoding_time(frame));
                            break;
                        case 'P' :
                            fprintf(pinfo_fmt->file, "%d", frame->poc);
                            break;
                        case 'V' :
                            fprintf(pinfo_fmt->file, "%ldx%ld", frame->width, frame->height);
                            break;
                    }
            case '\\':
                    if (*++fmt == 'n')
                        fprintf(pinfo_fmt->file,"\n");
                    else
                        fprintf(pinfo_fmt->file,"%c", *--fmt);
                    fmt++;
                break;
            default:
                fprintf(pinfo_fmt->file,"%c", *fmt);
                fmt++;
                break;
        }
    }
}

static int
read_write_stream(OVVCHdl *const hdl, FILE *fout)
{
    OVDec *const dec = hdl->dec;
    int nb_pic = 0;
    int ret;

    do {
        OVPictureUnit *pu = NULL;
        ret = ovdmx_extract_picture_unit(hdl->dmx, &pu);
        if (ret < 0) {
            break;
        }

        if (pu) {
            int nb_pic2;
            int dec_ret;
            dec_ret = ovdec_submit_picture_unit(dec, pu);

            ovpu_unref(&pu);

            do {
                OVFrame *frame = NULL;
                nb_pic2 = ovdec_receive_picture(dec, &frame);

                if (frame) {
                    if (fout) {
                        write_decoded_frame_to_file(frame, fout);
                    }

                    ov_log(NULL, OVLOG_DEBUG, "Got output picture with POC %d.\n", frame->poc);

                    if (hdl->pinfo_fmt->fmt_str)
                        output_pictures_info(hdl->pinfo_fmt, frame);

                    ovframe_unref(&frame);

                    ++nb_pic;
                }
            } while (nb_pic2 > 0);

        } else {
            break;
        }

    } while (ret >= 0);

    ret = 1;

    while (ret > 0) {
        OVFrame *frame = NULL;
        ret = ovdec_drain_picture(dec, &frame);

        if (frame) {
            if (fout) {
                write_decoded_frame_to_file(frame, fout);
            }

            ov_log(NULL, OVLOG_DEBUG, "Drain picture with POC %d.\n", frame->poc);

            if (hdl->pinfo_fmt->fmt_str)
                output_pictures_info(hdl->pinfo_fmt, frame);

            ovframe_unref(&frame);

            ++nb_pic;
        }
    }

    ov_log(NULL, OVLOG_INFO, "Decoded %d pictures\n", nb_pic);

    return 1;
}

static int
write_decoded_frame_to_file(OVFrame *const frame, FILE *out_file)
{
    uint8_t component = 0;
    int ret = 0;
    struct Window output_window = frame->output_window;
    int bd_shift = (frame->frame_info.chroma_format == OV_YUV_420_P8) ? 0: 1;

    uint16_t add_w = (output_window.offset_lft + output_window.offset_rgt);
    uint16_t add_h = (output_window.offset_abv + output_window.offset_blw);

    if (add_w || add_h || frame->width < (frame->linesize[0] >> bd_shift)) {
        for (component = 0; component < 3; component++) {
            uint16_t comp_w = component ? add_w : add_w << 1;
            uint16_t comp_h = component ? add_h : add_h << 1;
            uint16_t win_left  =  component ? output_window.offset_lft : output_window.offset_lft << 1;
            uint16_t win_top   =  component ? output_window.offset_abv : output_window.offset_abv << 1;
            int frame_h = (frame->height >> (!!component)) - comp_h;
            int frame_w = (frame->width  >> (!!component)) - comp_w;

            int offset_h = win_top * frame->linesize[component] ;
            int offset   = offset_h + (win_left << bd_shift) ;
            const uint8_t *data = (uint8_t*)frame->data[component] + offset;

            for (int j = 0; j < frame_h; j++) {
                ret += fwrite(data, frame_w << bd_shift, sizeof(uint8_t), out_file);
                data += frame->linesize[component];
            }
        }
    } else {
        const uint8_t *data = (uint8_t*)frame->data[0];
        const uint8_t *data_cb = (uint8_t*)frame->data[1];
        const uint8_t *data_cr = (uint8_t*)frame->data[2];
        ret += fwrite(data, frame->size[0], sizeof(uint8_t), out_file);
        ret += fwrite(data_cb, frame->size[1], sizeof(uint8_t), out_file);
        ret += fwrite(data_cr, frame->size[2], sizeof(uint8_t), out_file);
    }

    return ret;
}

static void
print_version(){
    printf("libovvc version %s\n", ovdec_version());
}

static void print_usage(){
  printf("usage: dectest [options]\n");
  printf("options:\n");
  printf("\t-h, --help\t\t\t\tShow this message.\n");
  printf("\t-v, --version\t\t\t\tShow version information.\n");
  printf("\t-l <level>, --log-level=<level>\t\tDefine the level of verbosity. Value between 0 and 6. (Default: 2)\n");
  printf("\t-o <file>, --outfile=<file>\t\tPath to the output file (Default: test.yuv).\n");
  printf("\t-t <nbthreads>, --framethr=<nbthreads>\t\tNumber of simultaneous frames decoded (Default: 0).\n");
  printf("\t-e <nbthreads>, --entrythr=<nbthreads>\t\tNumber of simultaneous entries decoded per frame (Default: 0).\n");
}
