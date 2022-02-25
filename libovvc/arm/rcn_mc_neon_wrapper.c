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

#include "arm/rcn_neon.h"

#define SIZE_BLOCK_4 1
#define SIZE_BLOCK_8 2
#define SIZE_BLOCK_16 3
#define SIZE_BLOCK_32 4
#define SIZE_BLOCK_64 5
#define SIZE_BLOCK_128 6


const int8_t ov_mc_filters_neon[16][8] =
{
    {   0, 1,  -3, 63,  4,  -2,  1,  0 },
    {  -1, 2,  -5, 62,  8,  -3,  1,  0 },
    {  -1, 3,  -8, 60, 13,  -4,  1,  0 },
    {  -1, 4, -10, 58, 17,  -5,  1,  0 },
    {  -1, 4, -11, 52, 26,  -8,  3, -1 },
    {  -1, 3,  -9, 47, 31, -10,  4, -1 },
    {  -1, 4, -11, 45, 34, -10,  4, -1 },
    {  -1, 4, -11, 40, 40, -11,  4, -1 },
    {  -1, 4, -10, 34, 45, -11,  4, -1 },
    {  -1, 4, -10, 31, 47,  -9,  3, -1 },
    {  -1, 3,  -8, 26, 52, -11,  4, -1 },
    {   0, 1,  -5, 17, 58, -10,  4, -1 },
    {   0, 1,  -4, 13, 60,  -8,  3, -1 },
    {   0, 1,  -3,  8, 62,  -5,  2, -1 },
    {   0, 1,  -2,  4, 63,  -3,  1,  0 },

    //Hpel for amvr
    {  0, 3, 9, 20, 20, 9, 3, 0 }
};

static const int8_t ov_mcp_filters_c[32][4] =
{
    //{  0, 64,  0,  0 },
    { -1, 63,  2,  0 },
    { -2, 62,  4,  0 },
    { -2, 60,  7, -1 },
    { -2, 58, 10, -2 },
    { -3, 57, 12, -2 },
    { -4, 56, 14, -2 },
    { -4, 55, 15, -2 },
    { -4, 54, 16, -2 },
    { -5, 53, 18, -2 },
    { -6, 52, 20, -2 },
    { -6, 49, 24, -3 },
    { -6, 46, 28, -4 },
    { -5, 44, 29, -4 },
    { -4, 42, 30, -4 },
    { -4, 39, 33, -4 },
    { -4, 36, 36, -4 },
    { -4, 33, 39, -4 },
    { -4, 30, 42, -4 },
    { -4, 29, 44, -5 },
    { -4, 28, 46, -6 },
    { -3, 24, 49, -6 },
    { -2, 20, 52, -6 },
    { -2, 18, 53, -5 },
    { -2, 16, 54, -4 },
    { -2, 15, 55, -4 },
    { -2, 14, 56, -4 },
    { -2, 12, 57, -3 },
    { -2, 10, 58, -2 },
    { -1,  7, 60, -2 },
    {  0,  4, 62, -2 },
    {  0,  2, 63, -1 },
    {  0,  0, 0, 0 }//to avoid buffer overflow
};

void sink(){};

void ov_put_vvc_bi0_pel_pixels_10_4_neon();
void ov_put_vvc_bi0_pel_pixels_10_8_neon();
void ov_put_vvc_bi0_pel_pixels_10_16_neon();
void ov_put_vvc_bi0_pel_pixels_10_32_neon();

void ov_put_vvc_bi1_pel_pixels_10_4_neon();//untested
void ov_put_vvc_bi1_pel_pixels_10_8_neon();
void ov_put_vvc_bi1_pel_pixels_10_16_neon();
void ov_put_vvc_bi1_pel_pixels_10_32_neon();

void ov_put_vvc_uni_qpel_h_10_8_neon();
void ov_put_vvc_uni_qpel_h_10_16_neon();
void ov_put_vvc_uni_qpel_h_10_32_neon();

