/*
 * ..
 */

#include <stdlib.h>

#include "common/intops.h"

#include "src/looprestoration.h"
#include "src/tables.h"


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
        const pixel *const above_2 = above_1 + PXSTRIDE(lpf_stride);
        pixel_copy(dst_l, above_1, unit_w);
        pixel_copy(dst_l + PXSTRIDE(dst_stride), above_1, unit_w);
        pixel_copy(dst_l + 2 * PXSTRIDE(dst_stride), above_2, unit_w);
    } else {
        // Pad with first row
        pixel_copy(dst_l, p, unit_w);
        pixel_copy(dst_l + PXSTRIDE(dst_stride), p, unit_w);
        pixel_copy(dst_l + 2 * PXSTRIDE(dst_stride), p, unit_w);
    }

    pixel *dst_tl = dst_l + 3 * PXSTRIDE(dst_stride);
    if (edges & LR_HAVE_BOTTOM) {
        // Copy next loop filtered rows
        const pixel *const below_1 = lpf + 6 * PXSTRIDE(lpf_stride);
        const pixel *const below_2 = below_1 + PXSTRIDE(lpf_stride);
        pixel_copy(dst_tl + stripe_h * PXSTRIDE(dst_stride), below_1, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * PXSTRIDE(dst_stride), below_2, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * PXSTRIDE(dst_stride), below_2, unit_w);
    } else {
        // Pad with last row
        const pixel *const src = p + (stripe_h - 1) * PXSTRIDE(p_stride);
        pixel_copy(dst_tl + stripe_h * PXSTRIDE(dst_stride), src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * PXSTRIDE(dst_stride), src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * PXSTRIDE(dst_stride), src, unit_w);
    }

    // Inner UNIT_WxSTRIPE_H
    for (int j = 0; j < stripe_h; j++) {
        pixel_copy(dst_tl, p, unit_w);
        dst_tl += PXSTRIDE(dst_stride);
        p += PXSTRIDE(p_stride);
    }

    if (!have_right) {
        pixel *pad = dst_l + unit_w;
        pixel *row_last = &dst_l[unit_w - 1];
        // Pad 3x(STRIPE_H+6) with last column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(pad, *row_last, 3);
            pad += PXSTRIDE(dst_stride);
            row_last += PXSTRIDE(dst_stride);
        }
    }

    if (!have_left) {
        // Pad 3x(STRIPE_H+6) with first column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(dst, *dst_l, 3);
            dst += PXSTRIDE(dst_stride);
            dst_l += PXSTRIDE(dst_stride);
        }
    }
}

// FIXME Could split into luma and chroma specific functions,
// (since first and last tops are always 0 for chroma)
// FIXME Could implement a version that requires less temporary memory
// (should be possible to implement with only 6 rows of temp storage)
static void wiener_c(pixel *p, const ptrdiff_t p_stride,
                     const pixel *lpf, const ptrdiff_t lpf_stride,
                     const int w, const int h,
                     const int16_t filterh[7], const int16_t filterv[7],
                     const enum LrEdgeFlags edges)
{
    // padding is 3 pixels above and 3 pixels below
    const ptrdiff_t tmp_stride = sizeof(pixel) * (w + 6);
    pixel tmp[(h + 6) * PXSTRIDE(tmp_stride)];
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
        tmp_ptr += PXSTRIDE(tmp_stride);
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

            p[j * PXSTRIDE(p_stride) + i] =
                iclip_pixel((sum + (1 << (round_bits_v - 1))) >> round_bits_v);
        }
    }
}

static void boxsum3(coef *dst, const ptrdiff_t dst_stride,
                    const pixel *const src, const ptrdiff_t src_stride,
                    const int w, const int h)
{
    for (int i = 1; i < w - 1; i++) {
        // Let a, b, c be the first pixel of the first 3 rows of src, the first
        // sum computed is a + b, this sum is not used, so we don't compute it.
        // The second sum is a + b + c, this sum is needed but does not need to
        // be stored.
        coef *d = dst + PXSTRIDE(dst_stride) + PXSTRIDE(dst_stride) + i;
        const pixel *s = src + PXSTRIDE(src_stride) + PXSTRIDE(src_stride) + i;
        int sum = *d = s[-PXSTRIDE(src_stride)] + s[0] + s[PXSTRIDE(src_stride)];

        for (int j = 3; j < h - 2; j++) {
            d += PXSTRIDE(dst_stride);
            s += PXSTRIDE(src_stride);
            sum = *d = sum + s[PXSTRIDE(src_stride)] - s[-(PXSTRIDE(src_stride) +
                                                           PXSTRIDE(src_stride))];
        }
        // Let y, z be the first pixel of the last 2 rows of src, the last sum
        // is y + z, this sum is not used as such, we don't compute it
    }

    for (int j = 2; j < h; j++) {
        int b = dst[1], c = dst[2];
        // We don't need to store a + b or a + b + c

        for (int i = 2; i < w - 2; i++) {
            const int a = b;
            b = c;
            c = dst[i + 1];
            dst[i] = a + b + c;
        }
        // We don't need to store the last b + c
        dst += PXSTRIDE(dst_stride);
    }
}

