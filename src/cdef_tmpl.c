/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdlib.h>

#include "common/intops.h"

#include "src/cdef.h"
#include "src/tables.h"

static ptrdiff_t get_buffer_stride_y(void) {
    return 8 * CDEF_BUFFER_UNITS + 4;
}

static ptrdiff_t get_buffer_stride_uv(int ss_hor) {
    // Each stride of buffer holds a rows from each source.
    return 2 * ((8 >> ss_hor) * CDEF_BUFFER_UNITS + 4);
}

static void cdef_prep_c(pixel *buf, const ptrdiff_t buf_stride,
                        const pixel *src, ptrdiff_t src_stride,
                        const pixel *top, const pixel *bottom,
                        int num_units, enum CdefEdgeFlags edges,
                        int ss_hor, int ss_ver) {
    assert(num_units <= CDEF_BUFFER_UNITS);
    assert(num_units == CDEF_BUFFER_UNITS || !(edges & CDEF_HAVE_RIGHT));
    src_stride = PXSTRIDE(src_stride);
    int x_end = (num_units * 8 >> ss_hor) + ((edges & CDEF_HAVE_RIGHT) ? 2 : 0);
    int h = 8 >> ss_ver;
    if (edges & CDEF_HAVE_LEFT) {
        if (edges & CDEF_LEFT_SKIP) {
            if (edges & CDEF_HAVE_TOP)
                for (int y = -2; y < 0; y++)
                    pixel_copy(buf + y * buf_stride - 2,
                               top + (y + 2) * src_stride - 2, 2);
            for (int y = 0; y < h; y++)
                pixel_copy(buf + y * buf_stride - 2,
                           src + y * src_stride - 2, 2);
            if (edges & CDEF_HAVE_BOTTOM)
                for (int y = h; y < h + 2; y++)
                    pixel_copy(buf + y * buf_stride - 2,
                               bottom + (y - h) * src_stride - 2, 2);
        } else {
            // Copy from the existing buf.
            pixel *buffer_end = buf + (CDEF_BUFFER_UNITS * 8 >> ss_hor) - 2;
            int start = (edges & CDEF_HAVE_TOP) ? -2 : 0;
            int end = h + ((edges & CDEF_HAVE_BOTTOM) ? 2 : 0);
            for (int y = start; y < end; y++)
                pixel_copy(buf + y * buf_stride - 2,
                           buffer_end + y * buf_stride, 2);
        }
    }
    if (edges & CDEF_HAVE_TOP)
        for (int y = -2; y < 0; y++)
            pixel_copy(buf + y * buf_stride,
                       top + (y + 2) * src_stride, x_end);
    for (int y = 0; y < h; y++)
        pixel_copy(buf + y * buf_stride, src + y * src_stride, x_end);
    if (edges & CDEF_HAVE_BOTTOM)
        for (int y = h; y < h + 2; y++)
            pixel_copy(buf + y * buf_stride,
                       bottom + (y - h) * src_stride, x_end);
}

static void cdef_prep_y_c(pixel *const buf, const pixel *const src,
                          const ptrdiff_t src_stride,
                          const pixel *const top, const pixel *const bottom,
                          int num_units, const enum CdefEdgeFlags edges)
{
    const ptrdiff_t buf_stride = get_buffer_stride_y();
    cdef_prep_c(buf + buf_stride * 2 + 2, buf_stride, src, src_stride,
                top, bottom, num_units, edges, 0, 0);
}

static void cdef_prep_uv_c(pixel *buf, pixel **const src, ptrdiff_t src_stride,
                           pixel **const top, pixel **const bottom,
                           int num_units, enum CdefEdgeFlags edges,
                           int ss_hor, int ss_ver) {
    const ptrdiff_t buf_stride = get_buffer_stride_uv(ss_hor);
    buf += buf_stride * 2 + 2;
    for (int pl = 1; pl <= 2; pl++) {
        cdef_prep_c(buf, buf_stride, src[pl - 1], src_stride,
                    top[pl - 1], bottom[pl - 1], num_units, edges,
                    ss_hor, ss_ver);
        // Jump to the middle of the stride to advance to the second plane.
        buf += buf_stride >> 1;
    }
}

#define cdef_prep_uv_fn(layout, ss_hor, ss_ver) \
static void cdef_prep_uv_##layout##_c(pixel *buf, pixel **const src, \
                                      const ptrdiff_t src_stride, \
                                      pixel **const top, \
                                      pixel **const bottom, \
                                      int num_units, \
                                      const enum CdefEdgeFlags edges) \
{ \
    cdef_prep_uv_c(buf, src, src_stride, top, bottom, num_units, edges, \
                   ss_hor, ss_ver); \
}

