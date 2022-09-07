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

    sub w11, xzr , #5 // -shift 
    dup V28.8h, w11
    mov w10, #0x03FF
    dup v30.8h, w10
    dup v31.8h, wzr

    ld1 {v0.8h}, [x1]
    
    DUP V11.8h, w2 // sao_offset_val 1 to 4
    DUP V12.8h, w3
    DUP V13.8h, w4
    DUP V14.8h, w5

    AND w9 ,w6, #31 // sao_left_class 1 to 4
    DUP v2.8h, w9

    ADD w6, w6, #1
    AND w9 ,w6, #31
    DUP v3.8h, w9

    ADD w6, w6, #1
    AND w9 ,w6, #31
    DUP v4.8h, w9

    ADD w6, w6, #1
    AND w9, w6, #31
    DUP v5.8h, w9

    // shift source before add
    SSHL V10.8H, V0.8H, V28.8H

    // perform the comparaison between V10 and (v2 to v5)  => save in  v6, v7, v8 and v9  


    // AND logic between v6,v7,v8, v9 and v11, v12, v13, v14 => save in v6 to v9 
    ADD V6.8H, V6.8H, V11.8H
    ADD V7.8H, V7.8H, V12.8H
    ADD V8.8H, V8.8H, V13.8H
    ADD V9.8H, V9.8H, V14.8H
    // OR logic between v6 and v7 => save in v6
    ORR V6.8h, V6.8h, V7.8h
    // OR logic between v8 and v9 => save in v8
    ORR V8.8h, V8.8h, V9.8h
    // OR logic between v6 and v8 => save in v6
    ORR V6.8h, V6.8h, V8.8h
    // ADD, CLIP and store
    ADD  V0.8H , V0.8H , V6.8H
    CLIP V0.8h, v31.8h, v30.8h
    ST1 {v0.8h}, [x0]
    ret lr
endfunc