static void boxsum5(coef *dst, const ptrdiff_t dst_stride,
                    const pixel *const src, const ptrdiff_t src_stride,
                    const int w, const int h)
{
    for (int i = 0; i < w; i++) {
        // Let a, b, c, d, e be the first pixel of the first 3 rows of src,
        // the first sum computed is a + b + c, this sum is not used as such,
        // we don't compute it. The second sum is a + b + c + d this sum is
        // needed but does not need to be stored.
        coef *d = dst + PXSTRIDE(dst_stride) + PXSTRIDE(dst_stride) + i;
        const pixel *s = src + 3 * PXSTRIDE(src_stride) + i;
        int sum = *d = src[i] + s[-PXSTRIDE(src_stride) - PXSTRIDE(src_stride)] +
                       s[-PXSTRIDE(src_stride)] + s[0] + s[PXSTRIDE(src_stride)];

        for (int j = 3; j < h - 2; j++) {
            d += PXSTRIDE(dst_stride);
            s += PXSTRIDE(src_stride);
            sum = *d = sum + s[PXSTRIDE(src_stride)] -
                       s[-4 * PXSTRIDE(src_stride)];
        }
        // Let w, x, y, z be the first pixel of the last 4 rows of src,
        // the second to last sum is w + x + y + z, and the last sum is
        // x + y + z, these sums are not used as such we don't compute them
    }

    for (int j = 0; j < h; j++) {
        int a = dst[0], b = dst[1], c = dst[2];
        // We don't need to store a + b + c or a + b + c + d
        dst[2] = a + b + c + dst[3] + dst[4];

        for (int i = 3; i < w - 2; i++) {
            const int d = dst[i];
            dst[i] = dst[i - 1] + dst[i + 2] - a;
            a = b;
            b = c;
            c = d;
        }

        // We don't need to store the last b + c + d + e and c + d + e
        dst += PXSTRIDE(dst_stride);
    }
}

static void boxsum3sqr(int32_t *dst, const ptrdiff_t dst_stride,
                       const pixel *const src, const ptrdiff_t src_stride,
                       const int w, const int h)
{
    for (int i = 1; i < w - 1; i++) {
        int32_t *d = dst + PXSTRIDE(dst_stride) + PXSTRIDE(dst_stride) + i;
        const pixel *s = src + PXSTRIDE(src_stride) + PXSTRIDE(src_stride) + i;
        int b = s[-PXSTRIDE(src_stride)] * s[-PXSTRIDE(src_stride)];
        int c = s[0] * s[0];

        for (int j = 2; j < h - 2; j++) {
            s += PXSTRIDE(src_stride);
            const int a = b;
            b = c;
            c = s[0] * s[0];
            *d = a + b + c;
            d += PXSTRIDE(dst_stride);
        }
    }

    for (int j = 2; j < h; j++) {
        int b = dst[1], c = dst[2];

        for (int i = 2; i < w - 2; i++) {
            const int a = b;
            b = c;
            c = dst[i + 1];
            dst[i] = a + b + c;
        }
        dst += PXSTRIDE(dst_stride);
    }
}

static void boxsum5sqr(int32_t *dst, const ptrdiff_t dst_stride,
                       const pixel *const src, const ptrdiff_t src_stride,
                       const int w, const int h)
{
    for (int i = 0; i < w; i++) {
        int32_t *ds = dst + PXSTRIDE(dst_stride) + PXSTRIDE(dst_stride) + i;
        const pixel *s = src + 4 * PXSTRIDE(src_stride) + i;

        int a = src[i] * src[i];
        int b = src[PXSTRIDE(src_stride) + i] * src[PXSTRIDE(src_stride) + i];
        int c = s[-PXSTRIDE(src_stride) - PXSTRIDE(src_stride)] *
                s[-PXSTRIDE(src_stride) - PXSTRIDE(src_stride)];
        int d = s[-PXSTRIDE(src_stride)] * s[-PXSTRIDE(src_stride)];
        int e = s[0] * s[0];

        int sum = *ds = a + b + c + d + e;

        for (int j = 3; j < h - 2; j++) {
            ds += PXSTRIDE(dst_stride);
            s += PXSTRIDE(src_stride);
            const int f = s[0] * s[0];
            sum = *ds = sum + f - a;
            a = b;
            b = c;
            c = d;
            d = e;
            e = f;
        }
    }

    for (int j = 0; j < h; j++) {
        int a = dst[0], b = dst[1], c = dst[2];
        dst[2] = a + b + c + dst[3] + dst[4];

        for (int i = 3; i < w - 2; i++) {
            const int d = dst[i];
            dst[i] = dst[i - 1] + dst[i + 2] - a;
            a = b;
            b = c;
            c = d;
        }
        dst += PXSTRIDE(dst_stride);
    }
}

