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

#include "src/cpu.h"
#include "src/cdef.h"

#if BITDEPTH == 8 && ARCH_X86_64
decl_cdef_prep_y_fn(dav1d_cdef_prep_y_8bpc_avx2);
decl_cdef_y_fn(dav1d_cdef_filter_y_8bpc_avx2);

decl_cdef_prep_uv_fn(dav1d_cdef_prep_uv_444_8bpc_avx2);
decl_cdef_prep_uv_fn(dav1d_cdef_prep_uv_422_8bpc_avx2);
decl_cdef_prep_uv_fn(dav1d_cdef_prep_uv_420_8bpc_avx2);
decl_cdef_uv_fn(dav1d_cdef_filter_uv_8x8_8bpc_avx2);
decl_cdef_uv_fn(dav1d_cdef_filter_uv_4x8_8bpc_avx2);
decl_cdef_uv_fn(dav1d_cdef_filter_uv_4x4_8bpc_avx2);
#endif

decl_cdef_dir_fn(BF(dav1d_cdef_dir, avx2));
decl_cdef_dir_fn(BF(dav1d_cdef_dir, sse4));
decl_cdef_dir_fn(BF(dav1d_cdef_dir, ssse3));

static ALWAYS_INLINE void cdef_dsp_init_x86(Dav1dCdefDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

#if BITDEPTH == 8
    if (!(flags & DAV1D_X86_CPU_FLAG_SSE2)) return;
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_SSSE3)) return;

    c->dir = BF(dav1d_cdef_dir, ssse3);

    if (!(flags & DAV1D_X86_CPU_FLAG_SSE41)) return;

    c->dir = BF(dav1d_cdef_dir, sse4);

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->dir = BF(dav1d_cdef_dir, avx2);
#if BITDEPTH == 8
    c->prep_y = BF(dav1d_cdef_prep_y, avx2);
    c->fb_y = BF(dav1d_cdef_filter_y, avx2);

    c->prep_uv[0] = BF(dav1d_cdef_prep_uv_444, avx2);
    c->fb_uv[0] = BF(dav1d_cdef_filter_uv_8x8, avx2);

    c->prep_uv[1] = BF(dav1d_cdef_prep_uv_422, avx2);
    c->fb_uv[1] = BF(dav1d_cdef_filter_uv_4x8, avx2);

    c->prep_uv[2] = BF(dav1d_cdef_prep_uv_420, avx2);
    c->fb_uv[2] = BF(dav1d_cdef_filter_uv_4x4, avx2);
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_AVX512ICL)) return;
#endif
}
