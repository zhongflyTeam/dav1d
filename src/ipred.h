/*
 * ..
 */

#ifndef __DAV1D_SRC_IPRED_H__
#define __DAV1D_SRC_IPRED_H__

#include "common/bitdepth.h"

#include "src/levels.h"

/*
 * Intra prediction.
 * - a is the angle (in degrees) for directional intra predictors. For other
 *   modes, it is ignored;
 * - topleft is the same as the argument given to dav1d_prepare_intra_edges(),
 *   see ipred_prepare.h for more detailed documentation.
 */
#define decl_angular_ipred_fn(name) \
void (name)(pixel *dst, ptrdiff_t stride, const pixel *topleft, int angle);
typedef decl_angular_ipred_fn(*angular_ipred_fn);

/*
 * Create a subsampled Y plane with the DC subtracted.
 * - w/h_pad is the edge of the width/height that extends outside the visible
 *   portion of the frame in 4px units;
 * - ac has a stride of 16.
 */
#define decl_cfl_ac_fn(name) \
void (name)(int16_t *ac, const pixel *y, ptrdiff_t stride, \
            int w_pad, int h_pad)
typedef decl_cfl_ac_fn(*cfl_ac_fn);

/*
 * dst[plane][x,y] = dc[plane] + alpha[plane] * ac[x,y]
 * - alphas contains two q3 scalars (one for each plane) in [-16,16] range;
 * - dc_pred[] is the first line of each plane's DC prediction, the second plane
 *   starting at an offset of 16 * sizeof(pixel) bytes.
 */
#define decl_cfl_pred_fn(name) \
void (name)(pixel *u_dst, pixel *v_dst, ptrdiff_t stride, \
            const int16_t *ac, const pixel *dc_pred, \
            const int8_t *const alphas, const int height)
typedef decl_cfl_pred_fn(*cfl_pred_fn);

/*
 * dst[x,y] = pal[idx[x,y]]
 * - palette indices are [0-7]
 */
#define decl_pal_pred_fn(name) \
void (name)(pixel *dst, ptrdiff_t stride, const uint16_t *pal, \
            const uint8_t *idx, const int w, const int h)
typedef decl_pal_pred_fn(*pal_pred_fn);

typedef struct Dav1dIntraPredDSPContext {
    angular_ipred_fn intra_pred[N_RECT_TX_SIZES][N_IMPL_INTRA_PRED_MODES];

    // chroma-from-luma
    cfl_ac_fn cfl_ac[3 /* 420, 422, 444 */][N_RECT_TX_SIZES /* chroma tx size */];
    cfl_pred_fn cfl_pred[4];

    // palette
    pal_pred_fn pal_pred;
} Dav1dIntraPredDSPContext;

void dav1d_intra_pred_dsp_init_8bpc(Dav1dIntraPredDSPContext *c);
void dav1d_intra_pred_dsp_init_10bpc(Dav1dIntraPredDSPContext *c);

#endif /* __DAV1D_SRC_IPRED_H__ */
