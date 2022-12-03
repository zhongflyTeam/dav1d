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

#include "tests/checkasm/checkasm.h"

#include <string.h>
#include <stdio.h>

#include "common/dump.h"

#include "src/levels.h"
#include "src/cdef.h"

static Dav1dCdefDSPContext cdef_ref;
static int cdef_ref_init = 0;

static int to_binary(int x) { /* 0-15 -> 0000-1111 */
    return (x & 1) + 5 * (x & 2) + 25 * (x & 4) + 125 * (x & 8);
}

static void init_tmp(pixel *buf, ptrdiff_t n, const int bitdepth_max) {
    const int fill_type = rnd() & 7;
    if (fill_type == 0)
        while (n--) /* check for cdef_filter underflows */
            *buf++ = rnd() & 1;
    else if (fill_type == 1)
        while (n--) /* check for cdef_filter overflows */
            *buf++ = bitdepth_max - (rnd() & 1);
    else
        while (n--)
            *buf++ = rnd() & bitdepth_max;
}

// CDEF prep functions are implementation dependent, so just do the standard
// abi/stack tests here.
static void check_cdef_prep_y(const cdef_prep_y_fn prep_fn) {
    ALIGN_STK_64(pixel, src_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8,),
            *const src = src_alloc + 8;
    ALIGN_STK_64(pixel, top_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2,),
            *const top = top_alloc + 8;
    ALIGN_STK_64(pixel, bot_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2,),
            *const bot = bot_alloc + 8;

    ALIGN_STK_64(pixel, c_buf, CDEF_BUFFER_Y_SIZE,);
    ALIGN_STK_64(pixel, a_buf, CDEF_BUFFER_Y_SIZE,);

    const ptrdiff_t stride = (CDEF_BUFFER_UNITS + 2) * 8 * sizeof(pixel);
    ptrdiff_t pxstride = PXSTRIDE(stride);

    declare_func(void, pixel *buffer, const pixel *src, ptrdiff_t src_stride,
                 const pixel *top, const pixel *bottom, int num_units,
                 enum CdefEdgeFlags edges);

    if (check_func(prep_fn, "cdef_prep_y_%dbpc", BITDEPTH)) {
        // Test that the prep and filter function work correctly for entire
        //      64 wide superblock rows and cropped superblocks.
        for (int length = CDEF_BUFFER_UNITS + 1; length > 0; length--) {
            // Test a full sized superblock w/ or w/o a sb to the right.
            int sb_has_right = length == CDEF_BUFFER_UNITS + 1;
            int num_units = length - sb_has_right;
            for (enum CdefEdgeFlags base_edges = 0; base_edges <= 0xf; base_edges++) {
                // Determine have right by sb_has_right.
                if (base_edges & CDEF_HAVE_RIGHT)
                    continue;
                enum CdefEdgeFlags sb_filter_edges =
                        base_edges | (sb_has_right ? CDEF_HAVE_RIGHT : 0);
                enum CdefEdgeFlags sb_edges = sb_filter_edges |
                                              ((sb_filter_edges & CDEF_HAVE_LEFT) && (rnd() & 1) ? CDEF_LEFT_SKIP : 0);
#if BITDEPTH == 16
                const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                const int bitdepth_max = 0xff;
#endif

                // Randomize, but these values shouldn't be read.
                init_tmp(c_buf, CDEF_BUFFER_Y_SIZE, bitdepth_max);
                init_tmp(a_buf, CDEF_BUFFER_Y_SIZE, bitdepth_max);

                if (!(sb_edges & CDEF_LEFT_SKIP)) {
                    enum CdefEdgeFlags prev_sb_edges =
                            base_edges | CDEF_HAVE_LEFT |
                                         CDEF_HAVE_RIGHT |
                                         CDEF_LEFT_SKIP;
                    init_tmp(src_alloc, pxstride * 8, bitdepth_max);
                    init_tmp(top_alloc, pxstride * 2, bitdepth_max);
                    init_tmp(bot_alloc, pxstride * 2, bitdepth_max);

                    call_ref(c_buf, src, stride, top, bot, CDEF_BUFFER_UNITS, prev_sb_edges);
                    call_new(a_buf, src, stride, top, bot, CDEF_BUFFER_UNITS, prev_sb_edges);
                }

                init_tmp(src_alloc, pxstride * 8, bitdepth_max);
                init_tmp(top_alloc, pxstride * 2, bitdepth_max);
                init_tmp(bot_alloc, pxstride * 2, bitdepth_max);

                call_ref(c_buf, src, stride, top, bot, num_units, sb_edges);
                call_new(a_buf, src, stride, top, bot, num_units, sb_edges);

                if (num_units == CDEF_BUFFER_UNITS || num_units == CDEF_BUFFER_UNITS - 1)
                    bench_new(a_buf, src, stride, top, bot, num_units, sb_edges);
            }
        }
    }
}

