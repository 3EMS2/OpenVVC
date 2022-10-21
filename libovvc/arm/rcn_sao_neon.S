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
#include "arm/asm.S"

.macro CLIP, val, min_val, max_val
       SMAX  \val, \val, \min_val
       SMIN  \val, \val, \max_val
.endm 


function sao_band_filter_neon, export=1
    
    mov w11, #5
    sub w10, wzr , w11 // -shift 
    dup V28.8h, w10
    // preparing the clippin borders
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr

    // preparing the first five registers
    AND w8 ,w5, #31
    dup V0.8h, w8
    Add w5, w5, #1
    AND w8 ,w5, #31
    dup V1.8h, w8
    Add w5, w5, #1
    AND w8 ,w5, #31
    dup V2.8h, w8
    Add w5, w5, #1
    AND w8 ,w5, #31
    dup V3.8h, w8
    //
    dup v9.16b, wzr

    ld1 {v4.8h}, [x4]
    dup v5.16b, v4.b[1]
    dup v6.16b, v4.b[2]
    dup v7.16b, v4.b[3]
    dup v4.16b, v4.b[0]

    // offset_val[0] broadcast over 8 16-bits registers
    mov v4.b[1] , wzr
    mov v4.b[3] , wzr
    mov v4.b[5] , wzr
    mov v4.b[7] , wzr
    mov v4.b[9] , wzr
    mov v4.b[11], wzr
    mov v4.b[13], wzr
    mov v4.b[15], wzr
    // offset_val[1] broadcast
    mov v5.b[1] , wzr
    mov v5.b[3] , wzr
    mov v5.b[5] , wzr
    mov v5.b[7] , wzr
    mov v5.b[9] , wzr
    mov v5.b[11], wzr
    mov v5.b[13], wzr
    mov v5.b[15], wzr
    // offset_val[2] broadcast
    mov v6.b[1] , wzr
    mov v6.b[3] , wzr
    mov v6.b[5] , wzr
    mov v6.b[7] , wzr
    mov v6.b[9] , wzr
    mov v6.b[11], wzr
    mov v6.b[13], wzr
    mov v6.b[15], wzr
    // offset_val[3] broadcast
    mov v7.b[1] , wzr
    mov v7.b[3] , wzr
    mov v7.b[5] , wzr
    mov v7.b[7] , wzr
    mov v7.b[9] , wzr
    mov v7.b[11], wzr
    mov v7.b[13], wzr
    mov v7.b[15], wzr
    // first loop

0:  
    CBZ x3, 3f
    sub x3, x3, #1 // x -=1
    mov x8, x2
    //second loop
    mov x9 , x1
    mov x10, x0
1: 
    CBZ x8, 2f // if x8 = 0 jump to 2 forward
    sub x8, x8, #8 // y -=8
    ld1 {v8.8h}, [x9]  // load source data
    add x9, x9, #16
    // shift source before add
    SSHL V9.8H, V8.8H, V28.8H

    // perform the comparaison between V10 and (v0 to v3)  => save in  v10, v11, v12 and v13  
    CMEQ v10.8h, v9.8h, v0.8h
    CMEQ v11.8h, v9.8h, v1.8h
    CMEQ v12.8h, v9.8h, v2.8h
    CMEQ v13.8h, v9.8h, v3.8h
    // AND logic between v10,v11,v12, v13 and v4, v5, v6, v7 => save in v10 to v13 
    AND V10.16b, V10.16b, V4.16b
    AND V11.16b, V11.16b, V5.16b
    AND V12.16b, V12.16b, V6.16b
    AND V13.16b, V13.16b, V7.16b
    // OR logic between v10 and v12 => save in v10
    ORR V10.16b, V10.16b, V11.16b
    ORR V12.16b, V12.16b, V13.16b
    ORR V10.16b, V10.16b, V12.16b

    // ADD, CLIP and store
    ADD  v8.8H , V8.8H , V10.8H
    CLIP V8.8h, v31.8h, v30.8h
    ST1 {v8.8h}, [x10]
    add x10, x10, #16
    B 1b
2: 
    add x0, x0, x6
    add x1, x1, x7
    B 0b

3:  
    ret lr
endfunc


function sao_edge_filter_h_neon, export=1

    
endfunc

function sao_edge_filter_v_neon, export=1

   
endfunc

function sao_edge_filter_d_neon, export=1

endfunc


function sao_edge_filter_b_neon, export=1
  

endfunc

