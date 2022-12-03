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

#include <string.h>

#include "common/intops.h"

#include "src/cdef_apply.h"
#include "src/cdef.h"

static void backup2lines(pixel *const dst[3], /*const*/ pixel *const src[3],
                         const ptrdiff_t stride[2],
                         const enum Dav1dPixelLayout layout)
{
    const ptrdiff_t y_stride = PXSTRIDE(stride[0]);
    if (y_stride < 0)
        pixel_copy(dst[0] + y_stride, src[0] + 7 * y_stride, -2 * y_stride);
    else
        pixel_copy(dst[0], src[0] + 6 * y_stride, 2 * y_stride);

    if (layout != DAV1D_PIXEL_LAYOUT_I400) {
        const ptrdiff_t uv_stride = PXSTRIDE(stride[1]);
        if (uv_stride < 0) {
            const int uv_off = layout == DAV1D_PIXEL_LAYOUT_I420 ? 3 : 7;
            pixel_copy(dst[1] + uv_stride, src[1] + uv_off * uv_stride, -2 * uv_stride);
            pixel_copy(dst[2] + uv_stride, src[2] + uv_off * uv_stride, -2 * uv_stride);
        } else {
            const int uv_off = layout == DAV1D_PIXEL_LAYOUT_I420 ? 2 : 6;
            pixel_copy(dst[1], src[1] + uv_off * uv_stride, 2 * uv_stride);
            pixel_copy(dst[2], src[2] + uv_off * uv_stride, 2 * uv_stride);
        }
    }
}

static int adjust_strength(const int strength, const unsigned var) {
    if (!var) return 0;
    const int i = var >> 6 ? imin(ulog2(var >> 6), 12) : 0;
    return (strength * (4 + i) + 8) >> 4;
}

