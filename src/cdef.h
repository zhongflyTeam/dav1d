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

#ifndef DAV1D_SRC_CDEF_H
#define DAV1D_SRC_CDEF_H

#include <stddef.h>
#include <stdint.h>

#include "common/bitdepth.h"

// Buffer size in units of cdef blocks (8x8 pixels for luma)
#define CDEF_BUFFER_UNITS 8

// Maximum required size of the cdef luma buffer
#define CDEF_BUFFER_Y_SIZE ((CDEF_BUFFER_UNITS * 8 + 8 * 2) * 12)

// Maximum required size of the cdef chroma buffer
#define CDEF_BUFFER_UV_SIZE (2 * (CDEF_BUFFER_UNITS * 8 + 8 * 2) * 12)

enum CdefEdgeFlags {
    CDEF_HAVE_LEFT = 1 << 0,
    CDEF_HAVE_RIGHT = 1 << 1,
    CDEF_HAVE_TOP = 1 << 2,
    CDEF_HAVE_BOTTOM = 1 << 3,

    // Used to prepare cdef buffers
    CDEF_LEFT_SKIP = 1 << 4,
};

// Move luma pixel data from $src to $buffer. The format of data in buffer is
// implementation defined for each pair of cdef_prep_y and cdef_y function.
// CDEF operates entirely on pre-filter data; if the right edge is present
// (according to $edges), then the prefilter data is located in $src. However,
// the edge pixels left of, above, or below $src may be post-filter. In order
// to get access to pre-filter top/bottom pixels, use $top and $bottom.
// Accessing pixels to the left is different. In the case that the 8x64 wide
// superblock row has been skipped (according to $edges), then the pixels are
// obtained from src. If not, then the data is obtained from the data in the
// buffer from the previous superblock row.
#define decl_cdef_prep_y_fn(name) \
void (name)(pixel *buffer, const pixel *src, ptrdiff_t src_stride, \
            const pixel *top, const pixel *bottom, int num_blocks, \
            enum CdefEdgeFlags edges)
typedef decl_cdef_prep_y_fn(*cdef_prep_y_fn);

// Move pixel data from both chroma planes ($src) to $buffer. The format of data
// in buffer is implementation defined for each pair of cdef_prep_y and cdef_y
// function.
#define decl_cdef_y_fn(name) \
void (name)(pixel *dst, ptrdiff_t stride, const pixel *buffer, ptrdiff_t cbx, \
            int pri_strength, int sec_strength, int dir, int damping, \
            enum CdefEdgeFlags edges HIGHBD_DECL_SUFFIX)
typedef decl_cdef_y_fn(*cdef_y_fn);

// Move pixel data from both chroma planes ($src) to $buffer. The format of data
// in buffer is implementation defined for each pair of cdef_prep_uv and cdef_uv
// function.
#define decl_cdef_prep_uv_fn(name) \
void (name)(pixel *buffer, pixel **src, ptrdiff_t src_stride, \
            pixel **top, pixel **bottom, int num_blocks,      \
            enum CdefEdgeFlags edges)
typedef decl_cdef_prep_uv_fn(*cdef_prep_uv_fn);

// Apply the chroma cdef filters on buffers at the $cbx th cdef block and the
// results are stored in dst. The format of data in buffer is implementation
// defined for each pair of cdef_prep_uv and cdef_uv function.
#define decl_cdef_uv_fn(name) \
void (name)(pixel **dst, ptrdiff_t stride, const pixel *buffer, ptrdiff_t cbx, \
            int pri_strength, int sec_strength, int dir, int damping, \
            enum CdefEdgeFlags edges HIGHBD_DECL_SUFFIX)
typedef decl_cdef_uv_fn(*cdef_uv_fn);

#define decl_cdef_dir_fn(name) \
int (name)(const pixel *dst, ptrdiff_t dst_stride, unsigned *var HIGHBD_DECL_SUFFIX)
typedef decl_cdef_dir_fn(*cdef_dir_fn);

typedef struct Dav1dCdefDSPContext {
    cdef_dir_fn dir;

    // The buffer storage format of prep_y and fb_y must match
    cdef_prep_y_fn prep_y;
    cdef_y_fn fb_y;

    // The buffer storage format of prep_y, fb_y, uv_estride must match.
    cdef_prep_uv_fn prep_uv[3 /* 444/luma, 422, 420 */];
    cdef_uv_fn fb_uv[3 /* 444/luma, 422, 420 */];
} Dav1dCdefDSPContext;

bitfn_decls(void dav1d_cdef_dsp_init, Dav1dCdefDSPContext *c);

#endif /* DAV1D_SRC_CDEF_H */
