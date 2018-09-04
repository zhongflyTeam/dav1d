/*
 * ..
 */

#ifndef __DAV1D_DATA_H__
#define __DAV1D_DATA_H__

#include <stddef.h>
#include <stdint.h>

#include "ref.h"

typedef struct Dav1dData {
    uint8_t *data; ///< data pointer
    size_t sz; ///< data size
    Dav1dRef *ref; ///< allocation origin
} Dav1dData;

/**
 * Allocate data.
 */
int dav1d_data_create(Dav1dData *data, size_t sz);

#endif /* __DAV1D_DATA_H__ */