static void check_cdef_prep_uv(const cdef_prep_uv_fn prep_fn, const int w, const int h) {
    ALIGN_STK_64(pixel, src_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8, [2]),
            *src[2] = {src_alloc[0] + 8, src_alloc[1] + 8};
    ALIGN_STK_64(pixel, top_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2, [2]),
            *top[2] = {top_alloc[0] + 8, top_alloc[1] + 8};
    ALIGN_STK_64(pixel, bot_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2, [2]),
            *bot[2] = {bot_alloc[0] + 8, bot_alloc[1] + 8};

    ALIGN_STK_64(pixel, c_buf, CDEF_BUFFER_UV_SIZE,);
    ALIGN_STK_64(pixel, a_buf, CDEF_BUFFER_UV_SIZE,);

    const ptrdiff_t stride = (CDEF_BUFFER_UNITS + 2) * 8 * sizeof(pixel);
    ptrdiff_t pxstride = PXSTRIDE(stride);

    declare_func(void, pixel *buffer, pixel **src, ptrdiff_t src_stride,
                 pixel **top, pixel **bottom, int num_units,
                 enum CdefEdgeFlags edges);

    if (check_func(prep_fn, "cdef_prep_uv_%dx%d_%dbpc", w, h, BITDEPTH)) {
        // Test that the prep and filter function work correctly for entire
        //      64 wide superblock rows and cropped superblocks.
        for (int length = CDEF_BUFFER_UNITS + 1; length > 0; length--) {
            int sb_has_right = length == CDEF_BUFFER_UNITS + 1;
            int num_units = length - sb_has_right;
            for (enum CdefEdgeFlags base_edges = 0; base_edges <= 0xf; base_edges++) {
                // Determine have right by sb_has_right.
                if (base_edges & CDEF_HAVE_RIGHT)
                    continue;
                enum CdefEdgeFlags sb_filter_edges =
                        base_edges | (sb_has_right ? CDEF_HAVE_RIGHT : 0);
                enum CdefEdgeFlags sb_edges = sb_filter_edges |
                                              ((sb_filter_edges & CDEF_HAVE_LEFT) && (rnd() & 1) ? CDEF_LEFT_SKIP : 0);
#if BITDEPTH == 16
                const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                const int bitdepth_max = 0xff;
#endif

                // Randomize, but these values shouldn't be read.
                init_tmp(c_buf, CDEF_BUFFER_UV_SIZE, bitdepth_max);
                init_tmp(a_buf, CDEF_BUFFER_UV_SIZE, bitdepth_max);

                if (!(sb_edges & CDEF_LEFT_SKIP)) {
                    enum CdefEdgeFlags prev_sb_edges =
                            base_edges | CDEF_HAVE_LEFT |
                                         CDEF_HAVE_RIGHT |
                                         CDEF_LEFT_SKIP;
                    for (int pl = 1; pl <= 2; pl++) {
                        init_tmp(src_alloc[pl - 1], pxstride * 8, bitdepth_max);
                        init_tmp(top_alloc[pl - 1], pxstride * 2, bitdepth_max);
                        init_tmp(bot_alloc[pl - 1], pxstride * 2, bitdepth_max);
                    }
                    call_ref(c_buf, src, stride, top, bot, CDEF_BUFFER_UNITS, prev_sb_edges);
                    call_new(a_buf, src, stride, top, bot, CDEF_BUFFER_UNITS, prev_sb_edges);
                }

                for (int pl = 1; pl <= 2; pl++) {
                    init_tmp(src_alloc[pl - 1], pxstride * 8, bitdepth_max);
                    init_tmp(top_alloc[pl - 1], pxstride * 2, bitdepth_max);
                    init_tmp(bot_alloc[pl - 1], pxstride * 2, bitdepth_max);
                }

                call_ref(c_buf, src, stride, top, bot, num_units, sb_edges);
                call_new(a_buf, src, stride, top, bot, num_units, sb_edges);

                if (num_units == CDEF_BUFFER_UNITS || num_units == CDEF_BUFFER_UNITS - 1)
                    bench_new(a_buf, src, stride, top, bot, num_units, sb_edges);
            }
        }
    }
}