cdef_prep_uv_fn(444, 0, 0);
cdef_prep_uv_fn(422, 1, 0);
cdef_prep_uv_fn(420, 1, 1);

static inline int constrain(const int diff, const int threshold,
                            const int shift)
{
    const int adiff = abs(diff);
    return apply_sign(imin(adiff, imax(0, threshold - (adiff >> shift))), diff);
}

static inline void fill(int16_t *tmp, const ptrdiff_t stride,
                        const int w, const int h)
{
    /* Use a value that's a large positive number when interpreted as unsigned,
     * and a large negative number when interpreted as signed. */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++)
            tmp[x] = INT16_MIN;
        tmp += stride;
    }
}

static void padding(int16_t *tmp, const ptrdiff_t tmp_stride,
                    const pixel *src, const ptrdiff_t src_stride,
                    const int w, const int h, const enum CdefEdgeFlags edges) {
    // fill extended input buffer
    int x_start = -2, x_end = w + 2, y_start = -2, y_end = h + 2;
    if (!(edges & CDEF_HAVE_TOP)) {
        fill(tmp - 2 - 2 * tmp_stride, tmp_stride, w + 4, 2);
        y_start = 0;
    }
    if (!(edges & CDEF_HAVE_BOTTOM)) {
        fill(tmp + h * tmp_stride - 2, tmp_stride, w + 4, 2);
        y_end -= 2;
    }
    if (!(edges & CDEF_HAVE_LEFT)) {
        fill(tmp + y_start * tmp_stride - 2, tmp_stride, 2, y_end - y_start);
        x_start = 0;
    }
    if (!(edges & CDEF_HAVE_RIGHT)) {
        fill(tmp + y_start * tmp_stride + w, tmp_stride, 2, y_end - y_start);
        x_end -= 2;
    }

    for (int y = y_start; y < y_end; y++)
        for (int x = x_start; x < x_end; x++)
            tmp[x + y * tmp_stride] = src[x + y * src_stride];
}

