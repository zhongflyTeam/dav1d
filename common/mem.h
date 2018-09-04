/*
 * ..
 */

#ifndef __DAV1D_COMMON_MEM_H__
#define __DAV1D_COMMON_MEM_H__

#include <assert.h>
#include <stdlib.h>

/*
 * Allocate 32-byte aligned memory. The return value can be released
 * by calling the standard free() function.
 */
static inline void *dav1d_alloc_aligned(size_t sz, size_t align) {
    void *ptr;
    assert(!(align & (align - 1)));
    if (posix_memalign(&ptr, align, sz)) return NULL;
    return ptr;
}

static inline void freep(void *ptr) {
    void **mem = (void **) ptr;
    if (*mem) {
        free(*mem);
        *mem = NULL;
    }
}

#endif /* __DAV1D_COMMON_MEM_H__ */
