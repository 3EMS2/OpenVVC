#define BITDEPTH 8
#define ov_bdclip(val) ov_clip_uintp2(val, BITDEPTH);

#include "rcn_transform_scale.c"

#undef BITDEPTH