/*
 * ..
 */

#ifndef __DAV1D_SRC_PICTURE_H__
#define __DAV1D_SRC_PICTURE_H__

#include <pthread.h>
#include <stdatomic.h>

#include "include/picture.h"

#include "src/thread_data.h"

enum PlaneType {
    PLANE_TYPE_Y,
    PLANE_TYPE_UV,
    PLANE_TYPE_BLOCK,
    PLANE_TYPE_ALL,
};

typedef struct Dav1dThreadPicture {
    Dav1dPicture p;
    int visible;
    struct thread_data *t;
    // [0] block data (including segmentation map and motion vectors)
    // [1] pixel data
    atomic_uint *progress;
} Dav1dThreadPicture;

/*
 * Allocate a picture with custom border size.
 */
int dav1d_thread_picture_alloc(Dav1dThreadPicture *p, int w, int h,
                               enum Dav1dPixelLayout layout, int bpc,
                               struct thread_data *t, int visible);

/**
 * Create a copy of a picture.
 */
void dav1d_picture_ref(Dav1dPicture *dst, const Dav1dPicture *src);
void dav1d_thread_picture_ref(Dav1dThreadPicture *dst,
                              const Dav1dThreadPicture *src);
void dav1d_thread_picture_unref(Dav1dThreadPicture *p);

/**
 * Wait for picture to reach a certain stage.
 *
 * y is in full-pixel units. If pt is not UV, this is in luma
 * units, else it is in chroma units.
 * plane_type is used to determine how many pixels delay are
 * introduced by loopfilter processes.
 */
void dav1d_thread_picture_wait(const Dav1dThreadPicture *p, int y,
                               enum PlaneType plane_type);

/**
 * Signal decoding progress.
 *
 * y is in full-pixel luma units.
 * plane_type denotes whether we have completed block data (pass 1;
 * PLANE_TYPE_BLOCK), pixel data (pass 2, PLANE_TYPE_Y) or both (no
 * 2-pass decoding; PLANE_TYPE_ALL).
 */
void dav1d_thread_picture_signal(const Dav1dThreadPicture *p, int y,
                                 enum PlaneType plane_type);

#endif /* __DAV1D_SRC_PICTURE_H__ */