static void selfguided_filter(int32_t *dst, const ptrdiff_t dst_stride,
                              const pixel *src, const ptrdiff_t src_stride,
                              const int w, const int h, const int n, const int s)
{
    const int tmp_stride = sizeof(pixel) * (w + 6);
    // FIXME Replace array with scratch memory
    int32_t A_[(h + 6) * PXSTRIDE(tmp_stride)];
    int32_t *A = A_ + 3 * PXSTRIDE(tmp_stride) + 3;
    // By inverting A and B after the boxsums, B can be of size coef instead
    // of int32_t
    coef B_[(h + 6) * PXSTRIDE(tmp_stride)];
    coef *B = B_ + 3 * PXSTRIDE(tmp_stride) + 3;

    const int step = (n == 25) + 1;
    if (n == 25) {
        boxsum5(B_, tmp_stride, src, src_stride, w + 6, h + 6);
        boxsum5sqr(A_, tmp_stride, src, src_stride, w + 6, h + 6);
    } else {
        boxsum3(B_, tmp_stride, src, src_stride, w + 6, h + 6);
        boxsum3sqr(A_, tmp_stride, src, src_stride, w + 6, h + 6);
    }

    int32_t *AA = A - PXSTRIDE(tmp_stride);
    coef *BB = B - PXSTRIDE(tmp_stride);
    for (int j = -1; j < h + 1; j+= step) {
        for (int i = -1; i < w + 1; i++) {
            const uint32_t a =
                (AA[i] + (1 << (2 * (BITDEPTH - 8)) >> 1)) >> (2 * (BITDEPTH - 8));
            const uint32_t b =
                (BB[i] + (1 << (BITDEPTH - 8) >> 1)) >> (BITDEPTH - 8);

            const uint32_t p = (a * n >= b * b) * (a * n - b * b);
            const uint32_t z = (p * s + (1 << 19)) >> 20;

            const int x = sgr_x_by_xplus1[imin(z, 255)];
            // This is where we invert A and B, so that B is of size coef.
            AA[i] = (((1 << 8) - x) * BB[i] * sgr_one_by_x[n - 1] + (1 << 11)) >> 12;
            BB[i] = x;
        }
        AA += step * PXSTRIDE(tmp_stride);
        BB += step * PXSTRIDE(tmp_stride);
    }

    src += 3 * PXSTRIDE(src_stride) + 3;
    if (n == 25) {
        for (int j = 0; j < h; j+=2) {
            for (int i = 0; i < w; i++) {
                const int32_t a = (B[i - PXSTRIDE(tmp_stride)] +
                                   B[i + PXSTRIDE(tmp_stride)]) * 6 +
                                  (B[i - 1 - PXSTRIDE(tmp_stride)] +
                                   B[i - 1 + PXSTRIDE(tmp_stride)] +
                                   B[i + 1 - PXSTRIDE(tmp_stride)] +
                                   B[i + 1 + PXSTRIDE(tmp_stride)]) * 5;
                const int32_t b = (A[i - PXSTRIDE(tmp_stride)] +
                                   A[i + PXSTRIDE(tmp_stride)]) * 6 +
                                  (A[i - 1 - PXSTRIDE(tmp_stride)] +
                                   A[i - 1 + PXSTRIDE(tmp_stride)] +
                                   A[i + 1 - PXSTRIDE(tmp_stride)] +
                                   A[i + 1 + PXSTRIDE(tmp_stride)]) * 5;
                dst[i] = (a * src[i] + b + (1 << 8)) >> 9;
            }
            dst += PXSTRIDE(dst_stride);
            src += PXSTRIDE(src_stride);
            B += PXSTRIDE(tmp_stride);
            A += PXSTRIDE(tmp_stride);
            for (int i = 0; i < w; i++) {
                const int32_t a = B[i] * 6 + (B[i - 1] + B[i + 1]) * 5;
                const int32_t b = A[i] * 6 + (A[i - 1] + A[i + 1]) * 5;
                dst[i] = (a * src[i] + b + (1 << 7)) >> 8;
            }
            dst += PXSTRIDE(dst_stride);
            src += PXSTRIDE(src_stride);
            B += PXSTRIDE(tmp_stride);
            A += PXSTRIDE(tmp_stride);
        }
    } else {
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                const int32_t a =
                    (B[i] + B[i - 1] + B[i + 1] +
                     B[i - PXSTRIDE(tmp_stride)] +
                     B[i + PXSTRIDE(tmp_stride)]) * 4 +
                    (B[i - 1 - PXSTRIDE(tmp_stride)] +
                     B[i - 1 + PXSTRIDE(tmp_stride)] +
                     B[i + 1 - PXSTRIDE(tmp_stride)] +
                     B[i + 1 + PXSTRIDE(tmp_stride)]) * 3;

                const int32_t b =
                    (A[i] + A[i - 1] + A[i + 1] +
                     A[i - PXSTRIDE(tmp_stride)] +
                     A[i + PXSTRIDE(tmp_stride)]) * 4 +
                    (A[i - 1 - PXSTRIDE(tmp_stride)] +
                     A[i - 1 + PXSTRIDE(tmp_stride)] +
                     A[i + 1 - PXSTRIDE(tmp_stride)] +
                     A[i + 1 + PXSTRIDE(tmp_stride)]) * 3;

                dst[i] = (a * src[i] + b + (1 << 8)) >> 9;
            }
            dst += PXSTRIDE(dst_stride);
            src += PXSTRIDE(src_stride);
            B += PXSTRIDE(tmp_stride);
            A += PXSTRIDE(tmp_stride);
        }
    }
}

