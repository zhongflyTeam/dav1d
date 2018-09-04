/*
 * ..
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/intops.h"
#include "common/mem.h"
#include "common/validate.h"

#include "src/picture.h"
#include "src/ref.h"

static int picture_alloc_with_edges(Dav1dPicture *const p,
                                    const int w, const int h,
                                    const enum Dav1dPixelLayout layout,
                                    const int bpc,
                                    const int extra, void **const extra_ptr)
{
    int aligned_h;

    if (p->data[0]) {
        fprintf(stderr, "Picture already allocated!\n");
        return -1;
    }
    assert(bpc > 0 && bpc <= 16);

    const int hbd = bpc > 8;
    const int aligned_w = (w + 127) & ~127;
    const int has_chroma = layout != DAV1D_PIXEL_LAYOUT_I400;
    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;
    p->stride[0] = aligned_w << hbd;
    p->stride[1] = has_chroma ? (aligned_w >> ss_hor) << hbd : 0;
    p->p.w = w;
    p->p.h = h;
    p->p.pri = DAV1D_COLOR_PRI_UNKNOWN;
    p->p.trc = DAV1D_TRC_UNKNOWN;
    p->p.mtrx = DAV1D_MC_UNKNOWN;
    p->p.chr = DAV1D_CHR_UNKNOWN;
    aligned_h = (h + 127) & ~127;
    p->p.layout = layout;
    p->p.bpc = bpc;
    const size_t y_sz = p->stride[0] * aligned_h;
    const size_t uv_sz = p->stride[1] * (aligned_h >> ss_ver);
    if (!(p->ref = dav1d_ref_create(y_sz + 2 * uv_sz + extra))) {
        fprintf(stderr, "Failed to allocate memory of size %zu: %s\n",
                y_sz + 2 * uv_sz + extra, strerror(errno));
        return -ENOMEM;
    }
    uint8_t *data = p->ref->data;
    p->data[0] = data;
    p->data[1] = has_chroma ? data + y_sz : NULL;
    p->data[2] = has_chroma ? data + y_sz + uv_sz : NULL;

    if (extra)
        *extra_ptr = &data[y_sz + uv_sz * 2];

    return 0;
}

int dav1d_thread_picture_alloc(Dav1dThreadPicture *const p,
                               const int w, const int h,
                               const enum Dav1dPixelLayout layout, const int bpc,
                               struct thread_data *const t)
{
    p->t = t;

    return picture_alloc_with_edges(&p->p, w, h, layout, bpc,
                                    t != NULL ? sizeof(atomic_int) : 0,
                                    (void **) &p->progress);
}

void dav1d_picture_ref(Dav1dPicture *const dst, const Dav1dPicture *const src) {
    validate_input(dst != NULL);
    validate_input(dst->data[0] == NULL);
    validate_input(src != NULL);

    if (src->ref) {
        validate_input(src->data[0] != NULL);
        dav1d_ref_inc(src->ref);
    }
    *dst = *src;
}

void dav1d_thread_picture_ref(Dav1dThreadPicture *dst,
                              const Dav1dThreadPicture *src)
{
    dav1d_picture_ref(&dst->p, &src->p);
    dst->t = src->t;
    dst->progress = src->progress;
}

void dav1d_picture_unref(Dav1dPicture *const p) {
    validate_input(p != NULL);

    if (p->ref) {
        validate_input(p->data[0] != NULL);
        dav1d_ref_dec(p->ref);
    }
    memset(p, 0, sizeof(*p));
}

void dav1d_thread_picture_unref(Dav1dThreadPicture *const p) {
    dav1d_picture_unref(&p->p);

    p->t = NULL;
    p->progress = NULL;
}

void dav1d_thread_picture_wait(const Dav1dThreadPicture *const p,
                               const int y_in, const enum PlaneType plane_type)
{
    if (!p->t)
        return;

    static const int plane_delay[] = { 16, 8, 0 };

    // convert to luma units; include plane delay from loopfilters; clip
    const int y_unclipped =
        (y_in + plane_delay[plane_type]) << (plane_type & 1);
    const int y = iclip(y_unclipped, 0, p->p.p.h - 1);

    if (atomic_load_explicit(p->progress, memory_order_acquire) >= y)
        return;

    pthread_mutex_lock(&p->t->lock);
    while (atomic_load_explicit(p->progress, memory_order_relaxed) < y) {
        pthread_cond_wait(&p->t->cond, &p->t->lock);
    }
    pthread_mutex_unlock(&p->t->lock);
}
