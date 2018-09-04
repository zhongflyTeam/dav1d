/*
 * ..
 */

#include <string.h>

#include "common/intops.h"

#include "src/cdef_apply.h"

static void backup2lines(pixel *const dst[3][2],
                         /*const*/ pixel *const src[3],
                         const ptrdiff_t src_stride[2], int y_off, int w,
                         const enum Dav1dPixelLayout layout)
{
    pixel_copy(dst[0][0], src[0] + (y_off - 2) * PXSTRIDE(src_stride[0]), w);
    pixel_copy(dst[0][1], src[0] + (y_off - 1) * PXSTRIDE(src_stride[0]), w);

    if (layout == DAV1D_PIXEL_LAYOUT_I400) return;
    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;

    w >>= ss_hor;
    y_off >>= ss_ver;
    pixel_copy(dst[1][0], src[1] + (y_off - 2) * PXSTRIDE(src_stride[1]), w);
    pixel_copy(dst[1][1], src[1] + (y_off - 1) * PXSTRIDE(src_stride[1]), w);
    pixel_copy(dst[2][0], src[2] + (y_off - 2) * PXSTRIDE(src_stride[1]), w);
    pixel_copy(dst[2][1], src[2] + (y_off - 1) * PXSTRIDE(src_stride[1]), w);
}

static void restore2lines(pixel *const dst[3],
                          const ptrdiff_t dst_stride[2],
                          /*const*/ pixel *const src[3][2],
                          int w, const enum Dav1dPixelLayout layout)
{
    pixel_copy(dst[0] - 2 * PXSTRIDE(dst_stride[0]), src[0][0], w);
    pixel_copy(dst[0] - 1 * PXSTRIDE(dst_stride[0]), src[0][1], w);

    if (layout == DAV1D_PIXEL_LAYOUT_I400) return;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;

    w >>= ss_hor;
    pixel_copy(dst[1] - 2 * PXSTRIDE(dst_stride[1]), src[1][0], w);
    pixel_copy(dst[1] - 1 * PXSTRIDE(dst_stride[1]), src[1][1], w);
    pixel_copy(dst[2] - 2 * PXSTRIDE(dst_stride[1]), src[2][0], w);
    pixel_copy(dst[2] - 1 * PXSTRIDE(dst_stride[1]), src[2][1], w);
}

static void backup2x8(pixel dst[3][8][2],
                      /*const*/ pixel *const src[3],
                      const ptrdiff_t src_stride[2], int x_off,
                      const enum Dav1dPixelLayout layout)
{
    for (int y = 0, y_off = 0; y < 8; y++, y_off += PXSTRIDE(src_stride[0]))
        pixel_copy(dst[0][y], &src[0][y_off + x_off - 2], 2);

    if (layout == DAV1D_PIXEL_LAYOUT_I400) return;
    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;

    x_off >>= ss_hor;
    for (int y = 0, y_off = 0; y < (8 >> ss_ver); y++, y_off += PXSTRIDE(src_stride[1])) {
        pixel_copy(dst[1][y], &src[1][y_off + x_off - 2], 2);
        pixel_copy(dst[2][y], &src[2][y_off + x_off - 2], 2);
    }
}

static void restore2x8(pixel *const dst[3],
                       const ptrdiff_t dst_stride[2],
                       const pixel src[3][8][2], const enum Dav1dPixelLayout layout)
{
    for (int y = 0, y_off = 0; y < 8; y++, y_off += PXSTRIDE(dst_stride[0]))
        pixel_copy(&dst[0][y_off - 2], src[0][y], 2);

    if (layout == DAV1D_PIXEL_LAYOUT_I400) return;
    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;

    for (int y = 0, y_off = 0; y < (8 >> ss_ver); y++, y_off += PXSTRIDE(dst_stride[1])) {
        pixel_copy(&dst[1][y_off - 2], src[1][y], 2);
        pixel_copy(&dst[2][y_off - 2], src[2][y], 2);
    }
}

static int adjust_strength(const int strength, const unsigned var) {
    if (!var) return 0;
    const int i = var >> 6 ? imin(ulog2(var >> 6), 12) : 0;
    return (strength * (4 + i) + 8) >> 4;
}