void ov_put_vvc_uni_qpel_v_10_8_neon_8lines();
void ov_put_vvc_uni_qpel_v_10_8_neon_4lines();
void ov_put_vvc_uni_qpel_v_10_16_neon_8lines();
void ov_put_vvc_uni_qpel_v_10_16_neon_4lines();
void ov_put_vvc_uni_qpel_v_10_32_neon_8lines();
void ov_put_vvc_uni_qpel_v_10_32_neon_4lines();

void ov_put_vvc_uni_qpel_hv_10_8_neon_8lines();
void ov_put_vvc_uni_qpel_hv_10_8_neon_4lines();
void ov_put_vvc_uni_qpel_hv_10_16_neon_8lines();
void ov_put_vvc_uni_qpel_hv_10_16_neon_4lines();
void ov_put_vvc_uni_qpel_hv_10_32_neon_8lines();
void ov_put_vvc_uni_qpel_hv_10_32_neon_4lines();

void ov_put_vvc_bi0_qpel_h_10_8_neon();
void ov_put_vvc_bi0_qpel_h_10_16_neon();
void ov_put_vvc_bi0_qpel_h_10_32_neon();

void ov_put_vvc_bi0_qpel_v_10_8_neon_8lines();
void ov_put_vvc_bi0_qpel_v_10_8_neon_4lines();
void ov_put_vvc_bi0_qpel_v_10_16_neon_8lines();
void ov_put_vvc_bi0_qpel_v_10_16_neon_4lines();
void ov_put_vvc_bi0_qpel_v_10_32_neon_8lines();
void ov_put_vvc_bi0_qpel_v_10_32_neon_4lines();

void ov_put_vvc_bi0_qpel_hv_10_8_neon_8lines();
void ov_put_vvc_bi0_qpel_hv_10_8_neon_4lines();
void ov_put_vvc_bi0_qpel_hv_10_16_neon_8lines();
void ov_put_vvc_bi0_qpel_hv_10_16_neon_4lines();
void ov_put_vvc_bi0_qpel_hv_10_32_neon_8lines();
void ov_put_vvc_bi0_qpel_hv_10_32_neon_4lines();

void ov_put_vvc_bi1_qpel_h_10_8_neon();
void ov_put_vvc_bi1_qpel_h_10_16_neon();
void ov_put_vvc_bi1_qpel_h_10_32_neon();

void ov_put_vvc_bi1_qpel_v_10_8_neon_8lines();
void ov_put_vvc_bi1_qpel_v_10_8_neon_4lines();
void ov_put_vvc_bi1_qpel_v_10_16_neon_8lines();
void ov_put_vvc_bi1_qpel_v_10_16_neon_4lines();
void ov_put_vvc_bi1_qpel_v_10_32_neon_8lines();
void ov_put_vvc_bi1_qpel_v_10_32_neon_4lines();

void ov_put_vvc_bi1_qpel_hv_10_8_neon_8lines();
void ov_put_vvc_bi1_qpel_hv_10_8_neon_4lines();
void ov_put_vvc_bi1_qpel_hv_10_16_neon_8lines();
void ov_put_vvc_bi1_qpel_hv_10_16_neon_4lines();
void ov_put_vvc_bi1_qpel_hv_10_32_neon_8lines();
void ov_put_vvc_bi1_qpel_hv_10_32_neon_4lines();

void ov_put_vvc_uni_qpel_h_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_uni_qpel_h_10_8_neon(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], width);
                  }
void ov_put_vvc_uni_qpel_h_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_uni_qpel_h_10_16_neon(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], width);
                  }
void ov_put_vvc_uni_qpel_h_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_uni_qpel_h_10_32_neon(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], width);
                  }

