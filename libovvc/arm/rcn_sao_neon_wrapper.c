#include <stddef.h>
#include <stdint.h>

#include "rcn_neon.h"


#include "dec_structures.h"
#include "rcn_structures.h"




static void sao_band_filter_0_10_neon(OVSample* _dst,
                         OVSample* _src,
                         ptrdiff_t _stride_dst,
                         ptrdiff_t _stride_src,
                         struct SAOParamsCtu* sao,
                         int width,
                         int height,
                         int c_idx){

                         }

static void sao_edge_filter_10_neon(OVSample* _dst,
                       OVSample* _src,
                       ptrdiff_t _stride_dst,
                       ptrdiff_t _stride_src,
                       SAOParamsCtu* sao,
                       int width,
                       int height,
                       int c_idx){


                       }



static void sao_edge_filter_7_10_neon(OVSample* _dst,
                         OVSample* _src,
                         ptrdiff_t _stride_dst,
                         ptrdiff_t _stride_src,
                         SAOParamsCtu* sao,
                         int width,
                         int height,
                         int c_idx){
                            
                         }









void rcn_init_sao_functions_neon(struct RCNFunctions *const rcn_funcs){
    rcn_funcs->sao.band= &sao_band_filter_0_10_neon;
    rcn_funcs->sao.edge[0]= &sao_edge_filter_7_10_neon;
    rcn_funcs->sao.edge[1]= &sao_edge_filter_10_neon;
}
