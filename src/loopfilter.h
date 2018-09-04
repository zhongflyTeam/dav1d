/*
 * ..
 */

#ifndef __DAV1D_SRC_LOOPFILTER_H__
#define __DAV1D_SRC_LOOPFILTER_H__

#include <stdint.h>
#include <stddef.h>

#include "common/bitdepth.h"

#include "src/levels.h"

#define decl_loopfilter_fn(name) \
void (name)(pixel *dst, ptrdiff_t stride, int mb_lim, int lim, int hev_thr);
typedef decl_loopfilter_fn(*loopfilter_fn);

typedef struct Dav1dLoopFilterDSPContext {
    /*
     * dimension 1: filter taps (0=4, 1=8, 2=16 for luma; 0=4, 1=6 for chroma)
     * dimension 2: 0=col-edge filter (h), 1=row-edge filter (v)
     *
     * dst/stride are aligned by 4
     */
    loopfilter_fn loop_filter[3][2];
    loopfilter_fn loop_filter_uv[2][2];
} Dav1dLoopFilterDSPContext;

void dav1d_loop_filter_dsp_init_8bpc(Dav1dLoopFilterDSPContext *c);
void dav1d_loop_filter_dsp_init_10bpc(Dav1dLoopFilterDSPContext *c);

#endif /* __DAV1D_SRC_LOOPFILTER_H__ */