static NOINLINE void
cdef_filter_block_c(pixel *dst, const ptrdiff_t dst_stride,
                    const pixel *const src, const ptrdiff_t src_stride,
                    const int pri_strength, const int sec_strength,
                    const int dir, const int damping, const int w, int h,
                    const enum CdefEdgeFlags edges HIGHBD_DECL_SUFFIX) {
    const ptrdiff_t tmp_stride = 12;
    assert((w == 4 || w == 8) && (h == 4 || h == 8));
    int16_t tmp_buf[144]; // 12*12 is the maximum value of tmp_stride * (h + 4)
    int16_t *tmp = tmp_buf + 2 * tmp_stride + 2;

    padding(tmp, tmp_stride, src, src_stride, w, h, edges);

    if (pri_strength) {
        const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
        const int pri_tap = 4 - ((pri_strength >> bitdepth_min_8) & 1);
        const int pri_shift = imax(0, damping - ulog2(pri_strength));
        if (sec_strength) {
            const int sec_shift = damping - ulog2(sec_strength);
            do {
                for (int x = 0; x < w; x++) {
                    const int px = tmp[x];
                    int sum = 0;
                    int max = px, min = px;
                    int pri_tap_k = pri_tap;
                    for (int k = 0; k < 2; k++) {
                        const int off1 = dav1d_cdef_directions[dir + 2][k]; // dir
                        const int p0 = tmp[x + off1];
                        const int p1 = tmp[x - off1];
                        sum += pri_tap_k * constrain(p0 - px, pri_strength, pri_shift);
                        sum += pri_tap_k * constrain(p1 - px, pri_strength, pri_shift);
                        // if pri_tap_k == 4 then it becomes 2 else it remains 3
                        pri_tap_k = (pri_tap_k & 3) | 2;
                        min = umin(p0, min);
                        max = imax(p0, max);
                        min = umin(p1, min);
                        max = imax(p1, max);
                        const int off2 = dav1d_cdef_directions[dir + 4][k]; // dir + 2
                        const int off3 = dav1d_cdef_directions[dir + 0][k]; // dir - 2
                        const int s0 = tmp[x + off2];
                        const int s1 = tmp[x - off2];
                        const int s2 = tmp[x + off3];
                        const int s3 = tmp[x - off3];
                        // sec_tap starts at 2 and becomes 1
                        const int sec_tap = 2 - k;
                        sum += sec_tap * constrain(s0 - px, sec_strength, sec_shift);
                        sum += sec_tap * constrain(s1 - px, sec_strength, sec_shift);
                        sum += sec_tap * constrain(s2 - px, sec_strength, sec_shift);
                        sum += sec_tap * constrain(s3 - px, sec_strength, sec_shift);
                        min = umin(s0, min);
                        max = imax(s0, max);
                        min = umin(s1, min);
                        max = imax(s1, max);
                        min = umin(s2, min);
                        max = imax(s2, max);
                        min = umin(s3, min);
                        max = imax(s3, max);
                    }
                    dst[x] = iclip(px + ((sum - (sum < 0) + 8) >> 4), min, max);
                }
                dst += PXSTRIDE(dst_stride);
                tmp += tmp_stride;
            } while (--h);
        } else { // pri_strength only
            do {
                for (int x = 0; x < w; x++) {
                    const int px = tmp[x];
                    int sum = 0;
                    int pri_tap_k = pri_tap;
                    for (int k = 0; k < 2; k++) {
                        const int off = dav1d_cdef_directions[dir + 2][k]; // dir
                        const int p0 = tmp[x + off];
                        const int p1 = tmp[x - off];
                        sum += pri_tap_k * constrain(p0 - px, pri_strength, pri_shift);
                        sum += pri_tap_k * constrain(p1 - px, pri_strength, pri_shift);
                        pri_tap_k = (pri_tap_k & 3) | 2;
                    }
                    dst[x] = px + ((sum - (sum < 0) + 8) >> 4);
                }
                dst += PXSTRIDE(dst_stride);
                tmp += tmp_stride;
            } while (--h);
        }
    } else { // sec_strength only
        assert(sec_strength);
        const int sec_shift = damping - ulog2(sec_strength);
        do {
            for (int x = 0; x < w; x++) {
                const int px = tmp[x];
                int sum = 0;
                for (int k = 0; k < 2; k++) {
                    const int off1 = dav1d_cdef_directions[dir + 4][k]; // dir + 2
                    const int off2 = dav1d_cdef_directions[dir + 0][k]; // dir - 2
                    const int s0 = tmp[x + off1];
                    const int s1 = tmp[x - off1];
                    const int s2 = tmp[x + off2];
                    const int s3 = tmp[x - off2];
                    const int sec_tap = 2 - k;
                    sum += sec_tap * constrain(s0 - px, sec_strength, sec_shift);
                    sum += sec_tap * constrain(s1 - px, sec_strength, sec_shift);
                    sum += sec_tap * constrain(s2 - px, sec_strength, sec_shift);
                    sum += sec_tap * constrain(s3 - px, sec_strength, sec_shift);
                }
                dst[x] = px + ((sum - (sum < 0) + 8) >> 4);
            }
            dst += PXSTRIDE(dst_stride);
            tmp += tmp_stride;
        } while (--h);
    }
}

static void cdef_filter_block_y_c(pixel *const dst, const ptrdiff_t stride,
                                  const pixel *const buf, ptrdiff_t cbx,
                                  const int pri_strength,
                                  const int sec_strength,
                                  const int dir, const int damping,
                                  const enum CdefEdgeFlags edges
                                  HIGHBD_DECL_SUFFIX)
{
    const ptrdiff_t buf_stride = get_buffer_stride_y();
    const ptrdiff_t buf_offset = buf_stride * 2 + 2 + cbx * 8;
    cdef_filter_block_c(dst, stride, buf + buf_offset, buf_stride,
                        pri_strength, sec_strength, dir, damping, 8, 8, edges
                        HIGHBD_TAIL_SUFFIX);
}

static void cdef_filter_block_uv_c(pixel **const dst, const ptrdiff_t stride,
                                  const pixel *buf, const ptrdiff_t cbx,
                                  const int pri_strength,
                                  const int sec_strength,
                                  const int dir, const int damping,
                                  const int w, const int h,
                                  const enum CdefEdgeFlags edges
                                  HIGHBD_DECL_SUFFIX)
{
    const int ss_hor = (w == 4);
    // Each stride of buffer holds a rows from each source.
    const ptrdiff_t buf_stride = get_buffer_stride_uv(ss_hor);
    buf += buf_stride * 2 + 2 + cbx * w;
    for (int pl = 1; pl <= 2; pl++) {
        cdef_filter_block_c(dst[pl - 1], stride, buf, buf_stride,
                            pri_strength, sec_strength, dir, damping, w, h,
                            edges HIGHBD_TAIL_SUFFIX);
        // Jump to the middle of the stride to advance to the second plane.
        buf += buf_stride >> 1;
    }
}

