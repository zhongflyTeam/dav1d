/*
 * ..
 */

#include "common/mem.h"

#include "src/ref.h"

Dav1dRef *dav1d_ref_create(const size_t size) {
    Dav1dRef *res = malloc(sizeof(Dav1dRef));
    void *data = dav1d_alloc_aligned(size, 32);

    if (!res || !data) {
        if (res) free(res);
        if (data) free(data);
        return NULL;
    }

    res->size = size;
    atomic_init(&res->ref_cnt, 1);
    res->data = data;

    return res;
}

void dav1d_ref_inc(Dav1dRef *const ref) {
    atomic_fetch_add(&ref->ref_cnt, 1);
}

void dav1d_ref_dec(Dav1dRef *const ref) {
    if (atomic_fetch_sub(&ref->ref_cnt, 1) == 1) {
        free(ref->data);
        free(ref);
    }
}
