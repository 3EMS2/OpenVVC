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

.macro OFFSET_REGUALATE, val, b=.b    
    mov \val\b[1] , wzr
    mov \val\b[3] , wzr
    mov \val\b[5] , wzr
    mov \val\b[7] , wzr
    mov \val\b[9] , wzr
    mov \val\b[11], wzr
    mov \val\b[13], wzr
    mov \val\b[15], wzr
.endm

.macro SHIFT_VECT_ELEM, val, h=.h
    mov \val\h[0], \val\h[1]
    mov \val\h[1], \val\h[2]
    mov \val\h[2], \val\h[3]
    mov \val\h[3], \val\h[4]
    mov \val\h[4], \val\h[5]
    mov \val\h[5], \val\h[6]
    mov \val\h[6], \val\h[7]
.endm

.macro REPLACE, val1, val2, h=.h
    mov \val1\h[0], \val2\h[0]
    mov \val1\h[1], \val2\h[1]
    mov \val1\h[2], \val2\h[2]
    mov \val1\h[3], \val2\h[3]
    mov \val1\h[4], \val2\h[4]
    mov \val1\h[5], \val2\h[5]
    mov \val1\h[6], \val2\h[6]
    mov \val1\h[7], \val2\h[7]
.endm

.macro ALIGN_UP, val1, val2, val3, h=.h
    mov \val1\h[0], \val3\h[7]
    mov \val1\h[1], \val2\h[0]
    mov \val1\h[2], \val2\h[1]
    mov \val1\h[3], \val2\h[2]
    mov \val1\h[4], \val2\h[3]
    mov \val1\h[5], \val2\h[4]
    mov \val1\h[6], \val2\h[5]
    mov \val1\h[7], \val2\h[6]
.endm

.macro ALIGN_DW, val1, val2, val3, h=.h
    mov \val1\h[0], \val2\h[1]
    mov \val1\h[1], \val2\h[2]
    mov \val1\h[2], \val2\h[3]
    mov \val1\h[3], \val2\h[4]
    mov \val1\h[4], \val2\h[5]
    mov \val1\h[5], \val2\h[6]
    mov \val1\h[6], \val2\h[7]
    mov \val1\h[7], \val3\h[0]
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

    ld1 {v4.8h-v7.8h}, [x4]

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
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr
    ld1 {v4.8h-v7.8h}, [x4]

0:  
    CBZ x3, 3f
    sub x3, x3, #1 // x -=1
    ld1 {v8.8h}, [x0]  // load current source data
    mov x8, x2 // store width to x8 register
    //
    mov x9 , x0 // strore src pointer to x9 register
    //
    LD1R {v10.8h}, [x1]
   
1: 
    CBZ x8, 2f
    sub x8, x8, #8 // y -=8
    Add x10, x9, #16
    ld1 {v11.8h}, [x10] // load next source data
    // need to be completed with equilante of _mm_alignr_epi8, use V12 => a, V13 => b
    ALIGN_UP v12, v8, v10
    ALIGN_DW v13, v8, v11
    //
    CMGE v14.8h, v8.8h, v12.8h
    CMGT v15.8h, v12.8h, v8.8h
    AND v14.16b, v14.16b, v12.16b
    AND v15.16b, v15.16b, v8.16b
    ORR v14.16b, v14.16b, v15.16b
    // _mm_cmpeq_epi16 
    CMEQ v15.8h, v12.8h, v14.8h  // x1
    CMEQ v16.8h, v8.8h, v14.8h  // x2
    SUB v15.8h, v16.8h, v15.8h   // x1=x1-x2

    CMGE v14.8h, v8.8h, v13.8h
    CMGT v16.8h, v13.8h, v8.8h
    AND v14.16b, v14.16b, v13.16b
    AND v16.16b, v16.16b, v8.16b
    ORR v14.16b, v14.16b, v16.16b
    // _mm_cmpeq_epi16 
    CMEQ V16.8h, v13.8h, v14.8H // x3
    CMEQ v17.8h, v8.8h, v14.8h // x2
    SUB v16.8h, v17.8h, v16.8h // x2=x2-x3

    add v15.8h, v15.8h, v16.8h // x1+x2

    mov w15, #-2
    dup v12.8h, w15
    CMEQ v12.8h, v15.8h, v12.8h // r0
    mov w15, #-1
    dup v13.8h, w15
    CMEQ v13.8h, v15.8h, v13.8h // r1
    mov w15, #1
    dup v14.8h, w15
    CMEQ v14.8h, v15.8h, v14.8h // r3
    mov w15, #2
    dup v16.8h, w15
    CMEQ v15.8h, v15.8h, v16.8h // r4
    // AND offset val
    AND v12.16b, v12.16b, v4.16b // r0 & offset0
    AND v13.16b, v13.16b, v5.16b // r1 & offset0
    AND v14.16b, v14.16b, v6.16b // r3 & offset0
    AND v15.16b, v15.16b, v7.16b // r4 & offset0
    // add all with current
    add v12.8h, v12.8h, v13.8h // r0 + r1
    add v12.8h, v12.8h, v14.8h // r0 + r1 + r3
    add v12.8h, v12.8h, v15.8h // r0 = r0 + r1 + r3 + r4
    add v12.8h, v12.8h, v8.8h  // r0 = r0 + current
    // clip and store
    CLIP V12.8h, v31.8h, v30.8h
    ST1 {v12.8h}, [x9]
    Add x9, x9, #16
    // prev = c
    // c = next
    REPLACE v10, v8 
    REPLACE v8, v11
    B 1b