#define cdef_uv_fn(w, h) \
static void cdef_filter_block_uv_##w##x##h##_c(pixel **const dst, \
                                               const ptrdiff_t stride, \
                                               const pixel *const buf,  \
                                               const ptrdiff_t cbx, \
                                               const int pri_strength, \
                                               const int sec_strength, \
                                               const int dir, \
                                               const int damping, \
                                               const enum CdefEdgeFlags edges \
                                               HIGHBD_DECL_SUFFIX) \
{ \
    cdef_filter_block_uv_c(dst, stride, buf, cbx, pri_strength, sec_strength, \
                            dir, damping, w, h, edges HIGHBD_TAIL_SUFFIX); \
}

cdef_uv_fn(4, 4);
cdef_uv_fn(4, 8);
cdef_uv_fn(8, 8);

static int cdef_find_dir_c(const pixel *img, const ptrdiff_t stride,
                           unsigned *const var HIGHBD_DECL_SUFFIX)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    int partial_sum_hv[2][8] = { { 0 } };
    int partial_sum_diag[2][15] = { { 0 } };
    int partial_sum_alt[4][11] = { { 0 } };

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            const int px = (img[x] >> bitdepth_min_8) - 128;

            partial_sum_diag[0][     y       +  x      ] += px;
            partial_sum_alt [0][     y       + (x >> 1)] += px;
            partial_sum_hv  [0][     y                 ] += px;
            partial_sum_alt [1][3 +  y       - (x >> 1)] += px;
            partial_sum_diag[1][7 +  y       -  x      ] += px;
            partial_sum_alt [2][3 - (y >> 1) +  x      ] += px;
            partial_sum_hv  [1][                x      ] += px;
            partial_sum_alt [3][    (y >> 1) +  x      ] += px;
        }
        img += PXSTRIDE(stride);
    }

    unsigned cost[8] = { 0 };
    for (int n = 0; n < 8; n++) {
        cost[2] += partial_sum_hv[0][n] * partial_sum_hv[0][n];
        cost[6] += partial_sum_hv[1][n] * partial_sum_hv[1][n];
    }
    cost[2] *= 105;
    cost[6] *= 105;

    static const uint16_t div_table[7] = { 840, 420, 280, 210, 168, 140, 120 };
    for (int n = 0; n < 7; n++) {
        const int d = div_table[n];
        cost[0] += (partial_sum_diag[0][n]      * partial_sum_diag[0][n] +
                    partial_sum_diag[0][14 - n] * partial_sum_diag[0][14 - n]) * d;
        cost[4] += (partial_sum_diag[1][n]      * partial_sum_diag[1][n] +
                    partial_sum_diag[1][14 - n] * partial_sum_diag[1][14 - n]) * d;
    }
    cost[0] += partial_sum_diag[0][7] * partial_sum_diag[0][7] * 105;
    cost[4] += partial_sum_diag[1][7] * partial_sum_diag[1][7] * 105;

    for (int n = 0; n < 4; n++) {
        unsigned *const cost_ptr = &cost[n * 2 + 1];
        for (int m = 0; m < 5; m++)
            *cost_ptr += partial_sum_alt[n][3 + m] * partial_sum_alt[n][3 + m];
        *cost_ptr *= 105;
        for (int m = 0; m < 3; m++) {
            const int d = div_table[2 * m + 1];
            *cost_ptr += (partial_sum_alt[n][m]      * partial_sum_alt[n][m] +
                          partial_sum_alt[n][10 - m] * partial_sum_alt[n][10 - m]) * d;
        }
    }

    int best_dir = 0;
    unsigned best_cost = cost[0];
    for (int n = 1; n < 8; n++) {
        if (cost[n] > best_cost) {
            best_cost = cost[n];
            best_dir = n;
        }
    }

    *var = (best_cost - (cost[best_dir ^ 4])) >> 10;
    return best_dir;
}

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
#include "src/arm/cdef.h"
#elif ARCH_X86
#include "src/x86/cdef.h"
#endif
#endif

COLD void bitfn(dav1d_cdef_dsp_init)(Dav1dCdefDSPContext *const c) {
    c->dir = cdef_find_dir_c;

    c->prep_y = cdef_prep_y_c;
    c->fb_y = cdef_filter_block_y_c;

    c->prep_uv[0] = cdef_prep_uv_444_c;
    c->prep_uv[1] = cdef_prep_uv_422_c;
    c->prep_uv[2] = cdef_prep_uv_420_c;
    c->fb_uv[0] = cdef_filter_block_uv_8x8_c;
    c->fb_uv[1] = cdef_filter_block_uv_4x8_c;
    c->fb_uv[2] = cdef_filter_block_uv_4x4_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    cdef_dsp_init_arm(c);
#elif ARCH_X86
    cdef_dsp_init_x86(c);
#endif
#endif
}
