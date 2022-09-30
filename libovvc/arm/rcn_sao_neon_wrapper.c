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
*   Ibrahim FARHAT
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
#include <stdint.h>

#include "rcn_neon.h"


#include "dec_structures.h"
#include "rcn_structures.h"


void ov_band_filter_neon(uint16_t *dst,const int16_t *src, int16_t sao_offset_val0, int16_t sao_offset_val1, int16_t sao_offset_val2, int16_t sao_offset_val3, uint8_t sao_left_class);
void ov_sao_edge_filter_h_neon(uint16_t *dst,const int16_t *src, int a_stride, int b_stride,int16_t* sao_offset_val);
void ov_sao_edge_filter_v_neon(uint16_t *dst,const int16_t *src, int a_stride, int b_stride,int16_t* sao_offset_val);
void ov_sao_edge_filter_d_neon(uint16_t *dst,const int16_t *src, int a_stride, int b_stride,int16_t* sao_offset_val);
void ov_sao_edge_filter_b_neon(uint16_t *dst,const int16_t *src, int a_stride, int b_stride,int16_t* sao_offset_val);

static void
sao_band_filter_0_10_neon(OVSample* _dst,
                         OVSample* _src,
                         ptrdiff_t _stride_dst,
                         ptrdiff_t _stride_src,
                         int width,
                         int height,
                         int8_t offset_val[],
                         uint8_t band_pos)
{
  int y, x;
  int shift = 10 - 5;

  uint16_t* dst = (uint16_t*)_dst;
  uint16_t* src = (uint16_t*)_src;
  ptrdiff_t stride_dst = _stride_dst;
  ptrdiff_t stride_src = _stride_src;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 8) {

    }
    dst += stride_dst;
    src += stride_src;
  }
}


static void
sao_edge_filter_v_neon(OVSample *dst, OVSample *src_row, OVSample *src_col,
                      ptrdiff_t stride_dst, ptrdiff_t stride_src,
                      int width, int height,
                      int8_t offset_val[],
                      uint8_t eo_dir)
{
  int x, y;

  for (x = 0; x < width; x += 8) {
    OVSample *src = dst;

    for (y = 0; y < height; y++) {



      src += stride_dst;
    }
    dst += 8;
  }
}

static void
sao_edge_filter_d_neon(OVSample *dst, OVSample *src_row, OVSample *src_col,
                      ptrdiff_t stride_dst, ptrdiff_t stride_src,
                      int width, int height,
                      int8_t offset_val[],
                      uint8_t eo_dir)
{
  int x, y;


  for (x = 0; x < width; x += 8) {
    OVSample *src = dst;

    for (y = 0; y < height; y++) {

      src += stride_dst;
    }
  }
}

static void
sao_edge_filter_b_neon(OVSample *dst, OVSample *src_row, OVSample *src_col,
                      ptrdiff_t stride_dst, ptrdiff_t stride_src,
                      int width, int height,
                      int8_t offset_val[],
                      uint8_t eo_dir)
{
  int x, y;

  for (x = 0; x < width; x += 8) {
    OVSample *src = dst;

    for (y = 0; y < height; y++) {



      src += stride_dst;
    }
  }
}


static void
sao_edge_filter_h_neon(OVSample *dst, OVSample *src_row, OVSample *src_col,
                      ptrdiff_t stride_dst, ptrdiff_t stride_src,
                      int width, int height,
                      int8_t offset_val[],
                      uint8_t eo_dir)
{
  int x, y;
  OVSample *src = dst;


  for (y = 0; y < height; y++) {

    for (x = 0; x < width; x += 8) {

    }
    src += stride_dst;
    dst += stride_dst;
  }
}
void rcn_init_sao_functions_neon(struct RCNFunctions *const rcn_funcs){
  rcn_funcs->sao.band= &sao_band_filter_0_10_neon;
  rcn_funcs->sao.edge2[0]= &sao_edge_filter_h_neon;
  rcn_funcs->sao.edge2[1]= &sao_edge_filter_v_neon;
  rcn_funcs->sao.edge2[2]= &sao_edge_filter_d_neon;
  rcn_funcs->sao.edge2[3]= &sao_edge_filter_b_neon;
}
