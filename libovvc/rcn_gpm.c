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

#include <stdint.h>

#include "ovutils.h"
#include "rcn.h"
#include "ctudec.h"
#include "drv_utils.h"

static const int8_t g_angle2mask[GEO_NUM_ANGLES] =
{
    0, -1,  1,  2,  3,  4, -1, -1,
    5, -1, -1,  4,  3,  2,  1, -1,

    0, -1,  1,  2,  3,  4, -1, -1,
    5, -1, -1,  4,  3,  2,  1, -1
};

uint8_t g_globalGeoWeights[GEO_NUM_PRESTORED_MASK][GEO_WEIGHT_MASK_SIZE * GEO_WEIGHT_MASK_SIZE];

const int8_t g_weightOffset_x[GEO_NUM_PARTITION_MODE][GEO_NUM_CU_SIZE][GEO_NUM_CU_SIZE] =
{
    {
        {53, 50, 44, 32}, {53, 50, 44, 32},
        {53, 50, 44, 32}, {53, 50, 44, 32}},
    {
        {55, 54, 52, 48}, {55, 54, 52, 48},
        {55, 54, 52, 48}, {55, 54, 52, 48}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 50, 44, 32}, {52, 48, 44, 32},
        {52, 48, 40, 32}, {52, 48, 40, 24}},
    {
        {52, 52, 48, 40}, {52, 48, 48, 40},
        {52, 48, 40, 40}, {52, 48, 40, 24}},
    {
        {52, 54, 52, 48}, {52, 48, 52, 48},
        {52, 48, 40, 48}, {52, 48, 40, 24}},
    {
        {51, 46, 36, 16}, {51, 46, 36, 16},
        {51, 46, 36, 16}, {51, 46, 36, 16}},
    {
        {49, 42, 28,  0}, {49, 42, 28,  0},
        {49, 42, 28,  0}, {49, 42, 28,  0}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 48, 40, 24}, {52, 48, 40, 24},
        {52, 48, 40, 24}, {52, 48, 40, 24}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}},
    {
        {52, 46, 36, 16}, {52, 48, 36, 16},
        {52, 48, 40, 16}, {52, 48, 40, 24}},
    {
        {52, 44, 32,  8}, {52, 48, 32,  8},
        {52, 48, 40,  8}, {52, 48, 40, 24}},
    {
        {52, 42, 28,  0}, {52, 48, 28,  0},
        {52, 48, 40,  0}, {52, 48, 40, 24}}
};

const int8_t g_weightOffset_y[GEO_NUM_PARTITION_MODE][GEO_NUM_CU_SIZE][GEO_NUM_CU_SIZE] =
{
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {53, 53, 53, 53}, {50, 50, 50, 50},
        {44, 44, 44, 44}, {32, 32, 32, 32}},
    {
        {55, 55, 55, 55}, {54, 54, 54, 54},
        {52, 52, 52, 52}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {53, 52, 52, 52}, {50, 50, 48, 48},
        {44, 44, 44, 40}, {32, 32, 32, 32}},
    {
        {54, 52, 52, 52}, {52, 52, 48, 48},
        {48, 48, 48, 40}, {40, 40, 40, 40}},
    {
        {55, 52, 52, 52}, {54, 54, 48, 48},
        {52, 52, 52, 40}, {48, 48, 48, 48}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {52, 52, 52, 52}, {48, 48, 48, 48},
        {40, 40, 40, 40}, {24, 24, 24, 24}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}},
    {
        {51, 51, 51, 51}, {46, 46, 46, 46},
        {36, 36, 36, 36}, {16, 16, 16, 16}},
    {
        {49, 49, 49, 49}, {42, 42, 42, 42},
        {28, 28, 28, 28}, { 0,  0,  0,  0}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}},
    {
        {51, 52, 52, 52}, {46, 46, 48, 48},
        {36, 36, 36, 40}, {16, 16, 16, 16}},
    {
        {50, 52, 52, 52}, {44, 44, 48, 48},
        {32, 32, 32, 40}, { 8,  8,  8,  8}},
    {
        {49, 52, 52, 52}, {42, 42, 48, 48},
        {28, 28, 28, 40}, { 0,  0,  0,  0}}
};


const int16_t g_GeoParams[GEO_NUM_PARTITION_MODE][2] =
{
    {0, 1},
    {0, 3},

    {2, 0},
    {2, 1},
    {2, 2},
    {2, 3},

    {3, 0},
    {3, 1},
    {3, 2},
    {3, 3},

    {4, 0},
    {4, 1},
    {4, 2},
    {4, 3},

    {5, 0},
    {5, 1},
    {5, 2},
    {5, 3},

    {8, 1},
    {8, 3},

    {11, 0},
    {11, 1},
    {11, 2},
    {11, 3},

    {12, 0},
    {12, 1},
    {12, 2},
    {12, 3},

    {13, 0},
    {13, 1},
    {13, 2},
    {13, 3},

    {14, 0},
    {14, 1},
    {14, 2},
    {14, 3},

    {16, 1},
    {16, 3},

    {18, 1},
    {18, 2},
    {18, 3},

    {19, 1},
    {19, 2},
    {19, 3},

    {20, 1},
    {20, 2},
    {20, 3},

    {21, 1},
    {21, 2},
    {21, 3},

    {24, 1},
    {24, 3},

    {27, 1},
    {27, 2},
    {27, 3},

    {28, 1},
    {28, 2},
    {28, 3},

    {29, 1},
    {29, 2},
    {29, 3},

    {30, 1},
    {30, 2},
    {30, 3}
};

const int8_t g_Dis[GEO_NUM_ANGLES] =
{
    8,  8,  8,  8,  4,  4,  2,  1,
    0, -1, -2, -4, -4, -8, -8, -8,

   -8, -8, -8, -8, -4, -4, -2, -1,
    0,  1,  2,  4,  4,  8,  8,  8
};

void
rcn_init_gpm_params()
{
    uint8_t angle_idx;
    for (angle_idx = 0; angle_idx < (GEO_NUM_ANGLES >> 2) + 1; angle_idx++){
        int x, y;

        if (g_angle2mask[angle_idx] == -1){
            continue;
        }

        int dist_x = g_Dis[angle_idx];
        int dist_y = g_Dis[(angle_idx + (GEO_NUM_ANGLES >> 2)) % GEO_NUM_ANGLES];

        int16_t rho = (int32_t)((uint32_t)dist_x << (GEO_MAX_CU_LOG2 + 1)) + (int32_t)((uint32_t)dist_y << (GEO_MAX_CU_LOG2 + 1));

        static const int16_t offset_msk = (2 * GEO_MAX_CU_SIZE - GEO_WEIGHT_MASK_SIZE) >> 1;
        int idx = 0;
        for (y = 0; y < GEO_WEIGHT_MASK_SIZE; y++) {
            int16_t lookup_y = (((y + offset_msk) << 1) + 1) * dist_y;
            for (x = 0; x < GEO_WEIGHT_MASK_SIZE; x++) {
                int16_t sx_i = ((x + offset_msk) << 1) + 1;
                int16_t weightIdx = sx_i * dist_x + lookup_y - rho;
                int weightLinearIdx = 32 + weightIdx;

                g_globalGeoWeights[g_angle2mask[angle_idx]][idx++] = ov_clip((weightLinearIdx + 4) >> 3, 0, 8);
            }
        }
    }
}
