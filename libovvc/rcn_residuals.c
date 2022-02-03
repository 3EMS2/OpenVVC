/* Operation performed on residual after residual coefficient have been
 * decoded and transform has been performed before adding them to
 * Prediction Block
 */

#include <stdlib.h>
#include "ovutils.h"
#include "ctudec.h"

#include "bitdepth.h"

static void
scale_add_residual(const int16_t *src, OVSample *dst,
                      int log2_tb_w, int log2_tb_h,
                      int scale)
{
    int i, j;
    int32_t value;
    uint16_t sign;
    const int16_t *_src = src;
    OVSample       *_dst = dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value = _src[j];
            sign  = value & (1 << 15);
            value = (ov_bdclip(abs(value)) * scale + (1 << (11 - 1))) >> 11;
            value = (ov_clip(sign ? -value : value ,-(1 << 15),1 << 15));
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

static void
scale_sub_residual(const int16_t *src, OVSample *dst,
                      int log2_tb_w, int log2_tb_h,
                      int scale)
{
    int i, j;
    int32_t value;
    uint16_t sign;
    const int16_t *_src = src;
    OVSample       *_dst = dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value = -_src[j];
            sign  = value & (1 << 15);
            value = (ov_bdclip(abs(value)) * scale + (1 << (11 - 1))) >> 11;
            value = (ov_clip(sign ? -value : value ,-(1 << 15),1 << 15));
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

static void
scale_add_half_residual(const int16_t *src, OVSample *dst,
                           int log2_tb_w, int log2_tb_h,
                           int scale)
{
    int i, j;
    int32_t value;
    uint16_t sign;
    const int16_t *_src = src;
    OVSample       *_dst = dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value = _src[j] >> 1;
            sign  = value & (1 << 15);
            value = (ov_bdclip(abs(value)) * scale + (1 << (11 - 1))) >> 11;
            value = (ov_clip(sign ? -value : value ,-(1 << 15),1 << 15));
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

static void
scale_sub_half_residual(const int16_t *src, OVSample *dst,
                           int log2_tb_w, int log2_tb_h,
                           int scale)
{
    int i, j;
    int32_t value;
    uint16_t sign;
    const int16_t *_src = src;
    OVSample       *_dst = dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value = (-_src[j]) >> 1;
            sign  = value & (1 << 15);
            value = (ov_bdclip(abs(value)) * scale + (1 << (11 - 1))) >> 11;
            value = (ov_clip(sign ? -value : value ,-(1 << 15),1 << 15));
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

static void
add_residual(const int16_t *src, OVSample *dst,
                 int log2_tb_w, int log2_tb_h,
                 int scale)
{
    int i, j;
    int32_t value;
    const int16_t *_src = src;
    OVSample       *_dst = dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value   = _src[j];
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

static void
sub_residual(const int16_t *src, OVSample *dst,
                 int log2_tb_w, int log2_tb_h,
                 int scale)
{
    int i, j;
    int32_t value;
    const int16_t *_src = src;
    OVSample       *_dst = dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value   = -_src[j];
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

static void
add_half_residual(const int16_t *src, OVSample *dst,
                      int log2_tb_w, int log2_tb_h,
                      int scale)
{
    int i, j;
    int32_t value;
    const int16_t *_src = src;
    OVSample       *_dst = dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value   = _src[j] >> 1;
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

static void
sub_half_residual(const int16_t *src, OVSample *dst,
                      int log2_tb_w, int log2_tb_h,
                      int scale)
{
    int i, j;
    int32_t value;
    const int16_t *_src = src;
    OVSample       *_dst = (OVSample *)dst;
    const int tb_w = 1 << log2_tb_w;
    const int tb_h = 1 << log2_tb_h;
    for (i = 0; i < tb_h; ++i){
        for (j = 0; j < tb_w; ++j){
            value   = (-_src[j]) >> 1;
            _dst[j] = ov_bdclip((int32_t)_dst[j] + value);
        }
        _dst += RCN_CTB_STRIDE;
        _src += tb_w;
    }
}

/* TYPE :  (sub_flag << 1)| scale_flag */
void
BD_DECL(rcn_init_ict_functions)(struct RCNFunctions *rcn_func, uint8_t type, uint8_t bitdepth)
{
    rcn_func->ict.add[0] = &add_residual;
    rcn_func->ict.add[1] = &add_residual;
    rcn_func->ict.add[2] = &add_residual;
    rcn_func->ict.add[3] = &add_residual;
    rcn_func->ict.add[4] = &add_residual;
    rcn_func->ict.add[5] = &add_residual;
    rcn_func->ict.add[6] = &add_residual;
    switch (type)
    {
        case 3:
            rcn_func->ict.ict[0][0] = &scale_add_residual;
            rcn_func->ict.ict[1][0] = &scale_add_residual;
            rcn_func->ict.ict[2][0] = &scale_add_residual;
            rcn_func->ict.ict[3][0] = &scale_add_residual;
            rcn_func->ict.ict[4][0] = &scale_add_residual;
            rcn_func->ict.ict[5][0] = &scale_add_residual;

            rcn_func->ict.ict[0][1] = &scale_sub_residual;
            rcn_func->ict.ict[1][1] = &scale_sub_residual;
            rcn_func->ict.ict[2][1] = &scale_sub_residual;
            rcn_func->ict.ict[3][1] = &scale_sub_residual;
            rcn_func->ict.ict[4][1] = &scale_sub_residual;
            rcn_func->ict.ict[5][1] = &scale_sub_residual;

            rcn_func->ict.ict[0][2] = &scale_sub_half_residual;
            rcn_func->ict.ict[1][2] = &scale_sub_half_residual;
            rcn_func->ict.ict[2][2] = &scale_sub_half_residual;
            rcn_func->ict.ict[3][2] = &scale_sub_half_residual;
            rcn_func->ict.ict[4][2] = &scale_sub_half_residual;
            rcn_func->ict.ict[5][2] = &scale_sub_half_residual;
            break;
        case 2:
            rcn_func->ict.ict[0][0] = &add_residual;
            rcn_func->ict.ict[1][0] = &add_residual;
            rcn_func->ict.ict[2][0] = &add_residual;
            rcn_func->ict.ict[3][0] = &add_residual;
            rcn_func->ict.ict[4][0] = &add_residual;
            rcn_func->ict.ict[5][0] = &add_residual;

            rcn_func->ict.ict[0][1] = &sub_residual;
            rcn_func->ict.ict[1][1] = &sub_residual;
            rcn_func->ict.ict[2][1] = &sub_residual;
            rcn_func->ict.ict[3][1] = &sub_residual;
            rcn_func->ict.ict[4][1] = &sub_residual;
            rcn_func->ict.ict[5][1] = &sub_residual;

            rcn_func->ict.ict[0][2] = &sub_half_residual;
            rcn_func->ict.ict[1][2] = &sub_half_residual;
            rcn_func->ict.ict[2][2] = &sub_half_residual;
            rcn_func->ict.ict[3][2] = &sub_half_residual;
            rcn_func->ict.ict[4][2] = &sub_half_residual;
            rcn_func->ict.ict[5][2] = &sub_half_residual;
            break;
        case 1:
            rcn_func->ict.ict[0][0] = &scale_add_residual;
            rcn_func->ict.ict[1][0] = &scale_add_residual;
            rcn_func->ict.ict[2][0] = &scale_add_residual;
            rcn_func->ict.ict[3][0] = &scale_add_residual;
            rcn_func->ict.ict[4][0] = &scale_add_residual;
            rcn_func->ict.ict[5][0] = &scale_add_residual;

            rcn_func->ict.ict[0][1] = &scale_add_residual;
            rcn_func->ict.ict[1][1] = &scale_add_residual;
            rcn_func->ict.ict[2][1] = &scale_add_residual;
            rcn_func->ict.ict[3][1] = &scale_add_residual;
            rcn_func->ict.ict[4][1] = &scale_add_residual;
            rcn_func->ict.ict[5][1] = &scale_add_residual;

            rcn_func->ict.ict[0][2] = &scale_add_half_residual;
            rcn_func->ict.ict[1][2] = &scale_add_half_residual;
            rcn_func->ict.ict[2][2] = &scale_add_half_residual;
            rcn_func->ict.ict[3][2] = &scale_add_half_residual;
            rcn_func->ict.ict[4][2] = &scale_add_half_residual;
            rcn_func->ict.ict[5][2] = &scale_add_half_residual;
            break;
        default:
            rcn_func->ict.ict[0][0] = &add_residual;
            rcn_func->ict.ict[1][0] = &add_residual;
            rcn_func->ict.ict[2][0] = &add_residual;
            rcn_func->ict.ict[3][0] = &add_residual;
            rcn_func->ict.ict[4][0] = &add_residual;
            rcn_func->ict.ict[5][0] = &add_residual;

            rcn_func->ict.ict[0][1] = &add_residual;
            rcn_func->ict.ict[1][1] = &add_residual;
            rcn_func->ict.ict[2][1] = &add_residual;
            rcn_func->ict.ict[3][1] = &add_residual;
            rcn_func->ict.ict[4][1] = &add_residual;
            rcn_func->ict.ict[5][1] = &add_residual;

            rcn_func->ict.ict[0][2] = &add_half_residual;
            rcn_func->ict.ict[1][2] = &add_half_residual;
            rcn_func->ict.ict[2][2] = &add_half_residual;
            rcn_func->ict.ict[3][2] = &add_half_residual;
            rcn_func->ict.ict[4][2] = &add_half_residual;
            rcn_func->ict.ict[5][2] = &add_half_residual;
            break;
    }
}