static void check_cdef_filter_y(const cdef_y_fn fn,
                                const cdef_prep_y_fn prep_fn)
{
    ALIGN_STK_64(pixel, c_src_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8,),
            *const c_src = c_src_alloc + 8;
    ALIGN_STK_64(pixel, a_src_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8,),
            *const a_src = a_src_alloc + 8;
    ALIGN_STK_64(pixel, top_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2,),
            *const top = top_alloc + 8;
    ALIGN_STK_64(pixel, bot_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2,),
            *const bot = bot_alloc + 8;

    ALIGN_STK_64(pixel, c_dst_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8,),
            *const c_dst = c_dst_alloc + 8;
    ALIGN_STK_64(pixel, a_dst_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8,),
            *const a_dst = a_dst_alloc + 8;

    ALIGN_STK_64(pixel, c_buf, CDEF_BUFFER_Y_SIZE,);
    ALIGN_STK_64(pixel, a_buf, CDEF_BUFFER_Y_SIZE,);

    const ptrdiff_t stride = (CDEF_BUFFER_UNITS + 2) * 8 * sizeof(pixel);
    ptrdiff_t pxstride = PXSTRIDE(stride);

    declare_func(void, pixel *dst, ptrdiff_t dst_stride,
                 const pixel *buf, ptrdiff_t cbx,
                 int pri_strength, int sec_strength, int dir, int damping,
                 enum CdefEdgeFlags edges HIGHBD_DECL_SUFFIX);

    for (int s = 0x1; s <= 0x3; s++) {
        if (check_func(fn, "cdef_filter_y_%02d_%dbpc", to_binary(s), BITDEPTH)) {
            int edges_benched[16] = {0};

            // Test that the prep and filter function work correctly for entire
            //      64 wide superblock rows and cropped superblocks.
            for (int length = CDEF_BUFFER_UNITS + 1; length > 0; length--) {
                // Test a full sized superblock w/ or w/o a sb to the right.
                int sb_has_right = length == CDEF_BUFFER_UNITS + 1;
                int num_units = length - sb_has_right;
                for (enum CdefEdgeFlags base_edges = 0; base_edges <= 0xf; base_edges++) {
                    // Determine have right by sb_has_right or position within
                    //   the superblock.
                    if (base_edges & CDEF_HAVE_RIGHT)
                        continue;
                    enum CdefEdgeFlags sb_filter_edges =
                            base_edges | (sb_has_right ? CDEF_HAVE_RIGHT : 0);
                    enum CdefEdgeFlags sb_edges = sb_filter_edges |
                            ((sb_filter_edges & CDEF_HAVE_LEFT) && (rnd() & 1) ? CDEF_LEFT_SKIP : 0);
#if BITDEPTH == 16
                    const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                    const int bitdepth_max = 0xff;
#endif
                    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;

                    // Randomize, but these values shouldn't be read.
                    init_tmp(c_buf, CDEF_BUFFER_Y_SIZE, bitdepth_max);
                    init_tmp(a_buf, CDEF_BUFFER_Y_SIZE, bitdepth_max);

                    if (!(sb_edges & CDEF_LEFT_SKIP)) {
                        enum CdefEdgeFlags prev_sb_edges =
                                base_edges | CDEF_HAVE_LEFT |
                                             CDEF_HAVE_RIGHT |
                                             CDEF_LEFT_SKIP;
                        init_tmp(c_src_alloc, pxstride * 8, bitdepth_max);
                        pixel_copy(a_src_alloc, c_src_alloc, pxstride * 8);
                        init_tmp(top_alloc, pxstride * 2, bitdepth_max);
                        init_tmp(bot_alloc, pxstride * 2, bitdepth_max);
                        cdef_ref.prep_y(c_buf, c_src, stride, top, bot,
                                      CDEF_BUFFER_UNITS, prev_sb_edges);
                        prep_fn(a_buf, a_src, stride, top, bot,
                                CDEF_BUFFER_UNITS, prev_sb_edges);
                    }

                    init_tmp(c_src_alloc, pxstride * 8, bitdepth_max);
                    pixel_copy(a_src_alloc, c_src_alloc, pxstride * 8);
                    init_tmp(top_alloc, pxstride * 2, bitdepth_max);
                    init_tmp(bot_alloc, pxstride * 2, bitdepth_max);

                    cdef_ref.prep_y(c_buf, c_src, stride, top, bot, num_units, sb_edges);
                    prep_fn(a_buf, a_src, stride, top, bot, num_units, sb_edges);

                    for (int dir = 0; dir < 8; dir++) {
                        // dirty the dst buffers
                        init_tmp(a_dst_alloc, CDEF_BUFFER_UNITS * 8 * 8 + 16, bitdepth_max);
                        init_tmp(c_dst_alloc, CDEF_BUFFER_UNITS * 8 * 8 + 16, bitdepth_max);
                        for (int cbx = 0; cbx < num_units; cbx++) {
                            enum CdefEdgeFlags edges = sb_filter_edges
                                                      | (cbx == 0 ? 0 : CDEF_HAVE_LEFT)
                                                      | (cbx != num_units - 1 ? CDEF_HAVE_RIGHT : 0);

                            pixel *c_dst_ptr = c_dst + cbx * 8, *a_dst_ptr = a_dst + cbx * 8;

                            const int pri_strength = s & 2 ? (1 + (rnd() % 15)) << bitdepth_min_8 : 0;
                            const int sec_strength = s & 1 ? 1 << ((rnd() % 3) + bitdepth_min_8) : 0;
                            const int damping = 3 + (rnd() & 3) + bitdepth_min_8;

                            call_ref(c_dst_ptr, stride, c_buf, cbx, pri_strength, sec_strength,
                                     dir, damping, edges HIGHBD_TAIL_SUFFIX);
                            call_new(a_dst_ptr, stride, a_buf, cbx, pri_strength, sec_strength,
                                     dir, damping, edges HIGHBD_TAIL_SUFFIX);
                            if (checkasm_check_pixel(c_dst_ptr, stride, a_dst_ptr, stride, 8, 8, "dst")) {
                                fprintf(stderr,
                                        "strength = %d:%d, dir = %d, damping = %d,"
                                        "edges = %04d, sb right = %d cdef block = %d/%d\n",
                                        pri_strength, sec_strength, dir, damping,
                                        to_binary(edges), sb_has_right, cbx, num_units);
                                return;
                            }
                            if (!edges_benched[edges] && dir == 7 &&
                                (edges == 0x5 || edges == 0xa || edges == 0xf))
                            {
                                bench_new(a_dst, stride, a_buf, cbx, pri_strength,
                                          sec_strength, dir, damping, edges
                                          HIGHBD_TAIL_SUFFIX);
                                edges_benched[edges] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void check_cdef_filter_uv(const cdef_uv_fn fn,
                                 const cdef_prep_uv_fn prep_fn,
                                 const int uv_idx, const int w, const int h) {
    ALIGN_STK_64(pixel, c_src_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8, [2]),
            *c_src[2] = {c_src_alloc[0] + 8, c_src_alloc[1] + 8};
    ALIGN_STK_64(pixel, a_src_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8, [2]),
            *a_src[2] = {a_src_alloc[0] + 8, a_src_alloc[1] + 8};
    ALIGN_STK_64(pixel, top_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2, [2]),
            *top[2] = {top_alloc[0] + 8, top_alloc[1] + 8};
    ALIGN_STK_64(pixel, bot_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 2, [2]),
            *bot[2] = {bot_alloc[0] + 8, bot_alloc[1] + 8};

    ALIGN_STK_64(pixel, c_dst_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8, [2]),
            *c_dst[2] = {c_dst_alloc[0] + 8, c_dst_alloc[1] + 8};
    ALIGN_STK_64(pixel, a_dst_alloc, (CDEF_BUFFER_UNITS + 2) * 8 * 8, [2]),
            *a_dst[2] = {a_dst_alloc[0] + 8, a_dst_alloc[1] + 8};

    ALIGN_STK_64(pixel, c_buf, CDEF_BUFFER_UV_SIZE,);
    ALIGN_STK_64(pixel, a_buf, CDEF_BUFFER_UV_SIZE,);

    const ptrdiff_t stride = (CDEF_BUFFER_UNITS + 2) * 8 * sizeof(pixel);
    ptrdiff_t pxstride = PXSTRIDE(stride);

    declare_func(void, pixel **dst, ptrdiff_t dst_stride,
                 const pixel *buf, ptrdiff_t cdx,
                 int pri_strength, int sec_strength, int dir, int damping,
                 enum CdefEdgeFlags edges HIGHBD_DECL_SUFFIX);

    for (int s = 0x1; s <= 0x3; s++) {
        if (check_func(fn, "cdef_filter_uv_%dx%d_%02d_%dbpc", w, h, to_binary(s), BITDEPTH)) {
            int edges_benched[16] = { 0 };

            // Test that the prep and filter function work correctly for entire
            //      64 wide superblock rows and cropped superblocks.
            for (int length = CDEF_BUFFER_UNITS + 1; length > 0; length--) {
                // Test a full sized superblock w/ or w/o a sb to the right.
                int sb_has_right = length == CDEF_BUFFER_UNITS + 1;
                int num_units = length - sb_has_right;
                for (enum CdefEdgeFlags base_edges = 0; base_edges <= 0xf; base_edges++) {
                    // Determine have right by sb_has_right or position within
                    //   the superblock.
                    if (base_edges & CDEF_HAVE_RIGHT)
                        continue;
                    enum CdefEdgeFlags sb_filter_edges =
                            base_edges | (sb_has_right ? CDEF_HAVE_RIGHT : 0);
                    enum CdefEdgeFlags sb_edges = sb_filter_edges |
                            ((sb_filter_edges & CDEF_HAVE_LEFT) && (rnd() & 1) ? CDEF_LEFT_SKIP : 0);
#if BITDEPTH == 16
                    const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                    const int bitdepth_max = 0xff;
#endif
                    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;

                    // Randomize, but these values shouldn't be read.
                    init_tmp(c_buf, CDEF_BUFFER_UV_SIZE, bitdepth_max);
                    init_tmp(a_buf, CDEF_BUFFER_UV_SIZE, bitdepth_max);

                    if (!(sb_edges & CDEF_LEFT_SKIP)) {
                        enum CdefEdgeFlags prev_sb_edges =
                                base_edges | CDEF_HAVE_LEFT |
                                             CDEF_HAVE_RIGHT |
                                             CDEF_LEFT_SKIP;
                        for (int pl = 1; pl <= 2; pl++) {
                            init_tmp(c_src_alloc[pl - 1], pxstride * 8, bitdepth_max);
                            pixel_copy(a_src_alloc[pl - 1], c_src_alloc[pl - 1], pxstride * 8);
                            init_tmp(top_alloc[pl - 1], pxstride * 2, bitdepth_max);
                            init_tmp(bot_alloc[pl - 1], pxstride * 2, bitdepth_max);
                        }
                        cdef_ref.prep_uv[uv_idx](c_buf, c_src, stride, top, bot,
                                                 CDEF_BUFFER_UNITS,
                                                 prev_sb_edges);
                        prep_fn(a_buf, a_src, stride, top, bot,
                                CDEF_BUFFER_UNITS, prev_sb_edges);
                    }

                    for (int pl = 1; pl <= 2; pl++) {
                        init_tmp(c_src_alloc[pl - 1], pxstride * 8, bitdepth_max);
                        pixel_copy(a_src_alloc[pl - 1], c_src_alloc[pl - 1], pxstride * 8);
                        init_tmp(top_alloc[pl - 1], pxstride * 2, bitdepth_max);
                        init_tmp(bot_alloc[pl - 1], pxstride * 2, bitdepth_max);
                    }

                    cdef_ref.prep_uv[uv_idx](c_buf, c_src, stride, top, bot, num_units, sb_edges);
                    prep_fn(a_buf, a_src, stride, top, bot, num_units, sb_edges);

                    for (int dir = 0; dir < 8; dir++) {
                        // dirty the dst buffers
                        for (int pl = 1; pl <= 2; pl++) {
                            init_tmp(a_dst_alloc[pl - 1], pxstride * 8, bitdepth_max);
                            init_tmp(c_dst_alloc[pl - 1], pxstride * 8, bitdepth_max);
                        }
                        for (int cbx = 0; cbx < num_units; cbx++) {
                            enum CdefEdgeFlags edges = sb_filter_edges
                                                       | (cbx == 0 ? 0 : CDEF_HAVE_LEFT)
                                                       | (cbx != num_units - 1 ? CDEF_HAVE_RIGHT : 0);

                            pixel *c_dst_ptrs[2] = {c_dst[0] + cbx * w, c_dst[1] + cbx * w},
                                  *a_dst_ptrs[2] = {a_dst[0] + cbx * w, a_dst[1] + cbx * w};

                            const int pri_strength = s & 2 ? (1 + (rnd() % 15)) << bitdepth_min_8 : 0;
                            const int sec_strength = s & 1 ? 1 << ((rnd() % 3) + bitdepth_min_8) : 0;
                            const int damping = 3 + (rnd() & 3) + bitdepth_min_8 - 1;

                            call_ref(c_dst_ptrs, stride, c_buf, cbx, pri_strength, sec_strength,
                                     dir, damping, edges HIGHBD_TAIL_SUFFIX);
                            call_new(a_dst_ptrs, stride, a_buf, cbx, pri_strength, sec_strength,
                                     dir, damping, edges HIGHBD_TAIL_SUFFIX);
                            for (int pl = 1; pl <= 2; pl++) {
                                if (checkasm_check_pixel(c_dst_ptrs[pl - 1], stride,
                                                         a_dst_ptrs[pl - 1], stride,
                                                         w, h, "dst")) {
                                    fprintf(stderr,
                                            "pl = %d, strength = %d:%d, dir = %d, damping = %d, "
                                            "edges = %04d, sb right = %d, cdef unit = %d/%d\n",
                                            pl, pri_strength, sec_strength, dir, damping,
                                            to_binary(edges), sb_has_right, cbx, num_units);
                                    return;
                                }
                            }
                            if (!edges_benched[edges] && dir == 7 &&
                                (edges == 0x5 || edges == 0xa || edges == 0xf))
                            {
                                bench_new(a_dst, stride, a_buf, cbx, pri_strength,
                                          sec_strength, dir, damping, edges HIGHBD_TAIL_SUFFIX);
                                edges_benched[edges] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void check_cdef_direction(const cdef_dir_fn fn) {
    ALIGN_STK_64(pixel, src, 8 * 8,);

    declare_func(int, pixel *src, ptrdiff_t dst_stride, unsigned *var
                 HIGHBD_DECL_SUFFIX);

    if (check_func(fn, "cdef_dir_%dbpc", BITDEPTH)) {
        unsigned c_var, a_var;
#if BITDEPTH == 16
        const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
        const int bitdepth_max = 0xff;
#endif
        init_tmp(src, 64, bitdepth_max);

        const int c_dir = call_ref(src, 8 * sizeof(pixel), &c_var HIGHBD_TAIL_SUFFIX);
        const int a_dir = call_new(src, 8 * sizeof(pixel), &a_var HIGHBD_TAIL_SUFFIX);
        if (c_var != a_var || c_dir != a_dir) {
            if (fail()) {
                hex_fdump(stderr, src, 8 * sizeof(pixel), 8, 8, "src");
                fprintf(stderr, "c_dir %d a_dir %d\n", c_dir, a_dir);
            }
        }
        bench_new(src, 8 * sizeof(pixel), &a_var HIGHBD_TAIL_SUFFIX);
    }
    report("cdef_dir");
}

void bitfn(checkasm_check_cdef)(void) {
    Dav1dCdefDSPContext c;
    bitfn(dav1d_cdef_dsp_init)(&c);
    if (!cdef_ref_init) {
        cdef_ref = c;
        cdef_ref_init = 1;
    }

    check_cdef_direction(c.dir);

    check_cdef_prep_y(c.prep_y);
    check_cdef_filter_y(c.fb_y, c.prep_y);

    // TODO: If prep_uv changes, but not fb_uv then filter tests should still
    //       be run.
    check_cdef_prep_uv(c.prep_uv[0], 8, 8);
    check_cdef_filter_uv(c.fb_uv[0], c.prep_uv[0], 0, 8, 8);
    check_cdef_prep_uv(c.prep_uv[1], 4, 8);
    check_cdef_filter_uv(c.fb_uv[1], c.prep_uv[1], 1, 4, 8);
    check_cdef_prep_uv(c.prep_uv[2], 4, 4);
    check_cdef_filter_uv(c.fb_uv[2], c.prep_uv[2], 2, 4, 4);

    report("cdef_filter");
}