2: 
    add x0, x0, x5
    // change load for the source left data
    add x1, x1, #2
    B 0b
3:  
    ret lr
 
endfunc


function sao_edge_filter_v_neon, export=1
      // preparing the clippin borders
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr
    // preparing offset values
    ld1 {v4.8h-v7.8h}, [x5]
    
0:  
    CBZ x3, 3f
    sub x3, x3, #8 // x -=8
    ld1 {v8.8h}, [x0]  // load current source data   : c
    ld1 {v9.8h}, [x1]  // load bottom source row data : a
    //
    add x1, x1, #16
    mov x8, x4 // store height to x8 register
    mov x9, x0 // strore src pointer to x9 register   
1: 
    CBZ x8, 2f
    sub x8, x8, #1 // y -=1
    add x14, x9, x6
    ld1 {v10.8h}, [x14] // load next source data : b

    // _mm_min_epu16(): use v14
    CMGE v14.8h, v8.8h, v9.8h
    CMGT v15.8h, v9.8h, v8.8h
    AND v14.16b, v14.16b, v9.16b
    AND v15.16b, v15.16b, v8.16b
    ORR v14.16b, v14.16b, v15.16b
    // _mm_cmpeq_epi16 
    CMEQ v15.8h, v9.8h, v14.8h  // x1
    CMEQ v16.8h, v8.8h, v14.8h  // x2
    SUB v15.8h, v16.8h, v15.8h   // x1=x1-x2

    // _mm_min_epu16(): use v14
    CMGE v14.8h, v8.8h, v10.8h
    CMGT v16.8h, v10.8h, v8.8h
    AND v14.16b, v14.16b, v10.16b
    AND v16.16b, v16.16b, v8.16b
    ORR v14.16b, v14.16b, v16.16b
    // _mm_cmpeq_epi16 
    CMEQ V16.8h, v10.8h, v14.8H // x3
    CMEQ v17.8h, v8.8h, v14.8h // x2
    SUB v16.8h, v17.8h, v16.8h // x2=x2-x3

    // _mm_add_epi16
    add v15.8h, v15.8h, v16.8h // x1+x2

    //_mm_cmpeq_epi16 => v12, v13, v14, v15
    mov w15, #-2
    dup v12.8h, w15
    CMEQ v12.8h, v15.8h, v12.8h // r0
    mov w15, #-1
    dup v13.8h, w15
    CMEQ v13.8h, v15.8h, v13.8h // r1
    mov w15, #1
    dup v14.8h, w15
    CMEQ v14.8h, v15.8h, v14.8h // r3
    mov w15, #2
    dup v16.8h, w15
    CMEQ v15.8h, v15.8h, v16.8h // r4
    // AND offset val
    AND v12.16b, v12.16b, v4.16b // r0 & offset0
    AND v13.16b, v13.16b, v5.16b // r1 & offset0
    AND v14.16b, v14.16b, v6.16b // r3 & offset0
    AND v15.16b, v15.16b, v7.16b // r4 & offset0
    // add all with current
    add v12.8h, v12.8h, v13.8h // r0 + r1
    add v12.8h, v12.8h, v14.8h // r0 + r1 + r3
    add v12.8h, v12.8h, v15.8h // r0 = r0 + r1 + r3 + r4
    add v12.8h, v12.8h, v8.8h  // r0 = r0 + current
    // clip and store
    CLIP V12.8h, v31.8h, v30.8h
    ST1 {v12.8h}, [x9]
    Add x9, x9, x6
    // prev = c
    // c = next
    REPLACE v9, v8 
    REPLACE v8, v10
    B 1b
