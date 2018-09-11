/*
 * ..
 */

#ifndef __DAV1D_SRC_LOOPRESTORATION_H__
#define __DAV1D_SRC_LOOPRESTORATION_H__

#include <stdint.h>
#include <stddef.h>

#include "common/bitdepth.h"

enum LrEdgeFlags {
    LR_HAVE_LEFT = 1 << 0,
    LR_HAVE_RIGHT = 1 << 1,
    LR_HAVE_TOP = 1 << 2,
    LR_HAVE_BOTTOM = 1 << 3,
};

// Although the spec applies restoration filters over 4x4 blocks, the wiener
// filter can be applied to a bigger surface.
//    * w is constrained by the restoration unit size (w <= 256)
//    * h is constrained by the stripe height (h <= 64)
typedef void (*wienerfilter_fn)(pixel *dst, ptrdiff_t dst_stride,
                                const pixel *lpf, ptrdiff_t lpf_stride,
                                int w, int h, const int16_t filterh[7],
                                const int16_t filterv[7], enum LrEdgeFlags edges);

typedef void (*selfguided_fn)(pixel *dst, ptrdiff_t dst_stride,
                              const pixel *lpf, ptrdiff_t lpf_stride,
                              int w, int h, int sgr_idx, const int16_t sgr_w[2],
                              const enum LrEdgeFlags edges);

typedef struct Dav1dLoopRestorationDSPContext {
    wienerfilter_fn wiener;
    selfguided_fn selfguided;
} Dav1dLoopRestorationDSPContext;

void dav1d_loop_restoration_dsp_init_8bpc(Dav1dLoopRestorationDSPContext *c);
void dav1d_loop_restoration_dsp_init_10bpc(Dav1dLoopRestorationDSPContext *c);

#endif /* __DAV1D_SRC_LOOPRESTORATION_H__ */
