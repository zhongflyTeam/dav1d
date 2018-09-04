/*
 * ..
 */

#ifndef __DAV1D_SRC_WARPMV_H__
#define __DAV1D_SRC_WARPMV_H__

#include "src/levels.h"

int get_shear_params(WarpedMotionParams *wm);
int find_affine_int(const int (*pts)[2][2], int np, int bw4, int bh4,
                    mv mv, WarpedMotionParams *wm, int by, int bx);

#endif /* __DAV1D_SRC_WARPMV_H__ */