2: 
    add x0, x0, #16
    B 0b
3:  
    ret lr
   
endfunc

function sao_edge_filter_d_neon, export=1
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr
    ld1 {v4.8h-v7.8h}, [x5]
    mov x10, x0
    mov x12, xzr
    
0:  
    CBZ x3, 3f
    sub x3, x3, #8 // x -=8
    
    
    ld1 {v9.8h}, [x1]  // load left data
    add x1, x1, #16
    
    ld1 {v8.8h}, [x10]  // load curent source row data 
    add x10, x10, #16
    mov x9, x0
    //
    mov x11, x2
    //
    mov x8, x4 
1: 
    CBZ x8, 2f
    sub x8, x8, #1 // y -=1
    add x14, x9, x6
    add x14, x14, x12
    ld1 {v10.8h}, [x14] // load next source data : _b
    add x14, x14, #16
    ld1 {v11.8h}, [x14] // load next source data : b_
    // store a0
    LD1R {v19.8h}, [x11]
    ld1 {v21.8h}, [x11]
    mov v21.h[0], v9.h[7]
    st1 {v21.8h}, [x11]
    add x11, x11, #2 

    // align up/down
    ALIGN_UP v12, v9, v19
    ALIGN_DW v13, v10, v11

    CMGE v14.8h, v8.8h, v12.8h
    CMGT v15.8h, v12.8h, v8.8h
    AND v14.16b, v14.16b, v12.16b
    AND v15.16b, v15.16b, v8.16b
    ORR v14.16b, v14.16b, v15.16b
    // _mm_cmpeq_epi16 
    CMEQ v15.8h, v12.8h, v14.8h  // x1
    CMEQ v16.8h, v8.8h, v14.8h  // x2
    SUB v15.8h, v16.8h, v15.8h   // x1=x1-x2

    CMGE v14.8h, v8.8h, v13.8h
    CMGT v16.8h, v13.8h, v8.8h
    AND v14.16b, v14.16b, v13.16b
    AND v16.16b, v16.16b, v8.16b
    ORR v14.16b, v14.16b, v16.16b
    // _mm_cmpeq_epi16 
    CMEQ V16.8h, v13.8h, v14.8H // x3
    CMEQ v17.8h, v8.8h, v14.8h // x2
    SUB v16.8h, v17.8h, v16.8h // x2=x2-x3

    // _mm_add_epi16
    add v15.8h, v15.8h, v16.8h // x1+x2

    mov w15, #-2
    dup v12.8h, w15
    CMEQ v12.8h, v15.8h, v12.8h // r0
    mov w15, #-1
    dup v13.8h, w15
    CMEQ v13.8h, v15.8h, v13.8h // r1
    mov w15, #1
    dup v14.8h, w15
    CMEQ v14.8h, v15.8h, v14.8h // r3
    mov w15, #2
    dup v16.8h, w15
    CMEQ v15.8h, v15.8h, v16.8h // r4
    // AND offset val
    AND v12.16b, v12.16b, v4.16b // r0 & offset0
    AND v13.16b, v13.16b, v5.16b // r1 & offset0
    AND v14.16b, v14.16b, v6.16b // r3 & offset0
    AND v15.16b, v15.16b, v7.16b // r4 & offset0
    // add all with current
    add v12.8h, v12.8h, v13.8h // r0 + r1
    add v12.8h, v12.8h, v14.8h // r0 + r1 + r3
    add v12.8h, v12.8h, v15.8h // r0 = r0 + r1 + r3 + r4
    add v12.8h, v12.8h, v8.8h  // r0 = r0 + current
    // clip and store
    CLIP V12.8h, v31.8h, v30.8h
    add x15, x9, x12
    ST1 {v12.8h}, [x15]

    REPLACE v9, v8 
    REPLACE v8, v10
    Add x9, x9, x6
    B 1b