void ov_put_vvc_uni_qpel_v_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if (height >= 8){
                      ov_put_vvc_uni_qpel_v_10_8_neon_8lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_uni_qpel_v_10_8_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_uni_qpel_v_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if (height >= 8){
                      ov_put_vvc_uni_qpel_v_10_16_neon_8lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_uni_qpel_v_10_16_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_uni_qpel_v_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if (height >= 8){
                      ov_put_vvc_uni_qpel_v_10_32_neon_8lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_uni_qpel_v_10_32_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }
                  }

void ov_put_vvc_uni_qpel_hv_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_uni_qpel_hv_10_8_neon_8lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_uni_qpel_hv_10_8_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_uni_qpel_hv_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_uni_qpel_hv_10_16_neon_8lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_uni_qpel_hv_10_16_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_uni_qpel_hv_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_uni_qpel_hv_10_32_neon_8lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_uni_qpel_hv_10_32_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }

void ov_put_vvc_bi0_qpel_h_10_8_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi0_qpel_h_10_8_neon(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], width);
                  }
void ov_put_vvc_bi0_qpel_h_10_16_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi0_qpel_h_10_16_neon(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], width);
                  }
void ov_put_vvc_bi0_qpel_h_10_32_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi0_qpel_h_10_32_neon(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], width);
                  }

void ov_put_vvc_bi0_qpel_v_10_8_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_bi0_qpel_v_10_8_neon_8lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi0_qpel_v_10_8_neon_4lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_qpel_v_10_16_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_bi0_qpel_v_10_16_neon_8lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi0_qpel_v_10_16_neon_4lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_qpel_v_10_32_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_bi0_qpel_v_10_32_neon_8lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi0_qpel_v_10_32_neon_4lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[my-1], width);
                    }
                  }

void ov_put_vvc_bi0_qpel_hv_10_8_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_bi0_qpel_hv_10_8_neon_8lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi0_qpel_hv_10_8_neon_4lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_qpel_hv_10_16_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_bi0_qpel_hv_10_16_neon_8lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi0_qpel_hv_10_16_neon_4lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_qpel_hv_10_32_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >=8){
                      ov_put_vvc_bi0_qpel_hv_10_32_neon_8lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi0_qpel_hv_10_32_neon_4lines(_dst, _src, _srcstride, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }

void ov_put_vvc_bi1_qpel_h_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi1_qpel_h_10_8_neon(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], width);
                  }
void ov_put_vvc_bi1_qpel_h_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi1_qpel_h_10_16_neon(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], width);
                  }
void ov_put_vvc_bi1_qpel_h_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi1_qpel_h_10_32_neon(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], width);
                  }

void ov_put_vvc_bi1_qpel_v_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 8){
                      ov_put_vvc_bi1_qpel_v_10_8_neon_8lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi1_qpel_v_10_8_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_qpel_v_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 8){
                      ov_put_vvc_bi1_qpel_v_10_16_neon_8lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi1_qpel_v_10_16_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_qpel_v_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 8){
                      ov_put_vvc_bi1_qpel_v_10_32_neon_8lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi1_qpel_v_10_32_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[my-1], width);
                    }
                  }

void ov_put_vvc_bi1_qpel_hv_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 8){
                      ov_put_vvc_bi1_qpel_hv_10_8_neon_8lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi1_qpel_hv_10_8_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_qpel_hv_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 8){
                      ov_put_vvc_bi1_qpel_hv_10_16_neon_8lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi1_qpel_hv_10_16_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_qpel_hv_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 8){
                      ov_put_vvc_bi1_qpel_hv_10_32_neon_8lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }else{
                      ov_put_vvc_bi1_qpel_hv_10_32_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mc_filters_neon[mx-1], ov_mc_filters_neon[my-1], width);
                    }
                  }

void ov_put_vvc_uni_epel_h_10_8_neon();
void ov_put_vvc_uni_epel_h_10_16_neon();
void ov_put_vvc_uni_epel_h_10_32_neon();

void ov_put_vvc_uni_epel_v_10_8_neon_4lines();
void ov_put_vvc_uni_epel_v_10_8_neon_2lines();
void ov_put_vvc_uni_epel_v_10_16_neon_4lines();
void ov_put_vvc_uni_epel_v_10_16_neon_2lines();
void ov_put_vvc_uni_epel_v_10_32_neon_4lines();
void ov_put_vvc_uni_epel_v_10_32_neon_2lines();