void bytefn(dav1d_cdef_brow)(Dav1dTaskContext *const tc,
                             pixel *const p[3],
                             const Av1Filter *const lflvl,
                             const int by_start, const int by_end,
                             const int sbrow_start, const int sby)
{
    Dav1dFrameContext *const f = (Dav1dFrameContext *)tc->f;
    const int bitdepth_min_8 = BITDEPTH == 8 ? 0 : f->cur.p.bpc - 8;
    const Dav1dDSPContext *const dsp = f->dsp;
    enum CdefEdgeFlags edges = CDEF_HAVE_BOTTOM | (by_start > 0 ? CDEF_HAVE_TOP : 0);
    pixel *ptrs[3] = { p[0], p[1], p[2] };
    const int sbsz = 16;
    const int sb64w = (f->bw + sbsz - 1) / sbsz;
    const int damping = f->frame_hdr->cdef.damping + bitdepth_min_8;
    const enum Dav1dPixelLayout layout = f->cur.p.layout;
    const int uv_idx = DAV1D_PIXEL_LAYOUT_I444 - layout;
    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;
    static const uint8_t uv_dirs[2][8] = { { 0, 1, 2, 3, 4, 5, 6, 7 },
                                           { 7, 0, 2, 4, 5, 6, 6, 6 } };
    const uint8_t *uv_dir = uv_dirs[layout == DAV1D_PIXEL_LAYOUT_I422];
    const int have_tt = f->c->n_tc > 1;
    const int sb128 = f->seq_hdr->sb128;
    const int resize = f->frame_hdr->width[0] != f->frame_hdr->width[1];
    const ptrdiff_t y_stride = PXSTRIDE(f->cur.stride[0]);
    const ptrdiff_t uv_stride = PXSTRIDE(f->cur.stride[1]);

    ALIGN_STK_64(pixel, y_buffer, CDEF_BUFFER_Y_SIZE,);
    ALIGN_STK_64(pixel, uv_buffer, CDEF_BUFFER_UV_SIZE,);

    for (int by = by_start; by < by_end; by += 2, edges |= CDEF_HAVE_TOP) {
        const int tf = tc->top_pre_cdef_toggle;
        const int by_idx = (by & 30) >> 1;
        if (by + 2 >= f->bh) edges &= ~CDEF_HAVE_BOTTOM;

        if ((!have_tt || sbrow_start || by + 2 < by_end) &&
            edges & CDEF_HAVE_BOTTOM)
        {
            // backup pre-filter data for next iteration
            pixel *const cdef_top_bak[3] = {
                f->lf.cdef_line[!tf][0] + have_tt * sby * 4 * y_stride,
                f->lf.cdef_line[!tf][1] + have_tt * sby * 8 * uv_stride,
                f->lf.cdef_line[!tf][2] + have_tt * sby * 8 * uv_stride
            };
            backup2lines(cdef_top_bak, ptrs, f->cur.stride, layout);
        }

        pixel *iptrs[3] = { ptrs[0], ptrs[1], ptrs[2] };
        edges &= ~CDEF_HAVE_LEFT;
        edges |= CDEF_HAVE_RIGHT;

        enum CdefEdgeFlags left_skip = 0, left_skip_uv = 0;

        for (int sbx = 0; sbx < sb64w; sbx++, edges |= CDEF_HAVE_LEFT) {
            // Share a 16-bit mask betweeen blocks.
            uint16_t noskip_mask;

            const int sb128x = sbx >> 1;
            const int sb64_idx = ((by & sbsz) >> 3) + (sbx & 1);
            const int cdef_idx = lflvl[sb128x].cdef_idx[sb64_idx];
            if (cdef_idx == -1 ||
                (!f->frame_hdr->cdef.y_strength[cdef_idx] &&
                 !f->frame_hdr->cdef.uv_strength[cdef_idx]) ||
                !(noskip_mask = lflvl[sb128x].noskip_mask[by_idx][sbx & 1]))
            {
                left_skip = CDEF_LEFT_SKIP;
                left_skip_uv = CDEF_LEFT_SKIP;
                goto next_sb;
            }

            const int y_lvl = f->frame_hdr->cdef.y_strength[cdef_idx];
            const int uv_lvl = f->frame_hdr->cdef.uv_strength[cdef_idx];

            const int y_pri_lvl = (y_lvl >> 2) << bitdepth_min_8;
            int y_sec_lvl = y_lvl & 3;
            y_sec_lvl += y_sec_lvl == 3;
            y_sec_lvl <<= bitdepth_min_8;

            const int uv_pri_lvl = (uv_lvl >> 2) << bitdepth_min_8;
            int uv_sec_lvl = uv_lvl & 3;
            uv_sec_lvl += uv_sec_lvl == 3;
            uv_sec_lvl <<= bitdepth_min_8;

            const pixel *top, *bot;
            ptrdiff_t offset;

            if (!have_tt) goto st_y;
            if (sbrow_start && by == by_start) {
                if (resize) {
                    offset = (sby - 1) * 4 * y_stride + sbx * sbsz * 4;
                    top = &f->lf.cdef_lpf_line[0][offset];
                } else {
                    offset = (sby * (4 << sb128) - 4) * y_stride + sbx * sbsz * 4;
                    top = &f->lf.lr_lpf_line[0][offset];
                }
                bot = iptrs[0] + 8 * y_stride;
            } else if (!sbrow_start && by + 2 >= by_end) {
                top = &f->lf.cdef_line[tf][0][sby * 4 * y_stride + sbx * sbsz * 4];
                if (resize) {
                    offset = (sby * 4 + 2) * y_stride + sbx * sbsz * 4;
                    bot = &f->lf.cdef_lpf_line[0][offset];
                } else {
                    const int line = sby * (4 << sb128) + 4 * sb128 + 2;
                    offset = line * y_stride + sbx * sbsz * 4;
                    bot = &f->lf.lr_lpf_line[0][offset];
                }
            } else {
                st_y:;
                offset = sby * 4 * y_stride;
                top = &f->lf.cdef_line[tf][0][have_tt * offset + sbx * sbsz * 4];
                bot = iptrs[0] + 8 * y_stride;
            }

            int sb_has_right = f->bw > (sbx + 1) * sbsz;
            int remaining_units = (sb_has_right ? sbsz : f->bw - sbx * sbsz) >> 1;
            enum CdefEdgeFlags sb_edges = edges & ~(sb_has_right ? 0 : CDEF_HAVE_RIGHT);

            dsp->cdef.prep_y(y_buffer, iptrs[0], f->cur.stride[0], top,
                             bot, remaining_units, sb_edges | left_skip);

            if (!uv_lvl) {
                left_skip_uv = CDEF_LEFT_SKIP;
            } else {
                pixel *top_uv[2], *bot_uv[2];
                for (int pl = 1; pl <= 2; pl++) {
                    if (!have_tt) goto st_uv;
                    if (sbrow_start && by == by_start) {
                        if (resize) {
                            offset = (sby - 1) * 4 * uv_stride + (sbx * sbsz * 4 >> ss_hor);
                            top_uv[pl - 1] = &f->lf.cdef_lpf_line[pl][offset];
                        } else {
                            const int line = sby * (4 << sb128) - 4;
                            offset = line * uv_stride + (sbx * sbsz * 4 >> ss_hor);
                            top_uv[pl - 1] = &f->lf.lr_lpf_line[pl][offset];
                        }
                        bot_uv[pl - 1] = iptrs[pl] + (8 >> ss_ver) * uv_stride;
                    } else if (!sbrow_start && by + 2 >= by_end) {
                        const ptrdiff_t top_offset = sby * 8 * uv_stride +
                                                     (sbx * sbsz * 4 >> ss_hor);
                        top_uv[pl - 1] = &f->lf.cdef_line[tf][pl][top_offset];
                        if (resize) {
                            offset = (sby * 4 + 2) * uv_stride + (sbx * sbsz * 4 >> ss_hor);
                            bot_uv[pl - 1] = &f->lf.cdef_lpf_line[pl][offset];
                        } else {
                            const int line = sby * (4 << sb128) + 4 * sb128 + 2;
                            offset = line * uv_stride + (sbx * sbsz * 4 >> ss_hor);
                            bot_uv[pl - 1] = &f->lf.lr_lpf_line[pl][offset];
                        }
                    } else {
                        st_uv:;
                        const ptrdiff_t offset = sby * 8 * uv_stride;
                        top_uv[pl - 1] = &f->lf.cdef_line[tf][pl][have_tt * offset + (sbx * sbsz * 4 >> ss_hor)];
                        bot_uv[pl - 1] = iptrs[pl] + (8 >> ss_ver) * uv_stride;
                    }
                }
                dsp->cdef.prep_uv[uv_idx](
                        uv_buffer, iptrs + 1, f->cur.stride[1],
                        top_uv, bot_uv, remaining_units,
                        sb_edges | left_skip_uv);
                left_skip_uv = 0;
            }

            pixel *bptrs[3] = { iptrs[0], iptrs[1], iptrs[2] };
            for (int bx = sbx * sbsz; bx < imin((sbx + 1) * sbsz, f->bw);
                 bx += 2, edges |= CDEF_HAVE_LEFT)
            {
                if (bx + 2 >= f->bw) edges &= ~CDEF_HAVE_RIGHT;

                // check if this 8x8 block had any coded coefficients; if not,
                // go to the next block
                const uint32_t bx_mask = 3U << (bx & 14);
                if (!(noskip_mask & bx_mask)) {
                    goto next_b;
                }

                int dir;
                unsigned variance;
                if (y_pri_lvl || uv_pri_lvl)
                    dir = dsp->cdef.dir(bptrs[0], f->cur.stride[0],
                                        &variance HIGHBD_CALL_SUFFIX);

                // cdef block x
                int cbx = (bx & (sbsz - 1)) >> 1;
                if (y_pri_lvl) {
                    const int adj_y_pri_lvl = adjust_strength(y_pri_lvl, variance);
                    if (adj_y_pri_lvl || y_sec_lvl)
                        dsp->cdef.fb_y(bptrs[0], f->cur.stride[0],
                                       y_buffer, cbx, adj_y_pri_lvl, y_sec_lvl,
                                       dir, damping, edges HIGHBD_CALL_SUFFIX);
                } else if (y_sec_lvl)
                    dsp->cdef.fb_y(bptrs[0], f->cur.stride[0],
                                   y_buffer, cbx, 0, y_sec_lvl, 0,
                                   damping, edges HIGHBD_CALL_SUFFIX);

                if (!uv_lvl) goto next_b;
                assert(layout != DAV1D_PIXEL_LAYOUT_I400);

                const int uvdir = uv_pri_lvl ? uv_dir[dir] : 0;
                dsp->cdef.fb_uv[uv_idx](bptrs + 1, f->cur.stride[1], uv_buffer,
                                        cbx, uv_pri_lvl, uv_sec_lvl, uvdir,
                                        damping - 1, edges HIGHBD_CALL_SUFFIX);

            next_b:
                bptrs[0] += 8;
                bptrs[1] += 8 >> ss_hor;
                bptrs[2] += 8 >> ss_hor;
            }

            left_skip = 0;
        next_sb:
            iptrs[0] += sbsz * 4;
            iptrs[1] += sbsz * 4 >> ss_hor;
            iptrs[2] += sbsz * 4 >> ss_hor;
        }

        ptrs[0] += 8 * PXSTRIDE(f->cur.stride[0]);
        ptrs[1] += 8 * PXSTRIDE(f->cur.stride[1]) >> ss_ver;
        ptrs[2] += 8 * PXSTRIDE(f->cur.stride[1]) >> ss_ver;
        tc->top_pre_cdef_toggle ^= 1;
    }
}
