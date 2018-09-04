/*
 * ..
 */

#ifndef __DAV1D_SRC_INTERNAL_H__
#define __DAV1D_SRC_INTERNAL_H__

#include "include/dav1d.h"

typedef struct Dav1dFrameContext Dav1dFrameContext;
typedef struct Dav1dTileContext Dav1dTileContext;

#include "src/cdef.h"
#include "src/cdf.h"
#include "src/env.h"
#include "src/intra_edge.h"
#include "src/ipred.h"
#include "src/itx.h"
#include "src/levels.h"
#include "src/lf_mask.h"
#include "src/loopfilter.h"
#include "src/looprestoration.h"
#include "src/mc.h"
#include "src/msac.h"
#include "src/picture.h"
#include "src/recon.h"
#include "src/ref_mvs.h"

typedef struct Dav1dDSPContext {
    Dav1dIntraPredDSPContext ipred;
    Dav1dMCDSPContext mc;
    Dav1dInvTxfmDSPContext itx;
    Dav1dLoopFilterDSPContext lf;
    Dav1dCdefDSPContext cdef;
    Dav1dLoopRestorationDSPContext lr;
} Dav1dDSPContext;

struct Dav1dContext {
    Dav1dFrameContext *fc;
    int n_fc;

    // cache of OBUs that make up a single frame before we submit them
    // to a frame worker to be decoded
    struct {
        Dav1dData data;
        int start, end;
    } tile[256];
    int n_tile_data, have_seq_hdr, have_frame_hdr;
    unsigned tile_mask;
    Av1SequenceHeader seq_hdr; // FIXME make ref?
    Av1FrameHeader frame_hdr; // FIXME make ref?

    // decoded output picture queue
    Dav1dPicture out;

    // reference/entropy state
    struct {
        Dav1dThreadPicture p;
        Dav1dRef *segmap;
        Av1SegmentationDataSet seg_data;
        Dav1dRef *refmvs;
        unsigned refpoc[7];
        WarpedMotionParams gmv[7];
        Av1LoopfilterModeRefDeltas lf_mode_ref_deltas;
        CdfContext cdf;
    } refs[8];

    Dav1dDSPContext dsp[3 /* 8, 10, 12 bits/component */];

    // tree to keep track of which edges are available
    struct {
        EdgeNode *root[2 /* BL_128X128 vs. BL_64X64 */];
        EdgeBranch branch_sb128[1 + 4 + 16 + 64];
        EdgeBranch branch_sb64[1 + 4 + 16];
        EdgeTip tip_sb128[256];
        EdgeTip tip_sb64[64];
    } intra_edge;
};

struct Dav1dFrameContext {
    Av1SequenceHeader seq_hdr;
    Av1FrameHeader frame_hdr;
    Dav1dThreadPicture refp[7], cur;
    Dav1dRef *mvs_ref;
    refmvs *mvs;
    Dav1dRef *cur_segmap_ref, *prev_segmap_ref;
    uint8_t *cur_segmap;
    const uint8_t *prev_segmap;
    unsigned refpoc[7];
    CdfContext cdf;
    struct {
        Dav1dData data;
        int start, end;
    } tile[256];
    int n_tile_data;

    const Dav1dContext *c;
    Dav1dTileContext *tc;
    int n_tc;
    const Dav1dDSPContext *dsp;
    struct {
        recon_b_intra_fn recon_b_intra;
        recon_b_inter_fn recon_b_inter;
        filter_frame_fn filter_frame;
    } bd_fn;

    ptrdiff_t b4_stride;
    int bw, bh, sb128w, sb128h;
    uint16_t dq[NUM_SEGMENTS][3 /* plane */][2 /* dc/ac */];
    const uint8_t *qm[2 /* is_1d */][N_RECT_TX_SIZES][3 /* plane */];
    BlockContext *a;
    int a_sz;
    AV1_COMMON *libaom_cm; // FIXME
    uint8_t jnt_weights[7][7];

    // loopfilter
    struct {
        uint8_t (*level)[4];
        Av1Filter *mask;
        int mask_sz;
        Av1FilterLUT lim_lut;
        int last_sharpness;
        uint8_t lvl[8 /* seg_id */][4 /* dir */][8 /* ref */][2 /* is_gmv */];
        uint8_t *tx_lpf_right_edge[2];
        int re_sz;
        pixel *cdef_line;
        pixel *cdef_line_ptr[2 /* pre, post */][3 /* plane */][2 /* y */];
        pixel *lr_lpf_line;
        pixel *lr_lpf_line_ptr[3 /* plane */];
    } lf;
};

struct Dav1dTileContext {
    const Dav1dFrameContext *f;
    int bx, by;
    struct {
        int col_start, col_end, row_start, row_end;
    } tiling;
    BlockContext l, *a;
    CdfContext cdf;
    MsacContext msac;
    coef *cf;
    pixel *emu_edge; // stride=160
    // FIXME types can be changed to pixel (and dynamically allocated)
    // which would make copy/assign operations slightly faster?
    uint16_t al_pal[2 /* a/l */][32 /* bx/y4 */][3 /* plane */][8 /* palette_idx */];
    uint16_t pal[3 /* plane */][8 /* palette_idx */];
    uint8_t pal_sz_uv[2 /* a/l */][32 /* bx4/by4 */];
    uint8_t txtp_map[32 * 32]; // inter-only
    uint16_t dqmem[NUM_SEGMENTS][3 /* plane */][2 /* dc/ac */];
    const uint16_t (*dq)[3][2];
    int last_qidx;
    int8_t last_delta_lf[4];
    uint8_t lflvlmem[8 /* seg_id */][4 /* dir */][8 /* ref */][2 /* is_gmv */];
    const uint8_t (*lflvl)[4][8][2];

    Av1Filter *lf_mask;
    int8_t *cur_sb_cdef_idx_ptr;
    // for chroma sub8x8, we need to know the filter for all 4 subblocks in
    // a 4x4 area, but the top/left one can go out of cache already, so this
    // keeps it accessible
    enum Filter2d tl_4x4_filter;
};

#endif /* __DAV1D_SRC_INTERNAL_H__ */