void ov_put_vvc_uni_epel_hv_10_8_neon_4lines();
void ov_put_vvc_uni_epel_hv_10_8_neon_2lines();
void ov_put_vvc_uni_epel_hv_10_16_neon_4lines();
void ov_put_vvc_uni_epel_hv_10_16_neon_2lines();
void ov_put_vvc_uni_epel_hv_10_32_neon_4lines();
void ov_put_vvc_uni_epel_hv_10_32_neon_2lines();

void ov_put_vvc_bi0_epel_h_10_8_neon();
void ov_put_vvc_bi0_epel_h_10_16_neon();
void ov_put_vvc_bi0_epel_h_10_32_neon();

void ov_put_vvc_bi0_epel_v_10_8_neon_4lines();
void ov_put_vvc_bi0_epel_v_10_8_neon_2lines();
void ov_put_vvc_bi0_epel_v_10_16_neon_4lines();
void ov_put_vvc_bi0_epel_v_10_16_neon_2lines();
void ov_put_vvc_bi0_epel_v_10_32_neon_4lines();
void ov_put_vvc_bi0_epel_v_10_32_neon_2lines();

void ov_put_vvc_bi0_epel_hv_10_8_neon_4lines();
void ov_put_vvc_bi0_epel_hv_10_8_neon_2lines();
void ov_put_vvc_bi0_epel_hv_10_16_neon_4lines();
void ov_put_vvc_bi0_epel_hv_10_16_neon_2lines();
void ov_put_vvc_bi0_epel_hv_10_32_neon_4lines();
void ov_put_vvc_bi0_epel_hv_10_32_neon_2lines();

void ov_put_vvc_bi1_epel_h_10_8_neon();
void ov_put_vvc_bi1_epel_h_10_16_neon();
void ov_put_vvc_bi1_epel_h_10_32_neon();

void ov_put_vvc_bi1_epel_v_10_8_neon_4lines();
void ov_put_vvc_bi1_epel_v_10_8_neon_2lines();
void ov_put_vvc_bi1_epel_v_10_16_neon_4lines();
void ov_put_vvc_bi1_epel_v_10_16_neon_2lines();
void ov_put_vvc_bi1_epel_v_10_32_neon_4lines();
void ov_put_vvc_bi1_epel_v_10_32_neon_2lines();

void ov_put_vvc_bi1_epel_hv_10_8_neon_4lines();
void ov_put_vvc_bi1_epel_hv_10_8_neon_2lines();
void ov_put_vvc_bi1_epel_hv_10_16_neon_4lines();
void ov_put_vvc_bi1_epel_hv_10_16_neon_2lines();
void ov_put_vvc_bi1_epel_hv_10_32_neon_4lines();
void ov_put_vvc_bi1_epel_hv_10_32_neon_2lines();

void ov_put_vvc_uni_epel_h_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_uni_epel_h_10_8_neon(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], width);
                  }
void ov_put_vvc_uni_epel_h_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_uni_epel_h_10_16_neon(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], width);
                  }
void ov_put_vvc_uni_epel_h_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_uni_epel_h_10_32_neon(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], width);
                  }

void ov_put_vvc_uni_epel_v_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_uni_epel_v_10_8_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[my - 1], width);
                    }else{
                      ov_put_vvc_uni_epel_v_10_8_neon_2lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[my - 1], width);
                    }
                  }
void ov_put_vvc_uni_epel_v_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_uni_epel_v_10_16_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[my - 1], width);
                    }else{
                      ov_put_vvc_uni_epel_v_10_16_neon_2lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[my - 1], width);
                    }
                  }
void ov_put_vvc_uni_epel_v_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_uni_epel_v_10_32_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[my - 1], width);
                    }else{
                      ov_put_vvc_uni_epel_v_10_32_neon_2lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[my - 1], width);
                    }
                  }

