/*
 * ..
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "common/validate.h"

#include "src/data.h"
#include "src/ref.h"

int dav1d_data_create(Dav1dData *const buf, const size_t sz) {
    validate_input_or_ret(buf != NULL, -EINVAL);

    buf->ref = dav1d_ref_create(sz);
    if (!buf->ref) return -ENOMEM;
    buf->data = buf->ref->data;
    buf->sz = sz;

    return 0;
}

void dav1d_data_unref(Dav1dData *const buf) {
    dav1d_ref_dec(buf->ref);
    memset(buf, 0, sizeof(*buf));
}
