/*
 * ..
 */

#ifndef __DAV1D_REF_H__
#define __DAV1D_REF_H__

#include <stdatomic.h>
#include <stddef.h>

typedef struct Dav1dRef {
    void *data;
    size_t size;
    atomic_int ref_cnt;
} Dav1dRef;

#endif /* __DAV1D_REF_H__ */
