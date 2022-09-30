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


function band_filter_neon, export=1


endfunc


function sao_edge_filter_h_neon, export=1

    sub w11, xzr , #5 // -shift 
    dup V28.8h, w11
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr

    ld1 {v0.8h}, [x1]
    
   
    // ADD, CLIP and store
    ADD  V0.8H , V0.8H , V6.8H
    CLIP V0.8h, v31.8h, v30.8h
    ST1 {v0.8h}, [x0]
    ret lr
endfunc

function sao_edge_filter_v_neon, export=1

    sub w11, xzr , #5 // -shift 
    dup V28.8h, w11
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr

    ld1 {v0.8h}, [x1]
    
   
    // ADD, CLIP and store
    ADD  V0.8H , V0.8H , V6.8H
    CLIP V0.8h, v31.8h, v30.8h
    ST1 {v0.8h}, [x0]
    ret lr
endfunc

function sao_edge_filter_d_neon, export=1

    sub w11, xzr , #5 // -shift 
    dup V28.8h, w11
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr

    ld1 {v0.8h}, [x1]
    
   
    // ADD, CLIP and store
    ADD  V0.8H , V0.8H , V6.8H
    CLIP V0.8h, v31.8h, v30.8h
    ST1 {v0.8h}, [x0]
    ret lr
endfunc


function sao_edge_filter_b_neon, export=1
  sub w11, xzr , #5 // -shift
  dup V28.8h, w11
  mov w10, #0x03FF
  dup v30.8h, w10
  dup v31.8h, wzr

  ld1 {v0.8h}, [x1]


  // ADD, CLIP and store
  ADD  V0.8H , V0.8H , V6.8H
  CLIP V0.8h, v31.8h, v30.8h
  ST1 {v0.8h}, [x0]
  ret lr

endfunc