2: 
    add x12, x12, #16
    B 0b
3:  
    ret lr
   
endfunc

function sao_edge_filter_b_neon, export=1
  
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr
    ld1 {v4.8h-v7.8h}, [x5]
    mov x10, x0
    mov x12, xzr
    
0:  
    CBZ x3, 3f
    sub x3, x3, #8 // x -=8
    
    ld1 {v9.8h}, [x1]  // load left data
    add x14, x1, #16
    ld1 {v20.8h}, [x14]  // load left data
    ld1 {v8.8h}, [x10]  // load curent source row data 
    add x1, x1, #16
    add x10, x10, #16

    mov x9, x0
    //
    mov x11, x2
    add x11, x2, #2
    //
    mov x8, x4 
1: 
    CBZ x8, 2f
    sub x8, x8, #1 // y -=1
    add x14, x9, x12
    add x14, x14, #16
    ld1 {v10.8h}, [x14] // load next source data : _b
    add x14, x9, x12
    add x14, x14, x6
    ld1 {v11.8h}, [x14] // load next source data : b_
    // store b0
    LD1R {v19.8h}, [x11]
    ld1 {v21.8h}, [x11]
    mov v21.h[0], v11.h[7]
    st1 {v21.8h}, [x11]
    add x11, x11, #2 

    // align up/down
    ALIGN_UP v12, v11, v19
    ALIGN_DW v13, v9, v20

    // min : result in v14
    CMGE v14.8h, v8.8h, v12.8h
    CMGT v15.8h, v12.8h, v8.8h
    AND v14.16b, v14.16b, v12.16b
    AND v15.16b, v15.16b, v8.16b
    ORR v14.16b, v14.16b, v15.16b
    CMEQ v15.8h, v12.8h, v14.8h  // x1
    CMEQ v16.8h, v8.8h, v14.8h  // x2
    SUB v15.8h, v16.8h, v15.8h   // x1=x1-x2

    CMGE v14.8h, v8.8h, v13.8h
    CMGT v16.8h, v13.8h, v8.8h
    AND v14.16b, v14.16b, v13.16b
    AND v16.16b, v16.16b, v8.16b
    ORR v14.16b, v14.16b, v16.16b
    // _mm_cmpeq_epi16 
    CMEQ V16.8h, v13.8h, v14.8H // x3
    CMEQ v17.8h, v8.8h, v14.8h // x2
    SUB v16.8h, v17.8h, v16.8h // x2=x2-x3

    add v15.8h, v15.8h, v16.8h // x1+x2

    //compare (-2,-1,1,2) => v12, v13, v14, v15
    mov w15, #-2
    dup v12.8h, w15
    CMEQ v12.8h, v15.8h, v12.8h // r0
    mov w15, #-1
    dup v13.8h, w15
    CMEQ v13.8h, v15.8h, v13.8h // r1
    mov w15, #1
    dup v14.8h, w15
    CMEQ v14.8h, v15.8h, v14.8h // r3
    mov w15, #2
    dup v16.8h, w15
    CMEQ v15.8h, v15.8h, v16.8h // r4
    // AND offset val
    AND v12.16b, v12.16b, v4.16b // r0 & offset0
    AND v13.16b, v13.16b, v5.16b // r1 & offset0
    AND v14.16b, v14.16b, v6.16b // r3 & offset0
    AND v15.16b, v15.16b, v7.16b // r4 & offset0
    // add all with current
    add v12.8h, v12.8h, v13.8h // r0 + r1
    add v12.8h, v12.8h, v14.8h // r0 + r1 + r3
    add v12.8h, v12.8h, v15.8h // r0 = r0 + r1 + r3 + r4
    add v12.8h, v12.8h, v8.8h  // r0 = r0 + current
    // clip and store
    CLIP V12.8h, v31.8h, v30.8h
    add x15, x9, x12
    ST1 {v12.8h}, [x15]

    REPLACE v9 , v8 
    REPLACE v20, V10
    REPLACE v8 , V11
    Add x9, x9, x6
    B 1b
2: 
    add x12, x12, #16
    B 0b
3:  
    ret lr
   
endfunc