void ov_put_vvc_uni_epel_hv_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_uni_epel_hv_10_8_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], ov_mcp_filters_c[my - 1], width);
                    }else{
                      ov_put_vvc_uni_epel_hv_10_8_neon_2lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], ov_mcp_filters_c[my - 1], width);
                    }
                  }
void ov_put_vvc_uni_epel_hv_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_uni_epel_hv_10_16_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], ov_mcp_filters_c[my - 1], width);
                    }else{
                      ov_put_vvc_uni_epel_hv_10_16_neon_2lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], ov_mcp_filters_c[my - 1], width);
                    }
                  }
void ov_put_vvc_uni_epel_hv_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride,
                  const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_uni_epel_hv_10_32_neon_4lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], ov_mcp_filters_c[my - 1], width);
                    }else{
                      ov_put_vvc_uni_epel_hv_10_32_neon_2lines(_dst, _dststride, _src, _srcstride, height, ov_mcp_filters_c[mx - 1], ov_mcp_filters_c[my - 1], width);
                    }
                  }


void ov_put_vvc_bi0_epel_h_10_8_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi0_epel_h_10_8_neon(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], width);
                  }
void ov_put_vvc_bi0_epel_h_10_16_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi0_epel_h_10_16_neon(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], width);
                  }
void ov_put_vvc_bi0_epel_h_10_32_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi0_epel_h_10_32_neon(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], width);
                  }

void ov_put_vvc_bi0_epel_v_10_8_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi0_epel_v_10_8_neon_4lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi0_epel_v_10_8_neon_2lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_epel_v_10_16_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi0_epel_v_10_16_neon_4lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi0_epel_v_10_16_neon_2lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_epel_v_10_32_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi0_epel_v_10_32_neon_4lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi0_epel_v_10_32_neon_2lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[my-1], width);
                    }
                  }

void ov_put_vvc_bi0_epel_hv_10_8_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi0_epel_hv_10_8_neon_4lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi0_epel_hv_10_8_neon_2lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_epel_hv_10_16_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi0_epel_hv_10_16_neon_4lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi0_epel_hv_10_16_neon_2lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi0_epel_hv_10_32_neon_wrapper(int16_t* _dst, const uint16_t* _src, ptrdiff_t _srcstride,
                  int height, intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi0_epel_hv_10_32_neon_4lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi0_epel_hv_10_32_neon_2lines(_dst, _src, _srcstride, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }
                  }

void ov_put_vvc_bi1_epel_h_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi1_epel_h_10_8_neon(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], width);
                  }
void ov_put_vvc_bi1_epel_h_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi1_epel_h_10_16_neon(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], width);
                  }
void ov_put_vvc_bi1_epel_h_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    ov_put_vvc_bi1_epel_h_10_32_neon(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], width);
                  }

void ov_put_vvc_bi1_epel_v_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi1_epel_v_10_8_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi1_epel_v_10_8_neon_2lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_epel_v_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi1_epel_v_10_16_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi1_epel_v_10_16_neon_2lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_epel_v_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi1_epel_v_10_32_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi1_epel_v_10_32_neon_2lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[my-1], width);
                    }
                  }

void ov_put_vvc_bi1_epel_hv_10_8_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi1_epel_hv_10_8_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi1_epel_hv_10_8_neon_2lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_epel_hv_10_16_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi1_epel_hv_10_16_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi1_epel_hv_10_16_neon_2lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }
                  }
void ov_put_vvc_bi1_epel_hv_10_32_neon_wrapper(uint16_t* _dst, ptrdiff_t _dststride, const uint16_t* _src0,
                  ptrdiff_t _srcstride, const int16_t* _src1, int height,
                  intptr_t mx, intptr_t my, int width){
                    if(height >= 4){
                      ov_put_vvc_bi1_epel_hv_10_32_neon_4lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }else{
                      ov_put_vvc_bi1_epel_hv_10_32_neon_2lines(_dst, _dststride, _src0, _srcstride, _src1, height, ov_mcp_filters_c[mx-1], ov_mcp_filters_c[my-1], width);
                    }
                  }