void bytefn(dav1d_cdef_brow)(const Dav1dFrameContext *const f,
                             pixel *const p[3],
                             const Av1Filter *const lflvl,
                             const int by_start, const int by_end)
{
    const Dav1dDSPContext *const dsp = f->dsp;
    enum CdefEdgeFlags edges = HAVE_BOTTOM | (by_start > 0 ? HAVE_TOP : 0);
    pixel *ptrs[3] = { p[0], p[1], p[2] };
    const int sbsz = 16;
    const int sb64w = f->sb128w << 1;
    const int damping = f->frame_hdr.cdef.damping + BITDEPTH - 8;
    const enum Dav1dPixelLayout layout = f->cur.p.p.layout;
    const int uv_idx = DAV1D_PIXEL_LAYOUT_I444 - layout;
    const int has_chroma = layout != DAV1D_PIXEL_LAYOUT_I400;
    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;

    // FIXME a design improvement that could be made here is to keep a set of
    // flags for each block position on whether the block was filtered; if not,
    // the backup of pre-filter data is empty, and the restore is therefore
    // unnecessary as well.

    for (int by = by_start; by < by_end; by += 2, edges |= HAVE_TOP) {
        if (by + 2 >= f->bh) edges &= ~HAVE_BOTTOM;

        if (edges & HAVE_TOP) {
            // backup post-filter data (will be restored at the end)
            backup2lines(f->lf.cdef_line_ptr[1], ptrs, f->cur.p.stride,
                         0, f->bw * 4, layout);

            // restore pre-filter data from last iteration
            restore2lines(ptrs, f->cur.p.stride, f->lf.cdef_line_ptr[0],
                          f->bw * 4, layout);
        }
        if (edges & HAVE_BOTTOM) {
            // backup pre-filter data for next iteration
            backup2lines(f->lf.cdef_line_ptr[0], ptrs, f->cur.p.stride,
                         8, f->bw * 4, layout);
        }

        pixel lr_bak[2 /* idx */][3 /* plane */][8 /* y */][2 /* x */];
        pixel *iptrs[3] = { ptrs[0], ptrs[1], ptrs[2] };
        edges &= ~HAVE_LEFT;
        edges |= HAVE_RIGHT;
        for (int sbx = 0, last_skip = 1; sbx < sb64w; sbx++, edges |= HAVE_LEFT) {
            const int sb128x = sbx >>1;
            const int sb64_idx = ((by & sbsz) >> 3) + (sbx & 1);
            const int cdef_idx = lflvl[sb128x].cdef_idx[sb64_idx];
            if (cdef_idx == -1 ||
                (!f->frame_hdr.cdef.y_strength[cdef_idx] &&
                 !f->frame_hdr.cdef.uv_strength[cdef_idx]))
            {
                last_skip = 1;
                goto next_sb;
            }

            const int y_lvl = f->frame_hdr.cdef.y_strength[cdef_idx];
            const int uv_lvl = f->frame_hdr.cdef.uv_strength[cdef_idx];
            pixel *bptrs[3] = { iptrs[0], iptrs[1], iptrs[2] };
            for (int bx = sbx * sbsz; bx < imin((sbx + 1) * sbsz, f->bw);
                 bx += 2, edges |= HAVE_LEFT)
            {
                if (bx + 2 >= f->bw) edges &= ~HAVE_RIGHT;

                // check if this 8x8 block had any coded coefficients; if not,
                // go to the next block
                const unsigned bx_mask = 3U << (bx & 30);
                const int by_idx = by & 30;
                if (!((lflvl[sb128x].noskip_mask[by_idx + 0] |
                       lflvl[sb128x].noskip_mask[by_idx + 1]) & bx_mask))
                {
                    last_skip = 1;
                    goto next_b;
                }

                if (!last_skip) {
                    // backup post-filter data (will be restored at the end)
                    backup2x8(lr_bak[1], bptrs, f->cur.p.stride, 0, layout);

                    // restore pre-filter data from last iteration
                    restore2x8(bptrs, f->cur.p.stride, lr_bak[0], layout);
                }
                if (edges & HAVE_RIGHT) {
                    // backup pre-filter data for next iteration
                    backup2x8(lr_bak[0], bptrs, f->cur.p.stride, 8, layout);
                }

                // the actual filter
                const int y_pri_lvl = (y_lvl >> 2) << (BITDEPTH - 8);
                int y_sec_lvl = y_lvl & 3;
                y_sec_lvl += y_sec_lvl == 3;
                y_sec_lvl <<= BITDEPTH - 8;
                const int uv_pri_lvl = (uv_lvl >> 2) << (BITDEPTH - 8);
                int uv_sec_lvl = uv_lvl & 3;
                uv_sec_lvl += uv_sec_lvl == 3;
                uv_sec_lvl <<= BITDEPTH - 8;
                unsigned variance;
                const int dir = dsp->cdef.dir(bptrs[0], f->cur.p.stride[0],
                                              &variance);
                if (y_lvl) {
                    dsp->cdef.fb[0](bptrs[0], f->cur.p.stride[0],
                                    adjust_strength(y_pri_lvl, variance),
                                    y_sec_lvl, y_pri_lvl ? dir : 0,
                                    damping, edges);
                }
                if (uv_lvl && has_chroma) {
                    const int uvdir =
                        f->cur.p.p.layout != DAV1D_PIXEL_LAYOUT_I422 ? dir :
                        ((uint8_t[]) { 7, 0, 2, 4, 5, 6, 6, 6 })[dir];
                    for (int pl = 1; pl <= 2; pl++) {
                        dsp->cdef.fb[uv_idx](bptrs[pl], f->cur.p.stride[1],
                                             uv_pri_lvl, uv_sec_lvl,
                                             uv_pri_lvl ? uvdir : 0,
                                             damping - 1, edges);
                    }
                }

                if (!last_skip) {
                    // restore post-filter data from the beginning of this loop
                    restore2x8(bptrs, f->cur.p.stride, lr_bak[1], layout);
                }
                last_skip = 0;

            next_b:
                bptrs[0] += 8;
                bptrs[1] += 8 >> ss_hor;
                bptrs[2] += 8 >> ss_hor;
            }

        next_sb:
            iptrs[0] += sbsz * 4;
            iptrs[1] += sbsz * 4 >> ss_hor;
            iptrs[2] += sbsz * 4 >> ss_hor;
        }

        if (edges & HAVE_TOP) {
            // restore post-filter data from the beginning of this loop
            restore2lines(ptrs, f->cur.p.stride, f->lf.cdef_line_ptr[1],
                          f->bw * 4, layout);
        }

        ptrs[0] += 8 * PXSTRIDE(f->cur.p.stride[0]);
        ptrs[1] += 8 * PXSTRIDE(f->cur.p.stride[1]) >> ss_ver;
        ptrs[2] += 8 * PXSTRIDE(f->cur.p.stride[1]) >> ss_ver;
    }
}