static void selfguided_c(pixel *p, const ptrdiff_t p_stride,
                         const pixel *lpf, const ptrdiff_t lpf_stride,
                         const int w, const int h, const int sgr_idx,
                         const int16_t sgr_w[2], const enum LrEdgeFlags edges)
{
    // padding is 3 pixels above and 3 pixels below
    const int tmp_stride = sizeof(pixel) * (w + 6);
    pixel tmp[(h + 6) * PXSTRIDE(tmp_stride)];

    padding(tmp, tmp_stride, p, p_stride, lpf, lpf_stride, w, h, edges);

    // both r1 and r0 can't be zero
    if (!sgr_params[sgr_idx][0]) {
        int32_t dst[h * w];
        const int s1 = sgr_params[sgr_idx][3];
        selfguided_filter(dst, w, tmp, tmp_stride, w, h, 9, s1);
        const int w1 = (1 << 7) - sgr_w[1];
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                const int32_t u = (p[i] << 4);
                const int32_t v = (u << 7) + w1 * (dst[j * w + i] - u);
                p[i] = iclip_pixel((v + (1 << 10)) >> 11);
            }
            p += PXSTRIDE(p_stride);
        }
    } else if (!sgr_params[sgr_idx][1]) {
        int32_t dst[h * w];
        const int s0 = sgr_params[sgr_idx][2];
        selfguided_filter(dst, w, tmp, tmp_stride, w, h, 25, s0);
        const int w0 = sgr_w[0];
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                const int32_t u = (p[i] << 4);
                const int32_t v = (u << 7) + w0 * (dst[j * w + i] - u);
                p[i] = iclip_pixel((v + (1 << 10)) >> 11);
            }
            p += PXSTRIDE(p_stride);
        }
    } else {
        int32_t dst0[h * w];
        int32_t dst1[h * w];
        const int s0 = sgr_params[sgr_idx][2];
        const int s1 = sgr_params[sgr_idx][3];
        const int w0 = sgr_w[0];
        const int w1 = (1 << 7) - w0 - sgr_w[1];
        selfguided_filter(dst0, w, tmp, tmp_stride, w, h, 25, s0);
        selfguided_filter(dst1, w, tmp, tmp_stride, w, h, 9, s1);
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                const int32_t u = (p[i] << 4);
                const int32_t v = (u << 7) + w0 * (dst0[j * w + i] - u) +
                                  w1 * (dst1[j * w + i] - u);
                p[i] = iclip_pixel((v + (1 << 10)) >> 11);
            }
            p += PXSTRIDE(p_stride);
        }
    }
}

void bitfn(dav1d_loop_restoration_dsp_init)(Dav1dLoopRestorationDSPContext *const c) {
    c->wiener = wiener_c;
    c->selfguided = selfguided_c;
}