void
rcn_init_mc_functions_neon(struct RCNFunctions* const rcn_funcs)
{
  struct MCFunctions* const mc_l = &rcn_funcs->mc_l;
  struct MCFunctions* const mc_c = &rcn_funcs->mc_c;

  /* Luma functions */

  mc_l->bidir0[0][SIZE_BLOCK_8] = &ov_put_vvc_bi0_pel_pixels_10_8_neon;
  mc_l->bidir1[0][SIZE_BLOCK_8] = &ov_put_vvc_bi1_pel_pixels_10_8_neon;
  
  mc_l->unidir[1][SIZE_BLOCK_8] = &ov_put_vvc_uni_qpel_h_10_8_neon_wrapper;
  mc_l->bidir0[1][SIZE_BLOCK_8] = &ov_put_vvc_bi0_qpel_h_10_8_neon_wrapper;
  mc_l->bidir1[1][SIZE_BLOCK_8] = &ov_put_vvc_bi1_qpel_h_10_8_neon_wrapper;
  
  mc_l->unidir[2][SIZE_BLOCK_8] = &ov_put_vvc_uni_qpel_v_10_8_neon_wrapper;
  mc_l->bidir0[2][SIZE_BLOCK_8] = &ov_put_vvc_bi0_qpel_v_10_8_neon_wrapper;
  mc_l->bidir1[2][SIZE_BLOCK_8] = &ov_put_vvc_bi1_qpel_v_10_8_neon_wrapper;
  
  mc_l->unidir[3][SIZE_BLOCK_8] = &ov_put_vvc_uni_qpel_hv_10_8_neon_wrapper;
  mc_l->bidir0[3][SIZE_BLOCK_8] = &ov_put_vvc_bi0_qpel_hv_10_8_neon_wrapper;
  mc_l->bidir1[3][SIZE_BLOCK_8] = &ov_put_vvc_bi1_qpel_hv_10_8_neon_wrapper;
  
  mc_l->bidir0[0][SIZE_BLOCK_16] = &ov_put_vvc_bi0_pel_pixels_10_16_neon;
  mc_l->bidir1[0][SIZE_BLOCK_16] = &ov_put_vvc_bi1_pel_pixels_10_16_neon;
  
  mc_l->unidir[1][SIZE_BLOCK_16] = &ov_put_vvc_uni_qpel_h_10_16_neon_wrapper;
  mc_l->bidir0[1][SIZE_BLOCK_16] = &ov_put_vvc_bi0_qpel_h_10_16_neon_wrapper;
  mc_l->bidir1[1][SIZE_BLOCK_16] = &ov_put_vvc_bi1_qpel_h_10_16_neon_wrapper;
  
  mc_l->unidir[2][SIZE_BLOCK_16] = &ov_put_vvc_uni_qpel_v_10_16_neon_wrapper;
  mc_l->bidir0[2][SIZE_BLOCK_16] = &ov_put_vvc_bi0_qpel_v_10_16_neon_wrapper;
  mc_l->bidir1[2][SIZE_BLOCK_16] = &ov_put_vvc_bi1_qpel_v_10_16_neon_wrapper;
  
  mc_l->unidir[3][SIZE_BLOCK_16] = &ov_put_vvc_uni_qpel_hv_10_16_neon_wrapper;
  mc_l->bidir0[3][SIZE_BLOCK_16] = &ov_put_vvc_bi0_qpel_hv_10_16_neon_wrapper;
  mc_l->bidir1[3][SIZE_BLOCK_16] = &ov_put_vvc_bi1_qpel_hv_10_16_neon_wrapper;
  
  mc_l->bidir0[0][SIZE_BLOCK_32] = &ov_put_vvc_bi0_pel_pixels_10_32_neon;
  mc_l->bidir1[0][SIZE_BLOCK_32] = &ov_put_vvc_bi1_pel_pixels_10_32_neon;
  
  mc_l->unidir[1][SIZE_BLOCK_32] = &ov_put_vvc_uni_qpel_h_10_32_neon_wrapper;
  mc_l->bidir0[1][SIZE_BLOCK_32] = &ov_put_vvc_bi0_qpel_h_10_32_neon_wrapper;
  mc_l->bidir1[1][SIZE_BLOCK_32] = &ov_put_vvc_bi1_qpel_h_10_32_neon_wrapper;
  
  mc_l->unidir[2][SIZE_BLOCK_32] = &ov_put_vvc_uni_qpel_v_10_32_neon_wrapper;
  mc_l->bidir0[2][SIZE_BLOCK_32] = &ov_put_vvc_bi0_qpel_v_10_32_neon_wrapper;
  mc_l->bidir1[2][SIZE_BLOCK_32] = &ov_put_vvc_bi1_qpel_v_10_32_neon_wrapper;
  
  mc_l->unidir[3][SIZE_BLOCK_32] = &ov_put_vvc_uni_qpel_hv_10_32_neon_wrapper;
  mc_l->bidir0[3][SIZE_BLOCK_32] = &ov_put_vvc_bi0_qpel_hv_10_32_neon_wrapper;
  mc_l->bidir1[3][SIZE_BLOCK_32] = &ov_put_vvc_bi1_qpel_hv_10_32_neon_wrapper;

  // /* Chroma functions */
  mc_c->bidir0[0][SIZE_BLOCK_4] = &ov_put_vvc_bi0_pel_pixels_10_4_neon;
  mc_c->bidir1[0][SIZE_BLOCK_4] = &ov_put_vvc_bi1_pel_pixels_10_4_neon;
  //
  mc_c->unidir[1][SIZE_BLOCK_4] = &ov_put_vvc_uni_epel_h_10_8_neon_wrapper;
  mc_c->bidir0[1][SIZE_BLOCK_4] = &ov_put_vvc_bi0_epel_h_10_8_neon_wrapper;
  mc_c->bidir1[1][SIZE_BLOCK_4] = &ov_put_vvc_bi1_epel_h_10_8_neon_wrapper;
  //
  mc_c->unidir[2][SIZE_BLOCK_4] = &ov_put_vvc_uni_epel_v_10_8_neon_wrapper;
  mc_c->bidir0[2][SIZE_BLOCK_4] = &ov_put_vvc_bi0_epel_v_10_8_neon_wrapper;
  mc_c->bidir1[2][SIZE_BLOCK_4] = &ov_put_vvc_bi1_epel_v_10_8_neon_wrapper;
  //
  mc_c->unidir[3][SIZE_BLOCK_4] = &ov_put_vvc_uni_epel_hv_10_8_neon_wrapper;
  mc_c->bidir0[3][SIZE_BLOCK_4] = &ov_put_vvc_bi0_epel_hv_10_8_neon_wrapper;
  mc_c->bidir1[3][SIZE_BLOCK_4] = &ov_put_vvc_bi1_epel_hv_10_8_neon_wrapper;
  //
  mc_c->bidir0[0][SIZE_BLOCK_8] = &ov_put_vvc_bi0_pel_pixels_10_8_neon;
  mc_c->bidir1[0][SIZE_BLOCK_8] = &ov_put_vvc_bi1_pel_pixels_10_8_neon;
  
  mc_c->unidir[1][SIZE_BLOCK_8] = &ov_put_vvc_uni_epel_h_10_8_neon_wrapper;
  mc_c->bidir0[1][SIZE_BLOCK_8] = &ov_put_vvc_bi0_epel_h_10_8_neon_wrapper;
  mc_c->bidir1[1][SIZE_BLOCK_8] = &ov_put_vvc_bi1_epel_h_10_8_neon_wrapper;
  
  mc_c->unidir[2][SIZE_BLOCK_8] = &ov_put_vvc_uni_epel_v_10_8_neon_wrapper;
  mc_c->bidir0[2][SIZE_BLOCK_8] = &ov_put_vvc_bi0_epel_v_10_8_neon_wrapper;
  mc_c->bidir1[2][SIZE_BLOCK_8] = &ov_put_vvc_bi1_epel_v_10_8_neon_wrapper;
  
  mc_c->unidir[3][SIZE_BLOCK_8] = &ov_put_vvc_uni_epel_hv_10_8_neon_wrapper;
  mc_c->bidir0[3][SIZE_BLOCK_8] = &ov_put_vvc_bi0_epel_hv_10_8_neon_wrapper;
  mc_c->bidir1[3][SIZE_BLOCK_8] = &ov_put_vvc_bi1_epel_hv_10_8_neon_wrapper;
  
  mc_c->bidir0[0][SIZE_BLOCK_16] = &ov_put_vvc_bi0_pel_pixels_10_16_neon;
  mc_c->bidir1[0][SIZE_BLOCK_16] = &ov_put_vvc_bi1_pel_pixels_10_16_neon;
  
  mc_c->unidir[1][SIZE_BLOCK_16] = &ov_put_vvc_uni_epel_h_10_16_neon_wrapper;
  mc_c->bidir0[1][SIZE_BLOCK_16] = &ov_put_vvc_bi0_epel_h_10_16_neon_wrapper;
  mc_c->bidir1[1][SIZE_BLOCK_16] = &ov_put_vvc_bi1_epel_h_10_16_neon_wrapper;
  
  mc_c->unidir[2][SIZE_BLOCK_16] = &ov_put_vvc_uni_epel_v_10_16_neon_wrapper;
  mc_c->bidir0[2][SIZE_BLOCK_16] = &ov_put_vvc_bi0_epel_v_10_16_neon_wrapper;
  mc_c->bidir1[2][SIZE_BLOCK_16] = &ov_put_vvc_bi1_epel_v_10_16_neon_wrapper;
  
  mc_c->unidir[3][SIZE_BLOCK_16] = &ov_put_vvc_uni_epel_hv_10_16_neon_wrapper;
  mc_c->bidir0[3][SIZE_BLOCK_16] = &ov_put_vvc_bi0_epel_hv_10_16_neon_wrapper;
  mc_c->bidir1[3][SIZE_BLOCK_16] = &ov_put_vvc_bi1_epel_hv_10_16_neon_wrapper;

  mc_c->bidir0[0][SIZE_BLOCK_32] = &ov_put_vvc_bi0_pel_pixels_10_32_neon;
  mc_c->bidir1[0][SIZE_BLOCK_32] = &ov_put_vvc_bi1_pel_pixels_10_32_neon;
  
  mc_c->unidir[1][SIZE_BLOCK_32] = &ov_put_vvc_uni_epel_h_10_32_neon_wrapper;
  mc_c->bidir0[1][SIZE_BLOCK_32] = &ov_put_vvc_bi0_epel_h_10_32_neon_wrapper;
  mc_c->bidir1[1][SIZE_BLOCK_32] = &ov_put_vvc_bi1_epel_h_10_32_neon_wrapper;
  
  mc_c->unidir[2][SIZE_BLOCK_32] = &ov_put_vvc_uni_epel_v_10_32_neon_wrapper;
  mc_c->bidir0[2][SIZE_BLOCK_32] = &ov_put_vvc_bi0_epel_v_10_32_neon_wrapper;
  mc_c->bidir1[2][SIZE_BLOCK_32] = &ov_put_vvc_bi1_epel_v_10_32_neon_wrapper;
  
  mc_c->unidir[3][SIZE_BLOCK_32] = &ov_put_vvc_uni_epel_hv_10_32_neon_wrapper;
  mc_c->bidir0[3][SIZE_BLOCK_32] = &ov_put_vvc_bi0_epel_hv_10_32_neon_wrapper;
  mc_c->bidir1[3][SIZE_BLOCK_32] = &ov_put_vvc_bi1_epel_hv_10_32_neon_wrapper;
}
