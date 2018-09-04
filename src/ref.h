/*
 * ..
 */

#ifndef __DAV1D_SRC_REF_H__
#define __DAV1D_SRC_REF_H__

#include "include/ref.h"

Dav1dRef *dav1d_ref_create(size_t size);
void dav1d_ref_inc(Dav1dRef *ref);
void dav1d_ref_dec(Dav1dRef *ref);

#endif /* __DAV1D_SRC_REF_H__ */
