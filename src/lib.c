/*
 * ..
 */

#include <errno.h>
#include <string.h>

#include "dav1d.h"

#include "include/version.h"

#include "common/mem.h"
#include "common/validate.h"

#include "src/data.h"
#include "src/internal.h"
#include "src/obu.h"
#include "src/qm.h"
#include "src/ref.h"
#include "src/wedge.h"

void dav1d_init(void) {
    av1_init_wedge_masks();
    av1_init_interintra_masks();
    av1_init_qm_tables();
}

const char *dav1d_version(void) {
    return DAV1D_VERSION;
}

void dav1d_default_settings(Dav1dSettings *const s) {
    s->n_frame_threads = 1;
    s->n_tile_threads = 1;
}

int dav1d_open(Dav1dContext **const c_out,
               const Dav1dSettings *const s)
{
    validate_input_or_ret(c_out != NULL, -EINVAL);
    validate_input_or_ret(s != NULL, -EINVAL);
    validate_input_or_ret(s->n_tile_threads >= 1 &&
                          s->n_tile_threads <= 256, -EINVAL);
    validate_input_or_ret(s->n_frame_threads >= 1 &&
                          s->n_frame_threads <= 256, -EINVAL);

    Dav1dContext *const c = *c_out = dav1d_alloc_aligned(sizeof(*c), 32);
    if (!c) goto error;
    memset(c, 0, sizeof(*c));

    c->n_fc = s->n_frame_threads;
    c->fc = dav1d_alloc_aligned(sizeof(*c->fc) * s->n_frame_threads, 32);
    if (!c->fc) goto error;
    memset(c->fc, 0, sizeof(*c->fc) * s->n_frame_threads);
    for (int n = 0; n < s->n_frame_threads; n++) {
        c->fc[n].c = c;
        c->fc[n].lf.last_sharpness = -1;
        c->fc[n].n_tc = s->n_tile_threads;
        c->fc[n].tc = dav1d_alloc_aligned(sizeof(*c->fc[n].tc) *
                                          s->n_tile_threads, 32);
        if (!c->fc[n].tc) goto error;
        memset(c->fc[n].tc, 0, sizeof(*c->fc[n].tc) * s->n_tile_threads);
        for (int m = 0; m < s->n_tile_threads; m++) {
            c->fc[n].tc[m].f = &c->fc[n];
            c->fc[n].tc[m].cf = dav1d_alloc_aligned(32 * 32 * sizeof(int32_t), 32);
            if (!c->fc[n].tc[m].cf) goto error;
            memset(c->fc[n].tc[m].cf, 0, 32 * 32 * sizeof(int32_t));
            c->fc[n].tc[m].emu_edge =
                dav1d_alloc_aligned(160 * (128 + 7) * sizeof(uint16_t), 32);
            if (!c->fc[n].tc[m].emu_edge) goto error;
        }
        c->fc[n].libaom_cm = av1_alloc_ref_mv_common();
    }

    // intra edge tree
    c->intra_edge.root[BL_128X128] = &c->intra_edge.branch_sb128[0].node;
    init_mode_tree(c->intra_edge.root[BL_128X128], c->intra_edge.tip_sb128, 1);
    c->intra_edge.root[BL_64X64] = &c->intra_edge.branch_sb64[0].node;
    init_mode_tree(c->intra_edge.root[BL_64X64], c->intra_edge.tip_sb64, 0);

    return 0;

error:
    if (c) {
        if (c->fc) {
            for (int n = 0; n < c->n_fc; n++)
                if (c->fc[n].tc)
                    free(c->fc[n].tc);
            free(c->fc);
        }
        free(c);
    }
    fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
    return -ENOMEM;
}

int dav1d_decode(Dav1dContext *const c,
                 Dav1dData *const in, Dav1dPicture *const out)
{
    int res;

    validate_input_or_ret(c != NULL, -EINVAL);
    validate_input_or_ret(out != NULL, -EINVAL);

    while (in->sz > 0) {
        if ((res = parse_obus(c, in)) < 0)
            return res;

        assert(res <= in->sz);
        in->sz -= res;
        in->data += res;
    }
    dav1d_data_unref(in);

    if (c->out.data[0]) {
        dav1d_picture_ref(out, &c->out);
        dav1d_picture_unref(&c->out);

        return 0;
    }

    return -EAGAIN;
}

void dav1d_close(Dav1dContext *const c) {
    validate_input(c != NULL);

    for (int n = 0; n < c->n_fc; n++) {
        for (int m = 0; m < c->fc[n].n_tc; m++) {
            free(c->fc[n].tc[m].cf);
            free(c->fc[n].tc[m].emu_edge);
        }
        free(c->fc[n].tc);
        free(c->fc[n].a);
        free(c->fc[n].lf.mask);
        free(c->fc[n].lf.level);
        free(c->fc[n].lf.tx_lpf_right_edge[0]);
        av1_free_ref_mv_common(c->fc[n].libaom_cm);
        free(c->fc[n].lf.cdef_line);
        free(c->fc[n].lf.lr_lpf_line);
    }
    free(c->fc);
    for (int n = 0; n < c->n_tile_data; n++)
        dav1d_data_unref(&c->tile[n].data);
    for (int n = 0; n < 8; n++) {
        if (c->refs[n].p.p.data[0])
            dav1d_thread_picture_unref(&c->refs[n].p);
        if (c->refs[n].refmvs)
            dav1d_ref_dec(c->refs[n].refmvs);
        if (c->refs[n].segmap)
            dav1d_ref_dec(c->refs[n].segmap);
    }
    free(c);
}
