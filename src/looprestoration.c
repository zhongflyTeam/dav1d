/*
 * ..
 */

#include <stdlib.h>

#include "common/intops.h"

#include "src/looprestoration.h"


// TODO Reuse p when no padding is needed (add and remove lpf pixels in p)
// TODO Chroma only requires 2 rows of padding.
static void padding(pixel *dst, const ptrdiff_t dst_stride,
                    const pixel *p, const ptrdiff_t p_stride,
                    const pixel *lpf, const ptrdiff_t lpf_stride,
                    int unit_w, const int stripe_h, const enum LrEdgeFlags edges)
{
    const int have_left = !!(edges & LR_HAVE_LEFT);
    const int have_right = !!(edges & LR_HAVE_RIGHT);

    // Copy more pixels if we don't have to pad them
    unit_w += 3 * have_left + 3 * have_right;
    pixel *dst_l = dst + 3 * !have_left;
    p -= 3 * have_left;
    lpf -= 3 * have_left;

    if (edges & LR_HAVE_TOP) {
        // Copy previous loop filtered rows
        const pixel *const above_1 = lpf;
        const pixel *const above_2 = above_1 + lpf_stride;
        pixel_copy(dst_l, above_1, unit_w);
        pixel_copy(dst_l + dst_stride, above_1, unit_w);
        pixel_copy(dst_l + 2 * dst_stride, above_2, unit_w);
    } else {
        // Pad with first row
        pixel_copy(dst_l, p, unit_w);
        pixel_copy(dst_l + dst_stride, p, unit_w);
        pixel_copy(dst_l + 2 * dst_stride, p, unit_w);
    }

    pixel *dst_tl = dst_l + 3 * dst_stride;
    if (edges & LR_HAVE_BOTTOM) {
        // Copy next loop filtered rows
        const pixel *const below_1 = lpf + 6 * lpf_stride;
        const pixel *const below_2 = below_1 + lpf_stride;
        pixel_copy(dst_tl + stripe_h * dst_stride, below_1, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * dst_stride, below_2, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * dst_stride, below_2, unit_w);
    } else {
        // Pad with last row
        const pixel *const src = p + (stripe_h - 1) * p_stride;
        pixel_copy(dst_tl + stripe_h * dst_stride, src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * dst_stride, src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * dst_stride, src, unit_w);
    }

    // Inner UNIT_WxSTRIPE_H
    for (int j = 0; j < stripe_h; j++) {
        pixel_copy(dst_tl, p, unit_w);
        dst_tl += dst_stride;
        p += p_stride;
    }

    if (!have_right) {
        pixel *pad = dst_l + unit_w;
        pixel *row_last = &dst_l[unit_w - 1];
        // Pad 3x(STRIPE_H+6) with last column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(pad, *row_last, 3);
            pad += dst_stride;
            row_last += dst_stride;
        }
    }

    if (!have_left) {
        // Pad 3x(STRIPE_H+6) with first column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(dst, *dst_l, 3);
            dst += dst_stride;
            dst_l += dst_stride;
        }
    }
}

// FIXME Could split into luma and chroma specific functions,
// (since first and last tops are always 0 for chroma)
// FIXME Could implement a version that requires less temporary memory
// (should be possible to implement with only 6 rows of temp storage)
static void wiener_filter_c(pixel *p, const ptrdiff_t p_stride,
                            const pixel *lpf, const ptrdiff_t lpf_stride,
                            const int w, const int h,
                            const int16_t filterh[7], const int16_t filterv[7],
                            const enum LrEdgeFlags edges)
{
    // padding is 3 pixels above and 3 pixels below
    const int tmp_stride = w + 6;
    pixel tmp[(h + 6) * tmp_stride];
    pixel *tmp_ptr = tmp;

    padding(tmp, tmp_stride, p, p_stride, lpf, lpf_stride, w, h, edges);

    // Values stored between horizontal and vertical filtering don't
    // fit in a uint8_t.
    uint16_t hor[(h + 6 /*padding*/) * w];
    uint16_t *hor_ptr = hor;

    const int round_bits_h = 3 + (BITDEPTH == 12) * 2;
    for (int j = 0; j < h + 6; j++) {
        for (int i = 0; i < w; i++) {
            int sum = (tmp_ptr[i + 3] << 7) + (1 << (BITDEPTH + 6));

            for (int k = 0; k < 7; k++) {
                sum += tmp_ptr[i + k] * filterh[k];
            }

            hor_ptr[i] = iclip((sum + (1 << (round_bits_h - 1))) >> round_bits_h, 0,
                               1 << ((BITDEPTH) + 1 + 7 - round_bits_h));
        }
        tmp_ptr += tmp_stride;
        hor_ptr += w;
    }

    const int round_bits_v = 11 - (BITDEPTH == 12) * 2;
    const int round_offset = 1 << (BITDEPTH + (round_bits_v - 1));
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            int sum = (hor[w * (j + 3) + i] << 7) - round_offset;

            for (int k = 0; k < 7; k++) {
                sum += hor[(j + k) * w + i] * filterv[k];
            }

            p[j * p_stride + i] =
                iclip_pixel((sum + (1 << (round_bits_v - 1))) >> round_bits_v);
        }
    }
}

void bitfn(dav1d_loop_restoration_dsp_init)(Dav1dLoopRestorationDSPContext *const c) {
    c->wiener_filter = wiener_filter_c;
}
